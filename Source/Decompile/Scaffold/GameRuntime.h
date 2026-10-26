#pragma once

#include "GameEnums.h"

#include <array>
#include <cstdint>
#include <optional>

namespace Desperabis
{
    struct PlayerCamera
    {
        // Native DOS eye coordinates; no renderer scaling or additional host eye offset.
        std::array<std::int16_t, 3> position;
        // Native view vector from player velocity words, not Euler angles; not normalized.
        std::array<std::int16_t, 3> direction;
    };

    enum class InteractionStatus
    {
        Applied,
        NoEffect,
        InventoryFull,
        InactiveObject,
        UnimplementedHandler
    };

    struct InteractionResult
    {
        InteractionStatus status;
        std::uint16_t inventorySlot = 0;
    };

    enum class DispatchStatus
    {
        ExecutedCase,
        UnimplementedHandler,
        NoSpecificCase,
        InactiveObject,
        DispatchDisabled
    };

    struct DispatchResult
    {
        DispatchStatus status;
        BehaviorType behavior;
        std::uint16_t moduleOffset;
    };

    enum class GameRuntimeState
    {
        NoLevel,
        Loading,
        Ready,
        LoadFailed
    };

    struct LevelLoadRequest
    {
        std::uint64_t id;
        std::int16_t levelNumber;
    };

    struct GameTickTiming
    {
        std::int16_t behaviorTicks = 1;
        std::int16_t frameTicks = 1;
    };

    enum class GameTickStatus
    {
        NoLevel,
        Loading,
        LoadFailed,
        Paused,
        Advanced
    };

    struct BasicPlayerMovementResult
    {
        std::uint16_t simulatedTicks = 0;
        std::uint16_t blockedTicks = 0; // Proposed X/Z was rolled back after unresolved map collision.
    };

    struct GameTickResult
    {
        GameTickStatus status;
        std::optional<InteractionResult> interaction;
        // Present only when advanced; entries correspond to native slots 2..164.
        std::optional<std::array<DispatchResult, 163>> objects;
        std::optional<BasicPlayerMovementResult> playerMovement = {};
    };
}
