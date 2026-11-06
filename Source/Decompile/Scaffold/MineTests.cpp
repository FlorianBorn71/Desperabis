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
    void Check(bool condition, const char* message)
    {
        if (!condition) { throw std::runtime_error(message); }
    }

    bool Same(const RawGameObject& a, const RawGameObject& b)
    {
        return std::memcmp(&a, &b, sizeof(a)) == 0;
    }

    struct Input final : InputHandler
    {
        bool interaction = false;
        bool IsKeyPressed(InputKey key) override { return interaction && key == InputKey::InteractionKey; }
    };

    struct Host final : GameContext
    {
        std::filesystem::path assets;
        int destroyed = 0;
        explicit Host(const std::filesystem::path& path) : assets(path) {}
        void OnObjectDestroyed(int) override { ++destroyed; }
        std::vector<std::uint8_t> Read(std::string_view name)
        {
            std::ifstream file(assets / name, std::ios::binary);
            if (!file) { throw std::runtime_error("Missing mine fixture: " + std::string(name)); }
            return {std::istreambuf_iterator<char>(file), {}};
        }
    protected:
        std::vector<std::uint8_t> LoadFile(std::string_view name) override { return Read(name); }
        void LoadLevel(const LevelLoadRequest& request) override
        {
            LevelLoadData data;
            LoadLevelGeometry(request.levelNumber, data);
            LoadLevelObjects(request.levelNumber, data);
            InitializeLevelPlayerSpawn(data, {0,0,100});
            CompleteLevelLoad(request.id, std::move(data));
        }
    };

    struct Fixture
    {
        Level level = CreateDummyLevel();
        GameContext core;
        Fixture()
        {
            core.level = &level;
            core.collisionMap = level.collisionMap.get();
            core.worldObjects[1].posX = 4000;
            core.worldObjects[1].posZ = 4000;
            core.soundListenerX = 4000;
            core.soundListenerZ = 4000;
            core.worldObjects[2].health = 1000;
            core.worldObjects[2].posX = 1500;
            core.worldObjects[2].posZ = 2500;
        }
    };

    void TestRope()
    {
        Fixture f;
        auto& rope = f.core.worldObjects[2];
        rope.behavior = BehaviorType::RopeSwitch;
        rope.velocityX = 5;
        for (const int ticks : {0,1,3,200})
        {
            for (int angle = 0; angle < 512; ++angle)
            {
                rope.velocityZ = static_cast<std::int16_t>(angle);
                f.core.tickCount = static_cast<std::int16_t>(ticks);
                const int phase = (angle + 5 * ticks) & 511;
                const int value = static_cast<int>(static_cast<std::int64_t>(Sine(static_cast<std::int16_t>(phase))) * 50 >> 16) + 50;
                DispatchObjectCase(rope, f.core);
                Check(rope.velocityZ == phase && rope.blendWeightOrSpriteOpacity == value,
                    "Rope phase must use velocityX*ticks and the signed high word of clamped sine*50.");
                f.core.playerCollisionSlot = 2;
                DispatchObjectCase(rope, f.core);
                Check(rope.blendWeightOrSpriteOpacity == 0, "A contacted rope must override its visual bend to zero.");
                f.core.playerCollisionSlot = 0;
            }
        }
    }

    void TestFallingAndShutter()
    {
        Fixture f;
        auto& dirt = f.core.worldObjects[2];
        dirt.behavior = BehaviorType::DirtFromAbove;
        dirt.health = 1001;
        dirt.posY = 10;
        const auto initial = dirt;
        DispatchObjectCase(dirt, f.core);
        Check(Same(dirt, initial), "Latent dirt must wait at health 1001 until native activation.");
        f.core.DamageObject(dirt, 0);
        Check(dirt.health == 1000, "The existing damage/activation path must release dormant mine objects.");
        dirt.renderPlacementMode = 6;
        auto& target = f.core.worldObjects[3];
        target = dirt;
        target.behavior = BehaviorType::None;
        target.health = 1001;
        target.collisionType = CollisionType::None;
        target.collisionMask = dirt.collisionMask = 255;
        target.collisionRadiusXZ = dirt.collisionRadiusXZ = 20;
        target.collisionRadiusY = dirt.collisionRadiusY = 20;
        dirt.groupIndexOrLock = 0;
        f.core.tickCount = 0;
        const auto beforeZero = dirt;
        DispatchObjectCase(dirt, f.core);
        Check(Same(dirt, beforeZero), "Falling dirt must not step at zero behavior ticks.");
        f.core.tickCount = 1;
        auto reference = dirt;
        for (int tick = 0; tick < 100 && dirt.behavior != BehaviorType::None; ++tick)
        {
            reference.velocityY = WrapSignedWord(reference.velocityY + 1);
            reference.posY = WrapSignedWord(reference.posY - ArithmeticShiftRight(reference.velocityY, 1));
            if (reference.posY < 10) { reference.renderPlacementMode = 7; }
            reference.blendWeightOrSpriteOpacity = WrapSignedWord(reference.blendWeightOrSpriteOpacity +
                ArithmeticShiftRight(reference.velocityY, 2));
            if (reference.posY < 0)
            {
                reference.collisionMask = 0;
                reference.collisionType = CollisionType::None;
                reference.soundEffectId = 0;
                if (reference.blendWeightOrSpriteOpacity > 100)
                {
                    reference.blendWeightOrSpriteOpacity = 100;
                    reference.behavior = BehaviorType::None;
                }
                reference.posY = 0;
            }
            DispatchObjectCase(dirt, f.core);
            Check(Same(dirt, reference), "Every falling tick must retain native gravity, morph, placement and strict landing order.");
            if (tick == 0) { Check(target.health == 1000, "Dirt must activate overlapping objects before it loses collision on landing."); }
        }
        Check(dirt.behavior == BehaviorType::None && dirt.posY == 0 && dirt.blendWeightOrSpriteOpacity == 100,
            "Falling dirt must settle at the floor and finish only after morph exceeds 100.");

        auto& shutter = dirt;
        shutter = {};
        shutter.health = 1001;
        shutter.behavior = BehaviorType::DirtFromAboveShutter;
        shutter.itemValue = -128;
        const auto asleep = shutter;
        DispatchObjectCase(shutter, f.core);
        Check(Same(shutter, asleep), "Shutters must also wait for exact health-1000 activation.");
        shutter.health = 1000;
        auto expected = shutter;
        int frames = 0;
        while (shutter.behavior != BehaviorType::None && frames++ < 1000)
        {
            const auto sign = SignWord(WrapSignedWord(expected.itemValue - expected.winZ));
            expected.generalPurpose2 = static_cast<std::uint16_t>(sign);
            expected.velocityZ = WrapSignedWord(expected.velocityZ + sign);
            expected.winZ = WrapSignedWord(expected.winZ + ArithmeticShiftRight(expected.velocityZ, 1));
            if (SignWord(WrapSignedWord(expected.itemValue - expected.winZ)) != sign)
            {
                expected.velocityZ = ArithmeticShiftRight(expected.velocityZ, 1);
                if (expected.velocityZ == 0) { expected.behavior = BehaviorType::None; }
            }
            DispatchObjectCase(shutter, f.core);
            Check(Same(shutter, expected), "Shutter oscillation must halve signed velocity only when crossing its target.");
        }
        Check(shutter.winZ == -128 && shutter.velocityZ == 1 &&
            shutter.behavior == BehaviorType::DirtFromAboveShutter,
            "Native signed half-steps leave the settled shutter at its target with velocity one, not a forced zero.");
    }

    void TestSupportsAndDoor()
    {
        Fixture f;
        auto& slab = f.core.worldObjects[2];
        slab.behavior = BehaviorType::CeilingConcreteSlab;
        slab.itemValue = static_cast<std::int16_t>(BehaviorType::DirtFromAbove);
        slab.collisionMask = 255;
        slab.collisionRadiusXZ = slab.collisionRadiusY = 20;
        auto& stake = f.core.worldObjects[3];
        stake = slab;
        stake.behavior = BehaviorType::CeilingConcreteSlabStake;
        stake.collisionType = CollisionType::CeilingConcreteSlabStake;
        stake.groupIndexOrLock = 1;
        DispatchObjectCase(slab, f.core);
        Check(slab.behavior == BehaviorType::CeilingConcreteSlab, "A touching stake must hold its ceiling slab.");
        f.core.field_D500 = false;
        stake.groupIndexOrLock = -1;
        DispatchObjectCase(stake, f.core);
        f.core.SetObjectHealth(stake, 0);
        DispatchObjectCase(slab, f.core);
        Check(stake.behavior == BehaviorType::CeilingConcreteSlabStake && slab.behavior == BehaviorType::CeilingConcreteSlab,
            "The native body gate must freeze both support transitions.");
        f.core.field_D500 = true;
        f.core.SetObjectHealth(stake, 1000);
        DispatchObjectCase(stake, f.core);
        Check(stake.behavior == BehaviorType::DirtFromAbove, "A destroyed stake must switch to its authored falling behavior.");
        f.core.SetObjectHealth(stake, 0);
        DispatchObjectCase(slab, f.core);
        Check(slab.behavior == BehaviorType::DirtFromAbove, "Removing support must release the slab on its next dispatch.");

        auto& door = f.core.worldObjects[4];
        door.health = 1000;
        door.behavior = BehaviorType::BurriedVerticalDoor;
        door.generalPurpose3 = 2;
        DispatchObjectCase(door, f.core);
        Check(door.behavior == BehaviorType::BurriedVerticalDoor, "A buried exit must wait for linked behavior completion, not health.");
        slab.behavior = BehaviorType::None;
        DispatchObjectCase(door, f.core);
        Check(door.behavior == BehaviorType::Teleporter && door.collisionMask == 3 && door.generalPurpose3 == 100,
            "A cleared buried exit must become a fully armed teleporter.");
        door.behavior = BehaviorType::BurriedVerticalDoor;
        door.generalPurpose3 = 166;
        bool rejected = false;
        try { DispatchObjectCase(door, f.core); }
        catch (const std::out_of_range&) { rejected = true; }
        Check(rejected, "Invalid buried-door links must fail explicitly.");
    }

    void TestSlabAndBlades()
    {
        Fixture f;
        auto& slab = f.core.worldObjects[2];
        slab.behavior = BehaviorType::VerticalConcreteSlab;
        slab.generalPurpose1 = 32000;
        slab.winZ = 128;
        f.core.collisionmapUploadDirty = false;
        auto expected = slab;
        for (int frame = 0; frame < 100 && slab.behavior != BehaviorType::None; ++frame)
        {
            expected.generalPurpose2 = static_cast<std::uint16_t>(expected.generalPurpose2 + 100);
            if (expected.itemValue >= WrapSignedWord(expected.generalPurpose1))
            {
                expected.itemValue = WrapSignedWord(expected.generalPurpose1);
                expected.generalPurpose2 = 0;
            }
            expected.itemValue = WrapSignedWord(expected.itemValue + WrapSignedWord(expected.generalPurpose2));
            expected.winZ = WrapSignedWord(128 - expected.itemValue / 200);
            if (expected.winZ < 0) { expected.winZ = 0; expected.behavior = BehaviorType::None; }
            DispatchObjectCase(slab, f.core);
            Check(Same(slab, expected), "Toppling slab must preserve native acceleration, signed item accumulator and strict negative-angle endpoint.");
        }
        Check(slab.behavior == BehaviorType::None && f.core.collisionmapUploadDirty &&
            f.core.GetCollisionMapPixelAtWorldXZ(slab.posX, slab.posZ) == 2,
            "A toppled slab must replace its blocking map footprint and dirty the renderer upload.");

        auto& blade = slab;
        blade = {};
        blade.health = 1000; blade.behavior = BehaviorType::RotatingBlades;
        blade.collisionType = CollisionType::RotatingBlades;
        blade.collisionMask = 1; blade.collisionRadiusXZ = blade.collisionRadiusY = 20;
        blade.velocityZ = 11; blade.winZ = 506; blade.doorLockOrGroupBitmask = 1;
        auto& player = f.core.worldObjects[1];
        player = blade;
        f.core.soundListenerX = 0;
        f.core.soundListenerZ = 0;
        f.core.playerLifeMaximum = 1000;
        for (const int speed : {0,1,2,17,100,150})
        {
            f.core.DEBUG_noPlayerCollision = true;
            f.core.ApplyPlayerLifeDelta(0);
            f.core.DEBUG_noPlayerCollision = false;
            blade.generalPurpose1 = static_cast<std::uint16_t>(speed);
            const auto life = f.core.playerLife = 1000 * 65536;
            blade.winZ = 506;
            f.core.tickCount = 3;
            DispatchObjectCase(blade, f.core);
            const int clamped = std::clamp(speed, 1, 100);
            Check(blade.winZ == 27 && blade.behaviorParameter == clamped && blade.generalPurpose1 == 0 &&
                f.core.playerLife == std::max(0, life - 29491 * clamped * clamped) && f.core.playerHitEffectTicks == 8,
                "Blade motion/damage must use clamped distance delta squared, independent of behavior tick count.");
        }
        f.core.DEBUG_noPlayerCollision = true;
        f.core.playerLife = 1;
        DispatchObjectCase(blade, f.core);
        Check(f.core.playerLife == f.core.playerLifeMaximum * 65536, "Blade damage must respect collision-off health policy.");
        f.core.DEBUG_noPlayerCollision = false;
        f.core.playerLifeDeltaSuppressed = 1;
        const auto life = f.core.playerLife;
        DispatchObjectCase(blade, f.core);
        Check(f.core.playerLife == life, "Blade damage must use shared native life-delta suppression.");
        f.core.playerLifeDeltaSuppressed = 0;
        blade.behaviorParameter = 3;
        f.core.playerLife = 100 * 65536;
        Input input;
        const auto contactLife = f.core.playerLife;
        Check(UpdatePlayerInteraction(f.core, input).status == InteractionStatus::Applied &&
            f.core.playerLife == contactLife - 29491 * 3,
            "Player blade contact must also apply the native direct-contact damage byte without Enter.");
    }

    void TestOriginal(const std::filesystem::path& assets)
    {
        Host host(assets);
        host.StartLevel(23);
        const auto original = host.worldObjects;
        int recovered = 0;
        for (int slot = 2; slot <= 164; ++slot)
        {
            auto& object = host.worldObjects[slot];
            if (object.health <= 0) { continue; }
            const auto result = DispatchObjectCase(object, host);
            Check(result.status != DispatchStatus::UnimplementedHandler,
                "Every live original mine behavior must have an implementation.");
            switch (original[slot].behavior)
            {
            case BehaviorType::VerticalConcreteSlab: case BehaviorType::RopeSwitch: case BehaviorType::Lorry:
            case BehaviorType::BurriedVerticalDoor: case BehaviorType::DirtFromAbove:
            case BehaviorType::DirtFromAboveShutter: case BehaviorType::CeilingConcreteSlab:
            case BehaviorType::CeilingConcreteSlabStake: case BehaviorType::RotatingBlades: ++recovered; break;
            default: break;
            }
        }
        Check(recovered == 12, "All twelve original objects belonging to the nine recovered selectors must be tested.");

        host.worldObjects = original;
        auto& stake = host.worldObjects[22];
        auto& slab = host.worldObjects[21];
        DispatchObjectCase(slab, host);
        Check(slab.behavior == BehaviorType::CeilingConcreteSlab, "Original mine slab must initially rest on its authored stake.");
        host.DamageObject(stake, 2);
        DispatchObjectCase(stake, host);
        Check(stake.behavior == BehaviorType::DirtFromAbove, "Weapon damage must break the original stake through its strength field.");
        for (int tick = 0; tick < 200; ++tick)
        {
            DispatchObjectCase(stake, host);
            DispatchObjectCase(slab, host);
        }
        Check(stake.behavior == BehaviorType::None && slab.behavior == BehaviorType::None &&
            slab.blendWeightOrSpriteOpacity == 100, "Original stake collapse must release and settle its overhead slab.");

        host.worldObjects = original;
        host.DamageObject(host.worldObjects[16], 0);
        host.DamageObject(host.worldObjects[18], 0);
        for (int tick = 0; tick < 300; ++tick)
        {
            DispatchObjectCase(host.worldObjects[16], host);
            DispatchObjectCase(host.worldObjects[18], host);
        }
        Check(host.worldObjects[16].behavior == BehaviorType::None &&
            host.worldObjects[18].behavior == BehaviorType::DirtFromAboveShutter &&
            host.worldObjects[18].winZ == -128 && host.worldObjects[18].velocityZ == 1,
            "Original activated dirt and shutter must complete their falling/opening sequences.");

        host.worldObjects = original;
        host.selectedWeapon = WeaponType::Showel;
        for (int hit = 0; hit < 5; ++hit) { host.DamageObject(host.worldObjects[12], 5); }
        host.tickCount = 100;
        DispatchObjectCase(host.worldObjects[12], host);
        DispatchObjectCase(host.worldObjects[10], host);
        Check(host.worldObjects[12].behavior == BehaviorType::None &&
            host.worldObjects[10].behavior == BehaviorType::Teleporter && host.worldObjects[10].collisionMask == 3,
            "Digging the original exit must activate the linked buried teleporter.");
    }

    void TestCart(const std::filesystem::path& assets)
    {
        Host host(assets);
        host.StartLevel(23);
        const auto original = host.worldObjects;
        auto& cart = host.worldObjects[50];
        for (int slot = 2; slot <= 164; ++slot) { if (slot != 50) { host.SetObjectHealth(host.worldObjects[slot], 0); } }
        Check(HandlePlayerObjectInteraction(cart, host).status == InteractionStatus::Applied &&
            cart.doorLockOrGroupBitmask == 1 && host.controlledVehicleSlot == 50,
            "Enter must board the original cart and select its native vehicle slot.");
        Input input;
        const auto originalX = cart.posX;
        for (int tick = 0; tick < 10; ++tick)
        {
            Check(host.Tick({0,1}, input).status == GameTickStatus::Advanced, "Cart movement must run on player-frame ticks.");
            const auto camera = host.GetPlayerCamera();
            Check(camera.position[0] == cart.posX && camera.position[2] == cart.posZ &&
                camera.position[1] == cart.posY + host.playerBodyHeight,
                "The riding camera must follow the cart on all axes without normal terrain movement.");
        }
        Check(cart.posX != originalX && cart.generalPurpose1 == 600 && cart.itemValue == 4000,
            "Cart must accelerate by 60 fixed-point units per player tick toward its running speed.");
        const auto phase = cart.generalPurpose1;
        HandlePlayerObjectInteraction(cart, host);
        Check(cart.blendWeightOrSpriteOpacity == 100, "Enter in the moving cart must engage the brake.");
        host.Tick({0,1}, input);
        Check(cart.itemValue == 1500 && cart.generalPurpose1 == phase + 60 && cart.blendWeightOrSpriteOpacity == 90,
            "Brake state must select slower target speed and decay by ten per movement tick.");
        const auto snapshot = host.CaptureSaveGame("Mine cart");
        Host loaded(assets);
        loaded.LoadGame(snapshot);
        Check(loaded.controlledVehicleSlot == 50 && Same(loaded.worldObjects[50], cart),
            "A cart save must rebuild the selected vehicle without altering its saved state.");
        host.Tick({0,1}, input);
        loaded.Tick({0,1}, input);
        Check(Same(host.worldObjects[50], loaded.worldObjects[50]) &&
            host.GetPlayerCamera().position == loaded.GetPlayerCamera().position,
            "Reloaded cart movement must continue exactly, including fixed-point fractions.");
        host.Tick({0,5}, input);
        for (int tick = 0; tick < 5; ++tick) { loaded.Tick({0,1}, input); }
        Check(Same(host.worldObjects[50], loaded.worldObjects[50]),
            "Batched player ticks must preserve exact cart simulation independently of the object clock.");
        const auto still = cart;
        host.Tick({5,0}, input);
        Check(Same(still, cart), "Object behavior ticks must register the cart but never advance its movement.");

        auto& obstacle = host.worldObjects[51];
        obstacle = cart;
        obstacle.behavior = BehaviorType::None;
        obstacle.collisionType = CollisionType::DamageObj;
        obstacle.collisionMask = 32;
        obstacle.collisionRadiusXZ = obstacle.collisionRadiusY = 100;
        host.Tick({0,1}, input);
        Check(obstacle.behavior == BehaviorType::WalkableDrawBridge && obstacle.collisionMask == 0 &&
            cart.doorLockOrGroupBitmask == 1, "The cart must extinguish track fires through DamageObject(-1) without derailing.");
        obstacle = cart;
        obstacle.behavior = BehaviorType::None;
        obstacle.collisionType = CollisionType::PushPlayer;
        obstacle.collisionMask = 32;
        obstacle.collisionRadiusXZ = obstacle.collisionRadiusY = 100;
        const auto destroyed = host.destroyed;
        host.Tick({0,1}, input);
        Check(obstacle.health == 0 && host.destroyed == destroyed + 1 && cart.doorLockOrGroupBitmask == 2,
            "A track obstacle must enter the crash state and be removed through lifecycle hooks.");
        for (int tick = 0; tick < 1000 && cart.behavior != BehaviorType::None; ++tick) { host.Tick({0,1}, input); }
        Check(cart.behavior == BehaviorType::None && cart.collisionMask == 0 &&
            cart.collisionType == CollisionType::None && cart.animFrame == 3 && cart.winZ == 256,
            "A crashing cart must roll over, damp its speed and retire its collision/behavior.");
        host.Tick({0,1}, input);
        Check(host.controlledVehicleSlot == 0, "A retired cart must release normal player movement.");

        host.worldObjects = original;
        host.controlledVehicleSlot = 0;
        host.worldObjects[1].posX = cart.posX;
        host.worldObjects[1].posY = cart.posY;
        host.worldObjects[1].posZ = cart.posZ;
        host.worldObjects[1].collisionMask = 3;
        host.worldObjects[1].collisionRadiusXZ = host.worldObjects[1].collisionRadiusY = 15;
        input.interaction = false;
        UpdatePlayerInteraction(host, input);
        Check(cart.doorLockOrGroupBitmask == 0, "Touching a stationary cart must not board it automatically.");
        input.interaction = true;
        Check(UpdatePlayerInteraction(host, input).status == InteractionStatus::Applied &&
            host.controlledVehicleSlot == 50, "The ordinary player-contact/Enter route must board the authored cart.");
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc != 2) { throw std::runtime_error("Usage: mine-tests <original-assets>"); }
        TestRope();
        TestFallingAndShutter();
        TestSupportsAndDoor();
        TestSlabAndBlades();
        TestOriginal(argv[1]);
        TestCart(argv[1]);
        std::cout << "PASS: all original mine handlers, native arithmetic, support/dirt/exit chains, blades, cart ride/crash and save continuation.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
