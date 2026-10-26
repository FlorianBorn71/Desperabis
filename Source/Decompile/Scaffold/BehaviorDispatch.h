#pragma once

#include "GameContext.h"

#include <vector>

namespace Desperabis
{
    // Gameplay cases of 0EF0:09F8; call on explicit interaction input, not on contact.
    // The host passes the selected object directly; prompt storage and recovered sound requests are game-side.
    InteractionResult HandlePlayerObjectInteraction(RawGameObject& object, GameContext& context);
    InteractionResult HandleDoorInteraction(RawGameObject& object, GameContext& context);
    InteractionResult UpdatePlayerInteraction(GameContext& context, InputHandler& input);

    struct BehaviorDescriptor
    {
        BehaviorType type;
        const char* name;
        std::uint16_t moduleOffset;
        bool hasSpecificCase;
    };

    const std::vector<BehaviorDescriptor>& BehaviorCatalog();
    const BehaviorDescriptor* FindBehavior(BehaviorType type);
    const BehaviorDescriptor* FindBehavior(std::string_view name);
    DispatchResult DispatchObjectCase(RawGameObject& object, GameContext& context);
    // Recovered shared gameplay prefix plus case dispatch; render-list postprocessing is separate.
    DispatchResult UpdateObject(RawGameObject& object, GameContext& context);
    // Results correspond to original slots 2..164, processed sequentially in place.
    std::array<DispatchResult, 163> UpdateObjects(GameContext& context);

#define BEHAVIOR_CASE(name, entry) DispatchResult Function_##name(RawGameObject&, GameContext&);
#define BEHAVIOR_COMMON(name)
#include "BehaviorCases.inc"
#undef BEHAVIOR_COMMON
#undef BEHAVIOR_CASE
}
