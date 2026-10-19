# Katooni's portable core, built and tested on the Mac (the firmware builds the same sources as an
# ESP-IDF component).
#
#   cmake -S embedded/core -B build/core && cmake --build build/core && ctest --test-dir build/core
#   embedded/core/coverage.sh        # the same, with a line-coverage report
cmake_minimum_required(VERSION 3.16)
project(katooni_core C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
option(KATOONI_COVERAGE "Build with clang source-based coverage" OFF)

add_compile_options(-Wall -Wextra -Werror -Wshadow -Wstrict-prototypes)
if(KATOONI_COVERAGE)
  add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
  add_link_options(-fprofile-instr-generate)
endif()

add_library(katooni_core STATIC
  src/lines.c
  src/scan.c
  src/classify.c
  src/emarker.c
  src/resistance.c
  src/cable.c
  src/session.c
  src/simcable.c
)
target_include_directories(katooni_core PUBLIC include)

enable_testing()
foreach(t lines classify emarker resistance session)
  add_executable(test_${t} tests/test_${t}.c)
  target_link_libraries(test_${t} katooni_core)
  add_test(NAME ${t} COMMAND test_${t})
  if(KATOONI_COVERAGE)
    set_tests_properties(${t} PROPERTIES ENVIRONMENT "LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/prof/${t}.profraw")
  endif()
endforeach()
