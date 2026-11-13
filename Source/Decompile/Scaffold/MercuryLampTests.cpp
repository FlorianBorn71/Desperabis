#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <algorithm>
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

    struct Audio final : AudioHandler
    {
        std::array<bool, SoundChannelCount> playing{};
        int plays = 0;
        std::uint8_t lastSample = 0;
        std::uint16_t GetSampleCount() const override { return 129; }
        bool IsChannelPlaying(std::size_t channel) const override { return playing.at(channel); }
        void PlaySample(std::size_t channel, std::uint8_t sample, std::uint8_t) override
        {
            playing.at(channel) = true;
            lastSample = sample;
            ++plays;
        }
        void StopAllSamples() override { playing.fill(false); }
    };

    struct Host final : GameContext
    {
        std::filesystem::path assets;
        explicit Host(const std::filesystem::path& path) : assets(path)
        {
            StartNewGame(2);
            StartLevel(34);
            Check(GetRuntimeState() == GameRuntimeState::Ready, "Original level 34 must load.");
            basicPlayerMovementEnabled = false;
        }
        void Keep(std::initializer_list<int> slots)
        {
            for (int slot = 2; slot <= 165; ++slot)
                if (std::find(slots.begin(), slots.end(), slot) == slots.end())
                    SetObjectHealth(worldObjects[slot], 0);
        }
        int RayCount() const
        {
            return static_cast<int>(std::count_if(worldObjects.begin(), worldObjects.end(), [](const auto& object)
            {
                return object.health > 0 && object.collisionType == CollisionType::MercuryRaySegment;
            }));
        }
        void Equip(std::uint16_t slot)
        {
            weaponHand = slot;
            selectedWeapon = WeaponType::TrapOrPole;
            weaponReady = true;
            weaponAnimationSpeed = 0;
        }
    protected:
        std::vector<std::uint8_t> LoadFile(std::string_view name) override
        {
            std::ifstream file(assets / name, std::ios::binary);
            if (!file) { throw std::runtime_error("Missing level 34 fixture: " + std::string(name)); }
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

    void TestBones(const std::filesystem::path& assets)
    {
        Audio audio;
        Host host(assets);
        host.audioHandler = &audio;
        std::ifstream table(assets / "SAMP\\S_TAB.PRB", std::ios::binary);
        const std::vector<std::uint8_t> effects{std::istreambuf_iterator<char>(table), {}};
        host.LoadSoundEffects(effects.data(), effects.size());
        host.Keep({12,41,43,45,46,47});
        auto& coffin = host.worldObjects[12];
        auto& bones = host.worldObjects[43];
        Function_BonesToGrab(bones, host);
        Check(bones.collisionMask == 0, "Closed coffin must keep its bones unpickable.");
        for (int tick = 0; tick < 51; ++tick) { HandlePlayerObjectInteraction(coffin, host); }
        Check(audio.plays == 0, "Opening the original coffin must remain silent, including held interaction.");
        Check(bones.health == 1000 && host.worldObjects[47].health == 1000,
            "Opening the original coffin must reveal all four bones.");
        Function_BonesToGrab(bones, host);
        const auto pickup = HandlePlayerObjectInteraction(bones, host);
        Check(pickup.status == InteractionStatus::Applied && pickup.inventorySlot != 0,
            "Revealed bones must enter inventory.");
        host.Equip(pickup.inventorySlot);
        const auto view = host.GetWeaponView();
        Check(view && view->kind == WeaponView::Kind::Model && view->object.modelId == 36 &&
            view->object.modelAnim == 4 && view->object.animFrame == 3,
            "Equipped bones must use their original first-person model.");
        const auto& dogBefore = host.worldObjects[41];
        host.worldObjects[1].posX = dogBefore.posX;
        host.worldObjects[1].posY = dogBefore.posY;
        host.worldObjects[1].posZ = dogBefore.posZ;
        Check(host.AttackWithSelectedWeapon() == WeaponActionStatus::Applied &&
            host.inventory[pickup.inventorySlot - 1].health == 0,
            "A bone must be placeable through the normal weapon action.");
        auto placed = std::find_if(host.worldObjects.begin() + 2, host.worldObjects.end(), [](const auto& object)
        {
            return object.health == 1000 && object.behavior == BehaviorType::BonesToGrab &&
                object.renderPlacementMode != 20;
        });
        Check(placed != host.worldObjects.end(), "Dropped bone must detach from its coffin.");
        Function_BonesToGrab(*placed, host);
        Check(placed->posY == host.GetTerrainFloorHeight(placed->posX, placed->posY, placed->posZ) + 3,
            "Dropped bones must rest just above terrain instead of intersecting it.");
        Function_BonesToGrab(*placed, host);
        auto& dog = host.worldObjects[41];
        Check(dog.behavior == BehaviorType::IdleAnimation && dog.modelAnim == 4 &&
            dog.collisionMask == 0 && placed->behavior == BehaviorType::None,
            "A dropped bone must calm the original dog into its chewing animation.");
    }

    void TestVaseKey(const std::filesystem::path& assets)
    {
        for (const bool carry : {false,true})
        {
            Host host(assets);
            host.Keep({64});
            auto* vase = &host.worldObjects[64];
            if (carry)
            {
                const auto pickup = HandlePlayerObjectInteraction(*vase, host);
                host.Equip(pickup.inventorySlot);
                Check(host.AttackWithSelectedWeapon() == WeaponActionStatus::Applied,
                    "The original vase must be placeable before breaking.");
                vase = nullptr;
                for (auto& object : host.worldObjects)
                    if (object.health == 1000 && object.behavior == BehaviorType::ActionObjectToGrab)
                        vase = &object;
                Check(vase != nullptr, "Carrying must retain the vase object.");
                Function_ActionObjectToGrab(*vase, host);
            }
            Check(vase->generalPurpose3 == 40, "Carrying and placement must preserve the vase's hidden key ID.");
            host.DamageObject(*vase, 3000);
            Check(vase->health == 1000 && vase->collisionType == CollisionType::Key &&
                vase->itemValue == 40 && vase->collisionMask == 3,
                "Breaking the original or carried vase must reveal its authored key.");
            host.worldObjects[1].posX = vase->posX;
            host.worldObjects[1].posY = vase->posY;
            host.worldObjects[1].posZ = vase->posZ;
            struct Input final : InputHandler
            {
                bool IsKeyPressed(InputKey key) override { return key == InputKey::InteractionKey; }
            } input;
            const auto pickup = UpdatePlayerInteraction(host, input);
            Check(pickup.status == InteractionStatus::Applied && pickup.inventorySlot > 0 &&
                host.inventory[pickup.inventorySlot - 1].collisionType == CollisionType::Key &&
                host.inventory[pickup.inventorySlot - 1].itemValue == 40,
                "The revealed key must be pickable through actual player collision and input.");
        }
    }

    void TestPortables(const std::filesystem::path& assets)
    {
        for (const int slot : {58,64})
        {
            Host host(assets);
            host.Keep({slot});
            const auto original = host.worldObjects[slot];
            const auto pickup = HandlePlayerObjectInteraction(host.worldObjects[slot], host);
            Check(pickup.status == InteractionStatus::Applied &&
                host.inventory[pickup.inventorySlot - 1].collisionType == CollisionType::Weapon,
                "Mirror and vase must use the native placeable weapon inventory.");
            host.Equip(pickup.inventorySlot);
            const auto heading = host.GetPlayerHeading();
            Check(host.GetWeaponView()->kind == WeaponView::Kind::Model &&
                host.AttackWithSelectedWeapon() == WeaponActionStatus::Applied,
                "Portable objects must be visible in-hand and placeable.");
            auto dropped = std::find_if(host.worldObjects.begin() + 2, host.worldObjects.end(), [](const auto& object)
            {
                return object.health == 1000 && object.behavior == BehaviorType::ActionObjectToGrab;
            });
            Check(dropped != host.worldObjects.end(), "Placed mirror/vase must retain its behavior.");
            Function_ActionObjectToGrab(*dropped, host);
            Check(dropped->collisionType == original.collisionType && dropped->winY == -heading &&
                (dropped->posX - 25) % 50 == 0 && (dropped->posZ - 25) % 50 == 0,
                "Placed mirror/vase must restore interaction, retain chosen facing and snap to the native grid.");
            Check(HandlePlayerObjectInteraction(*dropped, host).status == InteractionStatus::Applied,
                "Placed portable objects must be pickable again.");
        }
    }

    void TestLamp(const std::filesystem::path& assets)
    {
        Host host(assets);
        host.Keep({58,66,67,68,69});
        auto& lamp = host.worldObjects[66];
        Function_MercuryLamp(lamp, host);
        Check(host.RayCount() == 0 && lamp.animFrame == 1, "Dormant lamp must emit no ray.");
        Check(HandlePlayerObjectInteraction(host.worldObjects[69], host).status == InteractionStatus::Applied &&
            lamp.health == 1000, "Original wall switch must activate the mercury lamp.");
        Function_MercuryLamp(lamp, host);
        Check(lamp.animFrame == 4 && lamp.modelFrame2 == 0 && host.RayCount() > 0 &&
            lamp.generalPurpose1 == 1, "Active original lamp must produce beam meshes and a wall-hit endpoint.");
        const auto count = host.RayCount();
        Function_MercuryLamp(lamp, host);
        Check(host.RayCount() == count, "Beam refresh must honor the native interval.");
        const auto snapshot = host.CaptureSaveGame("Mercury");
        Host restored(assets);
        restored.LoadGame(snapshot);
        Check(restored.RayCount() == count && restored.worldObjects[66].generalPurpose1 == 1,
            "Native saves must preserve beams, refresh timer and endpoint lighting.");

        auto& mirror = host.worldObjects[58];
        auto& receiver = host.worldObjects[68];
        // Clear only the test ray corridor; authored models/groups remain unchanged.
        for (int row = 0; row < 200; ++row)
            for (int column = 0; column < 200; ++column)
                host.SetCollisionMapPixelAtWorldXZ(static_cast<std::int16_t>(112 + column * 25),
                    static_cast<std::int16_t>(38 + (162 - row) * 25), 0);
        lamp.posX = 2475; lamp.posY = 26; lamp.posZ = 1875; lamp.winY = 0;
        mirror.posX = 2475; mirror.posY = 26; mirror.posZ = 1775; mirror.winY = 64;
        receiver.posX = 2275; receiver.posY = 26; receiver.posZ = 1775;
        lamp.blendWeightOrSpriteOpacity = 0;
        Function_MercuryLamp(lamp, host);
        Check(receiver.collisionType == CollisionType::None && receiver.animFrame == 3 &&
            receiver.behaviorParameter == static_cast<std::uint8_t>(CollisionType::LightSensor) &&
            host.worldObjects[67].generalPurpose2 == 3,
            "A reflected ray must activate the original sensor and its door group.");
        Function_Door(host.worldObjects[67], host);
        Check(host.worldObjects[67].blendWeightOrSpriteOpacity > 0,
            "The activated receiver door must actually advance its opening animation.");
        const auto litSave = host.CaptureSaveGame("Sensor lit");
        Host litRestored(assets);
        litRestored.LoadGame(litSave);
        Check(litRestored.worldObjects[68].behaviorParameter ==
            static_cast<std::uint8_t>(CollisionType::LightSensor),
            "The target switch's persistent red-light activation must survive a native save.");
        receiver.collisionMask = 0;
        lamp.blendWeightOrSpriteOpacity = 0;
        Function_MercuryLamp(lamp, host);
        Check(host.RayCount() == 61 && lamp.generalPurpose1 == 0,
            "An unobstructed ray must stop at the 61-segment budget without retaining a stale wall light.");
        bool reflected = false;
        for (const auto& ray : host.worldObjects)
            if (ray.health > 0 && ray.collisionType == CollisionType::MercuryRaySegment)
            {
                Check(ray.collisionMask == 0 && ray.modelAnim == 2 && ray.animFrame == 2,
                    "Visible beam segments must not obstruct players or each other.");
                reflected |= ray.posX < mirror.posX && ray.winY == 128;
            }
        Check(reflected, "The ray must change direction using the mirror's native reflection formula.");
        mirror.collisionType = CollisionType::Door;
        lamp.blendWeightOrSpriteOpacity = 0;
        Function_MercuryLamp(lamp, host);
        Check(host.RayCount() < 5 && lamp.generalPurpose1 == 0,
            "Ordinary solid objects must stop the ray without generating a wall light.");
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc != 2) { throw std::runtime_error("Expected original asset directory."); }
        TestBones(argv[1]);
        TestPortables(argv[1]);
        TestVaseKey(argv[1]);
        TestLamp(argv[1]);
        std::cout << "Level 34 bones, dogs, portable objects and reflected mercury lamp passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
