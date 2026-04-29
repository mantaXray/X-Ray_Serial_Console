# X-Ray Serial Console

X-Ray Serial Console 是一个基于 Qt Widgets 的本地串口调试工具，当前定位为 Windows 优先、离线可用、面向串口联调与协议验证场景的桌面应用。

项目当前工程名为 `xray_serial_console`，品牌标识为 `X-Ray`，作者为 `mantaXray`。

## 当前已实现功能

### 串口连接与基础控制

- 支持串口枚举、手动刷新、打开与关闭串口。
- 支持波特率、数据位、校验位、停止位配置。
- 支持常用高速波特率，包括 `1M`、`1.5M`、`2M`、`2.5M`、`3M`。
- 支持自定义波特率输入与保存。
- 支持 `RTS` / `DTR` 手动控制。
- 支持在端口列表中显示 `COM` 口与设备描述信息。

### 接收区能力

- 支持普通文本显示与 `HEX` 显示切换。
- 支持时间戳显示。
- 支持自动换行开关。
- 支持暂停刷新，避免高频数据刷屏时影响观察。
- 支持超时分包显示，可按时间窗口聚合同一帧数据。
- 支持导出接收内容到文件。
- 支持持续保存接收数据。
- 支持接收内容筛选与关键字高亮。

### 发送区能力

- 支持普通文本发送与 `HEX` 发送。
- 支持发送末尾追加换行。
- 支持循环发送与发送间隔设置。
- 支持多种校验附加方式，包括 `CRC16`、`CRC32`、加和校验、异或校验等。
- 支持导入发送文件，并对大文件进行安全预览提示。
- 支持直接发送文件。
- 支持文件发送预设方案、自定义分块字节数、块间延时。
- 支持文件发送进度条与取消操作。
- 支持保存发送区上次编辑内容，重新运行后恢复发送草稿。
- 支持统一调整发送区与接收区字体和字号。

### 扩展工具能力

- 支持多字符串发送面板。
- 支持多字符串项保存当前发送区内容、启停联动、轮询发送和表格列宽持久化。
- 多字符串面板默认展开时会给表格足够宽度显示主要列；发送、HEX发送、换行列使用和主界面一致的高对比度勾选框。新增空白项默认按普通文本发送，需按 HEX 发送时勾选“HEX发送”列。
- 支持协议与数据工具箱。
- 支持统一 Modbus 读写工具，可选择 `03` / `04` 读取寄存器、`06` / `16` 写寄存器，支持定时扫描、自动解析、表格显示、默认数据格式切换和原始收发日志。
- Modbus 读写工具支持寄存器表默认铺满窗口；解析值单元格可通过右键菜单按地址单独设置格式，也可对多个选中单元格批量设置。
- Modbus 解析支持 `Float32 AB CD` 与 `Float32 CD AB`，Float32 从当前地址开始连续使用两个 16 位寄存器合成。
- Modbus 读写工具支持窗口位置、分隔条比例、寄存器表列宽、默认数据格式、单地址解析格式覆盖、原始日志时间戳与超时分包独立设置保存。
- Modbus 原始日志只记录读写工具自身请求后的响应；关闭窗口、停止扫描或响应超时后会释放接收会话，不再接收主面板发送后的回包。
- Modbus 扫描期间，主接收区和 Modbus 原始收发日志按各自的超时分包策略显示；工具请求日志、窗口布局保存和日志选项保存都不会提前冲刷主接收区缓存。
- Modbus 读写工具默认布局优先保证原始收发日志有足够高度，寄存器表和顶部参数控件保持紧凑；扫描间隔等数字框保留足够宽度显示完整数值，默认窗口宽度也减少无意义右侧空白。
- 工具箱当前包含：
  - 计算器
  - 文本 / HEX 转换
  - 自定义协议组包解析
- 工具菜单中“协议与数据工具箱”保留二级工具页入口；独立的 Modbus 读写工具直接显示在“工具”菜单下，避免只有一个功能时再套一层空菜单。
- 主面板发送、轮询发送、多字符串发送和工具发送都会在接收监视区记录发送方向；关闭时间戳只隐藏时间，不会隐藏发送帧。
- 接收区筛选生效时会在标题和空白提示里标明筛选状态，避免 S/R 计数增长但过滤后无匹配内容时误以为没有接收。

### 界面与工程能力

- 支持中文 / 英文界面切换。
- 顶部菜单已集成多字符串入口与工具入口。
- 毫秒等固定单位显示在输入框外侧，避免占用数字编辑区域或误导为可编辑单位。
- 自动生成 `xray_serial_console.ini` 配置文件，保存窗口布局、串口参数、显示选项、发送区草稿、发送选项、Modbus 工具布局、Modbus 解析格式设置和多字符串预设。
- 从 Qt Creator / CMake 构建目录运行时，配置写入系统应用配置目录并自动迁移旧构建目录 INI，避免每次构建清理后波特率、超时分包和发送草稿丢失；便携版目录已有 INI 时继续使用程序目录配置。
- 帮助菜单中的“使用说明”覆盖基础收发、接收显示、多字符串、Modbus、配置和打包入口；“关于”窗口只保留当前版本和首次发布说明。
- 底部状态栏已改为分段式显示，集中展示品牌、收发计数、串口状态与线路状态。
- 工程已完成 `X-Ray` 品牌化与翻译文件重命名。

## 工程结构

```text
.
|-- src/                         # C++ 实现文件与程序入口
|-- include/                     # C++ 头文件
|-- ui/                          # Qt Designer UI 文件
|-- resources/                   # qrc 资源清单与图标资源
|-- translations/                # Qt 翻译源文件
|-- docs/                        # 设计文档
|   `-- reference-images/        # 本地参考截图，默认不纳入版本库
|-- CMakeLists.txt               # CMake 构建配置
`-- README.md                    # 项目说明
```

`AGENTS.md`、`README.md`、`docs/design.md`、`docs/packaging-windows.md`、`docs/pitfalls.md` 和 `docs/skills/qt-widgets-cpp/SKILL.md` 已加入 CMake 文档文件列表，Qt Creator 重新配置 CMake 项目后可以在项目树中直接看到。

## 构建方式

### 依赖

- CMake 3.16 及以上
- Qt 6 或 Qt 5，需包含 `Widgets` 与 `LinguistTools`
- Windows 环境下需要 `setupapi`

### Windows / Qt 6.11 本机构建

```powershell
pwsh.exe -NoProfile -ExecutionPolicy Bypass -File scripts\build-local.ps1
```

脚本默认固定使用本机 Qt 安装路径 `D:\QT\6.11.0\mingw_64`、CMake 路径 `D:\QT\Tools\CMake_64\bin\cmake.exe` 和 MinGW 路径 `D:\QT\Tools\mingw1310_64\bin`，避免 PATH 中其他 MinGW 抢先被选中。

也可以使用 CMake Preset，Qt Creator 重新配置项目后通常可以直接识别：

```powershell
D:\QT\Tools\CMake_64\bin\cmake.exe --preset qt-6.11-mingw-debug
D:\QT\Tools\CMake_64\bin\cmake.exe --build --preset qt-6.11-mingw-debug
```

如果 Qt 安装路径不同，可通过脚本参数显式指定：

```powershell
pwsh.exe -NoProfile -ExecutionPolicy Bypass -File scripts\build-local.ps1 -QtPrefixPath "D:\Qt\6.x.x\mingw_64" -MingwBinPath "D:\Qt\Tools\mingwxxxx_64\bin"
```

### VS Code 运行与调试

仓库已提供 `.vscode/settings.json`、`.vscode/tasks.json` 和 `.vscode/launch.json`，默认使用 `qt-6.11-mingw-debug` 预设、MinGW `gdb.exe`，并在启动时把 Qt 与 MinGW 运行库目录加入 `PATH`。

在 VS Code 中可直接使用：

```text
Run and Debug: X-Ray Serial Console (MinGW Debug)
CMake: Run Without Debugging
CMake: Debug
```

如果手动从终端运行，需要先设置当前终端的运行环境：

```powershell
$env:PATH = "D:\QT\6.11.0\mingw_64\bin;D:\QT\Tools\mingw1310_64\bin;$env:PATH"
& ".\build\qt-6.11-mingw-debug\xray_serial_console.exe"
```

### 通用 CMake 示例

```powershell
cmake -S . -B build
cmake --build build
```

## 打包方式

项目提供 Windows 打包脚本：

```powershell
pwsh.exe -NoProfile -File scripts\package-windows.ps1 -Mode Portable -Clean
```

也可以直接双击：

```text
scripts\package-windows.bat
```

BAT 默认执行便携版打包流程；如果从命令行给 BAT 传参数，会原样转给 `package-windows.ps1`。

完整打包说明见 [docs/packaging-windows.md](docs/packaging-windows.md)。

默认会使用 `D:\QT\6.11.0\mingw_64` 和 `windeployqt.exe` 生成 Release 便携版，输出到 `packages/`：

- `packages/xray_serial_console-<版本号>-portable/`
- `packages/xray_serial_console-<版本号>-portable.zip`

如果 Qt 安装路径不同，可通过 `-QtPrefixPath` 指定，例如：

```powershell
pwsh.exe -NoProfile -File scripts\package-windows.ps1 -Mode Portable -QtPrefixPath "D:\Qt\6.x.x\mingw_64"
```

如果需要安装包，先安装 Inno Setup 6，然后执行：

```powershell
pwsh.exe -NoProfile -File scripts\package-windows.ps1 -Mode Installer -Clean
```

脚本会先生成便携版，再生成 `packages/installer/xray_serial_console.iss`，并在能找到 `ISCC.exe` 时继续编译安装包。未安装 Inno Setup 时，可先使用便携版压缩包发布。

打包需要在普通 PowerShell、Qt Creator 终端或本机开发终端里运行。受限沙箱里 `moc.exe` / `windeployqt.exe` 可能无法启动子进程，导致构建或部署失败。

Qt Creator 重新配置 CMake 后，`package-windows.ps1` 和 `package-windows.bat` 会同步到当前构建目录根部，例如 `build\Desktop_Qt_6_11_0_MinGW_64_bit-Debug\package-windows.bat`。在构建目录运行该副本时，脚本会读取 `CMakeCache.txt` 自动定位源码根目录。

## 当前设计重点

- 保持传统串口工具的高信息密度与低操作路径。
- 将串口 I/O 放入独立工作线程，避免主界面阻塞。
- 把多字符串发送与协议工具箱从主发送区解耦，形成独立发送链路。
- 用本地 INI 配置文件持久化保存显示选项、波特率、窗口布局、表格列宽、发送区草稿、Modbus 解析格式、多字符串预设、字体和文件发送参数等常用设置。
- 通过 `scripts/package-windows.ps1` 统一生成便携版和可选安装包，避免手工遗漏 Qt 运行库。

## 后续准备增加和修改的功能

### 近期计划

- 继续打磨统一 Modbus 读写工具，重点验证行数增加、窗口缩放、右键批量格式设置和表格滚动体验。
- 为 Modbus 增加更多常用数据格式与字节序选项，例如 `Double`、`Int32`、`UInt32` 和更多字节交换方式。
- 为 Modbus 增加参数预设保存能力，便于复用站号、功能码、地址、数量、扫描时间和写入值。
- 优化顶部控制区布局，减少局部留白与拥挤，提升串口参数区、发送参数区和工具入口的协调性。
- 为文件发送增加更明确的发送完成提示、异常中断说明与可复用方案保存。

### 中期计划

- 扩展 Modbus 功能码覆盖范围，评估加入 `01` / `02` 读线圈、`05` / `15` 写线圈等常用操作。
- 增加 Modbus 收发帧详情解析，支持在日志中展开站号、功能码、地址、数量、数据区和 CRC 校验结果。
- 增加更强的日志检索、关键字着色、会话留存和原始数据导出能力。
- 增加接收区与发送区的数据统计、异常诊断和吞吐观察信息。
- 优化大文件发送和高频串口场景下的界面响应策略。

### 持续改进方向

- 持续清理界面布局与视觉层级，降低局部拥挤和空白失衡问题。
- 持续收敛注释、文案、命名和配置项的一致性。
- 持续补充设计文档、模块说明、踩坑记录和后续开发约束。
- 将高频踩坑沉淀为可复用的 Codex skill，用于指导后续开发、验证和排障。
- 完善版本发布说明、打包流程和升级文档。

## 设计文档

详细设计请查看 [docs/design.md](docs/design.md)。
