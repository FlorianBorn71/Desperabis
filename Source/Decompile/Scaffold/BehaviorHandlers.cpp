// Initial unrecovered handlers. Edit these bodies as behavior is recovered; do not overwrite them by regenerating.
#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <stdexcept>

namespace Desperabis
{
    namespace
    {
        std::int16_t SineOffset(std::int16_t angle, std::int16_t scale)
        {
            std::int16_t x, z;
            GetClampedDirectionDisplacement(x, z, angle, scale);
            return x;
        }

        const RawGameObject& PreviousSegment(const RawGameObject& object, const GameContext& context)
        {
            const auto slot = object.doorLockOrGroupBitmask;
            if (slot < 0 || static_cast<std::size_t>(slot) >= context.worldObjects.size())
            {
                throw std::out_of_range("Anaconda predecessor is outside slots 0..165.");
            }
            return context.worldObjects[slot];
        }

        void AimSegmentPitch(RawGameObject& object, const RawGameObject& previous)
        {
            const auto dx = WrapSignedWord(previous.posX - object.posX);
            const auto dy = WrapSignedWord(previous.posY - object.posY);
            const auto dz = WrapSignedWord(previous.posZ - object.posZ);
            object.winX = GetVectorAngle(WrapSignedWord(-dy),
                WrapSignedWord(-std::abs(static_cast<int>(dx)) - std::abs(static_cast<int>(dz))));
        }

        void AdvanceTrackedTexture(RawGameObject& object, GameContext& context)
        {
            if (object.behaviorParameter == 0 || !context.level || object.itemValue < 1 ||
                object.itemValue >= static_cast<int>(context.level->trackedTextureFrameCounts.size()))
            {
                throw std::invalid_argument("Tracked texture requires loaded animation metadata and a nonzero period.");
            }
            const int frames = context.level->trackedTextureFrameCounts[object.itemValue];
            if (frames == 0) { throw std::invalid_argument("Tracked texture animation metadata is missing."); }
            if (object.groupIndexOrLock < 1 || object.groupIndexOrLock > 3)
            {
                throw std::out_of_range("Tracked texture requires reserved texture slot 13..15.");
            }
            auto elapsed = WrapSignedWord(object.generalPurpose2);
            if (elapsed >= object.behaviorParameter || context.levelElapsedTicks == 0)
            {
                object.generalPurpose1 = static_cast<std::uint16_t>(object.generalPurpose1 +
                    elapsed / object.behaviorParameter);
                elapsed = static_cast<std::int16_t>(elapsed % object.behaviorParameter);
                object.generalPurpose1 = static_cast<std::uint16_t>(
                    WrapSignedWord(2 * frames + WrapSignedWord(object.generalPurpose1) - 1) % frames + 1);
                context.level->trackedTextureFrames[12 + object.groupIndexOrLock] = object.generalPurpose1;
            }
            object.generalPurpose2 = static_cast<std::uint16_t>(elapsed + context.tickCount);
        }
    }

    DispatchResult Function_BouncingObject(RawGameObject& object, GameContext& context)
    {
        if ((object.modelId < 30 && object.generalPurpose1 != 1 && object.generalPurpose1 != 6 && object.generalPurpose1 != 7) ||
            (object.generalPurpose1 != 0 && object.generalPurpose1 != 1 &&
                object.generalPurpose1 != 5 && object.generalPurpose1 != 6 && object.generalPurpose1 != 7 &&
                object.generalPurpose1 != 8))
        {
            return { DispatchStatus::UnimplementedHandler, BehaviorType::BouncingObject, 0x1167 };
        }
        const auto volume = context.GetSoundVolumeAt(object.posX, object.posZ);
        for (int tick = 0; tick < context.tickCount; ++tick)
        {
            auto floor = context.GetFloorHeight(object.posX, object.posY, object.posZ);
            if (object.modelId < 30)
            {
                floor = WrapSignedWord(floor +
                    ArithmeticShiftRight(WrapSignedWord(context.GetSpriteBottom(object) * object.spriteScale), 6));
            }
            if (object.posY < floor)
            {
                object.posY = floor;
                ++object.generalPurpose2;
                const auto magnitude = WrapSignedWord(std::abs(static_cast<int>(object.velocityY)));
                object.velocityY = ArithmeticShiftRight(WrapSignedWord(magnitude * object.itemValue), 7);
                const auto material = context.GetCollisionMapPixelAtWorldXZ(object.posX, object.posZ) & 7;
                if (material == 1 || material == 3)
                {
                    context.SpawnWaterImpact(object.posX, object.posZ);
                    if (context.floorSurfaceValue != 16)
                    {
                        object.velocityY = 0;
                        object.behavior = BehaviorType::None;
                        context.SetObjectHealth(object, 5);
                    }
                    else
                    {
                        object.pickupType = PickupType::Type_3;
                        object.itemValue = 5;
                        object.collisionRadiusXZ = 22;
                        object.behavior = BehaviorType::SinkingObject;
                    }
                }
                if (object.velocityY < 2)
                {
                    object.behavior = static_cast<BehaviorType>(object.groupIndexOrLock);
                }
                object.velocityX = WrapSignedWord(object.velocityX - SignWord(object.velocityX));
                object.velocityZ = WrapSignedWord(object.velocityZ - SignWord(object.velocityZ));
                if (object.velocityY > 0 && object.soundEffectId != 0)
                {
                    context.PlaySoundEffect(static_cast<SoundEffect>(object.soundEffectId), volume, object, 1);
                }
            }
            else { object.velocityY = WrapSignedWord(object.velocityY - 1); }
            const auto ceiling = WrapSignedWord(context.GetRoomHeight(object.posX, object.posZ) + floor);
            if (object.posY > ceiling)
            {
                ++object.generalPurpose2;
                object.posY = WrapSignedWord(ceiling - 1);
                object.velocityY = 0;
            }
            if (object.generalPurpose1 == 1)
            {
                object.spriteScale = static_cast<std::uint8_t>(
                    context.Random(static_cast<std::uint16_t>(object.health) >> 3) + 6);
                object.spriteScale = std::min<std::uint8_t>(object.spriteScale, 24);
                object.winY = WrapSignedWord(object.winY ^ 1);
                if (context.Random(static_cast<std::uint16_t>(WrapSignedWord(100 - object.health))) < 6)
                {
                    context.lightingCache.AddAtWorldXZ(object.posX, object.posZ,
                        static_cast<std::int16_t>(context.Random(20)));
                }
            }
            else if (object.generalPurpose1 == 6)
            {
                if (WrapSignedWord(object.generalPurpose2) > 0)
                {
                    object.velocityX = ArithmeticShiftRight(object.velocityX, 1);
                    object.velocityZ = ArithmeticShiftRight(object.velocityZ, 1);
                    object.generalPurpose2 = 0;
                }
                const auto speed = WrapSignedWord(std::abs(static_cast<int>(object.velocityX)) +
                    std::abs(static_cast<int>(object.velocityZ)));
                object.winX = static_cast<std::int16_t>(WrapSignedWord(object.winX + speed) & 511);
                const auto target = speed >= 5 ? context.FindCollidingObject(object) : std::uint16_t{ 0 };
                object.soundEffectId = (context.GetCollisionMapPixelAtWorldXZ(object.posX, object.posZ) & 7) == 2 ? 26 : 41;
                if (target > 1)
                {
                    context.DamageObject(context.worldObjects[target], object.behaviorParameter);
                    context.PlaySoundEffect(static_cast<SoundEffect>(41), volume, object, 2);
                    object.velocityX = WrapSignedWord(-SignWord(object.velocityX));
                    object.velocityZ = WrapSignedWord(-SignWord(object.velocityZ));
                    if (object.velocityY > 0) { object.velocityY = 0; }
                }
            }
            else if (object.generalPurpose1 == 5)
            {
                object.winX = ArithmeticShiftRight(WrapSignedWord(object.winX * 3), 2);
                object.winZ = ArithmeticShiftRight(WrapSignedWord(object.winZ * 3), 2);
                if (object.blendWeightOrSpriteOpacity < 0)
                {
                    object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity +
                        GetStepToward(object.blendWeightOrSpriteOpacity, 0,
                            static_cast<std::int16_t>(context.Random(4) + 1)));
                }
            }
            else if (object.generalPurpose1 == 7)
            {
                object.collisionMask = 128;
                const auto slot = context.FindCollidingObject(object);
                object.collisionMask = 0;
                if (slot != 0)
                {
                    auto& target = context.worldObjects[slot];
                    context.PlaySoundEffect(static_cast<SoundEffect>(101), volume, target, 1);
                    object.posY = target.posY;
                    target.generalPurpose1 = static_cast<std::uint16_t>(
                        std::clamp<int>(WrapSignedWord(target.generalPurpose1 + 1), 0, 39));
                    target.animFrame = static_cast<std::uint8_t>(target.generalPurpose1 / 10 + 1);
                    target.displayType = DisplayType::VectorNormal;
                    context.SetObjectHealth(object, 0);
                    break;
                }
            }
            else if (object.generalPurpose1 == 8)
            {
                object.winX = WrapSignedWord(object.winX + GetStepToward(
                    object.winX, 0, WrapSignedWord(object.generalPurpose2 * 8)));
                const auto magnitude = WrapSignedWord(std::abs(static_cast<int>(object.velocityY)));
                object.winY = WrapSignedWord(object.winY +
                    context.RandomSigned(static_cast<std::uint16_t>(WrapSignedWord(magnitude * 6 + 1))));
                object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity +
                    GetStepToward(object.blendWeightOrSpriteOpacity, 100, WrapSignedWord(object.generalPurpose2 * 2)));
            }
            if ((context.GetCollisionMapPixelAtWorldXZ(
                WrapSignedWord(object.posX + object.velocityX), object.posZ) & 8) == 8)
            {
                // Native uses an uninitialized local source ID on this branch; use the actual fragment.
                if (object.soundEffectId != 0)
                {
                    context.PlaySoundEffect(static_cast<SoundEffect>(object.soundEffectId), volume, object, 1);
                }
                ++object.generalPurpose2;
                object.velocityX = WrapSignedWord(-object.velocityX);
            }
            if ((context.GetCollisionMapPixelAtWorldXZ(
                object.posX, WrapSignedWord(object.posZ + object.velocityZ)) & 8) == 8)
            {
                if (object.soundEffectId != 0)
                {
                    context.PlaySoundEffect(static_cast<SoundEffect>(object.soundEffectId), volume, object, 1);
                }
                ++object.generalPurpose2;
                object.velocityZ = WrapSignedWord(-object.velocityZ);
            }
            if (context.objectCollisionsEnabled)
            {
                object.posX = WrapSignedWord(object.posX + object.velocityX);
                object.posY = WrapSignedWord(object.posY + object.velocityY);
                object.posZ = WrapSignedWord(object.posZ + object.velocityZ);
            }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::BouncingObject, 0x1167 };
    }

    DispatchResult Function_WeaponDamageCollider(RawGameObject& object, GameContext& context)
    {
        const auto slot = context.FindCollidingObject(object);
        if (slot > 1)
        {
            context.SetObjectHealth(object, 0);
            auto& target = context.worldObjects[slot];
            std::uint8_t frame = 0;
            switch (target.collisionType)
            {
            case CollisionType::Skeleton:
            case CollisionType::PushingEnemy: frame = 2; break;
            case CollisionType::Devil:
            case CollisionType::SpiderLarge: frame = 4; break;
            case CollisionType::CeilingConcreteSlabStake:
            case CollisionType::DestroyableBarrier: frame = 3; break;
            default: break;
            }
            if (frame != 0)
            {
                context.InitializeDriftingParticle(object, frame, target.posX, target.posZ);
            }
            const auto amount = target.collisionType == CollisionType::Dragon ?
                object.behaviorParameter / 2 : object.behaviorParameter;
            context.DamageObject(target, static_cast<std::int16_t>(amount));
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::WeaponDamageCollider, 0x1198 };
    }

    DispatchResult Function_CrossbowBolt(RawGameObject& object, GameContext& context)
    {
        for (int tick = 0; tick < context.tickCount; ++tick)
        {
            if (object.health == 1000 && static_cast<std::int16_t>(object.behavior) > 0)
            {
                object.posX = WrapSignedWord(object.posX + object.velocityX);
                object.posY = WrapSignedWord(object.posY + object.velocityY);
                object.posZ = WrapSignedWord(object.posZ + object.velocityZ);
                object.winZ = 30;
                object.winY = static_cast<std::int16_t>(WrapSignedWord(object.winY + object.generalPurpose1) & 511);
                object.winX = WrapSignedWord(object.winX - 1);
                object.velocityY = static_cast<std::int16_t>(object.winX / 4);
                const auto floor = context.GetFloorHeight(object.posX, object.posY, object.posZ);
                const auto ceiling = WrapSignedWord(context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ) +
                    context.GetRoomHeight(object.posX, object.posZ));
                if (object.posY > ceiling)
                {
                    object.generalPurpose1 = static_cast<std::uint16_t>(context.RandomSigned(25));
                    object.velocityX = object.velocityY = object.velocityZ = 0;
                    object.posY = ceiling; object.winX = -10;
                }
                if (object.posY < floor) { object.winX = object.velocityY = 0; object.posY = floor; }
                if (object.posY == floor)
                {
                    object.velocityX = WrapSignedWord(object.velocityX - SignWord(object.velocityX));
                    object.velocityZ = WrapSignedWord(object.velocityZ - SignWord(object.velocityZ));
                    if (WrapSignedWord(context.GetTerrainFloorHeight(WrapSignedWord(object.posX + object.velocityX),
                        object.posY, WrapSignedWord(object.posZ + object.velocityZ)) - floor) > 6)
                    {
                        object.velocityX = object.velocityZ = 0; object.animFrame = 3;
                    }
                    const auto material = context.GetCollisionMapPixelAtWorldXZ(object.posX, object.posZ) & 7;
                    if (material == 1 && object.generalPurpose2 == 0) { context.SpawnWaterImpact(object.posX, object.posZ); }
                    else if (material == 3)
                    {
                        object.behavior = BehaviorType::None;
                        context.InitializeSmokeParticle(context.worldObjects[context.FindFreeOrLowestHealthObjectIndex()],
                            object.posX, object.posY, object.posZ, 10, 1, 1);
                        object.health = WrapSignedWord(context.Random(8) + 6);
                        context.PlaySoundEffect(static_cast<SoundEffect>(20),
                            context.GetSoundVolumeAt(object.posX, object.posZ), object, 1);
                        if (object.modelId > 30) { object.displayType = static_cast<DisplayType>(70); }
                        continue;
                    }
                    object.generalPurpose2 = static_cast<std::uint16_t>((object.generalPurpose2 + 1) & 3);
                }
                if ((context.GetCollisionMapPixelAtWorldXZ(WrapSignedWord(object.posX + object.velocityX), object.posZ) & 8) != 0)
                {
                    object.winX = WrapSignedWord(object.winX - 8);
                    object.posX = WrapSignedWord(object.posX - object.velocityX);
                    object.velocityX = WrapSignedWord(-2 * SignWord(object.velocityX));
                    object.velocityZ = WrapSignedWord(object.velocityZ - SignWord(object.velocityZ));
                    object.generalPurpose1 = static_cast<std::uint16_t>(context.RandomSigned(25));
                }
                if ((context.GetCollisionMapPixelAtWorldXZ(object.posX, WrapSignedWord(object.posZ + object.velocityZ)) & 8) != 0)
                {
                    object.winX = WrapSignedWord(object.winX - 8);
                    object.posZ = WrapSignedWord(object.posZ - object.velocityZ);
                    object.velocityZ = WrapSignedWord(-2 * SignWord(object.velocityZ));
                    object.velocityX = WrapSignedWord(object.velocityX - SignWord(object.velocityX));
                    object.generalPurpose1 = static_cast<std::uint16_t>(context.RandomSigned(25));
                }
                const auto slot = object.generalPurpose1 == 0 ? context.FindCollidingObject(object) : std::uint16_t{ 0 };
                if (slot > 1)
                {
                    object.generalPurpose1 = static_cast<std::uint16_t>(context.RandomSigned(10));
                    auto& target = context.worldObjects[slot];
                    if (target.collisionType == CollisionType::Switch && target.blendWeightOrSpriteOpacity == 0)
                    {
                        target.generalPurpose2 = target.generalPurpose1;
                        if (target.itemValue >= 1 && target.health == 1000)
                        {
                            context.ActivateObjectGroup(target.itemValue);
                            if (target.soundEffectId != 0)
                            {
                                context.PlaySoundEffect(static_cast<SoundEffect>(target.soundEffectId),
                                    context.GetSoundVolumeAt(target.posX, target.posZ), target, 1);
                            }
                        }
                    }
                    else if (target.collisionType != CollisionType::Switch && target.collisionType != CollisionType::Teleporter &&
                        target.collisionType != CollisionType::Door && target.collisionType != CollisionType::LevelEnd)
                    {
                        object.generalPurpose1 = slot; object.health = 2000; object.animFrame = 0;
                        object.generalPurpose3 = context.arrowPoison;
                    }
                    object.winX = std::min<std::int16_t>(object.winX, -6);
                    context.DamageObject(target, 49);
                }
                if (object.velocityX == 0 && object.velocityY == 0 && object.velocityZ == 0 && object.posY == floor)
                {
                    object.collisionRadiusXZ = 14; object.collisionRadiusY = 8;
                    object.generalPurpose1 = 1; object.pickupType = PickupType::Type_63; object.behavior = BehaviorType::None;
                    object.collisionMask = 2; object.behaviorParameter = 1;
                    const auto material = context.GetCollisionMapPixelAtWorldXZ(object.posX, object.posZ) & 7;
                    if (material == 1 || material == 3) { object.winX = 256; object.behavior = BehaviorType::FloatingDebris; }
                }
            }
            else if (object.health == 2000)
            {
                if (object.generalPurpose1 == 0 || object.generalPurpose1 >= context.worldObjects.size())
                {
                    throw std::out_of_range("Attached bolt target must be an object slot in 1..165.");
                }
                auto& target = context.worldObjects[object.generalPurpose1];
                object.posX = target.posX; object.posY = target.posY; object.posZ = target.posZ;
                if (object.generalPurpose3 > context.Random(100)) { context.DamageObject(target, 1); }
                if (target.behavior == BehaviorType::None || target.collisionType == CollisionType::None || target.groupIndexOrLock < 1)
                {
                    const auto floor = context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ);
                    if (object.posY < floor) { object.posY = WrapSignedWord(floor + 10); }
                    object.health = 1000; object.animFrame = 2; object.winX = -3;
                    object.velocityX = object.velocityY = object.velocityZ = 0; object.generalPurpose1 = 1;
                }
            }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::CrossbowBolt, 0x1297 };
    }

    DispatchResult Function_FireballProjectile(RawGameObject& object, GameContext& context)
    {
        if (object.groupIndexOrLock <= 11)
        {
            object.groupIndexOrLock = WrapSignedWord(object.groupIndexOrLock + context.tickCount);
            if (object.groupIndexOrLock > 11)
            {
                context.PlaySoundEffect(static_cast<SoundEffect>(34), context.GetSoundVolume(100), object, 1);
            }
            const auto camera = context.GetPlayerCamera();
            object.posX = camera.position[0]; object.posY = WrapSignedWord(camera.position[1] - 20);
            object.posZ = camera.position[2];
            object.velocityX = ArithmeticShiftRight(context.worldObjects[1].velocityX, 2);
            object.velocityZ = ArithmeticShiftRight(context.worldObjects[1].velocityZ, 2);
        }
        else for (int tick = 0; tick < context.tickCount; ++tick)
        {
            if (object.health <= 0) { continue; }
            object.blendWeightOrSpriteOpacity = WrapSignedWord(ArithmeticShiftRight(object.health, 1) + 1);
            object.posX = WrapSignedWord(object.posX + object.velocityX);
            object.posY = WrapSignedWord(object.posY + object.velocityY);
            object.posZ = WrapSignedWord(object.posZ + object.velocityZ);
            const auto strength = std::min(ArithmeticShiftRight(object.health, 1) + 2, 5);
            for (int a = -1; a <= 1; ++a)
            {
                for (int b = -2; b <= 2; ++b)
                {
                    const auto amount = strength - std::abs(a) - std::abs(b);
                    if (amount > 0)
                    {
                        context.lightingCache.AddAtWorldXZ(
                            WrapSignedWord(object.posX + 2 * (object.velocityZ * a + object.velocityX * b)),
                            WrapSignedWord(object.posZ + 2 * (object.velocityX * a + object.velocityZ * b)),
                            static_cast<std::int16_t>(amount));
                    }
                }
            }
            if (WrapSignedWord(object.generalPurpose2) > 0)
            {
                object.generalPurpose2 = 0;
                const auto advanced = WrapSignedWord(object.doorLockOrGroupBitmask + 1);
                // EB28 holds all 2*5 frames of SCHUSS1.AN2, not just one animation row.
                object.doorLockOrGroupBitmask = static_cast<std::int16_t>((advanced + 19) % 10 + 1);
                if (object.doorLockOrGroupBitmask < advanced) { context.SetObjectHealth(object, 0); }
            }
            else { ++object.generalPurpose2; }
            const auto slot = context.FindCollidingObject(object);
            if (slot > 1)
            {
                auto& target = context.worldObjects[slot];
                if (target.displayType != DisplayType::Type9)
                {
                    switch (target.collisionType)
                    {
                    case CollisionType::Skeleton:
                        target.displayType = static_cast<DisplayType>(80);
                        if (target.health < 2000) { target.health = 3250; }
                        break;
                    case CollisionType::PushingEnemy: case CollisionType::SpiderLarge:
                    case CollisionType::CeilingConcreteSlabStake: case CollisionType::DestroyableBarrier:
                    case CollisionType::AnacondaTail: case CollisionType::AnacondaHead:
                    case CollisionType::Dog: case CollisionType::KrakenHead: case CollisionType::AttachableDamageObject:
                        context.DamageObject(target, 100); target.displayType = static_cast<DisplayType>(120); break;
                    case CollisionType::RatAndSnake: case CollisionType::Crab: case CollisionType::BatIndoor:
                    case CollisionType::Harvestman: case CollisionType::DestroyOnCollide: case CollisionType::Cockroach:
                        context.DamageObject(target, 100); target.displayType = static_cast<DisplayType>(80); break;
                    case CollisionType::Dragon: case CollisionType::TriggerDebris: context.DamageObject(target, 200); break;
                    case CollisionType::Devil: case CollisionType::Shark: context.DamageObject(target, 30); break;
                    case CollisionType::BurnableOilPuddle:
                        if (target.displayType == DisplayType::VectorNormal) { target.displayType = static_cast<DisplayType>(120); }
                        break;
                    case CollisionType::Flies:
                        if (target.displayType == DisplayType::VectorNormal) { target.displayType = static_cast<DisplayType>(60); }
                        break;
                    default: break;
                    }
                }
                object.generalPurpose1 = slot;
                context.DamageObject(object, 1);
            }
            const auto floor = context.GetFloorHeight(object.posX, object.posY, object.posZ);
            if ((context.GetCollisionMapPixelAtWorldXZ(object.posX, object.posZ) & 8) != 0 ||
                WrapSignedWord(object.posY - object.collisionRadiusY) < floor ||
                WrapSignedWord(object.posY + object.collisionRadiusY) >
                    WrapSignedWord(context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ) +
                        context.GetRoomHeight(object.posX, object.posZ)))
            {
                context.DamageObject(object, 0);
            }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::FireballProjectile, 0x12C8 };
    }

    DispatchResult Function_FadingParticle(RawGameObject& object, GameContext&)
    {
        object.blendWeightOrSpriteOpacity = ArithmeticShiftRight(object.health, 2);
        return { DispatchStatus::ExecutedCase, BehaviorType::FadingParticle, 0x1369 };
    }

    DispatchResult Function_Behavior_6(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_6, 0x1386 };
    }

    DispatchResult Function_Behavior_7(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_7, 0x1416 };
    }

    DispatchResult Function_Pillar(RawGameObject& object, GameContext& context)
    {
        context.SetCollisionMapPixelAtWorldXZ(
            object.posX, object.posZ, CollisionMapValue_BlockingWall);
        return { DispatchStatus::ExecutedCase, BehaviorType::Pillar, 0x146B };
    }

    DispatchResult Function_LightSource(RawGameObject& object, GameContext& context)
    {
        const auto distance = context.GetSoundDistanceAt(object.posX, object.posZ);
        const auto volume = context.GetSoundVolumeAt(object.posX, object.posZ);
        if (distance >= 0 && distance <= 300)
        {
            context.field_DC34 = static_cast<std::uint16_t>(context.field_DC34 + context.Random(0xefff));
        }
        const auto amount = static_cast<std::int16_t>(context.Random(30) + 5);
        context.lightingCache.AddAtWorldXZ(object.posX, object.posZ, amount);
        const auto soundChance = context.Random(100);
        // Native effect 0 denotes no sample; keep the playback API's invalid-ID checks for other values.
        if (context.tickCount > soundChance && object.soundEffectId != 0)
        {
            context.PlaySoundEffect(static_cast<SoundEffect>(object.soundEffectId), volume, object, 2);
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::LightSource, 0x1488 };
    }

    DispatchResult Function_Behavior_11(RawGameObject& object, GameContext& context)
    {
        object.posY = context.GetFloorHeight(object.posX, object.posY, object.posZ);
        object.animFrame = 3;
        object.modelFrame2 = 0;
        object.posX = WrapSignedWord(object.posX + SignWord(object.velocityX) * context.tickCount);
        object.posZ = WrapSignedWord(object.posZ + SignWord(object.velocityZ) * context.tickCount);
        const auto material = context.GetCollisionMapPixelAtWorldXZ(object.posX, object.posZ) & 7;
        if (material != 1)
        {
            object.animFrame = 1;
            object.modelFrame2 = 2;
            object.behavior = BehaviorType::Frog;
        }
        if (material == 3)
        {
            object.behavior = BehaviorType::None;
            auto& smoke = context.worldObjects[context.FindFreeObjectIndex()];
            context.InitializeSmokeParticle(smoke, object.posX, object.posY, object.posZ, 10, 1, 1);
            context.SetObjectHealth(object, static_cast<std::int16_t>(context.Random(8) + 6));
            context.PlaySoundEffect(static_cast<SoundEffect>(20),
                context.GetSoundVolumeAt(object.posX, object.posZ), object, 1);
            if (object.modelId > 30) { object.displayType = static_cast<DisplayType>(70); }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::Behavior_11, 0x1515 };
    }

    DispatchResult Function_Frog(RawGameObject& object, GameContext& context)
    {
        for (int tick = 0; tick < context.tickCount; ++tick)
        {
            const auto floor = context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ);
            if (object.velocityX == 0 && object.velocityZ == 0)
            {
                GetClampedDirectionDisplacement(object.velocityX, object.velocityZ, object.winY, -6);
            }
            if (object.velocityY != 0)
            {
                object.posX = static_cast<std::int16_t>(WrapSignedWord(object.posX + object.velocityX) & 4095);
                object.posZ = static_cast<std::int16_t>(WrapSignedWord(object.posZ + object.velocityZ) & 4095);
                object.posY = WrapSignedWord(object.posY + object.velocityY);
            }
            if (object.posY <= floor)
            {
                if (object.velocityY != 0)
                {
                    const auto randomDistance = context.Random(300);
                    context.PlaySoundEffect(static_cast<SoundEffect>(26),
                        GameContext::GetSoundVolume(WrapSignedWord(
                            randomDistance + context.GetSoundDistanceAt(object.posX, object.posZ) + 500)), object, 1);
                }
                object.velocityY = 0;
                object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity +
                    GetStepToward(object.blendWeightOrSpriteOpacity, 0,
                        static_cast<std::int16_t>(context.Random(20) + 5)));
                object.posY = floor;
                object.generalPurpose2 = 20;
                const auto material = context.GetCollisionMapPixelAtWorldXZ(object.posX, object.posZ) & 7;
                if (material == 1 || material == 3)
                {
                    context.PlaySoundEffect(static_cast<SoundEffect>(10), 255, object, 1);
                    object.behavior = BehaviorType::Behavior_11;
                }
            }
            else if (object.blendWeightOrSpriteOpacity < 100)
            {
                object.blendWeightOrSpriteOpacity = WrapSignedWord(
                    object.blendWeightOrSpriteOpacity + object.generalPurpose2);
            }
            else { object.blendWeightOrSpriteOpacity = 100; }
            object.generalPurpose2 = static_cast<std::uint16_t>(object.generalPurpose2 + 1);
            if (object.posY > floor) { object.velocityY = WrapSignedWord(object.velocityY - 1); }
            else if (context.Random(50) == 0 && object.blendWeightOrSpriteOpacity == 0)
            {
                object.velocityY = static_cast<std::int16_t>(context.Random(4) + 3);
            }
            if (context.Random(250) == 0 && object.blendWeightOrSpriteOpacity == 0)
            {
                context.PlaySoundEffect(static_cast<SoundEffect>(35),
                    GameContext::GetSoundVolume(WrapSignedWord(
                        context.GetSoundDistanceAt(object.posX, object.posZ) + 200)), object, 1);
            }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::Frog, 0x15C1 };
    }

    DispatchResult Function_RatAndSnake(RawGameObject& object, GameContext& context)
    {
        const auto volume = context.GetSoundVolumeAt(object.posX, object.posZ);
        for (int tick = 0; tick < context.tickCount; ++tick)
        {
            if (object.health == 1000)
            {
                const auto nextX = WrapSignedWord(object.posX + object.velocityX);
                const auto nextZ = WrapSignedWord(object.posZ + object.velocityZ);
                if (context.objectCollisionsEnabled &&
                    (context.GetCollisionMapPixelAtWorldXZ(nextX, nextZ) & 7) != 1)
                {
                    object.posX = nextX;
                    object.posZ = nextZ;
                }
                else
                {
                    object.winY = static_cast<std::int16_t>((object.winY + 16) & 511);
                }
                if (WrapSignedWord(object.generalPurpose1) > 0 && context.Random(object.generalPurpose1) == 0)
                {
                    auto target = GetVectorAngle(WrapSignedWord(context.soundListenerX - object.posX),
                        WrapSignedWord(context.soundListenerZ - object.posZ));
                    NormalizeAnglePair(object.winY, target);
                    object.winY = WrapSignedWord(object.winY + GetStepToward(object.winY, target, 13));
                    GetClampedDirectionDisplacement(object.velocityX, object.velocityZ, object.winY, object.itemValue);
                }
                if (context.Random(20) == 0) { object.velocityX = 0; object.velocityZ = 0; }
                object.posY = context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ);
                if (context.Random(object.generalPurpose3) == 0)
                {
                    object.winY = static_cast<std::int16_t>((object.winY + context.RandomSigned(20)) & 511);
                    GetClampedDirectionDisplacement(object.velocityX, object.velocityZ, object.winY, object.itemValue);
                    object.generalPurpose3 = 15;
                }
                if (object.velocityX != 0 || object.velocityZ != 0)
                {
                    const auto speed = static_cast<std::uint16_t>(object.itemValue);
                    const auto phase = WrapSignedWord(static_cast<std::int32_t>(
                        (2u * speed * speed + static_cast<std::uint16_t>(object.blendWeightOrSpriteOpacity)) & 0xffffu));
                    object.blendWeightOrSpriteOpacity = static_cast<std::int16_t>(phase % 200);
                }
                const auto turn = WrapSignedWord(context.RandomSigned(1) * 64);
                std::int16_t correctionX = 0;
                std::int16_t correctionZ = 0;
                std::uint16_t center = 0;
                int attempts = 0;
                do
                {
                    ++attempts;
                    correctionX = 0;
                    correctionZ = 0;
                    context.QueryCollisionMapNeighborhood(WrapSignedWord(object.posX + object.velocityX),
                        WrapSignedWord(object.posZ + object.velocityZ), WrapSignedWord(object.itemValue + 1),
                        correctionX, correctionZ, center, 0x38);
                    if (correctionX != 0 || correctionZ != 0)
                    {
                        object.winY = WrapSignedWord(object.winY + turn);
                        GetDirectionDisplacement(object.velocityX, object.velocityZ, object.winY, object.itemValue);
                    }
                } while ((correctionX != 0 || correctionZ != 0) && attempts <= 15);
                if (attempts > 15) { context.HandleBlockedObject(object); }
                if (object.modelAnim == 1 && context.Random(300) == 0)
                {
                    context.PlaySoundEffect(SoundEffect::RatAndSnake, volume, object, 1);
                }
            }
            else if (object.health > 1100)
            {
                const auto floor = context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ);
                object.velocityY = WrapSignedWord(object.velocityY - 1);
                object.posY = WrapSignedWord(object.posY + object.velocityY);
                if (object.posY < floor) { object.posY = floor; object.velocityY = 0; }
                object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity + 1);
                if (object.blendWeightOrSpriteOpacity > 0) { object.behavior = BehaviorType::None; }
            }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::RatAndSnake, 0x15EF };
    }

    DispatchResult Function_BatIndoor(RawGameObject& object, GameContext& context)
    {
        for (int tick = 0; tick < context.tickCount; ++tick)
        {
            const auto distance = context.GetSoundDistanceAt(object.posX, object.posZ);
            if (object.health == 1000)
            {
                object.collisionType = CollisionType::BatIndoor;
                if (object.displayType == DisplayType::Hidden) { object.displayType = DisplayType::VectorNormal; }
                if (context.Random(static_cast<std::uint16_t>(WrapSignedWord((distance + 1) * 2))) == 0)
                {
                    context.PlaySoundEffect(static_cast<SoundEffect>(object.soundEffectId),
                        GameContext::GetSoundVolume(WrapSignedWord(distance * 2)), object, 1);
                }
                object.winX = WrapSignedWord(object.winX + GetStepToward(object.winX, 0, 23));
                object.posX = WrapSignedWord(object.posX + object.velocityX);
                object.posZ = WrapSignedWord(object.posZ + object.velocityZ);
                AddFixedPoint8(object.posY, object.doorLockOrGroupBitmask, object.velocityY);
                if (context.Random(20) == 0)
                {
                    if (object.behaviorParameter == 0 || context.Random(3) > 0)
                    {
                        object.velocityY = WrapSignedWord(
                            context.RandomSigned(static_cast<std::uint16_t>(WrapSignedWord(object.itemValue * 100))) +
                            object.itemValue * 15);
                    }
                    else
                    {
                        object.velocityY = GetStepToward(WrapSignedWord(object.posY * 8),
                            WrapSignedWord(context.GetPlayerCamera().position[1] * 8 + context.playerBodyHeight * 4), 600);
                    }
                }
                if (context.Random(10) == 1)
                {
                    object.generalPurpose2 = static_cast<std::uint16_t>(
                        WrapSignedWord(object.generalPurpose2 + context.RandomSigned(10)) & 511);
                }
                const int turn = context.Random(2) == 0 ? -20 : 20;
                const auto floor = context.GetFloorHeight(object.posX, object.posY, object.posZ);
                bool clear = false;
                for (int attempt = 0; attempt < 128; ++attempt)
                {
                    GetClampedDirectionDisplacement(object.velocityX, object.velocityZ,
                        WrapSignedWord(object.generalPurpose2), object.itemValue);
                    if (WrapSignedWord(floor + 15) > object.posY)
                    {
                        object.velocityY = WrapSignedWord(WrapSignedWord(floor + 15 - object.posY) * 150);
                    }
                    clear = true;
                    for (int lookahead = 1; lookahead <= 5; ++lookahead)
                    {
                        if ((context.GetCollisionMapPixelAtWorldXZ(
                            WrapSignedWord(object.posX + 3 * lookahead * object.velocityX),
                            WrapSignedWord(object.posZ + 3 * lookahead * object.velocityZ)) & 0x38) != 0)
                        {
                            clear = false;
                        }
                    }
                    if (clear) { break; }
                    object.generalPurpose2 = static_cast<std::uint16_t>(
                        WrapSignedWord(object.generalPurpose2 + turn) & 511);
                }
                // Native retries forever; 128 turns exhaust every heading reachable in steps of 20.
                if (!clear)
                {
                    context.HandleBlockedObject(object);
                    break;
                }
                if (tick == 0)
                {
                    object.blendWeightOrSpriteOpacity = static_cast<std::int16_t>(
                        WrapSignedWord(object.blendWeightOrSpriteOpacity + object.generalPurpose1) % 200);
                }
                const auto ceiling = WrapSignedWord(context.GetRoomHeight(object.posX, object.posZ) +
                    context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ));
                const auto maximumHeight = WrapSignedWord(floor + 10000);
                if (maximumHeight < object.posY) { object.velocityY = 0; object.posY = maximumHeight; }
                if (WrapSignedWord(ceiling - 20) < object.posY)
                {
                    if (distance > 350)
                    {
                        object.posY = ceiling;
                        object.blendWeightOrSpriteOpacity = 0;
                        object.health = 3000;
                    }
                    else { object.velocityY = -400; }
                }
                const auto heading = WrapSignedWord(object.generalPurpose2);
                const auto difference = WrapSignedWord(heading - object.winY);
                const auto magnitude = WrapSignedWord(std::abs(static_cast<int>(difference)));
                const auto target = magnitude < 256 ? heading : WrapSignedWord(heading - 512);
                object.winY = WrapSignedWord(object.winY + GetStepToward(object.winY, target, 18));
            }
            else if (object.health == 2000)
            {
                const auto floor = context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ);
                object.velocityY = WrapSignedWord(object.velocityY - 1);
                object.posY = WrapSignedWord(object.posY + object.velocityY);
                object.winX = WrapSignedWord(object.winX + GetStepToward(object.winX, 256, 23));
                if (object.posY < floor)
                {
                    object.posY = floor;
                    object.velocityY = 0;
                    if (object.blendWeightOrSpriteOpacity >= 0) { object.behavior = BehaviorType::None; }
                }
                if (object.blendWeightOrSpriteOpacity < 0 && object.winX == 256)
                {
                    object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity + 1);
                }
            }
            else if (object.health == 3000)
            {
                object.modelFrame2 = 3;
                object.winX = WrapSignedWord(object.winX + GetStepToward(object.winX, 128, 23));
                if (object.blendWeightOrSpriteOpacity < 100)
                {
                    object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity + 9);
                }
                if (distance < 300) { object.health = 1000; object.modelFrame2 = 2; }
            }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::BatIndoor, 0x161D };
    }

    DispatchResult Function_IdleAnimation(RawGameObject& object, GameContext& state)
    {
        const auto rateBits = static_cast<std::uint16_t>(object.itemValue);
        const auto phaseBits = static_cast<std::uint16_t>(object.blendWeightOrSpriteOpacity);
        const auto sumBits = static_cast<std::uint16_t>(
            static_cast<std::uint32_t>(rateBits) * static_cast<std::uint16_t>(state.tickCount) +
            phaseBits + 200u);
        const auto sum = WrapSignedWord(sumBits);
        object.blendWeightOrSpriteOpacity = static_cast<std::int16_t>(sum % 200);
        return { DispatchStatus::ExecutedCase, BehaviorType::IdleAnimation, 0x1666 };
    }

    DispatchResult Function_Flies(RawGameObject& object, GameContext& context)
    {
        // Burned flies disappear but still finish this update's animation and sound request.
        if (object.displayType == DisplayType::VectorOilTint) { context.SetObjectHealth(object, 0); }

        const auto rate = static_cast<std::int32_t>(object.itemValue) + context.Random(5);
        object.blendWeightOrSpriteOpacity = WrapSignedWord(
            object.blendWeightOrSpriteOpacity + rate * context.tickCount);

        // The original angle changes happen once per update, even when no ticks have elapsed.
        object.winX = static_cast<std::int16_t>((static_cast<std::uint16_t>(object.winX) + 5u) & 511u);
        object.winY = static_cast<std::int16_t>((object.winX + 3) & 511);
        object.winZ = static_cast<std::int16_t>((object.winX + 7) & 511);

        while (object.blendWeightOrSpriteOpacity > 100)
        {
            object.blendWeightOrSpriteOpacity -= 100;
            object.animFrame = object.modelFrame2;
            // Repeated crossings choose a new frame each time; adjacent frames must differ.
            do
            {
                object.modelFrame2 = static_cast<std::uint8_t>(context.Random(3) + 1);
            } while (object.modelFrame2 == object.animFrame);
        }
        context.PlaySoundEffect(SoundEffect::Flies,
            context.GetSoundVolumeAt(object.posX, object.posZ, 2), object, 1);
        return { DispatchStatus::ExecutedCase, BehaviorType::Flies, 0x1690 };
    }

    DispatchResult Function_BatOutdoor(RawGameObject& object, GameContext& context)
    {
        for (int tick = 0; tick < context.tickCount; ++tick)
        {
            if (object.health != 1000) { continue; }
            if (context.field_D500)
            {
                object.posX = WrapSignedWord(object.posX + object.velocityX);
                object.posY = WrapSignedWord(object.posY + object.velocityY);
                object.posZ = WrapSignedWord(object.posZ + object.velocityZ);
            }
            if (tick == 0)
            {
                object.blendWeightOrSpriteOpacity = static_cast<std::int16_t>(
                    WrapSignedWord(object.blendWeightOrSpriteOpacity + 41) % 200);
            }
            object.generalPurpose2 = static_cast<std::uint16_t>(
                WrapSignedWord(object.generalPurpose2 + object.generalPurpose1) % 5120);
            // CS:5BCD is the stored extended-precision angle step; 164C stores a Real48 temporary.
            const auto angle = RoundReal48(WrapSignedWord(object.generalPurpose2) * 0.0012271846484375);
            object.velocityX = WrapSignedWord(static_cast<std::int32_t>(std::nearbyint(
                object.itemValue * std::sin(angle * 3) + std::cos(angle * 8))));
            object.velocityY = WrapSignedWord(static_cast<std::int32_t>(
                std::nearbyint(object.itemValue * std::cos(angle * 5))));
            object.velocityZ = WrapSignedWord(static_cast<std::int32_t>(
                std::nearbyint(object.itemValue * std::sin(angle * 7))));
            object.winY = GetVectorAngle(object.velocityX, object.velocityZ);
            object.winX = WrapSignedWord(-object.velocityY * 6);
            const auto floor = context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ);
            object.posY = std::max(object.posY, WrapSignedWord(floor + 90));
            object.posY = std::min(object.posY, WrapSignedWord(floor + 200));
            if (object.doorLockOrGroupBitmask == 1)
            {
                context.soundListenerX = WrapSignedWord(object.posX - 100);
                context.soundListenerZ = WrapSignedWord(object.posZ - 100);
                auto& player = context.worldObjects[1];
                player.posX = context.soundListenerX;
                player.posY = WrapSignedWord(object.posY - player.collisionRadiusY - 2);
                player.posZ = context.soundListenerZ;
            }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::BatOutdoor, 0x16A3 };
    }

    DispatchResult Function_DriftingParticle(RawGameObject& object, GameContext& context)
    {
        // This particle uses whole-unit movement per tick, not the fire particle's fixed-point motion.
        for (int tick = 0; tick < context.tickCount; ++tick)
        {
            object.posX = WrapSignedWord(object.posX + object.velocityX);
            object.posY = WrapSignedWord(object.posY + object.velocityY);
            object.posZ = WrapSignedWord(object.posZ + object.velocityZ);
            if ((static_cast<std::uint16_t>(object.itemValue) & 1u) != 0)
            {
                object.velocityY = WrapSignedWord(object.velocityY - 1);
            }
            object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity - 5);
            if (object.blendWeightOrSpriteOpacity < 0) { context.SetObjectHealth(object, 0); }
            object.itemValue = WrapSignedWord(static_cast<std::uint16_t>(object.itemValue) ^ 1u);
            const auto blend = static_cast<int>(object.blendWeightOrSpriteOpacity);
            const auto shade = 37 - (blend >= 0 ? blend / 8 : -((-blend + 7) / 8));
            object.displayType = static_cast<DisplayType>(std::clamp(shade, 30, 36));
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::DriftingParticle, 0x16E5 };
    }

    DispatchResult Function_FireFly(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::FireFly, 0x17A6 };
    }

    DispatchResult Function_FireParticle(RawGameObject& object, GameContext& context)
    {
        // itemValue holds fractional Y; generalPurpose1 holds angular velocity for these particles.
        AddFixedPoint8(object.posY, object.itemValue,
            WrapSignedWord(static_cast<std::int32_t>(object.velocityY) * context.tickCount));
        const auto fadeStep = WrapSignedWord(context.tickCount * context.Random(3));
        object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity +
            GetStepToward(object.blendWeightOrSpriteOpacity, 0, fadeStep));
        object.velocityY = WrapSignedWord(object.velocityY +
            static_cast<std::int32_t>(object.doorLockOrGroupBitmask) * context.tickCount);
        object.winY = WrapSignedWord(object.winY +
            static_cast<std::int32_t>(object.generalPurpose1) * context.tickCount);
        const auto quarterHealth = object.health >= 0 ? object.health / 4 :
            -((-static_cast<int>(object.health) + 3) / 4);
        object.displayType = static_cast<DisplayType>(std::clamp(37 - quarterHealth, 30, 36));
        return { DispatchStatus::ExecutedCase, BehaviorType::FireParticle, 0x181E };
    }

    DispatchResult Function_BurningFireplace(RawGameObject& object, GameContext& context)
    {
        // Intensity controls sound and emission, not the weighted flickering-light footprint.
        const auto soundChance = context.Random(20);
        if (object.doorLockOrGroupBitmask >= soundChance)
        {
            context.PlaySoundEffect(SoundEffect::Burning,
                context.GetSoundVolumeAt(object.posX, object.posZ, 2), object, -1);
        }
        for (int offsetX = -2; offsetX <= 2; ++offsetX)
        {
            for (int offsetZ = -2; offsetZ <= 2; ++offsetZ)
            {
                const auto weight = 5 - std::abs(offsetX) - std::abs(offsetZ);
                const auto amount = static_cast<std::int16_t>(context.Random(30) * weight * weight / 8);
                context.lightingCache.AddAtWorldXZ(
                    WrapSignedWord(object.posX + offsetX * 40),
                    WrapSignedWord(object.posZ + offsetZ * 40), amount);
            }
        }
        const auto spawnChance = context.Random(100);
        // The native code attempts at most one spawn per update, using a signed low-word product.
        const auto spawnRate = WrapSignedWord(
            static_cast<std::int32_t>(object.doorLockOrGroupBitmask) * context.tickCount);
        if (spawnRate > spawnChance) { context.SpawnFireParticle(object); }
        return { DispatchStatus::ExecutedCase, BehaviorType::BurningFireplace, 0x18C6 };
    }

    DispatchResult Function_PulsatingLight(RawGameObject& object, GameContext& context)
    {
        object.generalPurpose2 = static_cast<std::uint16_t>((object.generalPurpose2 +
            static_cast<std::int32_t>(object.velocityZ) * context.tickCount) & 511);
        if (context.Random(15) == 0)
        {
            object.velocityX = static_cast<std::int16_t>(context.Random(10) + 8);
        }
        object.velocityZ = WrapSignedWord(object.velocityZ +
            GetStepToward(object.velocityZ, object.velocityX, context.tickCount));

        const auto sine = Sine(static_cast<std::int16_t>(object.generalPurpose2));
        // Bits 16..31 preserve the native downward rounding for negative fixed-point products.
        const auto pulse = WrapSignedWord(static_cast<std::int32_t>(
            static_cast<std::uint32_t>(750 * sine) >> 16));
        object.generalPurpose3 = static_cast<std::uint16_t>(800 + pulse);
        const auto lightPulse = WrapSignedWord(static_cast<std::int32_t>(
            static_cast<std::uint32_t>(25 * sine) >> 16));
        context.lightingCache.AddAtWorldXZ(object.posX, object.posZ,
            static_cast<std::int16_t>(30 - lightPulse));
        return { DispatchStatus::ExecutedCase, BehaviorType::PulsatingLight, 0x19CD };
    }

    DispatchResult Function_SmokeFading(RawGameObject& object, GameContext& state)
    {
        const auto motion = static_cast<std::uint16_t>(
            static_cast<std::uint32_t>(static_cast<std::uint16_t>(object.velocityY)) *
            static_cast<std::uint16_t>(state.tickCount));
        AddFixedPoint8(object.posY, object.itemValue, WrapSignedWord(motion));

        const auto opacity = object.health >= 0 ? object.health / 4 :
            -((-static_cast<std::int32_t>(object.health) + 3) / 4);
        object.blendWeightOrSpriteOpacity = static_cast<std::int16_t>(opacity > 5 ? 5 : opacity);

        if (object.spriteScale < 200)
        {
            const auto growth = static_cast<std::uint8_t>(
                static_cast<std::uint32_t>(static_cast<std::uint16_t>(object.velocityZ)) *
                static_cast<std::uint16_t>(state.tickCount));
            object.spriteScale = static_cast<std::uint8_t>(object.spriteScale + growth);
        }

        const auto step = WrapSignedWord(static_cast<std::int32_t>(state.tickCount) * 2);
        object.velocityY = WrapSignedWord(static_cast<std::int32_t>(object.velocityY) +
            GetStepToward(object.velocityY, 0, step));
        return { DispatchStatus::ExecutedCase, BehaviorType::SmokeFading, 0x1A9D };
    }

    DispatchResult Function_Behavior_25(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_25, 0x1B24 };
    }

    DispatchResult Function_DestroyableBarrier(RawGameObject& object, GameContext& context)
    {
        object.generalPurpose2 = static_cast<std::uint16_t>(object.generalPurpose2 + context.tickCount * 100);
        if (object.itemValue >= WrapSignedWord(object.generalPurpose1))
        {
            object.itemValue = WrapSignedWord(object.generalPurpose1);
            object.generalPurpose2 = 0;
        }
        object.itemValue = WrapSignedWord(object.itemValue + object.generalPurpose2 * context.tickCount);
        object.blendWeightOrSpriteOpacity = static_cast<std::uint16_t>(object.itemValue) >> 8;
        const bool destroyed = object.blendWeightOrSpriteOpacity >= 100;
        if (destroyed)
        {
            object.blendWeightOrSpriteOpacity = 100;
            object.behavior = BehaviorType::None;
        }
        context.SetBarrierCollisionMapAtWorldXZ(object.posX, object.posZ, object.winY, destroyed ? 2 : 16);
        return { DispatchStatus::ExecutedCase, BehaviorType::DestroyableBarrier, 0x1B64 };
    }

    DispatchResult Function_VerticalConcreteSlab(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::VerticalConcreteSlab, 0x1B77 };
    }

    DispatchResult Function_Behavior_28(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_28, 0x1B8A };
    }

    DispatchResult Function_RollingRock(RawGameObject& object, GameContext& context)
    {
        for (int tick = 0; tick < context.tickCount; ++tick)
        {
            if (object.health == 1000)
            {
                object.collisionType = static_cast<CollisionType>(9);
                const auto x = object.itemValue;
                const auto z = WrapSignedWord(object.generalPurpose1);
                object.winX = static_cast<std::int16_t>(
                    WrapSignedWord(object.winX - ArithmeticShiftRight(x, 7)) & 511);
                object.winZ = static_cast<std::int16_t>(
                    WrapSignedWord(object.winZ - ArithmeticShiftRight(z, 7)) & 511);
                object.posY = WrapSignedWord(object.posY + object.velocityY / 2);
                object.velocityY = WrapSignedWord(object.velocityY - 1);
                AddFixedPoint8(object.posX, object.velocityX, x);
                AddFixedPoint8(object.posZ, object.velocityZ, z);
                if ((context.GetCollisionMapPixelAtWorldXZ(
                    WrapSignedWord(object.posX + SignWord(x) * object.collisionRadiusXZ),
                    WrapSignedWord(object.posZ + SignWord(z) * object.collisionRadiusXZ)) & 8) == 8)
                {
                    context.SetObjectHealth(object, 2000);
                    object.collisionMask = 0;
                    object.collisionType = CollisionType::None;
                }
                const auto slot = context.FindCollidingObject(object);
                if (slot != 0 && object.behaviorParameter > 0) { context.DamageObject(context.worldObjects[slot], 100); }
                const auto floor = WrapSignedWord(
                    context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ) + object.collisionRadiusY);
                if (object.posY < floor)
                {
                    object.posY = floor;
                    object.velocityY = static_cast<std::int16_t>(
                        WrapSignedWord(std::abs(static_cast<int>(object.velocityY))) / 2);
                    if (object.velocityY > 2)
                    {
                        context.PlaySoundEffect(static_cast<SoundEffect>(37), 255, object, 2);
                    }
                }
            }
            else if (object.health == 2000)
            {
                object.velocityY = WrapSignedWord(object.velocityY - 1);
                object.posY = WrapSignedWord(object.posY + object.velocityY);
                const auto floor = WrapSignedWord(
                    context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ) + object.collisionRadiusY);
                if (object.posY < floor)
                {
                    object.posY = floor;
                    object.velocityY = 0;
                    object.renderPlacementMode = 0;
                    if (context.FindCollidingObject(object) == 0) { object.behavior = BehaviorType::Pillar; }
                }
            }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::RollingRock, 0x1BAD };
    }

    DispatchResult Function_Skeleton(RawGameObject& object, GameContext& context)
    {
        const auto distance = context.GetSoundDistanceAt(object.posX, object.posZ);
        const auto volume = context.GetSoundVolumeAt(object.posX, object.posZ);
        const auto footstepVolume = GameContext::GetSoundVolume(WrapSignedWord(distance * 3));
        for (int tick = 0; tick < context.tickCount; ++tick)
        {
            if (object.health == 1000)
            {
                object.posY = WrapSignedWord(context.GetTerrainFloorHeight(
                    object.posX, object.posY, object.posZ) + object.collisionRadiusY);
                if (object.itemValue == 0)
                {
                    object.posX = WrapSignedWord(object.posX + object.velocityX);
                    object.posZ = WrapSignedWord(object.posZ + object.velocityZ);
                }
                const auto phase = WrapSignedWord(object.animFrame * 100 + object.blendWeightOrSpriteOpacity);
                if ((distance > 37 && distance < 61 && object.itemValue == 0) ||
                    object.doorLockOrGroupBitmask > 0)
                {
                    object.blendWeightOrSpriteOpacity = 0;
                    object.itemValue = static_cast<std::int16_t>(context.Random(2) + 4);
                    object.generalPurpose2 = 0;
                    object.animFrame = 1;
                    object.modelFrame2 = 2;
                }
                auto target = object.velocityX != 0 || object.velocityZ != 0 ?
                    GetVectorAngle(object.velocityX, object.velocityZ) : object.winY;
                const auto difference = WrapSignedWord(phase - 250);
                const auto magnitude = WrapSignedWord(std::abs(static_cast<int>(difference)));
                target = WrapSignedWord(target - WrapSignedWord(magnitude - 125) / 6);
                NormalizeAnglePair(object.winY, target);
                object.winY = WrapSignedWord(object.winY + GetStepToward(
                    object.winY, target, static_cast<std::int16_t>(context.Random(8) + 11)));
                object.modelAnim = static_cast<std::uint8_t>(
                    ((object.winY + context.GetPlayerHeading() + 85) & 511) / 171 + 1);
                if (object.itemValue > 0) { object.modelAnim = static_cast<std::uint8_t>(object.itemValue); }
                object.doorLockOrGroupBitmask = 0;
                object.generalPurpose2 = static_cast<std::uint16_t>(object.generalPurpose2 + context.Random(5));
                if (object.itemValue == 0)
                {
                    object.blendWeightOrSpriteOpacity = WrapSignedWord(
                        object.blendWeightOrSpriteOpacity + context.Random(8) + 10);
                }
                else
                {
                    if (object.animFrame == 4)
                    {
                        object.generalPurpose2 = static_cast<std::uint16_t>(object.generalPurpose2 +
                            GetStepToward(WrapSignedWord(object.generalPurpose2), 1, 3));
                    }
                    object.blendWeightOrSpriteOpacity = WrapSignedWord(
                        object.blendWeightOrSpriteOpacity + WrapSignedWord(object.generalPurpose2));
                    target = GetVectorAngle(WrapSignedWord(context.soundListenerX - object.posX),
                        WrapSignedWord(context.soundListenerZ - object.posZ));
                    target = WrapSignedWord(target + WrapSignedWord(phase - 350) / 3);
                    NormalizeAnglePair(object.winY, target);
                    object.winY = WrapSignedWord(object.winY + GetStepToward(object.winY, target, 19));
                }
                bool advanced = false;
                while (object.blendWeightOrSpriteOpacity > 100)
                {
                    advanced = true;
                    object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity - 100);
                    object.animFrame = static_cast<std::uint8_t>((object.animFrame & 3) + 1);
                    object.modelFrame2 = static_cast<std::uint8_t>((object.animFrame & 3) + 1);
                }
                if (std::abs(object.posX % 100 - 50) / 2 < 2 &&
                    std::abs(object.posZ % 100 - 50) / 2 < 2)
                {
                    const auto first = context.Random(4);
                    const auto dx = WrapSignedWord(context.soundListenerX - object.posX);
                    const auto dz = WrapSignedWord(context.soundListenerZ - object.posZ);
                    std::int16_t x = 0;
                    std::int16_t z = 0;
                    if (!context.positionalSoundsSuppressed)
                    {
                        if (WrapSignedWord(std::abs(static_cast<int>(dx))) >
                            WrapSignedWord(std::abs(static_cast<int>(dz)))) { x = SignWord(dx); }
                        else { z = SignWord(dz); }
                    }
                    const auto clear = [&](std::int16_t stepX, std::int16_t stepZ)
                    {
                        bool result = true;
                        for (int step = 1; step <= 3; ++step)
                        {
                            if ((context.GetCollisionMapPixelAtWorldXZ(
                                WrapSignedWord(object.posX + stepX * step * 25),
                                WrapSignedWord(object.posZ + stepZ * step * 25)) & 0x38) != 0)
                            {
                                result = false;
                            }
                        }
                        return result;
                    };
                    bool canMove = clear(x, z);
                    if (!canMove || context.Random(8) == 0)
                    {
                        constexpr std::int16_t directions[4][2] = { {0,1}, {1,0}, {0,-1}, {-1,0} };
                        for (int index = first; index <= first + 3; ++index)
                        {
                            const auto& direction = directions[index & 3];
                            if (clear(direction[0], direction[1]))
                            {
                                x = direction[0];
                                z = direction[1];
                                canMove = true;
                            }
                        }
                    }
                    object.velocityX = canMove ? static_cast<std::int16_t>(x * 2) : 0;
                    object.velocityZ = canMove ? static_cast<std::int16_t>(z * 2) : 0;
                }
                if (advanced && (object.itemValue == 0 || object.animFrame != 4)) { object.generalPurpose2 = 3; }
                if (advanced && object.itemValue == 0 && (object.animFrame & 1) != 0)
                {
                    context.PlaySoundEffect(static_cast<SoundEffect>(106),
                        footstepVolume, object, 1);
                }
                if (object.itemValue > 0 && advanced)
                {
                    if (object.animFrame == 3)
                    {
                        context.PlaySoundEffect(static_cast<SoundEffect>(19), volume, object, 1);
                    }
                    else if (object.animFrame == 4 && distance < 72)
                    {
                        if (context.selectedWeapon == WeaponType::Sword &&
                            context.weaponAnimationPosition > 29 && context.weaponAnimationPosition < 41)
                        {
                            context.weaponAnimationSpeed = -4;
                            context.PlaySoundEffect(static_cast<SoundEffect>(39), GameContext::GetSoundVolume(0), object, 2);
                        }
                        else
                        {
                            context.playerHitEffectTicks = 5;
                            context.ApplyPlayerLifeDelta(-29491 * object.behaviorParameter);
                        }
                    }
                    else if (object.animFrame == 1) { object.itemValue = 0; }
                }
            }
            else if (object.health >= 3000 && object.health <= 3050)
            {
                context.DamageSkeleton(object, 1000);
            }
            else if (object.health >= 3050 && object.health <= 3900)
            {
                object.health = WrapSignedWord(object.health - 1);
            }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::Skeleton, 0x1BE9 };
    }

    DispatchResult Function_Cockroach(RawGameObject& object, GameContext& context)
    {
        constexpr std::int16_t speed = 3; // Original initialized word DS:02D4.
        for (int tick = 0; tick < context.tickCount; ++tick)
        {
            if (object.health != 1000) { continue; }
            object.soundEffectId = 3;
            object.posX = WrapSignedWord(object.posX + object.velocityX);
            object.posZ = WrapSignedWord(object.posZ + object.velocityZ);
            object.posY = WrapSignedWord(object.posY + object.velocityY);
            switch (object.itemValue)
            {
            case -1:
                if (context.GetSoundDistanceAt(object.posX, object.posZ) < 280) { object.itemValue = 15; }
                break;
            case 0:
                if (context.GetSoundDistanceAt(object.posX, object.posZ) < 150) { object.itemValue = 1; }
                break;
            case 1:
                if (context.Random(30) == 0) { object.itemValue = 2; }
                break;
            case 2:
            {
                object.animFrame = 3;
                object.modelFrame2 = 4;
                object.blendWeightOrSpriteOpacity = -100;
                if (object.doorLockOrGroupBitmask == 0) { object.doorLockOrGroupBitmask = 1; }
                if (object.velocityY > -5) { object.velocityY = WrapSignedWord(object.velocityY - 1); }
                const auto floor = context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ);
                if (object.posY < floor)
                {
                    object.posY = floor;
                    object.velocityY = 0;
                    object.itemValue = 3;
                }
                object.winZ = WrapSignedWord(object.winZ + GetStepToward(object.winZ, 150, 5));
                break;
            }
            case 3:
                object.collisionType = CollisionType::Cockroach;
                if (object.generalPurpose2 == 0 && context.Random(20) == 0 &&
                    object.blendWeightOrSpriteOpacity == -100)
                {
                    object.generalPurpose2 = static_cast<std::uint16_t>(context.RandomSigned(15));
                }
                else
                {
                    object.generalPurpose2 = static_cast<std::uint16_t>(WrapSignedWord(
                        object.generalPurpose2 - SignWord(WrapSignedWord(object.generalPurpose2))));
                }
                object.winY = WrapSignedWord(object.winY + object.generalPurpose2);
                if (context.Random(40) == 0) { object.itemValue = 4; }
                if (context.Random(15) == 0) { object.blendWeightOrSpriteOpacity = -100; }
                if (context.Random(15) == 0)
                {
                    object.blendWeightOrSpriteOpacity = static_cast<std::int16_t>(context.Random(100));
                }
                break;
            case 4:
                object.blendWeightOrSpriteOpacity = -100;
                object.animFrame = 1;
                object.modelFrame2 = 2;
                object.winZ = WrapSignedWord(object.winZ + GetStepToward(object.winZ, 0, 9));
                if (object.winZ == 0) { object.itemValue = 5; }
                break;
            case 5:
            case 15:
            {
                if (object.groupIndexOrLock < 1) { context.DamageObject(object, 1); }
                if (context.Random(20) == 0)
                {
                    object.blendWeightOrSpriteOpacity = 0;
                    object.velocityX = 0;
                    object.velocityZ = 0;
                }
                object.posY = context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ);
                object.winZ = 0;
                if (context.Random(15) == 0)
                {
                    object.winY = static_cast<std::int16_t>((object.winY + context.RandomSigned(20)) & 511);
                    GetClampedDirectionDisplacement(object.velocityX, object.velocityZ, object.winY, speed);
                    object.blendWeightOrSpriteOpacity = -100;
                }
                if (WrapSignedWord(object.generalPurpose1) > 0 && context.Random(object.generalPurpose1) == 0)
                {
                    auto target = GetVectorAngle(WrapSignedWord(context.soundListenerX - object.posX),
                        WrapSignedWord(context.soundListenerZ - object.posZ));
                    NormalizeAnglePair(object.winY, target);
                    object.winY = WrapSignedWord(object.winY + GetStepToward(object.winY, target, 14));
                    GetClampedDirectionDisplacement(object.velocityX, object.velocityZ, object.winY, speed);
                }
                const auto turn = WrapSignedWord(context.RandomSigned(1) * 64);
                std::int16_t correctionX = 0;
                std::int16_t correctionZ = 0;
                std::uint16_t center = 0;
                int attempts = 0;
                do
                {
                    ++attempts;
                    correctionX = 0;
                    correctionZ = 0;
                    context.QueryCollisionMapNeighborhood(WrapSignedWord(object.posX + object.velocityX),
                        WrapSignedWord(object.posZ + object.velocityZ), 5, correctionX, correctionZ, center, 0x38);
                    if (correctionX != 0 || correctionZ != 0)
                    {
                        object.winY = WrapSignedWord(object.winY + turn);
                        GetDirectionDisplacement(object.velocityX, object.velocityZ, object.winY, speed);
                    }
                } while ((correctionX != 0 || correctionZ != 0) && attempts <= 15);
                if (attempts > 15) { context.HandleBlockedObject(object); }
                if (object.velocityX != 0 || object.velocityZ != 0)
                {
                    const auto floor = context.GetTerrainFloorHeight(
                        WrapSignedWord(object.posX + 8 * object.velocityX), object.posY,
                        WrapSignedWord(object.posZ + 8 * object.velocityZ));
                    object.winX = GetVectorAngle(WrapSignedWord(object.posY - floor), -8 * speed);
                }
                break;
            }
            case 7:
                if (object.collisionMask < 131) { object.collisionMask = 0; }
                object.velocityX = 0;
                object.velocityZ = 0;
                object.blendWeightOrSpriteOpacity = 0;
                object.itemValue = 8;
                object.animFrame = 1;
                object.modelFrame2 = 2;
                object.modelAnim = 2;
                break;
            case 8:
                if (object.blendWeightOrSpriteOpacity < 100 && context.Random(7) == 0)
                {
                    object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity + 1);
                }
                break;
            case 10:
                if (context.Random(900) == 0)
                {
                    object.displayType = DisplayType::VectorNormal;
                    object.winY = static_cast<std::int16_t>(context.Random(512));
                    object.itemValue = 11;
                    object.collisionMask = 3;
                }
                break;
            case 11:
                if (context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ) > object.posY)
                {
                    object.posY = WrapSignedWord(object.posY + 1);
                }
                else { object.itemValue = 15; }
                break;
            case 16:
                object.collisionType = CollisionType::None;
                object.collisionMask = 0;
                object.animFrame = 4;
                object.modelFrame2 = 3;
                object.velocityX = 0;
                object.velocityY = 0;
                object.velocityZ = 0;
                object.winZ = WrapSignedWord(object.winZ + GetStepToward(object.winZ, 256, 17));
                if (object.blendWeightOrSpriteOpacity < 0)
                {
                    object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity + 1);
                }
                else if (object.winZ == 256) { object.behavior = BehaviorType::None; }
                break;
            default:
                break;
            }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::Cockroach, 0x1C17 };
    }

    DispatchResult Function_WaterImpact(RawGameObject& object, GameContext& context)
    {
        object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity +
            object.itemValue * context.tickCount);
        object.itemValue = WrapSignedWord(object.itemValue + context.tickCount);
        while (object.blendWeightOrSpriteOpacity > 100)
        {
            object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity - 100);
            ++object.animFrame;
            ++object.modelFrame2;
            object.itemValue = 3;
            if (object.modelFrame2 == 4) { context.SetObjectHealth(object, 0); }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::WaterImpact, 0x1C45 };
    }

    DispatchResult Function_RollingRockSlope(RawGameObject& object, GameContext& context)
    {
        if ((context.GetCollisionMapPixelAtWorldXZ(object.posX, object.posZ) & 0x38) == 0 &&
            context.levelElapsedTicks > 20)
        {
            object.behavior = BehaviorType::RollingRock;
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::RollingRockSlope, 0x1CA6 };
    }

    static void UpdateMummyOrGorilla(RawGameObject& object, GameContext& context,
        std::int16_t speed, std::int16_t animationIncrement, bool fadeDeath, bool randomPause)
    {
        for (int tick = 0; tick < context.tickCount; ++tick)
        {
            if (object.health >= 1001) { continue; }
            if (object.generalPurpose1 == 0)
            {
                const auto oldX = object.posX;
                const auto oldZ = object.posZ;
                const auto oldBlend = object.blendWeightOrSpriteOpacity;
                const auto distance = context.GetSoundDistanceAt(object.posX, object.posZ);
                if (object.displayType == DisplayType::Hidden)
                {
                    object.displayType = DisplayType::VectorNormal;
                    object.collisionMask = 254;
                }
                if (object.doorLockOrGroupBitmask < 10) { object.doorLockOrGroupBitmask = 11; }
                if (randomPause && context.Random(80) == 0) { object.velocityX = 0; object.velocityZ = 0; }
                object.posY = WrapSignedWord(
                    context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ) + 30);
                object.spriteScale = static_cast<std::uint8_t>((object.spriteScale + 1) & 1);
                if (context.Random(80) == 0 || distance < 150)
                {
                    object.generalPurpose2 = static_cast<std::uint16_t>(GetVectorAngle(
                        WrapSignedWord(context.soundListenerX - object.posX),
                        WrapSignedWord(context.soundListenerZ - object.posZ)));
                }
                if (context.Random(15) == 0 || WrapSignedWord(object.generalPurpose2) < 0)
                {
                    object.generalPurpose2 = static_cast<std::uint16_t>(
                        WrapSignedWord(object.winY + context.RandomSigned(20)) & 511);
                    GetClampedDirectionDisplacement(object.velocityX, object.velocityZ,
                        WrapSignedWord(object.generalPurpose2), speed);
                }
                if (object.velocityX != 0 || object.velocityZ != 0)
                {
                    object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity + animationIncrement);
                }
                if (distance < 70)
                {
                    context.ApplyPlayerLifeDelta(-29491 * object.behaviorParameter);
                    context.playerHitEffectTicks = 8;
                }
                else if (context.field_D500 && object.spriteScale == 0)
                {
                    object.posX = WrapSignedWord(object.posX + object.velocityX);
                    object.posY = WrapSignedWord(object.posY + object.velocityY);
                    object.posZ = WrapSignedWord(object.posZ + object.velocityZ);
                }
                const auto turn = WrapSignedWord(context.RandomSigned(1) * 64 + 512);
                std::int16_t correctionX = 0;
                std::int16_t correctionZ = 0;
                std::uint16_t center = 0;
                int attempts = 0;
                do
                {
                    correctionX = 0;
                    correctionZ = 0;
                    ++attempts;
                    context.QueryCollisionMapNeighborhood(WrapSignedWord(object.posX + object.velocityX),
                        WrapSignedWord(object.posZ + object.velocityZ), speed, correctionX, correctionZ, center, 0x38);
                    if (correctionX != 0 || correctionZ != 0)
                    {
                        object.generalPurpose2 = static_cast<std::uint16_t>(
                            WrapSignedWord(object.generalPurpose2 + turn) & 511);
                        GetDirectionDisplacement(object.velocityX, object.velocityZ,
                            WrapSignedWord(object.generalPurpose2), speed);
                        object.posX = WrapSignedWord(object.posX + 2 * correctionX);
                        object.posZ = WrapSignedWord(object.posZ + 2 * correctionZ);
                    }
                } while ((correctionX != 0 || correctionZ != 0) &&
                    WrapSignedWord(center) <= 0 && attempts <= 10);
                if (WrapSignedWord(center) > 0) { object.velocityX = 0; object.velocityZ = 0; }
                if (attempts > 10) { context.HandleBlockedObject(object); }
                auto target = WrapSignedWord(object.generalPurpose2);
                NormalizeAnglePair(object.winY, target);
                object.generalPurpose2 = static_cast<std::uint16_t>(target);
                object.winY = WrapSignedWord(object.winY + GetStepToward(object.winY, target, 9));
                if (object.posX == oldX && object.posZ == oldZ && distance > 69 && object.spriteScale == 0)
                {
                    object.blendWeightOrSpriteOpacity = oldBlend;
                }
                while (object.blendWeightOrSpriteOpacity > 100)
                {
                    object.blendWeightOrSpriteOpacity -= 100;
                    object.animFrame = static_cast<std::uint8_t>(object.modelFrame2 % 5);
                    if (object.modelFrame2 > 10)
                    {
                        object.modelAnim = static_cast<std::uint8_t>(object.modelFrame2 / 5 - 1);
                    }
                    object.modelFrame2 = static_cast<std::uint8_t>(
                        object.animFrame % 4 + object.doorLockOrGroupBitmask);
                    object.doorLockOrGroupBitmask = distance < 180 ? 16 : 11;
                }
            }
            else if (object.generalPurpose1 == 1)
            {
                object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity + object.itemValue);
                object.itemValue = WrapSignedWord(object.itemValue + 1);
                if (WrapSignedWord(object.generalPurpose2) > 0)
                {
                    --object.generalPurpose2;
                    object.blendWeightOrSpriteOpacity = 0;
                    object.itemValue = 0;
                }
                else if (object.blendWeightOrSpriteOpacity > 100)
                {
                    object.blendWeightOrSpriteOpacity = 0;
                    if (object.animFrame < 2)
                    {
                        object.generalPurpose2 = 20;
                        ++object.animFrame;
                        object.modelFrame2 = static_cast<std::uint8_t>(object.animFrame + 1);
                    }
                    else
                    {
                        object.animFrame = 3;
                        object.modelFrame2 = 0;
                        if (fadeDeath)
                        {
                            object.generalPurpose1 = 2;
                            context.SetObjectHealth(object, 250);
                            object.generalPurpose2 = 0;
                        }
                        else { object.behavior = BehaviorType::None; }
                    }
                }
            }
            else if (object.generalPurpose1 == 2)
            {
                object.itemValue = std::max<std::int16_t>(WrapSignedWord(35 - object.health / 6), 30);
                ++object.generalPurpose2;
                object.displayType = static_cast<DisplayType>(static_cast<std::uint8_t>(object.itemValue));
                GetClampedDirectionDisplacement(object.velocityX, object.velocityZ, object.winY,
                    static_cast<std::int16_t>(context.Random(50)));
                if (object.itemValue > 30 && (object.generalPurpose2 & 3) == 0)
                {
                    const auto slot = context.FindFreeObjectIndex();
                    // The shared lifetime countdown can free this source before the final substep.
                    if (&context.worldObjects[slot] == &object) { break; }
                    const auto x = WrapSignedWord(object.posX + object.velocityX);
                    const auto y = context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ);
                    const auto z = WrapSignedWord(object.posZ + object.velocityZ);
                    context.InitializeSmokeParticle(context.worldObjects[slot], x, y, z, 10, 1, 1);
                }
            }
        }
    }

    DispatchResult Function_Mummie(RawGameObject& object, GameContext& context)
    {
        UpdateMummyOrGorilla(object, context, 3, 8, true, true);
        return { DispatchStatus::ExecutedCase, BehaviorType::Mummie, 0x1CE2 };
    }

    DispatchResult Function_Gorilla(RawGameObject& object, GameContext& context)
    {
        UpdateMummyOrGorilla(object, context, 5, 13, false, false);
        return { DispatchStatus::ExecutedCase, BehaviorType::Gorilla, 0x1DC4 };
    }

    DispatchResult Function_ShootArrows(RawGameObject& object, GameContext& context)
    {
        if (object.generalPurpose1 != 1 && object.generalPurpose1 != 11)
        {
            return { DispatchStatus::UnimplementedHandler, BehaviorType::ShootArrows, 0x1E07 };
        }
        if (object.health == 1000)
        {
            for (int tick = 0; tick < context.tickCount; ++tick)
            {
                if (object.modelFrame2 > 0) { object.animFrame = object.modelFrame2; }
                if (WrapSignedWord(object.generalPurpose2) < 1)
                {
                    object.generalPurpose2 = static_cast<std::uint16_t>(object.itemValue);
                    context.SpawnEmitterProjectile(object);
                    if (object.itemValue == 0) { break; }
                }
                else { --object.generalPurpose2; }
            }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::ShootArrows, 0x1E07 };
    }

    DispatchResult Function_MercuryLamp(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::MercuryLamp, 0x1E53 };
    }

    DispatchResult Function_SpearOnGroundTrap(RawGameObject& object, GameContext& context)
    {
        const auto magnitude = WrapSignedWord(std::abs(static_cast<int>(
            WrapSignedWord(object.generalPurpose1 - 100))));
        object.blendWeightOrSpriteOpacity = std::max<std::int16_t>(WrapSignedWord(100 - magnitude), 0);
        if (object.doorLockOrGroupBitmask == 0) { object.doorLockOrGroupBitmask = 5; }
        if (WrapSignedWord(object.generalPurpose1) < 1)
        {
            object.generalPurpose1 = object.itemValue > 0 ? static_cast<std::uint16_t>(object.itemValue) :
                (context.GetSoundDistanceAt(object.posX, object.posZ) < 80 ? 200 : 0);
            object.generalPurpose2 = 0;
        }
        else
        {
            object.generalPurpose1 = static_cast<std::uint16_t>(
                object.generalPurpose1 - context.tickCount * object.doorLockOrGroupBitmask);
        }
        if (object.blendWeightOrSpriteOpacity > 10 && context.OverlapsPlayer(object))
        {
            context.ApplyLatchedPlayerContactDamage(object);
            if (object.animFrame == 1) { object.animFrame = 3; }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::SpearOnGroundTrap, 0x1E98 };
    }

    DispatchResult Function_Behavior_45(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_45, 0x1EAB };
    }

    DispatchResult Function_Behavior_46(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_46, 0x1EDB };
    }

    DispatchResult Function_WalkablePlanks(RawGameObject& object, GameContext& state)
    {
        if (state.tickCount > 0)
        {
            if (object.itemValue > 0 && state.Random(static_cast<std::uint16_t>(18 / state.tickCount + 1)) == 0)
            {
                --object.itemValue;
            }
            object.generalPurpose2 = static_cast<std::uint16_t>(object.generalPurpose2 + 11 * state.tickCount);
            object.winZ = WrapSignedWord(static_cast<std::int32_t>(
                static_cast<std::uint32_t>(object.itemValue * Sine(WrapSignedWord(object.generalPurpose2))) >> 16));
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::WalkablePlanks, 0x1F12 };
    }

    DispatchResult Function_BreakableStoneWallFloodingWater(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::BreakableStoneWallFloodingWater, 0x1F72 };
    }

    DispatchResult Function_WalkableDrawBridge(RawGameObject& object, GameContext& state)
    {
        if (object.health == 1000)
        {
            auto& angle = object.blendWeightOrSpriteOpacity;
            angle = WrapSignedWord(angle + GetStepToward(angle, 0,
                WrapSignedWord(object.velocityZ * state.tickCount)));
            if (angle > 0)
            {
                state.PlaySoundEffect(static_cast<SoundEffect>(object.soundEffectId),
                    state.GetSoundVolumeAt(object.posX, object.posZ), object, 2);
            }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::WalkableDrawBridge, 0x1F85 };
    }

    DispatchResult Function_Switch1(RawGameObject& object, GameContext& state)
    {
        if (object.generalPurpose1 == 0)
        {
            object.generalPurpose1 = 10;
        }
        auto& phase = object.blendWeightOrSpriteOpacity;
        phase = WrapSignedWord(phase +
            static_cast<std::int32_t>(WrapSignedWord(object.generalPurpose2)) * state.tickCount);
        if (phase < 0)
        {
            object.generalPurpose2 = 0;
            phase = 0;
        }
        if (phase > 100)
        {
            object.generalPurpose2 = static_cast<std::uint16_t>(-static_cast<std::int32_t>(object.generalPurpose1));
            phase = 100;
        }
        if (phase == 100 && object.behaviorParameter != 0)
        {
            object.generalPurpose2 = 0;
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::Switch1, 0x1FDD };
    }

    DispatchResult Function_SwitchGrouped(RawGameObject& object, GameContext& state)
    {
        if (object.groupIndexOrLock < 0 ||
            static_cast<std::size_t>(object.groupIndexOrLock) >= std::size(state.switchGroupMasks))
        {
            throw std::out_of_range("Grouped switch requires a group index in 0..4 (DS:EC70).");
        }
        object.collisionType = CollisionType::GroupedSwitch;
        auto& phase = object.blendWeightOrSpriteOpacity;
        phase = WrapSignedWord(phase +
            static_cast<std::int32_t>(WrapSignedWord(object.generalPurpose2)) * state.tickCount);
        if (phase < 0)
        {
            object.generalPurpose2 = 0;
            phase = 0;
        }
        if (phase > 100)
        {
            object.generalPurpose2 = 0;
            phase = 100;
        }
        auto& groupMask = state.switchGroupMasks[object.groupIndexOrLock];
        const auto mask = static_cast<std::uint8_t>(object.doorLockOrGroupBitmask);
        if (phase == 100)
        {
            groupMask &= static_cast<std::uint8_t>(~mask);
        }
        if (phase == 0)
        {
            groupMask |= mask;
        }
        if (phase == 100 && WrapSignedWord(object.generalPurpose1) > 0 && object.itemValue > 0)
        {
            object.generalPurpose1 = static_cast<std::uint16_t>(object.generalPurpose1 - state.tickCount);
            if (WrapSignedWord(object.generalPurpose1) < 1)
            {
                object.generalPurpose1 = 0;
                object.generalPurpose2 = static_cast<std::uint16_t>(-10);
                groupMask |= mask;
            }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::SwitchGrouped, 0x2065 };
    }

    DispatchResult Function_RopeSwitch(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::RopeSwitch, 0x2078 };
    }

    DispatchResult Function_Switch3(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Switch3, 0x20D7 };
    }

    DispatchResult Function_UnderwaterPlants(RawGameObject& object, GameContext& context)
    {
        object.velocityZ = WrapSignedWord(object.velocityZ + object.velocityX * context.tickCount);
        object.blendWeightOrSpriteOpacity = WrapSignedWord(
            static_cast<std::int32_t>(static_cast<std::uint32_t>(Sine(object.velocityZ) * 50) >> 16) + 50);
        if (object.velocityZ > 128)
        {
            object.animFrame = object.modelFrame2;
            if (object.itemValue == 0) { object.itemValue = 3; }
            const auto previousBit = object.animFrame >= 1 && object.animFrame <= 4 ? 1 << (object.animFrame - 1) : 0;
            const auto allowed = object.itemValue & 15 & ~previousBit;
            if (allowed == 0) { throw std::invalid_argument("Plant animation needs an allowed alternative frame."); }
            do { object.modelFrame2 = static_cast<std::uint8_t>(context.Random(4)); }
            while ((object.itemValue & (1 << object.modelFrame2)) == 0 ||
                object.modelFrame2 + 1 == object.animFrame);
            ++object.modelFrame2;
            object.velocityZ = -128;
            object.blendWeightOrSpriteOpacity = 0;
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::UnderwaterPlants, 0x212D };
    }

    DispatchResult Function_Behavior_55(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_55, 0x21FF };
    }

    DispatchResult Function_Switch4(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Switch4, 0x22E6 };
    }

    DispatchResult Function_MetalPole(RawGameObject& object, GameContext& context)
    {
        if (object.winX == 0)
        {
            object.posY = context.GetTerrainFloorHeight(object.posX,
                WrapSignedWord(context.GetPlayerCamera().position[1] + 40), object.posZ);
        }
        const auto slot = context.FindCollidingObject(object);
        if (slot != 0)
        {
            auto& target = context.worldObjects[slot];
            if (target.collisionType == CollisionType::SwitchForMetalPole)
            {
                if (object.soundEffectId != 0)
                {
                    context.PlaySoundEffect(static_cast<SoundEffect>(object.soundEffectId),
                        context.GetSoundVolumeAt(object.posX, object.posZ), object, 1);
                }
                target.collisionType = CollisionType::Switch; target.animFrame = 2; context.SetObjectHealth(object, 0);
            }
            else if (target.collisionType == CollisionType::BurnableOilPuddle && object.doorLockOrGroupBitmask == 21)
            {
                context.DamageObject(target, 0);
            }
        }
        if (object.doorLockOrGroupBitmask == 21 &&
            (context.GetCollisionMapPixelAtWorldXZ(object.posX, object.posZ) & 7) != 0)
        {
            object.doorLockOrGroupBitmask = 20; object.animFrame = 1;
            context.InitializeSmokeParticle(context.worldObjects[context.FindFreeOrLowestHealthObjectIndex()],
                object.posX, object.posY, object.posZ, 0, 1, 1);
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::MetalPole, 0x237D };
    }

    DispatchResult Function_KeyInBucket(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::KeyInBucket, 0x2484 };
    }

    DispatchResult Function_DiggableGround(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::DiggableGround, 0x2516 };
    }

    DispatchResult Function_Buried(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Buried, 0x2529 };
    }

    DispatchResult Function_Shark(RawGameObject& object, GameContext& context)
    {
        for (int tick = 0; tick < context.tickCount; ++tick)
        {
            if (object.health == 1000)
            {
                object.collisionMask = object.modelAnim == 1 ? 0 : 3;
                object.posY = context.GetFloorHeight(object.posX, object.posY, object.posZ);
                if (context.Random(30) == 0) { object.velocityY = static_cast<std::int16_t>(context.Random(5) - 2); }
                if (context.Random(15) == 0) { object.velocityY = 0; }
                if (context.Random(100) == 0) { object.generalPurpose1 = static_cast<std::uint16_t>(context.Random(600) + 60); }
                object.itemValue = WrapSignedWord(object.itemValue +
                    GetStepToward(object.itemValue, WrapSignedWord(object.generalPurpose1), 3));
                object.winY = static_cast<std::int16_t>(WrapSignedWord(object.winY + object.velocityY) & 511);
                std::int16_t x;
                std::int16_t z;
                GetClampedDirectionDisplacement(x, z, object.winY, object.itemValue);
                const auto probeX = WrapSignedWord(object.posX + SignWord(x) * (context.Random(30) + 30));
                const bool waterX = (context.GetCollisionMapPixelAtWorldXZ(probeX, object.posZ) & 7) == 1;
                const auto probeZ = WrapSignedWord(object.posZ + SignWord(z) * (context.Random(30) + 30));
                const bool waterZ = (context.GetCollisionMapPixelAtWorldXZ(object.posX, probeZ) & 7) == 1;
                if (!waterX || !waterZ) { object.velocityY = static_cast<std::int16_t>(context.Random(3) + 2); }
                else
                {
                    AddFixedPoint8(object.posX, object.velocityX, x);
                    AddFixedPoint8(object.posZ, object.velocityZ, z);
                }
                object.blendWeightOrSpriteOpacity = WrapSignedWord(
                    object.blendWeightOrSpriteOpacity + ArithmeticShiftRight(object.itemValue, 5));
                if (object.blendWeightOrSpriteOpacity > 100)
                {
                    do
                    {
                        object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity - 100);
                        object.animFrame = static_cast<std::uint8_t>((object.animFrame & 3) + 1);
                        object.modelFrame2 = static_cast<std::uint8_t>((object.animFrame & 3) + 1);
                    } while (object.blendWeightOrSpriteOpacity > 100);
                    if (object.animFrame == 2)
                    {
                        object.modelAnim = 1;
                        if (context.GetSoundDistanceAt(object.posX, object.posZ) < 150 || context.Random(15) == 0)
                        {
                            object.modelAnim = 2;
                        }
                    }
                }
            }
            else if (object.health == 2000)
            {
                if (object.itemValue > 10) { object.itemValue = WrapSignedWord(object.itemValue - 10); }
                object.blendWeightOrSpriteOpacity = static_cast<std::int16_t>(
                    context.Random(static_cast<std::uint16_t>(object.itemValue)));
                object.generalPurpose2 ^= 1;
                if ((object.generalPurpose2 & 1) != 0 && object.velocityY >= 0)
                {
                    object.velocityY = WrapSignedWord(object.velocityY - 1);
                }
                if (context.Random(3) == 0) { object.posY = WrapSignedWord(object.posY + object.velocityY); }
                const auto floor = WrapSignedWord(
                    context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ) - 10);
                if (floor > object.posY)
                {
                    object.collisionMask = 0;
                    object.collisionType = CollisionType::None;
                    object.behavior = BehaviorType::None;
                }
            }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::Shark, 0x2540 };
    }

    DispatchResult Function_Crab(RawGameObject& object, GameContext& context)
    {
        for (int tick = 0; tick < context.tickCount; ++tick)
        {
            if (object.health != 1000) { continue; }
            object.posY = context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ);
            if (context.Random(30) == 0) { object.itemValue = WrapSignedWord(context.Random(200) + 60); }
            if (context.Random(50) == 0) { object.itemValue = 0; }
            if (context.Random(20) == 0)
            {
                object.winY = WrapSignedWord(object.winY + context.RandomSigned(20)) & 511;
            }
            std::int16_t x, z;
            GetClampedDirectionDisplacement(x, z, object.winY, object.itemValue);
            const auto material = static_cast<std::uint8_t>(object.generalPurpose1);
            const bool clearX = (context.GetCollisionMapPixelAtWorldXZ(
                WrapSignedWord(object.posX + 30 * SignWord(x)), object.posZ) & 0x3f) == material;
            const bool clearZ = (context.GetCollisionMapPixelAtWorldXZ(object.posX,
                WrapSignedWord(object.posZ + 30 * SignWord(z))) & 0x3f) == material;
            if (!clearX || !clearZ)
            {
                object.winY = WrapSignedWord(object.winY + 11 * SignWord(object.itemValue)) & 511;
            }
            else
            {
                AddFixedPoint8(object.posX, object.velocityX, x);
                AddFixedPoint8(object.posZ, object.velocityZ, z);
            }
            object.blendWeightOrSpriteOpacity = WrapSignedWord(
                object.blendWeightOrSpriteOpacity + object.itemValue / 20) % 200;
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::Crab, 0x256E };
    }

    DispatchResult Function_DecoFish(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::DecoFish, 0x259C };
    }

    DispatchResult Function_DogLeash(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::DogLeash, 0x25FE };
    }

    DispatchResult Function_TriggerDebris(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::TriggerDebris, 0x2618 };
    }

    DispatchResult Function_FlipTRexHead(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::FlipTRexHead, 0x2651 };
    }

    DispatchResult Function_Ghost(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Ghost, 0x2669 };
    }

    DispatchResult Function_LightRayKillsGhost(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::LightRayKillsGhost, 0x2697 };
    }

    DispatchResult Function_ActivatableLadder(RawGameObject& object, GameContext& context)
    {
        if (object.health == 1000)
        {
            object.generalPurpose2 = static_cast<std::uint16_t>(object.generalPurpose2 + context.tickCount);
            object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity +
                ArithmeticShiftRight(WrapSignedWord(
                    WrapSignedWord(object.generalPurpose2) * context.tickCount), 2));
            if (object.blendWeightOrSpriteOpacity > 100)
            {
                object.blendWeightOrSpriteOpacity = 100;
                object.generalPurpose2 = 0;
                if (object.soundEffectId != 0) { object.soundEffectId = 54; }
            }
            if (object.soundEffectId != 0)
            {
                context.PlaySoundEffect(static_cast<SoundEffect>(object.soundEffectId),
                    context.GetSoundVolumeAt(object.posX, object.posZ), object, 1);
            }
            if (object.blendWeightOrSpriteOpacity == 100)
            {
                context.SetCollisionMapPixelAtWorldXZ(WrapSignedWord(object.posX + 25), object.posZ, 8);
                object.collisionMask = 3;
                object.soundEffectId = 0;
                if (context.OverlapsPlayer(object)) { context.playerLadderMovement = true; }
                object.collisionMask = 0;
            }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::ActivatableLadder, 0x26B5 };
    }

    DispatchResult Function_LiftUp(RawGameObject& object, GameContext& context)
    {
        if (object.health == 1000)
        {
            object.collisionType = CollisionType::LiftUp;
            object.velocityY = context.OverlapsPlayer(object) ? 1 : -2;
            object.posY = WrapSignedWord(object.posY + object.velocityY * context.tickCount);
            const auto floor = context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ);
            object.posY = std::max(object.posY, floor);
            if (object.posY != floor)
            {
                context.PlaySoundEffect(static_cast<SoundEffect>(object.soundEffectId),
                    context.GetSoundVolumeAt(object.posX, object.posZ), object, 1);
            }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::LiftUp, 0x26D6 };
    }

    DispatchResult Function_RechargeElectricity(RawGameObject& object, GameContext& context)
    {
        const auto distance = context.GetSoundDistanceAt(object.posX, object.posZ);
        if (WrapSignedWord(std::abs(static_cast<int>(WrapSignedWord(distance - 100)))) < 21 &&
            context.selectedWeapon == WeaponType::Electricity && context.weaponAmounts[7] < 360)
        {
            context.weaponAmounts[7] = WrapSignedWord(context.weaponAmounts[7] + 2 * context.tickCount + 2);
            context.AttackWithSelectedWeapon();
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::RechargeElectricity, 0x277C };
    }

    DispatchResult Function_Lorry(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Lorry, 0x27B1 };
    }

    DispatchResult Function_MummieGuard(RawGameObject& object, GameContext& context)
    {
        if (object.itemValue < 1)
        {
            object.collisionMask = 0;
            context.PlaySoundEffect(static_cast<SoundEffect>(object.soundEffectId),
                GameContext::GetSoundVolume(0), object, 1);
            object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity +
                GetStepToward(object.blendWeightOrSpriteOpacity, 100, WrapSignedWord(context.tickCount * 17)));
            if (object.blendWeightOrSpriteOpacity > 99)
            {
                object.animFrame = object.modelFrame2;
                object.behavior = BehaviorType::None;
            }
        }
        else if (context.behaviorDispatchEnabled && context.OverlapsPlayer(object))
        {
            object.itemValue = WrapSignedWord(object.itemValue - context.tickCount);
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::MummieGuard, 0x27CC };
    }

    DispatchResult Function_BearTrapAmmo(RawGameObject& object, GameContext& context)
    {
        const auto volume = context.GetSoundVolumeAt(object.posX, object.posZ);
        for (int tick = 0; tick < context.tickCount; ++tick)
        {
            object.posY = WrapSignedWord(object.posY + object.velocityY);
            object.velocityY = WrapSignedWord(object.velocityY - 1);
            const auto floor = context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ);
            if (object.posY < floor) { object.posY = floor; object.velocityY = 0; }
            if (object.generalPurpose1 == 0)
            {
                const auto slot = context.FindCollidingObject(object);
                if (slot > 1)
                {
                    object.velocityY = 5; object.generalPurpose1 = 1;
                    context.DamageObject(context.worldObjects[slot], object.behaviorParameter);
                    context.PlaySoundEffect(static_cast<SoundEffect>(27), volume, object, 1);
                    context.worldObjects[slot].velocityY = 0;
                }
            }
            else if (WrapSignedWord(object.generalPurpose1) > 0)
            {
                object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity + object.generalPurpose1);
                ++object.generalPurpose1;
                if (object.blendWeightOrSpriteOpacity > 100)
                {
                    object.blendWeightOrSpriteOpacity = 100; object.generalPurpose1 = 65535;
                }
            }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::BearTrapAmmo, 0x285C };
    }

    DispatchResult Function_FireExhauster(RawGameObject& object, GameContext& context)
    {
        for (int tick = 0; tick < context.tickCount; ++tick)
        {
            const auto phase = WrapSignedWord(object.generalPurpose1);
            if (phase == object.groupIndexOrLock)
            {
                context.PlaySoundEffect(static_cast<SoundEffect>(22),
                    context.GetSoundVolumeAt(object.posX, object.posZ), object, 1);
            }
            if ((phase & 1) != 0 && phase <= object.groupIndexOrLock)
            {
                auto source = object;
                source.generalPurpose1 = 7;
                const auto slot = context.SpawnEmitterProjectile(source);
                if (&context.worldObjects[slot] == &object) { break; }
            }
            object.generalPurpose1 = phase < 1 ? static_cast<std::uint16_t>(object.itemValue) :
                static_cast<std::uint16_t>(phase - 1);
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::FireExhauster, 0x288D };
    }

    DispatchResult Function_Behavior_82(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_82, 0x2947 };
    }

    DispatchResult Function_ItemHiddenInBox(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::ItemHiddenInBox, 0x2A65 };
    }

    DispatchResult Function_AnancondaHead(RawGameObject& object, GameContext& context)
    {
        if (object.health == 1000)
        {
            for (int tick = 0; tick < context.tickCount; ++tick)
            {
                const auto distance = context.GetSoundDistanceAt(object.posX, object.posZ);
                if (WrapSignedWord(object.generalPurpose1) > 0)
                {
                    --object.generalPurpose1;
                    object.doorLockOrGroupBitmask = -3;
                }
                else { object.doorLockOrGroupBitmask = 1; }
                std::int16_t x = 0, z = 0;
                std::uint16_t obstacle = 0;
                context.QueryCollisionMapNeighborhood(
                    WrapSignedWord(object.posX + object.velocityX * object.doorLockOrGroupBitmask),
                    object.posZ, 1, x, z, obstacle, 0x38);
                if (obstacle != 0) { object.velocityX = WrapSignedWord(-object.velocityX); }
                else if (x != 0) { object.velocityX = WrapSignedWord(std::abs(static_cast<int>(object.velocityX)) * x); }
                context.QueryCollisionMapNeighborhood(object.posX,
                    WrapSignedWord(object.posZ + object.velocityZ * object.doorLockOrGroupBitmask),
                    1, x, z, obstacle, 0x38);
                if (obstacle != 0) { object.velocityZ = WrapSignedWord(-object.velocityZ); }
                else if (z != 0) { object.velocityZ = WrapSignedWord(std::abs(static_cast<int>(object.velocityZ)) * z); }
                if (context.Random(50) == 0 && x == 0 && z == 0 && obstacle != 8)
                {
                    if (context.Random(3) == 0)
                    {
                        object.velocityX = WrapSignedWord(context.Random(5) - 2);
                        object.velocityZ = WrapSignedWord(context.Random(5) - 2);
                    }
                    else
                    {
                        object.velocityX = static_cast<std::int16_t>(
                            2 * SignWord(WrapSignedWord(context.soundListenerX - object.posX)));
                        object.velocityZ = static_cast<std::int16_t>(
                            2 * SignWord(WrapSignedWord(context.soundListenerZ - object.posZ)));
                    }
                }
                if (object.winX < 512)
                {
                    const auto threshold = distance < 200 ? context.Random(40) + 60 : 2;
                    if (object.winX >= 12) { object.velocityY = -1; }
                    if (object.winX <= threshold || object.velocityY == 0) { object.velocityY = 2; }
                    object.posY = WrapSignedWord(
                        context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ) +
                        std::abs(static_cast<int>(SineOffset(object.winX, 25))));
                }
                else { object.velocityY = 0; }
                if (context.field_D500)
                {
                    object.posX = WrapSignedWord(object.posX + object.velocityX * object.doorLockOrGroupBitmask);
                    object.posZ = WrapSignedWord(object.posZ + object.velocityZ * object.doorLockOrGroupBitmask);
                    object.winX = WrapSignedWord(object.winX + object.velocityY);
                }
                auto angle = WrapSignedWord(obstacle);
                if (WrapSignedWord(std::abs(static_cast<int>(object.velocityX)) +
                    std::abs(static_cast<int>(object.velocityZ))) > 0)
                {
                    angle = GetVectorAngle(WrapSignedWord(-object.velocityX), WrapSignedWord(-object.velocityZ));
                }
                if (WrapSignedWord(std::abs(static_cast<int>(WrapSignedWord(angle - object.winY)))) > 256)
                {
                    angle = WrapSignedWord(angle + 512);
                }
                object.winY = WrapSignedWord(object.winY + GetStepToward(object.winY, angle, 20));
                if (object.modelFrame2 == 0)
                {
                    const auto view = GetVectorAngle(WrapSignedWord(object.posX - context.soundListenerX),
                        WrapSignedWord(context.soundListenerZ - object.posZ));
                    object.animFrame = ((object.winY + view) & 511) <= 256 ? 2 : 1;
                }
                else
                {
                    object.blendWeightOrSpriteOpacity = WrapSignedWord(
                        object.blendWeightOrSpriteOpacity + 7) % 200;
                }
                object.generalPurpose2 = object.winX < 5 ?
                    static_cast<std::uint16_t>((object.generalPurpose2 + context.Random(17)) & 511) : 0;
            }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::AnancondaHead, 0x2A80 };
    }

    DispatchResult Function_AnancondaTailSegment(RawGameObject& object, GameContext& context)
    {
        for (int tick = 0; tick < context.tickCount && context.field_D500; ++tick)
        {
            const auto& previous = PreviousSegment(object, context);
            if (object.health == 1000)
            {
                object.winY = GetVectorAngle(WrapSignedWord(previous.posX - object.posX),
                    WrapSignedWord(previous.posZ - object.posZ));
                object.generalPurpose2 = static_cast<std::uint16_t>((previous.generalPurpose2 + 51) & 511);
                if (object.velocityX == 0)
                {
                    object.velocityX = WrapSignedWord(std::abs(static_cast<int>(previous.velocityX)) +
                        std::abs(static_cast<int>(previous.velocityZ)));
                }
                object.winY = static_cast<std::int16_t>((object.winY +
                    SineOffset(WrapSignedWord(object.generalPurpose2), WrapSignedWord(3 * object.velocityX))) & 511);
                object.generalPurpose1 = previous.collisionType == CollisionType::AnacondaHead ? 20 : 25;
                std::int16_t dx, dz;
                GetClampedDirectionDisplacement(dx, dz, object.winY, WrapSignedWord(object.generalPurpose1));
                object.posX = WrapSignedWord(previous.posX - dx);
                object.posY = context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ);
                if (object.velocityY > 0)
                {
                    // CS:8192 stores the extended-precision factor 2.56 (one cycle per 200 blend steps).
                    const auto phase = WrapSignedWord(static_cast<std::int32_t>(
                        std::nearbyint(object.blendWeightOrSpriteOpacity * 2.56)) +
                        30 * context.FindWorldObjectSlot(object));
                    object.posY = WrapSignedWord(object.posY + object.velocityY + SineOffset(phase, 18));
                }
                object.posY = ArithmeticShiftRight(WrapSignedWord(previous.posY + object.posY), 1);
                object.posZ = WrapSignedWord(previous.posZ - dz);
                std::int16_t correctionX = 0, correctionZ = 0;
                std::uint16_t obstacle = 0;
                context.QueryCollisionMapNeighborhood(object.posX, object.posZ, 4,
                    correctionX, correctionZ, obstacle, 0x38);
                object.posX = WrapSignedWord(object.posX + correctionX);
                object.posZ = WrapSignedWord(object.posZ + correctionZ);
                object.winY = GetVectorAngle(WrapSignedWord(previous.posX - object.posX),
                    WrapSignedWord(previous.posZ - object.posZ));
                AimSegmentPitch(object, previous);
                object.winZ = 0;
                context.SetObjectHealth(object, previous.health);
                object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity + 4) % 200;
            }
            else if (object.health == 2000)
            {
                object.collisionMask = 0;
                const auto floor = context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ);
                object.posY = WrapSignedWord(object.posY + GetStepToward(object.posY, floor, 1));
                AimSegmentPitch(object, previous);
            }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::AnancondaTailSegment, 0x2ABB };
    }

    DispatchResult Function_AnacondaHeadDying(RawGameObject& object, GameContext& context)
    {
        object.collisionMask = 0;
        object.winX = WrapSignedWord(object.winX +
            GetStepToward(object.winX, object.winX < 256 ? 0 : 512, WrapSignedWord(5 * context.tickCount)));
        const auto floor = context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ);
        object.posY = WrapSignedWord(object.posY + GetStepToward(object.posY, floor, context.tickCount));
        return { DispatchStatus::ExecutedCase, BehaviorType::AnacondaHeadDying, 0x2AE9 };
    }

    DispatchResult Function_EndBossHead(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::EndBossHead, 0x2BC4 };
    }

    DispatchResult Function_EndBossLeg(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::EndBossLeg, 0x2BDA };
    }

    DispatchResult Function_EndBossFeeler(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::EndBossFeeler, 0x2C97 };
    }

    DispatchResult Function_EndBossBody(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::EndBossBody, 0x2D5C };
    }

    DispatchResult Function_BreakableGround(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::BreakableGround, 0x2D84 };
    }

    DispatchResult Function_Behavior_96(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_96, 0x2E21 };
    }

    DispatchResult Function_Behavior_97(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_97, 0x2E6B };
    }

    DispatchResult Function_Behavior_98(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_98, 0x2E7B };
    }

    DispatchResult Function_Moon(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Moon, 0x2EAA };
    }

    DispatchResult Function_WaterDrop(RawGameObject& object, GameContext& context)
    {
        const auto floor = context.GetFloorHeight(object.posX, object.posY, object.posZ);
        const auto ticks = context.tickCount;
        object.posY = WrapSignedWord(object.posY + static_cast<std::int32_t>(object.velocityY) * ticks);
        object.displayType = DisplayType::SpriteTransparent;
        object.blendWeightOrSpriteOpacity = 4;
        if (WrapSignedWord(object.generalPurpose1) < 1)
        {
            const auto scale = object.spriteScale;
            if (scale > object.doorLockOrGroupBitmask)
            {
                object.velocityY = WrapSignedWord(object.velocityY - ticks);
            }
            else
            {
                object.spriteScale = static_cast<std::uint8_t>(scale + ticks);
            }
            if (object.velocityY < -16) { object.velocityY = -16; }
        }
        else
        {
            object.generalPurpose1 = static_cast<std::uint16_t>(object.generalPurpose1 - ticks);
        }
        if (object.posY < floor)
        {
            if (object.winX == 1)
            {
                context.PlaySoundEffect(SoundEffect::Drop,
                    context.GetSoundVolumeAt(object.posX, object.posZ, 3), object, 1);
            }
            object.winX = 0;
            object.generalPurpose2 = static_cast<std::uint16_t>(object.generalPurpose2 - 1u);
            object.posY = WrapSignedWord(floor - 1);
            object.velocityY = 0;
            object.animFrame = 2;
            if (WrapSignedWord(object.generalPurpose2) < 1)
            {
                object.animFrame = 1;
                object.generalPurpose2 = 4;
                object.posY = WrapSignedWord(floor + context.GetRoomHeight(object.posX, object.posZ));
                object.generalPurpose1 = static_cast<std::uint16_t>(object.itemValue);
                object.spriteScale = 0;
                object.winX = 1;
            }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::WaterDrop, 0x2EBD };
    }

    DispatchResult Function_Bubble(RawGameObject& object, GameContext& context)
    {
        object.winY = WrapSignedWord(object.winY + object.velocityX * static_cast<std::int32_t>(context.tickCount));
        const auto fraction = WrapSignedWord(object.itemValue + object.velocityY * static_cast<std::int32_t>(context.tickCount));
        const auto high = static_cast<std::int8_t>(static_cast<std::uint16_t>(fraction) >> 8);
        object.posY = WrapSignedWord(object.posY + high);
        object.itemValue = static_cast<std::uint8_t>(fraction);
        if ((context.GetCollisionMapPixelAtWorldXZ(object.posX, object.posZ) & 7) == 1 &&
            WrapSignedWord(object.posY + context.GetHeightSurfaceDepth(1)) > 0)
        {
            object.behavior = BehaviorType::BubblePop;
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::Bubble, 0x2FE2 };
    }

    DispatchResult Function_UnderwaterColorAnim(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::UnderwaterColorAnim, 0x3064 };
    }

    DispatchResult Function_BubblePop(RawGameObject& object, GameContext& context)
    {
        object.animFrame = 4;
        object.modelFrame2 = 5;
        object.blendWeightOrSpriteOpacity = WrapSignedWord(
            object.blendWeightOrSpriteOpacity + 2 * static_cast<std::int32_t>(context.tickCount));
        if (object.blendWeightOrSpriteOpacity > 100) { context.SetObjectHealth(object, 0); }
        object.displayType = static_cast<DisplayType>(std::clamp(
            object.blendWeightOrSpriteOpacity / 8 + 27, 30, 36));
        return { DispatchStatus::ExecutedCase, BehaviorType::BubblePop, 0x319B };
    }

    DispatchResult Function_Behavior_106(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_106, 0x31FD };
    }

    DispatchResult Function_FogParticle(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::FogParticle, 0x3405 };
    }

    DispatchResult Function_Behavior_111(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_111, 0x349A };
    }

    DispatchResult Function_NeonLights(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::NeonLights, 0x3636 };
    }

    DispatchResult Function_FloatingDebris(RawGameObject& object, GameContext& context)
    {
        if (!context.objectCollisionsEnabled)
        {
            return { DispatchStatus::ExecutedCase, BehaviorType::FloatingDebris, 0x372C };
        }

        const auto oldWinX = object.winX;
        if (context.FindCollidingObject(object) != 0 || context.OverlapsPlayer(object))
        {
            object.generalPurpose3 = static_cast<std::uint16_t>(100 + context.Random(300));
            object.velocityY = WrapSignedWord(std::abs(static_cast<int>(object.velocityY)));
        }
        const auto chance = object.behaviorParameter;
        if (chance > 0 && context.Random(chance) == 0) { object.generalPurpose3 = 0; }

        for (int tick = 0; tick < context.tickCount; ++tick)
        {
            if (WrapSignedWord(object.generalPurpose3) > 0)
            {
                object.generalPurpose3 = static_cast<std::uint16_t>(object.generalPurpose3 - context.Random(2));
                if (context.Random(10) == 0)
                {
                    object.generalPurpose1 = static_cast<std::uint16_t>(
                        (object.generalPurpose1 + context.RandomSigned(20)) & 511);
                }
                object.winX = static_cast<std::int16_t>(
                    (object.winX + context.Random(static_cast<std::uint16_t>((object.generalPurpose3 >> 4) + 1))) & 511);
                object.winY = static_cast<std::int16_t>(
                    (object.winY + context.Random(static_cast<std::uint16_t>((object.generalPurpose3 >> 5) + 1))) & 511);
                if (context.Random(50) == 0) { object.velocityY = static_cast<std::int16_t>(context.Random(30) - 12); }
                const auto threshold = context.Random(10);
                if (WrapSignedWord(std::abs(static_cast<int>(object.velocityY))) > threshold)
                {
                    object.posY = WrapSignedWord(object.posY + SignWord(object.velocityY));
                }
            }
            else
            {
                object.winX = WrapSignedWord(object.winX + GetStepToward(object.winX, 256, 1));
                if (context.Random(3) == 0) { object.posY = WrapSignedWord(object.posY - 1); }
                if (context.Random(300) == 0) { object.generalPurpose3 = context.Random(100); }
            }
        }

        // These operations happen once per call, including when tickCount is zero.
        if (context.Random(3) == 0) { object.posY = WrapSignedWord(object.posY - 1); }
        auto floor = context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ);
        if (static_cast<std::uint8_t>(object.collisionType) != 20)
        {
            floor = WrapSignedWord(floor + object.collisionRadiusY);
        }
        if (object.posY < floor) { object.posY = floor; }
        const auto surface = WrapSignedWord(-static_cast<std::int32_t>(context.GetHeightSurfaceDepth(1)));
        if (object.posY > surface)
        {
            object.posY = surface;
            const auto target = WrapSignedWord(256 + context.RandomSigned(8));
            object.winX = WrapSignedWord(oldWinX +
                GetStepToward(oldWinX, target, WrapSignedWord(6 * static_cast<std::int32_t>(context.tickCount))));
        }

        std::int16_t dx = 0;
        std::int16_t dz = 0;
        GetDirectionDisplacement(dx, dz, WrapSignedWord(object.generalPurpose1), WrapSignedWord(object.generalPurpose3));
        const auto probeX = WrapSignedWord(object.posX + 3 * SignWord(dx));
        if ((context.GetCollisionMapPixelAtWorldXZ(probeX, object.posZ) & 39) != 1)
        {
            object.generalPurpose1 ^= 256;
            dx = WrapSignedWord(-static_cast<std::int32_t>(dx));
        }
        const auto probeZ = WrapSignedWord(object.posZ + 3 * SignWord(dz));
        if ((context.GetCollisionMapPixelAtWorldXZ(object.posX, probeZ) & 39) != 1)
        {
            object.generalPurpose1 ^= 256;
            dz = WrapSignedWord(-static_cast<std::int32_t>(dz));
        }
        AddFixedPoint8(object.posX, object.velocityX, dx);
        AddFixedPoint8(object.posZ, object.velocityZ, dz);
        return { DispatchStatus::ExecutedCase, BehaviorType::FloatingDebris, 0x372C };
    }

    DispatchResult Function_SinkingObject(RawGameObject& object, GameContext& context)
    {
        object.posY = WrapSignedWord(object.posY + GetStepToward(object.posY,
            context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ), context.tickCount));
        return { DispatchStatus::ExecutedCase, BehaviorType::SinkingObject, 0x3746 };
    }

    DispatchResult Function_BurriedVerticalDoor(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::BurriedVerticalDoor, 0x377F };
    }

    DispatchResult Function_Teleporter(RawGameObject& object, GameContext& context)
    {
        if (WrapSignedWord(object.generalPurpose3) < 20 && !context.OverlapsPlayer(object))
        {
            ++object.generalPurpose3;
        }
        object.collisionType = CollisionType::Teleporter;
        return { DispatchStatus::ExecutedCase, BehaviorType::Teleporter, 0x37B0 };
    }

    DispatchResult Function_LevelStartOverride(RawGameObject& object, GameContext& context)
    {
        if (context.OverlapsPlayer(object) && object.health == 1000)
        {
            auto& player = context.worldObjects[1];
            player.posX = object.posX;
            player.posY = object.posY;
            player.posZ = object.posZ;
            context.SetObjectHealth(object, 0);
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::LevelStartOverride, 0x37DD };
    }

    DispatchResult Function_DirtFromAbove(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::DirtFromAbove, 0x381A };
    }

    DispatchResult Function_DirtFromAboveShutter(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::DirtFromAboveShutter, 0x3853 };
    }

    DispatchResult Function_CeilingConcreteSlab(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::CeilingConcreteSlab, 0x388C };
    }

    DispatchResult Function_BurnableOilPuddle(RawGameObject& object, GameContext& context)
    {
        for (int tick = 0; tick < context.tickCount; ++tick)
        {
            const auto display = static_cast<std::uint8_t>(object.displayType);
            if (display > 40 && display < 115)
            {
                context.lightingCache.AddAtWorldXZ(object.posX, object.posZ,
                    static_cast<std::int16_t>(context.Random(40)));
                if (object.blendWeightOrSpriteOpacity < 100)
                {
                    object.blendWeightOrSpriteOpacity = WrapSignedWord(
                        object.blendWeightOrSpriteOpacity + context.Random(2));
                }
                if (object.behaviorParameter < 7)
                {
                    object.behaviorParameter = static_cast<std::uint8_t>(
                        5 - object.blendWeightOrSpriteOpacity / 20);
                }
                if (context.Random(5) < 2)
                {
                    object.displayType = static_cast<DisplayType>(display + 1);
                }
                // Native contact collection precedes the radius change.
                const auto contacts = context.FindCollidingObjects(object);
                if (object.collisionRadiusXZ < 31)
                {
                    object.collisionRadiusXZ = WrapSignedWord(
                        (110 - object.blendWeightOrSpriteOpacity) / 4);
                }
                for (const auto slot : contacts)
                {
                    auto& target = context.worldObjects[slot];
                    const auto targetDisplay = static_cast<std::uint8_t>(target.displayType);
                    if (target.modelId > 29 && (targetDisplay < 20 || targetDisplay == 255) &&
                        target.collisionType != CollisionType::Weapon)
                    {
                        context.DamageObject(target, 200);
                        target.displayType = DisplayType::LastVectorBurning;
                    }
                }
            }
            else if (object.displayType == DisplayType::VectorOilTint)
            {
                context.SetObjectHealth(object, 0);
            }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::BurnableOilPuddle, 0x38B5 };
    }

    DispatchResult Function_CeilingConcreteSlabStake(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::CeilingConcreteSlabStake, 0x390C };
    }

    DispatchResult Function_UnreachableKey(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::UnreachableKey, 0x393F };
    }

    DispatchResult Function_Behavior_128(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_128, 0x3987 };
    }

    DispatchResult Function_AttackingBull(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::AttackingBull, 0x3A36 };
    }

    DispatchResult Function_ExtinguishLava(RawGameObject& object, GameContext& context)
    {
        if (object.health == 1000)
        {
            const auto volume = context.GetSoundVolumeAt(object.posX, object.posZ);
            for (int tick = 0; tick < context.tickCount; ++tick)
            {
                context.PlaySoundEffect(static_cast<SoundEffect>(context.Random(2) == 0 ? 10 : 43),
                    volume, object, 1);
                object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity +
                    std::abs(static_cast<int>(object.velocityX)) + std::abs(static_cast<int>(object.velocityZ)));
                while (object.blendWeightOrSpriteOpacity > 100)
                {
                    object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity - 100);
                    object.itemValue = WrapSignedWord(object.itemValue - 1);
                    if (object.itemValue < 1) { context.SetObjectHealth(object, 0); }
                    if (context.field_D500)
                    {
                        context.ExtinguishLavaTile(object.posX, object.posZ, object.behaviorParameter);
                    }
                    object.posX = WrapSignedWord(object.posX + SignWord(object.velocityX) * 100);
                    object.posZ = WrapSignedWord(object.posZ + SignWord(object.velocityZ) * 100);
                }
            }
        }
        else
        {
            object.posY = WrapSignedWord(context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ) - 4);
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::ExtinguishLava, 0x3A69 };
    }

    DispatchResult Function_FloodingWater(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::FloodingWater, 0x3BF7 };
    }

    DispatchResult Function_ClimbableLadder(RawGameObject& object, GameContext& context)
    {
        if (object.health == 1000)
        {
            object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity +
                GetStepToward(object.blendWeightOrSpriteOpacity, 100, context.tickCount));
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::ClimbableLadder, 0x3C30 };
    }

    DispatchResult Function_CurtainOpens(RawGameObject& object, GameContext& context)
    {
        object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity +
            GetStepToward(object.blendWeightOrSpriteOpacity, WrapSignedWord(object.generalPurpose1),
                WrapSignedWord(object.generalPurpose3 * context.tickCount)));
        return { DispatchStatus::ExecutedCase, BehaviorType::CurtainOpens, 0x3C5B };
    }

    DispatchResult Function_Meteor(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Meteor, 0x3C8B };
    }

    DispatchResult Function_Behavior_136(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_136, 0x3E89 };
    }

    DispatchResult Function_Behavior_137(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_137, 0x3E9C };
    }

    DispatchResult Function_Behavior_138(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_138, 0x3EB8 };
    }

    DispatchResult Function_SpawnInCrater(RawGameObject& object, GameContext& context)
    {
        const auto chance = context.Random(1000);
        if (object.doorLockOrGroupBitmask > chance)
        {
            const auto slot = context.FindFreeObjectIndex();
            // Native code reads source coordinates after allocation, including possible source clearing.
            context.InitializeSmokeParticle(context.worldObjects[slot], object.posX,
                WrapSignedWord(object.posY + object.collisionRadiusY), object.posZ, 5, 1, 1);
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::SpawnInCrater, 0x3F8A };
    }

    DispatchResult Function_RotatingMachineCenter(RawGameObject& object, GameContext& context)
    {
        if (object.health == 1000)
        {
            context.PlaySoundEffect(static_cast<SoundEffect>(object.soundEffectId),
                context.GetSoundVolumeAt(object.posX, object.posZ), object, 1);
            object.winY = static_cast<std::int16_t>(
                (object.winY + object.velocityY * context.tickCount) & 511);
            object.winX = static_cast<std::int16_t>(
                (object.winX + object.velocityX * context.tickCount) & 511);
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::RotatingMachineCenter, 0x3FE0 };
    }

    DispatchResult Function_Submarine(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Submarine, 0x4041 };
    }

    DispatchResult Function_GrasingCow(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::GrasingCow, 0x405C };
    }

    DispatchResult Function_Behavior_143(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_143, 0x40ED };
    }

    DispatchResult Function_CrackingSubmarineWindow(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::CrackingSubmarineWindow, 0x41AE };
    }

    DispatchResult Function_PuzzleGameTile(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::PuzzleGameTile, 0x42A1 };
    }

    DispatchResult Function_Behavior_146(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_146, 0x4313 };
    }

    DispatchResult Function_PuzzleGameHandler(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::PuzzleGameHandler, 0x43D5 };
    }

    DispatchResult Function_Behavior_148(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_148, 0x441F };
    }

    DispatchResult Function_PuzzleSpool(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::PuzzleSpool, 0x4480 };
    }

    DispatchResult Function_OpeningCreviceWithExit(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::OpeningCreviceWithExit, 0x4552 };
    }

    DispatchResult Function_Behavior_151(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_151, 0x458F };
    }

    DispatchResult Function_OpeningCrevice(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::OpeningCrevice, 0x45A2 };
    }

    DispatchResult Function_FlushingFishes(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::FlushingFishes, 0x45B7 };
    }

    DispatchResult Function_SomethingWithTheFishes(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::SomethingWithTheFishes, 0x4652 };
    }

    DispatchResult Function_GrandfatherClock(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::GrandfatherClock, 0x4691 };
    }

    DispatchResult Function_GrandfatherClockHand(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::GrandfatherClockHand, 0x46A4 };
    }

    DispatchResult Function_GrandfatherClockPendulum(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::GrandfatherClockPendulum, 0x46EB };
    }

    DispatchResult Function_Behavior_158(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_158, 0x4738 };
    }

    DispatchResult Function_RotateKnightsInClock(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::RotateKnightsInClock, 0x4782 };
    }

    DispatchResult Function_Behavior_161(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_161, 0x4816 };
    }

    DispatchResult Function_KnightsCrossbow(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::KnightsCrossbow, 0x482D };
    }

    DispatchResult Function_Behavior_163(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_163, 0x48AE };
    }

    DispatchResult Function_RotatingBlades(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::RotatingBlades, 0x4997 };
    }

    DispatchResult Function_ActionObjectToGrab(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::ActionObjectToGrab, 0x49AD };
    }

    DispatchResult Function_PaterNosterLift(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::PaterNosterLift, 0x4ABA };
    }

    DispatchResult Function_SqueezingMachine(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::SqueezingMachine, 0x4AD6 };
    }

    DispatchResult Function_Behavior_168(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_168, 0x4AF2 };
    }

    DispatchResult Function_RoofTopThunderHandler1(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::RoofTopThunderHandler1, 0x4B0E };
    }

    DispatchResult Function_Behavior_170(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_170, 0x4B66 };
    }

    DispatchResult Function_CutSceneCloud(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::CutSceneCloud, 0x4B9F };
    }

    DispatchResult Function_CeilingBladesComingDown(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::CeilingBladesComingDown, 0x4BDC };
    }

    DispatchResult Function_PortableCoffin(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::PortableCoffin, 0x4BFD };
    }

    DispatchResult Function_BladesFromSide(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::BladesFromSide, 0x4C87 };
    }

    DispatchResult Function_ExplodingMachine(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::ExplodingMachine, 0x4CD5 };
    }

    DispatchResult Function_Behavior_176(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_176, 0x4CE8 };
    }

    DispatchResult Function_PressureIndicator(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::PressureIndicator, 0x4D0C };
    }

    DispatchResult Function_Behavior_178(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_178, 0x4D3C };
    }

    DispatchResult Function_LargeFish(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::LargeFish, 0x4D5D };
    }

    DispatchResult Function_Behavior_181(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_181, 0x4D7E };
    }

    DispatchResult Function_DestroyableRoof(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::DestroyableRoof, 0x4E0D };
    }

    DispatchResult Function_RoofTopThunderHandler2(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::RoofTopThunderHandler2, 0x4E28 };
    }

    DispatchResult Function_RoofTopThunderHandler3(RawGameObject& object, GameContext& context)
    {
        object.collisionMask = 1;
        context.playerMapCollisionsEnabled = !context.OverlapsPlayer(object);
        object.collisionMask = 0;
        return { DispatchStatus::ExecutedCase, BehaviorType::RoofTopThunderHandler3, 0x4E85 };
    }

    DispatchResult Function_AmbientSoundPosition(RawGameObject& object, GameContext& context)
    {
        if (context.soundEnabled && WrapSignedWord(object.generalPurpose3) > context.Random(1000))
        {
            context.PlaySoundEffect(static_cast<SoundEffect>(object.soundEffectId),
                context.GetSoundVolumeAt(object.posX, object.posZ), object, object.doorLockOrGroupBitmask);
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::AmbientSoundPosition, 0x4EAF };
    }

    DispatchResult Function_LavaSmoke(RawGameObject& object, GameContext&)
    {
        if (object.health == 1000)
        {
            object.posY = WrapSignedWord(object.posY + object.velocityY);
            object.displayType = static_cast<DisplayType>(static_cast<std::uint8_t>(object.itemValue));
        }
        else { object.displayType = DisplayType::Hidden; }
        return { DispatchStatus::ExecutedCase, BehaviorType::LavaSmoke, 0x4EFB };
    }

    DispatchResult Function_TrackFireAnimation(RawGameObject& object, GameContext& context)
    {
        AdvanceTrackedTexture(object, context);
        return { DispatchStatus::ExecutedCase, BehaviorType::TrackFireAnimation, 0x4F34 };
    }

    DispatchResult Function_FireballImpact(RawGameObject& object, GameContext& context)
    {
        object.collisionMask = 0;
        object.spriteScale = static_cast<std::uint8_t>(object.spriteScale + 15 * context.tickCount);
        object.posY = WrapSignedWord(object.posY + 2 * context.tickCount);
        object.displayType = DisplayType::SpriteTransparent;
        object.blendWeightOrSpriteOpacity = object.health;
        return { DispatchStatus::ExecutedCase, BehaviorType::FireballImpact, 0x4F47 };
    }

    DispatchResult Function_TriggerObject(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::TriggerObject, 0x4F84 };
    }

    DispatchResult Function_ForceWeaponSelection(RawGameObject& object, GameContext& context)
    {
        context.selectedWeapon = context.requestedWeapon = static_cast<WeaponType>(object.itemValue & 255);
        return { DispatchStatus::ExecutedCase, BehaviorType::ForceWeaponSelection, 0x4FEB };
    }

    DispatchResult Function_WitchHead(RawGameObject& object, GameContext& context)
    {
        auto target = GetVectorAngle(WrapSignedWord(context.soundListenerX - object.posX),
            WrapSignedWord(context.soundListenerZ - object.posZ));
        NormalizeAnglePair(object.winY, target);
        if (context.tickCount > context.Random(25)) { object.velocityY = static_cast<std::int16_t>(context.Random(4) + 2); }
        const auto delta = GetStepToward(object.winY, target, WrapSignedWord(object.velocityY * context.tickCount));
        if (delta != 0 && object.winX < 10) { object.winY = WrapSignedWord(object.winY + delta); }
        else if (context.tickCount > context.Random(20)) { object.velocityY = 0; }
        if (context.playerCollisionSlot > 0)
        {
            context.DescribeShopItem(context.worldObjects.at(context.playerCollisionSlot));
        }
        object.generalPurpose2 = static_cast<std::uint16_t>(object.generalPurpose2 +
            GetStepToward(WrapSignedWord(object.generalPurpose2), context.playerCollisionSlot > 0 ? 256 : 0,
                WrapSignedWord(context.tickCount * 10)));
        object.winX = WrapSignedWord(static_cast<std::int32_t>(
            static_cast<std::uint32_t>(Cosine(WrapSignedWord(object.generalPurpose2)) * 27) >> 16) + 30);
        object.blendWeightOrSpriteOpacity = WrapSignedWord(
            object.blendWeightOrSpriteOpacity + object.velocityX * context.tickCount);
        if (object.blendWeightOrSpriteOpacity > 200)
        {
            object.blendWeightOrSpriteOpacity = 0;
            object.velocityX = 0;
        }
        if (context.tickCount > context.Random(120)) { object.velocityX = 25; }
        return { DispatchStatus::ExecutedCase, BehaviorType::WitchHead, 0x5007 };
    }

    DispatchResult Function_BuyItem(RawGameObject& object, GameContext& context)
    {
        if (object.itemValue < 2 || static_cast<std::size_t>(object.itemValue) >= context.worldObjects.size())
        {
            throw std::out_of_range("Shop stock must reference a world slot 2..165.");
        }
        auto& stock = context.worldObjects[object.itemValue];
        if (object.behaviorParameter >= context.weaponInventorySlots.size())
        {
            throw std::out_of_range("Shop ownership selector must be in 0..31.");
        }
        if (object.behaviorParameter > 0 && context.weaponInventorySlots[object.behaviorParameter] > 0)
        {
            stock.collisionType = CollisionType::None;
            context.SetObjectHealth(object, 0);
        }
        const int angle = ((object.winY - context.GetPlayerHeading()) & 511) >> 4;
        object.collisionMask = angle < 4 || angle > 28 ? 1 : 0;
        if (context.GetSoundDistanceAt(object.posX, object.posZ) > 50 && WrapSignedWord(object.generalPurpose3) > 0)
        {
            object.generalPurpose3 = static_cast<std::uint16_t>(object.generalPurpose3 - context.tickCount);
        }
        if (stock.health != 1000) { context.SetObjectHealth(object, 0); }
        return { DispatchStatus::ExecutedCase, BehaviorType::BuyItem, 0x501A };
    }

    DispatchResult Function_WitchMouthAnimation(RawGameObject& object, GameContext& context)
    {
        object.generalPurpose1 = context.IsBottomUserMessageActive() ? context.Random(20) : 1;
        AdvanceTrackedTexture(object, context);
        if (context.Random(20) == 0) { object.behaviorParameter = static_cast<std::uint8_t>(context.Random(3) + 3); }
        return { DispatchStatus::ExecutedCase, BehaviorType::WitchMouthAnimation, 0x502D };
    }

    DispatchResult Function_TutorialTrigger(RawGameObject& object, GameContext& context)
    {
        if (context.OverlapsPlayer(object) && context.FindCollidingObject(object) < 2)
        {
            context.HandleTextPromptContact(object);
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::TutorialTrigger, 0x5083 };
    }

    DispatchResult Function_SkullInCutScene(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::SkullInCutScene, 0x50AF };
    }

    DispatchResult Function_Behavior_255(RawGameObject&, GameContext&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_255, 0x50C1 };
    }

    DispatchResult Function_Behavior_300(RawGameObject& object, GameContext&)
    {
        object.pickupType = PickupType::Type_3; object.behavior = BehaviorType::None;
        object.itemValue = 5; object.collisionRadiusXZ = 22;
        return { DispatchStatus::ExecutedCase, BehaviorType::Behavior_300, 0x5108 };
    }

    DispatchResult Function_DisabledSentinel(RawGameObject& object, GameContext& context)
    {
        context.SetObjectHealth(object, 0);
        return { DispatchStatus::ExecutedCase, BehaviorType::DisabledSentinel, 0x1156 };
    }

}
