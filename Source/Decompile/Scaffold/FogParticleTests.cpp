#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>

using namespace Desperabis;

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition) { throw std::runtime_error(message); }
    }

    std::uint16_t Random(std::uint32_t& seed, std::uint16_t range)
    {
        seed = seed * 134775813u + 1u;
        return static_cast<std::uint16_t>((seed >> 16) % range);
    }

    std::int16_t Signed(std::uint32_t& seed, std::uint16_t radius)
    {
        const auto value = static_cast<std::int16_t>(Random(seed, 2 * radius + 1) - radius);
        return value == 0 ? static_cast<std::int16_t>(radius) : value;
    }

    struct Host final : GameContext
    {
        std::filesystem::path original;
        std::vector<std::uint8_t> LoadFile(std::string_view name) override
        {
            std::ifstream file(original / std::string(name), std::ios::binary);
            if (!file) { throw std::runtime_error("Missing fog fixture: " + std::string(name)); }
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

    void TestFog(const std::filesystem::path& original)
    {
        Host host;
        auto geometry = CreateDummyLevel();
        for (std::size_t row = 0; row < Level::Height; ++row)
        {
            std::fill(std::begin(geometry.floorHeights[row]), std::end(geometry.floorHeights[row]), 0);
            std::fill(std::begin(geometry.floorObjects[row]), std::end(geometry.floorObjects[row]), 0);
            std::fill(std::begin(geometry.ceilingObjects[row]), std::end(geometry.ceilingObjects[row]), 0);
        }
        geometry.heightSurfaces[0] = Level::HeightSurface{0, 0};
        host.level = &geometry;
        host.collisionMap = geometry.collisionMap.get();
        host.soundListenerX = host.soundListenerZ = 1500;
        auto& cloud = host.worldObjects[2];
        cloud.health = 1000;
        cloud.behavior = BehaviorType::FogParticle;
        cloud.posX = host.soundListenerX;
        cloud.posZ = host.soundListenerZ;
        for (int distance = 0; distance <= 1000; ++distance)
        {
            cloud.posX = WrapSignedWord(host.soundListenerX + distance);
            Check(DispatchObjectCase(cloud, host).status == DispatchStatus::ExecutedCase &&
                cloud.blendWeightOrSpriteOpacity == std::clamp(distance - 50, 0, 320),
                "Modern fog must preserve every fade-distance unit without the native coarse transparency bands.");
        }
        for (const auto phase : {0, 128, 256, 384, 511})
        {
            cloud.generalPurpose2 = static_cast<std::uint16_t>(phase);
            cloud.velocityZ = 0;
            DispatchObjectCase(cloud, host);
            const auto product = Sine(static_cast<std::int16_t>(phase)) * 5;
            const auto bob = product >= 0 ? product / 65536 : -((-product + 65535) / 65536);
            Check(cloud.posY == host.GetFloorHeight(cloud.posX, cloud.posY, cloud.posZ) + 6 + bob,
                "Fog bobbing must use the clamped native sine table and downward-rounded high-word product.");
        }
        for (std::uint32_t initial = 0; initial < 1000; ++initial)
        {
            host.randomSeed = initial;
            cloud.generalPurpose2 = 510;
            cloud.velocityZ = 3;
            host.tickCount = initial % 2 ? 0 : 7;
            auto seed = initial;
            const auto velocity = Random(seed, 40) == 0 ? Signed(seed, 4) : 3;
            DispatchObjectCase(cloud, host);
            Check(cloud.generalPurpose2 == 1 && cloud.velocityZ == velocity && host.randomSeed == seed,
                "Fog must advance once per dispatch using its old velocity, with exact one-in-40 speed-change RNG.");
        }
        host.original = original;
        host.StartLevel(24);
        int clouds = 0;
        for (auto& authored : host.worldObjects)
        {
            if (authored.behavior != BehaviorType::FogParticle || authored.health <= 0) { continue; }
            const auto oldX = authored.posX, oldZ = authored.posZ;
            for (int frame = 0; frame < 100; ++frame)
            {
                host.soundListenerX = oldX;
                host.soundListenerZ = oldZ;
                Check(DispatchObjectCase(authored, host).status == DispatchStatus::ExecutedCase &&
                    authored.blendWeightOrSpriteOpacity == 0 && authored.posX == oldX && authored.posZ == oldZ,
                    "Every original swamp cloud must fade completely at proximity while bobbing in place.");
                host.soundListenerX = WrapSignedWord(oldX + 434);
                DispatchObjectCase(authored, host);
                Check(authored.blendWeightOrSpriteOpacity == 320,
                    "Every original swamp cloud must reach its modern capped fade-distance endpoint.");
            }
            ++clouds;
        }
        Check(clouds == 29, "The fog regression must cover all 29 original swamp clouds.");
    }
}

int main(int argc, char** argv)
{
    try
    {
        Check(argc == 2, "Expected the original asset directory.");
        TestFog(argv[1]);
        std::cout << "Full-resolution fog fading, native bobbing/RNG and all 29 swamp clouds passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
