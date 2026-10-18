# gta_sa_module - GTA: San Andreas engine library + gta_standalone host.
#
# Builds on its own:
#     cmake -S . -B build
#     cmake --build build --config Release
# and can also be add_subdirectory()'d by a larger project.
#
# Two targets:
#   gta_sa_module  - the portable library (PUBLIC API in include/).
#   gta_standalone - a thin host exe that drives the library (NOT in the lib).

cmake_minimum_required(VERSION 3.20)
project(gta_sa_module LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)   # no compiler extensions (coordination rule)

# --- library target -------------------------------------------------------
add_library(gta_sa_module STATIC
    src/world/img_archive.cpp
    src/world/map_data.cpp
    src/world/world_streaming_impl.cpp
    src/world/gta_world.cpp
    src/peds/ped_manager.cpp
    src/vehicles/vehicle_manager.cpp
    src/audio/radio_player.cpp
    src/scripts/script_machine.cpp
)

target_include_directories(gta_sa_module
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include   # gta_sa/*.hpp (integrator API)
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src       # internal "world/..." headers
)

if(MSVC)
    target_compile_options(gta_sa_module PRIVATE /W4 /permissive-)
else()
    target_compile_options(gta_sa_module PRIVATE -Wall -Wextra)
endif()

# --- standalone host (separate target, lives outside the library) ---------
# The host is the FULL GTA engine (renderer + physics + dev tools + all the
# play features), under host/engine/. It is Windows/D3D9-only; the library above
# stays cross-platform. It links the library so the GTA gameplay/data API is
# present and exercised; another host can drive the library with its own renderer.
if(WIN32)
    add_executable(gta_standalone WIN32
        host/engine/main.cpp
        host/engine/World.cpp
        host/engine/TimeCycle.cpp
        host/engine/D3DRenderer.cpp
        host/engine/DffLoader.cpp
        host/engine/TxdLoader.cpp
        host/engine/ImgLoader.cpp
        host/engine/IdeLoader.cpp
        host/engine/IplLoader.cpp
        host/engine/BinaryIpl.cpp
        host/engine/ColLoader.cpp
        host/engine/AuditCollision.cpp
        host/engine/Fe2d.cpp
        host/engine/FeFont.cpp
        host/engine/FeText.cpp
        host/engine/FeMenu.cpp
        host/engine/SkinnedDff.cpp
        host/engine/IfpLoader.cpp
        host/engine/PlayerPed.cpp
        host/engine/PlayerFootIK.cpp
        host/engine/PlayerWeapon.cpp
        host/engine/Hud.cpp
        host/engine/Audio.cpp
        host/engine/Effects.cpp
        host/engine/AimTest.cpp
        host/engine/Movement.cpp
        host/engine/Camera.cpp
        host/engine/CameraShake.cpp
        host/engine/Water.cpp
        host/engine/ClimbScan.cpp
        host/engine/Sky.cpp
    )
    target_include_directories(gta_standalone PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/host/engine)
    target_link_libraries(gta_standalone PRIVATE gta_sa_module d3d9 shell32 ole32 winmm)
    if(MSVC)
        target_compile_definitions(gta_standalone PRIVATE
            UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS)
        # CPed.cpp-style /Zm is not needed here (play engine only); keep /W3.
        target_compile_options(gta_standalone PRIVATE /W3)
    else()
        # MinGW cross-build (scripts/build_mingw.sh): same entry point and charset.
        target_compile_definitions(gta_standalone PRIVATE UNICODE _UNICODE)
        target_link_options(gta_standalone PRIVATE -municode -static -static-libgcc -static-libstdc++)
    endif()
else()
    message(STATUS "gta_standalone host skipped (Windows/D3D9 only); library builds everywhere")
endif()
