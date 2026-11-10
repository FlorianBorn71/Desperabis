#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <stdexcept>

namespace Desperabis
{
    namespace
    {
        RawGameObject& AttachedParent(const RawGameObject& object, GameContext& context)
        {
            const auto slot = object.doorLockOrGroupBitmask;
            if (slot < 2 || slot >= static_cast<int>(context.worldObjects.size()))
            {
                throw std::out_of_range("Attached object requires a parent slot in 2..165.");
            }
            return context.worldObjects[slot];
        }

        void PlanDogRoute(RawGameObject& object, GameContext& context)
        {
            if (WrapSignedWord(object.generalPurpose3) >= 1) { return; }
            object.velocityX = object.posX;
            object.velocityZ = object.posZ;
            auto dx = WrapSignedWord(context.soundListenerX - object.posX) / 25;
            auto dz = WrapSignedWord(context.soundListenerZ - object.posZ) / 25;
            const auto stepX = WrapSignedWord(SignWord(static_cast<std::int16_t>(dx)) * 25);
            const auto stepZ = WrapSignedWord(SignWord(static_cast<std::int16_t>(dz)) * 25);
            if (std::abs(dx) < std::abs(dz)) { dx = 0; }
            const auto clear = [&](std::int16_t x, std::int16_t z)
            {
                return (context.GetCollisionMapPixelAtWorldXZ(x, z) & 0x38) == 0;
            };
            bool movedX = false;
            for (int step = 0; step < std::abs(dx); ++step)
            {
                if (clear(WrapSignedWord(object.velocityX + stepX), object.velocityZ) &&
                    clear(WrapSignedWord(object.velocityX + 2 * stepX), object.velocityZ))
                {
                    movedX = true;
                    object.velocityX = WrapSignedWord(object.velocityX + stepX);
                }
            }
            if (movedX) { dz = 0; }
            for (int step = 0; step < std::abs(dz); ++step)
            {
                if (clear(object.velocityX, WrapSignedWord(object.velocityZ + stepZ)) &&
                    clear(object.velocityX, WrapSignedWord(object.velocityZ + 2 * stepZ)))
                {
                    object.velocityZ = WrapSignedWord(object.velocityZ + stepZ);
                }
            }
        }

        void MoveDog(RawGameObject& object)
        {
            while (object.blendWeightOrSpriteOpacity > 100)
            {
                object.blendWeightOrSpriteOpacity -= 100;
                object.animFrame = static_cast<std::uint8_t>((object.animFrame & 3) + 1);
                object.modelFrame2 = static_cast<std::uint8_t>((object.animFrame & 3) + 1);
            }
            object.posX = WrapSignedWord(object.posX +
                GetStepToward(object.posX, object.velocityX, object.doorLockOrGroupBitmask));
            object.posZ = WrapSignedWord(object.posZ +
                GetStepToward(object.posZ, object.velocityZ, object.doorLockOrGroupBitmask));
        }

        void TurnDog(RawGameObject& object)
        {
            NormalizeAnglePair(object.winY, object.velocityY);
            object.winY = WrapSignedWord(object.winY + GetStepToward(object.winY, object.velocityY, 7));
        }

        std::int16_t PlayerAngle(const RawGameObject& object, const GameContext& context)
        {
            return GetVectorAngle(WrapSignedWord(context.soundListenerX - object.posX),
                WrapSignedWord(context.soundListenerZ - object.posZ));
        }

        std::int16_t SineDisplacement(std::uint16_t angle, int scale)
        {
            const auto product = static_cast<std::int64_t>(Sine(WrapSignedWord(angle))) * scale;
            return WrapSignedWord(static_cast<std::int32_t>(
                product >= 0 ? product / 65536 : -((-product + 65535) / 65536)));
        }
    }

    DispatchResult Function_Dog(RawGameObject& object, GameContext& context)
    {
        for (int tick = 0; tick < context.tickCount; ++tick)
        {
            if (object.health != 1000) { continue; }
            const auto distance = context.GetSoundDistanceAt(object.posX, object.posZ);
            const auto play = [&](int effect)
            {
                context.PlaySoundEffect(static_cast<SoundEffect>(effect),
                    GameContext::GetPositionalSoundVolume(distance), object, 1);
            };
            if (WrapSignedWord(object.generalPurpose3) > 0) { --object.generalPurpose3; }
            switch (object.itemValue)
            {
            case 99:
                object.collisionMask = 3;
                object.displayType = DisplayType::VectorNormal;
                object.itemValue = 1;
                break;
            case 1:
                object.animFrame = 2; object.modelAnim = 1; object.modelFrame2 = 1;
                object.generalPurpose1 = 0;
                object.velocityX = object.posX; object.velocityZ = object.posZ;
                object.winY = PlayerAngle(object, context);
                if (WrapSignedWord(object.generalPurpose2) < 1)
                {
                    if (object.blendWeightOrSpriteOpacity < 22) { play(55); }
                    object.blendWeightOrSpriteOpacity = WrapSignedWord(
                        object.blendWeightOrSpriteOpacity + object.doorLockOrGroupBitmask);
                    object.doorLockOrGroupBitmask = WrapSignedWord(object.doorLockOrGroupBitmask + 2);
                }
                else
                {
                    --object.generalPurpose2;
                    const auto product = static_cast<std::int64_t>(SineTable[
                        WrapSignedWord(object.generalPurpose2 * 30) & 511]) * 10;
                    const auto offset = product >= 0 ? product / 65536 : -((-product + 65535) / 65536);
                    object.blendWeightOrSpriteOpacity = WrapSignedWord(static_cast<std::int32_t>(offset) + 10);
                }
                if (object.blendWeightOrSpriteOpacity > 200)
                {
                    object.blendWeightOrSpriteOpacity = 0;
                    object.generalPurpose2 = static_cast<std::uint16_t>(context.Random(150) + 30);
                    object.doorLockOrGroupBitmask = 5;
                }
                if (distance < 190) { object.itemValue = -2; }
                break;
            case -2:
                play(24);
                object.itemValue = 2; object.modelAnim = 2; object.animFrame = 1; object.modelFrame2 = 2;
                object.blendWeightOrSpriteOpacity = 0; object.doorLockOrGroupBitmask = 5;
                break;
            case 2:
                if ((object.posX == object.velocityX && object.posZ == object.velocityZ) ||
                    context.GetSoundDistanceAt(object.velocityX, object.velocityZ) > 200)
                {
                    PlanDogRoute(object, context);
                }
                object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity +
                    6 * object.doorLockOrGroupBitmask + context.Random(15));
                if (context.Random(20) == 0)
                {
                    object.doorLockOrGroupBitmask = static_cast<std::int16_t>(context.Random(4) + 2);
                }
                MoveDog(object);
                object.velocityY = object.posX == object.velocityX && object.posZ == object.velocityZ ?
                    PlayerAngle(object, context) : GetVectorAngle(WrapSignedWord(object.velocityX - object.posX),
                        WrapSignedWord(object.velocityZ - object.posZ));
                TurnDog(object);
                if (distance > 600) { object.itemValue = 1; }
                if (distance < 90)
                {
                    object.itemValue = 3; object.generalPurpose2 = 5; play(24);
                }
                else if (context.Random(100) == 0) { play(55); }
                break;
            case 3:
                if (distance < 60)
                {
                    context.ApplyPlayerLifeDelta(-29491 * static_cast<std::int32_t>(object.behaviorParameter));
                    context.playerHitEffectTicks = 8;
                }
                if (WrapSignedWord(object.generalPurpose1) < 0)
                {
                    object.generalPurpose1 = 0; object.itemValue = 2;
                }
                object.generalPurpose1 = static_cast<std::uint16_t>(
                    object.generalPurpose1 + ArithmeticShiftRight(WrapSignedWord(object.generalPurpose2), 1));
                --object.generalPurpose2;
                object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity + 41);
                MoveDog(object);
                object.velocityY = PlayerAngle(object, context);
                TurnDog(object);
                break;
            case 100:
                object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity +
                    ArithmeticShiftRight(WrapSignedWord(object.generalPurpose2), 1));
                ++object.generalPurpose2;
                if (object.blendWeightOrSpriteOpacity > 100)
                {
                    object.blendWeightOrSpriteOpacity = 100;
                    object.behavior = BehaviorType::None;
                }
                break;
            }
            object.posY = WrapSignedWord(context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ) +
                16 + WrapSignedWord(object.generalPurpose1));
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::Dog, 0x25CA };
    }

    DispatchResult Function_DogLeash(RawGameObject& object, GameContext& context)
    {
        if (object.itemValue > 0)
        {
            if (static_cast<std::size_t>(object.itemValue) >= context.worldObjects.size())
            {
                throw std::out_of_range("Dog leash requires a linked world slot in 1..165.");
            }
            auto& dog = context.worldObjects[object.itemValue];
            if (dog.health == 1000)
            {
                if (dog.behavior == BehaviorType::None) { object.behavior = BehaviorType::None; }
                if (dog.collisionType != CollisionType::Dog) { object.modelFrame2 = 3; }
                const auto dx = WrapSignedWord(dog.posX - object.posX);
                const auto dz = WrapSignedWord(dog.posZ - object.posZ);
                const auto distance = RoundReal48(std::sqrt(static_cast<double>(dx) * dx +
                    static_cast<double>(dz) * dz));
                object.winY = GetVectorAngle(dx, dz);
                if (distance > 85)
                {
                    dog.velocityX = WrapSignedWord(dog.velocityX + GetStepToward(dog.velocityX, object.posX, 20));
                    dog.velocityZ = WrapSignedWord(dog.velocityZ + GetStepToward(dog.velocityZ, object.posZ, 20));
                    dog.generalPurpose3 = 12;
                }
                object.blendWeightOrSpriteOpacity = static_cast<std::int16_t>(std::nearbyint(std::min(distance, 100.0)));
            }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::DogLeash, 0x25FE };
    }

    DispatchResult Function_Ghost(RawGameObject& object, GameContext& context)
    {
        for (int tick = 0; tick < context.tickCount; ++tick)
        {
            if (object.health != 1000) { continue; }
            const auto distance = context.GetSoundDistanceAt(object.posX, object.posZ);
            object.winZ = static_cast<std::int16_t>((object.winZ + 7) & 511);
            if (object.generalPurpose1 != 4)
            {
                object.posY = WrapSignedWord(context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ) +
                    32 + SineDisplacement(static_cast<std::uint16_t>(object.winZ), 4));
            }
            const auto& player = context.worldObjects[1];
            const auto speed = WrapSignedWord(object.generalPurpose3);
            object.velocityX = GetStepToward(object.posX,
                WrapSignedWord(context.soundListenerX + ArithmeticShiftRight(player.velocityX, 1)), speed);
            object.velocityZ = GetStepToward(object.posZ,
                WrapSignedWord(context.soundListenerZ + ArithmeticShiftRight(player.velocityZ, 1)), speed);
            switch (object.generalPurpose1)
            {
            case 1:
            {
                object.velocityY = object.velocityX;
                object.velocityX = object.velocityZ;
                object.velocityZ = WrapSignedWord(-object.velocityY);
                if (context.Random(80) == 0) { object.generalPurpose1 = 0; }
                break;
            }
            case 2:
                object.velocityY = object.velocityX;
                object.velocityX = WrapSignedWord(-object.velocityZ);
                object.velocityZ = object.velocityY;
                if (context.Random(80) == 0) { object.generalPurpose1 = 0; }
                break;
            case 3:
                object.velocityX = object.velocityZ = 0;
                if (context.Random(50) == 0) { object.generalPurpose1 = context.Random(3); }
                break;
            case 4:
                object.velocityX = object.velocityZ = 0;
                break;
            default:
                if (context.Random(80) == 0) { object.generalPurpose1 = static_cast<std::uint16_t>(context.Random(3) + 1); }
                break;
            }
            object.generalPurpose3 = distance >= 0 && distance <= 60 ? 4 :
                distance >= 0 && distance <= 100 ? 2 : 1;
            const auto targetOpacity = static_cast<std::int16_t>(distance > 70 && distance < 180 ? 100 : 0);
            object.doorLockOrGroupBitmask = WrapSignedWord(object.doorLockOrGroupBitmask +
                GetStepToward(object.doorLockOrGroupBitmask, targetOpacity, 1));
            switch (object.itemValue)
            {
            case 0:
            {
                object.blendWeightOrSpriteOpacity = static_cast<std::int16_t>(
                    static_cast<std::uint16_t>(object.doorLockOrGroupBitmask) >> 3);
                if (distance < 90 && object.blendWeightOrSpriteOpacity > 0) { context.ApplyPlayerLifeDelta(-4 * 29491); }
                object.collisionMask = 64;
                const auto slot = context.FindCollidingObject(object);
                object.collisionMask = 0;
                if (slot > 1 && context.worldObjects[slot].collisionType == CollisionType::LightRayKillsGhost)
                {
                    object.itemValue = 20;
                }
                break;
            }
            case 20:
                context.PlaySoundEffect(static_cast<SoundEffect>(59), GameContext::GetPositionalSoundVolume(distance), object, 1);
                object.displayType = DisplayType::Type3;
                object.generalPurpose1 = 4;
                object.collisionMask = 0;
                object.generalPurpose2 = 28;
                object.itemValue = 21;
                object.blendWeightOrSpriteOpacity = 100;
                break;
            case 21:
                object.blendWeightOrSpriteOpacity = ArithmeticShiftRight(WrapSignedWord(object.generalPurpose2), 2);
                --object.generalPurpose2;
                if (object.blendWeightOrSpriteOpacity < -2) { object.itemValue = 22; }
                break;
            case 22:
                object.generalPurpose2 = 28;
                object.displayType = DisplayType::SpriteTransparent;
                object.itemValue = 23;
                object.animFrame = object.modelFrame2;
                object.blendWeightOrSpriteOpacity = 100;
                for (int part = -2; part <= 2; ++part)
                {
                    const auto slot = context.FindFreeObjectIndex();
                    // Allocation may clear this source when the fallback slot is the ghost itself.
                    auto fragment = object;
                    fragment.animFrame = 3;
                    fragment.velocityZ = 0;
                    fragment.behavior = BehaviorType::SmokeFading;
                    fragment.velocityY = static_cast<std::int16_t>(part * 50 - 90);
                    fragment.health = static_cast<std::int16_t>(context.Random(10) + 50);
                    if (part == 2)
                    {
                        fragment.animFrame = 4;
                        fragment.health = 62;
                        fragment.velocityZ = fragment.velocityY = 0;
                    }
                    else
                    {
                        fragment.spriteScale = static_cast<std::uint8_t>(30 - part * 6);
                        fragment.winY = static_cast<std::int16_t>(context.Random(2));
                        fragment.posY = WrapSignedWord(fragment.posY + part * 5 + 11);
                    }
                    context.ReplaceWorldObject(slot, fragment);
                }
                break;
            case 23:
                object.blendWeightOrSpriteOpacity = ArithmeticShiftRight(WrapSignedWord(object.generalPurpose2), 2);
                --object.generalPurpose2;
                if (object.blendWeightOrSpriteOpacity < 1) { context.SetObjectHealth(object, 0); }
                break;
            default:
                object.itemValue = 0;
                break;
            }
            const auto clear = [&](std::int16_t x, std::int16_t z)
            {
                return (context.GetCollisionMapPixelAtWorldXZ(x, z) & 0x38) == 0;
            };
            const auto nextX = WrapSignedWord(object.posX + object.velocityX);
            if (clear(nextX, object.posZ)) { object.posX = nextX; }
            const auto nextZ = WrapSignedWord(object.posZ + object.velocityZ);
            if (clear(object.posX, nextZ)) { object.posZ = nextZ; }
            if (!clear(object.posX, object.posZ)) { context.SetObjectHealth(object, 0); }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::Ghost, 0x2669 };
    }

    DispatchResult Function_BonesToGrab(RawGameObject& object, GameContext& context)
    {
        if (object.health != 1000) { object.collisionMask = 0; }
        else
        {
            object.collisionMask = 3;
            const auto slot = context.FindCollidingObject(object);
            object.posY = context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ);
            if (slot > 0 && context.worldObjects[slot].collisionType == CollisionType::Dog)
            {
                auto& dog = context.worldObjects[slot];
                dog.modelAnim = 4; dog.animFrame = 1; dog.modelFrame2 = 2;
                dog.behavior = BehaviorType::IdleAnimation; dog.itemValue = 11; dog.collisionMask = 0;
                object.collisionMask = 0; object.behavior = BehaviorType::None; object.renderPlacementMode = 6;
            }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::BonesToGrab, 0x299A };
    }

    DispatchResult Function_AttachableDamageObject(RawGameObject& object, GameContext& context)
    {
        if (object.health == 1000 && object.doorLockOrGroupBitmask > 1)
        {
            auto& parent = AttachedParent(object, context);
            if (parent.health > 1000 && parent.health < 2000)
            {
                context.SetObjectHealth(object, parent.health);
                object.displayType = parent.displayType;
            }
            object.winY = WrapSignedWord(parent.winY + object.velocityY);
            std::int16_t x, z;
            GetDirectionDisplacement(x, z, WrapSignedWord(object.winY - 128), object.itemValue);
            object.posX = WrapSignedWord(parent.posX + x); object.posY = parent.posY;
            object.posZ = WrapSignedWord(parent.posZ + z);
            switch (object.generalPurpose1)
            {
            case 0:
                object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity +
                    (context.Random(8) + 7) * context.tickCount);
                break;
            case 1:
            {
                const auto slot = context.FindCollidingObject(object);
                if (slot > 1) { context.DamageObject(context.worldObjects[slot], WrapSignedWord(200 * context.tickCount)); }
                object.winX = static_cast<std::int16_t>((object.winX + context.tickCount * object.generalPurpose3) & 511);
                break;
            }
            case 2:
                if (context.Random(20) == 0) { object.generalPurpose2 = context.Random(10); }
                object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity +
                    context.tickCount * object.generalPurpose2);
                break;
            case 3:
                object.velocityY = static_cast<std::int16_t>(
                    (object.velocityY + context.tickCount * object.generalPurpose3) & 511);
                break;
            }
            while (object.blendWeightOrSpriteOpacity > 100)
            {
                object.blendWeightOrSpriteOpacity -= 100;
                object.animFrame = object.modelFrame2;
                do { object.modelFrame2 = static_cast<std::uint8_t>(context.Random(4) + 1); }
                while (object.modelFrame2 == object.animFrame);
            }
            if (parent.groupIndexOrLock < 1) { context.DamageObject(object, 500); }
        }
        else if (object.health >= 1001 && object.health <= 1999)
        {
            if (object.doorLockOrGroupBitmask > 1)
            {
                context.SetObjectHealth(object, AttachedParent(object, context).health);
            }
        }
        else if (object.health == 2000)
        {
            object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity + 3 * context.tickCount);
            const auto floor = context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ);
            object.posY = WrapSignedWord(object.posY +
                GetStepToward(object.posY, floor, WrapSignedWord(2 * context.tickCount)));
            if (object.blendWeightOrSpriteOpacity > 100) { object.behavior = BehaviorType::None; }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::AttachableDamageObject, 0x2B63 };
    }

    DispatchResult Function_KrakenHead(RawGameObject& object, GameContext& context)
    {
        if (context.GetSoundDistanceAt(object.posX, object.posZ) >= WrapSignedWord(object.generalPurpose1) ||
            object.health != 1000) { return { DispatchStatus::ExecutedCase, BehaviorType::KrakenHead, 0x2B76 }; }
        if (context.GetSoundDistanceAt(object.posX, object.posZ) < 50)
        {
            context.playerHitEffectTicks = 8;
            context.ApplyPlayerLifeDelta(-29491 * static_cast<std::int32_t>(object.behaviorParameter));
        }
        if (object.generalPurpose1 != 1500)
        {
            context.PlaySoundEffect(static_cast<SoundEffect>(67), GameContext::GetPositionalSoundVolume(
                ArithmeticShiftRight(context.GetSoundDistanceAt(object.posX, object.posZ), 1)), object, 1);
        }
        object.generalPurpose1 = 1500;
        const auto speed = WrapSignedWord(1 + std::abs(static_cast<int>(
            SineDisplacement(object.generalPurpose2, WrapSignedWord(object.itemValue * context.tickCount)))));
        const auto targetX = WrapSignedWord(100 * object.velocityX + 50);
        const auto targetZ = WrapSignedWord(100 * WrapSignedWord(41 - object.velocityZ) + 50);
        object.posX = WrapSignedWord(object.posX + GetStepToward(object.posX, targetX, speed));
        object.posZ = WrapSignedWord(object.posZ + GetStepToward(object.posZ, targetZ, speed));
        if (object.winY > -1)
        {
            object.winY = static_cast<std::int16_t>(
                (object.winY + context.Random(static_cast<std::uint16_t>(3 * speed))) & 511);
        }
        object.generalPurpose2 = static_cast<std::uint16_t>(
            (object.generalPurpose2 + speed * (context.Random(3) + 4)) & 511);
        object.posY = WrapSignedWord(context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ) +
            object.velocityY + std::abs(static_cast<int>(SineDisplacement(object.generalPurpose2, 5))));
        object.blendWeightOrSpriteOpacity = static_cast<std::int16_t>(
            WrapSignedWord(object.blendWeightOrSpriteOpacity + 5 * speed) % 200);
        if (object.posX == targetX && object.posZ == targetZ)
        {
            if (!context.level) { throw std::runtime_error("Kraken navigation requires level geometry."); }
            auto dx = WrapSignedWord(context.soundListenerX - object.posX);
            auto dz = WrapSignedWord(object.posZ - context.soundListenerZ);
            if (std::abs(static_cast<int>(dx)) > std::abs(static_cast<int>(dz))) { dz = 0; }
            else { dx = 0; }
            dx = SignWord(dx); dz = SignWord(dz);
            constexpr std::array<std::array<int,2>,4> neighbors{{{0,1},{1,0},{0,-1},{-1,0}}};
            bool found = false;
            for (int attempt = 0; attempt < 8; ++attempt)
            {
                const auto& neighbor = neighbors[attempt & 3];
                const int column = object.velocityX + neighbor[0] - 1;
                const int row = object.velocityZ + neighbor[1] - 1;
                if (column < 0 || column >= static_cast<int>(Level::Width) ||
                    row < 0 || row >= static_cast<int>(Level::Height))
                {
                    throw std::out_of_range("Kraken navigation is outside the native 50x40 surface grid.");
                }
                if ((context.level->surfaceFlags[row][column] & 31) >= 2 ||
                    (attempt <= 4 && (neighbor[0] != dx || neighbor[1] != dz))) { continue; }
                object.velocityX = WrapSignedWord(object.velocityX + neighbor[0]);
                object.velocityZ = WrapSignedWord(object.velocityZ + neighbor[1]);
                found = true;
                break;
            }
            if (!found) { throw std::runtime_error("Kraken has no traversable neighboring tile; native navigation would loop forever."); }
        }
        if (WrapSignedWord(object.generalPurpose3) < 3) { context.DamageObject(object, 100); }
        return { DispatchStatus::ExecutedCase, BehaviorType::KrakenHead, 0x2B76 };
    }

    namespace
    {
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

        bool HasClearPlayerLine(const RawGameObject& object, const GameContext& context)
        {
            return context.HasClearPlayerLine(object.posX, object.posZ);
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

        bool UpdateSpiderOrScorpion(RawGameObject& object, GameContext& context,
            std::int16_t speed, std::int16_t floorOffset)
        {
            if (object.health != 1000) { return false; }
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
                    context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ) + floorOffset);
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
                    if (SpawnSpiderSlime(object, context)) { return true; }
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
                    context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ) + floorOffset);
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
            return false;
        }
    }

    DispatchResult Function_SpiderLarge(RawGameObject& object, GameContext& context)
    {
        for (int tick = 0; tick < context.tickCount; ++tick)
        {
            if (UpdateSpiderOrScorpion(object, context,
                WrapSignedWord(std::abs(static_cast<int>(object.velocityY))), 10)) { break; }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::SpiderLarge, 0x1D25 };
    }

    DispatchResult Function_Scorpion(RawGameObject& object, GameContext& context)
    {
        for (int tick = 0; tick < context.tickCount; ++tick)
        {
            if (UpdateSpiderOrScorpion(object, context, 2, 0)) { break; }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::Scorpion, 0x1E66 };
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
        if (object.itemValue != 0 && object.itemValue != 11 && object.itemValue != 3 && object.itemValue != 103 &&
            object.itemValue != 4 && object.itemValue != 5 && object.itemValue != 6 && object.itemValue != 7 &&
            object.itemValue != 8 && object.itemValue != 9)
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
            auto floor = object.itemValue >= 4 && object.itemValue <= 6 ?
                context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ) :
                context.GetFloorHeight(object.posX, object.posY, object.posZ);
            if (object.itemValue == 0)
            {
                if (wall) { context.SetObjectHealth(object, 0); }
            }
            else if (object.itemValue == 11)
            {
                --object.generalPurpose1;
                object.velocityY = ArithmeticShiftRight(WrapSignedWord(object.generalPurpose1), 2);
                if (object.health > 10 && object.posY < floor)
                {
                    context.SetObjectHealth(object, 0);
                    continue;
                }
                object.blendWeightOrSpriteOpacity = WrapSignedWord(
                    object.blendWeightOrSpriteOpacity + object.doorLockOrGroupBitmask);
            }
            else if (object.itemValue == 4)
            {
                if (object.spriteScale < 100)
                {
                    object.spriteScale = static_cast<std::uint8_t>(object.spriteScale + context.Random(4) + 2);
                }
                if (object.posY < floor || wall) { context.SetObjectHealth(object, 0); }
                if (object.spriteScale > 40 && object.velocityY < 0 && context.Random(5) == 0) { ++object.velocityY; }
                object.blendWeightOrSpriteOpacity = WrapSignedWord(object.health + 1);
            }
            else if (object.itemValue == 5)
            {
                object.modelAnim = static_cast<std::uint8_t>(object.modelAnim % 6 + 1);
                if (object.spriteScale < 100) { ++object.spriteScale; }
                if (object.posY < floor || wall)
                {
                    object.behavior = BehaviorType::FireballImpact;
                    context.SetObjectHealth(object, 10);
                    continue;
                }
            }
            else if (object.itemValue == 6)
            {
                if (object.health == 1000) { context.SetObjectHealth(object, static_cast<std::int16_t>(context.Random(100) + 100)); }
                if (object.spriteScale < 210) { object.spriteScale = static_cast<std::uint8_t>(object.spriteScale + 4); }
                object.blendWeightOrSpriteOpacity = WrapSignedWord(ArithmeticShiftRight(object.health, 2) + 1);
                if (wall) { context.SetObjectHealth(object, 0); }
                object.collisionMask = 2;
                const auto target = context.FindCollidingObject(object);
                if (target > 1) { context.DamageObject(context.worldObjects[target], 200); }
                object.collisionMask = 1;
            }
            else if (object.itemValue == 7)
            {
                if (object.spriteScale < 70)
                {
                    object.spriteScale = static_cast<std::uint8_t>(object.spriteScale + context.Random(3) + 1);
                }
                else { context.SetObjectHealth(object, 0); }
                object.generalPurpose2 = static_cast<std::uint16_t>(object.generalPurpose2 + context.Random(2) + 1);
                while (WrapSignedWord(object.generalPurpose2) > 6)
                {
                    object.generalPurpose2 -= 7;
                    object.velocityY = WrapSignedWord(object.velocityY + 1);
                }
                object.blendWeightOrSpriteOpacity = object.health;
                if (object.health < 18) { object.animFrame = 1; }
            }
            else if (object.itemValue == 8)
            {
                const auto threshold = 500 - static_cast<int>(context.GetSoundDistanceAt(object.posX, object.posZ));
                if (static_cast<int>(context.Random(5000)) < threshold) { object.itemValue = 9; }
            }
            else if (object.itemValue == 9)
            {
                object.velocityY = WrapSignedWord(object.velocityY - 1);
                if (floor < -300) { floor = -1000; }
                if (object.posY < floor)
                {
                    object.posY = floor;
                    object.velocityY = ArithmeticShiftRight(
                        WrapSignedWord(std::abs(static_cast<int>(object.velocityY))), 2);
                    if (object.velocityY < 4)
                    {
                        object.collisionType = CollisionType::None;
                        object.behavior = BehaviorType::None;
                    }
                    context.PlaySoundEffect(static_cast<SoundEffect>(37),
                        GameContext::GetSoundVolume(static_cast<std::int16_t>(context.Random(500))), object, 1);
                }
                object.behaviorParameter = static_cast<std::uint8_t>(std::abs(static_cast<int>(object.velocityY)) * 4);
            }
            else if (object.itemValue == 3)
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
            else if (object.itemValue == 103 && object.blendWeightOrSpriteOpacity < 100)
            {
                object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity + 6);
            }
            else if (object.itemValue == 103) { context.SetObjectHealth(object, 0); break; }
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
