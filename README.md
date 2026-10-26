# 桌面双击隐藏 · DesktopDoubleClick

适用于 Windows 10 / 11 x64 的原生 C++ / Win32 桌面工具。
双击桌面空白处隐藏桌面图标，再次双击恢复。

## 功能

- 中文设置窗口和系统托盘菜单。
- 登录 Windows 后自动启动（默认关闭）。
- 暂停 / 恢复功能，手动显示 / 隐藏图标。
- 跟随系统双击速度，或选择 200 / 350 / 500 / 700 毫秒。
- 退出时恢复图标（默认开启）。
- 启动提示和手动启动时打开设置窗口均可配置。
- 以管理员身份重启（仅本次，使用系统 UAC）。
- 5 MiB 驻留内存限制和手动刷新内存读数。
- 免安装，不需要 .NET、Electron、WebView 或额外常驻服务。

## 使用

下载 [Windows x64 程序包](downloads/DesktopDoubleClick_Windows_x64.zip?raw=true)，解压后运行 `DesktopDoubleClick.exe`。
首次启动会打开设置。单击托盘图标打开设置，右键托盘图标显示菜单。
取消“手动启动时打开设置窗口”并保存，可在之后手动启动时直接驻留托盘。
开机自启始终静默启动。

双击图标及其文字不会触发隐藏；拖动框选、长按和带修饰键的点击也不触发。
支持多显示器负坐标。针对 Windows 默认 Explorer 桌面设计。

## 内存说明

默认请求 Windows 为本进程启用 **5 MiB 工作集硬上限**。
工作集是物理驻留页，与私有提交量、虚拟地址空间不同。
**此设置不代表所有内存指标、所有场景均不超过 5 MB。**
设置页分别显示当前驻留量和私有提交量。系统若拒绝设置限制，会提示失败。
限制可能增加缺页及响应延迟；如遇卡顿，可关闭此选项。
后台没有定时刷新设置界面或反复清空工作集的任务。

## 当前验证状态

- 已在 Linux 上使用 MinGW-w64 交叉编译 Windows x64 EXE。
- 已通过严格编译警告、双击判定回归检查和 PE 系统 DLL 依赖检查。
- 已检查二进制对话框资源中的控件 ID、中文文字和几何边界。
- **尚未在 Windows 实机验证桌面交互、自启、UAC 交接、DPI 或内存峰值。**

## 编译

### Windows / Visual Studio

在 x64 Native Tools Command Prompt 中运行：

```bat
cd src
build.cmd
```

### Linux / MinGW-w64

安装 MinGW-w64 工具链后，在仓库根目录运行：

```bash
bash src/build.sh
```

两种方式均在仓库根目录输出 `DesktopDoubleClick.exe`。

### 双击逻辑回归检查

```bash
g++ -std=c++11 -O2 -Wall -Wextra -Werror src/test_click_matcher.cpp -o /tmp/test_clicks
/tmp/test_clicks
```

## 配置与移除

配置保存在当前用户注册表 `HKCU\Software\DesktopDoubleClick`。
自启使用 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` 中的 `DesktopDoubleClick` 值。
移除前请取消自启并保存，退出程序后删除程序文件。

若程序被强制结束且桌面图标仍隐藏，可通过桌面右键 → 查看 → 显示桌面图标恢复。

更多使用说明、兼容性和限制见 [README.txt](README.txt)。
