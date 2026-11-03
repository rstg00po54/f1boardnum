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
- **Fn + Backspace 长按 3 秒**：执行 STM32 软件复位（`NVIC_SystemReset()`），正常重新启动键盘，不是进入 DFU Bootloader。
- Fn：MCP23008 `0x21`，R2/C3（暂不发送 USB 键值）；Backspace：MCP23008 `0x21`，R3/C0。
- Fn 按下时 Backspace 不发送给电脑，避免等待重启时误删文字；松开任一键立即取消计时。
- 重启后需至少松开一个按键才能重新触发，防止持续按住时重复重启。

## 小键盘 MCP23008 0x27（键盘87_小键盘）

根据 `ProDoc_键盘87_小键盘_2026-11-03.epro` 中的 PCB PAD_NET 网表：

- 地址：`0x27`（实机 I²C 探测确认；MCP A2/A1/A0 均为高电平）
- MCP GP7 → C0、GP6 → C1、GP5 → C2、GP4 → C3
- MCP GP3 → R0、GP2 → R1、GP1 → R2、GP0 → R3
- 下面使用 USB HID 数字小键盘专用键值；数字键受电脑的 Num Lock 状态影响

| 行 / 列 | C0 | C1 | C2 | C3 |
|---|---|---|---|---|
| R0 | Num Lock (U1, 0x53) | KP7 (U2, 0x5F) | KP4 (U56, 0x5C) | KP1 (U62, 0x59) |
| R1 | KP/ (U44, 0x54) | KP8 (U51, 0x60) | KP5 (U57, 0x5D) | KP2 (U63, 0x5A) |
| R2 | KP* (U71, 0x55) | KP9 (U73, 0x61) | KP6 (U75, 0x5E) | KP3 (U66, 0x5B) |
| R3 | KP- (U72, 0x56) | KP+ (U74, 0x57) | KP Enter (U68, 0x58) | KP. (U67, 0x63) |

**注意：** 网表中还有 U3（标注「.」），但它的 K1、K2 分别连接到独立的 `KEY1`、`KEY2` 网络，没有连接到上述矩阵，因此没有给 U3 添加键位；该板的 MCP 矩阵中也没有数字小键盘 `0` 的接线。需要补线或修改原理图后才能加入该键。

当前固件总计 **87 个键位映射**（原 71 + 小键盘 16）。其余 MCP、Fn + Backspace 长按 3 秒重启及 RGB 扫描配置保持原样。
