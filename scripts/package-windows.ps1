[CmdletBinding()]
param(
    [ValidateSet("Portable", "Installer", "All")]
    [string]$Mode = "Portable",
    [string]$BuildDir = "",
    [string]$PackageDir = "packages",
    [string]$SourceDir = "",
    [string]$QtPrefixPath = "D:\QT\6.11.0\mingw_64",
    [string]$MingwBinPath = "D:\QT\Tools\mingw1310_64\bin",
    [string]$CMakePath = "",
    [string]$InnoSetupCompiler = "",
    [string]$Generator = "MinGW Makefiles",
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Release",
    [switch]$SkipBuild,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

function Resolve-RepositoryRoot {
    param([string]$ExplicitSourceDir)

    if (-not [string]::IsNullOrWhiteSpace($ExplicitSourceDir)) {
        $explicitPath = [System.IO.Path]::GetFullPath($ExplicitSourceDir)
        if (Test-Path -LiteralPath (Join-Path $explicitPath "CMakeLists.txt")) {
            return $explicitPath
        }
        throw "指定的源码目录不是有效工程根目录：$explicitPath"
    }

    $directCandidates = @(
        $PSScriptRoot,
        (Split-Path -Parent $PSScriptRoot),
        (Get-Location).Path
    )

    foreach ($candidate in $directCandidates) {
        if ([string]::IsNullOrWhiteSpace($candidate)) {
            continue
        }
        $candidatePath = [System.IO.Path]::GetFullPath($candidate)
        if (Test-Path -LiteralPath (Join-Path $candidatePath "CMakeLists.txt")) {
            return $candidatePath
        }
    }

    $cacheCandidates = @(
        (Join-Path $PSScriptRoot "CMakeCache.txt"),
        (Join-Path (Split-Path -Parent $PSScriptRoot) "CMakeCache.txt")
    )
    foreach ($cachePath in $cacheCandidates) {
        if (-not (Test-Path -LiteralPath $cachePath)) {
            continue
        }
        $cacheText = Get-Content -Encoding UTF8 -LiteralPath $cachePath -Raw
        $match = [regex]::Match($cacheText, '(?m)^CMAKE_HOME_DIRECTORY:INTERNAL=(.+)$')
        if ($match.Success) {
            $sourcePath = [System.IO.Path]::GetFullPath($match.Groups[1].Value.Trim())
            if (Test-Path -LiteralPath (Join-Path $sourcePath "CMakeLists.txt")) {
                return $sourcePath
            }
        }
    }

    throw "找不到工程源码根目录。请在仓库 scripts 目录或 CMake 构建目录运行，或通过 -SourceDir 指定。"
}

$repoRoot = Resolve-RepositoryRoot -ExplicitSourceDir $SourceDir
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

    $pathCommand = Get-Command $CommandName -ErrorAction SilentlyContinue
    if ($null -ne $pathCommand) {
        return $pathCommand.Source
    }

    foreach ($candidate in $Candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    throw "找不到 $CommandName，请通过参数显式传入路径。"
}

function Resolve-OptionalExecutable {
    param(
        [string]$ExplicitPath,
        [string[]]$Candidates,
        [string]$CommandName
    )

    try {
        return Resolve-Executable -ExplicitPath $ExplicitPath -Candidates $Candidates -CommandName $CommandName
    } catch {
        return ""
    }
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

function Get-AppVersion {
    $versionHeader = Join-Path $repoRoot "include\appversion.h"
    if (Test-Path -LiteralPath $versionHeader) {
        $versionText = Get-Content -Encoding UTF8 -LiteralPath $versionHeader -Raw
        $match = [regex]::Match($versionText, 'kSemanticVersion\s*=\s*"([^"]+)"')
        if ($match.Success) {
            return $match.Groups[1].Value
        }
    }

    $cmakeText = Get-Content -Encoding UTF8 -LiteralPath (Join-Path $repoRoot "CMakeLists.txt") -Raw
    $cmakeMatch = [regex]::Match($cmakeText, 'project\([^\)]*VERSION\s+([0-9]+(?:\.[0-9]+)*)')
    if ($cmakeMatch.Success) {
        return $cmakeMatch.Groups[1].Value
    }

    return "0.0.0"
}

function Assert-AppVersion {
    param([string]$Version)

    if (-not [regex]::IsMatch($Version, '^[1-9]\.[0-9]{2}\.[0-9]{2}$')) {
        throw "版本号必须使用 X.YY.ZZ 三段式，范围为 1.00.00 到 9.99.99，当前值：$Version"
    }
}

function Resolve-SafeChildPath {
    param(
        [string]$BasePath,
        [string]$ChildPath
    )

    $baseFullPath = [System.IO.Path]::GetFullPath($BasePath)
    $targetFullPath = if ([System.IO.Path]::IsPathRooted($ChildPath)) {
        [System.IO.Path]::GetFullPath($ChildPath)
    } else {
        [System.IO.Path]::GetFullPath((Join-Path $BasePath $ChildPath))
    }
    if (-not $targetFullPath.StartsWith($baseFullPath, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "目标路径不在工程目录下：$targetFullPath"
    }
    return $targetFullPath
}

function Get-CachedSourceDir {
    param([string]$BuildPath)

    $cachePath = Join-Path $BuildPath "CMakeCache.txt"
    if (-not (Test-Path -LiteralPath $cachePath)) {
        return ""
    }

    $cacheText = Get-Content -Encoding UTF8 -LiteralPath $cachePath -Raw
    $match = [regex]::Match($cacheText, '(?m)^CMAKE_HOME_DIRECTORY:INTERNAL=(.+)$')
    if (-not $match.Success) {
        return ""
    }

    return [System.IO.Path]::GetFullPath($match.Groups[1].Value.Trim())
}

function Clear-StaleBuildDirectory {
    param(
        [string]$BuildPath,
        [string]$ExpectedSourceDir
    )

    $cachedSourceDir = Get-CachedSourceDir -BuildPath $BuildPath
    if ([string]::IsNullOrWhiteSpace($cachedSourceDir)) {
        return
    }

    $expectedFullPath = [System.IO.Path]::GetFullPath($ExpectedSourceDir)
    if ($cachedSourceDir.Equals($expectedFullPath, [System.StringComparison]::OrdinalIgnoreCase)) {
        return
    }

    $repoFullPath = [System.IO.Path]::GetFullPath($repoRoot)
    if (-not $BuildPath.StartsWith($repoFullPath, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "拒绝清理仓库外构建目录：$BuildPath"
    }

    Write-Warning "构建目录缓存来自其他源码目录，将自动清理：$BuildPath"
    Write-Warning "缓存源码目录：$cachedSourceDir"
    Write-Warning "当前源码目录：$expectedFullPath"
    Remove-Item -LiteralPath $BuildPath -Recurse -Force
}

function Find-BuiltExecutable {
    param([string]$BuildPath)

    $candidatePaths = @(
        (Join-Path $BuildPath "xray_serial_console.exe"),
        (Join-Path $BuildPath "$Configuration\xray_serial_console.exe")
    )

    foreach ($candidate in $candidatePaths) {
        if (Test-Path -LiteralPath $candidate) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    $found = Get-ChildItem -LiteralPath $BuildPath -Recurse -Filter "xray_serial_console.exe" -ErrorAction SilentlyContinue |
        Sort-Object FullName |
        Select-Object -First 1
    if ($null -ne $found) {
        return $found.FullName
    }

    throw "构建目录中找不到 xray_serial_console.exe：$BuildPath"
}

function Assert-BuiltExecutableVersion {
    param(
        [string]$ExePath,
        [string]$Version
    )

    $bytes = [System.IO.File]::ReadAllBytes($ExePath)
    $asciiVersion = [System.Text.Encoding]::ASCII.GetBytes($Version)
    $unicodeVersion = [System.Text.Encoding]::Unicode.GetBytes($Version)

    function Test-ContainsBytes {
        param(
            [byte[]]$Haystack,
            [byte[]]$Needle
        )

        if ($Needle.Length -eq 0 -or $Haystack.Length -lt $Needle.Length) {
            return $false
        }

        for ($i = 0; $i -le $Haystack.Length - $Needle.Length; $i++) {
            $matched = $true
            for ($j = 0; $j -lt $Needle.Length; $j++) {
                if ($Haystack[$i + $j] -ne $Needle[$j]) {
                    $matched = $false
                    break
                }
            }
            if ($matched) {
                return $true
            }
        }

        return $false
    }

    $containsAsciiVersion = Test-ContainsBytes -Haystack $bytes -Needle $asciiVersion
    $containsUnicodeVersion = Test-ContainsBytes -Haystack $bytes -Needle $unicodeVersion
    if ($containsAsciiVersion -or $containsUnicodeVersion) {
        return
    }

    throw "构建产物版本与当前版本不一致：$ExePath 中未找到版本号 $Version。请清理构建目录后重新构建。"
}

function Write-InnoSetupScript {
    param(
        [string]$ScriptPath,
        [string]$PortablePath,
        [string]$InstallerOutputPath,
        [string]$Version
    )

    $sourcePath = $PortablePath.Replace("\", "\\")
    $outputPath = $InstallerOutputPath.Replace("\", "\\")
    $script = @"
#define MyAppName "X-Ray Serial Console"
#define MyAppPublisher "mantaXray"
#define MyAppVersion "$Version"
#define MyAppExeName "xray_serial_console.exe"

[Setup]
AppId={{7DDA9E68-00F8-4A3B-92B5-DA54CF8E4B8E}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=$outputPath
OutputBaseFilename=xray_serial_console-$Version-setup
Compression=lzma
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "chinesesimp"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "$sourcePath\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent
"@

    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $ScriptPath) | Out-Null
    Set-Content -Encoding UTF8 -LiteralPath $ScriptPath -Value $script
}

$cmakeExe = Resolve-Executable `
    -ExplicitPath $CMakePath `
    -Candidates @("D:\QT\Tools\CMake_64\bin\cmake.exe", "C:\Program Files\CMake\bin\cmake.exe") `
    -CommandName "cmake"

$qtBinPath = Join-Path $QtPrefixPath "bin"
$windeployqtExe = Resolve-Executable `
    -ExplicitPath "" `
    -Candidates @((Join-Path $qtBinPath "windeployqt.exe")) `
    -CommandName "windeployqt"

if (Test-Path -LiteralPath $qtBinPath) {
    $env:Path = "$qtBinPath;$env:Path"
}
if (Test-Path -LiteralPath $MingwBinPath) {
    $env:Path = "$MingwBinPath;$env:Path"
}

if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $scriptInBuildDir = Test-Path -LiteralPath (Join-Path $PSScriptRoot "CMakeCache.txt")
    $BuildDir = if ($scriptInBuildDir) {
        [System.IO.Path]::GetFullPath($PSScriptRoot)
    } else {
        "build\Package-MinGW-Release"
    }
}

$buildFullPath = Resolve-SafeChildPath -BasePath $repoRoot -ChildPath $BuildDir
$packageFullPath = Resolve-SafeChildPath -BasePath $repoRoot -ChildPath $PackageDir
$version = Get-AppVersion
Assert-AppVersion -Version $version
$portableName = "xray_serial_console-$version-portable"
$portablePath = Join-Path $packageFullPath $portableName
$stagingPath = Join-Path $packageFullPath "$portableName-staging"
$zipPath = Join-Path $packageFullPath "$portableName.zip"
$installerWorkPath = Join-Path $packageFullPath "installer"
$installerOutputPath = Join-Path $installerWorkPath "output"
$innoScriptPath = Join-Path $installerWorkPath "xray_serial_console.iss"

if ($Clean) {
    if (Test-Path -LiteralPath $stagingPath) {
        Remove-Item -LiteralPath $stagingPath -Recurse -Force
    }
    if (Test-Path -LiteralPath $installerWorkPath) {
        Remove-Item -LiteralPath $installerWorkPath -Recurse -Force
    }
}

if (-not $SkipBuild) {
    $cxxCompiler = Join-Path $MingwBinPath "g++.exe"
    $makeProgram = Join-Path $MingwBinPath "mingw32-make.exe"
    Clear-StaleBuildDirectory -BuildPath $buildFullPath -ExpectedSourceDir $repoRoot

    Invoke-Native -FilePath $cmakeExe `
        -Arguments @(
            "-S", $repoRoot,
            "-B", $buildFullPath,
            "-G", $Generator,
            "-DCMAKE_PREFIX_PATH=$QtPrefixPath",
            "-DCMAKE_BUILD_TYPE=$Configuration",
            "-DCMAKE_CXX_COMPILER=$cxxCompiler",
            "-DCMAKE_MAKE_PROGRAM=$makeProgram"
        ) `
        -Description "CMake 配置"

    if ($Clean) {
        Invoke-Native -FilePath $cmakeExe `
            -Arguments @("--build", $buildFullPath, "--config", $Configuration, "--target", "clean") `
            -Description "CMake 清理"
    }

    Invoke-Native -FilePath $cmakeExe `
        -Arguments @("--build", $buildFullPath, "--config", $Configuration) `
        -Description "CMake 构建"
}

$builtExe = Find-BuiltExecutable -BuildPath $buildFullPath
Assert-BuiltExecutableVersion -ExePath $builtExe -Version $version
Write-Host "应用版本：$version"
Write-Host "构建产物：$builtExe"

New-Item -ItemType Directory -Force -Path $packageFullPath | Out-Null
if (Test-Path -LiteralPath $stagingPath) {
    Remove-Item -LiteralPath $stagingPath -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $stagingPath | Out-Null

Copy-Item -LiteralPath $builtExe -Destination (Join-Path $stagingPath "xray_serial_console.exe") -Force
foreach ($readmeName in @("README.md", "README.zh-CN.md", "README.en-US.md")) {
    $readmePath = Join-Path $repoRoot $readmeName
    if (Test-Path -LiteralPath $readmePath) {
        Copy-Item -LiteralPath $readmePath -Destination (Join-Path $stagingPath $readmeName) -Force
    }
}

$deployMode = if ($Configuration -eq "Debug") { "--debug" } else { "--release" }
try {
    Invoke-Native -FilePath $windeployqtExe `
        -Arguments @($deployMode, "--compiler-runtime", (Join-Path $stagingPath "xray_serial_console.exe")) `
        -Description "windeployqt 部署"

    $platformPlugin = Join-Path $stagingPath "platforms\qwindows.dll"
    if (-not (Test-Path -LiteralPath $platformPlugin)) {
        throw "windeployqt 未生成 platforms\qwindows.dll，便携版缺少 Qt 平台插件，不能发布。"
    }
} catch {
    if (Test-Path -LiteralPath $stagingPath) {
        Remove-Item -LiteralPath $stagingPath -Recurse -Force
    }
    throw
}

if (Test-Path -LiteralPath $portablePath) {
    Remove-Item -LiteralPath $portablePath -Recurse -Force
}
Move-Item -LiteralPath $stagingPath -Destination $portablePath

if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}
Compress-Archive -Path $portablePath -DestinationPath $zipPath -Force

Write-Host "便携版目录：$portablePath"
Write-Host "便携版压缩包：$zipPath"

if ($Mode -eq "Portable") {
    return
}

Write-InnoSetupScript -ScriptPath $innoScriptPath `
    -PortablePath $portablePath `
    -InstallerOutputPath $installerOutputPath `
    -Version $version
Write-Host "Inno Setup 脚本：$innoScriptPath"

$isccExe = Resolve-OptionalExecutable `
    -ExplicitPath $InnoSetupCompiler `
    -Candidates @("C:\Program Files (x86)\Inno Setup 6\ISCC.exe", "C:\Program Files\Inno Setup 6\ISCC.exe") `
    -CommandName "ISCC"

if ([string]::IsNullOrWhiteSpace($isccExe)) {
    $message = "未找到 Inno Setup 编译器。安装 Inno Setup 6 后重新运行，或通过 -InnoSetupCompiler 指定 ISCC.exe。"
    if ($Mode -eq "Installer") {
        throw $message
    }
    Write-Warning $message
    return
}

New-Item -ItemType Directory -Force -Path $installerOutputPath | Out-Null
Invoke-Native -FilePath $isccExe `
    -Arguments @($innoScriptPath) `
    -Description "Inno Setup 编译"
Write-Host "安装包目录：$installerOutputPath"
