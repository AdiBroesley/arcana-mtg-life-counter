include("${CMAKE_CURRENT_LIST_DIR}/../arcana_component.cmake")
idf_component_register(
 SRCS main.cpp ${ARCANA_FIRMWARE_SOURCES}
 INCLUDE_DIRS ${ARCANA_INCLUDE_DIRS}
 REQUIRES ${ARCANA_REQUIRES})
arcana_configure_component()
