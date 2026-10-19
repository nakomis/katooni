# The portable core (embedded/core), built as an ESP-IDF component from the same sources the Mac
# tests use.
set(core ${COMPONENT_DIR}/../../core)
idf_component_register(
  SRCS ${core}/src/lines.c ${core}/src/scan.c ${core}/src/classify.c ${core}/src/emarker.c
       ${core}/src/resistance.c ${core}/src/cable.c ${core}/src/session.c ${core}/src/simcable.c
  INCLUDE_DIRS ${core}/include
)
