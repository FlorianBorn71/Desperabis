#include "BehaviorDispatch.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace Desperabis;

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition) { throw std::runtime_error(message); }
    }

    class Audio final : public AudioHandler
    {
    public:
        int starts = 0, stops = 0;
        bool underwater = false;
        std::array<bool, SoundChannelCount> playing{};
        std::uint16_t GetSampleCount() const override { return 129; }
        bool IsChannelPlaying(std::size_t channel) const override { return playing.at(channel); }
        void PlaySample(std::size_t channel, std::uint8_t, std::uint8_t) override { playing.at(channel) = true; }
        void StopAllSamples() override { playing.fill(false); }
        void StartUnderwaterPlayback(std::uint8_t sample, std::uint8_t volume) override
        {
            Check(sample == 58 && volume == 255, "Underwater must loop original effect 58/sample 58.");
            Check(!underwater, "Underwater audio must not restart every controller step.");
            underwater = true;
            ++starts;
        }
        void StopUnderwaterPlayback() override { underwater = false; ++stops; }
    };

    class Input final : public InputHandler
    {
    public:
        bool IsKeyPressed(InputKey) override { return false; }
    };

    class Host final : public GameContext
    {
    public:
        std::vector<bool> transitions;
        int oxygenChanges = 0, healthChanges = 0, creations = 0, destructions = 0;
        void OnUnderwaterChanged(bool value) override { transitions.push_back(value); }
        void OnPlayerOxygenChanged(std::int32_t, std::int32_t) override { ++oxygenChanges; }
        void OnPlayerHealthChanged(std::int32_t, std::int32_t) override { ++healthChanges; }
        void OnObjectCreated(int) override { ++creations; }
        void OnObjectDestroyed(int) override { ++destructions; }
        void Load()
        {
            const auto request = StartLevel(12);
            LevelLoadData data;
            data.level = std::make_unique<Level>(CreateDummyLevel());
            data.level->heightSurfaces[0] = Level::HeightSurface{100, 9};
            data.playerSpawn = PlayerCamera{{1500, 45, 2500}, {0, 0, 100}};
            CompleteLevelLoad(request.id, std::move(data));
            basicPlayerMovementEnabled = false;
        }
    protected:
        void LoadLevel(const LevelLoadRequest&) override {}
    };

    void Step(Host& host, int ticks = 1)
    {
        Input input;
        Check(host.Tick({static_cast<std::int16_t>(ticks), static_cast<std::int16_t>(ticks)}, input).status ==
            GameTickStatus::Advanced, "Underwater gameplay must advance.");
    }

    void TestOxygen()
    {
        Host host;
        host.Load();
        host.playerUnderwater = true;
        Step(host, 33);
        Check(host.playerOxygen == GameContext::PlayerOxygenMaximum - 33 * 14625 &&
            host.playerLife == 360 * 65536 && host.oxygenChanges == 33,
            "Oxygen must drain the recovered fixed-point amount per controller step, not behavior call.");
        Check(host.transitions == std::vector<bool>{true}, "Entering water must issue one view callback.");
        constexpr int stepsToEmpty = (GameContext::PlayerOxygenMaximum + 14624) / 14625;
        Check(stepsToEmpty == 1614, "Full native oxygen must last 1614 controller steps.");
        Step(host, stepsToEmpty - 34);
        Check(host.playerOxygen > 0 && host.playerLife == 360 * 65536,
            "Drowning must not begin before oxygen reaches zero.");
        Step(host);
        Check(host.playerOxygen == 0 && host.playerLife == 360 * 65536 - 45704 &&
            host.healthChanges == 1 && host.worldObjects[1].health == 1000,
            "Exhausted meter tag 200 must forward delta*200 >> 6 to player life, not object health.");
        Step(host, 10);
        Check(host.playerOxygen == 0 && host.playerLife == 360 * 65536 - 11 * 45704,
            "Drowning must continue each controller step with no oxygen underflow.");
        host.playerLifeDeltaSuppressed = 1;
        const auto life = host.playerLife;
        Step(host);
        Check(host.playerLife == life, "Drowning must honor player-life suppression.");
        host.playerLifeDeltaSuppressed = 0;
        host.playerUnderwater = false;
        Step(host);
        Check(host.playerOxygen == GameContext::PlayerOxygenMaximum &&
            host.transitions == std::vector<bool>({true, false}), "Surfacing must refill oxygen and clear tint.");
        host.playerUnderwater = true;
        host.playerOxygen = 0;
        host.playerLife = 100;
        Step(host, 3);
        Check(host.playerLife == 0 && host.playerOxygen == 0 && host.positionalSoundsSuppressed,
            "Drowning death must install native depletion state without refilling oxygen on later substeps.");
    }

    void TestLifecycleAndAudio()
    {
        Audio audio;
        Host host;
        host.audioHandler = &audio;
        host.soundEffectsLoaded = true;
        host.soundEffects[42] = {1, {43, 0, 0, 0}};
        host.soundEffects[55] = {2, {56, 57, 0, 0}};
        host.soundEffects[57] = {1, {58, 0, 0, 0}};
        host.Load();
        host.playerUnderwater = true;
        Step(host, 20);
        Check(audio.starts == 1 && audio.stops == 0, "Audio loop must be independent and persistent.");
        const auto oxygen = host.playerOxygen;
        host.SetPaused(true);
        Input input;
        Check(host.Tick({10, 10}, input).status == GameTickStatus::Paused &&
            host.playerOxygen == oxygen && !audio.underwater && host.transitions.size() == 1,
            "Pause must stop audio and oxygen consumption while retaining the frozen underwater view.");
        host.SetPaused(false);
        Check(audio.starts == 2 && audio.underwater, "Unpause must resume the underwater loop.");
        host.SetMenuPage(MenuPage::Inventory);
        Check(!audio.underwater && host.transitions.back() == false && host.playerOxygen == oxygen,
            "Modal pages must clear tint/audio without refilling oxygen.");
        host.SetMenuPage(MenuPage::Gameplay);
        Check(audio.underwater && host.transitions.back(), "Returning to gameplay must restore the view.");
        host.soundEnabled = false;
        Step(host);
        Check(!audio.underwater, "Disabling sound must stop the underwater loop.");
        host.soundEnabled = true;
        Step(host);
        Check(audio.underwater, "Enabling sound must restart the underwater loop.");
        host.Load();
        Check(!audio.underwater && !host.playerUnderwater &&
            host.playerOxygen == GameContext::PlayerOxygenMaximum && !host.transitions.back(),
            "Loading must reset oxygen and leave no stale loop or blue tint.");
        host.playerUnderwater = true;
        Step(host);
        host.EndLevel(true);
        Check(!audio.underwater && !host.transitions.back() && !host.playerUnderwater,
            "Game-part completion must clear the underwater view and sound.");
    }

    void TestBubblesAndMovement()
    {
        Host host;
        host.Load();
        host.randomSeed = 1;
        host.playerUnderwater = true;
        Step(host, 100);
        const auto count = std::count_if(host.worldObjects.begin(), host.worldObjects.end(), [](const auto& object)
            { return object.behavior == BehaviorType::Bubble && object.health > 0; });
        Check(count > 0 && host.creations > 1, "Underwater exhalation must spawn actual lifecycle-notified bubbles.");
        const auto slot = host.SpawnBubble(1500, -101, 2500);
        auto& bubble = host.worldObjects[slot];
        Check(bubble.modelId == 31 && bubble.modelAnim == 3 && bubble.animFrame == 3 &&
            bubble.health == 200 && bubble.displayType == DisplayType::VectorNormal &&
            bubble.velocityY >= 190 && bubble.velocityY < 390 && bubble.velocityX >= -4 && bubble.velocityX <= 4,
            "Bubbles must use the recovered native factory recipe, not new artwork.");
        bubble.velocityY = 256;
        bubble.velocityX = -3;
        bubble.winY = 50;
        bubble.itemValue = 0;
        std::fill(std::begin(host.collisionMap->data), std::end(host.collisionMap->data), std::uint8_t{1});
        host.tickCount = 1;
        Check(DispatchObjectCase(bubble, host).status == DispatchStatus::ExecutedCase &&
            bubble.posY == -100 && bubble.itemValue == 0 && bubble.winY == 47 &&
            bubble.behavior == BehaviorType::Bubble, "Bubble motion must use signed 8.8 rise and a strict surface crossing.");
        DispatchObjectCase(bubble, host);
        Check(bubble.behavior == BehaviorType::BubblePop, "A bubble crossing the water surface must switch to popping.");
        bubble.blendWeightOrSpriteOpacity = 0;
        host.tickCount = 50;
        DispatchObjectCase(bubble, host);
        Check(bubble.health == 200 && bubble.animFrame == 4 && bubble.modelFrame2 == 5 &&
            bubble.blendWeightOrSpriteOpacity == 100, "Pop must morph original frames 4/5 and remain alive at 100.");
        const auto destroyed = host.destructions;
        host.tickCount = 1;
        DispatchObjectCase(bubble, host);
        Check(bubble.health == 0 && host.destructions == destroyed + 1, "Completed pop must notify renderer destruction.");

        host.Load();
        host.basicPlayerMovementEnabled = true;
        for (auto& row : host.level->floorHeights) { std::fill(std::begin(row), std::end(row), std::uint8_t{100}); }
        for (auto& row : host.level->surfaceFlags) { std::fill(std::begin(row), std::end(row), std::uint8_t{1}); }
        std::fill(std::begin(host.collisionMap->data), std::end(host.collisionMap->data), std::uint8_t{1});
        host.worldObjects[1].posY = static_cast<std::int16_t>(host.GetTerrainFloorHeight(1500, 0, 2500) + 20);
        Step(host);
        Check(host.playerUnderwater && host.playerOxygen == GameContext::PlayerOxygenMaximum - 14625 &&
            host.transitions.back(), "Real floor-following movement must drive oxygen and the tint callback.");
    }
}

int main()
{
    try
    {
        TestOxygen();
        TestLifecycleAndAudio();
        TestBubblesAndMovement();
        std::cout << "PASS: native oxygen/drowning, looping audio, lifecycle callbacks, exhaled bubbles and surface pops.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
