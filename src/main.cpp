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
struct LogLine {
    std::string text;
    ImVec4 color;
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
auto g_StartTime = std::chrono::steady_clock::now();
bool g_MatrixMode = false;
int g_MatrixTimer = 0;


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


// Log function
void AddLog(const std::string& text, ImVec4 color = ImVec4(0.2f, 1.0f, 0.2f, 1.0f)) {
    g_ConsoleLog.push_back({ text, color });
    g_TerminalScrollToBottom = true;
    if (g_ConsoleLog.size() > 100) {
        g_ConsoleLog.erase(g_ConsoleLog.begin());
    }
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
        "asteroids_vectrex", "JS / C",
        "A retro vector-graphics clone of the classic Asteroids arcade game, built for web and native environments.",
        {
            "Simulates high-fidelity vector CRT glow effects.",
            "Employs precise 2D collision geometry.",
            "Smooth particle engines rendering asteroid fracturing.",
            "Runs at a locked 60 FPS on basic processors."
        },
        "https://github.com/lhcoyle4/asteroids_vectrex",
        "       /\\  *      \n  *   /  \\     +  \n     /____\\       \n   *   /\\   /\\  * \n      /  \\_/  \\   "
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


// Terminal commands execution logic
void ExecuteCommand(const std::string& cmdLine) {
    // Add command to history
    g_CmdHistory.push_back(cmdLine);
    g_HistoryPos = -1;

    // Echo command
    AddLog("> " + cmdLine, ImVec4(0.0f, 0.8f, 1.0f, 1.0f));

    // Parse command name and args
    std::string cmd = cmdLine;
    cmd.erase(0, cmd.find_first_not_of(" \t"));
    cmd.erase(cmd.find_last_not_of(" \t") + 1);
    
    std::string lowerCmd = cmd;
    std::transform(lowerCmd.begin(), lowerCmd.end(), lowerCmd.begin(), ::tolower);

    if (lowerCmd == "help") {
        AddLog("Available Commands:");
        AddLog("  help       - Shows list of commands.");
        AddLog("  about      - Display biographical profile.");
        AddLog("  projects   - Show C++ / low-level projects.");
        AddLog("  gis        - List spatial intelligence & cartography work.");
        AddLog("  neofetch   - Display system summary configuration.");
        AddLog("  matrix     - Toggle the falling code visualizer.");
        AddLog("  clear      - Clear the console scrollback.");
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
        g_ActiveTab = 1; // Switch tab
        AddLog("Opening Project Directory tab...", ImVec4(0.9f, 0.9f, 0.0f, 1.0f));
        AddLog("Use the tabs or double-click items to view details.");
        for (const auto& proj : g_Projects) {
            AddLog(" * " + proj.name + " (" + proj.language + ") - " + proj.desc);
        }
    }
    else if (lowerCmd == "gis") {
        g_ActiveTab = 2; // Switch tab
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
        g_MatrixMode = !g_MatrixMode;
        AddLog(g_MatrixMode ? "Initializing code fall stream... OK" : "Closing stream matrix... OK", ImVec4(0.0f, 1.0f, 0.5f, 1.0f));
    }
    else if (lowerCmd == "clear") {
        g_ConsoleLog.clear();
    }
    else {
        AddLog("Command not recognized: '" + cmd + "'. Type 'help' for available options.", ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
    }
}

// Input callback to handle history
int ConsoleInputCallback(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
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

// Setup plots
void UpdateHistoryPlots() {
    static float timer = 0.0f;
    timer += 0.03f;
    
    // Push new simulated values
    float cpu = 15.0f + 10.0f * sinf(timer * 0.5f) + (rand() % 500) / 100.0f;
    float ram = 45.3f + 1.2f * sinf(timer * 0.1f) + (rand() % 100) / 100.0f;
    float net = 5.0f + 25.0f * (sinf(timer) > 0.8f ? sinf(timer) * 3.0f : 0.2f) + (rand() % 200) / 100.0f;

    g_CpuHistory.push_back(cpu);
    g_RamHistory.push_back(ram);
    g_NetworkHistory.push_back(net);

    if (g_CpuHistory.size() > 100) g_CpuHistory.erase(g_CpuHistory.begin());
    if (g_RamHistory.size() > 100) g_RamHistory.erase(g_RamHistory.begin());
    if (g_NetworkHistory.size() > 100) g_NetworkHistory.erase(g_NetworkHistory.begin());
}

// Matrix falling code generator
void UpdateMatrixRain() {
    g_MatrixTimer++;
    if (g_MatrixTimer % 3 == 0) {
        std::string matrixChars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ$#@%&*()[]{}";
        std::string line = "";
        for (int i = 0; i < 40; ++i) {
            if (rand() % 10 == 0) {
                line += " ";
            } else {
                line += matrixChars[rand() % matrixChars.length()];
                line += " ";
            }
        }
        AddLog(line, ImVec4(0.0f, (float)(80 + rand() % 175) / 255.0f, 0.0f, 1.0f));
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
    
    // Set up radar pins (coordinates in Portland, ME region)
    g_RadarPins.push_back(ImVec2(100, 100)); // Portland Downtown
    g_RadarPins.push_back(ImVec2(140, 70));  // Casco Bay
    g_RadarPins.push_back(ImVec2(80, 120));  // Fore River
    g_RadarPins.push_back(ImVec2(50, 60));   // Back Cove

    // Initial console log
    AddLog("==================================================================", ImVec4(0.0f, 0.8f, 0.2f, 1.0f));
    AddLog(" LCOYLE4 CORE ENGINE v3.5.2 (WASM) INITIATED...", ImVec4(0.0f, 1.0f, 0.5f, 1.0f));
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
        if (g_MatrixMode) {
            UpdateMatrixRain();
        }

        // Start frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // RENDER CENTRAL PORTFOLIO WINDOW (Locks to browser size)
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | 
                                      ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoBringToFrontOnFocus;

        ImGui::Begin("Louie Coyle Portfolio Core", nullptr, windowFlags);

        // Top Menu Bar
        if (ImGui::BeginMenuBar()) {
            ImGui::Text("[LCOYLE4 SYSTEMS CORE] | ");
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

        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "SYS DIAGNOSTICS");
        ImGui::Separator();
        
        // Progress Bars for system parameters
        float currentCpu = g_CpuHistory.back();
        float currentRam = g_RamHistory.back();
        ImGui::Text("CPU Core Usage:");
        ImGui::ProgressBar(currentCpu / 100.0f, ImVec2(-FLT_MIN, 15.0f), "");
        ImGui::SameLine(0, 4);
        ImGui::Text("%.1f%%", currentCpu);

        ImGui::Text("Heap Memory Usage:");
        ImGui::ProgressBar(currentRam / 100.0f, ImVec2(-FLT_MIN, 15.0f), "");
        ImGui::SameLine(0, 4);
        ImGui::Text("%.1f%%", currentRam);
        
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "NAV COMMAND CENTER");
        ImGui::Separator();
        
        // Stark navigation menu buttons
        const char* menuOptions[] = {
            " [F1] TERMINAL CONSOLE",
            " [F2] PROJECT DIRECTORY",
            " [F3] GIS CARTOGRAPHY",
            " [F4] HARDWARE PLOTS",
            " [F5] INTRO & CREDITS"
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
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "SYSTEM TERMINAL SHELL (Type 'help' for instructions)");
            ImGui::Separator();

            // Terminal log scrollback area
            float footerHeightToReserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing(); 
            ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footerHeightToReserve), false, ImGuiWindowFlags_HorizontalScrollbar);
            
            // Stark border
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tight spacing
            for (const auto& item : g_ConsoleLog) {
                ImGui::TextColored(item.color, "%s", item.text.c_str());
            }
            if (g_TerminalScrollToBottom) {
                ImGui::SetScrollHereY(1.0f);
                g_TerminalScrollToBottom = false;
            }
            ImGui::PopStyleVar();
            ImGui::EndChild();

            ImGui::Separator();

            // Command input line
            ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory;
            ImGui::PushItemWidth(-FLT_MIN);
            if (ImGui::InputText("##Input", g_InputBuf, IM_ARRAYSIZE(g_InputBuf), inputFlags, &ConsoleInputCallback)) {
                std::string inputStr(g_InputBuf);
                if (!inputStr.empty()) {
                    ExecuteCommand(inputStr);
                }
                strcpy(g_InputBuf, ""); // Clear buffer
                ImGui::SetKeyboardFocusHere(-1); // Auto focus input field again
            }
            ImGui::PopItemWidth();
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
            }
            ImGui::EndChild();
        }
        else if (g_ActiveTab == 2) {
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

            float fontScale = g_MapScale / 16.0f;
            if (fontScale < 0.35f) fontScale = 0.35f;
            if (fontScale > 1.5f) fontScale = 1.5f;

            auto DrawScaledLabel = [&](ImVec2 pos, ImU32 color, const char* text, float minScaleToShow = 0.0f, bool layerToggle = true) {
                if (!g_ShowLabels || !layerToggle) return;
                if (g_MapScale < minScaleToShow) return;
                
                // Viewport boundary check before rendering text
                if (pos.x < canvasPos.x - 10.0f || pos.x > canvasPos.x + canvasSize.x + 10.0f ||
                    pos.y < canvasPos.y - 10.0f || pos.y > canvasPos.y + canvasSize.y + 10.0f) {
                    return;
                }
                
                float size = ImGui::GetFontSize() * fontScale;
                drawList->AddText(ImGui::GetFont(), size, pos, color, text);
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
                    if (ptCount > 0) {
                        float avgLon = sumLon / ptCount;
                        float avgLat = sumLat / ptCount;
                        ImVec2 labelPos = ProjectLonLat(avgLon, avgLat, canvasCenter, g_MapScale, g_MapOffset);
                        DrawScaledLabel(labelPos, IM_COL32(0, 200, 30, 150), country.name, 4.0f, g_ShowLabelsWorld);
                    }
                }
            }

            // Draw topographic contours (High-resolution Appalachians, Rockies, Cascades, Sierras)
            if (g_ShowContours) {
                for (int i = 0; i < US_Contours_Count; ++i) {
                    const auto& ct = US_Contours[i];
                    DrawMapLine(&US_Contours_Lon[ct.start_index], &US_Contours_Lat[ct.start_index], ct.count, IM_COL32(0, 120, 0, 60), 1.0f, false, canvasCenter);
                }
                
                // Draw text descriptors near ridges
                ImVec2 appCenter = ProjectLonLat(-77.0f, 40.0f, canvasCenter, g_MapScale, g_MapOffset);
                DrawScaledLabel(appCenter, IM_COL32(0, 180, 0, 90), "CONTOUR 1000m", 6.0f, g_ShowLabelsContours);
                
                ImVec2 rockCenter = ProjectLonLat(-110.0f, 42.0f, canvasCenter, g_MapScale, g_MapOffset);
                DrawScaledLabel(rockCenter, IM_COL32(0, 180, 0, 90), "CONTOUR 3000m", 6.0f, g_ShowLabelsContours);
            }

            // Draw hydrography lakes and rivers
            if (g_ShowLakes) {
                // Draw detailed lakes
                for (int i = 0; i < US_Lakes_Count; ++i) {
                    const auto& lake = US_Lakes[i];
                    for (int p = 0; p < lake.part_count; ++p) {
                        const auto& part = US_Lakes_Parts[lake.part_start + p];
                        DrawMapLine(&US_Lakes_Lon[part.start_index], &US_Lakes_Lat[part.start_index], part.count, IM_COL32(0, 100, 150, 140), 1.3f, true, canvasCenter);
                    }
                }
                
                // Draw high-resolution major rivers
                for (int i = 0; i < US_Rivers_Count; ++i) {
                    const auto& river = US_Rivers[i];
                    for (int p = 0; p < river.part_count; ++p) {
                        const auto& part = US_Rivers_Parts[river.part_start + p];
                        DrawMapLine(&US_Rivers_Lon[part.start_index], &US_Rivers_Lat[part.start_index], part.count, IM_COL32(0, 100, 150, 120), 1.2f, false, canvasCenter);
                    }
                }

                // River/Lake text labels
                ImVec2 supCenter = ProjectLonLat(-88.5f, 47.5f, canvasCenter, g_MapScale, g_MapOffset);
                DrawScaledLabel(supCenter, IM_COL32(0, 140, 170, 120), "L. SUPERIOR", 4.0f, g_ShowLabelsLakes);

                ImVec2 missCenter = ProjectLonLat(-90.5f, 35.1f, canvasCenter, g_MapScale, g_MapOffset);
                DrawScaledLabel(missCenter, IM_COL32(0, 140, 170, 120), "MISSISSIPPI R.", 5.0f, g_ShowLabelsLakes);
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
                        DrawScaledLabel(labelPos, IM_COL32(0, 220, 50, 130), state.name, 6.0f, g_ShowLabelsStates);
                    }
                }
            }

            // Draw roads (Interstate Highways)
            if (g_ShowHighways) {
                for (int i = 0; i < US_Highways_Count; ++i) {
                    const auto& hw = US_Highways[i];
                    DrawMapLine(&US_Highways_Lon[hw.start_index], &US_Highways_Lat[hw.start_index], hw.count, IM_COL32(0, 200, 50, 95), 1.2f, false, canvasCenter);
                    
                    if (hw.count > 0) {
                        int midIdx = hw.start_index + hw.count / 2;
                        ImVec2 labelPos = ProjectLonLat(US_Highways_Lon[midIdx], US_Highways_Lat[midIdx], canvasCenter, g_MapScale, g_MapOffset);
                        DrawScaledLabel(labelPos, IM_COL32(0, 240, 100, 180), hw.name, 8.0f, g_ShowLabelsHighways);
                    }
                }
            }

            // Draw secondary roads (US Highways)
            if (g_ShowUSHighways) {
                for (int i = 0; i < US_SecondaryHighways_Count; ++i) {
                    const auto& hw = US_SecondaryHighways[i];
                    DrawMapLine(&US_SecondaryHighways_Lon[hw.start_index], &US_SecondaryHighways_Lat[hw.start_index], hw.count, IM_COL32(0, 160, 40, 60), 0.9f, false, canvasCenter);
                    
                    if (hw.count > 0) {
                        int midIdx = hw.start_index + hw.count / 2;
                        ImVec2 labelPos = ProjectLonLat(US_SecondaryHighways_Lon[midIdx], US_SecondaryHighways_Lat[midIdx], canvasCenter, g_MapScale, g_MapOffset);
                        DrawScaledLabel(labelPos, IM_COL32(0, 180, 80, 150), hw.name, 12.0f, g_ShowLabelsUSHighways);
                    }
                }
            }

            // Draw railroads
            if (g_ShowRailways) {
                for (int i = 0; i < US_Railways_Count; ++i) {
                    const auto& rr = US_Railways[i];
                    DrawMapLine(&US_Railways_Lon[rr.start_index], &US_Railways_Lat[rr.start_index], rr.count, IM_COL32(0, 240, 200, 80), 1.1f, false, canvasCenter);
                    
                    if (rr.count > 0) {
                        int midIdx = rr.start_index + rr.count / 2;
                        ImVec2 labelPos = ProjectLonLat(US_Railways_Lon[midIdx], US_Railways_Lat[midIdx], canvasCenter, g_MapScale, g_MapOffset);
                        DrawScaledLabel(labelPos, IM_COL32(0, 200, 180, 160), rr.name, 10.0f, g_ShowLabelsRailways);
                    }
                }
            }

            // Draw gas/oil pipelines
            if (g_ShowPipelines) {
                for (int i = 0; i < US_Pipelines_Count; ++i) {
                    const auto& pl = US_Pipelines[i];
                    DrawMapLine(&US_Pipelines_Lon[pl.start_index], &US_Pipelines_Lat[pl.start_index], pl.count, IM_COL32(0, 150, 200, 85), 1.2f, false, canvasCenter);
                    
                    if (pl.count > 0) {
                        int midIdx = pl.start_index + pl.count / 2;
                        ImVec2 labelPos = ProjectLonLat(US_Pipelines_Lon[midIdx], US_Pipelines_Lat[midIdx], canvasCenter, g_MapScale, g_MapOffset);
                        DrawScaledLabel(labelPos, IM_COL32(0, 130, 180, 160), pl.name, 11.0f, g_ShowLabelsPipelines);
                    }
                }
            }

            // Draw energy corridors (HV transmission lines)
            if (g_ShowEnergyCorridors) {
                for (int i = 0; i < US_EnergyCorridors_Count; ++i) {
                    const auto& ec = US_EnergyCorridors[i];
                    DrawMapLine(&US_EnergyCorridors_Lon[ec.start_index], &US_EnergyCorridors_Lat[ec.start_index], ec.count, IM_COL32(0, 220, 220, 110), 1.3f, false, canvasCenter);
                    
                    if (ec.count > 0) {
                        int midIdx = ec.start_index + ec.count / 2;
                        ImVec2 labelPos = ProjectLonLat(US_EnergyCorridors_Lon[midIdx], US_EnergyCorridors_Lat[midIdx], canvasCenter, g_MapScale, g_MapOffset);
                        DrawScaledLabel(labelPos, IM_COL32(0, 190, 190, 160), ec.name, 10.0f, g_ShowLabelsEnergyCorridors);
                    }
                }
            }

            // Draw substations
            if (g_ShowSubstations) {
                for (int i = 0; i < US_Substations_Count; ++i) {
                    const auto& sub = US_Substations[i];
                    ImVec2 p = ProjectLonLat(sub.lon, sub.lat, canvasCenter, g_MapScale, g_MapOffset);
                    
                    if (p.x < canvasPos.x - 3.0f || p.x > canvasPos.x + canvasSize.x + 3.0f ||
                        p.y < canvasPos.y - 3.0f || p.y > canvasPos.y + canvasSize.y + 3.0f) {
                        continue;
                    }
                    
                    drawList->AddLine(ImVec2(p.x - 3, p.y), ImVec2(p.x + 3, p.y), IM_COL32(0, 220, 220, 180), 1.0f);
                    drawList->AddLine(ImVec2(p.x, p.y - 3), ImVec2(p.x, p.y + 3), IM_COL32(0, 220, 220, 180), 1.0f);
                    
                    float dx = io.MousePos.x - p.x;
                    float dy = io.MousePos.y - p.y;
                    if (hovered && sqrtf(dx * dx + dy * dy) < 4.0f) {
                        ImGui::SetTooltip("%s", sub.name);
                    }
                    
                    DrawScaledLabel(ImVec2(p.x + 6, p.y - 4), IM_COL32(0, 180, 180, 150), sub.name, 22.0f, g_ShowLabelsSubstations);
                }
            }

            // Draw power stations
            if (g_ShowPowerPlants) {
                for (int i = 0; i < US_PowerStations_Count; ++i) {
                    const auto& pp = US_PowerStations[i];
                    ImVec2 p = ProjectLonLat(pp.lon, pp.lat, canvasCenter, g_MapScale, g_MapOffset);
                    
                    float radius = 2.5f + sqrtf(pp.capacity) * 0.08f;
                    if (radius > 9.0f) radius = 9.0f;
                    
                    if (p.x < canvasPos.x - radius || p.x > canvasPos.x + canvasSize.x + radius ||
                        p.y < canvasPos.y - radius || p.y > canvasPos.y + canvasSize.y + radius) {
                        continue;
                    }
                    
                    ImU32 color = IM_COL32(0, 255, 100, 200); // Default NG/Gas: Light Green
                    if (strcmp(pp.fuel, "NUC") == 0) color = IM_COL32(255, 100, 0, 220); // Nuclear: Orange
                    else if (strcmp(pp.fuel, "HYC") == 0 || strcmp(pp.fuel, "WAT") == 0) color = IM_COL32(0, 150, 255, 200); // Hydro: Blue
                    else if (strcmp(pp.fuel, "COL") == 0) color = IM_COL32(180, 100, 255, 200); // Coal: Purple
                    
                    drawList->AddCircleFilled(p, radius, color);
                    drawList->AddCircle(p, radius + 2.0f, IM_COL32(0, 255, 100, 80), 8, 1.0f);
                    
                    float dx = io.MousePos.x - p.x;
                    float dy = io.MousePos.y - p.y;
                    if (hovered && sqrtf(dx * dx + dy * dy) < radius + 2.0f) {
                        ImGui::SetTooltip("%s\nFuel: %s | Capacity: %.1f MW", pp.name, pp.fuel, pp.capacity);
                    }
                    
                    if (g_MapScale >= 15.0f) {
                        char labelText[128];
                        snprintf(labelText, sizeof(labelText), "%s (%.0f MW)", pp.name, pp.capacity);
                        DrawScaledLabel(ImVec2(p.x + radius + 4, p.y - 4), color, labelText, 15.0f, g_ShowLabelsPowerPlants);
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

                    // Draw pin
                    drawList->AddCircleFilled(p, isSelected ? 5.5f : 4.0f, dotColor);
                    drawList->AddCircle(p, isSelected ? 9.0f : 7.0f, ringColor, 16, 1.0f);
                    
                    // Hover detection
                    float dx = io.MousePos.x - p.x;
                    float dy = io.MousePos.y - p.y;
                    if (hovered && sqrtf(dx * dx + dy * dy) < 9.0f) {
                        ImGui::SetTooltip("%s\nClick to select station.", city.name.c_str());
                        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                            g_SelectedCity = (int)i;
                        }
                    }

                    // Label offset text
                    DrawScaledLabel(ImVec2(p.x + 10, p.y - 7), isSelected ? IM_COL32(255, 220, 0, 240) : IM_COL32(0, 240, 50, 190), city.name.c_str(), 0.0f, g_ShowLabelsCities);
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

            ImGui::EndChild();
        }

        else if (g_ActiveTab == 3) {
            // ==========================================
            // TAB 3: DIAGNOSTICS & SYSTEM METRICS
            // ==========================================
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "WASM WORKSPACE COMPILATION METRICS & CORE LOADS");
            ImGui::Separator();

            ImGui::Text("Emscripten compiler generated memory layouts, thread pools, and processing loops:");
            ImGui::Spacing();

            // Real-time scrolling plot lines
            float plotWidth = io.DisplaySize.x - leftPanelWidth - 60.0f;
            if (plotWidth < 300.0f) plotWidth = 300.0f;

            ImGui::Text("Virtual CPU Execution Load (fluctuating %%):");
            ImGui::PlotLines("##CPUPlot", g_CpuHistory.data(), (int)g_CpuHistory.size(), 0, nullptr, 0.0f, 100.0f, ImVec2(plotWidth, 110.0f));

            ImGui::Text("Virtual Heap Allocation Size (MB):");
            ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.0f, 0.8f, 1.0f, 1.0f));
            ImGui::PlotLines("##RAMPlot", g_RamHistory.data(), (int)g_RamHistory.size(), 0, nullptr, 0.0f, 100.0f, ImVec2(plotWidth, 110.0f));
            ImGui::PopStyleColor(1); // reset

            ImGui::Text("WASM Port Pipeline Network IO (B/s):");
            ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(1.0f, 0.8f, 0.0f, 1.0f));
            ImGui::PlotLines("##NetPlot", g_NetworkHistory.data(), (int)g_NetworkHistory.size(), 0, nullptr, 0.0f, 100.0f, ImVec2(plotWidth, 110.0f));
            ImGui::PopStyleColor(1); // reset
        }
        else if (g_ActiveTab == 4) {
            // ==========================================
            // TAB 4: INTRO & CREDITS
            // ==========================================
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "WASM GRAPHICS SHADER PIPELINE - ABOUT");
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
