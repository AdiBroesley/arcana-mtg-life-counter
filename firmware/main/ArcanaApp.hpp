#pragma once
#include "Board.hpp"
#include "Profiler.hpp"
#include "RuntimePower.hpp"
#include "platform/IRandom.hpp"
#include "ui/HomeFace.hpp"
#include "esp_err.h"
#include <functional>

namespace mtg {
// Hardware and services that live for the whole boot. ARCANA's own app_main
// boots them and runs ARCANA. A launcher firmware that embeds ARCANA (for
// example as a git submodule) boots them once, shows its own menu, then hands
// the same platform to the app the user picks.
class HardwareRandom final : public IRandom {
public:
 unsigned uniform(unsigned lower,unsigned upper) override;
};
struct DevicePlatform {
 esp_err_t storage=ESP_FAIL; // SPIFFS mount result for /arcana.
 RuntimePower power;
 Profiler profiler;
 Board board;
 HardwareRandom random;
};
// Must run on the task that will run the app loop (Board records it for wake-ups).
void bootPlatform(DevicePlatform& platform);

struct ArcanaOptions {
 // When set, ARCANA's start screen offers "Apps", which calls this. A launcher
 // typically records its choice and restarts, so this need not return.
 std::function<void()> exitToLauncher;
 // Optional replacement for the single-player main face (see ui/HomeFace.hpp).
 IHomeFace* homeFace=nullptr;
 // Autosave file; give each app its own so they never resume each other's match.
 const char* matchFile="/arcana/match.json";
 // Start-screen title and subtitle; nullptr keeps ARCANA's.
 const char* title=nullptr;const char* subtitle=nullptr;
};
// Runs ARCANA on an already booted platform. Never returns.
[[noreturn]] void runArcana(DevicePlatform& platform,const ArcanaOptions& options={});
}
