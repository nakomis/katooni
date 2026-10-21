# Katooni firmware for the ESP32-S3.
#
#   embedded/build.sh                 the PCB pin profile
#   embedded/build.sh --breadboard    the breadboard profile (sdkconfig.breadboard), in build-breadboard/
#
# Wi-Fi credentials come from sdkconfig.secrets, which isn't committed: copy
# sdkconfig.secrets.example and fill it in. Without it the firmware runs its own access point.
cmake_minimum_required(VERSION 3.16)

set(IDF_TARGET esp32s3)
if(NOT SDKCONFIG_DEFAULTS)
  set(SDKCONFIG_DEFAULTS "sdkconfig.defaults")
endif()
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/sdkconfig.secrets)
  list(APPEND SDKCONFIG_DEFAULTS "sdkconfig.secrets")
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(katooni)
