#pragma once
// Build-time identity. A launcher firmware that embeds ARCANA defines the
// ARCANA_FIRMWARE_* macros so its Update menu installs the combined image from
// the launcher's own releases instead of replacing itself with ARCANA-only builds.
#ifndef ARCANA_FIRMWARE_VERSION
#define ARCANA_FIRMWARE_VERSION "0.17.1"
#endif
#ifndef ARCANA_FIRMWARE_REPOSITORY
#define ARCANA_FIRMWARE_REPOSITORY "michaelDengl/arcana-mtg-life-counter"
#endif
#ifndef ARCANA_FIRMWARE_ASSET
#define ARCANA_FIRMWARE_ASSET "arcana-jc3636k718.bin"
#endif
namespace mtg::appInfo {
inline constexpr const char* version=ARCANA_FIRMWARE_VERSION;
inline constexpr const char* repository=ARCANA_FIRMWARE_REPOSITORY;
inline constexpr const char* repositoryUrl="https://github.com/" ARCANA_FIRMWARE_REPOSITORY;
inline constexpr const char* creator="Michael Dengl";
inline constexpr const char* contact="michaeldengl@hotmail.com";
inline constexpr const char* group="MagicTG Kreuzlingen";
inline constexpr const char* whatsapp="https://chat.whatsapp.com/K5KtqoQl1mbJoa2dQBnBVV?s=cl&p=a&iam=2";
inline constexpr const char* board="jc3636k718";
inline constexpr const char* firmwareAsset=ARCANA_FIRMWARE_ASSET;
}
