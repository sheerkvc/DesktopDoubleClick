桌面双击隐藏 v2.0｜Windows 10 / 11 x64

快速开始
1. 先在托盘中退出旧版程序，再解压新版到固定文件夹。
2. 双击 DesktopDoubleClick.exe。首次运行会打开中文设置窗口。
3. 双击桌面空白处隐藏图标，再次双击恢复。
4. 单击右下角托盘图标打开设置；右键可暂停、显示/隐藏图标或退出。
5. 设置修改后点击“保存”。关闭设置窗口后，程序继续在后台运行。

新增设置
- 启用/暂停双击功能。
- 登录 Windows 后自动启动：默认关闭；使用当前用户的 Run 注册表项。
  自启静默运行，不弹设置窗口；程序移动位置后，请重新保存自启设置。
  请使用较短的本地固定路径；Windows Run 命令有长度限制。
- 退出程序时恢复图标：默认开启。
- 启动时显示托盘提示：默认关闭。
- 手动启动时打开设置：默认开启。
- 双击速度：跟随系统，或使用 200 / 350 / 500 / 700 毫秒。
  这里只改变本程序的判定，不改变系统鼠标双击设置。
- 以管理员身份重启：仅本次提升，系统会显示 UAC 确认。
  普通权限即可使用。开机自启仍使用普通权限，不创建提权计划任务。
  请先保存设置。使用另一个管理员帐户提升时，会使用那个帐户的设置。
  返回普通模式：退出程序后直接双击 EXE；若资源管理器本身已提升，需从普通权限进程启动。
- 恢复默认值：填入默认设置，保存后生效。
- 程序目录：打开当前 EXE 所在的文件夹。

内存目标与限制（请留意）
默认启用“限制驻留内存至 5 MiB”。
使用 Windows SetProcessWorkingSetSizeEx 的 HARDWS_MAX_ENABLE 标志，
请求把本进程的工作集上限设为 5,242,880 字节。
这限制的是物理驻留页，不等于进程的私有提交量或虚拟地址空间。
设置页分别显示“当前驻留”和“私有提交”，单位为 MiB。
因此，不能将这版程序理解为“所有内存指标、所有场景都绝不超过 5 MB”。
若系统拒绝设置限制，程序会提示失败，不会把它显示成已启用。
限制驻留内存可能增加缺页和操作延迟；如有卡顿，可在设置中关闭限制。
“刷新读数”只读取一次数据，不持续轮询；读数是取样时刻的快照。
本程序不定时清空工作集，也不通过反复清内存制造偏低读数。
设置窗口关闭时销毁，后台不保留设置界面控件。
原生 C++ / Win32，不使用 .NET、Electron、WebView、Python 或额外常驻服务。
不联网、不下载依赖、不采集信息。

关于“低于 5 MB”的验证状态
制作环境为 Linux，尚未在 Windows 实机测试交互、实际内存或峰值。
已验证：x64 编译、严格编译警告、双击逻辑回归检查、
系统 DLL 依赖，以及中文对话框资源结构与边界。
尚未验证：Windows 桌面切换、UAC 交接、自启、不同 DPI 和常驻内存。
不以 EXE 文件大小代替运行内存，也不把编译成功视为内存达标。

可自行检查内存
打开设置，点击“刷新读数”；也可在任务管理器“详细信息”中找到进程，
添加“工作集（内存）”“内存（专用工作集）”“提交大小”等列比较。
打开设置、连续隐藏/显示几十次、关闭设置，再分别查看。
任务管理器的默认“内存”列与提交大小含义不同，数值可能不同。

操作规则
- 双击图标或图标文字不会触发隐藏。
- 拖动框选、长按、按住 Ctrl / Shift / Alt / Win 的点击不触发。
- 保留系统位置容差与拖动阈值，支持多显示器负坐标。
- 托盘“暂停”为本次临时暂停；设置中的“启用”可保存为下次启动状态。
- 重新双击 EXE 会尝试打开现有实例设置，不启动第二个监听实例。
- 资源管理器重启后，托盘图标会重新添加，桌面连接在操作时重新获取。
- 针对默认 Explorer 桌面；动态壁纸、桌面整理软件可能截获点击，需要实测。

图标恢复
如果程序被强制结束，图标可能保持隐藏。
桌面右键 → 查看 → 显示桌面图标，可手动恢复。
正常退出时是否恢复由设置决定，默认恢复。

配置位置和移除
设置：HKCU\Software\DesktopDoubleClick，SettingsV2。
自启：HKCU\Software\Microsoft\Windows\CurrentVersion\Run，DesktopDoubleClick。
不修改管理员权限策略，不禁用 UAC，不修改其他启动项。
移除前先在设置取消自启并保存，退出后删除程序文件夹即可。
设置仅占一个很小的注册表值；如要清除，可手动删除上述程序设置项。
不要删除整个 Run 项，只删除 DesktopDoubleClick 值。

源码与编译
src/main.cpp                 桌面接口、托盘、监听及消息循环
src/settings_ui.h            原生中文设置窗口、权限重启、设置应用
src/app_config.h             注册表设置、自启、内存限制与读数接口
src/resource.h / app.rc       控件 ID 与对话框资源
src/click_matcher.h           平台无关的双击判定
src/test_click_matcher.cpp    双击回归检查
src/build.sh                 Linux MinGW-w64 x64 构建
src/build.cmd                Windows Visual Studio x64 构建
SHA256.txt                   EXE 校验值

Visual Studio：在 x64 Native Tools Command Prompt 运行 src\build.cmd。
Linux：安装 MinGW-w64 后运行 bash src/build.sh。
单元检查：g++ -std=c++11 src/test_click_matcher.cpp -o test_clicks，再运行。

参考
https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-setprocessworkingsetsizeex
https://learn.microsoft.com/en-us/windows/win32/memory/working-set
https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-ifolderview2-setcurrentfolderflags
https://learn.microsoft.com/en-us/windows/win32/api/oleacc/nf-oleacc-iaccessible-acchittest
https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelmouseproc
