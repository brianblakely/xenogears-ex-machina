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
add_library(xem-reconstruction STATIC
  src/reconstruction/program.cpp
  src/reconstruction/field_events.cpp
  src/reconstruction/field_return.cpp
  src/reconstruction/packed_field.cpp
  src/reconstruction/field_actor.cpp
  src/reconstruction/field_battle.cpp
  src/reconstruction/field_control.cpp
  src/reconstruction/field_sprite.cpp
  src/reconstruction/field_sprite_factory.cpp
  src/reconstruction/field_sprite_model.cpp
  src/reconstruction/field_motion.cpp
  src/reconstruction/field_collision.cpp
  src/reconstruction/field_media.cpp
  src/reconstruction/field_script.cpp
  src/reconstruction/field_disassembler.cpp
  src/reconstruction/field_update.cpp
  src/reconstruction/field_move.cpp
  src/reconstruction/field_dialogue.cpp
  src/reconstruction/field_transition.cpp
  src/reconstruction/original_layout.cpp
  src/reconstruction/battle.cpp
  src/reconstruction/battle_ai.cpp
  src/reconstruction/battle_attack.cpp
  src/reconstruction/battle_menu.cpp
  src/reconstruction/battle_turn.cpp
  src/reconstruction/battle_formula.cpp
  src/reconstruction/battle_levels.cpp
  src/reconstruction/battle_results.cpp
  src/reconstruction/battle_rewards.cpp
  src/reconstruction/battle_rules.cpp
  src/reconstruction/battle_steps.cpp
  src/reconstruction/battle_turns.cpp
  src/reconstruction/menu_equipment.cpp
  src/reconstruction/menu_items.cpp
  src/reconstruction/menu_save.cpp
  src/reconstruction/resident_heap.cpp
  src/reconstruction/sound_driver.cpp
  src/reconstruction/media.cpp
  src/reconstruction/field_movie.cpp
  src/reconstruction/field_gte.cpp
  src/reconstruction/gte.cpp
  src/reconstruction/gpu.cpp
  src/reconstruction/field_frame.cpp
  src/reconstruction/field_models.cpp
  src/reconstruction/field_characters.cpp
  src/reconstruction/field_dialogue_draw.cpp
  src/reconstruction/field_view.cpp
  src/reconstruction/disc_stream.cpp
  src/reconstruction/disc_read.cpp
  src/reconstruction/interrupts.cpp
  src/reconstruction/gpu_queue.cpp
  src/reconstruction/sound_tick.cpp)
target_include_directories(xem-reconstruction PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(xem-reconstruction PUBLIC xem_project_options)

add_executable(xem-analysis-runner src/analysis/main.cpp)
target_link_libraries(xem-analysis-runner PRIVATE xem-reconstruction)
add_executable(xem-event-disassembler src/analysis/event_disassembler.cpp)
target_link_libraries(xem-event-disassembler PRIVATE xem-reconstruction)
add_executable(xem-memory-runner src/analysis/memory_runner.cpp src/analysis/field_memory.cpp)
target_link_libraries(xem-memory-runner PRIVATE xem-reconstruction)

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
  add_executable(test-program tests/test_program.cpp)
  target_link_libraries(test-program PRIVATE xem-reconstruction)
  add_test(NAME connected-program COMMAND test-program)
  add_test(NAME analysis-execution
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/test_execution.py"
      "$<TARGET_FILE:xem-analysis-runner>")
  add_executable(test-field-events tests/test_field_events.cpp)
  target_link_libraries(test-field-events PRIVATE xem-reconstruction)
  add_test(NAME field-event-reconstruction COMMAND test-field-events)
  add_executable(test-field-return tests/test_field_return.cpp)
  target_link_libraries(test-field-return PRIVATE xem-reconstruction)
  add_test(NAME field-return-reconstruction COMMAND test-field-return)
  add_executable(test-field-actor tests/test_field_actor.cpp)
  target_link_libraries(test-field-actor PRIVATE xem-reconstruction)
  add_test(NAME field-actor-reconstruction COMMAND test-field-actor)
  add_executable(test-field-battle tests/test_field_battle.cpp)
  target_link_libraries(test-field-battle PRIVATE xem-reconstruction)
  add_test(NAME field-battle-reconstruction COMMAND test-field-battle)
  add_executable(test-field-control tests/test_field_control.cpp)
  target_link_libraries(test-field-control PRIVATE xem-reconstruction)
  add_test(NAME field-control-reconstruction COMMAND test-field-control)
  add_executable(test-field-sprite tests/test_field_sprite.cpp)
  target_link_libraries(test-field-sprite PRIVATE xem-reconstruction)
  add_test(NAME field-sprite-reconstruction COMMAND test-field-sprite)
  add_executable(test-field-sprite-factory tests/test_field_sprite_factory.cpp)
  target_link_libraries(test-field-sprite-factory PRIVATE xem-reconstruction)
  add_test(NAME field-sprite-factory-reconstruction COMMAND test-field-sprite-factory)
  add_executable(test-sprite-tasks tests/test_sprite_tasks.cpp)
  target_link_libraries(test-sprite-tasks PRIVATE xem-reconstruction)
  add_test(NAME sprite-task-reconstruction COMMAND test-sprite-tasks)
  add_executable(test-sprite-upload tests/test_sprite_upload.cpp)
  target_link_libraries(test-sprite-upload PRIVATE xem-reconstruction)
  add_test(NAME sprite-upload-reconstruction COMMAND test-sprite-upload)
  add_executable(test-sprite-model tests/test_sprite_model.cpp)
  target_link_libraries(test-sprite-model PRIVATE xem-reconstruction)
  add_test(NAME sprite-model-reconstruction COMMAND test-sprite-model)
  add_executable(test-field-motion tests/test_field_motion.cpp)
  target_link_libraries(test-field-motion PRIVATE xem-reconstruction)
  add_test(NAME field-motion-reconstruction COMMAND test-field-motion)
  add_executable(test-field-collision tests/test_field_collision.cpp)
  target_link_libraries(test-field-collision PRIVATE xem-reconstruction)
  add_test(NAME field-collision-reconstruction COMMAND test-field-collision)
  add_executable(test-field-gte tests/test_field_gte.cpp)
  target_link_libraries(test-field-gte PRIVATE xem-reconstruction)
  add_test(NAME field-gte-reconstruction COMMAND test-field-gte)
  add_executable(test-resident-heap tests/test_resident_heap.cpp)
  target_link_libraries(test-resident-heap PRIVATE xem-reconstruction)
  add_test(NAME resident-heap-reconstruction COMMAND test-resident-heap)
  add_executable(test-disc-read tests/test_disc_read.cpp)
  target_link_libraries(test-disc-read PRIVATE xem-reconstruction)
  add_test(NAME disc-read-reconstruction COMMAND test-disc-read)
  add_executable(test-interrupts tests/test_interrupts.cpp)
  target_link_libraries(test-interrupts PRIVATE xem-reconstruction)
  add_test(NAME interrupt-reconstruction COMMAND test-interrupts)
  add_executable(test-battle tests/test_battle.cpp)
  target_link_libraries(test-battle PRIVATE xem-reconstruction)
  add_test(NAME battle-reconstruction COMMAND test-battle)
  add_executable(test-menu tests/test_menu.cpp)
  target_link_libraries(test-menu PRIVATE xem-reconstruction)
  add_test(NAME menu-reconstruction COMMAND test-menu)
  add_executable(test-menu-save tests/test_menu_save.cpp)
  target_link_libraries(test-menu-save PRIVATE xem-reconstruction)
  add_test(NAME menu-save-reconstruction COMMAND test-menu-save)
  add_executable(test-sound-driver tests/test_sound_driver.cpp)
  target_link_libraries(test-sound-driver PRIVATE xem-reconstruction)
  add_test(NAME sound-driver-reconstruction COMMAND test-sound-driver)
  add_executable(test-sound-output tests/test_sound_output.cpp)
  target_link_libraries(test-sound-output PRIVATE xem-reconstruction)
  add_test(NAME sound-output-reconstruction COMMAND test-sound-output)
  add_executable(test-field-movie tests/test_field_movie.cpp)
  target_link_libraries(test-field-movie PRIVATE xem-reconstruction)
  add_test(NAME field-movie-reconstruction COMMAND test-field-movie)
  add_executable(test-field-script tests/test_field_script.cpp)
  target_link_libraries(test-field-script PRIVATE xem-reconstruction)
  add_test(NAME field-script-reconstruction COMMAND test-field-script)
  add_executable(test-field-disassembler tests/test_field_disassembler.cpp)
  target_link_libraries(test-field-disassembler PRIVATE xem-reconstruction)
  add_test(NAME field-disassembler-reconstruction COMMAND test-field-disassembler)
  add_executable(test-field-media tests/test_field_media.cpp)
  target_link_libraries(test-field-media PRIVATE xem-reconstruction)
  add_test(NAME field-media-reconstruction COMMAND test-field-media)
  add_executable(test-disc-stream tests/test_disc_stream.cpp)
  target_link_libraries(test-disc-stream PRIVATE xem-reconstruction)
  add_test(NAME disc-stream-reconstruction COMMAND test-disc-stream)
  add_executable(test-field-frame tests/test_field_frame.cpp)
  target_link_libraries(test-field-frame PRIVATE xem-reconstruction)
  add_test(NAME field-frame-reconstruction COMMAND test-field-frame)
  add_executable(test-packed-field tests/test_packed_field.cpp)
  target_link_libraries(test-packed-field PRIVATE xem-reconstruction)
  add_test(NAME packed-field-reconstruction COMMAND test-packed-field)
  add_executable(packed-field-probe tests/packed_field_probe.cpp)
  target_link_libraries(packed-field-probe PRIVATE xem-reconstruction)
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
