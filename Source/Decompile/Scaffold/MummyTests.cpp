#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <algorithm>
#include <cstdlib>
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

    std::int16_t Word(std::int64_t value)
    {
        const auto bits = (value % 65536 + 65536) % 65536;
        return static_cast<std::int16_t>(bits >= 32768 ? bits - 65536 : bits);
    }

    unsigned int Next(std::uint32_t& seed, unsigned int range)
    {
        seed = static_cast<std::uint32_t>(static_cast<std::uint64_t>(seed) * 134775813u + 1);
        return range == 0 ? 0 : (seed >> 16) % range;
    }

    int Signed(std::uint32_t& seed, unsigned int radius)
    {
        const int result = static_cast<int>(Next(seed, 2 * radius + 1)) - static_cast<int>(radius);
        return result == 0 ? static_cast<int>(radius) : result;
    }

    int Step(int current, int target, int speed)
    {
        const int delta = Word(target - current);
        return Word(std::abs(delta)) <= speed ? delta : ((delta > 0) - (delta < 0)) * speed;
    }

    void Direction(RawGameObject& object, int speed)
    {
        const auto high = [](std::int64_t product)
        {
            return product >= 0 ? product / 65536 : -((-product + 65535) / 65536);
        };
        const auto heading = object.generalPurpose2 & 511u;
        object.velocityX = Word(high(ClampedSineTable[heading] * static_cast<std::int64_t>(speed)));
        object.velocityZ = Word(-high(ClampedSineTable[(heading + 128) & 511u] * static_cast<std::int64_t>(speed)));
    }

    RawGameObject Mummy()
    {
        RawGameObject object;
        object.behavior = BehaviorType::Mummie;
        object.health = 1000;
        object.modelId = 35;
        object.modelAnim = 1;
        object.animFrame = 1;
        object.modelFrame2 = 11;
        object.posX = 1500;
        object.posY = 30;
        object.posZ = 2000;
        object.collisionType = CollisionType::PushingEnemy;
        object.collisionMask = 254;
        object.collisionRadiusXZ = 20;
        object.collisionRadiusY = 30;
        object.displayType = DisplayType::VectorNormal;
        object.groupIndexOrLock = 50;
        object.behaviorParameter = 2;
        object.generalPurpose2 = 65535;
        return object;
    }

    class Context final : public GameContext
    {
    public:
        int created = 0;
        int destroyed = 0;
        int lifeChanges = 0;
        std::string message;
        void OnObjectCreated(int) override { ++created; }
        void OnObjectDestroyed(int) override { ++destroyed; }
        void OnPlayerHealthChanged(std::int32_t, std::int32_t) override { ++lifeChanges; }
        void PromptUserMessage(std::string_view text, std::int16_t) override { message = text; }
    };

    struct Fixture
    {
        Level level = CreateDummyLevel();
        Context context;
        Fixture()
        {
            context.level = &level;
            context.collisionMap = level.collisionMap.get();
            context.soundListenerX = 1800;
            context.soundListenerZ = 2000;
            context.worldObjects[2] = Mummy();
            context.worldObjects[1] = Mummy();
            context.worldObjects[1].behavior = BehaviorType::None;
            context.worldObjects[1].collisionMask = 255;
        }
        RawGameObject& Object() { return context.worldObjects[2]; }
    };

    RawGameObject Reference(RawGameObject object, std::uint32_t& seed, int ticks,
        int listenerX, bool movement, std::int32_t& life)
    {
        for (int tick = 0; tick < ticks; ++tick)
        {
            if (object.health >= 1001) { continue; }
            if (object.generalPurpose1 == 0)
            {
                const int oldX = object.posX;
                const int oldZ = object.posZ;
                const auto oldBlend = object.blendWeightOrSpriteOpacity;
                const int distance = Word(Word(std::abs(Word(listenerX - object.posX))) +
                    Word(std::abs(Word(2000 - object.posZ))));
                if (object.displayType == DisplayType::Hidden)
                {
                    object.displayType = DisplayType::VectorNormal;
                    object.collisionMask = 254;
                }
                if (object.doorLockOrGroupBitmask < 10) { object.doorLockOrGroupBitmask = 11; }
                if (Next(seed, 80) == 0) { object.velocityX = 0; object.velocityZ = 0; }
                object.posY = 30;
                object.spriteScale = static_cast<std::uint8_t>((object.spriteScale + 1) & 1);
                if (Next(seed, 80) == 0 || distance < 150)
                {
                    object.generalPurpose2 = static_cast<std::uint16_t>(
                        GetVectorAngle(Word(listenerX - object.posX), Word(2000 - object.posZ)));
                }
                if (Next(seed, 15) == 0 || Word(object.generalPurpose2) < 0)
                {
                    object.generalPurpose2 = static_cast<std::uint16_t>((object.winY + Signed(seed, 20)) & 511);
                    Direction(object, 3);
                }
                if (object.velocityX != 0 || object.velocityZ != 0)
                {
                    object.blendWeightOrSpriteOpacity = Word(object.blendWeightOrSpriteOpacity + 8);
                }
                if (distance < 70) { life -= 29491 * object.behaviorParameter; }
                else if (movement && object.spriteScale == 0)
                {
                    object.posX = Word(object.posX + object.velocityX);
                    object.posY = Word(object.posY + object.velocityY);
                    object.posZ = Word(object.posZ + object.velocityZ);
                }
                Signed(seed, 1);
                int current = (object.winY & 511) + 512;
                const int target = object.generalPurpose2 & 511;
                while (std::abs(current - target) > 256) { current -= 512; }
                object.generalPurpose2 = static_cast<std::uint16_t>(target);
                object.winY = Word(current + Step(current, target, 9));
                if (object.posX == oldX && object.posZ == oldZ && distance > 69 && object.spriteScale == 0)
                {
                    object.blendWeightOrSpriteOpacity = oldBlend;
                }
                while (object.blendWeightOrSpriteOpacity > 100)
                {
                    object.blendWeightOrSpriteOpacity -= 100;
                    const int frame = object.modelFrame2;
                    object.animFrame = static_cast<std::uint8_t>(frame % 5);
                    if (frame > 10) { object.modelAnim = static_cast<std::uint8_t>(frame / 5 - 1); }
                    object.modelFrame2 = static_cast<std::uint8_t>(
                        object.animFrame % 4 + object.doorLockOrGroupBitmask);
                    object.doorLockOrGroupBitmask = distance < 180 ? 16 : 11;
                }
            }
            else if (object.generalPurpose1 == 1)
            {
                object.blendWeightOrSpriteOpacity = Word(object.blendWeightOrSpriteOpacity + object.itemValue);
                object.itemValue = Word(object.itemValue + 1);
                if (Word(object.generalPurpose2) > 0)
                {
                    --object.generalPurpose2;
                    object.blendWeightOrSpriteOpacity = 0;
                    object.itemValue = 0;
                }
                else if (object.blendWeightOrSpriteOpacity > 100)
                {
                    object.blendWeightOrSpriteOpacity = 0;
                    if (object.animFrame < 2)
                    {
                        object.generalPurpose2 = 20;
                        ++object.animFrame;
                        object.modelFrame2 = static_cast<std::uint8_t>(object.animFrame + 1);
                    }
                    else
                    {
                        object.animFrame = 3;
                        object.modelFrame2 = 0;
                        object.generalPurpose1 = 2;
                        object.health = 250;
                        object.generalPurpose2 = 0;
                    }
                }
            }
            else if (object.generalPurpose1 == 2)
            {
                object.itemValue = static_cast<std::int16_t>(std::max(35 - object.health / 6, 30));
                ++object.generalPurpose2;
                object.displayType = static_cast<DisplayType>(object.itemValue);
                const auto heading = object.generalPurpose2;
                object.generalPurpose2 = static_cast<std::uint16_t>(object.winY);
                Direction(object, static_cast<int>(Next(seed, 50)));
                object.generalPurpose2 = heading;
            }
        }
        return object;
    }

    void TestOracle()
    {
        Fixture fixture;
        for (int sample = 0; sample < 4096; ++sample)
        {
            auto object = Mummy();
            const int state = sample / 4 % 4;
            object.generalPurpose1 = static_cast<std::uint16_t>(state % 3);
            object.health = state == 3 ? 1001 : state == 2 ? static_cast<std::int16_t>(40 + sample % 200) : 1000;
            object.posY = Word(sample * 61);
            object.velocityX = Word(sample % 7 - 3);
            object.velocityY = Word(sample * 37);
            object.velocityZ = Word(sample % 9 - 4);
            object.winY = Word(sample * 113);
            object.generalPurpose2 = static_cast<std::uint16_t>(sample * 239);
            object.generalPurpose3 = 0xcafe;
            object.spriteScale = static_cast<std::uint8_t>(sample);
            object.displayType = sample % 7 == 0 ? DisplayType::Hidden : DisplayType::VectorNormal;
            object.itemValue = Word(sample * 137);
            object.modelFrame2 = static_cast<std::uint8_t>(sample);
            object.animFrame = static_cast<std::uint8_t>(sample % 5);
            object.doorLockOrGroupBitmask = Word(sample * 79);
            object.blendWeightOrSpriteOpacity = Word(sample * 71);
            fixture.Object() = object;
            fixture.context.tickCount = static_cast<std::int16_t>(sample % 4);
            fixture.context.field_D500 = sample % 3 != 0;
            fixture.context.soundListenerX = static_cast<std::int16_t>(1500 + sample % 220);
            fixture.context.playerLife = 360 * 65536;
            fixture.context.randomSeed = static_cast<std::uint32_t>(sample) * 65537u;
            auto seed = fixture.context.randomSeed;
            auto life = fixture.context.playerLife;
            const auto expected = Reference(object, seed, fixture.context.tickCount,
                fixture.context.soundListenerX, fixture.context.field_D500, life);
            Check(DispatchObjectCase(fixture.Object(), fixture.context).status == DispatchStatus::ExecutedCase,
                "Mummy dispatch must report the recovered case.");
            if (std::memcmp(&fixture.Object(), &expected, sizeof(expected)) != 0 ||
                fixture.context.randomSeed != seed || fixture.context.playerLife != life)
            {
                std::cerr << "Mummy oracle sample " << sample << '\n';
                throw std::runtime_error("Mummy record, RNG or player life differs from the native reference.");
            }
        }
    }

    void TestDamageAndDeath()
    {
        for (const auto ticks : {1, 2, 3, 7})
        {
            Fixture fixture;
            fixture.context.tickCount = static_cast<std::int16_t>(ticks);
            fixture.context.DamageObject(fixture.Object(), 50);
            Check(fixture.Object().generalPurpose1 == 1 && fixture.Object().modelAnim == 3 &&
                fixture.Object().collisionMask == 0 && fixture.Object().renderPlacementMode == 3,
                "Lethal mummy damage must enter the existing nonblocking collapse route.");
            for (int step = 0; step < 100 && fixture.Object().generalPurpose1 == 1; ++step)
            {
                DispatchObjectCase(fixture.Object(), fixture.context);
            }
            Check(fixture.Object().health == 250 && fixture.Object().generalPurpose1 == 2 &&
                fixture.Object().animFrame == 3 && fixture.Object().modelFrame2 == 0,
                "Collapse must finish in the 250-tick fading state.");
            for (int step = 0; step < 300 && fixture.Object().health > 0; ++step)
            {
                Check(UpdateObject(fixture.Object(), fixture.context).status == DispatchStatus::ExecutedCase,
                    "Fading must remain a recovered mummy case.");
            }
            Check(fixture.Object().health == 0 && fixture.context.destroyed == 1 && fixture.context.created > 0,
                "Batched countdown must remove the mummy once and create smoke through lifecycle hooks.");
        }
        Fixture fixture;
        fixture.Object().health = 1;
        fixture.Object().generalPurpose1 = 2;
        fixture.Object().generalPurpose2 = 3;
        UpdateObject(fixture.Object(), fixture.context);
        Check(fixture.Object().health == 0 && fixture.Object().behavior == BehaviorType::None &&
            fixture.context.destroyed == 1 && fixture.context.created == 0,
            "Final smoke allocation must not resurrect the expired source or query its cleared coordinates.");
        for (const auto distance : {69, 70, 149, 150, 179, 180})
        {
            Fixture hit;
            hit.context.soundListenerX = static_cast<std::int16_t>(1500 + distance);
            hit.context.playerLife = 90 * 65536;
            DispatchObjectCase(hit.Object(), hit.context);
            Check(hit.context.playerLife == 90 * 65536 - (distance < 70 ? 58982 : 0) &&
                hit.context.lifeChanges == (distance < 70 ? 1 : 0),
                "Mummy contact damage must use the strict distance threshold and notify the health HUD.");
        }
        Fixture depleted;
        depleted.context.soundListenerX = 1500;
        depleted.context.playerLife = 1;
        DispatchObjectCase(depleted.Object(), depleted.context);
        Check(depleted.context.playerLife == 0 && depleted.context.lifeChanges == 1 &&
            depleted.context.playerHitEffectTicks == 8,
            "Mummy contact must clamp depleted life and retain the native post-damage hit timer.");
        Fixture suppressed;
        suppressed.context.soundListenerX = 1500;
        suppressed.context.playerLifeDeltaSuppressed = 1;
        DispatchObjectCase(suppressed.Object(), suppressed.context);
        Check(suppressed.context.playerLife == 360 * 65536 && suppressed.context.lifeChanges == 0 &&
            suppressed.context.playerHitEffectTicks == 8,
            "Suppressed mummy damage must preserve life while retaining the native hit effect.");
        suppressed.context.playerLifeDeltaSuppressed = 0;
        suppressed.Object().behaviorParameter = 0;
        DispatchObjectCase(suppressed.Object(), suppressed.context);
        Check(suppressed.context.lifeChanges == 0 && suppressed.context.playerHitEffectTicks == 8,
            "Zero damage factors must not issue a false health callback.");
    }

    void TestGuards()
    {
        Fixture fixture;
        auto& object = fixture.Object();
        object.behavior = BehaviorType::MummieGuard;
        object.soundEffectId = 15;
        object.itemValue = 1;
        object.modelFrame2 = 2;
        object.blendWeightOrSpriteOpacity = 0;
        object.collisionMask = 1;
        DispatchObjectCase(object, fixture.context);
        Check(object.itemValue == 0 && object.collisionMask == 1 && object.blendWeightOrSpriteOpacity == 0,
            "Guard contact must finish its countdown before opening on a later frame.");
        fixture.context.tickCount = 3;
        DispatchObjectCase(object, fixture.context);
        Check(object.collisionMask == 0 && object.blendWeightOrSpriteOpacity == 51,
            "Guard opening must advance once per frame by 17 times the behavior ticks.");
        DispatchObjectCase(object, fixture.context);
        Check(object.blendWeightOrSpriteOpacity == 100 && object.animFrame == 2 && object.behavior == BehaviorType::None,
            "An opened guard must retain its final pose and stop its behavior.");

        for (const auto item : {-32768, -1, 0, 1, 110, 32767})
        {
            for (const auto blend : {-32768, -1, 0, 99, 100, 32767})
            {
                for (const auto ticks : {0, 1, 3, 1927, 32767})
                {
                    object.behavior = BehaviorType::MummieGuard;
                    object.itemValue = static_cast<std::int16_t>(item);
                    object.collisionMask = 1;
                    object.blendWeightOrSpriteOpacity = static_cast<std::int16_t>(blend);
                    fixture.context.tickCount = static_cast<std::int16_t>(ticks);
                    DispatchObjectCase(object, fixture.context);
                    Check(object.itemValue == (item < 1 ? item : Word(item - ticks)) &&
                        object.blendWeightOrSpriteOpacity ==
                            (item < 1 ? Word(blend + Step(blend, 100, Word(ticks * 17))) : blend),
                        "Guard countdown/opening must retain signed-word overflow and zero-tick semantics.");
                }
            }
        }
        object.itemValue = 110;
        fixture.context.tickCount = 3;
        fixture.context.worldObjects[1].posX = 1700;
        DispatchObjectCase(object, fixture.context);
        Check(object.itemValue == 110, "Nonoverlapping players must not advance the guard countdown.");

        class Audio final : public AudioHandler
        {
        public:
            int calls = 0;
            std::uint8_t lastVolume = 0;
            std::uint16_t GetSampleCount() const override { return 1; }
            bool IsChannelPlaying(std::size_t) const override { return false; }
            void PlaySample(std::size_t, std::uint8_t sample, std::uint8_t volume) override
            {
                Check(sample == 1, "Guard opening selected the wrong sample.");
                ++calls;
                lastVolume = volume;
            }
            void StopAllSamples() override {}
        } audio;
        fixture.context.audioHandler = &audio;
        fixture.context.soundEffectsLoaded = true;
        fixture.context.soundEffects[14] = {1, {1, 0, 0, 0}};
        object.itemValue = 0;
        object.blendWeightOrSpriteOpacity = 0;
        fixture.context.tickCount = 0;
        DispatchObjectCase(object, fixture.context);
        Check(audio.calls == 1 && audio.lastVolume == 255 && object.blendWeightOrSpriteOpacity == 0 &&
            fixture.context.soundChannelSources[3] == 2,
            "Guard opening must request full-volume sound even on a zero-tick frame.");
    }

    void TestWallsAndGates()
    {
        Fixture fixture;
        std::fill(std::begin(fixture.level.collisionMap->data), std::end(fixture.level.collisionMap->data), std::uint8_t{1});
        fixture.Object().velocityX = 3;
        fixture.Object().generalPurpose2 = 128;
        fixture.context.randomSeed = 1;
        DispatchObjectCase(fixture.Object(), fixture.context);
        Check(fixture.Object().velocityX == 3 && fixture.Object().velocityZ == 0,
            "Nonblocking center material bits must not stop mummy motion after the native 0x38 mask.");
        std::fill(std::begin(fixture.level.collisionMap->data), std::end(fixture.level.collisionMap->data), std::uint8_t{0x38});
        fixture.context.randomSeed = 1;
        DispatchObjectCase(fixture.Object(), fixture.context);
        Check(fixture.Object().velocityX == 0 && fixture.Object().velocityZ == 0,
            "A blocking center must stop mummy velocity.");
        const auto before = fixture.Object();
        fixture.context.behaviorDispatchEnabled = false;
        Check(DispatchObjectCase(fixture.Object(), fixture.context).status == DispatchStatus::DispatchDisabled &&
            std::memcmp(&before, &fixture.Object(), sizeof(before)) == 0,
            "Disabled mummy dispatch must preserve the record.");
        fixture.context.behaviorDispatchEnabled = true;
        fixture.Object() = Mummy();
        fixture.Object().health = 1001;
        fixture.context.ActivateObjectGroup(1);
        const auto dormant = fixture.Object();
        const auto seed = fixture.context.randomSeed;
        DispatchObjectCase(fixture.Object(), fixture.context);
        Check(std::memcmp(&dormant, &fixture.Object(), sizeof(dormant)) == 0 && fixture.context.randomSeed == seed,
            "The native damage/group activation route must not bypass the ordinary mummy's 1001 gate.");
    }

    void TestActivationTriggers()
    {
        class Input final : public InputHandler
        {
        public:
            int calls = 0;
            bool IsKeyPressed(InputKey) override { ++calls; return false; }
        } input;
        Fixture fixture;
        auto& context = fixture.context;
        auto& trigger = fixture.Object();
        trigger.behavior = BehaviorType::None;
        trigger.collisionType = CollisionType::TriggerActivation;
        trigger.collisionMask = 1;
        trigger.displayType = DisplayType::Hidden;
        trigger.itemValue = 1;
        for (const int slot : {3, 4, 5, 6, 140, 141, 164})
        {
            context.worldObjects[slot] = Mummy();
            context.worldObjects[slot].health = slot == 4 ? 1002 : 1001;
        }
        context.worldObjects[5].collisionType = CollisionType::RatAndSnake;
        context.worldObjects[6].groupIndexOrLock = 0;
        context.behaviorDispatchEnabled = false;
        Check(UpdatePlayerInteraction(context, input).status == InteractionStatus::NoEffect &&
            trigger.health == 1000 && context.worldObjects[3].health == 1001 && input.calls == 0,
            "Disabled automatic activation must neither consume the trigger nor query interaction input.");
        context.behaviorDispatchEnabled = true;
        context.objectCollisionsEnabled = false;
        Check(UpdatePlayerInteraction(context, input).status == InteractionStatus::NoEffect && trigger.health == 1000,
            "Disabled collision queries must not activate triggers.");
        context.objectCollisionsEnabled = true;
        context.worldObjects[1].posX = 1540;
        Check(UpdatePlayerInteraction(context, input).status == InteractionStatus::NoEffect && trigger.health == 1000,
            "Touching a trigger's boundary without strict overlap must not activate it.");
        context.worldObjects[1].posX = 1500;
        Check(UpdatePlayerInteraction(context, input).status == InteractionStatus::Applied &&
            trigger.health == 0 && context.destroyed == 1 && context.created == 0 && input.calls == 0,
            "Walking into the trigger must consume it through the lifecycle hook without an interaction key.");
        Check(context.worldObjects[3].health == 1000 && context.worldObjects[140].health == 1000 &&
            context.worldObjects[4].health == 1002 && context.worldObjects[141].health == 1001 &&
            context.worldObjects[164].health == 1001,
            "Activation volumes must force only their matching group in inclusive slots 2..140 to state 1000.");
        Check(context.worldObjects[5].collisionType == CollisionType::RatAndSnake &&
            context.worldObjects[5].modelAnim == 1 && context.worldObjects[5].health == 1000 &&
            context.worldObjects[6].generalPurpose1 == 1 && context.worldObjects[6].health == 1000,
            "The trigger must run zero-damage handling before, not after or instead of, forcing state 1000.");

        Fixture persistent;
        persistent.Object().collisionType = CollisionType::DeadBullHead;
        persistent.Object().collisionMask = 1;
        persistent.Object().itemValue = 1;
        persistent.context.worldObjects[3] = Mummy();
        persistent.context.worldObjects[3].health = 1001;
        UpdatePlayerInteraction(persistent.context, input);
        Check(persistent.Object().collisionType == CollisionType::None && persistent.Object().health == 1000 &&
            persistent.context.destroyed == 0 && persistent.context.worldObjects[3].health == 1000,
            "The bull-head subtype must remain alive while losing its one-shot trigger collision type.");
        persistent.context.worldObjects[3].health = 1001;
        UpdatePlayerInteraction(persistent.context, input);
        Check(persistent.context.worldObjects[3].health == 1001,
            "A consumed persistent trigger must not reactivate its group on the next contact.");

        Fixture wrapped;
        wrapped.Object().collisionType = CollisionType::TriggerActivation;
        wrapped.Object().itemValue = 31768;
        wrapped.context.worldObjects[3] = Mummy();
        wrapped.context.worldObjects[3].health = -32768;
        wrapped.context.HandleActivationTriggerContact(wrapped.Object());
        Check(wrapped.context.worldObjects[3].health == 1000 && wrapped.context.created == 1 &&
            wrapped.context.destroyed == 1,
            "Trigger group addition must wrap as a signed word and preserve inactive-to-active lifecycle hooks.");
    }

    void TestAssets(const std::filesystem::path& directory)
    {
        class Host final : public GameContext
        {
        public:
            std::filesystem::path directory;
            using GameContext::LoadLevelGeometry;
        protected:
            std::vector<std::uint8_t> LoadFile(std::string_view filename) override
            {
                Check(filename.substr(0, 6) == "LEVEL\\", "Unexpected mummy fixture asset request.");
                std::ifstream stream(directory / std::string(filename.substr(6)), std::ios::binary);
                Check(static_cast<bool>(stream), "Cannot open original mummy level geometry.");
                return {std::istreambuf_iterator<char>(stream), {}};
            }
        } host;
        host.directory = directory;
        int ordinary = 0;
        int dormant = 0;
        int guards = 0;
        for (const auto levelNumber : {12, 20, 26})
        {
            std::ifstream stream(directory / ("OBJPOS" + std::to_string(levelNumber) + ".DAT"), std::ios::binary);
            Check(static_cast<bool>(stream), "Cannot read original mummy object table.");
            LevelLoadData data;
            host.LoadLevelGeometry(static_cast<std::int16_t>(levelNumber), data);
            host.level = data.level.get();
            host.collisionMap = data.level->collisionMap.get();
            host.floorRepeatHeight = data.floorRepeatHeight;
            RawGameObject object;
            while (stream.read(reinterpret_cast<char*>(&object), sizeof(object)))
            {
                if (object.behavior != BehaviorType::Mummie && object.behavior != BehaviorType::MummieGuard) { continue; }
                host.worldObjects[2] = object;
                host.worldObjects[1] = object;
                host.worldObjects[1].behavior = BehaviorType::None;
                host.soundListenerX = object.posX;
                host.soundListenerZ = object.posZ;
                host.playerLife = 360 * 65536;
                host.positionalSoundsSuppressed = false;
                if (object.behavior == BehaviorType::Mummie) { ++ordinary; }
                else { ++guards; }
                if (object.health >= 1001) { ++dormant; }
                for (int step = 0; step < 120; ++step)
                {
                    if (host.worldObjects[2].behavior == BehaviorType::None) { break; }
                    Check(UpdateObject(host.worldObjects[2], host).status == DispatchStatus::ExecutedCase,
                        "An original mummy/guard must execute a recovered route on its real map.");
                }
                if (object.health >= 1001)
                {
                    Check(std::memcmp(&object, &host.worldObjects[2], sizeof(object)) == 0,
                        "Original dormant mummy records must remain unchanged.");
                }
                else if (object.behavior == BehaviorType::MummieGuard)
                {
                    Check(host.worldObjects[2].behavior == BehaviorType::None && host.worldObjects[2].collisionMask == 0,
                        "Both original guards must open and become nonblocking.");
                }
                else { Check(host.worldObjects[2].behavior == BehaviorType::Mummie, "An original mummy became stuck."); }
            }
            Check(stream.eof(), "Original mummy object-table read failed.");
        }
        Check(ordinary == 9 && dormant == 4 && guards == 2, "Unexpected original mummy/guard fixture counts.");

        LevelLoadData activationData;
        host.LoadLevelGeometry(12, activationData);
        host.level = activationData.level.get();
        host.collisionMap = activationData.level->collisionMap.get();
        std::ifstream objects(directory / "OBJPOS12.DAT", std::ios::binary);
        Check(static_cast<bool>(objects), "Cannot read original level-12 activation fixtures.");
        host.worldObjects = {};
        std::size_t slot = 2;
        while (slot < host.worldObjects.size() &&
            objects.read(reinterpret_cast<char*>(&host.worldObjects[slot]), sizeof(RawGameObject))) { ++slot; }
        Check(objects.eof(), "Original level-12 activation object-table read failed.");
        const auto trigger = host.worldObjects[22];
        Check(trigger.collisionType == CollisionType::TriggerActivation && trigger.itemValue == 1 &&
            trigger.displayType == DisplayType::Hidden && trigger.posX == 2050 && trigger.posZ == 3050,
            "Unexpected original level-12 mummy activation volume.");
        host.worldObjects[1] = trigger;
        host.worldObjects[1].collisionType = CollisionType::None;
        host.worldObjects[1].behavior = BehaviorType::None;
        class Input final : public InputHandler
        {
            bool IsKeyPressed(InputKey) override
            {
                throw std::runtime_error("The original mummy trigger must not require interaction input.");
            }
        } input;
        Check(UpdatePlayerInteraction(host, input).status == InteractionStatus::Applied &&
            host.playerCollisionSlot == 22 && host.worldObjects[22].health == 0,
            "The actual hidden level-12 trigger must fire on player overlap.");
        for (const int mummySlot : {13, 19, 21})
        {
            auto& mummy = host.worldObjects[mummySlot];
            Check(mummy.health == 1000, "The original group-1 mummy must wake through trigger contact.");
            const auto before = mummy;
            host.tickCount = 1;
            Check(DispatchObjectCase(mummy, host).status == DispatchStatus::ExecutedCase &&
                std::memcmp(&before, &mummy, sizeof(mummy)) != 0,
                "Each newly awakened original mummy must execute its alive behavior on the real map.");
        }
    }

    void TestRuntime()
    {
        class Host final : public GameContext
        {
        protected:
            void LoadLevel(const LevelLoadRequest&) override {}
        } host;
        class Input final : public InputHandler
        {
            bool IsKeyPressed(InputKey) override { return false; }
        } input;
        const auto request = host.StartLevel(12);
        LevelLoadData data;
        data.level = std::make_unique<Level>(CreateDummyLevel());
        data.objects[2] = Mummy();
        data.objects[2].health = 1001;
        auto& trigger = data.objects[3];
        trigger.health = 1000;
        trigger.collisionType = CollisionType::TriggerActivation;
        trigger.collisionMask = 1;
        trigger.displayType = DisplayType::Hidden;
        trigger.posX = 1800;
        trigger.posY = 20;
        trigger.posZ = 2000;
        trigger.collisionRadiusXZ = 20;
        trigger.collisionRadiusY = 30;
        trigger.itemValue = 1;
        data.playerSpawn = PlayerCamera{{1800, 45, 2000}, {0, 0, 100}};
        host.CompleteLevelLoad(request.id, std::move(data));
        host.basicPlayerMovementEnabled = false;
        host.worldObjects[2].velocityZ = -3;
        Check(host.Tick({0, 0}, input).status == GameTickStatus::Advanced &&
            host.worldObjects[3].health == 0 && host.worldObjects[2].health == 1000 &&
            host.worldObjects[2].posZ == 2000,
            "Runtime contact must wake a dormant mummy without input or movement ticks.");
        Check(host.Tick({3, 9}, input).status == GameTickStatus::Advanced &&
            host.worldObjects[2].spriteScale == 1 && host.worldObjects[2].posZ < 2000,
            "Runtime must advance alternating mummy movement through behavior ticks.");
        const auto before = host.worldObjects[2];
        host.Tick({0, 100}, input);
        Check(std::memcmp(&before, &host.worldObjects[2], sizeof(before)) == 0,
            "Frame/burning ticks alone must not advance mummy behavior.");
    }
}

int main(int argc, char** argv)
{
    try
    {
        Check(argc == 2, "Expected original LEVEL directory.");
        TestOracle();
        TestDamageAndDeath();
        TestGuards();
        TestWallsAndGates();
        TestActivationTriggers();
        TestAssets(argv[1]);
        TestRuntime();
        std::cout << "Mummy tests passed (4096 oracle cases, nine original mummies and two guards).\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
