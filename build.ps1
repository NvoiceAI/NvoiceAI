# Build script for NvoiceAI SDK on Windows using CMake and vcpkg
# This script handles:
#   - Clean build setup
#   - CMake configuration with vcpkg toolchain
#   - Project compilation
#   - Library deployment

param(
    [string]$BuildType = "Release",
    [switch]$Clean = $false,
    [switch]$Help
)

if ($Help) {
    Write-Host "NvoiceAI Build Script for Windows" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Usage:" -ForegroundColor Yellow
    Write-Host "  .\.\build.ps1                      # Incremental build (fast, reuses previous build)" -ForegroundColor White
    Write-Host "  .\.\build.ps1 -Clean               # Clean build (removes all artifacts and rebuilds)" -ForegroundColor White
    Write-Host "  .\.\build.ps1 -BuildType Debug     # Build in Debug mode" -ForegroundColor White
    Write-Host "  .\.\build.ps1 -Help                # Show this help message" -ForegroundColor White
    Write-Host ""
    Write-Host "Build Modes:" -ForegroundColor Yellow
    Write-Host "  (default)     Incremental build - only rebuilds changed files (fast)" -ForegroundColor White
    Write-Host "  -Clean        Clean build - removes build directory and rebuilds everything (slow)" -ForegroundColor White
    Write-Host ""
    Write-Host "Environment Setup:" -ForegroundColor Yellow
    Write-Host "  - Requires CMake to be installed" -ForegroundColor White
    Write-Host "  - Uses vcpkg toolchain from: C:\\Users\\zheng\\workspace\\vcpkg" -ForegroundColor White
    Write-Host "  - Copies libraries to build output directory for runtime" -ForegroundColor White
    exit 0
}

# Define paths
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $ScriptDir "build"
$VcpkgRoot = "C:\Users\zheng\workspace\vcpkg"
$VcpkgToolchain = Join-Path $VcpkgRoot "scripts\buildsystems\vcpkg.cmake"
$PkgConfigExe = Join-Path $VcpkgRoot "installed\x64-windows\tools\pkgconf\pkgconf.exe"

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "NvoiceAI SDK - Windows Build Script" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host ""
# Display build configuration
Write-Host "Build Configuration:" -ForegroundColor Blue
Write-Host "  Build Type: $BuildType" -ForegroundColor White
Write-Host "  Build Mode: $(if ($Clean) { 'Clean (full rebuild)' } else { 'Incremental (fast)' })" -ForegroundColor White
Write-Host ""
# Validate dependencies
Write-Host "Checking dependencies..." -ForegroundColor Blue

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Host "ERROR: CMake not found. Please install CMake." -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $VcpkgToolchain)) {
    Write-Host "ERROR: vcpkg toolchain not found at $VcpkgToolchain" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $PkgConfigExe)) {
    Write-Host "ERROR: pkgconf not found at $PkgConfigExe" -ForegroundColor Red
    exit 1
}

Write-Host "  CMake found" -ForegroundColor Green
Write-Host "  vcpkg toolchain found" -ForegroundColor Green
Write-Host "  pkgconf found" -ForegroundColor Green
Write-Host ""

# Clean build directory
Write-Host "Preparing build directory..." -ForegroundColor Blue
if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "  Removing existing build directory (Clean mode)..." -ForegroundColor Yellow
    try {
        Remove-Item -Path $BuildDir -Recurse -Force -ErrorAction Stop
    }
    catch {
        Write-Host "  WARNING: Could not remove build directory - it may be in use. Continuing with existing directory..." -ForegroundColor Yellow
    }
}

if (-not (Test-Path $BuildDir)) {
    Write-Host "  Creating fresh build directory..." -ForegroundColor Yellow
    New-Item -Path $BuildDir -ItemType Directory | Out-Null
}

if ($Clean) {
    Write-Host "  Build directory ready (Clean mode)" -ForegroundColor Green
}
else {
    Write-Host "  Build directory ready (Incremental mode)" -ForegroundColor Green
}
Write-Host ""

# Configure with CMake
Write-Host "Configuring CMake..." -ForegroundColor Blue
Push-Location $BuildDir

try {
    & cmake .. -DCMAKE_TOOLCHAIN_FILE="$VcpkgToolchain" -DPKG_CONFIG_EXECUTABLE="$PkgConfigExe"

    if ($LASTEXITCODE -ne 0) {
        Write-Host "CMake configuration failed!" -ForegroundColor Red
        exit 1
    }
    Write-Host "  CMake configuration complete" -ForegroundColor Green
}
catch {
    Write-Host "ERROR: CMake configuration failed: $_" -ForegroundColor Red
    exit 1
}

Write-Host ""

# Build the project
Write-Host "Building project..." -ForegroundColor Blue
Write-Host "  Building targets: nvoiceai_realtime_simple and offline_audio_processor" -ForegroundColor Yellow
Write-Host "  Note: nvoiceai_realtime_simple_c requires additional Windows compatibility work" -ForegroundColor Gray
Write-Host "  Using parallel build for faster compilation..." -ForegroundColor Gray

try {
    & cmake --build . --config $BuildType --target nvoiceai_realtime_simple --target offline_audio_processor --parallel

    if ($LASTEXITCODE -ne 0) {
        Write-Host "Build failed!" -ForegroundColor Red
        exit 1
    }
    Write-Host "  Build complete" -ForegroundColor Green
}
catch {
    Write-Host "ERROR: Build failed: $_" -ForegroundColor Red
    exit 1
}

Write-Host ""

# Deploy libraries
Write-Host "Deploying libraries..." -ForegroundColor Blue

$OutputDir = Join-Path $BuildDir $BuildType
$LibDir = Join-Path $ScriptDir "lib\win"

# Find the nvoiceai_realtime_simple.exe
$ExeFiles = @(Get-ChildItem -Path $OutputDir -Filter "nvoiceai_realtime_simple.exe" -Recurse -ErrorAction SilentlyContinue)

if ($ExeFiles.Count -gt 0) {
    $ExeDir = Split-Path -Parent $ExeFiles[0].FullName
    Write-Host "  Found executable at: $ExeDir" -ForegroundColor Yellow
    
    # Copy essential runtime dependencies
    # The example programs require these DLLs:
    # - nvoiceai_sdk.dll: Main SDK
    # - webrtc-audio-processing-2-1.dll: Audio processing (used by SDK)
    # - portaudio.dll: Audio device I/O (used by programs for capture)
    # - sndfile.dll: File I/O (used by programs for WAV file operations)
    # Note: Codec DLLs (FLAC, MP3, Opus, Vorbis, etc.) are not needed for WAV-only operations
    
    # Copy all DLLs from vcpkg lib directory to Release folder
    Write-Host "  Copying all runtime libraries..." -ForegroundColor Yellow
    $AllDlls = @(Get-ChildItem -Path $LibDir -Filter "*.dll" -ErrorAction SilentlyContinue)
    foreach ($dll in $AllDlls) {
        $destPath = Join-Path $ExeDir $dll.Name
        Copy-Item -Path $dll.FullName -Destination $destPath -Force
    }
    
    if ($AllDlls.Count -gt 0) {
        Write-Host "  Deployed $($AllDlls.Count) libraries" -ForegroundColor Green
    }
    else {
        Write-Host "  WARNING: No DLLs found in $LibDir" -ForegroundColor Yellow
    }
}
else {
    Write-Host "  WARNING: nvoiceai_realtime_simple.exe not found. Skipping library deployment." -ForegroundColor Yellow
}

Pop-Location

Write-Host ""
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "Build Successful!" -ForegroundColor Green
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Build artifacts location:" -ForegroundColor Yellow
Write-Host "  $BuildDir" -ForegroundColor White
Write-Host ""
Write-Host "To run the application, use:" -ForegroundColor Yellow
$RunPath = $BuildDir + "\Release\nvoiceai_realtime_simple.exe"
Write-Host "  $RunPath" -ForegroundColor White
Write-Host ""
Write-Host "Performance Tip:" -ForegroundColor Cyan
Write-Host "  - Use incremental builds by default (fast - only rebuilds changed files)" -ForegroundColor White
Write-Host "  - Use -Clean only when you need a fresh build (full rebuild)" -ForegroundColor White
Write-Host ""
