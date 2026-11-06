#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <stdexcept>

namespace Desperabis
{
    bool GameContext::IsCutscenePlaying() const { return !cutsceneFrames.empty(); }

    std::vector<GameContext::CutsceneFrame> GameContext::ReadCutscene(std::int16_t levelNumber)
    {
        // Other DEMO paths also visit playable levels; only the authored story scene auto-starts.
        if (levelNumber != 97) { return {}; }
        const auto bytes = LoadFile("LEVEL\\DEMO6.KOO");
        const auto invalid = [] { throw std::invalid_argument("LEVEL\\DEMO6.KOO: invalid camera path."); };
        if (bytes.size() < 9 || bytes[0] + 1 != levelNumber) { invalid(); }
        const std::size_t count = bytes[1] | (static_cast<std::size_t>(bytes[2]) << 8);
        if (count != bytes.size() - 3 || count % 6 != 0 || count > 64000) { invalid(); }
        const auto word = [&](std::size_t offset)
        {
            return WrapSignedWord(bytes[offset] | (static_cast<int>(bytes[offset + 1]) << 8));
        };
        std::vector<CutsceneFrame> frames;
        frames.reserve(count / 6);
        for (std::size_t offset = 3; offset < bytes.size(); offset += 6)
        {
            frames.push_back({word(offset), word(offset + 2), bytes[offset + 4], bytes[offset + 5]});
        }
        return frames;
    }

    void GameContext::BeginCutscene(std::vector<CutsceneFrame> frames)
    {
        cutsceneFrames = std::move(frames);
        cutsceneCursor = 0;
        if (!IsCutscenePlaying()) { return; }
        const auto& first = cutsceneFrames.front();
        if (first.x == -1 || first.z == -1) { return; }
        auto& player = worldObjects[1];
        // Present the first recorded pose immediately, rather than flashing the ordinary level spawn.
        player.posX = first.x;
        player.posZ = first.z;
        playerHeading = playerTargetHeading = static_cast<std::int16_t>(first.angle * 2);
        std::int16_t negativeZ;
        GetDirectionDisplacement(player.velocityX, negativeZ, playerHeading, 100);
        player.velocityZ = WrapSignedWord(-negativeZ);
        soundListenerX = first.x;
        soundListenerZ = first.z;
        lightingCache.Reset(first.x, first.z, lightingCache.activeRadius);
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
            return TickCutscene(timing);
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
        for (int step = 0; step < timing.frameTicks; ++step)
        {
            if (cutsceneCursor == cutsceneFrames.size())
            {
                EndLevel();
                result.status = GameTickStatus::LevelEnded;
                result.objects.reset();
                return result;
            }
            const auto frame = cutsceneFrames[cutsceneCursor++];
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
            // C558 applies action bits and the next X/Z/heading after simulation, not instead of it.
            if ((frame.actions & 0x20) && playerTargetLookHeight > 50) { playerTargetLookHeight -= 10; }
            if ((frame.actions & 0x10) && playerTargetLookHeight < 130) { playerTargetLookHeight += 10; }
            if ((frame.actions & 0x40) && playerCollisionSlot > 0)
            {
                result.interaction = HandlePlayerObjectInteraction(worldObjects.at(playerCollisionSlot), *this);
                if (nextLoadRequestId != requestId || runtimeState != GameRuntimeState::Ready ||
                    menuPage != MenuPage::Gameplay)
                {
                    result.status = GameTickStatus::LevelEnded;
                    result.objects.reset();
                    return result;
                }
            }
            if (frame.actions & 0x80) { result.weapon->attack = AttackWithSelectedWeapon(); }
            playerTargetHeading = static_cast<std::int16_t>(frame.angle * 2);
            if (frame.x == -1 || frame.z == -1)
            {
                EndLevel();
                result.status = GameTickStatus::LevelEnded;
                result.objects.reset();
                return result;
            }
            worldObjects[1].posX = frame.x;
            worldObjects[1].posZ = frame.z;
            soundListenerX = frame.x;
            soundListenerZ = frame.z;
        }
        return result;
    }
}
