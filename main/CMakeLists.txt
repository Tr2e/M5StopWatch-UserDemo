file(GLOB_RECURSE MY_SRCS
    "apps/*.c"
    "apps/*.cc"
    "apps/*.cpp"
    "assets/*.c"
    "assets/*.cc"
    "assets/*.cpp"
    "hal/*.c"
    "hal/*.cc"
    "hal/*.cpp"
)

set(MY_INCS
    "."
)

idf_component_register(
    SRCS 
        "main.cpp"
        ${MY_SRCS}
    INCLUDE_DIRS 
        ${MY_INCS}
    EMBED_FILES
        "assets/sfx/boot_sfx.bin"
    EMBED_TXTFILES
        "hal/utils/config_ap/assets/badge_config_ap.html"
)

# Software rasterization is a release hot path even in a debug firmware.
# Keep debug symbols/assertions. Use the platform memset instead of the
# compiler's byte-at-a-time inline expansion for depth buffer clears.
set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/app_lets_and_go_racer/view/garage_renderer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/app_lets_and_go_racer/view/race_renderer.cpp"
    PROPERTIES COMPILE_OPTIONS "-O2;-fno-builtin-memset"
)
