# build_wasm.ps1
# Compiles the C++ ImGui codebase to WebAssembly using Emscripten

Write-Host "=============================================" -ForegroundColor Cyan
Write-Host "LCOYLE4 WASM BUILD PIPELINE" -ForegroundColor Cyan
Write-Host "=============================================" -ForegroundColor Cyan

# Source the Emscripten environment variables
$emsdkDir = Resolve-Path "..\emsdk"
Write-Host "Locating Emscripten environment..." -ForegroundColor Yellow
if (Test-Path "$emsdkDir\emsdk_env.ps1") {
    # Execute the environment setup script
    . "$emsdkDir\emsdk_env.ps1"
} else {
    Write-Error "Could not find emsdk_env.ps1 in $emsdkDir!"
    exit 1
}

# Verify em++ is in PATH
if (!(Get-Command em++ -ErrorAction SilentlyContinue)) {
    Write-Error "em++ compiler not found in PATH! Make sure SDK is activated."
    exit 1
}

Write-Host "Compilation starting..." -ForegroundColor Yellow

# Construct the full command line string for cmd.exe
$cmdLine = "em++ -Os src/main.cpp imgui/imgui.cpp imgui/imgui_draw.cpp imgui/imgui_demo.cpp imgui/imgui_tables.cpp imgui/imgui_widgets.cpp imgui/backends/imgui_impl_sdl2.cpp imgui/backends/imgui_impl_opengl3.cpp -Iimgui -Iimgui/backends -s USE_SDL=2 -s USE_WEBGL2=1 -s MAX_WEBGL_VERSION=2 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s DISABLE_EXCEPTION_CATCHING=1 -s NO_EXIT_RUNTIME=0 -s ASSERTIONS=0 -s NO_FILESYSTEM=1 -DIMGUI_DISABLE_FILE_FUNCTIONS -DIMGUI_IMPL_OPENGL_ES3 -DIMGUI_USE_32BIT_VERTICES --shell-file src/shell.html -o index.html"

Write-Host "Executing em++ via cmd.exe..." -ForegroundColor Yellow
cmd.exe /c $cmdLine

if ($LASTEXITCODE -ne 0) {
    Write-Host "=============================================" -ForegroundColor Red
    Write-Host "COMPILATION FAILED! Check error log." -ForegroundColor Red
    Write-Host "=============================================" -ForegroundColor Red
    exit $LASTEXITCODE
}

# Post-processing: Add cache-busting timestamp to index.js script tag in index.html
$htmlFile = "index.html"
if (Test-Path $htmlFile) {
    $content = Get-Content $htmlFile -Raw
    $timestamp = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
    $content = $content -replace 'index\.js', "index.js?v=$timestamp"
    Set-Content $htmlFile $content
    Write-Host "Cache-busted index.html script tag with timestamp v=$timestamp" -ForegroundColor Green
}

Write-Host "=============================================" -ForegroundColor Green
Write-Host "COMPILATION SUCCESSFUL!" -ForegroundColor Green
Write-Host "Generated: index.html, index.js, index.wasm" -ForegroundColor Green
Write-Host "=============================================" -ForegroundColor Green
