# X-Ray Serial Console

[简体中文](README.zh-CN.md) | [English](README.en-US.md)

X-Ray Serial Console is a local serial debugging tool based on Qt Widgets. It is currently Windows-first, works offline, and is designed for serial integration, protocol verification, and device debugging workflows.

The current project name is `xray_serial_console`, the product brand is `X-Ray`, and the author is `mantaXray`.

## Implemented Features

### Serial Connection and Basic Controls

- Enumerates serial ports and supports manual refresh.
- Opens and closes serial ports.
- Configures baud rate, data bits, parity, and stop bits.
- Supports common high-speed baud rates, including `1M`, `1.5M`, `2M`, `2.5M`, and `3M`.
- Supports custom baud-rate input and persistence.
- Provides manual `RTS` / `DTR` control.
- Shows both `COM` port names and device descriptions in the port list.

### Receive Area

- Supports plain text and `HEX` display modes.
- Supports timestamps.
- Supports word wrap.
- Supports pause refresh for high-frequency traffic observation.
- Supports timeout-based packet grouping.
- Exports received content to files.
- Supports continuous receive-data saving.
- Supports receive-content filtering and keyword highlighting.

### Transmit Area

- Supports plain text and `HEX` transmit modes.
- Supports appending a newline to transmitted content.
- Supports cyclic transmit and configurable transmit interval.
- Supports checksum appending, including `CRC16`, `CRC32`, sum checksum, XOR checksum, and related modes.
- Supports importing transmit files and shows a safe preview prompt for large files.
- Supports direct file transmission.
- Supports file-transmit presets, custom chunk size, and inter-chunk delay.
- Shows file-transmit progress and supports cancellation.
- Restores the last transmit editor draft after restart.
- Supports unified font family and font-size adjustment for receive and transmit areas.

### Extended Tools

- Provides a multi-string transmit panel.
- The multi-string panel supports saving current transmit content, enable/disable coordination, polling transmit, and persistent table column widths.
- When the multi-string panel opens by default, the table receives enough width for its main columns. The send, HEX send, and newline columns use the same high-contrast checkbox style as the main UI. New blank rows default to plain-text sending; enable the "HEX Send" column when HEX sending is required.
- Provides a protocol and data toolbox.
- Provides a unified Modbus read/write tool. It supports `03` / `04` register reads, `06` / `16` register writes, periodic scanning, automatic parsing, table display, default data-format switching, and raw request/response logs.
- The Modbus tool table fills the window by default. Parsed-value cells can be configured per address from the context menu, and multiple selected cells can be configured in batch.
- Modbus parsing supports `Float32 AB CD` and `Float32 CD AB`. Float32 values are composed from two consecutive 16-bit registers starting at the current address.
- The Modbus tool persists window position, splitter ratio, register-table column widths, default data format, per-address parse-format overrides, raw-log timestamp setting, and independent timeout packet grouping.
- The Modbus raw log only records responses to requests sent by the tool itself. After the window closes, scanning stops, or a response times out, the receive session is released and no longer consumes replies to messages sent from the main panel.
- During Modbus scanning, the main receive area and the Modbus raw log use their own timeout packet grouping strategies. Tool request logs, layout persistence, and log-option persistence do not flush the main receive buffer early.
- The default Modbus layout prioritizes enough height for the raw request/response log while keeping the register table and top parameter controls compact. Numeric fields such as scan interval have enough width to show full values, and the default window width avoids unnecessary right-side blank space.
- The toolbox currently includes:
  - Calculator
  - Text / HEX conversion
  - Custom protocol packet building and parsing
- The Tools menu keeps the "Protocol and Data Toolbox" as a secondary tool-page entry. The standalone Modbus read/write tool is shown directly under the Tools menu to avoid an empty submenu when there is only one function.
- Main-panel transmit, polling transmit, multi-string transmit, and tool transmit are all recorded in the receive monitor as transmit-direction frames. Disabling timestamps hides only the time, not transmit frames.
- When receive filtering is active, the title and empty-state prompt indicate the filter status to avoid confusion when S/R counters increase but filtered content has no matches.

### UI and Engineering

- Supports Chinese / English UI switching.
- Integrates multi-string and tool entries into the top menu.
- Shows fixed units such as milliseconds outside numeric input boxes to avoid taking editable space or implying that the unit is editable.
- Automatically generates `xray_serial_console.ini` to persist window layout, serial parameters, display options, transmit draft, transmit options, Modbus tool layout, Modbus parse-format settings, and multi-string presets.
- When run from a Qt Creator / CMake build directory, settings are written to the system application configuration directory and old build-directory INI files are migrated automatically. This avoids losing baud-rate, timeout packet grouping, and transmit draft settings after build cleanup. Portable directories that already contain an INI file continue to use the program-directory configuration.
- The Help menu includes usage instructions covering basic send/receive, receive display, multi-string sending, Modbus, configuration, and packaging entry points. The About window keeps only the current version and first-release notes.
- The bottom status bar uses segmented status blocks for brand, send/receive counters, serial status, and line status.
- The project has completed `X-Ray` branding and translation-file renaming.

## Project Structure

```text
.
|-- src/                         # C++ implementation files and program entry
|-- include/                     # C++ header files
|-- ui/                          # Qt Designer UI files
|-- resources/                   # qrc resource manifest and icon resources
|-- translations/                # Qt translation source files
|-- docs/                        # Design documentation
|   `-- reference-images/        # Local reference screenshots, ignored by default
|-- CMakeLists.txt               # CMake build configuration
`-- README.md                    # GitHub language selector
```

`AGENTS.md`, `README.md`, `README.zh-CN.md`, `README.en-US.md`, `docs/design.md`, `docs/packaging-windows.md`, `docs/pitfalls.md`, and `docs/skills/qt-widgets-cpp/SKILL.md` are included in the CMake document file list, so Qt Creator can show them in the project tree after CMake reconfiguration.

## Build

### Dependencies

- CMake 3.16 or later
- Qt 6 or Qt 5 with `Widgets` and `LinguistTools`
- `setupapi` on Windows

### Windows / Qt 6.11 Local Build

```powershell
pwsh.exe -NoProfile -ExecutionPolicy Bypass -File scripts\build-local.ps1
```

The script uses the local Qt installation path `D:\QT\6.11.0\mingw_64`, CMake path `D:\QT\Tools\CMake_64\bin\cmake.exe`, and MinGW path `D:\QT\Tools\mingw1310_64\bin` by default. This avoids another MinGW installation in `PATH` taking precedence.

You can also use the CMake preset. Qt Creator usually detects it after reconfiguring the project:

```powershell
D:\QT\Tools\CMake_64\bin\cmake.exe --preset qt-6.11-mingw-debug
D:\QT\Tools\CMake_64\bin\cmake.exe --build --preset qt-6.11-mingw-debug
```

If Qt is installed in a different location, pass explicit script parameters:

```powershell
pwsh.exe -NoProfile -ExecutionPolicy Bypass -File scripts\build-local.ps1 -QtPrefixPath "D:\Qt\6.x.x\mingw_64" -MingwBinPath "D:\Qt\Tools\mingwxxxx_64\bin"
```

### VS Code Run and Debug

The repository provides `.vscode/settings.json`, `.vscode/tasks.json`, and `.vscode/launch.json`. They use the `qt-6.11-mingw-debug` preset, MinGW `gdb.exe`, and inject the Qt and MinGW runtime directories into `PATH` for launch/debug sessions.

In VS Code, use:

```text
Run and Debug: X-Ray Serial Console (MinGW Debug)
CMake: Run Without Debugging
CMake: Debug
```

For manual terminal execution, set the runtime environment first:

```powershell
$env:PATH = "D:\QT\6.11.0\mingw_64\bin;D:\QT\Tools\mingw1310_64\bin;$env:PATH"
& ".\build\qt-6.11-mingw-debug\xray_serial_console.exe"
```

### Generic CMake Example

```powershell
cmake -S . -B build
cmake --build build
```

## Packaging

The project provides a Windows packaging script:

```powershell
pwsh.exe -NoProfile -File scripts\package-windows.ps1 -Mode Portable -Clean
```

You can also double-click:

```text
scripts\package-windows.bat
```

The BAT entry runs the portable packaging flow by default. Command-line arguments passed to the BAT file are forwarded to `package-windows.ps1`.

See [docs/packaging-windows.md](docs/packaging-windows.md) for the full packaging guide.

By default, the packaging script uses `D:\QT\6.11.0\mingw_64` and `windeployqt.exe` to generate a Release portable package under `packages/`:

- `packages/xray_serial_console-<version>-portable/`
- `packages/xray_serial_console-<version>-portable.zip`

If Qt is installed in a different location, pass `-QtPrefixPath`, for example:

```powershell
pwsh.exe -NoProfile -File scripts\package-windows.ps1 -Mode Portable -QtPrefixPath "D:\Qt\6.x.x\mingw_64"
```

To create an installer, install Inno Setup 6 first, then run:

```powershell
pwsh.exe -NoProfile -File scripts\package-windows.ps1 -Mode Installer -Clean
```

The script first creates the portable package, then generates `packages/installer/xray_serial_console.iss`, and continues to build the installer when `ISCC.exe` is available. If Inno Setup is not installed, publish the portable ZIP first.

Packaging should be run in a normal PowerShell terminal, a Qt Creator terminal, or a local development terminal. In restricted sandboxes, `moc.exe` / `windeployqt.exe` may fail to launch child processes, causing build or deployment failures.

After Qt Creator reconfigures CMake, `package-windows.ps1` and `package-windows.bat` are copied to the current build directory root, such as `build\Desktop_Qt_6_11_0_MinGW_64_bit-Debug\package-windows.bat`. When that copied script is run from the build directory, it reads `CMakeCache.txt` to locate the source root automatically.

## Current Design Focus

- Keep the high information density and short operation paths expected from traditional serial tools.
- Keep serial I/O in a dedicated worker thread to avoid blocking the UI thread.
- Decouple multi-string sending and protocol tools from the main transmit area and give them independent transmit paths.
- Persist common display, baud-rate, layout, table-width, transmit-draft, Modbus parse-format, multi-string preset, font, and file-transmit settings through a local INI file.
- Use `scripts/package-windows.ps1` to generate portable and optional installer packages consistently, avoiding missing Qt runtime dependencies during manual packaging.

## Planned Work

### Short-Term

- Continue refining the unified Modbus read/write tool, especially row-count scaling, window resizing, batch format setting from the context menu, and table scrolling.
- Add more common Modbus data formats and byte orders, such as `Double`, `Int32`, `UInt32`, and additional byte-swap modes.
- Add Modbus parameter preset persistence for station ID, function code, address, quantity, scan interval, and write value reuse.
- Optimize the top control area layout to reduce local blank space and crowding, improving coordination between serial parameters, transmit parameters, and tool entries.
- Add clearer file-transmit completion prompts, abnormal interruption explanations, and reusable scheme persistence.

### Mid-Term

- Expand Modbus function-code coverage and evaluate `01` / `02` coil reads and `05` / `15` coil writes.
- Add detailed Modbus frame analysis so logs can expand station ID, function code, address, quantity, data area, and CRC result.
- Add stronger log search, keyword coloring, session retention, and raw-data export.
- Add receive/transmit statistics, abnormal diagnostics, and throughput observation.
- Improve UI responsiveness for large-file transmit and high-frequency serial scenarios.

### Continuous Improvement

- Continue cleaning up layout and visual hierarchy to reduce local crowding and blank-space imbalance.
- Continue aligning comments, copywriting, naming, and configuration keys.
- Continue adding design documentation, module notes, pitfall records, and future development constraints.
- Convert recurring pitfalls into reusable Codex skills for future development, verification, and troubleshooting.
- Improve version release notes, packaging workflow, and upgrade documentation.

## Design Documentation

See [docs/design.md](docs/design.md) for detailed design notes.
