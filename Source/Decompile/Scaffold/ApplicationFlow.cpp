#include "GameContext.h"
#include "BehaviorDispatch.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace Desperabis
{
    namespace
    {
        constexpr double TicksPerSecond = 33;
        constexpr std::array<std::uint8_t, 16> InventoryPayloads{
            14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 100, 101, 1, 2 };
        // Resource numbering follows the existing playable/menu order, not simple addition.
        constexpr std::array<std::int16_t, 26> PlayableLevels{
            50, 30, 31, 32, 33, 36, 34, 35,
            10, 11, 12, 13, 14, 15, 16, 17, 18, 90, 19, 20, 21, 22, 23, 24, 25, 26 };

        bool IsGamePartStart(std::int16_t level) { return level == 50 || level == 30 || level == 10; }

        class ApplicationInput final : public InputHandler
        {
        public:
            explicit ApplicationInput(std::array<bool, static_cast<std::size_t>(InputKey::Count)> keys)
                : keys(keys) {}
            bool IsKeyPressed(InputKey key) override { return keys.at(static_cast<std::size_t>(key)); }
            std::array<bool, static_cast<std::size_t>(InputKey::Count)> keys;
        };
    }

    void GameContext::PrepareLevelProgression(bool changingLevel)
    {
        for (auto& item : inventory)
        {
            if (item.collisionType == CollisionType::Weapon && item.doorLockOrGroupBitmask > 10)
            {
                item.health = 0;
                selectedWeapon = requestedWeapon = WeaponType::None;
            }
            if (changingLevel && item.collisionType == CollisionType::PickableMap) { item.health = 0; }
        }
        if (changingLevel) { hasMap = false; }
        weaponAnimationSpeed = weaponAnimationPosition = weaponAnimationRecoil = 0;
        weaponReady = true;
        RefreshWeaponInventory();
    }

    void GameContext::FinishGamePart()
    {
        StopAllSoundEffects();
        if (audioHandler && ambientPlaybackActive) { audioHandler->StopAmbientPlayback(); }
        ambientPlaybackActive = false;
        windX = windZ = 0;
        skyEnabled = false;
        ClearLightningFlashes();
        PromptUserMessage({}, 0);
        for (std::size_t slot = 1; slot < worldObjects.size(); ++slot)
        {
            SetObjectHealth(worldObjects[slot], 0);
        }
        ownedLevel.reset();
        level = nullptr;
        collisionMap = nullptr;
        loadedLevelNumber.reset();
        playerCollisionSlot = 0;
        hasActiveGame = false;
        runtimeState = GameRuntimeState::NoLevel;
        paused = false;
        SetMenuPage(MenuPage::MainMenu);
    }

    void GameContext::EndLevel(bool finishGamePart)
    {
        if (runtimeState != GameRuntimeState::Ready || menuPage != MenuPage::Gameplay || !loadedLevelNumber)
        {
            throw std::logic_error("Ending a level requires ready gameplay.");
        }
        const auto current = std::find(PlayableLevels.begin(), PlayableLevels.end(), *loadedLevelNumber);
        if (current == PlayableLevels.end())
        {
            throw std::logic_error("Current level is not in the playable level sequence.");
        }
        const auto next = current + 1;
        const bool completed = finishGamePart || next == PlayableLevels.end() || IsGamePartStart(*next);
        PrepareLevelProgression(!completed);
        if (completed)
        {
            FinishGamePart();
        }
        else { StartLevel(*next); }
    }

    void GameContext::StartNextGamePart()
    {
        if (runtimeState != GameRuntimeState::Ready || menuPage != MenuPage::Gameplay || !loadedLevelNumber)
        {
            throw std::logic_error("Game-part navigation requires ready gameplay.");
        }
        const auto current = std::find(PlayableLevels.begin(), PlayableLevels.end(), *loadedLevelNumber);
        if (current == PlayableLevels.end())
        {
            throw std::logic_error("Current level is not in the playable level sequence.");
        }
        const auto next = std::find_if(current + 1, PlayableLevels.end(), IsGamePartStart);
        if (next == PlayableLevels.end()) { throw std::logic_error("No next game part is available."); }
        PrepareLevelProgression(true);
        StartLevel(*next);
    }

    InteractionResult GameContext::HandleLevelEndContact(RawGameObject& object, bool interactionPressed)
    {
        if (object.collisionType != CollisionType::LevelEnd)
        {
            throw std::invalid_argument("Level-end contact requires collision type 100.");
        }
        if (object.health <= 0) { return { InteractionStatus::InactiveObject }; }
        if (object.groupIndexOrLock == 1000)
        {
            EndLevel(true);
            return { InteractionStatus::Applied };
        }
        if (!interactionPressed)
        {
            if (object.doorLockOrGroupBitmask != 0) { return { InteractionStatus::NoEffect }; }
            EndLevel();
            return { InteractionStatus::Applied };
        }
        if (object.doorLockOrGroupBitmask > 0) { return HandleLockedObjectInteraction(object, *this); }
        return { InteractionStatus::NoEffect };
    }

    ApplicationUpdateResult GameContext::UpdateApplication(double elapsedSeconds, InputHandler& input)
    {
        if (!std::isfinite(elapsedSeconds) || elapsedSeconds < 0)
        {
            throw std::invalid_argument("Application elapsed time must be finite and nonnegative.");
        }
        ApplicationInput snapshot({});
        std::array<bool, InputKeyCount> pressed{};
        for (std::size_t i = 0; i < InputKeyCount; ++i)
        {
            snapshot.keys[i] = input.IsKeyPressed(static_cast<InputKey>(i));
            pressed[i] = snapshot.keys[i] && !applicationPreviousKeys[i];
            if (!snapshot.keys[i]) { applicationBlockedKeys[i] = false; }
        }
        const auto nextLevelKey = static_cast<std::size_t>(InputKey::DebugNextLevelKey);
        const auto nextPartKey = static_cast<std::size_t>(InputKey::DebugNextGamePartKey);
        const bool levelNavigationPressed = (snapshot.keys[nextLevelKey] || snapshot.keys[nextPartKey]) &&
            !applicationPreviousKeys[nextLevelKey] && !applicationPreviousKeys[nextPartKey];
        applicationPreviousKeys = snapshot.keys;
        const auto wasPressed = [&pressed](InputKey key) { return pressed[static_cast<std::size_t>(key)]; };
        const auto isDown = [&snapshot](InputKey key) { return snapshot.IsKeyPressed(key); };
        const auto pageAtStart = menuPage;
        // No real-time catch-up for UI repeat/pulse or world simulation after a long host stall.
        const double delta = std::min(elapsedSeconds, 1.0);
        std::optional<InputKey> navigation;
        for (const auto key : { InputKey::UiUpKey, InputKey::UiDownKey, InputKey::UiLeftKey, InputKey::UiRightKey })
        {
            if (wasPressed(key))
            {
                navigation = key;
                navigationRepeatKey = key;
                navigationRepeatSeconds = 0.35;
                break;
            }
        }
        if (!navigation && navigationRepeatKey)
        {
            if (!isDown(*navigationRepeatKey)) { navigationRepeatKey.reset(); }
            else
            {
                navigationRepeatSeconds -= delta;
                if (navigationRepeatSeconds <= 0)
                {
                    navigation = navigationRepeatKey;
                    navigationRepeatSeconds = 0.1;
                }
            }
        }
        switch (menuPage)
        {
        case MenuPage::Gameplay:
            if (wasPressed(InputKey::MainMenuKey)) { SetMenuPage(MenuPage::MainMenu); }
            else if (wasPressed(InputKey::InventoryKey) && runtimeState == GameRuntimeState::Ready)
            {
                OpenInventory();
            }
            else if (runtimeState == GameRuntimeState::Ready && levelNavigationPressed &&
                isDown(InputKey::DebugNextGamePartKey))
            {
                StartNextGamePart();
            }
            else if (runtimeState == GameRuntimeState::Ready && levelNavigationPressed &&
                isDown(InputKey::DebugNextLevelKey))
            {
                EndLevel();
            }
            break;
        case MenuPage::Inventory:
            if (wasPressed(InputKey::UiCancelKey)) { SetMenuPage(MenuPage::Gameplay); }
            else
            {
                if (navigation)
                {
                    switch (*navigation)
                    {
                    case InputKey::UiUpKey: if (inventoryFocus >= 4) { inventoryFocus -= 4; } break;
                    case InputKey::UiDownKey: if (inventoryFocus < 12) { inventoryFocus += 4; } break;
                    case InputKey::UiLeftKey: if (inventoryFocus % 4 != 0) { --inventoryFocus; } break;
                    case InputKey::UiRightKey: if (inventoryFocus % 4 != 3) { ++inventoryFocus; } break;
                    default: break;
                    }
                }
                UpdateInventoryInput(wasPressed(InputKey::UiConfirmKey));
                inventoryPulseAccumulator += delta * TicksPerSecond;
                const auto steps = static_cast<unsigned int>(inventoryPulseAccumulator);
                inventoryPulseAccumulator -= steps;
                inventoryPulsePhase = static_cast<std::uint16_t>((inventoryPulsePhase + steps * 17) & 511);
            }
            break;
        case MenuPage::MainMenu:
        case MenuPage::AlternateMainMenu:
            if (wasPressed(InputKey::UiCancelKey))
            {
                // Unlike the native Cancel command, a host with no level cannot resume gameplay.
                if (runtimeState == GameRuntimeState::Ready) { SelectMainMenuAction(MainMenuAction::Cancel); }
            }
            else
            {
                if (navigation == InputKey::UiUpKey && mainMenuFocus > 0) { --mainMenuFocus; }
                if (navigation == InputKey::UiDownKey && mainMenuFocus < 1) { ++mainMenuFocus; }
                if (wasPressed(InputKey::UiConfirmKey))
                {
                    if (mainMenuFocus == 1) { SelectMainMenuAction(MainMenuAction::Quit); }
                    else if (runtimeState == GameRuntimeState::Ready)
                    {
                        SelectMainMenuAction(MainMenuAction::ResumeGame);
                    }
                }
            }
            break;
        case MenuPage::ReturnToGameplay: SetMenuPage(MenuPage::Gameplay); break;
        case MenuPage::Quit: break;
        default:
            throw std::logic_error("Nonblocking application page " +
                std::to_string(static_cast<std::uint16_t>(menuPage)) + " is not implemented.");
        }

        ApplicationUpdateResult result;
        if (applicationPageChanged || pageAtStart != menuPage)
        {
            for (std::size_t i = 0; i < InputKeyCount; ++i)
            {
                applicationBlockedKeys[i] = applicationBlockedKeys[i] || snapshot.keys[i];
            }
            applicationPageChanged = false;
            applicationTickAccumulator = 0;
            return result;
        }
        if (menuPage != MenuPage::Gameplay || paused || runtimeState != GameRuntimeState::Ready)
        {
            applicationTickAccumulator = 0;
            return result;
        }
        result.worldDeltaSeconds = delta;
        applicationTickAccumulator += delta * TicksPerSecond;
        const auto ticks = static_cast<std::int16_t>(applicationTickAccumulator);
        if (ticks == 0) { return result; }
        applicationTickAccumulator -= ticks;
        result.timing = { ticks, ticks };
        for (std::size_t i = 0; i < InputKeyCount; ++i)
        {
            snapshot.keys[i] = snapshot.keys[i] && !applicationBlockedKeys[i];
        }
        result.tick = Tick(result.timing, snapshot);
        if (result.tick->status == GameTickStatus::LevelEnded)
        {
            result.worldDeltaSeconds = 0;
            result.timing = { 0, 0 };
            for (std::size_t i = 0; i < InputKeyCount; ++i)
            {
                applicationBlockedKeys[i] = applicationBlockedKeys[i] || snapshot.keys[i];
            }
            applicationTickAccumulator = 0;
        }
        return result;
    }

    void GameContext::SwapInventoryItems(std::size_t first, std::size_t second)
    {
        std::swap(inventory.at(first), inventory.at(second));
        if (selectedWeapon == WeaponType::TrapOrPole)
        {
            if (weaponHand == first + 1) { weaponHand = static_cast<std::uint8_t>(second + 1); }
            else if (weaponHand == second + 1) { weaponHand = static_cast<std::uint8_t>(first + 1); }
        }
        RefreshWeaponInventory();
    }

    void GameContext::UpdateInventoryInput(bool confirm)
    {
        if (!confirm) { return; }
        const auto payload = InventoryPayloads.at(inventoryFocus);
        if (payload == 100) { SetMenuPage(MenuPage::Gameplay); return; }
        if (payload == 101)
        {
            if (!inventorySource) { return; }
            auto& item = inventory.at(InventoryPayloads.at(*inventorySource) - 1);
            if (item.collisionType != CollisionType::Medicine) { return; }
            if (item.itemValue > 0)
            {
                bool full;
                do
                {
                    --item.itemValue;
                    ApplyPlayerLifeDelta(29491);
                    full = playerLife >= static_cast<std::int32_t>(playerLifeMaximum) * 65536;
                } while (!full && item.itemValue >= 0);
                ++item.itemValue;
            }
            if (item.itemValue < 1) { item = {}; }
            inventorySource.reset();
            return;
        }
        auto& item = inventory.at(payload - 1);
        if (!inventorySource)
        {
            if (item.health > 0)
            {
                inventorySource = inventoryFocus;
                inventoryPulsePhase = 0;
                inventoryPulseAccumulator = 0;
            }
        }
        else
        {
            SwapInventoryItems(InventoryPayloads.at(*inventorySource) - 1, payload - 1);
            inventorySource.reset();
        }
    }

    ApplicationPresentation GameContext::GetApplicationPresentation() const
    {
        ApplicationPresentation result{ menuPage, {} };
        if (menuPage == MenuPage::Gameplay || menuPage == MenuPage::Quit) { return result; }
        const UiColor normal{ 204, 153, 85 }, bright{ 255, 230, 160 }, dark{ 85, 72, 55 };
        auto& draw = result.commands;
        draw.emplace_back(UiFill{ { 0, 0, result.Width, result.Height }, { 0, 0, 0 } });
        const auto text = [&draw, page = menuPage](float x, float y, std::string value, UiColor color,
            float scale = 0.65f, UiTextAlignment alignment = UiTextAlignment::Left)
        {
            draw.emplace_back(UiText{ x, y, std::move(value), scale, color, alignment,
                page == MenuPage::Inventory ? UiTextLayout::Native : UiTextLayout::TopAligned });
        };
        if (menuPage == MenuPage::MainMenu || menuPage == MenuPage::AlternateMainMenu)
        {
            text(180, 36, "Desperabis", bright, 1, UiTextAlignment::Center);
            text(180, 90, "Resume game", runtimeState == GameRuntimeState::Ready && CanResumeGame() ?
                (mainMenuFocus == 0 ? bright : normal) : dark, 0.8f, UiTextAlignment::Center);
            text(180, 120, "Quit", mainMenuFocus == 1 ? bright : normal, 0.8f, UiTextAlignment::Center);
            draw.emplace_back(UiFill{ { 94, mainMenuFocus == 0 ? 87.f : 117.f, 3, 15 }, bright });
            text(180, 185, "New game loading saving and settings", dark, 0.5f, UiTextAlignment::Center);
            text(180, 198, "are not implemented yet", dark, 0.5f, UiTextAlignment::Center);
            return result;
        }
        if (menuPage != MenuPage::Inventory)
        {
            throw std::logic_error("No presentation for application page " +
                std::to_string(static_cast<std::uint16_t>(menuPage)) + ".");
        }
        for (std::uint8_t cell = 0; cell < InventoryPayloads.size(); ++cell)
        {
            const auto payload = InventoryPayloads[cell];
            const float x = 10.f + (cell % 4) * 50.f;
            const float y = cell < 12 ? 10.f + (cell / 4) * 46.f : 182.f;
            std::uint8_t shade = cell == inventoryFocus ? 0 : 2;
            if (inventorySource == cell)
            {
                shade = static_cast<std::uint8_t>(std::nearbyint(
                    std::sin(inventoryPulsePhase * 0.012271846484375) * 1.9 + 2.0));
            }
            const std::uint8_t frame = cell < 12 ? 0 : cell == 12 ? 3 : cell == 13 ? 4 : 1;
            draw.emplace_back(UiBitmap{ { x, y, 0, 0 }, UiBitmapSource::Hud, 0, 0, frame, {},
                UiBitmapLayout::TopLeft, cell == 15, shade });
            if (payload == 100 || payload == 101)
            {
                continue;
            }
            const auto& item = inventory.at(payload - 1);
            if (item.health <= 0) { continue; }
            const auto picture = static_cast<std::uint8_t>(item.pickupType);
            if (picture == 0) { continue; }
            const float anchorX = x + (cell < 12 ? 21.f : 22.f), anchorY = y + 19;
            if (item.modelId >= 1 && item.modelId <= 6)
            {
                if (item.modelAnim == 0) { throw std::runtime_error("Inventory sprite has no animation."); }
                draw.emplace_back(UiBitmap{ { anchorX, anchorY, 0, 0 }, UiBitmapSource::LevelSprite, item.modelId,
                    static_cast<std::uint8_t>(item.modelAnim - 1), static_cast<std::uint8_t>(picture - 1), {},
                    UiBitmapLayout::SpriteAnchor });
            }
            else if (item.modelId >= 31 && item.modelId <= 50 && picture <= 14)
            {
                draw.emplace_back(UiBitmap{ { anchorX, anchorY, 0, 0 }, UiBitmapSource::ModelTexture, item.modelId,
                    0, picture, {}, UiBitmapLayout::Center });
            }
            else if (item.modelId >= 31 && item.modelId <= 50 && picture <= 220)
            {
                if (item.modelAnim == 0 || item.animFrame == 0)
                {
                    throw std::runtime_error("Inventory model has no animation/frame.");
                }
                draw.emplace_back(UiModel{ { anchorX - 19.5f, anchorY - 17, 39, 34 }, item.modelId,
                    static_cast<std::uint8_t>(item.modelAnim - 1), static_cast<std::uint8_t>(item.animFrame - 1),
                    item.modelFrame2, item.blendWeightOrSpriteOpacity, item.displayType,
                    { 30, -60, 30 }, static_cast<std::int16_t>(2 * picture + 50) });
            }
        }
        if (playerLifeMaximum <= 0) { throw std::logic_error("Inventory requires a positive player-life maximum."); }
        const auto life = std::clamp(playerLife / 65536, 0, static_cast<int>(playerLifeMaximum));
        text(234, 14, "Gesundheit:", {}, 1);
        text(236, 46, std::to_string(static_cast<int>(std::nearbyint(100.0 * life / playerLifeMaximum))) + "%", {}, 1);
        draw.emplace_back(UiHealthMeter{ 225, 27, static_cast<std::int16_t>(std::min(life / 4, 90)) });
        int medicine = 0;
        int fireballs = 0;
        for (const auto& item : inventory)
        {
            if (item.health == 1000 && item.collisionType == CollisionType::Medicine)
            {
                medicine += item.itemValue;
            }
            if (item.health == 1000 && item.collisionType == CollisionType::Weapon &&
                item.itemValue == static_cast<std::int16_t>(WeaponType::Fireball))
            {
                fireballs += item.generalPurpose1;
            }
        }
        const auto resource = [&draw, &text](float x, float y, float dx, float dy, UiBitmapSource source,
            std::uint8_t animation, std::uint8_t frame, int amount, std::string label)
        {
            draw.emplace_back(UiBitmap{ { x + dx, y + dy, 0, 0 }, source, 0, animation, frame, {},
                UiBitmapLayout::TopLeft, false, static_cast<std::uint8_t>(amount > 0 ? 0 : 6) });
            if (amount > 0) { text(x, y + 6, std::move(label), {}, 1, UiTextAlignment::Center); }
        };
        if (arrowPoison > 0)
        {
            draw.emplace_back(UiBitmap{ { 315, 83, 0, 0 }, UiBitmapSource::InventoryIcons, 0, 2, 0, {},
                UiBitmapLayout::TopLeft });
        }
        resource(250, 105, -18, -21, UiBitmapSource::InventoryIcons, 5, 0, gold, std::to_string(gold));
        const auto medicinePercent = static_cast<int>(std::nearbyint(medicine * 29491.0 * 100 / (360.0 * 65536)));
        resource(290, 105, -17, -37, UiBitmapSource::InventoryIcons, 2, 1, medicine,
            std::to_string(medicinePercent) + "%");
        resource(250, 170, -15, -33, UiBitmapSource::Hud, 1, 4, weaponAmounts[3], std::to_string(weaponAmounts[3]));
        resource(290, 170, -20, -37, UiBitmapSource::InventoryIcons, 0, 2, fireballs, std::to_string(fireballs));
        draw.emplace_back(UiModel{ { 308, 117, 40, 40 }, 31, 5, 3, 0, 0, DisplayType::VectorUnlit,
            { 0, 0, 0 }, 210, 128, weaponAmounts[7] > 0 ? UiColor{} : UiColor{ 64, 64, 64 } });
        if (weaponAmounts[7] > 0)
        {
            text(330, 176, std::to_string(static_cast<int>(std::nearbyint(weaponAmounts[7] / 3.6))) + "%",
                {}, 1, UiTextAlignment::Center);
        }
        if (hasMap)
        {
            draw.emplace_back(UiBitmap{ { 240, 204, 0, 0 }, UiBitmapSource::InventoryIcons, 0, 5, 1, {},
                UiBitmapLayout::TopLeft });
        }
        return result;
    }
}
