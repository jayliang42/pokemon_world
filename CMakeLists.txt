cmake_minimum_required(VERSION 3.10)
include("${CMAKE_SOURCE_DIR}/FindGfxLibs.cmake")

# Name of the project
project(final)

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

# We don't really need to include header and resource files to build, but it's
# nice to have them show up in IDEs.
file(GLOB_RECURSE HEADERS "src/*.h" "ext/*/*.h" "ext/glad/*/*.h")
file(GLOB_RECURSE GLSL "resources/*.glsl")

include_directories("ext")
include_directories("ext/glad/include")

# Set the executable.
add_executable(${CMAKE_PROJECT_NAME} ${SOURCES} ${HEADERS} ${GLSL})

# Helper function included from FindGfxLibs.cmake. Emscripten supplies its
# browser GLFW port through the linker, while native builds use the existing
# desktop GLFW installation.
if(EMSCRIPTEN)
  set(WEB_LINK_FLAGS
      "-sUSE_GLFW=3 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2"
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
