#include "GameContext.h"
#include "BehaviorDispatch.h"
#include "GameMath.h"
#include "GameCoreHost.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <map>
#include <regex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
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
        Check(caught, "Expected an explicit runtime integration error.");
    }

    LevelLoadData MakeLevel()
    {
        LevelLoadData data;
        data.level = std::make_unique<Level>(CreateDummyLevel());
        data.objects[1].health = 1000;
        data.objects[1].field_07 = 1;
        data.objects[1].collisionRadiusXZ = 10;
        data.objects[1].collisionRadiusY = 10;
        return data;
    }

    class Host final : public GameContext
    {
    public:
        std::vector<LevelLoadRequest> requests;
        bool synchronous = false;
        bool fail = false;

    protected:
        void LoadLevel(const LevelLoadRequest& request) override
        {
            requests.push_back(request);
            if (fail) { throw std::runtime_error("Host asset loading failed."); }
            if (synchronous) { CompleteLevelLoad(request.id, MakeLevel()); }
        }
    };

    class Input final : public InputHandler
    {
    public:
        bool pressed = false;
        int calls = 0;
        std::array<bool, 6> movement{};
        int movementCalls = 0;
        bool IsKeyPressed(InputKey key) override
        {
            if (key != InputKey::InteractionKey)
            {
                ++movementCalls;
                return movement.at(static_cast<std::size_t>(key));
            }
            ++calls;
            return pressed;
        }
    };

    void TestLoading()
    {
        static_assert(!std::is_copy_constructible_v<GameContext>);
        static_assert(std::is_nothrow_move_constructible_v<GameContext>);
        Host host;
        Input input;
        Check(host.GetRuntimeState() == GameRuntimeState::NoLevel && !host.GetLevel() &&
            !host.GetPendingLevelLoad() && !host.GetLoadedLevelNumber() &&
            host.Tick({}, input).status == GameTickStatus::NoLevel,
            "A host context must start idle without calling modal pages.");
        ExpectError<std::invalid_argument>([&] { host.StartLevel(0); });
        const auto request = host.StartLevel(12);
        Check(host.requests.size() == 1 && request.id != 0 && request.levelNumber == 12 &&
            host.GetPendingLevelLoad()->id == request.id &&
            host.Tick({}, input).status == GameTickStatus::Loading && input.calls == 0,
            "Starting a level must invoke the host once and suspend simulation.");
        ExpectError<std::logic_error>([&] { host.StartLevel(13); });
        ExpectError<std::logic_error>([&] { host.CompleteLevelLoad(request.id + 1, MakeLevel()); });
        ExpectError<std::invalid_argument>([&] { host.CompleteLevelLoad(request.id, {}); });
        auto missingMap = MakeLevel();
        missingMap.level->collisionMap.reset();
        ExpectError<std::invalid_argument>([&] { host.CompleteLevelLoad(request.id, std::move(missingMap)); });
        auto overflow = MakeLevel();
        overflow.objects[1].posX = 32767;
        ExpectError<std::overflow_error>([&] { host.CompleteLevelLoad(request.id, std::move(overflow)); });
        Check(!host.GetLevel() && host.GetRuntimeState() == GameRuntimeState::Loading,
            "Rejected level data must not partially install a world.");
        auto data = MakeLevel();
        const auto* owned = data.level.get();
        data.objects[2].health = 1000;
        data.objects[2].behavior = BehaviorType::PulsatingLight;
        data.floorRepeatHeight = 25;
        host.gold = 42;
        host.randomSeed = 123;
        host.userMessage = "old level";
        host.userMessageTicks = 10;
        host.switchGroupMasks[2] = 0;
        host.CompleteLevelLoad(request.id, std::move(data));
        Check(host.GetRuntimeState() == GameRuntimeState::Ready && host.GetLevel() == owned &&
            host.level == owned && host.collisionMap == owned->collisionMap.get() &&
            host.GetLoadedLevelNumber() == 12 && !host.GetPendingLevelLoad() &&
            host.GetMenuPage() == MenuPage::Gameplay && host.hasActiveGame &&
            host.GetWorldObjects()[2].behavior == BehaviorType::PulsatingLight &&
            host.floorRepeatHeight == 25 && host.gold == 42 && host.randomSeed == 123 &&
            host.userMessage.empty() && host.userMessageTicks == 0 && host.switchGroupMasks[2] == 0xff,
            "Completion must transfer ownership, install per-level state, and preserve session inventory/RNG.");
        ExpectError<std::logic_error>([&] { host.CompleteLevelLoad(request.id, MakeLevel()); });

        const auto next = host.StartLevel(13);
        Check(next.id > request.id && host.GetLevel() == owned &&
            host.Tick({}, input).status == GameTickStatus::Loading,
            "A transition must retain the previous world for display but suspend gameplay.");
        ExpectError<std::invalid_argument>([&] { host.FailLevelLoad(next.id, ""); });
        ExpectError<std::logic_error>([&] { host.FailLevelLoad(request.id, "stale"); });
        host.FailLevelLoad(next.id, "Missing level asset");
        Check(host.GetRuntimeState() == GameRuntimeState::LoadFailed &&
            host.GetLevelLoadError() == "Missing level asset" && host.GetLevel() == owned &&
            host.Tick({}, input).status == GameTickStatus::LoadFailed,
            "Async failures must be visible and must not resume the previous simulation silently.");
        const auto retry = host.StartLevel(13);
        ExpectError<std::logic_error>([&] { host.CompleteLevelLoad(next.id, MakeLevel()); });
        host.CompleteLevelLoad(retry.id, MakeLevel());
        Check(host.GetLoadedLevelNumber() == 13 && host.GetLevelLoadError().empty(),
            "Retries require a fresh request ID and clear an acknowledged error.");

        Host synchronous;
        synchronous.synchronous = true;
        synchronous.StartLevel(10);
        Check(synchronous.GetRuntimeState() == GameRuntimeState::Ready,
            "Host loading may complete synchronously.");
        Host throwing;
        throwing.fail = true;
        ExpectError<std::runtime_error>([&] { throwing.StartLevel(10); });
        Check(throwing.GetRuntimeState() == GameRuntimeState::LoadFailed &&
            throwing.GetLevelLoadError() == "Host asset loading failed.",
            "Synchronous host errors must be recorded and rethrown.");
        GameContext noHost;
        ExpectError<std::logic_error>([&] { noHost.StartLevel(10); });
        Check(noHost.GetRuntimeState() == GameRuntimeState::LoadFailed,
            "A missing loader must fail explicitly, without requiring UI virtual overrides.");
    }

    void TestLevelPlayerInitialization()
    {
        Host host;
        auto data = MakeLevel();
        data.objects[1].itemValue = 123;
        data.playerSpawn = PlayerCamera{ { 1500, 45, 2500 }, { -100, 0, 0 } };
        const auto request = host.StartLevel(10);
        host.CompleteLevelLoad(request.id, std::move(data));
        const auto& player = host.GetWorldObjects()[1];
        Check(player.health == 1000 && player.field_07 == 3 &&
            player.collisionRadiusXZ == 15 && player.collisionRadiusY == 23 &&
            player.posX == 1500 && player.posY == 20 && player.posZ == 2500 &&
            player.velocityX == -100 && player.velocityY == 0 && player.velocityZ == 0 &&
            player.itemValue == 0 &&
            host.GetPlayerCamera().position == std::array<std::int16_t, 3>{ 1500, 45, 2500 },
            "GameContext must initialize a clean normal player and convert spawn eye Y into object-center Y.");
        data = MakeLevel();
        data.playerSpawn = PlayerCamera{ { 1500, -32768, 2500 }, { 0, 0, 100 } };
        const auto wrapped = host.StartLevel(11);
        host.CompleteLevelLoad(wrapped.id, std::move(data));
        Check(host.GetWorldObjects()[1].posY == 32743 && host.GetPlayerCamera().position[1] == -32768,
            "Fresh-level eye/center conversion must wrap native signed words.");
        data = MakeLevel();
        data.objects[1].field_07 = 0x80;
        data.objects[1].collisionRadiusXZ = 7;
        data.objects[1].collisionRadiusY = 15;
        data.objects[1].health = 345;
        const auto restored = data.objects[1];
        const auto restore = host.StartLevel(12);
        host.CompleteLevelLoad(restore.id, std::move(data));
        Check(std::memcmp(&restored, &host.GetWorldObjects()[1], sizeof(restored)) == 0,
            "Loading an explicit player record without a spawn must preserve its state, including alternate collision radii.");
    }

    void TestTick()
    {
        Host host;
        host.basicPlayerMovementEnabled = false;
        const auto request = host.StartLevel(10);
        auto data = MakeLevel();
        auto& gold = data.objects[2];
        gold = data.objects[1];
        gold.collisionType = CollisionType::Gold;
        gold.itemValue = 7;
        auto& light = data.objects[3];
        light.health = 1000;
        light.behavior = BehaviorType::PulsatingLight;
        light.posX = 100;
        data.objects[4].health = 1000;
        data.objects[4].behavior = BehaviorType::Frog;
        data.objects[5].health = 1000;
        data.objects[5].displayType = static_cast<DisplayType>(60);
        host.CompleteLevelLoad(request.id, std::move(data));
        host.PromptUserMessage("message", 20);
        host.field_DC34 = 0xffff;
        host.lightingCache.contributions[0][0] = 99;
        Input input;
        input.pressed = true;
        auto expectedSeed = host.randomSeed;
        const auto nextRandom = [](std::uint32_t& seed, unsigned int range)
        {
            seed = static_cast<std::uint32_t>(static_cast<std::uint64_t>(seed) * 134775813u + 1u);
            return (seed >> 16) % range;
        };
        if (nextRandom(expectedSeed, 15) == 0) { nextRandom(expectedSeed, 10); }
        const auto expectedDisplay = 60 - nextRandom(expectedSeed, 4);
        const auto result = host.Tick({ 0, 3 }, input);
        Check(result.status == GameTickStatus::Advanced && result.interaction && result.objects &&
            result.interaction->status == InteractionStatus::Applied && input.calls == 1 &&
            host.gold == 7 && host.worldObjects[2].health == 0 &&
            (*result.objects)[0].status == DispatchStatus::InactiveObject &&
            (*result.objects)[1].status == DispatchStatus::ExecutedCase &&
            (*result.objects)[2].status == DispatchStatus::UnimplementedHandler,
            "Tick must run input/contact before objects and expose unrecovered behavior results.");
        Check(host.tickCount == 0 && host.frameTickCount == 3 && host.userMessageTicks == 17 &&
            static_cast<std::uint8_t>(host.worldObjects[5].displayType) == expectedDisplay &&
            host.randomSeed == expectedSeed &&
            host.GetLightingCache().contributions[11][10] == 30 &&
            host.GetLightingCache().contributions[0][0] == 0 && host.field_DC34 == 0,
            "Frame and behavior clocks must remain distinct; lighting/noise reset once per advanced tick.");
        const auto seed = host.randomSeed;
        host.Tick({ 0, 0 }, input);
        Check(host.randomSeed != seed && host.userMessageTicks == 17 &&
            host.GetLightingCache().contributions[11][10] == 30,
            "Zero native ticks must still update once; successive frames must not accumulate old lighting.");

        const auto before = host.GetWorldObjects();
        const auto cache = host.GetLightingCache().contributions;
        const auto pausedSeed = host.randomSeed;
        const auto calls = input.calls;
        host.SetPaused(true);
        const auto paused = host.Tick({ 10, 10 }, input);
        Check(host.IsPaused() && paused.status == GameTickStatus::Paused &&
            !paused.interaction && !paused.objects && host.userMessageTicks == 17,
            "Explicit pause must return without simulation or input.");
        host.SetPaused(false);
        host.SetMenuPage(MenuPage::Inventory);
        Check(host.Tick({}, input).status == GameTickStatus::Paused &&
            std::memcmp(before.data(), host.GetWorldObjects().data(), sizeof(before)) == 0 &&
            host.GetLightingCache().contributions == cache && host.randomSeed == pausedSeed && input.calls == calls,
            "Menus must suspend gameplay without calling modal UI hooks.");
        host.SetMenuPage(MenuPage::Gameplay);
        Check(host.Tick({}, input).status == GameTickStatus::Advanced,
            "Returning from a menu must allow gameplay to resume.");
        ExpectError<std::invalid_argument>([&] { host.Tick({ -1, 1 }, input); });
        ExpectError<std::invalid_argument>([&] { host.Tick({ 1, -1 }, input); });
        host.level = nullptr;
        ExpectError<std::logic_error>([&] { host.Tick({}, input); });
    }

    void TestPlayerCamera()
    {
        Host host;
        const GameContext& reader = host;
        Check(reader.GetPlayerCamera().position == std::array<std::int16_t, 3>{} &&
            reader.GetPlayerCamera().direction == std::array<std::int16_t, 3>{},
            "Camera getter must expose the initial player record without inventing a default orientation.");
        const auto request = host.StartLevel(10);
        auto data = MakeLevel();
        data.objects[1].posX = 150;
        data.objects[1].posY = -42;
        data.objects[1].posZ = 4050;
        data.objects[1].velocityX = -256;
        data.objects[1].velocityY = 64;
        data.objects[1].velocityZ = 128;
        data.objects[1].winX = 17;
        data.objects[1].winY = 29;
        data.objects[1].winZ = 31;
        host.CompleteLevelLoad(request.id, std::move(data));
        const auto camera = reader.GetPlayerCamera();
        Check(camera.position == std::array<std::int16_t, 3>{ 150, -30, 4050 } &&
            camera.direction == std::array<std::int16_t, 3>{ -256, 64, 128 },
            "Camera must reconstruct eye Y from the object center and read view words, not Euler angles.");
        host.SetPaused(true);
        const auto next = host.StartLevel(11);
        Check(reader.GetPlayerCamera().position == camera.position &&
            reader.GetPlayerCamera().direction == camera.direction,
            "Loading and pause must preserve access to the displayed world's camera.");
        host.FailLevelLoad(next.id, "Test failure");
        host.worldObjects[1].health = 0;
        host.worldObjects[1].posY = -32768;
        host.worldObjects[1].velocityY = 32767;
        const auto updated = reader.GetPlayerCamera();
        Check(updated.position[1] == -32756 && updated.direction[1] == 32767 &&
            camera.position[1] == -30 && camera.direction[1] == 64,
            "Camera snapshots must reflect current signed words independently of health and preserve earlier snapshots.");
    }

    void TestBasicPlayerMovement()
    {
        const auto load = [](Host& host, int heading)
        {
            const auto request = host.StartLevel(10);
            auto data = MakeLevel();
            auto& player = data.objects[1];
            player.posX = 1500;
            player.posY = 73;
            player.posZ = 2500;
            player.velocityX = static_cast<std::int16_t>(SineTable[static_cast<std::size_t>(heading)] / 655);
            player.velocityZ = static_cast<std::int16_t>(SineTable[static_cast<std::size_t>((heading + 128) % 512)] / 655);
            player.velocityY = 17;
            host.CompleteLevelLoad(request.id, std::move(data));
        };
        const auto fixed100 = [](std::int32_t sine)
        {
            const auto product = sine * 100;
            return product < 0 ? -((-product + 65535) / 65536) : product / 65536;
        };
        for (const int initialHeading : { 0, 128, 256, 384 })
        {
            Host host;
            for (unsigned int keys = 0; keys < 64; ++keys)
            {
                load(host, initialHeading);
                Input input;
                for (std::size_t key = 0; key < input.movement.size(); ++key)
                {
                    input.movement[key] = (keys & (1u << key)) != 0;
                }
                int forward = 0;
                int side = 0;
                int turn = 0;
                int heading = initialHeading;
                int x = 1500;
                int y = 73;
                int verticalSpeed = 0;
                int z = 2500;
                // Exhaust all key combinations across held input, braking and normal/diagonal limits.
                for (int frame = 0; frame < 8; ++frame)
                {
                    if (frame == 4) { input.movement.fill(false); }
                    const auto axis = [&](InputKey positive, InputKey negative, int amount)
                    {
                        return (input.movement[static_cast<std::size_t>(positive)] ? amount : 0) -
                            (input.movement[static_cast<std::size_t>(negative)] ? amount : 0);
                    };
                    forward += axis(InputKey::ForwardKey, InputKey::BackwardKey, 4);
                    side += axis(InputKey::StrafeRightKey, InputKey::StrafeLeftKey, 4);
                    turn += axis(InputKey::TurnRightKey, InputKey::TurnLeftKey, 7);
                    const auto sign = [](int value) { return (value > 0) - (value < 0); };
                    forward -= sign(forward);
                    side -= sign(side);
                    turn -= sign(turn);
                    const int ticks = frame % 3 + 1;
                    for (int tick = 0; tick < ticks; ++tick)
                    {
                        const int limit = forward != 0 && side != 0 ? 3 : 5;
                        forward = std::clamp(forward, -limit, limit);
                        side = std::clamp(side, -limit, limit);
                        turn = std::clamp(turn, -6, 6);
                        heading = (heading + turn + 512) % 512;
                        const int dx = fixed100(SineTable[static_cast<std::size_t>(heading)]);
                        const int dz = fixed100(SineTable[static_cast<std::size_t>((heading + 128) % 512)]);
                        x += (dx * forward + dz * side) / 100;
                        z += (dz * forward - dx * side) / 100;
                        y += --verticalSpeed;
                        if (y < 33) { y = 33; verticalSpeed = 0; }
                    }
                    const auto before = host.worldObjects;
                    const auto seed = host.randomSeed;
                    const auto result = host.Tick({ 0, static_cast<std::int16_t>(ticks) }, input);
                    const auto camera = host.GetPlayerCamera();
                    Check(result.playerMovement && result.playerMovement->simulatedTicks == ticks &&
                        result.playerMovement->blockedTicks == 0 && camera.position[0] == x &&
                        camera.position[2] == z && camera.position[1] == y + 12 && camera.direction[1] == 17 &&
                        camera.direction[0] == fixed100(SineTable[static_cast<std::size_t>(heading)]) &&
                        camera.direction[2] == fixed100(SineTable[static_cast<std::size_t>((heading + 128) % 512)]) &&
                        host.soundListenerX == x && host.soundListenerZ == z && host.randomSeed == seed,
                        "Basic movement differs from normal keyboard/local-axis arithmetic, view direction, or listener order.");
                    auto expected = before;
                    expected[1].posX = static_cast<std::int16_t>(x);
                    expected[1].posY = static_cast<std::int16_t>(y);
                    expected[1].posZ = static_cast<std::int16_t>(z);
                    expected[1].velocityX = camera.direction[0];
                    expected[1].velocityZ = camera.direction[2];
                    Check(std::memcmp(expected.data(), host.worldObjects.data(), sizeof(expected)) == 0,
                        "Movement must change only position and horizontal view words, preserving unrelated record bytes.");
                }
                Check(input.movementCalls == 8 * 6, "Movement input must be sampled once per host frame, not per substep.");
            }
        }

        Host host;
        Input input;
        input.movement[static_cast<std::size_t>(InputKey::ForwardKey)] = true;
        load(host, 0);
        const auto before = host.worldObjects;
        Check(host.Tick({ 10, 0 }, input).playerMovement->simulatedTicks == 0 &&
            input.movementCalls == 0 && std::memcmp(before.data(), host.worldObjects.data(), sizeof(before)) == 0,
            "Player simulation must use frame ticks, not behavior ticks, and zero frames must not poll movement.");
        host.basicPlayerMovementEnabled = false;
        Check(!host.Tick({ 0, 3 }, input).playerMovement &&
            input.movementCalls == 0 && host.worldObjects[1].posZ == 2500,
            "Hosts with an external controller must be able to disable the basic controller entirely.");
        host.basicPlayerMovementEnabled = true;
        host.Tick({}, input);
        Check(host.worldObjects[1].posZ == 2503, "First normal forward step must move three DOS units.");
        load(host, 0);
        host.Tick({}, input);
        Check(host.worldObjects[1].posZ == 2503, "Installing a level must reset movement momentum.");

        load(host, 0);
        host.worldObjects[1].health = 0;
        const auto calls = input.movementCalls;
        Check(host.Tick({}, input).playerMovement->simulatedTicks == 0 && input.movementCalls == calls,
            "Inactive players must not move or consume movement input.");
        load(host, 0);
        host.SetPaused(true);
        Check(host.Tick({}, input).status == GameTickStatus::Paused && input.movementCalls == calls,
            "Paused simulation must not sample movement.");
        host.SetPaused(false);

        // Symmetric obstacles produce zero correction; a blocked center must still reject movement.
        std::fill(std::begin(host.collisionMap->data), std::end(host.collisionMap->data), std::uint8_t{ 0x18 });
        const auto blocked = host.Tick({}, input);
        Check(blocked.playerMovement->blockedTicks == 1 &&
            host.worldObjects[1].posX == 1500 && host.worldObjects[1].posZ == 2500,
            "Symmetric blocked centers must report rollback instead of allowing movement through a wall.");
        host.objectCollisionsEnabled = false;
        Check(host.Tick({}, input).playerMovement->blockedTicks == 0 && host.worldObjects[1].posZ == 2505,
            "Disabling object collisions must bypass map correction explicitly.");
        host.objectCollisionsEnabled = true;
        load(host, 0);
        std::fill(std::begin(host.collisionMap->data), std::end(host.collisionMap->data), std::uint8_t{ 0x20 });
        Check(host.Tick({}, input).playerMovement->blockedTicks == 0 && host.worldObjects[1].posZ == 2503,
            "Normal player collision correction must use native mask 18h, not generic object mask 38h.");
        load(host, 0);
        // A complete wall row immediately ahead should push away and terminate without crossing.
        const auto column = (1500 - 112) / 25;
        const auto row = 162 - (2500 - 38) / 25 - 1;
        for (int offset = -3; offset <= 3; ++offset)
        {
            host.collisionMap->data[row * 200 + column + offset] = 0x18;
        }
        host.Tick({ 0, 10 }, input);
        Check(host.worldObjects[1].posZ < 2525,
            "Bounded neighborhood corrections must stop forward motion at a map wall.");
        load(host, 0);
        host.worldObjects[1].collisionRadiusXZ = 1;
        host.worldObjects[1].collisionRadiusY = 1;
        auto& pickup = host.worldObjects[2];
        pickup = host.worldObjects[1];
        --pickup.posY;
        pickup.posZ = 2503;
        pickup.collisionType = CollisionType::Gold;
        pickup.itemValue = 9;
        input.pressed = true;
        const auto gold = host.gold;
        const auto contact = host.Tick({}, input);
        Check(host.gold == gold + 9 && contact.interaction->status == InteractionStatus::Applied &&
            host.worldObjects[2].health == 0,
            "Contact and interaction must use the player position after basic movement.");
        load(host, 128);
        host.worldObjects[1].posX = 1599;
        host.Tick({}, input);
        Check(host.worldObjects[1].posX == 1602 && host.lightingCache.centerX == 1602 &&
            host.soundListenerX == 1602,
            "Listener and light-grid centers must follow the newly moved player in the same tick.");
        host.collisionMap = nullptr;
        ExpectError<std::logic_error>([&] { host.Tick({}, input); });
    }

    void TestPlayerGrounding()
    {
        const auto load = [](Host& host, std::int16_t y)
        {
            auto data = MakeLevel();
            data.objects[1].posX = 1500;
            data.objects[1].posY = y;
            data.objects[1].posZ = 2500;
            data.objects[1].velocityZ = 100;
            const auto request = host.StartLevel(10);
            auto* geometry = data.level.get();
            host.CompleteLevelLoad(request.id, std::move(data));
            return geometry;
        };
        Host host;
        Input input;
        load(host, 73);
        for (const int centerY : { 72, 70, 67, 63, 58, 52, 45, 37, 33, 33 })
        {
            host.Tick({ 0, 1 }, input);
            Check(host.worldObjects[1].posY == centerY &&
                host.GetPlayerCamera().position[1] == centerY + 12 &&
                host.GetPlayerCamera().direction[1] == 0,
                "Gravity must accelerate by one per frame, land at eye height 45, and not pitch the view.");
        }
        load(host, 73);
        host.Tick({ 0, 10 }, input);
        Check(host.worldObjects[1].posY == 33 && host.GetPlayerCamera().position[1] == 45,
            "Batched vertical substeps must land identically to single-frame ticks.");
        load(host, -90);
        host.Tick({}, input);
        Check(host.worldObjects[1].posY == 33,
            "A player below terrain must be lifted to support rather than fall through it.");

        load(host, 73);
        host.Tick({ 0, 4 }, input);
        host.SetPaused(true);
        host.Tick({ 0, 20 }, input);
        Check(host.worldObjects[1].posY == 63, "Pause must freeze a fall.");
        host.SetPaused(false);
        host.basicPlayerMovementEnabled = false;
        host.Tick({ 0, 20 }, input);
        Check(host.worldObjects[1].posY == 63, "Disabling the controller must also disable gravity.");
        host.basicPlayerMovementEnabled = true;
        host.Tick({ 0, 0 }, input);
        Check(host.worldObjects[1].posY == 63, "Zero frame ticks must not integrate gravity.");
        load(host, 73);
        host.Tick({}, input);
        Check(host.worldObjects[1].posY == 72, "Level installation must reset falling momentum.");

        auto* geometry = load(host, 33);
        // A shallow ramp: increasing raw heights lower the native floor Y.
        for (auto& row : geometry->floorHeights)
        {
            for (std::size_t column = 0; column < Level::Width; ++column)
            {
                row[column] = static_cast<std::uint8_t>(column);
            }
        }
        host.worldObjects[1].posY = static_cast<std::int16_t>(
            host.GetTerrainFloorHeight(1500, 45, 2500) + 33);
        const auto initialY = host.GetPlayerCamera().position[1];
        input.movement[static_cast<std::size_t>(InputKey::StrafeLeftKey)] = true;
        for (int tick = 0; tick < 80; ++tick)
        {
            host.Tick({}, input);
            const auto camera = host.GetPlayerCamera();
            const auto floor = host.GetTerrainFloorHeight(camera.position[0], camera.position[1], camera.position[2]);
            Check(camera.position[1] == floor + 45 && host.worldObjects[1].posY == floor + 33,
                "Walking uphill must follow interpolated terrain at the final horizontal position.");
        }
        Check(host.GetPlayerCamera().position[1] > initialY, "Uphill fixture must actually change elevation.");
        input.movement.fill(false);
        input.movement[static_cast<std::size_t>(InputKey::StrafeRightKey)] = true;
        for (int tick = 0; tick < 100; ++tick) { host.Tick({}, input); }
        input.movement.fill(false);
        host.Tick({ 0, 30 }, input);
        const auto camera = host.GetPlayerCamera();
        Check(camera.position[1] == host.GetTerrainFloorHeight(
            camera.position[0], camera.position[1], camera.position[2]) + 45 &&
            camera.position[1] < initialY,
            "Walking downhill must fall and settle onto the lower terrain, not retain the old Y.");
        // Native player support uses terrain interpolation, not the special flat-depth floor shortcut.
        for (auto& row : geometry->surfaceFlags) { std::fill(std::begin(row), std::end(row), std::uint8_t{ 1 }); }
        host.Tick({}, input);
        host.worldObjects[1].posX = 0;
        ExpectError<std::out_of_range>([&] { host.Tick({}, input); });
    }

    void TestRendererHostInteraction(const std::filesystem::path& levelDirectory,
        const std::filesystem::path& savePath)
    {
        const auto readBytes = [](const std::filesystem::path& path)
        {
            std::ifstream stream(path, std::ios::binary);
            if (!stream) { throw std::runtime_error("Cannot open renderer-host interaction fixture."); }
            return std::vector<std::uint8_t>(
                std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
        };
        GameCoreLevelSource source;
        source.levelNumber = 11;
        source.geometry = readBytes(levelDirectory / "FELD11.256");
        const auto objectRecords = readBytes(levelDirectory / "OBJPOS11.DAT");
        source.playerDirection = { 0, 0, 100 };
        const auto saved = readBytes(savePath);
        Check(saved.size() >= 299, "Original save fixture must contain the player record at byte 245.");
        const auto nativePlayer = DecodeGameObject(saved.data() + 245, 54);
        Check(nativePlayer.field_07 == 3 && nativePlayer.collisionRadiusXZ == 15 &&
            nativePlayer.collisionRadiusY == 23, "Original standing player collision shape changed.");
        const auto object = [&](std::size_t record)
        {
            Check(objectRecords.size() >= (record + 1) * 54, "Missing original interaction object.");
            return DecodeGameObject(objectRecords.data() + record * 54, 54);
        };
        int fileRequests = 0;
        GameCoreHost host([&](std::string_view filename)
        {
            Check(filename == "LEVEL\\OBJPOS11.DAT", "GameCore must choose the requested level's OBJPOS asset.");
            ++fileRequests;
            return readBytes(levelDirectory / "OBJPOS11.DAT");
        });
        const auto worldObject = [&](int slot)
        {
            std::uint8_t bytes[54];
            host.GetObjectBytes(slot, bytes);
            return DecodeGameObject(bytes, sizeof(bytes));
        };
        const auto loadAt = [&](const RawGameObject& target)
        {
            source.playerPosition = { target.posX, static_cast<std::int16_t>(target.posY + 45), target.posZ };
            Check(host.LoadLevel(source), "Real renderer-host level loading failed.");
            Check(fileRequests > 0, "Level loading must obtain objects through the file service.");
            for (std::size_t record = 0; record < objectRecords.size() / 54; ++record)
            {
                const auto loaded = worldObject(static_cast<int>(record + 2));
                Check(std::memcmp(&loaded, objectRecords.data() + record * 54, 54) == 0,
                    "GameCore must load every original OBJPOS record byte-for-byte before simulation.");
            }
            const auto player = worldObject(1);
            Check(player.field_07 == nativePlayer.field_07 &&
                player.collisionRadiusXZ == nativePlayer.collisionRadiusXZ &&
                player.collisionRadiusY == nativePlayer.collisionRadiusY &&
                host.GetPlayerCamera().position == source.playerPosition,
                "Host must use the original player collision shape and convert eye/center position exactly once.");
        };
        const auto medicine = object(32);
        Check(medicine.collisionType == CollisionType::Medicine && medicine.posY == 5,
            "Expected original level-11 floor medicine fixture.");
        loadAt(medicine);
        Check(host.Tick(0, 1, {}) && worldObject(34).health == 1000,
            "Contact without Enter must not consume medicine.");
        GameCoreInput enter;
        enter.interaction = true;
        Check(host.Tick(0, 1, enter) && worldObject(34).health == 0 &&
            host.GetLastInteractionMessage().find("Interaction applied") == 0,
            "Enter through the actual host must pick up real floor medicine after grounding.");
        const auto door = object(0);
        loadAt(door);
        Check(host.Tick(1, 1, enter), "Real unlocked-door host tick failed.");
        if (worldObject(2).field_26 != 3 || host.GetLastInteractionMessage().find("Interaction applied") != 0)
        {
            throw std::runtime_error("Real unlocked-door interaction failed: " + host.GetLastInteractionMessage() +
                ", player Y " + std::to_string(worldObject(1).posY) +
                ", door Y " + std::to_string(worldObject(2).posY) +
                ", rate " + std::to_string(worldObject(2).field_26));
        }
        const auto trigger = object(9);
        Check(trigger.collisionType == CollisionType::Switch, "Expected ordinary door-trigger fixture.");
        loadAt(trigger);
        Check(host.Tick(1, 1, {}), "Initial ordinary-switch behavior tick failed.");
        const auto linkedDoor = worldObject(6);
        Check(linkedDoor.health == 1001 && linkedDoor.field_26 == 0,
            "Expected level-11 door encoded with switch index 1.");
        Check(host.Tick(1, 1, enter) && worldObject(11).blendWeightOrSpriteOpacity == 10 &&
            worldObject(6).field_26 == 3 && worldObject(6).blendWeightOrSpriteOpacity == 3 &&
            host.GetLastInteractionMessage().find("Interaction applied") == 0,
            "Enter must pull the original level-11 switch and animate its linked door in the same tick.");
        for (int tick = 0; tick < 40; ++tick) { Check(host.Tick(1, 1, {}), "Switch/door animation tick failed."); }
        Check(worldObject(6).blendWeightOrSpriteOpacity == 100 &&
            worldObject(11).blendWeightOrSpriteOpacity == 0,
            "Linked door must finish opening while the ordinary switch returns to rest.");
    }

    void TestSimpleSwitchInteraction()
    {
        GameContext context;
        auto& lever = context.worldObjects[150];
        lever.health = 1000;
        lever.collisionType = CollisionType::Switch;
        lever.behavior = BehaviorType::Switch1;
        lever.itemValue = 1;
        lever.field_24 = 10;
        lever.field_32 = 1;
        const auto resting = lever;
        for (const int slot : { 2, 140, 141, 164 })
        {
            auto& target = context.worldObjects[static_cast<std::size_t>(slot)];
            target.health = 1001;
            target.collisionType = CollisionType::Door;
            target.modelFrame2 = 1;
            target.field_24 = 30;
            target.doorLockOrGroupBitmask = -1;
        }
        const auto before = context.worldObjects;
        Check(HandlePlayerObjectInteraction(lever, context).status == InteractionStatus::Applied &&
            lever.field_26 == 10 && lever.health == 1000 && context.inventory[0].health == 0,
            "Ordinary switch interaction must start its rate, not pick up a flagged lever.");
        for (const int slot : { 2, 140 })
        {
            const auto& target = context.worldObjects[static_cast<std::size_t>(slot)];
            Check(target.field_26 == 3 && target.itemValue == 30 && target.health == 1001 &&
                target.doorLockOrGroupBitmask == -1,
                "Switch-index matching must open all matching doors without consuming their health link or lock.");
        }
        Check(std::memcmp(&before[141], &context.worldObjects[141], sizeof(RawGameObject)) == 0 &&
            std::memcmp(&before[164], &context.worldObjects[164], sizeof(RawGameObject)) == 0,
            "Switch target dispatch must stop at native slot 140.");

        for (const auto phase : { -1, 1, 50, 100, 101 })
        {
            lever = resting;
            lever.blendWeightOrSpriteOpacity = static_cast<std::int16_t>(phase);
            const auto snapshot = context.worldObjects;
            Check(HandlePlayerObjectInteraction(lever, context).status == InteractionStatus::NoEffect &&
                std::memcmp(snapshot.data(), context.worldObjects.data(), sizeof(snapshot)) == 0,
                "Nonzero switch phase must reject reactivation without side effects.");
        }
        for (const auto health : { 999, 1001 })
        {
            lever = resting;
            lever.health = static_cast<std::int16_t>(health);
            context.worldObjects[2].field_26 = 0;
            Check(HandlePlayerObjectInteraction(lever, context).status == InteractionStatus::Applied &&
                lever.field_26 == 10 && context.worldObjects[2].field_26 == 0,
                "A non-1000 live switch must animate but not activate indexed targets.");
        }
        for (const auto index : { -1, 0 })
        {
            lever = resting;
            lever.itemValue = static_cast<std::int16_t>(index);
            Check(HandlePlayerObjectInteraction(lever, context).status == InteractionStatus::Applied &&
                lever.field_26 == 10, "A nonpositive switch index must still start the local animation.");
        }
        lever = resting;
        context.worldObjects[2].collisionType = CollisionType::Skeleton;
        const auto unsupported = context.worldObjects;
        Check(HandlePlayerObjectInteraction(lever, context).status == InteractionStatus::UnimplementedHandler &&
            std::memcmp(unsupported.data(), context.worldObjects.data(), sizeof(unsupported)) == 0,
            "Unsupported indexed target types must be explicit and must not partially activate other doors.");
        context.worldObjects[2].collisionType = CollisionType::Door;
        context.worldObjects[2].doorLockOrGroupBitmask = 1;
        context.worldObjects[2].field_26 = 0;
        lever = resting;
        Check(HandlePlayerObjectInteraction(lever, context).status == InteractionStatus::Applied &&
            context.worldObjects[2].field_26 == 0,
            "Switch activation must preserve positive key locks.");

        lever = resting;
        lever.itemValue = 32767;
        context.worldObjects[2].health = -31769;
        context.worldObjects[2].doorLockOrGroupBitmask = 0;
        Check(HandlePlayerObjectInteraction(lever, context).status == InteractionStatus::Applied &&
            context.worldObjects[2].field_26 == 3,
            "Switch health links must use wrapped word addition, without an extra target-health gate.");
        lever = resting;
        lever.behaviorParameter = 1;
        context.tickCount = 1;
        HandlePlayerObjectInteraction(lever, context);
        for (int tick = 0; tick < 20; ++tick) { Function_Switch1(lever, context); }
        Check(lever.blendWeightOrSpriteOpacity == 100 && lever.field_26 == 0,
            "A latched ordinary switch must remain pulled and stop animating.");
        Check(HandlePlayerObjectInteraction(lever, context).status == InteractionStatus::NoEffect,
            "A fully pulled latched switch must ignore further interaction.");
        lever = resting;
        lever.itemValue = 3;
        lever.field_26 = 123;
        lever.soundEffectId = 2;
        context.soundEffectsLoaded = true;
        context.soundEffects[1].sampleCount = 1;
        context.soundEffects[1].samples[0] = 1;
        const auto seed = context.randomSeed;
        Check(HandlePlayerObjectInteraction(lever, context).status == InteractionStatus::Applied &&
            lever.field_26 == 10 && context.randomSeed == seed * 134775813u + 1u,
            "At phase zero, native activation overwrites the rate and requests sound even when no target matches.");
        lever.health = 0;
        Check(HandlePlayerObjectInteraction(lever, context).status == InteractionStatus::InactiveObject,
            "Inactive ordinary switches must reject interaction.");
    }

    void TestLevelObjectLoading()
    {
        class FileHost final : public GameContext
        {
        public:
            using GameContext::LoadLevelObjects;
            std::vector<std::uint8_t> payload;
            std::string requested;
            bool missing = false;
        protected:
            std::vector<std::uint8_t> LoadFile(std::string_view filename) override
            {
                requested = filename;
                if (missing) { throw std::runtime_error(std::string(filename) + ": file not found"); }
                return payload;
            }
        };
        FileHost host;
        auto data = MakeLevel();
        data.objects[0].itemValue = 7;
        data.objects[165].itemValue = 8;
        const auto before = data.objects;
        host.payload.resize(163 * 54);
        for (std::size_t i = 0; i < host.payload.size(); ++i)
        {
            host.payload[i] = static_cast<std::uint8_t>(i * 17 + 3);
        }
        host.LoadLevelObjects(12, data);
        Check(host.requested == "LEVEL\\OBJPOS12.DAT" &&
            std::memcmp(&data.objects[2], host.payload.data(), host.payload.size()) == 0 &&
            std::memcmp(&data.objects[0], &before[0], 2 * sizeof(RawGameObject)) == 0 &&
            std::memcmp(&data.objects[165], &before[165], sizeof(RawGameObject)) == 0,
            "OBJPOS loading must fill all 163 ordinary slots without touching reserved, player or fallback slots.");
        host.payload.resize(54);
        host.LoadLevelObjects(99, data);
        Check(data.objects[3].health == 0 && data.objects[164].health == 0,
            "Shorter object files must clear unused slots instead of retaining previous objects.");
        const auto valid = data.objects;
        for (const std::size_t badSize : { std::size_t{ 53 }, std::size_t{ 55 }, std::size_t{ 164 * 54 } })
        {
            host.payload.resize(badSize);
            ExpectError<std::invalid_argument>([&] { host.LoadLevelObjects(12, data); });
            Check(std::memcmp(valid.data(), data.objects.data(), sizeof(valid)) == 0,
                "Invalid OBJPOS data must not partially replace records.");
        }
        host.missing = true;
        ExpectError<std::runtime_error>([&] { host.LoadLevelObjects(12, data); });
        ExpectError<std::invalid_argument>([&] { host.LoadLevelObjects(0, data); });
        GameCoreLevelSource source;
        source.levelNumber = 12;
        source.geometry.resize(Level::GeometryStreamSize);
        source.geometry[0] = 49;
        source.geometry[2] = 39;
        source.playerPosition = { 1500, 45, 2500 };
        bool invalid = false;
        GameCoreHost adapter([&](std::string_view filename) -> std::vector<std::uint8_t>
        {
            if (invalid) { return std::vector<std::uint8_t>(55); }
            throw std::runtime_error(std::string(filename) + ": missing");
        });
        Check(!adapter.LoadLevel(source) && !adapter.IsLevelReady() &&
            adapter.GetLastError().find("OBJPOS12.DAT") != std::string::npos,
            "Missing object-file errors must reach the host without installing a partial world.");
        invalid = true;
        Check(!adapter.LoadLevel(source) && !adapter.IsLevelReady() &&
            adapter.GetLastError().find("54-byte") != std::string::npos,
            "Malformed object-file errors must propagate through the level lifecycle.");
    }

    void TestObjectBoundary()
    {
        struct RendererObject
        {
            std::uint8_t bytes[54]{};
            int rendererMetadata = 123;
        } renderer;
        for (unsigned int value = 0; value < 65536; ++value)
        {
            for (std::size_t i = 0; i < sizeof(renderer.bytes); ++i)
            {
                renderer.bytes[i] = static_cast<std::uint8_t>(value + i * 17);
            }
            renderer.bytes[8] = static_cast<std::uint8_t>(value);
            renderer.bytes[9] = static_cast<std::uint8_t>(value >> 8);
            const auto game = DecodeGameObject(renderer.bytes, sizeof(renderer.bytes));
            Check(static_cast<std::uint16_t>(game.behavior) == value &&
                game.behaviorParameter == renderer.bytes[52] && game.spriteScale == renderer.bytes[53],
                "Byte import must preserve full-width behavior IDs and both independent trailing bytes.");
            std::array<std::uint8_t, 54> saved;
            std::copy(std::begin(renderer.bytes), std::end(renderer.bytes), saved.begin());
            std::fill(std::begin(renderer.bytes), std::end(renderer.bytes), std::uint8_t{ 0 });
            EncodeGameObject(game, renderer.bytes, sizeof(renderer.bytes));
            Check(std::equal(saved.begin(), saved.end(), std::begin(renderer.bytes)) &&
                renderer.rendererMetadata == 123, "Object export must not touch renderer metadata.");
        }
        ExpectError<std::invalid_argument>([&] { DecodeGameObject(nullptr, 54); });
        ExpectError<std::invalid_argument>([&] { DecodeGameObject(renderer.bytes, 53); });
        ExpectError<std::invalid_argument>([&] { EncodeGameObject({}, nullptr, 54); });
        ExpectError<std::invalid_argument>([&] { EncodeGameObject({}, renderer.bytes, 55); });
    }

    std::string ReadText(const char* path)
    {
        std::ifstream file(path);
        if (!file) { throw std::runtime_error("Cannot open enum compatibility input."); }
        return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
    }

    std::map<std::string, unsigned long> ReadEnum(const std::string& text, const std::string& name)
    {
        const std::regex declaration("enum\\s+class\\s+" + name + "(?:\\s*:\\s*[^\\{]+)?\\s*\\{([^}]*)\\}");
        std::smatch match;
        Check(std::regex_search(text, match, declaration), "Cannot find enum declaration.");
        const auto body = std::regex_replace(match[1].str(), std::regex("//[^\\r\\n]*"), "");
        const std::regex entry("^\\s*([A-Za-z_][A-Za-z_0-9]*)\\s*(?:=\\s*(0[xX][0-9A-Fa-f]+|[0-9]+))?\\s*$");
        std::map<std::string, unsigned long> values;
        unsigned long next = 0;
        std::size_t start = 0;
        while (start < body.size())
        {
            const auto comma = body.find(',', start);
            const auto part = body.substr(start, comma == std::string::npos ? comma : comma - start);
            if (part.find_first_not_of(" \r\n\t") != std::string::npos)
            {
                std::smatch item;
                Check(std::regex_match(part, item, entry), "Unsupported enum expression; update the compatibility test.");
                if (item[2].matched)
                {
                    const auto number = item[2].str();
                    next = std::stoul(number, nullptr, number.find_first_of("xX") != std::string::npos ? 16 : 10);
                }
                Check(values.emplace(item[1].str(), next++).second, "Duplicate enum name.");
            }
            if (comma == std::string::npos) { break; }
            start = comma + 1;
        }
        Check(!values.empty(), "Empty enum compatibility input.");
        return values;
    }

    void CheckEnums(const std::string& renderer, const std::string& game)
    {
        for (const auto* type : { "BehaviorType", "CollisionType", "DisplayType", "WeaponType" })
        {
            const auto source = ReadEnum(renderer, type);
            const auto destination = ReadEnum(game, type);
            for (const auto& item : source)
            {
                const auto name = std::string(type) == "BehaviorType" && item.first == "SwimmingParticles" ?
                    "FloatingDebris" : item.first;
                const auto found = destination.find(name);
                if (found == destination.end() || found->second != item.second)
                {
                    throw std::runtime_error(std::string("Renderer/game enum mismatch: ") + type + "::" + item.first);
                }
            }
        }
    }
}

int main(int argc, char** argv)
{
    try
    {
        Check(argc == 5, "Expected enum header paths, original level directory and native player save fixture.");
        TestLoading();
        TestLevelPlayerInitialization();
        TestTick();
        TestPlayerCamera();
        TestBasicPlayerMovement();
        TestPlayerGrounding();
        TestRendererHostInteraction(argv[3], argv[4]);
        TestSimpleSwitchInteraction();
        TestLevelObjectLoading();
        TestObjectBoundary();
        const auto renderer = ReadText(argv[1]);
        const auto game = ReadText(argv[2]);
        CheckEnums(renderer, game);
        auto mismatch = renderer;
        const auto pillar = mismatch.find("Pillar = 8");
        Check(pillar != std::string::npos, "Expected renderer Pillar fixture.");
        mismatch.replace(pillar, std::string("Pillar = 8").size(), "Pillar = 9");
        ExpectError<std::runtime_error>([&] { CheckEnums(mismatch, game); });
        std::cout << "PASS: owned level lifecycle, basic keyboard movement/collision/camera, non-blocking tick/pause/input, separate clocks, "
            "load errors, 65536 object payloads, and renderer enum compatibility.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
