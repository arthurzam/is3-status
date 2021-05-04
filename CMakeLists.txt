cmake_minimum_required(VERSION 3.5)

project(is3-status LANGUAGES C)

set (CMAKE_C_STANDARD 11)
add_compile_definitions(_GNU_SOURCE)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_executable(${PROJECT_NAME}
    "src/cmd_date.c"
    "src/ini_parser.c"
    "src/ini_parser.h"
    "src/main.c"
    "src/main.h"
    "src/vprint.c"
    "src/vprint.h"
    "src/fdpoll.c"
    "src/fdpoll.h"
    "src/handle_click_event.c"
)

include(GNUInstallDirs)
install(
    TARGETS ${PROJECT_NAME}
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(yajl "yajl" IMPORTED_TARGET REQUIRED)
target_link_libraries(${PROJECT_NAME} PkgConfig::yajl)

set(NEED_DBUS FALSE)

option(USE_ALSA "Enable accessing ALSA volume" TRUE)
if (USE_ALSA)
    target_sources(${PROJECT_NAME} PRIVATE "src/cmd_volume_alsa.c")
    find_package(ALSA REQUIRED)
    target_link_libraries(${PROJECT_NAME} ALSA::ALSA)
endif()

option(USE_BACKLIGHT "Enable Backlight module" TRUE)
if (USE_BACKLIGHT)
    target_sources(${PROJECT_NAME} PRIVATE "src/cmd_backlight.c")
endif()

option(USE_BATTERY "Enable Battery module" TRUE)
if (USE_BATTERY)
    target_sources(${PROJECT_NAME} PRIVATE "src/cmd_battery.c")
endif()

option(USE_CPU_TEMP "Enable CPU temperature module" TRUE)
if (USE_CPU_TEMP)
    target_sources(${PROJECT_NAME} PRIVATE "src/cmd_cpu_temperature.c")
endif()

option(USE_DISK_USAGE "Enable disk usage module" TRUE)
if (USE_DISK_USAGE)
    target_sources(${PROJECT_NAME} PRIVATE "src/cmd_disk_usage.c")
endif()

option(USE_ETH "Enable Ethernet IP module" TRUE)
if (USE_ETH)
    target_sources(${PROJECT_NAME} PRIVATE
        "src/cmd_eth.c"
        "src/networking.c"
        "src/networking.h"
    )
endif()

option(USE_LOAD "Enable system load module" TRUE)
if (USE_LOAD)
    target_sources(${PROJECT_NAME} PRIVATE "src/cmd_load.c")
endif()

option(USE_MEMORY "Enable RAM memory module" TRUE)
if (USE_MEMORY)
    target_sources(${PROJECT_NAME} PRIVATE "src/cmd_memory.c")
endif()

option(USE_MPRIS "Enable mpris module" TRUE)
if (USE_MPRIS)
    target_sources(${PROJECT_NAME} PRIVATE "src/cmd_mpris.c")
    set(NEED_DBUS TRUE)
endif()

option(USE_RUN_WATCH "Enable running watch module" TRUE)
if (USE_RUN_WATCH)
    target_sources(${PROJECT_NAME} PRIVATE "src/cmd_run_watch.c")
endif()

option(USE_SWAYWM "Enable accessing SwayWM's module" TRUE)
if (USE_SWAYWM)
    target_sources(${PROJECT_NAME} PRIVATE "src/cmd_sway_language.c")
endif()

option(USE_SYSTEMD "Enable systemd's module" TRUE)
if (USE_SYSTEMD)
    target_sources(${PROJECT_NAME} PRIVATE "src/cmd_systemd_watch.c")
    set(NEED_DBUS TRUE)
endif()

option(USE_X11 "Enable accessing X11's module" TRUE)
if (USE_X11)
    target_sources(${PROJECT_NAME} PRIVATE "src/cmd_x11_language.c")
    pkg_check_modules(X11 "x11" IMPORTED_TARGET REQUIRED)
    target_link_libraries(${PROJECT_NAME} PkgConfig::X11)
endif()

if (NEED_DBUS)
    target_sources(${PROJECT_NAME} PRIVATE
        "src/dbus_monitor.c"
        "src/dbus_monitor.h"
    )
    pkg_check_modules(libsystemd "libsystemd>=221" IMPORTED_TARGET REQUIRED)
    target_link_libraries(${PROJECT_NAME} PkgConfig::libsystemd)
endif()

option(USE_PROFILE "Disable infinite loop, not meant for deploying" FALSE)
if (USE_PROFILE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE "PROFILE")
endif()

option(USE_TESTS "Enable inner tests, not meant for deploying" FALSE)
if (USE_TESTS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE "TESTS")
endif()

option(USE_MAN "Generate and install man pages" TRUE)
if (USE_MAN)
    find_program(SCDOC scdoc REQUIRED)
    add_custom_command(
        TARGET ${PROJECT_NAME}
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/is3-status.1.scd"
        COMMAND ${SCDOC} < "${CMAKE_CURRENT_SOURCE_DIR}/is3-status.1.scd" > "${CMAKE_CURRENT_BINARY_DIR}/is3-status.1"
    )
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/is3-status.1" DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")
endif()
