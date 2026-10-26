cmake_minimum_required(VERSION 3.20)
project(DesperabisRecovery LANGUAGES CXX)

include(TestBigEndian)
test_big_endian(RECOVERY_BIG_ENDIAN)
if(RECOVERY_BIG_ENDIAN)
    message(FATAL_ERROR "The original DOS object layout requires a little-endian host.")
endif()

add_library(behavior-recovery STATIC
    Scaffold/GameEnums.h
    Scaffold/GameContext.h
    Scaffold/GameContext.cpp
    Scaffold/ApplicationFlow.h
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

add_executable(desperabis-recovery Scaffold/Main.cpp)
target_link_libraries(desperabis-recovery PRIVATE behavior-recovery)

include(CTest)
if(BUILD_TESTING)
    add_executable(behavior-recovery-tests Scaffold/Tests.cpp)
    target_link_libraries(behavior-recovery-tests PRIVATE behavior-recovery)
    add_executable(application-flow-tests Scaffold/ApplicationFlowTests.cpp)
    target_link_libraries(application-flow-tests PRIVATE behavior-recovery)
    add_test(NAME application-flow COMMAND application-flow-tests)
    add_executable(fireplace-tests Scaffold/FireplaceTests.cpp)
    target_link_libraries(fireplace-tests PRIVATE behavior-recovery)
    add_test(NAME fireplace-behavior COMMAND fireplace-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Data/SourceAssets/Original/LEVEL")
    add_executable(particle-tests Scaffold/ParticleTests.cpp)
    target_link_libraries(particle-tests PRIVATE behavior-recovery)
    add_test(NAME particle-spawning COMMAND particle-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Data/SourceAssets/Original/LEVEL/OBJPOS25.DAT")
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
    foreach(target behavior-recovery-tests application-flow-tests fireplace-tests particle-tests)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /WX)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
        endif()
    endforeach()
endif()
