cmake_minimum_required(VERSION 3.16)
project(avro-ime VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Fcitx5 only looks for addons under its own prefix (normally /usr). Installing to
# the default /usr/local would silently put the addon where Fcitx5 never looks.
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "Install prefix" FORCE)
endif()

option(BUILD_FCITX5 "Build the Fcitx5 input-method addon" ON)

# ---- core engine (no dependencies) ------------------------------------
add_library(avro_core STATIC core/avro_core.cpp)
target_include_directories(avro_core PUBLIC core)
set_target_properties(avro_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_options(avro_core PRIVATE -Wall -Wextra)

add_executable(avro-convert tools/avro_convert.cpp)
target_link_libraries(avro-convert PRIVATE avro_core)

enable_testing()
add_executable(test_core tests/test_core.cpp)
target_link_libraries(test_core PRIVATE avro_core)
add_test(NAME core COMMAND test_core)

# ---- Fcitx5 frontend --------------------------------------------------
if(BUILD_FCITX5)
  find_package(Fcitx5Core REQUIRED)
  find_package(Fcitx5Utils REQUIRED)
  find_package(Fcitx5Config REQUIRED)

  add_library(avroime MODULE fcitx5/avroime.cpp)
  target_link_libraries(avroime PRIVATE avro_core Fcitx5::Core Fcitx5::Utils Fcitx5::Config)
  target_compile_options(avroime PRIVATE -Wall -Wextra)
  # Fcitx5 loads  <addon dir>/libavroime.so  (keep the default "lib" prefix).
  install(TARGETS avroime DESTINATION "${FCITX_INSTALL_ADDONDIR}")
  install(FILES fcitx5/data/avroime-addon.conf
          DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon" RENAME avroime.conf)
  install(FILES fcitx5/data/avroime-im.conf
          DESTINATION "${FCITX_INSTALL_PKGDATADIR}/inputmethod" RENAME avroime.conf)
endif()
