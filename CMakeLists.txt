# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(StopWatch-UserDemo)

# The shared canvas backend is also a hot path for the software Racer renderer.
# Set properties in the dependency's directory without modifying its checkout.
idf_component_get_property(RACER_GFX_SOURCE_DIR M5GFX COMPONENT_DIR)
set_source_files_properties(
    "${RACER_GFX_SOURCE_DIR}/src/lgfx/v1/LGFXBase.cpp"
    "${RACER_GFX_SOURCE_DIR}/src/lgfx/v1/LGFX_Sprite.cpp"
    "${RACER_GFX_SOURCE_DIR}/src/lgfx/v1/misc/pixelcopy.cpp"
    DIRECTORY "${RACER_GFX_SOURCE_DIR}"
    PROPERTIES COMPILE_OPTIONS "-O2"
)
