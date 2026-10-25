// Initial unrecovered handlers. Edit these bodies as behavior is recovered; do not overwrite them by regenerating.
#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <iterator>
#include <stdexcept>

namespace Desperabis
{
    DispatchResult Function_Behavior_1(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_1, 0x1167 };
    }

    DispatchResult Function_Behavior_2(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_2, 0x1198 };
    }

    DispatchResult Function_Behavior_3(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_3, 0x1297 };
    }

    DispatchResult Function_Behavior_4(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_4, 0x12C8 };
    }

    DispatchResult Function_Behavior_5(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_5, 0x1369 };
    }

    DispatchResult Function_Behavior_6(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_6, 0x1386 };
    }

    DispatchResult Function_Behavior_7(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_7, 0x1416 };
    }

    DispatchResult Function_Pillar(RawGameObject& object, GameState& context)
    {
        context.SetCollisionMapPixelAtWorldXZ(
            object.posX, object.posZ, CollisionMapValue_BlockingWall);
        return { DispatchStatus::ExecutedCase, BehaviorType::Pillar, 0x146B };
    }

    DispatchResult Function_LightSource(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::LightSource, 0x1488 };
    }

    DispatchResult Function_Door(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Door, 0x14FF };
    }

    DispatchResult Function_Behavior_11(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_11, 0x1515 };
    }

    DispatchResult Function_Frog(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Frog, 0x15C1 };
    }

    DispatchResult Function_RatAndSnake(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::RatAndSnake, 0x15EF };
    }

    DispatchResult Function_BatIndoor(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::BatIndoor, 0x161D };
    }

    DispatchResult Function_IdleAnimation(RawGameObject& object, GameState& state)
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

    DispatchResult Function_Flies(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Flies, 0x1690 };
    }

    DispatchResult Function_BatOutdoor(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::BatOutdoor, 0x16A3 };
    }

    DispatchResult Function_Behavior_18(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_18, 0x16E5 };
    }

    DispatchResult Function_FireFly(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::FireFly, 0x17A6 };
    }

    DispatchResult Function_Behavior_21(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_21, 0x181E };
    }

    DispatchResult Function_BurningFireplace(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::BurningFireplace, 0x18C6 };
    }

    DispatchResult Function_PulsatingLight(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::PulsatingLight, 0x19CD };
    }

    DispatchResult Function_SmokeFading(RawGameObject& object, GameState& state)
    {
        const auto motion = static_cast<std::uint16_t>(
            static_cast<std::uint32_t>(static_cast<std::uint16_t>(object.velocityY)) *
            static_cast<std::uint16_t>(state.tickCount));
        AddFixedPoint8(object.posY, object.itemValue, WrapSignedWord(motion));

        const auto opacity = object.health >= 0 ? object.health / 4 :
            -((-static_cast<std::int32_t>(object.health) + 3) / 4);
        object.blendWeightOrSpriteOpacity = static_cast<std::int16_t>(opacity > 5 ? 5 : opacity);

        const auto scaleHighByte = object.spriteScale >> 8;
        if (scaleHighByte < 200)
        {
            const auto growth = static_cast<std::uint8_t>(
                static_cast<std::uint32_t>(static_cast<std::uint16_t>(object.velocityZ)) *
                static_cast<std::uint16_t>(state.tickCount));
            const auto newHighByte = static_cast<std::uint8_t>(scaleHighByte + growth);
            object.spriteScale = static_cast<std::uint16_t>(
                (object.spriteScale & 0xff) | (static_cast<std::uint16_t>(newHighByte) << 8));
        }

        const auto step = WrapSignedWord(static_cast<std::int32_t>(state.tickCount) * 2);
        object.velocityY = WrapSignedWord(static_cast<std::int32_t>(object.velocityY) +
            GetStepToward(object.velocityY, 0, step));
        return { DispatchStatus::ExecutedCase, BehaviorType::SmokeFading, 0x1A9D };
    }

    DispatchResult Function_Behavior_25(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_25, 0x1B24 };
    }

    DispatchResult Function_DestroyableBarrier(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::DestroyableBarrier, 0x1B64 };
    }

    DispatchResult Function_VerticalConcreteSlab(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::VerticalConcreteSlab, 0x1B77 };
    }

    DispatchResult Function_Behavior_28(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_28, 0x1B8A };
    }

    DispatchResult Function_RollingRock(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::RollingRock, 0x1BAD };
    }

    DispatchResult Function_Skeleton(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Skeleton, 0x1BE9 };
    }

    DispatchResult Function_Cockroach(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Cockroach, 0x1C17 };
    }

    DispatchResult Function_Behavior_33(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_33, 0x1C45 };
    }

    DispatchResult Function_RollingRockSlope(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::RollingRockSlope, 0x1CA6 };
    }

    DispatchResult Function_Mummie(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Mummie, 0x1CE2 };
    }

    DispatchResult Function_SpiderLarge(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::SpiderLarge, 0x1D25 };
    }

    DispatchResult Function_Harvestman(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Harvestman, 0x1D62 };
    }

    DispatchResult Function_Devil(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Devil, 0x1D94 };
    }

    DispatchResult Function_Gorilla(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Gorilla, 0x1DC4 };
    }

    DispatchResult Function_ShootArrows(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::ShootArrows, 0x1E07 };
    }

    DispatchResult Function_FlyingArrow(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::FlyingArrow, 0x1E1A };
    }

    DispatchResult Function_MercuryLamp(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::MercuryLamp, 0x1E53 };
    }

    DispatchResult Function_Scorpion(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Scorpion, 0x1E66 };
    }

    DispatchResult Function_SpearOnGroundTrap(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::SpearOnGroundTrap, 0x1E98 };
    }

    DispatchResult Function_Behavior_45(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_45, 0x1EAB };
    }

    DispatchResult Function_Behavior_46(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_46, 0x1EDB };
    }

    DispatchResult Function_WalkablePlanks(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::WalkablePlanks, 0x1F12 };
    }

    DispatchResult Function_BreakableStoneWallFloodingWater(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::BreakableStoneWallFloodingWater, 0x1F72 };
    }

    DispatchResult Function_WalkableDrawBridge(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::WalkableDrawBridge, 0x1F85 };
    }

    DispatchResult Function_Switch1(RawGameObject& object, GameState& state)
    {
        if (object.field_24 == 0)
        {
            object.field_24 = 10;
        }
        auto& phase = object.blendWeightOrSpriteOpacity;
        phase = WrapSignedWord(phase +
            static_cast<std::int32_t>(WrapSignedWord(object.field_26)) * state.tickCount);
        if (phase < 0)
        {
            object.field_26 = 0;
            phase = 0;
        }
        if (phase > 100)
        {
            object.field_26 = static_cast<std::uint16_t>(-static_cast<std::int32_t>(object.field_24));
            phase = 100;
        }
        if (phase == 100 && (object.spriteScale & 0xff) != 0)
        {
            object.field_26 = 0;
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::Switch1, 0x1FDD };
    }

    DispatchResult Function_SwitchGrouped(RawGameObject& object, GameState& state)
    {
        if (object.groupIndexOrLock < 0 ||
            static_cast<std::size_t>(object.groupIndexOrLock) >= std::size(state.switchGroupMasks))
        {
            throw std::out_of_range("Grouped switch requires a group index in 0..4 (DS:EC70).");
        }
        object.collisionType = CollisionType::GroupedSwitch;
        auto& phase = object.blendWeightOrSpriteOpacity;
        phase = WrapSignedWord(phase +
            static_cast<std::int32_t>(WrapSignedWord(object.field_26)) * state.tickCount);
        if (phase < 0)
        {
            object.field_26 = 0;
            phase = 0;
        }
        if (phase > 100)
        {
            object.field_26 = 0;
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
        if (phase == 100 && WrapSignedWord(object.field_24) > 0 && object.itemValue > 0)
        {
            object.field_24 = static_cast<std::uint16_t>(object.field_24 - state.tickCount);
            if (WrapSignedWord(object.field_24) < 1)
            {
                object.field_24 = 0;
                object.field_26 = static_cast<std::uint16_t>(-10);
                groupMask |= mask;
            }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::SwitchGrouped, 0x2065 };
    }

    DispatchResult Function_RopeSwitch(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::RopeSwitch, 0x2078 };
    }

    DispatchResult Function_Switch3(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Switch3, 0x20D7 };
    }

    DispatchResult Function_UnderwaterPlants(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::UnderwaterPlants, 0x212D };
    }

    DispatchResult Function_Behavior_55(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_55, 0x21FF };
    }

    DispatchResult Function_Switch4(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Switch4, 0x22E6 };
    }

    DispatchResult Function_MetalPole(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::MetalPole, 0x237D };
    }

    DispatchResult Function_KeyInBucket(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::KeyInBucket, 0x2484 };
    }

    DispatchResult Function_Dragon(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Dragon, 0x24C3 };
    }

    DispatchResult Function_Behavior_61(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_61, 0x24F3 };
    }

    DispatchResult Function_DragonHead(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::DragonHead, 0x2506 };
    }

    DispatchResult Function_DiggableGround(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::DiggableGround, 0x2516 };
    }

    DispatchResult Function_Buried(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Buried, 0x2529 };
    }

    DispatchResult Function_Shark(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Shark, 0x2540 };
    }

    DispatchResult Function_Crab(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Crab, 0x256E };
    }

    DispatchResult Function_DecoFish(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::DecoFish, 0x259C };
    }

    DispatchResult Function_Dog(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Dog, 0x25CA };
    }

    DispatchResult Function_DogLeash(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::DogLeash, 0x25FE };
    }

    DispatchResult Function_TriggerDebris(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::TriggerDebris, 0x2618 };
    }

    DispatchResult Function_FlipTRexHead(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::FlipTRexHead, 0x2651 };
    }

    DispatchResult Function_Ghost(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Ghost, 0x2669 };
    }

    DispatchResult Function_LightRayKillsGhost(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::LightRayKillsGhost, 0x2697 };
    }

    DispatchResult Function_ActivatableLadder(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::ActivatableLadder, 0x26B5 };
    }

    DispatchResult Function_LiftUp(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::LiftUp, 0x26D6 };
    }

    DispatchResult Function_RechargeElectricity(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::RechargeElectricity, 0x277C };
    }

    DispatchResult Function_Lorry(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Lorry, 0x27B1 };
    }

    DispatchResult Function_MummieGuard(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::MummieGuard, 0x27CC };
    }

    DispatchResult Function_BearTrapAmmo(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::BearTrapAmmo, 0x285C };
    }

    DispatchResult Function_FireExhauster(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::FireExhauster, 0x288D };
    }

    DispatchResult Function_Behavior_82(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_82, 0x2947 };
    }

    DispatchResult Function_BonesToGrab(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::BonesToGrab, 0x299A };
    }

    DispatchResult Function_ItemHiddenInBox(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::ItemHiddenInBox, 0x2A65 };
    }

    DispatchResult Function_AnancondaHead(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::AnancondaHead, 0x2A80 };
    }

    DispatchResult Function_AnancondaTailSegment(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::AnancondaTailSegment, 0x2ABB };
    }

    DispatchResult Function_Behavior_87(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_87, 0x2AE9 };
    }

    DispatchResult Function_AttachableDamageObject(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::AttachableDamageObject, 0x2B63 };
    }

    DispatchResult Function_KrakenHead(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::KrakenHead, 0x2B76 };
    }

    DispatchResult Function_EndBossHead(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::EndBossHead, 0x2BC4 };
    }

    DispatchResult Function_EndBossLeg(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::EndBossLeg, 0x2BDA };
    }

    DispatchResult Function_EndBossFeeler(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::EndBossFeeler, 0x2C97 };
    }

    DispatchResult Function_EndBossBody(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::EndBossBody, 0x2D5C };
    }

    DispatchResult Function_BreakableGround(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::BreakableGround, 0x2D84 };
    }

    DispatchResult Function_Behavior_96(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_96, 0x2E21 };
    }

    DispatchResult Function_Behavior_97(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_97, 0x2E6B };
    }

    DispatchResult Function_Behavior_98(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_98, 0x2E7B };
    }

    DispatchResult Function_Moon(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Moon, 0x2EAA };
    }

    DispatchResult Function_WaterDrop(RawGameObject& object, GameState& context)
    {
        const auto floor = context.GetFloorHeight(object.posX, object.posY, object.posZ);
        const auto ticks = context.tickCount;
        object.posY = WrapSignedWord(object.posY + static_cast<std::int32_t>(object.velocityY) * ticks);
        object.displayType = DisplayType::SpriteTransparent;
        object.blendWeightOrSpriteOpacity = 4;
        if (WrapSignedWord(object.field_24) < 1)
        {
            const auto scale = static_cast<std::uint8_t>(object.spriteScale >> 8);
            if (scale > object.doorLockOrGroupBitmask)
            {
                object.velocityY = WrapSignedWord(object.velocityY - ticks);
            }
            else
            {
                const auto nextScale = static_cast<std::uint8_t>(scale + ticks);
                object.spriteScale = static_cast<std::uint16_t>(
                    (object.spriteScale & 0xffu) | (static_cast<unsigned int>(nextScale) << 8));
            }
            if (object.velocityY < -16) { object.velocityY = -16; }
        }
        else
        {
            object.field_24 = static_cast<std::uint16_t>(object.field_24 - ticks);
        }
        if (object.posY < floor)
        {
            if (object.winX == 1)
            {
                context.PlaySoundEffect(SoundEffect::Drop,
                    context.GetSoundVolumeAt(object.posX, object.posZ, 3), object, 1);
            }
            object.winX = 0;
            object.field_26 = static_cast<std::uint16_t>(object.field_26 - 1u);
            object.posY = WrapSignedWord(floor - 1);
            object.velocityY = 0;
            object.animFrame = 2;
            if (WrapSignedWord(object.field_26) < 1)
            {
                object.animFrame = 1;
                object.field_26 = 4;
                object.posY = WrapSignedWord(floor + context.GetRoomHeight(object.posX, object.posZ));
                object.field_24 = static_cast<std::uint16_t>(object.itemValue);
                object.spriteScale &= 0xffu;
                object.winX = 1;
            }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::WaterDrop, 0x2EBD };
    }

    DispatchResult Function_Behavior_102(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_102, 0x2ED3 };
    }

    DispatchResult Function_Behavior_103(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_103, 0x2FE2 };
    }

    DispatchResult Function_UnderwaterColorAnim(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::UnderwaterColorAnim, 0x3064 };
    }

    DispatchResult Function_Behavior_105(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_105, 0x319B };
    }

    DispatchResult Function_Behavior_106(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_106, 0x31FD };
    }

    DispatchResult Function_Behavior_108(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_108, 0x3250 };
    }

    DispatchResult Function_LeafParticle1(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::LeafParticle1, 0x32B4 };
    }

    DispatchResult Function_FogParticle(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::FogParticle, 0x3405 };
    }

    DispatchResult Function_Behavior_111(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_111, 0x349A };
    }

    DispatchResult Function_LeafParticle2(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::LeafParticle2, 0x32E5 };
    }

    DispatchResult Function_RoofDoorHandler(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::RoofDoorHandler, 0x350D };
    }

    DispatchResult Function_Behavior_114(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_114, 0x35AB };
    }

    DispatchResult Function_NeonLights(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::NeonLights, 0x3636 };
    }

    DispatchResult Function_SwimmingParticles(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::SwimmingParticles, 0x372C };
    }

    DispatchResult Function_Behavior_118(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_118, 0x3746 };
    }

    DispatchResult Function_BurriedVerticalDoor(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::BurriedVerticalDoor, 0x377F };
    }

    DispatchResult Function_Teleporter(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Teleporter, 0x37B0 };
    }

    DispatchResult Function_LevelStartOverride(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::LevelStartOverride, 0x37DD };
    }

    DispatchResult Function_DirtFromAbove(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::DirtFromAbove, 0x381A };
    }

    DispatchResult Function_DirtFromAboveShutter(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::DirtFromAboveShutter, 0x3853 };
    }

    DispatchResult Function_CeilingConcreteSlab(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::CeilingConcreteSlab, 0x388C };
    }

    DispatchResult Function_BurnableOilPuddle(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::BurnableOilPuddle, 0x38B5 };
    }

    DispatchResult Function_CeilingConcreteSlabStake(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::CeilingConcreteSlabStake, 0x390C };
    }

    DispatchResult Function_UnreachableKey(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::UnreachableKey, 0x393F };
    }

    DispatchResult Function_Behavior_128(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_128, 0x3987 };
    }

    DispatchResult Function_AttackingBull(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::AttackingBull, 0x3A36 };
    }

    DispatchResult Function_ExtinguishLava(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::ExtinguishLava, 0x3A69 };
    }

    DispatchResult Function_FloodingWater(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::FloodingWater, 0x3BF7 };
    }

    DispatchResult Function_ClimbableLadder(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::ClimbableLadder, 0x3C30 };
    }

    DispatchResult Function_CurtainOpens(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::CurtainOpens, 0x3C5B };
    }

    DispatchResult Function_Meteor(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Meteor, 0x3C8B };
    }

    DispatchResult Function_Behavior_136(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_136, 0x3E89 };
    }

    DispatchResult Function_Behavior_137(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_137, 0x3E9C };
    }

    DispatchResult Function_Behavior_138(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_138, 0x3EB8 };
    }

    DispatchResult Function_SpawnInCrater(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::SpawnInCrater, 0x3F8A };
    }

    DispatchResult Function_RotatingMachineCenter(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::RotatingMachineCenter, 0x3FE0 };
    }

    DispatchResult Function_Submarine(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Submarine, 0x4041 };
    }

    DispatchResult Function_GrasingCow(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::GrasingCow, 0x405C };
    }

    DispatchResult Function_Behavior_143(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_143, 0x40ED };
    }

    DispatchResult Function_CrackingSubmarineWindow(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::CrackingSubmarineWindow, 0x41AE };
    }

    DispatchResult Function_PuzzleGameTile(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::PuzzleGameTile, 0x42A1 };
    }

    DispatchResult Function_Behavior_146(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_146, 0x4313 };
    }

    DispatchResult Function_PuzzleGameHandler(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::PuzzleGameHandler, 0x43D5 };
    }

    DispatchResult Function_Behavior_148(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_148, 0x441F };
    }

    DispatchResult Function_PuzzleSpool(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::PuzzleSpool, 0x4480 };
    }

    DispatchResult Function_OpeningCreviceWithExit(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::OpeningCreviceWithExit, 0x4552 };
    }

    DispatchResult Function_Behavior_151(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_151, 0x458F };
    }

    DispatchResult Function_OpeningCrevice(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::OpeningCrevice, 0x45A2 };
    }

    DispatchResult Function_FlushingFishes(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::FlushingFishes, 0x45B7 };
    }

    DispatchResult Function_SomethingWithTheFishes(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::SomethingWithTheFishes, 0x4652 };
    }

    DispatchResult Function_GrandfatherClock(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::GrandfatherClock, 0x4691 };
    }

    DispatchResult Function_GrandfatherClockHand(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::GrandfatherClockHand, 0x46A4 };
    }

    DispatchResult Function_GrandfatherClockPendulum(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::GrandfatherClockPendulum, 0x46EB };
    }

    DispatchResult Function_Behavior_158(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_158, 0x4738 };
    }

    DispatchResult Function_RotateKnightsInClock(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::RotateKnightsInClock, 0x4782 };
    }

    DispatchResult Function_Behavior_161(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_161, 0x4816 };
    }

    DispatchResult Function_KnightsCrossbow(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::KnightsCrossbow, 0x482D };
    }

    DispatchResult Function_Behavior_163(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_163, 0x48AE };
    }

    DispatchResult Function_RotatingBlades(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::RotatingBlades, 0x4997 };
    }

    DispatchResult Function_ActionObjectToGrab(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::ActionObjectToGrab, 0x49AD };
    }

    DispatchResult Function_PaterNosterLift(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::PaterNosterLift, 0x4ABA };
    }

    DispatchResult Function_SqueezingMachine(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::SqueezingMachine, 0x4AD6 };
    }

    DispatchResult Function_Behavior_168(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_168, 0x4AF2 };
    }

    DispatchResult Function_RoofTopThunderHandler1(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::RoofTopThunderHandler1, 0x4B0E };
    }

    DispatchResult Function_Behavior_170(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_170, 0x4B66 };
    }

    DispatchResult Function_CutSceneCloud(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::CutSceneCloud, 0x4B9F };
    }

    DispatchResult Function_CeilingBladesComingDown(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::CeilingBladesComingDown, 0x4BDC };
    }

    DispatchResult Function_PortableCoffin(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::PortableCoffin, 0x4BFD };
    }

    DispatchResult Function_BladesFromSide(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::BladesFromSide, 0x4C87 };
    }

    DispatchResult Function_ExplodingMachine(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::ExplodingMachine, 0x4CD5 };
    }

    DispatchResult Function_Behavior_176(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_176, 0x4CE8 };
    }

    DispatchResult Function_PressureIndicator(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::PressureIndicator, 0x4D0C };
    }

    DispatchResult Function_Behavior_178(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_178, 0x4D3C };
    }

    DispatchResult Function_LargeFish(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::LargeFish, 0x4D5D };
    }

    DispatchResult Function_Behavior_181(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_181, 0x4D7E };
    }

    DispatchResult Function_DestroyableRoof(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::DestroyableRoof, 0x4E0D };
    }

    DispatchResult Function_RoofTopThunderHandler2(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::RoofTopThunderHandler2, 0x4E28 };
    }

    DispatchResult Function_RoofTopThunderHandler3(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::RoofTopThunderHandler3, 0x4E85 };
    }

    DispatchResult Function_AmbientSoundPosition(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::AmbientSoundPosition, 0x4EAF };
    }

    DispatchResult Function_LavaSmoke(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::LavaSmoke, 0x4EFB };
    }

    DispatchResult Function_TrackFireAnimation(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::TrackFireAnimation, 0x4F34 };
    }

    DispatchResult Function_Behavior_204(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_204, 0x4F47 };
    }

    DispatchResult Function_TriggerObject(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::TriggerObject, 0x4F84 };
    }

    DispatchResult Function_Behavior_206(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_206, 0x4FEB };
    }

    DispatchResult Function_WitchHead(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::WitchHead, 0x5007 };
    }

    DispatchResult Function_BuyItem(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::BuyItem, 0x501A };
    }

    DispatchResult Function_Behavior_209(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_209, 0x502D };
    }

    DispatchResult Function_TutorialTrigger(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::TutorialTrigger, 0x5083 };
    }

    DispatchResult Function_SkullInCutScene(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::SkullInCutScene, 0x50AF };
    }

    DispatchResult Function_Behavior_255(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_255, 0x50C1 };
    }

    DispatchResult Function_Behavior_300(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::Behavior_300, 0x5108 };
    }

    DispatchResult Function_DisabledSentinel(RawGameObject&, GameState&)
    {
        return { DispatchStatus::UnimplementedHandler, BehaviorType::DisabledSentinel, 0x1156 };
    }

}
