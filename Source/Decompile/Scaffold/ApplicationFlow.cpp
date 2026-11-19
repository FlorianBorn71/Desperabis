#include "GameContext.h"
#include "BehaviorDispatch.h"
#include "StringResources.h"

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
        constexpr float MenuTextX = 85.f;
        constexpr float MenuHeadingY = 16.f;
        constexpr float MenuLineHeight = 16.f;
        class ApplicationInput final : public InputHandler
        {
        public:
            explicit ApplicationInput(std::array<bool, static_cast<std::size_t>(InputKey::Count)> keys)
                : keys(keys) {}
            bool IsKeyPressed(InputKey key) override { return keys.at(static_cast<std::size_t>(key)); }
            std::array<bool, static_cast<std::size_t>(InputKey::Count)> keys;
        };
    }

    float GameContext::GetMenuRowY(std::size_t row) const
    {
        if (menuPage == MenuPage::ChapterSelection) { return 68.f + static_cast<float>(row) * 24.f; }
        std::size_t count = 6;
        float spacing = 24.f, height = MenuLineHeight, center = ApplicationPresentation::Height * 0.5f;
        if (menuPage == MenuPage::LoadGame || menuPage == MenuPage::SaveGame)
        {
            count = GetSaveMenuSlotCount();
            spacing = menuPage == MenuPage::LoadGame ? 16.f : 17.5f;
            height *= 0.8f;
            center = 136.f; // Center lists below the common heading, inside the 52..220 content band.
        }
        else if (!CanResumeGame())
        {
            --count;
            --row;
        }
        return center - ((static_cast<float>(count) - 1) * spacing + height) * 0.5f +
            static_cast<float>(row) * spacing;
    }

    float GameContext::GetMenuCursorTargetY() const
    {
        if (!UsesMenuBackground(menuPage) || menuPage == MenuPage::Settings) { return 0; }
        const auto row = menuPage == MenuPage::ChapterSelection ? chapterFocus :
            menuPage == MenuPage::LoadGame || menuPage == MenuPage::SaveGame ? saveFocus : mainMenuFocus;
        return GetMenuRowY(row) + 5.f;
    }

    bool GameContext::IsMainMenuActionEnabled(MainMenuAction action) const
    {
        switch (action)
        {
        case MainMenuAction::Cancel:
        case MainMenuAction::ResumeGame: return runtimeState == GameRuntimeState::Ready && CanResumeGame();
        case MainMenuAction::NewGame: return !pendingLevelLoad;
        case MainMenuAction::LoadGame:
            return !pendingLevelLoad && std::any_of(saveSlots.begin(), saveSlots.end(),
                [](const SaveSlot& slot) { return slot.occupied && slot.error.empty(); });
        case MainMenuAction::SaveGame:
            return runtimeState == GameRuntimeState::Ready && CanResumeGame() && !IsCutscenePlaying();
        case MainMenuAction::Settings:
        case MainMenuAction::Quit: return true;
        case MainMenuAction::Configuration: return false;
        default: throw std::invalid_argument("Unknown main-menu action.");
        }
    }

    void GameContext::OpenCodeBook(std::int16_t section)
    {
        if (section <= 0 || menuPage != MenuPage::Gameplay || runtimeState != GameRuntimeState::Ready)
        {
            throw std::logic_error("Reading a code book requires a positive text section and active gameplay.");
        }
        const auto bytes = LoadFile("LEVEL\\TEXTE.TXT");
        const std::string contents(bytes.begin(), bytes.end());
        const auto marker = "@" + std::to_string(section);
        std::size_t start = 0;
        for (;;)
        {
            start = contents.find(marker, start);
            if (start == std::string::npos) { throw std::runtime_error("Code-book text section is missing."); }
            start += marker.size();
            if (start == contents.size() || contents[start] < '0' || contents[start] > '9') { break; }
        }
        const auto end = contents.find('@', start);
        std::vector<UiBitmap> pictures;
        float y = 5, width = 0;
        for (auto i = start; i < contents.size() && i != end; ++i)
        {
            const auto value = static_cast<std::uint8_t>(contents[i]);
            if (value == '\r' || value == ' ') { width += 8; }
            else if (value == '\n') { continue; }
            else if (value == '#') { y += 16; width = 0; }
            else if (value == '+') { y += 6; }
            else if (value == 254)
            {
                if (++i == contents.size() || i == end || contents[i] < 'A' || contents[i] > '^')
                {
                    throw std::runtime_error("Code-book illustration selector is invalid.");
                }
                pictures.push_back(UiBitmap{{20 + width, y + 20, 0, 0},
                    UiBitmapSource::TextIllustration, 0, 0, static_cast<std::uint8_t>(contents[i] - 'A'),
                    {}, UiBitmapLayout::TopLeft});
            }
            else { throw std::runtime_error("Code book contains an unrecovered text control or text layout."); }
        }
        if (pictures.empty()) { throw std::runtime_error("Code book contains no illustrations."); }
        codeBookPictures = std::move(pictures);
        SetMenuPage(MenuPage::CodeBook);
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
        if (QueueScreenChange([this] { FinishGamePart(); }, 0.6)) { return; }
        bossFall.reset();
        cutscenePlaying = false;
        cutsceneCameraSlot.reset();
        recordedCutscene.clear();
        recordedCameraTrack.clear();
        introCredit.reset();
        musicAmbientLoaded = false;
        ResetUnderwater();
        StopAllSoundEffects();
        if (audioHandler && ambientPlaybackActive) { audioHandler->StopAmbientPlayback(); }
        ambientPlaybackActive = false;
        activeAmbientSelector.reset();
        levelAmbientSound.reset();
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
        mainMenuFocus = 1;
    }

    void GameContext::EndLevel(bool finishGamePart, bool showStory)
    {
        if (runtimeState != GameRuntimeState::Ready || menuPage != MenuPage::Gameplay || !loadedLevelNumber)
        {
            throw std::logic_error("Ending a level requires ready gameplay.");
        }
        if (QueueScreenChange([this, finishGamePart, showStory] { EndLevel(finishGamePart, showStory); }, 0.6)) { return; }
        if (*loadedLevelNumber == 96)
        {
            StartLevel(95);
            return;
        }
        if (*loadedLevelNumber == 95)
        {
            pendingStorySections = {{95, {}}};
            BeginLevelStory();
            cutscenePlaying = false;
            SetMenuPage(MenuPage::StoryBook);
            return;
        }
        if (*loadedLevelNumber == 91)
        {
            FinishGamePart();
            return;
        }
        const auto& order = CurrentLevelOrder();
        const auto current = std::find(order.begin(), order.end(), *loadedLevelNumber);
        const auto next = finishGamePart ? std::optional<std::uint8_t>{} :
            NextPlayableLevel(order, static_cast<std::size_t>(current - order.begin()) + 1);
        const bool completed = !next;
        PrepareLevelProgression(!completed);
        if (completed && *loadedLevelNumber == 35)
        {
            StartLevel(91);
        }
        else if (completed && *loadedLevelNumber == 26 && activePartHasCredits)
        {
            StartLevel(96);
        }
        else if (completed)
        {
            FinishGamePart();
        }
        else { StartLevel(*next, showStory); }
    }

    void GameContext::StartNextGamePart()
    {
        if (runtimeState != GameRuntimeState::Ready || menuPage != MenuPage::Gameplay || !loadedLevelNumber)
        {
            throw std::logic_error("Game-part navigation requires ready gameplay.");
        }
        EnsureGameParts();
        const auto& order = CurrentLevelOrder();
        const auto current = std::find_if(gameParts.begin(), gameParts.end(), [&](const GamePart& part)
        {
            return part.levels == order;
        });
        if (current == gameParts.end() || current + 1 == gameParts.end())
        {
            throw std::logic_error("No next HERZ game part is available.");
        }
        const auto& next = *(current + 1);
        const auto start = NextPlayableLevel(next.levels, 0);
        if (!start) { throw std::logic_error("The next HERZ part has no playable levels."); }
        PrepareLevelProgression(true);
        StartLevel(*start);
    }

    bool GameContext::CanApplyWeaponCheatLoadout(std::int16_t levelNumber)
    {
        if (levelNumber <= 0) { throw std::invalid_argument("Level numbers must be positive."); }
        EnsureGameParts();
        const auto containsLevel = [levelNumber](const GamePart& part)
        {
            return std::find(part.levels.begin(), part.levels.end(), levelNumber) != part.levels.end();
        };
        return containsLevel(gameParts.at(0)) ||
            (gameParts.size() > 2 && containsLevel(gameParts[2]));
    }

    bool GameContext::CanApplyMapCheat(std::int16_t levelNumber)
    {
        if (levelNumber <= 0) { throw std::invalid_argument("Level numbers must be positive."); }
        EnsureGameParts();
        if (gameParts.size() < 2) { return false; }
        const auto& levels = gameParts[1].levels;
        return std::find(levels.begin(), levels.end(), levelNumber) != levels.end();
    }

    void GameContext::InitializeNewGame(std::int16_t chapter)
    {
        EnsureGameParts();
        if (static_cast<std::size_t>(chapter) > gameParts.size())
        {
            throw std::invalid_argument("The requested part is absent from HERZ.");
        }
        const auto& part = gameParts.at(static_cast<std::size_t>(chapter - 1));
        const auto start = NextPlayableLevel(part.levels, 0);
        if (!start) { throw std::logic_error("The selected HERZ part has no playable levels."); }
        activeLevelOrder = part.levels;
        activePartHasCredits = part.endsWithCredits;
        nativeSavePrefix.clear();
        inventory = {};
        gold = arrowPoison = 0;
        hasMap = false;
        weaponAmounts.fill(0);
        weaponAmounts[8] = weaponAmounts[23] = 100;
        weaponAmounts[20] = 1000;
        weaponInventorySlots.fill(0);
        selectedWeapon = requestedWeapon = WeaponType::None;
        weaponSpriteFrame = weaponHand = requestedWeaponHand = 0;
        weaponAnimationSpeed = weaponAnimationPosition = weaponAnimationRecoil = 0;
        weaponElectricityFiring = false;
        weaponElectricitySparking = false;
        weaponReady = true;
        playerLifeMaximum = 360;
        playerLifeDeltaSuppressed = playerHitEffectTicks = 0;
        ApplyPlayerLifeDelta(360 * 65536 - playerLife);
        positionalSoundsSuppressed = false;
        field_D500 = true;
        paused = false;
        PrepareStoryIntroductions(part.levels, *start);
        StartLevel(*start, true);
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

    bool GameContext::QueueScreenChange(std::function<void()> change, double duration, bool menuContentOnly)
    {
        if (!screenTransitionsEnabled || applyingScreenChange) { return false; }
        if (screenFade != ScreenFade::None) { return true; }
        pendingScreenChange = std::move(change);
        screenFade = ScreenFade::Out;
        screenFadeSeconds = 0;
        screenFadeDuration = duration;
        fadeMenuContentOnly = menuContentOnly;
        fadeScreenAudio = duration >= 0.6;
        return true;
    }

    void GameContext::SetScreenAudioGain(float gain)
    {
        if (audioHandler && ambientPlaybackActive) { audioHandler->SetAmbientGain(gain); }
        screenAudioGain = gain;
    }

    void GameContext::RefreshScreenAudio()
    {
        if (!audioHandler || !soundEnabled) { return; }
        std::optional<std::uint8_t> selector;
        if (menuPage == MenuPage::StoryBook || menuPage == MenuPage::CastleIntroduction)
        {
            if (storyPageIndex < storyPageAudio.size()) { selector = storyPageAudio[storyPageIndex]; }
        }
        else if (menuPage == MenuPage::Gameplay) { selector = levelAmbientSound; }
        else if (!hasActiveGame && (menuPage == MenuPage::MainMenu || menuPage == MenuPage::AlternateMainMenu))
        {
            // 0000:A681 selects MUSIC14; returning from a pause menu must not restart level audio.
            selector = std::uint8_t{114};
        }
        else { return; }
        if (selector && ambientPlaybackActive && activeAmbientSelector == selector) { return; }
        if (!selector)
        {
            if (ambientPlaybackActive) { audioHandler->StopAmbientPlayback(); }
            ambientPlaybackActive = false;
            activeAmbientSelector.reset();
            musicAmbientLoaded = false;
            return;
        }
        CreateAmbientLoop(*selector);
        if (*selector == 1) { audioHandler->SetAmbientIndoor(playerIndoors); }
        audioHandler->SetAmbientGain(screenAudioGain);
        if (*selector != 102) { audioHandler->StartAmbientPlayback(); }
        ambientPlaybackActive = true;
        activeAmbientSelector = selector;
    }

    void GameContext::BeginScreenFadeIn(double duration)
    {
        screenFade = ScreenFade::In;
        screenFadeSeconds = 0;
        screenFadeDuration = menuPage == MenuPage::MainMenu || menuPage == MenuPage::AlternateMainMenu ? 0.15 : duration;
        screenTint = {0, 0, 0, 255};
    }

    bool GameContext::UpdateScreenFade(double delta)
    {
        if (screenFade == ScreenFade::None) { return false; }
        double remaining = delta;
        do
        {
            if (screenFade == ScreenFade::Black)
            {
                if (runtimeState == GameRuntimeState::Loading) { return true; }
                BeginScreenFadeIn(screenFadeDuration);
            }
            const auto phase = screenFade;
            const double step = std::min(remaining, screenFadeDuration - screenFadeSeconds);
            screenFadeSeconds += step;
            remaining -= step;
            if (phase == ScreenFade::DeathRed || phase == ScreenFade::DeathBlack)
            {
                applicationTickAccumulator += step * TicksPerSecond;
                const auto ticks = static_cast<int>(applicationTickAccumulator);
                applicationTickAccumulator -= ticks;
                for (int tick = 0; tick < ticks; ++tick) { UpdatePlayerDeathCamera(); }
            }
            else { applicationTickAccumulator = 0; }
            const double t = std::clamp(screenFadeSeconds / screenFadeDuration, 0.0, 1.0);
            const double eased = t * t * (3 - 2 * t);
            const UiColor darkRed{96, 5, 5, 255}, black{0, 0, 0, 255}, white{};
            const UiColor from = phase == ScreenFade::In ? black :
                phase == ScreenFade::DeathBlack ? darkRed : white;
            const UiColor to = phase == ScreenFade::In ? white :
                phase == ScreenFade::DeathRed ? darkRed : black;
            const auto channel = [eased](std::uint8_t a, std::uint8_t b)
            {
                return static_cast<std::uint8_t>(std::lround(a + (b - a) * eased));
            };
            screenTint = {channel(from.r, to.r), channel(from.g, to.g), channel(from.b, to.b), 255};
            if (fadeScreenAudio)
            {
                const double deathProgress = ((phase == ScreenFade::DeathBlack ? 2.5 : 0.0) + screenFadeSeconds) / 3.5;
                const double gain = phase == ScreenFade::In ? eased :
                    phase == ScreenFade::DeathRed || phase == ScreenFade::DeathBlack ?
                        1 - deathProgress * deathProgress * (3 - 2 * deathProgress) : 1 - eased;
                SetScreenAudioGain(static_cast<float>(gain));
            }
            if (screenFadeSeconds < screenFadeDuration) { break; }
            if (phase == ScreenFade::In)
            {
                screenFade = ScreenFade::None;
                screenTint = {};
                break;
            }
            if (phase == ScreenFade::DeathRed)
            {
                screenFade = ScreenFade::DeathBlack;
                screenFadeSeconds = 0;
                screenFadeDuration = 1;
                continue;
            }
            screenFade = ScreenFade::Black;
            screenFadeSeconds = 0;
            if (phase == ScreenFade::DeathBlack) { screenFadeDuration = 0.6; }
            auto change = std::move(pendingScreenChange);
            applyingScreenChange = true;
            try { change(); }
            catch (...)
            {
                applyingScreenChange = false;
                BeginScreenFadeIn(screenFadeDuration);
                throw;
            }
            applyingScreenChange = false;
        } while (remaining > 0);
        return true;
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
        if (runtimeState != GameRuntimeState::Loading)
        {
            quickSaveFeedbackSeconds = std::max(0.0, quickSaveFeedbackSeconds - elapsedSeconds);
        }
        if (screenTransitionsEnabled && screenFade == ScreenFade::None &&
            menuPage == MenuPage::Gameplay && runtimeState == GameRuntimeState::Ready &&
            hasActiveGame && playerLife <= 0 && !cutscenePlaying)
        {
            screenFade = ScreenFade::DeathRed;
            screenFadeSeconds = 0;
            screenFadeDuration = 2.5;
            fadeScreenAudio = true;
            fadeMenuContentOnly = false;
            pendingScreenChange = [this] { FinishGamePart(); };
        }
        if (UpdateScreenFade(delta))
        {
            for (std::size_t i = 0; i < InputKeyCount; ++i)
            {
                applicationBlockedKeys[i] = applicationBlockedKeys[i] || snapshot.keys[i];
            }
            queuedText.clear();
            return {};
        }
        if (UsesMenuBackground(menuPage))
        {
            menuAnimationSeconds = std::fmod(menuAnimationSeconds + delta, 1.0);
        }
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
        if (wasPressed(InputKey::QuickLoadKey) && !pendingLevelLoad &&
            (menuPage == MenuPage::Gameplay || menuPage == MenuPage::MainMenu ||
                menuPage == MenuPage::AlternateMainMenu))
        {
            quickSaveFeedbackSeconds = 0;
            try
            {
                auto bytes = ReadSaveFile(QuickSaveSlot);
                if (!bytes) { throw std::runtime_error(GetString("save_empty")); }
                LoadGame(std::move(*bytes));
                quickSaveFeedback = GetString("save_quickloaded");
                quickSaveFeedbackSeconds = 2;
            }
            catch (const std::runtime_error& error)
            {
                SetMenuPage(MenuPage::LoadGame);
                saveFocus = QuickSaveSlot - 1;
                saveError = error.what();
            }
            applicationPageChanged = true;
        }
        else switch (menuPage)
        {
        case MenuPage::Gameplay:
            if (wasPressed(InputKey::QuickSaveKey))
            {
                quickSaveFeedbackSeconds = 0;
                if (runtimeState == GameRuntimeState::Ready && CanResumeGame() && !paused &&
                    playerLife > 0 && !IsCutscenePlaying())
                {
                    try
                    {
                        WriteSaveFile(QuickSaveSlot, CaptureSaveGame(GetString("save_quicksave")));
                        quickSaveFeedback = GetString("save_quicksaved");
                        quickSaveFeedbackSeconds = 2;
                    }
                    catch (const std::runtime_error& error) { PromptUserMessage(error.what(), 165); }
                }
                else { PromptUserMessage(GetString("save_unavailable"), 100); }
            }
            if (IsMuseumIntroPlaying())
            {
                if (runtimeState == GameRuntimeState::Ready && wasPressed(InputKey::UiCancelKey))
                {
                    FinishGamePart();
                }
                break;
            }
            if (IsBossFallPlaying()) { break; }
            if (wasPressed(InputKey::MainMenuKey) || wasPressed(InputKey::UiCancelKey)) { SetMenuPage(MenuPage::MainMenu); }
            else if (wasPressed(InputKey::InventoryKey) && runtimeState == GameRuntimeState::Ready &&
                !IsCutscenePlaying())
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
                EndLevel(false, false);
            }
            break;
        case MenuPage::StoryBook:
            if (wasPressed(InputKey::UiCancelKey)) { CompleteStoryBook(); break; }
            storySeconds += delta;
            storyPageSeconds += delta;
            if (storyEndsGame && storyPageIndex + 1 == storyPages.size() && storyPageSeconds >= 5)
            {
                CompleteStoryBook();
                break;
            }
            if (storyTurn >= 0)
            {
                storyTurn = std::min(1.0, storyTurn + delta / 1.2);
                if (storyTurn == 1)
                {
                    AdvanceStoryPage();
                }
            }
            else if (input.WasAnyKeyPressed() ||
                std::any_of(pressed.begin(), pressed.end(), [](bool value) { return value; }))
            {
                if (storyPageIndex + 1 == storyPages.size())
                {
                    CompleteStoryBook();
                }
                else if (IsCastleStoryPage(storyPageIndex + 1))
                {
                    if (!QueueScreenChange([this] { AdvanceStoryPage(); }, 0.6)) { AdvanceStoryPage(); }
                }
                else
                {
                    storyTurn = 0;
                    PlaySoundEffect(static_cast<SoundEffect>(51), 255, 0, 1);
                }
            }
            break;
        case MenuPage::CastleIntroduction:
            storyPageSeconds += delta;
            if (storyPageSeconds >= 5 || input.WasAnyKeyPressed() ||
                std::any_of(pressed.begin(), pressed.end(), [](bool value) { return value; }))
            {
                if (!QueueScreenChange([this] { AdvanceStoryPage(); }, 0.6)) { AdvanceStoryPage(); }
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
        case MenuPage::Map:
        case MenuPage::CodeBook:
            if (wasPressed(InputKey::UiCancelKey) || wasPressed(InputKey::UiConfirmKey))
            {
                SetMenuPage(MenuPage::Gameplay);
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
                if (navigation == InputKey::UiUpKey && mainMenuFocus > (CanResumeGame() ? 0 : 1)) { --mainMenuFocus; }
                if (navigation == InputKey::UiDownKey && mainMenuFocus < 5) { ++mainMenuFocus; }
                if (wasPressed(InputKey::UiConfirmKey))
                {
                    if (mainMenuFocus == 5) { SelectMainMenuAction(MainMenuAction::Quit); }
                    else if (mainMenuFocus == 4) { SelectMainMenuAction(MainMenuAction::Settings); }
                    else if (mainMenuFocus == 1 && runtimeState != GameRuntimeState::Loading)
                    {
                        EnsureGameParts();
                        SelectMainMenuAction(MainMenuAction::NewGame);
                    }
                    else if (mainMenuFocus == 2 || mainMenuFocus == 3)
                    {
                        const auto action = mainMenuFocus == 2 ? MainMenuAction::LoadGame : MainMenuAction::SaveGame;
                        if (IsMainMenuActionEnabled(action) &&
                            SelectMainMenuAction(action)) { RefreshSaveSlots(); }
                    }
                    else if (runtimeState == GameRuntimeState::Ready)
                    {
                        SelectMainMenuAction(MainMenuAction::ResumeGame);
                    }
                }
            }
            break;
        case MenuPage::Settings:
            if (wasPressed(InputKey::UiCancelKey)) { SetMenuPage(MenuPage::MainMenu); }
            break;
        case MenuPage::ChapterSelection:
            if (wasPressed(InputKey::UiCancelKey)) { SetMenuPage(MenuPage::MainMenu); }
            else
            {
                if (navigation == InputKey::UiUpKey && chapterFocus > 0) { --chapterFocus; }
                if (navigation == InputKey::UiDownKey &&
                    static_cast<std::size_t>(chapterFocus) + 1 < gameParts.size()) { ++chapterFocus; }
                if (wasPressed(InputKey::UiConfirmKey) && runtimeState != GameRuntimeState::Loading)
                {
                    StartNewGame(static_cast<std::int16_t>(chapterFocus + 1));
                }
            }
            break;
        case MenuPage::LoadGame:
        case MenuPage::SaveGame:
            if (wasPressed(InputKey::UiCancelKey))
            {
                if (editingSaveName) { editingSaveName = false; saveError.clear(); }
                else { SetMenuPage(MenuPage::MainMenu); }
            }
            else if (!editingSaveName)
            {
                if (navigation == InputKey::UiUpKey && saveFocus > 0) { --saveFocus; saveError.clear(); }
                if (navigation == InputKey::UiDownKey &&
                    saveFocus + 1u < GetSaveMenuSlotCount()) { ++saveFocus; saveError.clear(); }
                if (wasPressed(InputKey::UiConfirmKey) && runtimeState != GameRuntimeState::Loading)
                {
                    if (menuPage == MenuPage::LoadGame)
                    {
                        try
                        {
                            auto bytes = ReadSaveFile(static_cast<std::uint8_t>(saveFocus + 1));
                            if (!bytes) { saveError = GetString("save_empty"); }
                            else { LoadGame(std::move(*bytes)); }
                        }
                        catch (const std::runtime_error& error) { saveError = error.what(); }
                    }
                    else if (runtimeState == GameRuntimeState::Ready && CanResumeGame() && !IsCutscenePlaying())
                    {
                        editingSaveName = true;
                        saveCaretSeconds = 0;
                        saveError.clear();
                        saveName = saveSlots[saveFocus].occupied && saveSlots[saveFocus].error.empty() ?
                            saveSlots[saveFocus].name : "";
                    }
                }
            }
            else
            {
                saveCaretSeconds = std::fmod(saveCaretSeconds + delta, 1.0);
                if (!queuedText.empty()) { saveCaretSeconds = 0; }
                for (const auto c : queuedText)
                {
                    if (c == '\b') { if (!saveName.empty()) { saveName.pop_back(); } }
                    else if (saveName.size() < NativeSaveGame::NameCapacity) { saveName.push_back(c); }
                }
                if (wasPressed(InputKey::UiConfirmKey))
                {
                    if (saveName.empty()) { saveError = GetString("save_name_required"); }
                    else
                    {
                        try
                        {
                            WriteSaveFile(static_cast<std::uint8_t>(saveFocus + 1), CaptureSaveGame(saveName));
                            editingSaveName = false;
                            SetMenuPage(MenuPage::Gameplay);
                        }
                        catch (const std::runtime_error& error) { saveError = error.what(); }
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

        if (UsesMenuBackground(menuPage))
        {
            const float target = GetMenuCursorTargetY();
            menuCursorY += (target - menuCursorY) * static_cast<float>(1 - std::exp(-20 * delta));
            if (std::abs(target - menuCursorY) < 0.01f) { menuCursorY = target; }
        }
        queuedText.clear();
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
        const double cameraFrame = recordedCameraFrame + delta * TicksPerSecond;
        applicationTickAccumulator += delta * TicksPerSecond;
        const auto ticks = static_cast<std::int16_t>(applicationTickAccumulator);
        if (ticks == 0)
        {
            if (cutscenePlaying) { recordedCameraFrame = cameraFrame; }
            return result;
        }
        applicationTickAccumulator -= ticks;
        result.timing = { ticks, ticks };
        for (std::size_t i = 0; i < InputKeyCount; ++i)
        {
            snapshot.keys[i] = snapshot.keys[i] && !applicationBlockedKeys[i];
        }
        result.tick = Tick(result.timing, snapshot);
        if (result.tick->status == GameTickStatus::Advanced && cutscenePlaying)
        {
            recordedCameraFrame = cameraFrame;
        }
        if (result.tick->status == GameTickStatus::LevelEnded || menuPage != pageAtStart || screenFade != ScreenFade::None)
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
        result.screenTint = screenTint;
        if (fadeMenuContentOnly && screenFade != ScreenFade::None)
        {
            result.menuContentOpacity = screenTint.r / 255.f;
            result.screenTint = {};
        }
        if (screenFade == ScreenFade::Black && runtimeState == GameRuntimeState::Loading) { return result; }
        if (menuPage == MenuPage::CastleIntroduction)
        {
            result.commands = {UiFill{{0, 0, 360, 240}, {0, 0, 0, 255}},
                UiBitmap{{0, 0, 360, 240}, UiBitmapSource::CastleIntroduction,
                    0, 0, 0, {}, UiBitmapLayout::Stretch}};
            return result;
        }
        if (menuPage == MenuPage::StoryBook)
        {
            result.story = UiStoryBook{nextLoadRequestId, storyPageIndex, storySeconds, storyTurn,
                storyPages.at(storyPageIndex), storyPageIndex + 1 < storyPages.size() && !IsCastleStoryPage(storyPageIndex + 1) ?
                    storyPages.at(storyPageIndex + 1) : std::vector<UiDrawCommand>{}};
            result.story->rigidTurn = std::any_of(result.story->next.begin(), result.story->next.end(),
                [](const UiDrawCommand& command)
                {
                    const auto* bitmap = std::get_if<UiBitmap>(&command);
                    return bitmap && bitmap->source == UiBitmapSource::BookCover;
                });
            return result;
        }
        if (menuPage == MenuPage::Gameplay)
        {
            if (IsMuseumIntroPlaying() && introCredit && introCredit->remaining > 0)
            {
                // A5E7 fades in over sixteen native ticks and fades out over the last sixteen.
                const int shade = std::clamp(std::min(introCredit->duration - introCredit->remaining,
                    introCredit->remaining) / 2, 0, 8);
                result.commands.emplace_back(UiBitmap{{180, 125, 0, 0}, UiBitmapSource::IntroCredits,
                    0, static_cast<std::uint8_t>(introCredit->frame / 5),
                    static_cast<std::uint8_t>(introCredit->frame % 5),
                    {255, 255, 255, static_cast<std::uint8_t>(shade * 255 / 8)}, UiBitmapLayout::SpriteAnchor});
            }
            if (IsCutscenePlaying()) { return result; }
            if (runtimeState == GameRuntimeState::Ready && selectedWeapon == WeaponType::ShopGold &&
                worldObjects[1].itemValue <= 0)
            {
                result.commands.emplace_back(UiText{9, 6, FormatString("hud_gold", {{"amount", std::to_string(gold)}}), 0.65f,
                    {204, 153, 85}, UiTextAlignment::Left, UiTextLayout::Native});
            }
            if (hasMap && runtimeState == GameRuntimeState::Ready && collisionMap)
            {
                result.commands.emplace_back(UiFill{ { 288, 168, 72, 1 }, { 70, 60, 85 } });
                result.commands.emplace_back(UiFill{ { 288, 168, 1, 70 }, { 70, 60, 85 } });
                result.commands.emplace_back(UiMap{ { 289, 169, 71, 69 }, false,
                    (worldObjects[1].posX - 112.f) / 25.f,
                    162.f - (worldObjects[1].posZ - 38.f) / 25.f, playerHeading });
            }
            if (runtimeState == GameRuntimeState::Ready && quickSaveFeedbackSeconds > 0)
            {
                result.commands.emplace_back(UiText{2, 2, quickSaveFeedback, 0.3f,
                    {255, 160, 32}, UiTextAlignment::Left, UiTextLayout::ViewportTopLeft});
            }
            return result;
        }
        if (menuPage == MenuPage::Quit) { return result; }
        const UiColor normal{ 204, 153, 85 }, bright{ 255, 230, 160 }, dark{ 85, 72, 55 };
        auto& draw = result.commands;
        draw.emplace_back(UiFill{ { 0, 0, result.Width, result.Height }, { 0, 0, 0 } });
        if (menuPage == MenuPage::Map)
        {
            draw.emplace_back(UiMap{ { 0, 0, result.Width, result.Height }, true });
            return result;
        }
        if (menuPage == MenuPage::CodeBook)
        {
            draw.emplace_back(UiBitmap{{0, 0, result.Width, result.Height},
                UiBitmapSource::MapPaper, 0, 0, 0, {}, UiBitmapLayout::Stretch});
            for (const auto& picture : codeBookPictures) { draw.emplace_back(picture); }
            return result;
        }
        const auto text = [&draw, page = menuPage](float x, float y, std::string value, UiColor color,
            float scale = 0.65f, UiTextAlignment alignment = UiTextAlignment::Left)
        {
            draw.emplace_back(UiText{ x, y, std::move(value), scale, color, alignment,
                page == MenuPage::Inventory ? UiTextLayout::Native : UiTextLayout::Menu });
        };
        const auto sword = [&draw, this, normal](float x, float y)
        {
            const auto frame = static_cast<std::uint8_t>(menuAnimationSeconds * 10);
            draw.emplace_back(UiBitmap{ { x, y, 0, 0 }, UiBitmapSource::MenuCursor, 0,
                static_cast<std::uint8_t>(frame / 5), static_cast<std::uint8_t>(frame % 5),
                normal, UiBitmapLayout::SpriteAnchor });
        };
        if (UsesMenuBackground(menuPage))
        {
            draw.emplace_back(UiBitmap{ { 0, 0, result.Width, result.Height },
                UiBitmapSource::MenuBackground, 0, 0, 0, {}, UiBitmapLayout::Stretch });
        }
        if (menuPage == MenuPage::MainMenu || menuPage == MenuPage::AlternateMainMenu)
        {
            constexpr std::array<std::string_view, 6> labels{
                "menu_resume", "menu_new", "menu_load", "menu_save", "menu_settings", "menu_quit"};
            constexpr std::array<MainMenuAction, 6> actions{MainMenuAction::ResumeGame, MainMenuAction::NewGame,
                MainMenuAction::LoadGame, MainMenuAction::SaveGame, MainMenuAction::Settings, MainMenuAction::Quit};
            for (std::size_t row = CanResumeGame() ? 0 : 1; row < labels.size(); ++row)
            {
                text(MenuTextX, GetMenuRowY(row), GetString(labels[row]),
                    IsMainMenuActionEnabled(actions[row]) ? normal : dark, 1.f);
            }
            sword(48, menuCursorY);
            return result;
        }
        if (menuPage == MenuPage::Settings)
        {
            text(MenuTextX, (result.Height - MenuLineHeight) * 0.5f, GetString("menu_settings"), normal, 1.f);
            return result;
        }
        if (menuPage == MenuPage::ChapterSelection)
        {
            for (std::size_t part = 0; part < gameParts.size(); ++part)
            {
                text(75, GetMenuRowY(part),
                    GetString(IndexedStringId("chapter_", static_cast<int>(part) + 1)), normal, 0.8f);
            }
            sword(48, menuCursorY);
            return result;
        }
        if (menuPage == MenuPage::LoadGame || menuPage == MenuPage::SaveGame)
        {
            text(MenuTextX, MenuHeadingY, GetString(menuPage == MenuPage::LoadGame ? "menu_load" : "menu_save"), normal, 1.f);
            for (std::size_t slot = 0; slot < GetSaveMenuSlotCount(); ++slot)
            {
                const auto& entry = saveSlots[slot];
                const bool editing = editingSaveName && slot == saveFocus;
                text(MenuTextX, GetMenuRowY(slot),
                    editing ? saveName : slot + 1 == QuickSaveSlot ? GetString("save_quicksave") :
                        !entry.error.empty() ? GetString("save_unreadable") : entry.occupied ? entry.name : "",
                    slot + 1 == QuickSaveSlot && (!entry.occupied || !entry.error.empty()) ? dark : normal, 0.8f);
                std::get<UiText>(draw.back()).caret = editing && saveCaretSeconds < 0.5;
            }
            sword(48, menuCursorY);
            const auto& error = saveError.empty() ? saveSlots[saveFocus].error : saveError;
            if (!error.empty()) { text(180, 230, error, bright, 0.4f, UiTextAlignment::Center); }
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
        const auto focusedPayload = InventoryPayloads.at(inventoryFocus);
        if (focusedPayload <= inventory.size())
        {
            const auto& item = inventory.at(focusedPayload - 1);
            int label = static_cast<std::uint8_t>(item.collisionType);
            if (item.health > 0 && label > 0 && label < 120)
            {
                // Native 1E48:0B03 selects a BEZEICH row, not the weapon/attack ID in itemValue.
                if (item.renderPlacementMode == 20)
                {
                    if (item.spriteScale != 64) { label = 100 + item.spriteScale; }
                }
                else if ((item.collisionType == CollisionType::Weapon || item.collisionType == CollisionType::Ammo) &&
                    item.doorLockOrGroupBitmask > 0)
                {
                    label = 100 + item.doorLockOrGroupBitmask;
                }
                if (item.collisionType == CollisionType::BuyItem) { label = item.generalPurpose1; }
                const auto id = IndexedStringId("item_", label);
                // Deliberately blank native rows have no entry in the localized catalog.
                if (const auto* name = FindString(id); name && !name->empty())
                {
                    text(20, 161, item.collisionType == CollisionType::BuyItem ?
                        FormatString(id, {{"price", std::to_string(item.doorLockOrGroupBitmask)}}) : *name,
                        {255, 192, 128}, 1.f);
                }
            }
        }
        if (playerLifeMaximum <= 0) { throw std::logic_error("Inventory requires a positive player-life maximum."); }
        const auto life = std::clamp(playerLife / 65536, 0, static_cast<int>(playerLifeMaximum));
        text(234, 14, GetString("inventory_health"), {}, 1);
        text(236, 46, FormatString("percent", {{"amount",
            std::to_string(static_cast<int>(std::nearbyint(100.0 * life / playerLifeMaximum)))}}), {}, 1);
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
            FormatString("percent", {{"amount", std::to_string(medicinePercent)}}));
        resource(250, 170, -15, -33, UiBitmapSource::Hud, 1, 4, weaponAmounts[3], std::to_string(weaponAmounts[3]));
        resource(290, 170, -20, -37, UiBitmapSource::InventoryIcons, 0, 2, fireballs, std::to_string(fireballs));
        draw.emplace_back(UiModel{ { 308, 117, 40, 40 }, 31, 5, 3, 0, 0, DisplayType::VectorUnlit,
            { 0, 0, 0 }, 210, 128, weaponAmounts[7] > 0 ? UiColor{} : UiColor{ 64, 64, 64 } });
        if (weaponAmounts[7] > 0)
        {
            text(330, 176, FormatString("percent", {{"amount",
                std::to_string(static_cast<int>(std::nearbyint(weaponAmounts[7] / 3.6)))}}),
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
