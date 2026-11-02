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

    std::vector<std::uint8_t> Read(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file) { throw std::runtime_error("Missing trap fixture: " + path.string()); }
        return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
    }

    class Context final : public GameContext
    {
    public:
        using GameContext::LoadLevelGeometry;
        using GameContext::LoadLevelObjects;
        std::filesystem::path root;
        int created = 0, destroyed = 0, lifeChanges = 0;
        RawGameObject announced;
        void OnObjectCreated(int slot) override { ++created; announced = worldObjects[slot]; }
        void OnObjectDestroyed(int) override { ++destroyed; }
        void OnPlayerHealthChanged(std::int32_t, std::int32_t) override { ++lifeChanges; }
        std::vector<std::uint8_t> LoadFile(std::string_view path) override { return Read(root / path); }
    };

    class Audio final : public AudioHandler
    {
    public:
        int plays = 0;
        std::uint8_t playedSample = 0, playedVolume = 0;
        std::uint16_t GetSampleCount() const override { return 100; }
        bool IsChannelPlaying(std::size_t) const override { return plays != 0; }
        void PlaySample(std::size_t, std::uint8_t sample, std::uint8_t volume) override
        { ++plays; playedSample = sample; playedVolume = volume; }
        void StopAllSamples() override { plays = 0; }
    };

    struct Fixture
    {
        Level level = CreateDummyLevel();
        Context context;
        Fixture()
        {
            context.level = &level;
            context.collisionMap = level.collisionMap.get();
            std::fill(std::begin(context.collisionMap->data), std::end(context.collisionMap->data), 0);
            RawGameObject player;
            player.health = 1000; player.collisionMask = 255;
            player.posX = 1800; player.posZ = 2000; player.posY = 100;
            player.collisionRadiusXZ = player.collisionRadiusY = 20;
            context.ReplaceWorldObject(1, player);
            auto source = player;
            source.behavior = BehaviorType::ShootArrows; source.generalPurpose1 = 1;
            source.itemValue = 2; source.doorLockOrGroupBitmask = 10; source.posX = 1500;
            context.ReplaceWorldObject(2, source);
            context.soundListenerX = player.posX; context.soundListenerZ = player.posZ;
            context.created = context.destroyed = 0;
        }
        RawGameObject& Source() { return context.worldObjects[2]; }
    };

    void TestArrowInitialization()
    {
        for (int angle : { -513, -1, 0, 128, 256, 511, 512 })
        {
            Fixture f;
            f.Source().winY = static_cast<std::int16_t>(angle);
            f.Source().modelFrame2 = 4;
            f.context.tickCount = 1;
            const auto result = DispatchObjectCase(f.Source(), f.context);
            RawGameObject expected;
            expected.health = 1000; expected.collisionMask = 1; expected.spriteScale = 35;
            expected.behavior = BehaviorType::FlyingArrow; expected.collisionType = CollisionType::FlyingArrow;
            expected.posX = 1500; expected.posY = 100; expected.posZ = 2000;
            expected.modelId = 31; expected.modelAnim = 1; expected.animFrame = 2;
            expected.displayType = DisplayType::VectorNormal; expected.winY = (angle & 511) + 256;
            expected.behaviorParameter = 140; expected.collisionRadiusXZ = 10; expected.collisionRadiusY = 8;
            GetClampedDirectionDisplacement(expected.velocityX, expected.velocityZ, angle & 511, 10);
            const auto& shot = f.context.worldObjects[3];
            Check(result.status == DispatchStatus::ExecutedCase && std::memcmp(&shot, &expected, sizeof(shot)) == 0,
                "Arrow must match all 54 native initializer bytes.");
            Check(f.context.created == 1 && std::memcmp(&shot, &f.context.announced, sizeof(shot)) == 0 &&
                f.context.randomSeed == 0 && f.Source().animFrame == 4 && f.Source().generalPurpose2 == 2,
                "Arrow announcement must contain the complete record, without consuming RNG.");
        }
        Fixture floor;
        floor.Source().posY = -30000;
        floor.context.SpawnEmitterProjectile(floor.Source());
        Check(floor.context.worldObjects[3].posY ==
            floor.context.GetTerrainFloorHeight(1500, -30000, 2000) + 8,
            "Shared projectile initialization must apply terrain plus eight floor correction.");
    }

    void TestEmitterTimingAndAllocation()
    {
        Fixture batch, single;
        batch.context.tickCount = 8;
        DispatchObjectCase(batch.Source(), batch.context);
        for (int i = 0; i < 8; ++i) { DispatchObjectCase(single.Source(), single.context); }
        Check(batch.context.created == 3 && batch.context.created == single.context.created &&
            batch.Source().generalPurpose2 == 1 &&
            std::memcmp(batch.context.worldObjects.data(), single.context.worldObjects.data(),
                sizeof(batch.context.worldObjects)) == 0, "Emitter cadence is interval plus one substeps.");
        for (int ticks : { 0 })
        {
            Fixture f;
            const auto before = f.Source();
            f.context.tickCount = static_cast<std::int16_t>(ticks);
            DispatchObjectCase(f.Source(), f.context);
            Check(std::memcmp(&before, &f.Source(), sizeof(before)) == 0 && f.context.created == 0,
                "Zero emitter ticks do nothing.");
        }
        for (int health : { 999, 1001 })
        {
            Fixture f;
            f.Source().health = static_cast<std::int16_t>(health);
            DispatchObjectCase(f.Source(), f.context);
            Check(f.context.created == 0, "Only native health sentinel 1000 enables the emitter.");
        }
        Fixture once;
        once.Source().itemValue = 0; once.Source().generalPurpose2 = 65535;
        once.context.tickCount = 50;
        DispatchObjectCase(once.Source(), once.context);
        Check(once.context.created == 1, "Zero interval exits this dispatch immediately after its first shot.");
        Fixture full;
        for (int slot = 3; slot <= 165; ++slot) { full.context.worldObjects[slot].health = 1000; }
        DispatchObjectCase(full.Source(), full.context);
        Check(full.context.worldObjects[165].behavior == BehaviorType::FlyingArrow &&
            full.context.destroyed == 1 && full.context.created == 1,
            "Full permanent object pool must use native fallback slot165 and lifecycle callbacks.");
        Fixture lowest;
        for (int slot = 3; slot <= 165; ++slot) { lowest.context.worldObjects[slot].health = 1000; }
        lowest.context.worldObjects[17].health = 5;
        DispatchObjectCase(lowest.Source(), lowest.context);
        Check(lowest.context.worldObjects[17].behavior == BehaviorType::FlyingArrow,
            "Emitter uses the lowest-health allocator, not the free-only allocator.");
    }

    void TestArrowFlight()
    {
        Fixture f;
        DispatchObjectCase(f.Source(), f.context);
        auto& shot = f.context.worldObjects[3];
        const auto initial = shot;
        f.context.tickCount = 3;
        Check(DispatchObjectCase(shot, f.context).status == DispatchStatus::ExecutedCase &&
            shot.posX == initial.posX + 3 * initial.velocityX &&
            shot.posZ == initial.posZ + 3 * initial.velocityZ && shot.posY == initial.posY,
            "Runtime selector zero must fly without gravity.");
        f.context.worldObjects[1].posX = shot.posX;
        f.context.worldObjects[1].posZ = shot.posZ;
        shot.velocityX = shot.velocityZ = 0;
        f.context.tickCount = 1;
        const auto life = f.context.playerLife;
        DispatchObjectCase(shot, f.context);
        Check(f.context.playerLife == life - 29491 * 140 && shot.behaviorParameter == 0 &&
            shot.health == 0 && f.context.playerHitEffectTicks == 10 && f.context.destroyed == 1,
            "Arrow overlap must damage once and route destruction through shared collision41 activation.");
        Fixture wall;
        DispatchObjectCase(wall.Source(), wall.context);
        std::fill(std::begin(wall.context.collisionMap->data), std::end(wall.context.collisionMap->data), 8);
        DispatchObjectCase(wall.context.worldObjects[3], wall.context);
        Check(wall.context.worldObjects[3].health == 0 && wall.context.destroyed == 1,
            "Arrow wall collision must destroy and notify exactly once.");
    }

    void TestSpear()
    {
        for (int phase : { -32768, -1, 0, 1, 10, 11, 99, 100, 189, 190, 200, 201, 32767 })
        {
            Fixture f;
            auto& trap = f.Source();
            trap.behavior = BehaviorType::SpearOnGroundTrap; trap.generalPurpose1 = static_cast<std::uint16_t>(phase);
            trap.itemValue = 500; trap.doorLockOrGroupBitmask = 4; trap.behaviorParameter = 250;
            trap.posX = f.context.worldObjects[1].posX;
            const auto life = f.context.playerLife;
            const auto magnitude = WrapSignedWord(std::abs(static_cast<int>(WrapSignedWord(phase - 100))));
            const auto blend = std::max<int>(WrapSignedWord(100 - magnitude), 0);
            f.context.tickCount = 3;
            DispatchObjectCase(trap, f.context);
            Check(trap.blendWeightOrSpriteOpacity == blend &&
                WrapSignedWord(trap.generalPurpose1) == (phase < 1 ? 500 : phase - 12),
                "Spear computes morph before its single tick-scaled phase decrement.");
            Check(f.context.playerLife == life - (blend > 10 ? 29491 * 250 : 0) &&
                trap.generalPurpose2 == (blend > 10 ? 1 : 0),
                "Spear strict morph threshold gates latched contact damage.");
        }
        Fixture f;
        auto& trap = f.Source();
        trap.behavior = BehaviorType::SpearOnGroundTrap; trap.generalPurpose1 = 100;
        trap.behaviorParameter = 20; trap.animFrame = 1; trap.posX = 1800;
        trap.doorLockOrGroupBitmask = 0;
        const auto life = f.context.playerLife;
        f.context.tickCount = 0;
        DispatchObjectCase(trap, f.context);
        DispatchObjectCase(trap, f.context);
        Check(trap.doorLockOrGroupBitmask == 5 && trap.animFrame == 3 &&
            f.context.playerLife == life - 29491 * 20 && f.context.lifeChanges == 1,
            "Zero-tick spear still morphs and damages once per cycle, and switches the contact frame.");
        trap.generalPurpose1 = 0; trap.itemValue = 0;
        f.context.soundListenerX = 1880;
        DispatchObjectCase(trap, f.context);
        Check(trap.generalPurpose1 == 0 && trap.generalPurpose2 == 0,
            "Proximity trigger excludes distance exactly80 and resets the hit latch.");
        f.context.soundListenerX = 1879;
        DispatchObjectCase(trap, f.context);
        Check(trap.generalPurpose1 == 200 && trap.blendWeightOrSpriteOpacity == 0,
            "Proximity trigger arms phase200 without immediately changing the morph.");
        trap.generalPurpose1 = 100; trap.generalPurpose2 = 0;
        f.context.playerLifeDeltaSuppressed = 1;
        const auto suppressedLife = f.context.playerLife;
        DispatchObjectCase(trap, f.context);
        Check(f.context.playerLife == suppressedLife && trap.generalPurpose2 == 1 &&
            f.context.playerHitEffectTicks == 8,
            "Shared life suppression must not prevent the native spear contact latch.");
    }

    void TestOriginalFixtures(const std::filesystem::path& root)
    {
        Context loader;
        loader.root = root;
        LevelLoadData data;
        loader.LoadLevelGeometry(15, data);
        loader.LoadLevelObjects(15, data);
        Check(data.objects[3].behavior == BehaviorType::ShootArrows && data.objects[3].generalPurpose1 == 1,
            "Native level15 source must load into original slot3.");
        Fixture f;
        f.context.ReplaceWorldObject(3, data.objects[3]);
        f.context.ReplaceWorldObject(2, {});
        f.context.tickCount = 111;
        UpdateObject(f.context.worldObjects[3], f.context);
        Check(f.context.worldObjects[2].behavior == BehaviorType::FlyingArrow,
            "Original slot3 initial delay must emit via the shared active-object update.");
        for (int slot : { 11, 13, 15, 17, 18, 19 })
        {
            const auto native = data.objects[slot];
            Check(native.behavior == BehaviorType::SpearOnGroundTrap && native.itemValue == 500,
                "Native level15 spear fixtures must retain original slots and period500.");
            f.context.ReplaceWorldObject(slot, native);
            for (int frame = 0; frame < 300; ++frame)
            {
                f.context.tickCount = 1;
                Check(UpdateObject(f.context.worldObjects[slot], f.context).status == DispatchStatus::ExecutedCase,
                    "Every original spear fixture must execute through a complete cycle.");
            }
        }
        const auto fountainBytes = Read(root / "LEVEL\\OBJPOS30.DAT");
        Fixture fountain;
        fountain.Source() = DecodeGameObject(fountainBytes.data() + 39 * 54, 54);
        Audio audio;
        fountain.context.audioHandler = &audio;
        fountain.context.soundEffectsLoaded = true;
        fountain.context.soundEffects[85].sampleCount = 1;
        fountain.context.soundEffects[85].samples[0] = 12;
        fountain.context.soundListenerX = fountain.Source().posX;
        fountain.context.soundListenerZ = fountain.Source().posZ;
        fountain.Source().generalPurpose2 = 0;
        DispatchObjectCase(fountain.Source(), fountain.context);
        Check(fountain.context.created == 0 && fountain.Source().health == 1005,
            "The native fountain fixture is dormant until its health sentinel is activated.");
        fountain.context.SetObjectHealth(fountain.Source(), 1000);
        DispatchObjectCase(fountain.Source(), fountain.context);
        auto& drop = fountain.context.worldObjects[3];
        Check(drop.itemValue == 11 && drop.behavior == BehaviorType::FlyingArrow &&
            drop.modelId == fountain.Source().modelId && drop.generalPurpose1 == fountain.Source().velocityY &&
            drop.generalPurpose3 == fountain.Source().velocityX &&
            drop.doorLockOrGroupBitmask == fountain.Source().velocityZ,
            "The other native behavior40 source, level30 slot41, must spawn its fountain dependency.");
        Check(audio.plays == 1 && audio.playedSample == 12 &&
            audio.playedVolume == fountain.context.GetSoundVolumeAt(drop.posX, drop.posZ) &&
            fountain.context.soundChannelSources[3] == 500,
            "Fountain creation must use sound86, positional volume and shared sound source500.");
        drop.posY = 100;
        const auto phase = drop.generalPurpose1;
        DispatchObjectCase(drop, fountain.context);
        Check(drop.generalPurpose1 == static_cast<std::uint16_t>(phase - 1) &&
            drop.velocityY == ArithmeticShiftRight(WrapSignedWord(phase - 1), 2),
            "Fountain runtime selector11 uses signed quarter-speed vertical acceleration.");
    }
}

int main(int argc, char** argv)
{
    try
    {
        Check(argc == 2, "Expected original asset root.");
        TestArrowInitialization();
        TestEmitterTimingAndAllocation();
        TestArrowFlight();
        TestSpear();
        TestOriginalFixtures(argv[1]);
        std::cout << "Level15 arrow/spear trap tests passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
