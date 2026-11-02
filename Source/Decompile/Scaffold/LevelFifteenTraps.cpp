#include "GameContext.h"
#include "GameMath.h"

#include <algorithm>
#include <stdexcept>

namespace Desperabis
{
    void GameContext::ApplyLatchedPlayerContactDamage(RawGameObject& object)
    {
        if (object.generalPurpose2 == 0)
        {
            playerHitEffectTicks = 8;
            ApplyPlayerLifeDelta(-29491 * static_cast<std::int32_t>(object.behaviorParameter));
        }
        object.generalPurpose2 = 1;
    }

    std::uint16_t GameContext::SpawnEmitterProjectile(const RawGameObject& source)
    {
        if (source.generalPurpose1 != 1 && source.generalPurpose1 != 11)
        {
            throw std::invalid_argument("Unrecovered emitter projectile selector.");
        }
        const auto emitter = source;
        const auto slot = FindFreeOrLowestHealthObjectIndex();
        RawGameObject shot;
        shot.health = 1000;
        shot.collisionMask = 1;
        shot.spriteScale = 35;
        shot.behavior = BehaviorType::FlyingArrow;
        shot.collisionType = CollisionType::FlyingArrow;
        shot.posX = emitter.posX; shot.posY = emitter.posY; shot.posZ = emitter.posZ;
        GetClampedDirectionDisplacement(shot.velocityX, shot.velocityZ,
            emitter.winY & 511, emitter.doorLockOrGroupBitmask);
        shot.modelAnim = 1;
        shot.displayType = DisplayType::VectorNormal;
        if (emitter.generalPurpose1 == 1)
        {
            shot.modelId = 31;
            shot.animFrame = 2;
            shot.winY = static_cast<std::int16_t>((emitter.winY & 511) + 256);
            shot.behaviorParameter = 140;
            shot.collisionRadiusXZ = 10;
            shot.collisionRadiusY = 8;
        }
        else
        {
            shot.modelId = emitter.modelId;
            shot.animFrame = 1;
            shot.modelFrame2 = 2;
            shot.renderPlacementMode = 1;
            shot.collisionType = static_cast<CollisionType>(130);
            shot.collisionRadiusXZ = 22;
            shot.collisionRadiusY = 3;
            PlaySoundEffect(static_cast<SoundEffect>(86), GetSoundVolumeAt(shot.posX, shot.posZ), 500, 1);
            shot.generalPurpose1 = static_cast<std::uint16_t>(emitter.velocityY);
            shot.doorLockOrGroupBitmask = emitter.velocityZ;
            shot.itemValue = 11;
            shot.winY = static_cast<std::int16_t>(Random(512));
            shot.generalPurpose3 = static_cast<std::uint16_t>(emitter.velocityX);
        }
        shot.posY = std::max(shot.posY,
            WrapSignedWord(GetTerrainFloorHeight(shot.posX, shot.posY, shot.posZ) + 8));
        ReplaceWorldObject(slot, shot);
        return slot;
    }
}
