# lhcoyle4.github.io (WASM Portfolio)

This repository hosts my personal homepage and developer portfolio, built entirely from scratch in C++ and compiled to WebAssembly (WASM).

Visit the live page here: **[https://lhcoyle4.github.io/](https://lhcoyle4.github.io/)**

## Engineering Philosophy & Architecture
Modern web design is often bogged down by heavy frameworks (React, Angular), huge JS bundles, and DOM paint lag. This project follows the philosophy of **mechanical sympathy** and **stark zero-dependency systems engineering**:

* **Zero-DOM Rendering**: Bypasses the HTML Document Object Model entirely. The UI is drawn using the **Dear ImGui** immediate-mode framework and rendered directly to a WebGL2 canvas via pixel shaders.
* **Low-Level Code**: Written in pure C++17, compiling into highly efficient WASM bytecode.
* **Locked 60 FPS Performance**: Achieves native desktop-grade smoothness and sub-millisecond execution times.
* **CRT Retro Dashboard Style**: Features an interactive retro shell terminal (complete with commands), real-time virtual CPU/RAM plots, project listings with custom flow diagrams, and a GIS coordinates radar scan simulation.

## Repository Layout
* `/src/main.cpp`: Main C++ application implementing tabs, state, shell, and custom graphics.
* `/src/shell.html`: Custom retro-themed HTML shell with WebAssembly compilation loading bar.
* `/imgui/`: Embedded Dear ImGui core rendering files and SDL2/OpenGL backends.
* `build_wasm.ps1`: PowerShell build automation pipeline configuring EMSDK and calling the compiler.
* `index.html`, `index.js`, `index.wasm`: The compiled WebAssembly distribution files served by GitHub Pages.

## Building Locally
To build this project from source, you need to have the Emscripten SDK installed.

1. Install and activate Emscripten (ensure `emsdk` is in the parent directory or update `build_wasm.ps1` path):
   ```powershell
   git clone https://github.com/emscripten-core/emsdk.git
   cd emsdk
   ./emsdk install latest
   ./emsdk activate latest
   ```
2. Run the build script in this directory:
   ```powershell
   ./build_wasm.ps1
   ```
3. Test locally using a simple web server:
   ```powershell
   python -m http.server
   ```
   Open `http://localhost:8000` in your web browser.
