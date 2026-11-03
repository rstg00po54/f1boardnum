# MultiPad 方向板 Pin Map

## 主控 U25
- MCU: STM32F103C8T6
- USB D-: PA11 / HID_DN
- USB D+: PA12 / HID_DP
- SWDIO: PA13
- SWCLK: PA14
- USART1 TX: PA9
- USART1 RX: PA10
- RGB 数据控制: PB6
- MCP23008 SCL: PB8
- MCP23008 SDA: PB9

## MCP23008
- GP0 = R3
- GP1 = R2
- GP2 = R1
- GP3 = R0
- GP4 = C3
- GP5 = C2
- GP6 = C1
- GP7 = C0

矩阵扫描：R0~R3 输出，C0~C3 输入上拉。

## 10 键
| 键 | Row | Col | HID Usage |
|---|---:|---:|---:|
| Insert | R0 | C1 | 0x49 |
| Home | R1 | C1 | 0x4A |
| PageUp | R2 | C1 | 0x4B |
| Delete | R0 | C2 | 0x4C |
| End | R1 | C2 | 0x4D |
| PageDown | R2 | C2 | 0x4E |
| Left | R0 | C3 | 0x50 |
| Down | R1 | C3 | 0x51 |
| Right | R2 | C3 | 0x4F |
| Up | R3 | C3 | 0x52 |

## 软件重启快捷键
- **Fn + Space 长按 3 秒**：执行 STM32 软件复位（`NVIC_SystemReset()`），正常重新启动键盘，不是进入 DFU Bootloader。
- Fn：MCP23008 `0x21`，R2/C3（暂不发送 USB 键值）；Space：MCP23008 `0x24`，R3/C3。
- Fn 按下时 Space 不发送给电脑，避免等待重启时误输入空格；松开任一键立即取消计时。
- 重启后需至少松开一个按键才能重新触发，防止持续按住时重复重启。
