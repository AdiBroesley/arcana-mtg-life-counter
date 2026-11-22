# Architecture

`src/core` contains the game state, format rules and local-network protocol without SDL, LVGL or Windows dependencies. `GameSnapshot` holds the local player, opponents' life, team assignments, independent commander sources, cast counts, tracker choices and status markers. Only Commander and Commander 2HG use the commander-damage loss rule; other presets expose opponent life in the player tiles.

Every accepted mutation records an atomic before/after snapshot and a description. Undo/Redo uses a cursor, discards the redo branch after a new edit and retains at most 64 actions. Recent edits retain their own timestamped before-states for the rolling five-second main-face Undo. The quick group may include more than 64 rapid edits and becomes one redoable transaction. Overflow and invalid input leave the live state unchanged. Linked commander damage and its corresponding life correction remain one transaction.

## Session lifetime and identity

Matches live only in RAM. Firmware and simulator startup always show New Game; neither runtime loads, copies, serializes or writes match saves. End Game clears the active match/history; restart or process exit discards the session. Old game files remain untouched and ignored. There is no Continue action or background match-save worker.

`Serialization.cpp` remains a native test utility for validating in-memory state round trips and invariants. The embedded build excludes it. The former SessionPersistence service has been removed.

`DeviceProfile` independently persists the multiplayer name and stable identity through `IStorage`. Desktop `FileStorage` atomically replaces a sibling temporary file. Firmware `FlashStorage` alternates checksummed generation slots, preserving the last committed profile after an interrupted write. Explicit name confirmation is the only normal profile write. Match history never contains the device profile.

## Interface and rendering

`src/ui` renders the shared 360 x 360 circular LVGL interface. Stable event bindings request navigation; the next UI update rebuilds the view after event dispatch. Dirty-state and network revision checks refresh changed values, including battery bands and Undo expiry, without continuously invalidating an idle screen.

Transitions use a 180 ms ease-out fade at most every 33 ms. One background-opacity cover fades away above the new content, leaving the permanent frame rings visible. Widget opacity stays fully opaque, avoiding temporary alpha layers for each large annular control. The disabled main-face Undo target also stays opaque; only its glyphs dim. Curved Undo letters are pooled, updated in place and hidden when surplus, rather than destroyed and recreated for each edit.

`IHardware` supplies rotation, touch, brightness, battery telemetry and an optional profiling sink. The simulator transports LVGL's host framebuffer through SDL; the Guition firmware uses a full-frame RGB565 PSRAM drawing buffer and a small internal DMA staging buffer for QSPI transfers. A separate CPU1 task samples bounded I2C touch reads and queues press/release events while CPU0 runs the UI. Physical pin mappings and power limits are documented in [hardware.md](hardware.md).

Five seconds without input dim the real backlight to 10%, respecting lower selected levels. The first touch gesture or initial wheel burst restores brightness without editing game state. Dimming leaves the game and network active.

## Measurements and verification

On-demand 30-second captures aggregate fixed-size timing samples and print once at completion. Besides frame, LCD and input-queue timing, `history_record` measures history recording, `undo_apply` measures Undo/Redo in the model, and `undo_label` measures curved label layout. Clock reads for these probes occur only during a requested capture. Label timing excludes later rasterization; frame time outside LCD transfer also includes LVGL input callbacks/scheduling.

UI tests use actual LVGL pointer dispatch and deterministic ticks for touch targets, tap/hold, cancellation, pooled glyphs and fade behavior. Separate simulator processes verify fresh match startup while retaining an isolated device profile. Snapshot, profile fallback, idle backlight and network suites use isolated data.

## Local multiplayer

`Multiplayer` owns discovery, live roster and host/client state. `IPeerTransport` supplies bounded datagrams and optional radio lifecycle/discovery hooks. Desktop and firmware share nonblocking LAN transport with ephemeral gameplay endpoints. Desktop instances discover over their existing LAN. The firmware starts an open access point for Host, scans ARCANA SSIDs for Join, then connects and resolves the current game ID through the host's DHCP gateway. Its multicast sockets explicitly select the active AP/station interface. Scans and connection events never block the UI waiting for a network.

Hosts assign seats by persistent device identity and may start before every seat has a device. Unclaimed seats remain visible and manually editable by the host. A late arrival inherits the selected slot's current values. Sequenced commands, acknowledgements, deduplication, coalescing and full host snapshots reconcile clients; 2HG life and poison are shared within each team. Remote team corrections do not create a local quick-Undo group.

Live lobby state is ephemeral. A device can reconnect to a still-running host using its saved identity. After a radio interruption it repeats admission to update its UDP endpoint while retaining queued edits and its existing seat. Back/leave stops scanning or disconnects; Single ARCANA keeps the radio off. Host migration is not implemented. Direct ARCANA networking needs neither a router nor credentials. The separate update service supports home Wi-Fi provisioning only for internet firmware checks/downloads.


## Firmware delivery

`IFirmwareUpdate` exposes immutable status snapshots and check/install/setup/cancel actions to the shared LVGL screens. The ESP32 adapter owns short-lived internet/setup radio roles, a password-protected provisioning access point and an HTTP form for entering Wi-Fi credentials. Settings are stored separately in NVS; normal gameplay profiles stay in SPIFFS. A worker handles network time, certificate-verified HTTPS and streaming firmware writes; the UI remains on CPU0. Checks stop on New Game, setup expires after three minutes, and successful/failed checks power the radio down. No GitHub access token is stored on the device.

Only stable releases with a numeric version, the exact JC3636K718 asset name, an expected GitHub download URL, bounded size and a SHA-256 digest are eligible. Installation writes the inactive OTA slot and verifies size, digest, project name, embedded version and the ESP image before changing boot selection. The bootloader has rollback enabled; startup confirms a pending image after storage and display/UI initialization. The partition map preserves NVS, PHY and SPIFFS addresses. See `firmware/OTA.md` for migration and release operation.
