#include "BehaviorDispatch.h"
#include "GameMath.h"

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

    int Word(int value)
    {
        const auto bits = static_cast<unsigned int>(value) & 65535u;
        return bits >= 32768 ? static_cast<int>(bits) - 65536 : static_cast<int>(bits);
    }

    int Next(std::uint32_t& seed, unsigned int range)
    {
        seed = seed * 134775813u + 1;
        return static_cast<int>((seed >> 16) % range);
    }

    int Signed(std::uint32_t& seed, unsigned int radius)
    {
        const int value = Next(seed, 2 * radius + 1) - static_cast<int>(radius);
        return value == 0 ? static_cast<int>(radius) : value;
    }

    class Context : public GameContext
    {
    public:
        int creations = 0;
        int destructions = 0;
        int lifeChanges = 0;
        void OnObjectCreated(int) override { ++creations; }
        void OnObjectDestroyed(int) override { ++destructions; }
        void OnPlayerHealthChanged(std::int32_t, std::int32_t) override { ++lifeChanges; }
    };

    struct Fixture
    {
        Level level = CreateDummyLevel();
        Context context;
        Fixture()
        {
            context.level = &level;
            context.collisionMap = level.collisionMap.get();
            context.field_D500 = true;
            context.soundListenerX = 1800;
            context.soundListenerZ = 2000;
            RawGameObject object;
            object.health = 1000;
            object.posX = 1500;
            object.posY = 10;
            object.posZ = 2000;
            object.modelId = 36;
            object.modelAnim = 1;
            object.animFrame = 1;
            object.modelFrame2 = 2;
            object.collisionMask = 254;
            object.collisionRadiusXZ = 20;
            object.collisionRadiusY = 20;
            object.behaviorParameter = 3;
            object.groupIndexOrLock = 50;
            object.displayType = DisplayType::VectorNormal;
            context.ReplaceWorldObject(2, object);
            object.posX = 1800;
            object.collisionMask = 255;
            context.ReplaceWorldObject(1, object);
            context.creations = 0;
        }
        RawGameObject& Object() { return context.worldObjects[2]; }
    };

    struct Input final : InputHandler
    {
        bool IsKeyPressed(InputKey) override { return false; }
    };

    void TestTickBoundariesAndContexts()
    {
        for (auto type : { BehaviorType::SpiderLarge, BehaviorType::Harvestman,
            BehaviorType::Devil, BehaviorType::Gorilla })
        {
            for (std::uint32_t seed : { 1u, 19u, 2187u, 0xffffffffu })
            {
                Fixture batch;
                Fixture single;
                batch.Object().behavior = type;
                batch.Object().velocityY = 2;
                batch.Object().generalPurpose2 = 65535;
                batch.Object().doorLockOrGroupBitmask = 100;
                single.Object() = batch.Object();
                batch.context.randomSeed = single.context.randomSeed = seed;
                const auto original = batch.Object();
                batch.context.tickCount = 0;
                DispatchObjectCase(batch.Object(), batch.context);
                Check(std::memcmp(&original, &batch.Object(), sizeof(original)) == 0 &&
                    batch.context.randomSeed == seed, "Zero enemy ticks must not mutate state or RNG.");
                batch.context.tickCount = 80;
                DispatchObjectCase(batch.Object(), batch.context);
                single.context.tickCount = 1;
                for (int tick = 0; tick < 80; ++tick) { DispatchObjectCase(single.Object(), single.context); }
                Check(std::memcmp(&batch.Object(), &single.Object(), sizeof(original)) == 0 &&
                    batch.context.randomSeed == single.context.randomSeed &&
                    batch.context.playerLife == single.context.playerLife,
                    "Enemy substeps/RNG must be deterministic and independent across contexts.");
            }
        }
    }

    void TestGorilla()
    {
        Fixture fixture;
        auto& object = fixture.Object();
        object.behavior = BehaviorType::Gorilla;
        object.modelId = 35;
        object.modelFrame2 = 11;
        object.posY = 30;
        object.generalPurpose2 = 65535;
        fixture.context.tickCount = 1;
        fixture.context.randomSeed = 1;
        auto seed = fixture.context.randomSeed;
        const auto pursuit = Next(seed, 80);
        Next(seed, 15);
        const auto wandering = Signed(seed, 20);
        Signed(seed, 1);
        Check(pursuit != 0, "Reference fixture must select wandering, not pursuit.");
        DispatchObjectCase(object, fixture.context);
        Check(fixture.context.randomSeed == seed && object.blendWeightOrSpriteOpacity == 13,
            "Gorilla must omit mummy random-pause draw and use animation increment 13.");
        std::int16_t vx = 0;
        std::int16_t vz = 0;
        GetClampedDirectionDisplacement(vx, vz, static_cast<std::int16_t>(wandering & 511), 5);
        Check(object.velocityX == vx && object.velocityZ == vz && object.posY == 30,
            "Gorilla must use native speed 5 and terrain-relative height 30.");
        const auto paused = object;
        object.health = 1001;
        const auto pausedSeed = fixture.context.randomSeed;
        DispatchObjectCase(object, fixture.context);
        Check(object.posX == paused.posX && fixture.context.randomSeed == pausedSeed,
            "Inactive gorilla groups must remain suspended.");
        object.health = 1000;
        object.collisionType = CollisionType::PushingEnemy;
        fixture.context.DamageObject(object, 100);
        Check(object.generalPurpose1 == 1 && object.collisionMask == 0, "Gorilla death must enter shared collapse.");
        for (int tick = 0; tick < 150 && object.behavior != BehaviorType::None; ++tick)
        {
            DispatchObjectCase(object, fixture.context);
        }
        Check(object.behavior == BehaviorType::None && object.health == 1000 &&
            object.animFrame == 3 && object.modelFrame2 == 0 &&
            fixture.context.creations == 0, "Gorilla corpse must remain, without mummy fade/smoke.");
    }

    void TestContactsAndDeath()
    {
        for (auto type : { CollisionType::SpiderLarge, CollisionType::Harvestman, CollisionType::Devil })
        {
            Fixture fixture;
            auto& object = fixture.Object();
            object.behavior = static_cast<BehaviorType>(type);
            object.collisionType = type;
            fixture.context.worldObjects[1].posX = object.posX;
            fixture.context.soundListenerX = object.posX + 1;
            fixture.context.tickCount = 0;
            object.velocityX = 4;
            object.velocityZ = 5;
            const auto before = fixture.context.playerLife;
            Input input;
            Check(UpdatePlayerInteraction(fixture.context, input).status == InteractionStatus::Applied &&
                fixture.context.playerLife == before - 29491 * 3 &&
                fixture.context.lifeChanges == 1 && fixture.context.playerHitEffectTicks == 8,
                "Native enemy contact must apply player life damage without requiring input/ticks.");
            if (type != CollisionType::Devil)
            {
                Check(object.velocityX == 0 && object.velocityZ == 0 &&
                    WrapSignedWord(object.generalPurpose2) < 0 && object.winY == WrapSignedWord(object.generalPurpose2),
                    "Spider contact must stop translation and encode the signed retreat heading.");
            }
            fixture.context.DamageObject(object, 100);
            Check(object.generalPurpose1 == 1 && object.collisionType == CollisionType::None &&
                object.collisionMask == 0, "Enemy weapon death must disable contact and enter collapse.");
            fixture.context.tickCount = 1;
            for (int tick = 0; tick < 160 && object.behavior != BehaviorType::None; ++tick)
            {
                DispatchObjectCase(object, fixture.context);
            }
            Check(object.behavior == BehaviorType::None, "Every enemy collapse must reach its native resting pose.");
        }
    }

    void TestAmbushAndDevilAttack()
    {
        Fixture fixture;
        auto& object = fixture.Object();
        fixture.context.tickCount = 1;
        object.behavior = BehaviorType::Harvestman;
        object.generalPurpose1 = 2;
        object.posY = 80;
        object.winX = 128;
        fixture.context.soundListenerX = 1600;
        DispatchObjectCase(object, fixture.context);
        Check(object.generalPurpose1 == 3 && object.posY == 80, "Ceiling harvestman must trigger below distance 140.");
        for (int tick = 0; tick < 80 && object.generalPurpose1 != 0; ++tick)
        {
            DispatchObjectCase(object, fixture.context);
        }
        Check(object.generalPurpose1 == 0 && object.posY == 9 && object.winX == 0 &&
            object.modelAnim == 1, "Ceiling harvestman must land, unfold and enter ground movement.");
        object.behavior = BehaviorType::SpiderLarge;
        object.generalPurpose1 = 2;
        DispatchObjectCase(object, fixture.context);
        Check(object.generalPurpose1 == 3 && WrapSignedWord(object.generalPurpose2) == -2,
            "Suspended white spider must initialize the native release animation.");
        object.animFrame = 4;
        object.modelFrame2 = 5;
        object.generalPurpose2 = 1;
        object.blendWeightOrSpriteOpacity = 101;
        DispatchObjectCase(object, fixture.context);
        Check(object.generalPurpose1 == 4 && object.modelAnim == 1 && object.animFrame == 1,
            "White spider release must select the falling/walking bank.");
        object.posY = 30;
        object.winX = 0;
        fixture.context.tickCount = 30;
        DispatchObjectCase(object, fixture.context);
        Check(object.generalPurpose1 == 0, "White spider must land and enter ordinary walking.");

        object.behavior = BehaviorType::Devil;
        object.generalPurpose1 = 4;
        object.posY = 23;
        fixture.context.tickCount = 1;
        DispatchObjectCase(object, fixture.context);
        Check(object.generalPurpose1 == 5 && object.animFrame == 2 && object.modelFrame2 == 3,
            "Devil must begin windup with attack frames 2/3.");
        while (object.generalPurpose1 == 5) { DispatchObjectCase(object, fixture.context); }
        Check(object.generalPurpose1 == 6 && object.velocityY == 14, "Devil windup must enter its ballistic leap.");
        object.velocityX = 5;
        object.velocityZ = 0;
        fixture.context.soundListenerX = object.posX;
        const auto life = fixture.context.playerLife;
        DispatchObjectCase(object, fixture.context);
        Check(fixture.context.playerLife == life - 29491 * object.behaviorParameter &&
            fixture.context.playerHitEffectTicks == 6 && object.velocityX == 0 && object.velocityY == 0,
            "Devil leap must stop at melee range and apply native damage/hit timing.");
        fixture.context.soundListenerX = 1800;
        for (int tick = 0; tick < 80 && object.generalPurpose1 != 8; ++tick)
        {
            DispatchObjectCase(object, fixture.context);
        }
        Check(object.generalPurpose1 == 8, "Devil leap must land and recover.");
    }

    RawGameObject FindSpit(Fixture& fixture, std::uint32_t& spawnSeed)
    {
        auto& object = fixture.Object();
        object.behavior = BehaviorType::SpiderLarge;
        object.winY = 128;
        object.generalPurpose2 = 128;
        object.velocityY = 1;
        fixture.context.tickCount = 1;
        const auto source = object;
        for (std::uint32_t seed = 1; seed < 10000; ++seed)
        {
            object = source;
            fixture.context.randomSeed = seed;
            DispatchObjectCase(object, fixture.context);
            if (fixture.context.worldObjects[3].behavior == BehaviorType::FlyingArrow)
            {
                spawnSeed = seed;
                return fixture.context.worldObjects[3];
            }
        }
        throw std::runtime_error("Aligned native spider never spat slime.");
    }

    void TestSlime()
    {
        Fixture fixture;
        std::uint32_t seed = 0;
        const auto spawned = FindSpit(fixture, seed);
        Check(spawned.health == 100 && spawned.itemValue == 3 &&
            spawned.modelId == fixture.Object().modelId && spawned.modelAnim == 2 &&
            spawned.animFrame == 1 && spawned.velocityY == 7 &&
            spawned.velocityX == 7 && spawned.velocityZ == 0 &&
            spawned.behaviorParameter == 95 && spawned.collisionRadiusXZ == 7 &&
            fixture.context.creations == 1 && fixture.Object().doorLockOrGroupBitmask == 50,
            "Slime factory must reproduce native fields/assets and notify creation exactly once.");
        const auto isolated = fixture.context.worldObjects[3];
        fixture.context.tickCount = 0;
        DispatchObjectCase(fixture.context.worldObjects[3], fixture.context);
        Check(std::memcmp(&isolated, &fixture.context.worldObjects[3], sizeof(isolated)) == 0,
            "Zero projectile ticks must not move slime.");
        auto& slime = fixture.context.worldObjects[3];
        fixture.context.tickCount = 1;
        int x = spawned.posX;
        int y = spawned.posY;
        int vy = 7;
        int phase = 0;
        int airborne = 0;
        while (slime.itemValue == 3 && airborne < 100)
        {
            x = Word(x + 7);
            y = Word(y + vy);
            if (phase == 0) { --vy; }
            phase = (phase + 1) & 1;
            DispatchObjectCase(slime, fixture.context);
            ++airborne;
            if (y >= 0)
            {
                Check(slime.posX == x && slime.posY == y && slime.velocityY == vy &&
                    slime.generalPurpose1 == phase, "Slime must follow the independent half-rate gravity reference.");
            }
        }
        Check(airborne < 100 && slime.itemValue == 103 && slime.posY == 0 &&
            slime.velocityX == 0 && slime.velocityY == 0 && slime.animFrame == 3 && slime.modelFrame2 == 2,
            "Ground impact must stop slime and morph it into the native bank-2 puddle.");
        fixture.context.worldObjects[1].posX = slime.posX;
        fixture.context.worldObjects[1].posY = 0;
        fixture.context.worldObjects[1].posZ = slime.posZ;
        const auto life = fixture.context.playerLife;
        DispatchObjectCase(slime, fixture.context);
        Check(fixture.context.playerLife == life - 95 * 29491 && slime.behaviorParameter == 0 &&
            fixture.context.playerHitEffectTicks == 10, "Slime contact must damage once and clear its damage strength.");
        DispatchObjectCase(slime, fixture.context);
        Check(fixture.context.playerLife == life - 95 * 29491, "Slime must not repeatedly damage the player.");
        fixture.context.tickCount = 40;
        DispatchObjectCase(slime, fixture.context);
        Check(slime.health == 0 && fixture.context.destructions == 1,
            "Puddle dissolving must expire through lifecycle notifications.");

        Fixture full;
        const auto source = fixture.Object();
        for (int slot = 2; slot <= 165; ++slot)
        {
            auto occupied = source;
            occupied.health = 2000;
            occupied.behavior = BehaviorType::None;
            full.context.ReplaceWorldObject(slot, occupied);
        }
        auto spider = spawned;
        spider.behavior = BehaviorType::SpiderLarge;
        spider.health = 1000;
        spider.generalPurpose1 = 0;
        spider.generalPurpose2 = 128;
        spider.winY = 128;
        spider.velocityX = 0;
        spider.velocityY = 1;
        spider.itemValue = 0;
        spider.modelAnim = 1;
        full.context.ReplaceWorldObject(165, spider);
        full.context.randomSeed = seed;
        full.context.tickCount = 1;
        full.context.creations = full.context.destructions = 0;
        DispatchObjectCase(full.context.worldObjects[165], full.context);
        Check(full.context.worldObjects[165].behavior == BehaviorType::FlyingArrow &&
            full.context.worldObjects[165].itemValue == 3 &&
            full.context.worldObjects[165].doorLockOrGroupBitmask == 0 &&
            full.context.creations == 1 && full.context.destructions == 1,
            "Fallback allocator alias must replace the spider without corrupting its new slime.");
    }

    void TestOriginalFixtures(const std::filesystem::path& root)
    {
        class Host final : public Context
        {
        public:
            std::filesystem::path directory;
            using GameContext::LoadLevelGeometry;
        protected:
            std::vector<std::uint8_t> LoadFile(std::string_view filename) override
            {
                Check(filename.substr(0, 6) == "LEVEL\\", "Unexpected original enemy geometry request.");
                std::ifstream file(directory / std::string(filename.substr(6)), std::ios::binary);
                Check(static_cast<bool>(file), "Cannot open original enemy level geometry.");
                return { std::istreambuf_iterator<char>(file), {} };
            }
        } host;
        host.directory = root;
        int count[4]{};
        for (const auto& entry : std::filesystem::directory_iterator(root))
        {
            if (entry.path().filename().string().find("OBJPOS") != 0) { continue; }
            std::ifstream file(entry.path(), std::ios::binary);
            if (!file) { throw std::runtime_error("Cannot open original enemy fixture."); }
            const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)), {});
            LevelLoadData data;
            for (std::size_t offset = 0; offset + 54 <= bytes.size(); offset += 54)
            {
                auto object = DecodeGameObject(bytes.data() + offset, 54);
                const auto type = static_cast<int>(object.behavior);
                if (type < 36 || type > 39 || object.health <= 0) { continue; }
                ++count[type - 36];
                if (!data.level)
                {
                    host.LoadLevelGeometry(static_cast<std::int16_t>(
                        std::stoi(entry.path().stem().string().substr(6))), data);
                    host.level = data.level.get();
                    host.collisionMap = data.level->collisionMap.get();
                    host.floorRepeatHeight = data.floorRepeatHeight;
                }
                Fixture fixture;
                fixture.context.ReplaceWorldObject(2, object);
                fixture.context.tickCount = 1;
                Check(DispatchObjectCase(fixture.Object(), fixture.context).status == DispatchStatus::ExecutedCase,
                    "Every original fixture must route to a recovered handler.");
                object.health = 1000;
                fixture.context.ReplaceWorldObject(2, object);
                Check(DispatchObjectCase(fixture.Object(), fixture.context).status == DispatchStatus::ExecutedCase,
                    "Activated original fixtures must execute their recovered state.");
                host.ReplaceWorldObject(2, object);
                host.soundListenerX = WrapSignedWord(object.posX + 300);
                host.soundListenerZ = object.posZ;
                host.tickCount = 1;
                host.field_D500 = true;
                for (int step = 0; step < 120; ++step)
                {
                    auto& enemy = host.worldObjects[2];
                    if (enemy.health <= 0 || enemy.behavior == BehaviorType::None) { break; }
                    Check(UpdateObject(enemy, host).status == DispatchStatus::ExecutedCase,
                        "Original enemy must execute on its real terrain and collision map.");
                }
            }
        }
        Check(count[0] == 37 && count[1] == 7 && count[2] == 6 && count[3] == 14,
            "Original black/white spider, harvestman, devil and gorilla fixture counts changed.");
    }
}

int main(int argc, char** argv)
{
    try
    {
        Check(argc == 2, "Expected original LEVEL fixture directory.");
        TestTickBoundariesAndContexts();
        TestGorilla();
        TestContactsAndDeath();
        TestAmbushAndDevilAttack();
        TestSlime();
        TestOriginalFixtures(argv[1]);
        std::cout << "Four enemy behaviors and spider slime passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
