#pragma once

#include <cstdint>

namespace Desperabis
{
    // Original application-state values; unidentified pages retain their numeric identities.
    enum class MenuPage : std::uint16_t
    {
        Gameplay = 0,
        MainMenu = 1,
        Settings = 2,
        Configuration = 3,
        ChapterSelection = 4,
        UnidentifiedPage5 = 5,
        LoadGame = 6,
        SaveGame = 7,
        UnidentifiedPage8 = 8,
        UnidentifiedPage9 = 9,
        Inventory = 10,
        UnidentifiedPage11 = 11,
        Presentation20 = 20,
        AlternateMainMenu = 21,
        Presentation22 = 22,
        Presentation23 = 23,
        Presentation25 = 25,
        Presentation26 = 26,
        Presentation31 = 31,
        Presentation32 = 32,
        Presentation33 = 33,
        ReturnToGameplay = 99,
        Dialog100 = 100,
        Dialog101 = 101,
        ResourcePresentationFirst = 200,
        ResourcePresentationLast = 299,
        Quit = 0xffff
    };

    enum class MainMenuAction : std::int16_t
    {
        Cancel = -2, // Escape command, not a numbered menu entry.
        ResumeGame = -1,
        NewGame = 0,
        LoadGame = 1,
        SaveGame = 2,
        Settings = 3,
        Configuration = 4,
        Quit = 5
    };
}
