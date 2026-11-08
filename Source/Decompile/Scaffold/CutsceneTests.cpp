#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

using namespace Desperabis;

namespace
{
    std::filesystem::path assets;

    void Check(bool condition, const char* message)
    {
        if (!condition) { throw std::runtime_error(message); }
    }

    template<typename Exception, typename Action>
    void ExpectError(Action action)
    {
        bool caught = false;
        try { action(); }
        catch (const Exception&) { caught = true; }
        Check(caught, "Expected an explicit cutscene error.");
    }

    struct Input final : InputHandler
    {
        std::array<bool, static_cast<std::size_t>(InputKey::Count)> keys{};
        int calls = 0;
        bool IsKeyPressed(InputKey key) override { ++calls; return keys.at(static_cast<std::size_t>(key)); }
    };

    struct Host final : GameContext
    {
        bool original = false;
        int flashes = 0, trails = 0, pathReads = 0;
        void OnLightFlash(const LightFlash&) override { ++flashes; }
        void OnObjectCreated(int slot) override
        {
            const auto& object = worldObjects.at(slot);
            const auto& meteor = worldObjects[2];
            if (object.behavior == BehaviorType::SmokeFading && meteor.behavior == BehaviorType::Meteor &&
                object.posX == meteor.posX - meteor.velocityX * 10 &&
                object.posY == meteor.posY - meteor.velocityY * 10 &&
                object.posZ == meteor.posZ - meteor.velocityZ * 10)
            {
                Check(object.velocityY == 0 && object.spriteScale >= 80 && object.spriteScale < 160,
                    "Meteor trail creation must expose the fully initialized particle.");
                ++trails;
            }
        }
    protected:
        std::vector<std::uint8_t> LoadFile(std::string_view name) override
        {
            if (std::filesystem::path(name).extension() == ".KOO")
            {
                ++pathReads;
                throw std::runtime_error("The timed meteor scene must not load a recorded demo.");
            }
            std::ifstream file(assets / name, std::ios::binary);
            if (!file) { throw std::runtime_error("Missing cutscene fixture: " + std::string(name)); }
            return {std::istreambuf_iterator<char>(file), {}};
        }
        void LoadLevel(const LevelLoadRequest& request) override
        {
            LevelLoadData data;
            if (original)
            {
                LoadLevelGeometry(request.levelNumber, data);
                LoadLevelObjects(request.levelNumber, data);
                InitializeLevelPlayerSpawn(data, {0, 0, 100});
            }
            else
            {
                data.level = std::make_unique<Level>(CreateDummyLevel());
                data.playerSpawn = PlayerCamera{{1250, 45, 2950}, {0, 0, 100}};
                if (request.levelNumber == 97) { data.level->nativeInfo[104] = 20; }
            }
            CompleteLevelLoad(request.id, std::move(data));
        }
    };

    void TestTimedCameraAndInput()
    {
        Host host;
        Input input;
        host.StartLevel(97);
        host.selectedWeapon = host.requestedWeapon = WeaponType::Dagger;
        host.hasMap = true;
        const auto inventory = host.inventory;
        const auto initial = host.GetPlayerCamera();
        Check(host.IsCutscenePlaying() && host.worldObjects[1].itemValue == 700 && host.pathReads == 0,
            "The meteor scene must use its 20*35 native timer, not DEMO6's 498 frames.");
        Check(!host.OpenInventory() && !host.GetWeaponView() && host.GetApplicationPresentation().commands.empty(),
            "Cutscenes must suppress inventory, first-person weapons and gameplay overlays.");
        ExpectError<std::logic_error>([&] { host.CaptureSaveGame("cutscene"); });
        host.SetMenuPage(MenuPage::MainMenu);
        Check(!host.SelectMainMenuAction(MainMenuAction::SaveGame) &&
            host.SelectMainMenuAction(MainMenuAction::ResumeGame), "Pausing must remain resumable without enabling saves.");
        input.keys.fill(true);
        host.Tick({10, 0}, input);
        Check(host.GetPlayerCamera().position == initial.position && host.levelElapsedTicks == 0 &&
            host.worldObjects[1].itemValue == 700, "Zero-frame calls must not consume the scene timer.");
        host.Tick({1, 1}, input);
        host.Tick({0, 1}, input);
        Check(host.worldObjects[1].itemValue == 699, "The timer must count behavior ticks, not controller frames.");
        for (int tick = 1; tick < 699; ++tick)
        {
            const auto result = host.Tick({1, 1}, input);
            Check(result.status == GameTickStatus::Advanced &&
                host.GetPlayerCamera().position == initial.position,
                "Neutral timed playback must not translate a grounded camera.");
        }
        Check(host.worldObjects[1].itemValue == 1 && host.IsCutscenePlaying(),
            "The timed scene must remain active until its last native timer tick.");
        Check(input.calls == 0 && std::memcmp(host.inventory.data(), inventory.data(), sizeof(inventory)) == 0 &&
            host.weaponReady, "Real movement, look, interaction, selection and attack input must be ignored.");
        const auto ended = host.Tick({100, 100}, input);
        Check(ended.status == GameTickStatus::LevelEnded && !ended.objects &&
            host.GetLoadedLevelNumber() == 25 && !host.IsCutscenePlaying() && host.levelElapsedTicks == 0,
            "Timer expiry must advance to level 25 without ticking the new level during catch-up.");
        input.keys.fill(false);
        input.calls = 0;
        host.Tick({1, 1}, input);
        Check(input.calls > 0 && host.OpenInventory(), "Ordinary controls must return after the cutscene.");
    }

    void TestClockAndCleanup()
    {
        Input input;
        Host application;
        application.StartLevel(97);
        application.UpdateApplication(0, input);
        input.keys[static_cast<std::size_t>(InputKey::InventoryKey)] = true;
        application.UpdateApplication(1.0 / 66.0, input);
        Check(application.levelElapsedTicks == 0 && application.GetMenuPage() == MenuPage::Gameplay,
            "Sub-tick application frames must not advance playback or open inventory.");
        application.UpdateApplication(1.0 / 66.0, input);
        Check(application.levelElapsedTicks == 1 && application.worldObjects[1].itemValue == 699,
            "Timed cutscenes must use the core's 33 Hz application clock.");
        application.SetPaused(true);
        application.Tick({100, 100}, input);
        Check(application.levelElapsedTicks == 1 && application.worldObjects[1].itemValue == 699,
            "Pause must retain the cutscene timer.");
        application.StartLevel(10);
        Check(!application.IsCutscenePlaying(), "Manual loads must discard timed playback state.");
        ExpectError<std::invalid_argument>([&] { application.Tick({-1, 1}, input); });
    }

    void TestOriginalSceneAndBatching()
    {
        Host single, batch;
        single.original = batch.original = true;
        single.StartLevel(24);
        single.gold = 123;
        single.inventory[4].health = 1000;
        single.inventory[4].collisionType = CollisionType::Medicine;
        single.inventory[4].itemValue = 4;
        single.EndLevel();
        Check(single.GetLoadedLevelNumber() == 97 && single.IsCutscenePlaying(),
            "The original swamp exit must start the original post-swamp scene.");
        batch.StartLevel(97);
        std::array<std::uint8_t, 2000> originalSurfaces, originalFloors;
        std::memcpy(originalSurfaces.data(), single.GetLevel()->surfaceFlags, originalSurfaces.size());
        std::memcpy(originalFloors.data(), single.GetLevel()->floorObjects, originalFloors.size());
        for (int slot = 5; slot <= 34; ++slot)
        {
            const auto& wave = single.worldObjects[slot];
            Check(wave.health == 1000 && wave.behavior == BehaviorType::None &&
                wave.collisionType == CollisionType::Type_138 && wave.doorLockOrGroupBitmask == 138 &&
                wave.posX == 1050 && wave.posZ == 2150 && wave.velocityY == 0,
                "The original fire wave must come from its thirty authored dormant damage triggers.");
        }
        const auto initial = single.GetPlayerCamera();
        Check(initial.position == std::array<std::int16_t, 3>{1750, -111, 3250} &&
            single.GetLevel()->nativeInfo[104] == 20 && single.worldObjects[1].itemValue == 700,
            "The original scene must retain the authored windmill spawn and native duration.");
        const auto checkElevation = [&]
        {
            const auto camera = single.GetPlayerCamera();
            const auto& tracked = single.worldObjects[2];
            const double dx = tracked.posX - camera.position[0], dz = tracked.posZ - camera.position[2];
            const double expected = (tracked.posY - camera.position[1]) / std::hypot(dx, dz);
            Check(camera.elevationSlope && std::abs(*camera.elevationSlope - expected) < 0.00001 &&
                camera.direction == std::array<std::int16_t, 3>{
                    single.worldObjects[1].velocityX, single.worldObjects[1].velocityY, single.worldObjects[1].velocityZ},
                "Perspective meteor elevation must follow its actual height without changing native heading/direction.");
            const double pitch = std::atan(*camera.elevationSlope);
            Check(std::abs(pitch) < 0.4, "The original meteor must remain in a modest elevation, not the 62-degree horizon approximation.");
        };
        checkElevation();
        Input input;
        auto heading = single.GetPlayerHeading();
        auto target = heading;
        for (int tick = 0; tick < 150; ++tick)
        {
            NormalizeAnglePair(target, heading);
            heading = static_cast<std::int16_t>((heading + GetStepToward(heading, target, 6)) & 511);
            single.Tick({1, 1}, input);
            checkElevation();
            const auto& meteor = single.worldObjects[2];
            target = GetVectorAngle(static_cast<std::int16_t>(meteor.posX - initial.position[0]),
                static_cast<std::int16_t>(initial.position[2] - meteor.posZ));
            Check(single.GetPlayerCamera().position == initial.position &&
                single.GetPlayerHeading() == heading,
                "The windmill camera must stand still and turn toward the moving meteor.");
        }
        const auto batchResult = batch.Tick({150, 150}, input);
        Check(batchResult.playerMovement && batchResult.playerMovement->simulatedTicks == 150,
            "Batched playback must report the complete movement tick count.");
        const auto& meteor = single.worldObjects[2];
        Check(meteor.behavior == BehaviorType::Meteor && meteor.posX == 2530 &&
            meteor.posY == 680 && meteor.posZ == 7234 && meteor.collisionRadiusY == 200,
            "The original meteor must follow its native per-tick velocity outside the map.");
        Check(std::memcmp(single.worldObjects.data(), batch.worldObjects.data(), sizeof(single.worldObjects)) == 0 &&
            single.randomSeed == batch.randomSeed && single.GetPlayerHeading() == batch.GetPlayerHeading(),
            "Batched camera ticks must execute every intermediate world update and RNG draw.");
        bool impacted = false, debrisActivated = false;
        std::array<bool, 30> waveActivated{};
        for (int tick = 150; tick < 699; ++tick)
        {
            const auto before = single.worldObjects[2];
            std::array<BehaviorType, 43> debrisBefore;
            for (int index = 0; index < 43; ++index) { debrisBefore[index] = single.worldObjects[index + 35].behavior; }
            single.Tick({1, 1}, input);
            checkElevation();
            for (int index = 0; index < 30; ++index)
            {
                waveActivated[index] = waveActivated[index] ||
                    single.worldObjects[index + 5].behavior == BehaviorType::MeteorFireWave;
            }
            for (int index = 0; index < 43; ++index)
            {
                debrisActivated |= debrisBefore[index] == BehaviorType::DormantMeteorDebris &&
                    single.worldObjects[index + 35].behavior == BehaviorType::FlyingArrow;
            }
            Check(single.GetPlayerCamera().position == initial.position,
                "The authored windmill camera must remain stationary through impact and aftermath.");
            if (!impacted && single.worldObjects[2].behavior == BehaviorType::MeteorImpact)
            {
                impacted = true;
                Check(single.worldObjects[2].displayType == DisplayType::Hidden &&
                    single.worldObjects[2].velocityX == 0 && single.worldObjects[2].velocityY == 0 &&
                    single.worldObjects[2].velocityZ == 0 && before.behavior == BehaviorType::Meteor &&
                    single.flashes == 1 && single.playerRenderTable == 16,
                    "Meteor impact must stop motion, hide the model, select table 16 and emit one light flash.");
            }
        }
        Check(impacted && debrisActivated && single.trails > 200 && single.GetLevel()->trackedTextureFrames[15] > 0 &&
            single.worldObjects[1].itemValue == 1 && single.pathReads == 0,
            "The original impact must activate authored debris and include trail/animated aftermath without KOO access.");
        Check(std::all_of(waveActivated.begin(), waveActivated.end(), [](bool activated) { return activated; }),
            "Impact must activate every authored radial fire particle through the native damage helper.");
        const auto* burntSurfaces = &single.GetLevel()->surfaceFlags[0][0];
        int burntTiles = 0;
        for (std::size_t index = 0; index < originalSurfaces.size(); ++index)
        {
            if (originalSurfaces[index] == burntSurfaces[index]) { continue; }
            Check((originalSurfaces[index] & 31) == 0 && burntSurfaces[index] == 29,
                "The actual cutscene may burn only selector-zero cells, clearing their brightness bits.");
            ++burntTiles;
        }
        Check(burntTiles == 250 &&
            std::memcmp(single.GetLevel()->floorObjects, originalFloors.data(), originalFloors.size()) == 0,
            "The original fixture must burn 250 terrain tiles without replacing floor model geometry.");
        std::cout << "Original meteor: all 30 fire-wave triggers activated; " << burntTiles << " tiles burnt.\n";
        single.Tick({1, 1}, input);
        Check(single.GetLoadedLevelNumber() == 25 && !single.IsCutscenePlaying() &&
            single.gold == 123 && single.inventory[4].health == 1000 && single.inventory[4].itemValue == 4,
            "Automatic scene completion must preserve carried progress and load the next real level.");
    }

    void TestAftermathTimer()
    {
        GameContext context;
        auto level = CreateDummyLevel();
        context.level = &level;
        for (const int period : {0, 1, 10, 255})
        {
            for (const int ticks : {0, 1, 11, 256, 32767})
            {
                for (const int frame : {0, 1, 9, 10})
                {
                    RawGameObject object;
                    object.health = 1000;
                    object.behavior = BehaviorType::MeteorImpact;
                    object.behaviorParameter = static_cast<std::uint8_t>(period);
                    object.doorLockOrGroupBitmask = static_cast<std::int16_t>(frame);
                    context.tickCount = static_cast<std::int16_t>(ticks);
                    level.trackedTextureFrames[15] = 0;
                    int elapsed = ticks, expectedFrame = frame, textureFrame = 0;
                    while (elapsed > period)
                    {
                        elapsed -= period + 1;
                        expectedFrame = expectedFrame == 10 ? 1 : expectedFrame + 1;
                        textureFrame = expectedFrame;
                    }
                    const auto result = DispatchObjectCase(object, context);
                    Check(result.status == DispatchStatus::ExecutedCase && object.generalPurpose1 == elapsed &&
                        object.doorLockOrGroupBitmask == expectedFrame && level.trackedTextureFrames[15] == textureFrame,
                        "Meteor aftermath must preserve the native timer and ten-frame animation wrap.");
                }
            }
        }
    }

    void TestRecoveredFireBehaviors()
    {
        struct FireHost final : GameContext
        {
            int destroyed = 0, smoke = 0;
            void OnObjectDestroyed(int slot) override
            {
                Check(slot == 2, "Blocked fire removal must notify the source slot.");
                ++destroyed;
            }
            void OnObjectCreated(int slot) override
            {
                if (worldObjects[slot].behavior == BehaviorType::SmokeFading) { ++smoke; }
            }
        } context;
        auto level = CreateDummyLevel();
        level.BuildCollisionMap();
        context.level = &level;
        context.collisionMap = level.collisionMap.get();
        auto& object = context.worldObjects[2];
        object.health = 100;
        object.behavior = BehaviorType::DormantMeteorDebris;
        GameContext expected;
        expected.randomSeed = context.randomSeed = 123;
        for (int tick = 0; tick < 200; ++tick)
        {
            context.tickCount = 1;
            const auto health = expected.Random(50) + 70;
            const auto result = UpdateObject(object, context);
            Check(result.status == DispatchStatus::ExecutedCase && object.health == health &&
                object.behavior == BehaviorType::DormantMeteorDebris && context.randomSeed == expected.randomSeed,
                "Dormant debris must consume one RNG draw and refresh lifetime to 70..119.");
        }
        object.collisionType = CollisionType::Type_138;
        object.doorLockOrGroupBitmask = 41;
        context.DamageObject(object, 1);
        Check(object.behavior == BehaviorType::FlyingArrow && object.collisionType == CollisionType::None &&
            object.displayType == DisplayType::SpriteTransparent,
            "Authored debris activation must follow word 46 to behavior 41, not invent behavior 138.");

        const std::array<std::int16_t, 2> matchingPositions[]{ {100, 100}, {5000, 4000}, {1250, 2950} };
        for (const auto [x, z] : matchingPositions)
        {
            const int column = x / 100 - 1, row = 40 - z / 100;
            auto& surface = level.surfaceFlags[row][column];
            surface = 0xe0;
            const auto revision = level.geometryRevision;
            Check(context.ReplaceSurfaceSelector(x, z, 0, 29) && surface == 29 &&
                level.geometryRevision == revision + 1 &&
                !context.ReplaceSurfaceSelector(x, z, 0, 29) &&
                level.geometryRevision == revision + 1,
                "Surface replacement must clear brightness bits, use native cells and be idempotent.");
            surface = 0xe1;
            Check(!context.ReplaceSurfaceSelector(x, z, 0, 29) && surface == 0xe1,
                "Fire must leave unmatched surface selectors intact.");
        }
        const std::array<std::int16_t, 2> outsidePositions[]{
            {99, 100}, {5100, 100}, {100, 99}, {100, 4100}, {-100, 100} };
        for (const auto [x, z] : outsidePositions)
        {
            Check(!context.ReplaceSurfaceSelector(x, z, 0, 29),
                "Out-of-grid surface writes must be rejected without clamping onto edge tiles.");
        }
        ExpectError<std::overflow_error>([&] { context.ReplaceSurfaceSelector(32767, 100, 0, 29); });
        GameContext missing;
        ExpectError<std::logic_error>([&] { missing.ReplaceSurfaceSelector(100, 100, 0, 29); });

        object = {};
        object.health = 1000;
        object.behavior = BehaviorType::MeteorFireWave;
        object.posX = 1235; object.posY = 32760; object.posZ = 2955;
        object.velocityX = 5; object.velocityY = 4; object.velocityZ = -2;
        context.tickCount = 3;
        struct FireAudio final : AudioHandler
        {
            int starts = 0;
            std::uint16_t GetSampleCount() const override { return 129; }
            bool IsChannelPlaying(std::size_t) const override { return false; }
            void PlaySample(std::size_t, std::uint8_t sample, std::uint8_t volume) override
            {
                Check(sample == 22 && volume == 255, "Moving fire must request sound 22 at native full volume.");
                ++starts;
            }
            void StopAllSamples() override {}
        } audio;
        context.audioHandler = &audio;
        std::array<std::uint8_t, SoundEffectTableSize> soundTable{};
        soundTable[0] = 129; soundTable[2] = 5;
        soundTable[4 + 21 * 5] = 1;
        soundTable[5 + 21 * 5] = 22;
        context.LoadSoundEffects(soundTable.data(), soundTable.size());
        level.surfaceFlags[11][11] = 0x80;
        std::array<std::uint8_t, 2000> floorObjects;
        std::memcpy(floorObjects.data(), level.floorObjects, floorObjects.size());
        const auto collisionBefore = *context.collisionMap;
        Check(DispatchObjectCase(object, context).status == DispatchStatus::ExecutedCase &&
            object.posX == 1250 && object.posY == -32764 && object.posZ == 2949 &&
            level.surfaceFlags[11][11] == 29 &&
            std::memcmp(level.floorObjects, floorObjects.data(), floorObjects.size()) == 0 &&
            std::memcmp(&collisionBefore, context.collisionMap, sizeof(collisionBefore)) == 0,
            "Moving fire must wrap velocity*ticks and alter only the current surface byte, not the mesh/map.");
        context.tickCount = 0;
        object.velocityX = object.velocityY = object.velocityZ = 0;
        for (int call = 0; call < 100; ++call) { DispatchObjectCase(object, context); }
        Check(context.smoke > 0 && audio.starts > 0,
            "Moving fire must exercise its smoke creation and sound branches.");
        for (const auto& particle : context.worldObjects)
        {
            if (particle.behavior != BehaviorType::SmokeFading) { continue; }
            const auto smokeFloor = context.GetFloorHeight(object.posX, object.posY, object.posZ - 10);
            Check(particle.posX == object.posX && particle.posY == (std::max)(object.posY, smokeFloor) &&
                particle.posZ == object.posZ - 10 && particle.collisionRadiusXZ == 20 &&
                particle.renderPlacementMode == 130,
                "Fire smoke must expose the supplied coordinates, radius and placement.");
        }
        object.velocityX = 100;
        context.SetCollisionMapPixelAtWorldXZ(1350, object.posZ, 8);
        level.surfaceFlags[11][11] = 0x80;
        DispatchObjectCase(object, context);
        Check(object.health == 0 && context.destroyed == 1 && level.surfaceFlags[11][11] == 0x80,
            "The next-step obstruction must remove fire immediately without burning its current cell.");
    }

    void TestNativeSavePolicy()
    {
        Host host;
        host.original = true;
        host.StartLevel(24);
        auto save = NativeSaveGame::Decode(host.CaptureSaveGame("before scene"));
        const auto order = save.LevelOrder();
        const auto scene = std::find(order.begin(), order.end(), 97);
        Check(scene != order.end(), "Native part order must include the post-swamp scene.");
        save.SetWord(NativeSaveGame::Ordinal, static_cast<std::uint16_t>(scene - order.begin() + 1));
        save.SetChecksum();
        ExpectError<std::runtime_error>([&] { host.LoadGame(save.bytes); });
        Check(host.GetLoadedLevelNumber() == 24 && host.GetRuntimeState() == GameRuntimeState::Ready &&
            !host.GetPendingLevelLoad() && !host.GetPendingSaveGame(),
            "Unsupported native meteor saves must be rejected before changing the current game.");
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc != 2) { throw std::invalid_argument("Expected the original assets directory."); }
        assets = argv[1];
        TestTimedCameraAndInput();
        TestClockAndCleanup();
        TestOriginalSceneAndBatching();
        TestAftermathTimer();
        TestRecoveredFireBehaviors();
        TestNativeSavePolicy();
        std::cout << "Cutscene tests passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
