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

    class Host final : public GameContext
    {
    public:
        std::filesystem::path assets;
        int destroyed = 0;
        explicit Host(std::filesystem::path path) : assets(std::move(path)) { StartNewGame(2); }
        void OnObjectDestroyed(int) override { ++destroyed; }
    protected:
        std::vector<std::uint8_t> LoadFile(std::string_view filename) override
        {
            std::ifstream file(assets / filename, std::ios::binary);
            if (!file) { throw std::runtime_error("Missing park fixture: " + std::string(filename)); }
            return {std::istreambuf_iterator<char>(file), {}};
        }
        void LoadLevel(const LevelLoadRequest& request) override
        {
            LevelLoadData data;
            LoadLevelGeometry(request.levelNumber, data);
            LoadLevelObjects(request.levelNumber, data);
            InitializeLevelPlayerSpawn(data, {-256, 0, 0});
            CompleteLevelLoad(request.id, std::move(data));
        }
    };

    class Input final : public InputHandler
    {
    public:
        bool bucket = false, attack = false;
        bool IsKeyPressed(InputKey key) override
        {
            return (bucket && key == InputKey::SelectBucketKey) || (attack && key == InputKey::AttackKey);
        }
    };

    class Audio final : public AudioHandler
    {
    public:
        std::array<bool, 4> playing{};
        int stopped = 0;
        std::uint16_t GetSampleCount() const override { return 129; }
        bool IsChannelPlaying(std::size_t channel) const override { return playing.at(channel); }
        void PlaySample(std::size_t channel, std::uint8_t, std::uint8_t) override { playing.at(channel) = true; }
        void StopAllSamples() override { playing.fill(false); }
        void StopSample(std::size_t channel) override { playing.at(channel) = false; ++stopped; }
    };

    void EquipBucket(Host& host)
    {
        Check(HandlePlayerObjectInteraction(host.worldObjects[53], host).status == InteractionStatus::Applied,
            "The original bucket must be pickable.");
        host.RefreshWeaponInventory();
        host.weaponAnimationSpeed = 0;
        Check(host.weaponAmounts[9] == 0 && host.SelectWeapon(WeaponType::Bucket),
            "An empty bucket must be selectable from the backpack or either hand.");
        host.UpdateWeaponAnimation(0);
        host.UpdateWeaponAnimation(30);
        Check(host.selectedWeapon == WeaponType::Bucket && host.weaponAnimationSpeed == 0 &&
            !host.weaponReady && host.GetWeaponView().has_value(), "Empty buckets must remain visible and equipped.");
    }

    void TestBucket(const std::filesystem::path& assets)
    {
        Host host(assets);
        EquipBucket(host);
        Input input;
        host.basicPlayerMovementEnabled = true;
        host.ActivateObjectGroup(5);
        Check(host.worldObjects[41].health == 1000 &&
            host.worldObjects[55].collisionType == CollisionType::None && host.collisionmapUploadDirty,
            "The authored fountain group must activate its water emitter and floor replacement.");
        auto& player = host.worldObjects[1];
        player.posX = host.worldObjects[41].posX;
        player.posZ = host.worldObjects[41].posZ;
        player.posY = -175;
        player.collisionRadiusY = 25;
        host.soundListenerX = player.posX;
        host.soundListenerZ = player.posZ;
        const auto waterSlot = host.SpawnEmitterProjectile(host.worldObjects[41]);
        auto& drop = host.worldObjects[waterSlot];
        Check(drop.collisionType == CollisionType::FountainWater && drop.collisionMask == 1,
            "Original fountain emitter 11 must create bucket-filling collision-130 drops.");
        drop.posX = player.posX;
        drop.posZ = player.posZ;
        drop.posY = player.posY;
        host.frameTickCount = 1;
        host.Tick({0, 1}, input);
        Check(host.weaponAmounts[9] == 1 && drop.collisionMask == 0 && host.weaponReady,
            "Touching a water drop with the bucket equipped must fill one unit and consume that contact.");
        host.Tick({0, 1}, input);
        Check(host.weaponAmounts[9] == 1, "The same drop must not fill the bucket twice.");
        drop.collisionMask = 1;
        drop.posY = player.posY;
        host.selectedWeapon = WeaponType::Scythe;
        host.Tick({0, 1}, input);
        Check(host.weaponAmounts[9] == 1 && drop.collisionMask == 1,
            "Water must not fill an unequipped bucket.");
        host.selectedWeapon = WeaponType::Bucket;
        host.weaponAmounts[9] = 50;
        host.UpdateWeaponAnimation(0);
        Check(host.weaponAmounts[9] == 20, "Bucket contents must clamp to the native twenty-unit capacity.");
        const auto save = host.CaptureSaveGame("Full bucket");
        host.weaponAmounts[9] = 0;
        host.LoadGame(save);
        Check(host.weaponAmounts[9] == 20 && host.selectedWeapon == WeaponType::Bucket,
            "Native saves must preserve the bucket selector and water amount.");
        host.UpdateWeaponAnimation(30);

        auto view = host.GetWeaponView();
        Check(view && view->object.modelId == 33 && view->object.modelAnim == 3 &&
            view->object.animFrame == 1 && view->object.modelFrame2 == 2 &&
            view->object.blendWeightOrSpriteOpacity == 100 && !view->bindWeaponBitmap &&
            view->focalLength == 120, "Full bucket rendering must use level model 3, frames 1/2 and native projection.");
        host.weaponAmounts[9] = 10;
        host.UpdateWeaponAnimation(0);
        Check(host.GetWeaponView()->object.blendWeightOrSpriteOpacity == 50,
            "Half-full bucket must morph its water polygon halfway.");
        host.weaponAmounts[9] = 20;
        host.weaponAnimationSpeed = 0;
        host.weaponReady = true;
        host.playerUnderwater = true;
        Check(host.AttackWithSelectedWeapon() == WeaponActionStatus::NoEffect && host.weaponAmounts[9] == 20,
            "The bucket must obey the native underwater weapon restriction.");
        host.playerUnderwater = false;
        player.posX = 750; player.posZ = 3350;
        player.velocityX = player.velocityZ = 0;
        const auto before = std::count_if(host.worldObjects.begin(), host.worldObjects.end(),
            [](const RawGameObject& object) { return object.behavior == BehaviorType::WashingWater; });
        Check(host.AttackWithSelectedWeapon() == WeaponActionStatus::Applied &&
            host.weaponAmounts[9] == 0 && !host.weaponReady && host.weaponAnimationSpeed == -3,
            "One pour must empty the whole bucket, not just one unit.");
        const auto after = std::count_if(host.worldObjects.begin(), host.worldObjects.end(),
            [](const RawGameObject& object) { return object.behavior == BehaviorType::WashingWater; });
        Check(after - before == 20, "A full bucket must create twenty original washing-water records.");
        Check(host.AttackWithSelectedWeapon() == WeaponActionStatus::NoEffect,
            "An empty bucket or active pour animation must not pour again.");
        host.UpdateWeaponAnimation(5);
        view = host.GetWeaponView();
        Check(view && view->object.posY >= 50 && view->object.winX == -100 &&
            view->object.blendWeightOrSpriteOpacity == 0, "The empty bucket must tilt and recover after pouring.");
        host.UpdateWeaponAnimation(25);
        Check(host.weaponAnimationSpeed == 0 && host.GetWeaponView(),
            "An empty bucket must recover without disappearing from the hand.");
        host.SetPaused(true);
        const auto pausedWorld = host.worldObjects;
        const auto pausedSeed = host.randomSeed;
        input.attack = true;
        Check(host.Tick({30,30}, input).status == GameTickStatus::Paused &&
            std::memcmp(pausedWorld.data(), host.worldObjects.data(), sizeof(pausedWorld)) == 0 &&
            pausedSeed == host.randomSeed && host.weaponAmounts[9] == 0,
            "Pausing must suspend pouring, water particles and fountain contacts.");
    }

    void TestBucketInput(const std::filesystem::path& assets)
    {
        Host host(assets);
        HandlePlayerObjectInteraction(host.worldObjects[53], host);
        std::swap(host.inventory[0], host.inventory[5]);
        host.RefreshWeaponInventory();
        Input input;
        input.bucket = true;
        host.Tick({0,0}, input);
        Check(host.requestedWeapon == WeaponType::Bucket && host.inventory[1].itemValue == 9,
            "The bucket input must select an empty bucket from the backpack into the right hand.");
        input.bucket = false;
        host.Tick({0,30}, input);
        host.Tick({0,30}, input);
        Check(host.selectedWeapon == WeaponType::Bucket && host.GetWeaponView(),
            "Normal gameplay input must finish equipping and display the bucket.");
    }

    void TestWashing(const std::filesystem::path& assets)
    {
        Host host(assets);
        host.tickCount = 1;
        const auto originals = host.worldObjects;
        int washed = 0;
        for (std::size_t slot = 2; slot < originals.size(); ++slot)
        {
            if (originals[slot].collisionType != CollisionType::WashableMud) { continue; }
            host.worldObjects = originals;
            auto& mud = host.worldObjects[slot];
            // Isolate each authored mud footprint, including the tightly packed puddles.
            for (std::size_t other = 2; other < host.worldObjects.size(); ++other)
            {
                if (other != slot) { host.worldObjects[other].collisionMask = 0; }
            }
            RawGameObject water;
            water.health = 1000; water.modelId = 33; water.modelAnim = 4;
            water.animFrame = 1; water.modelFrame2 = 2;
            water.behavior = BehaviorType::WashingWater;
            water.posX = mud.posX; water.posY = mud.posY; water.posZ = mud.posZ;
            water.velocityX = 3;
            host.ReplaceWorldObject(150, water);
            Check(DispatchObjectCase(host.worldObjects[150], host).status == DispatchStatus::ExecutedCase &&
                mud.behavior == BehaviorType::WashingWater && mud.animFrame == 1,
                "Poured water must start the original mud's frame-1/2 dissolving morph.");
            const auto destroyed = host.destroyed;
            for (int tick = 0; tick < 2000 && mud.health > 0; ++tick) { DispatchObjectCase(mud, host); }
            Check(mud.health == 0 && host.destroyed > destroyed,
                "Every authored mud patch must dissolve completely and notify the renderer.");
            ++washed;
        }
        Check(washed == 9, "The park contains nine washable mud objects.");
        host.worldObjects = originals;
        auto& water = host.worldObjects[150];
        water = {};
        water.health = 1000; water.behavior = BehaviorType::WashingWater;
        water.posX = 750; water.posY = -197; water.posZ = 3350;
        host.SetCollisionMapPixelAtWorldXZ(water.posX, water.posZ, 0);
        DispatchObjectCase(water, host);
        Check(water.health == 0, "Poured water on material zero must disappear.");
    }

    void TestAnimals(const std::filesystem::path& assets)
    {
        Host host(assets);
        auto& bull = host.worldObjects[45];
        host.tickCount = 0;
        const auto original = bull;
        const auto seed = host.randomSeed;
        DispatchObjectCase(bull, host);
        Check(std::memcmp(&original, &bull, sizeof(bull)) == 0 && seed == host.randomSeed,
            "Zero-tick bull updates must neither move nor consume RNG.");
        host.tickCount = 1;
        host.soundListenerX = WrapSignedWord(bull.posX + 200);
        host.soundListenerZ = bull.posZ;
        for (int dx = 0; dx <= 200; dx += 25)
        {
            host.SetCollisionMapPixelAtWorldXZ(WrapSignedWord(bull.posX + dx), bull.posZ, 0);
        }
        bool armed = false;
        for (int tick = 0; tick < 1000 && !armed; ++tick)
        {
            DispatchObjectCase(bull, host);
            armed = bull.itemValue == 2;
        }
        Check(armed, "The original bull must turn toward a visible nearby player and autonomously arm its charge.");
        bull = original;
        bull.itemValue = 1;
        bull.winY = 384;
        DispatchObjectCase(bull, host);
        Check(bull.itemValue == 2 && bull.velocityX == -8 && bull.velocityZ == 0,
            "The bull must arm a fixed heading/speed before charging.");
        const auto x = bull.posX;
        host.SetCollisionMapPixelAtWorldXZ(WrapSignedWord(x - 58), bull.posZ, 0);
        DispatchObjectCase(bull, host);
        Check(bull.posX == x - 8 && bull.modelAnim == 2 && bull.behaviorParameter == 10 &&
            bull.generalPurpose2 == 31, "Charging must move, animate, nod and enable the original damage parameter.");
        Audio audio;
        host.audioHandler = &audio;
        std::array<std::uint8_t, SoundEffectTableSize> sounds{};
        sounds[0] = SoundEffectCount;
        sounds[2] = 5;
        for (std::size_t effect = 0; effect < SoundEffectCount; ++effect)
        {
            sounds[4 + effect * 5] = 1;
            sounds[5 + effect * 5] = static_cast<std::uint8_t>(effect + 1);
        }
        host.LoadSoundEffects(sounds.data(), sounds.size());
        host.PlaySoundEffect(static_cast<SoundEffect>(89), 255, 45, 2);
        host.PlaySoundEffect(static_cast<SoundEffect>(5), 255, 500, 1);
        host.randomSeed = 1;
        host.SetCollisionMapPixelAtWorldXZ(WrapSignedWord(bull.posX - 58), bull.posZ, 56);
        DispatchObjectCase(bull, host);
        Check(bull.itemValue == -1 && audio.stopped == 1 &&
            std::count(host.soundChannelSamples.begin(), host.soundChannelSamples.end(), 5) == 1,
            "Hitting a wall must reset the bull and stop sample 89 without stopping other sounds.");
        host.audioHandler = nullptr;
        bull.health = 1001;
        const auto inactive = bull;
        host.tickCount = 8;
        DispatchObjectCase(bull, host);
        Check(std::memcmp(&inactive, &bull, sizeof(bull)) == 0, "Non-live bull states must not charge.");
        for (const int slot : {33,34,35,50,75})
        {
            auto& cow = host.worldObjects[slot];
            cow.generalPurpose2 = 128;
            cow.itemValue = 0;
            cow.velocityX = 3;
            const auto bodyX = cow.posX;
            const auto bodyY = cow.posY;
            const auto bodyZ = cow.posZ;
            host.tickCount = 1;
            DispatchObjectCase(cow, host);
            Check(cow.blendWeightOrSpriteOpacity == 50 && cow.generalPurpose2 != 128 &&
                cow.posX == bodyX && cow.posY == bodyY && cow.posZ == bodyZ,
                "Cows must morph their heads without moving their bodies.");
        }
        Host batched(assets), stepped(assets);
        batched.randomSeed = stepped.randomSeed = 7;
        batched.tickCount = 19;
        DispatchObjectCase(batched.worldObjects[45], batched);
        stepped.tickCount = 1;
        for (int tick = 0; tick < 19; ++tick) { DispatchObjectCase(stepped.worldObjects[45], stepped); }
        Check(batched.randomSeed == stepped.randomSeed &&
            std::memcmp(&batched.worldObjects[45], &stepped.worldObjects[45], sizeof(RawGameObject)) == 0,
            "Batched bull updates must match the native per-tick loop and RNG sequence.");
    }

    void TestCoverage(const std::filesystem::path& assets)
    {
        Host host(assets);
        for (auto& object : host.worldObjects)
        {
            if (object.health > 1000 && object.health < 2000) { host.DamageObject(object, 0); }
        }
        for (const int ticks : {0,1,7,30,1,1,30})
        {
            host.tickCount = host.frameTickCount = static_cast<std::int16_t>(ticks);
            const auto results = UpdateObjects(host);
            for (std::size_t index = 0; index < results.size(); ++index)
            {
                const auto& result = results[index];
                if (result.behavior == BehaviorType::Moon) { continue; }
                if (result.status == DispatchStatus::UnimplementedHandler || result.status == DispatchStatus::Failed)
                {
                    throw std::runtime_error("Park behavior failed at slot " + std::to_string(index + 2) +
                        ", behavior " + std::to_string(static_cast<int>(result.behavior)));
                }
            }
        }
        Check(DispatchObjectCase(host.worldObjects[42], host).status == DispatchStatus::UnimplementedHandler,
            "Moon must remain deliberately unrecovered.");
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc != 2) { throw std::invalid_argument("Expected the original assets directory."); }
        const std::filesystem::path assets(argv[1]);
        TestBucket(assets);
        TestBucketInput(assets);
        TestWashing(assets);
        TestAnimals(assets);
        TestCoverage(assets);
        std::cout << "Park gameplay tests passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
