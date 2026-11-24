# Shared by ARCANA's own firmware/main component and by launcher firmwares that
# embed ARCANA (for example as a git submodule). Include it from a component's
# CMakeLists.txt, then:
#   idf_component_register(SRCS <own sources> ${ARCANA_FIRMWARE_SOURCES}
#     INCLUDE_DIRS <own dirs> ${ARCANA_INCLUDE_DIRS} REQUIRES ${ARCANA_REQUIRES})
#   arcana_configure_component()
# ARCANA_FIRMWARE_SOURCES excludes main.cpp (ARCANA's standalone app_main).
get_filename_component(ARCANA_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(ARCANA_FIRMWARE_DIR "${ARCANA_ROOT}/firmware/main")
set(ARCANA_FIRMWARE_SOURCES
  "${ARCANA_FIRMWARE_DIR}/ArcanaApp.cpp"
  "${ARCANA_FIRMWARE_DIR}/Board.cpp"
  "${ARCANA_FIRMWARE_DIR}/FlashStorage.cpp"
  "${ARCANA_FIRMWARE_DIR}/Wifi.cpp"
  "${ARCANA_FIRMWARE_DIR}/Bluetooth.cpp"
  "${ARCANA_FIRMWARE_DIR}/FirmwareUpdate.cpp"
  "${ARCANA_ROOT}/src/core/GameState.cpp"
  "${ARCANA_ROOT}/src/core/Serialization.cpp"
  "${ARCANA_ROOT}/src/core/RandomTools.cpp"
  "${ARCANA_ROOT}/src/core/DeviceProfile.cpp"
  "${ARCANA_ROOT}/src/core/Multiplayer.cpp"
  "${ARCANA_ROOT}/src/ui/AppUI.cpp"
  "${ARCANA_ROOT}/src/ui/AppUIDial.cpp"
  "${ARCANA_ROOT}/src/ui/FaceDesigns.cpp"
  "${ARCANA_ROOT}/src/ui/FaceLayouts.cpp"
  "${ARCANA_ROOT}/src/ui/AppUITools.cpp"
  "${ARCANA_ROOT}/src/ui/AppUIInformation.cpp"
  "${ARCANA_ROOT}/src/ui/AppUIMultiplayer.cpp"
  "${ARCANA_ROOT}/src/ui/Widgets/ToolIcons.cpp"
  "${ARCANA_ROOT}/src/assets/life_digits.c"
  "${ARCANA_ROOT}/src/assets/mtg_symbols.c"
  "${ARCANA_ROOT}/src/assets/mana_pips.c"
  "${ARCANA_ROOT}/src/assets/mana_life_digits.c"
  "${ARCANA_ROOT}/src/assets/heart_symbol.c"
  "${ARCANA_ROOT}/src/assets/tab_font.c"
  "${ARCANA_ROOT}/simulator/LanTransport.cpp"
)
set(ARCANA_INCLUDE_DIRS "${ARCANA_FIRMWARE_DIR}" "${ARCANA_ROOT}/src" "${ARCANA_ROOT}/simulator")
set(ARCANA_REQUIRES driver esp_adc esp_lcd esp_timer esp_wifi esp_netif nvs_flash spiffs esp_psram esp_pm esp_http_client esp_http_server app_update mbedtls bt)

function(arcana_configure_component)
 target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
 target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=missing-field-initializers -Wno-error=misleading-indentation)
 # Give the LVGL component the embedded configuration, independently of desktop LVGL.
 idf_component_get_property(LVGL_LIB lvgl__lvgl COMPONENT_LIB)
 # Component Manager can materialize cached directories as Windows junctions.
 # LVGL 9.2.2's own glob does not follow them and can silently omit core sources.
 file(GLOB_RECURSE LVGL_COMPLETE_SOURCES FOLLOW_SYMLINKS
  "${CMAKE_SOURCE_DIR}/managed_components/lvgl__lvgl/src/*.c"
  "${CMAKE_SOURCE_DIR}/managed_components/lvgl__lvgl/src/*.cpp")
 target_sources(${LVGL_LIB} PRIVATE ${LVGL_COMPLETE_SOURCES})
 target_include_directories(${LVGL_LIB} PUBLIC "${ARCANA_FIRMWARE_DIR}")
 target_compile_definitions(${LVGL_LIB} PUBLIC LV_CONF_INCLUDE_SIMPLE LV_KCONFIG_IGNORE)
endfunction()
