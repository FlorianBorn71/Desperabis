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
        if (object.collisionType == CollisionType::CurtainOpens)
        {
            object.collisionMask = 0;
            object.generalPurpose3 = static_cast<std::uint16_t>(object.velocityZ);
            context.PlaySoundEffect(static_cast<SoundEffect>(object.soundEffectId),
                GameContext::GetSoundVolume(static_cast<std::int16_t>(context.Random(300))), object, 1);
            return { InteractionStatus::Applied };
        }
        if (object.collisionType == CollisionType::PushPlayer)
        {
            if (object.behaviorParameter == 0 || context.DEBUG_noPlayerCollision)
            {
                return { InteractionStatus::NoEffect };
            }
            context.ApplyPlayerLifeDelta(-context.playerLife);
            return { InteractionStatus::Applied };
        }
        if (object.collisionType == CollisionType::Crab)
        {
            return context.HandleCrabContact(object);
        }
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
        if (object.collisionType == CollisionType::Devil ||
            object.collisionType == CollisionType::BatIndoor ||
            object.collisionType == CollisionType::Shark ||
            object.collisionType == CollisionType::AnacondaTail ||
            object.collisionType == CollisionType::AnacondaHead)
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
