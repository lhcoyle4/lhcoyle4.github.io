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
        ImGui::TextWrapped("Bio: BSCS + GIS Master's Cert. Portland, ME. Drone pilot, Python automations, C/C++ programmer.");

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
            ImGui::Separator();
            if (ImGui::Button("View Full GIS Map Portfolio Repo", ImVec2(-FLT_MIN, 40.0f))) {
                OpenGitHubLink("https://github.com/lhcoyle4/gis-portfolio");
            }
            ImGui::EndChild();

            ImGui::SameLine();

            // Interactive vector radar visualization panel
            ImGui::BeginChild("GisRadar", ImVec2(0, 0), true);
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "USGS GPS TELEMETRY & COORDINATE PING ACTIVE");
            ImGui::Separator();

            // Draw a spinning radar grid using ImGui DrawList!
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 canvasPos = ImGui::GetCursorScreenPos();
            ImVec2 canvasSize = ImGui::GetContentRegionAvail();
            
            float radarRadius = std::min(canvasSize.x, canvasSize.y) * 0.45f;
            ImVec2 radarCenter = ImVec2(canvasPos.x + canvasSize.x * 0.5f, canvasPos.y + canvasSize.y * 0.5f);

            // Background radar circles
            drawList->AddCircle(radarCenter, radarRadius, IM_COL32(0, 150, 0, 50), 32, 1.0f);
            drawList->AddCircle(radarCenter, radarRadius * 0.66f, IM_COL32(0, 150, 0, 40), 32, 1.0f);
            drawList->AddCircle(radarCenter, radarRadius * 0.33f, IM_COL32(0, 150, 0, 30), 32, 1.0f);
            
            // Grid lines
            drawList->AddLine(ImVec2(radarCenter.x - radarRadius, radarCenter.y), ImVec2(radarCenter.x + radarRadius, radarCenter.y), IM_COL32(0, 150, 0, 60));
            drawList->AddLine(ImVec2(radarCenter.x, radarCenter.y - radarRadius), ImVec2(radarCenter.x, radarCenter.y + radarRadius), IM_COL32(0, 150, 0, 60));

            // Spin sweep line
            g_RadarAngle += 0.02f;
            float lineX = radarRadius * cosf(g_RadarAngle);
            float lineY = radarRadius * sinf(g_RadarAngle);
            drawList->AddLine(radarCenter, ImVec2(radarCenter.x + lineX, radarCenter.y + lineY), IM_COL32(0, 255, 50, 200), 2.0f);

            // Shaded sweep wedge
            for (int i = 0; i < 30; i++) {
                float trailAngle = g_RadarAngle - i * 0.015f;
                float tx = radarRadius * cosf(trailAngle);
                float ty = radarRadius * sinf(trailAngle);
                drawList->AddLine(radarCenter, ImVec2(radarCenter.x + tx, radarCenter.y + ty), IM_COL32(0, 200, 50, (255 - i * 8)));
            }

            // Radar coordinates pins
            for (size_t i = 0; i < g_RadarPins.size(); ++i) {
                // Scale coordinates to fit radar circle
                float px = radarCenter.x + (g_RadarPins[i].x - 100.0f) * (radarRadius / 100.0f);
                float py = radarCenter.y + (g_RadarPins[i].y - 100.0f) * (radarRadius / 100.0f);
                
                // Animate alpha based on radar rotation
                float angleToPin = atan2f(py - radarCenter.y, px - radarCenter.x);
                if (angleToPin < 0) angleToPin += 2.0f * 3.14159f;
                float currentSweepAngle = fmodf(g_RadarAngle, 2.0f * 3.14159f);
                float diff = fmodf(currentSweepAngle - angleToPin + 2.0f * 3.14159f, 2.0f * 3.14159f);
                
                int alpha = 0;
                if (diff < 1.0f) {
                    alpha = (int)(255 * (1.0f - diff));
                } else {
                    alpha = 40; // faint glow
                }

                drawList->AddCircleFilled(ImVec2(px, py), 5.0f, IM_COL32(0, 255, 30, alpha));
                drawList->AddCircle(ImVec2(px, py), 9.0f, IM_COL32(0, 255, 30, alpha / 2), 16, 1.0f);
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
