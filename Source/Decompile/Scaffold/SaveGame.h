#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Desperabis
{
    struct GamePart
    {
        std::string name;
        std::vector<std::uint8_t> levels;
        bool endsWithCredits = false;
    };

    std::vector<GamePart> DecodeGameParts(const std::uint8_t* bytes, std::size_t size);

    struct NativeSaveGame
    {
        static constexpr std::size_t NameCapacity = 20;
        static constexpr std::size_t Info = 103, Ordinal = 227, Player = 245;
        static constexpr std::size_t Inventory = 9155, Meters = 9965;
        static constexpr std::size_t Amounts = 10014, Registry = 10078, Weapon = 10142;
        static constexpr std::size_t Switches = 10158, RenderTable = 10163;
        static constexpr std::size_t Geometry = 10164, Map = Geometry + 14000;
        static constexpr std::size_t MaximumSize = Map + 40004 + 5;

        std::vector<std::uint8_t> bytes;
        static NativeSaveGame Decode(std::vector<std::uint8_t> bytes);
        std::uint16_t Word(std::size_t offset) const;
        std::int16_t SignedWord(std::size_t offset) const;
        std::int32_t SignedDword(std::size_t offset) const;
        void SetWord(std::size_t offset, std::uint16_t value);
        void SetDword(std::size_t offset, std::uint32_t value);
        void SetChecksum();
        std::string Name() const;
        std::vector<std::uint8_t> LevelOrder() const;
        std::int16_t LevelNumber() const;
    };
}
