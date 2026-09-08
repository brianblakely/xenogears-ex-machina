cmake_minimum_required(VERSION 3.25)
project(XenogearsExMachina VERSION 0.0.0 LANGUAGES CXX)

include(CTest)
include(cmake/ProjectOptions.cmake)

configure_file(include/xem/build_info.hpp.in generated/xem/build_info.hpp @ONLY)
add_executable(xem-baseline src/runtime/main.cpp)
target_include_directories(xem-baseline PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(xem-baseline PRIVATE xem_project_options)

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
