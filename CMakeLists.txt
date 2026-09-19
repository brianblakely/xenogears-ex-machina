cmake_minimum_required(VERSION 3.25)
project(XenogearsExMachina VERSION 0.0.0 LANGUAGES CXX)

include(CTest)
include(cmake/ProjectOptions.cmake)

configure_file(include/xem/build_info.hpp.in generated/xem/build_info.hpp @ONLY)
add_executable(xem-baseline src/runtime/main.cpp)
target_include_directories(xem-baseline PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(xem-baseline PRIVATE xem_project_options)

# One implementation of recovered game behavior, shared by analysis, tests and
# the later native runtime. xem-baseline remains the Phase 0 build artifact.
add_library(xem-field-reconstruction STATIC
  src/reconstruction/field_events.cpp
  src/reconstruction/field_return.cpp
  src/reconstruction/packed_field.cpp
  src/reconstruction/field_actor.cpp
  src/reconstruction/field_battle.cpp
  src/reconstruction/field_control.cpp
  src/reconstruction/field_sprite.cpp
  src/reconstruction/field_sprite_factory.cpp
  src/reconstruction/field_motion.cpp
  src/reconstruction/field_collision.cpp
  src/reconstruction/field_media.cpp
  src/reconstruction/disc_stream.cpp
  src/reconstruction/connected_field.cpp)
target_include_directories(xem-field-reconstruction PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(xem-field-reconstruction PUBLIC xem_project_options)

# Analysis executable, not the shipping runtime or the native agent API.
add_executable(xem-reconstruct src/analysis/reconstruct.cpp)
target_link_libraries(xem-reconstruct PRIVATE xem-field-reconstruction)

find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
add_custom_target(check-repository
  COMMAND "${Python3_EXECUTABLE}" tools/repository/validate.py
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  VERBATIM)
add_custom_target(check-format
  COMMAND "${Python3_EXECUTABLE}" tools/repository/format.py --check
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  VERBATIM)

if(BUILD_TESTING)
  add_executable(test-connected-field tests/test_connected_field.cpp)
  target_link_libraries(test-connected-field PRIVATE xem-field-reconstruction)
  add_test(NAME connected-field-ownership COMMAND test-connected-field)
  add_test(NAME connected-reconstruction
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/test_connected.py"
      "$<TARGET_FILE:xem-reconstruct>")
  set_tests_properties(connected-field-ownership connected-reconstruction PROPERTIES LABELS connected)
  add_custom_target(check-connected
    COMMAND "$<TARGET_FILE:test-connected-field>"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/test_connected.py"
      "$<TARGET_FILE:xem-reconstruct>"
    DEPENDS xem-reconstruct test-connected-field
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    USES_TERMINAL
    VERBATIM)

  add_executable(test-field-events tests/test_field_events.cpp)
  target_link_libraries(test-field-events PRIVATE xem-field-reconstruction)
  add_test(NAME field-event-reconstruction COMMAND test-field-events)
  add_executable(test-field-return tests/test_field_return.cpp)
  target_link_libraries(test-field-return PRIVATE xem-field-reconstruction)
  add_test(NAME field-return-reconstruction COMMAND test-field-return)
  add_executable(test-field-actor tests/test_field_actor.cpp)
  target_link_libraries(test-field-actor PRIVATE xem-field-reconstruction)
  add_test(NAME field-actor-reconstruction COMMAND test-field-actor)
  add_executable(test-field-battle tests/test_field_battle.cpp)
  target_link_libraries(test-field-battle PRIVATE xem-field-reconstruction)
  add_test(NAME field-battle-reconstruction COMMAND test-field-battle)
  add_executable(test-field-control tests/test_field_control.cpp)
  target_link_libraries(test-field-control PRIVATE xem-field-reconstruction)
  add_test(NAME field-control-reconstruction COMMAND test-field-control)
  add_executable(test-field-sprite tests/test_field_sprite.cpp)
  target_link_libraries(test-field-sprite PRIVATE xem-field-reconstruction)
  add_test(NAME field-sprite-reconstruction COMMAND test-field-sprite)
  add_executable(test-field-sprite-factory tests/test_field_sprite_factory.cpp)
  target_link_libraries(test-field-sprite-factory PRIVATE xem-field-reconstruction)
  add_test(NAME field-sprite-factory-reconstruction COMMAND test-field-sprite-factory)
  add_executable(test-field-motion tests/test_field_motion.cpp)
  target_link_libraries(test-field-motion PRIVATE xem-field-reconstruction)
  add_test(NAME field-motion-reconstruction COMMAND test-field-motion)
  add_executable(test-field-collision tests/test_field_collision.cpp)
  target_link_libraries(test-field-collision PRIVATE xem-field-reconstruction)
  add_test(NAME field-collision-reconstruction COMMAND test-field-collision)
  add_executable(test-field-media tests/test_field_media.cpp)
  target_link_libraries(test-field-media PRIVATE xem-field-reconstruction)
  add_test(NAME field-media-reconstruction COMMAND test-field-media)
  add_executable(test-disc-stream tests/test_disc_stream.cpp)
  target_link_libraries(test-disc-stream PRIVATE xem-field-reconstruction)
  add_test(NAME disc-stream-reconstruction COMMAND test-disc-stream)
  add_executable(test-packed-field tests/test_packed_field.cpp)
  target_link_libraries(test-packed-field PRIVATE xem-field-reconstruction)
  add_test(NAME packed-field-reconstruction COMMAND test-packed-field)
  add_executable(packed-field-probe tests/packed_field_probe.cpp)
  target_link_libraries(packed-field-probe PRIVATE xem-field-reconstruction)
  add_test(NAME baseline-cli
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/test_baseline.py"
      "$<TARGET_FILE:xem-baseline>")
  add_test(NAME repository-contracts
    COMMAND "${Python3_EXECUTABLE}" -m unittest discover -s tests -p "test_*.py")
  set_tests_properties(repository-contracts PROPERTIES
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
  add_test(NAME repository-validation
    COMMAND "${Python3_EXECUTABLE}" tools/repository/validate.py)
  set_tests_properties(repository-validation PROPERTIES
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
endif()

install(TARGETS xem-baseline RUNTIME DESTINATION bin)
install(FILES LICENSE DESTINATION share/licenses/xenogears-ex-machina)
