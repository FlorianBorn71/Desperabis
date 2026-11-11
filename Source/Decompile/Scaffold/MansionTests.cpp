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
        bool interact = false;
        bool IsKeyPressed(InputKey key) override { return interact && key == InputKey::InteractionKey; }
    };

    struct Host final : GameContext
    {
        std::filesystem::path assets;
        explicit Host(const std::filesystem::path& path) : assets(path)
        {
            StartNewGame(2);
            StartLevel(33);
            Check(GetRuntimeState() == GameRuntimeState::Ready, "Original mansion fixture must load.");
        }
        void Keep(std::initializer_list<int> slots)
        {
            for (int slot = 2; slot <= 165; ++slot)
            {
                if (std::find(slots.begin(), slots.end(), slot) == slots.end()) { SetObjectHealth(worldObjects[slot], 0); }
            }
        }
        void StandAt(std::int16_t x, std::int16_t y, std::int16_t z)
        {
            auto& player = worldObjects[1];
            player.posX = x;
            player.posY = WrapSignedWord(GetTerrainFloorHeight(x, y, z) +
                playerBodyHeight - player.collisionRadiusY - 2);
            player.posZ = z;
        }
    protected:
        std::vector<std::uint8_t> LoadFile(std::string_view name) override
        {
            std::ifstream file(assets / name, std::ios::binary);
            if (!file) { throw std::runtime_error("Missing mansion fixture: " + std::string(name)); }
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

    void TestBucket(const std::filesystem::path& assets)
    {
        Host host(assets);
        host.Keep({129, 139, 140});
        auto& key = host.worldObjects[129];
        auto& wheel = host.worldObjects[139];
        auto& rope = host.worldObjects[140];
        Check(key.behavior == BehaviorType::KeyInBucket && key.itemValue == 2 &&
            key.doorLockOrGroupBitmask == 140 && wheel.doorLockOrGroupBitmask == 140,
            "The original bucket key and wheel must share their authored rope link.");
        const auto savedKey = key;
        bool reachedWindow = false, missedWindow = false, reversed = false;
        Input input;
        for (int turn = 0; turn < 600; ++turn)
        {
            const auto oldRopeY = rope.posY;
            const auto oldTarget = wheel.generalPurpose1;
            const auto oldDelta = WrapSignedWord(wheel.generalPurpose3);
            const auto oldVelocity = wheel.velocityZ;
            Check(HandlePlayerObjectInteraction(wheel, host).status == InteractionStatus::Applied,
                "The original rope wheel must respond to interaction.");
            const auto step = GetStepToward(oldVelocity == 0 ? WrapSignedWord(oldTarget) : oldVelocity,
                WrapSignedWord(oldTarget), wheel.itemValue);
            Check(rope.posY == WrapSignedWord(oldRopeY + (step == 0 ? 0 : oldDelta)),
                "Each wheel increment must move the bucket by exactly its authored two-unit step.");
            reversed |= wheel.generalPurpose1 != oldTarget;
            Function_KeyInBucket(key, host);
            const auto spatial = host.GetObjectRenderState(129);
            const bool accessible = (spatial.posY >= 60 && spatial.posY <= 80) ||
                (spatial.posY >= -140 && spatial.posY <= -120) || (spatial.posY >= -340 && spatial.posY <= -320);
            Check(key.collisionMask == (accessible ? 1 : 0), "The key must only become reachable beside one of the three windows.");
            reachedWindow |= accessible;
            missedWindow |= !accessible;
            host.StandAt(savedKey.posX, spatial.posY, savedKey.posZ);
            const auto& player = host.worldObjects[1];
            const bool withinHeight = std::abs(player.posY - spatial.posY) <
                player.collisionRadiusY + key.collisionRadiusY;
            Check((host.FindPlayerCollision() == 129) == (accessible && withinHeight),
                "The moving key must be reachable from the window floor, not by walking outside into the bucket.");
            Check(key.posX == savedKey.posX && key.posY == savedKey.posY && key.posZ == savedKey.posZ,
                "Bucket movement must not overwrite native saved key coordinates.");
        }
        Check(reachedWindow && missedWindow && reversed, "The original bucket must travel past windows and reverse direction.");
        for (int turn = 0; turn < 300 && host.FindPlayerCollision() != 129; ++turn)
        {
            HandlePlayerObjectInteraction(wheel, host);
            Function_KeyInBucket(key, host);
            host.StandAt(savedKey.posX, host.GetObjectRenderState(129).posY, savedKey.posZ);
        }
        Check(host.FindPlayerCollision() == 129, "The bucket must return within reach of a window.");
        input.interact = true;
        Check(UpdatePlayerInteraction(host, input).status == InteractionStatus::Applied && key.health == 0,
            "The reachable key must be pickable from its bucket.");
    }

    void TestCoffinAndBlades(const std::filesystem::path& assets)
    {
        Host host(assets);
        host.Keep({32, 43});
        auto& coffin = host.worldObjects[32];
        auto& blade = host.worldObjects[43];
        Check(coffin.behavior == BehaviorType::PortableCoffin && blade.behavior == BehaviorType::CeilingBladesComingDown,
            "The original coffin and ceiling-blade records must load.");
        host.DamageObject(blade, 0);
        coffin.winX = 0;
        Function_PortableCoffin(coffin, host);
        Check(coffin.winX == 12, "A placed coffin must tip twelve angle units toward its upright pose per behavior call.");
        blade.posX = coffin.posX; blade.posY = coffin.posY; blade.posZ = coffin.posZ;
        blade.collisionMask = 3;
        Function_PortableCoffin(coffin, host);
        Check(host.windX == 1 && coffin.collisionMask == 4, "The coffin must latch the native blade-blocking signal.");
        Function_CeilingBladesComingDown(blade, host);
        Check(blade.behavior == BehaviorType::None, "The ceiling blades must stop permanently against the coffin.");
        Host falling(assets);
        falling.Keep({43});
        auto& fall = falling.worldObjects[43];
        falling.DamageObject(fall, 0);
        fall.posY = WrapSignedWord(falling.GetTerrainFloorHeight(fall.posX, fall.posY, fall.posZ) - 1);
        Function_CeilingBladesComingDown(fall, falling);
        Check(fall.behavior == BehaviorType::None && fall.behaviorParameter == 0 &&
            fall.collisionType == CollisionType::DamageObj, "Landed blades must stop hurting the player.");
        Host carried(assets);
        carried.Keep({32});
        carried.inventory.fill({});
        Check(HandlePlayerObjectInteraction(carried.worldObjects[32], carried).status == InteractionStatus::Applied,
            "The coffin must be portable.");
        Input input;
        for (int tick = 0; tick < 60; ++tick) { carried.Tick({1,1}, input); }
        Check(carried.GetWeaponView().has_value() &&
            carried.GetWeaponView()->object.posZ < 35 &&
            carried.AttackWithSelectedWeapon() == WeaponActionStatus::Applied,
            "The portable coffin must have a first-person pose and remain placeable.");
    }

    void TestKnightAndSideBlades(const std::filesystem::path& assets)
    {
        Host host(assets);
        auto& knight = host.worldObjects[83];
        const auto trigger = host.worldObjects[124];
        Check(knight.health == 1035 && trigger.collisionType == CollisionType::TriggerActivation &&
            trigger.itemValue == 35, "The knight must await its original group-35 floor trigger.");
        host.StandAt(trigger.posX, trigger.posY, trigger.posZ);
        Input input;
        const auto result = host.Tick({1,1}, input);
        Check(result.status == GameTickStatus::Advanced &&
            host.worldObjects[124].collisionType != CollisionType::TriggerActivation,
            "Walking over the authored floor trigger must activate the knight without pressing Enter.");
        int arrows = 0;
        for (const auto& object : host.worldObjects)
        {
            if (object.health > 0 && object.behavior == BehaviorType::FlyingArrow)
            {
                ++arrows;
                Check(object.modelId == 31 && object.behaviorParameter == 140,
                    "The knight must fire the original damaging crossbow projectile.");
            }
        }
        Check(arrows == 1 && knight.behavior == BehaviorType::None && knight.animFrame == 3,
            "Activating the knight must fire once and switch to its authored spent frame.");
        host.Tick({1,1}, input);
        Check(knight.behavior == BehaviorType::None, "The spent knight must not rearm on further player contact.");
        Host side(assets);
        side.Keep({78});
        auto& blade = side.worldObjects[78];
        side.DamageObject(blade, 0);
        const auto z = blade.posZ;
        const auto y = blade.posY;
        side.tickCount = 1;
        std::fill(std::begin(side.collisionMap->data), std::end(side.collisionMap->data), std::uint8_t{0});
        Function_BladesFromSide(blade, side);
        Check(blade.posZ < z && blade.posY >= y - 1 && blade.winX == 0 && blade.winZ == 0,
            "The original side blade must slide along its fixed-point direction without spinning.");
        std::fill(std::begin(side.collisionMap->data), std::end(side.collisionMap->data), std::uint8_t{8});
        Function_BladesFromSide(blade, side);
        Check(blade.health == 2000 && blade.collisionMask == 0 && blade.collisionType == CollisionType::None,
            "A side blade that strikes a wall must enter its harmless falling state.");
    }

    void TestStick(const std::filesystem::path& assets)
    {
        Host host(assets);
        host.Keep({39, 84, 119});
        host.inventory.fill({});
        Check(HandlePlayerObjectInteraction(host.worldObjects[39], host).status == InteractionStatus::Applied,
            "The original wooden stick must be pickable.");
        Input input;
        for (int tick = 0; tick < 60; ++tick) { host.Tick({1,1}, input); }
        auto& torch = host.worldObjects[119];
        auto& player = host.worldObjects[1];
        player.posX = torch.posX; player.posY = torch.posY; player.posZ = torch.posZ;
        Check(UpdatePlayerInteraction(host, input).status == InteractionStatus::Applied,
            "Touching the knight's torch must light the held stick without an interaction key.");
        const auto lit = host.GetWeaponView();
        Check(lit && lit->object.animFrame == 5 && lit->object.doorLockOrGroupBitmask == 21,
            "The first-person stick must use the original burning frame.");
        auto& oil = host.worldObjects[84];
        host.StandAt(oil.posX, oil.posY, oil.posZ);
        host.SetObjectHealth(torch, 0);
        Check(host.AttackWithSelectedWeapon() == WeaponActionStatus::Applied,
            "The burning stick must be deployable above the oil.");
        Check(!host.GetWeaponView(), "A dropped stick must immediately disappear from the player's hand.");
        bool found = false;
        for (auto& object : host.worldObjects)
        {
            if (object.health > 0 && object.behavior == BehaviorType::MetalPole)
            {
                Function_MetalPole(object, host);
                Check(object.animFrame == 1 && object.doorLockOrGroupBitmask == 20,
                    "Dropping the burning stick must extinguish it even on a dry floor.");
                const auto floor = object.posY;
                for (const int height : {-300, -100, 100, -500})
                {
                    player.posY = static_cast<std::int16_t>(height);
                    Function_MetalPole(object, host);
                    Check(object.posY == floor, "A dropped stick must remain on its own storey when the player changes height.");
                }
                found = true;
                break;
            }
        }
        Check(found && oil.displayType == DisplayType::LastVectorBurning,
            "The burning stick must ignite the sewer oil before its dropped flame goes out.");
        auto blocker = oil;
        blocker.behavior = BehaviorType::None;
        blocker.collisionType = CollisionType::None;
        host.ReplaceWorldObject(2, blocker);
        oil.displayType = DisplayType::VectorNormal;
        auto stick = host.inventory[host.weaponHand - 1];
        stick.health = 1000;
        stick.posX = oil.posX; stick.posY = oil.posY; stick.posZ = oil.posZ;
        stick.winX = 0; stick.animFrame = 5; stick.doorLockOrGroupBitmask = 21;
        Function_MetalPole(stick, host);
        Check(oil.displayType == DisplayType::LastVectorBurning,
            "An unrelated earlier collision must not prevent a burning stick from igniting overlapping oil.");
    }

    void TestCoverage(const std::filesystem::path& assets)
    {
        Host host(assets);
        Input input;
        const auto sky = host.skyEnabled;
        for (int tick = 0; tick < 30; ++tick)
        {
            const auto result = host.Tick({1,1}, input);
            for (const auto& dispatch : result.objects.value())
            {
                Check(dispatch.status != DispatchStatus::UnimplementedHandler && dispatch.status != DispatchStatus::Failed,
                    "Every authored mansion behavior must now be recovered.");
            }
        }
        Check(host.skyEnabled == sky, "DistanceSkyToggle must leave the modern sky enabled behind occluding walls.");
        for (auto& object : host.worldObjects)
        {
            if (object.health > 1000 && object.health < 2000) { host.DamageObject(object, 0); }
        }
        for (int tick = 0; tick < 30; ++tick)
        {
            const auto result = host.Tick({1,1}, input);
            for (const auto& dispatch : result.objects.value())
            {
                Check(dispatch.status != DispatchStatus::UnimplementedHandler && dispatch.status != DispatchStatus::Failed,
                    "Activated mansion traps and their spawned objects must also have implemented handlers.");
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
        TestBucket(assets);
        TestCoffinAndBlades(assets);
        TestKnightAndSideBlades(assets);
        TestStick(assets);
        TestCoverage(assets);
        std::cout << "Mansion level 33 tests passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
