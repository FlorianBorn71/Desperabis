#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <stdexcept>
#include <vector>

namespace Desperabis
{
    DispatchResult Function_WindController(RawGameObject& object, GameContext& context)
    {
        if (context.Random(30) == 0)
        {
            object.velocityX = context.RandomSigned(8);
            object.velocityZ = context.RandomSigned(8);
        }
        if (context.Random(5) == 0)
        {
            context.windX = WrapSignedWord(context.windX + SignWord(WrapSignedWord(object.velocityX - context.windX)));
            context.windZ = WrapSignedWord(context.windZ + SignWord(WrapSignedWord(object.velocityZ - context.windZ)));
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::WindController, 0x3250 };
    }

    DispatchResult Function_AirborneLeaf(RawGameObject& object, GameContext& context)
    {
        for (int tick = 0; tick < context.tickCount; ++tick)
        {
            if (object.animFrame > 3) { context.SetObjectHealth(object, 0); }
            object.renderPlacementMode = 0;
            const auto floor = context.GetTerrainFloorHeight(object.posX, object.posY, object.posZ);
            object.posY = WrapSignedWord(object.posY + object.velocityY);
            if (object.posY < floor)
            {
                object.posY = floor;
                object.velocityX = object.velocityZ = 0;
                object.velocityY = static_cast<std::int16_t>(context.Random(6) + 2);
                if (object.health < 1000) { object.behavior = BehaviorType::None; }
            }
            else
            {
                object.winX = static_cast<std::int16_t>((object.winX + 6 + context.Random(5)) & 511);
                object.winZ = static_cast<std::int16_t>((object.winX + 3 + context.Random(5)) & 511);
            }
            if (context.Random(10) == 0)
            {
                object.velocityX = WrapSignedWord(object.velocityX + SignWord(WrapSignedWord(context.windX - object.velocityX)));
                object.velocityZ = WrapSignedWord(object.velocityZ + SignWord(WrapSignedWord(context.windZ - object.velocityZ)));
            }
            auto x = WrapSignedWord(object.posX + object.velocityX);
            auto z = WrapSignedWord(object.posZ + object.velocityZ);
            if ((context.GetCollisionMapPixelAtWorldXZ(x, z) & 7) == 1)
            {
                object.itemValue = 100;
                object.velocityX = object.velocityZ = 0;
                object.behavior = BehaviorType::GroundLeaf;
            }
            x = WrapSignedWord(object.posX + object.velocityX);
            z = WrapSignedWord(object.posZ + object.velocityZ);
            if ((context.GetCollisionMapPixelAtWorldXZ(x, z) & 56) != 0)
            {
                object.velocityY = WrapSignedWord(object.velocityY - 1);
                if (object.health > 1000) { context.SetObjectHealth(object, 100); }
                else { object.velocityY = 0; }
                object.posX = WrapSignedWord(object.posX - object.velocityX);
                object.posZ = WrapSignedWord(object.posZ - object.velocityZ);
                object.velocityX = WrapSignedWord(-object.velocityX);
                object.velocityZ = WrapSignedWord(-object.velocityZ);
            }
            else
            {
                object.posX = x;
                object.posZ = z;
                object.velocityY = WrapSignedWord(object.velocityY + SignWord(WrapSignedWord(-1 - object.velocityY)));
            }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::AirborneLeaf, 0x32B4 };
    }

    DispatchResult Function_GroundLeaf(RawGameObject& object, GameContext& context)
    {
        for (int tick = 0; tick < context.tickCount; ++tick)
        {
            object.renderPlacementMode = 2;
            if (object.animFrame > 3) { context.SetObjectHealth(object, 0); }
            object.posY = context.GetFloorHeight(object.posX, object.posY, object.posZ);
            if (context.Random(50) == 0)
            {
                object.velocityX = SignWord(context.windX);
                object.velocityZ = SignWord(context.windZ);
            }
            const bool ground = (context.GetCollisionMapPixelAtWorldXZ(
                WrapSignedWord(object.posX + object.velocityX), WrapSignedWord(object.posZ + object.velocityZ)) & 7) == 1;
            if (context.Random(4) == 0 && ground) { object.posX = WrapSignedWord(object.posX + object.velocityX); }
            if (context.Random(4) == 0 && ground) { object.posZ = WrapSignedWord(object.posZ + object.velocityZ); }
            if (!ground)
            {
                if (object.itemValue < 1) { object.behavior = BehaviorType::AirborneLeaf; }
                else { object.itemValue = WrapSignedWord(object.itemValue - 1); }
            }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::GroundLeaf, 0x32E5 };
    }

    void GameContext::InitializeRainDrop(std::uint16_t slot, std::int16_t x, std::int16_t y, std::int16_t z)
    {
        if (slot < 2 || slot > 165) { throw std::out_of_range("Rain requires an ordinary or fallback object slot."); }
        RawGameObject drop{};
        drop.health = 60;
        drop.posX = x;
        drop.posY = y;
        drop.posZ = z;
        drop.velocityY = static_cast<std::int16_t>(Random(5) - 19);
        drop.velocityX = static_cast<std::int16_t>(windX / 3);
        drop.velocityZ = static_cast<std::int16_t>(windZ / 3);
        drop.behavior = BehaviorType::RainDrop;
        drop.modelId = 1;
        drop.modelAnim = 4;
        drop.animFrame = 1;
        drop.spriteScale = 29;
        drop.displayType = static_cast<DisplayType>(13);
        drop.blendWeightOrSpriteOpacity = 4;
        drop.winY = static_cast<std::int16_t>(Random(2));
        if ((GetCollisionMapPixelAtWorldXZ(x, z) & 8) == 8 ||
            (HasCeilingAtPlayer() && playerMapCollisionsEnabled))
        {
            drop.health = 0;
        }
        ReplaceWorldObject(slot, drop);
    }

    DispatchResult Function_RainEmitter(RawGameObject& object, GameContext& context)
    {
        auto total = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(object.itemValue) * static_cast<unsigned>(context.tickCount) + object.generalPurpose1);
        const auto eyeY = WrapSignedWord(static_cast<std::int32_t>(context.GetPlayerCamera().position[1]));
        while ((total >> 8) != 0)
        {
            // Native 350D clears a slot before its second allocation in the actual spawn call.
            context.FindFreeObjectIndex();
            total = static_cast<std::uint16_t>(total - 256);
            const auto angle = WrapSignedWord(256 - context.GetPlayerHeading() + context.RandomSigned(100));
            const auto radius = static_cast<std::int16_t>(context.Random(250) + 100);
            std::int16_t offsetX, offsetZ;
            GetClampedDirectionDisplacement(offsetX, offsetZ, angle, radius);
            const auto slot = context.FindFreeObjectIndex();
            const auto x = WrapSignedWord(context.soundListenerX + offsetX);
            const auto y = WrapSignedWord(eyeY + 100 + context.Random(70));
            const auto z = WrapSignedWord(context.soundListenerZ + offsetZ);
            context.InitializeRainDrop(slot, x, y, z);
        }
        object.generalPurpose1 = total;
        return { DispatchStatus::ExecutedCase, BehaviorType::RainEmitter, 0x350D };
    }

    DispatchResult Function_RainDrop(RawGameObject& object, GameContext& context)
    {
        object.posY = WrapSignedWord(object.posY + object.velocityY * context.tickCount);
        object.posX = WrapSignedWord(object.posX + object.velocityX);
        object.posZ = WrapSignedWord(object.posZ + object.velocityZ);
        const auto floor = context.GetFloorHeight(object.posX, object.posY, object.posZ);
        if (object.posY < floor)
        {
            const auto lifetime = static_cast<std::int16_t>(context.Random(4) + 1);
            object.animFrame = 2;
            object.behavior = BehaviorType::None;
            object.posY = floor;
            context.SetObjectHealth(object, lifetime);
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::RainDrop, 0x35AB };
    }

    void GameContext::ClearLightningFlashes()
    {
        if (lightningFlashes.empty()) { return; }
        lightningFlashes.clear();
        OnLightningHidden();
    }

    void GameContext::GenerateLightningFlash(std::int16_t x, const RawGameObject* source)
    {
        if (playerLookHeight > 32747)
        {
            throw std::overflow_error("Lightning horizon would wrap native vertical coordinates.");
        }
        LightningFlash flash;
        if (source)
        {
            flash.sourceSlot = FindWorldObjectSlot(*source);
            flash.sourcePosition = { { source->posX, source->posY, source->posZ } };
        }
        struct Frame { std::int16_t x, y; bool branch; float weight; std::size_t parent; };
        const auto noParent = static_cast<std::size_t>(-1);
        std::vector<Frame> stack{ { x, 0, false, 1.f, noParent } };
        std::vector<unsigned> children;
        while (!stack.empty())
        {
            const auto frame = stack.back();
            stack.pop_back();
            if (frame.branch)
            {
                if (Random(5) == 0) { stack.push_back({ frame.x, frame.y, false, frame.weight * 0.55f, frame.parent }); }
                continue;
            }
            const auto nextX = WrapSignedWord(frame.x + RandomSigned(20));
            const auto nextY = WrapSignedWord(frame.y + 6 + Random(15));
            if (nextY > playerLookHeight) { continue; }
            const auto taper = [&](std::int16_t y)
            {
                return frame.weight * 2.f * static_cast<float>(playerLookHeight - y) / playerLookHeight;
            };
            const auto index = flash.lines.size();
            if (frame.parent != noParent) { ++children[frame.parent]; }
            flash.lines.push_back({ frame.x, frame.y, nextX, nextY, taper(frame.y), taper(nextY) });
            children.push_back(0);
            // The branch RNG belongs after the first recursive subtree, not before it.
            stack.push_back({ nextX, nextY, true, frame.weight, index });
            stack.push_back({ nextX, nextY, false, frame.weight, index });
        }
        for (std::size_t i = 0; i < children.size(); ++i)
        {
            if (children[i] == 0) { flash.lines[i].thickness2 = 0.f; }
        }
        lightningFlashes.push_back(std::move(flash));
        OnLightningShown(lightningFlashes.back());
    }

    DispatchResult Function_LightningGenerator(RawGameObject& object, GameContext& context)
    {
        if (context.skyEnabled)
        {
            for (int tick = 0; tick < context.tickCount; ++tick)
            {
                auto timer = WrapSignedWord(object.generalPurpose2);
                if (timer > 0) { timer = WrapSignedWord(timer - 1); }
                else
                {
                    timer = static_cast<std::int16_t>(context.Random(1000) + 200);
                    object.itemValue = static_cast<std::int16_t>(context.Random(400) - 80);
                    const auto volume = GameContext::GetSoundVolume(static_cast<std::int16_t>(context.Random(300)));
                    context.PlaySoundEffect(static_cast<SoundEffect>(102), volume, object, 2);
                }
                object.generalPurpose2 = static_cast<std::uint16_t>(timer);
                if (timer > 1 && timer <= 80)
                {
                    if (context.Random(3) == 0) { --object.generalPurpose2; }
                }
                else if (timer >= 81 && timer <= 89 && (timer & 1) != 0)
                {
                    if (context.Random(2) == 0 && !context.HasCeilingAtPlayer())
                    {
                        context.GenerateLightningFlash(object.itemValue, &object);
                    }
                }
            }
        }
        return { DispatchStatus::ExecutedCase, BehaviorType::LightningGenerator, 0x2ED3 };
    }
}
