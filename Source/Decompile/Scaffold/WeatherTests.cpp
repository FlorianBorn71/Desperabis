#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>

using namespace Desperabis;

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition) { throw std::runtime_error(message); }
    }

    std::uint16_t Random(std::uint32_t& seed, std::uint16_t range)
    {
        seed = seed * 134775813u + 1u;
        return static_cast<std::uint16_t>((seed >> 16) % range);
    }

    std::int16_t Signed(std::uint32_t& seed, std::uint16_t radius)
    {
        const auto value = static_cast<std::int16_t>(Random(seed, 2 * radius + 1) - radius);
        return value == 0 ? static_cast<std::int16_t>(radius) : value;
    }

    Level FlatLevel()
    {
        auto level = CreateDummyLevel();
        for (std::size_t row = 0; row < Level::Height; ++row)
        {
            std::fill(std::begin(level.floorHeights[row]), std::end(level.floorHeights[row]), 0);
            std::fill(std::begin(level.floorObjects[row]), std::end(level.floorObjects[row]), 0);
            std::fill(std::begin(level.ceilingObjects[row]), std::end(level.ceilingObjects[row]), 0);
        }
        level.heightSurfaces[0] = Level::HeightSurface{0, 0};
        std::fill(std::begin(level.collisionMap->data), std::end(level.collisionMap->data), 0);
        return level;
    }

    struct Host final : GameContext
    {
        std::filesystem::path original;
        struct Event { bool creation; int slot; RawGameObject record; };
        std::vector<Event> events;
        bool deferred = false;
        std::string message;
        std::vector<LightningFlash> shownLightning;
        int lightningHides = 0;

        void OnObjectCreated(int slot) override { events.push_back({true, slot, worldObjects[slot]}); }
        void OnObjectDestroyed(int slot) override { events.push_back({false, slot, worldObjects[slot]}); }
        void OnLightningShown(const LightningFlash& flash) override
        {
            Check(!GetLightningFlashes().empty() && &flash == &GetLightningFlashes().back(),
                "Show callback must receive the installed complete flash.");
            shownLightning.push_back(flash);
        }
        void OnLightningHidden() override
        {
            Check(GetLightningFlashes().empty(), "Hide callback must follow clearing the active snapshot.");
            ++lightningHides;
        }
        void PromptUserMessage(std::string_view text, std::int16_t) override { message = text; }
        bool IsUserMessageActive() const override { return !message.empty(); }
        void LoadLevel(const LevelLoadRequest& request) override
        {
            if (deferred) { return; }
            LevelLoadData data;
            if (original.empty())
            {
                data.level = std::make_unique<Level>(FlatLevel());
                data.level->hasSky = true;
                data.playerSpawn = PlayerCamera{{1500, 180, 1500}, {0, 0, 100}};
            }
            else
            {
                LoadLevelGeometry(request.levelNumber, data);
                LoadLevelObjects(request.levelNumber, data);
                InitializeLevelPlayerSpawn(data, {0, 0, 100});
            }
            CompleteLevelLoad(request.id, std::move(data));
            events.clear();
        }
        std::vector<std::uint8_t> LoadFile(std::string_view name) override
        {
            std::ifstream file(original / std::string(name), std::ios::binary);
            if (!file) { throw std::runtime_error("Missing weather fixture: " + std::string(name)); }
            return {std::istreambuf_iterator<char>(file), {}};
        }
    };

    struct Audio final : AudioHandler
    {
        struct Playback { std::uint8_t sample, volume; };
        std::vector<Playback> plays;
        std::uint16_t GetSampleCount() const override { return 129; }
        bool IsChannelPlaying(std::size_t) const override { return false; }
        void PlaySample(std::size_t, std::uint8_t sample, std::uint8_t volume) override
        { plays.push_back({sample, volume}); }
        void StopAllSamples() override {}
    };

    struct NoInput final : InputHandler
    {
        bool IsKeyPressed(InputKey) override { return false; }
    };

    RawGameObject Leaf(BehaviorType behavior)
    {
        RawGameObject leaf;
        leaf.health = 1000;
        leaf.behavior = behavior;
        leaf.posX = leaf.posZ = 1500;
        leaf.posY = 20;
        leaf.animFrame = 3;
        leaf.velocityX = -2;
        leaf.velocityZ = 3;
        leaf.velocityY = -3;
        leaf.itemValue = 2;
        return leaf;
    }

    void TestWindAndLeaves()
    {
        Host host;
        host.StartLevel(10);
        for (std::uint32_t seed = 0; seed < 200; ++seed)
        {
            auto wind = Leaf(BehaviorType::WindController);
            host.tickCount = static_cast<std::int16_t>(seed % 4);
            host.randomSeed = seed;
            host.windX = -3;
            host.windZ = 2;
            auto expectedSeed = seed;
            auto targetX = wind.velocityX, targetZ = wind.velocityZ;
            if (Random(expectedSeed, 30) == 0)
            {
                targetX = Signed(expectedSeed, 8);
                targetZ = Signed(expectedSeed, 8);
            }
            auto x = host.windX, z = host.windZ;
            if (Random(expectedSeed, 5) == 0)
            {
                x += SignWord(targetX - x);
                z += SignWord(targetZ - z);
            }
            DispatchObjectCase(wind, host);
            Check(host.randomSeed == expectedSeed && wind.velocityX == targetX && wind.velocityZ == targetZ &&
                host.windX == x && host.windZ == z, "Wind must update once, including zero-tick batches.");
        }

        for (const auto behavior : {BehaviorType::AirborneLeaf, BehaviorType::GroundLeaf})
        for (const std::uint8_t pixel : {std::uint8_t{0}, std::uint8_t{1}, std::uint8_t{56}})
        for (std::uint32_t seed = 0; seed < 100; ++seed)
        {
            host.randomSeed = seed;
            host.tickCount = static_cast<std::int16_t>(seed % 4);
            host.windX = 8;
            host.windZ = -5;
            std::fill(std::begin(host.collisionMap->data), std::end(host.collisionMap->data), pixel);
            auto actual = Leaf(behavior);
            if (seed % 3 == 0) { actual.animFrame = 4; }
            if (seed % 3 == 1) { actual.health = 1200; actual.posY = 1; }
            auto expected = actual;
            auto expectedSeed = seed;
            for (int tick = 0; tick < host.tickCount; ++tick)
            {
                if (expected.animFrame > 3) { expected.health = 0; }
                if (behavior == BehaviorType::GroundLeaf)
                {
                    expected.renderPlacementMode = 2;
                    expected.posY = 0;
                    if (Random(expectedSeed, 50) == 0) { expected.velocityX = 1; expected.velocityZ = -1; }
                    const bool ground = (pixel & 7) == 1;
                    if (Random(expectedSeed, 4) == 0 && ground) { expected.posX += expected.velocityX; }
                    if (Random(expectedSeed, 4) == 0 && ground) { expected.posZ += expected.velocityZ; }
                    if (!ground)
                    {
                        if (expected.itemValue < 1) { expected.behavior = BehaviorType::AirborneLeaf; }
                        else { --expected.itemValue; }
                    }
                    continue;
                }
                expected.renderPlacementMode = 0;
                expected.posY += expected.velocityY;
                if (expected.posY < 0)
                {
                    expected.posY = 0;
                    expected.velocityX = expected.velocityZ = 0;
                    expected.velocityY = static_cast<std::int16_t>(Random(expectedSeed, 6) + 2);
                    if (expected.health < 1000) { expected.behavior = BehaviorType::None; }
                }
                else
                {
                    expected.winX = static_cast<std::int16_t>((expected.winX + 6 + Random(expectedSeed, 5)) & 511);
                    expected.winZ = static_cast<std::int16_t>((expected.winX + 3 + Random(expectedSeed, 5)) & 511);
                }
                if (Random(expectedSeed, 10) == 0)
                {
                    expected.velocityX += SignWord(8 - expected.velocityX);
                    expected.velocityZ += SignWord(-5 - expected.velocityZ);
                }
                if ((pixel & 7) == 1)
                {
                    expected.itemValue = 100;
                    expected.velocityX = expected.velocityZ = 0;
                    expected.behavior = BehaviorType::GroundLeaf;
                }
                if ((pixel & 56) != 0)
                {
                    --expected.velocityY;
                    if (expected.health > 1000) { expected.health = 100; }
                    else { expected.velocityY = 0; }
                    expected.posX -= expected.velocityX;
                    expected.posZ -= expected.velocityZ;
                    expected.velocityX = -expected.velocityX;
                    expected.velocityZ = -expected.velocityZ;
                }
                else
                {
                    expected.posX += expected.velocityX;
                    expected.posZ += expected.velocityZ;
                    expected.velocityY += SignWord(-1 - expected.velocityY);
                }
            }
            DispatchObjectCase(actual, host);
            Check(host.randomSeed == expectedSeed && std::memcmp(&actual, &expected, sizeof(actual)) == 0,
                "Leaf reference mismatch: movement, death continuation, transitions or RNG order.");
        }
    }

    std::vector<LightningLine> ReferenceFlash(std::uint32_t& seed, std::int16_t startX, std::int16_t horizon)
    {
        std::vector<LightningLine> lines;
        std::function<void(std::int16_t, std::int16_t)> recurse = [&](std::int16_t x, std::int16_t y)
        {
            const auto nextX = WrapSignedWord(x + Signed(seed, 20));
            const auto nextY = static_cast<std::int16_t>(y + 6 + Random(seed, 15));
            if (nextY > horizon) { return; }
            lines.push_back({x, y, nextX, nextY});
            recurse(nextX, nextY);
            if (Random(seed, 5) == 0) { recurse(nextX, nextY); }
        };
        recurse(startX, 0);
        return lines;
    }

    void CheckFlash(const LightningFlash& actual, const std::vector<LightningLine>& expected)
    {
        Check(actual.paletteIndex == 250 && actual.renderTable == 16 && actual.lines.size() == expected.size(),
            "Flash metadata or complete native line count differs.");
        if (!actual.lines.empty()) { Check(actual.lines.front().thickness1 == 2.f, "Lightning trunk must start two native pixels thick."); }
        bool hasTip = actual.lines.empty();
        for (std::size_t i = 0; i < expected.size(); ++i)
        {
            const auto& a = actual.lines[i];
            const auto& e = expected[i];
            Check(a.x1 == e.x1 && a.y1 == e.y1 && a.x2 == e.x2 && a.y2 == e.y2,
                "Lightning endpoints/branch order differ from native recursion.");
            Check(a.thickness1 > 0.f && a.thickness2 >= 0.f && a.thickness2 < a.thickness1,
                "Every lightning segment must taper without negative thickness.");
            hasTip = hasTip || a.thickness2 == 0.f;
        }
        Check(hasTip, "Lightning tips must end at exactly zero thickness.");
    }

    void TestLightning(const std::filesystem::path& original)
    {
        Host host;
        host.StartLevel(10);
        std::size_t largest = 0;
        for (std::uint32_t seed = 0; seed < 200; ++seed)
        for (const std::int16_t horizon : {std::int16_t{-1}, std::int16_t{110}, std::int16_t{170}})
        {
            host.ClearLightningFlashes();
            host.randomSeed = seed;
            host.playerLookHeight = horizon;
            auto expectedSeed = seed;
            const auto expected = ReferenceFlash(expectedSeed, -80, horizon);
            host.GenerateLightningFlash(-80);
            CheckFlash(host.GetLightningFlashes().at(0), expected);
            Check(host.randomSeed == expectedSeed, "Lightning must retain terminating and postorder branch RNG.");
            largest = std::max(largest, expected.size());
        }
        Check(largest > 20, "Reference seeds must exercise substantial branched bolts.");
        host.playerLookHeight = 32767;
        bool rejected = false;
        try { host.GenerateLightningFlash(0); }
        catch (const std::overflow_error&) { rejected = true; }
        Check(rejected, "An overflowing native horizon must be rejected explicitly.");
        host.playerLookHeight = 110;
        Audio audio;
        const auto soundTable = [&]()
        {
            std::ifstream file(original / "SAMP\\S_TAB.PRB", std::ios::binary);
            Check(static_cast<bool>(file), "Missing original thunder sample table.");
            return std::vector<std::uint8_t>{std::istreambuf_iterator<char>(file), {}};
        }();
        host.LoadSoundEffects(soundTable.data(), soundTable.size());
        host.audioHandler = &audio;
        auto& storm = host.worldObjects[82];
        storm = Leaf(BehaviorType::LightningGenerator);
        storm.generalPurpose2 = 90;
        storm.itemValue = 134;
        host.randomSeed = 1;
        host.tickCount = 1;
        host.ClearLightningFlashes();
        auto seed = host.randomSeed;
        bool flashed = false, thundered = false;
        for (int step = 0; step < 200 && !thundered; ++step)
        {
            auto timer = WrapSignedWord(storm.generalPurpose2);
            const auto item = storm.itemValue;
            host.ClearLightningFlashes();
            if (timer > 0) { --timer; }
            else
            {
                const auto nextTimer = static_cast<std::int16_t>(Random(seed, 1000) + 200);
                const auto nextItem = static_cast<std::int16_t>(Random(seed, 400) - 80);
                const auto volume = GameContext::GetSoundVolume(static_cast<std::int16_t>(Random(seed, 300)));
                const std::array<std::uint8_t, 3> samples{102, 25, 103};
                const auto sample = samples[Random(seed, 3)];
                DispatchObjectCase(storm, host);
                Check(flashed && audio.plays.size() == 1 && audio.plays[0].sample == sample &&
                    audio.plays[0].volume == volume && storm.generalPurpose2 == nextTimer && storm.itemValue == nextItem &&
                    std::find(host.soundChannelSources.begin(), host.soundChannelSources.end(), 82) != host.soundChannelSources.end(),
                    "Thunder must follow the flash/tail and retain native sample, volume and source.");
                thundered = true;
                Check(host.randomSeed == seed, "Thunder sample selection must preserve the native RNG sequence.");
                continue;
            }
            std::vector<LightningLine> expected;
            bool flash = false;
            if (timer > 1 && timer <= 80) { if (Random(seed, 3) == 0) { --timer; } }
            else if (timer >= 81 && timer <= 89 && (timer & 1) != 0)
            {
                flash = Random(seed, 2) == 0;
                if (flash) { expected = ReferenceFlash(seed, item, host.playerLookHeight); }
            }
            DispatchObjectCase(storm, host);
            Check(storm.generalPurpose2 == timer && host.randomSeed == seed &&
                host.GetLightningFlashes().size() == (flash ? 1u : 0u) && audio.plays.empty(),
                "Lightning timer bands, acceleration, flash admission or thunder timing differ.");
            if (flash)
            {
                flashed = true;
                CheckFlash(host.GetLightningFlashes()[0], expected);
                Check(host.shownLightning.back().sourceSlot == 82 && host.shownLightning.back().sourcePosition.has_value(),
                    "Native generator dispatch must deliver its source metadata in the show callback.");
            }
        }
        Check(thundered, "Flash cycle did not finish with thunder.");
        host.skyEnabled = false;
        storm.generalPurpose2 = 90;
        seed = host.randomSeed;
        DispatchObjectCase(storm, host);
        Check(storm.generalPurpose2 == 90 && host.randomSeed == seed, "No sky must freeze lightning without RNG.");
        host.skyEnabled = true;
        host.level->ceilingObjects[25][14] = 1;
        storm.generalPurpose2 = 90;
        host.randomSeed = seed = 1;
        host.ClearLightningFlashes();
        Random(seed, 2);
        DispatchObjectCase(storm, host);
        Check(host.GetLightningFlashes().empty() && host.randomSeed == seed, "Player roof must suppress geometry after admission RNG.");
        host.level->ceilingObjects[25][14] = 0;
        host.GenerateLightningFlash(10);
        host.SetPaused(true);
        Check(!host.GetLightningFlashes().empty(), "Paused presentation must retain its world-frame snapshot.");
        host.SetPaused(false);
        host.tickCount = 0;
        UpdateObjects(host);
        Check(host.GetLightningFlashes().empty(), "A new object-update batch must clear old lightning.");
        host.GenerateLightningFlash(10);
        host.windX = 8;
        host.windZ = -8;
        host.StartLevel(11);
        Check(host.GetLightningFlashes().empty() && host.windX == 0 && host.windZ == 0 && host.skyEnabled,
            "Level installation must reset weather state and load its sky gate.");
        host.GenerateLightningFlash(10);
        host.EndLevel(true);
        Check(host.GetLightningFlashes().empty() && !host.skyEnabled && host.windX == 0,
            "Part completion must release transient weather.");
    }

    void TestRain()
    {
        for (const std::int16_t ticks : {std::int16_t{0}, std::int16_t{1}, std::int16_t{3}, std::int16_t{32767}})
        {
            Host host;
            host.StartLevel(10);
            host.windX = -8;
            host.windZ = 7;
            host.tickCount = ticks;
            host.randomSeed = 123;
            auto emitter = Leaf(BehaviorType::RainEmitter);
            emitter.itemValue = 2000;
            emitter.generalPurpose1 = 168;
            auto total = static_cast<std::uint16_t>(2000u * ticks + 168);
            const auto count = total / 256;
            auto seed = host.randomSeed;
            for (int i = 0; i < count; ++i)
            {
                const auto angle = WrapSignedWord(256 - host.GetPlayerHeading() + Signed(seed, 100));
                const auto radius = static_cast<std::int16_t>(Random(seed, 250) + 100);
                std::int16_t x, z;
                GetClampedDirectionDisplacement(x, z, angle, radius);
                x += host.soundListenerX;
                z += host.soundListenerZ;
                const auto y = static_cast<std::int16_t>(host.GetPlayerCamera().position[1] + 100 + Random(seed, 70));
                const auto vy = static_cast<std::int16_t>(Random(seed, 5) - 19);
                const auto rotation = Random(seed, 2);
                // The first two allocations select the same still-zero-health slot.
                if (i == 0) { Check(x >= 1150 && x <= 1850 && z >= 1150 && z <= 1850, "Rain must spawn around the listener."); }
                if (i == 0)
                {
                    host.worldObjects[0].posX = x;
                    host.worldObjects[0].posY = y;
                    host.worldObjects[0].posZ = z;
                    host.worldObjects[0].velocityY = vy;
                    host.worldObjects[0].winY = static_cast<std::int16_t>(rotation);
                }
            }
            DispatchObjectCase(emitter, host);
            Check(emitter.generalPurpose1 == total % 256 && host.randomSeed == seed && host.events.size() ==
                (count <= 164 ? count : 164 + 2 * (count - 164)),
                "Rain emission rate, low-word overflow, remainder, double allocation or fallback lifecycle differs.");
            if (count != 0)
            {
                const auto& drop = host.worldObjects[2];
                const auto& expected = host.worldObjects[0];
                Check(drop.health == 60 && drop.modelId == 1 && drop.modelAnim == 4 && drop.animFrame == 1 &&
                    drop.spriteScale == 29 && static_cast<int>(drop.displayType) == 13 &&
                    drop.blendWeightOrSpriteOpacity == 4 && drop.velocityX == -2 && drop.velocityZ == 2 &&
                    drop.velocityY == expected.velocityY && drop.winY == expected.winY &&
                    drop.posX == expected.posX && drop.posY == expected.posY && drop.posZ == expected.posZ,
                    "Rain factory fields, signed wind division or initial RNG differ.");
                Check(host.events[0].creation && host.events[0].slot == 2 &&
                    host.events[0].record.behavior == BehaviorType::RainDrop, "Creation must expose a fully initialized drop.");
            }
        }
        Host host;
        host.StartLevel(10);
        host.level->ceilingObjects[25][14] = 1;
        host.InitializeRainDrop(host.FindFreeObjectIndex(), 1600, 300, 1500);
        Check(host.worldObjects[2].health == 0 && host.events.empty(), "The player's roof must reject rain even at an outdoor spawn tile.");
        host.playerMapCollisionsEnabled = false;
        host.InitializeRainDrop(host.FindFreeObjectIndex(), 1600, 300, 1500);
        Check(host.worldObjects[2].health == 60, "Disabled map collisions must bypass only the player-roof check.");
        host.SetObjectHealth(host.worldObjects[2], 0);
        std::fill(std::begin(host.collisionMap->data), std::end(host.collisionMap->data), 8);
        host.InitializeRainDrop(host.FindFreeObjectIndex(), 1600, 300, 1500);
        Check(host.worldObjects[2].health == 0, "Spawn mask 8 must reject rain even with map collisions disabled.");
        std::fill(std::begin(host.collisionMap->data), std::end(host.collisionMap->data), 0);
        host.level->ceilingObjects[25][14] = 0;
        auto& drop = host.worldObjects[2];
        host.InitializeRainDrop(host.FindFreeObjectIndex(), 1600, 5, 1500);
        drop.velocityX = -2;
        drop.velocityZ = 3;
        host.tickCount = 0;
        DispatchObjectCase(drop, host);
        Check(drop.posX == 1598 && drop.posZ == 1503 && drop.posY == 5, "Zero-tick rain still moves horizontally once.");
        host.tickCount = 3;
        auto seed = host.randomSeed;
        const auto life = Random(seed, 4) + 1;
        UpdateObject(drop, host);
        Check(drop.posY == 0 && drop.posX == 1596 && drop.posZ == 1506 && drop.animFrame == 2 &&
            drop.behavior == BehaviorType::None && drop.health == life && host.randomSeed == seed,
            "Rain must turn into a short-lived, floor-clamped splash.");
        host.tickCount = static_cast<std::int16_t>(life);
        UpdateObject(drop, host);
        Check(drop.health == 0, "Generic countdown must retire the splash.");
    }

    void TestLightningCallbacks()
    {
        Host host;
        host.StartLevel(10);
        Check(host.shownLightning.empty() && host.lightningHides == 0, "No flash means no spurious hide callback.");
        auto& storm = host.worldObjects[82];
        storm = Leaf(BehaviorType::LightningGenerator);
        host.GenerateLightningFlash(134, &storm);
        host.GenerateLightningFlash(135, &storm);
        Check(host.shownLightning.size() == 2 && host.lightningHides == 0 &&
            host.shownLightning[0].sourceSlot == 82 &&
            host.shownLightning[0].sourcePosition == std::optional<std::array<std::int16_t, 3>>{{1500, 20, 1500}},
            "Each flash must notify the host with generator slot/position and complete geometry.");
        host.SetPaused(true);
        NoInput input;
        Check(host.Tick({1, 1}, input).status == GameTickStatus::Paused,
            "Paused ticks must not advance lightning presentation.");
        Check(host.lightningHides == 0, "Pausing must not hide the frozen world flash.");
        host.SetPaused(false);
        host.SetMenuPage(MenuPage::Inventory);
        Check(host.Tick({1, 1}, input).status == GameTickStatus::Paused && host.lightningHides == 0,
            "Modal ticks must retain lightning without show/hide callbacks.");
        host.SetMenuPage(MenuPage::Gameplay);
        host.tickCount = 0;
        UpdateObjects(host);
        Check(host.lightningHides == 1 && host.GetLightningFlashes().empty(),
            "The next object batch must hide all previous flashes exactly once.");
        host.ClearLightningFlashes();
        Check(host.lightningHides == 1, "An already-hidden flash must not notify again.");
        host.GenerateLightningFlash(10);
        Check(!host.shownLightning.back().sourcePosition && host.shownLightning.back().sourceSlot == -1,
            "Standalone geometry must not invent a world-space source.");
        host.deferred = true;
        const auto request = host.StartLevel(11);
        Check(host.lightningHides == 2, "Loading must immediately hide old lightning, even before completion.");
        LevelLoadData data;
        data.level = std::make_unique<Level>(FlatLevel());
        data.level->hasSky = true;
        data.playerSpawn = PlayerCamera{{1500, 180, 1500}, {0, 0, 100}};
        host.CompleteLevelLoad(request.id, std::move(data));
        Check(host.lightningHides == 2, "Load completion must not repeat an already-issued hide.");
        host.GenerateLightningFlash(20);
        host.EndLevel(true);
        Check(host.lightningHides == 3 && host.GetLightningFlashes().empty(), "Unloading a part must hide its lightning.");
    }

    void TestOriginalLevel(const std::filesystem::path& original)
    {
        Host host;
        host.original = original;
        host.StartLevel(10);
        Check(host.skyEnabled && host.worldObjects[12].behavior == BehaviorType::WindController &&
            host.worldObjects[34].behavior == BehaviorType::GroundLeaf &&
            host.worldObjects[79].behavior == BehaviorType::RainEmitter &&
            host.worldObjects[82].behavior == BehaviorType::LightningGenerator,
            "Original level 10 must bind its recovered weather selectors and sky.");
        auto& entrance = host.worldObjects[37];
        const auto destination = host.worldObjects[38];
        const auto key = host.worldObjects[63];
        Check(entrance.collisionType == CollisionType::Teleporter && entrance.doorLockOrGroupBitmask == 99 &&
            key.collisionType == CollisionType::Key && key.itemValue == 99 && host.worldObjects[85].doorLockOrGroupBitmask == 1,
            "The building uses an ordinary paired, key-99 teleporter, not a weather controller/master key.");
        const auto before = host.GetPlayerCamera();
        host.HandleTeleporterContact(entrance, true);
        Check(entrance.doorLockOrGroupBitmask == 99 && host.GetPlayerCamera().position == before.position,
            "The locked building must reject entry without its key.");
        host.inventory[0] = key;
        host.SetObjectHealth(host.worldObjects[63], 0);
        host.HandleTeleporterContact(entrance, true);
        Check(entrance.doorLockOrGroupBitmask == 0 && host.inventory[0].health == 0,
            "The existing interaction must consume key 99 and unlock the entrance.");
        host.HandleTeleporterContact(entrance, true);
        Check(host.worldObjects[1].posX == destination.posX && host.worldObjects[1].posZ == destination.posZ &&
            host.field_D503 && host.worldObjects[38].generalPurpose3 == 0,
            "The unlocked entrance must teleport into the building and install the destination mode/cooldown.");
        host.StartLevel(10);
        auto& storm = host.worldObjects[82];
        storm.generalPurpose2 = 90;
        host.randomSeed = 1;
        host.worldObjects[1].posX = host.soundListenerX = 2400;
        host.worldObjects[1].posZ = host.soundListenerZ = 2200;
        host.events.clear();
        bool flashed = false;
        for (int frame = 0; frame < 10; ++frame)
        {
            UpdateObjects(host);
            flashed = flashed || !host.GetLightningFlashes().empty();
        }
        Check(flashed, "Original outdoor fixture must generate a bolt within its flash band.");
        Check(std::any_of(host.events.begin(), host.events.end(), [](const Host::Event& event)
            { return event.creation && event.record.behavior == BehaviorType::RainDrop; }),
            "The original fixture must execute weather and create individual drops.");
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc != 2) { throw std::invalid_argument("Expected the original asset directory."); }
        const std::filesystem::path original(argv[1]);
        TestWindAndLeaves();
        TestRain();
        TestLightning(original);
        TestLightningCallbacks();
        TestOriginalLevel(original);
        std::cout << "Level-10 wind, leaves, rain, lightning/thunder and locked-teleporter tests passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
