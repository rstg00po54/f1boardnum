# KeyFlux Studio (Qt Quick / C++17)

Windows 原生桌面键盘灯效控制台，支持 **MultiPad STM32F103** + **Keychron V6 Max**。
代码位于仓库的 `desktop/KeyFlux/`，**不改已稳定的 STM32 固件**。

## 当前实现（第一版）

| 功能 | MultiPad | Keychron V6 Max |
|---|---|---|
| HID 设备检测与自动重连 | ✓ | ✓（需要 USB Raw HID） |
| 预览/调整灯效 | 自定义协议 0x02–0x05 | VIA RGB Matrix 0x07/0x03 |
| 亮度、速度、基础颜色 | ✓ | ✓（HSV 颜色） |
| 保存到设备 | STM32 Flash 0x08 | VIA EEPROM 0x09/0x03 |
| 状态刷新 | GET_STATE 0x01 | VIA GET 0x08/0x03 |
| 实时日志 | MultiPad 0x80 | QMK Console FF31/74（定制固件） |
| 日志开关 | 0x06 | 自定义 V6LG 命令（定制固件） |
| 单灯定位 | 71 个键位映射到原始 LED 编号 | 暂不支持 |

- MultiPad: VID/PID `0483:572B`，专用 HID Usage `FF60:61`，32 字节报告。
- Keychron: VID/PID `3434:0960`，VIA Raw HID Usage `FF60:61`。
- Keychron QMK 自定义日志通道来自仓库 `rstg00po54/qmk_firmware` 的 `v6max_dfumode_hotkey` 分支；**官方固件不一定支持日志开关**。主界面会先检测支持后再启用。
- 有线 USB 是第一版唯一承诺能使用 VIA 命令的连接方式；蓝牙/2.4G 连接可能只被识别为普通键盘，不能控制灯效。
- 键盘可视化为位置/颜色示意，**不是实际 LED 回读**。MultiPad 的 71 键可点击进行红色单灯定位；小键盘尚未焊好，暂不合并进空间映射。
- 第一版 **不支持键位重映射、宏、刷写固件**；这些是后续独立功能。
- Keychron 各灯效编号按其用户 QMK fork 中 `via_json/v6_max_ansi_encoder.json`。不同固件的效果菜单可能不同。

## VS Code（Windows + Qt 6.11.2 MinGW 64-bit）

1. 安装 VS Code 扩展：**CMake Tools**、**C/C++**、**Qt Qml**。
2. 从 GitHub 拉取 `keyflux_qt` 分支，在 VS Code 打开 **`desktop/KeyFlux` 文件夹**。
3. `Ctrl+Shift+P` → `CMake: Select a Kit` → 选择和 Qt 配套的 MinGW 64-bit。
4. 设置 CMake 的 Qt 路径（请以实际安装目录为准）：

   ```powershell
   cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.11.2/mingw_64
   cmake --build build --parallel
   ```

   如果使用 CMake Tools，等效做法是在工作区设置 `cmake.configureSettings` 的 `CMAKE_PREFIX_PATH`，然后 `CMake: Configure` → `CMake: Build`。**编译器必须匹配 Qt 对应的 MinGW 版本**。

5. 在 Qt Creator 中也可直接打开 `CMakeLists.txt`，选择截图中的 `Desktop Qt 6.11.2 MinGW 64-bit` Kit，点击运行。
6. 首次配置需要联网：CMake 会自动 FetchContent 下载官方 HIDAPI 0.15.0 源码并静态编译；日后正常运行不需要网络。

> Windows 打包（使用安装的 Qt/MinGW 环境，在构建目录找到 `KeyFlux.exe`）：
>
> ```powershell
> windeployqt --release --qmldir qml build/KeyFlux.exe
> ```
>
> 打包时要在 Qt 的 MinGW 命令环境中使用 `windeployqt`，并将对应的 MinGW 运行库一同发布。不同 CMake 生成器的 exe 目录可能不同。

## STM32 硬件性能仪表盘（实验版）

**需要配套的遥测固件**（`keyflux_telemetry` 分支自动构建的 `f1board-keyflux-telemetry-firmware`）。旧版 `main` 固件仍能控制 RGB，但不会回传性能数据；仪表盘显示「需刷遥测版」。

监控命令（只读）：
- PC 请求：32 字节 HID 报告 `[0x09, 0, ...]`
- STM32 回应：32 字节 `0x83`，`[1]=1` 为格式版本，`[2]=10` 为 USB FS 中断端点 `bInterval`（**10ms/100Hz 标称轮询**），`[15]=1` 为已采样
- 后续 16 位无符号小端：`[3..4]` 实际扫描 Hz，`[5..6]` 平均扫描 μs，`[7..8]` 最大扫描 μs，`[9..10]` 已提交 HID 报告/秒，`[11..12]` CPU 活跃千分比，`[13..14]` 采样窗口毫秒

**重要区别：**
- 100Hz 是 USB 描述符配置的轮询间隔换算值，并不是按键报告实测 100 次/s。
- HID 提交速率按键状态变化才增加，闲置为 0 正常。不是输入延迟、也不是主机实际轮询计数。
- CPU 负载通过 Cortex-M3 DWT 计数器统计主循环执行周期 / 真实采样窗口，闲置时使用 `__WFI()` 睡眠。它是**CPU 前台活跃占比估计**，中断在睡眠期间执行的耗时可能未完整计入，不等价于 PC 系统任务管理器的 CPU 百分比。
- 扫描平均/最大值包含 I²C 位操作及该次扫描中断执行时间。
- 测试前备份稳定固件，先验证按键输入、USB 枚举、RGB 显示、连接恢复及扫描耗时；新固件需实机验证后才能视为稳定。
- Keychron V6 Max 的性能指标需要它自己的 QMK 固件配合；当前面板只针对 STM32 MultiPad。

## CMake / HIDAPI 技术结构

```text
desktop/KeyFlux/
  CMakeLists.txt            Qt6 + FetchContent(hidapi-0.15.0)
  src/main.cpp              Qt Quick 入口
  src/DeviceBackend.h       UI 属性、命令、HID 线程
  src/DeviceBackend.cpp     MultiPad + VIA/QMK 两套协议
  qml/Main.qml              深色控制界面 / RGB 预览
```

- USB 通信在两个独立工作线程完成；UI 线程不执行阻塞式 HID 读写。
- Keychron 日志使用独立 Console HID `0xFF31:0x74`，不把键盘输入端点当作日志接口。
- 不连接设备时界面仍能启动；控制按钮会禁用。
- 日志最多保留约 13K 字符，避免持续输出造成内存累积。

## 安全与恢复

建议先在 PC 上测试识别、RGB 模式和亮度，确认无误再测试 Flash/EEPROM 保存。不要连续高速执行保存（正常滑块操作不会写 Flash）。固件依然以 GitHub 的 `main` 为已验证版本，本 Qt 应用在 `keyflux_qt` 分支开发。

## 测试状态

此分支首先使用 GitHub Actions 编译检查 Windows .exe / Qt 资源；**硬件的 HID 端口读写和 Keychron 灯效还需要你的 Windows 实机测试**。不得将编译成功视为实机验证通过。
