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

    struct Input final : InputHandler
    {
        bool interact = false;
        bool IsKeyPressed(InputKey key) override { return interact && key == InputKey::InteractionKey; }
    };

    struct Host final : GameContext
    {
        std::filesystem::path assets;
        int created = 0, flashes = 0;
        explicit Host(const std::filesystem::path& path) : assets(path)
        {
            StartNewGame(2);
            StartLevel(36);
            Check(GetRuntimeState() == GameRuntimeState::Ready, "Original rooftop fixture must load.");
            basicPlayerMovementEnabled = false;
        }
        void Keep(std::initializer_list<int> slots)
        {
            for (int slot = 2; slot <= 165; ++slot)
            {
                if (std::find(slots.begin(), slots.end(), slot) == slots.end()) { SetObjectHealth(worldObjects[slot], 0); }
            }
        }
        void StandAt(const RawGameObject& object, std::int16_t offsetX = 0)
        {
            worldObjects[1].posX = WrapSignedWord(object.posX + offsetX);
            worldObjects[1].posY = object.posY;
            worldObjects[1].posZ = object.posZ;
            soundListenerX = worldObjects[1].posX;
            soundListenerZ = worldObjects[1].posZ;
        }
        void OnObjectCreated(int) override { ++created; }
        void OnLightFlash(const LightFlash&) override { ++flashes; }
    protected:
        std::vector<std::uint8_t> LoadFile(std::string_view name) override
        {
            std::ifstream file(assets / name, std::ios::binary);
            if (!file) { throw std::runtime_error("Missing rooftop fixture: " + std::string(name)); }
            return {std::istreambuf_iterator<char>(file), {}};
        }
        void LoadLevel(const LevelLoadRequest& request) override
        {
            LevelLoadData data;
            LoadLevelGeometry(request.levelNumber, data);
            LoadLevelObjects(request.levelNumber, data);
            InitializeLevelPlayerSpawn(data, {0, 0, 100});
            CompleteLevelLoad(request.id, std::move(data));
        }
    };

    void TestContainers(const std::filesystem::path& assets)
    {
        for (const auto pair : {std::pair{59,60}, {64,70}, {67,68}, {92,93}})
        {
            Host host(assets);
            host.Keep({pair.first, pair.second});
            auto& box = host.worldObjects[pair.first];
            auto& item = host.worldObjects[pair.second];
            const auto originalItem = item;
            host.StandAt(box);
            Input input;
            Check(UpdatePlayerInteraction(host, input).status == InteractionStatus::NoEffect &&
                box.blendWeightOrSpriteOpacity == 0, "Opening requires held interaction input.");
            Function_ItemHiddenInBox(item, host);
            Check(item.collisionMask == 0, "Dormant contents must not be made pickable.");
            input.interact = true;
            host.tickCount = 99;
            host.frameTickCount = 1;
            for (int i = 0; i < 16; ++i)
            {
                Check(UpdatePlayerInteraction(host, input).status == InteractionStatus::Applied,
                    "Each original container must accept held interaction.");
            }
            Check(box.blendWeightOrSpriteOpacity == 96 && item.health > 1000 && item.collisionMask == 0,
                "Container opening uses frame ticks, not behavior ticks, and must not reveal contents early.");
            box.blendWeightOrSpriteOpacity = 94;
            UpdatePlayerInteraction(host, input);
            Check(box.blendWeightOrSpriteOpacity == 100 && box.collisionMask == 1 && item.health > 1000,
                "Landing exactly on phase 100 must not activate the contents.");
            input.interact = false;
            UpdatePlayerInteraction(host, input);
            Check(item.health > 1000, "Releasing interaction must pause opening, including at the endpoint.");
            input.interact = true;
            UpdatePlayerInteraction(host, input);
            Check(box.collisionType == CollisionType::Switch && box.collisionMask == 0 &&
                box.velocityX == 0 && box.blendWeightOrSpriteOpacity == 100 && item.health == 1000,
                "Overshooting the endpoint must activate the authored group and finish the container once.");
            UpdateObject(item, host);
            Check(item.collisionMask == 1, "Activated contents must acquire ordinary pickup collision.");
            host.StandAt(item);
            const auto result = UpdatePlayerInteraction(host, input);
            Check(result.status == InteractionStatus::Applied && item.health == 0 && result.inventorySlot != 0,
                "Every original revealed key and medicine must be reachable through the public pickup path.");
            const auto& stored = host.inventory[result.inventorySlot - 1];
            Check(stored.itemValue == originalItem.itemValue && stored.collisionType == originalItem.collisionType,
                "Opening must preserve the authored reward type and value.");
        }
    }

    void TestRooftopSequence(const std::filesystem::path& assets)
    {
        Host host(assets);
        host.Keep({30,33,34,36,37});
        auto& roof = host.worldObjects[30];
        auto& lever = host.worldObjects[33];
        auto& timer = host.worldObjects[34];
        auto& strike = host.worldObjects[36];
        auto& pole = host.worldObjects[37];
        Check(roof.health == 1001 && timer.health == 1003 && strike.health == 1001 &&
            timer.generalPurpose1 == 150, "Original rooftop event groups and countdown must load.");
        Function_RoofTopThunderHandler1(timer, host);
        Function_RoofTopThunderHandler2(strike, host);
        Function_DestroyableRoof(roof, host);
        Check(timer.generalPurpose1 == 150 && strike.displayType == DisplayType::Hidden && roof.modelAnim == 4,
            "Dormant rooftop controllers must leave their authored state unchanged.");
        pole.posX = lever.posX; pole.posY = lever.posY; pole.posZ = lever.posZ;
        Function_MetalPole(pole, host);
        Check(pole.health == 0 && lever.collisionType == CollisionType::Switch,
            "Fitting the original pole must enable the rooftop switch.");
        host.StandAt(lever);
        Input input;
        input.interact = true;
        Check(UpdatePlayerInteraction(host, input).status == InteractionStatus::Applied && timer.health == 1000,
            "Actual switch interaction must activate the delayed rooftop event.");
        input.interact = false;
        host.StandAt(roof, 250);
        host.Tick({150,0}, input);
        Check(timer.generalPurpose1 == 0 && roof.health == 1001 && strike.health == 1001,
            "The event must wait until the countdown becomes negative, not merely zero.");
        auto save = host.CaptureSaveGame("Roof waiting");
        Host restored(assets);
        restored.LoadGame(std::move(save));
        restored.basicPlayerMovementEnabled = false;
        Check(restored.worldObjects[34].generalPurpose1 == 0 && restored.worldObjects[30].health == 1001,
            "A native save must preserve the pending strike rather than prematurely exploding.");
        const auto created = host.created;
        const auto revision = host.level->geometryRevision;
        Check((host.level->surfaceFlags[9][16] & 31) == 28, "The original rooftop tile must use selector 28.");
        host.Tick({1,0}, input);
        Check(timer.behavior == BehaviorType::None && roof.health == 1000 && roof.modelAnim == 4 &&
            strike.health == 20 && strike.displayType == DisplayType::VectorLavaTint &&
            strike.animFrame >= 1 && strike.animFrame <= 4 && strike.modelFrame2 >= 1 &&
            strike.modelFrame2 <= 4 && strike.animFrame != strike.modelFrame2,
            "Slot order must reveal the animated strike now and defer the earlier roof slot until next tick.");
        host.Tick({1,0}, input);
        Check(roof.behavior == BehaviorType::None && roof.modelAnim == 5 && roof.animFrame == 4 &&
            host.worldObjects[33].health == 19 && host.worldObjects[33].collisionMask == 0,
            "The blast must select the broken roof and retire its overlapping switch.");
        Check(host.created - created == 23 && host.flashes == 1 &&
            host.level->surfaceFlags[9][16] == 160 && host.level->geometryRevision == revision + 1 &&
            host.playerLife > 0, "The blast must emit 16 projectiles and seven smoke particles, change terrain and spare distant players.");
        restored.Tick({1,0}, input);
        restored.Tick({1,0}, input);
        Check(restored.worldObjects[30].behavior == BehaviorType::None &&
            restored.level->surfaceFlags[9][16] == 160 && restored.flashes == 1,
            "Restored countdowns must complete the same one-shot roof destruction.");
        save = host.CaptureSaveGame("Roof broken");
        restored.LoadGame(std::move(save));
        restored.basicPlayerMovementEnabled = false;
        restored.flashes = 0;
        restored.Tick({1,0}, input);
        Check(restored.worldObjects[30].modelAnim == 5 && restored.worldObjects[30].animFrame == 4 &&
            restored.flashes == 0, "A saved broken roof must not explode again.");
        host.Tick({20,0}, input);
        Check(strike.health == 0, "The strike's native finite lifetime must remove it.");
    }

    void TestBlastBoundary(const std::filesystem::path& assets)
    {
        for (const auto distance : {199,200})
        {
            Host host(assets);
            host.Keep({30});
            auto& roof = host.worldObjects[30];
            host.StandAt(roof, static_cast<std::int16_t>(distance));
            host.SetObjectHealth(roof, 1000);
            Function_DestroyableRoof(roof, host);
            Check((host.playerLife == 0) == (distance < 200),
                "The roof blast must deplete player life strictly inside the native 200-unit X/Z radius.");
            Check(host.worldObjects[1].health > 0,
                "Blast damage must use player life, not the player object's active-state word.");
        }
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc != 2) { throw std::invalid_argument("Expected original asset root."); }
        const std::filesystem::path assets = argv[1];
        TestContainers(assets);
        TestRooftopSequence(assets);
        TestBlastBoundary(assets);
        std::cout << "PASS: original rooftop containers/rewards, pole-switch countdown, strike/blast, save continuation and damage boundary.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
