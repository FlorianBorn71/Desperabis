#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
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

    struct Input final : InputHandler
    {
        bool forward = false;
        bool jump = false;
        bool interact = false;
        bool IsKeyPressed(InputKey key) override
        {
            return (key == InputKey::ForwardKey && forward) || (key == InputKey::JumpKey && jump) ||
                (key == InputKey::InteractionKey && interact);
        }
    };

    struct Audio final : AudioHandler
    {
        struct Playback { std::size_t channel; std::uint8_t sample; std::uint8_t volume; };
        std::vector<Playback> plays;
        std::array<bool, SoundChannelCount> busy{};
        bool keepPlaying = false;
        std::uint16_t GetSampleCount() const override { return 129; }
        bool IsChannelPlaying(std::size_t channel) const override { return busy.at(channel); }
        void PlaySample(std::size_t channel, std::uint8_t sample, std::uint8_t volume) override
        {
            plays.push_back({ channel,sample,volume });
            busy.at(channel) = keepPlaying;
        }
        void StopAllSamples() override { busy.fill(false); }
        void StopAmbientPlayback() override {}
    };

    struct Host final : GameContext
    {
        std::filesystem::path root;
        using GameContext::LoadLevelObjects;
        void Load(std::int16_t eyeY = 45)
        {
            const auto request = StartLevel(11);
            LevelLoadData data;
            data.level = std::make_unique<Level>(CreateDummyLevel());
            data.playerSpawn = PlayerCamera{ {1500,eyeY,2500}, {0,0,100} };
            CompleteLevelLoad(request.id, std::move(data));
        }
        void EnableAudio(Audio& audio)
        {
            audioHandler = &audio;
            soundEffectsLoaded = true;
            for (std::size_t index = 0; index < soundEffects.size(); ++index)
            {
                soundEffects[index].sampleCount = 1;
                soundEffects[index].samples[0] = static_cast<std::uint8_t>(index + 1);
            }
        }
    protected:
        void LoadLevel(const LevelLoadRequest&) override {}
        std::vector<std::uint8_t> LoadFile(std::string_view filename) override
        {
            std::ifstream input(root / filename, std::ios::binary);
            if (!input) { throw std::runtime_error("Cannot read original bridge fixture."); }
            return { std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>() };
        }
    };

    void Step(Host& host, Input& input, std::int16_t ticks = 1)
    {
        Check(host.Tick({0,ticks}, input).status == GameTickStatus::Advanced, "Support tick failed.");
    }

    int Eye(const Host& host) { return host.GetPlayerCamera().position[1]; }

    RawGameObject Bridge(const Host& host, std::int16_t y = 100)
    {
        RawGameObject bridge;
        bridge.health = 1000;
        bridge.posX = host.worldObjects[1].posX;
        bridge.posY = y;
        bridge.posZ = host.worldObjects[1].posZ;
        bridge.collisionType = CollisionType::WalkablePlanks;
        bridge.collisionMask = 3;
        bridge.collisionRadiusXZ = 30;
        bridge.collisionRadiusY = 30;
        bridge.soundEffectId = 85;
        return bridge;
    }

    void SetSlope(Level& level, std::uint8_t rightHeight)
    {
        for (auto& row : level.floorHeights)
        {
            std::fill(std::begin(row), std::end(row), std::uint8_t{100});
            std::fill(std::begin(row) + 15, std::end(row), rightHeight);
        }
    }

    void TestSlopes()
    {
        Host host;
        Input input;
        host.Load();
        std::int16_t x = 77;
        std::int16_t z = 88;
        Check(host.QueryTerrainSupport(1500, 45, 2500, x, z) == 0 && x == 0 && z == 0,
            "Flat terrain must return its floor and reset both correction outputs.");
        SetSlope(*host.level, 125);
        const auto center = host.GetTerrainFloorHeight(1500, 45, 2500);
        Check(center - host.GetTerrainFloorHeight(1530, 45, 2500) == 16,
            "Threshold fixture must have an exact 16-unit height difference.");
        host.QueryTerrainSupport(1500, 45, 2500, x, z);
        Check(x == 0 && z == 0, "A difference of exactly 16 must not push the player.");
        SetSlope(*host.level, 128);
        Check(center - host.GetTerrainFloorHeight(1530, 45, 2500) == 18,
            "Steep fixture must exceed the threshold at the nearest representable even terrain height.");
        host.QueryTerrainSupport(1500, 45, 2500, x, z);
        Check(x == 3 && z == 0, "Three steep neighbors must push the player down the X slope.");
        Step(host, input);
        Check(host.worldObjects[1].posX == 1503, "The controller must apply terrain correction even without input.");
        host.Load();
        for (std::size_t row = 0; row < Level::Height; ++row)
        {
            for (std::size_t column = 0; column < Level::Width; ++column)
            {
                host.level->floorHeights[row][column] = row <= 15 ? 128 : 100;
            }
        }
        host.QueryTerrainSupport(1500, 45, 2500, x, z);
        Check(x == 0 && z == 3, "The terrain probe must use world Z rather than the height-array row direction.");
        for (std::size_t row = 0; row < Level::Height; ++row)
        {
            for (std::size_t column = 0; column < Level::Width; ++column)
            {
                host.level->floorHeights[row][column] = static_cast<std::uint8_t>(
                    100 + (row <= 15 ? 28 : 0) + (column >= 15 ? 28 : 0));
            }
        }
        host.QueryTerrainSupport(1500, 45, 2500, x, z);
        Check(x == 2 && z == 2, "Diagonal neighbors must contribute to both correction words.");
        host.Load();
        SetSlope(*host.level, 70);
        host.QueryTerrainSupport(1500, 45, 2500, x, z);
        Check(x == -3 && z == 0, "The opposite slope must reverse the correction.");
        host.field_D500 = false;
        Step(host, input);
        Check(host.worldObjects[1].posX == 1500, "The native body-mode gate must suppress slope movement.");

        host.Load();
        SetSlope(*host.level, 128);
        for (int px = 1400; px <= 1600; px += 5)
        {
            for (int pz = 2400; pz <= 2600; pz += 5)
            {
                host.SetCollisionMapPixelAtWorldXZ(static_cast<std::int16_t>(px),
                    static_cast<std::int16_t>(pz), 0x18);
            }
        }
        Step(host, input);
        Check(host.worldObjects[1].posX == 1500 && host.worldObjects[1].posZ == 2500,
            "Slope correction must not bypass collision-map rejection.");
    }

    void TestBridgeContacts()
    {
        Input input;
        for (int rejection = 0; rejection <= 7; ++rejection)
        {
            Host host;
            host.Load(rejection == 5 ? 95 : 145);
            auto& bridge = host.worldObjects[2];
            bridge = Bridge(host);
            if (rejection == 1) { bridge.health = 999; }
            if (rejection == 2) { bridge.collisionMask = 0; }
            if (rejection == 3) { bridge.blendWeightOrSpriteOpacity = 100; }
            if (rejection == 4) { bridge.collisionType = CollisionType::None; }
            if (rejection == 6) { host.field_D500 = false; }
            if (rejection == 7) { bridge.collisionMask = 0x83; }
            Step(host, input);
            const bool supported = rejection == 0 || rejection == 7;
            Check(host.playerGrounded == supported && bridge.itemValue == (supported ? 7 : 0),
                "Only live, overlapping, mask-compatible, opened planks above the player's feet may support them.");
            Check(Eye(host) == (supported ? 145 : (rejection == 5 ? 94 : 144)),
                "Bridge support must cancel gravity; rejected contacts must keep falling.");
        }
        Host host;
        host.Load(140);
        host.worldObjects[2] = Bridge(host);
        Step(host, input);
        Check(Eye(host) == 144 && host.playerGrounded, "Bridge eye height must approach its target by four.");
        Step(host, input);
        Check(Eye(host) == 145, "Bridge support must settle at plank Y plus body height.");
        host.worldObjects[1].posX += 500;
        Step(host, input);
        Check(Eye(host) == 144 && !host.playerGrounded, "Leaving the bridge must restore falling immediately.");

        host.Load(145);
        auto& bridge = host.worldObjects[2];
        bridge = Bridge(host);
        const auto sum = host.worldObjects[1].collisionRadiusXZ + bridge.collisionRadiusXZ;
        const auto threshold = 3 * sum / 2;
        bridge.posX = static_cast<std::int16_t>(1500 + threshold - 1);
        Check(threshold - 1 > sum, "Widened bridge probe fixture must lie outside ordinary object overlap.");
        Step(host, input);
        Check(host.playerGrounded, "Bridge contact must use the native 1.5x horizontal radius.");
        bridge.posX = static_cast<std::int16_t>(host.worldObjects[1].posX + threshold);
        Step(host, input);
        Check(!host.playerGrounded, "Bridge horizontal overlap must be strict at the widened boundary.");

        host.Load(145);
        SetSlope(*host.level, 128);
        host.worldObjects[2] = Bridge(host);
        Step(host, input);
        Check(host.playerGrounded && host.worldObjects[1].posX == 1500 && host.playerLookHeight == 110,
            "Bridge support must suppress both terrain pushing and terrain-look slope correction.");

        for (int count : {49,50})
        {
            host.Load(145);
            for (int slot = 2; slot < count + 2; ++slot)
            {
                host.worldObjects[slot] = Bridge(host);
                host.worldObjects[slot].collisionType = CollisionType::None;
            }
            host.worldObjects[count + 2] = Bridge(host);
            Step(host, input);
            Check(host.playerGrounded == (count == 49), "Only the first 50 native contacts may supply bridge support.");
        }
    }

    std::uint16_t Random(std::uint32_t& seed, std::uint16_t range)
    {
        seed = seed * 0x08088405u + 1u;
        return static_cast<std::uint16_t>((seed >> 16) % range);
    }

    void TestFootsteps()
    {
        Audio audio;
        Host host;
        Input input;
        host.Load();
        host.EnableAudio(audio);
        Step(host, input, 20);
        Check(audio.plays.empty() && host.randomSeed == 0, "Standing on ordinary ground must not draw RNG or play footsteps.");
        input.forward = true;
        host.playerWalkPhase = 247;
        Step(host, input);
        Check(host.playerWalkPhase == 256 && audio.plays.empty(), "Exactly 256 phase units must not trigger a step.");
        std::uint32_t seed = host.randomSeed;
        const auto volume = GameContext::GetSoundVolume(static_cast<std::int16_t>(430 + Random(seed, 150)));
        Random(seed, 1);
        Step(host, input);
        Check(host.playerWalkPhase == 15 && audio.plays.size() == 1 && audio.plays[0].sample == 1 &&
            audio.plays[0].volume == volume && host.randomSeed == seed &&
            host.soundChannelSources[audio.plays[0].channel] == -6,
            "A step must wrap once, choose the terrain effect, and draw volume before the sample variant.");

        for (std::uint8_t surface = 0; surface < 8; ++surface)
        {
            Audio materialAudio;
            Host material;
            Input moving;
            moving.forward = true;
            material.Load();
            material.EnableAudio(materialAudio);
            material.randomSeed = 1;
            material.playerWalkPhase = 256;
            material.SetCollisionMapPixelAtWorldXZ(1500, 2500, surface);
            static constexpr std::array<std::uint8_t, 8> effects{1,10,26,10,26,97,10,105};
            Step(material, moving);
            Check(materialAudio.plays.size() == 1 && materialAudio.plays[0].sample == effects[surface],
                "Each collision-map material must use its recovered footstep effect.");
            const auto oldZ = material.worldObjects[1].posZ;
            Step(material, moving);
            if (surface == 4 || surface == 6)
            {
                Check(material.worldObjects[1].posZ - oldZ == (surface == 4 ? 2 : 3),
                    "Slow floor materials must limit speed on the next substep.");
            }
        }

        Audio snowAudio;
        Host snow;
        snow.Load();
        snow.EnableAudio(snowAudio);
        snow.SetCollisionMapPixelAtWorldXZ(1500, 2500, 4);
        snow.playerWalkPhase = 256;
        Step(snow, input);
        Check(snowAudio.plays.size() == 1 && snowAudio.plays[0].sample == 52,
            "Surface four must have its native one-in-five replacement effect.");

        Audio bridgeAudio;
        Host bridge;
        bridge.Load(145);
        bridge.EnableAudio(bridgeAudio);
        bridgeAudio.keepPlaying = true;
        bridge.worldObjects[2] = Bridge(bridge);
        for (int attempt = 0; attempt < 3; ++attempt)
        {
            bridge.playerWalkPhase = 256;
            Step(bridge, input);
        }
        Check(bridgeAudio.plays.size() == 2 && bridgeAudio.plays[0].sample == 85 &&
            bridgeAudio.plays[1].sample == 85,
            "Bridge-provided footsteps must use source -6 and enforce the two-instance limit.");

        Audio landingAudio;
        Host landing;
        Input idle;
        landing.Load(100);
        landing.EnableAudio(landingAudio);
        Step(landing, idle, 10);
        Check(landingAudio.plays.empty(), "Falling without walking must not play until impact.");
        Step(landing, idle);
        Check(landingAudio.plays.size() == 1 && landing.playerWalkPhase == 0 && landing.playerGrounded,
            "A landing faster than four units must play one step and reset the walking phase.");
        landing.Load(50);
        landingAudio.plays.clear();
        Step(landing, idle, 3);
        Check(landing.playerGrounded && landingAudio.plays.empty(), "A soft landing must not manufacture a footstep.");

        Audio mutedAudio;
        Host muted;
        muted.Load();
        muted.EnableAudio(mutedAudio);
        muted.soundEnabled = false;
        muted.playerWalkPhase = 256;
        seed = muted.randomSeed;
        Random(seed, 150);
        Random(seed, 1);
        Step(muted, input);
        Check(mutedAudio.plays.empty() && muted.randomSeed == seed,
            "Muting must suppress playback without changing the native volume/variant RNG order.");

        Audio waterAudio;
        Host water;
        water.Load(-151);
        water.EnableAudio(waterAudio);
        for (auto& row : water.level->floorHeights) { std::fill(std::begin(row), std::end(row), std::uint8_t{100}); }
        for (auto& row : water.level->surfaceFlags) { std::fill(std::begin(row), std::end(row), std::uint8_t{1}); }
        water.level->heightSurfaces[0] = Level::HeightSurface{100,0};
        water.SetCollisionMapPixelAtWorldXZ(1500, 2500, 1);
        water.playerWalkPhase = 257;
        Step(water, idle);
        Check(water.playerUnderwater && waterAudio.plays.empty() && water.randomSeed == 1,
            "Submerged contact must suppress the step and draw only the native underwater phase increment.");

        Audio batchAudio;
        Host batch;
        batch.Load();
        batch.EnableAudio(batchAudio);
        Step(batch, input, 29);
        Check(batchAudio.plays.size() == 1 && batch.playerWalkPhase == 5,
            "Batched substeps must advance cadence using the once-sampled movement speed.");
        const auto oldSeed = batch.randomSeed;
        Step(batch, input, 0);
        Check(batch.randomSeed == oldSeed && batch.playerWalkPhase == 5 && batchAudio.plays.size() == 1,
            "Zero movement ticks must not change cadence or sound.");
    }

    void TestBridgeBehaviors()
    {
        Audio audio;
        Host host;
        host.Load();
        host.EnableAudio(audio);
        auto& object = host.worldObjects[2];
        object = Bridge(host);
        object.itemValue = 7;
        object.generalPurpose2 = 117;
        host.randomSeed = 1;
        host.tickCount = 1;
        Check(Function_WalkablePlanks(object, host).status == DispatchStatus::ExecutedCase &&
            object.generalPurpose2 == 128 && object.winZ == 7 && object.generalPurpose1 == 0,
            "Planks must use the phase at byte offset 38 and the signed high-word sine product.");
        object.generalPurpose2 = 373;
        host.randomSeed = 1;
        Function_WalkablePlanks(object, host);
        Check(object.winZ == -7, "Negative plank displacement must retain its sign.");
        host.tickCount = 19;
        Function_WalkablePlanks(object, host);
        Check(object.itemValue == 6, "A plank update longer than 18 ticks must decay amplitude exactly once.");
        const auto oldPhase = object.generalPurpose2;
        const auto oldSeed = host.randomSeed;
        host.tickCount = 0;
        Function_WalkablePlanks(object, host);
        Check(object.generalPurpose2 == oldPhase && host.randomSeed == oldSeed,
            "A zero-tick plank update must avoid the native divide-by-zero.");

        object = Bridge(host);
        object.health = 1004;
        object.velocityZ = 3;
        object.blendWeightOrSpriteOpacity = 10;
        host.tickCount = 2;
        Function_WalkableDrawBridge(object, host);
        Check(object.blendWeightOrSpriteOpacity == 10 && audio.plays.empty(),
            "A drawbridge must remain closed before activation changes its health to 1000.");
        host.SetObjectHealth(object, 1000);
        Function_WalkableDrawBridge(object, host);
        Check(object.blendWeightOrSpriteOpacity == 4 && audio.plays.size() == 1 && audio.plays[0].sample == 85,
            "An activated drawbridge must approach angle zero at velocityZ*ticks and sound while moving.");
        Function_WalkableDrawBridge(object, host);
        Check(object.blendWeightOrSpriteOpacity == 0 && audio.plays.size() == 1,
            "An open drawbridge must stop at zero, enabling the player support gate.");
    }

    void SetUpPortal(Host& host)
    {
        host.Load();
        host.worldObjects[2] = Bridge(host, 20);
        auto& source = host.worldObjects[2];
        source.behavior = BehaviorType::Teleporter;
        source.collisionType = CollisionType::Teleporter;
        source.itemValue = 1;
        source.generalPurpose1 = 2;
        source.generalPurpose3 = 20;
        auto& destination = host.worldObjects[3];
        destination = source;
        destination.posX = 2200;
        destination.posZ = 3100;
        destination.posY = 200;
        destination.velocityY = 1;
        destination.itemValue = 2;
        destination.generalPurpose1 = 1;
        destination.winY = 128;
    }

    void TestTeleporters()
    {
        Host host;
        Input input;
        SetUpPortal(host);
        auto& source = host.worldObjects[2];
        auto& destination = host.worldObjects[3];
        input.interact = true;
        destination.velocityZ = 1;
        destination.behaviorParameter = 10;
        Step(host, input);
        const auto camera = host.GetPlayerCamera();
        Check(camera.position[0] == 2200 && camera.position[2] == 3100 && camera.position[1] == 200 &&
            host.GetPlayerHeading() == 128 && camera.direction[0] == 100 && camera.direction[2] == 0,
            "Enter must transfer to the destination eye height and immediately apply its heading.");
        Check(destination.generalPurpose3 == 0 && host.soundListenerX == 2200 && host.soundListenerZ == 3100 &&
            host.playerRenderTable == 10 && host.field_D503,
            "Arrival must reset destination cooldown and refresh listener/render state.");
        Step(host, input);
        Check(host.worldObjects[1].posX == 2200, "Held Enter must not bounce through an uncharged destination.");

        SetUpPortal(host);
        source.generalPurpose3 = 10;
        Check(HandlePlayerObjectInteraction(source, host).status == InteractionStatus::NoEffect,
            "Manual teleport cooldown must be strictly greater than ten.");
        source.generalPurpose3 = 11;
        source.doorLockOrGroupBitmask = 7;
        host.inventory[0].health = 1000;
        host.inventory[0].collisionType = CollisionType::Key;
        host.inventory[0].itemValue = 6;
        Check(HandlePlayerObjectInteraction(source, host).status == InteractionStatus::NoEffect &&
            source.doorLockOrGroupBitmask == 7 && host.inventory[0].health == 1000,
            "Wrong teleporter keys must not be consumed.");
        host.inventory[0].itemValue = 7;
        Check(HandlePlayerObjectInteraction(source, host).status == InteractionStatus::Applied &&
            source.doorLockOrGroupBitmask == 0 && host.inventory[0].health == 0 &&
            host.worldObjects[1].posX == 1500,
            "Unlocking must reuse the door key path without transferring on the same Enter.");
        Check(HandlePlayerObjectInteraction(source, host).status == InteractionStatus::Applied &&
            host.worldObjects[1].posX == 2200, "The following Enter must transfer through the unlocked portal.");

        SetUpPortal(host);
        source.doorLockOrGroupBitmask = 10;
        input.interact = false;
        Step(host, input);
        Check(host.worldObjects[1].posX == 1500, "Automatic teleport requires word 46 strictly above ten.");
        source.doorLockOrGroupBitmask = 11;
        source.groupIndexOrLock = 1;
        Step(host, input);
        Check(host.worldObjects[1].posX == 1500, "Automatic teleport requires a zero word-44 gate.");
        source.groupIndexOrLock = 0;
        source.generalPurpose3 = 0;
        Step(host, input);
        Check(host.worldObjects[1].posX == 2200,
            "Automatic teleport must use word 46, independently of the manual word-40 cooldown.");

        SetUpPortal(host);
        source.velocityX = 2;
        source.blendWeightOrSpriteOpacity = 98;
        host.frameTickCount = 1;
        HandlePlayerObjectInteraction(source, host);
        Check(source.blendWeightOrSpriteOpacity == 100 && source.velocityX == 2 &&
            host.worldObjects[1].posX == 1500, "The opening stage must not finish at exactly 100.");
        HandlePlayerObjectInteraction(source, host);
        Check(source.blendWeightOrSpriteOpacity == 100 && source.velocityX == 0,
            "Opening must clamp and finish only after overshooting 100.");
        HandlePlayerObjectInteraction(source, host);
        Check(host.worldObjects[1].posX == 2200, "Transfer must wait until the opening stage has completed.");

        source.generalPurpose3 = 10;
        host.tickCount = 20;
        Function_Teleporter(source, host);
        Check(source.generalPurpose3 == 11, "Portal cooldown must recharge once per dispatch, not once per tick.");
        source.posX = host.worldObjects[1].posX;
        source.posY = host.worldObjects[1].posY;
        source.posZ = host.worldObjects[1].posZ;
        Function_Teleporter(source, host);
        Check(source.generalPurpose3 == 11, "A portal must not recharge while the player overlaps it.");

        SetUpPortal(host);
        destination.behavior = BehaviorType::None;
        destination.velocityY = 0;
        destination.winY = -1;
        for (auto& row : host.level->floorHeights) { std::fill(std::begin(row), std::end(row), std::uint8_t{10}); }
        for (auto& row : host.level->surfaceFlags) { std::fill(std::begin(row), std::end(row), std::uint8_t{1}); }
        host.level->heightSurfaces[0] = Level::HeightSurface{40,1};
        host.TeleportPlayer(2);
        Check(Eye(host) == 29 && host.GetPlayerHeading() == 0,
            "Terrain-mode destinations need no teleporter behavior and negative headings preserve orientation.");
        host.worldObjects[4] = destination;
        host.worldObjects[4].posX = 2400;
        host.worldObjects[4].velocityY = 1;
        host.worldObjects[4].posY = 300;
        host.worldObjects[4].generalPurpose3 = 20;
        host.worldObjects[5] = host.worldObjects[4];
        host.worldObjects[5].health = 1001;
        host.worldObjects[5].posX = 2600;
        host.worldObjects[142] = host.worldObjects[4];
        host.worldObjects[142].posX = 2800;
        host.TeleportPlayer(2);
        Check(host.worldObjects[1].posX == 2400 && Eye(host) == 300 &&
            destination.generalPurpose3 == 0 && host.worldObjects[4].generalPurpose3 == 0 &&
            host.worldObjects[5].generalPurpose3 == 20 && host.worldObjects[142].generalPurpose3 == 20,
            "Transfer must visit every health-1000 destination in slots 2..141, with the last match winning.");
        host.worldObjects[141] = host.worldObjects[4];
        host.worldObjects[141].posX = 2700;
        host.worldObjects[141].velocityZ = -1;
        host.playerRenderTable = 17;
        host.field_D503 = true;
        host.TeleportPlayer(2);
        Check(host.worldObjects[1].posX == 2700 && host.playerRenderTable == 17 && !host.field_D503,
            "Slot 141 must be included; zero selectors preserve the table and nonpositive velocityZ clears D503.");
        bool rejected = false;
        try { host.TeleportPlayer(12345); }
        catch (const std::runtime_error&) { rejected = true; }
        Check(rejected && host.worldObjects[1].posX == 2700, "Missing destinations must report a data error.");

        SetUpPortal(host);
        input = {};
        Step(host, input);
        input.jump = true;
        Step(host, input);
        input.jump = false;
        input.forward = true;
        Step(host, input);
        HandlePlayerObjectInteraction(source, host);
        Step(host, input);
        if (Eye(host) != 204 || host.worldObjects[1].posX != 2205)
        {
            throw std::runtime_error("Teleport retained-momentum camera differs: eye=" +
                std::to_string(Eye(host)) + ", X=" + std::to_string(host.worldObjects[1].posX));
        }
    }

    void TestLaddersAndMapGates()
    {
        Host host;
        Input input;
        Audio audio;
        host.Load(150);
        host.EnableAudio(audio);
        auto& object = host.worldObjects[2];
        object = Bridge(host, 125);
        object.behavior = BehaviorType::ActivatableLadder;
        object.health = 1001;
        host.tickCount = 4;
        Function_ActivatableLadder(object, host);
        Check(object.generalPurpose2 == 0, "An inactive ladder must wait for health exactly 1000.");
        object.health = 1000;
        Function_ActivatableLadder(object, host);
        Check(object.generalPurpose2 == 4 && object.blendWeightOrSpriteOpacity == 4,
            "An activated ladder must accelerate its opening using word-38*ticks SAR 2.");
        object.blendWeightOrSpriteOpacity = 99;
        Function_ActivatableLadder(object, host);
        Check(object.blendWeightOrSpriteOpacity == 100 && object.generalPurpose2 == 0 &&
            object.collisionMask == 0 && host.playerLadderMovement &&
            host.GetCollisionMapPixelAtWorldXZ(1525, 2500) == 8 &&
            audio.plays.size() == 2 && audio.plays.back().sample == 54,
            "A fully opened ladder must update the collision map and enable nearby vertical control.");
        input.jump = true;
        Step(host, input);
        Check(Eye(host) == 150 && host.playerGrounded,
            "Entering a ladder from midair must first establish supported movement.");
        Step(host, input);
        Check(Eye(host) == 151 && host.playerGrounded,
            "Ladder control must use Sign(vertical speed + posture delta), not jump acceleration.");
        object.health = 0;
        UpdateObjects(host);
        Check(!host.playerLadderMovement, "Each world pass must clear the ladder flag without an active writer.");

        host.Load();
        object = Bridge(host, 20);
        object.behavior = BehaviorType::RoofTopThunderHandler3;
        Function_RoofTopThunderHandler3(object, host);
        Check(!host.playerMapCollisionsEnabled && object.collisionMask == 0,
            "Behavior 184 must disable wall correction while the player overlaps its temporary mask.");
        object.posX += 1000;
        Function_RoofTopThunderHandler3(object, host);
        Check(host.playerMapCollisionsEnabled && object.collisionMask == 0,
            "Leaving behavior 184's volume must restore map collisions.");
        std::fill(std::begin(host.collisionMap->data), std::end(host.collisionMap->data), std::uint8_t{0x18});
        input = {};
        input.forward = true;
        host.playerMapCollisionsEnabled = false;
        Step(host, input);
        Check(host.worldObjects[1].posZ == 2503, "The independent DS:2DDC gate must bypass blocked map pixels.");
        host.playerMapCollisionsEnabled = true;
        host.field_D500 = false;
        Step(host, input);
        Check(host.worldObjects[1].posZ == 2508, "The native body-mode gate must independently bypass map correction.");

        host.Load(100);
        object = Bridge(host, 75);
        object.collisionType = CollisionType::ClimbableLadder;
        SetSlope(*host.level, 128);
        Function_ClimbableLadder(object, host);
        Check(object.blendWeightOrSpriteOpacity == host.tickCount,
            "A live climbable ladder must approach its fully open pose by tick count.");
        input = {};
        Step(host, input);
        Check(!host.playerGrounded && Eye(host) == 99 && host.worldObjects[1].posX == 1500 &&
            host.GetPlayerCamera().lookHeight == 110,
            "Collision 48 must suppress terrain pushing/look without inventing bridge grounding or vertical mode.");
        host.Load();
        host.worldObjects[1].blendWeightOrSpriteOpacity = 12;
        input.forward = true;
        Step(host, input);
        Step(host, input);
        Check(host.worldObjects[1].posZ == 2502 && host.worldObjects[1].blendWeightOrSpriteOpacity == 10,
            "The player word-42 drag counter must reduce forward movement to its sign and decay per substep.");
    }

    void TestOriginalTeleporters(const std::filesystem::path& root)
    {
        for (std::int16_t level : {std::int16_t{10},std::int16_t{23},std::int16_t{36}})
        {
            Host host;
            host.root = root;
            LevelLoadData data;
            host.LoadLevelObjects(level, data);
            host.Load();
            host.worldObjects = data.objects;
            int count = 0;
            for (std::size_t slot = 2; slot <= 141; ++slot)
            {
                const auto& original = data.objects[slot];
                if (original.health != 1000 || original.collisionType != CollisionType::Teleporter) { continue; }
                ++count;
                auto lastMatch = slot;
                for (auto next = slot + 1; next <= 141; ++next)
                {
                    if (data.objects[next].health == 1000 &&
                        data.objects[next].collisionType == CollisionType::Teleporter &&
                        data.objects[next].itemValue == original.itemValue)
                    {
                        lastMatch = next;
                    }
                }
                host.TeleportPlayer(original.itemValue);
                Check(host.worldObjects[1].posX == data.objects[lastMatch].posX &&
                    host.worldObjects[1].posZ == data.objects[lastMatch].posZ &&
                    host.worldObjects[slot].generalPurpose3 == 0,
                    "Original level 10/23/36 destinations must transfer without requiring behavior 120.");
            }
            Check(count > 0, "Expected original teleporter records were not found.");
        }
    }

    void TestOriginalBridges(const std::filesystem::path& root)
    {
        for (std::int16_t level : {std::int16_t{23},std::int16_t{34}})
        {
            Host reader;
            reader.root = root;
            LevelLoadData data;
            reader.LoadLevelObjects(level, data);
            int count = 0;
            for (const auto& original : data.objects)
            {
                if (original.health <= 999 || original.collisionType != CollisionType::WalkablePlanks) { continue; }
                ++count;
                Host host;
                Input input;
                host.Load(static_cast<std::int16_t>(original.posY + 45));
                for (auto& row : host.level->floorHeights) { std::fill(std::begin(row), std::end(row), std::uint8_t{255}); }
                host.worldObjects[1].posX = original.posX;
                host.worldObjects[1].posZ = original.posZ;
                host.worldObjects[2] = original;
                Step(host, input);
                Check(host.playerGrounded == (original.blendWeightOrSpriteOpacity == 0),
                    "Original level 23/34 bridge records must obey the same opening/support gate.");
                if (level == 23)
                {
                    host.SetObjectHealth(host.worldObjects[2], 1000);
                    host.tickCount = 100;
                    Function_WalkableDrawBridge(host.worldObjects[2], host);
                    Step(host, input);
                    Check(host.playerGrounded, "The original drawbridge must become walkable after it opens.");
                }
            }
            Check(count > 0, "Expected original bridge records were not found.");
        }
    }
}

int main(int argc, char** argv)
{
    try
    {
        Check(argc == 3, "Usage: player-support-tests <Original asset directory> <DOS executable>");
        std::ifstream executable(argv[2], std::ios::binary);
        std::array<char, 8> table{};
        executable.seekg(0x31aa6);
        executable.read(table.data(), static_cast<std::streamsize>(table.size()));
        Check(executable.good() && table == std::array<char, 8>{1,10,26,10,26,97,10,105},
            "The terrain footstep table must match initialized DS:0386 in the original executable.");
        TestSlopes();
        TestBridgeContacts();
        TestFootsteps();
        TestBridgeBehaviors();
        TestTeleporters();
        TestLaddersAndMapGates();
        TestOriginalTeleporters(argv[1]);
        TestOriginalBridges(argv[1]);
        std::cout << "Player support, teleport, ladder and footstep tests passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
