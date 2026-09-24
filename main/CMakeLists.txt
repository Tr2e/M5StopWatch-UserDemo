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
    icon_stopwatch.c
    icon_watch_face.c
)
foreach(STOCK_DEMO_IMAGE IN LISTS STOCK_DEMO_IMAGE_SRCS)
    list(FILTER MY_SRCS EXCLUDE REGEX "/assets/images/${STOCK_DEMO_IMAGE}$")
endforeach()
list(FILTER MY_SRCS EXCLUDE REGEX "/assets/images/watch_face/")
list(FILTER MY_SRCS EXCLUDE REGEX "/assets/fonts/CommissionerMedium64.c$")

# Gundam Museum is an RX-78-only product experience. Keep the other authored
# models in the repository for archival/host-side geometry checks, but do not
# compile or link their mesh builders into device firmware.
set(ARCHIVED_GUNDAM_MODEL_SRCS
    char_zaku.cpp
    destiny_gundam.cpp
    nu_gundam.cpp
    sazabi.cpp
    strike_gundam.cpp
)
foreach(ARCHIVED_GUNDAM_MODEL IN LISTS ARCHIVED_GUNDAM_MODEL_SRCS)
    list(FILTER MY_SRCS EXCLUDE REGEX "/apps/app_gundam_museum/model/${ARCHIVED_GUNDAM_MODEL}$")
endforeach()

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

# Device-only benchmark controls. Product builds keep the interactive app and
# topology counters enabled; CI/device audits may opt into a self-running app
# or compile the TRI/QUAD accounting out for an A/B measurement.
set(STOPWATCH_BENCHMARK_AUTORUN OFF CACHE BOOL "Run the 3D benchmark automatically at boot")
set(STOPWATCH_BENCHMARK_RX60_ONLY OFF CACHE BOOL "Run only the RX-78 60% benchmark stage")
set(STOPWATCH_COLLECT_TOPOLOGY_STATS ON CACHE BOOL "Collect TRI/QUAD benchmark statistics")
if(STOPWATCH_BENCHMARK_AUTORUN)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE STOPWATCH_BENCHMARK_AUTORUN=1)
else()
    target_compile_definitions(${COMPONENT_LIB} PRIVATE STOPWATCH_BENCHMARK_AUTORUN=0)
endif()
if(STOPWATCH_BENCHMARK_RX60_ONLY)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE STOPWATCH_BENCHMARK_RX60_ONLY=1)
else()
    target_compile_definitions(${COMPONENT_LIB} PRIVATE STOPWATCH_BENCHMARK_RX60_ONLY=0)
endif()
if(STOPWATCH_COLLECT_TOPOLOGY_STATS)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE STOPWATCH_COLLECT_TOPOLOGY_STATS=1)
else()
    target_compile_definitions(${COMPONENT_LIB} PRIVATE STOPWATCH_COLLECT_TOPOLOGY_STATS=0)
endif()

# Software rasterization is a release hot path even in a debug firmware.
# Keep debug symbols/assertions. Use the platform memset instead of the
# compiler's byte-at-a-time inline expansion for depth buffer clears.
set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/app_lets_and_go_racer/view/garage_renderer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/app_lets_and_go_racer/view/race_renderer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/app_gundam_arena/view/arena_renderer.cpp"
    PROPERTIES COMPILE_OPTIONS "-O2;-fno-builtin-memset"
)
set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/app_gundam_museum/view/museum_renderer.cpp"
    PROPERTIES COMPILE_OPTIONS "-O2;-fno-builtin-memset;-mtext-section-literals"
)
