#include "BehaviorDispatch.h"

namespace Desperabis
{
    InteractionResult UpdatePlayerInteraction(GameContext& context, InputHandler& input)
    {
        context.playerCollisionSlot = context.FindPlayerCollision();
        if (context.playerCollisionSlot == 0)
        {
            return { InteractionStatus::NoEffect };
        }
        auto& object = context.worldObjects[context.playerCollisionSlot];
        if (object.collisionType == CollisionType::Door)
        {
            // Player contact keeps a door open even without an interaction-key press.
            object.behaviorParameter = 1;
        }
        if (!input.IsKeyPressed(InputKey::InteractionKey)) { return { InteractionStatus::NoEffect }; }
        return HandlePlayerObjectInteraction(object, context);
    }
}
