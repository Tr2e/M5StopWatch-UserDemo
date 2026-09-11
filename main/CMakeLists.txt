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

# Keep the product firmware focused on the customized experiences. Launcher and
# Setup remain available as required shell/system apps. The denylist also keeps
# stock demo sources out if they are reintroduced by an upstream merge.
set(STOCK_DEMO_APP_DIRS
    app_alarm_clock
    app_badge
    app_fft
    app_imu
    app_lucky_wheel
    app_stopwatch
    app_template
    app_watch_face
)
foreach(STOCK_DEMO_APP IN LISTS STOCK_DEMO_APP_DIRS)
    list(FILTER MY_SRCS EXCLUDE REGEX "/apps/${STOCK_DEMO_APP}/")
endforeach()

set(STOCK_DEMO_IMAGE_SRCS
    alarm_icon.c
    icon_clock.c
    icon_edit_badge.c
    icon_fft.c
    icon_imu.c
    icon_lucky_wheel.c
    icon_stopwatch.c
    icon_watch_face.c
    lucky_wheel_pointer.c
)
foreach(STOCK_DEMO_IMAGE IN LISTS STOCK_DEMO_IMAGE_SRCS)
    list(FILTER MY_SRCS EXCLUDE REGEX "/assets/images/${STOCK_DEMO_IMAGE}$")
endforeach()
list(FILTER MY_SRCS EXCLUDE REGEX "/assets/images/watch_face/")
list(FILTER MY_SRCS EXCLUDE REGEX "/assets/fonts/CommissionerMedium64.c$")

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
