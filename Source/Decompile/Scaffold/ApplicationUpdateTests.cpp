#include "GameContext.h"
#include "BehaviorDispatch.h"
#include "StringResources.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <iterator>

using namespace Desperabis;

namespace
{
    std::filesystem::path originalAssets;
    void Check(bool condition, const char* message)
    {
        if (!condition) { throw std::runtime_error(message); }
    }

    template<typename Exception, typename Action>
    void ExpectError(Action action)
    {
        bool caught = false;
        try { action(); }
        catch (const Exception&) { caught = true; }
        Check(caught, "Expected an explicit application update error.");
    }

    class Input final : public InputHandler
    {
    public:
        std::array<bool, static_cast<std::size_t>(InputKey::Count)> keys{};
        int calls = 0;
        void Set(InputKey key, bool value) { keys.at(static_cast<std::size_t>(key)) = value; }
        bool IsKeyPressed(InputKey key) override { ++calls; return keys.at(static_cast<std::size_t>(key)); }
    };

    class Host final : public GameContext
    {
    public:
        bool synchronous = true;
        bool fail = false;
        bool reportFailure = false;
        std::vector<LevelLoadRequest> requests;
        std::array<std::optional<std::vector<std::uint8_t>>, QuickSaveSlot> saves{};
        bool writeFails = false;
        int saveWrites = 0;
        std::string prompt;
        void PromptUserMessage(std::string_view text, std::int16_t) override { prompt = text; }
        std::vector<std::uint8_t> LoadFile(std::string_view filename) override
        {
            std::ifstream file(originalAssets / filename, std::ios::binary);
            if (!file) { throw std::runtime_error("Missing test asset."); }
            return { std::istreambuf_iterator<char>(file), {} };
        }
        std::optional<std::vector<std::uint8_t>> ReadSaveFile(std::uint8_t slot) override { return saves.at(slot - 1); }
        void WriteSaveFile(std::uint8_t slot, const std::vector<std::uint8_t>& bytes) override
        {
            if (writeFails) { throw std::runtime_error("Test disk is full."); }
            saves.at(slot - 1) = bytes;
            ++saveWrites;
        }
        int healthNotifications = 0;
        void OnPlayerHealthChanged(std::int32_t, std::int32_t) override { ++healthNotifications; }
        void LoadLevel(const LevelLoadRequest& request) override
        {
            requests.push_back(request);
            if (fail) { throw std::runtime_error("New-game fixture failed to load."); }
            if (reportFailure) { FailLevelLoad(request.id, "New-game fixture reported failure."); return; }
            if (synchronous)
            {
                if (GetPendingSaveGame())
                {
                    LevelLoadData data;
                    LoadLevelGeometry(request.levelNumber, data);
                    LoadLevelObjects(request.levelNumber, data);
                    InitializeLevelPlayerSpawn(data, { 0, 0, 100 });
                    CompleteLevelLoad(request.id, std::move(data));
                }
                else { CompleteLevelLoad(request.id, MakeLevel()); }
            }
        }
        static LevelLoadData MakeLevel()
        {
            LevelLoadData data;
            data.level = std::make_unique<Level>(CreateDummyLevel());
            data.objects[1].health = 1000;
            return data;
        }
    };

    void Ready(Host& host, Input& input)
    {
        host.basicPlayerMovementEnabled = false;
        host.behaviorDispatchEnabled = false;
        host.StartLevel(50);
        host.UpdateApplication(0, input);
    }

    ApplicationUpdateResult Tap(GameContext& context, Input& input, InputKey key)
    {
        input.Set(key, true);
        const auto result = context.UpdateApplication(0, input);
        input.Set(key, false);
        context.UpdateApplication(0, input);
        return result;
    }

    void Move(GameContext& context, Input& input, InputKey key, int count = 1)
    {
        for (int i = 0; i < count; ++i) { Tap(context, input, key); }
    }

    RawGameObject Item(CollisionType type, std::int16_t value = 0)
    {
        RawGameObject item;
        item.health = 1000;
        item.collisionType = type;
        item.itemValue = value;
        item.generalPurpose1 = 123;
        item.posX = 456;
        item.winZ = 321;
        return item;
    }

    bool Same(const RawGameObject& first, const RawGameObject& second)
    {
        return std::memcmp(&first, &second, sizeof(first)) == 0;
    }

    std::uint8_t FrameShade(const GameContext& context, int cell)
    {
        const float x = 10.f + (cell % 4) * 50.f;
        const float y = cell < 12 ? 10.f + (cell / 4) * 46.f : 182.f;
        for (const auto& command : context.GetApplicationPresentation().commands)
        {
            const auto* picture = std::get_if<UiBitmap>(&command);
            if (picture && picture->source == UiBitmapSource::Hud && picture->animation == 0 &&
                picture->bounds.x == x && picture->bounds.y == y)
            {
                return picture->shade;
            }
        }
        throw std::runtime_error("Missing inventory frame in presentation.");
    }

    void TestFrameTimingAndRoutes()
    {
        Host host;
        Input input;
        Ready(host, input);
        input.calls = 0;
        Check(!host.UpdateApplication(0, input).tick &&
            input.calls == static_cast<int>(InputKey::Count), "Input must be sampled once even on zero-tick frames.");
        Check(!host.UpdateApplication(1.0 / 66, input).tick, "Half a tick must not run gameplay.");
        auto update = host.UpdateApplication(1.0 / 66, input);
        Check(update.tick && update.timing.behaviorTicks == 1, "The core must accumulate 33 Hz gameplay time.");
        Check(host.UpdateApplication(20, input).timing.behaviorTicks == 33, "Catch-up must be capped to one second.");
        host.UpdateApplication(0.9 / 33, input);
        const auto frozenObjects = host.worldObjects;
        const auto frozenRandom = host.randomSeed;
        input.Set(InputKey::InventoryKey, true);
        update = host.UpdateApplication(0, input);
        Check(host.GetMenuPage() == MenuPage::Inventory && !update.tick && update.worldDeltaSeconds == 0,
            "F2 must be interpreted by the core without a simulation tick.");
        for (int i = 0; i < 10; ++i)
        {
            update = host.UpdateApplication(100, input);
            Check(!update.tick && update.worldDeltaSeconds == 0, "Inventory must freeze world time.");
        }
        Check(std::memcmp(host.worldObjects.data(), frozenObjects.data(), sizeof(frozenObjects)) == 0 &&
            host.randomSeed == frozenRandom, "Inventory must not simulate objects or consume gameplay RNG.");
        input.Set(InputKey::InventoryKey, false);
        host.UpdateApplication(0, input);
        Tap(host, input, InputKey::UiCancelKey);
        Check(host.GetMenuPage() == MenuPage::Gameplay, "Escape must leave inventory immediately.");
        Check(!host.UpdateApplication(0.2 / 33, input).tick, "Modal entry must discard fractional world catch-up.");
        update = host.UpdateApplication(0.8 / 33, input);
        Check(update.tick && update.timing.behaviorTicks == 1, "Resume must restart the gameplay accumulator.");
        input.Set(InputKey::MainMenuKey, true);
        input.Set(InputKey::InventoryKey, true);
        host.UpdateApplication(0, input);
        Check(host.GetMenuPage() == MenuPage::MainMenu, "F1 must take precedence over F2.");
        host.UpdateApplication(30, input);
        Check(host.GetMenuPage() == MenuPage::MainMenu, "Held page keys must not toggle pages.");
        input.keys.fill(false);
        host.UpdateApplication(0, input);
        Tap(host, input, InputKey::UiConfirmKey);
        Check(host.GetMenuPage() == MenuPage::Gameplay, "Main-menu Resume must return to gameplay.");
        Tap(host, input, InputKey::MainMenuKey);
        Tap(host, input, InputKey::UiCancelKey);
        Check(host.GetMenuPage() == MenuPage::Gameplay, "Main-menu Escape must resume an active game.");
        host.SetPaused(true);
        Check(!host.UpdateApplication(50, input).tick, "Explicit pause must suppress simulation.");
        Tap(host, input, InputKey::MainMenuKey);
        Check(host.GetMenuPage() == MenuPage::MainMenu, "Application input must work while explicitly paused.");
        Tap(host, input, InputKey::UiCancelKey);
        host.SetPaused(false);
        Check(host.UpdateApplication(1.0 / 33, input).timing.behaviorTicks == 1,
            "Explicit pause must not accumulate catch-up.");
    }

    void TestGatesLoadingAndQuit()
    {
        Host host;
        Input input;
        Tap(host, input, InputKey::UiCancelKey);
        Check(host.GetMenuPage() == MenuPage::MainMenu && !host.SelectMainMenuAction(MainMenuAction::ResumeGame),
            "A host without a level must not resume gameplay.");
        host.synchronous = false;
        const auto request = host.StartLevel(50);
        host.SetMenuPage(MenuPage::Gameplay);
        host.UpdateApplication(0, input);
        Tap(host, input, InputKey::InventoryKey);
        Check(host.GetMenuPage() == MenuPage::Gameplay && !host.UpdateApplication(100, input).tick,
            "Loading must neither enter inventory nor simulate.");
        Tap(host, input, InputKey::MainMenuKey);
        Check(host.GetMenuPage() == MenuPage::MainMenu, "Main-menu input must work during loading.");
        host.CompleteLevelLoad(request.id, Host::MakeLevel());
        host.basicPlayerMovementEnabled = false;
        host.behaviorDispatchEnabled = false;
        host.UpdateApplication(0, input);
        host.positionalSoundsSuppressed = true;
        Tap(host, input, InputKey::InventoryKey);
        Check(host.GetMenuPage() == MenuPage::Gameplay, "The native suppression gate must reject F2.");
        Tap(host, input, InputKey::MainMenuKey);
        Tap(host, input, InputKey::UiConfirmKey);
        Tap(host, input, InputKey::UiCancelKey);
        Check(host.GetMenuPage() == MenuPage::MainMenu, "Suppressed Resume and Escape must remain gated.");
        Move(host, input, InputKey::UiDownKey, 4);
        Tap(host, input, InputKey::UiConfirmKey);
        Check(host.GetMenuPage() == MenuPage::Quit && !host.UpdateApplication(100, input).tick &&
            host.GetApplicationPresentation().commands.empty(), "Quit must be requested by the core without more simulation.");

        for (const auto delta : { -1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN() })
        {
            ExpectError<std::invalid_argument>([&] { host.UpdateApplication(delta, input); });
        }
        host.SetMenuPage(MenuPage::Presentation20);
        ExpectError<std::logic_error>([&] { host.UpdateApplication(0, input); });
        ExpectError<std::logic_error>([&] { host.GetApplicationPresentation(); });
        Check(host.GetMenuPage() == MenuPage::Presentation20, "Future presentation pages must not silently skip.");
    }

    void TestMenuFidelity()
    {
        Host host;
        Input input;
        Ready(host, input);
        Tap(host, input, InputKey::UiCancelKey);
        Check(host.GetMenuPage() == MenuPage::MainMenu, "ESC in gameplay must open the main menu.");
        const auto cursorY = [&]
        {
            for (const auto& command : host.GetApplicationPresentation().commands)
            {
                if (const auto* bitmap = std::get_if<UiBitmap>(&command);
                    bitmap && bitmap->source == UiBitmapSource::MenuCursor) { return bitmap->bounds.y; }
            }
            throw std::runtime_error("Missing menu sword.");
        };
        bool newGame = false, settings = false, disabledLoad = false;
        for (const auto& command : host.GetApplicationPresentation().commands)
        {
            const auto* text = std::get_if<UiText>(&command);
            if (!text) { continue; }
            if (text->text == GetString("menu_new")) { newGame = text->x == 85 && text->y == 76 && text->scale == 1; }
            if (text->text == GetString("menu_settings")) { settings = true; }
            if (text->text == GetString("menu_load")) { disabledLoad = text->color.r < 200; }
            Check(text->text != GetString("menu_title"), "The original crypt menu has no added title.");
        }
        Check(newGame && settings && disabledLoad,
            "The original-style menu must expose Settings, omit Configuration, and disable Load without valid saves.");
        const float initial = cursorY();
        Tap(host, input, InputKey::UiDownKey);
        Check(cursorY() == initial, "Changing rows at zero elapsed time must not teleport the sword.");
        host.UpdateApplication(0.025, input);
        Check(cursorY() > initial && cursorY() < 81, "The sword must move through intermediate row positions.");
        host.UpdateApplication(0.5, input);
        Check(cursorY() == 81, "The sword must settle on the new row without moving the text.");
        Tap(host, input, InputKey::UiDownKey);
        Tap(host, input, InputKey::UiConfirmKey);
        Check(host.GetMenuPage() == MenuPage::MainMenu, "A greyed-out Load action must not open the slot list.");
        host.saves[0] = host.CaptureSaveGame("Available save");
        Tap(host, input, InputKey::UiCancelKey);
        Tap(host, input, InputKey::UiCancelKey);
        Move(host, input, InputKey::UiDownKey, 2);
        Tap(host, input, InputKey::UiConfirmKey);
        Check(host.GetMenuPage() == MenuPage::LoadGame, "A valid save must enable Load on menu reentry.");
        host.SetMenuPage(MenuPage::MainMenu);
        Check(host.SelectMainMenuAction(MainMenuAction::Settings) && host.GetMenuPage() == MenuPage::Settings,
            "Settings must be a reachable screen rather than an unsupported-page error.");
        Tap(host, input, InputKey::UiCancelKey);

        host.screenTransitionsEnabled = true;
        host.SetMenuPage(MenuPage::LoadGame);
        host.UpdateApplication(0.075, input);
        auto presentation = host.GetApplicationPresentation();
        Check(presentation.screenTint.r == 255 && presentation.menuContentOpacity > 0.49f &&
            presentation.menuContentOpacity < 0.51f && presentation.page == MenuPage::MainMenu,
            "Menu navigation must fade foreground content, not the crypt background.");
        host.UpdateApplication(0.075, input);
        presentation = host.GetApplicationPresentation();
        Check(presentation.page == MenuPage::LoadGame && presentation.screenTint.r == 255 &&
            presentation.menuContentOpacity == 0,
            "The menu page must switch with its text invisible and its background fully visible.");
        host.UpdateApplication(0.15, input);
        Check(host.GetApplicationPresentation().menuContentOpacity == 1,
            "The destination menu text must fade fully back in.");

        Host cinematic;
        Input cinematicInput;
        cinematic.synchronous = false;
        const auto cinematicLoad = cinematic.StartLevel(50);
        auto timedLevel = Host::MakeLevel();
        timedLevel.level->nativeInfo[104] = 10;
        cinematic.CompleteLevelLoad(cinematicLoad.id, std::move(timedLevel));
        cinematic.UpdateApplication(0, cinematicInput);
        Tap(cinematic, cinematicInput, InputKey::MainMenuKey);
        bool disabledSave = false;
        for (const auto& command : cinematic.GetApplicationPresentation().commands)
        {
            if (const auto* text = std::get_if<UiText>(&command); text && text->text == GetString("menu_save"))
            {
                disabledSave = text->color.r < 200;
            }
        }
        Move(cinematic, cinematicInput, InputKey::UiDownKey, 3);
        Tap(cinematic, cinematicInput, InputKey::UiConfirmKey);
        Check(cinematic.IsCutscenePlaying() && disabledSave && cinematic.GetMenuPage() == MenuPage::MainMenu,
            "Save must be greyed out and unselectable while a cutscene is paused in the menu.");

        Host book;
        Input bookInput;
        book.StartNewGame(2);
        book.UpdateApplication(0, bookInput);
        Tap(book, bookInput, InputKey::UiConfirmKey);
        book.UpdateApplication(0.3, bookInput);
        Check(book.GetApplicationPresentation().story->turn > 0,
            "The ESC regression must exercise an in-progress paper turn.");
        Tap(book, bookInput, InputKey::UiCancelKey);
        Check(book.GetMenuPage() == MenuPage::Gameplay && book.levelElapsedTicks == 0,
            "ESC must quit the complete book even during a page turn, without starting gameplay simulation.");
    }

    void TestMenuLayoutsAndInventoryNames()
    {
        const auto checkLayout = [](const GameContext& host, std::size_t count, bool heading)
        {
            const bool chapters = host.GetMenuPage() == MenuPage::ChapterSelection;
            std::vector<UiText> rows;
            for (const auto& command : host.GetApplicationPresentation().commands)
            {
                if (const auto* text = std::get_if<UiText>(&command); text && text->scale >= 0.8f)
                {
                    Check(text->x == (chapters ? 75 : 85),
                        "Crypt menu labels must retain their page's original text column.");
                    if (heading && text->scale == 1)
                    {
                        Check(text->y == 16, "Submenu headings must share a common top margin.");
                    }
                    else { rows.push_back(*text); }
                }
            }
            Check(rows.size() == count, "The centered menu must retain all of its rows, including empty save slots.");
            const float center = (rows.front().y + rows.back().y + 16 * rows.back().scale) * 0.5f;
            Check(std::abs(center - (chapters ? 98.4f : heading ? 136.f : 120.f)) < 0.01f,
                "Menu rows must be vertically centered in their content band, not packed against the top.");
            if (chapters)
            {
                for (std::size_t row = 0; row < rows.size(); ++row)
                {
                    Check(rows[row].scale == 0.8f && rows[row].y == 68.f + row * 24.f &&
                        rows[row].text != GetString("menu_new"),
                        "New Game must match the original three-choice layout at Y=68/92/116 without a caption.");
                }
            }
            for (std::size_t row = 1; row < rows.size(); ++row)
            {
                Check(rows[row].y - rows[row - 1].y >= (host.GetMenuPage() == MenuPage::LoadGame ? 15.99f : 17.49f) &&
                    rows[row].y + 16 * rows[row].scale <= (host.GetMenuPage() == MenuPage::LoadGame ? 223 : 222),
                    "Menu rows must remain separated and inside the logical screen.");
            }
        };
        Host cold;
        checkLayout(cold, 5, false);
        Host host;
        Input input;
        Ready(host, input);
        Tap(host, input, InputKey::MainMenuKey);
        Move(host, input, InputKey::UiDownKey);
        Tap(host, input, InputKey::UiConfirmKey);
        checkLayout(host, 3, false);
        for (const auto page : {MenuPage::MainMenu, MenuPage::AlternateMainMenu, MenuPage::LoadGame, MenuPage::SaveGame})
        {
            host.SetMenuPage(page);
            const bool slots = page == MenuPage::LoadGame || page == MenuPage::SaveGame;
            checkLayout(host, page == MenuPage::LoadGame ? 11 : slots ? 10 : 6, slots);
        }
        host.SetMenuPage(MenuPage::Settings);
        checkLayout(host, 1, false);
        host.SetMenuPage(MenuPage::Gameplay);
        Tap(host, input, InputKey::InventoryKey);
        const auto name = [&]() -> std::string
        {
            for (const auto& command : host.GetApplicationPresentation().commands)
            {
                if (const auto* text = std::get_if<UiText>(&command); text && text->x == 20 && text->y == 161)
                {
                    Check(text->layout == UiTextLayout::Native, "Item names must use the original inventory font metrics.");
                    Check(text->color.r == 255 && text->color.g == 192 && text->color.b == 128,
                        "Item names must use the same light-orange tint as prompts.");
                    return text->text;
                }
            }
            return {};
        };
        auto& sword = host.inventory[13];
        sword = Item(CollisionType::Weapon, 24);
        sword.doorLockOrGroupBitmask = 6;
        Check(name() == "Sword", "Inventory names must follow the authored name subtype, not the attack ID.");
        LoadStringResources(std::filesystem::path("Localization") / "strings.json", "de-de");
        Check(name() == "Schwert", "The inventory name under the cursor must use the selected locale.");
        LoadStringResources(std::filesystem::path("Localization") / "strings.json");
        sword.renderPlacementMode = 20;
        sword.spriteScale = 42;
        Check(name() == "Foil", "Placement-20 inventory items must resolve their alternate native name selector.");
        sword.spriteScale = 64;
        Check(name().empty(), "Deliberately unnamed native rows must not invent a label or keep the previous item's name.");
        sword = Item(CollisionType::Key, 3);
        Check(name() == GetString("item_06"), "Ordinary items must resolve names by collision type.");
        sword.health = 0;
        Check(name().empty(), "Empty inventory slots must clear the item-name line.");
        host.inventory[12] = Item(CollisionType::Medicine, 20);
        Tap(host, input, InputKey::UiRightKey);
        Check(name() == GetString("item_02"), "Moving focus must immediately update the selected item's name.");
        Move(host, input, InputKey::UiDownKey, 3);
        Check(name().empty(), "Back and Use cells must not display an inventory-item name.");
    }

    void TestScreenFades()
    {
        Host host;
        Input input;
        Ready(host, input);
        host.screenTransitionsEnabled = true;
        const auto seed = host.randomSeed;
        input.Set(InputKey::InventoryKey, true);
        host.UpdateApplication(0, input);
        Check(host.GetMenuPage() == MenuPage::Gameplay, "Opening inventory must retain gameplay until black.");
        host.UpdateApplication(0.075, input);
        const auto halfway = host.GetApplicationPresentation().screenTint;
        Check(halfway.r >= 127 && halfway.r <= 128 && halfway.g == halfway.r && halfway.b == halfway.r &&
            host.GetMenuPage() == MenuPage::Gameplay && host.randomSeed == seed && host.levelElapsedTicks == 0,
            "Quick fades must darken the outgoing screen without simulating gameplay.");
        input.Set(InputKey::UiCancelKey, true);
        host.UpdateApplication(0.075, input);
        Check(host.GetMenuPage() == MenuPage::Inventory && host.GetApplicationPresentation().screenTint.r == 0,
            "The screen must change only at the black midpoint.");
        host.UpdateApplication(0.15, input);
        host.UpdateApplication(0, input);
        Check(host.GetMenuPage() == MenuPage::Inventory && host.GetApplicationPresentation().screenTint.r == 255,
            "Fade-in must end at exact white and consume input pressed during the transition.");
        input.keys.fill(false);
        host.UpdateApplication(0, input);
        input.Set(InputKey::UiCancelKey, true);
        host.UpdateApplication(0, input);
        host.UpdateApplication(0.15, input);
        Check(host.GetMenuPage() == MenuPage::Gameplay && host.GetApplicationPresentation().screenTint.r == 0,
            "Closing inventory must also cross black before returning to gameplay.");
        host.UpdateApplication(0.15, input);

        Host next;
        Input loadingInput;
        Ready(next, loadingInput);
        next.StartLevel(30);
        next.UpdateApplication(0, loadingInput);
        next.screenTransitionsEnabled = true;
        next.synchronous = false;
        const auto requestCount = next.requests.size();
        next.EndLevel();
        next.UpdateApplication(0.3, loadingInput);
        Check(next.requests.size() == requestCount && next.GetLoadedLevelNumber() == 30,
            "Level changes must not destroy or replace the outgoing world before it fades out.");
        next.UpdateApplication(0.3, loadingInput);
        next.UpdateApplication(1, loadingInput);
        Check(next.GetRuntimeState() == GameRuntimeState::Loading &&
            next.GetApplicationPresentation().screenTint.r == 0,
            "Deferred level loading must hold a fully black screen.");
        auto data = Host::MakeLevel();
        data.level->nativeInfo[103] = 31;
        next.CompleteLevelLoad(next.GetPendingLevelLoad()->id, std::move(data));
        next.UpdateApplication(0.3, loadingInput);
        Check(next.GetMenuPage() == MenuPage::StoryBook && next.GetApplicationPresentation().screenTint.r > 0 &&
            next.GetApplicationPresentation().screenTint.r < 255,
            "The destination story must fade in only after level loading completes.");
        next.UpdateApplication(0.3, loadingInput);
        Check(next.GetApplicationPresentation().screenTint.r == 255,
            "Book fade-in must finish at exact neutral white.");
        for (int page = 0; !next.GetApplicationPresentation().story->next.empty() && page < 50; ++page)
        {
            loadingInput.Set(InputKey::UiConfirmKey, true);
            next.UpdateApplication(0, loadingInput);
            loadingInput.Set(InputKey::UiConfirmKey, false);
            next.UpdateApplication(0.6, loadingInput);
            next.UpdateApplication(0.6, loadingInput);
        }
        loadingInput.Set(InputKey::UiConfirmKey, true);
        next.UpdateApplication(0, loadingInput);
        next.UpdateApplication(0.3, loadingInput);
        Check(next.GetMenuPage() == MenuPage::StoryBook && next.GetApplicationPresentation().screenTint.r < 255,
            "The final book page must remain intact while fading out to gameplay.");
        next.UpdateApplication(0.3, loadingInput);
        Check(next.GetMenuPage() == MenuPage::Gameplay && next.GetApplicationPresentation().screenTint.r == 0,
            "Book-to-level transitions must reveal gameplay only at black.");
        next.UpdateApplication(0.6, loadingInput);

        Host failed;
        Input failedInput;
        Ready(failed, failedInput);
        failed.StartLevel(30);
        failed.UpdateApplication(0, failedInput);
        failed.screenTransitionsEnabled = true;
        failed.fail = true;
        failed.EndLevel();
        ExpectError<std::runtime_error>([&] { failed.UpdateApplication(0.6, failedInput); });
        failed.UpdateApplication(0.6, failedInput);
        Check(failed.GetRuntimeState() == GameRuntimeState::LoadFailed &&
            !failed.GetLevelLoadError().empty() && failed.GetApplicationPresentation().screenTint.r == 255,
            "Deferred transition failures must remain explicit and cannot leave a permanent black mask.");

        Host death;
        Input dyingInput;
        Ready(death, dyingInput);
        death.screenTransitionsEnabled = true;
        death.ApplyPlayerLifeDelta(-death.playerLife);
        dyingInput.Set(InputKey::MainMenuKey, true);
        death.UpdateApplication(1, dyingInput);
        auto tint = death.GetApplicationPresentation().screenTint;
        Check(death.GetMenuPage() == MenuPage::Gameplay && tint.r > tint.g && tint.g > 5,
            "Death must slowly tint the world red and ignore menu input.");
        death.UpdateApplication(1, dyingInput);
        death.UpdateApplication(0.5, dyingInput);
        tint = death.GetApplicationPresentation().screenTint;
        Check(tint.r == 96 && tint.g == 5 && tint.b == 5,
            "Death must reach the specified dark-red endpoint before fading to black.");
        death.UpdateApplication(0.5, dyingInput);
        tint = death.GetApplicationPresentation().screenTint;
        Check(tint.r > 0 && tint.r < 96 && death.GetMenuPage() == MenuPage::Gameplay,
            "The dark-red screen must fade fully out before game cleanup.");
        death.UpdateApplication(0.5, dyingInput);
        Check(death.GetMenuPage() == MenuPage::MainMenu && death.GetRuntimeState() == GameRuntimeState::NoLevel &&
            death.GetApplicationPresentation().screenTint.r == 0 && !death.CanResumeGame(),
            "Death must clear the world and resume/save availability at black.");
        death.UpdateApplication(0.075, dyingInput);
        Check(death.GetApplicationPresentation().screenTint.r == 128,
            "The main menu must reach half brightness halfway through its normal 0.15-second fade-in after death.");
        death.UpdateApplication(0.075, dyingInput);
        Check(death.GetApplicationPresentation().screenTint.r == 255,
            "The main menu must complete its normal-speed fade-in after death.");
    }

    void TestNewGameSelection()
    {
        const auto hasText = [](const GameContext& host, const std::string& value)
        {
            for (const auto& command : host.GetApplicationPresentation().commands)
            {
                if (const auto* text = std::get_if<UiText>(&command); text && text->text == value) { return true; }
            }
            return false;
        };
        const auto focusedPart = [](const GameContext& host, std::size_t part)
        {
            float rowY = -100;
            const auto presentation = host.GetApplicationPresentation();
            for (const auto& command : presentation.commands)
            {
                if (const auto* text = std::get_if<UiText>(&command);
                    text && text->text == GetString(IndexedStringId("chapter_", static_cast<int>(part) + 1)))
                {
                    rowY = text->y;
                }
            }
            for (const auto& command : presentation.commands)
            {
                if (const auto* bitmap = std::get_if<UiBitmap>(&command); bitmap &&
                    bitmap->source == UiBitmapSource::MenuCursor && std::abs(bitmap->bounds.y - rowY - 5) < 0.01f)
                {
                    return true;
                }
            }
            return false;
        };
        for (int part = 0; part < 3; ++part)
        {
            Host host;
            Input input;
            Ready(host, input);
            host.inventory[4] = Item(CollisionType::Key, 123);
            host.gold = 17;
            host.arrowPoison = 20;
            host.hasMap = true;
            host.playerLife = 123 * 65536;
            host.playerLifeMaximum = 200;
            host.playerLifeDeltaSuppressed = 1;
            host.playerHitEffectTicks = 99;
            host.positionalSoundsSuppressed = true;
            host.DEBUG_noPlayerCollision = true;
            host.soundEnabled = false;
            host.playerOxygen = 5;
            host.weaponAmounts.fill(200);
            host.weaponInventorySlots.fill(5);
            host.selectedWeapon = host.requestedWeapon = WeaponType::Fireball;
            host.weaponAnimationSpeed = host.weaponAnimationPosition = 100;
            host.weaponSpriteFrame = host.weaponHand = host.requestedWeaponHand = 2;
            host.weaponElectricityFiring = true;
            host.weaponElectricitySparking = true;
            host.weaponAnimationRecoil = 99;
            host.weaponReady = false;
            host.SetPaused(true);
            Tap(host, input, InputKey::MainMenuKey);
            Check(hasText(host, "New game"), "The main menu must expose New Game.");
            Tap(host, input, InputKey::UiConfirmKey);
            Check(host.GetMenuPage() == MenuPage::ChapterSelection && host.requests.size() == 1 &&
                Same(host.inventory[4], Item(CollisionType::Key, 123)),
                "Entering part selection must not reset or replace the active game.");
            const auto objects = host.worldObjects;
            const auto seed = host.randomSeed;
            Check(!host.UpdateApplication(10, input).tick &&
                std::memcmp(objects.data(), host.worldObjects.data(), sizeof(objects)) == 0 &&
                host.randomSeed == seed, "Part selection must freeze gameplay and RNG.");
            Move(host, input, InputKey::UiUpKey, 4);
            Move(host, input, InputKey::UiDownKey, part);
            constexpr std::array<int, 3> starts{ 50, 30, 10 };
            host.UpdateApplication(0.5, input);
            Check(hasText(host, GetString("chapter_01")) && hasText(host, GetString("chapter_02")) &&
                hasText(host, GetString("chapter_03")) && focusedPart(host, static_cast<std::size_t>(part)) &&
                !hasText(host, "Starts at level " + std::to_string(starts[part])),
                "Part selection must use localized labels with the HERZ order and omit the level-start prompt.");
            input.Set(InputKey::AttackKey, true);
            input.Set(InputKey::DebugNextLevelKey, true);
            const auto transition = Tap(host, input, InputKey::UiConfirmKey);
            Check(!transition.tick && transition.worldDeltaSeconds == 0 && transition.timing.behaviorTicks == 0 &&
                host.GetLoadedLevelNumber() == starts[part] && host.requests.size() == 2 &&
                host.GetRuntimeState() == GameRuntimeState::Ready &&
                host.GetMenuPage() == (part == 0 ? MenuPage::Gameplay : MenuPage::StoryBook) &&
                !host.IsPaused(), "Part acceptance must load once, unpause, and not simulate the transition frame.");
            Check(host.inventory[4].health == 0 && host.gold == 0 && host.arrowPoison == 0 && !host.hasMap &&
                host.playerLifeMaximum == 360 && host.playerLife == 360 * 65536 &&
                host.playerLifeDeltaSuppressed == 0 && host.playerHitEffectTicks == 0 &&
                host.playerOxygen == GameContext::PlayerOxygenMaximum && host.healthNotifications == 1 &&
                host.selectedWeapon == WeaponType::None && host.requestedWeapon == WeaponType::None &&
                host.weaponSpriteFrame == 0 && host.weaponHand == 0 && host.requestedWeaponHand == 0 &&
                host.weaponAnimationSpeed == 0 && host.weaponAnimationPosition == 0 && host.weaponAnimationRecoil == 0 &&
                host.weaponReady && !host.weaponElectricityFiring && !host.weaponElectricitySparking &&
                !host.positionalSoundsSuppressed,
                "New Game must reset carried inventory, money, poison, maps, vital meters and weapon commands.");
            Check(host.DEBUG_noPlayerCollision && !host.soundEnabled &&
                !host.basicPlayerMovementEnabled && !host.behaviorDispatchEnabled,
                "New Game must preserve host debug, audio and simulation preferences.");
            const GameContext defaults;
            Check(host.weaponAmounts == defaults.weaponAmounts &&
                host.weaponInventorySlots == defaults.weaponInventorySlots,
                "New Game must restore native resource defaults, not carry ammunition from the old game.");
            for (int page = 0; (host.GetMenuPage() == MenuPage::StoryBook ||
                host.GetMenuPage() == MenuPage::CastleIntroduction) && page < 100; ++page)
            {
                Check(!host.UpdateApplication(0.1, input).tick,
                    "The introduction must pause simulation before the starting level.");
                Tap(host, input, InputKey::UiCancelKey);
                if (host.GetMenuPage() == MenuPage::StoryBook)
                {
                    host.UpdateApplication(0.6, input);
                    host.UpdateApplication(0.6, input);
                }
            }
            Check(host.GetMenuPage() == MenuPage::Gameplay, "The complete introduction must finish in gameplay.");
            Check(host.UpdateApplication(1.0 / 33, input).tick.has_value() && host.requests.size() == 2 &&
                host.GetLoadedLevelNumber() == starts[part],
                "The next frame must resume simulation without a held Tab skipping the chosen starting level.");
            input.keys.fill(false);
            host.UpdateApplication(0, input);
            if (part == 2)
            {
                for (int i = 0; i < 6; ++i) { Tap(host, input, InputKey::DebugNextLevelKey); }
                Check(host.GetLoadedLevelNumber() == 16,
                    "Completing the HERZ introduction followed by six debug advances must reach the forest.");
            }
        }
        Host cancelled;
        Input input;
        Ready(cancelled, input);
        cancelled.inventory[0] = Item(CollisionType::Medicine, 42);
        Tap(cancelled, input, InputKey::MainMenuKey);
        Move(cancelled, input, InputKey::UiDownKey);
        Tap(cancelled, input, InputKey::UiConfirmKey);
        Move(cancelled, input, InputKey::UiDownKey, 5);
        cancelled.UpdateApplication(0.5, input);
        Check(focusedPart(cancelled, 2), "Part navigation must stop at the final HERZ part.");
        Tap(cancelled, input, InputKey::UiCancelKey);
        Check(cancelled.GetMenuPage() == MenuPage::MainMenu && cancelled.requests.size() == 1 &&
            Same(cancelled.inventory[0], Item(CollisionType::Medicine, 42)),
            "Cancelling selection must return to the menu without losing progress.");
        Move(cancelled, input, InputKey::UiDownKey);
        input.Set(InputKey::UiConfirmKey, true);
        cancelled.UpdateApplication(0, input);
        cancelled.UpdateApplication(1, input);
        Check(cancelled.GetMenuPage() == MenuPage::ChapterSelection && cancelled.requests.size() == 1 &&
            focusedPart(cancelled, 0),
            "Selection reentry must reset focus and held Enter must not immediately start the highlighted part.");
        input.keys.fill(false);
        cancelled.UpdateApplication(0, input);

        Host deferred;
        deferred.basicPlayerMovementEnabled = false;
        deferred.behaviorDispatchEnabled = false;
        deferred.synchronous = false;
        deferred.SetMenuPage(MenuPage::ChapterSelection);
        Input pendingInput;
        deferred.UpdateApplication(0, pendingInput);
        Tap(deferred, pendingInput, InputKey::UiConfirmKey);
        Check(deferred.GetRuntimeState() == GameRuntimeState::Loading &&
            deferred.GetPendingLevelLoad()->levelNumber == 50 && !deferred.hasActiveGame &&
            !deferred.UpdateApplication(1, pendingInput).tick,
            "Deferred New Game must remain loading, not report a ready active game.");
        deferred.SetMenuPage(MenuPage::MainMenu);
        Check(!deferred.SelectMainMenuAction(MainMenuAction::NewGame),
            "New Game must be disabled while a load is pending.");
        const auto request = *deferred.GetPendingLevelLoad();
        deferred.inventory[0] = Item(CollisionType::Medicine, 42);
        deferred.gold = 9;
        ExpectError<std::logic_error>([&] { deferred.StartNewGame(2); });
        Check(deferred.requests.size() == 1 && deferred.GetPendingLevelLoad()->id == request.id &&
            Same(deferred.inventory[0], Item(CollisionType::Medicine, 42)) && deferred.gold == 9,
            "A pending load must not be overwritten or reset by another new game.");
        deferred.CompleteLevelLoad(request.id, Host::MakeLevel());
        Check(deferred.GetRuntimeState() == GameRuntimeState::Ready && deferred.hasActiveGame &&
            deferred.GetMenuPage() == MenuPage::Gameplay, "Deferred new-game completion must enter gameplay.");

        Host failed;
        Input failureInput;
        failed.fail = true;
        failed.SetMenuPage(MenuPage::ChapterSelection);
        failed.UpdateApplication(0, failureInput);
        ExpectError<std::runtime_error>([&] { Tap(failed, failureInput, InputKey::UiConfirmKey); });
        Check(failed.GetRuntimeState() == GameRuntimeState::LoadFailed &&
            failed.GetMenuPage() == MenuPage::ChapterSelection && !failed.hasActiveGame &&
            !failed.GetPendingLevelLoad() && !failed.GetLevelLoadError().empty(),
            "Failed New Game must expose its load error and not transition to successful gameplay.");
        for (const auto part : { -1, 0, 4 })
        {
            ExpectError<std::invalid_argument>([&] { failed.StartNewGame(static_cast<std::int16_t>(part)); });
        }
        failed.fail = false;
        failed.reportFailure = true;
        ExpectError<std::runtime_error>([&] { failed.StartNewGame(1); });
        Check(failed.GetMenuPage() == MenuPage::ChapterSelection && !failed.hasActiveGame &&
            failed.GetLevelLoadError() == "New-game fixture reported failure.",
            "A host-reported synchronous failure must not look like successful New Game.");
    }

    void TestSaveLoadMenus()
    {
        Host host;
        Input input;
        Ready(host, input);
        host.gold = 23;
        host.playerLife = 123 * 65536 + 17;
        const auto contains = [&host](std::string_view value)
        {
            for (const auto& command : host.GetApplicationPresentation().commands)
            {
                if (const auto* text = std::get_if<UiText>(&command); text && text->text.find(value) != std::string::npos)
                {
                    return true;
                }
            }
            return false;
        };
        const auto open = [&](int row)
        {
            Tap(host, input, InputKey::MainMenuKey);
            Move(host, input, InputKey::UiDownKey, row);
            Tap(host, input, InputKey::UiConfirmKey);
        };
        open(3);
        const auto row = [&host]()
        {
            for (const auto& command : host.GetApplicationPresentation().commands)
            {
                if (const auto* text = std::get_if<UiText>(&command); text && text->x == 85 && text->scale == 0.8f)
                {
                    return *text;
                }
            }
            throw std::runtime_error("Missing native save row.");
        };
        Check(host.GetMenuPage() == MenuPage::SaveGame && row().text.empty() && !contains("Empty"),
            "Unused slots must be blank native rows, without labels or numbering.");
        const auto seed = host.randomSeed;
        Check(!host.UpdateApplication(1, input).tick && host.randomSeed == seed,
            "Save/load menus must freeze the world and RNG.");
        Tap(host, input, InputKey::UiConfirmKey);
        Check(row().text.empty() && row().caret, "Empty slots must start editing in place with a visible caret.");
        host.UpdateApplication(0.6, input);
        Check(!row().caret, "The inline caret must blink while the world remains paused.");
        host.QueueTextInput(std::string(20, '\b') + "River gate");
        host.UpdateApplication(0, input);
        Check(row().text == "River gate" && row().caret,
            "Save names must replace their row in place, with a separate caret.");
        host.QueueTextInput(std::string(20, '\b'));
        host.UpdateApplication(0, input);
        Check(row().text.empty() && row().caret, "Erasing every character must keep a valid empty editor.");
        Tap(host, input, InputKey::UiConfirmKey);
        Check(contains("Enter a save name.") && !host.saves[0],
            "An empty name must report validation without saving or leaving the editor.");
        host.QueueTextInput("River gate");
        host.UpdateApplication(0, input);
        const auto savedFrame = Tap(host, input, InputKey::UiConfirmKey);
        Check(host.GetMenuPage() == MenuPage::Gameplay && !savedFrame.tick && host.saves[0] &&
            NativeSaveGame::Decode(*host.saves[0]).Name() == "River gate",
            "Confirming a name must write a native SPL and return without simulating a transition frame.");
        const auto original = *host.saves[0];
        host.gold = 99;
        host.playerLife = 42 * 65536;
        open(2);
        Check(host.GetMenuPage() == MenuPage::LoadGame && contains("River gate"),
            "Load slots must show names read from SPL headers.");
        const auto loadedFrame = Tap(host, input, InputKey::UiConfirmKey);
        Check(!loadedFrame.tick && host.GetMenuPage() == MenuPage::Gameplay &&
            host.GetRuntimeState() == GameRuntimeState::Ready && host.gold == 23 &&
            host.playerLife == 123 * 65536 + 17,
            "Loading must restore saved progress even when reloading the same level.");
        open(3);
        Tap(host, input, InputKey::UiConfirmKey);
        host.QueueTextInput(std::string(20, '\b') + "Cancelled");
        host.UpdateApplication(0, input);
        Tap(host, input, InputKey::UiCancelKey);
        Check(host.GetMenuPage() == MenuPage::SaveGame && *host.saves[0] == original,
            "Escape while editing must not overwrite an existing slot.");
        Tap(host, input, InputKey::UiConfirmKey);
        host.QueueTextInput(std::string(20, '\b') + std::string(30, 'A'));
        host.UpdateApplication(0, input);
        Check(row().text == std::string(NativeSaveGame::NameCapacity, 'A') && row().caret,
            "Save names must stop at the original 20-byte limit.");
        host.writeFails = true;
        Tap(host, input, InputKey::UiConfirmKey);
        Check(host.GetMenuPage() == MenuPage::SaveGame && contains("Test disk is full.") &&
            *host.saves[0] == original, "Save failures must be visible and preserve the previous slot.");
        host.writeFails = false;
        Tap(host, input, InputKey::UiConfirmKey);
        Check(host.GetMenuPage() == MenuPage::Gameplay &&
            NativeSaveGame::Decode(*host.saves[0]).Name() == std::string(NativeSaveGame::NameCapacity, 'A'),
            "Confirming an edited existing slot must overwrite immediately without another prompt.");
        open(3);
        Tap(host, input, InputKey::UiConfirmKey);
        Tap(host, input, InputKey::UiCancelKey);
        Tap(host, input, InputKey::UiCancelKey);
        Tap(host, input, InputKey::UiCancelKey);
        Check(host.GetMenuPage() == MenuPage::Gameplay, "Cancelling save UI must return through the main menu.");
        host.saves[1] = original;
        host.saves[1]->back() ^= 1;
        open(2);
        Move(host, input, InputKey::UiDownKey);
        const auto requests = host.requests.size();
        Tap(host, input, InputKey::UiConfirmKey);
        Check(contains("checksum mismatch") && host.requests.size() == requests &&
            host.GetMenuPage() == MenuPage::LoadGame && host.gold == 23,
            "Corrupt slots must report an error without starting a load or replacing the current game.");
        Move(host, input, InputKey::UiDownKey);
        Tap(host, input, InputKey::UiConfirmKey);
        Check(contains("This slot is empty."), "An empty slot must not pretend to load successfully.");
    }

    void TestQuickSaveLoad()
    {
        Host host;
        Input input;
        Ready(host, input);
        const auto quick = GameContext::QuickSaveSlot - 1;
        host.gold = 23;
        host.saves[0] = host.CaptureSaveGame("Manual save");
        const auto manual = *host.saves[0];
        const auto feedback = [&host](std::string_view value) -> std::optional<UiText>
        {
            for (const auto& command : host.GetApplicationPresentation().commands)
            {
                if (const auto* text = std::get_if<UiText>(&command); text && text->text == value) { return *text; }
            }
            return {};
        };
        host.prompt = "Puzzle prompt";
        input.Set(InputKey::QuickSaveKey, true);
        host.UpdateApplication(0, input);
        host.gold = 99;
        host.UpdateApplication(0, input);
        Check(host.saveWrites == 1 && host.saves[quick] && *host.saves[0] == manual &&
            NativeSaveGame::Decode(*host.saves[quick]).Name() == "Quicksave" && host.prompt == "Puzzle prompt",
            "F5 must write one ordinary native SPL with the localized title, without repeating or replacing manual saves.");
        const auto savedFeedback = feedback("Quicksaved.");
        Check(savedFeedback && savedFeedback->alignment == UiTextAlignment::Left &&
            savedFeedback->layout == UiTextLayout::ViewportTopLeft &&
            savedFeedback->x > 0 && savedFeedback->x <= 2 &&
            savedFeedback->y > 0 && savedFeedback->y <= 2 &&
            savedFeedback->scale < 0.5f && savedFeedback->color.r == 255 &&
            savedFeedback->color.g >= 150 && savedFeedback->color.g < savedFeedback->color.r &&
            savedFeedback->color.b < savedFeedback->color.g,
            "Quicksave must show small bright-orange text close to the top-left edge without replacing an active gameplay prompt.");
        input.Set(InputKey::QuickSaveKey, false);
        host.UpdateApplication(0, input);
        const auto loaded = Tap(host, input, InputKey::QuickLoadKey);
        Check(!loaded.tick && host.GetMenuPage() == MenuPage::Gameplay && host.gold == 23,
            "F6 must reload the quicksave even in the same level without simulating the transition frame.");
        Check(feedback("Quickloaded.") && !feedback("Quicksaved."),
            "Quickload must replace the corner notification after rebuilding the loaded scene.");
        host.gold = 45;
        Tap(host, input, InputKey::QuickSaveKey);
        host.gold = 67;
        Tap(host, input, InputKey::QuickLoadKey);
        Check(host.gold == 45 && host.saveWrites == 2, "A new F5 press must replace the previous quicksave.");

        host.saves[0] = host.saves[quick];
        host.SetMenuPage(MenuPage::LoadGame);
        Tap(host, input, InputKey::UiConfirmKey);
        Check(host.gold == 45, "A quicksave copied into a manual slot must load as an ordinary SPL.");
        host.gold = 78;
        host.saves[quick] = host.CaptureSaveGame("Imported DOS save");
        host.saves[0].reset();
        host.gold = 99;
        host.SetMenuPage(MenuPage::MainMenu);
        Move(host, input, InputKey::UiDownKey, 2);
        Tap(host, input, InputKey::UiConfirmKey);
        Check(host.GetMenuPage() == MenuPage::LoadGame,
            "A quicksave alone must enable the main menu's Load game action.");
        const auto quickLabel = [&host]()
        {
            std::vector<UiText> rows;
            for (const auto& command : host.GetApplicationPresentation().commands)
            {
                if (const auto* text = std::get_if<UiText>(&command); text && text->scale == 0.8f)
                {
                    rows.push_back(*text);
                }
            }
            Check(rows.size() == GameContext::QuickSaveSlot, "The load menu must include all ten manual slots plus quicksave.");
            return rows.back().text;
        };
        Check(quickLabel() == "Quicksave", "An imported SPL in quicksav.spl must still be labeled Quicksave.");
        LoadStringResources(std::filesystem::path("Localization") / "strings.json", "de-de");
        Check(quickLabel() == "Schnellspeicherung", "The quicksave label must follow the UI locale, not its stored title.");
        LoadStringResources(std::filesystem::path("Localization") / "strings.json");
        Move(host, input, InputKey::UiDownKey, 10);
        Tap(host, input, InputKey::UiConfirmKey);
        Check(host.gold == 78 && host.GetMenuPage() == MenuPage::Gameplay,
            "The extra load-menu row must load arbitrary native save contents from the quick slot.");
        host.SetMenuPage(MenuPage::MainMenu);
        host.gold = 90;
        Tap(host, input, InputKey::QuickLoadKey);
        Check(host.gold == 78 && host.GetMenuPage() == MenuPage::Gameplay, "F6 must also work from the main menu.");
        LoadStringResources(std::filesystem::path("Localization") / "strings.json", "de-de");
        Tap(host, input, InputKey::QuickSaveKey);
        Check(NativeSaveGame::Decode(*host.saves[quick]).Name() == "Schnellspeicherung",
            "Quicksaves must store the localized native save title.");
        Check(feedback("Schnell gespeichert.").has_value(), "Quicksave feedback must follow the UI locale.");
        LoadStringResources(std::filesystem::path("Localization") / "strings.json");
        host.synchronous = false;
        Tap(host, input, InputKey::QuickLoadKey);
        host.UpdateApplication(5, input);
        Check(!feedback("Quickloaded."), "Quickload feedback must wait until the loaded world is ready.");
        const auto request = host.requests.back();
        host.synchronous = true;
        host.LoadLevel(request);
        Check(feedback("Quickloaded.").has_value(), "Loading time must not consume the quickload notification's lifetime.");
        host.SetPaused(true);
        host.UpdateApplication(1, input);
        host.UpdateApplication(0.9, input);
        Check(feedback("Quickloaded.").has_value(), "Corner notifications must remain visible for about two seconds.");
        host.UpdateApplication(0.2, input);
        Check(!feedback("Quickloaded."), "Corner notifications must expire even when gameplay is paused.");
    }

    void TestQuickSaveFailures()
    {
        Host host;
        Input input;
        Ready(host, input);
        const auto quick = GameContext::QuickSaveSlot - 1;
        Tap(host, input, InputKey::QuickSaveKey);
        const auto saved = *host.saves[quick];
        host.writeFails = true;
        Tap(host, input, InputKey::QuickSaveKey);
        Check(*host.saves[quick] == saved && host.prompt == "Test disk is full.",
            "A failed quicksave must preserve the previous file and visibly report the failure.");
        for (const auto& command : host.GetApplicationPresentation().commands)
        {
            const auto* text = std::get_if<UiText>(&command);
            Check(!text || text->text != "Quicksaved.", "A failed quicksave must clear any previous success notification.");
        }
        host.writeFails = false;
        const auto healthyLife = host.playerLife;
        const auto writesBeforeDeath = host.saveWrites;
        for (const auto life : {0, -65536})
        {
            host.playerLife = life;
            host.prompt.clear();
            Tap(host, input, InputKey::QuickSaveKey);
            Check(host.saveWrites == writesBeforeDeath && *host.saves[quick] == saved &&
                host.prompt == GetString("save_unavailable"),
                "F5 must not write or overwrite a save when player life is zero or negative.");
        }
        host.playerLife = healthyLife;
        host.saves[quick]->back() ^= 1;
        const auto requests = host.requests.size();
        Tap(host, input, InputKey::QuickLoadKey);
        bool checksumError = false;
        for (const auto& command : host.GetApplicationPresentation().commands)
        {
            if (const auto* text = std::get_if<UiText>(&command);
                text && text->text.find("checksum mismatch") != std::string::npos) { checksumError = true; }
        }
        Check(checksumError && host.requests.size() == requests && host.GetMenuPage() == MenuPage::LoadGame,
            "A corrupt quicksave must show its error without replacing the current game.");
        host.saves[quick].reset();
        host.SetMenuPage(MenuPage::MainMenu);
        Tap(host, input, InputKey::QuickLoadKey);
        bool emptyError = false;
        for (const auto& command : host.GetApplicationPresentation().commands)
        {
            if (const auto* text = std::get_if<UiText>(&command); text && text->text == GetString("save_empty"))
            {
                emptyError = true;
            }
        }
        Check(emptyError && host.requests.size() == requests, "F6 without a file must visibly report an empty quicksave.");
        host.saves[quick] = saved;
        host.SetMenuPage(MenuPage::Gameplay);
        host.BeginCreviceFall(host.worldObjects[2]);
        const auto writes = host.saveWrites;
        Tap(host, input, InputKey::QuickSaveKey);
        Check(host.saveWrites == writes && *host.saves[quick] == saved && host.prompt == GetString("save_unavailable"),
            "F5 during a cutscene must preserve the previous quicksave without writing.");
    }

    void TestInventorySelectionAndNavigation()
    {
        Host host;
        Input input;
        Ready(host, input);
        const auto key = Item(CollisionType::Key, 7);
        const auto other = Item(CollisionType::Medicine, 42);
        host.inventory[13] = key;
        host.inventory[12] = other;
        Tap(host, input, InputKey::InventoryKey);
        Move(host, input, InputKey::UiUpKey);
        Move(host, input, InputKey::UiLeftKey);
        Check(FrameShade(host, 0) == 0, "Inventory navigation must stop at top and left edges.");
        Tap(host, input, InputKey::UiConfirmKey);
        Check(Same(host.inventory[13], key), "Selection must retain the complete source record in its slot.");
        Move(host, input, InputKey::UiRightKey);
        Tap(host, input, InputKey::UiConfirmKey);
        Check(Same(host.inventory[12], key) && Same(host.inventory[13], other),
            "Enter must swap complete inventory records.");
        Tap(host, input, InputKey::UiConfirmKey);
        Tap(host, input, InputKey::UiConfirmKey);
        Check(Same(host.inventory[12], key) && FrameShade(host, 1) == 0, "A self-swap must clear selection.");
        Tap(host, input, InputKey::UiConfirmKey);
        Move(host, input, InputKey::UiRightKey);
        Move(host, input, InputKey::UiDownKey, 3);
        Tap(host, input, InputKey::UiConfirmKey);
        Check(Same(host.inventory[0], key) && host.inventory[12].health == 0,
            "Keys must be allowed in the visually left hand.");
        Move(host, input, InputKey::UiRightKey, 2);
        Move(host, input, InputKey::UiDownKey);
        Check(FrameShade(host, 15) == 0, "Inventory navigation must stop at bottom and right edges.");
        Move(host, input, InputKey::UiLeftKey, 3);
        Tap(host, input, InputKey::UiConfirmKey);
        Check(host.GetMenuPage() == MenuPage::Gameplay, "The Back cell must exit inventory.");
        Tap(host, input, InputKey::InventoryKey);
        Check(FrameShade(host, 0) == 0, "Inventory entry must reset focus and selection.");
        Tap(host, input, InputKey::UiConfirmKey);
        Tap(host, input, InputKey::UiCancelKey);
        Check(host.GetMenuPage() == MenuPage::Gameplay, "Escape must exit even with a selected item.");
    }

    void TestRepeatPulseAndInputLeakage()
    {
        Host host;
        Input input;
        Ready(host, input);
        host.inventory[13] = Item(CollisionType::Weapon, static_cast<std::int16_t>(WeaponType::Dagger));
        host.weaponAmounts[1] = 360;
        host.RefreshWeaponInventory();
        input.Set(InputKey::InventoryKey, true);
        input.Set(InputKey::UiConfirmKey, true);
        host.UpdateApplication(0, input);
        host.UpdateApplication(0.2, input);
        Check(FrameShade(host, 0) == 0, "Held Enter from gameplay must not select an inventory source.");
        input.keys.fill(false);
        host.UpdateApplication(0, input);
        Tap(host, input, InputKey::UiConfirmKey);
        for (int i = 1; i <= 512; ++i)
        {
            host.UpdateApplication(1.0 / 33, input);
            const auto phase = (i * 17) & 511;
            const int state = static_cast<int>(std::nearbyint(std::sin(phase * 0.012271846484375) * 1.9 + 2.0));
            Check(FrameShade(host, 0) == state,
                "The core must advance the selected-source pulse independently of world ticks.");
        }
        input.Set(InputKey::UiRightKey, true);
        host.UpdateApplication(0, input);
        Check(FrameShade(host, 1) == 0, "Navigation must act on the first press.");
        host.UpdateApplication(0.34, input);
        Check(FrameShade(host, 1) == 0, "Held navigation must observe the initial repeat delay.");
        host.UpdateApplication(0.02, input);
        Check(FrameShade(host, 2) == 0, "Held navigation must repeat after the delay.");
        host.UpdateApplication(0.11, input);
        Check(FrameShade(host, 3) == 0, "Navigation repeat must continue at the shorter interval.");
        input.keys.fill(false);
        input.Set(InputKey::SelectDaggerKey, true);
        Tap(host, input, InputKey::UiCancelKey);
        auto update = host.UpdateApplication(1.0 / 33, input);
        Check(update.tick && host.inventory[1].health == 0 && host.inventory[13].health == 1000,
            "Keys held while closing UI must not leak into gameplay.");
        input.Set(InputKey::SelectDaggerKey, false);
        host.UpdateApplication(0, input);
        input.Set(InputKey::SelectDaggerKey, true);
        host.UpdateApplication(1.0 / 33, input);
        Check(host.inventory[1].itemValue == static_cast<std::int16_t>(WeaponType::Dagger),
            "Releasing and pressing a blocked gameplay key must re-enable it.");
    }

    void UseSelected(GameContext& host, Input& input)
    {
        Move(host, input, InputKey::UiDownKey, 3);
        Move(host, input, InputKey::UiRightKey);
        Tap(host, input, InputKey::UiConfirmKey);
    }

    void TestPotionCompatibility()
    {
        constexpr int dose = 29491;
        struct Case { int gap; std::int16_t quantity; int healed; std::int16_t remaining; bool suppressed; };
        const Case cases[] = {
            { 0, 5, 0, 5, false }, { 1, 5, 1, 5, false }, { dose, 5, dose, 5, false },
            { dose * 2, 5, dose * 2, 4, false }, { dose * 10, 2, dose * 3, 0, false },
            { dose, 0, 0, 0, false }, { dose * 2, 2, 0, 0, true }
        };
        for (const auto& test : cases)
        {
            Host host;
            Input input;
            Ready(host, input);
            host.inventory[13] = Item(CollisionType::Medicine, test.quantity);
            host.playerLife = host.playerLifeMaximum * 65536 - test.gap;
            const auto previousLife = host.playerLife;
            host.playerLifeDeltaSuppressed = test.suppressed ? 1 : 0;
            Tap(host, input, InputKey::InventoryKey);
            Tap(host, input, InputKey::UiConfirmKey);
            UseSelected(host, input);
            Check(host.playerLife == previousLife + test.healed &&
                host.inventory[13].itemValue == test.remaining, "Medicine must preserve native inclusive-clamp/refund arithmetic.");
            Check((host.healthNotifications > 0) == (test.healed > 0), "Potion healing must use player-life notifications.");
            if (test.remaining == 0)
            {
                Check(Same(host.inventory[13], RawGameObject{}), "Empty medicine must clear the entire item record.");
            }
            Check(FrameShade(host, 0) == 2, "Handled medicine must clear source selection.");
        }
        Host host;
        Input input;
        Ready(host, input);
        host.inventory[13] = Item(CollisionType::Key);
        Tap(host, input, InputKey::InventoryKey);
        Tap(host, input, InputKey::UiConfirmKey);
        UseSelected(host, input);
        host.UpdateApplication(4.0 / 33, input);
        Check(FrameShade(host, 0) == 3, "Using a nonmedicine item must retain its selection.");
        Move(host, input, InputKey::UiRightKey);
        Tap(host, input, InputKey::UiConfirmKey);
        Check(host.inventory[0].collisionType == CollisionType::Key, "Nonmedicine Use must leave the source available for moving.");
    }

    void TestPresentationAndHandReference()
    {
        Host host;
        Input input;
        Ready(host, input);
        auto dagger = Item(CollisionType::Weapon, 1);
        dagger.modelId = 39;
        dagger.modelAnim = 2;
        dagger.animFrame = 1;
        dagger.pickupType = static_cast<PickupType>(22);
        dagger.displayType = DisplayType::VectorNormal;
        auto sword = dagger;
        sword.itemValue = 2;
        sword.animFrame = 2;
        sword.pickupType = static_cast<PickupType>(40);
        auto sprite = Item(CollisionType::Medicine, 25);
        sprite.modelId = 1;
        sprite.modelAnim = 2;
        sprite.pickupType = static_cast<PickupType>(2);
        auto bitmap = dagger;
        bitmap.pickupType = static_cast<PickupType>(1);
        host.inventory[13] = dagger;
        host.inventory[12] = sword;
        host.inventory[11] = sprite;
        host.inventory[10] = bitmap;
        Tap(host, input, InputKey::InventoryKey);
        int models = 0, bitmaps = 0;
        for (const auto& command : host.GetApplicationPresentation().commands)
        {
            if (const auto* model = std::get_if<UiModel>(&command))
            {
                if (model->modelId == 31)
                {
                    Check(model->animation == 5 && model->frame == 3 && model->depth == 210,
                        "Electricity must use the native shared UI model frame.");
                    continue;
                }
                Check(model->modelId == 39 && model->animation == 1 &&
                    model->depth == (model->frame == 0 ? 94 : 130) &&
                    model->angles == std::array<std::int16_t, 3>{ 30, -60, 30 },
                    "Model recipe must retain original frame, fixed pose and byte-50 preview distance.");
                ++models;
            }
            if (const auto* picture = std::get_if<UiBitmap>(&command))
            {
                if (picture->source == UiBitmapSource::LevelSprite)
                {
                    Check(picture->assetSlot == 1 && picture->animation == 1 && picture->frame == 1 &&
                        picture->layout == UiBitmapLayout::SpriteAnchor && picture->bounds.x == 131 && picture->bounds.y == 29,
                        "Sprite recipe must use the byte-50 image selector.");
                    ++bitmaps;
                }
                if (picture->source == UiBitmapSource::ModelTexture)
                {
                    Check(picture->assetSlot == 39 && picture->frame == 1 && picture->layout == UiBitmapLayout::Center,
                        "Model bitmap selectors are direct texture slots, not one-based animation frames.");
                    ++bitmaps;
                }
            }
        }
        Check(models == 2 && bitmaps == 2, "Inventory presentation must include bitmap and actual model paths.");
        host.inventory[13] = Item(CollisionType::Weapon, static_cast<std::int16_t>(WeaponType::TrapOrPole));
        host.selectedWeapon = WeaponType::TrapOrPole;
        host.weaponHand = 14;
        Tap(host, input, InputKey::UiConfirmKey);
        Move(host, input, InputKey::UiDownKey, 3);
        Move(host, input, InputKey::UiRightKey, 2);
        Tap(host, input, InputKey::UiConfirmKey);
        Check(host.weaponHand == 1 && host.weaponInventorySlots[6] == 1,
            "Inventory swaps must keep the placeable weapon reference and registry attached to their record.");
    }

    void TestMapFlowAndDirtyWrites()
    {
        Host host;
        Input input;
        Ready(host, input);
        Check(host.collisionmapUploadDirty, "A new level must require its first map upload.");
        host.worldObjects[1].posX = 1112;
        host.worldObjects[1].posZ = 3088;
        host.worldObjects[1].collisionRadiusXZ = host.worldObjects[1].collisionRadiusY = 30;
        host.worldObjects[1].collisionMask = 1;
        Check(host.GetApplicationPresentation().commands.empty(), "The corner map must require ownership.");
        auto pickup = Item(CollisionType::PickableMap);
        Check(HandlePlayerObjectInteraction(pickup, host).status == InteractionStatus::Applied &&
            host.hasMap && pickup.health == 0, "Picking up the map must enable the corner view.");
        const auto corner = host.GetApplicationPresentation();
        const auto* map = std::get_if<UiMap>(&corner.commands.back());
        Check(corner.page == MenuPage::Gameplay && map && !map->overview &&
            map->bounds.x == 289 && map->bounds.y == 169 && map->bounds.width == 71 &&
            map->bounds.height == 69 && map->centerColumn == 40 && map->centerRow == 40,
            "The live corner map must be clipped and centered using native world-to-map coordinates.");
        auto& picture = host.worldObjects[2];
        picture = host.worldObjects[1];
        picture.collisionType = CollisionType::PictureMap;
        host.objectCollisionsEnabled = true;
        input.Set(InputKey::InteractionKey, true);
        input.Set(InputKey::UiConfirmKey, true);
        const auto opened = host.UpdateApplication(1.0 / 33, input);
        Check(opened.tick && opened.tick->interaction &&
            opened.tick->interaction->status == InteractionStatus::Applied &&
            host.GetMenuPage() == MenuPage::Map && opened.worldDeltaSeconds == 0 &&
            picture.health == 1000 && host.hasMap,
            "Enter on a wall map must open page 11 without consuming the object or advancing world time.");
        const auto objects = host.worldObjects;
        const auto seed = host.randomSeed;
        for (int i = 0; i < 10; ++i)
        {
            const auto update = host.UpdateApplication(1, input);
            Check(!update.tick && update.worldDeltaSeconds == 0 && host.GetMenuPage() == MenuPage::Map &&
                host.randomSeed == seed &&
                std::memcmp(objects.data(), host.worldObjects.data(), sizeof(objects)) == 0,
                "The map must freeze gameplay and ignore the held opening Enter key.");
        }
        const auto modal = host.GetApplicationPresentation();
        const auto* overview = std::get_if<UiMap>(&modal.commands[1]);
        Check(static_cast<int>(MenuPage::Map) == 11 && overview && overview->overview &&
            overview->bounds.width == 360 && overview->bounds.height == 240,
            "Page 11 must draw the generated parchment map on the full canvas.");
        input.Set(InputKey::InteractionKey, false);
        input.Set(InputKey::UiConfirmKey, false);
        host.UpdateApplication(0, input);
        input.Set(InputKey::InteractionKey, true);
        input.Set(InputKey::UiConfirmKey, true);
        host.UpdateApplication(1, input);
        Check(host.GetMenuPage() == MenuPage::Gameplay, "A fresh Enter press must close the map.");
        host.UpdateApplication(1.0 / 33, input);
        Check(host.GetMenuPage() == MenuPage::Gameplay, "The closing Enter must not reopen the wall map.");
        input.Set(InputKey::InteractionKey, false);
        input.Set(InputKey::UiConfirmKey, false);
        host.UpdateApplication(0, input);
        host.hasMap = false;
        host.OpenMap();
        host.UpdateApplication(0, input);
        Tap(host, input, InputKey::UiCancelKey);
        Check(host.GetMenuPage() == MenuPage::Gameplay && !host.hasMap,
            "Escape must close a wall map, which must not grant ownership.");

        host.collisionmapUploadDirty = false;
        host.SetCollisionMapPixelAtWorldXZ(150, 4050, 1);
        Check(host.collisionmapUploadDirty, "Single-pixel writes must invalidate the map upload.");
        host.collisionmapUploadDirty = false;
        host.SetCollisionMapPixelAtWorldXZ(150, 4050, 1);
        Check(!host.collisionmapUploadDirty, "Repeated identical writes must not force another upload.");
        host.SetDoorCollisionMapAtWorldXZ(150, 4050, 56);
        Check(host.collisionmapUploadDirty, "Door spans must invalidate the map upload.");
        host.collisionmapUploadDirty = false;
        host.SetDoorCollisionMapAtWorldXZ(150, 4050, 56);
        Check(!host.collisionmapUploadDirty, "Unchanged door spans must remain clean.");
        host.SetBarrierCollisionMapAtWorldXZ(150, 4050, 0, 16);
        Check(host.collisionmapUploadDirty, "Barrier spans must invalidate the map upload.");
        host.SetDoorCollisionMapAtWorldXZ(150, 4050, 56);
        host.collisionmapUploadDirty = false;
        auto wall = Item(CollisionType::WallDestruction);
        wall.posX = 150; wall.posZ = 4050;
        host.DamageObject(wall, 1);
        Check(host.collisionmapUploadDirty && wall.health == 0,
            "Wall destruction must invalidate the map when it removes collision pixels.");
        host.collisionmapUploadDirty = false;
        auto fountain = Item(CollisionType::WaterRecharge, 1);
        fountain.posX = 150; fountain.posZ = 4050;
        host.DamageObject(fountain, 0);
        Check(host.collisionmapUploadDirty, "Floor footprint changes must invalidate map coverage.");
        host.collisionmapUploadDirty = false;
        host.StartLevel(50);
        Check(host.collisionmapUploadDirty, "Replacing a level must invalidate even an already clean map.");
        ExpectError<std::logic_error>([] { GameContext empty; empty.OpenMap(); });
    }

    void TestNativeArtworkLayout()
    {
        GameContext host;
        host.SetMenuPage(MenuPage::Inventory);
        host.playerLife = 181 * 65536 + 32768;
        host.gold = 12;
        host.arrowPoison = 7;
        host.hasMap = true;
        int frames = 0, meters = 0, fills = 0, icons = 0;
        Check(ApplicationPresentation::Width == 360 && ApplicationPresentation::Height == 240,
            "The original Mode X inventory canvas is 360x240.");
        for (const auto& command : host.GetApplicationPresentation().commands)
        {
            if (const auto* fill = std::get_if<UiFill>(&command))
            {
                Check(fill->bounds.width == 360 && fill->bounds.height == 240 &&
                    fill->color.r == 0 && fill->color.g == 0 && fill->color.b == 0,
                    "Inventory must have its original black backdrop, not invented panel fills.");
                ++fills;
            }
            if (const auto* picture = std::get_if<UiBitmap>(&command))
            {
                if (picture->source == UiBitmapSource::Hud && picture->animation == 0)
                {
                    const int cell = frames++;
                    Check(picture->layout == UiBitmapLayout::TopLeft && picture->bounds.width == 0 &&
                        picture->bounds.height == 0 && picture->bounds.x == 10 + (cell % 4) * 50 &&
                        picture->bounds.y == (cell < 12 ? 10 + (cell / 4) * 46 : 182) &&
                        picture->frame == (cell < 12 ? 0 : cell == 12 ? 3 : cell == 13 ? 4 : 1) &&
                        picture->flipHorizontal == (cell == 15),
                        "Frames, Back/Use and mirrored hand art must use native selectors, positions and pixel sizes.");
                }
                if (picture->source == UiBitmapSource::InventoryIcons)
                {
                    ++icons;
                    if (picture->animation == 5 && picture->frame == 0)
                    {
                        Check(picture->bounds.x == 232 && picture->bounds.y == 84 && picture->shade == 0,
                            "Gold must use its native icon, placement and availability shade.");
                    }
                    if (picture->animation == 2 && picture->frame == 1)
                    {
                        Check(picture->bounds.x == 273 && picture->bounds.y == 68 && picture->shade == 6,
                            "Unavailable medicine must retain its original darkened icon.");
                    }
                }
            }
            if (const auto* meter = std::get_if<UiHealthMeter>(&command))
            {
                ++meters;
                Check(meter->x == 225 && meter->y == 27 && meter->filledColumns == 45,
                    "The health meter must use the native high-word / 4 threshold and placement.");
            }
            if (const auto* text = std::get_if<UiText>(&command))
            {
                Check(text->layout == UiTextLayout::Native,
                    "Inventory text must retain native glyph offsets and advances, not top-aligned modern metrics.");
                Check(text->text == "Health:" || text->text == "50%" || text->text == "12",
                    "Native inventory must not contain invented instructions, resource names or action captions.");
            }
        }
        Check(frames == 16 && meters == 1 && fills == 1 && icons == 5,
            "The complete native frame, meter and resource-icon recipe must be present.");
        host.SetMenuPage(MenuPage::MainMenu);
        for (const auto& command : host.GetApplicationPresentation().commands)
        {
            if (const auto* text = std::get_if<UiText>(&command))
            {
                Check(text->layout == UiTextLayout::Menu, "Menu text must use the original 480-line pixel aspect.");
            }
        }
    }
}

int main(int argc, char** argv)
{
    if (argc != 2) { throw std::invalid_argument("Expected the original asset directory."); }
    originalAssets = argv[1];
    try
    {
        TestFrameTimingAndRoutes();
        TestGatesLoadingAndQuit();
        TestNewGameSelection();
        TestMenuFidelity();
        TestMenuLayoutsAndInventoryNames();
        TestScreenFades();
        TestSaveLoadMenus();
        TestQuickSaveLoad();
        TestQuickSaveFailures();
        TestInventorySelectionAndNavigation();
        TestRepeatPulseAndInputLeakage();
        TestPotionCompatibility();
        TestPresentationAndHandReference();
        TestNativeArtworkLayout();
        TestMapFlowAndDirtyWrites();
        std::cout << "PASS: nonblocking F1/F2 flow, modal clocks, input edges/repeat, inventory swaps, "
            "medicine arithmetic, 512 pulse phases and drawing recipes.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
