#include "GameContext.h"
#include "BehaviorDispatch.h"

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
    void Check(bool value, const std::string& message)
    {
        if (!value) { throw std::runtime_error(message); }
    }

    std::vector<std::uint8_t> Read(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file) { throw std::runtime_error("Missing fixture: " + path.string()); }
        return { std::istreambuf_iterator<char>(file), {} };
    }

    template<typename Action> void Reject(Action action)
    {
        bool rejected = false;
        try { action(); }
        catch (const std::runtime_error&) { rejected = true; }
        Check(rejected, "Malformed native input must fail explicitly.");
    }

    class Host final : public GameContext
    {
    public:
        std::filesystem::path assets;
        bool deferred = false;
        int created = 0, destroyed = 0, freshLoads = 0;
        explicit Host(std::filesystem::path path) : assets(std::move(path)) {}
        void OnObjectCreated(int) override { ++created; }
        void OnObjectDestroyed(int) override { ++destroyed; }
        void OnFreshLevelLoaded() override { ++freshLoads; }
        void Complete()
        {
            const auto request = *GetPendingLevelLoad();
            LevelLoadData data;
            LoadLevelGeometry(request.levelNumber, data);
            LoadLevelObjects(request.levelNumber, data);
            InitializeLevelPlayerSpawn(data, { -256, 0, 0 });
            CompleteLevelLoad(request.id, std::move(data));
        }
    protected:
        std::vector<std::uint8_t> LoadFile(std::string_view filename) override { return Read(assets / filename); }
        void LoadLevel(const LevelLoadRequest&) override { if (!deferred) { Complete(); } }
    };

    void TestOriginal(const std::filesystem::path& path, const std::filesystem::path& assets)
    {
        const auto bytes = Read(path);
        auto save = NativeSaveGame::Decode(bytes);
        Check(save.bytes == bytes, "Decoding must never mutate original SPL data.");
        save.SetChecksum();
        Check(save.bytes == bytes, "The original 32-bit checksum must reproduce every fixture byte.");
        Host host(assets);
        host.LoadGame(bytes);
        Check(host.freshLoads == 0, "Original saves must bypass fresh-level placement correction.");
        Check(host.GetRuntimeState() == GameRuntimeState::Ready && host.GetLoadedLevelNumber() == save.LevelNumber(),
            "The saved ordinal/order, not LEVELS.INF's story index, must identify the restored level.");
        Check(std::memcmp(&host.worldObjects[1], bytes.data() + NativeSaveGame::Player, 165 * 54) == 0,
            "Restoration must retain all 165 native world records: " + path.string());
        Check(std::memcmp(host.inventory.data(), bytes.data() + NativeSaveGame::Inventory, 14 * 54) == 0,
            "Restoration must retain both hands and all twelve backpack slots.");
        Check(host.playerLife == save.SignedDword(NativeSaveGame::Meters + 4) &&
            host.playerOxygen == save.SignedDword(NativeSaveGame::Meters + 24) &&
            host.gold == save.SignedWord(NativeSaveGame::Meters + 16) &&
            host.arrowPoison == save.SignedWord(NativeSaveGame::Meters + 43),
            "Native meters and inventory counters must not be reconstructed from level defaults.");
        auto captured = host.CaptureSaveGame(save.Name());
        if (captured != bytes)
        {
            const auto mismatch = std::mismatch(captured.begin(), captured.end(), bytes.begin(), bytes.end());
            throw std::runtime_error("Original -> core -> SPL differs at byte " +
                std::to_string(mismatch.first - captured.begin()) + ": " + path.string());
        }
        Check(host.created > 0, "Restored objects must announce lifecycle creation.");
        const auto before = host.worldObjects;
        auto broken = bytes;
        broken[NativeSaveGame::Geometry] ^= 1;
        Reject([&] { host.LoadGame(broken); });
        Check(host.GetRuntimeState() == GameRuntimeState::Ready &&
            std::memcmp(before.data(), host.worldObjects.data(), sizeof(before)) == 0,
            "Rejecting corrupt saves must not destroy the currently active game.");
    }

    void TestNewGame(const std::filesystem::path& assets)
    {
        Host host(assets);
        host.LoadGameParts();
        Check(host.GetGameParts().size() == 3 && host.GetGameParts()[1].levels.front() == 29 &&
            host.GetGameParts()[2].levels.front() == 9 && host.GetGameParts()[2].endsWithCredits,
            "HERZ must retain transitional indices and its END sentinel.");
        for (std::size_t part = 0; part < host.GetGameParts().size(); ++part)
        {
            for (const auto level : host.GetGameParts()[part].levels)
            {
                Check(host.CanApplyWeaponCheatLoadout(level) == (part == 0 || part == 2),
                    "Weapon cheats must follow HERZ training/part-2 membership and exclude every part-1 level.");
            }
        }
        Check(host.CanApplyWeaponCheatLoadout(50) && host.CanApplyWeaponCheatLoadout(10) &&
            host.CanApplyWeaponCheatLoadout(90) && !host.CanApplyWeaponCheatLoadout(30) &&
            !host.CanApplyWeaponCheatLoadout(36) && !host.CanApplyWeaponCheatLoadout(255),
            "Weapon cheat eligibility must cover training, part-2 shop, nonnumeric part-1 order and unknown levels.");
        host.StartNewGame(1);
        Check(host.freshLoads == 1, "A fresh level must run placement correction exactly once.");
        host.worldObjects[2].generalPurpose1 = 1234;
        host.gold = 57;
        host.playerLife = 123 * 65536 + 42;
        host.playerOxygen = 71 * 65536;
        host.hasMap = true;
        host.SetCollisionMapPixelAtWorldXZ(1000, 1000, 2);
        auto bytes = host.CaptureSaveGame("Fresh native save");
        auto parsed = NativeSaveGame::Decode(bytes);
        Check(parsed.Word(21) == 38 && parsed.LevelNumber() == 50 &&
            parsed.bytes.size() == bytes.size(), "Fresh snapshots must use the original version/layout.");
        Check(parsed.Word(NativeSaveGame::Meters + 8) == 123 &&
            parsed.Word(NativeSaveGame::Meters + 18) == 57 && parsed.Word(NativeSaveGame::Meters + 28) == 71,
            "Fresh saves must seed native displayed meter words, not show empty bars after a DOS load.");
        Host loaded(assets);
        loaded.deferred = true;
        loaded.LoadGame(bytes);
        Check(loaded.GetRuntimeState() == GameRuntimeState::Loading && !loaded.hasActiveGame &&
            loaded.GetPendingSaveGame(), "Deferred restores must retain the validated snapshot until completion.");
        loaded.Complete();
        Check(loaded.freshLoads == 0, "Deferred saves must also bypass fresh-level placement correction.");
        Check(loaded.gold == 57 && loaded.playerLife == host.playerLife && loaded.playerOxygen == host.playerOxygen &&
            loaded.hasMap && loaded.worldObjects[2].generalPurpose1 == 1234 &&
            loaded.CaptureSaveGame("Fresh native save") == bytes,
            "Port-created saves must retain objects, fractional meters, collision edits and inventory on reload.");
        auto malformed = bytes;
        malformed.resize(200);
        Reject([&] { NativeSaveGame::Decode(malformed); });
        malformed = bytes;
        malformed[0] = 21;
        Reject([&] { NativeSaveGame::Decode(malformed); });
        malformed = bytes;
        malformed[NativeSaveGame::Map] = 255;
        Reject([&] { NativeSaveGame::Decode(malformed); });
        malformed = bytes;
        malformed[227] = 0;
        Reject([&] { NativeSaveGame::Decode(malformed); });
        const auto herz = Read(assets / "GRAFIK\\HERZ.256");
        auto badHerz = herz;
        badHerz[4] = 255;
        Reject([&] { DecodeGameParts(badHerz.data(), badHerz.size()); });
        Reject([&] { DecodeGameParts(herz.data(), herz.size() - 1); });
    }

    void TestCustomOrderAndGeometry(const std::filesystem::path& assets)
    {
        std::vector<std::uint8_t> herz(4 + 16 * 4);
        herz[0] = 15; herz[2] = 3;
        const std::string names[]{ "Custom mission", "Another part" };
        for (std::size_t part = 0; part < 2; ++part)
        {
            const auto row = 4 + part * 32;
            herz[row] = static_cast<std::uint8_t>(names[part].size());
            std::copy(names[part].begin(), names[part].end(), herz.begin() + row + 1);
            herz[row + 16] = 2;
        }
        herz[21] = 30; herz[22] = 34;
        herz[53] = 10; herz[54] = 16; herz[55] = 96;
        Host host(assets);
        host.SetGameParts(herz.data(), herz.size());
        host.StartNewGame(1);
        Check(host.GetLoadedLevelNumber() == 30 && host.GetGameParts()[0].name == names[0],
            "Custom HERZ data, not a compiled list, must select labels and part starts.");
        host.EndLevel();
        Check(host.GetLoadedLevelNumber() == 34, "Custom HERZ order must control progression.");
        const auto custom = NativeSaveGame::Decode(host.CaptureSaveGame("Custom order"));
        Check(custom.LevelOrder() == std::vector<std::uint8_t>{ 30, 34 } && custom.Word(227) == 2,
            "Native saves must retain the actual HERZ sequence and ordinal.");
        host.EndLevel();
        Check(host.GetMenuPage() == MenuPage::MainMenu, "Custom part completion must use its list boundary.");
        host.StartNewGame(2);
        host.EndLevel();
        Check(host.GetLoadedLevelNumber() == 16, "Reordered HERZ lists must not use numeric level addition.");
        auto front = std::find_if(host.worldObjects.begin(), host.worldObjects.end(),
            [](const auto& object) { return object.behavior == BehaviorType::ExtinguishLava; });
        Check(front != host.worldObjects.end(), "Expected an original forest flooding front.");
        host.SetObjectHealth(*front, 1000);
        host.tickCount = 40;
        UpdateObject(*front, host);
        const auto slot = static_cast<std::size_t>(front - host.worldObjects.begin());
        host.level->wallTextures[39][49] = 0;
        host.level->floorObjects[39][49] = 0;
        host.level->ceilingObjects[39][49] = 0;
        host.level->floorHeights[39][49] = 123;
        host.level->roomHeights[39][49] = 42;
        host.level->surfaceFlags[39][49] = 128;
        host.level->collisionMaterials[39][49] = 2;
        auto bytes = host.CaptureSaveGame("Partial river");
        Host restored(assets);
        restored.LoadGame(bytes);
        Check(restored.CaptureSaveGame("Partial river") == bytes,
            "All seven modified native planes, collision pixels and partial flood state must survive a save.");
        host.tickCount = restored.tickCount = 40;
        UpdateObject(host.worldObjects[slot], host);
        UpdateObject(restored.worldObjects[slot], restored);
        Check(restored.CaptureSaveGame("Partial river") == host.CaptureSaveGame("Partial river"),
            "A restored flood must continue identically using the saved part list and native front fields.");
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc != 4) { throw std::invalid_argument("Expected assets, save fixtures and original game paths."); }
        std::size_t count = 0;
        for (const auto& root : { std::filesystem::path(argv[2]), std::filesystem::path(argv[3]) })
        {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
            {
                if (entry.is_regular_file() && entry.path().extension() == ".SPL")
                {
                    TestOriginal(entry.path(), argv[1]);
                    ++count;
                }
            }
        }
        TestNewGame(argv[1]);
        TestCustomOrderAndGeometry(argv[1]);
        std::cout << "PASS: " << count << " original SPL/core byte-exact round trips, native 32-bit checksum, "
            "fresh/deferred restores, all world/inventory/meter/map state and malformed input.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
