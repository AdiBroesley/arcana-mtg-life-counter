# Hardware verification status

The hardware build targets **Guition JC3636K718C_I_Y1 / I_YR1**, the finished 1.8-inch round IPS knob with base. This replaces the original, unverified Waveshare target. A seller's listing may advertise the screen as 1.85 inches. The earlier reported K829 marking is not a verified alternative board configuration.

## Evidence

The user supplied the listing PDF and `JC3636K718_knob_EN.zip` on 2026-11-20. The listing's edition-selection image identifies JC3636K718C_I_YR1. The archive contains the board schematic `6-Schematic diagram/JC3636K718.pdf`, base schematic `JC3636K718_P.pdf`, operating instructions, a getting-started guide, Arduino demos and a board-specific ESP-IDF voice-assistant example.

Vendor archive URL: https://pan.jczn1688.com/directlink/1/HMI%20display/JC3636K718_knob_EN.zip

The firmware uses the pin definitions and 184 panel initialization commands in `2-Program Example/Demo_arduino/ST77916_LVGL_DEMO/{pincfg.h,scr_st77916.h}`. `tools/import_knob_panel.py` imports only register data; it does not execute vendor scripts. The archive and unrelated media/tools are not copied into this repository. The supplied Arduino example uses LVGL 8; ARCANA retains LVGL 9.2.2 and uses ESP-IDF drivers directly.

## Board configuration

| Peripheral | Controller / GPIO |
| --- | --- |
| MCU | ESP32-S3R8, 8 MB OPI PSRAM, 16 MB quad flash |
| Screen | ST77916, 360 × 360, RGB565 over QSPI |
| QSPI | Clock 11, CS 12, D0–D3 13/14/15/16 |
| LCD reset / backlight | 17 / 21; PWM brightness |
| Touch | CST816S; I²C SDA 9, SCL 10, reset 8, interrupt 7 |
| Knob | Hardware-verified: clockwise pulses 1, counterclockwise pulses 2 |
| SD | CLK 39, CMD 38, D0–D3 40/41/48/47; not used by ARCANA |
| Audio | BCK 3, WS 45, data 42, mute 46; not used |
| Microphone | PDM data 4, clock 5; not used |
| Battery sense | GPIO 6 / ADC1 channel 5; calibrated VCC_OUT estimate on battery, USB-power indication when connected |
| RGB ring | GPIO 0 RGB_DATA; WS2812/GRB, uniform padded frame covering up to 64 pixels |

The knob uses independent direction pulses rather than standard quadrature decoding. There is no verified user-facing encoder push action in this port. The base BOOT switch is not a gameplay input.

## Runtime boundary

The embedded adapter implements screen refresh, bounded touch reads, corrected knob direction, PWM brightness with five-second idle dimming, on-demand timing capture, hardware randomness, persistent two-slot device-profile storage, and on-demand direct Wi-Fi hosting/scanning/joining using the existing LAN protocol. Battery sensing uses calibrated GPIO6 ADC readings and a voltage-based estimate while unplugged; the sensed rail is VCC_OUT, so USB power displays a separate symbol and charging/full status remains unavailable. The RGB ring is a steady, dim battery indicator. Audio, microphone and haptics are not initialized.

The desktop renderer remains 32-bit; hardware uses RGB565 with byte swapping before QSPI DMA. UI/game sources are shared. A 180-degree display/touch rotation is available through menuconfig. The flash clock is configured at 80 MHz; the vendor's Arduino screenshot uses QIO 120 MHz. Physical display/rotation/touch validation is required before declaring the board port proven.

On 2026-11-20, the first hardware image was flashed and verified on the user's unit. The captured boot passes the 8 MB PSRAM test and initializes both display and touch drivers without crashing. The user confirmed that the display matches the digital twin, corrected rotary direction works, unplugging USB shows battery bars, and the full-frame rendering update substantially improves responsiveness. Device-name persistence after power cycling and battery runtime still need physical validation. Matches are RAM-only; power cycling always returns to New Game. See [firmware installation](../firmware/README.md) for backup, build and flashing steps and the current bring-up status.

## Battery-status RGB ring

The vendor files `Demo_idf/demo/main/device/pinconfig.h` and `Demo_idf/demo/main/led_strip/led_strip_init.c` identify GPIO0, a 13-pixel WS2812 demo configuration and GRB ordering. The base schematic lists 12 RGB packages (LED2-LED13); physical coverage was reported incomplete, so this count is not treated as proof of the unit's full ring length. ARCANA sends a uniform padded frame for up to 64 LEDs (excess data is ignored by shorter chains) using the built-in ESP-IDF RMT TX/copy encoder at 10 MHz with DMA, with WS2812 pulse timings matching the local SDK example and a 300 us low reset/latch. No additional LED library is needed.

All addressed LEDs receive the same single-channel colour: 5/255 green (about 2%) for estimated battery >=30%, or 5/255 red (about 2%) below 30%. The internal DMA buffer holds the complete frame to avoid interrupt-refill timing gaps, and the symbol payload is heap-allocated in internal RAM rather than enlarging the UI task stack. Unknown readings and external USB power select off; the board cannot accurately infer cell charge from VCC_OUT while externally powered. Battery polling remains every five seconds. Only a colour change sends an LED frame; the RMT channel is disabled after each bounded transfer. Driver failures are logged and disable the ring without stopping gameplay. LED brightness is independent of backlight dimming. Runtime impact and current draw have not been measured.

## Power and battery follow-up, 2026-11-22

The user reports a flawless five-device multiplayer playtest on 2026-11-21, with approximately four hours of runtime. That is physical validation of the accepted wheel-v2 multiplayer firmware; it does not establish runtime for the later power candidate. Only the owner's fifth device is available for this power update. Cross-device automatic-sleep testing remains pending.

The supplied base schematic identifies an ETA6003Q3Q charger rather than an I2C fuel gauge. BAT_ADC is a divided VCC_OUT measurement. No dedicated battery charge gauge is shown. See the [battery investigation and power diagnostics](../firmware/README.md#battery-sensing-investigation-2026-11-22) for the source and limits. The power candidate adds CPU scaling, event-driven idle input, tickless automatic light sleep and less idle network traffic while preserving screen/RGB behavior. USB-host connections and access-point hosting prevent automatic sleep by design.


On 2026-11-22 the owner's power-v1 USB-guard firmware passed independent flash verification and clean boot, USB idle/reconnect diagnostics, Wi-Fi AP/scan/start/stop diagnostics, and an unplugged Single ARCANA check. The SDK's cumulative counters recorded 3,353 successful sleep intervals and 68 GPIO wakeups; the user confirmed normal wake, wheel and menu behavior. Battery-only samples were 4,126-4,136 mV (the existing approximate curve reported 92-93%). Reconnected external power activated the sleep guard. No common gauge responded at 0x36, 0x55 or 0x62. No battery-current measurement or full discharge was performed, and a second physical device was unavailable for connected-client sleep verification. Only device five received this power update; the other four retain the accepted wheel-v2 firmware.
