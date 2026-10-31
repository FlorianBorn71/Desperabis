#include "BehaviorDispatch.h"
#include "GameMath.h"

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
        if (object.collisionType == CollisionType::LevelEnd)
        {
            const auto automatic = context.HandleLevelEndContact(object, false);
            if (automatic.status != InteractionStatus::NoEffect) { return automatic; }
        }
        if (object.collisionType == CollisionType::DestroyOnCollide)
        {
            if (!context.behaviorDispatchEnabled) { return { InteractionStatus::NoEffect }; }
            context.DamageObject(object, 0);
            return { InteractionStatus::Applied };
        }
        if (object.collisionType == CollisionType::DamageObj ||
            (object.collisionType == CollisionType::BurnableOilPuddle &&
                static_cast<std::uint8_t>(object.displayType) > 49))
        {
            return context.HandleDamageObjectContact(object);
        }
        if (object.collisionType == CollisionType::TriggerTextPrompt)
        {
            return context.HandleTextPromptContact(object);
        }
        if (object.collisionType == CollisionType::TriggerActivation ||
            object.collisionType == CollisionType::DeadBullHead)
        {
            return context.HandleActivationTriggerContact(object);
        }
        if (object.collisionType == CollisionType::Cockroach)
        {
            return context.HandleCockroachContact(object);
        }
        if (object.collisionType == CollisionType::SpiderLarge ||
            object.collisionType == CollisionType::Harvestman)
        {
            object.velocityX = 0;
            object.velocityZ = 0;
            const auto angle = GetVectorAngle(WrapSignedWord(context.soundListenerX - object.posX),
                WrapSignedWord(context.soundListenerZ - object.posZ));
            object.generalPurpose2 = static_cast<std::uint16_t>(((angle + 256) & 511) - 512);
            object.winY = WrapSignedWord(object.generalPurpose2);
            context.ApplyPlayerLifeDelta(-29491 * object.behaviorParameter);
            context.playerHitEffectTicks = 8;
            return { InteractionStatus::Applied };
        }
        if (object.collisionType == CollisionType::Devil)
        {
            return context.HandleDamageObjectContact(object);
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
