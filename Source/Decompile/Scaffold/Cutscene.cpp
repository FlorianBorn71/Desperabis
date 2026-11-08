#include "GameContext.h"

#include <algorithm>
#include <stdexcept>

namespace Desperabis
{
    bool GameContext::IsCutscenePlaying() const { return cutscenePlaying; }

    void GameContext::BeginCutscene(bool restoringSave)
    {
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
}
