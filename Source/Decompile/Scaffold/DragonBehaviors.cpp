#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <algorithm>
#include <stdexcept>

namespace Desperabis
{
    bool GameContext::HasClearPlayerLine(std::int16_t x, std::int16_t z) const
    {
        if (GetSoundDistanceAt(x, z) > 1000) { return false; }
        const auto dx = WrapSignedWord(x - soundListenerX), dz = WrapSignedWord(z - soundListenerZ);
        for (int sample = 2; sample <= 30; ++sample)
        {
            const auto sx = WrapSignedWord(soundListenerX + ArithmeticShiftRight(WrapSignedWord(dx * sample), 5));
            const auto sz = WrapSignedWord(soundListenerZ + ArithmeticShiftRight(WrapSignedWord(dz * sample), 5));
            if ((GetCollisionMapPixelAtWorldXZ(sx, sz) & 0x38) != 0) { return false; }
        }
        return true;
    }

    namespace
    {
        std::int16_t ScaledTrig(std::int32_t value, int scale)
        {
            const auto product = static_cast<std::int64_t>(value) * scale;
            return WrapSignedWord(static_cast<std::int32_t>(product >= 0 ? product / 65536 :
                -((-product + 65535) / 65536)));
        }

        bool SpawnDragonShot(RawGameObject& source, GameContext& context, bool breath,
            const std::array<std::int16_t, 3>& position, std::int16_t angle)
        {
            const auto dragon = source;
            const auto slot = context.FindFreeOrLowestHealthObjectIndex();
            RawGameObject shot;
            shot.health = breath ? WrapSignedWord(dragon.itemValue / 3 + 7) : 500;
            shot.behavior = BehaviorType::FlyingArrow;
            shot.collisionType = CollisionType::FlyingArrow;
            shot.collisionMask = 1;
            shot.modelId = 5; shot.modelAnim = static_cast<std::uint8_t>(context.Random(6) + 1); shot.animFrame = 3;
            shot.posX = position[0]; shot.posY = position[1]; shot.posZ = position[2];
            GetClampedDirectionDisplacement(shot.velocityX, shot.velocityZ, angle & 511, breath ? 4 : 8);
            shot.velocityY = breath ? -2 : -1;
            shot.itemValue = breath ? 4 : 5;
            shot.behaviorParameter = breath ? 7 : 50;
            shot.collisionRadiusXZ = shot.collisionRadiusY = breath ? 25 : 20;
            shot.spriteScale = breath ? 3 : 40;
            shot.displayType = breath ? DisplayType::SpriteTransparent : DisplayType::SpriteTorchlight;
            shot.blendWeightOrSpriteOpacity = breath ? 10 : 0;
            shot.renderPlacementMode = 1;
            shot.posY = std::max(shot.posY,
                WrapSignedWord(context.GetTerrainFloorHeight(shot.posX, shot.posY, shot.posZ) + 8));
            context.ReplaceWorldObject(slot, shot);
            return &context.worldObjects[slot] == &source;
        }

        void WalkDragon(RawGameObject& object, GameContext& context, std::int16_t oldHeading)
        {
            object.posY = WrapSignedWord(context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ) + 50);
            GetClampedDirectionDisplacement(object.velocityX, object.velocityZ, WrapSignedWord(object.generalPurpose1), 3);
            if (object.velocityX == 0) { object.velocityX = SignWord(WrapSignedWord(context.soundListenerX - object.posX)); }
            if (object.velocityZ == 0) { object.velocityZ = SignWord(WrapSignedWord(context.soundListenerZ - object.posZ)); }
            int phaseStep = oldHeading == WrapSignedWord(object.generalPurpose1) &&
                object.velocityX == 0 && object.velocityZ == 0 ? 0 : 6;
            if (object.animFrame == 2 || object.animFrame == 4)
            {
                const auto x = WrapSignedWord(object.posX + object.velocityX);
                if (context.GetRoomHeight(x, object.posZ) > 150) { object.posX = x; }
                else { phaseStep -= 2; }
                const auto z = WrapSignedWord(object.posZ + object.velocityZ);
                if (context.GetRoomHeight(object.posX, z) > 150) { object.posZ = z; }
                else { phaseStep -= 2; }
            }
            if (phaseStep > 0) { object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity + phaseStep); }
            while (object.blendWeightOrSpriteOpacity > 100)
            {
                object.blendWeightOrSpriteOpacity -= 100;
                object.doorLockOrGroupBitmask = 1;
                object.animFrame = static_cast<std::uint8_t>((object.animFrame & 3) + 1);
                object.modelFrame2 = static_cast<std::uint8_t>((object.animFrame & 3) + 1);
                if (object.animFrame == 2 || object.animFrame == 4)
                {
                    context.PlaySoundEffect(static_cast<SoundEffect>(23),
                        context.GetSoundVolumeAt(object.posX, object.posZ), object, 2);
                }
            }
            const auto phase = WrapSignedWord(object.animFrame * 100 + object.blendWeightOrSpriteOpacity);
            object.winZ = ScaledTrig(SineTable[phase & 511], -5);
            object.winY = WrapSignedWord(object.generalPurpose1 + ScaledTrig(SineTable[(phase + 128) & 511], -10));
        }

        void SpawnDragonFragment(const RawGameObject& dragon, GameContext& context, int part)
        {
            const auto slot = context.FindFreeObjectIndex();
            if (context.worldObjects[slot].health > 0) { return; }
            RawGameObject fragment;
            fragment.modelId = dragon.modelId;
            fragment.modelAnim = static_cast<std::uint8_t>(context.Random(3) + 4);
            fragment.health = 1000; fragment.displayType = dragon.displayType; fragment.renderPlacementMode = 1;
            fragment.velocityX = context.RandomSigned(7);
            fragment.velocityY = static_cast<std::int16_t>(context.Random(6) + 5);
            fragment.velocityZ = context.RandomSigned(7);
            fragment.behavior = BehaviorType::BouncingObject; fragment.itemValue = 50; fragment.soundEffectId = 3;
            fragment.posX = dragon.posX; fragment.posY = dragon.posY; fragment.posZ = dragon.posZ;
            fragment.winX = static_cast<std::int16_t>(context.Random(512));
            fragment.winY = static_cast<std::int16_t>(context.Random(512));
            fragment.winZ = static_cast<std::int16_t>(context.Random(512));
            if (part < 2)
            {
                fragment.renderPlacementMode = 3; fragment.modelAnim = 3; fragment.animFrame = 1;
                fragment.velocityX = fragment.velocityY = fragment.velocityZ = 0;
                fragment.winX = 100; fragment.winZ = 0; fragment.winY = part == 1 ? 256 : 0;
            }
            else
            {
                fragment.animFrame = static_cast<std::uint8_t>(context.Random(2) + 1);
                fragment.modelFrame2 = static_cast<std::uint8_t>(3 - fragment.animFrame);
                fragment.generalPurpose1 = 5; fragment.blendWeightOrSpriteOpacity = -100;
            }
            context.ReplaceWorldObject(slot, fragment);
        }

        void SpawnDeathCloud(const RawGameObject& dragon, GameContext& context)
        {
            const auto slot = context.FindFreeOrLowestHealthObjectIndex();
            RawGameObject cloud;
            cloud.health = 100; cloud.collisionMask = 1; cloud.spriteScale = 35;
            cloud.behavior = BehaviorType::FlyingArrow; cloud.collisionType = CollisionType::FlyingArrow;
            const auto speed = static_cast<std::int16_t>(context.Random(3) + 2);
            const auto angle = static_cast<std::int16_t>(context.Random(512));
            cloud.posZ = WrapSignedWord(dragon.posZ + context.RandomSigned(30));
            cloud.posY = WrapSignedWord(context.GetTerrainFloorHeight(dragon.posX, dragon.posY, dragon.posZ) + context.Random(20));
            cloud.posX = WrapSignedWord(dragon.posX + context.RandomSigned(30));
            GetClampedDirectionDisplacement(cloud.velocityX, cloud.velocityZ, angle, speed);
            cloud.modelId = 5; cloud.modelAnim = static_cast<std::uint8_t>(context.Random(6) + 1); cloud.animFrame = 3;
            cloud.displayType = DisplayType::SpriteTransparent; cloud.velocityY = static_cast<std::int16_t>(context.Random(2));
            cloud.itemValue = 6; cloud.behaviorParameter = 25; cloud.collisionRadiusXZ = cloud.collisionRadiusY = 25;
            cloud.spriteScale = static_cast<std::uint8_t>(context.Random(20) + 50);
            cloud.health = static_cast<std::int16_t>(context.Random(20) + 40);
            cloud.renderPlacementMode = 3; cloud.blendWeightOrSpriteOpacity = 20;
            cloud.posY = std::max(cloud.posY,
                WrapSignedWord(context.GetTerrainFloorHeight(cloud.posX, cloud.posY, cloud.posZ) + 8));
            context.ReplaceWorldObject(slot, cloud);
        }
    }

    DispatchResult Function_Dragon(RawGameObject& object, GameContext& context)
    {
        for (int tick = 0; tick < context.tickCount; ++tick)
        {
            if (object.health != 1000) { continue; }
            const auto distance = context.GetSoundDistanceAt(object.posX, object.posZ);
            if (context.HasClearPlayerLine(object.posX, object.posZ)) { object.velocityY = 40; }
            const auto oldHeading = WrapSignedWord(object.generalPurpose1);
            object.collisionType = CollisionType::Dragon; object.spriteScale = 1;
            if (context.Random(40) == 0 && object.itemValue == 0) { object.itemValue = 50; }
            auto target = GetVectorAngle(WrapSignedWord(context.soundListenerX - object.posX),
                WrapSignedWord(context.soundListenerZ - object.posZ));
            if (object.velocityY > 0)
            {
                auto heading = WrapSignedWord(object.generalPurpose1);
                NormalizeAnglePair(heading, target);
                object.generalPurpose1 = static_cast<std::uint16_t>(
                    WrapSignedWord(heading + GetStepToward(heading, target, distance < 200 ? 3 : 2)));
                --object.velocityY;
            }
            if (target == WrapSignedWord(object.generalPurpose1) && object.itemValue == 0) { object.itemValue = 50; }
            if (object.itemValue < -40) { object.spriteScale = 2; }
            if (object.itemValue > 23)
            {
                object.spriteScale = 2;
                const auto mouth = context.GetModelAnchor(object, true);
                if (!mouth) { throw std::runtime_error("Living dragon model has no muzzle marker."); }
                if (distance < 200)
                {
                    context.OnFireEmitted(context.FindWorldObjectSlot(object), *mouth);
                    context.PlaySoundEffect(static_cast<SoundEffect>(22),
                        context.GetSoundVolumeAt(object.posX, object.posZ), object, 2);
                    if (SpawnDragonShot(object, context, true, *mouth, object.winY)) { break; }
                }
                else if (object.itemValue == 24)
                {
                    context.PlaySoundEffect(static_cast<SoundEffect>(9), 255, object, 2);
                    bool replaced = false;
                    for (const int offset : { -10, 0, 10 })
                    {
                        if (SpawnDragonShot(object, context, false, *mouth, WrapSignedWord(object.winY + offset)))
                        {
                            replaced = true; break;
                        }
                    }
                    if (replaced) { break; }
                }
            }
            object.itemValue = WrapSignedWord(object.itemValue - SignWord(object.itemValue));
            if (object.velocityY > 0) { WalkDragon(object, context, oldHeading); }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::Dragon, 0x24C3 };
    }

    DispatchResult Function_DragonHead(RawGameObject& object, GameContext& context)
    {
        const int slot = object.doorLockOrGroupBitmask;
        if (slot < 1 || slot >= static_cast<int>(context.worldObjects.size()))
        {
            throw std::out_of_range("Dragon head parent must be a slot in 1..165.");
        }
        const auto& dragon = context.worldObjects[slot];
        object.modelAnim = static_cast<std::uint8_t>(((-context.GetPlayerHeading() - dragon.winY + 43) & 511) / 86 + 1);
        object.animFrame = dragon.spriteScale;
        return { DispatchStatus::ExecutedCase, BehaviorType::DragonHead, 0x2506 };
    }

    DispatchResult Function_DragonDying(RawGameObject& object, GameContext& context)
    {
        object.blendWeightOrSpriteOpacity = WrapSignedWord(object.blendWeightOrSpriteOpacity + context.tickCount * 6);
        object.generalPurpose1 = static_cast<std::uint16_t>(object.generalPurpose1 - context.tickCount);
        while (object.blendWeightOrSpriteOpacity > 100)
        {
            object.blendWeightOrSpriteOpacity -= 100;
            ++object.animFrame; ++object.modelFrame2; object.generalPurpose2 = 0;
        }
        if (object.animFrame >= object.itemValue)
        {
            object.animFrame = object.modelFrame2 = static_cast<std::uint8_t>(object.itemValue);
            object.blendWeightOrSpriteOpacity = 0;
        }
        if (WrapSignedWord(object.generalPurpose1) < 1 && object.collisionType == CollisionType::Dragon)
        {
            const auto dragon = object;
            context.FlashLight({dragon.posX, dragon.posY, dragon.posZ}, {255,160,50}, 0.04f, 0.8f, 650.f);
            context.PlaySoundEffect(static_cast<SoundEffect>(32), 255, -1, 1);
            context.PlaySoundEffect(static_cast<SoundEffect>(25), 255, -2, 1);
            for (int part = 0; part < 22; ++part) { SpawnDragonFragment(dragon, context, std::min(part, 2)); }
            for (int particle = 0; particle < 20; ++particle) { SpawnDeathCloud(dragon, context); }
            RawGameObject key = dragon;
            key.health = 1000; key.behavior = BehaviorType::None;
            key.posY = context.GetTerrainFloorHeight(dragon.posX, dragon.posY, dragon.posZ);
            key.modelId = 39; key.animFrame = key.modelAnim = 1;
            key.modelFrame2 = 0;
            key.pickupType = PickupType::Type_4; key.itemValue = 1; key.displayType = DisplayType::VectorUnlit;
            key.collisionRadiusXZ = 13; key.collisionRadiusY = 12;
            key.collisionType = CollisionType::Key; key.spriteScale = 25;
            const auto slot = context.FindWorldObjectSlot(object);
            if (slot > 0) { context.ReplaceWorldObject(slot, key); }
            else { object = key; }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::DragonDying, 0x24F3 };
    }
}
