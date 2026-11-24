# v0.17.0: idle display and experimental Bluetooth

v0.17.0 is published for installation through the knob updater at the user's request. Its Bluetooth feature remains experimental: two physical ARCANA knobs were unavailable, so discovery, radio synchronization, reconnect behaviour and battery consumption still need hardware validation. Wi-Fi is retained as a selectable fallback; the earlier v0.16.3 release remains available.

## Display

Gear > Display contains Brightness, Dim After, Life Only, Wake Protection and Idle Brightness. Defaults are five seconds, Life Only OFF, Wake Protection OFF and 10% idle brightness. The backlight never increases when entering idle. Dim After accepts 0–300 seconds; 0 means Never.

Life Only replaces the own-life screen with one centred number on black, in every watch-face theme. It removes menu/counter hit targets as well as artwork. With protection OFF, a short tap changes life by one and a hold changes it by five each second. Rebuilding the normal face waits for the touch gesture to finish so a release cannot land on a different control. Wheel input wakes and applies its full delta immediately. With protection ON, the entire wake tap or initial wheel burst is consumed. The idle state is independent of manual brightness, including levels below 10%.

Multiplayer updates continue in the background and update the minimal life number without waking the display. Unchanged idle frames do not redraw. Menus and opponent pages dim normally. A black image does not turn off an IPS backlight: this feature is chiefly visual, and no runtime improvement is claimed.

Only the independent device profile is saved. Match autosaving remains removed. Existing profiles migrate without losing their identity, name, brightness or watch face. Optional invalid fields fall back to defaults; failed saves leave the old settings in memory.

## Radios

Multi ARCANA > name > Connection selects Bluetooth or Wi-Fi. Bluetooth is the experimental default; select Wi-Fi to play with the existing installed firmware. Both choices use the same game protocol, names, seat reservations, late joins, team mapping, sequence acknowledgements and retries. All knobs in a match must select the same radio. A live session cannot change radios. Wi-Fi is preserved as an explicit fallback, without an automatic mid-game switch.

The Windows twin still uses its existing IPv4 LAN sockets for either menu choice. Native framing tests exercise packet boundaries and protocol recovery; they do not emulate the ESP32 controller or prove BLE performance.

### BLE transport

- ESP-IDF 5.5 NimBLE, initialized only for hosting/scanning/joining; stopped and deinitialized when leaving. Single ARCANA leaves Bluetooth off.
- Host is a connectable GATT peripheral. Guests are central clients. Up to nine guest connections plus the host; the larger game roster can retain manual seats.
- Service UUID `c2f00001-5a6c-4b9d-8e70-415243414e41`, guest writes to `c2f00002-5a6c-4b9d-8e70-415243414e41`, host notifies on `c2f00003-5a6c-4b9d-8e70-415243414e41`.
- Service advertising carries the UUID; scan response carries `ARCANA-<name>`. Hosts continue advertising with an active game so delayed guests can join. Scan entries expire after eight seconds.
- Preferred ATT MTU 247, with fragmentation down to MTU 23 (20-byte characteristic payload). A 12-byte, versioned header carries message sequence, full length, offset and fragment length. Reassembly is isolated per connection, bounded to 8 KiB and expires after two seconds. BLE supplies link CRC/retries; incomplete application messages are rejected, and the existing game protocol retries unacknowledged edits.
- Bounded queues and a two-millisecond transmit budget per UI pass avoid synchronous transmission loops in drawing/input handling. A pending, unsent authoritative state can be replaced by the latest state; partially transmitted messages finish first. No application JSON parsing or LVGL work occurs in BLE callbacks.
- Guests become ready only after service discovery and notification subscription. Link loss triggers reconnect to the selected host address; the existing identity/sequence protocol reclaims the same seat and preserves pending edits.
- BLE controller modem sleep uses the main crystal and supports the existing dynamic CPU frequency/automatic light-sleep configuration. The Wi-Fi host's application sleep lock is not applied to BLE hosting. USB power still inhibits automatic light sleep for diagnostics. Connection timing, sleep stability and savings need battery-powered hardware measurement.
- Games are passwordless, as before. BLE advertisement names and game traffic are unencrypted and intended for a trusted nearby play group; no bonding or phone pairing is required.

Home-Wi-Fi setup and HTTPS GitHub OTA retain the original Wi-Fi object. They do not use BLE. Partition layout and OTA slot sizes are unchanged.

## Validation and the next hardware check

Native tests cover saved defaults/migration/failure handling; idle state below 10%; protected/unprotected wake gestures; minimal rendering on all twelve faces; hold repeat; continued state updates; and no redraws while unchanged. BLE framing tests cover negotiated MTUs, maximum message length, loss, malformed offsets/lengths, stale fragments, message replacement, sequence wrap and independent connections. Framed multiplayer tests cover late joins, lost-edit recovery and switching back to Wi-Fi after leaving. Existing game, LAN, storage and OTA compatibility suites remain required.

Before treating BLE as hardware-validated, use two updated knobs: host with empty seats, join after starting, change life/counters on both, exercise Undo and 2HG, interrupt/reconnect a guest, end/recreate a match, then repeat on battery through idle and wake. Also verify Wi-Fi play with a v0.16.3 peer, Wi-Fi setup/OTA after leaving BLE, repeated radio switching, and measured idle current/runtime. Serial command `z` reports BLE lifecycle/connection status; `e` reports existing power/sleep statistics. Uppercase Wi-Fi probe commands require selecting Wi-Fi first.

## Local verification, 2026-11-24

- All **12 native CTest suites passed**, including all twelve watch-face idle/wake cases, BLE framing/retry/late join, real LAN discovery and separate host/client processes.
- All **11 firmware host-tool tests passed**.
- ESP-IDF 5.5 firmware build passed (explicit PlatformIO exit code 0; the verified image matches the packaged candidate) with NimBLE, the original Wi-Fi transport and the unchanged `arcana-ota-v1` partitions. The packaged application fits the unchanged 3,604,480-byte OTA slot. Build sizes do not measure runtime heap or current consumption.
- Packaged application: **2,250,480 bytes**, embedded version **0.17.0**, SHA-256 `4b7595951b919f3946f846d939f4f0dc841459e5e3531f0f6be76f57eaddab2e`.
- Local artifacts: `build/candidates/v0.17.0/arcana-jc3636k718.bin`, matching JSON manifest and `SHA256SUMS`. The packaging tool checked the embedded version, project and OTA size limit. Native preview: `build/mtg_simulator.exe`; UI captures include `build/ui-display-settings.bmp`, `build/ui-life-only.bmp` and `build/ui-connection.bmp`.
- Physical BLE discovery/sync, repeated radio teardown/start, battery-powered sleep/wake and current/runtime remain untested. No device flashing was performed during local validation. The user subsequently authorized pushing the source and publishing v0.17.0 for the knob updater. The v0.16.3 rollback image remains archived in `build/releases/v0.16.3` and in its existing GitHub release.

## Release distribution

The version tag uses the repository's existing firmware-release workflow to build and package a fresh image, JSON manifest and SHA-256 checksums. These release-build bytes can differ from the locally built candidate above. GitHub's firmware asset digest and size are checked against the uploaded binary before publishing, and the latest-release endpoint is verified afterward. Publishing as the latest regular release is required for compatibility with the existing updater; the Bluetooth feature remains explicitly experimental.

## v0.17.1 wake protection correction

v0.17.1 is distributed through the existing knob updater. Physical-device confirmation remains pending.

A slow full-face redraw after leaving Life Only can delay delivery of wheel pulses past the old 250 ms wake-guard deadline. The old policy classified them by delivery time, so movement from the same protected gesture could change life when the screen returned. A new regression test reproduced this before the fix.

The board now keeps the first and latest physical sample times for each wheel batch, separately from its existing latency timestamp. Wake protection classifies the batch from those acquisition times and extends the protected burst from its latest physical pulse. A genuinely separate gesture after a quiet gap is still accepted without an extra delay. Net-zero wheel motion remains visible as physical activity, so a reversing gesture wakes the screen without editing life.

The digital twin now queues wheel input until the UI consumes it, matching the board instead of filtering it immediately when sampled. Tests cover protected wake taps and complete held touches, queued pulses across a simulated 400 ms rendering delay, subsequent deliberate gestures, low and normal brightness, and net-zero motion across all twelve watch faces. Wake Protection OFF still applies the first input. Defaults and saved-profile format are unchanged. Physical-device confirmation remains separate from these software tests.

Local v0.17.1 validation: all 12 native suites passed, including the regression that failed before the fix. The ESP-IDF 5.5 firmware build returned exit code 0. The verified local candidate is `build/candidates/v0.17.1/arcana-jc3636k718.bin` (2,250,672 bytes), SHA-256 `bc6de4d675d8856f074edde1f9f54603e50204b7987e571caa98aeb28f52187e`. This records local validation before publication; no device was physically flashed. Public release-build bytes may differ from this local candidate. Use the released manifest and SHA256SUMS for the downloadable image.
