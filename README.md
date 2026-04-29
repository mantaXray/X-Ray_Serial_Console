# X-Ray Serial Console

[简体中文](README.zh-CN.md) | [English](README.en-US.md)

## 简体中文

X-Ray Serial Console 是一个基于 Qt Widgets 的本地串口调试工具，当前定位为 Windows 优先、离线可用，面向串口联调、协议验证和设备调试场景。

- 查看完整中文文档：[README.zh-CN.md](README.zh-CN.md)
- Windows 构建：`pwsh.exe -NoProfile -ExecutionPolicy Bypass -File scripts\build-local.ps1`
- Windows 打包：`pwsh.exe -NoProfile -File scripts\package-windows.ps1 -Mode Portable -Clean`
- 详细设计：[docs/design.md](docs/design.md)

## English

X-Ray Serial Console is a local serial debugging tool based on Qt Widgets. It is Windows-first, works offline, and is designed for serial integration, protocol verification, and device debugging workflows.

- Read the full English documentation: [README.en-US.md](README.en-US.md)
- Windows build: `pwsh.exe -NoProfile -ExecutionPolicy Bypass -File scripts\build-local.ps1`
- Windows package: `pwsh.exe -NoProfile -File scripts\package-windows.ps1 -Mode Portable -Clean`
- Design notes: [docs/design.md](docs/design.md)
