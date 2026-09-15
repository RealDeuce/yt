cmake_minimum_required(VERSION 3.20)

project(YankeeTrader LANGUAGES C)

include(CTest)
include(GNUInstallDirs)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(YT_USE_SYSTEM_OPENDOORS
    "Use an installed OpenDoors package instead of the pinned source tree" OFF)
set(YT_OPENDOORS_SOURCE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/opendoors" CACHE PATH
    "Path to an OpenDoors source checkout")

if(YT_USE_SYSTEM_OPENDOORS)
    find_package(OpenDoors 6.3 CONFIG REQUIRED COMPONENTS Static)
else()
    if(NOT EXISTS "${YT_OPENDOORS_SOURCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "OpenDoors was not found at ${YT_OPENDOORS_SOURCE_DIR}. "
            "Initialize third_party/opendoors or set YT_OPENDOORS_SOURCE_DIR.")
    endif()

    # OpenDoors is a complete project and intentionally uses CTest's global
    # BUILD_TESTING option.  Keep its test and example trees out of a normal
    # Yankee Trader build while preserving this project's test selection.
    set(_yt_build_testing "${BUILD_TESTING}")
    set(BUILD_TESTING OFF CACHE BOOL "Build tests" FORCE)
    set(OPENDOORS_BUILD_SHARED OFF CACHE BOOL
        "Build the OpenDoors shared library" FORCE)
    set(OPENDOORS_BUILD_STATIC ON CACHE BOOL
        "Build the OpenDoors static library" FORCE)
    set(OPENDOORS_BUILD_EXAMPLES OFF CACHE BOOL
        "Build the OpenDoors example programs" FORCE)
    add_subdirectory("${YT_OPENDOORS_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}/opendoors" EXCLUDE_FROM_ALL)
    set(BUILD_TESTING "${_yt_build_testing}" CACHE BOOL "Build tests" FORCE)
    unset(_yt_build_testing)
endif()

function(yt_configure_target target)
    target_include_directories(${target} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)
    if(WIN32)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_definitions(${target} PRIVATE _POSIX_C_SOURCE=200809L)
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wconversion -Wshadow -Wstrict-prototypes
            -pedantic -ffp-contract=off)
    endif()
endfunction()

set(YT_CORE_SOURCES
    src/qb.c
    src/qb_format.c
    src/yt_brun_fatal.c
    src/yt_cli.c
    src/yt_data.c
    src/yt_file.c
    src/yt_input_model.c
	    src/yt_main_error.c
    src/yt_pager.c
    src/yt_platform.c
    src/yt_presentation.c
    src/yt_random.c
    src/yt_sound.c
	    src/yt_startup_model.c
    src/yt_text.c)

set(YT_GAME_SOURCES
    src/yt_config.c
    src/yt_config_output.c
    src/yt_game.c
    src/yt_init.c
    src/yt_maint.c
    src/yt_names.c
    src/yt_planet_updater.c
    src/yt_player_cache.c
    src/yt_port_math.c
    src/yt_portname.c
    src/yt_score.c
    src/yt_score_format.c
    src/yt_team.c)

add_library(yt_core STATIC ${YT_CORE_SOURCES})
yt_configure_target(yt_core)
if(WIN32)
	target_link_libraries(yt_core PUBLIC bcrypt)
else()
    target_link_libraries(yt_core PUBLIC m)
endif()

add_library(yt_game STATIC ${YT_GAME_SOURCES})
yt_configure_target(yt_game)
target_link_libraries(yt_game PUBLIC yt_core)

set(YT_RUNTIME_SOURCES
    src/yt_door.c
    src/yt_input.c
    src/yt_output.c
    src/yt_session.c
    src/yt_session_action.c
    src/yt_session_attack.c
    src/yt_session_bribe.c
    src/yt_session_computer.c
    src/yt_session_computer_assets.c
    src/yt_session_computer_media.c
    src/yt_session_computer_menu.c
    src/yt_session_computer_route.c
    src/yt_session_radio.c
    src/yt_session_sector.c
    src/yt_session_sector_entry.c
    src/yt_session_shell.c
    src/yt_session_death.c
    src/yt_session_deployed_attack.c
    src/yt_session_earth.c
    src/yt_session_error.c
    src/yt_session_fighters.c
    src/yt_session_genesis.c
    src/yt_session_info.c
    src/yt_session_io.c
    src/yt_session_lockout.c
    src/yt_session_lottery.c
    src/yt_session_mines.c
    src/yt_session_movement.c
    src/yt_session_nearest.c
    src/yt_session_planet_inventory.c
    src/yt_session_planet_landing.c
    src/yt_session_planet_management.c
    src/yt_session_planet_menu.c
    src/yt_session_planet_move.c
    src/yt_session_planets.c
    src/yt_session_port_ownership.c
    src/yt_session_port_trade.c
    src/yt_session_ports.c
    src/yt_session_profit.c
    src/yt_session_projectile.c
    src/yt_session_projectile_missile.c
    src/yt_session_projectile_plasma.c
    src/yt_session_registration.c
    src/yt_session_route.c
    src/yt_session_salvage.c
    src/yt_session_spies.c
    src/yt_session_startup.c
    src/yt_session_team.c
    src/yt_session_xannor.c)

add_library(yt_runtime STATIC ${YT_RUNTIME_SOURCES})
yt_configure_target(yt_runtime)
target_link_libraries(yt_runtime PUBLIC yt_game OpenDoors::Static)

add_executable(yt src/main_yt.c)
yt_configure_target(yt)
target_link_libraries(yt PRIVATE yt_runtime)

function(yt_add_utility target source)
    add_executable(${target} "${source}")
    yt_configure_target(${target})
    target_link_libraries(${target} PRIVATE yt_game)
endfunction()

yt_add_utility(ytmaint src/main_ytmaint.c)
yt_add_utility(ytconfig src/main_ytconfig.c)
yt_add_utility(yt-init src/main_yt_init.c)
yt_add_utility(rmt-init src/main_rmt_init.c)
target_sources(rmt-init PRIVATE src/yt_rmt_door.c)
target_link_libraries(rmt-init PRIVATE OpenDoors::Static)
yt_add_utility(portname src/main_portname.c)
yt_add_utility(local src/main_local.c)

add_executable(yt-assetgen EXCLUDE_FROM_ALL src/main_assetgen.c)
yt_configure_target(yt-assetgen)

set(YT_DATA_NAMES
    YTOPEN.ANS
    YTOPEN.ASC
    YTINSTR.DOC
    YTSYSOP.DOC
    XANNORHQ.TXT
    YTECHO.TXT
    LOCKOUT.DAT
    YT.REG)
set(YT_GENERATED_DATA_DIR "${CMAKE_CURRENT_BINARY_DIR}/data")
set(YT_DATA_FILES)
foreach(name IN LISTS YT_DATA_NAMES)
    list(APPEND YT_DATA_FILES "${YT_GENERATED_DATA_DIR}/${name}")
endforeach()
add_custom_command(
    OUTPUT ${YT_DATA_FILES}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${YT_GENERATED_DATA_DIR}"
    COMMAND $<TARGET_FILE:yt-assetgen> "${YT_GENERATED_DATA_DIR}/"
    DEPENDS yt-assetgen src/yt_assets.inc
    COMMENT "Generating Yankee Trader package data"
    VERBATIM)
add_custom_target(yt-data ALL DEPENDS ${YT_DATA_FILES})

if(BUILD_TESTING)
    function(yt_add_test target source library)
        add_executable(${target} "${source}")
        yt_configure_target(${target})
        target_compile_definitions(${target} PRIVATE YT_TESTING)
        target_link_libraries(${target} PRIVATE ${library})
        add_test(NAME ${target} COMMAND ${target})
    endfunction()

    yt_add_test(test_qb tests/test_qb.c yt_core)
    yt_add_test(test_data tests/test_data.c yt_core)
    target_compile_definitions(test_data PRIVATE
        YT_DATA_DIR="${YT_GENERATED_DATA_DIR}/")
    add_dependencies(test_data yt-data)
    yt_add_test(test_sound tests/test_sound.c yt_core)
    yt_add_test(test_input_model tests/test_input_model.c yt_core)
    if(NOT WIN32)
        target_compile_definitions(test_input_model PRIVATE _XOPEN_SOURCE=700)
    endif()
    yt_add_test(test_presentation tests/test_presentation.c yt_game)
    target_sources(test_presentation PRIVATE
	    tests/hostile_attack_model.c
	    tests/hostile_bribe_model.c
        tests/hostile_surrender_model.c
        tests/info_panel_model.c
        tests/player_death_model.c
        tests/sector_mine_model.c)
    target_compile_definitions(test_presentation PRIVATE
        YT_DATA_DIR="${YT_GENERATED_DATA_DIR}/")
    add_dependencies(test_presentation yt-data)
    add_executable(test_output tests/test_output.c src/yt_output.c
        src/yt_input.c)
    yt_configure_target(test_output)
    target_compile_definitions(test_output PRIVATE YT_TESTING)
    target_include_directories(test_output PRIVATE
        "${YT_OPENDOORS_SOURCE_DIR}/include")
    target_link_libraries(test_output PRIVATE yt_game)
    add_test(NAME test_output COMMAND test_output)
    add_executable(test_input_adapter tests/test_input_adapter.c src/yt_input.c
        src/yt_door.c)
    yt_configure_target(test_input_adapter)
    target_compile_definitions(test_input_adapter PRIVATE YT_TESTING)
    target_include_directories(test_input_adapter PRIVATE
        "${YT_OPENDOORS_SOURCE_DIR}/include")
    target_link_libraries(test_input_adapter PRIVATE yt_game)
    add_test(NAME test_input_adapter COMMAND test_input_adapter)
    yt_add_test(test_ytconfig_output tests/test_ytconfig_output.c yt_game)
    yt_add_test(test_score tests/test_score.c yt_game)
    target_sources(test_score PRIVATE
        tests/direct_attack_model.c
	    tests/hostile_attack_model.c
        tests/hostile_bribe_model.c
        tests/hostile_surrender_model.c
        tests/player_death_model.c
        tests/projectile_command_model.c
        tests/projectile_plasma_route_model.c)
    yt_add_test(test_route tests/test_route.c yt_runtime)

    yt_add_test(test_session_credit tests/test_session_credit.c yt_runtime)
    yt_add_test(test_session_action tests/test_session_action.c yt_runtime)
    yt_add_test(test_session_computer tests/test_session_computer.c yt_runtime)
    yt_add_test(test_session_attack tests/test_session_attack.c yt_runtime)
    yt_add_test(test_session_death tests/test_session_death.c yt_runtime)
    yt_add_test(test_session_earth tests/test_session_earth.c yt_runtime)
    yt_add_test(test_session_fighters tests/test_session_fighters.c yt_runtime)
    yt_add_test(test_session_genesis tests/test_session_genesis.c yt_runtime)
    yt_add_test(test_session_mines tests/test_session_mines.c yt_runtime)
    yt_add_test(test_session_movement tests/test_session_movement.c yt_runtime)
    yt_add_test(test_session_nearest tests/test_session_nearest.c yt_runtime)
    yt_add_test(test_session_planets tests/test_session_planets.c yt_runtime)
    yt_add_test(test_session_ports tests/test_session_ports.c yt_runtime)
    yt_add_test(test_session_profit tests/test_session_profit.c yt_runtime)
    yt_add_test(test_session_projectile tests/test_session_projectile.c
        yt_runtime)
    yt_add_test(test_session_radio tests/test_session_radio.c yt_runtime)
    yt_add_test(test_session_sector tests/test_session_sector.c yt_runtime)
    yt_add_test(test_session_shell tests/test_session_shell.c yt_runtime)
    yt_add_test(test_session_spies tests/test_session_spies.c yt_runtime)
    yt_add_test(test_session_team tests/test_session_team.c yt_runtime)
    foreach(session_test
        test_route
        test_session_credit
        test_session_action
        test_session_computer
        test_session_attack
        test_session_death
        test_session_earth
        test_session_fighters
        test_session_genesis
        test_session_mines
        test_session_movement
        test_session_nearest
        test_session_planets
        test_session_ports
        test_session_profit
        test_session_projectile
        test_session_radio
        test_session_sector
        test_session_shell
        test_session_spies
        test_session_team)
        target_sources(${session_test} PRIVATE tests/session_test_runtime.c)
    endforeach()
    yt_add_test(test_planet_updater tests/test_planet_updater.c yt_game)
    yt_add_test(test_port_report tests/test_port_report.c yt_game)

    add_executable(test_assets tests/test_assets.c)
    yt_configure_target(test_assets)
    target_compile_definitions(test_assets PRIVATE
        YT_DATA_DIR="${YT_GENERATED_DATA_DIR}/")
    add_dependencies(test_assets yt-data)
    add_test(NAME test_assets COMMAND test_assets)

    yt_add_test(test_clean_install tests/test_clean_install.c yt_game)
    target_compile_definitions(test_clean_install PRIVATE
        YT_INIT_EXE="$<TARGET_FILE:yt-init>")
    add_dependencies(test_clean_install yt-init ytmaint)

    yt_add_test(test_utilities tests/test_utilities.c yt_game)
    if(NOT WIN32)
        target_compile_definitions(test_utilities PRIVATE _XOPEN_SOURCE=700)
    endif()
    target_compile_definitions(test_utilities PRIVATE
        YT_CONFIG_EXE="$<TARGET_FILE:ytconfig>"
        YT_PORTNAME_EXE="$<TARGET_FILE:portname>"
        YT_RMT_INIT_EXE="$<TARGET_FILE:rmt-init>"
        YT_LOCAL_EXE="$<TARGET_FILE:local>")
    add_dependencies(test_utilities ytconfig portname rmt-init local)
endif()

install(TARGETS yt ytmaint ytconfig yt-init rmt-init portname local
    RUNTIME DESTINATION "${CMAKE_INSTALL_DATADIR}/yankee-trader")
install(FILES ${YT_DATA_FILES}
    DESTINATION "${CMAKE_INSTALL_DATADIR}/yankee-trader")
