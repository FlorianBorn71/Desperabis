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
        if (object.collisionType == CollisionType::TriggerActivation ||
            object.collisionType == CollisionType::DeadBullHead)
        {
            return context.HandleActivationTriggerContact(object);
        }
        if (object.collisionType == CollisionType::Cockroach)
        {
            return context.HandleCockroachContact(object);
        }
        if (object.collisionType == CollisionType::Door)
        {
            // Player contact keeps a door open even without an interaction-key press.
            object.behaviorParameter = 1;
        }
        if (object.collisionType == CollisionType::Teleporter)
        {
            const auto automatic = context.HandleTeleporterContact(object, false);
            if (automatic.status != InteractionStatus::NoEffect) { return automatic; }
        }
        if (!input.IsKeyPressed(InputKey::InteractionKey)) { return { InteractionStatus::NoEffect }; }
        return HandlePlayerObjectInteraction(object, context);
    }
}
