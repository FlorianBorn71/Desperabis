#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace Desperabis;

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition) { throw std::runtime_error(message); }
    }

    struct Fixture
    {
        Level level = CreateDummyLevel();
        GameContext context;

        Fixture()
        {
            context.level = &level;
            context.collisionMap = level.collisionMap.get();
            context.soundListenerX = 1800;
            context.soundListenerZ = 2000;
            auto& object = Object();
            object.behavior = BehaviorType::Skeleton;
            object.collisionType = CollisionType::Skeleton;
            object.modelId = 38;
            object.health = 1000;
            object.posX = 1500;
            object.posZ = 2000;
            object.collisionRadiusY = 20;
            object.groupIndexOrLock = 60;
            object.animFrame = 1;
            object.modelFrame2 = 2;
            object.behaviorParameter = 20;
        }

        RawGameObject& Object() { return context.worldObjects[2]; }

        void Fill(std::uint8_t value)
        {
            std::fill(std::begin(level.collisionMap->data), std::end(level.collisionMap->data), value);
        }
    };

    unsigned int Next(std::uint32_t& seed, unsigned int range)
    {
        seed = static_cast<std::uint32_t>((static_cast<std::uint64_t>(seed) * 134775813u + 1) & 0xffffffffu);
        return range == 0 ? 0 : (seed >> 16) % range;
    }

    std::int16_t Word(std::int64_t value)
    {
        const auto bits = (value % 65536 + 65536) % 65536;
        return static_cast<std::int16_t>(bits >= 32768 ? bits - 65536 : bits);
    }

    void ReferenceStep(RawGameObject& object, std::uint32_t& seed, int distance, std::int32_t& life)
    {
        object.posY = object.collisionRadiusY;
        if (object.itemValue == 0)
        {
            object.posX = Word(object.posX + object.velocityX);
            object.posZ = Word(object.posZ + object.velocityZ);
        }
        const int phase = Word(100 * object.animFrame + object.blendWeightOrSpriteOpacity);
        if (((distance >= 38 && distance <= 60) && object.itemValue == 0) || object.doorLockOrGroupBitmask > 0)
        {
            object.blendWeightOrSpriteOpacity = 0;
            object.itemValue = static_cast<std::int16_t>(4 + Next(seed, 2));
            object.generalPurpose2 = 0;
            object.animFrame = 1;
            object.modelFrame2 = 2;
        }
        int target = object.velocityX || object.velocityZ ?
            GetVectorAngle(object.velocityX, object.velocityZ) : object.winY;
        target = Word(target - Word(Word(std::abs(Word(phase - 250))) - 125) / 6);
        const auto steer = [&](int& heading)
        {
            heading &= 511;
            int current = (static_cast<std::uint16_t>(object.winY) % 512) + 512;
            while (std::abs(current - heading) > 256) { current -= 512; }
            return current;
        };
        int current = steer(target);
        const int speed = 11 + Next(seed, 8);
        object.winY = Word(current + std::clamp(target - current, -speed, speed));
        object.modelAnim = static_cast<std::uint8_t>(((object.winY + 85) & 511) / 171 + 1);
        if (object.itemValue > 0) { object.modelAnim = static_cast<std::uint8_t>(object.itemValue); }
        object.doorLockOrGroupBitmask = 0;
        object.generalPurpose2 = static_cast<std::uint16_t>(object.generalPurpose2 + Next(seed, 5));
        if (object.itemValue == 0)
        {
            object.blendWeightOrSpriteOpacity = Word(object.blendWeightOrSpriteOpacity + 10 + Next(seed, 8));
        }
        else
        {
            if (object.animFrame == 4)
            {
                const int acceleration = Word(object.generalPurpose2);
                object.generalPurpose2 = static_cast<std::uint16_t>(acceleration + std::clamp(1 - acceleration, -3, 3));
            }
            object.blendWeightOrSpriteOpacity = Word(object.blendWeightOrSpriteOpacity + Word(object.generalPurpose2));
            target = Word(GetVectorAngle(Word(1500 + distance - object.posX), Word(2000 - object.posZ)) +
                Word(phase - 350) / 3);
            current = steer(target);
            object.winY = Word(current + std::clamp(target - current, -19, 19));
        }
        const int advances = object.blendWeightOrSpriteOpacity > 100 ?
            (object.blendWeightOrSpriteOpacity - 1) / 100 : 0;
        if (advances != 0)
        {
            object.blendWeightOrSpriteOpacity = Word(object.blendWeightOrSpriteOpacity - advances * 100);
            object.animFrame = static_cast<std::uint8_t>((object.animFrame + advances - 1) % 4 + 1);
            object.modelFrame2 = static_cast<std::uint8_t>(object.animFrame % 4 + 1);
            if (object.itemValue == 0 || object.animFrame != 4) { object.generalPurpose2 = 3; }
            if (object.itemValue > 0)
            {
                if (object.animFrame == 4 && distance < 72) { life -= 29491 * object.behaviorParameter; }
                else if (object.animFrame == 1) { object.itemValue = 0; }
            }
        }
    }

    void TestNativeArithmetic()
    {
        Fixture f;
        for (unsigned int sample = 0; sample < 65536; ++sample)
        {
            RawGameObject expected;
            expected.behavior = BehaviorType::Skeleton;
            expected.collisionType = CollisionType::Skeleton;
            expected.health = 1000;
            expected.posX = 1500;
            expected.posZ = 2000;
            expected.velocityX = static_cast<std::int16_t>(static_cast<int>(sample % 9) - 4);
            expected.velocityZ = static_cast<std::int16_t>(static_cast<int>((sample / 9) % 9) - 4);
            expected.collisionRadiusY = 23;
            expected.winY = Word(sample);
            expected.itemValue = sample % 3 == 0 ? 0 : sample % 3 == 1 ? 4 : 5;
            expected.animFrame = static_cast<std::uint8_t>(sample % 4 + 1);
            expected.modelFrame2 = 35;
            expected.blendWeightOrSpriteOpacity = Word(sample * 17);
            expected.generalPurpose2 = static_cast<std::uint16_t>(sample % 200);
            expected.doorLockOrGroupBitmask = sample % 11 == 0 ? 1 : 0;
            expected.behaviorParameter = static_cast<std::uint8_t>(sample % 256);
            expected.renderPlacementMode = 0xa5;
            expected.generalPurpose3 = 0xbeef;
            expected.groupIndexOrLock = 60;
            expected.spriteScale = 41;
            f.Object() = expected;
            const int distance = sample % 100;
            f.context.soundListenerX = static_cast<std::int16_t>(1500 + distance);
            f.context.playerLife = 360 * 65536;
            auto life = f.context.playerLife;
            f.context.randomSeed = sample * 65537u;
            auto seed = f.context.randomSeed;
            ReferenceStep(expected, seed, distance, life);
            Function_Skeleton(f.Object(), f.context);
            Check(std::memcmp(&expected, &f.Object(), sizeof(expected)) == 0 &&
                f.context.randomSeed == seed && f.context.playerLife == life,
                "Skeleton full-record, signed-word, RNG or fixed-point oracle differs.");
        }
    }

    class Audio final : public AudioHandler
    {
    public:
        std::vector<std::pair<std::uint8_t, std::uint8_t>> calls;
        std::uint16_t GetSampleCount() const override { return 129; }
        bool IsChannelPlaying(std::size_t) const override { return false; }
        void PlaySample(std::size_t, std::uint8_t sample, std::uint8_t volume) override
        {
            calls.emplace_back(sample, volume);
        }
    };

    void TestAudio()
    {
        Fixture f;
        Audio audio;
        f.context.audioHandler = &audio;
        f.context.soundEffectsLoaded = true;
        for (std::size_t i = 0; i < f.context.soundEffects.size(); ++i)
        {
            f.context.soundEffects[i].sampleCount = 1;
            f.context.soundEffects[i].samples[0] = static_cast<std::uint8_t>(i + 1);
        }
        f.Object().velocityX = 2;
        f.Object().animFrame = 2;
        f.Object().blendWeightOrSpriteOpacity = 100;
        f.context.soundListenerX = 1650;
        Function_Skeleton(f.Object(), f.context);
        Check(audio.calls.size() == 1 && audio.calls[0].first == 106 && audio.calls[0].second == 130,
            "Footsteps must use the dispatch-entry cached distance, scaled by three.");
        audio.calls.clear();
        f.Object().posX = 1500;
        f.Object().itemValue = 4;
        f.Object().animFrame = 2;
        f.Object().generalPurpose2 = 1;
        f.Object().blendWeightOrSpriteOpacity = 100;
        Function_Skeleton(f.Object(), f.context);
        Check(audio.calls.size() == 1 && audio.calls[0].first == 19,
            "Sword swing sound must occur on the transition to attack frame three.");
    }

    void TestMovement()
    {
        for (const int ticks : { 0,1,2,10,100 })
        {
            Fixture f;
            f.Object().velocityX = 2;
            f.context.tickCount = static_cast<std::int16_t>(ticks);
            Check(Function_Skeleton(f.Object(), f.context).status == DispatchStatus::ExecutedCase,
                "Skeleton must report its recovered case.");
            Check(f.Object().posX == 1500 + ticks * 2 && f.Object().posZ == 2000 &&
                f.Object().posY == (ticks == 0 ? 0 : 20), "Skeleton movement/terrain/tick count differs.");
        }
        for (const int health : { 1,999,1001,1010,2999,3901 })
        {
            Fixture f;
            f.Object().health = static_cast<std::int16_t>(health);
            const auto before = f.Object();
            const auto seed = f.context.randomSeed;
            Function_Skeleton(f.Object(), f.context);
            Check(std::memcmp(&before, &f.Object(), sizeof(before)) == 0 && seed == f.context.randomSeed,
                "Skeleton must honor the exact native health gates.");
        }
        Fixture disabled;
        disabled.context.objectCollisionsEnabled = false;
        disabled.Object().velocityZ = 2;
        Function_Skeleton(disabled.Object(), disabled.context);
        Check(disabled.Object().posZ == 2002, "Skeleton movement is not gated by DS:D500.");

        for (const int offset : { -4,-3,0,3,4 })
        {
            Fixture f;
            f.Object().posX = static_cast<std::int16_t>(1550 + offset);
            f.Object().posZ = 2050;
            f.context.soundListenerX = 1900;
            f.context.soundListenerZ = 2050;
            Function_Skeleton(f.Object(), f.context);
            Check(f.Object().velocityX == (std::abs(offset) < 4 ? 2 : 0) && f.Object().velocityZ == 0,
                "Tile-center direction selection must use strict truncated distance <2.");
        }
        Fixture blocked;
        blocked.Fill(0x38);
        blocked.Object().posX = 1550;
        blocked.Object().posZ = 2050;
        Function_Skeleton(blocked.Object(), blocked.context);
        Check(blocked.Object().velocityX == 0 && blocked.Object().velocityZ == 0,
            "Skeleton must stop when all four three-sample paths are blocked.");

        Fixture suppressed;
        suppressed.context.positionalSoundsSuppressed = true;
        suppressed.Object().posX = 1550;
        suppressed.Object().posZ = 2050;
        suppressed.context.randomSeed = 0;
        Function_Skeleton(suppressed.Object(), suppressed.context);
        Check(suppressed.Object().velocityX == 0 && suppressed.Object().velocityZ == 0,
            "Suppression must suppress pursuit, not only sound.");

        Fixture cached;
        cached.Object().velocityX = 2;
        cached.context.soundListenerX = 1561;
        cached.context.tickCount = 4;
        Function_Skeleton(cached.Object(), cached.context);
        Check(cached.Object().posX == 1508 && cached.Object().itemValue == 0,
            "Attack entry must retain the distance cached before all substeps.");
    }

    void TestPathSelection()
    {
        constexpr int directions[4][2] = { {0,1}, {1,0}, {0,-1}, {-1,0} };
        Fixture f;
        for (int depth = 1; depth <= 3; ++depth)
        {
            for (int mask = 0; mask < 16; ++mask)
            {
                f.Fill(0);
                for (int direction = 0; direction < 4; ++direction)
                {
                    if ((mask & (1 << direction)) != 0)
                    {
                        f.context.SetCollisionMapPixelAtWorldXZ(
                            static_cast<std::int16_t>(1550 + directions[direction][0] * depth * 25),
                            static_cast<std::int16_t>(2050 + directions[direction][1] * depth * 25), 0x38);
                    }
                }
                for (unsigned int sample = 0; sample < 256; ++sample)
                {
                    RawGameObject object;
                    object.behavior = BehaviorType::Skeleton;
                    object.health = 1000;
                    object.posX = 1550;
                    object.posZ = 2050;
                    object.animFrame = 1;
                    f.Object() = object;
                    f.context.randomSeed = sample * 65537u;
                    f.context.soundListenerX = 1850;
                    f.context.soundListenerZ = 2050;
                    auto seed = f.context.randomSeed;
                    Next(seed, 8);
                    Next(seed, 5);
                    Next(seed, 8);
                    const int first = Next(seed, 4);
                    int selected = (mask & 2) != 0 ? -1 : 1;
                    if (selected == -1 || Next(seed, 8) == 0)
                    {
                        for (int index = first; index < first + 4; ++index)
                        {
                            if ((mask & (1 << (index % 4))) == 0) { selected = index % 4; }
                        }
                    }
                    Function_Skeleton(f.Object(), f.context);
                    Check(f.Object().velocityX == (selected == -1 ? 0 : directions[selected][0] * 2) &&
                        f.Object().velocityZ == (selected == -1 ? 0 : directions[selected][1] * 2) &&
                        f.context.randomSeed == seed,
                        "Skeleton three-probe obstacles, last-clear cardinal priority or RNG short circuit differs.");
                }
            }
        }
    }

    void ArmStrike(Fixture& f, int distance, int frame = 3)
    {
        f.context.soundListenerX = static_cast<std::int16_t>(1500 + distance);
        f.context.soundListenerZ = 2000;
        f.Object().itemValue = 4;
        f.Object().animFrame = static_cast<std::uint8_t>(frame);
        f.Object().modelFrame2 = static_cast<std::uint8_t>(frame + 1);
        f.Object().blendWeightOrSpriteOpacity = 100;
        f.Object().generalPurpose2 = 1;
    }

    void TestCombat()
    {
        for (const int distance : { 37,38,60,61 })
        {
            Fixture f;
            f.context.soundListenerX = static_cast<std::int16_t>(1500 + distance);
            Function_Skeleton(f.Object(), f.context);
            Check((f.Object().itemValue > 0) == (distance > 37 && distance < 61),
                "Skeleton attack-entry distance endpoints differ.");
        }
        for (const int distance : { 0,71,72,200 })
        {
            Fixture f;
            ArmStrike(f, distance);
            const auto life = f.context.playerLife;
            Function_Skeleton(f.Object(), f.context);
            Check(f.Object().animFrame == 4 && f.context.playerLife ==
                life - (distance < 72 ? 29491 * 20 : 0),
                "Skeleton frame-four strike must use cached distance and the native signed 16.16 constant.");
            Check(f.context.worldObjects[1].health == 0,
                "Skeleton damage must not target player-object health.");
        }
        for (const auto weapon : { WeaponType::None, WeaponType::Dagger, WeaponType::Sword })
        {
            for (const int phase : { 29,30,40,41 })
            {
                Fixture f;
                ArmStrike(f, 50);
                f.context.selectedWeapon = weapon;
                f.context.weaponAnimationPosition = static_cast<std::int16_t>(phase);
                const auto life = f.context.playerLife;
                Function_Skeleton(f.Object(), f.context);
                const bool blocked = weapon == WeaponType::Sword && phase > 29 && phase < 41;
                Check(f.context.playerLife == life - (blocked ? 0 : 29491 * 20) &&
                    f.context.weaponAnimationSpeed == (blocked ? -4 : 0), "Sword parry window differs.");
            }
        }
        Fixture noAdvance;
        ArmStrike(noAdvance, 50);
        noAdvance.Object().blendWeightOrSpriteOpacity = 0;
        const auto life = noAdvance.context.playerLife;
        Function_Skeleton(noAdvance.Object(), noAdvance.context);
        Check(noAdvance.context.playerLife == life, "Strike must require an animation advance.");

        Fixture wrap;
        ArmStrike(wrap, 100, 4);
        Function_Skeleton(wrap.Object(), wrap.context);
        Check(wrap.Object().animFrame == 1 && wrap.Object().itemValue == 0,
            "Attack animation must return to movement on frame one.");

        Fixture invulnerable;
        ArmStrike(invulnerable, 50);
        invulnerable.context.playerLifeDeltaSuppressed = 1;
        const auto invulnerableLife = invulnerable.context.playerLife;
        Function_Skeleton(invulnerable.Object(), invulnerable.context);
        Check(invulnerable.context.playerLife == invulnerableLife && invulnerable.context.playerHitEffectTicks == 5,
            "Invulnerability suppresses the meter delta, not the hit-effect assignment.");

        GameContext meter;
        meter.ApplyPlayerLifeDelta(100000);
        Check(meter.playerLife == 360 * 65536, "Player life must clamp to its native maximum.");
        meter.ApplyPlayerLifeDelta(-360 * 65536);
        Check(meter.playerLife == 0 && meter.playerPostureDelta == -1 && meter.positionalSoundsSuppressed &&
            meter.playerHitEffectTicks == 1000 && meter.worldObjects[1].velocityZ == 400,
            "Meter depletion must install the recovered player-death state.");
        meter.playerHitEffectTicks = 1;
        meter.ApplyPlayerLifeDelta(-1);
        Check(meter.playerHitEffectTicks == 1, "The native suppression flag prevents duplicate death setup.");
    }

    void TestDeath()
    {
        Fixture f;
        f.Object().health = 3000;
        const auto original = f.Object();
        Function_Skeleton(f.Object(), f.context);
        Check(f.Object().health == 0 && f.Object().groupIndexOrLock == -940, "Skeleton death damage differs.");
        constexpr std::array<int, 8> parts{ 1,2,3,6,4,260,4,260 };
        for (std::size_t i = 0; i < parts.size(); ++i)
        {
            const auto& part = f.context.worldObjects[i + 3];
            Check(part.health == 1000 && part.behavior == BehaviorType::BouncingObject,
                "Skeleton death must create all eight native fragment records.");
            if (parts[i] == 6)
            {
                Check(part.modelId == 39 && part.modelAnim == 2 && part.animFrame == 2 &&
                    part.collisionType == CollisionType::Weapon && part.itemValue == 2 &&
                    part.collisionRadiusXZ == 15 && part.pickupType == PickupType::Type_40 &&
                    part.spriteScale == 41 && part.soundEffectId == 39,
                    "Part six must be the recoverable sword, not another bone.");
            }
            else
            {
                Check(part.modelId == original.modelId && part.modelAnim == (parts[i] & 255) &&
                    part.winY == parts[i] && part.generalPurpose1 == 8 && part.animFrame == 5 &&
                    part.modelFrame2 == (parts[i] == 4 || parts[i] == 260 ? 35 : 0),
                    "Body-part identity/mirrored angle/default bytes differ.");
            }
        }
        for (int step = 0; step < 100; ++step) { UpdateObjects(f.context); }
        for (std::size_t slot = 3; slot <= 10; ++slot)
        {
            Check(f.context.worldObjects[slot].behavior == BehaviorType::None,
                "Spawned skeleton body parts/sword must bounce and settle.");
        }
        Fixture countdown;
        countdown.Object().health = 3051;
        Function_Skeleton(countdown.Object(), countdown.context);
        Check(countdown.Object().health == 3050, "Delayed death countdown differs.");
        Function_Skeleton(countdown.Object(), countdown.context);
        Check(countdown.Object().health == 0, "3050 must enter the damage path before the countdown path.");

        Fixture exhausted;
        for (auto& object : exhausted.context.worldObjects) { object.health = 1000; }
        exhausted.Object().health = 3000;
        Function_Skeleton(exhausted.Object(), exhausted.context);
        Check(exhausted.Object().health == 0 && exhausted.context.worldObjects[165].modelAnim == 4 &&
            exhausted.context.worldObjects[165].winY == 260,
            "Exhaustion must repeatedly overwrite the native fallback slot.");
    }

    void TestWater()
    {
        Fixture f;
        f.Fill(1);
        auto& fragment = f.context.worldObjects[3];
        f.context.SpawnSkeletonFragment(f.Object(), 1, 0, 0, 0);
        fragment.posY = -1;
        fragment.velocityY = -4;
        Check(Function_BouncingObject(fragment, f.context).status == DispatchStatus::ExecutedCase &&
            fragment.health == 5 && fragment.behavior == BehaviorType::None,
            "Skeleton fragments hitting ordinary water must make a short-lived resting remnant.");
        auto& impact = f.context.worldObjects[4];
        Check(impact.behavior == BehaviorType::WaterImpact && impact.modelId == 39 && impact.modelAnim == 4,
            "Water contact must allocate the native splash animation.");
        for (int step = 0; step < 100; ++step) { Function_WaterImpact(impact, f.context); }
        Check(impact.health == 0, "Water animation must expire.");

        Fixture unsupported;
        auto& object = unsupported.Object();
        object.behavior = BehaviorType::BouncingObject;
        object.modelId = 1;
        const auto before = object;
        Check(Function_BouncingObject(object, unsupported.context).status == DispatchStatus::UnimplementedHandler &&
            std::memcmp(&before, &object, sizeof(before)) == 0,
            "Unrecovered non-skeleton fragment paths must remain explicitly unsupported.");
    }

    void TestRuntime()
    {
        class Host final : public GameContext
        {
            void LoadLevel(const LevelLoadRequest& request) override
            {
                LevelLoadData data;
                data.level = std::make_unique<Level>(CreateDummyLevel());
                data.objects[1].health = 1000;
                data.objects[1].posX = 1550;
                data.objects[1].posY = 20;
                data.objects[1].posZ = 2000;
                data.objects[1].collisionRadiusY = 23;
                auto& skeleton = data.objects[2];
                skeleton.behavior = BehaviorType::Skeleton;
                skeleton.collisionType = CollisionType::Skeleton;
                skeleton.modelId = 38;
                skeleton.health = 1000;
                skeleton.posX = 1500;
                skeleton.posZ = 2000;
                skeleton.itemValue = 4;
                skeleton.animFrame = 3;
                skeleton.generalPurpose2 = 1;
                skeleton.blendWeightOrSpriteOpacity = 100;
                skeleton.behaviorParameter = 20;
                CompleteLevelLoad(request.id, std::move(data));
            }
        };
        class Input final : public InputHandler
        {
            bool IsKeyPressed(InputKey key) override { return key == InputKey::ForwardKey; }
        };
        Host host;
        Input input;
        host.StartLevel(11);
        host.playerLife = 65536;
        host.SetPaused(true);
        host.Tick({ 1,1 }, input);
        Check(host.playerLife == 65536, "Pausing must prevent skeleton damage.");
        host.SetPaused(false);
        Check(host.Tick({ 1,1 }, input).status == GameTickStatus::Advanced &&
            host.playerLife == 0 && host.playerPostureDelta == -1 && host.worldObjects[1].velocityZ == 400,
            "The installed game tick must dispatch the attack and preserve player-death setup.");
        const auto player = host.worldObjects[1];
        host.Tick({ 8,8 }, input);
        Check(host.worldObjects[1].posX == player.posX && host.worldObjects[1].posZ == player.posZ &&
            host.worldObjects[1].velocityZ == 400,
            "Normal player input must not overwrite the recovered death state.");
    }

    void TestOriginalRecords(const std::filesystem::path& directory)
    {
        int count = 0;
        for (const auto& entry : std::filesystem::directory_iterator(directory))
        {
            const auto name = entry.path().filename().string();
            if (name.rfind("OBJPOS", 0) != 0 || entry.path().extension() != ".DAT") { continue; }
            std::ifstream input(entry.path(), std::ios::binary);
            if (!input) { throw std::runtime_error("Cannot read original object records."); }
            const std::vector<std::uint8_t> bytes{ std::istreambuf_iterator<char>(input), {} };
            for (std::size_t offset = 0; offset + sizeof(RawGameObject) <= bytes.size(); offset += sizeof(RawGameObject))
            {
                auto object = DecodeGameObject(bytes.data() + offset, sizeof(RawGameObject));
                if (object.behavior != BehaviorType::Skeleton) { continue; }
                ++count;
                Fixture f;
                f.Object() = object;
                Check(Function_Skeleton(f.Object(), f.context).status == DispatchStatus::ExecutedCase,
                    "Original skeleton record failed.");
                f.Object().health = 3000;
                Function_Skeleton(f.Object(), f.context);
                Check(f.Object().health == 0, "Original skeleton death failed.");
            }
        }
        Check(count == 15, "Expected all fifteen original skeleton records.");
    }
}

int main(int argc, char** argv)
{
    try
    {
        Check(argc == 2, "Expected the original LEVEL directory.");
        TestNativeArithmetic();
        TestAudio();
        TestMovement();
        TestPathSelection();
        TestCombat();
        TestDeath();
        TestWater();
        TestRuntime();
        TestOriginalRecords(argv[1]);
        std::cout << "Skeleton movement, combat, death, fragments and original records passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
