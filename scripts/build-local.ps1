[CmdletBinding()]
param(
    [string]$BuildDir = "build\Desktop_Qt_6_11_0_MinGW_64_bit-Debug",
    [string]$CMakePath = "",
    [string]$QtPrefixPath = "D:\QT\6.11.0\mingw_64",
    [string]$MingwBinPath = "D:\QT\Tools\mingw1310_64\bin",
    [string]$Generator = "MinGW Makefiles",
    [switch]$Configure
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $repoRoot

function Resolve-Executable {
    param(
        [string]$ExplicitPath,
        [string[]]$Candidates,
        [string]$CommandName
    )

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        if (Test-Path -LiteralPath $ExplicitPath) {
            return (Resolve-Path -LiteralPath $ExplicitPath).Path
        }
        throw "指定的 $CommandName 不存在：$ExplicitPath"
    }

    foreach ($candidate in $Candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    $pathCommand = Get-Command $CommandName -ErrorAction SilentlyContinue
    if ($null -ne $pathCommand) {
        return $pathCommand.Source
    }

    throw "找不到 $CommandName，请通过参数显式传入路径。"
}

function Invoke-Native {
    param(
        [string]$FilePath,
        [string[]]$Arguments,
        [string]$Description
    )

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Description 失败，退出码：$LASTEXITCODE"
    }
}

$cmakeExe = Resolve-Executable `
    -ExplicitPath $CMakePath `
    -Candidates @("D:\QT\Tools\CMake_64\bin\cmake.exe", "C:\Program Files\CMake\bin\cmake.exe") `
    -CommandName "cmake"

$cxxCompiler = Join-Path $MingwBinPath "g++.exe"
$makeProgram = Join-Path $MingwBinPath "mingw32-make.exe"

if (-not (Test-Path -LiteralPath $QtPrefixPath)) {
    throw "QtPrefixPath 不存在：$QtPrefixPath"
}

if (-not (Test-Path -LiteralPath $cxxCompiler)) {
    throw "找不到 MinGW C++ 编译器：$cxxCompiler"
}

if (-not (Test-Path -LiteralPath $makeProgram)) {
    throw "找不到 MinGW Make：$makeProgram"
}

Write-Host "CMake: $cmakeExe"
Write-Host "QtPrefixPath: $QtPrefixPath"
Write-Host "CXX: $cxxCompiler"
Write-Host "Make: $makeProgram"

if ($Configure -or -not (Test-Path -LiteralPath (Join-Path $BuildDir "CMakeCache.txt"))) {
    Invoke-Native -FilePath $cmakeExe `
        -Arguments @(
            "-S", ".",
            "-B", $BuildDir,
            "-G", $Generator,
            "-DCMAKE_PREFIX_PATH=$QtPrefixPath",
            "-DCMAKE_CXX_COMPILER=$cxxCompiler",
            "-DCMAKE_MAKE_PROGRAM=$makeProgram"
        ) `
        -Description "CMake 配置"
}

Invoke-Native -FilePath $cmakeExe `
    -Arguments @("--build", $BuildDir) `
    -Description "CMake 构建"
