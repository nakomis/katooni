# Katooni firmware for the ESP32-S3.
#
#   . ~/esp/v5.5/esp-idf/export.sh
#   cd embedded && idf.py build
#
# Wi-Fi credentials come from sdkconfig.secrets, which isn't committed: copy
# sdkconfig.secrets.example and fill it in. Without it the firmware builds and runs its own access
# point instead.
cmake_minimum_required(VERSION 3.16)

set(IDF_TARGET esp32s3)
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/sdkconfig.secrets)
  set(SDKCONFIG_DEFAULTS "sdkconfig.defaults;sdkconfig.secrets")
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(katooni)
