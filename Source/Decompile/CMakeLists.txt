cmake_minimum_required(VERSION 3.20)
project(DesperabisRecovery LANGUAGES CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake")

include(TestBigEndian)
test_big_endian(RECOVERY_BIG_ENDIAN)
if(RECOVERY_BIG_ENDIAN)
    message(FATAL_ERROR "The original DOS object layout requires a little-endian host.")
endif()

add_library(behavior-recovery STATIC
    Scaffold/GameEnums.h
    Scaffold/GameContext.h
    Scaffold/GameContext.cpp
    Scaffold/GameRuntime.h
    Scaffold/ApplicationFlow.h
    Scaffold/ApplicationFlow.cpp
    Scaffold/BehaviorDispatch.cpp
    Scaffold/CollisionMap.cpp
    Scaffold/Heightfield.cpp
    Scaffold/Lighting.cpp
    Scaffold/Door.cpp
    Scaffold/GameMath.cpp
    Scaffold/BehaviorHandlers.cpp
    Scaffold/ObjectInteraction.cpp
    Scaffold/PlayerInteraction.cpp
)
target_include_directories(behavior-recovery PUBLIC Scaffold)
target_compile_features(behavior-recovery PUBLIC cxx_std_17)
add_library(Desperabis::GameCore ALIAS behavior-recovery)
set_target_properties(behavior-recovery PROPERTIES OUTPUT_NAME Desperabis-GameCore)

add_executable(desperabis-recovery Scaffold/Main.cpp)
target_link_libraries(desperabis-recovery PRIVATE behavior-recovery)

include(CTest)
foreach(target Continuous Experimental Nightly NightlyMemoryCheck)
    if(TARGET ${target})
        set_target_properties(${target} PROPERTIES FOLDER "CMake/CTest")
    endif()
endforeach()
if(BUILD_TESTING)
    add_executable(tutorial-tests Scaffold/TutorialTests.cpp)
    target_link_libraries(tutorial-tests PRIVATE Desperabis::GameCore)
    add_test(NAME tutorial-prompts COMMAND tutorial-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Data/SourceAssets/Original")
    add_executable(ambient-tests Scaffold/AmbientTests.cpp)
    target_link_libraries(ambient-tests PRIVATE Desperabis::GameCore)
    add_test(NAME ambient-sound COMMAND ambient-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Data/SourceAssets/Original")
    add_executable(game-runtime-tests Scaffold/GameRuntimeTests.cpp)
    target_link_libraries(game-runtime-tests PRIVATE Desperabis::GameCore)
    add_test(NAME game-runtime COMMAND game-runtime-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/../NewRenderer/SharedSource/base/GamePlay/GamePlay.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/Scaffold/GameEnums.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Data/SourceAssets/Original/LEVEL"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Data/SaveGames/Nice/STAND4.SPL")
    add_executable(behavior-recovery-tests Scaffold/Tests.cpp)
    target_link_libraries(behavior-recovery-tests PRIVATE behavior-recovery)
    add_executable(application-flow-tests Scaffold/ApplicationFlowTests.cpp)
    target_link_libraries(application-flow-tests PRIVATE behavior-recovery)
    add_test(NAME application-flow COMMAND application-flow-tests)
    add_executable(application-update-tests Scaffold/ApplicationUpdateTests.cpp)
    target_link_libraries(application-update-tests PRIVATE Desperabis::GameCore)
    add_test(NAME application-update COMMAND application-update-tests)
    add_executable(fireplace-tests Scaffold/FireplaceTests.cpp)
    target_link_libraries(fireplace-tests PRIVATE behavior-recovery)
    add_test(NAME fireplace-behavior COMMAND fireplace-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Data/SourceAssets/Original/LEVEL")
    add_executable(particle-tests Scaffold/ParticleTests.cpp)
    target_link_libraries(particle-tests PRIVATE behavior-recovery)
    add_test(NAME particle-spawning COMMAND particle-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Data/SourceAssets/Original/LEVEL/OBJPOS25.DAT")
    add_executable(floating-debris-tests Scaffold/FloatingDebrisTests.cpp)
    target_link_libraries(floating-debris-tests PRIVATE behavior-recovery)
    add_test(NAME floating-debris COMMAND floating-debris-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Data/SourceAssets/Original/LEVEL")
    add_executable(rat-snake-tests Scaffold/RatSnakeTests.cpp)
    target_link_libraries(rat-snake-tests PRIVATE behavior-recovery)
    add_test(NAME rat-snake-behavior COMMAND rat-snake-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Data/SourceAssets/Original/LEVEL")
    add_executable(cockroach-tests Scaffold/CockroachTests.cpp)
    target_link_libraries(cockroach-tests PRIVATE behavior-recovery)
    add_test(NAME cockroach-behavior COMMAND cockroach-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Data/SourceAssets/Original/LEVEL"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Game/Original/DESPERAB.EXE")
    add_executable(bat-indoor-tests Scaffold/BatIndoorTests.cpp)
    target_link_libraries(bat-indoor-tests PRIVATE behavior-recovery)
    add_test(NAME bat-indoor-behavior COMMAND bat-indoor-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Data/SourceAssets/Original/LEVEL")
    add_executable(mummy-tests Scaffold/MummyTests.cpp)
    target_link_libraries(mummy-tests PRIVATE behavior-recovery)
    add_test(NAME mummy-behavior COMMAND mummy-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Data/SourceAssets/Original/LEVEL")
    add_executable(skeleton-tests Scaffold/SkeletonTests.cpp)
    target_link_libraries(skeleton-tests PRIVATE behavior-recovery)
    add_test(NAME skeleton-behavior COMMAND skeleton-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Data/SourceAssets/Original/LEVEL")
    add_executable(damage-tests Scaffold/DamageTests.cpp)
    target_link_libraries(damage-tests PRIVATE behavior-recovery)
    add_test(NAME weapon-damage COMMAND damage-tests)
    add_executable(weapon-tests Scaffold/WeaponTests.cpp)
    target_link_libraries(weapon-tests PRIVATE behavior-recovery)
    add_test(NAME weapon-sequences COMMAND weapon-tests)
    add_executable(ranged-weapon-tests Scaffold/RangedWeaponTests.cpp)
    target_link_libraries(ranged-weapon-tests PRIVATE behavior-recovery)
    add_test(NAME ranged-weapons COMMAND ranged-weapon-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Data/SourceAssets/Original/ANIMS")
    add_executable(player-posture-tests Scaffold/PlayerPostureTests.cpp)
    target_link_libraries(player-posture-tests PRIVATE behavior-recovery)
    add_test(NAME player-posture COMMAND player-posture-tests)
    add_executable(player-look-tests Scaffold/PlayerLookTests.cpp)
    target_link_libraries(player-look-tests PRIVATE behavior-recovery)
    add_test(NAME player-look COMMAND player-look-tests)
    add_executable(player-support-tests Scaffold/PlayerSupportTests.cpp)
    target_link_libraries(player-support-tests PRIVATE behavior-recovery)
    add_test(NAME player-support COMMAND player-support-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Data/SourceAssets/Original"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Game/Original/DESPERAB.EXE")
    add_executable(player-health-tests Scaffold/PlayerHealthTests.cpp)
    target_link_libraries(player-health-tests PRIVATE behavior-recovery)
    add_test(NAME player-health COMMAND player-health-tests)
    add_test(NAME behavior-recovery-routes COMMAND behavior-recovery-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Game/Original/DESPERAB.EXE"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Data/SourceAssets/Original/SAMP/S_TAB.PRB")
endif()

foreach(target behavior-recovery desperabis-recovery)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endforeach()
if(BUILD_TESTING)
    foreach(target tutorial-tests ambient-tests behavior-recovery-tests application-flow-tests application-update-tests fireplace-tests particle-tests floating-debris-tests rat-snake-tests cockroach-tests bat-indoor-tests mummy-tests skeleton-tests damage-tests weapon-tests ranged-weapon-tests game-runtime-tests player-posture-tests player-look-tests player-support-tests player-health-tests)
        set_target_properties(${target} PROPERTIES FOLDER "Tests")
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /WX)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
        endif()
    endforeach()
endif()
