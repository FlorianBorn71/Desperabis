#include "GameContext.h"
#include "BehaviorDispatch.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "GameMath.h"

namespace Desperabis
{
    namespace
    {
        constexpr double RecordedTicksPerSecond = 33.0;
        constexpr double RadiansPerHeadingUnit = 6.2831853071795864769 / 512.0;

        double EaseCameraEndpoint(double fraction)
        {
            // Match stationary velocity/acceleration at the endpoint and unit speed at the join.
            return fraction * fraction * fraction * (6 + fraction * (-8 + fraction * 3));
        }
    }

    void GameContext::PrepareRecordedCutsceneCamera()
    {
        recordedCameraTrack.clear();
        recordedCameraFrame = recordedCameraEaseFrames = 0;
        if (!std::isfinite(cutsceneSmoothingSeconds) || cutsceneSmoothingSeconds < 0)
        {
            throw std::invalid_argument("Cutscene smoothing requires finite nonnegative seconds.");
        }
        if (cutsceneSmoothingSeconds == 0) { return; }
        const auto count = recordedCutscene.size() / 6;
        std::vector<std::array<double, 5>> source(count);
        int look = playerTargetLookHeight;
        int heading = recordedCutscene[4] * 2;
        auto eyeY = GetPlayerCamera().position[1];
        for (std::size_t frame = 0; frame < count; ++frame)
        {
            const auto offset = frame * 6;
            const auto coordinate = [&](std::size_t index)
            {
                return WrapSignedWord(recordedCutscene[index] | (recordedCutscene[index + 1] << 8));
            };
            const auto actions = recordedCutscene[offset + 5];
            if ((actions & 0x20) != 0 && look > 50) { look -= 10; }
            if ((actions & 0x10) != 0 && look < 130) { look += 10; }
            // Unwrap before filtering: 510 -> 0 is a two-unit turn, not almost a full revolution.
            const int next = recordedCutscene[offset + 4] * 2;
            heading += ((next - heading + 256) & 511) - 256;
            const auto x = coordinate(offset), z = coordinate(offset + 2);
            // Carry the previous eye height through stacked/repeated-floor selection.
            eyeY = WrapSignedWord(GetTerrainFloorHeight(x, eyeY, z) + playerBodyHeight);
            source[frame] = {static_cast<double>(x), static_cast<double>(z),
                heading * RadiansPerHeadingUnit, std::atan((110.0 - look) / 100.0), static_cast<double>(eyeY)};
        }
        const double sigma = std::min(cutsceneSmoothingSeconds, count / RecordedTicksPerSecond) *
            RecordedTicksPerSecond;
        recordedCameraEaseFrames = std::min(sigma, (count - 1) * 0.5);
        const auto radius = static_cast<int>(std::min(static_cast<double>(count - 1), std::ceil(sigma * 3)));
        std::vector<double> weights(radius * 2 + 1);
        double total = 0;
        for (int step = -radius; step <= radius; ++step)
        {
            const double distance = step == 0 ? 0 : step / sigma;
            total += weights[step + radius] = std::exp(-0.5 * distance * distance);
        }
        recordedCameraTrack.resize(count);
        for (std::size_t frame = 0; frame < count; ++frame)
        {
            for (int step = -radius; step <= radius; ++step)
            {
                const auto index = std::clamp<int>(static_cast<int>(frame) + step, 0, static_cast<int>(count) - 1);
                for (std::size_t channel = 0; channel < source[frame].size(); ++channel)
                {
                    recordedCameraTrack[frame][channel] += source[index][channel] * weights[step + radius] / total;
                }
            }
        }
    }

    std::optional<RecordedCutsceneCamera> GameContext::GetRecordedCutsceneCamera() const
    {
        if (!cutscenePlaying || recordedCameraTrack.empty()) { return {}; }
        const double last = static_cast<double>(recordedCameraTrack.size() - 1);
        double time = std::clamp(recordedCameraFrame, 0.0, last);
        if (recordedCameraEaseFrames > 0)
        {
            if (time < recordedCameraEaseFrames)
            {
                time = recordedCameraEaseFrames * EaseCameraEndpoint(time / recordedCameraEaseFrames);
            }
            else if (last - time < recordedCameraEaseFrames)
            {
                time = last - recordedCameraEaseFrames * EaseCameraEndpoint((last - time) / recordedCameraEaseFrames);
            }
        }
        const int frame = static_cast<int>(std::floor(time));
        const double u = time - frame, inverse = 1 - u;
        // Uniform cubic B-splines are C2 continuous and remain inside their control-point hull.
        const double weights[]{inverse * inverse * inverse / 6,
            (3 * u * u * u - 6 * u * u + 4) / 6,
            (-3 * u * u * u + 3 * u * u + 3 * u + 1) / 6, u * u * u / 6};
        std::array<double, 5> pose{};
        for (int point = 0; point < 4; ++point)
        {
            const auto index = std::clamp(frame + point - 1, 0, static_cast<int>(last));
            for (std::size_t channel = 0; channel < pose.size(); ++channel)
            {
                pose[channel] += recordedCameraTrack[index][channel] * weights[point];
            }
        }
        return RecordedCutsceneCamera{{pose[0], pose[4], pose[1]}, pose[2], pose[3]};
    }

    bool GameContext::IsCutscenePlaying() const { return cutscenePlaying || IsBossFallPlaying(); }

    bool GameContext::IsMuseumIntroPlaying() const
    {
        return (pendingLevelLoad && pendingLevelLoad->levelNumber == 98) ||
            (loadedLevelNumber == 98 && cutscenePlaying);
    }

    void GameContext::StartMuseumIntro()
    {
        paused = false;
        StartLevel(98);
        if (runtimeState == GameRuntimeState::LoadFailed) { throw std::runtime_error(levelLoadError); }
        SetMenuPage(MenuPage::Gameplay);
    }

    void GameContext::UpdateIntroCredits(RawGameObject& object)
    {
        object.generalPurpose1 = static_cast<std::uint16_t>(
            WrapSignedWord(object.generalPurpose1 + tickCount));
        if (WrapSignedWord(object.generalPurpose1) <= object.itemValue) { return; }
        // Native VORSPANN indices are 1..10. Index zero is an authored initial delay,
        // and indices after ten leave the completed credit sequence alone.
        const int frame = object.doorLockOrGroupBitmask;
        if (frame >= 1 && frame <= 10)
        {
            introCredit = IntroCredit{static_cast<std::uint8_t>(frame - 1),
                object.blendWeightOrSpriteOpacity, object.blendWeightOrSpriteOpacity};
        }
        object.doorLockOrGroupBitmask = WrapSignedWord(object.doorLockOrGroupBitmask + 1);
        object.generalPurpose1 = static_cast<std::uint16_t>(
            WrapSignedWord(object.generalPurpose1 - object.itemValue));
    }

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
        StopAllSoundEffects();
        PlaySoundEffect(SoundEffect::BossFallRoar, GetSoundVolume(0), -1, 1);
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
        recordedCameraTrack.clear();
        recordedCameraFrame = 0;
        recordedCutsceneFrame = 0;
        cutsceneCameraSlot.reset();
        cinematicDepthShade = 0;
        introCredit.reset();
        if ((loadedLevelNumber == 96 || loadedLevelNumber == 91 || loadedLevelNumber == 98) && !restoringSave)
        {
            const bool crystalBall = loadedLevelNumber == 91;
            auto path = LoadFile(loadedLevelNumber == 98 ? "LEVEL\\DEMO0.KOO" :
                crystalBall ? "LEVEL\\DEMO7.KOO" : "LEVEL\\DEMO6.KOO");
            if (path.size() < 9 || path[0] != *loadedLevelNumber)
            {
                throw std::invalid_argument("Cutscene recording does not match the loaded scene.");
            }
            const auto size = static_cast<std::size_t>(path[1] | (path[2] << 8));
            if (size == 0 || size % 6 != 0 || path.size() != size + 3)
            {
                throw std::invalid_argument("Cutscene recording requires complete six-byte camera frames.");
            }
            for (std::size_t offset = 3; offset < path.size(); offset += 6)
            {
                const int supportedActions = loadedLevelNumber == 96 ? 0 : 0x70;
                if ((path[offset + 5] & ~supportedActions) != 0)
                {
                    throw std::invalid_argument("Cutscene recording contains unrecovered action bits.");
                }
            }
            recordedCutscene.assign(path.begin() + 3, path.end());
            PrepareRecordedCutsceneCamera();
            cutscenePlaying = true;
            if (loadedLevelNumber == 98)
            {
                auto& player = worldObjects[1];
                player.posX = WrapSignedWord(path[3] | (path[4] << 8));
                player.posZ = WrapSignedWord(path[5] | (path[6] << 8));
                playerHeading = playerTargetHeading = static_cast<std::int16_t>(path[7] * 2);
                std::int16_t negativeZ;
                GetDirectionDisplacement(player.velocityX, negativeZ, playerHeading, 100);
                player.velocityZ = WrapSignedWord(-negativeZ);
                soundListenerX = player.posX;
                soundListenerZ = player.posZ;
            }
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

    bool GameContext::UpdateCutsceneCamera()
    {
        if (!cutscenePlaying || !cutsceneCameraSlot) { return false; }
        const auto& anchor = worldObjects.at(*cutsceneCameraSlot);
        if (anchor.health <= 0) { cutsceneCameraSlot.reset(); return false; }
        // D65E..D680: the selected object supplies working/view XYZ, not player-center Y.
        auto& player = worldObjects[1];
        player.posX = anchor.posX;
        player.posY = WrapSignedWord(anchor.posY + playerBodyHeight - player.collisionRadiusY - 2);
        player.posZ = anchor.posZ;
        std::int16_t negativeZ;
        GetDirectionDisplacement(player.velocityX, negativeZ, playerHeading, 100);
        player.velocityZ = WrapSignedWord(-static_cast<int>(negativeZ));
        soundListenerX = player.posX;
        soundListenerZ = player.posZ;
        lightingCache.Reset(soundListenerX, soundListenerZ, lightingCache.activeRadius);
        return true;
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
                if (introCredit && --introCredit->remaining <= 0) { introCredit.reset(); }
                result = TickGameplay({1, 1}, input);
                if (result.status != GameTickStatus::Advanced || nextLoadRequestId != requestId) { return result; }
                if (result.playerMovement)
                {
                    movement.simulatedTicks += result.playerMovement->simulatedTicks;
                    movement.blockedTicks += result.playerMovement->blockedTicks;
                    result.playerMovement = movement;
                }
                auto& player = worldObjects[1];
                // C558 replays actions against the collision selected by this tick, then installs the recorded pose.
                const auto actions = recordedCutscene[offset + 5];
                if ((actions & 0x20) != 0 && playerTargetLookHeight > 50) { playerTargetLookHeight -= 10; }
                if ((actions & 0x10) != 0 && playerTargetLookHeight < 130) { playerTargetLookHeight += 10; }
                if ((actions & 0x40) != 0 && playerCollisionSlot > 0)
                {
                    result.interaction = HandlePlayerObjectInteraction(worldObjects.at(playerCollisionSlot), *this);
                    if (nextLoadRequestId != requestId)
                    {
                        result.status = GameTickStatus::LevelEnded;
                        result.objects.reset();
                        return result;
                    }
                }
                player.posX = coordinate(offset);
                player.posZ = coordinate(offset + 2);
                playerTargetHeading = static_cast<std::int16_t>(recordedCutscene[offset + 4] * 2);
                soundListenerX = player.posX;
                soundListenerZ = player.posZ;
                recordedCameraFrame += 1;
                if (++recordedCutsceneFrame == recordedCutscene.size() / 6)
                {
                    if (loadedLevelNumber == 91 || loadedLevelNumber == 98) { FinishGamePart(); }
                    else { EndLevel(); }
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
            UpdateCutsceneCamera();
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
            cutsceneCameraSlot = playerCollisionSlot;
            if (object.behaviorParameter > 0) { skyEnabled = false; }
            playerTargetHeading = playerHeading = object.winY;
            if (object.itemValue > 0) { HandleActivationTriggerContact(object); }
            SetObjectHealth(object, 1000);
            object.behavior = BehaviorType::None;
        }
    }
}
