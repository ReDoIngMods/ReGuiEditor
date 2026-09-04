# Reconfigures (only if needed, or if -Reconfigure is passed) and builds the
# reguieditor target. Run from anywhere - paths are resolved relative to this
# script's own location.
param(
    [switch]$Reconfigure
)

$ErrorActionPreference = "Stop"
$repoRoot = $PSScriptRoot
$buildDir = Join-Path $repoRoot "build"

if ($Reconfigure -or -not (Test-Path (Join-Path $buildDir "CMakeCache.txt"))) {
    cmake -S $repoRoot -B $buildDir
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

# The post-build bundling step overwrites build/reguieditor/reguieditor.exe in
# place, which fails with a permission error if a previous run is still
# holding that file open.
$justStoppedRunningInstance = $false
$running = Get-Process reguieditor -ErrorAction SilentlyContinue
if ($running) {
    Write-Host "Stopping running reguieditor.exe (PID $($running.Id -join ', ')) so the build can overwrite it..."
    $running | Stop-Process -Force
    $running | Wait-Process -Timeout 5 -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 500
    $justStoppedRunningInstance = $true
}

cmake --build $buildDir --target reguieditor -j
if ($LASTEXITCODE -ne 0 -and $justStoppedRunningInstance) {
    # Windows doesn't always finish releasing a just-killed process's file
    # handles (or an antivirus's post-exit scan of them) the instant
    # Stop-Process/Wait-Process return - the very next build can fail with no
    # real compiler diagnostic as a result. One retry after a short pause has
    # reliably cleared it in practice.
    Write-Host "Build failed right after closing reguieditor.exe - retrying once..."
    Start-Sleep -Seconds 1
    cmake --build $buildDir --target reguieditor -j
}
exit $LASTEXITCODE
