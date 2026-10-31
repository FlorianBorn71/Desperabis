#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <algorithm>
#include <cstdlib>

namespace Desperabis
{
    namespace
    {
        std::int16_t PlayerAngle(const RawGameObject& object, const GameContext& context)
        {
            return GetVectorAngle(WrapSignedWord(context.soundListenerX - object.posX),
                WrapSignedWord(context.soundListenerZ - object.posZ));
        }

        void Steer(RawGameObject& object, std::int16_t step)
        {
            auto target = WrapSignedWord(object.generalPurpose2);
            NormalizeAnglePair(object.winY, target);
            object.generalPurpose2 = static_cast<std::uint16_t>(target);
            object.winY = WrapSignedWord(object.winY + GetStepToward(object.winY, target, step));
        }

        std::uint16_t TurnAtWalls(RawGameObject& object, GameContext& context,
            std::int16_t correctionStep, std::int16_t turnSpeed)
        {
            const auto turn = WrapSignedWord(context.RandomSigned(1) * 64);
            std::uint16_t center = 0;
            int attempts = 0;
            std::int16_t x = 0;
            std::int16_t z = 0;
            do
            {
                x = 0;
                z = 0;
                ++attempts;
                context.QueryCollisionMapNeighborhood(WrapSignedWord(object.posX + object.velocityX),
                    WrapSignedWord(object.posZ + object.velocityZ), correctionStep, x, z, center, 0x38);
                if (x != 0 || z != 0)
                {
                    object.generalPurpose2 = static_cast<std::uint16_t>(
                        WrapSignedWord(object.generalPurpose2 + turn));
                    GetDirectionDisplacement(object.velocityX, object.velocityZ,
                        WrapSignedWord(object.generalPurpose2), turnSpeed);
                }
            } while ((x != 0 || z != 0) && attempts <= 15);
            if (attempts > 15) { context.HandleBlockedObject(object); }
            return center;
        }

        std::int16_t SineDisplacement(std::uint16_t angle, int scale)
        {
            const auto product = static_cast<std::int64_t>(Sine(WrapSignedWord(angle))) * scale;
            return WrapSignedWord(static_cast<std::int32_t>(
                product >= 0 ? product / 65536 : -((-product + 65535) / 65536)));
        }

        bool HasClearPlayerLine(const RawGameObject& object, const GameContext& context)
        {
            if (context.GetSoundDistanceAt(object.posX, object.posZ) > 1000) { return false; }
            const auto dx = WrapSignedWord(object.posX - context.soundListenerX);
            const auto dz = WrapSignedWord(object.posZ - context.soundListenerZ);
            for (int sample = 2; sample <= 30; ++sample)
            {
                const auto x = WrapSignedWord(context.soundListenerX +
                    ArithmeticShiftRight(WrapSignedWord(dx * sample), 5));
                const auto z = WrapSignedWord(context.soundListenerZ +
                    ArithmeticShiftRight(WrapSignedWord(dz * sample), 5));
                if ((context.GetCollisionMapPixelAtWorldXZ(x, z) & 0x38) != 0) { return false; }
            }
            return true;
        }

        bool SpawnSpiderSlime(RawGameObject& source, GameContext& context)
        {
            // The native lowest-health allocator may replace the source in fallback slot 165.
            const auto spider = source;
            const auto slot = context.FindFreeOrLowestHealthObjectIndex();
            const bool replacedSource = &context.worldObjects[slot] == &source;
            RawGameObject slime;
            slime.behavior = BehaviorType::FlyingArrow;
            slime.collisionType = CollisionType::FlyingArrow;
            slime.health = 100;
            slime.collisionMask = 1;
            slime.spriteScale = 35;
            slime.posX = spider.posX;
            slime.posY = std::max(spider.posY, WrapSignedWord(
                context.GetTerrainFloorHeight(spider.posX, spider.posY, spider.posZ) + 8));
            slime.posZ = spider.posZ;
            GetClampedDirectionDisplacement(slime.velocityX, slime.velocityZ, spider.winY & 511, 7);
            slime.modelId = spider.modelId;
            slime.modelAnim = 2;
            slime.animFrame = 1;
            slime.renderPlacementMode = 1;
            slime.behaviorParameter = 95;
            slime.displayType = DisplayType::VectorNormal;
            slime.collisionRadiusXZ = 7;
            slime.collisionRadiusY = 7;
            slime.velocityY = 7;
            slime.itemValue = 3;
            slime.winY = WrapSignedWord(180 - (spider.winY & 511));
            context.ReplaceWorldObject(slot, slime);
            return replacedSource;
        }
    }

    DispatchResult Function_SpiderLarge(RawGameObject& object, GameContext& context)
    {
        for (int tick = 0; tick < context.tickCount; ++tick)
        {
            if (object.health != 1000) { continue; }
            const auto speed = WrapSignedWord(std::abs(static_cast<int>(object.velocityY)));
            if (object.generalPurpose1 == 0)
            {
                const auto oldYaw = object.winY;
                const auto oldX = object.posX;
                const auto oldZ = object.posZ;
                object.collisionType = CollisionType::SpiderLarge;
                if (context.field_D500)
                {
                    object.posX = WrapSignedWord(object.posX + object.velocityX);
                    object.posZ = WrapSignedWord(object.posZ + object.velocityZ);
                }
                if (context.Random(50) == 0)
                {
                    object.velocityY = 1;
                    object.velocityX = 0;
                    object.velocityZ = 0;
                }
                object.posY = WrapSignedWord(
                    context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ) + 10);
                if (context.Random(15) == 0 || WrapSignedWord(object.generalPurpose2) < 0)
                {
                    object.generalPurpose2 = static_cast<std::uint16_t>(
                        WrapSignedWord(object.winY + context.RandomSigned(20)) & 511);
                    GetClampedDirectionDisplacement(object.velocityX, object.velocityZ,
                        WrapSignedWord(object.generalPurpose2), speed);
                }
                if (object.velocityX != 0 || object.velocityZ != 0 ||
                    object.winY != WrapSignedWord(object.generalPurpose2))
                {
                    object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity +
                        3 * (std::abs(static_cast<int>(object.velocityX)) +
                            std::abs(static_cast<int>(object.velocityZ))) + context.Random(6));
                    if (object.blendWeightOrSpriteOpacity > 100)
                    {
                        if (object.velocityY > 2)
                        {
                            context.PlaySoundEffect(static_cast<SoundEffect>(95),
                                context.GetSoundVolumeAt(object.posX, object.posZ), 360, 2);
                        }
                        ++object.animFrame;
                        if (object.animFrame > 3) { object.animFrame = 1; }
                        object.modelFrame2 = static_cast<std::uint8_t>(object.animFrame + 1);
                        if (object.modelFrame2 > 3) { object.modelFrame2 = 1; }
                        object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity - 100);
                    }
                }
                if (context.Random(50) == 0)
                {
                    object.generalPurpose2 = static_cast<std::uint16_t>(PlayerAngle(object, context));
                }
                const auto center = TurnAtWalls(object, context, 2, 6);
                auto target = WrapSignedWord(object.generalPurpose2);
                NormalizeAnglePair(object.winY, target);
                object.generalPurpose2 = static_cast<std::uint16_t>(target);
                const auto step = GetStepToward(object.winY, target,
                    static_cast<std::int16_t>(context.Random(10) + 10));
                if (step != 0)
                {
                    object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity + 11);
                }
                object.winY = WrapSignedWord(object.winY + step);
                const auto playerYaw = PlayerAngle(object, context);
                if (context.Random(30) == 0 && object.doorLockOrGroupBitmask == 0 &&
                    WrapSignedWord(object.winY - playerYaw) / 10 == 0)
                {
                    if (SpawnSpiderSlime(object, context)) { break; }
                    object.doorLockOrGroupBitmask = 50;
                }
                else if (object.doorLockOrGroupBitmask > 0) { --object.doorLockOrGroupBitmask; }
                if (WrapSignedWord(object.winY - playerYaw) / 20 == 0 && object.velocityY < 6)
                {
                    object.velocityY = WrapSignedWord(object.velocityY + 2);
                }
                else if (object.velocityY > 1) { --object.velocityY; }
                if ((oldYaw & 511) != (object.winY & 511))
                {
                    GetClampedDirectionDisplacement(object.velocityX, object.velocityZ,
                        WrapSignedWord(object.generalPurpose2), speed);
                }
                if (center == 1) { object.posX = oldX; object.posZ = oldZ; }
            }
            else if (object.generalPurpose1 == 1)
            {
                object.winX = 0;
                object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity + 1);
                if (object.blendWeightOrSpriteOpacity > 0) { object.behavior = BehaviorType::None; }
                object.velocityY = WrapSignedWord(object.velocityY - 1);
                object.posY = WrapSignedWord(object.posY + object.velocityY);
                const auto floor = WrapSignedWord(
                    context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ) + 10);
                if (object.posY < floor) { object.velocityY = 0; object.posY = floor; }
                object.winZ = WrapSignedWord(object.winZ + GetStepToward(object.winZ, 256, 17));
            }
            else if (object.generalPurpose1 == 2)
            {
                object.generalPurpose2 = static_cast<std::uint16_t>(-2);
                object.generalPurpose1 = 3;
            }
            else if (object.generalPurpose1 == 3)
            {
                if (context.Random(10) == 0)
                {
                    object.generalPurpose2 = static_cast<std::uint16_t>(context.Random(10) - 3);
                }
                object.blendWeightOrSpriteOpacity = std::max<std::int16_t>(-20,
                    WrapSignedWord(object.blendWeightOrSpriteOpacity + object.generalPurpose2));
                if (object.blendWeightOrSpriteOpacity > 100)
                {
                    ++object.animFrame;
                    ++object.modelFrame2;
                    object.blendWeightOrSpriteOpacity = 0;
                    object.generalPurpose2 = 0;
                    if (object.animFrame == 5)
                    {
                        object.generalPurpose1 = 4;
                        object.modelFrame2 = 2;
                        object.modelAnim = 1;
                        object.animFrame = 1;
                        object.velocityY = 0;
                    }
                }
            }
            else if (object.generalPurpose1 == 4)
            {
                object.winX = WrapSignedWord(object.winX + GetStepToward(object.winX,
                    object.winX > 256 ? 512 : 0, 7));
                object.blendWeightOrSpriteOpacity = -100;
                object.velocityY = std::max<std::int16_t>(-5, WrapSignedWord(object.velocityY - 1));
                const auto floor = context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ);
                object.posY = std::max(floor, WrapSignedWord(object.posY + object.velocityY));
                if ((object.winX & 511) == 0 && object.posY == floor) { object.generalPurpose1 = 0; }
            }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::SpiderLarge, 0x1D25 };
    }

    DispatchResult Function_Harvestman(RawGameObject& object, GameContext& context)
    {
        for (int tick = 0; tick < context.tickCount; ++tick)
        {
            switch (object.generalPurpose1)
            {
            case 0:
                object.collisionType = CollisionType::Harvestman;
                object.collisionRadiusXZ = 8;
                object.collisionRadiusY = 5;
                if (context.field_D500)
                {
                    object.posX = WrapSignedWord(object.posX + object.velocityX);
                    object.posY = WrapSignedWord(object.posY + object.velocityY);
                    object.posZ = WrapSignedWord(object.posZ + object.velocityZ);
                }
                if (context.Random(50) == 0) { object.velocityX = 0; object.velocityZ = 0; }
                object.posY = WrapSignedWord(
                    context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ) + 9);
                if (context.Random(15) == 0)
                {
                    object.generalPurpose2 = static_cast<std::uint16_t>(
                        WrapSignedWord(object.winY + context.RandomSigned(20)) & 511);
                    GetClampedDirectionDisplacement(object.velocityX, object.velocityZ,
                        WrapSignedWord(object.generalPurpose2), 2);
                }
                if (object.velocityX != 0 || object.velocityZ != 0 ||
                    object.winY != WrapSignedWord(object.generalPurpose2))
                {
                    object.blendWeightOrSpriteOpacity = WrapSignedWord(
                        object.blendWeightOrSpriteOpacity + 8 + context.Random(6));
                    if (object.blendWeightOrSpriteOpacity > 100)
                    {
                        object.animFrame = object.modelFrame2;
                        do { object.modelFrame2 = static_cast<std::uint8_t>(context.Random(4) + 1); }
                        while (object.modelFrame2 == object.animFrame);
                        object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity - 100);
                    }
                }
                if (context.Random(70) == 0)
                {
                    object.generalPurpose2 = static_cast<std::uint16_t>(PlayerAngle(object, context));
                }
                TurnAtWalls(object, context, 4, 2);
                Steer(object, 16);
                break;
            case 1:
            {
                auto height = static_cast<std::int16_t>(9);
                if (object.winX < 200) { object.winX = WrapSignedWord(object.winX + GetStepToward(object.winX, 0, 11)); }
                else { height = -3; }
                const auto floor = WrapSignedWord(
                    context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ) + height);
                if (object.posY < floor) { object.velocityY = 0; object.posY = floor; }
                else
                {
                    object.velocityY = WrapSignedWord(object.velocityY - 1);
                    object.posY = WrapSignedWord(object.posY + object.velocityY);
                }
                object.blendWeightOrSpriteOpacity = WrapSignedWord(
                    object.blendWeightOrSpriteOpacity + object.generalPurpose2);
                if (object.blendWeightOrSpriteOpacity > 100)
                {
                    object.blendWeightOrSpriteOpacity = 100;
                    object.behavior = BehaviorType::None;
                }
                ++object.generalPurpose2;
                break;
            }
            case 2:
                object.generalPurpose2 = static_cast<std::uint16_t>((object.generalPurpose2 + 1) & 511);
                object.velocityX = SineDisplacement(object.generalPurpose2, 12);
                object.winY = WrapSignedWord(object.winY + object.velocityX / 3) & 511;
                if (context.Random(100) == 0) { object.blendWeightOrSpriteOpacity = -12; }
                if (object.blendWeightOrSpriteOpacity < 0) { ++object.blendWeightOrSpriteOpacity; }
                if (context.GetSoundDistanceAt(object.posX, object.posZ) < 140)
                {
                    object.generalPurpose2 = 0;
                    object.generalPurpose1 = 3;
                }
                break;
            case 3:
            {
                object.velocityY = WrapSignedWord(object.velocityY - 1);
                object.posY = WrapSignedWord(object.posY + object.velocityY);
                object.posX = WrapSignedWord(object.posX + object.velocityX);
                object.posZ = WrapSignedWord(object.posZ + object.velocityZ);
                std::uint16_t center = 0;
                context.QueryCollisionMapNeighborhood(object.posX, object.posZ, 3,
                    object.velocityX, object.velocityZ, center, 0x38);
                const auto floor = WrapSignedWord(
                    context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ) + 9);
                object.posY = std::max(object.posY, floor);
                ++object.generalPurpose2;
                const auto step = WrapSignedWord(object.generalPurpose2);
                object.winX = WrapSignedWord(object.winX + GetStepToward(object.winX, 0, step));
                object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity +
                    GetStepToward(object.blendWeightOrSpriteOpacity, 100, step));
                if (object.posY == floor && object.winX == 0) { object.generalPurpose1 = 10; }
                break;
            }
            case 10:
                object.generalPurpose2 = static_cast<std::uint16_t>(object.winY);
                object.velocityX = 0;
                object.velocityY = 0;
                object.velocityZ = 0;
                object.blendWeightOrSpriteOpacity = 0;
                object.modelFrame2 = 3;
                object.animFrame = 2;
                object.modelAnim = 1;
                object.generalPurpose1 = 0;
                break;
            default:
                if (context.GetSoundDistanceAt(object.posX, object.posZ) < 130)
                {
                    object.generalPurpose2 = 0;
                    object.generalPurpose1 = 3;
                }
                break;
            }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::Harvestman, 0x1D62 };
    }

    DispatchResult Function_Devil(RawGameObject& object, GameContext& context)
    {
        for (int tick = 0; tick < context.tickCount; ++tick)
        {
            switch (object.generalPurpose1)
            {
            case 0:
            {
                object.collisionType = CollisionType::Devil;
                const auto distance = context.GetSoundDistanceAt(object.posX, object.posZ);
                auto target = PlayerAngle(object, context);
                NormalizeAnglePair(object.winY, target);
                object.winY = WrapSignedWord(object.winY + GetStepToward(object.winY, target, 17));
                object.posY = WrapSignedWord(
                    context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ) + 23);
                object.generalPurpose2 = static_cast<std::uint16_t>(
                    (object.generalPurpose2 + 15 + context.Random(6)) & 511);
                object.blendWeightOrSpriteOpacity = WrapSignedWord(SineDisplacement(object.generalPurpose2, 12) + 12);
                if (object.winY == target && (context.Random(50) == 0 || distance < 120))
                {
                    object.generalPurpose1 = 2;
                }
                GetDirectionDisplacement(object.velocityX, object.velocityZ, object.winY, 5);
                break;
            }
            case 1:
            {
                const auto floor = WrapSignedWord(
                    context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ) + 23);
                if (object.posY < floor) { object.velocityY = 0; object.posY = floor; }
                else
                {
                    object.velocityY = WrapSignedWord(object.velocityY - 1);
                    object.posY = WrapSignedWord(object.posY + object.velocityY);
                }
                object.blendWeightOrSpriteOpacity = WrapSignedWord(
                    object.blendWeightOrSpriteOpacity - object.generalPurpose2);
                if (object.blendWeightOrSpriteOpacity < 0)
                {
                    object.blendWeightOrSpriteOpacity = 0;
                    object.behavior = BehaviorType::None;
                }
                if (object.posY == floor) { ++object.generalPurpose2; }
                break;
            }
            case 2:
                object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity + 4);
                if (object.blendWeightOrSpriteOpacity > 100) { object.generalPurpose1 = 3; }
                break;
            case 3:
                object.blendWeightOrSpriteOpacity = 100;
                if (context.Random(20) == 0) { object.generalPurpose1 = 4; }
                break;
            case 4:
                object.doorLockOrGroupBitmask = 0;
                context.PlaySoundEffect(static_cast<SoundEffect>(21),
                    context.GetSoundVolumeAt(object.posX, object.posZ),
                    static_cast<std::int16_t>(context.FindWorldObjectSlot(object)), 1);
                object.animFrame = 2;
                object.modelFrame2 = 3;
                object.blendWeightOrSpriteOpacity = 0;
                object.generalPurpose1 = 5;
                object.generalPurpose2 = 3;
                break;
            case 5:
                object.blendWeightOrSpriteOpacity = WrapSignedWord(
                    object.blendWeightOrSpriteOpacity + object.generalPurpose2);
                ++object.generalPurpose2;
                object.velocityY = 14;
                if (object.blendWeightOrSpriteOpacity > 100) { object.generalPurpose1 = 6; }
                break;
            case 6:
            {
                const auto distance = context.GetSoundDistanceAt(object.posX, object.posZ);
                if ((context.GetCollisionMapPixelAtWorldXZ(object.posX,
                    WrapSignedWord(object.posZ + 4 * object.velocityZ)) & 0x38) != 0)
                {
                    object.doorLockOrGroupBitmask = static_cast<std::int16_t>(context.Random(20) + 10);
                    object.velocityZ = WrapSignedWord(-SignWord(object.velocityZ));
                }
                if ((context.GetCollisionMapPixelAtWorldXZ(
                    WrapSignedWord(object.posX + 4 * object.velocityX), object.posZ) & 0x38) != 0)
                {
                    object.doorLockOrGroupBitmask = static_cast<std::int16_t>(context.Random(20) + 10);
                    object.velocityX = WrapSignedWord(-SignWord(object.velocityX));
                }
                object.posX = WrapSignedWord(object.posX + object.velocityX);
                object.posZ = WrapSignedWord(object.posZ + object.velocityZ);
                object.velocityY = WrapSignedWord(object.velocityY - 1);
                object.posY = WrapSignedWord(object.posY + ArithmeticShiftRight(object.velocityY, 1));
                const auto floor = WrapSignedWord(
                    context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ) + 23);
                object.blendWeightOrSpriteOpacity = 100;
                if (object.posY < floor)
                {
                    object.velocityY = 0;
                    object.posY = floor;
                    object.animFrame = 3;
                    object.modelFrame2 = 1;
                    object.blendWeightOrSpriteOpacity = 0;
                    object.generalPurpose1 = 7;
                }
                if (distance < 70)
                {
                    object.velocityX = 0;
                    object.velocityZ = 0;
                    if (object.velocityY > 0) { object.velocityY = 0; }
                    context.ApplyPlayerLifeDelta(-29491 * object.behaviorParameter);
                    context.playerHitEffectTicks = 6;
                }
                break;
            }
            case 7:
                object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity + 9);
                if (object.blendWeightOrSpriteOpacity > 100) { object.generalPurpose1 = 8; }
                break;
            case 8:
                object.blendWeightOrSpriteOpacity = 0;
                object.animFrame = 1;
                object.modelFrame2 = 2;
                object.generalPurpose2 = 0;
                object.generalPurpose1 = 0;
                object.modelAnim = 1;
                GetDirectionDisplacement(object.velocityX, object.velocityZ,
                    WrapSignedWord(object.winY + context.RandomSigned(1) * 128), 3);
                if (context.Random(3) == 0)
                {
                    object.doorLockOrGroupBitmask = static_cast<std::int16_t>(context.Random(20) + 10);
                }
                if (object.doorLockOrGroupBitmask > 0) { object.generalPurpose1 = 11; }
                break;
            case 10:
                if (HasClearPlayerLine(object, context)) { object.generalPurpose1 = 4; }
                break;
            case 11:
                object.modelAnim = 2;
                object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity + 27);
                while (object.blendWeightOrSpriteOpacity > 100)
                {
                    object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity - 100);
                    object.animFrame = static_cast<std::uint8_t>((object.animFrame & 3) + 1);
                    object.modelFrame2 = static_cast<std::uint8_t>((object.animFrame & 3) + 1);
                }
                if ((context.GetCollisionMapPixelAtWorldXZ(
                    WrapSignedWord(object.posX + SignWord(object.velocityX) * 20), object.posZ) & 0x38) != 0)
                {
                    object.velocityX = 0;
                }
                if ((context.GetCollisionMapPixelAtWorldXZ(object.posX,
                    WrapSignedWord(object.posZ + SignWord(object.velocityZ) * 20)) & 0x38) != 0)
                {
                    object.velocityZ = 0;
                }
                object.posX = WrapSignedWord(object.posX + object.velocityX);
                object.posZ = WrapSignedWord(object.posZ + object.velocityZ);
                object.doorLockOrGroupBitmask = WrapSignedWord(object.doorLockOrGroupBitmask - 1);
                if (object.doorLockOrGroupBitmask < 1) { object.generalPurpose1 = 8; }
                break;
            default:
                break;
            }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::Devil, 0x1D94 };
    }

    DispatchResult Function_FlyingArrow(RawGameObject& object, GameContext& context)
    {
        if (object.itemValue != 3 && object.itemValue != 103)
        {
            return { DispatchStatus::UnimplementedHandler, BehaviorType::FlyingArrow, 0x1E1A };
        }
        for (int tick = 0; tick < context.tickCount; ++tick)
        {
            if (object.health >= 1001) { continue; }
            object.posX = WrapSignedWord(object.posX + object.velocityX);
            object.posY = WrapSignedWord(object.posY + object.velocityY);
            object.posZ = WrapSignedWord(object.posZ + object.velocityZ);
            const bool wall = (context.GetCollisionMapPixelAtWorldXZ(object.posX, object.posZ) & 8) != 0;
            const auto floor = context.GetFloorHeight(object.posX, object.posY, object.posZ);
            if (object.itemValue == 3)
            {
                if (object.generalPurpose1 == 0) { object.velocityY = WrapSignedWord(object.velocityY - 1); }
                object.generalPurpose1 = static_cast<std::uint16_t>((object.generalPurpose1 + 1) & 1);
                if (wall)
                {
                    if (object.velocityX != 0 || object.velocityZ != 0) { object.winY = WrapSignedWord(object.winY + 180); }
                    object.velocityX = 0;
                    object.velocityZ = 0;
                    object.velocityY = WrapSignedWord(-std::abs(static_cast<int>(object.velocityY)));
                }
                if (object.posY < floor)
                {
                    object.velocityX = 0;
                    object.velocityY = 0;
                    object.velocityZ = 0;
                    object.posY = floor;
                    object.itemValue = 103;
                    object.animFrame = 3;
                    object.modelFrame2 = 2;
                    object.blendWeightOrSpriteOpacity = 0;
                }
            }
            else if (object.blendWeightOrSpriteOpacity < 100)
            {
                object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity + 6);
            }
            else { context.SetObjectHealth(object, 0); break; }
            if (context.OverlapsPlayer(object))
            {
                context.ApplyPlayerLifeDelta(-29491 * object.behaviorParameter);
                if (object.behaviorParameter > 0) { context.playerHitEffectTicks = 10; }
                object.behaviorParameter = 0;
                context.DamageObject(object, 0);
            }
            else if (wall) { context.DamageObject(object, 0); }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::FlyingArrow, 0x1E1A };
    }
}
