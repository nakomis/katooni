# The Mac simulator (KATOO-7): the core, the web page and its API, with canned cables.
#
#   cmake -S embedded/sim -B build/sim && cmake --build build/sim && ctest --test-dir build/sim
#   build/sim/katooni-sim --demo        then open http://127.0.0.1:8080/
#
# Includes the core and its tests, so ctest here runs both.
cmake_minimum_required(VERSION 3.16)
project(katooni_sim C)

enable_testing()
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../core ${CMAKE_BINARY_DIR}/core)

add_executable(katooni-sim sim.c)
target_link_libraries(katooni-sim katooni_core)
target_compile_options(katooni-sim PRIVATE -Wall -Wextra -Werror -Wshadow)
target_compile_definitions(katooni-sim PRIVATE
  KATOONI_PAGE="${CMAKE_CURRENT_LIST_DIR}/../main/web/index.html")

find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME sim COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/test_sim.py $<TARGET_FILE:katooni-sim>)
