#include "GameContext.h"
#include "GameMath.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>

namespace Desperabis
{
    namespace
    {
        std::int16_t AbsoluteWord(std::int16_t value)
        {
            return value < 0 ? WrapSignedWord(-static_cast<std::int32_t>(value)) : value;
        }

        struct HeightPosition
        {
            std::size_t column;
            std::size_t row;
            std::uint16_t fractionX;
            std::uint16_t fractionZ;
        };

        HeightPosition GetHeightPosition(std::int16_t x, std::int16_t z)
        {
            const int column = x / 100;
            const int zTile = z / 100;
            if (column < -128 || column > 127 || zTile < -128 || zTile > 127)
            {
                throw std::overflow_error("Height lookup would overflow the DOS signed-byte division.");
            }
            const int row = 41 - zTile;
            if (column < 1 || column > static_cast<int>(Level::Width) ||
                row < 1 || row > static_cast<int>(Level::Height))
            {
                throw std::out_of_range("Height lookup is outside the 50x40 level.");
            }
            // The original arrays are one-based; their instruction displacements are biased by 51.
            return { static_cast<std::size_t>(column - 1), static_cast<std::size_t>(row - 1),
                static_cast<std::uint16_t>((x % 100) * 661),
                static_cast<std::uint16_t>((z % 100) * 661) };
        }

        void CheckHeightCorners(const HeightPosition& position)
        {
            if (position.column + 1 >= Level::Width || position.row + 1 >= Level::Height)
            {
                throw std::out_of_range("Height interpolation requires four samples inside the level.");
            }
        }

        std::uint16_t Interpolate(std::uint16_t first, std::uint16_t second, std::uint16_t fraction)
        {
            const auto sum = static_cast<std::uint32_t>(first) * (65535u - fraction) +
                static_cast<std::uint32_t>(second) * fraction;
            return static_cast<std::uint16_t>(sum >> 16);
        }

        std::uint16_t InterpolateHeight(const std::array<std::int16_t, 4>& corners,
            const HeightPosition& position)
        {
            const auto upper = Interpolate(static_cast<std::uint16_t>(corners[0]),
                static_cast<std::uint16_t>(corners[1]), position.fractionX);
            const auto lower = Interpolate(static_cast<std::uint16_t>(corners[2]),
                static_cast<std::uint16_t>(corners[3]), position.fractionX);
            return Interpolate(lower, upper, position.fractionZ);
        }

        std::array<std::int16_t, 4> GetCorners(
            const std::uint8_t (&plane)[Level::Height][Level::Width], const HeightPosition& position)
        {
            CheckHeightCorners(position);
            const auto row = position.row;
            const auto column = position.column;
            return { plane[row][column], plane[row][column + 1],
                plane[row + 1][column], plane[row + 1][column + 1] };
        }

        std::int16_t RepeatFloor(std::int16_t corner, std::int16_t queryHeight, std::int16_t step)
        {
            corner = WrapSignedWord(corner % step - step);
            const auto threshold = WrapSignedWord(queryHeight - 28);
            for (unsigned int iterations = 0; threshold > corner; ++iterations)
            {
                if (iterations == 65536u)
                {
                    throw std::runtime_error("Repeated-floor lookup would loop forever in the DOS game.");
                }
                corner = WrapSignedWord(corner + step);
            }
            return std::clamp<std::int16_t>(corner, 0, 255);
        }

        bool AxisOverlaps(std::int16_t first, std::int16_t second,
            std::int16_t firstRadius, std::int16_t secondRadius)
        {
            const auto difference = WrapSignedWord(first - second);
            const auto distance = difference < 0 ? WrapSignedWord(-difference) : difference;
            return distance < WrapSignedWord(firstRadius + secondRadius);
        }

        bool ObjectsOverlap(const RawGameObject& source, const RawGameObject& candidate)
        {
            return (source.field_07 & candidate.field_07) != 0 &&
                AxisOverlaps(source.posX, candidate.posX, source.collisionRadiusXZ, candidate.collisionRadiusXZ) &&
                AxisOverlaps(source.posY, candidate.posY, source.collisionRadiusY, candidate.collisionRadiusY) &&
                AxisOverlaps(source.posZ, candidate.posZ, source.collisionRadiusXZ, candidate.collisionRadiusXZ);
        }
    }

    GameContext::~GameContext() = default;

    MenuPage GameContext::GetMenuPage() const
    {
        return menuPage;
    }

    void GameContext::SetMenuPage(MenuPage page)
    {
        menuPage = page;
    }

    bool GameContext::CanResumeGame() const
    {
        return hasActiveGame && !positionalSoundsSuppressed;
    }

    bool GameContext::SelectMainMenuAction(MainMenuAction action)
    {
        if (menuPage != MenuPage::MainMenu && menuPage != MenuPage::AlternateMainMenu)
        {
            throw std::logic_error("Main-menu choices require an active main menu.");
        }
        switch (action)
        {
        case MainMenuAction::Cancel:
            if (positionalSoundsSuppressed) { return false; }
            menuPage = MenuPage::Gameplay;
            break;
        case MainMenuAction::ResumeGame:
            if (!CanResumeGame()) { return false; }
            menuPage = MenuPage::Gameplay;
            break;
        case MainMenuAction::NewGame: menuPage = MenuPage::ChapterSelection; break;
        case MainMenuAction::LoadGame: menuPage = MenuPage::LoadGame; break;
        case MainMenuAction::SaveGame:
            if (!CanResumeGame()) { return false; }
            menuPage = MenuPage::SaveGame;
            break;
        case MainMenuAction::Settings: menuPage = MenuPage::Settings; break;
        case MainMenuAction::Configuration: menuPage = MenuPage::Configuration; break;
        case MainMenuAction::Quit: menuPage = MenuPage::Quit; break;
        default: throw std::invalid_argument("Unknown main-menu action.");
        }
        return true;
    }

    bool GameContext::OpenInventory()
    {
        if (menuPage != MenuPage::Gameplay)
        {
            throw std::logic_error("Inventory can only be opened from gameplay.");
        }
        if (positionalSoundsSuppressed) { return false; }
        menuPage = MenuPage::Inventory;
        return true;
    }

    void GameContext::StartNewGame(std::int16_t chapter)
    {
        if (chapter <= 0)
        {
            throw std::invalid_argument("A new game requires a positive chapter number.");
        }
        InitializeNewGame(chapter);
        hasActiveGame = true;
        menuPage = MenuPage::Gameplay;
    }

    void GameContext::RunApplication()
    {
        while (menuPage != MenuPage::Quit)
        {
            RunApplicationStep();
        }
    }

    void GameContext::RunApplicationStep()
    {
        switch (menuPage)
        {
        case MenuPage::Gameplay: menuPage = RunGameplay(); break;
        case MenuPage::MainMenu:
        case MenuPage::AlternateMainMenu:
            SelectMainMenuAction(ShowMainMenu());
            break;
        case MenuPage::Settings: menuPage = ShowSettings(); break;
        case MenuPage::Configuration: menuPage = ShowConfiguration(); break;
        case MenuPage::ChapterSelection:
        {
            const auto chapter = ShowChapterSelection();
            if (chapter) { StartNewGame(*chapter); }
            else { menuPage = MenuPage::MainMenu; }
            break;
        }
        case MenuPage::LoadGame: menuPage = ShowLoadGame(); break;
        case MenuPage::SaveGame:
            // The native dispatcher rechecks this shared gate even when a page is selected directly.
            menuPage = positionalSoundsSuppressed ? MenuPage::MainMenu : ShowSaveGame();
            break;
        case MenuPage::Inventory: menuPage = ShowInventory(); break;
        case MenuPage::ReturnToGameplay: menuPage = MenuPage::Gameplay; break;
        case MenuPage::Presentation20:
            menuPage = ShowUnrecoveredPage(menuPage);
            positionalSoundsSuppressed = true;
            break;
        case MenuPage::Presentation26:
        case MenuPage::Presentation32:
            ShowUnrecoveredPage(menuPage);
            menuPage = MenuPage::Gameplay;
            break;
        case MenuPage::Presentation31:
        case MenuPage::Presentation33:
        case MenuPage::Dialog101:
            ShowUnrecoveredPage(menuPage);
            menuPage = MenuPage::MainMenu;
            break;
        case MenuPage::UnidentifiedPage5:
        case MenuPage::UnidentifiedPage8:
        case MenuPage::UnidentifiedPage9:
        case MenuPage::UnidentifiedPage11:
        case MenuPage::Presentation22:
        case MenuPage::Presentation23:
        case MenuPage::Presentation25:
        case MenuPage::Dialog100:
            menuPage = ShowUnrecoveredPage(menuPage);
            break;
        case MenuPage::Quit: break;
        default:
        {
            const auto value = static_cast<std::uint16_t>(menuPage);
            if (value >= 1 && value <= 20)
            {
                // Unassigned states in this native range return to gameplay.
                menuPage = MenuPage::Gameplay;
            }
            else if (value >= 200 && value <= 299)
            {
                menuPage = ShowUnrecoveredPage(menuPage);
            }
            else
            {
                // The original dispatcher's default branch requests application shutdown.
                menuPage = MenuPage::Quit;
            }
            break;
        }
        }
    }

    MainMenuAction GameContext::ShowMainMenu()
    {
        throw std::logic_error("Main-menu input and drawing are not implemented.");
    }

    std::optional<std::int16_t> GameContext::ShowChapterSelection()
    {
        throw std::logic_error("Chapter-selection input and drawing are not implemented.");
    }

    MenuPage GameContext::RunGameplay()
    {
        throw std::logic_error("The complete gameplay frame loop is not implemented.");
    }

    MenuPage GameContext::ShowSettings()
    {
        throw std::logic_error("The settings page is not implemented.");
    }

    MenuPage GameContext::ShowConfiguration()
    {
        throw std::logic_error("The configuration page is not implemented.");
    }

    MenuPage GameContext::ShowLoadGame()
    {
        throw std::logic_error("The load-game page and loading are not implemented.");
    }

    MenuPage GameContext::ShowSaveGame()
    {
        throw std::logic_error("The save-game page and saving are not implemented.");
    }

    MenuPage GameContext::ShowInventory()
    {
        throw std::logic_error("The inventory page loop is not implemented.");
    }

    MenuPage GameContext::ShowUnrecoveredPage(MenuPage page)
    {
        throw std::logic_error("Application page " +
            std::to_string(static_cast<std::uint16_t>(page)) + " is not implemented.");
    }

    void GameContext::InitializeNewGame(std::int16_t)
    {
        throw std::logic_error("New-game resource loading and initialization are not implemented.");
    }

    std::int16_t GameContext::RandomSigned(std::uint16_t radius)
    {
        if (radius > 32767)
        {
            throw std::invalid_argument("Signed random radius must fit a nonnegative signed word.");
        }
        const auto value = static_cast<std::int16_t>(
            static_cast<int>(Random(static_cast<std::uint16_t>(radius * 2u + 1u))) - radius);
        return value == 0 ? static_cast<std::int16_t>(radius) : value;
    }

    std::uint16_t GameContext::SpawnFireParticle(const RawGameObject& source)
    {
        const auto x = source.posX;
        const auto y = WrapSignedWord(source.posY + source.collisionRadiusY);
        const auto z = source.posZ;
        const auto slot = FindFreeObjectIndex();
        // Allocation can clear the source itself after its lifetime reaches zero.
        if (source.collisionType == CollisionType::None) { return 0; }
        if (source.collisionType != CollisionType::DamageObj)
        {
            throw std::logic_error("Only the fire branch of the native fragment factory is implemented.");
        }
        auto& particle = worldObjects[slot];
        particle.modelId = 39;
        particle.modelAnim = 3;
        particle.animFrame = 5;
        particle.modelFrame2 = 2;
        particle.blendWeightOrSpriteOpacity = 100;
        particle.renderPlacementMode = 130;
        particle.velocityY = static_cast<std::int16_t>(Random(500) + 600);
        particle.behavior = BehaviorType::FireParticle;
        particle.field_24 = Random(4);
        particle.health = static_cast<std::int16_t>(Random(60) + 50);
        particle.displayType = DisplayType::VectorUnlit;
        const auto acceleration = RandomSigned(30) + particle.velocityY / particle.health;
        // Arithmetic right shift rounds negative odd values down, unlike signed division.
        const auto half = acceleration >= 0 ? acceleration / 2 : -((-acceleration + 1) / 2);
        particle.doorLockOrGroupBitmask = static_cast<std::int16_t>(std::clamp(-half, -30, 1));
        particle.posX = x;
        particle.posY = y;
        particle.posZ = z;
        particle.collisionRadiusXZ = 10;
        particle.collisionRadiusY = 10;
        particle.winX = static_cast<std::int16_t>(Random(512));
        particle.winY = static_cast<std::int16_t>(Random(512));
        return slot;
    }

    void GameContext::InitializeDriftingParticle(RawGameObject& object, std::uint8_t frame,
        std::int16_t x, std::int16_t z)
    {
        object.health = 50;
        object.modelId = 39;
        object.modelAnim = 3;
        object.animFrame = frame;
        object.modelFrame2 = 1;
        object.blendWeightOrSpriteOpacity = 100;
        object.displayType = DisplayType::Type30;
        object.behavior = BehaviorType::ExplosionSmokeCloud;
        object.collisionType = CollisionType::None;
        object.velocityY = 1;
        // The old position chooses the drift direction before X/Z are replaced.
        object.velocityX = SignWord(WrapSignedWord(object.posX - x));
        object.velocityZ = SignWord(WrapSignedWord(object.posZ - z));
        object.posX = x;
        object.posZ = z;
        object.winX = static_cast<std::int16_t>(Random(512));
        object.winY = static_cast<std::int16_t>(Random(512));
    }

    void GameContext::InitializeSmokeParticle(RawGameObject& object, std::int16_t x, std::int16_t y,
        std::int16_t z, std::int16_t radiusXZ, std::int16_t riseFactor, std::uint8_t placement)
    {
        object.modelId = 1;
        object.modelAnim = 2;
        object.animFrame = 3;
        object.behavior = BehaviorType::SmokeFading;
        object.displayType = DisplayType::SpriteTransparent;
        object.blendWeightOrSpriteOpacity = 10;
        object.winY = static_cast<std::int16_t>(Random(2));
        object.collisionRadiusXZ = radiusXZ;
        object.collisionRadiusY = 5;
        object.velocityZ = 2;
        object.health = WrapSignedWord(riseFactor * 50 + Random(50));
        const auto size = static_cast<std::uint8_t>(Random(
            static_cast<std::uint16_t>(object.collisionRadiusXZ)) + 10);
        // Only the scale's high byte is initialized; its low byte belongs to the existing record.
        object.spriteScale = static_cast<std::uint16_t>((object.spriteScale & 0x00ffu) |
            (static_cast<std::uint16_t>(size) << 8));
        object.velocityY = WrapSignedWord(riseFactor * 200 + Random(130));
        object.posX = x;
        object.posY = y;
        object.posZ = z;
        object.renderPlacementMode = placement;
        const auto floor = GetFloorHeight(object.posX, object.posY, object.posZ);
        if (object.posY < floor) { object.posY = floor; }
    }

    std::uint16_t GameContext::FindFreeObjectIndex()
    {
        std::uint16_t slot = 2;
        while (slot < 165 && worldObjects[slot].health != 0) { ++slot; }
        worldObjects[slot] = {};
        return slot;
    }

    std::uint16_t GameContext::FindFreeOrLowestHealthObjectIndex()
    {
        std::uint16_t lowestSlot = 165;
        std::int16_t lowestHealth = 1000;
        std::uint16_t slot = 2;
        for (; slot <= 164; ++slot)
        {
            const auto health = worldObjects[slot].health;
            if (health < lowestHealth)
            {
                lowestHealth = health;
                lowestSlot = slot;
            }
            if (health == 0) { break; }
        }
        if (slot > 164) { slot = lowestSlot; }
        worldObjects[slot] = {};
        return slot;
    }

    std::uint8_t GameContext::GetCollisionMapPixelAtWorldXZ(std::int16_t x, std::int16_t z) const
    {
        if (!collisionMap)
        {
            throw std::runtime_error("GetCollisionMapPixelAtWorldXZ requires collision-map memory (DS:69C4).");
        }
        const auto column = WrapSignedWord(static_cast<std::int32_t>(x) - 112) / 25;
        const auto row = WrapSignedWord(162 - WrapSignedWord(static_cast<std::int32_t>(z) - 38) / 25);
        if (column < 0 || column > 319 || row < 0 || row > 239)
        {
            return 0;
        }
        const auto& map = *collisionMap;
        const auto stride = static_cast<std::uint16_t>(map.widthMinusOne + 1u);
        const auto address = static_cast<std::uint16_t>(offsetof(CollisionMap, data) + row * stride + column);
        if (address >= sizeof(CollisionMap))
        {
            throw std::out_of_range("Collision-map read is outside the allocated dummy map.");
        }
        // Wrapped offsets can address the header as well as pixel data.
        const auto* bytes = reinterpret_cast<const unsigned char*>(&map);
        return bytes[address];
    }

    void GameContext::SetCollisionMapPixelAtWorldXZ(
        std::int16_t x, std::int16_t z, std::uint16_t value)
    {
        if (!collisionMap)
        {
            throw std::runtime_error("SetCollisionMapPixelAtWorldXZ requires collision-map memory (DS:69C4).");
        }
        auto& map = *collisionMap;
        const auto stride = static_cast<std::uint16_t>(map.widthMinusOne + 1u);
        const auto column = WrapSignedWord(static_cast<std::int32_t>(x) - 112) / 25;
        const auto row = WrapSignedWord(162 - WrapSignedWord(static_cast<std::int32_t>(z) - 38) / 25);
        const auto index = static_cast<std::uint16_t>(row * stride + column);
        constexpr auto dataOffset = offsetof(CollisionMap, data);
        const auto address = static_cast<std::uint16_t>(dataOffset + index);
        if (address >= sizeof(CollisionMap))
        {
            throw std::out_of_range("Collision-map write is outside the allocated dummy map.");
        }
        if (address >= dataOffset)
        {
            map.data[address - dataOffset] = static_cast<std::uint8_t>(value);
        }
        else
        {
            // Normalize the far pointer to offset zero; wrapped writes can still hit the header.
            auto* bytes = reinterpret_cast<unsigned char*>(&map);
            bytes[address] = static_cast<std::uint8_t>(value);
        }
    }

    void GameContext::SetDoorCollisionMapAtWorldXZ(
        std::int16_t x, std::int16_t z, std::uint16_t value)
    {
        if (!collisionMap)
        {
            throw std::runtime_error("Door collision updates require collision-map memory.");
        }
        auto& map = *collisionMap;
        auto* bytes = reinterpret_cast<unsigned char*>(&map);
        const int column = WrapSignedWord(static_cast<std::int32_t>(x) - 137) / 25;
        const int centerRow = 162 - WrapSignedWord(static_cast<std::int32_t>(z) - 38) / 25;
        for (int offset = -1; offset <= 1; ++offset)
        {
            // A wrapped write can touch the header, changing the stride for the next row.
            const auto stride = static_cast<std::uint16_t>(map.widthMinusOne + 1u);
            const auto row = WrapSignedWord(centerRow + offset);
            std::array<std::uint16_t, 3> addresses{};
            for (int pixel = 0; pixel < 3; ++pixel)
            {
                addresses[pixel] = static_cast<std::uint16_t>(
                    offsetof(CollisionMap, data) + row * stride + column + pixel);
                if (addresses[pixel] >= sizeof(CollisionMap))
                {
                    throw std::out_of_range("Door collision write is outside the allocated map.");
                }
            }
            for (const auto address : addresses) { bytes[address] = static_cast<std::uint8_t>(value); }
        }
    }

    std::int16_t GameContext::GetHeightSurfaceDepth(std::uint16_t surface)
    {
        surface &= 31;
        floorSurfaceValue = -1;
        std::int16_t depth = 1000;
        if (surface >= 1 && surface <= 4)
        {
            if (!level) { throw std::runtime_error("Surface depth requires level information."); }
            const auto& entry = level->heightSurfaces[surface - 1];
            if (!entry)
            {
                throw std::runtime_error("Floor surface slot requires height/value metadata from level information.");
            }
            depth = entry->depth;
            floorSurfaceValue = entry->value;
        }
        return depth;
    }

    std::int16_t GameContext::GetFloorHeight(std::int16_t x, std::int16_t y, std::int16_t z)
    {
        if (!level) { throw std::runtime_error("GetFloorHeight requires level geometry."); }
        const auto position = GetHeightPosition(x, z);
        const auto depth = GetHeightSurfaceDepth(level->surfaceFlags[position.row][position.column]);
        const auto flatFloor = WrapSignedWord(-static_cast<std::int32_t>(depth));
        if (flatFloor >= -900) { return flatFloor; }
        return GetTerrainFloorHeight(x, y, z);
    }

    std::int16_t GameContext::GetTerrainFloorHeight(std::int16_t x, std::int16_t y, std::int16_t z) const
    {
        if (!level) { throw std::runtime_error("Terrain floor lookup requires level geometry."); }
        const auto position = GetHeightPosition(x, z);
        const auto surface = level->surfaceFlags[position.row][position.column] & 31;
        auto corners = GetCorners(level->floorHeights, position);
        const auto halfY = y >= 0 ? y / 2 : (static_cast<int>(y) - 1) / 2;
        const auto queryHeight = WrapSignedWord(-halfY);
        constexpr int stackedFloorSurface = 28;
        if (surface == stackedFloorSurface)
        {
            const auto room = GetCorners(level->roomHeights, position);
            for (std::size_t i = 0; i < corners.size(); ++i)
            {
                corners[i] = WrapSignedWord(corners[i] - room[i]);
            }
            if (WrapSignedWord(*std::max_element(corners.begin(), corners.end()) - 5) < queryHeight)
            {
                for (std::size_t i = 0; i < corners.size(); ++i)
                {
                    corners[i] = WrapSignedWord(corners[i] + room[i]);
                }
            }
        }
        if (floorRepeatHeight > 0 && surface != 0)
        {
            for (auto& corner : corners) { corner = RepeatFloor(corner, queryHeight, floorRepeatHeight); }
        }
        return WrapSignedWord(-2 * static_cast<std::int32_t>(InterpolateHeight(corners, position)));
    }

    std::int16_t GameContext::GetRoomHeight(std::int16_t x, std::int16_t z) const
    {
        if (!level) { throw std::runtime_error("GetRoomHeight requires level geometry."); }
        const auto position = GetHeightPosition(x, z);
        if (level->ceilingObjects[position.row][position.column] == 0) { return 20000; }
        const auto corners = GetCorners(level->roomHeights, position);
        return WrapSignedWord(2 * static_cast<std::int32_t>(InterpolateHeight(corners, position)));
    }

    std::uint16_t GameContext::FindCollidingObject(std::int16_t sourceSlot) const
    {
        if (!objectCollisionsEnabled)
        {
            return 0;
        }
        if (sourceSlot < 0 || static_cast<std::size_t>(sourceSlot) >= worldObjects.size())
        {
            throw std::out_of_range("Collision source slot must be in 0..165.");
        }
        const auto& source = worldObjects[sourceSlot];
        return FindCollidingObject(source);
    }

    std::uint16_t GameContext::FindCollidingObject(const RawGameObject& source) const
    {
        if (!objectCollisionsEnabled) { return 0; }
        for (std::uint16_t slot = 2; slot <= 164; ++slot)
        {
            const auto& candidate = worldObjects[slot];
            if (&candidate != &source && candidate.health > 0 && ObjectsOverlap(source, candidate))
            {
                return slot;
            }
        }
        return 0;
    }

    bool GameContext::OverlapsPlayer(const RawGameObject& source) const
    {
        return objectCollisionsEnabled && ObjectsOverlap(source, worldObjects[1]);
    }

    std::uint16_t GameContext::FindPlayerCollision() const
    {
        const auto first = FindCollidingObject(1);
        if (first == 0)
        {
            return 0;
        }
        auto slot = first;
        do
        {
            ++slot;
            const auto& candidate = worldObjects[slot];
            if (candidate.health > 999 && candidate.field_32 != 0 &&
                ObjectsOverlap(worldObjects[1], candidate))
            {
                return slot;
            }
        } while (slot <= 163);
        return first;
    }

    std::uint16_t GameContext::Random(std::uint16_t range)
    {
        if (range == 0) { throw std::invalid_argument("Random requires a nonzero range."); }
        // 2CE4:170A uses the high seed word modulo the range, not multiply-high scaling.
        randomSeed = randomSeed * 0x08088405u + 1u;
        return static_cast<std::uint16_t>((randomSeed >> 16) % range);
    }

    void GameContext::LoadSoundEffects(const std::uint8_t* data, std::size_t size)
    {
        if (!data || size != SoundEffectTableSize)
        {
            throw std::invalid_argument("S_TAB.PRB must contain its four-byte header and 129 five-byte records.");
        }
        if (data[0] != SoundEffectCount || data[1] != 0 || data[2] != 5 || data[3] != 0)
        {
            throw std::invalid_argument("Unsupported sound-effect table dimensions.");
        }
        for (std::size_t i = 0; i < SoundEffectCount; ++i)
        {
            if (data[4 + i * 5] > 4)
            {
                throw std::invalid_argument("Sound effects support at most four sample references.");
            }
        }
        std::memcpy(soundEffects.data(), data + 4, SoundEffectCount * sizeof(SoundEffectDefinition));
        soundEffectsLoaded = true;
    }

    std::int16_t GameContext::GetSoundDistanceAt(std::int16_t x, std::int16_t z) const
    {
        return positionalSoundsSuppressed ? static_cast<std::int16_t>(10000) :
            WrapSignedWord(AbsoluteWord(WrapSignedWord(soundListenerX - x)) +
                AbsoluteWord(WrapSignedWord(soundListenerZ - z)));
    }

    std::uint8_t GameContext::GetSoundVolumeAt(
        std::int16_t x, std::int16_t z, std::int16_t distanceScale) const
    {
        const auto distance = GetSoundDistanceAt(x, z);
        const auto scaled = WrapSignedWord(static_cast<std::int32_t>(distance) * distanceScale);
        const auto magnitude = static_cast<std::uint16_t>(AbsoluteWord(scaled));
        return static_cast<std::uint8_t>(std::clamp(355 - static_cast<int>(magnitude >> 1), 0, 255));
    }

    SoundPlaybackStatus GameContext::PlaySoundEffect(
        SoundEffect effect, std::uint8_t volume, std::int16_t sourceId, std::int16_t instanceLimit)
    {
        const auto effectId = static_cast<std::uint16_t>(effect);
        if (effectId < 1 || effectId > SoundEffectCount)
        {
            throw std::out_of_range("Sound-effect ID must be in the native range 1..129.");
        }
        if (!soundEffectsLoaded)
        {
            if (!audioHandler) { return SoundPlaybackStatus::AudioUnavailable; }
            throw std::runtime_error("PlaySoundEffect requires the S_TAB.PRB effect table.");
        }
        const auto& definition = soundEffects[effectId - 1];
        if (definition.sampleCount == 0) { return SoundPlaybackStatus::NoSample; }
        if (definition.sampleCount > definition.samples.size())
        {
            throw std::invalid_argument("Sound-effect sample count exceeds its four-reference record.");
        }
        const auto sample = definition.samples[Random(definition.sampleCount)];
        if (!audioHandler) { return SoundPlaybackStatus::AudioUnavailable; }
        if (sample == 0 || sample > audioHandler->GetSampleCount()) { return SoundPlaybackStatus::NoSample; }
        if (volume == 0) { return SoundPlaybackStatus::Inaudible; }
        if (!soundEnabled) { return SoundPlaybackStatus::Disabled; }

        std::size_t freeChannel = SoundChannelCount;
        int freeCount = 0;
        int sameSourceCount = 0;
        for (std::size_t channel = 0; channel < SoundChannelCount; ++channel)
        {
            if (!audioHandler->IsChannelPlaying(channel))
            {
                freeChannel = channel;
                ++freeCount;
            }
            else if (soundChannelSources[channel] == sourceId)
            {
                ++sameSourceCount;
            }
        }
        if (instanceLimit < 0 && freeCount < 2) { return SoundPlaybackStatus::ReservedChannel; }
        if (freeCount == 0) { return SoundPlaybackStatus::NoFreeChannel; }
        if (AbsoluteWord(instanceLimit) <= sameSourceCount) { return SoundPlaybackStatus::SourceLimitReached; }
        audioHandler->PlaySample(freeChannel, sample, volume);
        soundChannelSources[freeChannel] = sourceId;
        return SoundPlaybackStatus::Started;
    }

    SoundPlaybackStatus GameContext::PlaySoundEffect(
        SoundEffect effect, std::uint8_t volume, const RawGameObject& source, std::int16_t instanceLimit)
    {
        for (std::size_t slot = 0; slot < worldObjects.size(); ++slot)
        {
            if (&source == &worldObjects[slot])
            {
                return PlaySoundEffect(effect, volume, static_cast<std::int16_t>(slot), instanceLimit);
            }
        }
        if (!audioHandler) { return PlaySoundEffect(effect, volume, 0, instanceLimit); }
        throw std::invalid_argument("Object sound sources must belong to GameContext::worldObjects.");
    }

    void GameContext::PromptUserMessage(std::string_view text, std::int16_t duration)
    {
        auto length = std::min<std::size_t>(text.size(), 150);
        // Preserve the original byte limit without cutting a UTF-8 character in half.
        while (length > 0 && length < text.size() &&
            (static_cast<unsigned char>(text[length]) & 0xc0u) == 0x80u)
        {
            --length;
        }
        userMessage = text.substr(0, length);
        userMessageTicks = text.empty() ? 0 :
            duration < 0 ? WrapSignedWord(-static_cast<std::int32_t>(duration)) : duration;
    }

    void GameContext::UpdateUserMessage(std::int16_t ticks)
    {
        if (ticks < 0) { throw std::invalid_argument("Message updates require nonnegative ticks."); }
        if (userMessageTicks > 0) { userMessageTicks = WrapSignedWord(userMessageTicks - ticks); }
    }
}
