# GTA SA 1.0 clean-room C++ conversion
# Static library target - reference for engine rewrite, NOT a runnable game.
# Game assets load from the user's own install at runtime and are never shipped.

cmake_minimum_required(VERSION 3.20)

project(gta_sa_cpp
    VERSION 1.0.0
    DESCRIPTION "GTA San Andreas 1.0 clean-room C++ conversion (interoperability research)"
    LANGUAGES CXX
)

# C++17 required (deduction guides, structured bindings, etc.)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MSVC: CPed.cpp (9914 lines) hits C1202 ("recursive type or function
# dependency context too complex") without /Zm. The flag goes through the
# CL environment variable because MSBuild rejects /Zm8000 as fatal D8000,
# while cl.exe accepts it (D9014 warning only). Verified 2026-10-09:
# CL=/Zm8000 => 0 errors. Note: /Zm2000 does NOT fix C1202, /Zm8000 does.
# CL=/Zm8000 removed 2026-10-09: set it manually when building gta_sa lib

# Static library
add_library(gta_sa STATIC
    src/CVector.cpp
    src/CMatrix.cpp
    src/CColModel.cpp
    src/CColSphere.cpp
    src/CCollision.cpp
    src/CColAccel.cpp
    src/CCollisionData.cpp
    src/CColTrianglePlane.cpp
    src/CPlaceable.cpp
    src/CEntity.cpp
    src/CPhysical.cpp
    src/CObject.cpp
    src/CBuilding.cpp
    src/CWorld.cpp
    src/CCam.cpp
    src/CIdleCam.cpp
    src/CCamera.cpp
    src/CRenderer.cpp
    src/CPostEffects.cpp
    src/CShadows.cpp
    src/CWeather.cpp
    src/CFont.cpp
    src/CSprite2d.cpp
    src/CBaseModelInfo.cpp
    src/CAtomicModelInfo.cpp
    src/CClumpModelInfo.cpp
    src/CPedModelInfo.cpp
    src/CWeaponModelInfo.cpp
    src/CTimeInfo.cpp
    src/CVehicleModelInfo.cpp
    src/CModelInfo.cpp
    # src/CPool.cpp  # header-only template, no .cpp needed
    src/CAnimManager.cpp
    src/CAnimBlendAssociation.cpp
    src/CAnimBlendAssocGroup.cpp
    src/CAnimBlendHierarchy.cpp
    src/CAnimBlendSequence.cpp
    src/CAnimBlendNode.cpp
    src/CAnimBlendStaticAssociation.cpp
    src/CAnimBlendClumpData.cpp
    src/CVehicle.cpp
    src/CAutomobile.cpp
    src/CBike.cpp
    src/CBoat.cpp
    src/CTrain.cpp
    src/CPlane.cpp
    src/CHeli.cpp
    src/CDamageManager.cpp
    src/CCarEnterExit.cpp
    src/CDoor.cpp
    src/CBouncingPanel.cpp
    src/CRideAnimData.cpp
    src/CWeapon.cpp
    src/CFire.cpp
    src/CFireManager.cpp
    src/CWeaponEffects.cpp
    src/CExplosion.cpp
    src/CShotInfo.cpp
    src/CBulletInfo.cpp
    src/CAudioEngine.cpp
    src/CAESound.cpp
    src/CAEAudioHardware.cpp
    src/CAERadioTrackManager.cpp
)

target_include_directories(gta_sa
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# MSVC-specific settings
if(MSVC)
    target_compile_options(gta_sa PRIVATE
        /W3            # warning level 3
        /permissive-   # standards conformance
        /Zc:__cplusplus
    )
    # Silence deprecation warnings for now during conversion
    target_compile_definitions(gta_sa PRIVATE
        _CRT_SECURE_NO_WARNINGS
    )
else()
    target_compile_options(gta_sa PRIVATE
        -Wall -Wextra -Wno-unused-parameter
    )
endif()

# TODO: add more subsystems as they're converted.
# See ../port_plan/layering.json for dependency order.
# Next up: enums (eWeatherType etc. are inline in CWeather.h for now),
# the real RenderWare layer (replaces include/RenderTypes.h), core containers
# (CPtrListSingleLink/CPtrListDoubleLink - CRenderer::tScanLists), CGame,
# audio entities (CAEFrontendAudioEntity etc. - still opaque stand-ins in
# CAudioEngine.h; CAEWeatherAudioEntity - blocks CWeather::m_WeatherAudioEntity
# definition), full audio enums (eSoundBankSlot, eAudioEvents, eSurfaceType).

# fx + platform_os subsystems - method bodies filled 2026-10-09
# FxManager_c, CFileMgr, CTimer, CPad - all methods implemented (DirectInput/RW-dependent parts TODO(port)).
target_sources(gta_sa PRIVATE
    src/FxManager.cpp
    src/FxSystem.cpp
    src/CFileMgr.cpp
    src/CFileLoader.cpp
    src/CTimer.cpp
    src/CPad.cpp
    src/CPedIntelligence.cpp
    src/CPed.cpp
    src/CPlayerPed.cpp
    src/CHud.cpp)

# streaming subsystem (2026-10-08)
# CStreaming - first class with filled method bodies (24 methods, see BUILD_NOTES).
# Header-only: include/CModelInfo.h (minimal declarations; full Models port TODO).
target_sources(gta_sa PRIVATE
    src/CStreaming.cpp
)

# scripts_missions subsystem (2026-10-09)
# CRunningScript - the SCM script VM thread: Init, Process, ProcessOneCommand,
# parameter collection/storage (Collect/StoreParameters,
# ReadParametersForNewlyStartedScript, ReadTextLabelFromScript), variable
# pointer helpers, UpdateCompareFlag, UpdatePC, script list management, and
# the ProcessCommands0To99 opcode dispatch block (84 opcodes implemented:
# control flow, WAIT, arithmetic, comparisons, GOSUB/RETURN, script control).
# The other 26 dispatch blocks (opcodes 100-2699) are TODO stubs.
# CTheScripts - script manager: switch jump table, IsPlayerOnAMission,
# WipeLocalVariableMemoryForMissionScript, StartNewScript. Init/Process/
# Save/Load and the script-thing helpers need unported subsystems (TODO).
target_sources(gta_sa PRIVATE
    src/CRunningScript.cpp
    src/CTheScripts.cpp
)

# gtasa_cpp.exe - standalone playable executable (added 2026-10-09)
# Links D3D9 directly. Does NOT link gta_sa lib yet (milestone 1: standalone).
add_executable(gtasa_cpp
    src/main.cpp
    src/D3DRenderer.cpp
    src/DffLoader.cpp
    src/TxdLoader.cpp
    src/ImgLoader.cpp
    src/IdeLoader.cpp
    src/IplLoader.cpp
)
target_include_directories(gtasa_cpp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(gtasa_cpp PRIVATE d3d9 shell32)
if(MSVC)
    target_compile_options(gtasa_cpp PRIVATE
        /W3
        /permissive-
        /Zc:__cplusplus
    )
    target_compile_definitions(gtasa_cpp PRIVATE
        _CRT_SECURE_NO_WARNINGS
        UNICODE
        _UNICODE
    )
else()
    target_compile_options(gtasa_cpp PRIVATE -Wall -Wextra)
endif()
set_target_properties(gtasa_cpp PROPERTIES
    WIN32_EXECUTABLE TRUE
    CXX_STANDARD 20
)
