#include "BehaviorDispatch.h"

#include <algorithm>
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

    std::vector<std::uint8_t> Read(std::string_view name)
    {
        std::ifstream file(assets / name, std::ios::binary);
        if (!file) { throw std::runtime_error("Missing cutscene fixture: " + std::string(name)); }
        return {std::istreambuf_iterator<char>(file), {}};
    }

    int Word(const std::vector<std::uint8_t>& bytes, std::size_t offset)
    {
        const int value = bytes.at(offset) + 256 * bytes.at(offset + 1);
        return value >= 32768 ? value - 65536 : value;
    }

    std::vector<std::uint8_t> Path(std::size_t frames, std::uint8_t actions = 0)
    {
        const auto count = frames * 6;
        std::vector<std::uint8_t> bytes{96, static_cast<std::uint8_t>(count),
            static_cast<std::uint8_t>(count >> 8)};
        for (std::size_t i = 0; i < frames; ++i)
        {
            bytes.insert(bytes.end(), {0xe2, 4, 0x86, 0x0b, 0, actions});
        }
        return bytes;
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
        std::optional<std::vector<std::uint8_t>> path;
        int flashes = 0, trails = 0;
        void OnLightFlash(const LightFlash&) override { ++flashes; }
        void OnObjectCreated(int slot) override
        {
            const auto& object = worldObjects.at(slot);
            if (object.behavior == BehaviorType::SmokeFading)
            {
                Check(object.velocityY == 0 && object.spriteScale >= 80 && object.spriteScale < 160,
                    "Meteor trail creation must expose the fully initialized particle.");
                ++trails;
            }
        }
    protected:
        std::vector<std::uint8_t> LoadFile(std::string_view name) override
        {
            if (name == "LEVEL\\DEMO6.KOO" && path) { return *path; }
            return Read(name);
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
                data.playerSpawn = PlayerCamera{{1250, 100, 2950}, {0, 0, 100}};
            }
            CompleteLevelLoad(request.id, std::move(data));
        }
    };

    void Equip(Host& host)
    {
        host.selectedWeapon = host.requestedWeapon = WeaponType::Dagger;
        host.weaponHand = host.requestedWeaponHand = 2;
        host.weaponAnimationSpeed = 0;
        host.weaponAnimationPosition = 10;
        host.weaponReady = true;
        auto& item = host.inventory[1];
        item.health = 1000;
        item.collisionType = CollisionType::Weapon;
        item.itemValue = static_cast<std::int16_t>(WeaponType::Dagger);
        host.weaponAmounts[1] = 100;
        host.RefreshWeaponInventory();
    }

    void TestValidation()
    {
        const auto original = Read("LEVEL\\DEMO6.KOO");
        Check(original.size() == 2991 && original[0] == 96 && Word(original, 1) == 2988,
            "The original scene must have 498 frames and zero-based level index 96.");
        std::vector<std::vector<std::uint8_t>> invalid{{}, {96, 0, 0}, {96, 6, 0}};
        auto bad = Path(1);
        bad[0] = 97;
        invalid.push_back(bad);
        bad = Path(1); bad[1] = 12; invalid.push_back(bad);
        bad = Path(1); bad.pop_back(); invalid.push_back(bad);
        bad = Path(1); bad.push_back(0); bad[1] = 7; invalid.push_back(bad);
        invalid.push_back(Path(10667));
        for (const auto& bytes : invalid)
        {
            Host host;
            host.path = bytes;
            ExpectError<std::invalid_argument>([&] { host.StartLevel(97); });
            Check(host.GetRuntimeState() == GameRuntimeState::LoadFailed &&
                !host.IsCutscenePlaying() && !host.GetLevelLoadError().empty(),
                "Malformed paths must fail the load explicitly, without active playback.");
        }
        Host host;
        host.StartLevel(11);
        Check(!host.IsCutscenePlaying(), "Playable levels visited by DEMO files must remain interactive.");
    }

    void TestEveryRecordedPoseAndInput()
    {
        Host host;
        Input input;
        host.StartLevel(97);
        Equip(host);
        host.hasMap = true;
        const auto bytes = Read("LEVEL\\DEMO6.KOO");
        const auto inventory = host.inventory;
        Check(host.GetPlayerCamera().position[0] == 1250 && host.GetPlayerCamera().position[2] == 2950,
            "The initial recorded pose must be visible before the first tick.");
        Check(!host.OpenInventory() && !host.GetWeaponView() && host.GetApplicationPresentation().commands.empty(),
            "Cutscenes must suppress inventory, first-person weapons and gameplay overlays.");
        ExpectError<std::logic_error>([&] { host.CaptureSaveGame("cutscene"); });
        host.SetMenuPage(MenuPage::MainMenu);
        Check(!host.SelectMainMenuAction(MainMenuAction::SaveGame) &&
            host.SelectMainMenuAction(MainMenuAction::ResumeGame), "Pausing must remain resumable without enabling saves.");
        input.keys.fill(true);
        const auto initial = host.GetPlayerCamera();
        host.Tick({10, 0}, input);
        Check(host.GetPlayerCamera().position == initial.position && host.levelElapsedTicks == 0,
            "Zero-frame calls must not consume frames or actions.");
        for (std::size_t i = 0; i < 498; ++i)
        {
            const auto result = host.Tick({1, 1}, input);
            const auto camera = host.GetPlayerCamera();
            Check(result.status == GameTickStatus::Advanced && camera.position[0] == Word(bytes, 3 + i * 6) &&
                camera.position[2] == Word(bytes, 5 + i * 6), "Every recorded X/Z frame must be applied in order.");
        }
        Check(input.calls == 0 && std::memcmp(host.inventory.data(), inventory.data(), sizeof(inventory)) == 0 &&
            host.weaponReady, "Real movement, look, interaction, selection and attack input must be ignored.");
        Check(host.GetPlayerCamera().position[0] == 1235 && host.GetPlayerCamera().position[2] == 3135 &&
            host.levelElapsedTicks == 498, "The complete last frame must remain visible before EOF.");
        Check(host.Tick({1, 1}, input).status == GameTickStatus::LevelEnded &&
            host.GetLoadedLevelNumber() == 25 && !host.IsCutscenePlaying(),
            "Path exhaustion must advance through the core level order to level 25.");
        input.keys.fill(false);
        input.calls = 0;
        host.Tick({1, 1}, input);
        Check(input.calls > 0 && host.OpenInventory(), "Ordinary controls must return after the cutscene.");
    }

    void TestActionsAndClock()
    {
        Input input;
        Host look;
        look.path = Path(40, 0x20);
        for (std::size_t i = 20; i < 40; ++i) { look.path->at(8 + i * 6) = 0x10; }
        look.StartLevel(97);
        look.Tick({20, 20}, input);
        Check(look.playerTargetLookHeight == 50, "Look-down actions must saturate at the native lower target.");
        look.Tick({20, 20}, input);
        Check(look.playerTargetLookHeight == 130, "Scripted look-up uses 130, not the normal keyboard limit.");

        Host rotate;
        rotate.path = Path(3);
        rotate.path->at(14) = 15; // Ignored low action bits.
        rotate.path->at(13) = 64;
        rotate.StartLevel(97);
        rotate.Tick({2, 2}, input);
        Check(rotate.GetPlayerHeading() == 0, "The next target heading is applied after simulation.");
        rotate.Tick({1, 1}, input);
        Check(rotate.GetPlayerHeading() == 2, "Scripted heading must approach angle*2 at two units per frame.");

        Host attack;
        attack.path = Path(2, 0x80);
        attack.StartLevel(97);
        Equip(attack);
        const auto result = attack.Tick({1, 1}, input);
        Check(result.weapon && result.weapon->attack == WeaponActionStatus::Applied &&
            attack.weaponAnimationSpeed == -3 && attack.worldObjects[2].behavior == BehaviorType::WeaponDamageCollider,
            "The recorded attack bit must drive ordinary weapon gameplay despite real-input suppression.");

        Host interact;
        interact.path = Path(2, 0x40);
        interact.StartLevel(97);
        auto& key = interact.worldObjects[2];
        key.health = 1000;
        key.collisionType = CollisionType::Key;
        key.pickupType = PickupType::Type_1;
        key.collisionMask = 1;
        key.collisionRadiusXZ = key.collisionRadiusY = 100;
        key.posX = 1250; key.posZ = 2950; key.posY = interact.worldObjects[1].posY;
        key.itemValue = 3;
        Check(interact.Tick({1, 1}, input).interaction->status == InteractionStatus::Applied && key.health == 0,
            "The recorded interaction bit must use the ordinary selected-object handler.");

        Host sentinel;
        sentinel.path = Path(2);
        sentinel.path->at(9) = sentinel.path->at(10) = 255;
        sentinel.StartLevel(97);
        sentinel.Tick({1, 1}, input);
        Check(sentinel.Tick({1, 1}, input).status == GameTickStatus::LevelEnded &&
            sentinel.GetLoadedLevelNumber() == 25, "Either -1 coordinate must terminate a path safely.");
        Host zSentinel;
        zSentinel.path = Path(1);
        zSentinel.path->at(5) = zSentinel.path->at(6) = 255;
        zSentinel.StartLevel(97);
        Check(zSentinel.Tick({1, 1}, input).status == GameTickStatus::LevelEnded,
            "The Z-coordinate sentinel must also terminate safely.");
        Host catchup;
        catchup.path = Path(2);
        catchup.StartLevel(97);
        const auto ended = catchup.Tick({100, 100}, input);
        Check(ended.status == GameTickStatus::LevelEnded && !ended.objects &&
            catchup.GetLoadedLevelNumber() == 25 && catchup.levelElapsedTicks == 0,
            "Catch-up must stop at scene completion, without returning stale objects or ticking the next level.");

        Host application;
        application.StartLevel(97);
        application.UpdateApplication(0, input);
        input.keys[static_cast<std::size_t>(InputKey::InventoryKey)] = true;
        application.UpdateApplication(1.0 / 66.0, input);
        Check(application.levelElapsedTicks == 0 && application.GetMenuPage() == MenuPage::Gameplay,
            "Sub-tick application frames must not advance playback or open inventory.");
        application.UpdateApplication(1.0 / 66.0, input);
        Check(application.levelElapsedTicks == 1, "Cutscenes must use the core's 33 Hz application clock.");
        application.SetPaused(true);
        application.Tick({100, 100}, input);
        Check(application.levelElapsedTicks == 1, "Pause must retain the cutscene cursor.");
        application.StartLevel(10);
        Check(!application.IsCutscenePlaying(), "Manual loads must discard playback state.");
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
            "The original swamp exit must start the original post-swamp camera scene.");
        batch.StartLevel(97);
        Input input;
        for (int tick = 0; tick < 150; ++tick) { single.Tick({1, 1}, input); }
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
        bool impacted = false;
        for (int tick = 150; tick < 498; ++tick)
        {
            const auto before = single.worldObjects[2];
            single.Tick({1, 1}, input);
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
        Check(impacted && single.trails > 200 && single.GetLevel()->trackedTextureFrames[15] > 0,
            "The original scene must include a visible trail and the native animated aftermath.");
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
                        "Meteor aftermath must preserve the native strict timer comparison and ten-frame animation wrap.");
                }
            }

        }
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
            "A native cutscene save without a cursor must be rejected before changing the current game.");
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc != 2) { throw std::invalid_argument("Expected the original assets directory."); }
        assets = argv[1];
        TestValidation();
        TestEveryRecordedPoseAndInput();
        TestActionsAndClock();
        TestOriginalSceneAndBatching();
        TestAftermathTimer();
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
