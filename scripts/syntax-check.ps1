[CmdletBinding(PositionalBinding = $false)]
param(
    [Parameter(Position = 0, ValueFromRemainingArguments = $true)]
    [string[]]$Files = @(),
    [string]$CompilerPath = "D:\QT\Tools\mingw1310_64\bin\g++.exe",
    [string]$BuildDir = "build\Desktop_Qt_6_11_0_MinGW_64_bit-Debug",
    [string]$QtRoot = "D:\QT\6.11.0\mingw_64"
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $repoRoot

if ($Files.Count -eq 0) {
    $Files = @(
        "src\appsettings.cpp",
        "src\mainwindow.cpp",
        "src\toolboxdialog.cpp",
        "src\multistringdock.cpp",
        "src\serialportworker.cpp",
        "src\main.cpp"
    )
}

if (-not (Test-Path -LiteralPath $CompilerPath)) {
    $compiler = Get-Command g++ -ErrorAction SilentlyContinue
    if ($null -eq $compiler) {
        throw "找不到 g++，请通过 -CompilerPath 指定 MinGW g++.exe。"
    }
    $CompilerPath = $compiler.Source
}

$defines = @(
    "-DMINGW_HAS_SECURE_API=1",
    "-DQT_CORE_LIB",
    "-DQT_GUI_LIB",
    "-DQT_NEEDS_QMAIN",
    "-DQT_QML_DEBUG",
    "-DQT_WIDGETS_LIB",
    "-DUNICODE",
    "-DWIN32",
    "-DWIN64",
    "-D_ENABLE_EXTENDED_ALIGNED_STORAGE",
    "-D_UNICODE",
    "-D_WIN64"
)

$includes = @(
    "-I$BuildDir/xray_serial_console_autogen/include",
    "-Iinclude",
    "-isystem", "$QtRoot/include/QtCore",
    "-isystem", "$QtRoot/include",
    "-isystem", "$QtRoot/mkspecs/win32-g++",
    "-isystem", "$QtRoot/include/QtWidgets",
    "-isystem", "$QtRoot/include/QtGui"
)

foreach ($file in $Files) {
    if (-not (Test-Path -LiteralPath $file)) {
        throw "待检查文件不存在：$file"
    }
    Write-Host "Syntax check: $file"
    & $CompilerPath @defines @includes -g -std=gnu++17 -fsyntax-only $file
    if ($LASTEXITCODE -ne 0) {
        throw "语法检查失败：$file，退出码：$LASTEXITCODE"
    }
}
