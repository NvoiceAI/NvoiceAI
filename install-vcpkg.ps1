# vcpkg Installation Script for Windows
# This script clones vcpkg from GitHub and builds it for use with CMake

param(
    [string]$VcpkgDir = "vcpkg",
    [switch]$Help
)

if ($Help) {
    Write-Host "vcpkg Installation Script" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Usage:" -ForegroundColor Yellow
    Write-Host "  .\install-vcpkg.ps1                       # Install vcpkg in ./vcpkg directory" -ForegroundColor White
    Write-Host "  .\install-vcpkg.ps1 -VcpkgDir C:\vcpkg    # Install vcpkg in specified directory" -ForegroundColor White
    Write-Host "  .\install-vcpkg.ps1 -Help                 # Show this help message" -ForegroundColor White
    Write-Host ""
    Write-Host "Description:" -ForegroundColor Yellow
    Write-Host "  Clones vcpkg from GitHub and builds it for use with CMake projects." -ForegroundColor White
    Write-Host "  vcpkg is used for C++ package management on Windows." -ForegroundColor White
    Write-Host ""
    Write-Host "Requirements:" -ForegroundColor Yellow
    Write-Host "  - Git installed and in PATH" -ForegroundColor White
    Write-Host "  - Visual Studio Build Tools or Visual Studio Community" -ForegroundColor White
    Write-Host "  - Internet connection for cloning repository" -ForegroundColor White
    exit 0
}

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "vcpkg Installation Script" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host ""

# Check if Git is installed
Write-Host "Checking dependencies..." -ForegroundColor Blue
if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    Write-Host "ERROR: Git not found. Please install Git and add it to PATH." -ForegroundColor Red
    exit 1
}
Write-Host "  Git found" -ForegroundColor Green

# Check if vcpkg already exists
if (Test-Path $VcpkgDir) {
    Write-Host ""
    Write-Host "vcpkg directory already exists at: $VcpkgDir" -ForegroundColor Yellow
    Write-Host "  Skipping clone. Updating existing installation..." -ForegroundColor Yellow
    
    Push-Location $VcpkgDir
    try {
        Write-Host ""
        Write-Host "Pulling latest changes from GitHub..." -ForegroundColor Blue
        & git pull
        
        if ($LASTEXITCODE -ne 0) {
            Write-Host "WARNING: Git pull failed. Continuing with existing installation." -ForegroundColor Yellow
        }
    }
    catch {
        Write-Host "WARNING: Failed to update: $_" -ForegroundColor Yellow
    }
    Pop-Location
}
else {
    Write-Host "  vcpkg directory does not exist" -ForegroundColor White
    Write-Host ""
    
    # Clone vcpkg repository
    Write-Host "Cloning vcpkg from GitHub..." -ForegroundColor Blue
    Write-Host "  Source: https://github.com/Microsoft/vcpkg.git" -ForegroundColor White
    Write-Host "  Destination: $VcpkgDir" -ForegroundColor White
    Write-Host ""
    
    try {
        & git clone https://github.com/Microsoft/vcpkg.git $VcpkgDir
        
        if ($LASTEXITCODE -ne 0) {
            Write-Host "ERROR: Failed to clone vcpkg repository" -ForegroundColor Red
            exit 1
        }
        Write-Host "  Clone completed successfully" -ForegroundColor Green
    }
    catch {
        Write-Host "ERROR: Clone failed: $_" -ForegroundColor Red
        exit 1
    }
}

Write-Host ""

# Build vcpkg
Write-Host "Building vcpkg..." -ForegroundColor Blue

$VcpkgExe = Join-Path $VcpkgDir "vcpkg.exe"
$BootstrapScript = Join-Path $VcpkgDir "bootstrap-vcpkg.bat"

if (-not (Test-Path $BootstrapScript)) {
    Write-Host "ERROR: bootstrap-vcpkg.bat not found at $BootstrapScript" -ForegroundColor Red
    Write-Host "  vcpkg repository may not be properly cloned." -ForegroundColor Yellow
    exit 1
}

# Check if vcpkg.exe already exists
if (Test-Path $VcpkgExe) {
    Write-Host "  vcpkg.exe already built" -ForegroundColor Green
}
else {
    Write-Host "  Running bootstrap script..." -ForegroundColor Yellow
    
    try {
        Push-Location $VcpkgDir
        
        # Run the bootstrap script
        & cmd.exe /c $BootstrapScript
        
        if ($LASTEXITCODE -ne 0) {
            Write-Host "ERROR: Bootstrap script failed with exit code $LASTEXITCODE" -ForegroundColor Red
            Pop-Location
            exit 1
        }
        
        Pop-Location
        
        if (-not (Test-Path $VcpkgExe)) {
            Write-Host "ERROR: vcpkg.exe was not created by bootstrap script" -ForegroundColor Red
            exit 1
        }
        
        Write-Host "  Bootstrap completed successfully" -ForegroundColor Green
    }
    catch {
        Write-Host "ERROR: Bootstrap failed: $_" -ForegroundColor Red
        exit 1
    }
}

Write-Host ""
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "vcpkg Installation Successful!" -ForegroundColor Green
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Installation Details:" -ForegroundColor Yellow
Write-Host "  vcpkg Location: $VcpkgDir" -ForegroundColor White
Write-Host "  vcpkg Executable: $VcpkgExe" -ForegroundColor White
Write-Host ""
Write-Host "Next Steps:" -ForegroundColor Cyan
Write-Host "  1. Use vcpkg with CMake by passing the toolchain file:" -ForegroundColor White
Write-Host "     -DCMAKE_TOOLCHAIN_FILE=""$VcpkgDir\scripts\buildsystems\vcpkg.cmake""" -ForegroundColor Gray
Write-Host ""
Write-Host "  2. Install packages with:" -ForegroundColor White
Write-Host "     $VcpkgExe install <package>:x64-windows" -ForegroundColor Gray
Write-Host ""
Write-Host "  3. Example build commands:" -ForegroundColor White
Write-Host "     .\build.ps1 -VcpkgDir $VcpkgDir" -ForegroundColor Gray
Write-Host ""
Write-Host "Troubleshooting:" -ForegroundColor Cyan
Write-Host "  - If bootstrap fails, ensure Visual Studio Build Tools are installed" -ForegroundColor White
Write-Host "  - Run 'Get-Command msbuild' to verify build tools are available" -ForegroundColor White
Write-Host ""
