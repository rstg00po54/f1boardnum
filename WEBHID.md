# MultiPad WebHID 灯效与 USB 日志控制（开发分支）

> **实验功能，尚未通过硬件验证。** 请先用 ST-Link 备份已能工作的 `main` 固件，并在确认能恢复后再刷这个分支。不要直接合并到 `main`。

## 做了什么

STM32F103C8T6 原有 USB Boot Keyboard HID（接口 0、EP 0x81、8 字节）保持不变，新增独立的 **Vendor WebHID**（接口 1、EP 0x82 IN / 0x02 OUT，均 32 字节，Usage Page `0xFF60`、Usage `0x61`）。这不会自动成为 QMK/VIA 设备，只给仓库自带的网页控制台使用。

- 灯效：关闭 / 呼吸 / 常亮 / 彩虹流动。
- 可调节颜色、亮度、速度（**仅在 RAM 生效；断电后恢复默认**）。
- USB 日志：网页选择「开启日志」之后，将 `printf()` 的 UART1 内容同步排队发送到 USB HID；若浏览器接收速度不足，优先保护按键输入，可能丢弃部分日志。
- 页面显示 MCP 在线数量、键位数、RGB 灯数。
- 原来的 6 颗 MCP、87 个键位、USART1 115200 和 Fn + Backspace 长按 3 秒重启保留。原来的按键高亮只覆盖有 `led` 编号的键位。

## 编译、刷机

在仓库根目录执行：

```bash
git fetch origin && git switch --track origin/webhid_rgb_console
cmake --preset Release && cmake --build --preset Release -j 2
arm-none-eabi-objcopy -O binary build/Release/f1board.elf build/Release/f1board.bin
```

如果你已经在此分支，直接 `git pull --ff-only`。烧录方式沿用当前的 ST-Link/SWD，**WebHID 不等于 Bootloader，也不增加 USB 刷机能力**。

仓库包含 GitHub Actions 交叉编译检查；先确保编译通过，并检查 Flash 不超过 STM32F103C8T6 的 64KB。

## 打开网页

使用 Chrome 或 Edge，在仓库根目录执行：

```bash
python3 -m http.server 8080
```

然后打开：

http://localhost:8080/web/

点击 **连接键盘**，授权 `MultiPad Direction` 带 `0xFF60/0x61` 厂商自定义集合的 HID 接口。若网页提示找不到接口，说明可能刷的是旧固件，或 USB 描述符未成功枚举。

**重要：** WebHID 要求安全上下文（`localhost` 或 HTTPS）。Firefox、Safari 等浏览器不一定支持。本站不会把日志发送到服务器，但日志里包含按键事件，调试时不要输入密码。

## 通信协议（定长 32 字节，无 Report ID）

现阶段固件沿用 STM32Cube 示例 VID/PID：`0483:572B`。该 VID 不属于本项目，不应用于商品化或批量出货；将来应换成合法授权的 VID/PID 并同步修改网页。

**主机发送**：所有消息 32 字节，未使用的字节填 0。

| Byte 0 | 用途 | 参数 |
|---|---|---|
| `0x01` | 读取状态 | 无 |
| `0x02` | 设定模式 | Byte 1: 0=关闭，1=呼吸，2=常亮，3=彩虹 |
| `0x03` | 设定颜色 | Bytes 1-3: R/G/B，0-255 |
| `0x04` | 设定亮度 | Byte 1: 0-255 |
| `0x05` | 设定速度 | Byte 1: 1-10 |
| `0x06` | 开关日志 | Byte 1: 0=关、1=开 |

**固件回复**：

- `0x81` 状态：`[0x81, version, mode, brightness, red, green, blue, speed, log_enabled, mcp_online, mcp_total, key_count, led_count, ...]`
- `0x82` 回执：`[0x82, command, ok, ...]`；`ok=1` 表示参数已接受
- `0x80` 日志：`[0x80, length (0-30), 30 bytes UTF-8/ASCII payload]`

## 排查

1. 串口日志原样继续通过 USART1（PA9 TX / PA10 RX）输出。
2. USB 能打字但网页不能连接：检查 `lsusb -v` 是否存在两个 HID 接口；页面过滤的是 `0483:572B` 与 Usage Page `0xFF60`。
3. USB 完全无法枚举：用 ST-Link 恢复 `main` 分支的已知可用固件，核对 USB 描述符、EP2 和 PMA 分配。
4. 灯效设置能接收但不变：检查 PB6 的 RGB 信号与 100 颗 LED 供电，原程序中 RGB 输出极性保持不变。
5. 需要 `usevia.app` 官方页面支持：那属于另外的 VIA 协议移植工作，这个自定义页面不会自动出现在 VIA 列表里。
