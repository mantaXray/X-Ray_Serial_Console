# Windows 打包说明

本文记录 X-Ray Serial Console 在 Windows 下生成便携版和安装包的流程。

## 1. 产物类型

当前支持两类发布产物：

- 便携版：无需安装，解压后直接运行 `xray_serial_console.exe`。
- 安装包：基于便携版目录生成 Inno Setup 安装程序，可创建开始菜单和桌面快捷方式。

便携版是基础产物。安装包只负责安装体验，不重新收集 Qt 依赖。

## 2. 前置条件

必须具备：

- Qt MinGW 套件，例如 `D:\QT\6.11.0\mingw_64`
- `windeployqt.exe`，通常位于 Qt 套件的 `bin` 目录
- CMake
- PowerShell 7，推荐使用 `pwsh.exe -NoProfile`

生成安装包还需要：

- Inno Setup 6
- `ISCC.exe`，常见路径为 `C:\Program Files (x86)\Inno Setup 6\ISCC.exe`

## 3. 脚本位置

仓库内主脚本：

```powershell
scripts\package-windows.ps1
```

可双击执行的包装脚本：

```text
scripts\package-windows.bat
```

CMake 配置工程后，脚本也会复制到当前 Qt/CMake 构建目录根部：

```text
build\Desktop_Qt_6_11_0_MinGW_64_bit-Debug\package-windows.ps1
build\Desktop_Qt_6_11_0_MinGW_64_bit-Debug\package-windows.bat
```

Qt Creator 重新配置 CMake 后，也可以在对应构建目录中找到这个脚本。脚本放在构建目录运行时，会读取同目录下的 `CMakeCache.txt` 找回源码根目录，并默认使用当前构建目录作为构建输入。

## 4. 生成便携版

从仓库根目录运行：

```powershell
pwsh.exe -NoProfile -File scripts\package-windows.ps1 -Mode Portable -Clean
```

或者直接双击：

```text
scripts\package-windows.bat
```

从 Qt/CMake 构建目录运行：

```powershell
pwsh.exe -NoProfile -File .\package-windows.ps1 -Mode Portable -Clean
```

也可以双击构建目录中的：

```text
package-windows.bat
```

输出位置：

```text
packages\xray_serial_console-<版本号>-portable\
packages\xray_serial_console-<版本号>-portable.zip
```

便携版目录必须包含：

```text
xray_serial_console.exe
platforms\qwindows.dll
Qt*.dll
```

脚本会检查 `platforms\qwindows.dll`。如果该文件不存在，脚本会失败并清理临时目录，避免生成不可运行的假便携包。

脚本会从 `include/appversion.h` 读取 `AppVersion::kSemanticVersion` 作为包版本号，并检查待打包的 `xray_serial_console.exe` 中是否包含同一版本字符串。若版本不一致，脚本会失败，避免把旧构建产物重新打成新版本包。发布前如果界面仍显示旧版本，先清理并重新构建对应构建目录，再重新打包。

如果构建目录中的 `CMakeCache.txt` 来自其他源码目录，例如旧项目 `demo_widgets`，脚本会在正常构建流程中自动清理该构建目录并重新配置，避免 CMake 报 “source does not match the source used to generate cache”。

## 5. 生成安装包

先安装 Inno Setup 6，然后运行：

```powershell
pwsh.exe -NoProfile -File scripts\package-windows.ps1 -Mode Installer -Clean
```

如果 `ISCC.exe` 不在默认路径，可显式指定：

```powershell
pwsh.exe -NoProfile -File scripts\package-windows.ps1 -Mode Installer -InnoSetupCompiler "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" -Clean
```

使用 BAT 传参也可以：

```bat
scripts\package-windows.bat -Mode Installer -Clean
```

输出位置：

```text
packages\installer\xray_serial_console.iss
packages\installer\output\xray_serial_console-<版本号>-setup.exe
```

## 6. 常用参数

```powershell
# 指定 Qt 套件
-QtPrefixPath "D:\QT\6.11.0\mingw_64"

# 指定 MinGW bin，用于补充编译器运行库路径
-MingwBinPath "D:\QT\Tools\mingw1310_64\bin"

# 指定构建目录
-BuildDir "build\Desktop_Qt_6_11_0_MinGW_64_bit-Debug"

# 不重新构建，只基于已有 exe 做部署
-SkipBuild

# 清理旧打包产物
-Clean
```

不带参数运行 `package-windows.bat` 时，等价于：

```powershell
pwsh.exe -NoProfile -File package-windows.ps1 -Mode Portable -Clean
```

## 7. 验收清单

发布前至少检查：

- 便携版 ZIP 能解压运行。
- 便携版目录包含 `platforms\qwindows.dll`。
- 在没有 Qt Creator 环境变量的普通终端中能启动程序。
- 安装包能安装、启动、卸载。
- 版本号与 `include/appversion.h` 中的 `kSemanticVersion` 一致。

## 8. 常见问题

### 只复制 exe 后无法运行

Qt 程序不能只复制 exe。必须通过 `windeployqt.exe` 收集 Qt DLL、平台插件和编译器运行库。

### `Unable to query qtpaths`

这是 `windeployqt.exe` 调用内部 Qt 工具失败。若发生在 Codex 沙箱或受限终端中，换到普通 PowerShell、Qt Creator 终端或本机开发终端执行。

### 找不到源码根目录

如果脚本既不在仓库 `scripts\` 下，也不在 CMake 构建目录下，使用 `-SourceDir` 指定仓库根目录：

```powershell
pwsh.exe -NoProfile -File .\package-windows.ps1 -SourceDir "D:\QT\projects\demo_widgets" -Mode Portable
```

### 构建目录缓存来自其他项目

如果看到类似下面的错误：

```text
The source "...X-Ray_Serial_Console/CMakeLists.txt" does not match the source "...demo_widgets/CMakeLists.txt" used to generate cache.
```

说明当前构建目录复用了其他项目的 CMake cache。正常运行 `package-windows.ps1` 且不加 `-SkipBuild` 时，脚本会自动清理这种过期构建目录并重新配置。若手动处理，可删除：

```powershell
Remove-Item -LiteralPath ".\build\Package-MinGW-Release" -Recurse -Force
```
