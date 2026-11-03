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

## How to test

1. Save a working copy of your old stable `.bin` (for recovery). Have ST-Link / bootloader flashing recovery available.
2. Download `f1board-hardware-i2c1-400khz-firmware` from this branch's passing GitHub Actions `KeyFlux STM32 Hardware I2C1 400kHz` run; use `f1board.bin`.
3. Flash with the **same verified flashing workflow and target address** used for your prior stable image.
4. After reboot, open KeyFlux with MultiPad selected; verify 6/6 MCP online, press/release many keys (including simultaneous keys), inspect actual scan Hz, average/max scan ms, CPU active % and dropped/offline devices. Check RGB, USB reconnection.
5. Compare actual metrics with the previous ~29.61ms/scan image. A successful compile **does not prove electrical timing or input correctness** on hardware.

The USB HID descriptor is **still bInterval=10ms = nominal 100Hz**. This change increases matrix communication speed; it does **not** automatically make USB polling 1000Hz. Blocking HAL I2C also consumes foreground CPU cycles; achieving low CPU time with sustained high scan Hz may require a nonblocking driver or changed scan scheduling.

## Compile

From repository root: `cmake --preset Release && cmake --build --preset Release -j 2` (ARM GCC toolchain needed).
