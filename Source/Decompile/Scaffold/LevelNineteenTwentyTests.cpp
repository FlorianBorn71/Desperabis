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

    struct Context : GameContext
    {
        int created = 0, destroyed = 0;
        void OnObjectCreated(int) override { ++created; }
        void OnObjectDestroyed(int) override { ++destroyed; }
    };

    struct Input final : InputHandler
    {
        bool IsKeyPressed(InputKey) override { return false; }
    };

    struct Host final : Context
    {
        std::filesystem::path assets;
        explicit Host(const std::filesystem::path& path) : assets(path) {}
    protected:
        std::vector<std::uint8_t> LoadFile(std::string_view name) override
        {
            std::ifstream file(assets / name, std::ios::binary);
            if (!file) { throw std::runtime_error("Missing level 19/20 fixture: " + std::string(name)); }
            return {std::istreambuf_iterator<char>(file), {}};
        }
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
        Context context;
        Fixture()
        {
            context.level = &level;
            context.collisionMap = level.collisionMap.get();
            context.soundListenerX = 1600;
            context.soundListenerZ = 2500;
            context.lightingCache.Reset(1500, 2500);
        }
        RawGameObject& Ghost()
        {
            auto& ghost = context.worldObjects[2];
            ghost = {};
            ghost.health = 1000; ghost.behavior = BehaviorType::Ghost;
            ghost.collisionType = CollisionType::Ghost;
            ghost.posX = 1500; ghost.posZ = 2500;
            ghost.modelId = 2; ghost.modelAnim = 1; ghost.animFrame = 1; ghost.modelFrame2 = 2;
            ghost.collisionRadiusXZ = 13; ghost.collisionRadiusY = 31;
            ghost.generalPurpose3 = 2; ghost.spriteScale = 64;
            return ghost;
        }
    };

    void TestDiggingAndBurial()
    {
        Fixture f;
        auto& ground = f.context.worldObjects[2];
        ground.health = 1000; ground.behavior = BehaviorType::DiggableGround;
        ground.posX = 1500; ground.posZ = 2500;
        ground.collisionRadiusXZ = ground.collisionRadiusY = 20;
        f.context.tickCount = 0;
        DispatchObjectCase(ground, f.context);
        Check(ground.collisionType == CollisionType::DiggableGround && ground.collisionMask == 131 &&
            ground.behaviorParameter == 21 && ground.blendWeightOrSpriteOpacity == 0,
            "Dig sites must initialize collision and default digging progress even at zero ticks.");
        auto& key = f.context.worldObjects[3];
        key.health = 1000; key.behavior = BehaviorType::Buried; key.collisionType = CollisionType::Key;
        key.posX = ground.posX; key.posZ = ground.posZ; key.collisionMask = 128;
        key.collisionRadiusXZ = key.collisionRadiusY = 10;
        key.displayType = DisplayType::VectorOilTint; key.itemValue = 2;
        key.pickupType = PickupType::Type_1;
        const auto initial = key;
        f.context.field_D500 = false;
        DispatchObjectCase(key, f.context);
        Check(std::memcmp(&key, &initial, sizeof(key)) == 0, "Burial must respect the native body-mode gate.");
        f.context.field_D500 = true;
        DispatchObjectCase(key, f.context);
        Check(key.displayType == DisplayType::Hidden && key.doorLockOrGroupBitmask == 22 && key.velocityY == 100,
            "Buried items must preserve their display mode and install the default reveal threshold.");
        f.context.selectedWeapon = WeaponType::Showel;
        ground.behaviorParameter = 23;
        f.context.DamageObject(ground, 4);
        Check(ground.itemValue == 0, "Other damage amounts must not advance digging.");
        for (int hit = 1; hit <= 5; ++hit)
        {
            f.context.DamageObject(ground, 5);
            Check(ground.itemValue == hit * 23 && f.context.weaponAnimationRecoil == 2,
                "Each shovel contact must accumulate authored dig progress and recoil.");
            f.context.tickCount = 6;
            DispatchObjectCase(ground, f.context);
            DispatchObjectCase(key, f.context);
            if (hit < 5)
            {
                Check(ground.blendWeightOrSpriteOpacity == hit * 23 && key.displayType == DisplayType::Hidden,
                    "The dig morph must approach accumulated progress without revealing the item too early.");
            }
        }
        Check(ground.behavior == BehaviorType::None && ground.blendWeightOrSpriteOpacity == 100 &&
            ground.collisionMask == 0 && key.behavior == BehaviorType::None &&
            key.displayType == DisplayType::VectorOilTint && key.collisionMask == 1 && key.doorLockOrGroupBitmask == 0,
            "Completed digging must finish the morph, remove the covering collision and restore the pickup.");
        HandlePlayerObjectInteraction(key, f.context);
        Check(key.health == 0 && std::any_of(f.context.inventory.begin(), f.context.inventory.end(),
            [](const RawGameObject& item) { return item.collisionType == CollisionType::Key && item.itemValue == 2; }),
            "A revealed key must be collectable through the normal inventory path.");
    }

    void TestBurialCollisionOrdering()
    {
        Fixture f;
        auto& ground = f.context.worldObjects[2];
        ground.health = 1000; ground.collisionType = CollisionType::DiggableGround;
        ground.collisionMask = 131; ground.collisionRadiusXZ = ground.collisionRadiusY = 20;
        auto& buried = f.context.worldObjects[3];
        buried.health = 1000; buried.behavior = BehaviorType::Buried;
        buried.collisionMask = 128; buried.collisionRadiusXZ = buried.collisionRadiusY = 10;
        buried.displayType = DisplayType::VectorNormal; buried.velocityY = 50;
        ground.blendWeightOrSpriteOpacity = 50;
        DispatchObjectCase(buried, f.context);
        Check(buried.collisionMask == 128 && buried.displayType == DisplayType::Hidden,
            "First hiding must override the threshold mask clear in the original statement order.");
        DispatchObjectCase(buried, f.context);
        Check(buried.collisionMask == 0 && buried.displayType == DisplayType::Hidden,
            "An already hidden item must clear its mask exactly at its reveal threshold.");
        DispatchObjectCase(buried, f.context);
        Check(buried.behavior == BehaviorType::None && buried.collisionMask == 1,
            "An item released by its own threshold must restore visibility on the following query.");
        buried.behavior = BehaviorType::Buried; buried.collisionMask = 128;
        ground.collisionType = CollisionType::None;
        const auto covered = buried;
        DispatchObjectCase(buried, f.context);
        Check(std::memcmp(&covered, &buried, sizeof(buried)) == 0,
            "Selecting an overlapping non-dig object must not hide or release a buried item.");
    }

    void TestShovelContact()
    {
        Fixture f;
        auto& context = f.context;
        auto& player = context.worldObjects[1];
        player.health = 1000; player.posX = 1445; player.posY = 30; player.posZ = 2500; player.velocityX = 100;
        auto& ground = context.worldObjects[2];
        ground.health = 1000; ground.behavior = BehaviorType::DiggableGround;
        ground.posX = 1500; ground.posZ = 2500;
        ground.collisionRadiusXZ = ground.collisionRadiusY = 20; ground.behaviorParameter = 23;
        DispatchObjectCase(ground, context);
        auto& held = context.inventory[1];
        held.health = 1000; held.collisionType = CollisionType::Weapon; held.itemValue = 8; held.generalPurpose1 = 100;
        context.selectedWeapon = context.requestedWeapon = WeaponType::Showel;
        context.weaponHand = context.requestedWeaponHand = 2; context.weaponReady = true;
        context.RefreshWeaponInventory();
        Check(context.AttackWithSelectedWeapon() == WeaponActionStatus::Applied, "An owned shovel must accept an attack.");
        context.UpdateWeaponAnimation(6);
        auto& contact = context.worldObjects[3];
        Check(contact.behavior == BehaviorType::WeaponDamageCollider && contact.behaviorParameter == 5,
            "The actual shovel swing must create the native damage-five contact.");
        DispatchObjectCase(contact, context);
        Check(ground.itemValue == 23 && context.weaponAnimationRecoil == 2 && contact.health == 0,
            "Shovel animation/contact collision must reach digging through the normal weapon damage path.");
    }

    void TestLeash()
    {
        Fixture f;
        auto& leash = f.context.worldObjects[2];
        leash.health = 1000; leash.behavior = BehaviorType::DogLeash; leash.itemValue = 3;
        leash.posX = 1500; leash.posZ = 2500;
        auto& dog = f.context.worldObjects[3];
        dog.health = 1000; dog.behavior = BehaviorType::Dog; dog.collisionType = CollisionType::Dog;
        dog.posX = 1551; dog.posZ = 2568;
        dog.velocityX = 1700; dog.velocityZ = 2700;
        f.context.tickCount = 0;
        DispatchObjectCase(leash, f.context);
        Check(leash.blendWeightOrSpriteOpacity == 85 && leash.winY == GetVectorAngle(51, 68) &&
            dog.generalPurpose3 == 0 && dog.velocityX == 1700,
            "A leash must measure Euclidean distance and leave a dog exactly 85 units away unconstrained.");
        dog.posX = 1552;
        f.context.tickCount = 30;
        DispatchObjectCase(leash, f.context);
        Check(dog.velocityX == 1680 && dog.velocityZ == 2680 && dog.generalPurpose3 == 12,
            "Beyond 85 units the leash must pull the dog's route endpoint by 20 once per update, not per tick.");
        dog.posX = 1700; dog.posZ = 2500;
        DispatchObjectCase(leash, f.context);
        Check(leash.blendWeightOrSpriteOpacity == 100, "Leash geometry must cap its length at 100.");
        dog.health = 1001;
        const auto suspended = leash;
        DispatchObjectCase(leash, f.context);
        Check(std::memcmp(&suspended, &leash, sizeof(leash)) == 0, "A dormant linked dog must suspend leash updates.");
        dog.health = 1000; dog.collisionType = CollisionType::None; dog.behavior = BehaviorType::None;
        DispatchObjectCase(leash, f.context);
        Check(leash.behavior == BehaviorType::None && leash.modelFrame2 == 3,
            "A dead dog must switch the leash frame and stop its behavior after the dog's pose finishes.");
        leash.itemValue = 166; leash.behavior = BehaviorType::DogLeash;
        bool rejected = false;
        try { DispatchObjectCase(leash, f.context); }
        catch (const std::out_of_range&) { rejected = true; }
        Check(rejected, "Invalid leash links must be reported explicitly.");
    }

    void TestBell()
    {
        struct Audio final : AudioHandler
        {
            int starts = 0;
            std::uint8_t volume = 0;
            std::uint16_t GetSampleCount() const override { return 129; }
            bool IsChannelPlaying(std::size_t) const override { return false; }
            void StopAllSamples() override {}
            void PlaySample(std::size_t, std::uint8_t, std::uint8_t loudness) override { ++starts; volume = loudness; }
        } audio;
        Fixture f;
        auto& bell = f.context.worldObjects[2];
        bell.health = 1000; bell.behavior = BehaviorType::PeriodicGlobalSound;
        bell.soundEffectId = 1; bell.itemValue = 2; bell.generalPurpose1 = 5;
        f.context.audioHandler = &audio; f.context.soundEffectsLoaded = true;
        f.context.soundEffects[0].sampleCount = 1; f.context.soundEffects[0].samples[0] = 42;
        f.context.tickCount = 3;
        DispatchObjectCase(bell, f.context);
        Check(bell.itemValue == 5 && audio.starts == 0, "Bell timing must use a strict greater-than test.");
        f.context.tickCount = 1;
        DispatchObjectCase(bell, f.context);
        Check(bell.itemValue == 0 && audio.starts == 1 && audio.volume == 255,
            "The bell must reset its counter and request full-volume sound regardless of position.");
        f.context.tickCount = 100;
        DispatchObjectCase(bell, f.context);
        Check(bell.itemValue == 0 && audio.starts == 2, "Batched bell ticks must discard overshoot and play at most once.");
        bell.soundEffectId = 0;
        const auto seed = f.context.randomSeed;
        DispatchObjectCase(bell, f.context);
        Check(audio.starts == 2 && f.context.randomSeed == seed, "The authored zero-effect bell must be a native no-op.");
        bell.itemValue = 32767; f.context.tickCount = 1;
        DispatchObjectCase(bell, f.context);
        Check(bell.itemValue == -32768, "Bell counters must retain signed-word wrapping.");
    }

    void TestPickupTriggers()
    {
        for (const auto collision : {CollisionType::Ammo, CollisionType::Weapon})
        {
            for (const bool remove : {false, true})
            {
                Fixture f;
                auto& pickup = f.context.worldObjects[2];
                pickup.health = 1000; pickup.behavior = BehaviorType::TriggerObject;
                pickup.collisionType = collision; pickup.itemValue = 3; pickup.generalPurpose3 = 6;
                f.context.weaponAmounts[3] = static_cast<std::int16_t>(remove ? 6 : 5);
                f.context.weaponInventorySlots[3] = static_cast<std::uint16_t>(remove ? 1 : 0);
                DispatchObjectCase(pickup, f.context);
                Check(remove ? pickup.health == 0 && f.context.destroyed == 1 :
                    pickup.health == 1000 && pickup.behavior == BehaviorType::None && f.context.destroyed == 0,
                    "Pickup triggers must remove owned weapons/threshold ammo or disarm themselves exactly once.");
                if (!remove)
                {
                    f.context.weaponAmounts[3] = 20; f.context.weaponInventorySlots[3] = 1;
                    DispatchObjectCase(pickup, f.context);
                    Check(pickup.health == 1000, "A retained pickup must not be rechecked after its trigger clears.");
                }
            }
        }
        Fixture invalid;
        auto& pickup = invalid.context.worldObjects[2];
        pickup.health = 1000; pickup.behavior = BehaviorType::TriggerObject; pickup.collisionType = CollisionType::Ammo;
        for (const auto index : std::array<std::int16_t, 2>{-1, 32})
        {
            pickup.itemValue = index;
            bool rejected = false;
            try { DispatchObjectCase(pickup, invalid.context); }
            catch (const std::out_of_range&) { rejected = true; }
            Check(rejected && pickup.health == 1000, "Invalid pickup indices must be reported without destroying the object.");
        }
    }

    void TestGhostBoundariesAndLead()
    {
        for (const auto distance : std::array<std::int16_t, 11>{0, 60, 61, 70, 71, 89, 90, 100, 101, 179, 180})
        {
            Fixture f;
            auto& ghost = f.Ghost();
            ghost.generalPurpose1 = 4; ghost.doorLockOrGroupBitmask = 7;
            f.context.soundListenerX = static_cast<std::int16_t>(1500 + distance);
            const auto life = f.context.playerLife;
            DispatchObjectCase(ghost, f.context);
            const bool visible = distance > 70 && distance < 180;
            const auto speed = distance <= 60 ? 4 : distance <= 100 ? 2 : 1;
            Check(ghost.generalPurpose3 == speed && ghost.doorLockOrGroupBitmask == (visible ? 8 : 6) &&
                f.context.playerLife == life - (visible && distance < 90 ? 4 * 29491 : 0),
                "Ghost speed, visibility and damage must preserve their distinct inclusive/exclusive distance boundaries.");
        }
        Fixture f;
        auto& ghost = f.Ghost();
        ghost.generalPurpose3 = 4;
        f.context.soundListenerX = 1500;
        f.context.worldObjects[1].velocityX = -3; f.context.worldObjects[1].velocityZ = 5;
        DispatchObjectCase(ghost, f.context);
        Check(ghost.velocityX == -2 && ghost.velocityZ == 2,
            "Ghost lead must use signed arithmetic half-steps rather than truncating negative motion toward zero.");
    }

    void TestGhostMovementAndDamage()
    {
        for (std::uint16_t mode = 0; mode <= 4; ++mode)
        {
            Fixture f;
            auto& ghost = f.Ghost();
            ghost.generalPurpose1 = mode;
            f.context.soundListenerZ = 2600; f.context.randomSeed = 1;
            DispatchObjectCase(ghost, f.context);
            const std::int16_t dx = mode == 2 ? -2 : mode >= 3 ? 0 : 2;
            const std::int16_t dz = mode == 1 ? -2 : mode >= 3 ? 0 : 2;
            Check(ghost.posX == 1500 + dx && ghost.posZ == 2500 + dz &&
                ghost.generalPurpose3 == 1 && ghost.winZ == 7 &&
                ghost.posY == (mode == 4 ? 0 : 32),
                "Ghost modes must chase/orbit/wait, use the previous speed and bob only outside frozen mode.");
        }
        Fixture f;
        auto& ghost = f.Ghost();
        ghost.generalPurpose1 = 4; ghost.doorLockOrGroupBitmask = 64;
        f.context.soundListenerX = 1580;
        const auto life = f.context.playerLife;
        DispatchObjectCase(ghost, f.context);
        Check(ghost.blendWeightOrSpriteOpacity == 8 && f.context.playerLife == life - 4 * 29491 &&
            ghost.generalPurpose3 == 2 && ghost.collisionMask == 0,
            "A visible ghost inside distance 90 must drain player life without requiring physical contact.");
        ghost.health = 1040;
        const auto sleeping = ghost;
        const auto seed = f.context.randomSeed;
        DispatchObjectCase(ghost, f.context);
        Check(std::memcmp(&sleeping, &ghost, sizeof(ghost)) == 0 && f.context.randomSeed == seed,
            "Dormant ghost groups must not animate, move, attack or consume RNG.");
        ghost.health = 1000; ghost.posX = 1500; ghost.posZ = 2500; ghost.generalPurpose1 = 0;
        f.context.SetCollisionMapPixelAtWorldXZ(1500, 2500, 56);
        DispatchObjectCase(ghost, f.context);
        Check(ghost.health == 0 && f.context.destroyed == 1, "A ghost inside a wall must use the native deletion path.");
    }

    void TestGhostLightDeath()
    {
        Fixture f;
        auto& ghost = f.Ghost();
        auto& light = f.context.worldObjects[3];
        light.health = 1000; light.behavior = BehaviorType::LightRayKillsGhost;
        light.collisionType = CollisionType::LightRayKillsGhost; light.collisionMask = 64;
        light.posX = 1500; light.posY = 32; light.posZ = 2500;
        light.collisionRadiusXZ = 5; light.collisionRadiusY = 30;
        DispatchObjectCase(light, f.context);
        Check(f.context.lightingCache.contributions[10][10] == 250,
            "The ghost-killing light must feed the existing generic lighting cache.");
        DispatchObjectCase(ghost, f.context);
        Check(ghost.itemValue == 20 && ghost.collisionMask == 0, "Light overlap must begin ghost death, not ordinary damage.");
        DispatchObjectCase(ghost, f.context);
        Check(ghost.itemValue == 21 && ghost.generalPurpose1 == 4 && ghost.generalPurpose2 == 28 &&
            ghost.displayType == DisplayType::Type3 && ghost.blendWeightOrSpriteOpacity == 100,
            "Ghost death must install the original frozen dissolve state.");
        for (int tick = 0; tick < 38; ++tick) { DispatchObjectCase(ghost, f.context); }
        Check(ghost.itemValue == 22 && ghost.blendWeightOrSpriteOpacity == -3,
            "The dissolve must use signed arithmetic shifts through the negative threshold.");
        Context randomReference;
        randomReference.randomSeed = f.context.randomSeed;
        const auto sourceY = ghost.posY;
        DispatchObjectCase(ghost, f.context);
        Check(ghost.itemValue == 23 && ghost.displayType == DisplayType::SpriteTransparent && f.context.created == 5,
            "The split must publish five separately tracked fading fragments.");
        for (int part = 0; part < 5; ++part)
        {
            const auto& fragment = f.context.worldObjects[4 + part];
            const auto lifetime = randomReference.Random(10) + 50;
            const auto direction = part == 4 ? 0 : randomReference.Random(2);
            Check(fragment.behavior == BehaviorType::SmokeFading &&
                fragment.animFrame == (part == 4 ? 4 : 3) &&
                fragment.velocityY == (part == 4 ? 0 : (part - 2) * 50 - 90) &&
                (part == 4 ? fragment.health == 62 : fragment.health == lifetime &&
                    fragment.spriteScale == 42 - part * 6 && fragment.winY == direction &&
                    fragment.posY == sourceY + (part - 2) * 5 + 11),
                "Ghost fragments must retain their native frames, vertical rates, lifetimes and sizes.");
        }
        Check(f.context.randomSeed == randomReference.randomSeed, "Ghost splitting must retain every native random draw, including the final overridden lifetime.");
        for (int tick = 0; tick < 26; ++tick) { DispatchObjectCase(ghost, f.context); }
        Check(ghost.health == 0 && f.context.destroyed == 1, "The original ghost must disappear after its final fade.");
    }

    void TestGhostBatchAndFallback()
    {
        Fixture batch, single;
        batch.Ghost(); single.Ghost();
        batch.context.tickCount = 100;
        DispatchObjectCase(batch.context.worldObjects[2], batch.context);
        for (int tick = 0; tick < 100; ++tick) { DispatchObjectCase(single.context.worldObjects[2], single.context); }
        Check(std::memcmp(batch.context.worldObjects.data(), single.context.worldObjects.data(),
            sizeof(batch.context.worldObjects)) == 0 && batch.context.randomSeed == single.context.randomSeed &&
            batch.context.playerLife == single.context.playerLife, "Ghost substeps must preserve RNG, movement and player damage.");
        single.context.tickCount = 0;
        const auto unchanged = single.context.worldObjects[2];
        DispatchObjectCase(single.context.worldObjects[2], single.context);
        Check(std::memcmp(&unchanged, &single.context.worldObjects[2], sizeof(unchanged)) == 0,
            "Zero ghost ticks must not perform an update.");
        Fixture full;
        auto source = full.Ghost();
        for (auto& object : full.context.worldObjects) { object.health = 1; }
        source.itemValue = 22;
        full.context.ReplaceWorldObject(165, source);
        DispatchObjectCase(full.context.worldObjects[165], full.context);
        const auto& replacement = full.context.worldObjects[165];
        Check(replacement.behavior == BehaviorType::SmokeFading && replacement.health == 62 &&
            replacement.animFrame == 4 && replacement.velocityY == 0 && replacement.velocityZ == 0,
            "A full-world split must preserve the native allocator/source alias rather than corrupting the replacement.");
    }

    void TestOriginalLevels(const std::filesystem::path& assets)
    {
        Host dig(assets);
        dig.StartLevel(19);
        auto& key = dig.worldObjects[86];
        auto& ground = dig.worldObjects[77];
        DispatchObjectCase(key, dig);
        Check(key.displayType == DisplayType::Hidden, "The authored key must initially be covered by its dig site.");
        dig.selectedWeapon = WeaponType::Showel;
        for (int hit = 0; hit < 5; ++hit)
        {
            dig.DamageObject(ground, 5);
            dig.tickCount = 6;
            DispatchObjectCase(ground, dig);
            DispatchObjectCase(key, dig);
        }
        Check(key.behavior == BehaviorType::None && key.displayType != DisplayType::Hidden && key.collisionMask == 1,
            "Five original shovel hits must reveal the level-19 key.");
        HandlePlayerObjectInteraction(key, dig);
        Check(key.health == 0, "The original revealed key must be pickable.");

        for (const auto level : std::array<std::int16_t, 2>{19, 20})
        {
            Host host(assets);
            host.StartLevel(level);
            Input input;
            for (int step = 0; step < 300; ++step)
            {
                if (level == 20 && step == 30) { host.ActivateObjectGroup(40); }
                const auto result = host.Tick({1,1}, input);
                Check(result.status == GameTickStatus::Advanced && result.objects, "Original level 19/20 gameplay must advance.");
                for (const auto& dispatch : *result.objects)
                {
                    if (dispatch.behavior == BehaviorType::Moon || dispatch.behavior == BehaviorType::UnderwaterColorAnim) { continue; }
                    if (dispatch.status == DispatchStatus::UnimplementedHandler)
                    {
                        throw std::runtime_error("Missing level " + std::to_string(level) + " behavior " +
                            std::to_string(static_cast<int>(dispatch.behavior)));
                    }
                }
            }
        }
    }
}

int main(int argc, char** argv)
{
    try
    {
        Check(argc == 2, "Usage: level-nineteen-twenty-tests <Original asset directory>");
        TestDiggingAndBurial();
        TestBurialCollisionOrdering();
        TestShovelContact();
        TestLeash();
        TestBell();
        TestPickupTriggers();
        TestGhostMovementAndDamage();
        TestGhostBoundariesAndLead();
        TestGhostLightDeath();
        TestGhostBatchAndFallback();
        TestOriginalLevels(argv[1]);
        std::cout << "Levels 19/20: digging, buried pickups, leash, bell, ghosts/light death and pickup triggers passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
