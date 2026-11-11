#include "BehaviorDispatch.h"
#include "GameMath.h"

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
    void Check(bool value, const char* message)
    {
        if (!value) { throw std::runtime_error(message); }
    }

    bool Same(const RawGameObject& left, const RawGameObject& right)
    {
        return std::memcmp(&left, &right, sizeof(left)) == 0;
    }

    struct Input final : InputHandler
    {
        bool interact = false, right = false;
        bool IsKeyPressed(InputKey key) override
        {
            return (interact && key == InputKey::InteractionKey) || (right && key == InputKey::TurnRightKey);
        }
    };

    struct Host final : GameContext
    {
        std::filesystem::path assets;
        int failures = 0;
        explicit Host(const std::filesystem::path& path) : assets(path)
        {
            StartNewGame(2);
            StartLevel(32);
            Check(GetRuntimeState() == GameRuntimeState::Ready, "Original submarine level must load.");
        }
        void OnObjectUpdateFailed(int, BehaviorType, std::string_view message) override
        {
            ++failures;
            throw std::runtime_error(std::string(message));
        }
        void IsolateSubmarine()
        {
            for (int slot = 2; slot <= 164; ++slot)
            {
                if (slot != 16) { SetObjectHealth(worldObjects[slot], 0); }
            }
            auto& sub = worldObjects[16];
            sub.posX = 1500; sub.posY = -100; sub.posZ = 2500;
            sub.velocityX = sub.velocityY = sub.velocityZ = 0;
            sub.winY = WrapSignedWord(512 - GetPlayerHeading());
            std::fill(std::begin(collisionMap->data), std::end(collisionMap->data), std::uint8_t{0});
            for (auto& row : level->floorHeights) { std::fill(std::begin(row), std::end(row), std::uint8_t{10}); }
            worldObjects[1].posX = sub.posX;
            worldObjects[1].posZ = sub.posZ;
            worldObjects[1].posY = WrapSignedWord(sub.posY + playerBodyHeight - worldObjects[1].collisionRadiusY - 2);
        }
    protected:
        std::vector<std::uint8_t> LoadFile(std::string_view name) override
        {
            std::ifstream file(assets / name, std::ios::binary);
            if (!file) { throw std::runtime_error("Missing submarine fixture: " + std::string(name)); }
            return {std::istreambuf_iterator<char>(file), {}};
        }
        void LoadLevel(const LevelLoadRequest& request) override
        {
            LevelLoadData data;
            LoadLevelGeometry(request.levelNumber, data);
            LoadLevelObjects(request.levelNumber, data);
            InitializeLevelPlayerSpawn(data, {0, 0, 100});
            CompleteLevelLoad(request.id, std::move(data));
        }
    };

    void TestTextureNoise(const std::filesystem::path& assets)
    {
        Host host(assets);
        auto& noise = host.worldObjects[36];
        auto& target = host.level->texturePixels[8];
        const auto& source = host.level->texturePixels[9];
        Check(noise.behavior == BehaviorType::UnderwaterTextureNoise && noise.itemValue == 8 &&
            noise.generalPurpose1 == 9 && noise.generalPurpose3 == 30 &&
            !target.pixels.empty() && !source.pixels.empty(),
            "The original noise object must load its two original paletted textures.");
        const auto original = target.pixels;
        const auto object = noise;
        host.tickCount = 7;
        host.randomSeed = 1234;
        DispatchObjectCase(noise, host);
        Check(target.pixels == original && Same(noise, object) && host.randomSeed == 1234 && target.revision == 0,
            "Texture noise must be completely inert outside underwater mode.");
        host.playerUnderwater = true;
        auto expected = original;
        GameContext reference;
        reference.randomSeed = host.randomSeed;
        std::int16_t last = 0;
        for (int copy = 0; copy < 210; ++copy)
        {
            const auto color = source.pixels.at(reference.Random(source.width));
            last = WrapSignedWord(reference.Random(static_cast<std::uint16_t>(expected.size())));
            expected.at(static_cast<std::uint16_t>(last)) = color;
        }
        DispatchObjectCase(noise, host);
        Check(target.pixels == expected && expected != original && noise.velocityX == last &&
            target.revision == 1 && host.randomSeed == reference.randomSeed,
            "Noise must copy exactly thirty pixels per tick, sampling the source's FIRST row in native RNG order.");
        host.tickCount = 0;
        const auto seed = host.randomSeed;
        DispatchObjectCase(noise, host);
        Check(target.revision == 1 && host.randomSeed == seed, "Zero-tick noise must not upload or consume random numbers.");
        const std::uint8_t truncated[]{1,0,1,0,7};
        bool rejected = false;
        try { host.level->LoadTexturePixels(8, truncated, sizeof(truncated)); }
        catch (const std::invalid_argument&) { rejected = true; }
        Check(rejected && target.pixels == expected, "Malformed texture input must be rejected without losing the live pixels.");
    }

    void TestFish(const std::filesystem::path& assets)
    {
        Host original(assets);
        auto level = CreateDummyLevel();
        GameContext fish, split;
        fish.level = split.level = &level;
        fish.collisionMap = split.collisionMap = level.collisionMap.get();
        int count = 0;
        for (const auto& record : original.worldObjects)
        {
            if (record.behavior != BehaviorType::DecoFish) { continue; }
            ++count;
            auto batched = record, stepped = record;
            fish.randomSeed = split.randomSeed = 789;
            fish.tickCount = 127;
            DispatchObjectCase(batched, fish);
            split.tickCount = 1;
            for (int tick = 0; tick < 127; ++tick) { DispatchObjectCase(stepped, split); }
            Check(Same(batched, stepped) && fish.randomSeed == split.randomSeed,
                "Every authored decorative fish must preserve phase, fixed-point carries and RNG over batched ticks.");
            Check(batched.posY == record.posY && batched.collisionMask == record.collisionMask,
                "Decorative fish must retain their authored depth and collision mask.");
        }
        Check(count == 22, "Level 32 must contain exactly twenty-two decorative fish.");
        auto& object = fish.worldObjects[2];
        object = {};
        object.behavior = BehaviorType::DecoFish;
        object.health = 1000;
        object.posX = 1000; object.posZ = 2000;
        object.winY = 128; object.velocityY = -17;
        object.blendWeightOrSpriteOpacity = 95;
        object.generalPurpose1 = 100; object.generalPurpose3 = 10;
        fish.tickCount = 1; fish.randomSeed = 1;
        DispatchObjectCase(object, fish);
        Check(object.blendWeightOrSpriteOpacity == 105 && object.generalPurpose3 == 3 &&
            object.generalPurpose1 == 97 && object.posX == 999 && object.velocityX == 159 &&
            object.posZ == 2000 && object.winY == 127 && object.velocityY == -16,
            "Native fish slow-half phase, negative fixed-point carry and truncation-toward-zero turn must match exactly.");
        object.winY = 0; object.velocityX = object.velocityZ = object.velocityY = 0;
        const auto z = object.posZ;
        object.generalPurpose1 = 500;
        fish.randomSeed = 1;
        DispatchObjectCase(object, fish);
        Check(object.posZ > z, "Heading-zero fish must swim in positive native Z, not the reflected camera direction.");
        object.velocityY = 0;
        fish.randomSeed = 0;
        DispatchObjectCase(object, fish);
        Check(object.velocityY != 0, "The native one-percent random turn must initialize and accelerate a turn.");
    }

    void TestVehicle(const std::filesystem::path& assets)
    {
        Host host(assets);
        host.IsolateSubmarine();
        auto& sub = host.worldObjects[16];
        Input input;
        input.interact = true;
        Check(UpdatePlayerInteraction(host, input).status == InteractionStatus::Applied && host.controlledVehicleSlot == 16,
            "Actual player contact and Enter must board the authored submarine through collision 78.");
        input.interact = false;
        const auto z = sub.posZ;
        const auto oxygen = host.playerOxygen;
        for (int tick = 0; tick < 34; ++tick)
        {
            Check(host.Tick({0,1}, input).status == GameTickStatus::Advanced, "Submarine controller must run on player-frame ticks.");
            const auto camera = host.GetPlayerCamera();
            Check(camera.position[0] == sub.posX && camera.position[2] == sub.posZ &&
                camera.position[1] == sub.posY + host.playerBodyHeight,
                "The submarine camera must follow its controlled pose without gravity.");
        }
        Check(sub.posZ > z && sub.modelFrame2 == 2 && sub.animFrame == 1 &&
            sub.blendWeightOrSpriteOpacity == 0 && sub.collisionRadiusXZ == 62 && sub.collisionRadiusY == 50,
            "The submarine must travel forward, close its hatch at phase greater than 200, and use native collision bounds.");
        host.Tick({0,1}, input);
        Check(host.playerUnderwater && host.playerOxygen == oxygen &&
            host.lightingCache.contributions[10][10] == 28 &&
            host.lightingCache.contributions[7][7] == 4,
            "Closed-cabin movement must set underwater presentation and the 7x7 headlamp without consuming player oxygen.");
        HandlePlayerObjectInteraction(sub, host);
        Check(sub.blendWeightOrSpriteOpacity == 100, "Enter in the moving submarine must open its steering pulse.");
        const auto yaw = sub.winY;
        input.right = true;
        host.Tick({0,1}, input);
        Check(sub.winY != yaw && sub.blendWeightOrSpriteOpacity == 95 && sub.modelFrame2 >= 2 && sub.modelFrame2 <= 3,
            "A steering pulse must rotate the submarine toward keyboard heading, select the turn pose and decay by five.");
        input.right = false;
        const auto snapshot = host.CaptureSaveGame("Submarine");
        Host loaded(assets);
        loaded.LoadGame(snapshot);
        Check(loaded.controlledVehicleSlot == 16 && Same(loaded.worldObjects[16], sub),
            "Reloading a riding save must restore the submarine selection and complete native object state.");
        loaded.randomSeed = host.randomSeed;
        host.Tick({0,1}, input);
        loaded.Tick({0,1}, input);
        Check(Same(loaded.worldObjects[16], sub) && loaded.GetPlayerCamera().position == host.GetPlayerCamera().position,
            "The restored submarine must continue its fixed-point motion and riding camera exactly.");
        const auto pose = sub;
        host.Tick({7,0}, input);
        Check(Same(sub, pose), "Behavior ticks alone must select but not move the controlled submarine.");
        host.SetObjectHealth(sub, 0);
        host.Tick({0,1}, input);
        Check(host.controlledVehicleSlot == 0, "Retiring a controlled submarine must release normal player movement.");
    }

    void TestStressAndExit(const std::filesystem::path& assets)
    {
        Input input;
        Host host(assets);
        host.IsolateSubmarine();
        auto& sub = host.worldObjects[16];
        sub.modelFrame2 = 2;
        HandlePlayerObjectInteraction(sub, host);
        auto& obstacle = host.worldObjects[2];
        obstacle = {};
        obstacle.health = 1000; obstacle.collisionMask = 1;
        obstacle.collisionType = CollisionType::PushPlayer;
        obstacle.posX = sub.posX; obstacle.posY = sub.posY; obstacle.posZ = sub.posZ;
        obstacle.collisionRadiusXZ = 100; obstacle.collisionRadiusY = 100;
        obstacle.behaviorParameter = 17;
        host.Tick({0,1}, input);
        Check(host.windX == 17 && obstacle.behaviorParameter == 0,
            "Obstacle contact must add authored hull stress and consume its damage byte.");
        host.Tick({0,1}, input);
        Check(host.windX == 17, "The same obstacle must not repeatedly apply consumed hull stress.");
        obstacle.collisionType = CollisionType::LevelEnd;
        obstacle.doorLockOrGroupBitmask = 1;
        host.Tick({0,1}, input);
        Check(sub.behavior == BehaviorType::None && sub.collisionMask == 0 &&
            host.controlledVehicleSlot == 0 && obstacle.collisionRadiusXZ == 200,
            "Submarine exit contact must retire the controller and widen the exit trigger for the player.");

        Host wall(assets);
        wall.IsolateSubmarine();
        auto& boat = wall.worldObjects[16];
        HandlePlayerObjectInteraction(boat, wall);
        wall.SetCollisionMapPixelAtWorldXZ(WrapSignedWord(boat.posX + 25), boat.posZ, 0x18);
        wall.Tick({0,1}, input);
        Check(wall.windX == 3 && boat.velocityY == 30,
            "A blocking map correction must add three hull stress and start a thirty-step impact cooldown.");
        wall.Tick({0,1}, input);
        Check(wall.windX == 3 && boat.velocityY == 29,
            "Map impacts must not add stress again while the native cooldown is positive.");
    }

    void TestWindow(const std::filesystem::path& assets)
    {
        Host host(assets);
        auto& window = host.worldObjects[60];
        Check(window.behavior == BehaviorType::CrackingSubmarineWindow && window.itemValue == 54,
            "The original cracking window must retain its authored failure threshold.");
        host.windX = 20;
        host.tickCount = 0;
        DispatchObjectCase(window, host);
        Check(window.blendWeightOrSpriteOpacity == 0, "Zero ticks cannot advance a crack below overload.");
        host.tickCount = 5;
        DispatchObjectCase(window, host);
        Check(window.blendWeightOrSpriteOpacity == 1 && window.behavior == BehaviorType::CrackingSubmarineWindow,
            "Pressure must advance cracking by one, not by a whole frame's tick count.");
        host.windX = 0;
        DispatchObjectCase(window, host);
        Check(window.blendWeightOrSpriteOpacity == 1, "Reduced hull stress must not heal the window.");
        host.windX = 100;
        window.blendWeightOrSpriteOpacity = 53;
        host.tickCount = 0;
        DispatchObjectCase(window, host);
        Check(window.behavior == BehaviorType::CrackingSubmarineWindow, "Stress exactly 100 must not force immediate overload.");
        host.windX = 101;
        DispatchObjectCase(window, host);
        Check(window.behavior == BehaviorType::None && window.animFrame == 2 && window.modelFrame2 == 0 &&
            window.blendWeightOrSpriteOpacity == 55 && host.playerLife == 0 &&
            host.playerHitEffectTicks == 1000 && host.playerPostureDelta == -1,
            "Stress above 100 must shatter even on zero ticks and enter the shared player-death state.");

        Host threshold(assets);
        auto& pane = threshold.worldObjects[60];
        pane.blendWeightOrSpriteOpacity = 53;
        threshold.windX = 54; threshold.tickCount = 5;
        DispatchObjectCase(pane, threshold);
        Check(pane.blendWeightOrSpriteOpacity == 54 && pane.behavior == BehaviorType::None && threshold.playerLife == 0,
            "Reaching the authored threshold exactly must also shatter the pane.");
    }

    void TestHeldPole(const std::filesystem::path& assets)
    {
        Host host(assets);
        auto& pole = host.worldObjects[49];
        Check(pole.behavior == BehaviorType::MetalPole && pole.modelId == 37 &&
            pole.modelAnim == 1 && pole.animFrame == 3, "The original underwater pole must retain its authored model.");
        host.inventory.fill({});
        host.RefreshWeaponInventory();
        const auto pickup = HandlePlayerObjectInteraction(pole, host);
        Check(pickup.status == InteractionStatus::Applied, "The underwater pole must enter the player's inventory.");
        Input input;
        for (int tick = 0; tick < 60; ++tick)
        {
            host.Tick({1, 1}, input);
        }
        const auto view = host.GetWeaponView();
        Check(host.selectedWeapon == WeaponType::TrapOrPole && host.weaponSpriteFrame == 0 &&
            view && view->kind == WeaponView::Kind::Model && view->object.modelId == 37 &&
            view->object.modelAnim == 1 && view->object.animFrame == 3 && !view->bindWeaponBitmap,
            "Picking up the actual level-32 pole must show its first-person model without a weapon sprite frame.");
    }

    void TestCoverage(const std::filesystem::path& assets)
    {
        Host host(assets);
        Input input;
        for (int tick = 0; tick < 30; ++tick)
        {
            const auto result = host.Tick({1,1}, input);
            Check(result.status == GameTickStatus::Advanced && host.failures == 0,
                "The original submarine level must advance without failed object handlers.");
            for (const auto& dispatch : result.objects.value())
            {
                Check(dispatch.status != DispatchStatus::UnimplementedHandler && dispatch.status != DispatchStatus::Failed,
                    "Every authored level-32 behavior must now have an implemented handler.");
            }
        }
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc != 2) { throw std::invalid_argument("Expected original assets directory."); }
        const std::filesystem::path assets(argv[1]);
        TestTextureNoise(assets);
        TestFish(assets);
        TestVehicle(assets);
        TestStressAndExit(assets);
        TestWindow(assets);
        TestHeldPole(assets);
        TestCoverage(assets);
        std::cout << "Submarine level 32 tests passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
