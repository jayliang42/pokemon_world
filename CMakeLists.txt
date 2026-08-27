cmake_minimum_required(VERSION 3.10)
include("${CMAKE_SOURCE_DIR}/FindGfxLibs.cmake")

# Name of the project
project(final)

include(CTest)

# Enforce use of C++14
set(CMAKE_CXX_STANDARD_REQUIRED 14)
set(CMAKE_CXX_STANDARD 14)

# Use glob to get the list of all source files.
file(GLOB_RECURSE SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp" "${CMAKE_SOURCE_DIR}/ext/*/*.cpp" "${CMAKE_SOURCE_DIR}/ext/glad/src/*.c")

if(EMSCRIPTEN)
  # GLAD loads desktop OpenGL symbols through dlopen/dlsym, which is not part
  # of the browser build. WebGL exposes GLES3 directly instead.
  list(FILTER SOURCES EXCLUDE REGEX ".*/ext/glad/src/glad\\.c$")
endif()

if(BUILD_TESTING AND NOT EMSCRIPTEN)
  add_executable(alpha_nest_tests
      tests/alpha_nest_test.cpp
      src/AlphaNest.cpp)
  target_include_directories(alpha_nest_tests PRIVATE
      "${CMAKE_SOURCE_DIR}/src")
  add_test(NAME pokemon_alpha_nest COMMAND alpha_nest_tests)

  add_executable(regional_research_tests
      tests/regional_research_test.cpp
      src/RegionalResearch.cpp)
  target_include_directories(regional_research_tests PRIVATE
      "${CMAKE_SOURCE_DIR}/src"
      "${CMAKE_SOURCE_DIR}/ext/glm/include")
  add_test(NAME pokemon_regional_research COMMAND regional_research_tests)

  add_executable(resource_locator_tests
      tests/resource_locator_test.cpp
      src/ResourceLocator.cpp)
  target_include_directories(resource_locator_tests PRIVATE
      "${CMAKE_SOURCE_DIR}/src")
  add_test(NAME pokemon_resource_locator COMMAND resource_locator_tests)

  add_executable(capture_projectile_tests
      tests/capture_projectile_test.cpp
      src/CaptureProjectile.cpp)
  target_include_directories(capture_projectile_tests PRIVATE
      "${CMAKE_SOURCE_DIR}/src"
      "${CMAKE_SOURCE_DIR}/ext/glm/include")
  add_test(NAME pokemon_capture_projectile COMMAND capture_projectile_tests)

  add_executable(hud_telemetry_tests
      tests/hud_telemetry_test.cpp
      src/HudTelemetry.cpp)
  target_include_directories(hud_telemetry_tests PRIVATE
      "${CMAKE_SOURCE_DIR}/src")
  add_test(NAME hud_telemetry_contract COMMAND hud_telemetry_tests)

  add_executable(game_session_tests
      tests/game_session_test.cpp
      src/GameSession.cpp)
  target_include_directories(game_session_tests PRIVATE
      "${CMAKE_SOURCE_DIR}/src")
  add_test(NAME game_session_fixed_timestep COMMAND game_session_tests)

  add_executable(player_controller_tests
      tests/player_controller_test.cpp
      src/PlayerController.cpp
      src/PokemonAnimation.cpp)
  target_include_directories(player_controller_tests PRIVATE
      "${CMAKE_SOURCE_DIR}/src"
      "${CMAKE_SOURCE_DIR}/ext/glm/include")
  add_test(NAME player_controller_physics COMMAND player_controller_tests)

  add_executable(terrain_height_map_tests
      tests/terrain_height_map_test.cpp
      src/TerrainHeightMap.cpp)
  target_include_directories(terrain_height_map_tests PRIVATE
      "${CMAKE_SOURCE_DIR}/src")
  add_test(NAME terrain_height_sampling COMMAND terrain_height_map_tests)

  add_executable(third_person_camera_tests
      tests/third_person_camera_test.cpp
      src/ThirdPersonCamera.cpp)
  target_include_directories(third_person_camera_tests PRIVATE
      "${CMAKE_SOURCE_DIR}/src"
      "${CMAKE_SOURCE_DIR}/ext/glm/include")
  add_test(NAME third_person_camera_collision COMMAND third_person_camera_tests)

  add_executable(pokemon_behavior_tests
      tests/pokemon_behavior_test.cpp
      src/BattleMechanics.cpp
      src/Pokemon.cpp
      src/PokemonEcology.cpp
      src/PokemonPerception.cpp
      src/PokemonNavigation.cpp
      src/PokemonAnimation.cpp
      src/PokemonSpecies.cpp
      src/PokemonSpawn.cpp
      src/WorldRegion.cpp)
  target_include_directories(pokemon_behavior_tests PRIVATE
      "${CMAKE_SOURCE_DIR}/src"
      "${CMAKE_SOURCE_DIR}/ext/glm/include")
  add_test(NAME pokemon_behavior_states COMMAND pokemon_behavior_tests)

  add_executable(pokemon_ecology_tests
      tests/pokemon_ecology_test.cpp
      src/PokemonEcology.cpp
      src/PokemonSpecies.cpp)
  target_include_directories(pokemon_ecology_tests PRIVATE
      "${CMAKE_SOURCE_DIR}/src")
  add_test(NAME pokemon_day_night_ecology COMMAND pokemon_ecology_tests)

  add_executable(world_simulation_tests
      tests/world_simulation_test.cpp
	  src/BattleMechanics.cpp
      src/FieldCamp.cpp
      src/Pokemon.cpp
	  src/PokemonEcology.cpp
      src/PokemonPerception.cpp
      src/PokemonNavigation.cpp
      src/PokemonAnimation.cpp
      src/PokemonSpecies.cpp
      src/PokemonSpawn.cpp
      src/WorldLayout.cpp
      src/WorldRegion.cpp
      src/WorldLighting.cpp)
  target_include_directories(world_simulation_tests PRIVATE
      "${CMAKE_SOURCE_DIR}/src"
      "${CMAKE_SOURCE_DIR}/ext/glm/include")
  add_test(NAME pokemon_world_ten_minute_simulation COMMAND world_simulation_tests)

  add_executable(frame_capture_tests
      tests/frame_capture_test.cpp
      src/FrameCapture.cpp)
  target_include_directories(frame_capture_tests PRIVATE
      "${CMAKE_SOURCE_DIR}/src")
  add_test(NAME native_frame_capture_encoding COMMAND frame_capture_tests)

  add_executable(pokemon_perception_tests
      tests/pokemon_perception_test.cpp
      src/PokemonPerception.cpp)
  target_include_directories(pokemon_perception_tests PRIVATE
      "${CMAKE_SOURCE_DIR}/src"
      "${CMAKE_SOURCE_DIR}/ext/glm/include")
  add_test(NAME pokemon_perception_model COMMAND pokemon_perception_tests)

  add_executable(pokemon_sightline_tests
      tests/pokemon_sightline_test.cpp
      src/PokemonSightline.cpp)
  target_include_directories(pokemon_sightline_tests PRIVATE
      "${CMAKE_SOURCE_DIR}/src"
      "${CMAKE_SOURCE_DIR}/ext/glm/include")
  add_test(NAME pokemon_sightline_occlusion COMMAND pokemon_sightline_tests)

  add_executable(pokemon_group_alert_tests
      tests/pokemon_group_alert_test.cpp
      src/PokemonGroupAlert.cpp)
  target_include_directories(pokemon_group_alert_tests PRIVATE
      "${CMAKE_SOURCE_DIR}/src"
      "${CMAKE_SOURCE_DIR}/ext/glm/include")
  add_test(NAME pokemon_group_alert_propagation COMMAND pokemon_group_alert_tests)

  add_executable(pokemon_targeting_tests
      tests/pokemon_targeting_test.cpp
      src/PokemonTargeting.cpp)
  target_include_directories(pokemon_targeting_tests PRIVATE
      "${CMAKE_SOURCE_DIR}/src"
      "${CMAKE_SOURCE_DIR}/ext/glm/include")
  add_test(NAME pokemon_target_selection COMMAND pokemon_targeting_tests)

  add_executable(field_radar_tests
      tests/field_radar_test.cpp
      src/FieldRadar.cpp)
  target_include_directories(field_radar_tests PRIVATE
      "${CMAKE_SOURCE_DIR}/src"
      "${CMAKE_SOURCE_DIR}/ext/glm/include")
  add_test(NAME pokemon_field_radar COMMAND field_radar_tests)

  add_executable(capture_mechanics_tests
      tests/capture_mechanics_test.cpp
      src/CaptureMechanics.cpp)
  target_include_directories(capture_mechanics_tests PRIVATE
      "${CMAKE_SOURCE_DIR}/src")
  add_test(NAME pokemon_capture_mechanics COMMAND capture_mechanics_tests)

  add_executable(capture_sequence_tests
      tests/capture_sequence_test.cpp
      src/CaptureSequence.cpp)
  target_include_directories(capture_sequence_tests PRIVATE
      "${CMAKE_SOURCE_DIR}/src")
  add_test(NAME pokemon_capture_sequence COMMAND capture_sequence_tests)

  add_executable(battle_mechanics_tests
      tests/battle_mechanics_test.cpp
      src/BattleMechanics.cpp)
  target_include_directories(battle_mechanics_tests PRIVATE
      "${CMAKE_SOURCE_DIR}/src")
  add_test(NAME pokemon_battle_mechanics COMMAND battle_mechanics_tests)

  add_executable(battle_move_volume_tests
      tests/battle_move_volume_test.cpp
      src/BattleMoveVolume.cpp
      src/CaptureProjectile.cpp)
  target_include_directories(battle_move_volume_tests PRIVATE
      "${CMAKE_SOURCE_DIR}/src"
      "${CMAKE_SOURCE_DIR}/ext/glm/include")
  add_test(NAME pokemon_battle_move_volume COMMAND battle_move_volume_tests)

	add_executable(battle_move_loadout_tests
	    tests/battle_move_loadout_test.cpp
	    src/BattleMechanics.cpp
	    src/BattleMoveLoadout.cpp)
	target_include_directories(battle_move_loadout_tests PRIVATE
	    "${CMAKE_SOURCE_DIR}/src")
	add_test(NAME pokemon_battle_move_loadout COMMAND battle_move_loadout_tests)

  add_executable(battle_sequence_tests
      tests/battle_sequence_test.cpp
      src/BattleSequence.cpp)
  target_include_directories(battle_sequence_tests PRIVATE
      "${CMAKE_SOURCE_DIR}/src")
  add_test(NAME pokemon_battle_sequence COMMAND battle_sequence_tests)

  add_executable(research_mission_tests
      tests/research_mission_test.cpp
      src/ResearchMission.cpp)
  target_include_directories(research_mission_tests PRIVATE
      "${CMAKE_SOURCE_DIR}/src")
  add_test(NAME pokemon_research_mission COMMAND research_mission_tests)

  add_executable(research_run_state_tests
      tests/research_run_state_test.cpp
      src/ResearchRunState.cpp)
  target_include_directories(research_run_state_tests PRIVATE
      "${CMAKE_SOURCE_DIR}/src")
  add_test(NAME pokemon_research_run_state COMMAND research_run_state_tests)

  add_executable(research_run_simulation_tests
      tests/research_run_simulation_test.cpp
      src/BattleMechanics.cpp
      src/BattleSequence.cpp
      src/CaptureMechanics.cpp
      src/CaptureSequence.cpp
      src/ResearchMission.cpp
      src/ResearchProgression.cpp
      src/ResearchRunState.cpp)
  target_include_directories(research_run_simulation_tests PRIVATE
      "${CMAKE_SOURCE_DIR}/src")
  add_test(NAME pokemon_research_run_simulation COMMAND research_run_simulation_tests)

  add_executable(research_progression_tests
      tests/research_progression_test.cpp
      src/ResearchProgression.cpp)
  target_include_directories(research_progression_tests PRIVATE
      "${CMAKE_SOURCE_DIR}/src")
  add_test(NAME pokemon_research_progression COMMAND research_progression_tests)

  add_executable(field_lure_tests
      tests/field_lure_test.cpp
      src/FieldLure.cpp)
  target_include_directories(field_lure_tests PRIVATE
      "${CMAKE_SOURCE_DIR}/src"
      "${CMAKE_SOURCE_DIR}/ext/glm/include")
  add_test(NAME pokemon_field_lure COMMAND field_lure_tests)

  add_executable(field_camp_tests
      tests/field_camp_test.cpp
      src/FieldCamp.cpp)
  target_include_directories(field_camp_tests PRIVATE
      "${CMAKE_SOURCE_DIR}/src"
      "${CMAKE_SOURCE_DIR}/ext/glm/include")
  add_test(NAME pokemon_field_camp COMMAND field_camp_tests)

  add_executable(game_save_tests
      tests/game_save_test.cpp
      src/GameSave.cpp
      src/ResearchProgression.cpp)
  target_include_directories(game_save_tests PRIVATE
      "${CMAKE_SOURCE_DIR}/src")
  add_test(NAME pokemon_game_save_codec COMMAND game_save_tests)

  add_executable(game_save_storage_tests
      tests/game_save_storage_test.cpp
      src/GameSaveStorage.cpp)
  target_include_directories(game_save_storage_tests PRIVATE
      "${CMAKE_SOURCE_DIR}/src")
  add_test(NAME pokemon_game_save_storage COMMAND game_save_storage_tests)

  add_executable(world_lighting_tests
      tests/world_lighting_test.cpp
      src/WorldLighting.cpp)
  target_include_directories(world_lighting_tests PRIVATE
      "${CMAKE_SOURCE_DIR}/src"
      "${CMAKE_SOURCE_DIR}/ext/glm/include")
  add_test(NAME world_lighting_cycle COMMAND world_lighting_tests)

  add_executable(world_layout_tests
      tests/world_layout_test.cpp
      src/WorldLayout.cpp
      src/WorldRegion.cpp)
  target_include_directories(world_layout_tests PRIVATE
      "${CMAKE_SOURCE_DIR}/src"
      "${CMAKE_SOURCE_DIR}/ext/glm/include")
  add_test(NAME world_landmark_layout COMMAND world_layout_tests)

  add_executable(world_region_tests
      tests/world_region_test.cpp
      src/WorldRegion.cpp)
  target_include_directories(world_region_tests PRIVATE
      "${CMAKE_SOURCE_DIR}/src"
      "${CMAKE_SOURCE_DIR}/ext/glm/include")
  add_test(NAME pokemon_world_region_blending COMMAND world_region_tests)

  add_executable(pokemon_navigation_tests
      tests/pokemon_navigation_test.cpp
      src/PokemonNavigation.cpp)
  target_include_directories(pokemon_navigation_tests PRIVATE
      "${CMAKE_SOURCE_DIR}/src"
      "${CMAKE_SOURCE_DIR}/ext/glm/include")
  add_test(NAME pokemon_overworld_navigation COMMAND pokemon_navigation_tests)
endif()

# We don't really need to include header and resource files to build, but it's
# nice to have them show up in IDEs.
file(GLOB_RECURSE HEADERS "src/*.h" "ext/*/*.h" "ext/glad/*/*.h")
file(GLOB_RECURSE GLSL "resources/*.glsl")

include_directories("ext")
include_directories("ext/glad/include")

# Set the executable.
add_executable(${CMAKE_PROJECT_NAME} ${SOURCES} ${HEADERS} ${GLSL})

# On macOS, keep the existing target name while packaging the same executable
# and resources as a launchable application bundle. A command-line copy remains
# at build/final so existing QA commands continue to work.
if(APPLE AND NOT EMSCRIPTEN)
  file(GLOB_RECURSE NATIVE_BUNDLE_RESOURCES "${CMAKE_SOURCE_DIR}/resources/*")
  list(FILTER NATIVE_BUNDLE_RESOURCES EXCLUDE REGEX "/\\.DS_Store$")
  foreach(BUNDLE_RESOURCE IN LISTS NATIVE_BUNDLE_RESOURCES)
    file(RELATIVE_PATH BUNDLE_RESOURCE_PATH
        "${CMAKE_SOURCE_DIR}/resources" "${BUNDLE_RESOURCE}")
    get_filename_component(BUNDLE_RESOURCE_DIRECTORY
        "${BUNDLE_RESOURCE_PATH}" DIRECTORY)
    if(BUNDLE_RESOURCE_DIRECTORY STREQUAL "")
      set(BUNDLE_PACKAGE_LOCATION "Resources")
    else()
      set(BUNDLE_PACKAGE_LOCATION
          "Resources/${BUNDLE_RESOURCE_DIRECTORY}")
    endif()
    set_source_files_properties("${BUNDLE_RESOURCE}" PROPERTIES
        MACOSX_PACKAGE_LOCATION "${BUNDLE_PACKAGE_LOCATION}"
        HEADER_FILE_ONLY TRUE)
  endforeach()
  target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${NATIVE_BUNDLE_RESOURCES})
  set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
      MACOSX_BUNDLE TRUE
      OUTPUT_NAME "Pokemon World"
      MACOSX_BUNDLE_BUNDLE_NAME "Pokemon World"
      MACOSX_BUNDLE_GUI_IDENTIFIER "com.jayliang.pokemon-world"
      MACOSX_BUNDLE_SHORT_VERSION_STRING "0.4"
      MACOSX_BUNDLE_BUNDLE_VERSION "0.4")
  add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "$<TARGET_FILE:${CMAKE_PROJECT_NAME}>"
          "${CMAKE_CURRENT_BINARY_DIR}/final"
      COMMENT "Keeping build/final for command-line and QA compatibility")
endif()

# Helper function included from FindGfxLibs.cmake. Emscripten supplies its
# browser GLFW port through the linker, while native builds use the existing
# desktop GLFW installation.
if(EMSCRIPTEN)
  set(WEB_LINK_FLAGS
      "-sUSE_GLFW=3 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sALLOW_MEMORY_GROWTH=1"
      "--preload-file=${CMAKE_SOURCE_DIR}/resources@/resources"
      "--shell-file=${CMAKE_SOURCE_DIR}/web/shell.html")
  string(REPLACE ";" " " WEB_LINK_FLAGS_STRING "${WEB_LINK_FLAGS}")
  set_property(TARGET ${CMAKE_PROJECT_NAME} APPEND_STRING PROPERTY LINK_FLAGS " ${WEB_LINK_FLAGS_STRING}")
  set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME "index" SUFFIX ".html")
else()
  findGLFW3(${CMAKE_PROJECT_NAME})
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/ext/glm/include/glm/glm.hpp")
  target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC "${CMAKE_SOURCE_DIR}/ext/glm/include")
else()
  findGLM(${CMAKE_PROJECT_NAME})
endif()

# OS specific options and libraries
if(NOT EMSCRIPTEN)
  if(NOT WIN32)
    message(STATUS "Adding GCC style compiler flags")
    add_compile_options("-Wall")
    add_compile_options("-pedantic")
    add_compile_options("-Werror=return-type")

    if(APPLE)
      # Add required frameworks for GLFW.
      target_link_libraries(${CMAKE_PROJECT_NAME} "-framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo")
    else()
      # Link the Linux OpenGL library.
      target_link_libraries(${CMAKE_PROJECT_NAME} "GL" "dl")
    endif()
  else()
    # Link OpenGL on Windows.
    target_link_libraries(${CMAKE_PROJECT_NAME} opengl32.lib)
  endif()
endif()
