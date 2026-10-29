#pragma once

#include "GameRuntime.h"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

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

    struct ApplicationUpdateResult
    {
        std::optional<GameTickResult> tick;
        GameTickTiming timing{ 0, 0 };
        double worldDeltaSeconds = 0;
    };

    struct UiRectangle
    {
        float x, y, width, height;
    };

    struct UiColor
    {
        std::uint8_t r = 255, g = 255, b = 255, a = 255;
    };

    struct UiFill
    {
        UiRectangle bounds;
        UiColor color;
    };

    enum class UiTextAlignment { Left, Center, Right };
    enum class UiTextLayout { TopAligned, Native };

    struct UiText
    {
        float x, y;
        std::string text;
        float scale = 1;
        UiColor color;
        UiTextAlignment alignment = UiTextAlignment::Left;
        UiTextLayout layout = UiTextLayout::TopAligned;
    };

    enum class UiBitmapSource { Hud, LevelSprite, ModelTexture, InventoryIcons };
    enum class UiBitmapLayout { Fit, TopLeft, SpriteAnchor, Center };

    struct UiBitmap
    {
        UiRectangle bounds;
        UiBitmapSource source;
        // Animation frames are zero-based; model textures retain their native slot index.
        std::uint8_t assetSlot, animation, frame;
        UiColor color;
        UiBitmapLayout layout = UiBitmapLayout::Fit;
        bool flipHorizontal = false;
        std::uint8_t shade = 0;
    };

    struct UiHealthMeter
    {
        float x, y;
        std::int16_t filledColumns;
    };

    struct UiModel
    {
        UiRectangle bounds;
        std::uint8_t modelId, animation, frame, blendFrame;
        std::int16_t blendWeight;
        DisplayType displayType;
        std::array<std::int16_t, 3> angles;
        std::int16_t depth;
        float focalLength = 128;
        UiColor color;
    };

    using UiDrawCommand = std::variant<UiFill, UiText, UiBitmap, UiHealthMeter, UiModel>;

    struct ApplicationPresentation
    {
        static constexpr float Width = 360, Height = 240;
        MenuPage page;
        std::vector<UiDrawCommand> commands;
    };
}
