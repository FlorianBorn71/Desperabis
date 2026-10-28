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

    class Context final : public GameContext
    {
    public:
        std::int16_t bottom = 8;
        std::int16_t GetSpriteBottom(const RawGameObject&) const override { return bottom; }
    };

    struct Fixture
    {
        Level level = CreateDummyLevel();
        Context context;
        Fixture()
        {
            context.level = &level;
            context.collisionMap = level.collisionMap.get();
            level.heightSurfaces[0] = Level::HeightSurface{ 0, 0 };
            std::memset(level.ceilingObjects, 0, sizeof(level.ceilingObjects));
            auto& player = context.worldObjects[1];
            player.health = 1000;
            player.posX = player.posZ = 1000;
            player.posY = 120;
            player.velocityX = 100;
            context.soundListenerX = context.soundListenerZ = 1000;
            context.lightingCache.Reset(1000, 1000);
        }

        void Equip(WeaponType weapon, std::int16_t amount = 100)
        {
            context.selectedWeapon = context.requestedWeapon = weapon;
            context.weaponHand = context.requestedWeaponHand = 2;
            context.weaponAnimationSpeed = 0;
            context.weaponReady = true;
            auto& held = context.inventory[1];
            held = {};
            held.health = 1000; held.collisionType = CollisionType::Weapon;
            held.itemValue = static_cast<std::int16_t>(weapon);
            held.field_24 = static_cast<std::uint16_t>(amount);
            context.weaponAmounts[static_cast<std::size_t>(weapon)] = amount;
            context.RefreshWeaponInventory();
        }

        RawGameObject& Enemy(std::uint16_t slot, std::int16_t x, std::int16_t y = 100)
        {
            auto& enemy = context.worldObjects[slot];
            enemy = {};
            enemy.health = 1000; enemy.collisionType = CollisionType::Skeleton;
            enemy.behavior = BehaviorType::Skeleton; enemy.field_07 = 2;
            enemy.groupIndexOrLock = 200;
            enemy.posX = x; enemy.posY = y; enemy.posZ = 1000;
            enemy.collisionRadiusXZ = 12; enemy.collisionRadiusY = 20;
            return enemy;
        }
    };

    void TestCrossbow()
    {
        Fixture f;
        auto& c = f.context;
        f.Equip(WeaponType::Crossbow, 2);
        c.worldObjects[1].velocityX = -99;
        Check(c.AttackWithSelectedWeapon() == WeaponActionStatus::Applied, "Crossbow input must fire.");
        auto& bolt = c.worldObjects[2];
        Check(bolt.behavior == BehaviorType::CrossbowBolt && bolt.health == 1000 &&
            bolt.modelId == 31 && bolt.modelAnim == 1 && bolt.animFrame == 2 &&
            bolt.collisionType == CollisionType::Ammo && bolt.itemValue == 3 &&
            bolt.velocityX == -24 && bolt.velocityY == 0 && bolt.winX == 21 &&
            bolt.posX == 1000 && bolt.posY == 100 && bolt.posZ == 1000 &&
            bolt.collisionRadiusXZ == 1 && bolt.collisionRadiusY == 1 &&
            c.weaponAmounts[3] == 1, "Crossbow factory must preserve native signed division and pickup fields.");
        c.UpdateWeaponAnimation(1);
        Check(c.weaponAnimationSpeed == 49 && !c.weaponReady, "Crossbow must enter its reload countdown.");
        c.UpdateWeaponAnimation(20);
        c.UpdateWeaponAnimation(1);
        Check(c.weaponReady && c.weaponAnimationPosition == 1, "Reload latch becomes true in the 11..29 phase.");
        c.tickCount = 1;
        DispatchObjectCase(bolt, c);
        Check(bolt.posX == 976, "Bolt must integrate its previous X velocity.");
        Check(bolt.posY == 100, "Bolt must integrate its previous Y velocity.");
        Check(bolt.velocityY == 5, "Bolt must calculate its next gravity step from winX.");
        Check(bolt.winX == 20, "Bolt must decrement winX each flight step.");

        Fixture hit;
        hit.Equip(WeaponType::Crossbow);
        hit.context.arrowPoison = 100;
        auto& enemy = hit.Enemy(20, 1025);
        hit.context.AttackWithSelectedWeapon();
        auto& attached = hit.context.worldObjects[2];
        DispatchObjectCase(attached, hit.context);
        Check(attached.health == 2000 && attached.field_24 == 20 && attached.animFrame == 0 &&
            attached.field_28 == 100 && enemy.groupIndexOrLock == 151, "Bolt must deal 49 and attach to its target.");
        enemy.posX = 1100;
        DispatchObjectCase(attached, hit.context);
        Check(attached.posX == 1100 && enemy.groupIndexOrLock == 150, "Attached poison follows the target and deals damage.");
        enemy.behavior = BehaviorType::None;
        DispatchObjectCase(attached, hit.context);
        Check(attached.health == 1000 && attached.animFrame == 2 && attached.field_24 == 1 &&
            attached.velocityX == 0 && attached.winX == -3, "A bolt detaches when its target stops being active.");
        attached.posY = -1; attached.velocityY = 0;
        DispatchObjectCase(attached, hit.context);
        Check(attached.behavior == BehaviorType::None && attached.field_32 == 63 &&
            attached.collisionRadiusXZ == 14 && attached.field_07 == 2, "A settled bolt becomes recoverable ammunition.");

        Fixture wall;
        wall.Equip(WeaponType::Crossbow);
        wall.context.SetCollisionMapPixelAtWorldXZ(1012, 1000, 8);
        wall.context.AttackWithSelectedWeapon();
        Check(wall.context.worldObjects[2].velocityX == 0 && wall.context.worldObjects[2].field_24 == 1,
            "Crossbow muzzle probes must stop bolts fired directly into a wall.");
    }

    void TestFireball()
    {
        Fixture f;
        auto& c = f.context;
        f.Equip(WeaponType::Fireball, 2);
        Check(c.AttackWithSelectedWeapon() == WeaponActionStatus::Applied, "Fireball input must create a charge.");
        auto& ball = c.worldObjects[2];
        Check(ball.behavior == BehaviorType::FireballProjectile && ball.health == 38 &&
            ball.collisionType == CollisionType::Type_22 && ball.field_26 == 100 &&
            ball.spriteScale == 45 && ball.behaviorParameter == 50 && ball.velocityX == 25 &&
            c.inventory[1].field_24 == 1, "Fireball factory must persist the held item's remaining charge.");
        c.tickCount = 12;
        c.worldObjects[1].velocityX = -99;
        DispatchObjectCase(ball, c);
        Check(ball.groupIndexOrLock == 12 && ball.posX == 1000 && ball.velocityX == -25,
            "Fireball windup follows the player using SAR, without advancing flight in the same dispatch.");
        c.tickCount = 1;
        DispatchObjectCase(ball, c);
        Check(ball.posX == 975 && ball.blendWeightOrSpriteOpacity == 20 && ball.doorLockOrGroupBitmask == 1,
            "Fireball launch must move, light its surroundings and begin SCHUSS1's ten-frame cycle.");
        bool lit = false;
        for (const auto& row : c.lightingCache.contributions)
        {
            for (const auto value : row) { lit = lit || value != 0; }
        }
        Check(lit, "A flying fireball must add the native directional light stencil.");
        for (int step = 0; step < 20; ++step) { DispatchObjectCase(ball, c); }
        Check(ball.health == 0 && ball.doorLockOrGroupBitmask == 1, "Fireball expires when the flattened animation wraps.");
        c.UpdateWeaponAnimation(1);
        Check(c.weaponAnimationSpeed == 118 && c.weaponReady && c.weaponAmounts[4] == 1,
            "Fireball attack animation must decay twice as fast and retain item-local charges.");
        c.weaponAnimationSpeed = 0;
        c.AttackWithSelectedWeapon();
        c.UpdateWeaponAnimation(1);
        Check(c.inventory[1].health == 0 && !c.weaponReady, "The last charge removes its held item.");

        Fixture hit;
        hit.Equip(WeaponType::Fireball);
        auto& enemy = hit.Enemy(20, 1025);
        hit.context.AttackWithSelectedWeapon();
        auto& projectile = hit.context.worldObjects[2];
        projectile.groupIndexOrLock = 12;
        DispatchObjectCase(projectile, hit.context);
        Check(enemy.health == 3250 && static_cast<int>(enemy.displayType) == 80 && projectile.health == 0,
            "Fireballs put skeletons into their native burning death state, not generic 100-point damage.");
    }

    void TestFireballBreakup(const std::filesystem::path& directory)
    {
        std::ifstream stream(directory / "ANIM4.AN2", std::ios::binary);
        if (!stream) { throw std::runtime_error("Cannot read the demo level's original common sprite bank."); }
        const std::vector<std::uint8_t> sprites(std::istreambuf_iterator<char>(stream), {});
        Check(sprites.at(0) == 6 && sprites.at(1) == 3, "ANIM4 must contain six rows with three frames each.");
        std::array<std::int16_t, 2> flameBottoms{};
        size_t offset = 2;
        for (size_t frame = 0; frame < 14; ++frame)
        {
            const auto width = sprites.at(offset + 4) + 256u * sprites.at(offset + 5) + 1;
            const auto height = sprites.at(offset + 6) + 256u * sprites.at(offset + 7);
            if (frame >= 12)
            {
                flameBottoms[frame - 12] = WrapSignedWord(
                    WrapSignedWord(sprites.at(offset + 2) + 256 * sprites.at(offset + 3)) + height);
            }
            offset += 8 + width * (height + 1);
        }
        Check(flameBottoms[0] == 16 && flameBottoms[1] == 11,
            "Use the native breakup flame bounds from row five of the level-mapped sprite bank.");
        Fixture f;
        auto& c = f.context;
        f.Equip(WeaponType::Fireball);
        c.AttackWithSelectedWeapon();
        auto& ball = c.worldObjects[2];
        ball.posY = 10;
        c.DamageObject(ball, 0);
        Check(ball.health == 0, "Terrain impact must consume the parent fireball.");
        for (size_t slot = 3; slot <= 11; ++slot)
        {
            const auto& particle = c.worldObjects[slot];
            Check(particle.behavior == BehaviorType::BouncingObject && particle.field_24 == 1 &&
                particle.modelId == 1 && particle.modelAnim == 5 && particle.animFrame >= 1 &&
                particle.animFrame <= 2 && particle.health >= 50 && particle.health <= 99 &&
                particle.groupIndexOrLock == 5 && particle.itemValue == 75 &&
                particle.spriteScale == 24, "Explosion must create nine native bouncing flame particles.");
        }
        bool faded = false;
        for (int tick = 0; tick < 110; ++tick)
        {
            c.lightingCache.contributions = {};
            for (size_t slot = 3; slot <= 11; ++slot)
            {
                auto& particle = c.worldObjects[slot];
                const auto before = particle.health;
                const auto wasFading = particle.behavior == BehaviorType::FadingParticle;
                c.bottom = flameBottoms.at(particle.animFrame - 1);
                const auto result = UpdateObject(particle, c);
                Check(result.status != DispatchStatus::UnimplementedHandler,
                    "Every live explosion particle must have a recovered update route.");
                if (before > 0) { Check(particle.health == before - 1, "Particles must retain the native lifetime countdown."); }
                if (particle.behavior == BehaviorType::FadingParticle)
                {
                    faded = true;
                    if (before > 0 && wasFading)
                    {
                        Check(particle.blendWeightOrSpriteOpacity == ArithmeticShiftRight(particle.health, 2),
                            "A settled flame must fade with its remaining lifetime.");
                    }
                }
            }
        }
        Check(faded, "Bouncing flames must reach the native fading behavior after settling.");
        for (size_t slot = 3; slot <= 11; ++slot)
        {
            Check(c.worldObjects[slot].health == 0, "All breakup flames must eventually expire.");
        }

        Fixture oracle;
        auto& o = oracle.context;
        RawGameObject initial;
        initial.behavior = BehaviorType::BouncingObject;
        initial.field_24 = 1;
        initial.modelId = 1;
        initial.modelAnim = 5;
        initial.animFrame = 1;
        initial.spriteScale = 24;
        initial.posX = initial.posZ = 1000;
        initial.posY = 100;
        initial.velocityX = 3;
        initial.velocityZ = -2;
        initial.winY = -1234;
        initial.groupIndexOrLock = 5;
        initial.itemValue = 75;
        for (std::uint32_t word = 0; word <= 65535; ++word)
        {
            auto particle = initial;
            particle.health = WrapSignedWord(word);
            auto expected = particle;
            std::uint32_t seed = word * 977u + 17u;
            o.randomSeed = seed;
            o.lightingCache.contributions = {};
            const auto random = [&seed](std::uint16_t range)
            {
                seed = seed * 134775813u + 1u;
                return range == 0 ? 0u : (seed >> 16) % range;
            };
            const auto scale = static_cast<std::uint8_t>(random(static_cast<std::uint16_t>(word >> 3)) + 6);
            expected.spriteScale = std::min<std::uint8_t>(scale, 24);
            expected.winY = WrapSignedWord(expected.winY ^ 1);
            std::int16_t light = 0;
            if (random(static_cast<std::uint16_t>(100 - word)) < 6)
            {
                light = static_cast<std::int16_t>(random(20));
            }
            expected.velocityY = -1;
            expected.posX = 1003;
            expected.posY = 99;
            expected.posZ = 998;
            Check(Function_BouncingObject(particle, o).status == DispatchStatus::ExecutedCase &&
                std::memcmp(&particle, &expected, sizeof(particle)) == 0 && o.randomSeed == seed,
                "Flame movement, byte truncation and RNG order must match all native health words.");
            auto expectedLighting = decltype(o.lightingCache.contributions){};
            expectedLighting[LightingCache::MaxRadius][LightingCache::MaxRadius] = light;
            Check(o.lightingCache.contributions == expectedLighting, "Flame light contribution differs.");

            particle = initial;
            particle.health = WrapSignedWord(word);
            expected = particle;
            expected.blendWeightOrSpriteOpacity = static_cast<std::int16_t>(
                expected.health >= 0 ? expected.health / 4 : -((-static_cast<int>(expected.health) + 3) / 4));
            Function_FadingParticle(particle, o);
            Check(std::memcmp(&particle, &expected, sizeof(particle)) == 0,
                "FadingParticle must only assign the arithmetic-shifted health to opacity.");
        }

        auto frozen = initial;
        frozen.health = 60;
        const auto before = frozen;
        const auto seed = o.randomSeed;
        o.tickCount = 0;
        Function_BouncingObject(frozen, o);
        Check(std::memcmp(&before, &frozen, sizeof(frozen)) == 0 && o.randomSeed == seed,
            "Zero-tick bouncing flames must not move, flicker or consume randomness.");
        frozen.behavior = BehaviorType::FadingParticle;
        Function_FadingParticle(frozen, o);
        Check(frozen.blendWeightOrSpriteOpacity == 15, "Fading opacity updates even with zero behavior ticks.");

        Fixture batch;
        Fixture steps;
        auto combined = initial;
        combined.health = 60;
        auto separate = combined;
        batch.context.randomSeed = steps.context.randomSeed = 12345;
        batch.context.tickCount = 32;
        Function_BouncingObject(combined, batch.context);
        for (int tick = 0; tick < 32; ++tick) { Function_BouncingObject(separate, steps.context); }
        Check(std::memcmp(&combined, &separate, sizeof(combined)) == 0 &&
            batch.context.randomSeed == steps.context.randomSeed &&
            batch.context.lightingCache.contributions == steps.context.lightingCache.contributions,
            "Batched flame substeps must preserve movement, settling, lighting and RNG order.");
        auto gated = initial;
        gated.health = 60;
        steps.context.objectCollisionsEnabled = false;
        Function_BouncingObject(gated, steps.context);
        Check(gated.posX == initial.posX && gated.posY == initial.posY && gated.posZ == initial.posZ &&
            gated.velocityY == -1 && gated.winY == WrapSignedWord(initial.winY ^ 1),
            "The native motion gate must not suppress gravity or flame flicker updates.");
    }

    void TestStoneAndPlacement()
    {
        Fixture f;
        auto& c = f.context;
        f.Equip(WeaponType::Stone);
        c.inventory[1].modelId = 1; c.inventory[1].modelAnim = 4;
        c.inventory[1].animFrame = 1; c.inventory[1].spriteScale = 64;
        c.inventory[1].behaviorParameter = 14;
        c.inventory[6] = c.inventory[1];
        c.inventory[6].behaviorParameter = 17;
        c.AttackWithSelectedWeapon();
        auto& stone = c.worldObjects[2];
        Check(stone.behavior == BehaviorType::BouncingObject && stone.field_24 == 6 &&
            stone.itemValue == 60 && stone.groupIndexOrLock == 300 && stone.velocityX == 16 &&
            stone.velocityY == 6 && stone.posY == 130 && stone.posZ == 984 &&
            stone.behaviorParameter == 14 && c.inventory[1].behaviorParameter == 17 &&
            c.inventory[6].health == 0, "Stone throws copy the record and refill the vacated hand from the first spare.");
        stone.posY = -1; stone.velocityY = -6;
        DispatchObjectCase(stone, c);
        Check(stone.posY == 10 && stone.velocityY == 2 && stone.velocityX == 7 && stone.field_26 == 0,
            "Stone floor bounce must use encoded sprite height, fixed-point restitution and signed half-speed damping.");
        stone.posY = -1; stone.velocityY = -1; stone.velocityX = stone.velocityZ = 0;
        DispatchObjectCase(stone, c);
        Check(stone.behavior == BehaviorType::Behavior_300, "A stone's final bounce selects the resting initializer.");
        DispatchObjectCase(stone, c);
        Check(stone.behavior == BehaviorType::None && stone.itemValue == 5 &&
            stone.field_32 == 3 && stone.collisionRadiusXZ == 22, "Resting stone restores its pickup identity.");

        Fixture trap;
        trap.Equip(WeaponType::TrapOrPole);
        auto& tc = trap.context;
        tc.inventory[1].behavior = BehaviorType::BearTrapAmmo;
        tc.inventory[1].behaviorParameter = 30;
        tc.inventory[1].field_07 = 126;
        tc.inventory[1].collisionRadiusXZ = tc.inventory[1].collisionRadiusY = 20;
        tc.AttackWithSelectedWeapon();
        auto& placed = tc.worldObjects[2];
        Check(placed.behavior == BehaviorType::BearTrapAmmo && tc.inventory[1].health == 0 &&
            placed.velocityY == -10 && placed.field_24 == 0, "Placement copies the held trap without replacing its behavior.");
        auto& victim = trap.Enemy(20, 1000, 110);
        victim.velocityY = 8;
        DispatchObjectCase(placed, tc);
        Check(victim.groupIndexOrLock == 170 && victim.velocityY == 0 && placed.field_24 == 1,
            "A trap damages its first victim, stops its vertical motion and closes only once.");
        tc.tickCount = 20;
        DispatchObjectCase(placed, tc);
        Check(placed.blendWeightOrSpriteOpacity == 100 && placed.field_24 == 65535 &&
            placed.behavior == BehaviorType::BearTrapAmmo && victim.groupIndexOrLock == 170,
            "Closed traps keep their behavior, clamp the blend and disarm via signed field_24=-1.");

        Fixture water;
        water.Equip(WeaponType::TrapOrPole);
        water.context.inventory[1].behavior = BehaviorType::BearTrapAmmo;
        water.context.SetCollisionMapPixelAtWorldXZ(1000, 1000, 1);
        water.context.AttackWithSelectedWeapon();
        Check(water.context.inventory[1].health == 1000 && water.context.worldObjects[2].health == 0 &&
            water.context.weaponAnimationSpeed == 0 && water.context.weaponAmounts[6] == 99,
            "Native water refusal keeps the item but preserves the common allocation/resource side effects.");

        Fixture pole;
        pole.Equip(WeaponType::TrapOrPole);
        auto& pc = pole.context;
        pc.inventory[1].behavior = BehaviorType::MetalPole;
        pc.inventory[1].field_07 = 126;
        pc.inventory[1].collisionRadiusXZ = pc.inventory[1].collisionRadiusY = 20;
        auto& socket = pole.Enemy(20, 1000, 0);
        socket.collisionType = CollisionType::SwitchForMetalPole;
        pc.AttackWithSelectedWeapon();
        DispatchObjectCase(pc.worldObjects[2], pc);
        Check(socket.collisionType == CollisionType::Switch && socket.animFrame == 2 &&
            pc.worldObjects[2].health == 0, "Placing a pole into its matching socket consumes it and enables the switch.");
    }

    void TestElectricityAndShovel()
    {
        Fixture f;
        auto& c = f.context;
        f.Equip(WeaponType::Electricity, 100);
        c.frameTickCount = 3;
        auto& first = f.Enemy(20, 1100, 500);
        auto& tie = f.Enemy(21, 900);
        c.AttackWithSelectedWeapon();
        const auto& beam = c.worldObjects[2];
        Check(c.weaponAmounts[7] == 93 && first.groupIndexOrLock == 196 && tie.groupIndexOrLock == 200 &&
            beam.displayType == DisplayType::VectorUnlit && beam.modelAnim == 6 && beam.health == 5 &&
            beam.winX == GetVectorAngle(WrapSignedWord(first.posY - beam.posY), -50) &&
            beam.behavior == BehaviorType::None && beam.modelFrame2 != beam.animFrame &&
            c.worldObjects[3].behavior == BehaviorType::LightSource && c.worldObjects[3].health == 4 &&
            c.worldObjects[3].posY == first.posY,
            "Electricity uses distance-shell targeting, first-slot ties, no vertical gate, tick-scaled cost and beam/impact lifetimes.");
        c.UpdateWeaponAnimation(1);
        Check(c.weaponAnimationSpeed == 0 && c.weaponReady && c.weaponAnimationPosition == 15,
            "Electricity is continuous once its attack sentinel has been consumed.");

        Fixture miss;
        miss.Equip(WeaponType::Electricity, 100);
        miss.context.AttackWithSelectedWeapon();
        Check(miss.context.weaponAmounts[7] == 100 && miss.context.weaponAnimationSpeed == -4 &&
            miss.context.worldObjects[2].health == 0, "No electric target restores the pre-attack charge.");
        miss.Equip(WeaponType::Electricity, 1);
        miss.Enemy(20, 1100);
        miss.context.AttackWithSelectedWeapon();
        Check(miss.context.weaponAmounts[7] == 0 && miss.context.worldObjects[2].health == 0,
            "The last single electric charge is consumed without creating a beam.");

        Fixture shovel;
        shovel.Equip(WeaponType::Showel);
        auto& sc = shovel.context;
        sc.AttackWithSelectedWeapon();
        sc.UpdateWeaponAnimation(5);
        Check(sc.worldObjects[2].health == 0, "Shovel has no immediate damage contact.");
        sc.UpdateWeaponAnimation(1);
        const auto& contact = sc.worldObjects[2];
        Check(contact.behavior == BehaviorType::WeaponDamageCollider && contact.behaviorParameter == 5 &&
            contact.collisionRadiusXZ == 15 && contact.collisionRadiusY == 6 && contact.health == 10 &&
            contact.posX == 1055 && contact.posY == 90 && sc.weaponAnimationPosition == 17,
            "Shovel creates its low damage/digging contact at pose 17.");
        sc.weaponAnimationRecoil = 3;
        sc.weaponAnimationSpeed = -2;
        sc.UpdateWeaponAnimation(1);
        Check(sc.weaponSpriteFrame == 2 && sc.weaponAnimationRecoil == 0,
            "Shovel sprite selection precedes its recoil reset.");
    }

    void TestAnimationPhases()
    {
        Fixture f;
        auto& c = f.context;
        f.Equip(WeaponType::Crossbow, 0);
        c.weaponAnimationSpeed = -3; c.weaponReady = true;
        c.UpdateWeaponAnimation(7);
        Check(c.weaponAnimationSpeed == 0 && !c.weaponReady && c.weaponSpriteFrame == 1,
            "An empty crossbow does not start reloading.");
        c.weaponAmounts[3] = 4;
        c.UpdateWeaponAnimation(1);
        Check(c.weaponAnimationSpeed == 49 && c.weaponAnimationPosition == 30, "New ammunition starts a reload from idle.");
        c.weaponAnimationSpeed = 40;
        c.UpdateWeaponAnimation(0);
        Check(c.weaponAnimationPosition == 0, "Crossbow reload passes through pose zero at phase 40.");
        c.weaponAnimationSpeed = 15; c.requestedWeapon = WeaponType::Dagger;
        c.UpdateWeaponAnimation(1);
        Check(c.selectedWeapon == WeaponType::Dagger && c.weaponAnimationSpeed == -2,
            "Crossbow commits requested selection inside its middle reload phase.");

        f.Equip(WeaponType::Fireball);
        c.weaponAnimationSpeed = 85;
        c.UpdateWeaponAnimation(0);
        Check(c.weaponSpriteFrame == 3, "Fireball selects the third casting sprite only in phase 81..90.");
        c.weaponAnimationSpeed = 50;
        c.UpdateWeaponAnimation(0);
        Check(c.weaponAnimationSpeed == 0 && c.weaponSpriteFrame == 1, "Fireball recovery phase 30..60 resets to idle.");

        f.Equip(WeaponType::Stone);
        c.weaponAnimationSpeed = -3; c.weaponReady = false;
        c.UpdateWeaponAnimation(3);
        Check(c.weaponAnimationSpeed == 17 && c.weaponAnimationPosition == 7 &&
            c.weaponAmounts[5] == 100 && !c.weaponReady, "Stone animation must not silently replace the native ready latch.");

        f.Equip(WeaponType::TrapOrPole);
        c.weaponAnimationSpeed = -3;
        c.UpdateWeaponAnimation(1);
        Check(c.weaponAnimationSpeed == 110 && c.weaponAnimationPosition == 0,
            "Placement's initial sentinel does not also decay the countdown.");
        c.weaponAnimationSpeed = 100;
        c.UpdateWeaponAnimation(1);
        Check(c.weaponAnimationSpeed == 0 && c.weaponReady, "Placement skips the middle countdown interval.");

        f.Equip(WeaponType::Electricity);
        c.weaponAnimationSpeed = -2;
        c.UpdateWeaponAnimation(1);
        Check(c.weaponAnimationSpeed == 199 && c.weaponAnimationPosition == 0, "Electric draw starts at phase 200.");
        c.weaponAnimationSpeed = 101;
        c.UpdateWeaponAnimation(1);
        Check(c.weaponAnimationSpeed == 100, "Electric phase 101 is not part of the 102..200 fast reset.");
        c.weaponAnimationSpeed = 102;
        c.UpdateWeaponAnimation(1);
        Check(c.weaponAnimationSpeed == 0 && c.weaponAnimationPosition == 15, "Electric phase 102 skips directly to idle.");
    }

    void TestSoundsAndProjectileSurfaces()
    {
        class Audio final : public AudioHandler
        {
        public:
            std::vector<std::uint8_t> samples;
            std::uint16_t GetSampleCount() const override { return 129; }
            bool IsChannelPlaying(std::size_t) const override { return false; }
            void PlaySample(std::size_t, std::uint8_t sample, std::uint8_t) override { samples.push_back(sample); }
            void StopAllSamples() override {}
        } audio;
        Fixture f;
        auto& c = f.context;
        c.audioHandler = &audio;
        c.soundEffectsLoaded = true;
        for (std::size_t effect = 0; effect < c.soundEffects.size(); ++effect)
        {
            c.soundEffects[effect].sampleCount = 1;
            c.soundEffects[effect].samples[0] = static_cast<std::uint8_t>(effect + 1);
        }
        f.Equip(WeaponType::Crossbow);
        c.AttackWithSelectedWeapon();
        Check(audio.samples.size() == 1 && audio.samples.back() == 7, "Crossbow input must emit effect 7.");
        c.worldObjects[2].health = 0;
        f.Equip(WeaponType::Fireball);
        c.AttackWithSelectedWeapon();
        auto& ball = c.worldObjects[2];
        c.tickCount = 11;
        DispatchObjectCase(ball, c);
        Check(audio.samples.size() == 1, "Fireball windup must stay silent through group 11.");
        c.tickCount = 1;
        DispatchObjectCase(ball, c);
        Check(audio.samples.size() == 2 && audio.samples.back() == 34 && ball.posX == 1000,
            "Fireball launch effect 34 occurs exactly when windup crosses 11, before flight.");
        DispatchObjectCase(ball, c);
        Check(audio.samples.size() == 2, "Fireball flight must not repeat its launch sound.");
        c.worldObjects[2].health = 0;
        f.Equip(WeaponType::Showel);
        c.AttackWithSelectedWeapon(); c.UpdateWeaponAnimation(6);
        Check(audio.samples.back() == 19, "Shovel contact emits the native swing sound.");

        Fixture ceiling;
        ceiling.Equip(WeaponType::Crossbow);
        ceiling.context.AttackWithSelectedWeapon();
        auto& bolt = ceiling.context.worldObjects[2];
        bolt.posY = 20100;
        DispatchObjectCase(bolt, ceiling.context);
        Check(bolt.posY == 20000 && bolt.velocityX == 0 && bolt.winX == -10 && bolt.field_24 != 0,
            "Bolt ceiling impact stops translation and installs a randomized angular fall.");

        Fixture water;
        water.Equip(WeaponType::Crossbow);
        water.context.AttackWithSelectedWeapon();
        auto& floating = water.context.worldObjects[2];
        floating.posY = -1; floating.velocityX = floating.velocityZ = 0;
        water.context.SetCollisionMapPixelAtWorldXZ(1000, 1000, 1);
        DispatchObjectCase(floating, water.context);
        Check(floating.behavior == BehaviorType::FloatingDebris && floating.winX == 256,
            "A resting water bolt switches to the recovered floating-debris controller.");

        Fixture tools;
        auto& t = tools.context;
        t.audioHandler = &audio; t.soundEffectsLoaded = true; t.soundEffects = c.soundEffects;
        tools.Equip(WeaponType::Scythe);
        const auto sounds = audio.samples.size();
        t.AttackWithSelectedWeapon(); t.UpdateWeaponAnimation(6);
        Check(audio.samples.size() == sounds + 1 && audio.samples.back() == 19,
            "Scythe contact must emit the shared swing effect 19 exactly once.");
        t.worldObjects[2].health = 0;
        tools.Equip(WeaponType::Lentils, 12);
        t.inventory[1].modelId = 33;
        t.AttackWithSelectedWeapon();
        Check(audio.samples.size() == sounds + 1, "The Lentils factory itself is silent.");
        auto& marker = t.worldObjects[2];
        marker.posY = 1;
        auto& bridge = t.worldObjects[20];
        bridge.health = 1000; bridge.field_07 = 128;
        bridge.posX = marker.posX; bridge.posZ = marker.posZ;
        bridge.collisionRadiusXZ = 30; bridge.collisionRadiusY = 4;
        DispatchObjectCase(marker, t);
        Check(audio.samples.size() == sounds + 2 && audio.samples.back() == 101,
            "A Lentils marker hitting an invisible bridge must emit effect 101.");
    }

    void TestBoundariesAndAssets(const std::filesystem::path& directory)
    {
        Fixture f;
        for (const auto weapon : { WeaponType::Crossbow, WeaponType::Fireball, WeaponType::Stone,
            WeaponType::TrapOrPole, WeaponType::Electricity, WeaponType::Showel,
            WeaponType::Scythe, WeaponType::Lentils })
        {
            f.Equip(weapon);
            f.context.inventory[1].behavior = BehaviorType::BearTrapAmmo;
            f.context.playerUnderwater = true;
            Check(f.context.AttackWithSelectedWeapon() == WeaponActionStatus::NoEffect &&
                f.context.weaponAmounts[static_cast<std::size_t>(weapon)] == 100,
                "Underwater restrictions must precede ammunition use and allocation for every added weapon.");
            f.context.playerUnderwater = false;
        }
        for (const int id : { 9, 23 })
        {
            f.Equip(static_cast<WeaponType>(id));
            const auto objects = f.context.worldObjects;
            Check(f.context.AttackWithSelectedWeapon() == WeaponActionStatus::UnimplementedWeapon &&
                std::memcmp(objects.data(), f.context.worldObjects.data(), sizeof(objects)) == 0,
                "Unrecovered tools must remain explicitly unsupported without mutation.");
        }
        f.Equip(WeaponType::TrapOrPole);
        f.context.inventory[1].behavior = BehaviorType::BonesToGrab;
        Check(f.context.AttackWithSelectedWeapon() == WeaponActionStatus::UnimplementedWeapon &&
            f.context.inventory[1].health == 1000, "Unrecovered placeable behaviors must not consume the held item.");

        const auto read = [&](const char* name)
        {
            std::ifstream stream(directory / name, std::ios::binary);
            if (!stream) { throw std::runtime_error("Cannot read original projectile animation fixture."); }
            return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(stream), {});
        };
        const auto fireball = read("SCHUSS1.AN2");
        Check(fireball.at(0) * fireball.at(1) == 10, "Native EB28 cycle must equal all ten original SCHUSS1 frames.");
        const auto sprites = read("ANIM1.AN2");
        std::size_t offset = 2;
        for (int frame = 0; frame < 6; ++frame)
        {
            const auto width = sprites.at(offset + 4) + 256u * sprites.at(offset + 5) + 1;
            const auto height = sprites.at(offset + 6) + 256u * sprites.at(offset + 7) + 1;
            offset += 8 + width * height;
        }
        const auto relY = WrapSignedWord(sprites.at(offset + 2) + 256 * sprites.at(offset + 3));
        Check(relY + sprites.at(offset + 6) + 256 * sprites.at(offset + 7) == 8,
            "The original stone's native bottom is eight (encoded height), not nine (pixel count).");
    }

    RawGameObject ReadOriginalObject(const std::filesystem::path& animations, const char* file, int slot)
    {
        std::ifstream stream(animations.parent_path() / "LEVEL" / file, std::ios::binary);
        std::array<std::uint8_t, sizeof(RawGameObject)> bytes{};
        stream.seekg((slot - 2) * static_cast<std::streamoff>(bytes.size()));
        stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
        Check(stream.good(), "Cannot read original tool/obstacle fixture.");
        return DecodeGameObject(bytes.data(), bytes.size());
    }

    void TestScytheHedge(const std::filesystem::path& directory)
    {
        Fixture f;
        auto& c = f.context;
        f.Equip(WeaponType::Scythe);
        c.inventory[1] = ReadOriginalObject(directory, "OBJPOS30.DAT", 94);
        auto& hedge = c.worldObjects[20];
        hedge = ReadOriginalObject(directory, "OBJPOS30.DAT", 100);
        Check(c.inventory[1].itemValue == 24 && c.inventory[1].doorLockOrGroupBitmask == 18 &&
            hedge.behavior == BehaviorType::DestroyableBarrier && hedge.behaviorParameter == 100 &&
            hedge.doorLockOrGroupBitmask == 1, "Use the actual scythe and hedge records, not guessed target parameters.");
        c.worldObjects[1].posX = WrapSignedWord(hedge.posX - 55);
        c.worldObjects[1].posY = WrapSignedWord(hedge.posY + 35);
        c.worldObjects[1].posZ = hedge.posZ;
        c.playerLookHeight = 90;
        c.tickCount = 1;
        DispatchObjectCase(hedge, c);
        Check(c.GetCollisionMapPixelAtWorldXZ(hedge.posX, hedge.posZ) == 16,
            "An intact hedge must maintain its blocking map segment.");
        Check(c.AttackWithSelectedWeapon() == WeaponActionStatus::Applied, "Scythe input must enter the swing.");
        c.UpdateWeaponAnimation(5);
        Check(c.worldObjects[2].health == 0, "The scythe must not hit before pose 17.");
        c.UpdateWeaponAnimation(1);
        auto& contact = c.worldObjects[2];
        Check(contact.behavior == BehaviorType::WeaponDamageCollider && contact.behaviorParameter == 5 &&
            contact.collisionRadiusXZ == 15 && contact.collisionRadiusY == 6 &&
            contact.posX == hedge.posX && contact.posY == hedge.posY && contact.posZ == hedge.posZ &&
            c.weaponAmounts[24] == 99 && c.weaponAmounts[8] == 100,
            "Scythe must use the exact shared low shovel contact, including the native counter alias.");
        DispatchObjectCase(contact, c);
        Check(hedge.field_24 == 32000 && hedge.field_07 == 0 &&
            contact.behavior == BehaviorType::DriftingParticle && contact.animFrame == 3,
            "A scythe hit must cut the original hedge and create its native impact fragment.");
        for (int tick = 0; tick < 22; ++tick) { DispatchObjectCase(hedge, c); }
        Check(hedge.behavior == BehaviorType::DestroyableBarrier && hedge.blendWeightOrSpriteOpacity == 98 &&
            c.GetCollisionMapPixelAtWorldXZ(hedge.posX, hedge.posZ) == 16,
            "Map blocking must persist through the hedge's destruction animation.");
        DispatchObjectCase(hedge, c);
        Check(hedge.behavior == BehaviorType::None && hedge.blendWeightOrSpriteOpacity == 100 &&
            c.GetCollisionMapPixelAtWorldXZ(hedge.posX, hedge.posZ) == 2,
            "Completed hedge destruction must remove the blocking map pixels.");

        Fixture selection;
        auto& s = selection.context;
        s.inventory[2] = c.inventory[1];
        s.RefreshWeaponInventory();
        Check(s.weaponAmounts[24] == 0 && s.SelectWeapon(WeaponType::Scythe),
            "The host hotkey must select an owned scythe despite its natively zero initial counter.");
        s.UpdateWeaponAnimation(1);
        s.UpdateWeaponAnimation(1);
        Check(s.selectedWeapon == WeaponType::Scythe && s.weaponReady && s.weaponAmounts[24] == 0,
            "The scythe's shared animation, not an invented charge refill, enables the reusable tool.");
    }

    void TestScytheAnimationParity()
    {
        Fixture scythe;
        Fixture shovel;
        scythe.Equip(WeaponType::Scythe);
        shovel.Equip(WeaponType::Showel);
        const auto objects = scythe.context.worldObjects;
        for (const int ticks : { 0, 1, 7, 40 })
        {
            for (int bits = 0; bits <= 65535; ++bits)
            {
                for (auto* context : { &scythe.context, &shovel.context })
                {
                    context->worldObjects = objects;
                    context->weaponAnimationSpeed = WrapSignedWord(bits);
                    context->weaponAnimationPosition = 3;
                    context->weaponAnimationRecoil = 2;
                    context->weaponReady = true;
                    context->weaponAmounts[8] = 17;
                    context->weaponAmounts[24] = 19;
                    Check(context->UpdateWeaponAnimation(static_cast<std::int16_t>(ticks)) == WeaponActionStatus::Applied,
                        "Both shared swing families must support every signed phase word.");
                }
                const auto& a = scythe.context;
                const auto& b = shovel.context;
                Check(a.weaponAnimationSpeed == b.weaponAnimationSpeed &&
                    a.weaponAnimationPosition == b.weaponAnimationPosition &&
                    a.weaponAnimationRecoil == b.weaponAnimationRecoil &&
                    a.weaponSpriteFrame == b.weaponSpriteFrame && a.weaponReady == b.weaponReady &&
                    a.weaponAmounts[8] == b.weaponAmounts[8] && a.weaponAmounts[24] == 19 &&
                    std::memcmp(a.worldObjects.data(), b.worldObjects.data(), sizeof(objects)) == 0,
                    "Scythe and shovel must preserve identical native pose, recoil, counter and contact behavior.");
            }
        }
    }

    void TestBarrierMapAndArithmetic()
    {
        Fixture f;
        auto& c = f.context;
        const int column = (1000 - 112) / 25;
        const int row = 162 - (1000 - 38) / 25;
        for (int heading = 0; heading < 512; ++heading)
        {
            std::memset(c.collisionMap->data, 0, sizeof(c.collisionMap->data));
            c.SetBarrierCollisionMapAtWorldXZ(1000, 1000, static_cast<std::int16_t>(heading), 16);
            const bool horizontal = (((heading - 64 + 512) % 512) / 128) % 2 != 0;
            int written = 0;
            for (int dz = -1; dz <= 1; ++dz)
            {
                for (int dx = -1; dx <= 1; ++dx)
                {
                    const auto value = c.collisionMap->data[(row + dz) * 200 + column + dx];
                    Check(value == ((horizontal ? dz == 0 : dx == 0) ? 16 : 0),
                        "Barrier heading quadrants must write a three-pixel line, not a door's 3x3 square.");
                    if (value != 0) { ++written; }
                }
            }
            Check(written == 3, "Exactly three pixels belong to the barrier segment.");
        }
        for (const int ticks : { 0, 1, 3, 32767 })
        {
            for (const int goal : { 0, 255, 32000, 32768, 65535 })
            {
                RawGameObject object;
                object.behavior = BehaviorType::DestroyableBarrier;
                object.health = 1000;
                object.posX = object.posZ = 1000;
                object.itemValue = 1234; object.field_26 = 65530;
                object.field_24 = static_cast<std::uint16_t>(goal);
                c.tickCount = static_cast<std::int16_t>(ticks);
                auto acceleration = static_cast<std::uint16_t>(65530 + ticks * 100);
                auto position = std::int16_t{ 1234 };
                const auto target = WrapSignedWord(goal);
                if (position >= target) { position = target; acceleration = 0; }
                position = WrapSignedWord(position + acceleration * ticks);
                const auto blend = std::min<int>(static_cast<std::uint16_t>(position) / 256, 100);
                DispatchObjectCase(object, c);
                Check(object.field_26 == acceleration && object.itemValue == position &&
                    object.blendWeightOrSpriteOpacity == blend &&
                    object.behavior == (blend == 100 ? BehaviorType::None : BehaviorType::DestroyableBarrier),
                    "Barrier acceleration, signed comparison and unsigned high-byte blending must wrap natively.");
            }
        }
    }

    void TestLentils(const std::filesystem::path& directory)
    {
        Fixture f;
        auto& c = f.context;
        f.Equip(WeaponType::Lentils, 12);
        c.inventory[1] = ReadOriginalObject(directory, "OBJPOS34.DAT", 44);
        const auto held = c.inventory[1];
        Check(held.itemValue == 20 && held.doorLockOrGroupBitmask == 15 && held.modelId == 33,
            "The Lentils factory fixture must be the actual labeled held item.");
        c.randomSeed = 123;
        auto seed = c.randomSeed;
        const auto random = [&seed](int range)
        {
            seed = seed * 134775813u + 1u;
            return static_cast<int>((seed >> 16) % range);
        };
        const auto signedRandom = [&random](int radius)
        {
            const auto value = random(2 * radius + 1) - radius;
            return value == 0 ? radius : value;
        };
        Check(c.AttackWithSelectedWeapon() == WeaponActionStatus::Applied, "Lentils must be throwable.");
        for (int slot = 2; slot <= 8; ++slot)
        {
            const auto angle = static_cast<std::int16_t>(signedRandom(20));
            const auto speed = random(5) + 4;
            auto expected = held;
            expected.blendWeightOrSpriteOpacity = 10; expected.field_26 = 100;
            expected.collisionRadiusXZ = 11; expected.collisionRadiusY = 6;
            expected.behavior = BehaviorType::BouncingObject; expected.itemValue = 50;
            expected.field_24 = 7; expected.groupIndexOrLock = 0;
            expected.renderPlacementMode = 6; expected.field_32 = 0;
            expected.animFrame = 2; expected.health = 200;
            expected.velocityX = WrapSignedWord(static_cast<std::int32_t>(
                static_cast<std::uint32_t>(Sine(angle) * speed) >> 16));
            expected.velocityZ = WrapSignedWord(static_cast<std::int32_t>(
                static_cast<std::uint32_t>(Cosine(angle) * speed) >> 16));
            expected.velocityY = static_cast<std::int16_t>(random(3) + 3);
            expected.posX = WrapSignedWord(1000 + signedRandom(5));
            expected.posY = 100;
            expected.posZ = WrapSignedWord(1000 + signedRandom(5));
            if (slot == 8) { expected.soundEffectId = 101; }
            Check(std::memcmp(&c.worldObjects[slot], &expected, sizeof(expected)) == 0,
                "All seven Lentils records must match native copy/overwrite fields and RNG draw order.");
        }
        Check(c.randomSeed == seed && c.weaponAmounts[20] == 5 && c.weaponAnimationSpeed == -3 &&
            std::memcmp(&held, &c.inventory[1], sizeof(held)) == 0 && c.worldObjects[9].health == 0,
            "Throwing Lentils must create exactly seven markers without consuming the held supply object.");
        c.UpdateWeaponAnimation(1);
        Check(c.weaponAnimationSpeed == 118 && c.weaponAmounts[20] == 12 && c.weaponReady,
            "Lentils must share the casting countdown and restore the next burst's counter.");

        auto& marker = c.worldObjects[2];
        auto& bridge = c.worldObjects[20];
        bridge = ReadOriginalObject(directory, "OBJPOS34.DAT", 2);
        Check(bridge.collisionType == CollisionType::WalkablePlanks && bridge.field_07 == 131 &&
            bridge.displayType == DisplayType::Hidden, "Use a real initially invisible bridge piece.");
        const auto prototype = marker;
        c.tickCount = 5;
        for (const int hits : { 0, 9, 10, 38, 39, 65535 })
        {
            marker = prototype;
            marker.posX = bridge.posX; marker.posZ = bridge.posZ; marker.posY = 1;
            bridge.field_24 = static_cast<std::uint16_t>(hits);
            bridge.displayType = DisplayType::Hidden;
            DispatchObjectCase(marker, c);
            const auto count = std::clamp<int>(WrapSignedWord(hits + 1), 0, 39);
            Check(marker.health == 0 && marker.field_07 == 0 && marker.posY == bridge.posY &&
                marker.posX == bridge.posX && marker.posZ == bridge.posZ &&
                bridge.field_24 == count && bridge.animFrame == count / 10 + 1 &&
                bridge.displayType == DisplayType::VectorNormal && bridge.health == 1000,
                "A Lentils hit must reveal one piece, cap coverage at 39 and stop immediately without damaging it.");
        }
        marker = prototype;
        marker.posX = bridge.posX; marker.posZ = bridge.posZ; marker.posY = -100;
        f.level.heightSurfaces[0] = Level::HeightSurface{ 500, 0 };
        std::memset(f.level.surfaceFlags, 1, sizeof(f.level.surfaceFlags));
        c.tickCount = 1;
        bridge.displayType = DisplayType::Hidden;
        DispatchObjectCase(marker, c);
        Check(marker.health == 200 && marker.field_07 == 0 && bridge.displayType == DisplayType::Hidden,
            "A marker below the bridge must not reveal it through a blanket proximity effect.");

        Fixture depleted;
        depleted.Equip(WeaponType::Lentils, 0);
        depleted.context.AttackWithSelectedWeapon();
        Check(depleted.context.weaponAmounts[20] == -1 && depleted.context.worldObjects[2].health == 200 &&
            depleted.context.worldObjects[3].health == 0, "A stale ready latch still follows the native single-marker edge case.");
        Fixture initial;
        initial.Equip(WeaponType::Lentils, 1000);
        initial.context.AttackWithSelectedWeapon();
        Check(initial.context.weaponAmounts[20] == 5 && initial.context.weaponAnimationSpeed == -3,
            "The native initial counter must terminate without recursive host stack growth.");
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc != 2) { throw std::runtime_error("Expected original ANIMS fixture directory."); }
        TestCrossbow();
        TestFireball();
        TestFireballBreakup(argv[1]);
        TestStoneAndPlacement();
        TestElectricityAndShovel();
        TestAnimationPhases();
        TestSoundsAndProjectileSurfaces();
        TestBoundariesAndAssets(argv[1]);
        TestScytheHedge(argv[1]);
        TestScytheAnimationParity();
        TestBarrierMapAndArithmetic();
        TestLentils(argv[1]);
        std::cout << "Ranged weapon, placement, scythe/hedge and Lentils/bridge tests passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
