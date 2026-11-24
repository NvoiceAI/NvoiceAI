# Build script for NvoiceAI SDK on Windows using CMake and vcpkg
# This script handles:
#   - Clean build setup
#   - CMake configuration with vcpkg toolchain
#   - Project compilation
#   - Library deployment

param(
    [string]$BuildType = "Release",
    [string]$VcpkgDir = "",
    [switch]$Clean = $false,
    [switch]$Help
)

if ($Help) {
    Write-Host "NvoiceAI Build Script for Windows" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Usage:" -ForegroundColor Yellow
    Write-Host "  .\build.ps1 -VcpkgDir C:\path\to\vcpkg                # Incremental build (fast)" -ForegroundColor White
    Write-Host "  .\build.ps1 -VcpkgDir C:\path\to\vcpkg -Clean         # Clean build (full rebuild)" -ForegroundColor White
    Write-Host "  .\build.ps1 -VcpkgDir C:\path\to\vcpkg -BuildType Debug # Build in Debug mode" -ForegroundColor White

    Write-Host "  .\build.ps1 -Help                                      # Show this help message" -ForegroundColor White
    Write-Host ""
    Write-Host "Build Modes:" -ForegroundColor Yellow
    Write-Host "  (default)     Incremental build - only rebuilds changed files (fast)" -ForegroundColor White
    Write-Host "  -Clean        Clean build - removes build directory and rebuilds everything (slow)" -ForegroundColor White
    Write-Host ""
    Write-Host "Parameters:" -ForegroundColor Yellow
    Write-Host "  -VcpkgDir <path>        Specify vcpkg root directory (required)" -ForegroundColor White
    Write-Host "  -BuildType <type>       Build type: Release (default) or Debug" -ForegroundColor White
    Write-Host ""
    Write-Host "Environment Setup:" -ForegroundColor Yellow
    Write-Host "  - Requires CMake to be installed" -ForegroundColor White
    Write-Host "  - Requires -VcpkgDir parameter pointing to vcpkg installation" -ForegroundColor White
    Write-Host "  - Copies libraries to build output directory for runtime" -ForegroundColor White
    exit 0
}

# Define paths
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $ScriptDir "build"

# ==================== Validate vcpkg Root ====================

if (-not $VcpkgDir -or $VcpkgDir -eq "") {
    Write-Host "ERROR: vcpkg root path must be specified via -VcpkgDir parameter" -ForegroundColor Red
    Write-Host ""
    Write-Host "Usage: .\build.ps1 -VcpkgDir C:\path\to\vcpkg" -ForegroundColor Yellow
    exit 1
}

if (-not (Test-Path $VcpkgDir)) {
    Write-Host "ERROR: vcpkg root path not found: $VcpkgDir" -ForegroundColor Red
    exit 1
}

$VcpkgExe = Join-Path $VcpkgDir "vcpkg.exe"
if (-not (Test-Path $VcpkgExe)) {
    Write-Host "ERROR: vcpkg.exe not found at: $VcpkgExe" -ForegroundColor Red
    exit 1
}

$VcpkgToolchain = Join-Path $VcpkgDir "scripts\buildsystems\vcpkg.cmake"
$PkgConfigExe = Join-Path $VcpkgDir "installed\x64-windows\tools\pkgconf\pkgconf.exe"

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "NvoiceAI SDK - Windows Build Script" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host ""
# Display build configuration
Write-Host "Build Configuration:" -ForegroundColor Blue
Write-Host "  Build Type: $BuildType" -ForegroundColor White
Write-Host "  Build Mode: $(if ($Clean) { 'Clean (full rebuild)' } else { 'Incremental (fast)' })" -ForegroundColor White
Write-Host "  vcpkg Root: $VcpkgDir" -ForegroundColor White
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

Write-Host "  CMake found" -ForegroundColor Green
Write-Host "  vcpkg toolchain found" -ForegroundColor Green
Write-Host ""

# ==================== vcpkg Package Management ====================

Write-Host "Checking vcpkg packages..." -ForegroundColor Blue

$RequiredPackages = @("pkgconf", "spdlog", "portaudio", "libsndfile", "abseil")

# Get list of installed packages
$InstalledPackages = & $VcpkgExe list 2>&1

foreach ($package in $RequiredPackages) {
    $IsInstalled = $InstalledPackages | Where-Object { $_ -match "^$package\s" }
    
    if ($IsInstalled) {
        Write-Host "  $package : Found" -ForegroundColor Green
    }
    else {
        Write-Host "  $package : MISSING - Installing..." -ForegroundColor Yellow
        & $VcpkgExe install "$package`:x64-windows"
        
        if ($LASTEXITCODE -ne 0) {
            Write-Host "ERROR: Failed to install $package. vcpkg exited with code $LASTEXITCODE" -ForegroundColor Red
            Write-Host "This may be a temporary network issue. Try running the script again." -ForegroundColor Yellow
            exit 1
        }
        Write-Host "  $package : Installed successfully" -ForegroundColor Green
    }
}

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
    $CmakeArgs = @(
        "..",
        "-DCMAKE_TOOLCHAIN_FILE=`"$VcpkgToolchain`"",
        "-DPKG_CONFIG_EXECUTABLE=`"$PkgConfigExe`""
    )
    
    & cmake @CmakeArgs

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
Write-Host "  Also building: nvoiceai_realtime_sherpa_asr and nvoiceai_realtime_sherpa_asr_c" -ForegroundColor Yellow
Write-Host "  Note: nvoiceai_realtime_simple_c requires additional Windows compatibility work" -ForegroundColor Gray
Write-Host "  Using parallel build for faster compilation..." -ForegroundColor Gray

try {
    $BuildTargets = @("nvoiceai_realtime_simple", "offline_audio_processor", "nvoiceai_realtime_sherpa_asr", "nvoiceai_realtime_sherpa_asr_c")
    
    $BuildArgs = @("--build", ".", "--config", $BuildType)
    foreach ($target in $BuildTargets) {
        $BuildArgs += "--target"
        $BuildArgs += $target
    }
    $BuildArgs += "--parallel"
    
    & cmake @BuildArgs

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
        Write-Host "    Copied $($dll.Name) to $ExeDir" -ForegroundColor Gray
    }
    
    if ($AllDlls.Count -gt 0) {
        Write-Host "  Deployed $($AllDlls.Count) libraries" -ForegroundColor Green
    }
    else {
        Write-Host "  WARNING: No DLLs found in $LibDir" -ForegroundColor Yellow
    }
    
    # Sherpa-ONNX DLL deployment
    Write-Host "  Copying Sherpa-ONNX runtime libraries..." -ForegroundColor Yellow
    $SherpaLibDir = Join-Path $ScriptDir "third_party\sherpa-onnx-v2.0.1-win-x64-shared-MD-Release-lib\sherpa-onnx-v2.0.1-win-x64-shared-MD-Release-lib\lib"
    $SherpaDlls = @(Get-ChildItem -Path $SherpaLibDir -Filter "*.dll" -ErrorAction SilentlyContinue)
    foreach ($dll in $SherpaDlls) {
        $destPath = Join-Path $ExeDir $dll.Name
        Copy-Item -Path $dll.FullName -Destination $destPath -Force
        Write-Host "    Copied $($dll.Name) to $ExeDir" -ForegroundColor Gray
    }
    if ($SherpaDlls.Count -gt 0) {
        Write-Host "  Deployed $($SherpaDlls.Count) Sherpa-ONNX libraries" -ForegroundColor Green
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
Write-Host "  $BuildDir\Release\nvoiceai_realtime_simple.exe" -ForegroundColor White
Write-Host "  $BuildDir\Release\nvoiceai_realtime_sherpa_asr.exe" -ForegroundColor White
Write-Host "  $BuildDir\Release\nvoiceai_realtime_sherpa_asr_c.exe" -ForegroundColor White
Write-Host ""
