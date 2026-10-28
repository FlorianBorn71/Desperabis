#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <utility>

using namespace Desperabis;

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition) { throw std::runtime_error(message); }
    }

    template<typename Exception, typename Action>
    void ExpectError(Action action)
    {
        bool caught = false;
        try { action(); }
        catch (const Exception&) { caught = true; }
        Check(caught, "Expected an explicit weapon-state error.");
    }

    std::int16_t Word(int value)
    {
        const auto bits = static_cast<unsigned int>(value) & 65535u;
        return static_cast<std::int16_t>(bits < 32768u ? static_cast<int>(bits) : static_cast<int>(bits) - 65536);
    }

    void Equip(GameContext& context, WeaponType weapon)
    {
        context.selectedWeapon = context.requestedWeapon = weapon;
        context.weaponHand = context.requestedWeaponHand = 2;
        context.weaponAnimationSpeed = 0;
        context.weaponAnimationPosition = weapon == WeaponType::Sword ? 20 : 10;
        context.weaponReady = true;
        auto& item = context.inventory[1];
        item = {};
        item.health = 1000;
        item.collisionType = CollisionType::Weapon;
        item.itemValue = static_cast<std::int16_t>(weapon);
        context.weaponAmounts[static_cast<std::size_t>(weapon)] = 100;
        context.RefreshWeaponInventory();
        context.worldObjects[1].health = 1000;
        context.worldObjects[1].itemValue = 0;
        context.worldObjects[1].posX = 1000;
        context.worldObjects[1].posY = 45;
        context.worldObjects[1].posZ = 1000;
        context.worldObjects[1].velocityX = 100;
    }

    class Input final : public InputHandler
    {
    public:
        bool attack = false;
        bool dagger = false;
        bool sword = false;
        int calls = 0;
        bool IsKeyPressed(InputKey key) override
        {
            ++calls;
            if (key == InputKey::AttackKey) { return attack; }
            if (key == InputKey::SelectDaggerKey) { return dagger; }
            if (key == InputKey::SelectSwordKey) { return sword; }
            return false;
        }
    };

    class Host final : public GameContext
    {
    protected:
        void LoadLevel(const LevelLoadRequest& request) override
        {
            LevelLoadData data;
            data.level = std::make_unique<Level>(CreateDummyLevel());
            auto& player = data.objects[1];
            player.health = 1000;
            player.posX = player.posZ = 1000;
            player.posY = 20;
            player.collisionRadiusY = 23;
            player.velocityX = 100;
            CompleteLevelLoad(request.id, std::move(data));
        }
    };

    void TestDaggerContactAndRecovery()
    {
        GameContext context;
        Equip(context, WeaponType::Dagger);
        const auto seed = context.randomSeed;
        Check(context.AttackWithSelectedWeapon() == WeaponActionStatus::Applied &&
            context.weaponAnimationSpeed == -3 && context.weaponAmounts[1] == 99,
            "Dagger input must consume one resource and immediately enter the attack sentinel.");
        RawGameObject expected;
        expected.behavior = BehaviorType::WeaponDamageCollider;
        expected.health = 10;
        expected.collisionMask = 254;
        expected.behaviorParameter = 8;
        expected.collisionRadiusXZ = 18;
        expected.collisionRadiusY = 8;
        expected.posX = 1040;
        expected.posY = 38;
        expected.posZ = 1000;
        Check(std::memcmp(&context.worldObjects[2], &expected, sizeof(expected)) == 0 &&
            context.randomSeed == seed, "Dagger contact must match the complete native initialized record.");
        Check(context.AttackWithSelectedWeapon() == WeaponActionStatus::NoEffect,
            "Attack input must not restart a nonzero phase.");
        for (int tick = 1; tick <= 11; ++tick)
        {
            context.UpdateWeaponAnimation(1);
            Check(context.weaponAnimationSpeed == (tick == 11 ? 0 : 40 - tick) &&
                context.weaponAmounts[1] == 100 && context.weaponReady,
                "Dagger recovery must regenerate its resource and skip phase 21..30.");
        }
        Check(context.AttackWithSelectedWeapon() == WeaponActionStatus::Applied &&
            context.worldObjects[3].behavior == BehaviorType::WeaponDamageCollider,
            "A completed dagger recovery must allow another attack without a release edge.");

        GameContext batched;
        Equip(batched, WeaponType::Dagger);
        batched.weaponAnimationSpeed = -3;
        batched.UpdateWeaponAnimation(20);
        Check(batched.weaponAnimationSpeed == 20 && batched.weaponAnimationPosition == 10,
            "Dagger maps the phase once per frame, then decays by the whole tick count.");
        batched.UpdateWeaponAnimation(20);
        Check(batched.weaponAnimationSpeed == 0 && batched.weaponAnimationPosition == 25,
            "Dagger batching must not invent per-tick visual state transitions.");
    }

    void TestDaggerAllPhases()
    {
        GameContext context;
        Equip(context, WeaponType::Dagger);
        for (const int ticks : { 0, 1, 7, 32767 })
        {
            for (int bits = 0; bits < 65536; ++bits)
            {
                const int oldPhase = Word(bits);
                int phase = oldPhase;
                int position = 1234;
                if (oldPhase == -3) { phase = 40; }
                else if (oldPhase == -1) { phase = 20; }
                else if (oldPhase == -2) { position = 1; phase = 6; }
                else if (oldPhase == 0) { position = 10; }
                else if (oldPhase >= 1 && oldPhase <= 5) { position = 10 - oldPhase * 5; }
                else if (oldPhase >= 15 && oldPhase <= 20) { position = (oldPhase - 15) * 5; }
                else if (oldPhase >= 21 && oldPhase <= 30) { phase = 0; }
                else if (oldPhase >= 31 && oldPhase <= 40) { position = (oldPhase - 31) * 6 + 10; }
                else { position = 1; }
                if (phase > 0) { phase -= phase < ticks ? phase : ticks; }
                context.weaponAnimationSpeed = static_cast<std::int16_t>(oldPhase);
                context.weaponAnimationPosition = 1234;
                context.weaponReady = false;
                context.weaponAmounts[1] = -1;
                Check(context.UpdateWeaponAnimation(static_cast<std::int16_t>(ticks)) == WeaponActionStatus::Applied &&
                    context.weaponAnimationSpeed == phase && context.weaponAnimationPosition == position &&
                    context.weaponReady && context.weaponAmounts[1] == 100 && context.weaponSpriteFrame == 1,
                    "All signed dagger phase words must obey the native branch/decay mapping.");
            }
        }
    }

    void TestSwordTiming()
    {
        GameContext context;
        Equip(context, WeaponType::Sword);
        context.worldObjects[2].modelId = 99;
        Check(context.AttackWithSelectedWeapon() == WeaponActionStatus::Applied &&
            context.worldObjects[2].modelId == 0 && context.worldObjects[2].health == 0,
            "Sword input must clear a reclaiming allocation but must not create an immediate contact.");
        for (int tick = 1; tick <= 29; ++tick)
        {
            context.UpdateWeaponAnimation(1);
            Check((context.worldObjects[2].behavior == BehaviorType::WeaponDamageCollider) == (tick >= 11),
                "Sword contact must be created exactly on swing tick 11, not on input.");
            if (tick == 11)
            {
                RawGameObject expected;
                expected.behavior = BehaviorType::WeaponDamageCollider;
                expected.health = 10;
                expected.collisionMask = 254;
                expected.behaviorParameter = 20;
                expected.collisionRadiusXZ = 22;
                expected.collisionRadiusY = 12;
                expected.posX = 1045;
                expected.posY = 40;
                expected.posZ = 1000;
                Check(std::memcmp(&context.worldObjects[2], &expected, sizeof(expected)) == 0 &&
                    context.weaponAnimationPosition == 32 && !context.weaponReady,
                    "Sword contact, strike position and latch must match the complete native record.");
            }
            if (tick == 19)
            {
                Check(context.weaponAnimationSpeed == -2 && context.weaponAnimationPosition == 10,
                    "Sword swing completion must enter the recovery/draw sentinel.");
            }
            if (tick == 12) { Check(context.weaponReady, "Sword refreshes its strike latch on every substep."); }
        }
        Check(context.weaponAnimationSpeed == 0 && context.weaponAnimationPosition == 20 &&
            context.weaponReady && context.weaponAmounts[2] == 100 && context.worldObjects[3].health == 0,
            "Sword must finish recovery at tick 29 without a second contact.");

        GameContext batched;
        Equip(batched, WeaponType::Sword);
        batched.AttackWithSelectedWeapon();
        batched.UpdateWeaponAnimation(29);
        Check(batched.weaponAnimationSpeed == context.weaponAnimationSpeed &&
            batched.weaponAnimationPosition == context.weaponAnimationPosition &&
            std::memcmp(&batched.worldObjects[2], &context.worldObjects[2], sizeof(RawGameObject)) == 0,
            "Sword must execute every substep when a batch crosses its strike and recovery thresholds.");
        batched.UpdateWeaponAnimation(32767);
        Check(batched.weaponAnimationSpeed == 0 && batched.weaponAnimationPosition == 20 &&
            batched.worldObjects[3].health == 0, "Maximum sword tick batches must remain idle after recovery.");
    }

    void TestSwordAllPhases()
    {
        GameContext context;
        Equip(context, WeaponType::Sword);
        for (int bits = 0; bits < 65536; ++bits)
        {
            int phase = Word(bits);
            int position = 31;
            if (phase == 0) { position = 20; }
            else if (phase == -3) { position = 21; phase = 20; }
            else if (phase == -2) { position = 10; phase = 10; }
            else if (phase == -1) { position = 0; phase = 1010; }
            else if (phase == -4) { position = 0; phase = 0; }
            if (phase > 0) { --phase; }
            if (position != 20) { position = Word(position + 1); }
            const bool hit = position == 32;
            if (position >= 40) { phase = -2; position = 10; }
            context.weaponAnimationSpeed = Word(bits);
            context.weaponAnimationPosition = 31;
            context.weaponReady = false;
            context.weaponAmounts[2] = -1;
            context.worldObjects[2] = {};
            context.UpdateWeaponAnimation(1);
            Check(context.weaponAnimationSpeed == phase && context.weaponAnimationPosition == position &&
                context.weaponReady == !hit && context.weaponAmounts[2] == 100 &&
                (context.worldObjects[2].behavior == BehaviorType::WeaponDamageCollider) == hit,
                "All signed sword phase words must obey the native sentinel/substep/strike mapping.");
        }
        context.weaponAnimationSpeed = -3;
        context.weaponAnimationPosition = 123;
        context.weaponReady = false;
        context.weaponAmounts[2] = 9;
        context.UpdateWeaponAnimation(0);
        Check(context.weaponAnimationSpeed == -3 && context.weaponAnimationPosition == 123 &&
            !context.weaponReady && context.weaponAmounts[2] == 9,
            "Unlike dagger, sword zero-tick updates must skip all branch-local state changes.");
        context.weaponAnimationSpeed = 100;
        context.weaponAnimationPosition = 32767;
        context.UpdateWeaponAnimation(1);
        Check(context.weaponAnimationPosition == -32768 && context.weaponAnimationSpeed == 99,
            "Sword position increments must wrap before the signed completion comparison.");
    }

    void TestGatesAndAllocation()
    {
        GameContext context;
        Equip(context, WeaponType::Dagger);
        context.weaponReady = false;
        Check(context.AttackWithSelectedWeapon() == WeaponActionStatus::NoEffect, "False ready latch blocks input.");
        context.weaponReady = true;
        context.positionalSoundsSuppressed = true;
        Check(context.AttackWithSelectedWeapon() == WeaponActionStatus::NoEffect, "DS:D4FA blocks attack input.");
        context.positionalSoundsSuppressed = false;
        context.playerUnderwater = true;
        Check(context.AttackWithSelectedWeapon() == WeaponActionStatus::Applied, "Dagger is allowed underwater.");
        context.weaponAnimationSpeed = 0;
        context.selectedWeapon = WeaponType::Sword;
        context.weaponAmounts[2] = 9;
        Check(context.AttackWithSelectedWeapon() == WeaponActionStatus::NoEffect &&
            context.weaponAmounts[2] == 9, "Underwater sword input must not consume resources or allocate.");
        context.playerUnderwater = false;
        context.selectedWeapon = static_cast<WeaponType>(9);
        const auto objects = context.worldObjects;
        Check(context.AttackWithSelectedWeapon() == WeaponActionStatus::UnimplementedWeapon &&
            context.UpdateWeaponAnimation(1) == WeaponActionStatus::UnimplementedWeapon &&
            std::memcmp(objects.data(), context.worldObjects.data(), sizeof(objects)) == 0,
            "Unrecovered weapons must report their boundary without allocating fake contacts.");
        context.worldObjects[1].itemValue = 1;
        Check(context.UpdateWeaponAnimation(1) == WeaponActionStatus::NoEffect,
            "The player-record itemValue gate must suspend weapon animation.");
        ExpectError<std::invalid_argument>([&] { context.UpdateWeaponAnimation(-1); });

        Equip(context, WeaponType::Dagger);
        context.weaponAmounts[1] = 0;
        Check(context.AttackWithSelectedWeapon() == WeaponActionStatus::Applied &&
            context.weaponAmounts[1] == -1 && !context.weaponReady,
            "Native input trusts the old ready latch, then decrements even a stale zero resource.");

        Equip(context, WeaponType::Sword);
        for (auto& object : context.worldObjects) { object.health = 1000; }
        context.worldObjects[20].health = 9;
        context.worldObjects[20].modelId = 99;
        context.AttackWithSelectedWeapon();
        Check(context.worldObjects[20].health == 0 && context.worldObjects[20].modelId == 0,
            "The otherwise-empty sword input allocation must reclaim the lowest-health ordinary record.");
        context.worldObjects[20].health = 1000;
        context.worldObjects[165].modelId = 99;
        context.UpdateWeaponAnimation(11);
        Check(context.worldObjects[165].behavior == BehaviorType::WeaponDamageCollider &&
            context.worldObjects[165].modelId == 0,
            "Delayed sword contact must use first-free fallback 165 when no ordinary slot is free.");
        Check(UpdateObjects(context)[162].status != DispatchStatus::ExecutedCase &&
            context.worldObjects[165].health == 10, "Fallback contacts are initialized but not normally updated.");
    }

    void TestContactArithmeticAndDebug()
    {
        GameContext context;
        Equip(context, WeaponType::Dagger);
        for (int angle = 0; angle <= 512; ++angle)
        {
            auto& player = context.worldObjects[1];
            GetDirectionDisplacement(player.velocityX, player.velocityZ, static_cast<std::int16_t>(angle), 100);
            context.worldObjects[2] = {};
            context.weaponAnimationSpeed = 0;
            context.weaponAmounts[1] = 100;
            context.weaponReady = true;
            context.AttackWithSelectedWeapon();
            const auto& contact = context.worldObjects[2];
            Check(contact.posX == Word(1000 + Word(player.velocityX * 40) / 100) &&
                contact.posZ == Word(1000 + Word(player.velocityZ * 40) / 100),
                "Every heading must place the dagger contact from the native player direction vector.");
        }
        context.debugWeaponColliders = true;
        context.playerLookHeight = 89;
        context.worldObjects[2] = {};
        context.weaponAnimationSpeed = 0;
        context.AttackWithSelectedWeapon();
        const auto& debug = context.worldObjects[2];
        Check(debug.posY == 32 && debug.modelId == 1 && debug.modelAnim == 1 &&
            static_cast<std::uint8_t>(debug.displayType) == 1 && debug.renderPlacementMode == 64,
            "Negative pitch division must round down and debug contact visualization must use native bytes.");
        context.weaponAnimationSpeed = 0;
        context.worldObjects[1].velocityX = 1000;
        ExpectError<std::overflow_error>([&] { context.AttackWithSelectedWeapon(); });
        context.weaponAnimationSpeed = 0;
        context.worldObjects[1].velocityX = 32767;
        context.worldObjects[2] = {};
        context.AttackWithSelectedWeapon();
        Check(context.worldObjects[2].posX == 1000,
            "Contact placement must truncate the multiplication before signed-byte division.");
    }

    void TestSelectionAndWorldIntegration()
    {
        Host host;
        host.basicPlayerMovementEnabled = false;
        host.StartLevel(11);
        auto& pickup = host.worldObjects[2];
        pickup.health = 1000;
        pickup.collisionType = CollisionType::Weapon;
        pickup.itemValue = 1;
        pickup.pickupType = PickupType::Type_1;
        Check(HandlePlayerObjectInteraction(pickup, host).inventorySlot == 1 &&
            host.inventory[0].itemValue == 1 && host.weaponInventorySlots[1] == 1,
            "An actual dagger pickup must use the first hand and register the weapon.");
        Input input;
        input.attack = true;
        for (int tick = 0; tick < 7; ++tick) { host.Tick({ 1, 1 }, input); }
        Check(host.selectedWeapon == WeaponType::Dagger && host.weaponHand == 1 &&
            host.weaponAnimationSpeed == -3 && host.worldObjects[2].health == 10,
            "Held attack must begin once the picked-up dagger has completed native equip timing.");

        auto& enemy = host.worldObjects[4];
        enemy.health = 1000;
        enemy.collisionType = CollisionType::Skeleton;
        enemy.collisionMask = 2;
        enemy.collisionRadiusXZ = 15;
        enemy.collisionRadiusY = 20;
        enemy.posX = 1040;
        enemy.posY = 20;
        enemy.posZ = 1000;
        enemy.groupIndexOrLock = 60;
        input.attack = false;
        host.Tick({ 1, 1 }, input);
        Check(enemy.groupIndexOrLock == 52 && host.worldObjects[2].behavior == BehaviorType::DriftingParticle &&
            host.worldObjects[2].animFrame == 2,
            "A queued player attack must resolve on the following world pass through shared skeleton damage.");
        for (int tick = 0; tick < 10; ++tick) { host.Tick({ 1, 1 }, input); }
        Check(enemy.groupIndexOrLock == 52, "Impact conversion must prevent repeated damage from the same dagger contact.");

        auto& sword = host.inventory[5];
        sword.health = 1000;
        sword.collisionType = CollisionType::Weapon;
        sword.itemValue = 2;
        host.weaponAmounts[2] = 360;
        host.RefreshWeaponInventory();
        Check(host.SelectWeapon(WeaponType::Sword) && host.inventory[1].itemValue == 2 &&
            host.weaponInventorySlots[2] == 2 && host.weaponAnimationSpeed == -1,
            "Weapon hotkey selection must move a backpack weapon into hand 2 and begin holstering.");
        for (int tick = 0; tick < 17; ++tick) { host.Tick({ 1, 1 }, input); }
        Check(host.selectedWeapon == WeaponType::Sword && host.weaponHand == 2 &&
            host.weaponAnimationSpeed == 0 && host.weaponAnimationPosition == 20,
            "Dagger holstering followed by sword draw must settle into the native sword idle state.");
        Check(!host.SelectWeapon(WeaponType::Sword), "Selecting the current weapon must not restart its animation.");

        input.attack = true;
        auto result = host.Tick({ 1, 1 }, input);
        Check(result.weapon && result.weapon->attack == WeaponActionStatus::Applied,
            "Tick must expose the attack result and consume the host attack key.");
        input.attack = false;
        for (int tick = 0; tick < 11; ++tick) { host.Tick({ 1, 1 }, input); }
        Check(enemy.groupIndexOrLock == 52, "The sword contact must not run in the world pass that created it.");
        host.Tick({ 1, 1 }, input);
        Check(enemy.groupIndexOrLock == 32, "The delayed sword contact must apply exactly 20 damage on the next pass.");
        const auto phase = host.weaponAnimationSpeed;
        const auto calls = input.calls;
        host.SetPaused(true);
        Check(host.Tick({}, input).status == GameTickStatus::Paused &&
            host.weaponAnimationSpeed == phase && input.calls == calls,
            "Paused frames must neither sample attack input nor advance weapon state.");
        host.SetPaused(false);
        const auto inventory = host.inventory;
        host.StartLevel(12);
        Check(host.weaponAnimationSpeed == phase &&
            std::memcmp(inventory.data(), host.inventory.data(), sizeof(inventory)) == 0,
            "Level changes must not clear inventory or reset persistent native weapon globals.");
    }

    void TestSwordAudio()
    {
        class Audio final : public AudioHandler
        {
        public:
            int calls = 0;
            void StopAllSamples() override {}
            std::uint16_t GetSampleCount() const override { return 1; }
            bool IsChannelPlaying(std::size_t) const override { return false; }
            void PlaySample(std::size_t channel, std::uint8_t sample, std::uint8_t volume) override
            {
                Check(channel == 3 && sample == 1 && volume == 255, "Sword strike must play effect 19 at full volume.");
                ++calls;
            }
        } audio;
        GameContext context;
        Equip(context, WeaponType::Sword);
        context.audioHandler = &audio;
        context.soundEffectsLoaded = true;
        context.soundEffects[18].sampleCount = 1;
        context.soundEffects[18].samples[0] = 1;
        context.AttackWithSelectedWeapon();
        context.UpdateWeaponAnimation(10);
        Check(audio.calls == 0, "Sword swing sound must not play at input or before the contact frame.");
        context.UpdateWeaponAnimation(1);
        Check(audio.calls == 1 && context.soundChannelSources[3] == -3 && context.randomSeed == 1,
            "Sword sound must retain native source -3 and consume one sample-selection RNG draw.");
        context.UpdateWeaponAnimation(100);
        Check(audio.calls == 1, "Sword recovery must not replay the strike sound.");
    }

    void TestSelectionGatesAndUnarmedTransition()
    {
        Host host;
        host.basicPlayerMovementEnabled = false;
        host.StartLevel(11);
        for (const int index : { 0, 13 })
        {
            host.inventory[static_cast<std::size_t>(index)].health = 1000;
            host.inventory[static_cast<std::size_t>(index)].collisionType = CollisionType::Weapon;
            host.inventory[static_cast<std::size_t>(index)].itemValue = 1;
        }
        host.inventory[1] = host.inventory[0];
        host.inventory[1].itemValue = 2;
        host.inventory[1].health = 1001;
        host.weaponAmounts[1] = host.weaponAmounts[2] = 360;
        host.RefreshWeaponInventory();
        Check(host.weaponInventorySlots[1] == 14 && host.weaponInventorySlots[2] == 0 &&
            host.requestedWeaponHand == 1,
            "Registry refresh must prefer the last healthy duplicate and ignore health other than 1000.");
        Input input;
        host.Tick({ 1, 1 }, input);
        Check(host.selectedWeapon == WeaponType::Dagger && host.weaponHand == 1,
            "A non-1000 second hand must not override a usable first hand.");
        host.inventory[1].health = 1000;
        host.RefreshWeaponInventory();
        host.Tick({ 1, 1 }, input);
        Check(host.requestedWeapon == WeaponType::Sword && host.requestedWeaponHand == 2 &&
            host.selectedWeapon == WeaponType::Dagger && host.weaponAnimationSpeed == 19 &&
            !host.SelectWeapon(WeaponType::Sword),
            "Second-hand priority must request holstering, and hotkeys must reject a busy phase.");
        host.weaponAnimationSpeed = 0;
        host.weaponAmounts[2] = 0;
        Check(!host.SelectWeapon(WeaponType::Sword), "A depleted weapon must not be selected by hotkey.");
        host.weaponAmounts[2] = 360;
        Check(!host.SelectWeapon(WeaponType::Dagger), "Selecting the current type must not swap duplicates.");
        host.selectedWeapon = host.requestedWeapon = WeaponType::Sword;
        host.weaponHand = host.requestedWeaponHand = 2;
        input.dagger = true;
        host.Tick({ 1, 1 }, input);
        Check(host.inventory[1].itemValue == 1 && host.inventory[13].itemValue == 2 &&
            host.weaponAnimationSpeed == -1 && host.requestedWeapon == WeaponType::Dagger,
            "Tick must route the dagger hotkey through the native backpack-to-second-hand swap.");
        host.weaponAnimationSpeed = 0;
        host.weaponInventorySlots[1] = 15;
        ExpectError<std::out_of_range>([&] { host.SelectWeapon(WeaponType::Dagger); });
        ExpectError<std::out_of_range>([&] { host.SelectWeapon(static_cast<WeaponType>(32)); });
        host.inventory[0].itemValue = 32;
        ExpectError<std::out_of_range>([&] { host.RefreshWeaponInventory(); });

        GameContext unarmed;
        unarmed.worldObjects[1].posX = unarmed.worldObjects[1].posZ = 1000;
        unarmed.worldObjects[1].posY = 45;
        unarmed.worldObjects[1].velocityX = 100;
        unarmed.UpdateWeaponAnimation(1);
        Check(unarmed.AttackWithSelectedWeapon() == WeaponActionStatus::Applied,
            "The unarmed bridge into weapon selection must preserve native attack input.");
        unarmed.UpdateWeaponAnimation(3);
        Check(unarmed.worldObjects[2].health == 0, "Unarmed contact must wait for its countdown crossing.");
        unarmed.UpdateWeaponAnimation(1);
        Check(unarmed.worldObjects[2].health == 3 && unarmed.worldObjects[2].behaviorParameter == 4 &&
            unarmed.worldObjects[2].posX == 1030,
            "The native unarmed threshold must create one short-lived contact.");
        unarmed.field_D500 = false;
        unarmed.weaponAnimationSpeed = -3;
        unarmed.UpdateWeaponAnimation(1);
        Check(unarmed.weaponAnimationSpeed == -3, "DS:D500 must gate the unarmed branch without resetting its command.");
    }
}

int main()
{
    try
    {
        TestDaggerContactAndRecovery();
        TestDaggerAllPhases();
        TestSwordTiming();
        TestSwordAllPhases();
        TestGatesAndAllocation();
        TestContactArithmeticAndDebug();
        TestSelectionAndWorldIntegration();
        TestSwordAudio();
        TestSelectionGatesAndUnarmedTransition();
        std::cout << "Weapon sequence tests passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
