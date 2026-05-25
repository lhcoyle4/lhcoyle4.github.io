#define WIN32_LEAN_AND_MEAN
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <SDL.h>
#include <SDL_opengl.h>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <sstream>
#include <ctime>
#include "map_data.h"


#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <functional>
static std::function<void()>            MainLoopForEmscriptenP;
static void MainLoopForEmscripten()     { MainLoopForEmscriptenP(); }
#define EMSCRIPTEN_MAINLOOP_BEGIN       MainLoopForEmscriptenP = [&]() { do
#define EMSCRIPTEN_MAINLOOP_END         while (0); }; emscripten_set_main_loop(MainLoopForEmscripten, 0, true)
#else
#define EMSCRIPTEN_MAINLOOP_BEGIN
#define EMSCRIPTEN_MAINLOOP_END
#endif

// Structs for state
struct LogSegment {
    std::string text;
    ImVec4 color;
};

struct LogLine {
    std::string text;
    ImVec4 color;
    std::vector<LogSegment> segments;
};

struct FSNode {
    std::string name;
    bool is_dir;
    std::string content;
    std::vector<FSNode> children;
};

struct Project {
    std::string name;
    std::string language;
    std::string desc;
    std::vector<std::string> highlights;
    std::string url;
    std::string asciiArt;
};

struct MapCity {
    std::string name;
    float lon;
    float lat;
    std::string desc;
};

// Global variables
std::vector<LogLine> g_ConsoleLog;
char g_InputBuf[256] = "";
std::vector<std::string> g_CmdHistory;
int g_HistoryPos = -1;
bool g_TerminalScrollToBottom = false;
int g_ActiveTab = 0; // 0: Terminal, 1: Projects, 2: GIS, 3: Diagnostics, 4: About
std::vector<Project> g_Projects;
float g_RadarAngle = 0.0f;
std::vector<ImVec2> g_RadarPins;
std::vector<float> g_CpuHistory;
std::vector<float> g_RamHistory;
std::vector<float> g_NetworkHistory;
// Smooth displayed values (lerped every frame toward the latest target)
float g_CpuSmooth  = 20.0f;
float g_RamSmooth  = 45.0f;
float g_NetSmooth  = 5.0f;
// Target values set once per sample interval
float g_CpuTarget  = 20.0f;
float g_RamTarget  = 45.0f;
float g_NetTarget  = 5.0f;
auto g_StartTime = std::chrono::steady_clock::now();
bool g_MatrixMode = false;
int g_MatrixTimer = 0;

// MOTD quote pool (subset of the desktop MOTD app's DEV_TIPS)
static const char* g_DevTips[] = {
    "Talk is cheap. Show me the code. — Linus Torvalds",
    "Simplicity is the ultimate sophistication. — Leonardo da Vinci",
    "The important thing is not to stop questioning. — Albert Einstein",
    "We are what we repeatedly do. Excellence is not an act, but a habit. — Aristotle",
    "Out of clutter, find simplicity. In the middle of difficulty lies opportunity. — Einstein",
    "Nature does not hurry, yet everything is accomplished. — Lao Tzu",
    "The beginning of knowledge is the discovery of something we do not understand. — Frank Herbert",
    "Science is a way of thinking much more than it is a body of knowledge. — Carl Sagan",
    "We are made of starstuff. We are a way for the cosmos to know itself. — Carl Sagan",
    "I have not failed. I've just found 10,000 ways that won't work. — Thomas Edison",
    "GIS Tip: WGS84 (EPSG:4326) uses degrees; Web Mercator (EPSG:3857) uses meters.",
    "Tip: git status -sb shows concise branch status with tracking info.",
    "Tip: Alt+Left-Click + Drag anywhere to move a window. Right-Click to resize.",
    "GIS Tip: SNAP is excellent for historical SAR and optical remote sensing analysis.",
    "Not all those who wander are lost. — J.R.R. Tolkien",
    "The journey of a thousand miles begins with one step. — Lao Tzu",
};

// Virtual Filesystem Globals
FSNode g_FSRoot;
std::vector<std::string> g_CurrentDirParts;
bool g_FocusTerminalInput = true;

// Vi State Globals
bool g_ViMode = false;
std::string g_ViFilename = "";
std::string g_ViContent = "";
std::vector<std::string> g_ViLines;
int g_ViScrollLine = 0;
bool g_ViCommandActive = false;
char g_ViCommandChar = ':';
char g_ViCmdInput[128] = "";
std::string g_ViSearchQuery = "";
int g_ViSearchMatchIdx = -1;
int g_ViScrollToLine = -1;
bool g_FocusViInput = false;

struct ViSearchMatch {
    int line_idx;
    int char_pos;
};
std::vector<ViSearchMatch> g_ViSearchMatches;
int g_ViActiveMatchIdx = -1;


// Tab Completion Globals
std::string g_LastCompletedBuf = "";
bool g_TabCompleting = false;
std::vector<std::string> g_TabMatches;
int g_TabMatchIdx = -1;
int g_TabBaseLen = 0;

// Pinned Congested Menu State
struct PinnedMenuItem {
    std::string name;
    std::string layerName;
    ImU32 color;
};
bool g_PinnedMenuOpen = false;
ImVec2 g_PinnedMenuPos;
std::vector<PinnedMenuItem> g_PinnedMenuItems;
bool g_PinnedMenuJustOpened = false;

// SGE (Search Generative Experience) AI Overview Popup State
bool g_SgeOpen = false;
std::string g_SgeEntityName = "";
std::string g_SgeEntityLayer = "";
std::string g_SgeSummaryText = "";

// Matrix Falling Code Visualizer Globals
struct MatrixColumn {
    float y;
    float speed;
    int length;
    std::vector<char> chars;
    float nextChangeTime;
    bool active;
    float spawnDelay;
};
std::vector<MatrixColumn> s_MatrixColumns;

// Star Wars ASCII Movie Globals
struct StarWarsFrame {
    int delay;
    std::string lines[13];
};
std::vector<StarWarsFrame> g_StarWarsFrames;
bool g_StarWarsMovieMode = false;
int g_StarWarsCurrentFrameIdx = 0;
double g_StarWarsFrameShowTime = 0.0;
bool g_StarWarsMovieLoaded = false;
bool g_StarWarsMovieLoading = false;

#ifdef __EMSCRIPTEN__
#define KEEP_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define KEEP_EXPORT
#include <fstream>
#endif

void ParseStarWarsMovie(const std::string& rawText) {
    g_StarWarsFrames.clear();
    std::vector<std::string> lines;
    std::string currentLine;
    for (char c : rawText) {
        if (c == '\n') {
            if (!currentLine.empty() && currentLine.back() == '\r') {
                currentLine.pop_back();
            }
            lines.push_back(currentLine);
            currentLine.clear();
        } else if (c != '\0') {
            currentLine.push_back(c);
        }
    }
    if (!currentLine.empty()) {
        if (currentLine.back() == '\r') currentLine.pop_back();
        lines.push_back(currentLine);
    }

    for (size_t i = 0; i + 13 < lines.size(); i += 14) {
        StarWarsFrame frame;
        int delay = 1;
        if (!lines[i].empty()) {
            int val = 0;
            bool hasDigits = false;
            for (char digit : lines[i]) {
                if (digit >= '0' && digit <= '9') {
                    val = val * 10 + (digit - '0');
                    hasDigits = true;
                }
            }
            if (hasDigits) delay = val;
        }
        frame.delay = delay;
        for (int j = 0; j < 13; ++j) {
            frame.lines[j] = lines[i + 1 + j];
        }
        g_StarWarsFrames.push_back(frame);
    }
    g_StarWarsMovieLoaded = true;
    g_StarWarsMovieLoading = false;
}

extern "C" {
    KEEP_EXPORT void LoadStarWarsMovieData(const char* data) {
        if (data) {
            std::string text(data);
            ParseStarWarsMovie(text);
        }
    }
}

void StartLoadingStarWarsMovie() {
    if (g_StarWarsMovieLoaded || g_StarWarsMovieLoading) return;
    g_StarWarsMovieLoading = true;
#ifdef __EMSCRIPTEN__
    EM_ASM({
        fetch('starwars.txt')
            .then(response => {
                if (!response.ok) {
                    throw new Error('HTTP status ' + response.status);
                }
                return response.text();
            })
            .then(text => {
                var length = lengthBytesUTF8(text) + 1;
                var buffer = _malloc(length);
                stringToUTF8(text, buffer, length);
                _LoadStarWarsMovieData(buffer);
                _free(buffer);
            })
            .catch(err => {
                console.error('Failed to fetch starwars.txt:', err);
            });
    });
#else
    std::ifstream f("starwars.txt", std::ios::in | std::ios::binary);
    if (f) {
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        f.close();
        LoadStarWarsMovieData(content.c_str());
    } else {
        g_StarWarsMovieLoading = false;
    }
#endif
}

// Explorer State Globals
static int g_ExpSelectedProj = -1;
static std::vector<std::string> g_ExpCurrentDirParts;
static std::vector<std::vector<std::string>> g_ExpBackHistory;
static std::vector<std::vector<std::string>> g_ExpForwardHistory;
static std::string g_ExpSelectedFile = "";
static char g_ExpSearchBuf[64] = "";
static std::string g_ExpSearchQuery = "";

struct CommitInfo {
    std::string message;
    std::string author;
    std::string date;
    std::string changes;
};

static CommitInfo GetMockCommit(const std::string& filename) {
    if (filename == "main.cpp") {
        return { "feat: optimize mouse drag latency by bypassing hook delay", "Louie Coyle <louie@lhcoyle4.com>", "2026-05-20 09:15:44", "+45, -12 lines" };
    }
    if (filename == "main.c") {
        return { "feat: add custom SDL audio mixer and synth pipelines", "Louie Coyle <louie@lhcoyle4.com>", "2026-04-15 17:45:00", "+87, -4 lines" };
    }
    if (filename == "config.json") {
        return { "chore: add WSL configuration shortcuts", "Louie Coyle <louie@lhcoyle4.com>", "2026-05-12 16:04:30", "+5, -1 lines" };
    }
    if (filename == "LensOCR.cs") {
        return { "refactor: port Windows OCR asynchronous loader to C# class", "Louie Coyle <louie@lhcoyle4.com>", "2026-05-05 10:11:12", "+64, -2 lines" };
    }
    if (filename == "game.js") {
        return { "feat: implement high-fidelity CRT bloom phosphor decay filter", "Louie Coyle <louie@lhcoyle4.com>", "2026-04-28 12:00:55", "+38, -15 lines" };
    }
    return { "docs: update specifications and project setup instructions", "Louie Coyle <louie@lhcoyle4.com>", "2026-05-18 14:32:01", "+12, -2 lines" };
}


const float Lake_Superior_Lon[] = { -92.1f, -90.0f, -87.0f, -88.0f, -92.1f };
const float Lake_Superior_Lat[] = { 46.7f,  48.0f,  46.5f,  46.0f,  46.7f };
const int Lake_Superior_Count = sizeof(Lake_Superior_Lon) / sizeof(float);

const float Lake_Michigan_Huron_Lon[] = { -87.0f, -84.0f, -82.0f, -83.0f, -87.0f, -88.0f, -87.0f };
const float Lake_Michigan_Huron_Lat[] = { 46.0f,  46.0f,  44.0f,  43.0f,  41.8f,  44.0f,  46.0f };
const int Lake_Michigan_Huron_Count = sizeof(Lake_Michigan_Huron_Lon) / sizeof(float);

const float Lake_Erie_Ontario_Lon[] = { -83.0f, -80.0f, -76.0f, -77.0f, -80.0f, -83.0f };
const float Lake_Erie_Ontario_Lat[] = { 42.0f,  42.2f,  44.0f,  43.5f,  43.5f,  42.0f };
const int Lake_Erie_Ontario_Count = sizeof(Lake_Erie_Ontario_Lon) / sizeof(float);

const float Appalachian_Lon[] = { -82.0f, -80.0f, -76.0f, -72.0f };
const float Appalachian_Lat[] = { 35.0f,  38.0f,  41.0f,  44.0f };
const int Appalachian_Count = sizeof(Appalachian_Lon) / sizeof(float);

const float Rockies_Lon_1[] = { -115.0f, -110.0f, -112.0f, -118.0f };
const float Rockies_Lat_1[] = { 35.0f,   40.0f,   44.0f,   48.0f };
const int Rockies_Count_1 = sizeof(Rockies_Lon_1) / sizeof(float);

const float Rockies_Lon_2[] = { -106.0f, -105.0f, -108.0f, -112.0f };
const float Rockies_Lat_2[] = { 35.0f,   40.0f,   45.0f,   47.0f };
const int Rockies_Count_2 = sizeof(Rockies_Lon_2) / sizeof(float);

// Major Hydrographic Rivers (Mississippi & Colorado)
const float Mississippi_Lon[] = { -95.2f, -93.3f, -90.5f, -90.2f, -89.2f, -90.0f, -90.9f, -90.0f, -89.2f };
const float Mississippi_Lat[] = { 47.2f,  45.0f,  41.5f,  38.6f,  37.0f,  35.1f,  32.3f,  29.9f,  29.1f };
const int Mississippi_Count = sizeof(Mississippi_Lon) / sizeof(float);

const float Colorado_Lon[] = { -105.8f, -109.9f, -112.1f, -114.7f, -114.5f };
const float Colorado_Lat[] = { 40.4f,  38.2f,  36.1f,  36.0f,  32.5f };
const int Colorado_Count = sizeof(Colorado_Lon) / sizeof(float);

// Map viewer state
std::vector<MapCity> g_MapCities;
float g_MapScale = 8.0f;
ImVec2 g_MapOffset = ImVec2(0.0f, 0.0f);
bool g_ShowBoundary = true;
bool g_ShowWorld = true;
bool g_ShowStates = true;
bool g_ShowLakes = true;
bool g_ShowHighways = true;
bool g_ShowUSHighways = false;
bool g_ShowRailways = true;
bool g_ShowPowerPlants = true;
bool g_ShowSubstations = true;
bool g_ShowPipelines = true;
bool g_ShowEnergyCorridors = true;
bool g_ShowCities = true;
bool g_ShowContours = true;
bool g_ShowGrid = true;
int g_SelectedCity = 0; // Default: Portland, ME
bool g_ShowLabels = true;
bool g_ShowLabelsWorld = true;
bool g_ShowLabelsStates = true;
bool g_ShowLabelsHighways = true;
bool g_ShowLabelsUSHighways = true;
bool g_ShowLabelsRailways = true;
bool g_ShowLabelsPowerPlants = true;
bool g_ShowLabelsSubstations = true;
bool g_ShowLabelsPipelines = true;
bool g_ShowLabelsEnergyCorridors = true;
bool g_ShowLabelsLakes = true;
bool g_ShowLabelsCities = true;
bool g_ShowLabelsContours = true;


#include <ctime>

// Log function
void AddLog(const std::string& text, ImVec4 color = ImVec4(0.2f, 1.0f, 0.2f, 1.0f)) {
    LogLine line;
    line.text = text;
    line.color = color;
    g_ConsoleLog.push_back(line);
    g_TerminalScrollToBottom = true;
    if (g_ConsoleLog.size() > 100) {
        g_ConsoleLog.erase(g_ConsoleLog.begin());
    }
}

// Log function overload for colored segments
void AddLog(const std::vector<LogSegment>& segments) {
    LogLine line;
    line.color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    line.segments = segments;
    for (const auto& seg : segments) {
        line.text += seg.text;
    }
    g_ConsoleLog.push_back(line);
    g_TerminalScrollToBottom = true;
    if (g_ConsoleLog.size() > 100) {
        g_ConsoleLog.erase(g_ConsoleLog.begin());
    }
}

// Helper to get formatted date string for ls
std::string GetCurrentDateString() {
    time_t rawtime;
    struct tm* timeinfo;
    char buffer[80];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer, sizeof(buffer), "%b %d %H:%M", timeinfo);
    return std::string(buffer);
}

// Helper to format file sizes
std::string FormatSize(size_t size) {
    if (size == 0) return "0";
    if (size < 1024) return std::to_string(size);
    double kbs = size / 1024.0;
    if (kbs < 1024.0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1fK", kbs);
        return std::string(buf);
    }
    double mbs = kbs / 1024.0;
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1fM", mbs);
    return std::string(buf);
}

// Helper to count subdirectories
size_t CountSubdirs(const FSNode& node) {
    size_t count = 0;
    for (const auto& child : node.children) {
        if (child.is_dir) count++;
    }
    return count;
}

// Helper to add ls entry
void AddLogLSEntry(const std::string& name, bool is_dir, size_t size, size_t subdirs_count, const std::string& dateStr) {
    std::string perms = is_dir ? "drwxr-xr-x" : ((name.size() > 3 && name.substr(name.size() - 3) == ".sh") ? "-rwxr-xr-x" : "-rw-r--r--");
    std::string links = std::to_string(is_dir ? (subdirs_count + 2) : 1);
    std::string sizeStr = is_dir ? "4.0K" : FormatSize(size);
    
    char meta[128];
    snprintf(meta, sizeof(meta), "%s %2s louie louie %5s %s ", perms.c_str(), links.c_str(), sizeStr.c_str(), dateStr.c_str());
    
    std::vector<LogSegment> segments;
    segments.push_back({ meta, ImVec4(0.7f, 0.7f, 0.7f, 1.0f) });
    
    ImVec4 nameColor = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
    std::string dispName = name;
    if (is_dir) {
        nameColor = ImVec4(0.20f, 0.60f, 0.86f, 1.00f); // Folder blue
    } else if (name.size() > 3 && name.substr(name.size() - 3) == ".sh") {
        nameColor = ImVec4(0.18f, 0.80f, 0.44f, 1.00f); // Executable green
    } else if (name.size() > 4 && name.substr(name.size() - 4) == ".txt") {
        nameColor = ImVec4(0.90f, 0.90f, 0.90f, 1.00f); // Text file white
    }
    
    segments.push_back({ dispName, nameColor });
    AddLog(segments);
}

// Prompt logging helper
void AddLogPrompt(const std::string& path, const std::string& command) {
    std::vector<LogSegment> segments;
    segments.push_back({ "louie@lhcoyle4-core", ImVec4(0.18f, 0.80f, 0.44f, 1.00f) });
    segments.push_back({ ":", ImVec4(0.90f, 0.90f, 0.90f, 1.00f) });
    segments.push_back({ path, ImVec4(0.20f, 0.60f, 0.86f, 1.00f) });
    segments.push_back({ "$ ", ImVec4(0.90f, 0.90f, 0.90f, 1.00f) });
    segments.push_back({ command, ImVec4(0.95f, 0.95f, 0.95f, 1.00f) });
    AddLog(segments);
}

// Redirect URL using JS
void OpenGitHubLink(const std::string& url) {
#ifdef __EMSCRIPTEN__
    std::string jsCmd = "window.open('" + url + "', '_blank');";
    emscripten_run_script(jsCmd.c_str());
#else
    printf("Opening URL: %s\n", url.c_str());
#endif
}

// Launch PERMADRIFT as a fullscreen iframe overlay within this page
void LaunchPermadrift() {
#ifdef __EMSCRIPTEN__
    EM_ASM({
        if (document.getElementById('pd-ov')) return;
        var ov = document.createElement('div');
        ov.id = 'pd-ov';
        ov.style.cssText = 'position:fixed;top:0;left:0;width:100%;height:100%;z-index:9999;background:#000;';
        var btn = document.createElement('button');
        btn.textContent = 'EXIT PERMADRIFT';
        btn.style.cssText = 'position:absolute;top:8px;right:8px;z-index:10000;background:rgba(0,15,30,0.9);color:#70d8ff;border:1px solid #2a6080;font-family:monospace;font-size:12px;padding:5px 10px;cursor:pointer;letter-spacing:0.08em;';
        btn.onclick = function() { closePD(); };
        var fr = document.createElement('iframe');
        fr.src = './permadrift/';
        fr.style.cssText = 'width:100%;height:100%;border:none;display:block;';
        fr.allow = 'autoplay';
        ov.appendChild(fr);
        ov.appendChild(btn);
        document.body.appendChild(ov);
        function closePD() {
            var o = document.getElementById('pd-ov');
            if (o) document.body.removeChild(o);
            window.removeEventListener('message', pdMsg);
        }
        function pdMsg(e) {
            if (e.data === 'permadrift_exit') closePD();
        }
        window.addEventListener('message', pdMsg);
    });
#else
    printf("PERMADRIFT: would launch iframe overlay\n");
#endif
}

#include <iomanip>
#include <cctype>

// Helper to URL encode query parameters
std::string UrlEncode(const std::string& value) {
    std::ostringstream escaped;
    escaped << std::hex;
    for (char c : value) {
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else if (c == ' ') {
            escaped << '+';
        } else {
            escaped << '%' << std::uppercase << std::setw(2) << std::setfill('0') << (int)(unsigned char)c;
        }
    }
    return escaped.str();
}

// Trigger Google Search in a new tab
void SearchGoogle(const std::string& name) {
    std::string query = UrlEncode(name);
    std::string url = "https://www.google.com/search?q=" + query;
    OpenGitHubLink(url);
}

// Custom style
void SetupRetroStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.PopupRounding = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.TabRounding = 0.0f;
    
    style.Colors[ImGuiCol_Text] = ImVec4(0.0f, 1.0f, 0.3f, 1.0f); // Matrix Green
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.0f, 0.6f, 0.1f, 1.0f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.02f, 0.04f, 0.02f, 0.95f); // Tech Green-Black
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.03f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.01f, 0.06f, 0.01f, 0.95f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.0f, 0.8f, 0.2f, 0.6f);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.08f, 0.0f, 0.8f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.0f, 0.20f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.0f, 0.35f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.0f, 0.15f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.0f, 0.3f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.0f, 0.08f, 0.0f, 0.5f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.0f, 0.08f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.0f, 0.04f, 0.0f, 0.5f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.0f, 0.6f, 0.15f, 0.6f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.0f, 0.8f, 0.2f, 0.8f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.0f, 1.0f, 0.3f, 1.0f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.0f, 1.0f, 0.3f, 1.0f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.0f, 0.6f, 0.2f, 0.8f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.0f, 1.0f, 0.3f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.0f, 0.15f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.0f, 0.3f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.0f, 0.5f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.0f, 0.25f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.0f, 0.35f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.0f, 0.5f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_Separator] = ImVec4(0.0f, 0.8f, 0.2f, 0.4f);
    style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.0f, 0.8f, 0.2f, 0.8f);
    style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.0f, 1.0f, 0.3f, 1.0f);
    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.0f, 0.6f, 0.15f, 0.2f);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.0f, 0.8f, 0.2f, 0.6f);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.0f, 1.0f, 0.3f, 0.9f);
    style.Colors[ImGuiCol_Tab] = ImVec4(0.0f, 0.15f, 0.0f, 0.8f);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.0f, 0.35f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_TabActive] = ImVec4(0.0f, 0.45f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.0f, 0.1f, 0.0f, 0.8f);
    style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.0f, 0.25f, 0.0f, 1.0f);
}

// Populate projects list
void InitializeProjects() {
    g_Projects.push_back({
        "alt_drag_resize", "Python",
        "A background system tray Python application bringing Linux-style Alt+Click window moving and resizing to Windows.",
        {
            "Uses pynput keyboard/mouse listeners and win32gui window management.",
            "Features automated task management to close windows with Ctrl+Alt+Q.",
            "Includes startup shortcut generation script.",
            "Tray icon integration using pystray and custom PIL-generated bitmaps."
        },
        "https://github.com/lhcoyle4/alt_drag_resize",
        "  +-----------------+\n  | Python Alt-Drag |\n  |  (pynput/Win32) |\n  |   [System Tray] |\n  +-----------------+"
    });

    g_Projects.push_back({
        "alt_drag_resizer_C", "C++",
        "A lightweight, highly efficient Windows desktop utility that enables Linux-style 'Alt+Drag' window resizing and moving.",
        {
            "Written in pure C++ / Win32 API to achieve zero overhead.",
            "Listens to mouse/keyboard hooks to move and resize windows dynamically.",
            "Sub-millisecond latency for immediate responsiveness.",
            "Precompiled executable sizes under 600 KB."
        },
        "https://github.com/lhcoyle4/alt_drag_resizer_C",
        "  +-----------------+\n  | Window Manager  |\n  | [Alt]+[L-Click] |\n  |   --> Moves!    |\n  | [Alt]+[R-Click] |\n  |   --> Resizes!  |\n  +-----------------+"
    });

    g_Projects.push_back({
        "terminal_launcher_C", "C++",
        "A hotkey-driven system tray application that launches configured shells and environments with sub-millisecond dispatching.",
        {
            "Built with pure Win32 API, featuring custom vector-drawn system tray icon.",
            "Asynchronous process spawning preserving window hierarchy.",
            "Configuration scanner parsing global shortcuts from config.json.",
            "Low-level keyboard hooks bypass heavy shell layers for lightning-fast loads."
        },
        "https://github.com/lhcoyle4/terminal_launcher_C",
        "   +--------------+\n   |  pwsh (Ctrl) |\n   |  cmd  (Alt)  |\n   |  wsl  (Shift)|\n   |   [Global]   |\n   +--------------+"
    });

    g_Projects.push_back({
        "lens_ocr_C", "C++ / C#",
        "A dual-process screen snipping OCR utility with seamless Google Search query integration.",
        {
            "Handles high-performance screenshot capture in C++ using GDI+.",
            "Orchestrates background Windows OCR runtime APIs in a C# sub-module.",
            "Automates parsing, clipping, and querying terms directly in browser tabs.",
            "Communication done via low-latency file pipelines."
        },
        "https://github.com/lhcoyle4/lens_ocr_C",
        "  [Snipping Utility]\n      | (Capture)\n      v\n  [Windows OCR API]\n      | (Text parsed)\n      v\n  [Google Search Browser]"
    });

    g_Projects.push_back({
        "PERMADRIFT", "C / SDL2",
        "A roguelite Autodyne drift-ship survival game set in a dying-sun far-future. "
        "Pilot through the Permasphere debris belt, collect relics, and achieve Chronicle.",
        {
            "Procedurally generated asteroid fields with 30+ roguelite upgrade relics.",
            "Authentic vector-CRT phosphor glow rendering via SDL2 software renderer.",
            "Full controller (twin-stick + D-pad), mouse-aim, and rebindable keybinds.",
            "Infinite freeroam world with camera tracking and screen-wrap physics.",
            "Native Windows exe + WASM browser build via Emscripten CI pipeline."
        },
        "https://github.com/lhcoyle4/asteroids_vectrex",
        "    *  .  PERMADRIFT  .\n  .   /\\   *  .  *   .\n  *  /  \\  AUTODYNE  *\n    /____\\ .  *  .  *\n  *  DRIFT-SHIP  .  *\n  . PERMASPHERE  .   "
    });

    g_Projects.push_back({
        "joust_C", "C / SDL2",
        "A pure C remake of the classic arcade hit Joust, utilizing raw SDL2 for graphics, sound, and platform abstraction.",
        {
            "Written entirely in C99, bypassing modern heavy runtime layers.",
            "Direct graphics rendering through WebGL-backed SDL textures.",
            "Nostalgic synth physics and audio pipelines via SDL_mixer.",
            "Custom asset packing keeping sizes extremely compact."
        },
        "https://github.com/lhcoyle4/joust_C",
        "   _  \n  (o\\  <-- (Ostrich Mount)\n   \\ \\_ \n   /_/  \n  // \\\\  "
    });

    g_Projects.push_back({
        "motd", "Python",
        "A beautiful, high-contrast, dynamic developer dashboard for Windows terminal shells (PowerShell, CMD, Bash) displaying system diagnostics, weather, git updates, and tasks.",
        {
            "Live system diagnostics (CPU, RAM, Disk) using psutil.",
            "Weather reports pulled dynamically from wttr.in.",
            "Scans workspace Git repositories to track dirty files, local commits, and remote syncing.",
            "Implements a background process cache to maintain startup delays under 50ms."
        },
        "https://github.com/lhcoyle4/motd",
        "  +-----------------+\n  |  [SYSTEM HEALTH]| \xE2\x9B\x85 Weather\n  |  CPU [\xE2\x96\x88\xE2\x96\x88\xE2\x96\x88\xE2\x96\x88\xE2\x96\x91\xE2\x96\x91\xE2\x96\x91]  | \xF0\x9F\x9\x83 Git Repos\n  |  RAM [\xE2\x96\x88\xE2\x96\x88\xE2\x96\x88\xE2\x96\x88\xE2\x96\x88\xE2\x96\x88\xE2\x96\x91]  | \xF0\x9F\x93\x9D Checklist\n  +-----------------+"
    });

    g_Projects.push_back({
        "terminal_launcher", "Python / C++",
        "A hybrid system tray keyboard hook service and configuration GUI that dispatches user shell environments instantly.",
        {
            "Combines keyboard hooks in Python (pynput) and C++ for sub-millisecond dispatching.",
            "Built-in system tray integration via pystray and custom Win32 vector icons.",
            "Parses JSON-configured global shortcuts to run custom executable paths.",
            "Enables administrative elevation (-Verb RunAs) directly from shortcuts."
        },
        "https://github.com/lhcoyle4/terminal_launcher",
        "  +-------------------+\n  | System Tray App   |\n  | [Ctrl+Alt+T]      |\n  |   --> launch CLI  |\n  | [Ctrl+Alt+/]      |\n  |   --> launch agy  |\n  +-------------------+"
    });

    g_Projects.push_back({
        "lhcoyle4.github.io", "C++ / WASM",
        "My personal homepage and developer portfolio, built entirely in C++ and compiled to WebAssembly (WASM).",
        {
            "Draws the user interface using Dear ImGui, rendering directly to a WebGL2 canvas.",
            "Maintains zero runtime DOM dependencies for smooth 60 FPS painting.",
            "Features a retro-themed console shell with custom virtual filesystem.",
            "Visualizes dynamic system telemetry and custom GIS cartography maps."
        },
        "https://github.com/lhcoyle4/lhcoyle4.github.io",
        "  +--------------------+\n  | Dear ImGui Canvas  |\n  | [WASM Bytecode]    |\n  |   --> WebGL2       |\n  |   --> 60 FPS Paint |\n  +--------------------+"
    });
}

// Populate GIS map cities
void InitializeMapData() {
    g_MapCities.push_back({ "Portland, ME", -70.25f, 43.66f, "Louie's Base Station. RTK drone mapping active, storm surge models running. Status: ONLINE." });
    g_MapCities.push_back({ "Seattle, WA", -122.33f, 47.60f, "USGS Station NW-1. LiDAR elevation grid processed. Status: ACTIVE." });
    g_MapCities.push_back({ "San Francisco, CA", -122.42f, 37.77f, "USGS Station SW-2. Crustal deformation GPS tracking active. Status: ACTIVE." });
    g_MapCities.push_back({ "Los Angeles, CA", -118.24f, 34.05f, "USGS Station SW-1. Water table and aquifer telemetry. Status: NOMINAL." });
    g_MapCities.push_back({ "Denver, CO", -104.99f, 39.73f, "USGS Station CO-1. Mountain snowpack & runoff models active. Status: ACTIVE." });
    g_MapCities.push_back({ "Houston, TX", -95.36f, 29.76f, "USGS Station S-1. Gulf storm warning and drainage grid. Status: STANDBY." });
    g_MapCities.push_back({ "Chicago, IL", -87.62f, 41.87f, "USGS Station MW-1. Lake Michigan water level sensors online. Status: NOMINAL." });
    g_MapCities.push_back({ "Miami, FL", -80.19f, 25.76f, "USGS Station SE-1. Everglades NDVI rewilding study. Status: ACTIVE." });
    g_MapCities.push_back({ "New York, NY", -74.00f, 40.71f, "USGS Station E-1. Estuary tide level monitoring. Status: NOMINAL." });
    g_MapCities.push_back({ "Washington, DC", -77.03f, 38.90f, "National Map HQ. Central server catalog synced. Status: ONLINE." });
}
// Helper to get current path string
std::string GetCurrentPathString() {
    if (g_CurrentDirParts.empty()) return "/";
    std::string s = "";
    for (const auto& part : g_CurrentDirParts) {
        s += "/" + part;
    }
    return s;
}

// Find a node by traversing directory parts
FSNode* FindNodeFromParts(const std::vector<std::string>& parts) {
    const FSNode* curr = &g_FSRoot;
    for (const auto& part : parts) {
        bool found = false;
        for (const auto& child : curr->children) {
            if (child.name == part) {
                curr = &child;
                found = true;
                break;
            }
        }
        if (!found) return nullptr;
    }
    return const_cast<FSNode*>(curr);
}

// Resolve relative or absolute path to an FSNode
FSNode* ResolvePath(const std::string& path, std::vector<std::string>& outParts) {
    std::vector<std::string> parts;
    if (path.empty()) {
        outParts = g_CurrentDirParts;
        return FindNodeFromParts(outParts);
    }
    if (path[0] == '/') {
        parts = {};
    } else {
        parts = g_CurrentDirParts;
    }

    std::stringstream ss(path);
    std::string segment;
    while (std::getline(ss, segment, '/')) {
        if (segment.empty() || segment == ".") {
            continue;
        }
        if (segment == "..") {
            if (!parts.empty()) {
                parts.pop_back();
            }
        } else {
            parts.push_back(segment);
        }
    }

    FSNode* node = FindNodeFromParts(parts);
    if (node) {
        outParts = parts;
    }
    return node;
}

// Initialize Virtual Filesystem
void InitializeVirtualFS() {
    g_FSRoot.name = "";
    g_FSRoot.is_dir = true;

    FSNode projects;
    projects.name = "projects";
    projects.is_dir = true;

    // alt_drag_resize
    {
        FSNode dir;
        dir.name = "alt_drag_resize";
        dir.is_dir = true;

        FSNode gitignore;
        gitignore.name = ".gitignore";
        gitignore.is_dir = false;
        gitignore.content = "__pycache__/\n*.pyc\n";
        dir.children.push_back(gitignore);

        FSNode readme;
        readme.name = "README.md";
        readme.is_dir = false;
        readme.content = "# Alt-Drag & Resize Tool for Windows\n\n"
                         "This tool brings a popular Linux window management feature to Windows. It allows you to move and resize windows without having to find the title bar or corners.\n\n"
                         "## How to use:\n"
                         "- **Move a window:** Hold the `Alt` key and **Left-Click + Drag** anywhere inside a window.\n"
                         "- **Resize a window:** Hold the `Alt` key and **Right-Click + Drag** anywhere inside a window.\n"
                         "- **Exit:** Press `Alt + Shift + Q` or close the command window.";
        dir.children.push_back(readme);

        FSNode addStartup;
        addStartup.name = "add_to_startup.py";
        addStartup.is_dir = false;
        addStartup.content = "import os, sys\n"
                             "import winshell\n"
                             "from win32com.client import Dispatch\n\n"
                             "def create_startup_shortcut():\n"
                             "    startup_path = winshell.startup()\n"
                             "    shortcut_path = os.path.join(startup_path, 'AltDrag.lnk')\n"
                             "    shell = Dispatch('WScript.Shell')\n"
                             "    shortcut = shell.CreateShortCut(shortcut_path)\n"
                             "    shortcut.Targetpath = sys.executable\n"
                             "    shortcut.Arguments = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'main.py')\n"
                             "    shortcut.WorkingDirectory = os.path.dirname(os.path.abspath(__file__))\n"
                             "    shortcut.save()\n\n"
                             "if __name__ == '__main__':\n"
                             "    create_startup_shortcut()\n";
        dir.children.push_back(addStartup);

        FSNode mainPy;
        mainPy.name = "main.py";
        mainPy.is_dir = false;
        mainPy.content = "import win32gui, win32api, win32con\n"
                         "from pynput import mouse, keyboard\n"
                         "import sys, os, threading\n"
                         "import tkinter as tk\n"
                         "from tkinter import messagebox\n"
                         "from PIL import Image, ImageDraw\n"
                         "import pystray\n\n"
                         "class AltDragTool:\n"
                         "    def __init__(self):\n"
                         "        self.alt_pressed = False\n"
                         "        self.dragging = False\n"
                         "        self.resizing = False\n"
                         "        self.target_hwnd = None\n\n"
                         "    def run(self):\n"
                         "        # Tray icon and listener threads setup\n"
                         "        pass\n\n"
                         "if __name__ == '__main__':\n"
                         "    AltDragTool().run()";
        dir.children.push_back(mainPy);

        FSNode runBat;
        runBat.name = "run.bat";
        runBat.is_dir = false;
        runBat.content = "@echo off\nstart /b pythonw.exe main.py\n";
        dir.children.push_back(runBat);

        projects.children.push_back(dir);
    }

    // alt_drag_resizer_C
    {
        FSNode dir;
        dir.name = "alt_drag_resizer_C";
        dir.is_dir = true;

        FSNode readme;
        readme.name = "README.md";
        readme.is_dir = false;
        readme.content = "# alt_drag_resizer_C\n"
                         "A lightweight, highly efficient Windows desktop utility that enables Linux-style 'Alt+Drag' window resizing and moving.\n\n"
                         "## Key Features\n"
                         "- Written in pure C++ / Win32 API to achieve zero overhead.\n"
                         "- Listens to mouse/keyboard hooks to move and resize windows dynamically.\n"
                         "- Sub-millisecond latency for immediate responsiveness.\n"
                         "- Precompiled executable sizes under 600 KB.";
        dir.children.push_back(readme);

        FSNode mainCpp;
        mainCpp.name = "main.cpp";
        mainCpp.is_dir = false;
        mainCpp.content = "#define WIN32_LEAN_AND_MEAN\n"
                          "#include <windows.h>\n\n"
                          "HHOOK g_hMouseHook = NULL;\n"
                          "HWND g_hDragWindow = NULL;\n"
                          "POINT g_PtStart;\n"
                          "RECT g_RcStart;\n\n"
                          "LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {\n"
                          "    if (nCode >= 0) {\n"
                          "        MSLLHOOKSTRUCT* pMouse = (MSLLHOOKSTRUCT*)lParam;\n"
                          "        if (wParam == WM_MOUSEMOVE && (GetAsyncKeyState(VK_MENU) & 0x8000)) {\n"
                          "            int dx = pMouse->pt.x - g_PtStart.x;\n"
                          "            int dy = pMouse->pt.y - g_PtStart.y;\n"
                          "            SetWindowPos(g_hDragWindow, NULL, g_RcStart.left + dx, g_RcStart.top + dy, 0, 0, SWP_NOSIZE | SWP_NOZORDER);\n"
                          "        }\n"
                          "    }\n"
                          "    return CallNextHookEx(g_hMouseHook, nCode, wParam, lParam);\n"
                          "}\n\n"
                          "int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {\n"
                          "    g_hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, hInst, 0);\n"
                          "    MSG msg;\n"
                          "    while (GetMessage(&msg, NULL, 0, 0)) {\n"
                          "        TranslateMessage(&msg);\n"
                          "        DispatchMessage(&msg);\n"
                          "    }\n"
                          "    UnhookWindowsHookEx(g_hMouseHook);\n"
                          "    return 0;\n"
                          "}";
        dir.children.push_back(mainCpp);
        projects.children.push_back(dir);
    }

    // terminal_launcher_C
    {
        FSNode dir;
        dir.name = "terminal_launcher_C";
        dir.is_dir = true;

        FSNode gitignore;
        gitignore.name = ".gitignore";
        gitignore.is_dir = false;
        gitignore.content = "*.exe\n";
        dir.children.push_back(gitignore);

        FSNode buildBat;
        buildBat.name = "build.bat";
        buildBat.is_dir = false;
        buildBat.content = "@echo off\n"
                           "echo Compiling Terminal Launcher...\n"
                           "g++ -O3 -std=c++17 main.cpp -o TerminalLauncher.exe -luser32 -lshell32 -lgdi32 -ladvapi32 -mwindows -static\n";
        dir.children.push_back(buildBat);

        FSNode configJson;
        configJson.name = "config.json";
        configJson.is_dir = false;
        configJson.content = "{\n"
                             "    \"shortcuts\": {\n"
                             "        \"<alt>+<ctrl>+u\": \"wsl\",\n"
                             "        \"<alt>+<ctrl>+p\": \"powershell\",\n"
                             "        \"<alt>+<ctrl>+c\": \"cmd\",\n"
                             "        \"<alt>+<ctrl>+7\": \"pwsh\",\n"
                             "        \"<alt>+<ctrl>+/\": \"pwsh -NoExit -Command agy\",\n"
                             "        \"<alt>+<ctrl>+a\": \"pwsh -Verb RunAs\"\n"
                             "    }\n"
                             "}\n";
        dir.children.push_back(configJson);

        FSNode mainCpp;
        mainCpp.name = "main.cpp";
        mainCpp.is_dir = false;
        mainCpp.content = "#define WIN32_LEAN_AND_MEAN\n"
                          "#include <windows.h>\n"
                          "#include <shellapi.h>\n"
                          "#include <string>\n"
                          "#include <vector>\n\n"
                          "void LaunchCommand(std::string command) {\n"
                          "    // Parses commands and launches them as admin/user relative to profile dir\n"
                          "}\n\n"
                          "void RegisterAllHotkeys(HWND hWnd) {\n"
                          "    // Scans config.json shortcuts and binds global key hooks\n"
                          "}\n\n"
                          "int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {\n"
                          "    // Singleton mutex guard and helper thread message queue\n"
                          "    return 0;\n"
                          "}";
        dir.children.push_back(mainCpp);

        FSNode launcherExe;
        launcherExe.name = "TerminalLauncher.exe";
        launcherExe.is_dir = false;
        launcherExe.content = "[Binary Executable Data - Win32 background dispatcher]";
        dir.children.push_back(launcherExe);

        projects.children.push_back(dir);
    }

    // lens_ocr_C
    {
        FSNode dir;
        dir.name = "lens_ocr_C";
        dir.is_dir = true;

        FSNode gitignore;
        gitignore.name = ".gitignore";
        gitignore.is_dir = false;
        gitignore.content = "*.exe\n";
        dir.children.push_back(gitignore);

        FSNode buildBat;
        buildBat.name = "build.bat";
        buildBat.is_dir = false;
        buildBat.content = "@echo off\n"
                           "echo Compiling OCR Helper using csc...\n"
                           "powershell -Command \"csc.exe /r:System.dll /out:OcrHelper.exe OcrHelper.cs\"\n"
                           "echo Compiling Lens OCR...\n"
                           "g++ -O3 -std=c++17 main.cpp -o LensOcr.exe -luser32 -lshell32 -lgdi32 -ladvapi32 -mwindows -static\n";
        dir.children.push_back(buildBat);

        FSNode configJson;
        configJson.name = "config.json";
        configJson.is_dir = false;
        configJson.content = "{\n"
                             "    \"shortcuts\": {\n"
                             "        \"ocr_to_clipboard\": \"<alt>+<ctrl>+o\",\n"
                             "        \"ocr_and_search\": \"<alt>+<ctrl>+s\"\n"
                             "    }\n"
                             "}\n";
        dir.children.push_back(configJson);

        FSNode lensOcrExe;
        lensOcrExe.name = "LensOcr.exe";
        lensOcrExe.is_dir = false;
        lensOcrExe.content = "[Binary Executable Data - Win32 application]";
        dir.children.push_back(lensOcrExe);

        FSNode mainCpp;
        mainCpp.name = "main.cpp";
        mainCpp.is_dir = false;
        mainCpp.content = "#define WIN32_LEAN_AND_MEAN\n"
                          "#include <windows.h>\n"
                          "#include <shellapi.h>\n"
                          "#include <string>\n"
                          "#include <vector>\n\n"
                          "void TriggerSelection(bool searchMode) {\n"
                          "    // Creates a layered transparent crop window\n"
                          "}\n\n"
                          "void OnSelectionCompleted(RECT rc) {\n"
                          "    // Snipping area, captures screen bits, saves temp BMP,\n"
                          "    // runs OcrHelper.exe and populates clipboard or Google search.\n"
                          "}\n\n"
                          "int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {\n"
                          "    // Standard tray icon registration & global hooks\n"
                          "    return 0;\n"
                          "}";
        dir.children.push_back(mainCpp);

        FSNode ocrCs;
        ocrCs.name = "OcrHelper.cs";
        ocrCs.is_dir = false;
        ocrCs.content = "using System;\n"
                        "using System.IO;\n"
                        "using System.Threading.Tasks;\n"
                        "using Windows.Graphics.Imaging;\n"
                        "using Windows.Media.Ocr;\n\n"
                        "class Program {\n"
                        "    static void Main(string[] args) {\n"
                        "        // Reads file path, runs UWP OCR engine asynchronously\n"
                        "    }\n"
                        "}";
        dir.children.push_back(ocrCs);

        FSNode ocrExe;
        ocrExe.name = "OcrHelper.exe";
        ocrExe.is_dir = false;
        ocrExe.content = "[Binary Executable Data - .NET Core application]";
        dir.children.push_back(ocrExe);

        projects.children.push_back(dir);
    }

    // asteroids_vectrex
    {
        FSNode dir;
        dir.name = "asteroids_vectrex";
        dir.is_dir = true;

        FSNode readme;
        readme.name = "README.md";
        readme.is_dir = false;
        readme.content = "# asteroids_vectrex\n"
                         "A retro vector-graphics clone of the classic Asteroids arcade game, built for web and native environments.\n\n"
                         "## Key Features\n"
                         "- Simulates high-fidelity vector CRT glow effects.\n"
                         "- Employs precise 2D collision geometry.\n"
                         "- Smooth particle engines rendering asteroid fracturing.";
        dir.children.push_back(readme);

        FSNode gameJs;
        gameJs.name = "game.js";
        gameJs.is_dir = false;
        gameJs.content = "class VectorShip {\n"
                         "    constructor(x, y) {\n"
                         "        this.x = x;\n"
                         "        this.y = y;\n"
                         "        this.angle = 0;\n"
                         "        this.velocity = { x: 0, y: 0 };\n"
                         "    }\n"
                         "    thrust() {\n"
                         "        this.velocity.x += Math.cos(this.angle) * 0.1;\n"
                         "        this.velocity.y += Math.sin(this.angle) * 0.1;\n"
                         "    }\n"
                         "    draw(ctx) {\n"
                         "        ctx.strokeStyle = '#00ff44';\n"
                         "        ctx.shadowBlur = 15;\n"
                         "        ctx.shadowColor = '#00ff44';\n"
                         "        ctx.beginPath();\n"
                         "        ctx.moveTo(this.x + Math.cos(this.angle) * 10, this.y + Math.sin(this.angle) * 10);\n"
                         "        ctx.lineTo(this.x + Math.cos(this.angle + 2.5) * 8, this.y + Math.sin(this.angle + 2.5) * 8);\n"
                         "        ctx.lineTo(this.x + Math.cos(this.angle - 2.5) * 8, this.y + Math.sin(this.angle - 2.5) * 8);\n"
                         "        ctx.closePath();\n"
                         "        ctx.stroke();\n"
                         "    }\n"
                         "}";
        dir.children.push_back(gameJs);
        projects.children.push_back(dir);
    }

    // joust_C
    {
        FSNode dir;
        dir.name = "joust_C";
        dir.is_dir = true;

        FSNode readme;
        readme.name = "README.md";
        readme.is_dir = false;
        readme.content = "# joust_C\n"
                         "A pure C remake of the classic arcade hit Joust, utilizing raw SDL2 for graphics, sound, and platform abstraction.\n\n"
                         "## Key Features\n"
                         "- Written entirely in C99, bypassing modern heavy runtime layers.\n"
                         "- Direct graphics rendering through WebGL-backed SDL textures.\n"
                         "- Nostalgic synth physics and audio pipelines.";
        dir.children.push_back(readme);

        FSNode mainC;
        mainC.name = "main.c";
        mainC.is_dir = false;
        mainC.content = "#include <SDL.h>\n"
                       "#include <stdbool.h>\n\n"
                       "int main(int argc, char* argv[]) {\n"
                       "    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) return -1;\n"
                       "    SDL_Window* win = SDL_CreateWindow(\"Joust C\", 100, 100, 640, 480, 0);\n"
                       "    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);\n"
                       "    bool running = true;\n"
                       "    SDL_Event e;\n"
                       "    while (running) {\n"
                       "        while (SDL_PollEvent(&e)) {\n"
                       "            if (e.type == SDL_QUIT) running = false;\n"
                       "        }\n"
                       "        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);\n"
                       "        SDL_RenderClear(ren);\n"
                       "        SDL_RenderPresent(ren);\n"
                       "    }\n"
                       "    SDL_DestroyRenderer(ren);\n"
                       "    SDL_DestroyWindow(win);\n"
                       "    SDL_Quit();\n"
                       "    return 0;\n"
                       "}";
        dir.children.push_back(mainC);
        projects.children.push_back(dir);
    }

    // motd
    {
        FSNode dir;
        dir.name = "motd";
        dir.is_dir = true;

        FSNode gitignore;
        gitignore.name = ".gitignore";
        gitignore.is_dir = false;
        gitignore.content = "cache.json\ntodo.json\n";
        dir.children.push_back(gitignore);

        FSNode configJson;
        configJson.name = "config.json";
        configJson.is_dir = false;
        configJson.content = "{\n"
                             "    \"username\": \"Louie\",\n"
                             "    \"weather_location\": \"Portland,ME\",\n"
                             "    \"theme\": \"ocean\",\n"
                             "    \"sections\": {\n"
                             "        \"system_stats\": true,\n"
                             "        \"weather\": true,\n"
                             "        \"git_status\": true,\n"
                             "        \"todos\": true,\n"
                             "        \"dev_tips\": true\n"
                             "    }\n"
                             "}\n";
        dir.children.push_back(configJson);

        FSNode cacheJson;
        cacheJson.name = "cache.json";
        cacheJson.is_dir = false;
        cacheJson.content = "{\n"
                            "    \"timestamp\": 1780000000.0,\n"
                            "    \"weather\": \"⛅ +15°C Sunny\",\n"
                            "    \"git_status\": {\n"
                            "        \"motd\": {\n"
                            "            \"branch\": \"master\",\n"
                            "            \"dirty\": false,\n"
                            "            \"modified\": 0,\n"
                            "            \"untracked\": 0,\n"
                            "            \"remote\": \"Synced\"\n"
                            "        }\n"
                            "    }\n"
                            "}\n";
        dir.children.push_back(cacheJson);

        FSNode todoJson;
        todoJson.name = "todo.json";
        todoJson.is_dir = false;
        todoJson.content = "[]\n";
        dir.children.push_back(todoJson);

        FSNode installPs;
        installPs.name = "install.ps1";
        installPs.is_dir = false;
        installPs.content = "# install.ps1 - Installer for Terminal MOTD\n"
                             "param ([switch]$Uninstall)\n\n"
                             "$scriptPath = \"$PSScriptRoot\\main.py\"\n"
                             "if ($Uninstall) {\n"
                             "    Write-Host \"Uninstalling Terminal MOTD hook...\"\n"
                             "    # Safely parses and removes hooks from PowerShell profile scripts\n"
                             "} else {\n"
                             "    Write-Host \"Installing Terminal MOTD hook...\"\n"
                             "    # Inserts Python invocation trigger to user profile files\n"
                             "}\n";
        dir.children.push_back(installPs);

        FSNode mainPy;
        mainPy.name = "main.py";
        mainPy.is_dir = false;
        mainPy.content = "import os, sys, json, time, subprocess, argparse\n"
                         "from datetime import datetime\n"
                         "try: import psutil\n"
                         "except ImportError: psutil = None\n\n"
                         "def get_system_stats():\n"
                         "    if not psutil: return None\n"
                         "    return {\n"
                         "        'cpu_pct': psutil.cpu_percent(interval=None),\n"
                         "        'ram_pct': psutil.virtual_memory().percent,\n"
                         "        'uptime': '7h 9m'\n"
                         "    }\n\n"
                         "def check_git_repo(path):\n"
                         "    return {'branch': 'master', 'dirty': False, 'remote': 'Synced'}\n\n"
                         "def print_dashboard():\n"
                         "    print('Welcome, Louie!')\n"
                         "    stats = get_system_stats()\n"
                         "    if stats:\n"
                         "        print(f'CPU: {stats[\"cpu_pct\"]}% | RAM: {stats[\"ram_pct\"]}%')\n\n"
                         "if __name__ == '__main__':\n"
                         "    print_dashboard()\n";
        dir.children.push_back(mainPy);

        FSNode readme;
        readme.name = "README.md";
        readme.is_dir = false;
        readme.content = "# Terminal MOTD (Message of the Day) Dashboard\n\n"
                         "A beautiful, high-contrast, dynamic developer dashboard for Windows terminal shells (PowerShell, CMD, Bash).\n\n"
                         "## Features\n"
                         "- 🖥️ **System Health Monitor**: Live metrics for CPU, RAM, and Disk space.\n"
                         "- ⛅ **Dynamic Weather**: Local weather info sourced directly from wttr.in.\n"
                         "- 🗃️ **Git Repository Tracker**: Scans Git folders inside your sandbox.\n"
                         "- 📝 **Productivity Todo checklist**: Persisted checklist manager.\n";
        dir.children.push_back(readme);

        projects.children.push_back(dir);
    }

    // terminal_launcher
    {
        FSNode dir;
        dir.name = "terminal_launcher";
        dir.is_dir = true;

        FSNode gitignore;
        gitignore.name = ".gitignore";
        gitignore.is_dir = false;
        gitignore.content = "__pycache__/\n*.exe\n";
        dir.children.push_back(gitignore);

        FSNode buildBat;
        buildBat.name = "build.bat";
        buildBat.is_dir = false;
        buildBat.content = "@echo off\n"
                           "g++ -O3 -std=c++17 main.cpp -o TerminalLauncher.exe -luser32 -lshell32 -lgdi32 -ladvapi32 -mwindows -static\n";
        dir.children.push_back(buildBat);

        FSNode configJson;
        configJson.name = "config.json";
        configJson.is_dir = false;
        configJson.content = "{\n"
                             "    \"shortcuts\": {\n"
                             "        \"<alt>+<ctrl>+u\": \"wsl\",\n"
                             "        \"<alt>+<ctrl>+p\": \"powershell\",\n"
                             "        \"<alt>+<ctrl>+c\": \"cmd\",\n"
                             "        \"<alt>+<ctrl>+7\": \"pwsh\",\n"
                             "        \"<alt>+<ctrl>+/\": \"pwsh -NoExit -Command agy\",\n"
                             "        \"<alt>+<ctrl>+a\": \"pwsh -Verb RunAs\"\n"
                             "    }\n"
                             "}\n";
        dir.children.push_back(configJson);

        FSNode mainCpp;
        mainCpp.name = "main.cpp";
        mainCpp.is_dir = false;
        mainCpp.content = "#define WIN32_LEAN_AND_MEAN\n"
                          "#include <windows.h>\n"
                          "#include <shellapi.h>\n"
                          "#include <string>\n"
                          "#include <vector>\n\n"
                          "void LaunchCommand(std::string command) {\n"
                          "    // Parse and run program commands\n"
                          "}\n\n"
                          "int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {\n"
                          "    // Single instance mutex and notification icons setup\n"
                          "    return 0;\n"
                          "}";
        dir.children.push_back(mainCpp);

        FSNode addStartup;
        addStartup.name = "add_to_startup.py";
        addStartup.is_dir = false;
        addStartup.content = "import os, sys\n"
                             "import winshell\n"
                             "from win32com.client import Dispatch\n\n"
                             "def create_startup_shortcut():\n"
                             "    startup_path = winshell.startup()\n"
                             "    shortcut_path = os.path.join(startup_path, 'TerminalLauncher.lnk')\n"
                             "    shell = Dispatch('WScript.Shell')\n"
                             "    shortcut = shell.CreateShortCut(shortcut_path)\n"
                             "    shortcut.Targetpath = sys.executable.replace('python.exe', 'pythonw.exe')\n"
                             "    shortcut.WorkingDirectory = os.path.dirname(os.path.abspath(__file__))\n"
                             "    shortcut.save()\n\n"
                             "if __name__ == '__main__':\n"
                             "    create_startup_shortcut()\n";
        dir.children.push_back(addStartup);

        FSNode mainPy;
        mainPy.name = "main.py";
        mainPy.is_dir = false;
        mainPy.content = "import os, sys, threading, subprocess, json\n"
                         "import pystray, pynput\n"
                         "from PIL import Image, ImageDraw\n\n"
                         "class TerminalLauncher:\n"
                         "    def __init__(self):\n"
                         "        self.config = json.load(open('config.json'))\n\n"
                         "    def launch(self, command):\n"
                         "        subprocess.Popen(f'start {command}', shell=True)\n\n"
                         "    def run(self):\n"
                         "        pass\n\n"
                         "if __name__ == '__main__':\n"
                         "    TerminalLauncher().run()\n";
        dir.children.push_back(mainPy);

        FSNode readme;
        readme.name = "README_AUTO.md";
        readme.is_dir = false;
        readme.content = "# Terminal Launcher\n\n"
                         "A terminal launcher utility with configuration file support.\n\n"
                         "## Features\n"
                         "- Quick terminal launching\n"
                         "- Configuration-based setup\n"
                         "- Startup integration\n";
        dir.children.push_back(readme);

        FSNode runBat;
        runBat.name = "run.bat";
        runBat.is_dir = false;
        runBat.content = "@echo off\nstart \"\" \"%~dp0TerminalLauncher.exe\"\n";
        dir.children.push_back(runBat);

        FSNode launcherExe;
        launcherExe.name = "TerminalLauncher.exe";
        launcherExe.is_dir = false;
        launcherExe.content = "[Binary Executable Data - Win32 background dispatcher]";
        dir.children.push_back(launcherExe);

        projects.children.push_back(dir);
    }

    // lhcoyle4.github.io
    {
        FSNode dir;
        dir.name = "lhcoyle4.github.io";
        dir.is_dir = true;

        FSNode buildWasm;
        buildWasm.name = "build_wasm.ps1";
        buildWasm.is_dir = false;
        buildWasm.content = "# build_wasm.ps1\n"
                             "# Compiles the C++ ImGui codebase to WebAssembly using Emscripten\n"
                             "Write-Host \"LHCOYLE4 WASM BUILD PIPELINE\" -ForegroundColor Cyan\n"
                             ". \"..\\emsdk\\emsdk_env.ps1\"\n"
                             "cmd.exe /c \"em++ -Os src/main.cpp imgui/*.cpp -s USE_SDL=2 -o index.html\"\n";
        dir.children.push_back(buildWasm);

        FSNode genMap;
        genMap.name = "generate_map_data.py";
        genMap.is_dir = false;
        genMap.content = "import urllib.request, json\n\n"
                         "# Downloads USGS topographic data and state boundaries\n"
                         "# Generates coordinate arrays for states, contours, and energy grids\n"
                         "print('Downloading state contours...')\n"
                         "with open('src/map_data.h', 'w') as f:\n"
                         "    f.write('// Auto-generated vector layer coordinates\\n')\n";
        dir.children.push_back(genMap);

        FSNode indexHtml;
        indexHtml.name = "index.html";
        indexHtml.is_dir = false;
        indexHtml.content = "<!doctype html>\n<html>\n<head>\n    <title>LHCOYLE4 // Systems Core V3</title>\n"
                             "</head>\n<body>\n    <canvas id=\"canvas\"></canvas>\n"
                             "    <script async src=\"index.js\"></script>\n</body>\n</html>\n";
        dir.children.push_back(indexHtml);

        FSNode readme;
        readme.name = "README.md";
        readme.is_dir = false;
        readme.content = "# lhcoyle4.github.io (WASM Portfolio)\n\n"
                         "This repository hosts my personal homepage and developer portfolio, built entirely from scratch in C++ and compiled to WebAssembly (WASM).\n\n"
                         "## Engineering Architecture\n"
                         "- **Zero-DOM Rendering**: drawn using Dear ImGui immediate-mode and rendered directly to WebGL2 via shaders.\n"
                         "- **Low-Level Code**: pure C++17 compiling to highly optimized WASM bytecode.";
        dir.children.push_back(readme);

        // Nested directory src/
        FSNode srcDir;
        srcDir.name = "src";
        srcDir.is_dir = true;

        FSNode srcMain;
        srcMain.name = "main.cpp";
        srcMain.is_dir = false;
        srcMain.content = "#include <imgui.h>\n"
                           "#include <SDL.h>\n\n"
                           "void InitializeVirtualFS() {\n"
                           "    // Recursively populates file hierarchies for virtual console explore\n"
                           "}\n\n"
                           "int main(int argc, char* argv[]) {\n"
                           "    // Instantiates WebGL frame handlers, maps rendering loop\n"
                           "    return 0;\n"
                           "}";
        srcDir.children.push_back(srcMain);

        FSNode mapData;
        mapData.name = "map_data.h";
        mapData.is_dir = false;
        mapData.content = "// map_data.h (TRUNCATED SAMPLE FOR CONSOLE EXPLORER)\n"
                           "#pragma once\n"
                           "const float US_States_Lon[] = { -87.35930f, -85.60667f, -85.43141f };\n"
                           "const float US_States_Lat[] = { 34.0f, 35.0f, 36.0f };\n"
                           "// [Remaining 1.9MB of coordinate arrays omitted for workspace view...]\n";
        srcDir.children.push_back(mapData);

        FSNode shellHtml;
        shellHtml.name = "shell.html";
        shellHtml.is_dir = false;
        shellHtml.content = "<!doctype html>\n<html>\n<head>\n    <title>LHCOYLE4 Systems Core</title>\n"
                             "</head>\n<body>\n    <div id=\"loader\">Booting...</div>\n"
                             "    <canvas id=\"canvas\"></canvas>\n</body>\n</html>\n";
        srcDir.children.push_back(shellHtml);

        dir.children.push_back(srcDir);
        projects.children.push_back(dir);
    }

    g_FSRoot.children.push_back(projects);

    FSNode bio;
    bio.name = "bio.txt";
    bio.is_dir = false;
    bio.content = "LOUIE COYLE - SOFTWARE ENGINEER & GIS SPECIALIST\n"
                  "====================================================\n"
                  "Background: BSCS (Computer Science) + GIS Master's Certificate.\n"
                  "Location:   Portland, ME.\n"
                  "Focus:      Systems programming, automation, and spatial analysis.\n"
                  "Skills:     C, C++, Python, Javascript, Rust, Win32 API, OpenGL,\n"
                  "            QGIS, ArcGIS, LiDAR processing, Drone mapping.\n"
                  "Philosophy: Mechanics sympathy. Bypassing bloated abstractions\n"
                  "            to build responsive, zero-dependency tools.";
    g_FSRoot.children.push_back(bio);

    FSNode contact;
    contact.name = "contact.txt";
    contact.is_dir = false;
    contact.content = "CONTACT CHANNELS\n"
                      "================\n"
                      "GitHub:     https://github.com/lhcoyle4\n"
                      "Location:   Portland, Maine, USA\n"
                      "Telemetry:  ONLINE\n\n"
                      "Send an inquiry to inspect the grid or commission project work.";
    g_FSRoot.children.push_back(contact);

    FSNode skills;
    skills.name = "skills.txt";
    skills.is_dir = false;
    skills.content = "TECHNICAL SPECIALIZATION GRID\n"
                      "=============================\n"
                      "1. Systems: C, C++, Rust, x86 Assembly, Win32 API, POSIX\n"
                      "2. GIS:     GDAL/OGR, PDAL, QGIS, ArcGIS Pro, Python ArcPy, RTK GNSS\n"
                      "3. Graphics: OpenGL, WebGL, GLSL, Dear ImGui Canvas Shaders\n"
                      "4. Automation: Python, Bash scripting, PowerShell, CI/CD Actions";
    g_FSRoot.children.push_back(skills);
}

// Terminal commands execution logic
void ExecuteCommand(const std::string& cmdLine) {
    g_CmdHistory.push_back(cmdLine);
    g_HistoryPos = -1;

    std::vector<std::string> args;
    std::stringstream ss(cmdLine);
    std::string arg;
    while (ss >> arg) {
        args.push_back(arg);
    }

    if (args.empty()) return;

    std::string cmd = args[0];
    std::string lowerCmd = cmd;
    std::transform(lowerCmd.begin(), lowerCmd.end(), lowerCmd.begin(), ::tolower);

    if (lowerCmd == "help") {
        AddLog("Available Shell Commands:");
        AddLog("  ls [dir]        - List directory contents.");
        AddLog("  cd [dir]        - Change current directory.");
        AddLog("  cat [file]      - Print file contents.");
        AddLog("  grep [pat] [f]  - Search file for pattern.");
        AddLog("  vi [file]       - Open read-only file viewer (Esc or :q to exit).");
        AddLog("  clear / cls     - Clear console screen.");
        AddLog("  matrix          - Start movie-accurate code waterfall (Esc to exit).");
        AddLog("  neofetch        - Display system configuration summary.");
        AddLog("  about           - Biographical profile.");
        AddLog("  projects        - Switch to project directory tab.");
        AddLog("  gis             - Switch to GIS Cartography map tab.");
        AddLog("  permadrift      - Launch PERMADRIFT drift-ship game in browser.");
        AddLog("  starwars        - Run local Star Wars Episode IV ASCII movie (Esc to exit).");
    }
    else if (lowerCmd == "about") {
        AddLog("====================================================", ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
        AddLog("Louie Coyle - Software Engineer & GIS Specialist", ImVec4(1.0f, 0.8f, 0.0f, 1.0f));
        AddLog("----------------------------------------------------");
        AddLog("Background: BSCS (Computer Science) + GIS Master's Certificate.");
        AddLog("Location:   Portland, ME.");
        AddLog("Focus:      Systems programming, automation, and spatial analysis.");
        AddLog("Skills:     C, C++, Python, Javascript, Rust, Win32 API, OpenGL,");
        AddLog("            QGIS, ArcGIS, LiDAR processing, Drone mapping.");
        AddLog("Philosophy: Mechanics sympathy. Bypassing bloated abstractions");
        AddLog("            to build responsive, zero-dependency tools.");
        AddLog("====================================================", ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
    }
    else if (lowerCmd == "projects") {
        g_ActiveTab = 1;
        AddLog("Opening Project Directory tab...", ImVec4(0.9f, 0.9f, 0.0f, 1.0f));
        AddLog("Use the tabs or double-click items to view details.");
        for (const auto& proj : g_Projects) {
            AddLog(" * " + proj.name + " (" + proj.language + ") - " + proj.desc);
        }
    }
    else if (lowerCmd == "gis") {
        g_ActiveTab = 2;
        AddLog("Accessing GIS Cartography tab...", ImVec4(0.9f, 0.9f, 0.0f, 1.0f));
        AddLog("Loading raster grid and coordinates...");
    }
    else if (lowerCmd == "neofetch") {
        AddLog("                 .,-:;//;:=,             louie@lhcoyle4-core", ImVec4(0.0f, 1.0f, 0.5f, 1.0f));
        AddLog("             . :H@@@MM@M#H/.,+%;,        -------------------", ImVec4(0.0f, 1.0f, 0.5f, 1.0f));
        AddLog("          ,/X+ +M@@M@MM%=,-%HMMM@X/,     OS: WebAssembly (Emscripten)", ImVec4(0.0f, 1.0f, 0.5f, 1.0f));
        AddLog("        -+@MM;  .=- -,:=;;3MMMMMMMM@+,   Kernel: Dear ImGui WebGL Canvas", ImVec4(0.0f, 1.0f, 0.5f, 1.0f));
        AddLog("       %+MMM@H:;;;;:;;;;;;HM##M##M#M##X  Uptime: (See System Metrics)", ImVec4(0.0f, 1.0f, 0.5f, 1.0f));
        AddLog("      -@M###@H:;;;;;;;;;;;;:;;;;;;;;;;:, Shell: Dear ImGui immediate-mode UI", ImVec4(0.0f, 1.0f, 0.5f, 1.0f));
        AddLog("      %M###@H:;;;;;;;;;;;;;;;;;;;;;;;;;; CPU: Clang WASM Virtual Machine", ImVec4(0.0f, 1.0f, 0.5f, 1.0f));
        AddLog("      HM##M#;::;;;;;;;;;;;;;;;;;;;;;;;;; Memory: Heap Auto-Growing", ImVec4(0.0f, 1.0f, 0.5f, 1.0f));
        AddLog("      XMMMM@H:;;;;;;;;;;;;;;;;;;;;;;;=   Resolution: Responsive Canvas", ImVec4(0.0f, 1.0f, 0.5f, 1.0f));
        AddLog("      XMMMM@H:;;;;;;;;;;;;;;;;;;;;;;;;   Credentials: BSCS + GIS Master's Cert", ImVec4(0.0f, 1.0f, 0.5f, 1.0f));
        AddLog("      `@M@M#;::;;;;;;;;;;;;;;;;;;;;;;X   Specialties: Fiber/OSP, Drone mapping,", ImVec4(0.0f, 1.0f, 0.5f, 1.0f));
        AddLog("       `XMM@H:;;;;;;;;;;;;;;;;;;;;;;+                Python Automation, Win32", ImVec4(0.0f, 1.0f, 0.5f, 1.0f));
        AddLog("         `+HM@H;;;;;;;;;;;;;;;;;;;+`     GitHub: https://github.com/lhcoyle4", ImVec4(0.0f, 1.0f, 0.5f, 1.0f));
        AddLog("            `=HMMMMMMMMMMMMMM@H=`        ", ImVec4(0.0f, 1.0f, 0.5f, 1.0f));
        AddLog("                 -:=/;;//=:`             ", ImVec4(0.0f, 1.0f, 0.5f, 1.0f));
    }
    else if (lowerCmd == "matrix") {
        g_MatrixMode = true;
    }
    else if (lowerCmd == "starwars") {
        if (!g_StarWarsMovieLoaded) {
            AddLog("Star Wars ASCIIMATION is still loading... Please wait and try again in a few seconds.", ImVec4(1.0f, 0.8f, 0.0f, 1.0f));
            StartLoadingStarWarsMovie();
        } else {
            g_StarWarsMovieMode = true;
            g_StarWarsCurrentFrameIdx = 0;
            g_StarWarsFrameShowTime = ImGui::GetTime();
        }
    }
    else if (lowerCmd == "clear" || lowerCmd == "cls") {
        g_ConsoleLog.clear();
    }
    else if (lowerCmd == "ls") {
        std::string targetPath = "";
        if (args.size() > 1) {
            // Ignore flag arguments (like -lah or -l) and check for a target path
            if (args[1][0] == '-') {
                if (args.size() > 2) {
                    targetPath = args[2];
                }
            } else {
                targetPath = args[1];
            }
        }
        std::vector<std::string> dummyParts;
        FSNode* node = ResolvePath(targetPath, dummyParts);
        if (!node) {
            AddLog("ls: cannot access '" + targetPath + "': No such file or directory", ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        } else if (!node->is_dir) {
            std::string dateStr = GetCurrentDateString();
            AddLogLSEntry(node->name, false, node->content.size(), 0, dateStr);
        } else {
            // Find parent node for ..
            std::vector<std::string> parentParts = dummyParts;
            if (!parentParts.empty()) {
                parentParts.pop_back();
            }
            FSNode* parentNode = FindNodeFromParts(parentParts);

            // Calculate total blocks (4KB blocks)
            size_t totalBlocks = 0;
            totalBlocks += 8; // . and .. (4K each)
            for (const auto& child : node->children) {
                if (child.is_dir) {
                    totalBlocks += 4;
                } else {
                    totalBlocks += ((child.content.size() + 1023) / 1024) * 4;
                }
            }
            AddLog("total " + std::to_string(totalBlocks) + "K");

            std::string dateStr = GetCurrentDateString();

            // . entry
            AddLogLSEntry(".", true, 4096, CountSubdirs(*node), dateStr);

            // .. entry
            if (parentNode) {
                AddLogLSEntry("..", true, 4096, CountSubdirs(*parentNode), dateStr);
            } else {
                AddLogLSEntry("..", true, 4096, CountSubdirs(g_FSRoot), dateStr);
            }

            // Directory children
            for (const auto& child : node->children) {
                size_t subdirs = child.is_dir ? CountSubdirs(child) : 0;
                AddLogLSEntry(child.name, child.is_dir, child.content.size(), subdirs, dateStr);
            }
        }
    }
    else if (lowerCmd == "cd") {
        std::string target = "/";
        if (args.size() > 1) {
            target = args[1];
        }
        std::vector<std::string> outParts;
        FSNode* node = ResolvePath(target, outParts);
        if (!node) {
            AddLog("cd: no such file or directory: " + target, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        } else if (!node->is_dir) {
            AddLog("cd: not a directory: " + target, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        } else {
            g_CurrentDirParts = outParts;
        }
    }
    else if (lowerCmd == "cat") {
        if (args.size() < 2) {
            AddLog("cat: missing file operand", ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        } else {
            std::string filepath = args[1];
            std::vector<std::string> dummyParts;
            FSNode* node = ResolvePath(filepath, dummyParts);
            if (!node) {
                AddLog("cat: " + filepath + ": No such file or directory", ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            } else if (node->is_dir) {
                AddLog("cat: " + filepath + ": Is a directory", ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            } else {
                std::stringstream fileSs(node->content);
                std::string fileLine;
                while (std::getline(fileSs, fileLine)) {
                    AddLog(fileLine, ImVec4(0.0f, 1.0f, 0.3f, 1.0f));
                }
            }
        }
    }
    else if (lowerCmd == "grep") {
        if (args.size() < 3) {
            AddLog("Usage: grep [pattern] [file]", ImVec4(1.0f, 0.8f, 0.0f, 1.0f));
        } else {
            std::string pattern = args[1];
            std::string filepath = args[2];
            std::vector<std::string> dummyParts;
            FSNode* node = ResolvePath(filepath, dummyParts);
            if (!node) {
                AddLog("grep: " + filepath + ": No such file or directory", ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            } else if (node->is_dir) {
                AddLog("grep: " + filepath + ": Is a directory", ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            } else {
                std::string lowerPattern = pattern;
                std::transform(lowerPattern.begin(), lowerPattern.end(), lowerPattern.begin(), ::tolower);
                
                std::stringstream fileSs(node->content);
                std::string fileLine;
                int lineNum = 1;
                bool matchFound = false;
                while (std::getline(fileSs, fileLine)) {
                    std::string lowerLine = fileLine;
                    std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);
                    if (lowerLine.find(lowerPattern) != std::string::npos) {
                        std::string prefix = std::to_string(lineNum) + ": ";
                        AddLog(prefix + fileLine, ImVec4(0.0f, 1.0f, 0.3f, 1.0f));
                        matchFound = true;
                    }
                    lineNum++;
                }
                if (!matchFound) {
                    AddLog("No matches found for '" + pattern + "'", ImVec4(0.0f, 0.6f, 0.1f, 1.0f));
                }
            }
        }
    }
    else if (lowerCmd == "vi" || lowerCmd == "vim") {
        if (args.size() < 2) {
            AddLog("Usage: vi [file]", ImVec4(1.0f, 0.8f, 0.0f, 1.0f));
        } else {
            std::string filepath = args[1];
            std::vector<std::string> dummyParts;
            FSNode* node = ResolvePath(filepath, dummyParts);
            if (!node) {
                AddLog("vi: " + filepath + ": No such file or directory", ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            } else if (node->is_dir) {
                AddLog("vi: " + filepath + ": Is a directory", ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            } else {
                g_ViMode = true;
                g_ViFilename = node->name;
                g_ViContent = node->content;
                g_ViLines.clear();
                
                std::stringstream fileSs(node->content);
                std::string fileLine;
                while (std::getline(fileSs, fileLine)) {
                    g_ViLines.push_back(fileLine);
                }
                
                g_ViScrollLine = 0;
                g_ViCommandActive = false;
                g_ViSearchQuery = "";
                g_ViSearchMatchIdx = -1;
                g_ViScrollToLine = -1;
                strcpy(g_ViCmdInput, "");
                g_FocusViInput = false;
            }
        }
    }
    else if (lowerCmd == "permadrift") {
        AddLog("====================================================", ImVec4(0.35f, 0.8f, 1.0f, 1.0f));
        AddLog("  PERMADRIFT // DRIFTING AT THE WORLDS END",           ImVec4(0.35f, 0.8f, 1.0f, 1.0f));
        AddLog("----------------------------------------------------", ImVec4(0.35f, 0.8f, 1.0f, 1.0f));
        AddLog("  Pilot an Autodyne drift-ship through the Permasphere.");
        AddLog("  Collect relics. Survive the void. Achieve Chronicle.");
        AddLog("  [C] Triskelion Burst  [V] Phase Shift  [B] Nova Shell");
        AddLog("====================================================", ImVec4(0.35f, 0.8f, 1.0f, 1.0f));
        AddLog("Launching PERMADRIFT...", ImVec4(0.9f, 0.9f, 0.0f, 1.0f));
        LaunchPermadrift();
    }
    else {
        AddLog("Command not recognized: '" + cmd + "'. Type 'help' for available options.", ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
    }

    // Add clean separation spacing after command outputs (except screen control commands)
    if (lowerCmd != "clear" && lowerCmd != "cls" && lowerCmd != "matrix" && lowerCmd != "starwars") {
        AddLog("");
    }
}

// Helper to get Google AI Generative Search (SGE) Overview concise summary
std::string GetMockAIOverview(const std::string& name, const std::string& layer) {
    if (layer == "City" || layer == "USGS Station") {
        if (name.find("Seattle") != std::string::npos) {
            return "USGS Station NW-1 telemetry grid located in Seattle, WA. The station monitors local seismology, water level telemetry, and provides real-time atmospheric data. Historically, this area is key to the Puget Sound monitoring network. Latest logs show standard geological stability and nominal groundwater indicators.";
        }
        if (name.find("San Francisco") != std::string::npos) {
            return "USGS Station SW-2 located in San Francisco, CA. Main functions include monitoring San Andreas Fault crustal deformation, high-precision GPS telemetry, and localized seismic micro-tremors. Currently operating on high-bandwidth telemetry links. Status is active with normal strain telemetry.";
        }
        if (name.find("Los Angeles") != std::string::npos) {
            return "USGS Station SW-1 in Los Angeles, CA. Specializes in groundwater aquifer level sensing, tectonic plate boundary tracking along the southern fault segments, and localized micro-earthquake tracking. Status is nominal with standard water table parameters.";
        }
        if (name.find("Denver") != std::string::npos) {
            return "USGS Station CO-1 in Denver, CO. Monitors Rocky Mountain snowpack depth, river runoff telemetry, and regional seismic activity. Telemetry feeds directly into national hydrological forecast systems. Status is active with normal snowpack levels.";
        }
        if (name.find("Houston") != std::string::npos) {
            return "USGS Station S-1 in Houston, TX. Monitors Gulf coast tide warnings, water table subsidence due to groundwater withdrawal, and regional stormwater drainage telemetry. Currently operating on standby mode with standard coastal telemetry.";
        }
        if (name.find("Chicago") != std::string::npos) {
            return "USGS Station MW-1 in Chicago, IL. Monitors Lake Michigan water level sensors, local seismic activity, and regional river flow telemetry. Telemetry feeds directly into municipal water safety systems. Status is nominal.";
        }
        if (name.find("Miami") != std::string::npos) {
            return "USGS Station SE-1 in Miami, FL. Dedicated to monitoring Everglades rewilding hydrology, saltwater intrusion into aquifers, and coastal tide telemetry. Part of the South Florida ecosystem restoration network. Status is active.";
        }
        if (name.find("New York") != std::string::npos) {
            return "USGS Station E-1 in New York, NY. Specializes in estuary tide level monitoring, Hudson River salinity telemetry, and local fault line crustal stability tracking. Status is nominal with normal tide cycles.";
        }
        return "USGS Telemetry Station. Monitors local geological, hydrological, and seismic telemetry. Status: ACTIVE. Links verified.";
    }
    if (layer == "Power Plant" || layer == "Power Station") {
        return "The " + name + " Power Station is a vital utility node in the US energy infrastructure grid. It supports grid stability in the region, feeding electricity directly into high-voltage energy corridors. Recent telemetry confirms active status with nominal power output phase-matched to the regional grid.";
    }
    if (layer == "Substation") {
        return "The " + name + " Substation acts as a critical electrical transmission and distribution node in the regional high-voltage grid. It steps down high-voltage power from transmission corridors to local sub-transmission grids. Operating at standard phase and frequency, it features telemetry for remote monitoring and load balancing. Current load capacity is nominal.";
    }
    if (layer == "Highway" || layer == "Interstate" || layer == "US Highway") {
        return "Interstate " + name + " is a major arterial corridor facilitating logistics, passenger transport, and interstate commerce. Monitored by regional telemetry nodes for traffic flow and structural integrity. Part of the National Highway System, it intersects key power and energy transmission corridors across its route.";
    }
    if (layer == "Railway") {
        return "The " + name + " railway network is a heavy-rail freight corridor. It serves as a logistics backbone for transporting bulk commodities (including fuel for power stations). Connected to national railway signaling databases.";
    }
    if (layer == "Pipeline") {
        return "The " + name + " is a high-capacity energy transit pipeline. It transports critical resources across several state boundaries, supplying fuel to major power stations. Monitored continuously for pressure anomalies and flow velocity.";
    }
    if (layer == "Corridor" || layer == "Energy Corridor") {
        return "The " + name + " is a high-voltage electrical transmission corridor (HVAC/HVDC). It connects generation hubs to regional distribution substations, maintaining grid synchronization. Operating at nominal phase and thermal limits.";
    }
    if (layer == "Lake") {
        return "Lake " + name + " is a major freshwater body integrated into the regional hydrological and climate monitoring grid. Monitored by USGS water level and quality sensors for estuary management and flood mitigation.";
    }
    if (layer == "River") {
        return "The " + name + " is a key watercourse flowing through major agricultural and industrial zones. Serves as a primary drainage basin and cooling source for power stations. Flow rates and water quality are logged in real-time.";
    }
    if (layer == "State") {
        return "State administrative boundary for " + name + ". Displays state-level regulatory zones, environmental protection areas, and state-jurisdiction energy grids.";
    }
    return "AI Search Generative Overview for " + name + ". Part of the " + layer + " mapping layer database. Operating parameters are within normal design limits. For more information, consult the technical manual.";
}

void OpenSgeOverview(const std::string& name, const std::string& layer) {
    g_SgeEntityName = name;
    g_SgeEntityLayer = layer;
    g_SgeSummaryText = GetMockAIOverview(name, layer);
    g_SgeOpen = true;
}

// Helper function to handle tab completion
void HandleTabCompletion(ImGuiInputTextCallbackData* data) {
    std::string bufStr(data->Buf, data->BufTextLen);
    
    // Check if we are continuing a cycling completion
    if (g_TabCompleting && !g_TabMatches.empty() && bufStr == g_LastCompletedBuf) {
        g_TabMatchIdx = (g_TabMatchIdx + 1) % g_TabMatches.size();
        std::string match = g_TabMatches[g_TabMatchIdx];
        
        // Replace from the base index to the end of the buffer
        data->DeleteChars(g_TabBaseLen, data->BufTextLen - g_TabBaseLen);
        data->InsertChars(g_TabBaseLen, match.c_str());
        
        g_LastCompletedBuf = std::string(data->Buf, data->BufTextLen);
        return;
    }
    
    // Starting a new completion: reset variables
    g_TabCompleting = false;
    g_TabMatches.clear();
    g_TabMatchIdx = -1;
    
    // Find the word being completed (the word after the last space)
    int lastSpace = -1;
    for (int i = data->BufTextLen - 1; i >= 0; --i) {
        if (data->Buf[i] == ' ') {
            lastSpace = i;
            break;
        }
    }
    
    std::string prefix = "";
    int wordStart = 0;
    bool isCommand = false;
    
    if (lastSpace == -1) {
        // First word in buffer: completing a command name
        prefix = bufStr;
        wordStart = 0;
        isCommand = true;
    } else {
        // Subsequent word in buffer: completing a file/directory path
        prefix = bufStr.substr(lastSpace + 1);
        wordStart = lastSpace + 1;
        isCommand = false;
    }
    
    std::vector<std::string> candidates;
    
    if (isCommand) {
        // Command list
        std::vector<std::string> commands = {
            "help", "about", "projects", "gis", "neofetch", "matrix",
            "clear", "cls", "ls", "cd", "cat", "grep", "vi", "vim", "starwars"
        };
        for (const auto& cmd : commands) {
            if (cmd.size() >= prefix.size() && cmd.substr(0, prefix.size()) == prefix) {
                candidates.push_back(cmd);
            }
        }
    } else {
        // File or directory completion
        // Split prefix into folder path and prefix name (e.g. projects/alt -> projects/ and alt)
        size_t lastSlash = prefix.find_last_of('/');
        std::string dirPart = "";
        std::string filePrefix = prefix;
        if (lastSlash != std::string::npos) {
            dirPart = prefix.substr(0, lastSlash + 1);
            filePrefix = prefix.substr(lastSlash + 1);
        }
        
        // Resolve directory node path
        std::vector<std::string> dummyParts;
        FSNode* dirNode = ResolvePath(dirPart, dummyParts);
        if (dirNode && dirNode->is_dir) {
            for (const auto& child : dirNode->children) {
                if (child.name.size() >= filePrefix.size() && child.name.substr(0, filePrefix.size()) == filePrefix) {
                    std::string completion = dirPart + child.name;
                    if (child.is_dir) {
                        completion += "/";
                    }
                    candidates.push_back(completion);
                }
            }
        }
    }
    
    if (!candidates.empty()) {
        g_TabMatches = candidates;
        g_TabMatchIdx = 0;
        g_TabCompleting = true;
        g_TabBaseLen = wordStart;
        
        std::string match = g_TabMatches[0];
        data->DeleteChars(g_TabBaseLen, data->BufTextLen - g_TabBaseLen);
        data->InsertChars(g_TabBaseLen, match.c_str());
        
        g_LastCompletedBuf = std::string(data->Buf, data->BufTextLen);
    }
}

// Input callback to handle history and tab completion
int ConsoleInputCallback(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
        HandleTabCompletion(data);
    }
    else if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
        int prevHistoryPos = g_HistoryPos;
        if (data->EventKey == ImGuiKey_UpArrow) {
            if (g_HistoryPos == -1)
                g_HistoryPos = (int)g_CmdHistory.size() - 1;
            else if (g_HistoryPos > 0)
                g_HistoryPos--;
        }
        else if (data->EventKey == ImGuiKey_DownArrow) {
            if (g_HistoryPos != -1) {
                g_HistoryPos++;
                if (g_HistoryPos >= (int)g_CmdHistory.size())
                    g_HistoryPos = -1;
            }
        }

        // Apply history
        if (prevHistoryPos != g_HistoryPos) {
            const char* historyStr = (g_HistoryPos >= 0) ? g_CmdHistory[g_HistoryPos].c_str() : "";
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, historyStr);
        }
    }
    return 0;
}

// Update history plots — sample a new value every SAMPLE_INTERVAL seconds.
// Display values snap immediately to the new target; no per-frame lerp so
// bars stay perfectly still between updates.
void UpdateHistoryPlots() {
    static double s_SimTimer       = 0.0;
    static double s_LastSampleTime = -1.0; // -1 = not yet initialised
    constexpr double SAMPLE_INTERVAL = 5.0; // seconds between new data points

    double now = ImGui::GetTime();

    // On the very first call, align the sample clock to now so we don't
    // immediately fire a sample (which would cause a jump from the flat init values).
    if (s_LastSampleTime < 0.0) {
        s_LastSampleTime = now;
        // Seed display values so bars start settled.
        g_CpuSmooth = g_CpuTarget = 20.0f;
        g_RamSmooth = g_RamTarget = 45.0f;
        g_NetSmooth = g_NetTarget =  5.0f;
    }

    // ---- Sample new values once per interval, snap display immediately ----
    if (now - s_LastSampleTime >= SAMPLE_INTERVAL) {
        s_LastSampleTime += SAMPLE_INTERVAL; // fixed-step to avoid drift
        s_SimTimer       += SAMPLE_INTERVAL;

        float t = (float)s_SimTimer;
        // Pure sine — targets shift gradually, never jump
        g_CpuTarget = 18.0f + 12.0f * sinf(t * 0.4f);
        g_RamTarget = 45.0f +  3.0f * sinf(t * 0.15f);
        g_NetTarget =  5.0f + 20.0f * (sinf(t * 0.7f) > 0.6f
                                        ? sinf(t * 0.7f) * 2.5f : 0.3f);

        // Snap display values — bars don't move at all between updates
        g_CpuSmooth = g_CpuTarget;
        g_RamSmooth = g_RamTarget;
        g_NetSmooth = g_NetTarget;

        // Commit the new value as a history point
        g_CpuHistory.push_back(g_CpuSmooth);
        g_RamHistory.push_back(g_RamSmooth);
        g_NetworkHistory.push_back(g_NetSmooth);

        if (g_CpuHistory.size()     > 100) g_CpuHistory.erase(g_CpuHistory.begin());
        if (g_RamHistory.size()     > 100) g_RamHistory.erase(g_RamHistory.begin());
        if (g_NetworkHistory.size() > 100) g_NetworkHistory.erase(g_NetworkHistory.begin());
    }
    // No per-frame lerp — display values are only written on sample ticks above.
}

struct MatchRange {
    int start;
    int end;
    bool isActive;
};

// Print string with search query highlighted
void PrintWithSearchHighlight(const std::string& token, const ImVec4& syntaxColor, const ImVec4& searchColor, const std::vector<MatchRange>& lineMatches, int tokenStart) {
    if (lineMatches.empty()) {
        ImGui::TextColored(syntaxColor, "%s", token.c_str());
        ImGui::SameLine(0, 0);
        return;
    }

    int cur = tokenStart;
    int tokenEnd = tokenStart + (int)token.length();

    while (cur < tokenEnd) {
        bool insideMatch = false;
        bool activeMatch = false;
        int nextBoundary = tokenEnd;

        for (const auto& mr : lineMatches) {
            if (cur >= mr.start && cur < mr.end) {
                insideMatch = true;
                activeMatch = mr.isActive;
                nextBoundary = std::min(nextBoundary, mr.end);
                break;
            } else if (mr.start > cur) {
                nextBoundary = std::min(nextBoundary, mr.start);
            }
        }

        int len = nextBoundary - cur;
        std::string sub = token.substr(cur - tokenStart, len);

        if (insideMatch) {
            ImVec2 screenPos = ImGui::GetCursorScreenPos();
            ImVec2 textSize = ImGui::CalcTextSize(sub.c_str());
            if (activeMatch) {
                // Draw active match (solid orange background, white text)
                ImGui::GetWindowDrawList()->AddRectFilled(screenPos, ImVec2(screenPos.x + textSize.x, screenPos.y + textSize.y), IM_COL32(230, 90, 0, 255));
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", sub.c_str());
            } else {
                // Draw normal match (semi-transparent green background, default search text color)
                ImGui::GetWindowDrawList()->AddRectFilled(screenPos, ImVec2(screenPos.x + textSize.x, screenPos.y + textSize.y), IM_COL32(0, 180, 50, 80));
                ImGui::TextColored(searchColor, "%s", sub.c_str());
            }
        } else {
            ImGui::TextColored(syntaxColor, "%s", sub.c_str());
        }
        ImGui::SameLine(0, 0);

        cur = nextBoundary;
    }
}

// Render highlighted line based on file extension
void RenderHighlightedLine(const std::string& line, const std::string& filename, const std::string& searchQuery, int lineIdx = -1) {
    std::vector<MatchRange> lineMatches;
    if (!searchQuery.empty()) {
        std::string lowerLine = line;
        std::string lowerQuery = searchQuery;
        std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);
        std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);
        size_t pos = 0;
        while ((pos = lowerLine.find(lowerQuery, pos)) != std::string::npos) {
            MatchRange mr;
            mr.start = (int)pos;
            mr.end = (int)(pos + searchQuery.length());
            mr.isActive = false;
            if (g_ViMode && lineIdx >= 0 && g_ViActiveMatchIdx >= 0 && g_ViActiveMatchIdx < (int)g_ViSearchMatches.size()) {
                const auto& am = g_ViSearchMatches[g_ViActiveMatchIdx];
                if (am.line_idx == lineIdx && am.char_pos == (int)pos) {
                    mr.isActive = true;
                }
            }
            lineMatches.push_back(mr);
            pos += searchQuery.length();
        }
    }

    if (line.empty()) {
        ImGui::TextUnformatted("");
        return;
    }

    std::string ext = "";
    size_t dotIdx = filename.find_last_of('.');
    if (dotIdx != std::string::npos) ext = filename.substr(dotIdx + 1);

    ImVec4 colNormal = ImVec4(0.0f, 1.0f, 0.3f, 1.0f); // Matrix Green
    ImVec4 colComment = ImVec4(0.35f, 0.65f, 0.35f, 1.0f);
    ImVec4 colKeyword = ImVec4(0.0f, 0.7f, 1.0f, 1.0f);
    ImVec4 colString = ImVec4(0.9f, 0.6f, 0.3f, 1.0f);
    ImVec4 colNumber = ImVec4(0.8f, 0.5f, 0.9f, 1.0f);
    ImVec4 colSearch = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); // Yellow

    if (ext == "md" || ext == "txt") {
        if (line[0] == '#') {
            PrintWithSearchHighlight(line, colKeyword, colSearch, lineMatches, 0);
            ImGui::TextUnformatted("");
            return;
        }
        PrintWithSearchHighlight(line, colNormal, colSearch, lineMatches, 0);
        ImGui::TextUnformatted("");
        return;
    }

    int i = 0;
    int len = (int)line.length();
    size_t commentPos = line.find("//");
    if (ext == "json") commentPos = std::string::npos;

    while (i < len) {
        if (commentPos != std::string::npos && (size_t)i == commentPos) {
            std::string commentStr = line.substr(i);
            PrintWithSearchHighlight(commentStr, colComment, colSearch, lineMatches, i);
            break;
        }

        char c = line[i];

        if (c == '"' || c == '\'') {
            char quoteChar = c;
            std::string strLit = "";
            strLit += c;
            int startPos = i;
            i++;
            while (i < len) {
                strLit += line[i];
                if (line[i] == quoteChar && line[i - 1] != '\\') {
                    i++;
                    break;
                }
                i++;
            }
            PrintWithSearchHighlight(strLit, colString, colSearch, lineMatches, startPos);
            continue;
        }

        if (c == '#') {
            std::string preproc = "";
            int startPos = i;
            while (i < len && !isspace(line[i]) && line[i] != '<' && line[i] != '"') {
                preproc += line[i];
                i++;
            }
            PrintWithSearchHighlight(preproc, colNumber, colSearch, lineMatches, startPos);
            continue;
        }

        if (isdigit(c)) {
            std::string num = "";
            int startPos = i;
            while (i < len && (isdigit(line[i]) || line[i] == '.' || line[i] == 'f' || line[i] == 'x')) {
                num += line[i];
                i++;
            }
            PrintWithSearchHighlight(num, colNumber, colSearch, lineMatches, startPos);
            continue;
        }

        if (isalpha(c) || c == '_') {
            std::string ident = "";
            int startPos = i;
            while (i < len && (isalnum(line[i]) || line[i] == '_')) {
                ident += line[i];
                i++;
            }

            bool isKw = false;
            static const std::vector<std::string> keywords = {
                "class", "struct", "void", "int", "float", "double", "bool", "if", "else", 
                "while", "for", "return", "public", "private", "static", "using", "namespace", 
                "const", "new", "delete", "true", "false", "null", "function", "var", "let", 
                "import", "export", "from", "char", "unsigned", "define", "include"
            };
            if (std::find(keywords.begin(), keywords.end(), ident) != keywords.end()) {
                isKw = true;
            }

            PrintWithSearchHighlight(ident, isKw ? colKeyword : colNormal, colSearch, lineMatches, startPos);
            continue;
        }

        std::string punc = "";
        punc += c;
        PrintWithSearchHighlight(punc, colNormal, colSearch, lineMatches, i);
        i++;
    }
    ImGui::TextUnformatted("");
}

// Fullscreen-accurate Matrix falling code visualizer
void UpdateAndRenderMatrix(ImDrawList* drawList, ImVec2 canvasPos, ImVec2 canvasSize) {
    int colWidth = 14;
    int numCols = (int)canvasSize.x / colWidth;
    if (numCols < 1) numCols = 1;

    if (s_MatrixColumns.size() != (size_t)numCols) {
        s_MatrixColumns.resize(numCols);
        for (int i = 0; i < numCols; ++i) {
            s_MatrixColumns[i].active = false;
            s_MatrixColumns[i].y = -(rand() % 400 + 50);
            s_MatrixColumns[i].speed = 120.0f + (rand() % 200);
            s_MatrixColumns[i].length = 8 + (rand() % 18);
            s_MatrixColumns[i].nextChangeTime = 0.0f;
            s_MatrixColumns[i].spawnDelay = (rand() % 100) / 20.0f;
            
            s_MatrixColumns[i].chars.resize(s_MatrixColumns[i].length);
            std::string matrixChars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ$#@%&*()[]{}";
            for (int k = 0; k < s_MatrixColumns[i].length; ++k) {
                s_MatrixColumns[i].chars[k] = matrixChars[rand() % matrixChars.length()];
            }
        }
    }

    drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(0, 0, 0, 255));

    float deltaTime = ImGui::GetIO().DeltaTime;
    if (deltaTime > 0.1f) deltaTime = 0.1f;

    std::string matrixChars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ$#@%&*()[]{}";
    float glyphHeight = 15.0f;

    for (int i = 0; i < numCols; ++i) {
        auto& col = s_MatrixColumns[i];
        
        if (col.spawnDelay > 0.0f) {
            col.spawnDelay -= deltaTime;
            continue;
        }

        col.y += col.speed * deltaTime;

        col.nextChangeTime -= deltaTime;
        if (col.nextChangeTime <= 0.0f) {
            col.nextChangeTime = 0.05f + (rand() % 10) / 100.0f;
            int numChanges = 1 + rand() % 3;
            for (int n = 0; n < numChanges; ++n) {
                int idx = rand() % col.length;
                col.chars[idx] = matrixChars[rand() % matrixChars.length()];
            }
        }

        float x = canvasPos.x + i * colWidth + 2.0f;
        for (int k = 0; k < col.length; ++k) {
            float y = col.y - k * glyphHeight;
            if (y < canvasPos.y - glyphHeight || y > canvasPos.y + canvasSize.y) {
                continue;
            }

            char charBuf[2] = { col.chars[k], '\0' };
            ImU32 color;
            if (k == 0) {
                color = IM_COL32(210, 255, 210, 255); // Head (White-green)
            } else {
                float fade = (float)(col.length - k) / (float)col.length;
                int alpha = (int)(255.0f * fade);
                int green = (int)(255.0f * fade);
                int red_blue = (int)(50.0f * fade);
                color = IM_COL32(red_blue, green, red_blue, alpha);
            }

            drawList->AddText(ImGui::GetFont(), glyphHeight, ImVec2(x, y), color, charBuf);
        }

        if (col.y - col.length * glyphHeight > canvasSize.y) {
            col.y = -glyphHeight;
            col.speed = 120.0f + (rand() % 200);
            col.length = 8 + (rand() % 18);
            col.chars.resize(col.length);
            for (int k = 0; k < col.length; ++k) {
                col.chars[k] = matrixChars[rand() % matrixChars.length()];
            }
            col.spawnDelay = (rand() % 50) / 20.0f;
        }
    }
}

int main(int, char**)
{
    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return 1;
    }

    // Decide GL+GLSL versions (WebGL 2.0)
    const char* glsl_version = "#version 300 es";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    
    // Launching viewport size
    int window_width = 1200;
    int window_height = 780;
    SDL_Window* window = SDL_CreateWindow("Louie Coyle WASM Portfolio", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, window_width, window_height, window_flags);
    if (window == nullptr)
    {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return 1;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (gl_context == nullptr)
    {
        printf("Error: SDL_GL_CreateContext(): %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls

    // Setup Dear ImGui style
    SetupRetroStyle();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Disable ini files since we are running in browser sandbox
    io.IniFilename = nullptr;

    // Load initial structures
    InitializeProjects();
    InitializeMapData();
    InitializeVirtualFS();
    StartLoadingStarWarsMovie();
    
    // Set up radar pins (coordinates in Portland, ME region)
    g_RadarPins.push_back(ImVec2(100, 100)); // Portland Downtown
    g_RadarPins.push_back(ImVec2(140, 70));  // Casco Bay
    g_RadarPins.push_back(ImVec2(80, 120));  // Fore River
    g_RadarPins.push_back(ImVec2(50, 60));   // Back Cove

    // Initial console log
    AddLog("==================================================================", ImVec4(0.0f, 0.8f, 0.2f, 1.0f));
    AddLog(" LHCOYLE4 CORE ENGINE v3.5.2 (WASM) INITIATED...", ImVec4(0.0f, 1.0f, 0.5f, 1.0f));
    AddLog(" COMPILING INTERFACES & OpenGL / WebGL3 SHADERS... OK", ImVec4(0.0f, 0.8f, 0.2f, 1.0f));
    AddLog(" SCANNING FOR ACTIVE PROJECTS... 5 IDENTIFIED.", ImVec4(0.0f, 0.8f, 0.2f, 1.0f));
    AddLog(" TYPE 'help' FOR LIST OF SYSTEM COMMANDS OR USE TABS TO BROWSE.", ImVec4(1.0f, 0.8f, 0.0f, 1.0f));
    AddLog("==================================================================", ImVec4(0.0f, 0.8f, 0.2f, 1.0f));
    AddLog("");

    // Initialize mock statistics arrays
    for (int i = 0; i < 100; ++i) {
        g_CpuHistory.push_back(10.0f);
        g_RamHistory.push_back(45.0f);
        g_NetworkHistory.push_back(1.0f);
    }

    ImVec4 clear_color = ImVec4(0.01f, 0.02f, 0.01f, 1.00f);
    bool show_demo_window = false;

    // Main loop
    bool done = false;
    EMSCRIPTEN_MAINLOOP_BEGIN
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        // Handle window resizing to fit full screen inside browser canvas
        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        io.DisplaySize = ImVec2((float)w, (float)h);

        // Update calculations
        UpdateHistoryPlots();

        // Start frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // Switch tabs with Ctrl+1 to Ctrl+5 or F1-F5 keys
        if (io.KeyCtrl) {
            if (ImGui::IsKeyPressed(ImGuiKey_1)) { g_ActiveTab = 0; g_FocusTerminalInput = true; }
            else if (ImGui::IsKeyPressed(ImGuiKey_2)) { g_ActiveTab = 1; }
            else if (ImGui::IsKeyPressed(ImGuiKey_3)) { g_ActiveTab = 2; }
            else if (ImGui::IsKeyPressed(ImGuiKey_4)) { g_ActiveTab = 3; }
            else if (ImGui::IsKeyPressed(ImGuiKey_5)) { g_ActiveTab = 4; }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F1)) { g_ActiveTab = 0; g_FocusTerminalInput = true; }
        else if (ImGui::IsKeyPressed(ImGuiKey_F2)) { g_ActiveTab = 1; }
        else if (ImGui::IsKeyPressed(ImGuiKey_F3)) { g_ActiveTab = 2; }
        else if (ImGui::IsKeyPressed(ImGuiKey_F4)) { g_ActiveTab = 3; }
        else if (ImGui::IsKeyPressed(ImGuiKey_F5)) { g_ActiveTab = 4; }

        // RENDER CENTRAL PORTFOLIO WINDOW (Locks to browser size)
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | 
                                      ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoBringToFrontOnFocus;

        ImGui::Begin("Louie Coyle Portfolio Core", nullptr, windowFlags);

        // Top Menu Bar
        if (ImGui::BeginMenuBar()) {
            ImGui::Text("[LHCOYLE4 SYSTEMS CORE] | ");
            ImGui::TextDisabled("STATUS: NOMINAL | ");
            
            // Render dynamic Uptime
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - g_StartTime).count();
            int mins = elapsed / 60;
            int secs = elapsed % 60;
            ImGui::Text("UPTIME: %02d:%02d | ", mins, secs);

            ImGui::Text("FPS: %.1f | ", io.Framerate);

            // Optional ImGui demo trigger
            ImGui::Separator();
            if (ImGui::Button("Launch Official ImGui Demo")) {
                show_demo_window = !show_demo_window;
            }
            ImGui::EndMenuBar();
        }

        // Layout: Left Panel (Navigation & Stats) and Right Panel (Workspace)
        float leftPanelWidth = 260.0f;
        float rightPanelWidth = io.DisplaySize.x - leftPanelWidth - 25.0f;
        if (rightPanelWidth < 300.0f) rightPanelWidth = 300.0f; // bounds checks

        // Left Panel (System Info & Menu Selection)
        ImGui::BeginChild("LeftPanel", ImVec2(leftPanelWidth, 0), true);

        // ---- Mini MOTD ----
        // Time-aware greeting + rotating quote, styled like the desktop MOTD app
        {
            time_t rawtime = time(nullptr);
            struct tm* ti = localtime(&rawtime);
            int hour = ti ? ti->tm_hour : 12;
            const char* greeting = (hour < 12) ? "Good Morning" : (hour < 18) ? "Good Afternoon" : "Good Evening";
            char dateStr[48];
            if (ti) strftime(dateStr, sizeof(dateStr), "%a %b %d  %H:%M", ti);
            else     snprintf(dateStr, sizeof(dateStr), "-- --- --  --:--");

            ImGui::TextColored(ImVec4(0.0f, 0.9f, 1.0f, 1.0f), "[ LHCOYLE4 MOTD ]");
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.45f, 1.0f), "%s, Louie!", greeting);
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.35f, 0.65f, 0.35f, 1.0f), " %s", dateStr);

            ImGui::Spacing();

            // Rotating quote — advances every 30 seconds
            static int  s_QuoteIdx      = 0;
            static double s_LastQuoteT  = -1.0;
            int numTips = (int)(sizeof(g_DevTips) / sizeof(g_DevTips[0]));
            double now = ImGui::GetTime();
            if (s_LastQuoteT < 0.0) { s_LastQuoteT = now; s_QuoteIdx = 0; }
            if (now - s_LastQuoteT >= 30.0) {
                s_LastQuoteT += 30.0;
                s_QuoteIdx = (s_QuoteIdx + 1) % numTips;
            }
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.55f, 1.0f), "\"%s\"", g_DevTips[s_QuoteIdx]);
            ImGui::PopTextWrapPos();
        }
        
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "NAV COMMAND CENTER");
        ImGui::Separator();
        
        // Stark navigation menu buttons
        const char* menuOptions[] = {
            " [Ctrl+1] TERMINAL CONSOLE",
            " [Ctrl+2] PROJECT DIRECTORY",
            " [Ctrl+3] GIS CARTOGRAPHY",
            " [Ctrl+4] PORTFOLIO MOTD",
            " [Ctrl+5] INTRO & CREDITS"
        };
        for (int i = 0; i < 5; ++i) {
            bool selected = (g_ActiveTab == i);
            if (selected) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.5f, 0.1f, 1.0f));
            }
            if (ImGui::Button(menuOptions[i], ImVec2(-FLT_MIN, 35.0f))) {
                g_ActiveTab = i;
            }
            if (selected) {
                ImGui::PopStyleColor();
            }
            ImGui::Spacing();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextWrapped("Bio: BSCS + GIS Master's Cert. Portland, ME. Aspiring drone pilot, Python automations, C/C++ programmer.");

        ImGui::EndChild();

        ImGui::SameLine();

        // Right Panel (Workspace)
        ImGui::BeginChild("RightPanel", ImVec2(rightPanelWidth, 0), true);

        if (g_ActiveTab == 0) {
            // ==========================================
            // TAB 0: TERMINAL CONSOLE
            // ==========================================
            if (g_StarWarsMovieMode) {
                ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.0f, 1.0f), "STAR WARS EPISODE IV: A NEW HOPE (ASCIIMATION). PRESS ESC TO EXIT.");
                ImGui::Separator();

                if (!g_StarWarsFrames.empty()) {
                    double now = ImGui::GetTime();
                    double elapsed = now - g_StarWarsFrameShowTime;
                    const auto& currentFrame = g_StarWarsFrames[g_StarWarsCurrentFrameIdx];
                    double frameDuration = currentFrame.delay * 0.067;

                    while (elapsed >= frameDuration) {
                        g_StarWarsCurrentFrameIdx++;
                        if (g_StarWarsCurrentFrameIdx >= (int)g_StarWarsFrames.size()) {
                            g_StarWarsCurrentFrameIdx = 0;
                        }
                        g_StarWarsFrameShowTime += frameDuration;

                        const auto& nextFrame = g_StarWarsFrames[g_StarWarsCurrentFrameIdx];
                        frameDuration = nextFrame.delay * 0.067;
                        elapsed = now - g_StarWarsFrameShowTime;
                    }

                    ImGui::BeginChild("StarWarsMovieCanvas", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                    
                    const auto& frame = g_StarWarsFrames[g_StarWarsCurrentFrameIdx];
                    float maxLineWidth = 0.0f;
                    for (int j = 0; j < 13; ++j) {
                        float w = ImGui::CalcTextSize(frame.lines[j].c_str()).x;
                        if (w > maxLineWidth) maxLineWidth = w;
                    }
                    float textHeight = ImGui::GetTextLineHeightWithSpacing() * 13;
                    ImVec2 availSize = ImGui::GetContentRegionAvail();

                    float startX = (availSize.x - maxLineWidth) * 0.5f;
                    float startY = (availSize.y - textHeight) * 0.5f;
                    if (startX < 0.0f) startX = 0.0f;
                    if (startY < 0.0f) startY = 0.0f;

                    ImGui::SetCursorPos(ImVec2(startX, startY));
                    ImGui::BeginGroup();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
                    for (int j = 0; j < 13; ++j) {
                        if (frame.lines[j].empty()) {
                            ImGui::NewLine();
                        } else {
                            ImGui::TextUnformatted(frame.lines[j].c_str());
                        }
                    }
                    ImGui::PopStyleColor();
                    ImGui::EndGroup();
                    
                    ImGui::EndChild();
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error: No movie frames loaded.");
                }

                if (ImGui::IsKeyPressed(ImGuiKey_Escape) || 
                    ((ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl)) && ImGui::IsKeyPressed(ImGuiKey_C))) {
                    g_StarWarsMovieMode = false;
                    AddLog("Stopped Star Wars ASCIIMATION.", ImVec4(0.0f, 1.0f, 0.3f, 1.0f));
                    g_FocusTerminalInput = true;
                }
            } else if (g_MatrixMode) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.3f, 1.0f), "MATRIX CODE WATERFALL ACTIVE. PRESS ESC OR CTRL+C TO TERMINATE.");
                ImGui::Separator();
                
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                ImVec2 canvasPos = ImGui::GetCursorScreenPos();
                ImVec2 canvasSize = ImGui::GetContentRegionAvail();
                if (canvasSize.y < 100.0f) canvasSize.y = 100.0f;
                
                UpdateAndRenderMatrix(drawList, canvasPos, canvasSize);
                ImGui::Dummy(canvasSize);

                if (ImGui::IsKeyPressed(ImGuiKey_Escape) || 
                    ((ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl)) && ImGui::IsKeyPressed(ImGuiKey_C))) {
                    g_MatrixMode = false;
                    AddLog("^C", ImVec4(0.0f, 1.0f, 0.3f, 1.0f));
                    g_FocusTerminalInput = true;
                }
            } else if (g_ViMode) {
                // Vi Reader View
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "~ [VI] - %s [READ-ONLY] - %d lines ~", g_ViFilename.c_str(), (int)g_ViLines.size());
                ImGui::Separator();
                
                float statusHeight = ImGui::GetFrameHeightWithSpacing() + 10.0f;
                ImGui::BeginChild("ViScrollingContent", ImVec2(0, -statusHeight), false, ImGuiWindowFlags_HorizontalScrollbar);
                
                if (g_ViScrollToLine >= 0) {
                    float line_height = ImGui::GetTextLineHeightWithSpacing();
                    ImGui::SetScrollY(g_ViScrollToLine * line_height);
                    g_ViScrollToLine = -1;
                }

                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));
                for (size_t i = 0; i < g_ViLines.size(); ++i) {
                    ImGui::TextColored(ImVec4(0.3f, 0.6f, 0.3f, 1.0f), "%4d │ ", (int)i + 1);
                    ImGui::SameLine();
                    RenderHighlightedLine(g_ViLines[i], g_ViFilename, g_ViSearchQuery, (int)i);
                }
                ImGui::PopStyleVar();
                ImGui::EndChild();
                
                ImGui::Separator();
                
                if (g_ViCommandActive) {
                    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.3f, 1.0f), "%c", g_ViCommandChar);
                    ImGui::SameLine();
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
                    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
                    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                    ImGui::PushItemWidth(-FLT_MIN);
                    
                    if (g_FocusViInput) {
                        ImGui::SetKeyboardFocusHere(0);
                        g_FocusViInput = false;
                    }
                    
                    if (ImGui::InputText("##ViCommandInput", g_ViCmdInput, IM_ARRAYSIZE(g_ViCmdInput), ImGuiInputTextFlags_EnterReturnsTrue)) {
                        std::string cmdStr(g_ViCmdInput);
                        if (g_ViCommandChar == ':') {
                            if (cmdStr == "q" || cmdStr == "q!") {
                                g_ViMode = false;
                                AddLog("Closed vi viewer.", ImVec4(0.0f, 1.0f, 0.3f, 1.0f));
                                g_FocusTerminalInput = true;
                            }
                        } else if (g_ViCommandChar == '/') {
                            g_ViSearchQuery = cmdStr;
                            g_ViSearchMatches.clear();
                            g_ViActiveMatchIdx = -1;
                            if (!g_ViSearchQuery.empty()) {
                                std::string lowerQuery = g_ViSearchQuery;
                                std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);
                                for (size_t i = 0; i < g_ViLines.size(); ++i) {
                                    std::string lowerLine = g_ViLines[i];
                                    std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);
                                    size_t pos = 0;
                                    while ((pos = lowerLine.find(lowerQuery, pos)) != std::string::npos) {
                                        g_ViSearchMatches.push_back({ (int)i, (int)pos });
                                        pos += lowerQuery.length();
                                    }
                                }
                                if (!g_ViSearchMatches.empty()) {
                                    g_ViActiveMatchIdx = 0;
                                    g_ViScrollToLine = g_ViSearchMatches[0].line_idx;
                                }
                            }
                        }
                        strcpy(g_ViCmdInput, "");
                        g_ViCommandActive = false;
                    }
                    
                    ImGui::PopItemWidth();
                    ImGui::PopStyleVar();
                    ImGui::PopStyleColor(3);
                } else {
                    if (g_ViSearchQuery.empty()) {
                        ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.2f, 1.0f), "\":q\" to quit | \"/\" to search");
                    } else {
                        if (g_ViSearchMatches.empty()) {
                            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Pattern not found: %s", g_ViSearchQuery.c_str());
                        } else {
                            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Match %d of %d | \":q\" to quit | \"/\" to search | \"n\"/\"N\" to cycle", 
                                               g_ViActiveMatchIdx + 1, (int)g_ViSearchMatches.size());
                        }
                    }
                    
                    // Robust keypress and char queue check to support all layouts and focus states
                    bool colonPressed = false;
                    bool slashPressed = false;
                    bool nPressed = false;
                    bool NPressed = false;

                    if (ImGui::IsKeyPressed(ImGuiKey_Slash)) slashPressed = true;
                    if (ImGui::IsKeyPressed(ImGuiKey_Semicolon) && io.KeyShift) colonPressed = true;
                    if (ImGui::IsKeyPressed(ImGuiKey_N)) {
                        if (io.KeyShift) NPressed = true;
                        else nPressed = true;
                    }

                    for (int i = 0; i < io.InputQueueCharacters.Size; ++i) {
                        ImWchar c = io.InputQueueCharacters[i];
                        if (c == ':') colonPressed = true;
                        if (c == '/') slashPressed = true;
                        if (c == 'n') nPressed = true;
                        if (c == 'N') NPressed = true;
                    }

                    if (colonPressed) {
                        g_ViCommandActive = true;
                        g_ViCommandChar = ':';
                        g_FocusViInput = true;
                        strcpy(g_ViCmdInput, "");
                    } else if (slashPressed) {
                        g_ViCommandActive = true;
                        g_ViCommandChar = '/';
                        g_FocusViInput = true;
                        strcpy(g_ViCmdInput, "");
                    } else if (nPressed) {
                        if (!g_ViSearchMatches.empty()) {
                            g_ViActiveMatchIdx = (g_ViActiveMatchIdx + 1) % g_ViSearchMatches.size();
                            g_ViScrollToLine = g_ViSearchMatches[g_ViActiveMatchIdx].line_idx;
                        }
                    } else if (NPressed) {
                        if (!g_ViSearchMatches.empty()) {
                            g_ViActiveMatchIdx = (g_ViActiveMatchIdx - 1 + (int)g_ViSearchMatches.size()) % g_ViSearchMatches.size();
                            g_ViScrollToLine = g_ViSearchMatches[g_ViActiveMatchIdx].line_idx;
                        }
                    }

                    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                        g_ViMode = false;
                        AddLog("Closed vi viewer.", ImVec4(0.0f, 1.0f, 0.3f, 1.0f));
                        g_FocusTerminalInput = true;
                    }
                }
            } else {
                // Render standard inline terminal console
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "SYSTEM TERMINAL SHELL (Type 'help' for instructions)");
                ImGui::Separator();
                
                ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
                
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 5));
                for (const auto& item : g_ConsoleLog) {
                    if (item.segments.empty()) {
                        ImGui::TextColored(item.color, "%s", item.text.c_str());
                    } else {
                        for (size_t s = 0; s < item.segments.size(); ++s) {
                            ImGui::TextColored(item.segments[s].color, "%s", item.segments[s].text.c_str());
                            if (s + 1 < item.segments.size()) {
                                ImGui::SameLine(0, 0);
                            }
                        }
                    }
                }
                
                // Inline command input prompt with separate color-coded segments
                ImGui::TextColored(ImVec4(0.18f, 0.80f, 0.44f, 1.00f), "louie@lhcoyle4-core");
                ImGui::SameLine(0, 0);
                ImGui::TextColored(ImVec4(0.90f, 0.90f, 0.90f, 1.00f), ":");
                ImGui::SameLine(0, 0);
                std::string pathStr = GetCurrentPathString();
                ImGui::TextColored(ImVec4(0.20f, 0.60f, 0.86f, 1.00f), "%s", pathStr.c_str());
                ImGui::SameLine(0, 0);
                ImGui::TextColored(ImVec4(0.90f, 0.90f, 0.90f, 1.00f), "$ ");
                ImGui::SameLine();
                
                ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackCompletion;
                
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_BorderShadow, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_NavHighlight, ImVec4(0, 0, 0, 0));
                
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
                ImGui::PushItemWidth(-FLT_MIN);
                
                if (g_FocusTerminalInput) {
                    ImGui::SetKeyboardFocusHere(0);
                    g_FocusTerminalInput = false;
                }
                
                if (ImGui::InputText("##InlineInput", g_InputBuf, IM_ARRAYSIZE(g_InputBuf), inputFlags, &ConsoleInputCallback)) {
                    std::string inputStr(g_InputBuf);
                    
                    // Echo the prompt and typed command to terminal log with full color-coding
                    AddLogPrompt(GetCurrentPathString(), inputStr);
                    
                    if (!inputStr.empty()) {
                        ExecuteCommand(inputStr);
                    }
                    
                    strcpy(g_InputBuf, "");
                    g_FocusTerminalInput = true;
                    g_TerminalScrollToBottom = true;
                }
                
                ImGui::PopItemWidth();
                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(6);
                
                if (g_TerminalScrollToBottom) {
                    ImGui::SetScrollHereY(1.0f);
                    g_TerminalScrollToBottom = false;
                }
                
                // Redirect keyboard focus if user clicks or inputs key on the terminal window
                if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    g_FocusTerminalInput = true;
                }
                
                ImGui::PopStyleVar();
                ImGui::EndChild();
            }
        }
        else if (g_ActiveTab == 1) {
            // ==========================================
            // TAB 1: PROJECT DIRECTORY
            // ==========================================
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "DIRECTORY OF VERIFIED CODEBASES");
            ImGui::Separator();

            static int selectedProj = 0;

            // Split directory layout: left item listing, right detailed specifications
            ImGui::BeginChild("ProjectList", ImVec2(240, 0), true);
            for (size_t i = 0; i < g_Projects.size(); ++i) {
                bool selected = (selectedProj == (int)i);
                std::string label = g_Projects[i].name + " (" + g_Projects[i].language + ")";
                if (ImGui::Selectable(label.c_str(), selected)) {
                    selectedProj = (int)i;
                }
            }
            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild("ProjectDetails", ImVec2(0, 0), true);
            if (selectedProj < (int)g_Projects.size()) {
                const auto& proj = g_Projects[selectedProj];
                
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "%s Specification", proj.name.c_str());
                ImGui::Text("Core Language: %s", proj.language.c_str());
                ImGui::Separator();
                
                ImGui::TextWrapped("Description: %s", proj.desc.c_str());
                ImGui::Spacing();
                
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "Key Features & Mechanical Implementations:");
                for (const auto& highlight : proj.highlights) {
                    ImGui::BulletText("%s", highlight.c_str());
                }
                ImGui::Spacing();

                // ASCII Art schema preview
                ImGui::TextColored(ImVec4(0.3f, 0.3f, 0.3f, 1.0f), "SYSTEM FLOW DIAGRAM:");
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.8f, 1.0f));
                ImGui::TextUnformatted(proj.asciiArt.c_str());
                ImGui::PopStyleColor();
                
                ImGui::Spacing();
                ImGui::Separator();
                if (ImGui::Button("Inspect GitHub Repository Source Code", ImVec2(-FLT_MIN, 40.0f))) {
                    OpenGitHubLink(proj.url);
                }
                if (proj.name == "PERMADRIFT") {
                    ImGui::Spacing();
                    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.02f, 0.04f, 0.18f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.00f, 0.15f, 0.45f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.40f, 0.85f, 1.00f, 1.0f));
                    if (ImGui::Button("[>  PLAY PERMADRIFT  <]", ImVec2(-FLT_MIN, 48.0f))) {
                        LaunchPermadrift();
                    }
                    ImGui::PopStyleColor(3);
                }

                // NAVIGATION HISTORY AND EXPLORER STATE UPDATE ON SELECTION CHANGE
                if (g_ExpSelectedProj != selectedProj) {
                    g_ExpSelectedProj = selectedProj;
                    g_ExpCurrentDirParts = { "projects", g_Projects[selectedProj].name };
                    g_ExpBackHistory.clear();
                    g_ExpForwardHistory.clear();
                    g_ExpSelectedFile = "";
                    g_ExpSearchBuf[0] = '\0';
                    g_ExpSearchQuery = "";
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "EMBEDDED CODE REPOSITORY EXPLORER");
                
                // Back, Forward, Up navigation controls
                bool canGoBack = !g_ExpBackHistory.empty();
                bool canGoForward = !g_ExpForwardHistory.empty();
                bool canGoUp = g_ExpCurrentDirParts.size() > 2;

                if (!canGoBack) ImGui::BeginDisabled();
                if (ImGui::Button("< Back")) {
                    g_ExpForwardHistory.push_back(g_ExpCurrentDirParts);
                    g_ExpCurrentDirParts = g_ExpBackHistory.back();
                    g_ExpBackHistory.pop_back();
                    g_ExpSelectedFile = "";
                }
                if (!canGoBack) ImGui::EndDisabled();

                ImGui::SameLine();
                if (!canGoForward) ImGui::BeginDisabled();
                if (ImGui::Button("Forward >")) {
                    g_ExpBackHistory.push_back(g_ExpCurrentDirParts);
                    g_ExpCurrentDirParts = g_ExpForwardHistory.back();
                    g_ExpForwardHistory.pop_back();
                    g_ExpSelectedFile = "";
                }
                if (!canGoForward) ImGui::EndDisabled();

                ImGui::SameLine();
                if (!canGoUp) ImGui::BeginDisabled();
                if (ImGui::Button("^ Up")) {
                    std::vector<std::string> newParts = g_ExpCurrentDirParts;
                    newParts.pop_back();
                    g_ExpBackHistory.push_back(g_ExpCurrentDirParts);
                    g_ExpForwardHistory.clear();
                    g_ExpCurrentDirParts = newParts;
                    g_ExpSelectedFile = "";
                }
                if (!canGoUp) ImGui::EndDisabled();

                ImGui::SameLine();
                std::string pathStr = "/";
                for (size_t i = 0; i < g_ExpCurrentDirParts.size(); ++i) {
                    pathStr += g_ExpCurrentDirParts[i];
                    if (i + 1 < g_ExpCurrentDirParts.size()) pathStr += "/";
                }
                ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), " Path: %s", pathStr.c_str());

                ImGui::Spacing();

                // Explorer layout split: Left (Folders/Files), Right (Viewer + Git Commit History)
                ImVec2 avail = ImGui::GetContentRegionAvail();
                float explorerHeight = avail.y > 150.0f ? avail.y - 10.0f : 150.0f; // fill remaining or clamp

                ImGui::BeginChild("ExplorerColumnsContainer", ImVec2(0, explorerHeight), true);
                ImGui::Columns(2, "ExplorerColumnsSplit", true);
                
                // Initialize column width
                static bool setColWidthExplorer = true;
                if (setColWidthExplorer) {
                    ImGui::SetColumnWidth(0, 180.0f);
                    setColWidthExplorer = false;
                }

                // Column 0: File explorer hierarchy tree
                FSNode* dirNode = FindNodeFromParts(g_ExpCurrentDirParts);
                if (dirNode && dirNode->is_dir) {
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Directories & Files");
                    ImGui::Separator();
                    
                    for (const auto& child : dirNode->children) {
                        if (child.is_dir) {
                            std::string label = "[D] " + child.name + "/";
                            if (ImGui::Selectable(label.c_str(), false)) {
                                std::vector<std::string> newParts = g_ExpCurrentDirParts;
                                newParts.push_back(child.name);
                                g_ExpBackHistory.push_back(g_ExpCurrentDirParts);
                                g_ExpForwardHistory.clear();
                                g_ExpCurrentDirParts = newParts;
                                g_ExpSelectedFile = "";
                            }
                        } else {
                            bool isSelected = (g_ExpSelectedFile == child.name);
                            std::string label = "[F] " + child.name;
                            if (ImGui::Selectable(label.c_str(), isSelected)) {
                                g_ExpSelectedFile = child.name;
                            }
                        }
                    }
                } else {
                    ImGui::TextDisabled("Empty Directory");
                }

                // Column 1: Git commit history + File contents viewer
                ImGui::NextColumn();

                if (g_ExpSelectedFile.empty()) {
                    ImGui::TextDisabled("Select a file from the list to view its code and Git metadata.");
                } else {
                    FSNode* fileNode = nullptr;
                    if (dirNode && dirNode->is_dir) {
                        for (auto& child : dirNode->children) {
                            if (!child.is_dir && child.name == g_ExpSelectedFile) {
                                fileNode = &child;
                                break;
                            }
                        }
                    }

                    if (fileNode) {
                        CommitInfo ci = GetMockCommit(g_ExpSelectedFile);
                        
                        // Commit history header panel
                        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.01f, 0.05f, 0.01f, 1.0f));
                        ImGui::BeginChild("GitCommitPanel", ImVec2(0, 52), true);
                        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "Latest commit: %s", ci.message.c_str());
                        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Authored by %s on %s", ci.author.c_str(), ci.date.c_str());
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), " (%s)", ci.changes.c_str());
                        ImGui::EndChild();
                        ImGui::PopStyleColor();

                        ImGui::Spacing();

                        // File search bar
                        ImGui::PushItemWidth(250.0f);
                        if (ImGui::InputText("Search File Content", g_ExpSearchBuf, IM_ARRAYSIZE(g_ExpSearchBuf))) {
                            g_ExpSearchQuery = g_ExpSearchBuf;
                        }
                        ImGui::PopItemWidth();

                        ImGui::Spacing();

                        // Split into lines for syntax highlighting render
                        std::vector<std::string> fileLines;
                        std::stringstream fileSs(fileNode->content);
                        std::string fileLine;
                        while (std::getline(fileSs, fileLine)) {
                            fileLines.push_back(fileLine);
                        }

                        // File contents scrolling reader child
                        float searchAndHeaderHeight = 52.0f + ImGui::GetFrameHeightWithSpacing() + 30.0f;
                        float fileViewerHeight = explorerHeight - searchAndHeaderHeight > 100.0f ? explorerHeight - searchAndHeaderHeight : 100.0f;
                        ImGui::BeginChild("ExplorerFileViewer", ImVec2(0, fileViewerHeight), true, ImGuiWindowFlags_HorizontalScrollbar);
                        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));
                        for (size_t i = 0; i < fileLines.size(); ++i) {
                            ImGui::TextColored(ImVec4(0.3f, 0.6f, 0.3f, 1.0f), "%4d │ ", (int)i + 1);
                            ImGui::SameLine();
                            RenderHighlightedLine(fileLines[i], fileNode->name, g_ExpSearchQuery);
                        }
                        ImGui::PopStyleVar();
                        ImGui::EndChild();
                    } else {
                        ImGui::TextDisabled("File data missing.");
                    }
                }

                ImGui::Columns(1);
                ImGui::EndChild();
            }
            ImGui::EndChild();
        }
        else if (g_ActiveTab == 2) {
            g_PinnedMenuJustOpened = false;
            // ==========================================
            // TAB 2: GIS CARTOGRAPHY
            // ==========================================
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "SPATIAL INTELLIGENCE & SATELLITE CARTOGRAPHY");
            ImGui::Separator();

            ImGui::BeginChild("GisInfo", ImVec2(340, 0), true);
            
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Academic & Professional GIS Projects");
            ImGui::Separator();

            if (ImGui::CollapsingHeader("1. Chernobyl Forest Rewilding Study", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::TextWrapped("Analyzed land-use regression and NDVI vegetation changes using Landsat multispectral imagery. Built classification models mapping habitat suitability during rewilding phases.");
            }
            if (ImGui::CollapsingHeader("2. Portland Storm Inundation Grid", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::TextWrapped("Simulated coastal flooding hazards by processing dense USGS LiDAR terrain elevation points, outputting localized hydraulic drainage maps.");
            }
            if (ImGui::CollapsingHeader("3. Rayne's Neck Drone Mapping", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::TextWrapped("Captured high-accuracy aerial orthomosaics and structural elevations of seawalls using RTK GPS flight telemetry and photogrammetry.");
            }
            
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "USGS Map Layer Controls");
            ImGui::Separator();
            bool prevShowLabels = g_ShowLabels;
            if (ImGui::Checkbox("Show Map Labels", &g_ShowLabels)) {
                if (g_ShowLabels != prevShowLabels) {
                    g_ShowLabelsWorld = g_ShowLabels;
                    g_ShowLabelsStates = g_ShowLabels;
                    g_ShowLabelsHighways = g_ShowLabels;
                    g_ShowLabelsUSHighways = g_ShowLabels;
                    g_ShowLabelsRailways = g_ShowLabels;
                    g_ShowLabelsPowerPlants = g_ShowLabels;
                    g_ShowLabelsSubstations = g_ShowLabels;
                    g_ShowLabelsPipelines = g_ShowLabels;
                    g_ShowLabelsEnergyCorridors = g_ShowLabels;
                    g_ShowLabelsLakes = g_ShowLabels;
                    g_ShowLabelsCities = g_ShowLabels;
                    g_ShowLabelsContours = g_ShowLabels;
                }
            }
            ImGui::Checkbox("Show National Boundary", &g_ShowBoundary);
            
            ImGui::Checkbox("Show World Countries", &g_ShowWorld);
            if (g_ShowWorld) {
                ImGui::Indent(15.0f);
                ImGui::Checkbox("World Labels", &g_ShowLabelsWorld);
                ImGui::Unindent(15.0f);
            }
            
            ImGui::Checkbox("Show State Borders", &g_ShowStates);
            if (g_ShowStates) {
                ImGui::Indent(15.0f);
                ImGui::Checkbox("State Labels", &g_ShowLabelsStates);
                ImGui::Unindent(15.0f);
            }
            
            ImGui::Checkbox("Show Hydrography (Lakes & Rivers)", &g_ShowLakes);
            if (g_ShowLakes) {
                ImGui::Indent(15.0f);
                ImGui::Checkbox("Hydrography Labels", &g_ShowLabelsLakes);
                ImGui::Unindent(15.0f);
            }
            
            ImGui::Checkbox("Show Roads (Interstates)", &g_ShowHighways);
            if (g_ShowHighways) {
                ImGui::Indent(15.0f);
                ImGui::Checkbox("Interstate Labels", &g_ShowLabelsHighways);
                ImGui::Unindent(15.0f);
            }
            
            ImGui::Checkbox("Show Secondary Highways", &g_ShowUSHighways);
            if (g_ShowUSHighways) {
                ImGui::Indent(15.0f);
                ImGui::Checkbox("Secondary Hwy Labels", &g_ShowLabelsUSHighways);
                ImGui::Unindent(15.0f);
            }
            
            ImGui::Checkbox("Show Railroads", &g_ShowRailways);
            if (g_ShowRailways) {
                ImGui::Indent(15.0f);
                ImGui::Checkbox("Railroad Labels", &g_ShowLabelsRailways);
                ImGui::Unindent(15.0f);
            }
            
            ImGui::Checkbox("Show Power Stations", &g_ShowPowerPlants);
            if (g_ShowPowerPlants) {
                ImGui::Indent(15.0f);
                ImGui::Checkbox("Power Station Labels", &g_ShowLabelsPowerPlants);
                ImGui::Unindent(15.0f);
            }
            
            ImGui::Checkbox("Show Grid Substations", &g_ShowSubstations);
            if (g_ShowSubstations) {
                ImGui::Indent(15.0f);
                ImGui::Checkbox("Substation Labels", &g_ShowLabelsSubstations);
                ImGui::Unindent(15.0f);
            }
            
            ImGui::Checkbox("Show Gas & Oil Pipelines", &g_ShowPipelines);
            if (g_ShowPipelines) {
                ImGui::Indent(15.0f);
                ImGui::Checkbox("Pipeline Labels", &g_ShowLabelsPipelines);
                ImGui::Unindent(15.0f);
            }
            
            ImGui::Checkbox("Show HV Energy Corridors", &g_ShowEnergyCorridors);
            if (g_ShowEnergyCorridors) {
                ImGui::Indent(15.0f);
                ImGui::Checkbox("Corridor Labels", &g_ShowLabelsEnergyCorridors);
                ImGui::Unindent(15.0f);
            }
            
            ImGui::Checkbox("Show USGS Telemetry Grid", &g_ShowGrid);
            
            ImGui::Checkbox("Show USGS Stations (Cities)", &g_ShowCities);
            if (g_ShowCities) {
                ImGui::Indent(15.0f);
                ImGui::Checkbox("Station Labels", &g_ShowLabelsCities);
                ImGui::Unindent(15.0f);
            }
            
            ImGui::Checkbox("Show Topographic Contours", &g_ShowContours);
            if (g_ShowContours) {
                ImGui::Indent(15.0f);
                ImGui::Checkbox("Contour Labels", &g_ShowLabelsContours);
                ImGui::Unindent(15.0f);
            }

            g_ShowLabels = (g_ShowLabelsWorld || g_ShowLabelsStates || g_ShowLabelsHighways ||
                            g_ShowLabelsUSHighways || g_ShowLabelsRailways || g_ShowLabelsPowerPlants ||
                            g_ShowLabelsSubstations || g_ShowLabelsPipelines || g_ShowLabelsEnergyCorridors ||
                            g_ShowLabelsLakes || g_ShowLabelsCities || g_ShowLabelsContours);

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Selected Station Telemetry");
            ImGui::Separator();
            if (g_SelectedCity >= 0 && g_SelectedCity < (int)g_MapCities.size()) {
                const auto& city = g_MapCities[g_SelectedCity];
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "%s", city.name.c_str());
                ImGui::Text("Coordinates: %.2f N, %.2f W", city.lat, -city.lon);
                ImGui::TextWrapped("%s", city.desc.c_str());
            } else {
                ImGui::TextDisabled("No station selected. Click a station pin on the map to query telemetry.");
            }

            ImGui::Spacing();
            ImGui::Separator();
            if (ImGui::Button("View Full GIS Map Portfolio Repo", ImVec2(-FLT_MIN, 40.0f))) {
                OpenGitHubLink("https://github.com/lhcoyle4/gis-portfolio");
            }
            ImGui::EndChild();

            ImGui::SameLine();

            // Interactive National Map Viewer panel
            ImGui::BeginChild("GisMapViewer", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 canvasPos = ImGui::GetCursorScreenPos();
            ImVec2 canvasSize = ImGui::GetContentRegionAvail();
            
            // Map coordinate projection lambda (Equirectangular with Y inversion and aspect correction)
            auto ProjectLonLat = [&](float lon, float lat, ImVec2 center, float scale, ImVec2 offset) -> ImVec2 {
                float x = (lon - (-96.0f)) * scale + offset.x + center.x;
                float y = -(lat - 37.0f) * scale * 1.35f + offset.y + center.y;
                return ImVec2(x, y);
            };

            // Drawing line segment-by-segment with viewport clipping
            auto DrawMapLine = [&](const float* lons, const float* lats, int count, ImU32 color, float thickness, bool closed, ImVec2 center) {
                if (count < 2) return;
                for (int i = 0; i < count - 1; ++i) {
                    ImVec2 p1 = ProjectLonLat(lons[i], lats[i], center, g_MapScale, g_MapOffset);
                    ImVec2 p2 = ProjectLonLat(lons[i+1], lats[i+1], center, g_MapScale, g_MapOffset);
                    
                    float minX = p1.x < p2.x ? p1.x : p2.x;
                    float maxX = p1.x > p2.x ? p1.x : p2.x;
                    float minY = p1.y < p2.y ? p1.y : p2.y;
                    float maxY = p1.y > p2.y ? p1.y : p2.y;
                    if (maxX < canvasPos.x || minX > canvasPos.x + canvasSize.x ||
                        maxY < canvasPos.y || minY > canvasPos.y + canvasSize.y) {
                        continue;
                    }
                    drawList->AddLine(p1, p2, color, thickness);
                }
                if (closed) {
                    ImVec2 p1 = ProjectLonLat(lons[count-1], lats[count-1], center, g_MapScale, g_MapOffset);
                    ImVec2 p2 = ProjectLonLat(lons[0], lats[0], center, g_MapScale, g_MapOffset);
                    
                    float minX = p1.x < p2.x ? p1.x : p2.x;
                    float maxX = p1.x > p2.x ? p1.x : p2.x;
                    float minY = p1.y < p2.y ? p1.y : p2.y;
                    float maxY = p1.y > p2.y ? p1.y : p2.y;
                    if (!(maxX < canvasPos.x || minX > canvasPos.x + canvasSize.x ||
                          maxY < canvasPos.y || minY > canvasPos.y + canvasSize.y)) {
                        drawList->AddLine(p1, p2, color, thickness);
                    }
                }
            };

            auto IsMouseNearLine = [&](const float* lons, const float* lats, int count, ImVec2 center, float threshold, bool closed) -> bool {
                if (count < 2) return false;
                ImVec2 mousePos = io.MousePos;
                float thresholdSqr = threshold * threshold;
                
                auto DistToSegSqr = [&](ImVec2 p1, ImVec2 p2) -> float {
                    float dx = p2.x - p1.x;
                    float dy = p2.y - p1.y;
                    float lenSqr = dx * dx + dy * dy;
                    float t = 0.0f;
                    if (lenSqr > 1e-6f) {
                        t = ((mousePos.x - p1.x) * dx + (mousePos.y - p1.y) * dy) / lenSqr;
                        if (t < 0.0f) t = 0.0f;
                        else if (t > 1.0f) t = 1.0f;
                    }
                    float cx = p1.x + t * dx;
                    float cy = p1.y + t * dy;
                    return (mousePos.x - cx) * (mousePos.x - cx) + (mousePos.y - cy) * (mousePos.y - cy);
                };

                for (int i = 0; i < count - 1; ++i) {
                    ImVec2 p1 = ProjectLonLat(lons[i], lats[i], center, g_MapScale, g_MapOffset);
                    ImVec2 p2 = ProjectLonLat(lons[i+1], lats[i+1], center, g_MapScale, g_MapOffset);
                    
                    float minX = p1.x < p2.x ? p1.x : p2.x;
                    float maxX = p1.x > p2.x ? p1.x : p2.x;
                    float minY = p1.y < p2.y ? p1.y : p2.y;
                    float maxY = p1.y > p2.y ? p1.y : p2.y;
                    
                    if (mousePos.x < minX - threshold || mousePos.x > maxX + threshold ||
                        mousePos.y < minY - threshold || mousePos.y > maxY + threshold) {
                        continue;
                    }
                    
                    if (DistToSegSqr(p1, p2) < thresholdSqr) return true;
                }
                
                if (closed) {
                    ImVec2 p1 = ProjectLonLat(lons[count-1], lats[count-1], center, g_MapScale, g_MapOffset);
                    ImVec2 p2 = ProjectLonLat(lons[0], lats[0], center, g_MapScale, g_MapOffset);
                    float minX = p1.x < p2.x ? p1.x : p2.x;
                    float maxX = p1.x > p2.x ? p1.x : p2.x;
                    float minY = p1.y < p2.y ? p1.y : p2.y;
                    float maxY = p1.y > p2.y ? p1.y : p2.y;
                    if (!(mousePos.x < minX - threshold || mousePos.x > maxX + threshold ||
                          mousePos.y < minY - threshold || mousePos.y > maxY + threshold)) {
                        if (DistToSegSqr(p1, p2) < thresholdSqr) return true;
                    }
                }
                return false;
            };

            struct QueuedLabel {
                ImVec2 pos;
                ImU32 color;
                std::string text;
                float size;
                int priority;
                std::string layerName;
            };
            static std::vector<QueuedLabel> s_QueuedLabels;
            s_QueuedLabels.clear();

            float fontScale = g_MapScale / 28.0f;
            if (fontScale < 0.36f) fontScale = 0.36f;
            if (fontScale > 0.85f) fontScale = 0.85f;

            float iconScale = fontScale * 2.0f;

            auto QueueScaledLabel = [&](ImVec2 pos, ImU32 color, const char* text, float minScaleToShow, bool layerToggle, int priority, const char* layerName) {
                if (!g_ShowLabels || !layerToggle) return;
                if (g_MapScale < minScaleToShow) return;
                
                // Viewport boundary check
                if (pos.x < canvasPos.x - 10.0f || pos.x > canvasPos.x + canvasSize.x + 10.0f ||
                    pos.y < canvasPos.y - 10.0f || pos.y > canvasPos.y + canvasSize.y + 10.0f) {
                    return;
                }
                
                // Prevent duplicate/overlapping labels with the exact same name and layer within 80 pixels
                for (const auto& ql : s_QueuedLabels) {
                    if (strcmp(ql.text.c_str(), text) == 0 && strcmp(ql.layerName.c_str(), layerName) == 0) {
                        float dx = ql.pos.x - pos.x;
                        float dy = ql.pos.y - pos.y;
                        if (dx * dx + dy * dy < 80.0f * 80.0f) {
                            return;
                        }
                    }
                }
                
                float size = ImGui::GetFontSize() * fontScale;
                s_QueuedLabels.push_back({ pos, color, text, size, priority, layerName });
            };

            ImVec2 canvasCenter = ImVec2(canvasPos.x + canvasSize.x * 0.5f, canvasPos.y + canvasSize.y * 0.5f);

            // Bounding box for mouse input checks
            bool hovered = ImGui::IsWindowHovered();
            
            // Mouse Dragging to Pan Map (Left, Right, or Middle click drag - RTS-style panning)
            if (hovered && !ImGui::IsAnyItemActive() && 
                (ImGui::IsMouseDragging(ImGuiMouseButton_Left) || 
                 ImGui::IsMouseDragging(ImGuiMouseButton_Right) || 
                 ImGui::IsMouseDragging(ImGuiMouseButton_Middle))) {
                g_MapOffset.x += io.MouseDelta.x;
                g_MapOffset.y += io.MouseDelta.y;
            }

            // Mouse Wheel to Zoom Map (Focusing on cursor coordinates)
            if (hovered && io.MouseWheel != 0.0f) {
                float zoomFactor = 1.15f;
                if (io.MouseWheel < 0.0f) zoomFactor = 0.85f;
                ImVec2 mousePos = io.MousePos;
                
                ImVec2 mapMouse = ImVec2((mousePos.x - canvasCenter.x - g_MapOffset.x) / g_MapScale, 
                                         (mousePos.y - canvasCenter.y - g_MapOffset.y) / (g_MapScale * 1.35f));
                
                g_MapScale *= zoomFactor;
                if (g_MapScale < 2.0f) g_MapScale = 2.0f;
                if (g_MapScale > 120.0f) g_MapScale = 120.0f;
                
                g_MapOffset.x = mousePos.x - canvasCenter.x - mapMouse.x * g_MapScale;
                g_MapOffset.y = mousePos.y - canvasCenter.y - mapMouse.y * g_MapScale * 1.35f;
            }

            // Push clipping rect so map rendering stays strictly within canvas bounds
            drawList->PushClipRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), true);

            // Draw grid lines
            if (g_ShowGrid) {
                // Lines of longitude (-120 to -70 every 10 degrees)
                for (float lon = -120.0f; lon <= -70.0f; lon += 10.0f) {
                    ImVec2 p1 = ProjectLonLat(lon, 24.0f, canvasCenter, g_MapScale, g_MapOffset);
                    ImVec2 p2 = ProjectLonLat(lon, 50.0f, canvasCenter, g_MapScale, g_MapOffset);
                    drawList->AddLine(p1, p2, IM_COL32(0, 100, 0, 45), 1.0f);
                    
                    // Draw label near top
                    char label[32];
                    snprintf(label, sizeof(label), "%.0f W", -lon);
                    drawList->AddText(ImVec2(p2.x + 4, p2.y + 4), IM_COL32(0, 120, 0, 100), label);
                }
                // Lines of latitude (25 to 50 every 5 degrees)
                for (float lat = 25.0f; lat <= 50.0f; lat += 5.0f) {
                    ImVec2 p1 = ProjectLonLat(-125.0f, lat, canvasCenter, g_MapScale, g_MapOffset);
                    ImVec2 p2 = ProjectLonLat(-65.0f, lat, canvasCenter, g_MapScale, g_MapOffset);
                    drawList->AddLine(p1, p2, IM_COL32(0, 100, 0, 45), 1.0f);
                    
                    // Draw label near left edge
                    char label[32];
                    snprintf(label, sizeof(label), "%.0f N", lat);
                    drawList->AddText(ImVec2(p1.x + 4, p1.y - 12), IM_COL32(0, 120, 0, 100), label);
                }
            }

            // Draw world countries (rest of map of planet)
            if (g_ShowWorld) {
                for (int i = 0; i < World_Countries_Count; ++i) {
                    const auto& country = World_Countries[i];
                    float sumLon = 0.0f;
                    float sumLat = 0.0f;
                    int ptCount = 0;
                    for (int p = 0; p < country.part_count; ++p) {
                        const auto& part = World_Countries_Parts[country.part_start + p];
                        DrawMapLine(&World_Countries_Lon[part.start_index], &World_Countries_Lat[part.start_index], part.count, IM_COL32(0, 160, 20, 85), 0.8f, true, canvasCenter);
                        for (int pt = 0; pt < part.count; ++pt) {
                            sumLon += World_Countries_Lon[part.start_index + pt];
                            sumLat += World_Countries_Lat[part.start_index + pt];
                            ptCount++;
                        }
                    }
                    if (ptCount > 0 && strcmp(country.name, "United States") != 0) {
                        float avgLon = sumLon / ptCount;
                        float avgLat = sumLat / ptCount;
                        ImVec2 labelPos = ProjectLonLat(avgLon, avgLat, canvasCenter, g_MapScale, g_MapOffset);
                        QueueScaledLabel(labelPos, IM_COL32(0, 200, 30, 150), country.name, 4.0f, g_ShowLabelsWorld, 80, "Country");
                    }
                }
                // Queue a single United States country label at the center of the coterminous US
                ImVec2 usLabelPos = ProjectLonLat(-98.5795f, 39.8283f, canvasCenter, g_MapScale, g_MapOffset);
                QueueScaledLabel(usLabelPos, IM_COL32(0, 200, 30, 150), "United States", 4.0f, g_ShowLabelsWorld, 80, "Country");
            }

            // Draw topographic contours (High-resolution Appalachians, Rockies, Cascades, Sierras)
            if (g_ShowContours) {
                for (int i = 0; i < US_Contours_Count; ++i) {
                    const auto& ct = US_Contours[i];
                    DrawMapLine(&US_Contours_Lon[ct.start_index], &US_Contours_Lat[ct.start_index], ct.count, IM_COL32(0, 120, 0, 60), 1.0f, false, canvasCenter);
                }
                
                // Draw text descriptors near ridges
                ImVec2 appCenter = ProjectLonLat(-77.0f, 40.0f, canvasCenter, g_MapScale, g_MapOffset);
                QueueScaledLabel(appCenter, IM_COL32(0, 180, 0, 90), "CONTOUR 1000m", 6.0f, g_ShowLabelsContours, 40, "Contour");
                
                ImVec2 rockCenter = ProjectLonLat(-110.0f, 42.0f, canvasCenter, g_MapScale, g_MapOffset);
                QueueScaledLabel(rockCenter, IM_COL32(0, 180, 0, 90), "CONTOUR 3000m", 6.0f, g_ShowLabelsContours, 40, "Contour");
            }

            // Draw hydrography lakes and rivers
            if (g_ShowLakes) {
                // Draw detailed lakes
                for (int i = 0; i < US_Lakes_Count; ++i) {
                    const auto& lake = US_Lakes[i];
                    for (int p = 0; p < lake.part_count; ++p) {
                        const auto& part = US_Lakes_Parts[lake.part_start + p];
                        DrawMapLine(&US_Lakes_Lon[part.start_index], &US_Lakes_Lat[part.start_index], part.count, IM_COL32(0, 220, 120, 200), 2.2f, true, canvasCenter);
                        
                        bool isLakeHovered = hovered && IsMouseNearLine(&US_Lakes_Lon[part.start_index], &US_Lakes_Lat[part.start_index], part.count, canvasCenter, 4.0f, true);
                        if (isLakeHovered) {
                            ImGui::SetTooltip("[Lake] %s\nClick to get AI Overview details.", lake.name);
                            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && io.MouseDragMaxDistanceSqr[0] < 16.0f) {
                                OpenSgeOverview(lake.name, "Lake");
                            }
                        }
                    }
                }
                
                // Draw high-resolution major rivers
                for (int i = 0; i < US_Rivers_Count; ++i) {
                    const auto& river = US_Rivers[i];
                    for (int p = 0; p < river.part_count; ++p) {
                        const auto& part = US_Rivers_Parts[river.part_start + p];
                        DrawMapLine(&US_Rivers_Lon[part.start_index], &US_Rivers_Lat[part.start_index], part.count, IM_COL32(0, 200, 100, 180), 1.8f, false, canvasCenter);
                        
                        bool isRiverHovered = hovered && IsMouseNearLine(&US_Rivers_Lon[part.start_index], &US_Rivers_Lat[part.start_index], part.count, canvasCenter, 4.0f, false);
                        if (isRiverHovered) {
                            ImGui::SetTooltip("[River] %s\nClick to get AI Overview details.", river.name);
                            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && io.MouseDragMaxDistanceSqr[0] < 16.0f) {
                                OpenSgeOverview(river.name, "River");
                            }
                        }
                    }
                }

                // River/Lake text labels
                ImVec2 supCenter = ProjectLonLat(-88.5f, 47.5f, canvasCenter, g_MapScale, g_MapOffset);
                QueueScaledLabel(supCenter, IM_COL32(0, 200, 120, 180), "L. SUPERIOR", 4.0f, g_ShowLabelsLakes, 50, "Lake");

                ImVec2 missCenter = ProjectLonLat(-90.5f, 35.1f, canvasCenter, g_MapScale, g_MapOffset);
                QueueScaledLabel(missCenter, IM_COL32(0, 200, 120, 180), "MISSISSIPPI R.", 5.0f, g_ShowLabelsLakes, 50, "River");
            }

            // Draw state borders
            if (g_ShowStates) {
                for (int i = 0; i < US_States_Count; ++i) {
                    const auto& state = US_States[i];
                    float sumLon = 0.0f;
                    float sumLat = 0.0f;
                    int ptCount = 0;
                    for (int p = 0; p < state.part_count; ++p) {
                        const auto& part = US_States_Parts[state.part_start + p];
                        DrawMapLine(&US_States_Lon[part.start_index], &US_States_Lat[part.start_index], part.count, IM_COL32(0, 150, 0, 75), 1.0f, true, canvasCenter);
                        for (int pt = 0; pt < part.count; ++pt) {
                            sumLon += US_States_Lon[part.start_index + pt];
                            sumLat += US_States_Lat[part.start_index + pt];
                            ptCount++;
                        }
                    }
                    if (ptCount > 0) {
                        float avgLon = sumLon / ptCount;
                        float avgLat = sumLat / ptCount;
                        ImVec2 labelPos = ProjectLonLat(avgLon, avgLat, canvasCenter, g_MapScale, g_MapOffset);
                        QueueScaledLabel(labelPos, IM_COL32(0, 220, 50, 130), state.name, 6.0f, g_ShowLabelsStates, 90, "State");
                    }
                }
            }

            // Draw roads (Interstate Highways)
            if (g_ShowHighways) {
                for (int i = 0; i < US_Highways_Count; ++i) {
                    const auto& hw = US_Highways[i];
                    DrawMapLine(&US_Highways_Lon[hw.start_index], &US_Highways_Lat[hw.start_index], hw.count, IM_COL32(0, 200, 50, 95), 1.2f, false, canvasCenter);
                    
                    bool isHwHovered = hovered && IsMouseNearLine(&US_Highways_Lon[hw.start_index], &US_Highways_Lat[hw.start_index], hw.count, canvasCenter, 4.0f, false);
                    if (isHwHovered) {
                        ImGui::SetTooltip("[Interstate] %s\nClick to get AI Overview details.", hw.name);
                        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && io.MouseDragMaxDistanceSqr[0] < 16.0f) {
                            OpenSgeOverview(hw.name, "Interstate");
                        }
                    }
                    
                    if (hw.count > 0) {
                        int midIdx = hw.start_index + hw.count / 2;
                        ImVec2 labelPos = ProjectLonLat(US_Highways_Lon[midIdx], US_Highways_Lat[midIdx], canvasCenter, g_MapScale, g_MapOffset);
                        QueueScaledLabel(labelPos, IM_COL32(0, 240, 100, 180), hw.name, 8.0f, g_ShowLabelsHighways, 30, "Interstate");
                    }
                }
            }

            // Draw secondary roads (US Highways)
            if (g_ShowUSHighways) {
                for (int i = 0; i < US_SecondaryHighways_Count; ++i) {
                    const auto& hw = US_SecondaryHighways[i];
                    DrawMapLine(&US_SecondaryHighways_Lon[hw.start_index], &US_SecondaryHighways_Lat[hw.start_index], hw.count, IM_COL32(0, 160, 40, 60), 0.9f, false, canvasCenter);
                    
                    bool isHwHovered = hovered && IsMouseNearLine(&US_SecondaryHighways_Lon[hw.start_index], &US_SecondaryHighways_Lat[hw.start_index], hw.count, canvasCenter, 4.0f, false);
                    if (isHwHovered) {
                        ImGui::SetTooltip("[US Highway] %s\nClick to get AI Overview details.", hw.name);
                        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && io.MouseDragMaxDistanceSqr[0] < 16.0f) {
                            OpenSgeOverview(hw.name, "US Highway");
                        }
                    }
                    
                    if (hw.count > 0) {
                        int midIdx = hw.start_index + hw.count / 2;
                        ImVec2 labelPos = ProjectLonLat(US_SecondaryHighways_Lon[midIdx], US_SecondaryHighways_Lat[midIdx], canvasCenter, g_MapScale, g_MapOffset);
                        QueueScaledLabel(labelPos, IM_COL32(0, 180, 80, 150), hw.name, 12.0f, g_ShowLabelsUSHighways, 20, "US Hwy");
                    }
                }
            }

            // Draw railroads
            if (g_ShowRailways) {
                for (int i = 0; i < US_Railways_Count; ++i) {
                    const auto& rr = US_Railways[i];
                    DrawMapLine(&US_Railways_Lon[rr.start_index], &US_Railways_Lat[rr.start_index], rr.count, IM_COL32(0, 240, 200, 80), 1.1f, false, canvasCenter);
                    
                    bool isRrHovered = hovered && IsMouseNearLine(&US_Railways_Lon[rr.start_index], &US_Railways_Lat[rr.start_index], rr.count, canvasCenter, 4.0f, false);
                    if (isRrHovered) {
                        ImGui::SetTooltip("[Railway] %s\nClick to get AI Overview details.", rr.name);
                        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && io.MouseDragMaxDistanceSqr[0] < 16.0f) {
                            OpenSgeOverview(rr.name, "Railway");
                        }
                    }
                    
                    if (rr.count > 0) {
                        int midIdx = rr.start_index + rr.count / 2;
                        ImVec2 labelPos = ProjectLonLat(US_Railways_Lon[midIdx], US_Railways_Lat[midIdx], canvasCenter, g_MapScale, g_MapOffset);
                        QueueScaledLabel(labelPos, IM_COL32(0, 200, 180, 160), rr.name, 10.0f, g_ShowLabelsRailways, 15, "Railway");
                    }
                }
            }

            // Draw gas/oil pipelines
            if (g_ShowPipelines) {
                for (int i = 0; i < US_Pipelines_Count; ++i) {
                    const auto& pl = US_Pipelines[i];
                    DrawMapLine(&US_Pipelines_Lon[pl.start_index], &US_Pipelines_Lat[pl.start_index], pl.count, IM_COL32(0, 150, 200, 85), 1.2f, false, canvasCenter);
                    
                    bool isPlHovered = hovered && IsMouseNearLine(&US_Pipelines_Lon[pl.start_index], &US_Pipelines_Lat[pl.start_index], pl.count, canvasCenter, 4.0f, false);
                    if (isPlHovered) {
                        ImGui::SetTooltip("[Pipeline] %s\nClick to get AI Overview details.", pl.name);
                        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && io.MouseDragMaxDistanceSqr[0] < 16.0f) {
                            OpenSgeOverview(pl.name, "Pipeline");
                        }
                    }
                    
                    if (pl.count > 0) {
                        int midIdx = pl.start_index + pl.count / 2;
                        ImVec2 labelPos = ProjectLonLat(US_Pipelines_Lon[midIdx], US_Pipelines_Lat[midIdx], canvasCenter, g_MapScale, g_MapOffset);
                        QueueScaledLabel(labelPos, IM_COL32(0, 130, 180, 160), pl.name, 11.0f, g_ShowLabelsPipelines, 10, "Pipeline");
                    }
                }
            }

            // Draw energy corridors (HV transmission lines)
            if (g_ShowEnergyCorridors) {
                for (int i = 0; i < US_EnergyCorridors_Count; ++i) {
                    const auto& ec = US_EnergyCorridors[i];
                    DrawMapLine(&US_EnergyCorridors_Lon[ec.start_index], &US_EnergyCorridors_Lat[ec.start_index], ec.count, IM_COL32(0, 220, 220, 110), 1.3f, false, canvasCenter);
                    
                    bool isEcHovered = hovered && IsMouseNearLine(&US_EnergyCorridors_Lon[ec.start_index], &US_EnergyCorridors_Lat[ec.start_index], ec.count, canvasCenter, 4.0f, false);
                    if (isEcHovered) {
                        ImGui::SetTooltip("[Corridor] %s\nClick to get AI Overview details.", ec.name);
                        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && io.MouseDragMaxDistanceSqr[0] < 16.0f) {
                            OpenSgeOverview(ec.name, "Corridor");
                        }
                    }
                    
                    if (ec.count > 0) {
                        int midIdx = ec.start_index + ec.count / 2;
                        ImVec2 labelPos = ProjectLonLat(US_EnergyCorridors_Lon[midIdx], US_EnergyCorridors_Lat[midIdx], canvasCenter, g_MapScale, g_MapOffset);
                        QueueScaledLabel(labelPos, IM_COL32(0, 190, 190, 160), ec.name, 10.0f, g_ShowLabelsEnergyCorridors, 5, "Corridor");
                    }
                }
            }

            // Draw substations
            if (g_ShowSubstations) {
                for (int i = 0; i < US_Substations_Count; ++i) {
                    const auto& sub = US_Substations[i];
                    ImVec2 p = ProjectLonLat(sub.lon, sub.lat, canvasCenter, g_MapScale, g_MapOffset);
                    
                    if (p.x < canvasPos.x - 6.0f * iconScale || p.x > canvasPos.x + canvasSize.x + 6.0f * iconScale ||
                        p.y < canvasPos.y - 8.0f * iconScale || p.y > canvasPos.y + canvasSize.y + 8.0f * iconScale) {
                        continue;
                    }
                    
                    // Transmission tower primitive shape (indicates substation location)
                    ImU32 subColor = IM_COL32(0, 220, 220, 180);
                    // Left leg: top to bottom-left
                    drawList->AddLine(ImVec2(p.x, p.y - 7.0f * iconScale), ImVec2(p.x - 4.0f * iconScale, p.y + 7.0f * iconScale), subColor, 1.0f);
                    // Right leg: top to bottom-right
                    drawList->AddLine(ImVec2(p.x, p.y - 7.0f * iconScale), ImVec2(p.x + 4.0f * iconScale, p.y + 7.0f * iconScale), subColor, 1.0f);
                    // Horizontal structural beams:
                    drawList->AddLine(ImVec2(p.x - 2.0f * iconScale, p.y), ImVec2(p.x + 2.0f * iconScale, p.y), subColor, 1.0f);
                    drawList->AddLine(ImVec2(p.x - 3.0f * iconScale, p.y + 3.5f * iconScale), ImVec2(p.x + 3.0f * iconScale, p.y + 3.5f * iconScale), subColor, 1.0f);
                    // Top cross-arm hanger:
                    drawList->AddLine(ImVec2(p.x - 4.5f * iconScale, p.y - 2.5f * iconScale), ImVec2(p.x + 4.5f * iconScale, p.y - 2.5f * iconScale), subColor, 1.0f);
                    
                    float dx = io.MousePos.x - p.x;
                    float dy = io.MousePos.y - p.y;
                    bool isHovered = hovered && (dx >= -5.0f * iconScale && dx <= 5.0f * iconScale && dy >= -8.0f * iconScale && dy <= 8.0f * iconScale);
                    if (isHovered) {
                        ImGui::SetTooltip("%s\nClick for AI Overview.", sub.name);
                        // Prevent click action during click-and-drag panning using MouseDragMaxDistanceSqr
                        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && io.MouseDragMaxDistanceSqr[0] < 16.0f) {
                            OpenSgeOverview(std::string(sub.name), "Substation");
                        }
                    }
                    
                    QueueScaledLabel(ImVec2(p.x + 6.0f * iconScale + 2.0f, p.y - 5.0f * iconScale), IM_COL32(0, 180, 180, 150), sub.name, 22.0f, g_ShowLabelsSubstations, 60, "Substation");
                }
            }

            // Draw power stations
            if (g_ShowPowerPlants) {
                for (int i = 0; i < US_PowerStations_Count; ++i) {
                    const auto& pp = US_PowerStations[i];
                    ImVec2 p = ProjectLonLat(pp.lon, pp.lat, canvasCenter, g_MapScale, g_MapOffset);
                    
                    // Capacity scale factor (larger capacity = slightly larger icon, scaled dynamically with zoom)
                    float scale = (0.7f + sqrtf(pp.capacity) * 0.015f) * iconScale;
                    if (scale > 2.2f) scale = 2.2f;
                    float radius = 6.0f * scale;
                    
                    if (p.x < canvasPos.x - radius - 2.0f || p.x > canvasPos.x + canvasSize.x + radius + 2.0f ||
                        p.y < canvasPos.y - radius - 5.0f || p.y > canvasPos.y + canvasSize.y + radius + 2.0f) {
                        continue;
                    }
                    
                    ImU32 color = IM_COL32(0, 255, 100, 200); // Default NG/Gas: Light Green
                    if (strcmp(pp.fuel, "NUC") == 0) color = IM_COL32(255, 100, 0, 220); // Nuclear: Orange
                    else if (strcmp(pp.fuel, "HYC") == 0 || strcmp(pp.fuel, "WAT") == 0) color = IM_COL32(0, 150, 255, 200); // Hydro: Blue
                    else if (strcmp(pp.fuel, "COL") == 0) color = IM_COL32(180, 100, 255, 200); // Coal: Purple
                    
                    // Draw factory/power plant with smokestack primitive
                    // Main building block
                    drawList->AddRectFilled(ImVec2(p.x - 4.0f * scale, p.y - 1.0f * scale), ImVec2(p.x + 1.5f * scale, p.y + 4.0f * scale), color);
                    // Chimney/smokestack
                    drawList->AddRectFilled(ImVec2(p.x + 1.5f * scale, p.y - 4.0f * scale), ImVec2(p.x + 3.0f * scale, p.y + 4.0f * scale), color);
                    
                    // Dark borders to make primitives pop
                    drawList->AddRect(ImVec2(p.x - 4.0f * scale, p.y - 1.0f * scale), ImVec2(p.x + 1.5f * scale, p.y + 4.0f * scale), IM_COL32(0, 0, 0, 200), 0.0f, 0, 1.0f);
                    drawList->AddRect(ImVec2(p.x + 1.5f * scale, p.y - 4.0f * scale), ImVec2(p.x + 3.0f * scale, p.y + 4.0f * scale), IM_COL32(0, 0, 0, 200), 0.0f, 0, 1.0f);
                    
                    // Smoke puff plume coming from smokestack
                    ImU32 smokeColor = IM_COL32(220, 220, 220, 160);
                    drawList->AddCircleFilled(ImVec2(p.x + 2.25f * scale, p.y - 5.5f * scale), 1.3f * scale, smokeColor);
                    
                    float dx = io.MousePos.x - p.x;
                    float dy = io.MousePos.y - p.y;
                    bool isHovered = hovered && (dx >= -4.0f * scale && dx <= 3.0f * scale && dy >= -6.0f * scale && dy <= 4.0f * scale);
                    if (isHovered) {
                        ImGui::SetTooltip("%s\nFuel: %s | Capacity: %.1f MW\nClick for AI Overview.", pp.name, pp.fuel, pp.capacity);
                        // Prevent click action during click-and-drag panning using MouseDragMaxDistanceSqr
                        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && io.MouseDragMaxDistanceSqr[0] < 16.0f) {
                            OpenSgeOverview(std::string(pp.name), "Power Station");
                        }
                    }
                    
                    if (g_MapScale >= 15.0f) {
                        char labelText[128];
                        snprintf(labelText, sizeof(labelText), "%s (%.0f MW)", pp.name, pp.capacity);
                        QueueScaledLabel(ImVec2(p.x + 3.5f * scale + 4.0f, p.y - 3.0f * scale), color, labelText, 15.0f, g_ShowLabelsPowerPlants, 70, "Power Plant");
                    }
                }
            }

            // Draw national borders (Detailed 50m outline)
            if (g_ShowBoundary) {
                for (int i = 0; i < US_Border_Parts_Count; ++i) {
                    const auto& part = US_Border_Parts[i];
                    DrawMapLine(&US_Border_Lon[part.start_index], &US_Border_Lat[part.start_index], part.count, IM_COL32(0, 255, 30, 220), 2.2f, true, canvasCenter);
                }
            }

            // Draw cities (USGS stations)
            if (g_ShowCities) {
                for (size_t i = 0; i < g_MapCities.size(); ++i) {
                    const auto& city = g_MapCities[i];
                    ImVec2 p = ProjectLonLat(city.lon, city.lat, canvasCenter, g_MapScale, g_MapOffset);
                    
                    bool isSelected = ((int)i == g_SelectedCity);
                    ImU32 dotColor = isSelected ? IM_COL32(255, 200, 0, 255) : IM_COL32(0, 255, 30, 255);
                    ImU32 ringColor = isSelected ? IM_COL32(255, 200, 0, 180) : IM_COL32(0, 255, 30, 120);
                    // Draw antenna tower primitive (scaled dynamically with zoom)
                    float s = (isSelected ? 1.5f : 1.1f) * iconScale;
                    
                    // Draw base A-frame tower
                    drawList->AddLine(ImVec2(p.x - 3.0f * s, p.y + 5.0f * s), ImVec2(p.x, p.y - 3.0f * s), dotColor, 1.0f);
                    drawList->AddLine(ImVec2(p.x + 3.0f * s, p.y + 5.0f * s), ImVec2(p.x, p.y - 3.0f * s), dotColor, 1.0f);
                    drawList->AddLine(ImVec2(p.x - 1.8f * s, p.y + 1.5f * s), ImVec2(p.x + 1.8f * s, p.y + 1.5f * s), dotColor, 1.0f);
                    
                    // Draw antenna mast
                    drawList->AddLine(ImVec2(p.x, p.y - 3.0f * s), ImVec2(p.x, p.y - 9.0f * s), dotColor, 1.0f);
                    // Cross bar
                    drawList->AddLine(ImVec2(p.x - 1.5f * s, p.y - 6.0f * s), ImVec2(p.x + 1.5f * s, p.y - 6.0f * s), dotColor, 1.0f);
                    
                    // Transmitter beacon
                    drawList->AddCircleFilled(ImVec2(p.x, p.y - 9.0f * s), 1.3f * s, ringColor);
                    // Outer signal ring
                    drawList->AddCircle(ImVec2(p.x, p.y - 9.0f * s), isSelected ? 6.0f * s : 4.0f * s, ringColor, 12, 0.8f);
                    
                    // Hover detection
                    float dx = io.MousePos.x - p.x;
                    float dy = io.MousePos.y - p.y;
                    bool isIconHovered = hovered && (dx >= -4.0f * s && dx <= 4.0f * s && dy >= -10.0f * s && dy <= 6.0f * s);
                    if (isIconHovered) {
                        ImGui::SetTooltip("%s\nUSGS Telemetry Station.\nClick for AI Overview.", city.name.c_str());
                        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && io.MouseDragMaxDistanceSqr[0] < 16.0f) {
                            g_SelectedCity = (int)i;
                            OpenSgeOverview(city.name, "USGS Station");
                        }
                    }
 
                    // Label offset text (scaled dynamically with icon scale to avoid overlap)
                    QueueScaledLabel(ImVec2(p.x + 6.0f * s + 2.0f, p.y - 8.0f * s), isSelected ? IM_COL32(255, 220, 0, 240) : IM_COL32(0, 240, 50, 190), city.name.c_str(), 0.0f, g_ShowLabelsCities, 100, "City");
                }
            }

            // Draw queued labels with collision detection & clustering
            if (!s_QueuedLabels.empty()) {
                // Sort by priority descending (highest priority first)
                std::sort(s_QueuedLabels.begin(), s_QueuedLabels.end(), [](const QueuedLabel& a, const QueuedLabel& b) {
                    return a.priority > b.priority;
                });

                // Helper to check bounding box overlaps
                auto Overlaps = [](const QueuedLabel& a, const ImVec2& aSize, const QueuedLabel& b, const ImVec2& bSize) -> bool {
                    float paddingX = 8.0f;
                    float paddingY = 4.0f;
                    return (a.pos.x - paddingX < b.pos.x + bSize.x && a.pos.x + aSize.x + paddingX > b.pos.x &&
                            a.pos.y - paddingY < b.pos.y + bSize.y && a.pos.y + aSize.y + paddingY > b.pos.y);
                };

                struct LabelCluster {
                    int leadIdx;
                    std::vector<int> memberIdxs;
                };

                std::vector<LabelCluster> clusters;
                std::vector<ImVec2> textSizes(s_QueuedLabels.size());
                for (size_t i = 0; i < s_QueuedLabels.size(); ++i) {
                    textSizes[i] = ImGui::GetFont()->CalcTextSizeA(s_QueuedLabels[i].size, FLT_MAX, 0.0f, s_QueuedLabels[i].text.c_str());
                }

                for (size_t i = 0; i < s_QueuedLabels.size(); ++i) {
                    bool joined = false;
                    for (auto& cluster : clusters) {
                        int lead = cluster.leadIdx;
                        if (Overlaps(s_QueuedLabels[lead], textSizes[lead], s_QueuedLabels[i], textSizes[i])) {
                            cluster.memberIdxs.push_back((int)i);
                            joined = true;
                            break;
                        }
                    }
                    if (!joined) {
                        LabelCluster newCluster;
                        newCluster.leadIdx = (int)i;
                        clusters.push_back(newCluster);
                    }
                }

                // Render clusters
                for (const auto& cluster : clusters) {
                    int lead = cluster.leadIdx;
                    const auto& label = s_QueuedLabels[lead];
                    ImVec2 pos = label.pos;
                    ImVec2 size = textSizes[lead];

                    // Draw lead label text
                    drawList->AddText(ImGui::GetFont(), label.size, pos, label.color, label.text.c_str());

                    float indicatorWidth = 0.0f;
                    bool hasIndicator = !cluster.memberIdxs.empty();
                    if (hasIndicator) {
                        // Draw yellow [+] indicator next to the lead text
                        char indText[16];
                        snprintf(indText, sizeof(indText), " [+%d]", (int)cluster.memberIdxs.size());
                        ImVec2 indPos = ImVec2(pos.x + size.x + 2.0f, pos.y);
                        float indSize = label.size;
                        ImVec2 indTextSize = ImGui::GetFont()->CalcTextSizeA(indSize, FLT_MAX, 0.0f, indText);
                        indicatorWidth = indTextSize.x + 4.0f;
                        drawList->AddText(ImGui::GetFont(), indSize, indPos, IM_COL32(255, 220, 0, 245), indText);
                    }

                    // Check hover on lead label + indicator bounding box
                    ImVec2 totalSize = ImVec2(size.x + indicatorWidth, size.y);
                    bool isHovered = hovered &&
                                     io.MousePos.x >= pos.x && io.MousePos.x <= pos.x + totalSize.x &&
                                     io.MousePos.y >= pos.y && io.MousePos.y <= pos.y + totalSize.y;

                    if (isHovered) {
                        if (hasIndicator) {
                            ImGui::BeginTooltip();
                            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "CONGESTED AREA - OVERLAPPING LAYERS (Click to pin):");
                            ImGui::Separator();
                            
                            // Lead label
                            ImVec4 colLead = ImGui::ColorConvertU32ToFloat4(label.color);
                            ImGui::TextColored(colLead, "[%s] %s", label.layerName.c_str(), label.text.c_str());

                            // Members
                            for (int idx : cluster.memberIdxs) {
                                const auto& mb = s_QueuedLabels[idx];
                                ImVec4 col = ImGui::ColorConvertU32ToFloat4(mb.color);
                                ImGui::TextColored(col, "[%s] %s", mb.layerName.c_str(), mb.text.c_str());
                            }
                            ImGui::EndTooltip();

                            // Click to pin menu
                            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && io.MouseDragMaxDistanceSqr[0] < 16.0f) {
                                g_PinnedMenuOpen = true;
                                g_PinnedMenuPos = io.MousePos;
                                g_PinnedMenuItems.clear();
                                g_PinnedMenuItems.push_back({ label.text, label.layerName, label.color });
                                for (int idx : cluster.memberIdxs) {
                                    const auto& mb = s_QueuedLabels[idx];
                                    g_PinnedMenuItems.push_back({ mb.text, mb.layerName, mb.color });
                                }
                                g_PinnedMenuJustOpened = true;
                            }
                        } else {
                            ImGui::BeginTooltip();
                            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(label.color), "[%s] %s", label.layerName.c_str(), label.text.c_str());
                            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Click to get AI Overview details.");
                            ImGui::EndTooltip();

                            // Click to get direct SGE AI Overview
                            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && io.MouseDragMaxDistanceSqr[0] < 16.0f) {
                                OpenSgeOverview(label.text, label.layerName);
                            }
                        }
                    }
                }
            }

            // Pop clipping rect
            drawList->PopClipRect();

            // Draw HUD Info Overlays (Floating inside canvas)
            
            // 1. Compass Rose (Top Right)
            ImVec2 compassCenter = ImVec2(canvasPos.x + canvasSize.x - 45.0f, canvasPos.y + 45.0f);
            drawList->AddCircle(compassCenter, 20.0f, IM_COL32(0, 255, 30, 100), 16, 1.0f);
            drawList->AddLine(ImVec2(compassCenter.x, compassCenter.y + 15.0f), ImVec2(compassCenter.x, compassCenter.y - 15.0f), IM_COL32(0, 255, 30, 150), 1.5f);
            drawList->AddLine(ImVec2(compassCenter.x - 15.0f, compassCenter.y), ImVec2(compassCenter.x + 15.0f, compassCenter.y), IM_COL32(0, 255, 30, 100), 1.0f);
            drawList->AddTriangleFilled(ImVec2(compassCenter.x - 4, compassCenter.y - 5), ImVec2(compassCenter.x + 4, compassCenter.y - 5), ImVec2(compassCenter.x, compassCenter.y - 17), IM_COL32(0, 255, 50, 220));
            drawList->AddText(ImVec2(compassCenter.x - 3, compassCenter.y - 32), IM_COL32(0, 255, 30, 200), "N");

            // 2. Cursor Lat/Lon Telemetry (Bottom Left)
            float mouseLon = (io.MousePos.x - canvasCenter.x - g_MapOffset.x) / g_MapScale + (-96.0f);
            float mouseLat = -(io.MousePos.y - canvasCenter.y - g_MapOffset.y) / (g_MapScale * 1.35f) + 37.0f;
            
            char telemetryText[64];
            if (hovered && io.MousePos.x >= canvasPos.x && io.MousePos.x <= canvasPos.x + canvasSize.x &&
                io.MousePos.y >= canvasPos.y && io.MousePos.y <= canvasPos.y + canvasSize.y) {
                snprintf(telemetryText, sizeof(telemetryText), "CURSOR: %.4f N, %.4f W", mouseLat, -mouseLon);
            } else {
                snprintf(telemetryText, sizeof(telemetryText), "CURSOR: OUT OF BOUNDS");
            }
            drawList->AddText(ImVec2(canvasPos.x + 15.0f, canvasPos.y + canvasSize.y - 30.0f), IM_COL32(0, 255, 30, 220), telemetryText);

            // 3. Dynamic Map Scale Bar (Bottom Left, above coordinates)
            float scaleDegrees = 10.0f; // 10 degrees Longitude at 38N (~550 miles)
            float scaleBarPx = scaleDegrees * g_MapScale;
            ImVec2 barStart = ImVec2(canvasPos.x + 15.0f, canvasPos.y + canvasSize.y - 55.0f);
            drawList->AddLine(barStart, ImVec2(barStart.x + scaleBarPx, barStart.y), IM_COL32(0, 255, 30, 200), 2.0f);
            drawList->AddLine(barStart, ImVec2(barStart.x, barStart.y - 5.0f), IM_COL32(0, 255, 30, 200), 2.0f);
            drawList->AddLine(ImVec2(barStart.x + scaleBarPx, barStart.y), ImVec2(barStart.x + scaleBarPx, barStart.y - 5.0f), IM_COL32(0, 255, 30, 200), 2.0f);
            drawList->AddText(ImVec2(barStart.x + scaleBarPx + 8.0f, barStart.y - 8.0f), IM_COL32(0, 255, 30, 180), "550 mi");

            // 4. Manual Zoom / Reset buttons Overlay (Bottom Right)
            ImGui::SetCursorScreenPos(ImVec2(canvasPos.x + canvasSize.x - 180.0f, canvasPos.y + canvasSize.y - 45.0f));
            if (ImGui::Button("[+]", ImVec2(35, 30))) {
                g_MapScale *= 1.25f;
                if (g_MapScale > 120.0f) g_MapScale = 120.0f;
            }
            ImGui::SameLine();
            if (ImGui::Button("[-]", ImVec2(35, 30))) {
                g_MapScale *= 0.8f;
                if (g_MapScale < 2.0f) g_MapScale = 2.0f;
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset", ImVec2(55, 30))) {
                g_MapScale = 8.0f;
                g_MapOffset = ImVec2(0.0f, 0.0f);
            }

            // Render Pinned Congested Area Menu
            if (g_PinnedMenuOpen) {
                ImGui::SetNextWindowPos(g_PinnedMenuPos, ImGuiCond_Appearing);
                ImGui::SetNextWindowSize(ImVec2(340, 0)); // Auto height
                ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.01f, 0.05f, 0.02f, 0.98f)); // Dark green
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 0.8f, 0.0f, 1.0f)); // Amber border
                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
                
                ImGuiWindowFlags menuFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
                
                if (ImGui::Begin("##PinnedCongestedMenu", &g_PinnedMenuOpen, menuFlags)) {
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "CONGESTED AREA - OVERLAPPING LAYERS");
                    ImGui::Separator();
                    ImGui::Spacing();
                    
                    for (size_t idx = 0; idx < g_PinnedMenuItems.size(); ++idx) {
                        const auto& item = g_PinnedMenuItems[idx];
                        ImVec4 col = ImGui::ColorConvertU32ToFloat4(item.color);
                        char itemLabel[256];
                        snprintf(itemLabel, sizeof(itemLabel), "[%s] %s", item.layerName.c_str(), item.name.c_str());
                        
                        ImGui::PushStyleColor(ImGuiCol_Text, col);
                        ImGui::PushID((int)idx);
                        if (ImGui::Selectable(itemLabel, false)) {
                            OpenSgeOverview(item.name, item.layerName);
                        }
                        ImGui::PopID();
                        ImGui::PopStyleColor(); // Text
                    }
                    
                    ImGui::Spacing();
                    ImGui::Separator();
                    if (ImGui::Button("Close Menu", ImVec2(-FLT_MIN, 0))) {
                        g_PinnedMenuOpen = false;
                    }
                    
                    // Click outside to close detection
                    if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                        if (!g_PinnedMenuJustOpened) {
                            g_PinnedMenuOpen = false;
                        }
                    }
                }
                ImGui::End();
                
                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(2);
            }
            
            // Render SGE AI Overview Popup Modal
            if (g_SgeOpen) {
                ImGui::SetNextWindowSize(ImVec2(520, 320), ImGuiCond_FirstUseEver);
                ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.02f, 0.08f, 0.04f, 0.96f)); // Deep dark green/blue SGE style
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 1.0f, 0.3f, 1.0f)); // Glowing active green border
                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
                
                if (ImGui::Begin("✦ AI Overview", &g_SgeOpen, ImGuiWindowFlags_NoCollapse)) {
                    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.3f, 1.0f), "AI OVERVIEW FOR:");
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "%s", g_SgeEntityName.c_str());
                    
                    ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 0.8f), "Source Layer: %s", g_SgeEntityLayer.c_str());
                    ImGui::Separator();
                    ImGui::Spacing();
                    
                    // Render description inside a child frame with customized background
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.12f, 0.05f, 1.0f));
                    if (ImGui::BeginChild("##SgeTextChild", ImVec2(0, -45), true)) {
                        ImGui::TextWrapped("%s", g_SgeSummaryText.c_str());
                        ImGui::EndChild();
                    }
                    ImGui::PopStyleColor(); // FrameBg
                    
                    ImGui::Spacing();
                    
                    // Sources footer (clickable links to external references)
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Sources:");
                    ImGui::SameLine();
                    
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.2f, 0.05f, 1.0f));
                    if (ImGui::Button("USGS.gov")) {
                        SearchGoogle(g_SgeEntityName + " USGS");
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Wikipedia")) {
                        SearchGoogle(g_SgeEntityName + " Wikipedia");
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Energy.gov")) {
                        SearchGoogle(g_SgeEntityName + " Energy");
                    }
                    ImGui::PopStyleColor(); // Button
                }
                ImGui::End();
                
                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(2);
            }

            ImGui::EndChild();
        }

        else if (g_ActiveTab == 3) {
            // ==========================================
            // TAB 3: PORTFOLIO MOTD DASHBOARD
            // ==========================================

            // Helper: draw an MOTD-style block-char bar
            auto DrawMotdBar = [](float pct, int width = 20) {
                int filled = (int)(pct / 100.0f * width + 0.5f);
                if (filled > width) filled = width;
                std::string bar;
                for (int i = 0; i < filled; ++i)   bar += "\xe2\x96\x88"; // U+2588 FULL BLOCK
                for (int i = filled; i < width; ++i) bar += "\xe2\x96\x91"; // U+2591 LIGHT SHADE
                return bar;
            };

            // Color for bar based on percentage
            auto BarColor = [](float pct) -> ImVec4 {
                if (pct < 60.0f) return ImVec4(0.0f, 1.0f, 0.3f,  1.0f); // green
                if (pct < 85.0f) return ImVec4(1.0f, 0.78f, 0.05f, 1.0f); // yellow
                return                   ImVec4(0.9f, 0.18f, 0.10f, 1.0f); // red
            };

            // Get current time
            time_t rawtime = time(nullptr);
            struct tm* ti = localtime(&rawtime);
            int hour = ti ? ti->tm_hour : 12;
            const char* greeting = (hour < 12) ? "Good Morning" : (hour < 18) ? "Good Afternoon" : "Good Evening";
            char dateStr[64];
            if (ti) strftime(dateStr, sizeof(dateStr), "%A, %b %d %Y  |  %I:%M %p", ti);
            else     snprintf(dateStr, sizeof(dateStr), "--- --- -- ----  --:-- --");

            // ── Header ──────────────────────────────────────────────────────
            ImGui::TextColored(ImVec4(0.0f, 0.9f, 1.0f, 1.0f),
                "LHCOYLE4 // PORTFOLIO DASHBOARD");
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.45f, 1.0f), "%s, Louie!  ", greeting);
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.35f, 0.65f, 0.35f, 1.0f), "%s", dateStr);
            ImGui::Spacing();

            // ── System Health ────────────────────────────────────────────────
            ImGui::TextColored(ImVec4(0.0f, 0.9f, 1.0f, 1.0f), "[ System Health ]");
            ImGui::Separator();

            // CPU
            {
                std::string bar = DrawMotdBar(g_CpuSmooth);
                ImGui::Text("  CPU  ["); ImGui::SameLine(0, 0);
                ImGui::TextColored(BarColor(g_CpuSmooth), "%s", bar.c_str()); ImGui::SameLine(0, 0);
                ImGui::Text("]"); ImGui::SameLine();
                ImGui::TextColored(BarColor(g_CpuSmooth), " %.1f%%", g_CpuSmooth);
            }
            // HEAP
            {
                std::string bar = DrawMotdBar(g_RamSmooth);
                ImGui::Text("  HEAP ["); ImGui::SameLine(0, 0);
                ImGui::TextColored(BarColor(g_RamSmooth), "%s", bar.c_str()); ImGui::SameLine(0, 0);
                ImGui::Text("]"); ImGui::SameLine();
                ImGui::TextColored(BarColor(g_RamSmooth), " %.1f%%", g_RamSmooth);
            }
            // NET IO
            {
                std::string bar = DrawMotdBar(g_NetSmooth);
                ImGui::Text("  NET  ["); ImGui::SameLine(0, 0);
                ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.05f, 1.0f), "%s", bar.c_str()); ImGui::SameLine(0, 0);
                ImGui::Text("]"); ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.05f, 1.0f), " %.1f%%", g_NetSmooth);
            }

            ImGui::Spacing();

            // Scrolling plot lines (compact)
            float plotW = ImGui::GetContentRegionAvail().x;
            ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.0f, 1.0f, 0.3f, 1.0f));
            ImGui::PlotLines("##MCPU", g_CpuHistory.data(), (int)g_CpuHistory.size(), 0, "vCPU", 0.0f, 100.0f, ImVec2(plotW, 55.0f));
            ImGui::PopStyleColor();
            ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.0f, 0.8f, 1.0f, 1.0f));
            ImGui::PlotLines("##MRAM", g_RamHistory.data(), (int)g_RamHistory.size(), 0, "HEAP", 0.0f, 100.0f, ImVec2(plotW, 55.0f));
            ImGui::PopStyleColor();
            ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(1.0f, 0.78f, 0.05f, 1.0f));
            ImGui::PlotLines("##MNET", g_NetworkHistory.data(), (int)g_NetworkHistory.size(), 0, "NET", 0.0f, 100.0f, ImVec2(plotW, 55.0f));
            ImGui::PopStyleColor();

            ImGui::Spacing();

            // ── Sandbox Repos ────────────────────────────────────────────────
            ImGui::TextColored(ImVec4(0.0f, 0.9f, 1.0f, 1.0f), "[ Sandbox Repos ]");
            ImGui::Separator();

            struct RepoInfo { const char* name; const char* branch; const char* status; const char* remote; };
            static const RepoInfo repos[] = {
                { "lhcoyle4.github.io", "master",  "Clean",    "Synced"    },
                { "asteroids_vectrex",  "master",  "Clean",    "Synced"    },
                { "motd",               "master",  "Clean",    "Synced"    },
                { "alt_drag_resize",    "master",  "Clean",    "Synced"    },
                { "terminal_launcher",  "master",  "Clean",    "Synced"    },
            };
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                "  %-22s  %-10s  %-8s  %s", "Repo", "Branch", "Status", "Remote");
            for (auto& r : repos) {
                ImGui::Text("  •"); ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.45f, 1.0f), "%-22s", r.name); ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f,  1.0f), "  %-10s", r.branch); ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.3f,  1.0f), "  %-8s", r.status); ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.3f,  1.0f), "  %s", r.remote);
            }

            ImGui::Spacing();

            // ── Quote / Tip ─────────────────────────────────────────────────
            ImGui::TextColored(ImVec4(0.0f, 0.9f, 1.0f, 1.0f), "[ Status & Insights ]");
            ImGui::Separator();
            {
                static int  s_FullQuoteIdx   = 0;
                static double s_FullQuoteT   = -1.0;
                int numTips = (int)(sizeof(g_DevTips) / sizeof(g_DevTips[0]));
                double now2 = ImGui::GetTime();
                if (s_FullQuoteT < 0.0) { s_FullQuoteT = now2; s_FullQuoteIdx = (numTips / 2) % numTips; }
                if (now2 - s_FullQuoteT >= 30.0) {
                    s_FullQuoteT += 30.0;
                    s_FullQuoteIdx = (s_FullQuoteIdx + 1) % numTips;
                }
                ImGui::Text("  Tip/Quote: "); ImGui::SameLine(0, 0);
                ImGui::PushTextWrapPos(0.0f);
                ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.55f, 1.0f), "\"%s\"", g_DevTips[s_FullQuoteIdx]);
                ImGui::PopTextWrapPos();
            }
        }
        else if (g_ActiveTab == 4) {
            // ==========================================
            // TAB 4: INTRO & CREDITS
            // ==========================================
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "INFO & CREDITS");
            ImGui::Separator();

            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Why build a personal portfolio website in C++ / WebAssembly?");
            ImGui::TextWrapped(
                "Modern web development has become layered in layers of bulky frameworks, heavy Javascript bundles, and overhead "
                "abstractions. This page is built with the opposite design philosophy: Mechanical Sympathy.\n\n"
                "By writing in low-level C++, utilizing the immediate-mode Dear ImGui rendering library, and compiling directly to "
                "highly optimized WebAssembly (WASM) via Emscripten, we bypass the browser DOM entirely. The interface is rendered "
                "directly as pixel shaders on a single WebGL canvas at a locked 60 FPS, with minimal execution latency.\n\n"
                "This project serves as a showcase of C++ capabilities, proving that lightweight systems programming can be directly "
                "ported to web pages to deliver raw desktop-grade speed and responsive graphics."
            );

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "Engineering Specs:");
            ImGui::BulletText("UI Engine:         Dear ImGui v%s (Immediate Mode)", IMGUI_VERSION);
            ImGui::BulletText("Compiler:          Emscripten v5.0.7 (Clang C++ to WASM)");
            ImGui::BulletText("Window Abstraction: SDL2 (Simple DirectMedia Layer)");
            ImGui::BulletText("Graphics Pipeline: OpenGL ES 3.0 / WebGL2");
            ImGui::BulletText("Platform Host:     GitHub Pages");
        }

        ImGui::EndChild();

        ImGui::End();

        // 1. Show the big demo window if selected
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // Rendering
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }
    EMSCRIPTEN_MAINLOOP_END;

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
