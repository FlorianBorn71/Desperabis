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
        int created = 0, destroyed = 0;
        explicit Host(const std::filesystem::path& path) : assets(path) {}
        void Load(std::int16_t number = 18)
        {
            StartLevel(number);
            Check(GetRuntimeState() == GameRuntimeState::Ready, "Original cave must load.");
        }
        void Isolate(std::size_t slot)
        {
            for (std::size_t i = 2; i < worldObjects.size(); ++i)
            {
                if (i != slot) { SetObjectHealth(worldObjects[i], 0); }
            }
        }
        void OnObjectCreated(int) override { ++created; }
        void OnObjectDestroyed(int) override { ++destroyed; }
    protected:
        std::vector<std::uint8_t> LoadFile(std::string_view name) override
        {
            std::ifstream file(assets / name, std::ios::binary);
            if (!file) { throw std::runtime_error("Missing fixture: " + std::string(name)); }
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

    std::uint16_t Next(std::uint32_t& seed, std::uint16_t range)
    {
        seed = seed * 0x08088405u + 1u;
        return static_cast<std::uint16_t>((seed >> 16) % range);
    }

    std::int16_t SineProduct(std::int16_t angle, int scale)
    {
        const auto product = static_cast<std::uint32_t>(
            static_cast<std::int64_t>(ClampedSineTable[angle & 511]) * scale);
        return WrapSignedWord(static_cast<std::int32_t>(product >> 16));
    }

    void TestOriginalAudit(const std::filesystem::path& assets)
    {
        Input input;
        for (const auto number : {17, 18})
        {
            Host host(assets);
            host.Load(static_cast<std::int16_t>(number));
            if (number == 18)
            {
                Check(host.worldObjects[11].health == 1010 && host.worldObjects[61].itemValue == 10,
                    "The authored anaconda must await its original group-10 trigger.");
                host.HandleActivationTriggerContact(host.worldObjects[61]);
                Check(host.worldObjects[11].health == 1000 && host.worldObjects[36].health == 1000,
                    "The original entrance trigger must activate the head and body.");
            }
            for (int step = 0; step < 300; ++step)
            {
                const auto result = host.Tick({1, 1}, input);
                Check(result.status == GameTickStatus::Advanced && result.objects, "Full cave gameplay must advance.");
                for (const auto& dispatch : *result.objects)
                {
                    if (dispatch.status == DispatchStatus::UnimplementedHandler)
                    {
                        throw std::runtime_error("Missing cave " + std::to_string(number) +
                            " behavior " + std::to_string(static_cast<int>(dispatch.behavior)));
                    }
                }
            }
        }
    }

    void TestContacts(const std::filesystem::path& assets)
    {
        Input input;
        for (const auto slot : {11, 35, 36})
        {
            Host host(assets);
            host.Load();
            host.Isolate(slot);
            auto& target = host.worldObjects[slot];
            target.health = 1000;
            target.collisionMask = 3;
            auto& player = host.worldObjects[1];
            player.posX = target.posX; player.posY = target.posY; player.posZ = target.posZ;
            const auto life = host.playerLife;
            Check(UpdatePlayerInteraction(host, input).status == InteractionStatus::Applied &&
                host.playerLife == life - 29491 * target.behaviorParameter && host.playerHitEffectTicks == 8,
                "Original bat, head and tail contacts must apply byte-52 damage without Enter.");
            host.DEBUG_noPlayerCollision = true;
            host.ApplyPlayerLifeDelta(0);
            const auto remaining = host.playerLife;
            UpdatePlayerInteraction(host, input);
            Check(host.playerLife == remaining, "Cave contacts must respect the player-damage cheat.");
            host.DEBUG_noPlayerCollision = false;
            target.behaviorParameter = 0;
            Check(UpdatePlayerInteraction(host, input).status == InteractionStatus::NoEffect &&
                host.playerLife == remaining, "Zero-damage cave contacts must remain harmless.");
        }
        Host host(assets);
        host.Load(17);
        host.Isolate(2);
        auto& shark = host.worldObjects[2];
        shark.modelAnim = 2;
        Function_Shark(shark, host);
        host.worldObjects[1].posX = shark.posX;
        host.worldObjects[1].posY = shark.posY;
        host.worldObjects[1].posZ = shark.posZ;
        const auto life = host.playerLife;
        Check(UpdatePlayerInteraction(host, input).status == InteractionStatus::Applied &&
            host.playerLife == life - 29491 * shark.behaviorParameter,
            "The original cave-17 shark must inflict its authored contact damage.");
    }

    void TestLift(const std::filesystem::path& assets)
    {
        Host host(assets);
        host.Load();
        host.Isolate(44);
        auto& lift = host.worldObjects[44];
        auto& player = host.worldObjects[1];
        player.posX = static_cast<std::int16_t>(lift.posX + 8);
        player.posZ = static_cast<std::int16_t>(lift.posZ - 8);
        player.posY = static_cast<std::int16_t>(lift.posY + 20);
        const auto initialY = lift.posY;
        host.tickCount = 3;
        Function_LiftUp(lift, host);
        Check(lift.posY == initialY + 3 && lift.velocityY == 1, "An occupied lift must rise one unit per behavior tick.");
        const auto x = player.posX, z = player.posZ;
        Input input;
        Check(host.Tick({0, 1}, input).status == GameTickStatus::Advanced, "Lift movement must run through gameplay.");
        Check(host.playerGrounded && player.posX == x - 1 && player.posZ == z + 1 &&
            host.GetPlayerCamera().position[1] == lift.posY + host.playerBodyHeight,
            "The lift must center and carry the player at the platform height without Enter.");
        player.posX = static_cast<std::int16_t>(lift.posX + 200);
        host.tickCount = 1;
        Function_LiftUp(lift, host);
        Check(lift.posY == initialY + 1 && lift.velocityY == -2, "An empty lift must descend twice as fast.");
        host.tickCount = 300;
        Function_LiftUp(lift, host);
        Check(lift.posY == host.GetTerrainFloorHeight(lift.posX, lift.posY, lift.posZ),
            "Descending lifts must stop at their terrain floor.");
        lift.health = 1001;
        const auto before = lift;
        Function_LiftUp(lift, host);
        Check(std::memcmp(&before, &lift, sizeof(lift)) == 0, "Dormant lifts must not move.");
    }

    void TestPhysicalContacts(const std::filesystem::path& assets)
    {
        Input input;
        Host host(assets);
        host.Load(17);
        host.Isolate(55);
        auto& gorilla = host.worldObjects[55];
        auto& player = host.worldObjects[1];
        gorilla.velocityX = 7; gorilla.velocityZ = -9;
        player.posX = static_cast<std::int16_t>(gorilla.posX + 5);
        player.posY = gorilla.posY;
        player.posZ = static_cast<std::int16_t>(gorilla.posZ - 5);
        const auto x = player.posX, z = player.posZ;
        Check(host.Tick({0, 3}, input).status == GameTickStatus::Advanced &&
            gorilla.velocityX == 0 && gorilla.velocityZ == 0 &&
            player.posX == x + 3 && player.posZ == z - 3,
            "The original gorilla contact must halt its horizontal velocity and repel the player per movement substep.");

        Host electric(assets);
        electric.Load();
        electric.Isolate(62);
        const auto& pedestal = electric.worldObjects[62];
        auto& touching = electric.worldObjects[1];
        touching.posX = pedestal.posX; touching.posY = pedestal.posY; touching.posZ = pedestal.posZ;
        const auto life = electric.playerLife;
        Check(electric.Tick({0, 3}, input).status == GameTickStatus::Advanced &&
            electric.playerLife == life - 3 * 29491 * pedestal.behaviorParameter &&
            electric.playerHitEffectTicks == 8,
            "Touching the original recharge pedestal must inflict native byte-52 shock damage every movement substep.");
        electric.DEBUG_noPlayerCollision = true;
        electric.Tick({0, 3}, input);
        Check(electric.playerLife == electric.playerLifeMaximum * 65536 && electric.playerHitEffectTicks == 0,
            "Collision-off must restore full life and ignore the pedestal shock.");
    }

    void TestRecharge(const std::filesystem::path& assets)
    {
        Host host(assets);
        host.Load();
        host.Isolate(62);
        auto& pedestal = host.worldObjects[62];
        host.selectedWeapon = WeaponType::Electricity;
        host.tickCount = 3;
        host.weaponAnimationSpeed = 1;
        host.soundListenerZ = pedestal.posZ;
        for (const int distance : {79, 80, 100, 120, 121})
        {
            host.soundListenerX = static_cast<std::int16_t>(pedestal.posX - distance);
            host.weaponAmounts[7] = 100;
            Function_RechargeElectricity(pedestal, host);
            Check(host.weaponAmounts[7] == (distance >= 80 && distance <= 120 ? 108 : 100),
                "Recharge must use the native inclusive 80..120 Manhattan-distance band.");
        }
        host.soundListenerX = static_cast<std::int16_t>(pedestal.posX - 100);
        host.weaponAmounts[7] = 360;
        Function_RechargeElectricity(pedestal, host);
        Check(host.weaponAmounts[7] == 360, "A full electricity weapon must not recharge.");
        host.weaponAmounts[7] = 359;
        Function_RechargeElectricity(pedestal, host);
        Check(host.weaponAmounts[7] == 367, "Native recharge tests the limit before adding; do not silently clamp.");
        host.selectedWeapon = WeaponType::Dagger;
        host.weaponAmounts[7] = 10;
        Function_RechargeElectricity(pedestal, host);
        Check(host.weaponAmounts[7] == 10, "The pedestal must only charge a selected electricity weapon.");

        host.selectedWeapon = WeaponType::Electricity;
        host.weaponAnimationSpeed = 0; host.weaponReady = true; host.weaponAmounts[7] = 100;
        host.frameTickCount = 3;
        auto& player = host.worldObjects[1];
        player.posX = host.soundListenerX; player.posY = pedestal.posY; player.posZ = pedestal.posZ;
        const auto creations = host.created;
        Function_RechargeElectricity(pedestal, host);
        Check(host.weaponAmounts[7] == 101 && host.created >= creations + 2,
            "Recharge must invoke the original beam attack and consume its normal firing cost.");
    }

    void TestFireJets(const std::filesystem::path& assets)
    {
        Host host(assets);
        host.Load();
        host.Isolate(45);
        auto& emitter = host.worldObjects[45];
        emitter.generalPurpose1 = 5; emitter.groupIndexOrLock = 5; emitter.itemValue = 7;
        host.tickCount = 6;
        const auto count = host.created;
        Function_FireExhauster(emitter, host);
        Check(emitter.generalPurpose1 == 7 && host.created == count + 3,
            "A fire jet must emit on odd active phases and reload its authored interval.");
        std::size_t firstShot = 0;
        for (std::size_t slot = 2; slot < host.worldObjects.size(); ++slot)
        {
            const auto& shot = host.worldObjects[slot];
            if (shot.health <= 0 || shot.behavior != BehaviorType::FlyingArrow) { continue; }
            if (firstShot == 0) { firstShot = slot; }
            Check(shot.itemValue == 7 && shot.modelId == 1 && shot.modelAnim == 5 &&
                shot.animFrame == 2 && shot.spriteScale == 5 && shot.behaviorParameter == 25 &&
                shot.velocityY == -1 && shot.health >= 22 && shot.health <= 26 &&
                shot.collisionRadiusXZ == 10 && shot.collisionRadiusY == 10 &&
                shot.posY >= host.GetTerrainFloorHeight(shot.posX, shot.posY, shot.posZ) + 8,
                "Fire jets must spawn native variant-7 damaging flame sprites.");
            std::int16_t dx, dz;
            GetClampedDirectionDisplacement(dx, dz, emitter.winY, emitter.behaviorParameter);
            Check(shot.velocityX == dx && shot.velocityZ == dz, "The emitter byte parameter is projectile speed.");
        }
        Check(firstShot != 0, "The active jet must produce projectiles.");
        auto& shot = host.worldObjects[firstShot];
        shot.health = 17; shot.spriteScale = 69; shot.generalPurpose2 = 6;
        auto seed = host.randomSeed;
        const auto growth = Next(seed, 3) + 1;
        const auto rise = Next(seed, 2) + 1;
        const auto y = shot.posY;
        host.tickCount = 1;
        Function_FlyingArrow(shot, host);
        Check(shot.posY == y - 1 && shot.spriteScale == 69 + growth && shot.velocityY == 0 &&
            shot.generalPurpose2 == rise - 1 && shot.blendWeightOrSpriteOpacity == 17 &&
            shot.animFrame == 1 && host.randomSeed == seed,
            "Flame growth, rising remainder, fading frame and RNG must follow the native per-tick recipe.");
        Function_FlyingArrow(shot, host);
        Check(shot.health == 0, "An oversized flame must expire through the lifecycle helper.");

        auto source = emitter;
        source.generalPurpose1 = 7;
        const auto contact = host.SpawnEmitterProjectile(source);
        auto& flame = host.worldObjects[contact];
        flame.velocityX = flame.velocityY = flame.velocityZ = 0;
        host.worldObjects[1].posX = flame.posX; host.worldObjects[1].posY = flame.posY;
        host.worldObjects[1].posZ = flame.posZ;
        const auto life = host.playerLife;
        Function_FlyingArrow(flame, host);
        Check(host.playerLife == life - 29491 * 25 && flame.behaviorParameter == 0,
            "A flame must apply damage once and consume its contact-damage byte.");
        Function_FlyingArrow(flame, host);
        Check(host.playerLife == life - 29491 * 25, "A persistent overlapping flame must not damage twice.");
    }

    void TestAnaconda(const std::filesystem::path& assets)
    {
        Host host(assets);
        host.Load();
        host.HandleActivationTriggerContact(host.worldObjects[61]);
        std::memset(host.collisionMap->data, 0, sizeof(host.collisionMap->data));
        auto& head = host.worldObjects[11];
        head.generalPurpose1 = 0;
        host.tickCount = 1;
        host.randomSeed = 1;
        auto seed = host.randomSeed;
        Check(Next(seed, 50) != 0, "The fixed anaconda test seed must avoid random steering.");
        host.soundListenerX = static_cast<std::int16_t>(head.posX - 300);
        host.soundListenerZ = head.posZ;
        const auto original = head;
        const auto floor = host.GetTerrainFloorHeight(head.posX, head.posY, head.posZ);
        const auto lift = static_cast<std::int32_t>(25 * Sine(original.winX)) >> 16;
        Function_AnancondaHead(head, host);
        Check(head.posX == original.posX && head.posZ == original.posZ + 2 &&
            head.winX == original.winX - 1 && head.posY == floor + std::abs(lift) &&
            head.doorLockOrGroupBitmask == 1 && head.velocityY == -1 && head.winY == 512 &&
            host.randomSeed == seed, "The authored head must crawl, pitch and rise using native integer trig.");
        host.DamageObject(head, 1);
        Check(head.generalPurpose1 == 10, "A nonlethal head hit must start the native recoil timer.");
        const auto recoilZ = head.posZ;
        Function_AnancondaHead(head, host);
        Check(head.generalPurpose1 == 9 && head.doorLockOrGroupBitmask == -3 && head.posZ == recoilZ - 6,
            "An injured anaconda must backpedal at three times its crawl velocity.");

        auto& tail = host.worldObjects[36];
        const auto blend = tail.blendWeightOrSpriteOpacity;
        const auto phase = static_cast<std::int16_t>((head.generalPurpose2 + 51) & 511);
        const auto angle = static_cast<std::int16_t>((GetVectorAngle(
            WrapSignedWord(head.posX - tail.posX), WrapSignedWord(head.posZ - tail.posZ)) +
            SineProduct(phase, 3 * tail.velocityX)) & 511);
        const auto expectedX = WrapSignedWord(head.posX - SineProduct(angle, 20));
        const auto expectedZ = WrapSignedWord(head.posZ + SineProduct(
            static_cast<std::int16_t>(angle + 128), 20));
        const auto expectedY = ArithmeticShiftRight(WrapSignedWord(head.posY +
            host.GetTerrainFloorHeight(expectedX, tail.posY, tail.posZ)), 1);
        Function_AnancondaTailSegment(tail, host);
        Check(tail.generalPurpose1 == 20 && tail.generalPurpose2 == ((head.generalPurpose2 + 51) & 511) &&
            tail.health == head.health && tail.winZ == 0 && tail.blendWeightOrSpriteOpacity == (blend + 4) % 200,
            "The first tail segment must follow the head at native spacing and phase.");
        Check(tail.posX == expectedX && tail.posY == expectedY && tail.posZ == expectedZ &&
            tail.winY == GetVectorAngle(WrapSignedWord(head.posX - expectedX), WrapSignedWord(head.posZ - expectedZ)),
            "Segment positions and yaw must use the native sine displacement and signed half-height average.");
        host.field_D500 = false;
        const auto frozen = tail;
        Function_AnancondaTailSegment(tail, host);
        Check(std::memcmp(&tail, &frozen, sizeof(tail)) == 0, "The native body-mode gate must freeze tail updates.");
        host.field_D500 = true;
        host.DamageObject(head, 300);
        Check(head.health == 2000 && head.behavior == BehaviorType::AnacondaHeadDying &&
            head.modelAnim == 3 && head.animFrame == 3, "A lethal head hit must enter the recovered death selector.");
        host.tickCount = 2;
        UpdateObjects(host);
        Check(head.collisionMask == 0 && tail.health == 2000 && tail.collisionMask == 0 &&
            host.worldObjects[60].health == 2000 && host.worldObjects[60].collisionMask == 0,
            "Head death must propagate through the entire authored chain and disable body contacts.");
        for (int step = 0; step < 300; ++step) { UpdateObjects(host); }
        Check(head.posY == host.GetTerrainFloorHeight(head.posX, head.posY, head.posZ) &&
            (head.winX == 0 || head.winX == 512), "The dead head must settle flat on the floor.");
    }

    void TestSaveContinuation(const std::filesystem::path& assets)
    {
        Host host(assets);
        host.Load();
        host.HandleActivationTriggerContact(host.worldObjects[61]);
        host.tickCount = 1;
        for (int step = 0; step < 20; ++step) { UpdateObjects(host); }
        const auto saved = host.CaptureSaveGame("Cave");
        Host restored(assets);
        restored.LoadGame(saved);
        Check(std::memcmp(host.worldObjects.data(), restored.worldObjects.data(), sizeof(host.worldObjects)) == 0,
            "Native SPL must preserve the cave chain, lift, fire-jet timers and spawned flames.");
        restored.randomSeed = host.randomSeed;
        restored.soundListenerX = host.soundListenerX;
        restored.soundListenerZ = host.soundListenerZ;
        restored.tickCount = host.tickCount;
        for (int step = 0; step < 20; ++step)
        {
            UpdateObjects(host);
            UpdateObjects(restored);
            Check(std::memcmp(host.worldObjects.data(), restored.worldObjects.data(), sizeof(host.worldObjects)) == 0 &&
                host.randomSeed == restored.randomSeed,
                "Cave objects must continue identically after native save restoration with the same RNG stream.");
        }
    }
}

int main(int argc, char** argv)
{
    try
    {
        Check(argc == 2, "Expected original asset directory.");
        TestOriginalAudit(argv[1]);
        TestContacts(argv[1]);
        TestPhysicalContacts(argv[1]);
        TestLift(argv[1]);
        TestRecharge(argv[1]);
        TestFireJets(argv[1]);
        TestAnaconda(argv[1]);
        TestSaveContinuation(argv[1]);
        std::cout << "Original cave 17/18 behaviors, contacts, lift, recharge, fire jets, anaconda and saves passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
