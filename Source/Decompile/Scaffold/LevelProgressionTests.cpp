#include "BehaviorDispatch.h"

#include <cstring>
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

    template<typename Exception, typename Action>
    void ExpectError(Action action)
    {
        bool caught = false;
        try { action(); }
        catch (const Exception&) { caught = true; }
        Check(caught, "Expected an explicit level-flow error.");
    }

    class Input final : public InputHandler
    {
    public:
        std::array<bool, static_cast<std::size_t>(InputKey::Count)> keys{};
        bool IsKeyPressed(InputKey key) override { return keys.at(static_cast<std::size_t>(key)); }
        void Set(InputKey key, bool down) { keys.at(static_cast<std::size_t>(key)) = down; }
    };

    class EffectsOnlyAudio final : public AudioHandler
    {
    public:
        int stops = 0;
        std::uint16_t GetSampleCount() const override { return 129; }
        bool IsChannelPlaying(std::size_t) const override { return false; }
        void PlaySample(std::size_t, std::uint8_t, std::uint8_t) override {}
        void StopAllSamples() override { ++stops; }
    };

    class Host final : public GameContext
    {
    public:
        std::vector<LevelLoadRequest> requests;
        bool synchronous = true;
        bool fail = false;
        int creations = 0, destructions = 0;
        std::filesystem::path original;
        std::string message;

        static LevelLoadData MakeLevel()
        {
            LevelLoadData data;
            data.level = std::make_unique<Level>(CreateDummyLevel());
            data.objects[1].health = 1000;
            data.objects[1].collisionMask = 1;
            data.objects[1].collisionRadiusXZ = data.objects[1].collisionRadiusY = 10;
            data.objects[2].health = 1000;
            data.objects[2].behavior = BehaviorType::IdleAnimation;
            data.objects[2].animFrame = 5;
            return data;
        }

        void OnObjectCreated(int) override { ++creations; }
        void OnObjectDestroyed(int) override { ++destructions; }
        void PromptUserMessage(std::string_view text, std::int16_t) override { message = text; }
        bool IsUserMessageActive() const override { return !message.empty(); }

    protected:
        void LoadLevel(const LevelLoadRequest& request) override
        {
            Check(GetRuntimeState() == GameRuntimeState::Loading &&
                GetPendingLevelLoad()->id == request.id, "Host must only load a core-authorized request.");
            requests.push_back(request);
            if (fail) { throw std::runtime_error("Fixture asset load failed."); }
            if (!synchronous) { return; }
            if (original.empty()) { CompleteLevelLoad(request.id, MakeLevel()); }
            else
            {
                LevelLoadData data;
                LoadLevelGeometry(request.levelNumber, data);
                LoadLevelObjects(request.levelNumber, data);
                InitializeLevelPlayerSpawn(data, { 0, 0, 100 });
                CompleteLevelLoad(request.id, std::move(data));
            }
        }

        std::vector<std::uint8_t> LoadFile(std::string_view filename) override
        {
            std::ifstream stream(original / std::string(filename), std::ios::binary);
            if (!stream) { throw std::runtime_error("Missing original fixture: " + std::string(filename)); }
            return { std::istreambuf_iterator<char>(stream), {} };
        }
    };

    RawGameObject Exit(std::int16_t lock = 0)
    {
        RawGameObject exit;
        exit.health = 1000;
        exit.collisionType = CollisionType::LevelEnd;
        exit.collisionMask = 1;
        exit.collisionRadiusXZ = exit.collisionRadiusY = 10;
        exit.doorLockOrGroupBitmask = lock;
        return exit;
    }

    void Ready(Host& host, Input& input, std::int16_t level = 10)
    {
        host.basicPlayerMovementEnabled = false;
        host.StartLevel(level);
        host.UpdateApplication(0, input);
    }

    void TestDestinationsAndCompletion()
    {
        constexpr std::array<std::int16_t, 26> sequence{
            50, 30, 31, 32, 33, 36, 34, 35,
            10, 11, 12, 13, 14, 15, 16, 17, 18, 90, 19, 20, 21, 22, 23, 24, 25, 26 };
        for (std::size_t i = 0; i < sequence.size(); ++i)
        {
            Host host;
            Input input;
            EffectsOnlyAudio audio;
            host.audioHandler = &audio;
            Ready(host, input, sequence[i]);
            const int destroyed = host.destructions;
            host.EndLevel();
            if (sequence[i] == 50 || sequence[i] == 35 || sequence[i] == 26)
            {
                Check(host.requests.size() == 1 && host.GetMenuPage() == MenuPage::MainMenu &&
                    host.GetRuntimeState() == GameRuntimeState::NoLevel && !host.CanResumeGame() &&
                    !host.GetLoadedLevelNumber() && !host.GetPendingLevelLoad() && !host.GetLevel() &&
                    !host.collisionMap && host.destructions == destroyed + 2,
                    "Completing a game part must unload through lifecycle hooks and return to a non-resumable menu.");
                for (const auto& object : host.worldObjects)
                {
                    Check(object.health == 0, "Completed parts must leave no live world objects.");
                }
                input.Set(InputKey::UiCancelKey, true);
                host.UpdateApplication(0, input);
                Check(host.GetMenuPage() == MenuPage::MainMenu, "Escape cannot resume a completed part.");
                ExpectError<std::logic_error>([&] { host.EndLevel(); });
            }
            else
            {
                Check(host.requests.size() == 2 && host.requests.back().levelNumber == sequence[i + 1] &&
                    host.requests.back().id > host.requests.front().id &&
                    host.GetLoadedLevelNumber() == sequence[i + 1] &&
                    host.GetRuntimeState() == GameRuntimeState::Ready,
                    "Core must choose the next resource level, including nonnumeric transitions.");
            }
            Check(audio.stops >= 2, "Level transitions must stop the old effect channels.");
        }
        Host unknown;
        Input input;
        Ready(unknown, input, 99);
        ExpectError<std::logic_error>([&] { unknown.EndLevel(); });
        Check(unknown.GetLoadedLevelNumber() == 99 && unknown.requests.size() == 1,
            "Unknown progression must fail without inventing a destination.");
    }

    void TestContactAndTickBoundary()
    {
        Host host;
        Input input;
        Ready(host, input);
        host.ReplaceWorldObject(3, Exit());
        host.randomSeed = 123;
        input.Set(InputKey::AttackKey, true);
        const auto update = host.UpdateApplication(1, input);
        Check(update.tick && update.tick->status == GameTickStatus::LevelEnded &&
            update.tick->interaction->status == InteractionStatus::Applied &&
            !update.tick->objects && !update.tick->weapon && update.worldDeltaSeconds == 0 &&
            update.timing.frameTicks == 0 && host.GetLoadedLevelNumber() == 11 &&
            host.worldObjects[2].animFrame == 5 && host.randomSeed == 123,
            "Exit contact must load once and never simulate or attack in the new world using old-frame ticks.");
        Check(!host.UpdateApplication(1, input).tick, "Load completion must discard catch-up and block held input.");

        Host forced;
        Input noInput;
        Ready(forced, noInput);
        auto finalExit = Exit(42);
        finalExit.groupIndexOrLock = 1000;
        forced.ReplaceWorldObject(3, finalExit);
        Check(forced.Tick({ 0, 0 }, noInput).status == GameTickStatus::LevelEnded &&
            forced.GetMenuPage() == MenuPage::MainMenu && forced.requests.size() == 1,
            "Native field-44 terminal exits must finish the part regardless of lock or tick count.");

        Host locked;
        Input unlockInput;
        Ready(locked, unlockInput);
        locked.ReplaceWorldObject(3, Exit(7));
        Check(locked.Tick({ 0, 0 }, unlockInput).interaction->status == InteractionStatus::NoEffect &&
            locked.GetLoadedLevelNumber() == 10, "A locked exit must not auto-advance.");
        unlockInput.Set(InputKey::InteractionKey, true);
        Check(locked.Tick({ 0, 0 }, unlockInput).interaction->status == InteractionStatus::NoEffect &&
            locked.requests.size() == 1, "Locked exits require a matching key.");
        locked.inventory[0].health = 1000;
        locked.inventory[0].collisionType = CollisionType::Key;
        locked.inventory[0].itemValue = 6;
        Check(locked.Tick({ 0, 0 }, unlockInput).interaction->status == InteractionStatus::NoEffect &&
            locked.inventory[0].health == 1000, "Wrong keys must not unlock or advance.");
        locked.inventory[0].itemValue = 7;
        Check(locked.Tick({ 0, 0 }, unlockInput).interaction->status == InteractionStatus::Applied &&
            locked.worldObjects[3].doorLockOrGroupBitmask == 0 && locked.inventory[0].health == 0 &&
            locked.requests.size() == 1, "Manual exit interaction must unlock before automatic progression.");
        Check(locked.Tick({ 0, 0 }, unlockInput).status == GameTickStatus::LevelEnded &&
            locked.GetLoadedLevelNumber() == 11, "Unlocked exits advance on subsequent contact.");
        auto blocked = Exit(-1);
        Check(locked.HandleLevelEndContact(blocked, false).status == InteractionStatus::NoEffect &&
            locked.HandleLevelEndContact(blocked, true).status == InteractionStatus::NoEffect,
            "Negative exit gates are not ordinary automatic exits.");
        blocked.health = 0;
        Check(locked.HandleLevelEndContact(blocked, false).status == InteractionStatus::InactiveObject,
            "Inactive exits must not advance.");
    }

    void TestInventoryAndAsyncLoading()
    {
        Host host;
        Input input;
        Ready(host, input);
        for (int i = 0; i < 3; ++i)
        {
            host.inventory[i].health = 1000;
            host.inventory[i].collisionType = CollisionType::Weapon;
            host.inventory[i].itemValue = static_cast<std::int16_t>(i + 1);
            host.inventory[i].doorLockOrGroupBitmask = static_cast<std::int16_t>(i == 0 ? 11 : i == 1 ? 10 : -1);
        }
        host.inventory[3].health = 1000;
        host.inventory[3].collisionType = CollisionType::Key;
        host.inventory[3].itemValue = 99;
        host.inventory[4].health = 1000;
        host.inventory[4].collisionType = CollisionType::PickableMap;
        host.inventory[5] = host.inventory[0];
        host.inventory[5].health = 0;
        const auto removed = host.inventory[0];
        const auto kept = host.inventory[1];
        const auto key = host.inventory[3];
        host.hasMap = true;
        host.gold = 123;
        host.playerLife = 200 * 65536;
        host.weaponAmounts[3] = 17;
        host.selectedWeapon = host.requestedWeapon = WeaponType::Crossbow;
        host.weaponAnimationSpeed = 35;
        host.synchronous = false;
        host.EndLevel();
        Check(host.GetRuntimeState() == GameRuntimeState::Loading && host.GetPendingLevelLoad()->levelNumber == 11 &&
            host.GetLoadedLevelNumber() == 10, "Asynchronous loading must keep the old world frozen until completion.");
        auto expected = removed;
        expected.health = 0;
        Check(std::memcmp(&host.inventory[0], &expected, sizeof(expected)) == 0 &&
            std::memcmp(&host.inventory[1], &kept, sizeof(kept)) == 0 &&
            std::memcmp(&host.inventory[3], &key, sizeof(key)) == 0 &&
            host.inventory[2].health == 1000 && host.inventory[4].health == 0 && !host.hasMap &&
            host.selectedWeapon == WeaponType::None && host.requestedWeapon == WeaponType::None &&
            host.weaponAnimationSpeed == 0 && host.weaponInventorySlots[1] == 0 &&
            host.gold == 123 && host.playerLife == 200 * 65536 && host.weaponAmounts[3] == 17,
            "Progression must preserve keys/life/ammo, apply signed weapon/map cleanup, and cancel old attacks.");
        const auto request = *host.GetPendingLevelLoad();
        Check(!host.UpdateApplication(20, input).tick, "A pending next-level load must not advance simulation.");
        ExpectError<std::logic_error>([&] { host.EndLevel(); });
        host.FailLevelLoad(request.id, "Missing next-level asset.");
        Check(host.GetRuntimeState() == GameRuntimeState::LoadFailed && !host.GetPendingLevelLoad() &&
            host.GetLevelLoadError() == "Missing next-level asset." &&
            !host.UpdateApplication(20, input).tick, "Load failure must remain explicit and frozen.");
        ExpectError<std::logic_error>([&] { host.CompleteLevelLoad(request.id, Host::MakeLevel()); });
        host.synchronous = true;
        host.StartLevel(11);
        Check(host.GetLoadedLevelNumber() == 11 && host.GetLevelLoadError().empty(), "Failed transitions can be retried.");

        Host throwing;
        Input otherInput;
        Ready(throwing, otherInput);
        throwing.fail = true;
        ExpectError<std::runtime_error>([&] { throwing.EndLevel(); });
        Check(throwing.GetRuntimeState() == GameRuntimeState::LoadFailed &&
            throwing.GetLoadedLevelNumber() == 10 && throwing.requests.back().levelNumber == 11,
            "Synchronous provider errors must not look like successful level loads.");
        Host inactive;
        Input inactiveInput;
        Ready(inactive, inactiveInput);
        inactive.inventory[0] = removed;
        inactive.inventory[0].health = 0;
        inactive.selectedWeapon = inactive.requestedWeapon = WeaponType::Crossbow;
        inactive.EndLevel();
        Check(inactive.selectedWeapon == WeaponType::None && inactive.requestedWeapon == WeaponType::None,
            "Native transition weapon cleanup must not acquire an invented health gate.");
    }

    void TestCoreNavigationInput()
    {
        Host host;
        Input input;
        Ready(host, input);
        input.Set(InputKey::DebugNextLevelKey, true);
        Check(!host.UpdateApplication(0, input).tick && host.GetLoadedLevelNumber() == 11,
            "Debug level navigation is a core input decision even on zero-tick frames.");
        host.UpdateApplication(0, input);
        Check(host.GetLoadedLevelNumber() == 11, "Held navigation must not skip multiple levels.");
        input.Set(InputKey::DebugNextLevelKey, false);
        input.Set(InputKey::DebugNextGamePartKey, true);
        host.UpdateApplication(0, input);
        Check(host.GetLoadedLevelNumber() == 11, "Changing Shift while Tab is held must not create another press.");
        input.keys.fill(false);
        host.UpdateApplication(0, input);
        ExpectError<std::logic_error>([&] { host.StartNextGamePart(); });
        host.StartLevel(30);
        host.UpdateApplication(0, input);
        input.Set(InputKey::DebugNextGamePartKey, true);
        host.UpdateApplication(0, input);
        Check(host.GetLoadedLevelNumber() == 10, "Core must choose the next game-part entry.");
    }

    void TestOriginalExit(const std::filesystem::path& original)
    {
        Host host;
        Input input;
        host.original = original;
        Ready(host, input);
        Check(host.worldObjects[17].collisionType == CollisionType::LevelEnd &&
            host.worldObjects[17].doorLockOrGroupBitmask == 0, "Expected original level-10 exit at slot 17.");
        const auto exit = host.worldObjects[17];
        for (int slot = 2; slot <= 164; ++slot)
        {
            if (slot != 17) { host.SetObjectHealth(host.worldObjects[slot], 0); }
        }
        auto player = host.worldObjects[1];
        player.posX = exit.posX;
        player.posY = exit.posY;
        player.posZ = exit.posZ;
        host.ReplaceWorldObject(1, player);
        const auto result = host.Tick({ 1, 1 }, input);
        Check(result.status == GameTickStatus::LevelEnded && host.requests.size() == 2 &&
            host.requests.back().levelNumber == 11 && host.GetLoadedLevelNumber() == 11 &&
            host.GetLevel()->geometryWidth > 0 && host.GetPlayerCamera().position !=
                std::array<std::int16_t, 3>{ exit.posX, exit.posY, exit.posZ },
            "Original exit contact must authorize and install actual level-11 geometry, objects and player spawn.");
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc != 2) { throw std::invalid_argument("Expected the original assets directory."); }
        TestDestinationsAndCompletion();
        TestContactAndTickBoundary();
        TestInventoryAndAsyncLoading();
        TestCoreNavigationInput();
        TestOriginalExit(argv[1]);
        std::cout << "Level progression tests passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
