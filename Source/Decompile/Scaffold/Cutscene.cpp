#include "GameContext.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "GameMath.h"

namespace Desperabis
{
    bool GameContext::IsCutscenePlaying() const { return cutscenePlaying || IsBossFallPlaying(); }

    void GameContext::BeginBossFall(int bossSlot, int trapSlot)
    {
        const auto& boss = worldObjects.at(bossSlot);
        const auto& trap = worldObjects.at(trapSlot);
        const int rootSlot = boss.doorLockOrGroupBitmask;
        if (bossFall || bossSlot < 2 || trapSlot < 2 || rootSlot < 2 ||
            rootSlot >= static_cast<int>(worldObjects.size()) ||
            worldObjects[rootSlot].health <= 0 || worldObjects[rootSlot].modelId <= 30 ||
            trap.behavior != BehaviorType::BossPitTrap)
        {
            throw std::logic_error("Boss fall requires a live boss, staged model assembly and pit trigger.");
        }
        bossFall = BossFall{bossSlot, trapSlot, rootSlot};
    }

    std::vector<RawGameObject> GameContext::GetBossFallView() const
    {
        if (!bossFall) { return {}; }
        const auto& fall = *bossFall;
        const auto coordinate = [](double value) { return WrapSignedWord(static_cast<int>(std::nearbyint(value))); };
        std::vector<RawGameObject> objects;
        const auto addBackground = [&](int frame, int x, int y, double z, int yaw = 0)
        {
            RawGameObject object{};
            object.health = 1000;
            object.modelId = 36;
            object.modelAnim = 1;
            object.animFrame = static_cast<std::uint8_t>(frame);
            object.displayType = DisplayType::VectorNormal;
            object.posX = WrapSignedWord(x);
            object.posY = WrapSignedWord(-y);
            object.posZ = coordinate(z);
            object.winY = WrapSignedWord(yaw);
            objects.push_back(object);
        };
        addBackground(2, 50 - fall.elapsed / 2, 0, 1000 - fall.backgroundPosition);
        addBackground(1, 0, 0, -fall.backgroundPosition);
        constexpr int x[]{-400, -250, -100, 140, 380};
        constexpr int y[]{-50, 20, 90, -10, 38};
        constexpr int depth[]{140, 320, 200, 290, 400};
        for (int i = 0; i < 5; ++i)
        {
            addBackground(3, x[i], y[i], (coordinate(fall.position) + depth[i]) * 1.2 * (i + 11),
                coordinate(i * 111 + fall.elapsed * 1.2 * (i - 3)));
        }
        auto root = worldObjects.at(fall.rootSlot);
        root.posX = root.posY = 0;
        root.posZ = coordinate(fall.position);
        root.winX = root.winZ = 0;
        root.winY = WrapSignedWord(fall.elapsed + 100);
        objects.push_back(root);
        for (const auto& object : worldObjects)
        {
            if (object.health <= 0 || object.renderPlacementMode != 20 ||
                object.doorLockOrGroupBitmask != fall.rootSlot) { continue; }
            objects.push_back(ResolveAttachedRenderState(object, root));
        }
        return objects;
    }

    GameTickResult GameContext::TickBossFall(GameTickTiming timing)
    {
        tickCount = timing.behaviorTicks;
        frameTickCount = timing.frameTicks;
        for (int tick = 0; tick < timing.behaviorTicks && bossFall; ++tick)
        {
            auto& fall = *bossFall;
            ++fall.elapsed;
            if (fall.velocity > 0) { fall.position += fall.velocity; }
            fall.velocity += 0.25;
            fall.backgroundPosition += fall.backgroundVelocity;
            fall.backgroundVelocity -= 0.04;
            for (std::size_t slot = 2; slot <= 141; ++slot)
            {
                auto& object = worldObjects[slot];
                if (object.health != 1000 || object.behavior != BehaviorType::RopeSwitch) { continue; }
                object.velocityZ = static_cast<std::int16_t>((object.velocityZ + object.velocityX) & 511);
                object.blendWeightOrSpriteOpacity = WrapSignedWord(50 + WrapSignedWord(
                    static_cast<std::int32_t>(static_cast<std::uint32_t>(50 * Sine(object.velocityZ)) >> 16)));
            }
            if (fall.elapsed <= 300 && std::nearbyint(fall.position) - 1500 <= 940) { continue; }
            const auto defeated = fall.bossSlot;
            const auto group = worldObjects[fall.trapSlot].itemValue;
            bossFall.reset();
            SetObjectHealth(worldObjects[defeated], 0);
            for (std::size_t slot = 2; slot <= 140; ++slot)
            {
                auto& object = worldObjects[slot];
                if (object.renderPlacementMode == 20 && object.doorLockOrGroupBitmask == defeated)
                {
                    SetObjectHealth(object, 0);
                }
                if (object.health == 1000 + group) { DamageObject(object, 0); }
                if (object.health < 1000) { SetObjectHealth(object, 0); }
            }
        }
        return {GameTickStatus::Advanced, {}, {}};
    }

    void GameContext::BeginCutscene(bool restoringSave)
    {
        bossFall.reset();
        recordedCutscene.clear();
        recordedCutsceneFrame = 0;
        cinematicDepthShade = 0;
        if (loadedLevelNumber == 96 && !restoringSave)
        {
            auto path = LoadFile("LEVEL\\DEMO6.KOO");
            if (path.size() < 9 || path[0] != 96)
            {
                throw std::invalid_argument("Ending requires a level-96 DEMO6 recording.");
            }
            const auto size = static_cast<std::size_t>(path[1] | (path[2] << 8));
            if (size == 0 || size % 6 != 0 || path.size() != size + 3)
            {
                throw std::invalid_argument("Ending recording requires complete six-byte camera frames.");
            }
            for (std::size_t offset = 3; offset < path.size(); offset += 6)
            {
                if (path[offset + 5] != 0)
                {
                    throw std::invalid_argument("Ending recording contains unrecovered action bits.");
                }
            }
            recordedCutscene.assign(path.begin() + 3, path.end());
            cutscenePlaying = true;
            return;
        }
        // 07E0..07E8 initializes player word 34 from LEVELS.INF byte 104, at 35 ticks per unit.
        const auto duration = level->nativeInfo[104];
        cutscenePlaying = duration != 0 && (!restoringSave || worldObjects[1].itemValue > 0);
        if (cutscenePlaying && !restoringSave)
        {
            worldObjects[1].itemValue = static_cast<std::int16_t>(duration * 35);
        }
    }

    GameTickResult GameContext::Tick(GameTickTiming timing, InputHandler& input)
    {
        if (timing.behaviorTicks < 0 || timing.frameTicks < 0)
        {
            throw std::invalid_argument("Game ticks require nonnegative behavior and frame counts.");
        }
        if (IsCutscenePlaying() && runtimeState == GameRuntimeState::Ready &&
            !paused && menuPage == MenuPage::Gameplay)
        {
            return IsBossFallPlaying() ? TickBossFall(timing) : TickCutscene(timing);
        }
        return TickGameplay(timing, input);
    }

    GameTickResult GameContext::TickCutscene(GameTickTiming timing)
    {
        class NoInput final : public InputHandler
        {
            bool IsKeyPressed(InputKey) override { return false; }
        } input;
        GameTickResult result{GameTickStatus::Advanced, {}, {}};
        BasicPlayerMovementResult movement{};
        const auto requestId = nextLoadRequestId;
        if (!recordedCutscene.empty())
        {
            for (int tick = 0; tick < timing.behaviorTicks; ++tick)
            {
                const auto offset = recordedCutsceneFrame * 6;
                const auto coordinate = [&](std::size_t index)
                {
                    return WrapSignedWord(recordedCutscene[index] | (recordedCutscene[index + 1] << 8));
                };
                result = TickGameplay({1, 1}, input);
                if (result.status != GameTickStatus::Advanced || nextLoadRequestId != requestId) { return result; }
                if (result.playerMovement)
                {
                    movement.simulatedTicks += result.playerMovement->simulatedTicks;
                    movement.blockedTicks += result.playerMovement->blockedTicks;
                    result.playerMovement = movement;
                }
                auto& player = worldObjects[1];
                player.posX = coordinate(offset);
                player.posZ = coordinate(offset + 2);
                playerTargetHeading = static_cast<std::int16_t>(recordedCutscene[offset + 4] * 2);
                soundListenerX = player.posX;
                soundListenerZ = player.posZ;
                if (++recordedCutsceneFrame == recordedCutscene.size() / 6)
                {
                    StartLevel(95);
                    result.status = GameTickStatus::LevelEnded;
                    result.objects.reset();
                    return result;
                }
            }
            return result;
        }
        for (int step = 0; step < timing.frameTicks; ++step)
        {
            const auto ticks = static_cast<std::int16_t>(
                (step + 1) * timing.behaviorTicks / timing.frameTicks -
                step * timing.behaviorTicks / timing.frameTicks);
            result = TickGameplay({ticks, 1}, input);
            if (result.status != GameTickStatus::Advanced || nextLoadRequestId != requestId) { return result; }
            if (result.playerMovement)
            {
                movement.simulatedTicks += result.playerMovement->simulatedTicks;
                movement.blockedTicks += result.playerMovement->blockedTicks;
                result.playerMovement = movement;
            }
            // C9E0..CA18 counts down after world simulation; no KOO pose overrides this camera.
            auto& remaining = worldObjects[1].itemValue;
            remaining = static_cast<std::int16_t>(std::max(0, static_cast<int>(remaining) - ticks));
            if (remaining == 0)
            {
                EndLevel();
                result.status = GameTickStatus::LevelEnded;
                result.objects.reset();
                return result;
            }

        }
        return result;
    }

    void GameContext::UpdateObjectPoseSequence(RawGameObject& object)
    {
        if (object.health != 1000) { return; }
        for (int tick = 0; tick < tickCount; ++tick)
        {
            const auto duration = WrapSignedWord(object.generalPurpose3);
            if (duration <= 0) { continue; }
            if (WrapSignedWord(object.generalPurpose1) > 0 && object.doorLockOrGroupBitmask > 0)
            {
                const auto& first = worldObjects.at(object.generalPurpose1);
                const auto& second = worldObjects.at(object.doorLockOrGroupBitmask);
                const auto fraction = RoundReal48(static_cast<double>(object.itemValue) / std::max<int>(duration, 1));
                const auto interpolate = [&](std::int16_t a, std::int16_t b)
                {
                    return WrapSignedWord(static_cast<std::int32_t>(
                        std::nearbyint(b * fraction + a * (1.0 - fraction))));
                };
                object.posX = interpolate(first.posX, second.posX);
                object.posY = interpolate(first.posY, second.posY);
                object.posZ = interpolate(first.posZ, second.posZ);
                object.winX = interpolate(first.winX, second.winX);
                object.winY = interpolate(first.winY, second.winY);
                object.winZ = interpolate(first.winZ, second.winZ);
                if (object.blendWeightOrSpriteOpacity >= 0)
                {
                    object.blendWeightOrSpriteOpacity = interpolate(0, 100);
                }
            }
            auto speed = WrapSignedWord(object.generalPurpose2);
            if (speed < 2) { speed = 2; }
            object.itemValue = WrapSignedWord(object.itemValue + ArithmeticShiftRight(speed, 1));
            object.generalPurpose2 = static_cast<std::uint16_t>(WrapSignedWord(speed + object.groupIndexOrLock));
            if (object.itemValue > duration && object.doorLockOrGroupBitmask > 0)
            {
                const auto next = object.doorLockOrGroupBitmask;
                auto pose = worldObjects.at(next);
                pose.collisionRadiusY = 70;
                pose.generalPurpose1 = static_cast<std::uint16_t>(next);
                pose.behavior = BehaviorType::ObjectPoseSequence;
                pose.collisionType = CollisionType::None;
                ReplaceWorldObject(FindWorldObjectSlot(object), pose);
            }
        }
    }

    void GameContext::UpdateEndingSequenceTrigger(RawGameObject& object)
    {
        windX = 0;
        windZ = -20;
        object.groupIndexOrLock = WrapSignedWord(object.groupIndexOrLock - tickCount);
        if (object.groupIndexOrLock < 1)
        {
            playerCollisionSlot = static_cast<std::uint16_t>(FindWorldObjectSlot(object));
            if (object.behaviorParameter > 0) { skyEnabled = false; }
            playerTargetHeading = playerHeading = object.winY;
            if (object.itemValue > 0) { HandleActivationTriggerContact(object); }
            SetObjectHealth(object, 1000);
            object.behavior = BehaviorType::None;
        }
    }
}
