#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
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
        bool original = false, deferred = false;
        int flashes = 0, trails = 0, pathReads = 0;
        std::vector<std::uint8_t> recording;
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
                if (!recording.empty()) { return recording; }
                if (name != "LEVEL\\DEMO7.KOO" && name != "LEVEL\\DEMO0.KOO" && name != "LEVEL\\DEMO6.KOO")
                {
                    throw std::runtime_error("The timed meteor scene must not load a recorded demo.");
                }
            }
            std::ifstream file(assets / name, std::ios::binary);
            if (!file) { throw std::runtime_error("Missing cutscene fixture: " + std::string(name)); }
            return {std::istreambuf_iterator<char>(file), {}};
        }
        void LoadLevel(const LevelLoadRequest& request) override
        {
            if (deferred) { return; }
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

    void TestCreviceMorph()
    {
        for (const auto [phase, ticks] : {
            std::pair<int, int>{0, 1}, {45 * 256, 3}, {100 * 256, 4}, {32700, 3}})
        {
            GameContext context, reference;
            context.soundEnabled = false;
            context.randomSeed = reference.randomSeed = 123;
            context.tickCount = static_cast<std::int16_t>(ticks);
            context.soundListenerX = 1490;
            context.soundListenerZ = 2510;
            auto& player = context.worldObjects[1];
            player.posX = 1490; player.posZ = 2510;
            auto& opening = context.worldObjects[59];
            opening.health = 1000;
            opening.behavior = BehaviorType::CreviceOpening;
            opening.posX = 1500; opening.posZ = 2500;
            opening.itemValue = static_cast<std::int16_t>(phase);
            const auto counter = WrapSignedWord(phase + ticks * 200 + reference.Random(
                static_cast<std::uint16_t>(ticks * 200)));
            const auto blend = (static_cast<std::uint16_t>(counter) >> 8) + reference.RandomSigned(1);
            const auto result = DispatchObjectCase(opening, context);
            Check(result.status == DispatchStatus::ExecutedCase && opening.itemValue == counter &&
                opening.renderPlacementMode == 3 && opening.blendWeightOrSpriteOpacity == blend &&
                context.windX == blend && context.randomSeed == reference.randomSeed &&
                context.playerBodyHeight == 44 && player.posX == 1491 && player.posZ == 2509,
                "The crevice must preserve native fractional morphing, unsigned high-byte extraction, jitter and centering.");
            Check(context.playerPostureDelta == (blend > 45 ? -3 : 0) &&
                opening.behavior == (blend > 100 ? BehaviorType::None : BehaviorType::CreviceOpening),
                "Falling must start above blend 45; the opening controller must stop only above 100.");
            auto& segment = context.worldObjects[56];
            segment.health = 1000;
            segment.behavior = BehaviorType::CreviceSegment;
            Check(DispatchObjectCase(segment, context).status == DispatchStatus::ExecutedCase &&
                segment.blendWeightOrSpriteOpacity == blend,
                "Companion pieces must follow the opening controller's shared morph phase.");
        }
    }

    void TestOriginalCreviceFall()
    {
        Host host;
        host.original = true;
        host.soundEnabled = false;
        host.StartLevel(25);
        auto& trigger = host.worldObjects[59];
        auto& player = host.worldObjects[1];
        Check(trigger.behavior == BehaviorType::CreviceFallTrigger && trigger.health == 1000,
            "The original burning forest must contain its live crevice trigger in slot 59.");
        player.posX = WrapSignedWord(trigger.posX + trigger.collisionRadiusXZ + player.collisionRadiusXZ + 1);
        player.posY = trigger.posY;
        player.posZ = trigger.posZ;
        host.tickCount = 1;
        DispatchObjectCase(trigger, host);
        Check(!host.IsCutscenePlaying() && trigger.behavior == BehaviorType::CreviceFallTrigger,
            "The crevice must not open before the authored contact boundary.");
        player.posX = trigger.posX;
        const auto life = host.playerLife;
        DispatchObjectCase(trigger, host);
        Check(host.IsCutscenePlaying() && trigger.behavior == BehaviorType::CreviceOpening &&
            player.itemValue == 65 && host.playerTargetLookHeight == 10 && !host.OpenInventory() &&
            !host.GetWeaponView(), "Proximity must start the native 65-tick fall, look down and suppress inventory/weapons.");
        for (const int slot : {56, 57, 58, 60, 61, 62})
        {
            Check(host.worldObjects[slot].behavior == BehaviorType::CreviceSegment,
                "All six original companion pieces must use the shared crevice morph.");
        }
        Input input;
        input.keys.fill(true);
        host.Tick({10, 0}, input);
        Check(player.itemValue == 65 && host.windX == 0,
            "A zero-frame call must not advance the crevice countdown or morph.");
        host.Tick({35, 35}, input);
        const auto upperEye = host.GetPlayerCamera().position[1];
        host.Tick({29, 29}, input);
        Check(host.GetLoadedLevelNumber() == 25 && player.itemValue == 1 &&
            trigger.blendWeightOrSpriteOpacity > 45 && host.playerBodyHeight < 28 &&
            host.GetPlayerCamera().position[1] < upperEye && host.playerLife == life &&
            input.calls == 0 && host.pathReads == 0,
            "The opening must lower the live player below normal crouch height, ignore input and remain in level 25 until expiry.");
        const auto ended = host.Tick({10, 10}, input);
        Check(ended.status == GameTickStatus::LevelEnded && !ended.objects &&
            host.GetLoadedLevelNumber() == 26 && !host.IsCutscenePlaying() && host.levelElapsedTicks == 0,
            "Crevice expiry must use normal story/progression to level 26 without ticking it during catch-up.");
    }

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

    void TestCrystalBallEnding()
    {
        Host single, batch;
        single.original = batch.original = true;
        batch.cutsceneSmoothingSeconds = 0;
        single.StartLevel(35);
        single.EndLevel();
        batch.StartLevel(91);
        Check(single.GetLoadedLevelNumber() == 91 && single.IsCutscenePlaying() && single.pathReads == 1,
            "The castle finale must start the level-91 recording, not return directly to the menu.");
        Check(single.worldObjects[6].behavior == BehaviorType::TrackFireAnimation &&
            single.GetLevel()->trackedTextureFrameCounts[6] == 20,
            "The crystal ball must load its twenty authored masks into reserved texture slot 14.");
        Input input;
        input.keys.fill(true);
        single.Tick({0,0}, input);
        single.SetPaused(true);
        single.Tick({10,10}, input);
        Check(single.levelElapsedTicks == 0, "Pause and zero ticks must freeze the recorded ending.");
        single.SetPaused(false);
        bool animated = false, appeared = false;
        std::array<bool, 6> opacities{};
        int previousFrame = 0;
        for (int tick = 0; tick < 1457; ++tick)
        {
            const auto result = single.Tick({1,1}, input);
            Check(result.status == GameTickStatus::Advanced && result.objects.has_value(),
                "Every recorded crystal-ball frame must advance the world.");
            for (const auto& dispatched : *result.objects)
            {
                Check(dispatched.status != DispatchStatus::UnimplementedHandler,
                    "The authored crystal-ball scene must have no missing behavior handlers.");
            }
            const auto frame = single.GetLevel()->trackedTextureFrames[14];
            animated |= previousFrame != 0 && frame != previousFrame;
            previousFrame = frame;
            const auto& skull = single.worldObjects[18];
            if (tick < 1167)
            {
                Check(skull.health == 1001 && skull.blendWeightOrSpriteOpacity == 0,
                    "The skull must remain invisible until the recording uses its switch.");
            }
            if (skull.health == 1000)
            {
                appeared = true;
                Check(skull.blendWeightOrSpriteOpacity >= 0 && skull.blendWeightOrSpriteOpacity <= 5,
                    "Skull transparency must stay in the native 0..5 range.");
                opacities[skull.blendWeightOrSpriteOpacity] = true;
            }
            const auto presented = single.GetObjectRenderState(18);
            Check(presented.posX == 1000 && presented.posZ == 3499 &&
                skull.generalPurpose3 == 13 && skull.doorLockOrGroupBitmask == 21,
                "The skull must follow the table's attachment without corrupting its native phase or links.");
        }
        batch.Tick({1457,1457}, input);
        Check(animated && appeared && std::count(opacities.begin(), opacities.end(), true) > 2 &&
            input.calls == 0 && single.playerTargetLookHeight == 70,
            "Playback must animate the ball, execute recorded look/use cues, pulse the skull and ignore live input.");
        Check(std::memcmp(single.worldObjects.data(), batch.worldObjects.data(), sizeof(single.worldObjects)) == 0 &&
            single.randomSeed == batch.randomSeed && single.GetPlayerHeading() == batch.GetPlayerHeading(),
            "Batched recorded playback must retain every intermediate event, behavior update and RNG draw.");
        single.Tick({100,100}, input);
        Check(!single.IsCutscenePlaying() && !single.GetLoadedLevelNumber() &&
            single.GetMenuPage() == MenuPage::MainMenu && single.GetRuntimeState() == GameRuntimeState::NoLevel,
            "The final recorded frame must finish Part 1 without loading Part 2's epilogue.");
        batch.StartLevel(35);
        Check(!batch.IsCutscenePlaying(), "Manual loads must discard crystal-ball playback.");
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

    void TestMuseumIntro()
    {
        Host single, batch;
        single.original = batch.original = true;
        batch.cutsceneSmoothingSeconds = 0;
        single.StartMuseumIntro();
        batch.StartMuseumIntro();
        Check(single.GetLoadedLevelNumber() == 98 && single.IsMuseumIntroPlaying() &&
            single.IsCutscenePlaying() && !single.CanResumeGame() && !single.OpenInventory() &&
            !single.GetWeaponView() && single.pathReads == 1,
            "Startup must run DEMO0 in museum 98 without creating a resumable campaign.");
        const auto initial = single.GetPlayerCamera();
        std::ifstream pathFile(assets / "LEVEL\\DEMO0.KOO", std::ios::binary);
        const std::vector<std::uint8_t> path{std::istreambuf_iterator<char>(pathFile), {}};
        Check(initial.position[0] == WrapSignedWord(path[3] | (path[4] << 8)) &&
            initial.position[2] == WrapSignedWord(path[5] | (path[6] << 8)) &&
            single.GetPlayerHeading() == path[7] * 2,
            "The initial native camera must match the recording independently of smoothed presentation.");
        ExpectError<std::logic_error>([&] { single.CaptureSaveGame("museum"); });
        Input input;
        input.keys.fill(true);
        std::array<bool, 10> credits{};
        int interactions = 0, neonTransitions = 0;
        std::uint8_t previousAlpha = 0;
        bool fadingIn = false, fadingOut = false;
        for (int tick = 1; tick <= 4938; ++tick)
        {
            const auto lamp = single.worldObjects[33].displayType;
            const auto result = single.Tick({1, 1}, input);
            Check(result.status == GameTickStatus::Advanced && result.objects,
                "The full original museum tour must advance without terminating early.");
            for (const auto& object : *result.objects)
            {
                Check(object.status != DispatchStatus::UnimplementedHandler && object.status != DispatchStatus::Failed,
                    "Every active museum behavior must execute without failures or unrecovered cases.");
            }
            if ((path[3 + (tick - 1) * 6 + 5] & 0x40) != 0)
            {
                Check(result.interaction && result.interaction->status != InteractionStatus::UnimplementedHandler,
                    "Recorded museum uses must route through a recovered interaction.");
                ++interactions;
            }
            neonTransitions += lamp != single.worldObjects[33].displayType;
            const auto& steady = single.worldObjects[42];
            Check(steady.itemValue == 1 && steady.displayType == DisplayType::VectorUnlit,
                "The authored steady neon must never flicker.");
            const auto presentation = single.GetApplicationPresentation();
            if (tick <= 680)
            {
                Check(presentation.commands.empty(),
                    "Credit index zero is a native no-op, not the first image.");
            }
            if (!presentation.commands.empty())
            {
                Check(presentation.commands.size() == 1, "The intro must suppress ordinary gameplay overlays.");
                const auto& image = std::get<UiBitmap>(presentation.commands.front());
                const auto index = image.animation * 5 + image.frame;
                Check(image.source == UiBitmapSource::IntroCredits && index < 10 &&
                    image.bounds.x == 180 && image.bounds.y == 125 && image.layout == UiBitmapLayout::SpriteAnchor,
                    "Credits must retain the original VORSPANN image and native anchor.");
                credits[index] = true;
                fadingIn |= image.color.a > previousAlpha;
                fadingOut |= image.color.a < previousAlpha;
                previousAlpha = image.color.a;
                if (tick == 681) { Check(index == 0 && image.color.a == 0, "First credit starts transparent at tick 681."); }
                if (tick == 697) { Check(index == 0 && image.color.a == 255, "Credit must reach full opacity after 16 ticks."); }
            }
            if (tick == 1031)
            {
                Check(presentation.commands.empty(), "The 350-tick credit must expire before the next image.");
            }
        }
        batch.Tick({4938, 4938}, input);
        Check(std::memcmp(single.worldObjects.data(), batch.worldObjects.data(), sizeof(single.worldObjects)) == 0 &&
            single.randomSeed == batch.randomSeed && single.GetPlayerHeading() == batch.GetPlayerHeading(),
            "Museum batching must preserve every path pose, intermediate behavior and RNG draw.");
        Check(std::all_of(credits.begin(), credits.end(), [](bool value) { return value; }) && fadingIn && fadingOut,
            "Playback must show all ten credits with fade-in and fade-out.");
        Check(neonTransitions > 10, "The original flickering neon must change state throughout the tour.");
        Check(interactions == 3, ("Recorded museum uses with a selected collision: " +
            std::to_string(interactions) + ", expected 3.").c_str());
        Check(input.calls == 0, "Museum playback must ignore real gameplay input.");
        const auto ended = single.Tick({100,100}, input);
        Check(ended.status == GameTickStatus::LevelEnded && !ended.objects &&
            !single.GetLoadedLevelNumber() && !single.GetLevel() && !single.IsMuseumIntroPlaying() &&
            !single.IsCutscenePlaying() && !single.CanResumeGame() &&
            single.GetMenuPage() == MenuPage::MainMenu && single.GetRuntimeState() == GameRuntimeState::NoLevel,
            "The last museum frame must clean up and return to a non-resumable main menu.");
        single.UpdateApplication(0, input);
        single.UpdateApplication(0, input);
        Check(single.GetMenuPage() == MenuPage::MainMenu,
            "Held keys at natural completion must not activate a menu choice.");
        batch.StartLevel(10);
        Check(!batch.IsMuseumIntroPlaying() && !batch.IsCutscenePlaying() &&
            batch.GetApplicationPresentation().commands.empty(),
            "Manual loads must discard museum playback and credit overlays.");
    }

    void TestMuseumInputAndDeferredLoad()
    {
        Host host;
        host.original = true;
        host.StartMuseumIntro();
        Input input;
        host.UpdateApplication(0, input);
        input.keys.fill(true);
        input.keys[static_cast<std::size_t>(InputKey::UiCancelKey)] = false;
        host.UpdateApplication(1.0 / 33, input);
        Check(host.GetLoadedLevelNumber() == 98 && host.GetMenuPage() == MenuPage::Gameplay &&
            host.levelElapsedTicks == 1,
            "F1, inventory, confirm, gameplay and debug navigation must not interrupt the museum.");
        input.keys[static_cast<std::size_t>(InputKey::UiCancelKey)] = true;
        host.UpdateApplication(0, input);
        Check(!host.IsCutscenePlaying() && !host.CanResumeGame() && !host.GetLevel() &&
            host.GetMenuPage() == MenuPage::MainMenu,
            "Only fresh Escape must skip directly to a clean main menu, even on a zero-tick frame.");
        host.UpdateApplication(1, input);
        Check(host.GetMenuPage() == MenuPage::MainMenu, "Held Escape/Enter must not dismiss or activate the menu.");
        input.keys.fill(false);
        host.UpdateApplication(0, input);
        input.keys[static_cast<std::size_t>(InputKey::UiConfirmKey)] = true;
        host.UpdateApplication(0, input);
        Check(host.GetMenuPage() == MenuPage::ChapterSelection,
            "The first menu choice after the intro must offer New Game, not disabled Resume.");

        Host deferred;
        deferred.deferred = true;
        deferred.StartMuseumIntro();
        Check(deferred.IsMuseumIntroPlaying() && !deferred.CanResumeGame() &&
            deferred.GetRuntimeState() == GameRuntimeState::Loading,
            "Deferred startup must remain an intro while its load is pending.");
        input.keys.fill(true);
        deferred.UpdateApplication(0, input);
        Check(deferred.GetMenuPage() == MenuPage::Gameplay && deferred.GetPendingLevelLoad(),
            "Input during loading must neither expose menus nor invalidate a pending host load.");
        const auto request = *deferred.GetPendingLevelLoad();
        LevelLoadData data;
        data.level = std::make_unique<Level>(CreateDummyLevel());
        data.playerSpawn = PlayerCamera{{1250,45,2950},{0,0,100}};
        deferred.CompleteLevelLoad(request.id, std::move(data));
        deferred.UpdateApplication(0, input);
        Check(deferred.IsMuseumIntroPlaying(), "Held loading-screen Escape requires release before skipping.");
        input.keys.fill(false);
        deferred.UpdateApplication(0, input);
        input.keys[static_cast<std::size_t>(InputKey::UiCancelKey)] = true;
        deferred.UpdateApplication(0, input);
        Check(!deferred.GetLoadedLevelNumber() && deferred.GetMenuPage() == MenuPage::MainMenu,
            "A deferred intro must remain skippable after a fresh Escape.");

        Host failed;
        failed.deferred = true;
        failed.StartMuseumIntro();
        failed.FailLevelLoad(failed.GetPendingLevelLoad()->id, "Missing museum");
        Check(!failed.IsMuseumIntroPlaying() && !failed.CanResumeGame() &&
            failed.GetLevelLoadError() == "Missing museum",
            "Intro load failure must be explicit and must not expose a resumable game.");
    }

    void TestMuseumFacing()
    {
        GameContext context;
        auto& object = context.worldObjects[2];
        object.health = 1000;
        object.behavior = BehaviorType::FlipTRexHead;
        object.posX = 1000;
        object.posZ = 1000;
        for (const int yaw : {0, 128, 256, 511})
        {
            object.winY = static_cast<std::int16_t>(yaw);
            bool frames[3]{};
            for (const int x : {500, 1000, 1500})
            {
                for (const int z : {500, 1000, 1500})
                {
                    context.soundListenerX = static_cast<std::int16_t>(x);
                    context.soundListenerZ = static_cast<std::int16_t>(z);
                    const auto view = GetVectorAngle(WrapSignedWord(1000 - x), WrapSignedWord(z - 1000));
                    const auto result = DispatchObjectCase(object, context);
                    Check(result.status == DispatchStatus::ExecutedCase &&
                        object.animFrame == (((yaw + view) & 511) <= 256 ? 2 : 1),
                        "Museum facing must select native frames across the inclusive half-circle boundary.");
                    frames[object.animFrame] = true;
                }
            }
            Check(frames[1] && frames[2], "Both museum viewing sides must be reachable for every authored orientation.");
        }
    }

    std::vector<std::uint8_t> NoisyCameraRecording()
    {
        constexpr int count = 792, size = count * 6;
        std::vector<std::uint8_t> bytes(size + 3);
        bytes[0] = 98;
        bytes[1] = static_cast<std::uint8_t>(size & 255);
        bytes[2] = static_cast<std::uint8_t>(size >> 8);
        for (int frame = 0; frame < count; ++frame)
        {
            const int offset = 3 + frame * 6;
            const int x = 1250 + std::min(frame, 220) * 2 + (frame % 4 < 2 ? 6 : -6);
            const int z = 2950 + (frame % 6 < 3 ? 10 : -10);
            bytes[offset] = static_cast<std::uint8_t>(x & 255);
            bytes[offset + 1] = static_cast<std::uint8_t>(x >> 8);
            bytes[offset + 2] = static_cast<std::uint8_t>(z & 255);
            bytes[offset + 3] = static_cast<std::uint8_t>(z >> 8);
            bytes[offset + 4] = static_cast<std::uint8_t>((250 + frame / 100 + (frame % 4 < 2 ? 3 : -3)) & 255);
            if ((frame >= 180 && frame < 186) || (frame >= 660 && frame < 662)) { bytes[offset + 5] = 0x20; }
            if ((frame >= 360 && frame < 366) || (frame >= 480 && frame < 482)) { bytes[offset + 5] = 0x10; }
        }
        return bytes;
    }

    void TestRecordedCameraSmoothing()
    {
        Host strong, weak, native;
        strong.recording = weak.recording = native.recording = NoisyCameraRecording();
        strong.cutsceneSmoothingSeconds = 1.0;
        weak.cutsceneSmoothingSeconds = 0.15;
        native.cutsceneSmoothingSeconds = 0;
        strong.StartMuseumIntro();
        weak.StartMuseumIntro();
        native.StartMuseumIntro();
        Check(strong.GetRecordedCutsceneCamera() && weak.GetRecordedCutsceneCamera() &&
            !native.GetRecordedCutsceneCamera(), "Smoothing must be enabled by default and disabled with zero.");
        std::array<double, 3> acceleration{};
        std::array<std::array<double, 2>, 3> previous{}, beforePrevious{};
        double previousYaw = 0, previousPitch = 0, peakTurn = 0, peakPitchStep = 0;
        double minimumPitch = 0, maximumPitch = 0;
        double minimumYaw = 10, maximumYaw = 0;
        Input input;
        const int count = static_cast<int>((strong.recording.size() - 3) / 6);
        for (int frame = 0; frame < count - 1; ++frame)
        {
            const auto pose = *strong.GetRecordedCutsceneCamera();
            const auto weakPose = *weak.GetRecordedCutsceneCamera();
            const int offset = 3 + frame * 6;
            const std::array<std::array<double, 2>, 3> positions{{
                {pose.position[0], pose.position[2]}, {weakPose.position[0], weakPose.position[2]},
                {static_cast<double>(strong.recording[offset] | (strong.recording[offset + 1] << 8)),
                    static_cast<double>(strong.recording[offset + 2] | (strong.recording[offset + 3] << 8))}}};
            for (std::size_t track = 0; track < positions.size(); ++track)
            {
                if (frame > 1)
                {
                    acceleration[track] = std::max(acceleration[track], std::hypot(
                        positions[track][0] - 2 * previous[track][0] + beforePrevious[track][0],
                        positions[track][1] - 2 * previous[track][1] + beforePrevious[track][1]));
                }
                beforePrevious[track] = previous[track];
                previous[track] = positions[track];
            }
            if (frame > 0)
            {
                peakTurn = std::max(peakTurn, std::abs(pose.yaw - previousYaw));
                peakPitchStep = std::max(peakPitchStep, std::abs(pose.pitch - previousPitch));
            }
            previousYaw = pose.yaw;
            previousPitch = pose.pitch;
            minimumPitch = std::min(minimumPitch, pose.pitch);
            maximumPitch = std::max(maximumPitch, pose.pitch);
            minimumYaw = std::min(minimumYaw, pose.yaw);
            maximumYaw = std::max(maximumYaw, pose.yaw);
            strong.Tick({1,1}, input);
            weak.Tick({1,1}, input);
            native.Tick({1,1}, input);
            Check(std::memcmp(strong.worldObjects.data(), native.worldObjects.data(), sizeof(strong.worldObjects)) == 0 &&
                strong.randomSeed == native.randomSeed,
                "Camera smoothing must not alter native collision, simulation, pitch controls or RNG.");
        }
        std::cout << "Recorded camera peak acceleration: raw " << acceleration[2] << ", weak " << acceleration[1]
            << ", strong " << acceleration[0] << "; peak yaw/pitch steps " << peakTurn << '/' << peakPitchStep
            << "; pitch range " << minimumPitch << ".." << maximumPitch << ", ending " << previousPitch << ".\n";
        Check(acceleration[0] < acceleration[2] * 0.05 && acceleration[0] < acceleration[1] * 0.5,
            "Strong smoothing must remove at least 95 percent of bounce acceleration and exceed the weak setting.");
        Check(peakTurn < 0.005 && minimumYaw < 6.283185307 && maximumYaw > 6.283185307 &&
            peakPitchStep < 0.01 && minimumPitch < -0.15 && maximumPitch > 0.5 && std::abs(previousPitch) < 0.01,
            "Unwrapped yaw must cross zero without a spin; pitch must smoothly look both ways and return to neutral.");
        strong.cutsceneSmoothingSeconds = 0;
        Check(strong.GetRecordedCutsceneCamera().has_value(), "Settings changes must wait until the next KOO load.");
        strong.StartMuseumIntro();
        Check(!strong.GetRecordedCutsceneCamera(), "Reloading with zero must restore native camera presentation.");
        weak.Tick({1,1}, input);
        Check(!weak.GetRecordedCutsceneCamera(), "Finishing a recording must discard its camera curve.");
        for (const double invalid : {-1.0, std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::quiet_NaN()})
        {
            Host host;
            host.recording = NoisyCameraRecording();
            host.cutsceneSmoothingSeconds = invalid;
            ExpectError<std::invalid_argument>([&] { host.StartMuseumIntro(); });
        }
    }

    void TestRecordedCameraClock()
    {
        Host single, split;
        single.recording = split.recording = NoisyCameraRecording();
        single.StartMuseumIntro();
        split.StartMuseumIntro();
        Input input;
        single.UpdateApplication(0, input);
        split.UpdateApplication(0, input);
        single.UpdateApplication(0.5, input);
        for (int frame = 0; frame < 60; ++frame) { split.UpdateApplication(1.0 / 120, input); }
        const auto once = *single.GetRecordedCutsceneCamera(), many = *split.GetRecordedCutsceneCamera();
        Check(std::abs(once.position[0] - many.position[0]) < 1e-9 &&
            std::abs(once.position[1] - many.position[1]) < 1e-9 &&
            std::abs(once.position[2] - many.position[2]) < 1e-9 &&
            std::abs(once.yaw - many.yaw) < 1e-12 && std::abs(once.pitch - many.pitch) < 1e-12,
            "Camera sampling must depend on elapsed time, not host frame rate or simulation batching.");
        single.UpdateApplication(1, input);
        const auto before = *single.GetRecordedCutsceneCamera();
        const auto tick = single.levelElapsedTicks;
        single.UpdateApplication(0.1 / 33, input);
        const auto after = *single.GetRecordedCutsceneCamera();
        Check(single.levelElapsedTicks == tick && std::abs(after.position[0] - before.position[0]) > 0.01 &&
            std::abs(after.position[0] - before.position[0]) < 1,
            "Sub-tick rendering must move continuously with sub-DOS-unit precision, not jump at 33 Hz.");
        single.SetPaused(true);
        single.UpdateApplication(1, input);
        Check(single.GetRecordedCutsceneCamera()->position == after.position,
            "Pausing must freeze even a fractional camera pose.");
        single.SetPaused(false);
        single.UpdateApplication(0, input);
        Check(single.GetRecordedCutsceneCamera()->position == after.position,
            "Resuming must not snap back to an integer simulation tick.");
        single.StartLevel(10);
        Check(!single.GetRecordedCutsceneCamera(), "Manual level loads must not retain an old camera curve.");

        Host endpoint;
        endpoint.recording = NoisyCameraRecording();
        endpoint.StartMuseumIntro();
        endpoint.UpdateApplication(0, input);
        const auto start = endpoint.GetRecordedCutsceneCamera()->position[0];
        endpoint.UpdateApplication(0.0001, input);
        const auto first = endpoint.GetRecordedCutsceneCamera()->position[0];
        endpoint.UpdateApplication(0.0001, input);
        const auto second = endpoint.GetRecordedCutsceneCamera()->position[0];
        Check(std::abs(first - start) < 1e-5 && std::abs(second - 2 * first + start) < 1e-5,
            "Starting playback must ease in with approximately zero velocity and acceleration.");

        Host ending;
        ending.original = true;
        ending.StartLevel(96);
        Check(ending.GetRecordedCutsceneCamera().has_value(), "DEMO6 must use the shared smoothing path too.");
        Host crystal;
        crystal.original = true;
        crystal.StartLevel(91);
        Check(crystal.GetRecordedCutsceneCamera().has_value(), "DEMO7 must use the shared smoothing path too.");
    }

    void TestRecordedCameraHeight()
    {
        Host smooth, native;
        auto recording = NoisyCameraRecording();
        const int count = static_cast<int>((recording.size() - 3) / 6);
        for (int frame = 0; frame < count; ++frame)
        {
            const int offset = 3 + frame * 6, x = 1250 + frame * 2;
            recording[offset] = static_cast<std::uint8_t>(x & 255);
            recording[offset + 1] = static_cast<std::uint8_t>(x >> 8);
            recording[offset + 4] = recording[offset + 5] = 0;
        }
        const auto load = [&](Host& host)
        {
            host.deferred = true;
            host.recording = recording;
            host.StartMuseumIntro();
            LevelLoadData data;
            data.level = std::make_unique<Level>(CreateDummyLevel());
            for (auto& row : data.level->floorHeights)
            {
                for (std::size_t column = 0; column < std::size(row); ++column)
                {
                    row[column] = static_cast<std::uint8_t>(column * 4);
                }
            }
            GameContext terrain;
            terrain.level = data.level.get();
            const auto eyeY = WrapSignedWord(terrain.GetTerrainFloorHeight(1250, 45, 2950) + 45);
            data.playerSpawn = PlayerCamera{{1250, eyeY, 2950}, {0,0,100}};
            host.CompleteLevelLoad(host.GetPendingLevelLoad()->id, std::move(data));
        };
        native.cutsceneSmoothingSeconds = 0;
        load(smooth);
        load(native);
        const auto initial = *smooth.GetRecordedCutsceneCamera();
        smooth.worldObjects[1].posY += 32;
        Check(smooth.GetRecordedCutsceneCamera()->position == initial.position,
            "The prepared camera height must not read the live quantized player Y.");
        smooth.worldObjects[1].posY -= 32;
        std::array<double, 2> previous{}, beforePrevious{}, acceleration{}, step{};
        int fractional = 0;
        Input input;
        for (int frame = 0; frame < count - 1; ++frame)
        {
            const auto pose = *smooth.GetRecordedCutsceneCamera();
            const std::array<double, 2> heights{pose.position[1],
                static_cast<double>(smooth.GetTerrainFloorHeight(
                    static_cast<std::int16_t>(1250 + frame * 2), 45, 2950) + 45)};
            fractional += std::abs(pose.position[1] - std::round(pose.position[1])) > 0.01;
            for (std::size_t track = 0; track < heights.size(); ++track)
            {
                if (frame > 0) { step[track] = std::max(step[track], std::abs(heights[track] - previous[track])); }
                if (frame > 1)
                {
                    acceleration[track] = std::max(acceleration[track],
                        std::abs(heights[track] - 2 * previous[track] + beforePrevious[track]));
                }
                beforePrevious[track] = previous[track];
                previous[track] = heights[track];
            }
            smooth.Tick({1,1}, input);
            native.Tick({1,1}, input);
            Check(std::memcmp(smooth.worldObjects.data(), native.worldObjects.data(), sizeof(smooth.worldObjects)) == 0 &&
                smooth.randomSeed == native.randomSeed,
                "Terrain-height smoothing must not change native support, collisions or simulation.");
        }
        Check(step[1] >= 2 && step[0] < 1 && acceleration[0] < acceleration[1] * 0.1 && fractional > count / 2,
            "The Y curve must remove terrain steps with fractional heights and at least 90 percent less acceleration.");
        Check(previous[0] < initial.position[1] - 100,
            "Height smoothing must preserve the traversed slope and native standing eye offset.");
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
        TestCreviceMorph();
        TestOriginalCreviceFall();
        TestTimedCameraAndInput();
        TestClockAndCleanup();
        TestOriginalSceneAndBatching();
        TestCrystalBallEnding();
        TestMuseumIntro();
        TestMuseumInputAndDeferredLoad();
        TestMuseumFacing();
        TestRecordedCameraSmoothing();
        TestRecordedCameraClock();
        TestRecordedCameraHeight();
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
