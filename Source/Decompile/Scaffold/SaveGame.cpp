#include "GameContext.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace Desperabis
{
    namespace
    {
        std::uint32_t Checksum(const std::vector<std::uint8_t>& bytes)
        {
            std::uint32_t sum = 0;
            for (std::size_t i = 0; i < bytes.size() - 4; ++i) { sum += bytes[i]; }
            return sum;
        }
    }

    std::vector<GamePart> DecodeGameParts(const std::uint8_t* bytes, std::size_t size)
    {
        if (!bytes || size < 4) { throw std::runtime_error("HERZ requires an image header."); }
        const auto width = static_cast<std::size_t>(bytes[0] | (bytes[1] << 8)) + 1;
        const auto height = static_cast<std::size_t>(bytes[2] | (bytes[3] << 8)) + 1;
        if (width < 2 || width > 256 || height < 2 || height > 6 || height % 2 ||
            size != 4 + width * height)
        {
            throw std::runtime_error("HERZ requires paired name/level rows for one to three parts.");
        }
        std::vector<GamePart> parts;
        for (std::size_t row = 0; row < height; row += 2)
        {
            const auto* name = bytes + 4 + row * width;
            const auto* order = name + width;
            if (name[0] == 0 || name[0] >= width || order[0] == 0 ||
                order[0] > 62 || order[0] >= width)
            {
                throw std::runtime_error("HERZ contains an invalid part name or level list.");
            }
            GamePart part;
            part.name.assign(reinterpret_cast<const char*>(name + 1), name[0]);
            while (!part.name.empty() && part.name.back() == '\0') { part.name.pop_back(); }
            if (part.name.empty()) { throw std::runtime_error("HERZ contains an empty part name."); }
            part.levels.assign(order + 1, order + 1 + order[0]);
            if (std::any_of(part.levels.begin(), part.levels.end(), [](auto level) { return level == 0 || level == 96; }))
            {
                throw std::runtime_error("HERZ contains an invalid level index.");
            }
            part.endsWithCredits = static_cast<std::size_t>(order[0]) + 1 < width && order[order[0] + 1] == 96;
            parts.push_back(std::move(part));
        }
        return parts;
    }

    std::uint16_t NativeSaveGame::Word(std::size_t offset) const
    {
        return static_cast<std::uint16_t>(bytes.at(offset) | (bytes.at(offset + 1) << 8));
    }

    std::int16_t NativeSaveGame::SignedWord(std::size_t offset) const
    {
        const auto word = Word(offset);
        return static_cast<std::int16_t>(word <= 0x7fff ? word : static_cast<int>(word) - 0x10000);
    }

    std::int32_t NativeSaveGame::SignedDword(std::size_t offset) const
    {
        const auto value = static_cast<std::uint32_t>(Word(offset)) |
            (static_cast<std::uint32_t>(Word(offset + 2)) << 16);
        return value <= 0x7fffffffu ? static_cast<std::int32_t>(value) :
            static_cast<std::int32_t>(static_cast<std::int64_t>(value) - 0x100000000LL);
    }

    void NativeSaveGame::SetWord(std::size_t offset, std::uint16_t value)
    {
        bytes.at(offset) = static_cast<std::uint8_t>(value);
        bytes.at(offset + 1) = static_cast<std::uint8_t>(value >> 8);
    }

    void NativeSaveGame::SetDword(std::size_t offset, std::uint32_t value)
    {
        SetWord(offset, static_cast<std::uint16_t>(value));
        SetWord(offset + 2, static_cast<std::uint16_t>(value >> 16));
    }

    void NativeSaveGame::SetChecksum() { SetDword(bytes.size() - 4, Checksum(bytes)); }
    std::string NativeSaveGame::Name() const
    {
        return { reinterpret_cast<const char*>(bytes.data() + 1), bytes.at(0) };
    }
    std::vector<std::uint8_t> NativeSaveGame::LevelOrder() const
    {
        return { bytes.begin() + 24, bytes.begin() + 24 + bytes.at(23) };
    }
    std::int16_t NativeSaveGame::LevelNumber() const { return bytes.at(23 + Word(Ordinal)); }

    NativeSaveGame NativeSaveGame::Decode(std::vector<std::uint8_t> bytes)
    {
        if (bytes.size() < Map + 10 || bytes.size() > MaximumSize)
        {
            throw std::runtime_error("Invalid SPL file size.");
        }
        NativeSaveGame save{ std::move(bytes) };
        if (save.bytes[0] > NameCapacity || (save.SignedWord(21) > 0 && save.Word(21) != 38) ||
            save.bytes[23] == 0 || save.bytes[23] > 62 || save.Word(Ordinal) == 0 ||
            save.Word(Ordinal) > save.bytes[23] || save.Word(243) != 165)
        {
            throw std::runtime_error("Invalid SPL version, name, level order or object count.");
        }
        for (auto level : save.LevelOrder())
        {
            if (level == 0 || level == 96) { throw std::runtime_error("Invalid SPL level index."); }
        }
        const auto width = static_cast<std::size_t>(save.Word(Map)) + 1;
        const auto height = static_cast<std::size_t>(save.Word(Map + 2)) + 1;
        if (width > 200 || height > 200 || save.bytes.size() != Map + 4 + width * height + 5)
        {
            throw std::runtime_error("Invalid SPL collision-map dimensions or length.");
        }
        const auto checksum = static_cast<std::uint32_t>(save.Word(save.bytes.size() - 4)) |
            (static_cast<std::uint32_t>(save.Word(save.bytes.size() - 2)) << 16);
        if (checksum != Checksum(save.bytes)) { throw std::runtime_error("SPL checksum mismatch."); }
        return save;
    }

    void GameContext::LoadGameParts()
    {
        const auto bytes = LoadFile("GRAFIK\\HERZ.256");
        SetGameParts(bytes.data(), bytes.size());
    }

    void GameContext::SetGameParts(const std::uint8_t* bytes, std::size_t size)
    {
        auto parts = DecodeGameParts(bytes, size);
        if (runtimeState == GameRuntimeState::Loading) { throw std::logic_error("Cannot replace parts during loading."); }
        gameParts = std::move(parts);
        chapterFocus = 0;
    }

    const std::vector<GamePart>& GameContext::GetGameParts() const { return gameParts; }
    void GameContext::EnsureGameParts() { if (gameParts.empty()) { LoadGameParts(); } }

    const std::vector<std::uint8_t>& GameContext::CurrentLevelOrder()
    {
        EnsureGameParts();
        if (!loadedLevelNumber) { throw std::logic_error("Level order requires a loaded level."); }
        if (std::find(activeLevelOrder.begin(), activeLevelOrder.end(), *loadedLevelNumber) != activeLevelOrder.end())
        {
            return activeLevelOrder;
        }
        for (const auto& part : gameParts)
        {
            if (std::find(part.levels.begin(), part.levels.end(), *loadedLevelNumber) != part.levels.end())
            {
                activeLevelOrder = part.levels;
                activePartHasCredits = part.endsWithCredits;
                return activeLevelOrder;
            }
        }
        throw std::logic_error("Current level is not in HERZ or the loaded save's level order.");
    }

    std::optional<std::uint8_t> GameContext::NextPlayableLevel(
        const std::vector<std::uint8_t>& order, std::size_t first)
    {
        const auto info = LoadFile("LEVEL\\LEVELS.INF");
        if (info.size() % 124 != 0) { throw std::runtime_error("Invalid LEVELS.INF record length."); }
        for (auto i = first; i < order.size(); ++i)
        {
            const auto offset = static_cast<std::size_t>(order[i]) * 124;
            if (info.size() < offset + 124) { throw std::runtime_error("HERZ references a missing level-info record."); }
            // Presentation selectors are not ported yet; keep their indices in HERZ/SPL, but skip playback.
            if (info[offset + 100] >= 100) { continue; }
            return order[i];
        }
        return std::nullopt;
    }

    const std::vector<std::uint8_t>* GameContext::GetPendingSaveGame() const
    {
        return pendingSaveGame ? &pendingSaveGame->bytes : nullptr;
    }

    void GameContext::LoadGame(std::vector<std::uint8_t> bytes)
    {
        if (pendingLevelLoad) { throw std::logic_error("A level load is already in progress."); }
        auto save = NativeSaveGame::Decode(std::move(bytes));
        const auto number = save.LevelNumber();
        if (number == 97) { throw std::runtime_error("Loading native meteor cutscene saves is unsupported."); }
        pendingSaveGame = std::move(save);
        StartLevel(number);
    }

    void GameContext::RestoreSaveGame()
    {
        const auto& save = *pendingSaveGame;
        const auto oldLife = playerLife, oldOxygen = playerOxygen;
        for (std::size_t i = 0; i < inventory.size(); ++i)
        {
            inventory[i] = DecodeGameObject(save.bytes.data() + NativeSaveGame::Inventory + i * 54, 54);
        }
        playerHeading = playerTargetHeading = save.SignedWord(229);
        playerBodyHeight = save.SignedWord(237);
        playerLifeMaximum = save.SignedWord(NativeSaveGame::Meters + 2);
        playerLife = save.SignedDword(NativeSaveGame::Meters + 4);
        gold = save.SignedWord(NativeSaveGame::Meters + 16);
        playerOxygen = save.SignedDword(NativeSaveGame::Meters + 24);
        playerMapCollisionsEnabled = save.bytes[NativeSaveGame::Meters + 34] != 0;
        playerLifeDeltaSuppressed = save.SignedWord(NativeSaveGame::Meters + 35);
        arrowPoison = save.SignedWord(NativeSaveGame::Meters + 43);
        hasMap = save.bytes[save.bytes.size() - 5] != 0;
        for (std::size_t i = 0; i < weaponAmounts.size(); ++i)
        {
            weaponAmounts[i] = save.SignedWord(NativeSaveGame::Amounts + 2 * i);
            weaponInventorySlots[i] = save.Word(NativeSaveGame::Registry + 2 * i);
        }
        const auto w = NativeSaveGame::Weapon;
        selectedWeapon = static_cast<WeaponType>(save.bytes[w]);
        requestedWeapon = static_cast<WeaponType>(save.bytes[w + 1]);
        weaponSpriteFrame = save.bytes[w + 2];
        weaponHand = save.bytes[w + 3];
        requestedWeaponHand = save.bytes[w + 4];
        weaponAnimationSpeed = save.SignedWord(w + 5);
        weaponAnimationPosition = save.SignedWord(w + 7);
        weaponAnimationRecoil = save.SignedWord(w + 9);
        weaponReady = save.bytes[w + 15] != 0;
        weaponElectricityFiring = false;
        weaponElectricitySparking = false;
        playerHitEffectTicks = 0;
        positionalSoundsSuppressed = false;
        playerRenderTable = save.bytes[NativeSaveGame::RenderTable];
        std::copy_n(save.bytes.data() + NativeSaveGame::Switches, 5, switchGroupMasks);
        activeLevelOrder = save.LevelOrder();
        activePartHasCredits = save.bytes[24 + save.bytes[23]] == 96;
        nativeSavePrefix.assign(save.bytes.begin(), save.bytes.begin() + NativeSaveGame::Geometry);
        if (oldLife != playerLife) { OnPlayerHealthChanged(oldLife, playerLife); }
        if (oldOxygen != playerOxygen) { OnPlayerOxygenChanged(oldOxygen, playerOxygen); }
        paused = false;
        pendingSaveGame.reset();
    }

    std::vector<std::uint8_t> GameContext::CaptureSaveGame(std::string_view name)
    {
        if (IsCutscenePlaying()) { throw std::logic_error("Saving is unavailable during a cutscene."); }
        if (runtimeState != GameRuntimeState::Ready || !CanResumeGame() || !level || !collisionMap)
        {
            throw std::logic_error("Saving requires a ready active game.");
        }
        if (name.empty() || name.size() > NativeSaveGame::NameCapacity || name.find('\0') != std::string_view::npos)
        {
            throw std::invalid_argument("Save names require 1..20 DOS characters.");
        }
        const auto& order = CurrentLevelOrder();
        const auto width = static_cast<std::size_t>(collisionMap->widthMinusOne) + 1;
        const auto height = static_cast<std::size_t>(collisionMap->heightMinusOne) + 1;
        if (width > 200 || height > 200) { throw std::logic_error("Collision map is outside native bounds."); }
        NativeSaveGame save;
        save.bytes.resize(NativeSaveGame::Map + 4 + width * height + 5);
        if (!nativeSavePrefix.empty()) { std::copy(nativeSavePrefix.begin(), nativeSavePrefix.end(), save.bytes.begin()); }
        save.bytes[0] = static_cast<std::uint8_t>(name.size());
        std::copy(name.begin(), name.end(), save.bytes.begin() + 1);
        save.SetWord(21, 38);
        std::fill(save.bytes.begin() + 23, save.bytes.begin() + 87, std::uint8_t{});
        save.bytes[23] = static_cast<std::uint8_t>(order.size());
        std::copy(order.begin(), order.end(), save.bytes.begin() + 24);
        if (activePartHasCredits) { save.bytes[24 + order.size()] = 96; }
        std::copy(level->nativeInfo.begin(), level->nativeInfo.end(), save.bytes.begin() + NativeSaveGame::Info);
        const auto ordinal = std::find(order.begin(), order.end(), *loadedLevelNumber) - order.begin() + 1;
        save.SetWord(NativeSaveGame::Ordinal, static_cast<std::uint16_t>(ordinal));
        save.SetWord(229, static_cast<std::uint16_t>(playerHeading));
        const auto camera = GetPlayerCamera();
        for (std::size_t i = 0; i < 3; ++i) { save.SetWord(231 + i * 2, static_cast<std::uint16_t>(camera.position[i])); }
        save.SetWord(237, static_cast<std::uint16_t>(playerBodyHeight));
        save.SetWord(243, 165);
        for (std::size_t i = 1; i < worldObjects.size(); ++i)
        {
            EncodeGameObject(worldObjects[i], save.bytes.data() + NativeSaveGame::Player + (i - 1) * 54, 54);
        }
        for (std::size_t i = 0; i < inventory.size(); ++i)
        {
            EncodeGameObject(inventory[i], save.bytes.data() + NativeSaveGame::Inventory + i * 54, 54);
        }
        save.SetWord(NativeSaveGame::Meters, 1);
        save.SetWord(NativeSaveGame::Meters + 2, static_cast<std::uint16_t>(playerLifeMaximum));
        save.SetDword(NativeSaveGame::Meters + 4, static_cast<std::uint32_t>(playerLife));
        save.SetWord(NativeSaveGame::Meters + 16, static_cast<std::uint16_t>(gold));
        save.SetWord(NativeSaveGame::Meters + 20, 200);
        save.SetWord(NativeSaveGame::Meters + 22, 360);
        save.SetDword(NativeSaveGame::Meters + 24, static_cast<std::uint32_t>(playerOxygen));
        save.bytes[NativeSaveGame::Meters + 34] = playerMapCollisionsEnabled ? 1 : 0;
        save.SetWord(NativeSaveGame::Meters + 35, static_cast<std::uint16_t>(playerLifeDeltaSuppressed));
        save.SetWord(NativeSaveGame::Meters + 43, static_cast<std::uint16_t>(arrowPoison));
        if (nativeSavePrefix.empty())
        {
            save.SetWord(NativeSaveGame::Meters + 8, static_cast<std::uint16_t>(
                static_cast<std::uint32_t>(playerLife) >> 16));
            save.SetWord(NativeSaveGame::Meters + 18, static_cast<std::uint16_t>(gold));
            save.SetWord(NativeSaveGame::Meters + 28, static_cast<std::uint16_t>(
                static_cast<std::uint32_t>(playerOxygen) >> 16));
            for (std::size_t i = 0; i < gameParts.size(); ++i)
            {
                if (gameParts[i].levels == order) { save.SetWord(NativeSaveGame::Meters + 47, static_cast<std::uint16_t>(31 + i)); }
            }
        }
        for (std::size_t i = 0; i < weaponAmounts.size(); ++i)
        {
            save.SetWord(NativeSaveGame::Amounts + i * 2, static_cast<std::uint16_t>(weaponAmounts[i]));
            save.SetWord(NativeSaveGame::Registry + i * 2, weaponInventorySlots[i]);
        }
        const auto w = NativeSaveGame::Weapon;
        save.bytes[w] = static_cast<std::uint8_t>(selectedWeapon);
        save.bytes[w + 1] = static_cast<std::uint8_t>(requestedWeapon);
        save.bytes[w + 2] = weaponSpriteFrame;
        save.bytes[w + 3] = weaponHand;
        save.bytes[w + 4] = requestedWeaponHand;
        save.SetWord(w + 5, static_cast<std::uint16_t>(weaponAnimationSpeed));
        save.SetWord(w + 7, static_cast<std::uint16_t>(weaponAnimationPosition));
        save.SetWord(w + 9, static_cast<std::uint16_t>(weaponAnimationRecoil));
        save.bytes[w + 15] = weaponReady ? 1 : 0;
        std::copy_n(switchGroupMasks, 5, save.bytes.data() + NativeSaveGame::Switches);
        save.bytes[NativeSaveGame::RenderTable] = playerRenderTable;
        const std::uint8_t* planes[]{ &level->wallTextures[0][0], &level->floorObjects[0][0],
            &level->ceilingObjects[0][0], &level->floorHeights[0][0], &level->roomHeights[0][0],
            &level->surfaceFlags[0][0], &level->collisionMaterials[0][0] };
        for (std::size_t i = 0; i < 7; ++i) { std::copy_n(planes[i], 2000, save.bytes.data() + NativeSaveGame::Geometry + i * 2000); }
        save.SetWord(NativeSaveGame::Map, collisionMap->widthMinusOne);
        save.SetWord(NativeSaveGame::Map + 2, collisionMap->heightMinusOne);
        std::copy_n(collisionMap->data, width * height, save.bytes.data() + NativeSaveGame::Map + 4);
        save.bytes[save.bytes.size() - 5] = hasMap ? 1 : 0;
        save.SetChecksum();
        return std::move(save.bytes);
    }

    std::optional<std::vector<std::uint8_t>> GameContext::ReadSaveFile(std::uint8_t)
    {
        throw std::runtime_error("The host must implement save-file reading.");
    }
    void GameContext::WriteSaveFile(std::uint8_t, const std::vector<std::uint8_t>&)
    {
        throw std::runtime_error("The host must implement save-file writing.");
    }

    void GameContext::RefreshSaveSlots()
    {
        std::array<SaveSlot, 10> slots{};
        for (std::size_t i = 0; i < slots.size(); ++i)
        {
            try
            {
                auto bytes = ReadSaveFile(static_cast<std::uint8_t>(i + 1));
                if (bytes)
                {
                    slots[i].occupied = true;
                    slots[i].name = NativeSaveGame::Decode(std::move(*bytes)).Name();
                }
            }
            catch (const std::runtime_error& error) { slots[i].error = error.what(); }
        }
        saveSlots = std::move(slots);
    }

    void GameContext::QueueTextInput(std::string_view text)
    {
        if (menuPage != MenuPage::SaveGame || !editingSaveName) { return; }
        for (const unsigned char c : text)
        {
            if (c == '\b' || c >= 32) { queuedText.push_back(static_cast<char>(c)); }
        }
    }
}
