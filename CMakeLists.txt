cmake_minimum_required(VERSION 3.25)
project(XenogearsExMachina VERSION 0.0.0 LANGUAGES CXX)

include(CTest)
include(cmake/ProjectOptions.cmake)

configure_file(include/xem/build_info.hpp.in generated/xem/build_info.hpp @ONLY)
add_executable(xem-baseline src/runtime/main.cpp)
target_include_directories(xem-baseline PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(xem-baseline PRIVATE xem_project_options)

# Reviewed Phase 1 logic for reuse by the native runtime. The baseline does not
# yet launch the original game or supply the unresolved subsystem services.
add_library(xem-field-reconstruction STATIC
  src/reconstruction/field_events.cpp
  src/reconstruction/field_return.cpp)
target_include_directories(xem-field-reconstruction PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(xem-field-reconstruction PUBLIC xem_project_options)

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
  add_executable(test-field-events tests/test_field_events.cpp)
  target_link_libraries(test-field-events PRIVATE xem-field-reconstruction)
  add_test(NAME field-event-reconstruction COMMAND test-field-events)
  add_executable(test-field-return tests/test_field_return.cpp)
  target_link_libraries(test-field-return PRIVATE xem-field-reconstruction)
  add_test(NAME field-return-reconstruction COMMAND test-field-return)
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
