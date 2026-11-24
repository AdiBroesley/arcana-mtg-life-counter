#include "ArcanaApp.hpp"

// Standalone ARCANA. A launcher firmware provides its own app_main and calls
// bootPlatform()/runArcana() from firmware/main/ArcanaApp.hpp instead.
extern "C" void app_main() {
 static mtg::DevicePlatform platform;
 mtg::bootPlatform(platform);
 mtg::runArcana(platform);
}
