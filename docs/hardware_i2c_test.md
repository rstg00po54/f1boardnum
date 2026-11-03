# STM32F103C8T6 / MCP23008 hardware I2C1 experiment

Development branch: `keyflux_hw_i2c` (based on `keyflux_telemetry`; stable `main` unchanged).

## Changes

- Replaces software-emulated I2C (bit-banging on PB8/PB9) with the STM32F103 **I2C1 peripheral, remapped to PB8=SCL / PB9=SDA**.
- Uses 7-bit addressing for the six MCP23008 devices; the ST HAL expects the addresses left-shifted by one bit.
- Fast-mode `400000` Hz, duty 2, standard 7-bit addressing and blocking HAL calls with a 3ms timeout.
- The six original MCP23008 addresses, keyboard matrix mapping, LED animation and WebHID telemetry protocol are unchanged.
- Calls `HAL_I2C_Mem_Write` / `HAL_I2C_Mem_Read` for MCP register settings; scans each matrix column with a short hardware transmit/receive pair, using MCP23008 IOCON.SEQOP=1 to keep the register pointer at GPIO.
- On startup/recovery, disables hardware I2C before attempting up to 9 open-drain SCL clocks and a STOP, then remaps and reinitializes I2C1. It never drives SDA or SCL push-pull.
- ST official `stm32f1xx_hal_i2c.c/.h` included under original ST source license in `Drivers/STM32F1xx_HAL_Driver`.

## Important electrical requirements

A shared I2C bus across six PCBs requires **external pull-up resistors to 3.3V** on SDA/SCL, common GND, acceptable combined bus capacitance and stable contacts. Software internal pull-ups are not a substitute for proper Fast-mode pull-ups. Check 3.3V logic compatibility on the actual board. If errors occur, switch `HW_I2C_CLOCK_HZ` from `400000U` to `100000U` in `Core/Src/main.c` and rebuild.

## Bus stuck low / MCP 0/6 recovery

If KeyFlux shows `MCP 0/6` and `PB8 SCL=0, PB9 SDA=0`, **405Hz and 0.06ms scan measurements are invalid**: no MCP columns have been scanned.

The original software I2C mode uses GPIO input pull-ups (`CRH` nibble `0x8`, `ODR=1` when lines are released). STM32F103 hardware I2C1 requires PB8/PB9 in alternate-function open-drain mode, which does **not** supply the old GPIO input pull-ups. A board relying only on internal pulls can therefore go from `MCP 6/6` to `MCP 0/6` when switched to hardware I2C. Another possibility is an actual short or a device actively holding SCL/SDA low, so physical idle-voltage measurements are needed to distinguish them.

Current test build:

- Resets I2C1 peripheral on recovery and probes configured addresses at 400kHz.
- If all addresses fail, retries hardware I2C at 100kHz.
- If hardware still detects no MCP, **automatically switches to the original software I2C protocol with GPIO internal pull-ups**. Log should contain `SOFTWARE FALLBACK`, and after initialization the six MCP devices should be visible again if no other wiring issue is present.
- It keeps this fallback until reboot, so the keyboard can remain functional while testing. **This does not make hardware 400kHz work without external pull-ups.**

To enable real 400kHz hardware I2C, verify that SCL and SDA each have suitable external resistors (a single pair on the shared bus, typically around 2.2kΩ–4.7kΩ to the **3.3V logic supply**, selected for total bus capacitance and sink current) and that all boards share GND. If unsure, measure the lines with a multimeter when idle; never short SDA/SCL directly to 3.3V. Always disconnect power before adding/removing wiring. With pull-ups fixed, reboot to retry hardware mode.

## How to test

1. Save a working copy of your old stable `.bin` (for recovery). Have ST-Link / bootloader flashing recovery available.
2. Download `f1board-hardware-i2c1-400khz-firmware` from this branch's passing GitHub Actions `KeyFlux STM32 Hardware I2C1 400kHz` run; use `f1board.bin`.
3. Flash with the **same verified flashing workflow and target address** used for your prior stable image.
4. After reboot, open KeyFlux with MultiPad selected; verify 6/6 MCP online, press/release many keys (including simultaneous keys), inspect actual scan Hz, average/max scan ms, CPU active % and dropped/offline devices. Check RGB, USB reconnection.
5. Compare actual metrics with the previous ~29.61ms/scan image. A successful compile **does not prove electrical timing or input correctness** on hardware.

The USB HID descriptor is **still bInterval=10ms = nominal 100Hz**. This change increases matrix communication speed; it does **not** automatically make USB polling 1000Hz. Blocking HAL I2C also consumes foreground CPU cycles; achieving low CPU time with sustained high scan Hz may require a nonblocking driver or changed scan scheduling.

## Compile

From repository root: `cmake --preset Release && cmake --build --preset Release -j 2` (ARM GCC toolchain needed).
