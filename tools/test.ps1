[CmdletBinding()]
param(
    [switch]$UnitOnly
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
Set-Location $repo

function Invoke-Wsl {
    $distro = (wsl.exe -l -q 2>$null | Where-Object { $_ -match '\S' } | Select-Object -First 1)
    if (-not $distro) { return $false }
    $distro = $distro -replace "`0", ''

    Write-Host "running the suite under WSL ($distro)" -ForegroundColor Cyan
    wsl.exe -d $distro -- make test
    if ($LASTEXITCODE -ne 0) { throw "host unit tests failed" }

    if (-not $UnitOnly) {
        wsl.exe -d $distro -- make ARCH=x86_64
        if ($LASTEXITCODE -ne 0) { throw "freestanding build failed" }

        wsl.exe -d $distro -- make test-ns ARCH=x86_64
        if ($LASTEXITCODE -ne 0) { throw "namespace boot test failed" }
    }
    return $true
}

function Invoke-Native {
    $cc = (Get-Command gcc -ErrorAction SilentlyContinue) ?? (Get-Command clang -ErrorAction SilentlyContinue)
    if (-not $cc) { throw "no WSL distro and no gcc/clang on PATH; nothing can be run here" }

    Write-Host "no WSL distro found - running host unit tests with $($cc.Name)" -ForegroundColor Yellow
    New-Item -ItemType Directory -Force build/host | Out-Null

    & $cc.Source -std=c11 -g -O1 -I. -Wall -Wextra -Wshadow `
        -Wconversion -DINIT_HOSTED=1 -fno-builtin tests/host/test_main.c `
        -o build/host/unit.exe
    if ($LASTEXITCODE -ne 0) { throw "host unit test build failed" }

    & ./build/host/unit.exe
    if ($LASTEXITCODE -ne 0) { throw "host unit tests failed" }
}

$ranUnderWsl = $false
if (Get-Command wsl.exe -ErrorAction SilentlyContinue) {
    $ranUnderWsl = Invoke-Wsl
}
if (-not $ranUnderWsl) {
    Invoke-Native
}

Write-Host "OK" -ForegroundColor Green
