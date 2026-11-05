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

    struct Input final : InputHandler
    {
        bool IsKeyPressed(InputKey) override { return false; }
    };

    struct Host final : GameContext
    {
        std::filesystem::path assets;
        explicit Host(const std::filesystem::path& path) : assets(path) {}
    protected:
        std::vector<std::uint8_t> LoadFile(std::string_view name) override
        {
            std::ifstream file(assets / name, std::ios::binary);
            if (!file) { throw std::runtime_error("Missing sewer fixture: " + std::string(name)); }
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
        GameContext context;
        Fixture()
        {
            context.level = &level;
            context.collisionMap = level.collisionMap.get();
            context.soundListenerX = 1800;
            context.soundListenerZ = 2500;
        }
        RawGameObject& Dog()
        {
            auto& dog = context.worldObjects[2];
            dog = {};
            dog.health = 1000; dog.behavior = BehaviorType::Dog; dog.collisionType = CollisionType::Dog;
            dog.posX = dog.velocityX = 1500; dog.posZ = dog.velocityZ = 2500;
            dog.itemValue = 1; dog.groupIndexOrLock = 90; dog.doorLockOrGroupBitmask = 5;
            dog.behaviorParameter = 5;
            return dog;
        }
        RawGameObject& Scorpion()
        {
            auto& scorpion = context.worldObjects[2];
            scorpion = {};
            scorpion.health = 1000; scorpion.behavior = BehaviorType::Scorpion;
            scorpion.collisionType = CollisionType::SpiderLarge; scorpion.collisionMask = 3;
            scorpion.modelId = 36; scorpion.modelAnim = 1; scorpion.animFrame = 1; scorpion.modelFrame2 = 2;
            scorpion.posX = 1500; scorpion.posZ = 2500; scorpion.winY = 128;
            scorpion.collisionRadiusXZ = 20; scorpion.collisionRadiusY = 10;
            scorpion.generalPurpose2 = 65535; scorpion.doorLockOrGroupBitmask = -1;
            scorpion.groupIndexOrLock = 110; scorpion.behaviorParameter = 10; scorpion.soundEffectId = 1;
            context.randomSeed = 1;
            return scorpion;
        }
        RawGameObject& Head()
        {
            auto& head = context.worldObjects[2];
            head = {};
            head.health = 1000; head.behavior = BehaviorType::KrakenHead; head.collisionType = CollisionType::KrakenHead;
            head.velocityX = 15; head.velocityZ = 16; head.velocityY = 16;
            head.posX = 1550; head.posZ = 2550; head.itemValue = 6;
            head.generalPurpose1 = 300; head.generalPurpose3 = 7; head.groupIndexOrLock = 150;
            head.behaviorParameter = 11;
            context.soundListenerX = 1650; context.soundListenerZ = 2550;
            return head;
        }
        RawGameObject& Arm()
        {
            auto& arm = context.worldObjects[3];
            arm = {};
            arm.health = 1000; arm.behavior = BehaviorType::AttachableDamageObject;
            arm.collisionType = CollisionType::AttachableDamageObject; arm.doorLockOrGroupBitmask = 2;
            arm.itemValue = 8; arm.groupIndexOrLock = 10; arm.generalPurpose3 = 1;
            arm.animFrame = 1; arm.modelFrame2 = 4;
            return arm;
        }
    };

    void TestDogs()
    {
        Fixture f;
        auto& dog = f.Dog();
        Function_Dog(dog, f.context);
        Check(dog.posY == 16 && dog.animFrame == 2 && dog.modelFrame2 == 1 &&
            dog.blendWeightOrSpriteOpacity == 5 && dog.doorLockOrGroupBitmask == 7 &&
            dog.winY == GetVectorAngle(300,0), "Idle dog must face the player and accelerate its idle blend.");
        f.context.soundListenerX = 1689;
        Function_Dog(dog, f.context);
        Check(dog.itemValue == -2, "A dog must wake strictly inside distance 190.");
        Function_Dog(dog, f.context);
        Check(dog.itemValue == 2 && dog.modelAnim == 2 && dog.animFrame == 1 &&
            dog.modelFrame2 == 2 && dog.blendWeightOrSpriteOpacity == 0,
            "The alert transition must install the original running animation.");
        const auto before = dog.posX;
        Function_Dog(dog, f.context);
        Check(dog.posX > before && dog.velocityX == 1675 && dog.posZ == 2500,
            "Chasing must plan a 25-unit axis route and move toward it.");

        dog = f.Dog(); dog.itemValue = 2;
        std::fill(std::begin(f.context.collisionMap->data), std::end(f.context.collisionMap->data), std::uint8_t{56});
        Function_Dog(dog, f.context);
        Check(dog.posX == 1500 && dog.posZ == 2500 && dog.velocityX == 1500,
            "Blocked collision-map samples must stop dog route planning.");
        std::fill(std::begin(f.context.collisionMap->data), std::end(f.context.collisionMap->data), std::uint8_t{0});
        dog = f.Dog(); dog.itemValue = 2; f.context.soundListenerX = 1589;
        Function_Dog(dog, f.context);
        Check(dog.itemValue == 3 && dog.generalPurpose2 == 5, "A nearby running dog must enter its leap.");
        f.context.soundListenerX = static_cast<std::int16_t>(dog.posX + 59);
        const auto life = f.context.playerLife;
        Function_Dog(dog, f.context);
        Check(f.context.playerLife == life - 5 * 29491 && dog.generalPurpose1 == 2 &&
            dog.generalPurpose2 == 4 && dog.posY == 18 && f.context.playerHitEffectTicks == 8,
            "Leaping dogs must apply fractional player damage and the original vertical arc.");
        f.context.tickCount = 11;
        f.context.soundListenerX = static_cast<std::int16_t>(dog.posX + 100);
        Function_Dog(dog, f.context);
        Check(dog.itemValue == 2, "A dog must return to running after the leap height becomes negative.");
        f.context.DamageObject(dog, 100);
        Check(dog.itemValue == 100 && dog.collisionMask == 0 && dog.modelFrame2 == 4,
            "A killed dog must retain a separate noncolliding corpse transition.");
        f.context.tickCount = 30;
        Function_Dog(dog, f.context);
        Check(dog.behavior == BehaviorType::None && dog.blendWeightOrSpriteOpacity == 100 &&
            dog.health == 1000, "A dead dog must finish its death pose and remain in the world.");
    }

    void TestDogBatchAndBones()
    {
        Fixture a, b;
        auto& first = a.Dog();
        auto& second = b.Dog();
        first.itemValue = second.itemValue = 2;
        a.context.tickCount = 100;
        Function_Dog(first, a.context);
        for (int step = 0; step < 100; ++step) { Function_Dog(second, b.context); }
        Check(std::memcmp(&first, &second, sizeof(first)) == 0 &&
            a.context.randomSeed == b.context.randomSeed && a.context.playerLife == b.context.playerLife,
            "Dog frame batching must preserve records, RNG and damage.");
        b.context.tickCount = 0;
        const auto unchanged = second;
        Function_Dog(second, b.context);
        Check(std::memcmp(&unchanged, &second, sizeof(second)) == 0, "Zero dog ticks must do no work.");

        Fixture f;
        auto& dog = f.Dog();
        dog.collisionMask = 3; dog.collisionRadiusXZ = 20; dog.collisionRadiusY = 20;
        auto& bones = f.context.worldObjects[3];
        bones.health = 1000; bones.behavior = BehaviorType::BonesToGrab;
        bones.posX = dog.posX; bones.posZ = dog.posZ;
        bones.collisionRadiusXZ = bones.collisionRadiusY = 10;
        Function_BonesToGrab(bones, f.context);
        Check(dog.behavior == BehaviorType::IdleAnimation && dog.modelAnim == 4 &&
            dog.itemValue == 11 && dog.collisionMask == 0 &&
            bones.behavior == BehaviorType::None && bones.renderPlacementMode == 6 && bones.collisionMask == 0,
            "Thrown bones must pacify only the dog actually touching them.");
    }

    void TestHeadNavigation()
    {
        Fixture f;
        auto& head = f.Head();
        Function_KrakenHead(head, f.context);
        Check(head.generalPurpose1 == 1500 && head.velocityX == 16 && head.velocityZ == 16 &&
            head.generalPurpose2 >= 4 && head.generalPurpose2 <= 6 &&
            head.posY >= 16 && head.posY <= 21 && head.blendWeightOrSpriteOpacity == 5,
            "An awakened kraken must choose the preferred clear neighboring tile and animate.");
        const auto before = head.posX;
        Function_KrakenHead(head, f.context);
        Check(head.posX > before && head.posX <= before + 7, "Kraken motion must approach its tile center.");
        head = f.Head();
        f.level.surfaceFlags[15][15] = 2;
        Function_KrakenHead(head, f.context);
        Check(head.velocityX == 15 && head.velocityZ == 15,
            "Blocked preferred tiles must use the native cardinal fallback order.");

        head = f.Head();
        f.context.soundListenerX = 1850;
        const auto unchanged = head;
        Function_KrakenHead(head, f.context);
        Check(std::memcmp(&head, &unchanged, sizeof(head)) == 0,
            "Kraken wake distance must use a strict less-than gate.");
        f.context.soundListenerX = 1599;
        const auto life = f.context.playerLife;
        Function_KrakenHead(head, f.context);
        Check(f.context.playerLife == life - 11 * 29491, "A kraken head must damage the player inside distance 50.");

        head = f.Head();
        for (auto& row : f.level.surfaceFlags) { std::fill(std::begin(row), std::end(row), std::uint8_t{2}); }
        bool rejected = false;
        try { Function_KrakenHead(head, f.context); }
        catch (const std::runtime_error&) { rejected = true; }
        Check(rejected, "An enclosed kraken must report the native infinite-loop condition instead of hanging.");
    }

    void TestAttachmentModes()
    {
        for (int mode = 0; mode < 4; ++mode)
        {
            Fixture f;
            auto& head = f.Head();
            auto& arm = f.Arm();
            head.winY = 30;
            arm.velocityY = 73; arm.generalPurpose1 = static_cast<std::uint16_t>(mode);
            arm.generalPurpose2 = 6; arm.generalPurpose3 = 2;
            f.context.tickCount = 3;
            std::int16_t x, z;
            GetDirectionDisplacement(x, z, 30 + 73 - 128, 8);
            Function_AttachableDamageObject(arm, f.context);
            Check(arm.winY == 103 && arm.posX == head.posX + x && arm.posY == head.posY &&
                arm.posZ == head.posZ + z, "Every live attached mode must follow its parent's world-space pose.");
            if (mode == 0) { Check(arm.blendWeightOrSpriteOpacity >= 21, "Tentacle blend must use its random speed."); }
            if (mode == 1) { Check(arm.winX == 6, "Attached blades must rotate pitch rather than act like tentacles."); }
            if (mode == 2) { Check(arm.blendWeightOrSpriteOpacity <= 27, "Variable-speed attached animation must stay bounded."); }
            if (mode == 3) { Check(arm.velocityY == 79 && arm.winY == 103, "Orbital phase must advance after placement."); }
        }
        Fixture f;
        auto& head = f.Head();
        auto& arm = f.Arm();
        head.health = 1007; head.displayType = DisplayType::VectorOilTint;
        Function_AttachableDamageObject(arm, f.context);
        Check(arm.health == 1007 && arm.displayType == head.displayType,
            "Dormant arms must copy the parent's dormant health and display mode.");
        head.health = 1000;
        Function_AttachableDamageObject(arm, f.context);
        Check(arm.health == 1000, "Dormant arms must wake with their parent.");
        arm.generalPurpose1 = 1;
        arm.collisionMask = 3; arm.collisionRadiusXZ = arm.collisionRadiusY = 30;
        auto& target = f.context.worldObjects[4];
        target.health = 1000; target.collisionType = CollisionType::Dog;
        target.behavior = BehaviorType::Dog; target.groupIndexOrLock = 90;
        target.posX = arm.posX; target.posY = arm.posY; target.posZ = arm.posZ;
        target.collisionMask = 3; target.collisionRadiusXZ = target.collisionRadiusY = 20;
        Function_AttachableDamageObject(arm, f.context);
        Check(target.itemValue == 100, "The reusable attached-blade mode must damage other colliding objects.");
    }

    void TestScorpions()
    {
        Fixture f;
        auto& scorpion = f.Scorpion();
        scorpion.velocityY = 20;
        scorpion.health = 1001;
        const auto sleeping = scorpion;
        DispatchObjectCase(scorpion, f.context);
        Check(std::memcmp(&sleeping, &scorpion, sizeof(scorpion)) == 0 && f.context.randomSeed == 1,
            "Dormant scorpions must not move or consume random numbers.");
        scorpion.health = 1000;
        const auto result = DispatchObjectCase(scorpion, f.context);
        std::int16_t vx, vz;
        GetClampedDirectionDisplacement(vx, vz, WrapSignedWord(scorpion.generalPurpose2), 2);
        Check(result.status == DispatchStatus::ExecutedCase && result.behavior == BehaviorType::Scorpion &&
            scorpion.posY == 0 && scorpion.velocityX == vx && scorpion.velocityZ == vz,
            "Scorpions must use fixed speed two and zero terrain offset, not the spider's velocityY/height.");
        scorpion.velocityX = 2; scorpion.velocityZ = 1;
        const auto oldX = scorpion.posX, oldZ = scorpion.posZ;
        f.context.field_D500 = false;
        DispatchObjectCase(scorpion, f.context);
        Check(scorpion.posX == oldX && scorpion.posZ == oldZ,
            "The native body-mode gate must suppress scorpion translation.");
        f.context.field_D500 = true;
        std::fill(std::begin(f.context.collisionMap->data), std::end(f.context.collisionMap->data), std::uint8_t{1});
        const auto materialDx = scorpion.velocityX, materialDz = scorpion.velocityZ;
        DispatchObjectCase(scorpion, f.context);
        Check(scorpion.posX == oldX + materialDx && scorpion.posZ == oldZ + materialDz,
            "Scorpion wall probes must ignore the low collision-map material bits.");
        scorpion = f.Scorpion();
        std::fill(std::begin(f.context.collisionMap->data), std::end(f.context.collisionMap->data), std::uint8_t{0});
        f.context.SetCollisionMapPixelAtWorldXZ(1525, 2500, 56);
        f.context.discardBlockedObjects = true;
        DispatchObjectCase(scorpion, f.context);
        Check(scorpion.health == 0, "Enclosed scorpions must use the existing blocked-object lifecycle response.");

        Fixture contact;
        auto& enemy = contact.Scorpion();
        auto player = enemy;
        player.collisionMask = 255;
        contact.context.ReplaceWorldObject(1, player);
        contact.context.soundListenerX = static_cast<std::int16_t>(enemy.posX + 1);
        enemy.velocityX = 4; enemy.velocityZ = 5;
        const auto life = contact.context.playerLife;
        Input input;
        Check(UpdatePlayerInteraction(contact.context, input).status == InteractionStatus::Applied &&
            contact.context.playerLife == life - 10 * 29491 && contact.context.playerHitEffectTicks == 8 &&
            enemy.velocityX == 0 && enemy.velocityZ == 0 && WrapSignedWord(enemy.generalPurpose2) < 0,
            "Scorpion contact must damage player life and install the native retreat heading without Enter.");
        contact.context.DamageObject(enemy, 3);
        Check(enemy.groupIndexOrLock == 107 && enemy.generalPurpose1 == 0,
            "Nonlethal scorpion damage must preserve its live state.");
        contact.context.DamageObject(enemy, 107);
        Check(enemy.generalPurpose1 == 1 && enemy.collisionMask == 0 && enemy.animFrame == 5 &&
            enemy.modelFrame2 == 4 && enemy.blendWeightOrSpriteOpacity == -100,
            "Lethal scorpion damage must enter the shared noncolliding collapse.");
        contact.context.tickCount = 100;
        DispatchObjectCase(enemy, contact.context);
        Check(enemy.behavior == BehaviorType::Scorpion && enemy.posY == 0 && enemy.winZ == 256 &&
            enemy.blendWeightOrSpriteOpacity == 0,
            "The scorpion corpse must settle at ground height and retain its exact 100-tick boundary.");
        contact.context.tickCount = 1;
        DispatchObjectCase(enemy, contact.context);
        Check(enemy.behavior == BehaviorType::None && enemy.health == 1000,
            "The scorpion's completed death pose must persist as a world object.");
    }

    void TestRotatingCenters()
    {
        struct Audio final : AudioHandler
        {
            std::array<bool, SoundChannelCount> playing{};
            int starts = 0;
            std::uint8_t sample = 0, volume = 0;
            std::uint16_t GetSampleCount() const override { return 129; }
            bool IsChannelPlaying(std::size_t channel) const override { return playing.at(channel); }
            void StopAllSamples() override { playing.fill(false); }
            void PlaySample(std::size_t channel, std::uint8_t id, std::uint8_t loudness) override
            {
                playing.at(channel) = true; sample = id; volume = loudness; ++starts;
            }
        } audio;
        Fixture f;
        auto& center = f.context.worldObjects[2];
        center.health = 1000; center.behavior = BehaviorType::RotatingMachineCenter;
        center.soundEffectId = 60; center.posX = 1500; center.posZ = 2500;
        center.velocityX = 3; center.velocityY = -5;
        center.winX = 510; center.winY = 1; center.winZ = 47;
        f.context.audioHandler = &audio;
        f.context.soundEffectsLoaded = true;
        f.context.soundEffects[59].sampleCount = 1;
        f.context.soundEffects[59].samples[0] = 42;
        f.context.tickCount = 4;
        DispatchObjectCase(center, f.context);
        Check(center.winX == 10 && center.winY == 493 && center.winZ == 47 &&
            audio.starts == 1 && audio.sample == 42 &&
            audio.volume == GameContext::GetSoundVolume(300),
            "Active machinery must wrap both signed angular rates and play its positional sound once per call.");
        DispatchObjectCase(center, f.context);
        Check(audio.starts == 1 && center.winX == 22 && center.winY == 473,
            "Machine sound must be limited per source without stopping rotation.");
        f.context.StopAllSoundEffects();
        f.context.tickCount = 0;
        const auto still = center;
        DispatchObjectCase(center, f.context);
        Check(std::memcmp(&still, &center, sizeof(center)) == 0 && audio.starts == 2,
            "Zero-tick machinery must keep its angles but still request sound, as in the original dispatcher.");
        for (auto health : {1001, 1999, 2000, 0})
        {
            f.context.SetObjectHealth(center, static_cast<std::int16_t>(health));
            const auto inactive = center;
            const auto seed = f.context.randomSeed;
            f.context.tickCount = 7;
            DispatchObjectCase(center, f.context);
            Check(std::memcmp(&inactive, &center, sizeof(center)) == 0 &&
                f.context.randomSeed == seed && audio.starts == 2,
                "Only health 1000 may animate or play rotating-center sound.");
        }
    }

    void TestOriginalTentacles(const std::filesystem::path& assets)
    {
        Host host(assets);
        host.StartLevel(21);
        int heads = 0, arms = 0, dogs = 0;
        std::size_t selected = 0;
        for (std::size_t slot = 2; slot <= 164; ++slot)
        {
            const auto& object = host.worldObjects[slot];
            if (object.behavior == BehaviorType::Dog) { ++dogs; }
            if (object.behavior == BehaviorType::AttachableDamageObject) { ++arms; }
            if (object.behavior != BehaviorType::KrakenHead) { continue; }
            ++heads;
            int attached = 0;
            for (const auto& candidate : host.worldObjects)
            {
                if (candidate.behavior == BehaviorType::AttachableDamageObject &&
                    candidate.doorLockOrGroupBitmask == static_cast<int>(slot)) { ++attached; }
            }
            Check(attached >= 6 && attached <= 8 && object.generalPurpose3 == attached,
                "Each authored kraken's surviving-arm counter must match its separate linked objects.");
            if (selected == 0 && object.health == 1000) { selected = slot; }
        }
        Check(heads == 8 && arms == 56 && dogs == 4 && selected != 0,
            "The original first sewer must supply all eight krakens, 56 arms and four dogs.");
        auto& head = host.worldObjects[selected];
        host.soundListenerX = static_cast<std::int16_t>(head.posX + 100);
        host.soundListenerZ = head.posZ;
        std::vector<std::size_t> slots;
        for (std::size_t slot = 2; slot <= 164; ++slot)
        {
            if (host.worldObjects[slot].behavior == BehaviorType::AttachableDamageObject &&
                host.worldObjects[slot].doorLockOrGroupBitmask == static_cast<int>(selected)) { slots.push_back(slot); }
        }
        Check(slots.size() == 7, "The selected independent-damage fixture must have seven arms.");
        const auto sibling = host.worldObjects[slots[1]];
        auto& arm = host.worldObjects[slots[0]];
        host.DamageObject(arm, 3);
        Check(arm.groupIndexOrLock == 7 && head.generalPurpose3 == 7, "Partial arm damage must not affect head strength or arm count.");
        host.DamageObject(arm, 7);
        Check(arm.health == 2000 && arm.modelFrame2 == 5 && arm.collisionMask == 0 &&
            head.generalPurpose3 == 6 && head.groupIndexOrLock == 150 &&
            std::memcmp(&sibling, &host.worldObjects[slots[1]], sizeof(sibling)) == 0,
            "Killing one arm must decrement only its own parent's count and leave its siblings intact.");
        host.DamageObject(arm, 100);
        Check(head.generalPurpose3 == 6, "A dead arm must never decrement the parent twice.");
        host.tickCount = 34;
        Function_AttachableDamageObject(arm, host);
        Check(arm.behavior == BehaviorType::None && arm.health == 2000 && arm.modelFrame2 == 5,
            "A killed arm must settle into its own detached corpse pose.");
        host.tickCount = 1;
        for (std::size_t i = 1; i < 5; ++i) { host.DamageObject(host.worldObjects[slots[i]], 10); }
        Check(head.generalPurpose3 == 2, "Five independent kills must leave exactly two live arms.");
        Function_KrakenHead(head, host);
        Check(head.health == 1000 && head.groupIndexOrLock == 50, "Fewer than three arms must begin native head depletion.");
        Function_KrakenHead(head, host);
        Check(head.health == 2000 && head.behavior == BehaviorType::AttachableDamageObject && head.modelFrame2 == 2,
            "The head must enter its own corpse transition after native depletion.");
        Function_AttachableDamageObject(host.worldObjects[slots[5]], host);
        Function_AttachableDamageObject(host.worldObjects[slots[6]], host);
        Check(head.generalPurpose3 == 0 && host.worldObjects[slots[5]].health == 2000 &&
            host.worldObjects[slots[6]].health == 2000, "Head death must finish only its remaining live arms.");
    }

    void TestOriginalKrakenMotion(const std::filesystem::path& assets)
    {
        Host host(assets);
        host.StartLevel(21);
        host.DEBUG_noPlayerCollision = true;
        for (std::size_t slot = 2; slot <= 164; ++slot)
        {
            auto& head = host.worldObjects[slot];
            if (head.behavior != BehaviorType::KrakenHead) { continue; }
            host.SetObjectHealth(head, 1000);
            host.soundListenerX = static_cast<std::int16_t>(head.posX + 100);
            host.soundListenerZ = head.posZ;
            const auto initialX = head.posX, initialZ = head.posZ;
            bool moved = false;
            for (int step = 0; step < 600; ++step)
            {
                Function_KrakenHead(head, host);
                moved |= head.posX != initialX || head.posZ != initialZ;
                for (auto& arm : host.worldObjects)
                {
                    if (arm.behavior != BehaviorType::AttachableDamageObject ||
                        arm.doorLockOrGroupBitmask != static_cast<int>(slot)) { continue; }
                    host.SetObjectHealth(arm, 1000);
                    Function_AttachableDamageObject(arm, host);
                    Check(arm.posY == head.posY && arm.health == 1000,
                        "Every original live tentacle must follow its moving head without sharing damage.");
                }
            }
            Check(moved && head.generalPurpose1 == 1500,
                "Every original sewer head must awaken and traverse its authored surface grid.");
        }
    }

    void TestOriginalSecondSewer(const std::filesystem::path& assets)
    {
        Host host(assets);
        host.StartLevel(22);
        host.DEBUG_noPlayerCollision = true;
        std::vector<std::size_t> scorpions;
        for (std::size_t slot = 2; slot <= 164; ++slot)
        {
            if (host.worldObjects[slot].behavior == BehaviorType::Scorpion) { scorpions.push_back(slot); }
        }
        Check(scorpions.size() == 4 &&
            host.worldObjects[10].behavior == BehaviorType::RotatingMachineCenter &&
            host.worldObjects[13].behavior == BehaviorType::RotatingMachineCenter &&
            host.worldObjects[10].health == 1011 && host.worldObjects[13].health == 1016,
            "The original second sewer must supply four scorpions and both dormant machine groups.");
        host.ActivateObjectGroup(11);
        Check(host.worldObjects[10].health == 1000 && host.worldObjects[11].health == 1000 &&
            host.worldObjects[12].health == 1000 && host.worldObjects[13].health == 1016,
            "Activating group 11 must wake only its center and attached parts.");
        host.ActivateObjectGroup(16);
        const std::array<std::int16_t, 4> initialPitch{
            host.worldObjects[11].winX, host.worldObjects[12].winX,
            host.worldObjects[14].winX, host.worldObjects[15].winX};
        for (int step = 0; step < 100; ++step)
        {
            std::array<std::int16_t, 2> oldYaw{host.worldObjects[10].winY, host.worldObjects[13].winY};
            const auto results = UpdateObjects(host);
            for (const auto& dispatch : results)
            {
                Check(dispatch.status != DispatchStatus::UnimplementedHandler,
                    "Activated original second-sewer objects must have no missing behavior handlers.");
            }
            for (int machine = 0; machine < 2; ++machine)
            {
                const int slot = machine == 0 ? 10 : 13;
                const auto& center = host.worldObjects[slot];
                Check(center.winY == ((oldYaw[machine] + 2) & 511),
                    "Both original centers must advance their authored two-unit angular rate.");
                for (int part = slot + 1; part <= slot + 2; ++part)
                {
                    const auto& blade = host.worldObjects[part];
                    std::int16_t dx, dz;
                    GetDirectionDisplacement(dx, dz, WrapSignedWord(center.winY + blade.velocityY - 128), 80);
                    Check(blade.doorLockOrGroupBitmask == slot && blade.health == 1000 &&
                        blade.posX == center.posX + dx && blade.posZ == center.posZ + dz &&
                        blade.posY == center.posY &&
                        blade.winX == ((initialPitch[machine * 2 + part - slot - 1] + (step + 1) * 4) & 511),
                        "All four original machine parts must orbit their own center and rotate independently.");
                }
            }
        }
        for (const auto slot : scorpions)
        {
            auto& scorpion = host.worldObjects[slot];
            Check(scorpion.behavior == BehaviorType::Scorpion && scorpion.health == 1000 &&
                scorpion.animFrame >= 1 && scorpion.animFrame <= 3,
                "Every original scorpion must remain active with its walking animation.");
            host.DamageObject(scorpion, 110);
            for (int tick = 0; tick < 101; ++tick) { DispatchObjectCase(scorpion, host); }
            Check(scorpion.behavior == BehaviorType::None && scorpion.health == 1000 &&
                scorpion.posY == host.GetTerrainFloorHeight(scorpion.posX, scorpion.posY, scorpion.posZ),
                "Every original scorpion must finish its own ground-level death pose.");
        }
        host.DEBUG_noPlayerCollision = false;
        const auto& blade = host.worldObjects[11];
        auto& player = host.worldObjects[1];
        player.posX = blade.posX; player.posY = blade.posY; player.posZ = blade.posZ;
        Input input;
        Check(UpdatePlayerInteraction(host, input).status == InteractionStatus::Applied && host.playerLife == 0,
            "The original activated machine parts must retain their lethal player-contact response.");
    }

    void TestOriginalGameplay(const std::filesystem::path& assets)
    {
        for (const auto level : std::array<std::int16_t, 2>{21, 22})
        {
            Host host(assets);
            host.StartLevel(level);
            Input input;
            for (int step = 0; step < 300; ++step)
            {
                if (level == 22 && step == 30)
                {
                    host.ActivateObjectGroup(11);
                    host.ActivateObjectGroup(16);
                }
                const auto result = host.Tick({1,1}, input);
                Check(result.status == GameTickStatus::Advanced && result.objects, "Original sewer gameplay must advance.");
                for (const auto& dispatch : *result.objects)
                {
                    if (dispatch.status == DispatchStatus::UnimplementedHandler)
                    {
                        throw std::runtime_error("Missing sewer behavior in level " + std::to_string(level) +
                            ": " + std::to_string(static_cast<int>(dispatch.behavior)));
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
        Check(argc == 2, "Usage: sewer-enemy-tests <Original asset directory>");
        TestDogs();
        TestDogBatchAndBones();
        TestHeadNavigation();
        TestAttachmentModes();
        TestScorpions();
        TestRotatingCenters();
        TestOriginalTentacles(argv[1]);
        TestOriginalKrakenMotion(argv[1]);
        TestOriginalSecondSewer(argv[1]);
        TestOriginalGameplay(argv[1]);
        std::cout << "Sewers: dogs, krakens, independent tentacles, scorpions, rotating machinery and both original levels passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
