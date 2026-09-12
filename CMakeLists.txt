cmake_minimum_required(VERSION 3.20)
project(fsr3lite LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(EPOXY REQUIRED IMPORTED_TARGET epoxy)

add_executable(fsr3lite
  src/app/main.cpp
  src/gfx/shader.cpp
  src/gfx/texture.cpp
  src/gfx/gpu_timer.cpp
  src/renderer/camera.cpp
  src/renderer/scene.cpp
  src/renderer/gltf_loader.cpp
  src/renderer/gbuffer.cpp
  src/upscale/upscaler.cpp
  src/opticalflow/optical_flow.cpp
  src/framegen/frame_generator.cpp
  src/metrics/image_metrics.cpp
)

target_include_directories(fsr3lite PRIVATE src)
target_include_directories(fsr3lite SYSTEM PRIVATE third_party)
target_link_libraries(fsr3lite PRIVATE PkgConfig::SDL2 PkgConfig::EPOXY)

# Shaders are loaded from disk at runtime (hot reload), so the binary needs to
# know where the source tree lives.
target_compile_definitions(fsr3lite PRIVATE
  FSR3LITE_SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders"
  FSR3LITE_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")

target_compile_options(fsr3lite PRIVATE -Wall -Wextra -Wno-unused-parameter)
