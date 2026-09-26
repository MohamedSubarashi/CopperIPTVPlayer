# Development launcher: puts the Qt bin dir on PATH (so the debug build finds
# its DLLs) and starts the app from build/debug.
param(
    [string]$Exe = "C:\Users\mohya\Downloads\Apps\Copper IPTV Player\build\debug\CopperIPTVPlayer.exe",
    [string]$QtBin = "C:/Qt/6.11.2/mingw_64/bin"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $Exe)) {
    throw "Executable not found: $Exe (build the Debug configuration first: cmake --build --preset windows-debug)"
}

$env:PATH = "$QtBin;$env:PATH"
Write-Host "Starting Copper IPTV Player (debug)..."
& $Exe @args