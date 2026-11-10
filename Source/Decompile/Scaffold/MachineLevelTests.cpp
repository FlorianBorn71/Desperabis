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

    struct Input final : InputHandler
    {
        bool IsKeyPressed(InputKey) override { return false; }
    };

    struct Host final : GameContext
    {
        std::filesystem::path assets;
        int created = 0;
        int flashes = 0;
        explicit Host(const std::filesystem::path& path) : assets(path)
        {
            StartNewGame(2);
            StartLevel(31);
            Check(GetRuntimeState() == GameRuntimeState::Ready, "Original machinery level must load.");
        }
        void OnObjectCreated(int) override { ++created; }
        void OnLightFlash(const LightFlash&) override { ++flashes; }
        void SetEyeY(std::int16_t y)
        {
            worldObjects[1].posY = WrapSignedWord(y - worldObjects[1].collisionRadiusY - 2);
        }
    protected:
        std::vector<std::uint8_t> LoadFile(std::string_view name) override
        {
            std::ifstream file(assets / name, std::ios::binary);
            if (!file) { throw std::runtime_error("Missing machinery fixture: " + std::string(name)); }
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

    void TestRotatingSwitches(const std::filesystem::path& assets)
    {
        Host host(assets);
        for (const int slot : {29,78,108,112})
        {
            auto& object = host.worldObjects[slot];
            Check(object.behavior == BehaviorType::RotatingSwitch, "Original rotating-switch selector must stay 56.");
            Check(HandlePlayerObjectInteraction(object, host).status == InteractionStatus::Applied,
                "Original rotating switches must accept ordinary switch interaction.");
            host.tickCount = 1;
            DispatchObjectCase(object, host);
            Check(object.winZ == WrapSignedWord(object.generalPurpose1) &&
                object.blendWeightOrSpriteOpacity == object.winZ,
                "Switches must rotate around Z and publish the pre-clamp animation phase.");
            object.winZ = WrapSignedWord(object.velocityZ - 1);
            object.generalPurpose2 = 4;
            host.tickCount = 3;
            DispatchObjectCase(object, host);
            Check(object.winZ == object.velocityZ && object.blendWeightOrSpriteOpacity == object.velocityZ + 11 &&
                WrapSignedWord(object.generalPurpose2) == (object.behaviorParameter ? 0 : -WrapSignedWord(object.generalPurpose1)),
                "Switch endpoint clamping must retain native phase overshoot and latch/reversal behavior.");
            object.behaviorParameter = 0;
            object.winZ = 1;
            object.generalPurpose2 = static_cast<std::uint16_t>(-4);
            DispatchObjectCase(object, host);
            Check(object.winZ == 0 && object.generalPurpose2 == 0 && object.blendWeightOrSpriteOpacity == -11,
                "Returning switches stop at zero while preserving the original phase-write ordering.");
            host.tickCount = 0;
            DispatchObjectCase(object, host);
            Check(object.blendWeightOrSpriteOpacity == 0 &&
                HandlePlayerObjectInteraction(object, host).status == InteractionStatus::Applied,
                "A returned rotating switch must be usable again.");
        }
        Check(host.worldObjects[64].generalPurpose2 != 0 &&
            host.worldObjects[75].health == 1000 && host.worldObjects[96].generalPurpose2 != 0,
            "The original rotating switches must activate their machinery and exit-door groups.");
        host.tickCount = 34;
        DispatchObjectCase(host.worldObjects[96], host);
        Check(host.worldObjects[96].blendWeightOrSpriteOpacity == 100,
            "The original exit-door group must actually open, not just receive an activation flag.");
    }

    void TestMachinery(const std::filesystem::path& assets)
    {
        Host host(assets);
        for (const auto type : {BehaviorType::PaterNosterLift, BehaviorType::SqueezingMachine})
        {
            int count = 0;
            for (auto object : host.worldObjects)
            {
                if (object.behavior != type) { continue; }
                ++count;
                const auto dormant = object;
                host.tickCount = 7;
                DispatchObjectCase(object, host);
                Check(std::memcmp(&object, &dormant, sizeof(object)) == 0,
                    "Dormant oscillating machinery must not move or change collisions.");
                object.health = 1000;
                for (const int ticks : {0,1,7,30,127})
                {
                    const auto old = object;
                    host.tickCount = static_cast<std::int16_t>(ticks);
                    host.SetEyeY(1000);
                    DispatchObjectCase(object, host);
                    const auto phase = (old.velocityZ + old.velocityX * ticks) & 511;
                    const auto product = static_cast<std::uint32_t>(
                        static_cast<std::int64_t>(Sine(static_cast<std::int16_t>(phase))) * WrapSignedWord(old.generalPurpose1));
                    const auto coordinate = WrapSignedWord(old.velocityY + WrapSignedWord(product >> 16));
                    Check(object.velocityZ == phase &&
                        (type == BehaviorType::PaterNosterLift ? object.posY : object.posX) == coordinate &&
                        object.collisionType == CollisionType::WalkablePlanks,
                        "All original platforms and presses must follow the native signed-amplitude sine motion.");
                    host.SetEyeY(WrapSignedWord(object.posY + 20));
                    host.tickCount = 0;
                    DispatchObjectCase(object, host);
                    Check(object.collisionType == CollisionType::PushPlayer,
                        "The strict eye-height boundary must switch a platform into side-contact machinery.");
                    object.doorLockOrGroupBitmask = 1;
                    DispatchObjectCase(object, host);
                    Check(object.collisionRadiusXZ == 20, "Machine support mode one must narrow the collision radius.");
                }
            }
            Check(count == 8, "Level 31 must contain eight paternoster parts and eight squeezing parts.");
        }

        auto& platform = host.worldObjects[2];
        host.DamageObject(platform, 0);
        host.tickCount = 0;
        host.SetEyeY(WrapSignedWord(platform.posY + 45));
        DispatchObjectCase(platform, host);
        auto& player = host.worldObjects[1];
        player.posX = platform.posX; player.posY = platform.posY; player.posZ = platform.posZ;
        player.velocityX = player.velocityZ = 0;
        host.playerBodyHeight = 45;
        host.frameTickCount = 1;
        Input input;
        host.Tick({0,1}, input);
        Check(platform.itemValue == 7, "Existing player support must recognize a moving paternoster platform.");

        auto& press = host.worldObjects[66];
        for (int slot = 2; slot <= 164; ++slot)
        {
            if (slot != 66) { host.SetObjectHealth(host.worldObjects[slot], 0); }
        }
        player.posX = press.posX; player.posY = press.posY; player.posZ = press.posZ;
        press.collisionType = CollisionType::PushPlayer;
        press.behaviorParameter = 1;
        host.playerLife = 100 * 65536;
        UpdatePlayerInteraction(host, input);
        Check(host.playerLife == 0, "Squeezing-machine side contact must retain native lethal collision-nine semantics.");
    }

    void TestExplosion(const std::filesystem::path& assets)
    {
        Host host(assets);
        auto& machine = host.worldObjects[75];
        auto& gauge = host.worldObjects[107];
        host.ActivateObjectGroup(50);
        Check(machine.health == 1000 && gauge.health == 1000, "The original machine switch must activate machine and gauge.");
        host.tickCount = 200;
        DispatchObjectCase(gauge, host);
        Check(gauge.winZ == gauge.velocityZ, "Pressure gauge must approach its authored angle without overshooting.");
        host.tickCount = 211;
        DispatchObjectCase(machine, host);
        Check(machine.groupIndexOrLock == -1 && machine.behavior == BehaviorType::ExplodingMachine,
            "The native countdown expires now but explodes only on the following dispatch.");
        const auto created = host.created;
        host.tickCount = 1;
        DispatchObjectCase(machine, host);
        Check(machine.behavior == BehaviorType::ExplodedMachineFade &&
            machine.collisionType == CollisionType::None && machine.collisionMask == 0 &&
            machine.animFrame == 2 && machine.modelFrame2 == 1 && machine.blendWeightOrSpriteOpacity == 100 &&
            host.created == created + 20 && host.playerRenderTable == 16 && host.flashes == 1 &&
            host.worldObjects[82].displayType == DisplayType::LastVectorBurning,
            "Explosion must activate group 51, spawn twenty particles with lifecycle hooks, and start the wreck morph.");
        int sparks = 0, smoke = 0;
        for (auto& object : host.worldObjects)
        {
            if (object.behavior != BehaviorType::FlyingArrow || object.health <= 0) { continue; }
            Check(object.modelId == 1 && object.modelAnim == 5 && object.collisionMask == 1 &&
                object.collisionType == CollisionType::FlyingArrow &&
                object.displayType == DisplayType::SpriteTransparent &&
                object.posY >= host.GetTerrainFloorHeight(object.posX, object.posY, object.posZ) + 8,
                "Explosion particles must use the native emitter recipe and terrain clearance.");
            if (object.itemValue == 7)
            {
                ++sparks;
                Check(object.health >= 22 && object.health <= 26 && object.spriteScale == 5 &&
                    object.behaviorParameter == 25 && object.renderPlacementMode == 1,
                    "Selector-seven sparks must preserve their authored damage, scale and lifetime.");
            }
            else
            {
                ++smoke;
                Check(object.itemValue == 6 && object.health >= 25 && object.health <= 34 &&
                    object.spriteScale >= 40 && object.spriteScale <= 59 &&
                    object.behaviorParameter == 70 && object.renderPlacementMode == 3,
                    "Selector-eight smoke must preserve its distinct damage, scale and lifetime.");
            }
            Check(DispatchObjectCase(object, host).status == DispatchStatus::ExecutedCase,
                "Every spawned explosion particle must have a recovered continuation.");
        }
        Check(sparks > 0 && smoke > 0 && sparks + smoke == 20, "The original explosion must use both particle variants.");
        host.tickCount = 7;
        DispatchObjectCase(machine, host);
        Check(machine.blendWeightOrSpriteOpacity == 44, "Wreck morph must decay at eight units per tick.");
        DispatchObjectCase(machine, host);
        Check(machine.blendWeightOrSpriteOpacity == 0 && machine.health == 1000,
            "The wreck must remain as the final model frame, not disappear through a fabricated death.");
    }

    void TestDrainageAndFish(const std::filesystem::path& assets)
    {
        Host host(assets);
        auto& drain = host.worldObjects[101];
        host.tickCount = 10;
        DispatchObjectCase(drain, host);
        Check(drain.velocityZ == 430 && host.GetHeightSurfaceDepth(1) == 430,
            "Dormant drainage must restore its initial controller depth without draining.");
        host.ActivateObjectGroup(53);
        auto& fish = host.worldObjects[105];
        host.tickCount = 0;
        DispatchObjectCase(fish, host);
        Check(fish.health == 1000 && fish.behavior == BehaviorType::LargeFish && fish.posY <= -450,
            "Activated fish must stay twenty units below the water plane, including zero-tick updates.");
        Host stepped(assets);
        stepped.worldObjects[105] = fish;
        host.randomSeed = stepped.randomSeed = 7;
        host.tickCount = 19;
        DispatchObjectCase(fish, host);
        stepped.tickCount = 1;
        for (int i = 0; i < 19; ++i) { DispatchObjectCase(stepped.worldObjects[105], stepped); }
        Check(host.randomSeed == stepped.randomSeed &&
            std::memcmp(&fish, &stepped.worldObjects[105], sizeof(fish)) == 0,
            "Batched fish locomotion must preserve the shared native wandering helper and RNG sequence.");
        host.ActivateObjectGroup(52);
        for (int slot : {103,104})
        {
            auto& gate = host.worldObjects[slot];
            Check((host.level->surfaceFlags[40 - gate.posZ / 100][gate.posX / 100 - 1] & 31) == 1,
                "Original water gates must start with selector one.");
            DispatchObjectCase(gate, host);
            Check(gate.behavior == BehaviorType::None &&
                host.GetFloorHeight(gate.posX, gate.posY, gate.posZ) ==
                    host.GetTerrainFloorHeight(gate.posX, gate.posY, gate.posZ),
                "The two activated gates must remove their water selector and finish once.");
        }
        host.tickCount = 10;
        const auto revision = host.level->geometryRevision;
        for (int i = 0; i < 49; ++i) { DispatchObjectCase(drain, host); }
        Check(drain.velocityZ == 478 && host.GetHeightSurfaceDepth(1) == 478 &&
            host.level->geometryRevision > revision && host.GetFloorHeight(3950, -450, 3250) == -478,
            "The drain must lower the shared physical water plane from -430 to -478 without overshoot.");
        for (int slot : {105,106})
        {
            auto& object = host.worldObjects[slot];
            object.posX = 3985; object.posZ = 3250; object.posY = -445;
            object.health = 1000;
            host.tickCount = 0;
            DispatchObjectCase(object, host);
            Check(object.behavior == BehaviorType::StrandedFish && object.modelAnim == 2 &&
                object.collisionType == CollisionType::None && object.blendWeightOrSpriteOpacity == -100 &&
                object.itemValue == 30 && object.generalPurpose1 == 11 &&
                object.posY == host.GetTerrainFloorHeight(object.posX, object.posY, object.posZ) + 5,
                "Drained fish must transition to the native flopping animation at terrain plus five.");
            host.tickCount = 3;
            DispatchObjectCase(object, host);
            Check(object.collisionMask == 0 && object.winZ == 1 &&
                object.blendWeightOrSpriteOpacity >= -199 && object.blendWeightOrSpriteOpacity <= 199,
                "Stranded fish must be harmless and retain signed blend cycling.");
        }
        const auto save = host.CaptureSaveGame("Drained machinery");
        Host restored(assets);
        restored.LoadGame(save);
        restored.tickCount = 0;
        DispatchObjectCase(restored.worldObjects[101], restored);
        Check(restored.GetHeightSurfaceDepth(1) == 478 &&
            restored.worldObjects[105].behavior == BehaviorType::StrandedFish &&
            restored.worldObjects[103].behavior == BehaviorType::None,
            "Native saves must preserve drainage, removed water gates and stranded-fish continuation.");
        host.SetPaused(true);
        const auto world = host.worldObjects;
        const auto seed = host.randomSeed;
        Input input;
        host.Tick({30,30}, input);
        Check(host.randomSeed == seed && std::memcmp(world.data(), host.worldObjects.data(), sizeof(world)) == 0,
            "Paused machinery and fish must not move, drain or consume RNG.");
    }

    void TestCoverage(const std::filesystem::path& assets)
    {
        Host host(assets);
        for (auto& object : host.worldObjects)
        {
            if (object.health > 1000 && object.health < 2000) { host.DamageObject(object, 0); }
        }
        for (int frame = 0; frame < 60; ++frame)
        {
            host.tickCount = host.frameTickCount = frame % 3 == 0 ? 0 : 7;
            const auto results = UpdateObjects(host);
            for (std::size_t i = 0; i < results.size(); ++i)
            {
                if (results[i].status == DispatchStatus::UnimplementedHandler || results[i].status == DispatchStatus::Failed)
                {
                    throw std::runtime_error("Machinery-level handler failed at slot " + std::to_string(i + 2) +
                        ", behavior " + std::to_string(static_cast<int>(results[i].behavior)));
                }
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
        TestRotatingSwitches(assets);
        TestMachinery(assets);
        TestExplosion(assets);
        TestDrainageAndFish(assets);
        TestCoverage(assets);
        std::cout << "Machinery level 31 tests passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
