# Wireless firmware updates

ARCANA uses the existing public repository, `michaelDengl/arcana-mtg-life-counter`, as its firmware release source. It downloads a compiled application image from GitHub Releases. Source-code commits alone do not create an update.

## One-time USB setup

Earlier devices have one application partition and cannot install OTA images. Install the OTA-enabled firmware once using `tools/knob_device.py` and the verified backup for that exact device. Do not use `--initialize-storage` on an existing ARCANA. The installer validates the new partition map and initializes its OTA boot-selection data.

| Partition | Offset | Size |
| --- | --- | --- |
| NVS | `0x9000` | `0x6000` |
| PHY | `0xf000` | `0x1000` |
| OTA slot 0 | `0x10000` | `0x370000` |
| OTA slot 1 | `0x380000` | `0x370000` |
| OTA boot selection | `0x6f0000` | `0x2000` |
| Existing profile storage | `0x710000` | `0x8f0000` |

The persistent profile partition stays unchanged. The two application slots fit inside the old application region; each accommodates firmware up to 3,604,480 bytes. Wireless updates replace application code only, not the bootloader, partition table or profiles.

## Connect for updates

1. At startup, open **About > Wi-Fi & Updates > Wi-Fi setup**.
2. On a phone, connect to the displayed **ARCANA-SETUP-…** network using the displayed temporary password.
3. Open **http://192.168.4.1** in the phone's browser. Enter a 2.4 GHz home Wi-Fi network or compatible phone hotspot and save.
4. ARCANA stops its setup network, connects and checks the latest stable GitHub Release. Return the phone to its normal network.

Wi-Fi settings stay on the device in NVS. ARCANA stores no GitHub token. The setup access point is password protected, closes on Back, and expires after three minutes. The device checks once at startup when credentials exist and on manual Check; New Game cancels update networking so play retains its existing radio behavior.

## Install an update

The startup **Update** button is disabled unless a newer compatible release has been found. Tap it, review the installed/latest versions, then tap **Install**. ARCANA displays progress, verifies the download and restarts. It uses the inactive slot so an interrupted download leaves the current application selected. A pending updated application must pass storage/display initialization and five seconds of main-loop operation to cancel the bootloader's rollback state.

Offline, connection failures, no release, or an incompatible release leave Update disabled. About > Wi-Fi & Updates provides the status, manual retry and Wi-Fi setup. OTA is unavailable during a live match.

## Prepare a release

Keep `src/core/AppInfo.hpp`, the top-level CMake project and `firmware/CMakeLists.txt` on the same numeric version. Build and test, then run:

```powershell
.\.tools\esp-python\Scripts\python.exe tools/package_firmware.py --tag v0.15.1
```

Packaging checks the built image's project, embedded version and size, then creates `build/release-assets/arcana-jc3636k718.bin`, a JSON description and `SHA256SUMS`. The binary filename is part of the model compatibility contract. GitHub supplies its asset digest; the device requires `sha256:` followed by 64 lowercase hex digits.

The included GitHub Actions workflow builds a version tag or an explicitly selected existing tag, uploads reviewable artifacts and prepares a **draft** release. Review the build and release before publishing. Devices ignore drafts and prereleases, and compare the latest stable release numerically against the installed version. Never put Wi-Fi passwords or GitHub credentials in the firmware/repository.

## Validation status

Native coverage checks startup button availability, About/contact navigation, release version/model/size/digest/source validation, and rejection of partition maps that move profile storage. Hardware boot, provisioning, TLS release checks and a real OTA download/reboot require separate device checks. The full end-to-end flow is not considered verified until a published newer image has installed and booted on the owner device.
