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

    std::vector<std::uint8_t> Read(const std::filesystem::path& filename)
    {
        std::ifstream file(filename, std::ios::binary);
        if (!file) { throw std::runtime_error("Missing original dragon fixture: " + filename.string()); }
        return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
    }

    int Next(std::uint32_t& seed, int range)
    {
        seed = seed * 134775813u + 1;
        return static_cast<int>((seed >> 16) % range);
    }

    int Signed(std::uint32_t& seed, int radius)
    {
        const int result = Next(seed, radius * 2 + 1) - radius;
        return result == 0 ? radius : result;
    }

    class Context final : public GameContext
    {
    public:
        using GameContext::LoadLevelGeometry;
        using GameContext::LoadLevelObjects;
        using GameContext::InitializeLevelPlayerSpawn;
        std::filesystem::path root;
        std::optional<LevelLoadRequest> request;
        std::optional<std::array<std::int16_t, 3>> announcedHead;
        int created = 0, destroyed = 0, lifeChanges = 0;
        void OnObjectCreated(int slot) override
        {
            ++created;
            if (slot == 5)
            {
                announcedHead = std::array<std::int16_t, 3>{ worldObjects[5].posX, worldObjects[5].posY, worldObjects[5].posZ };
            }
        }
        void OnObjectDestroyed(int) override { ++destroyed; }
        void OnPlayerHealthChanged(std::int32_t, std::int32_t) override { ++lifeChanges; }
        std::vector<std::uint8_t> LoadFile(std::string_view filename) override { return Read(root / filename); }
        void LoadLevel(const LevelLoadRequest& load) override { request = load; }
    };

    struct Fixture
    {
        Level level = CreateDummyLevel();
        Context context;
        explicit Fixture(const std::filesystem::path& root)
        {
            const auto records = Read(root / "LEVEL\\OBJPOS15.DAT");
            const auto model = Read(root / "VEKT\\VEKT19.3DA");
            level.modelAnchors[6] = std::make_shared<ModelAnchorGeometry>();
            level.modelAnchors[6]->Load(model.data(), model.size());
            context.root = root;
            context.level = &level;
            context.collisionMap = level.collisionMap.get();
            context.objectCollisionsEnabled = true;
            std::fill(std::begin(context.collisionMap->data), std::end(context.collisionMap->data), 0);
            for (auto& row : level.ceilingObjects) { std::fill(std::begin(row), std::end(row), std::uint8_t{ 0 }); }
            context.ReplaceWorldObject(12, DecodeGameObject(records.data() + 10 * 54, 54));
            context.ReplaceWorldObject(5, DecodeGameObject(records.data() + 3 * 54, 54));
            auto player = Dragon();
            player.behavior = BehaviorType::None;
            player.posX = 2100; player.posZ = 2475; player.posY = 50;
            player.collisionRadiusXZ = 15; player.collisionRadiusY = 23;
            player.collisionMask = 255;
            context.ReplaceWorldObject(1, player);
            Dragon().posX = 2000; Dragon().posZ = 2475; Dragon().posY = 50;
            Dragon().winX = Dragon().winY = Dragon().winZ = 0;
            Dragon().generalPurpose1 = 0; Dragon().blendWeightOrSpriteOpacity = 0;
            context.soundListenerX = player.posX; context.soundListenerZ = player.posZ;
            context.tickCount = 1;
            context.created = context.destroyed = 0;
        }
        RawGameObject& Dragon() { return context.worldObjects[12]; }
        RawGameObject& Head() { return context.worldObjects[5]; }
    };

    void TestOriginalLevel(const std::filesystem::path& root)
    {
        Context context;
        context.root = root;
        LevelLoadData data;
        context.LoadLevelGeometry(15, data);
        context.LoadLevelObjects(15, data);
        Check(data.level->modelFileIndices[6] == 19 && data.level->modelFileIndices[9] == 42,
            "Level 15 must resolve the dragon and key model assets.");
        Check(data.objects[12].behavior == BehaviorType::Dragon && data.objects[5].behavior == BehaviorType::DragonHead &&
            data.objects[5].doorLockOrGroupBitmask == 12, "Original head must link to native dragon slot 12.");
        const auto& geometry = *data.level->modelAnchors[6];
        Check(geometry.animations.size() == 6 && geometry.animations[0].size() == 4 &&
            geometry.animations[0][0].billboardVertex == 13 && geometry.animations[0][0].muzzleVertex == 37,
            "Original dragon needs six banks, four frames, head vertex 13 and muzzle vertex 37.");
        Check(geometry.animations[0][0].vertices[12] == std::array<std::int16_t, 3>{ 0, -336, -301 } &&
            !geometry.animations[2][0].billboardVertex, "Fragments must not inherit the live head attachment.");
        const auto bytes = Read(root / "VEKT\\VEKT19.3DA");
        ModelAnchorGeometry truncated;
        bool rejected = false;
        try { truncated.Load(bytes.data(), bytes.size() - 1); }
        catch (const std::invalid_argument&) { rejected = true; }
        Check(rejected, "Truncated model anchors must report errors.");
        context.InitializeLevelPlayerSpawn(data, { 0, 0, 100 });
        context.StartLevel(15);
        context.CompleteLevelLoad(context.request->id, std::move(data));
        Check(context.announcedHead == context.GetModelAnchor(context.worldObjects[12], false),
            "Level installation must initialize attachment coordinates before announcing the head to the host.");
    }

    void TestAttachment(const std::filesystem::path& root)
    {
        Fixture f(root);
        auto& body = f.Dragon();
        f.context.UpdateModelAttachments();
        Check(f.Head().posX == 2000 && f.Head().posY == 84 && f.Head().posZ == 2445,
            "Head must start on live vertex 13, not stale OBJPOS coordinates.");
        body.blendWeightOrSpriteOpacity = 50;
        f.context.UpdateModelAttachments();
        Check(f.Head().posY == 83 && f.Head().posZ == 2443, "Attachment must interpolate to modelFrame2.");
        body.blendWeightOrSpriteOpacity = 150;
        Check(f.context.GetModelAnchor(body, false) == std::optional<std::array<std::int16_t, 3>>{
            { 2000, 83, 2443 } }, "Native triangular morph weight must fold 150 back to 50 percent.");
        f.context.tickCount = 0;
        body.spriteScale = 2;
        for (int heading = 0; heading < 512; ++heading)
        {
            body.winY = static_cast<std::int16_t>(heading);
            const auto result = DispatchObjectCase(f.Head(), f.context);
            Check(result.status == DispatchStatus::ExecutedCase && f.Head().modelAnim ==
                ((-f.context.GetPlayerHeading() - heading + 43) & 511) / 86 + 1 && f.Head().animFrame == 2,
                "All direction-bank boundaries and mouth state must update even at zero ticks.");
        }
        body.behavior = BehaviorType::None;
        f.context.UpdateModelAttachments();
        Check(f.Head().health == 0 && f.context.destroyed == 1, "Key replacement must remove the linked head immediately.");
    }

    void TestAttacks(const std::filesystem::path& root)
    {
        for (int distance : { 199, 200 })
        {
            Fixture f(root);
            auto& body = f.Dragon();
            f.context.soundListenerX = static_cast<std::int16_t>(body.posX + distance);
            body.itemValue = 24;
            const auto mouth = f.context.GetModelAnchor(body, true);
            std::uint32_t seed = 123;
            f.context.randomSeed = seed;
            Next(seed, 40);
            const int count = distance < 200 ? 1 : 3;
            std::vector<int> banks;
            for (int i = 0; i < count; ++i) { banks.push_back(Next(seed, 6) + 1); }
            Check(DispatchObjectCase(body, f.context).status == DispatchStatus::ExecutedCase, "Dragon attack must execute.");
            std::vector<RawGameObject> shots;
            for (const auto& object : f.context.worldObjects)
            {
                if (object.behavior == BehaviorType::FlyingArrow) { shots.push_back(object); }
            }
            Check(static_cast<int>(shots.size()) == count && body.itemValue == 23 && body.spriteScale == 2 &&
                f.context.randomSeed == seed, "Distance 200 must switch from breath to a three-shot volley without extra RNG.");
            for (int i = 0; i < count; ++i)
            {
                const auto& shot = shots[i];
                std::int16_t vx = 0, vz = 0;
                GetClampedDirectionDisplacement(vx, vz, count == 1 ? 0 : static_cast<std::int16_t>(i * 10 - 10),
                    count == 1 ? 4 : 8);
                Check(shot.posX == (*mouth)[0] && shot.posY == (*mouth)[1] && shot.posZ == (*mouth)[2] &&
                    shot.velocityX == vx && shot.velocityZ == vz && shot.modelAnim == banks[i] &&
                    shot.itemValue == (count == 1 ? 4 : 5) && shot.behaviorParameter == (count == 1 ? 7 : 50),
                    "Shots must use the actual muzzle, native spread, bank RNG and damage.");
            }
        }
        Fixture breath(root);
        breath.Dragon().itemValue = 50;
        breath.context.tickCount = 27;
        DispatchObjectCase(breath.Dragon(), breath.context);
        Check(breath.context.created == 27 && breath.Dragon().itemValue == 23,
            "Close breath must emit once per tick throughout phases 50 through 24.");
        Fixture zero(root);
        zero.context.tickCount = 0;
        zero.Dragon().itemValue = 24;
        const auto before = zero.Dragon();
        const auto seed = zero.context.randomSeed;
        DispatchObjectCase(zero.Dragon(), zero.context);
        Check(std::memcmp(&before, &zero.Dragon(), 54) == 0 && zero.context.randomSeed == seed,
            "Zero-tick live dragon must not move, attack or consume RNG.");
        Fixture fallback(root);
        for (std::uint16_t slot = 2; slot <= 165; ++slot)
        {
            if (slot == 12) { continue; }
            auto active = fallback.Dragon();
            active.behavior = BehaviorType::None;
            fallback.context.ReplaceWorldObject(slot, active);
        }
        fallback.Dragon().itemValue = 24;
        fallback.context.soundListenerX = 2500;
        DispatchObjectCase(fallback.Dragon(), fallback.context);
        Check(fallback.context.worldObjects[165].behavior == BehaviorType::FlyingArrow &&
            fallback.Dragon().itemValue == 23, "Full pools must use slot 165 without corrupting the body.");
    }

    void TestWalking(const std::filesystem::path& root)
    {
        for (int frame = 1; frame <= 4; ++frame)
        {
            Fixture f(root);
            f.Dragon().animFrame = static_cast<std::uint8_t>(frame);
            f.Dragon().generalPurpose1 = 128;
            f.Dragon().modelFrame2 = static_cast<std::uint8_t>(frame % 4 + 1);
            f.Dragon().itemValue = -50;
            DispatchObjectCase(f.Dragon(), f.context);
            Check(f.Dragon().posX == (frame % 2 == 0 ? 2003 : 2000) &&
                f.Dragon().blendWeightOrSpriteOpacity == 6 && f.Dragon().velocityY == 39,
                "Only live walking frames two/four move, with six-unit phase speed and alert decay.");
        }
        Fixture boundary(root);
        auto& body = boundary.Dragon();
        for (auto& row : boundary.level.ceilingObjects) { std::fill(std::begin(row), std::end(row), std::uint8_t{ 1 }); }
        for (auto& row : boundary.level.roomHeights) { std::fill(std::begin(row), std::end(row), std::uint8_t{ 75 }); }
        body.animFrame = 2; body.itemValue = -50; body.generalPurpose1 = 128;
        DispatchObjectCase(body, boundary.context);
        Check(body.posX == 2000 && body.blendWeightOrSpriteOpacity == 2,
            "Room height exactly 150 blocks both components and reduces the phase step by four.");
        Fixture wrap(root);
        wrap.Dragon().blendWeightOrSpriteOpacity = 99;
        wrap.Dragon().itemValue = -50;
        DispatchObjectCase(wrap.Dragon(), wrap.context);
        Check(wrap.Dragon().animFrame == 2 && wrap.Dragon().modelFrame2 == 3 &&
            wrap.Dragon().blendWeightOrSpriteOpacity == 5 && wrap.Dragon().doorLockOrGroupBitmask == 1,
            "Walking must wrap strictly above 100 and select the next target frame.");
    }

    void TestProjectileEffects(const std::filesystem::path& root)
    {
        for (int kind : { 4, 5, 6 })
        {
            Fixture f(root);
            auto shot = f.Dragon();
            shot.behavior = BehaviorType::FlyingArrow; shot.collisionType = CollisionType::FlyingArrow;
            shot.itemValue = static_cast<std::int16_t>(kind); shot.health = 50;
            shot.behaviorParameter = 7; shot.spriteScale = 40; shot.modelAnim = 1; shot.animFrame = 3;
            shot.posX = f.context.worldObjects[1].posX; shot.posZ = f.context.worldObjects[1].posZ;
            shot.posY = f.context.worldObjects[1].posY; shot.velocityX = shot.velocityY = shot.velocityZ = 0;
            f.context.ReplaceWorldObject(2, shot);
            const auto previousLife = f.context.playerLife;
            DispatchObjectCase(f.context.worldObjects[2], f.context);
            Check(f.context.playerLife == previousLife - 29491 * 7 && f.context.lifeChanges == 1 &&
                f.context.worldObjects[2].behaviorParameter == 0,
                "All dragon projectile contacts must damage core playerLife once and clear their damage byte.");
            DispatchObjectCase(f.context.worldObjects[2], f.context);
            Check(f.context.lifeChanges == 1, "A lingering fire particle must not repeatedly damage the player.");
        }
        Fixture impact(root);
        auto shot = impact.Dragon();
        shot.itemValue = 5; shot.behavior = BehaviorType::FlyingArrow; shot.health = 500;
        shot.posY = -1000; shot.velocityX = shot.velocityY = shot.velocityZ = 0;
        shot.spriteScale = 40;
        impact.context.ReplaceWorldObject(2, shot);
        DispatchObjectCase(impact.context.worldObjects[2], impact.context);
        auto& flash = impact.context.worldObjects[2];
        Check(flash.behavior == BehaviorType::FireballImpact && flash.health == 10 && flash.spriteScale == 41,
            "Fireballs hitting terrain must become the native ten-tick flash.");
        impact.context.tickCount = 2;
        DispatchObjectCase(flash, impact.context);
        Check(flash.spriteScale == 71 && flash.posY == -996 && flash.collisionMask == 0 &&
            flash.blendWeightOrSpriteOpacity == 10 && flash.displayType == DisplayType::SpriteTransparent,
            "Impact flashes must expand, rise, disable collision and fade.");
        Fixture substeps(root);
        shot.velocityY = -1; shot.modelAnim = 1;
        substeps.context.ReplaceWorldObject(2, shot);
        substeps.context.tickCount = 3;
        DispatchObjectCase(substeps.context.worldObjects[2], substeps.context);
        const auto& substepFlash = substeps.context.worldObjects[2];
        Check(substepFlash.behavior == BehaviorType::FireballImpact && substepFlash.health == 10 &&
            substepFlash.posY == -1003 && substepFlash.spriteScale == 43 && substepFlash.modelAnim == 4,
            "Native flying-arrow dispatch must finish all substeps even after its helper sets impact behavior.");
    }

    void TestDeath(const std::filesystem::path& root)
    {
        Fixture f(root);
        auto& body = f.Dragon();
        body.groupIndexOrLock = 0;
        f.context.DamageObject(body, 0);
        Check(body.behavior == BehaviorType::DragonDying && body.modelAnim == 2 &&
            body.generalPurpose1 == 250 && body.itemValue == 4, "Lethal damage must enter the native dying bank.");
        f.context.tickCount = 249;
        DispatchObjectCase(body, f.context);
        Check(body.behavior == BehaviorType::DragonDying && body.animFrame == 4 && body.modelFrame2 == 4 &&
            body.blendWeightOrSpriteOpacity == 0, "Corpse must hold its final pose until the 250-tick explosion.");
        f.context.UpdateModelAttachments();
        Check(f.Head().health == 1000, "Head must remain attached through the death animation.");
        f.context.tickCount = 1;
        std::uint32_t seed = 91;
        f.context.randomSeed = seed;
        for (int part = 0; part < 22; ++part)
        {
            Next(seed, 3); Signed(seed, 7); Next(seed, 6); Signed(seed, 7);
            Next(seed, 512); Next(seed, 512); Next(seed, 512);
            if (part >= 2) { Next(seed, 2); }
        }
        for (int cloud = 0; cloud < 20; ++cloud)
        {
            Next(seed, 3); Next(seed, 512); Signed(seed, 30); Next(seed, 20); Signed(seed, 30);
            Next(seed, 6); Next(seed, 2); Next(seed, 20); Next(seed, 20);
        }
        DispatchObjectCase(body, f.context);
        Check(body.behavior == BehaviorType::None && body.collisionType == CollisionType::Key &&
            body.modelId == 39 && body.modelFrame2 == 0 && body.pickupType == PickupType::Type_4 &&
            body.itemValue == 1 && body.health == 1000 && body.spriteScale == 25,
            "Death must replace the original slot with the actual pickable unlit key.");
        Check(f.context.randomSeed == seed && f.context.created == 43 && f.context.destroyed == 1,
            "Death must preserve fragment/cloud RNG and lifecycle-safe body replacement.");
        int fragments = 0, clouds = 0;
        for (auto& object : f.context.worldObjects)
        {
            if (object.behavior == BehaviorType::BouncingObject)
            {
                ++fragments;
                Check(DispatchObjectCase(object, f.context).status == DispatchStatus::ExecutedCase,
                    "Both body chunks and randomized morphing fragments must have working bounce behavior.");
            }
            if (object.behavior == BehaviorType::FlyingArrow && object.itemValue == 6) { ++clouds; }
        }
        Check(fragments == 22 && clouds == 20, "Death must generate two corpse halves, twenty shards and twenty clouds.");
        f.context.UpdateModelAttachments();
        Check(f.Head().health == 0, "Explosion/key transition must not leave a ghost head.");
        const auto pickup = HandlePlayerObjectInteraction(body, f.context);
        Check(pickup.status == InteractionStatus::Applied && pickup.inventorySlot == 1 &&
            f.context.inventory[0].collisionType == CollisionType::Key && f.context.inventory[0].itemValue == 1 &&
            f.context.inventory[0].modelId == 39 && f.context.inventory[0].pickupType == PickupType::Type_4 &&
            body.health == 0, "The boss key must transfer its native inventory icon and lock ID into inventory.");
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc != 2) { throw std::invalid_argument("Expected original asset root."); }
        const std::filesystem::path root = argv[1];
        TestOriginalLevel(root);
        TestAttachment(root);
        TestAttacks(root);
        TestWalking(root);
        TestProjectileEffects(root);
        TestDeath(root);
        std::cout << "Original dragon attachment, attacks, projectiles, movement and death verified.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
