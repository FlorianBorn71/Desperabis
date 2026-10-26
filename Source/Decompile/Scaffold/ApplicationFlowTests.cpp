#include "GameContext.h"

#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace Desperabis;

namespace
{
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
        Check(caught, "Expected an explicit application-flow error.");
    }

    class TestContext final : public GameContext
    {
    public:
        std::vector<MainMenuAction> choices{ MainMenuAction::Quit };
        std::vector<MenuPage> gameplayDestinations{ MenuPage::Quit };
        std::vector<MenuPage> visits;
        std::optional<std::int16_t> selectedChapter;
        std::optional<std::int16_t> initializedChapter;
        MenuPage pageDestination = MenuPage::Quit;
        MenuPage inventoryDestination = MenuPage::Quit;
        bool initializationFails = false;
        std::size_t choiceIndex = 0;
        std::size_t gameplayIndex = 0;

    protected:
        MainMenuAction ShowMainMenu() override
        {
            visits.push_back(GetMenuPage());
            return choices.at(choiceIndex++);
        }

        std::optional<std::int16_t> ShowChapterSelection() override
        {
            visits.push_back(MenuPage::ChapterSelection);
            return selectedChapter;
        }

        MenuPage RunGameplay() override
        {
            visits.push_back(MenuPage::Gameplay);
            return gameplayDestinations.at(gameplayIndex++);
        }

        MenuPage ShowSettings() override { return Visit(MenuPage::Settings); }
        MenuPage ShowConfiguration() override { return Visit(MenuPage::Configuration); }
        MenuPage ShowLoadGame() override { return Visit(MenuPage::LoadGame); }
        MenuPage ShowSaveGame() override { return Visit(MenuPage::SaveGame); }
        MenuPage ShowInventory() override
        {
            visits.push_back(MenuPage::Inventory);
            return inventoryDestination;
        }
        MenuPage ShowUnrecoveredPage(MenuPage page) override { return Visit(page); }

        void InitializeNewGame(std::int16_t chapter) override
        {
            if (initializationFails) { throw std::runtime_error("Test initialization failed."); }
            initializedChapter = chapter;
        }

    private:
        MenuPage Visit(MenuPage page)
        {
            visits.push_back(page);
            return pageDestination;
        }
    };

    void TestMainMenu()
    {
        const std::array<std::pair<MainMenuAction, MenuPage>, 7> routes{ {
            { MainMenuAction::ResumeGame, MenuPage::Gameplay },
            { MainMenuAction::NewGame, MenuPage::ChapterSelection },
            { MainMenuAction::LoadGame, MenuPage::LoadGame },
            { MainMenuAction::SaveGame, MenuPage::SaveGame },
            { MainMenuAction::Settings, MenuPage::Settings },
            { MainMenuAction::Configuration, MenuPage::Configuration },
            { MainMenuAction::Quit, MenuPage::Quit }
        } };
        GameContext context;
        Check(context.GetMenuPage() == MenuPage::MainMenu && !context.CanResumeGame(),
            "Application flow must initially select the main menu without an active game.");
        for (const auto mainPage : { MenuPage::MainMenu, MenuPage::AlternateMainMenu })
        {
            for (const auto& route : routes)
            {
                for (const bool active : { false, true })
                {
                    for (const bool suppressed : { false, true })
                    {
                        context.SetMenuPage(mainPage);
                        context.hasActiveGame = active;
                        context.positionalSoundsSuppressed = suppressed;
                        const bool enabled = (route.first != MainMenuAction::ResumeGame &&
                            route.first != MainMenuAction::SaveGame) || (active && !suppressed);
                        Check(context.SelectMainMenuAction(route.first) == enabled &&
                            context.GetMenuPage() == (enabled ? route.second : mainPage),
                            "Main-menu routing or resume/save availability differs.");
                    }
                }
            }
            for (const bool suppressed : { false, true })
            {
                context.SetMenuPage(mainPage);
                context.hasActiveGame = false;
                context.positionalSoundsSuppressed = suppressed;
                Check(context.SelectMainMenuAction(MainMenuAction::Cancel) == !suppressed &&
                    context.GetMenuPage() == (suppressed ? mainPage : MenuPage::Gameplay),
                    "Escape must use the native shared gate, not the resume-item availability flag.");
            }
        }
        context.SetMenuPage(MenuPage::MainMenu);
        ExpectError<std::invalid_argument>([&] {
            context.SelectMainMenuAction(static_cast<MainMenuAction>(100));
        });
        Check(context.GetMenuPage() == MenuPage::MainMenu, "Invalid choices must preserve the page.");
        context.SetMenuPage(MenuPage::Settings);
        ExpectError<std::logic_error>([&] { context.SelectMainMenuAction(MainMenuAction::Quit); });
    }

    void TestOuterRouting()
    {
        TestContext context;
        for (unsigned int value = 0; value <= 0xffff; ++value)
        {
            const auto page = static_cast<MenuPage>(value);
            context.SetMenuPage(page);
            context.visits.clear();
            context.choiceIndex = 0;
            context.gameplayIndex = 0;
            context.positionalSoundsSuppressed = false;
            context.RunApplicationStep();
            bool calledPage = false;
            auto destination = MenuPage::Quit;
            switch (value)
            {
            case 0: case 1: case 2: case 3: case 5: case 6: case 7:
            case 8: case 9: case 10: case 11: case 20: case 21:
            case 22: case 23: case 25: case 100:
                calledPage = true;
                break;
            case 4: case 31: case 33: case 101:
                calledPage = true;
                destination = MenuPage::MainMenu;
                break;
            case 26: case 32:
                calledPage = true;
                destination = MenuPage::Gameplay;
                break;
            case 99: destination = MenuPage::Gameplay; break;
            default:
                if (value >= 12 && value <= 19) { destination = MenuPage::Gameplay; }
                if (value >= 200 && value <= 299) { calledPage = true; }
                break;
            }
            Check(context.GetMenuPage() == destination &&
                context.visits.size() == (calledPage ? 1u : 0u) &&
                (!calledPage || context.visits.front() == page),
                "Outer application routing differs for a native state word.");
            Check(context.positionalSoundsSuppressed == (value == 20),
                "Presentation state 20 must set the native shared suppression gate.");
        }
        context.SetMenuPage(MenuPage::SaveGame);
        context.positionalSoundsSuppressed = true;
        context.visits.clear();
        context.RunApplicationStep();
        Check(context.GetMenuPage() == MenuPage::MainMenu && context.visits.empty(),
            "The outer dispatcher must reject a gated save without invoking its page.");
    }

    void TestNewGameAndInventory()
    {
        TestContext context;
        context.SetMenuPage(MenuPage::ChapterSelection);
        context.RunApplicationStep();
        Check(context.GetMenuPage() == MenuPage::MainMenu && !context.initializedChapter,
            "Cancelling chapter selection must not initialize a game.");
        context.SetMenuPage(MenuPage::ChapterSelection);
        context.selectedChapter = std::int16_t{ 2 };
        context.initializationFails = true;
        ExpectError<std::runtime_error>([&] { context.RunApplicationStep(); });
        Check(context.GetMenuPage() == MenuPage::ChapterSelection && !context.hasActiveGame,
            "Failed new-game initialization must not report gameplay readiness.");
        context.initializationFails = false;
        context.RunApplicationStep();
        Check(context.GetMenuPage() == MenuPage::Gameplay && context.hasActiveGame &&
            context.initializedChapter == 2, "Chapter acceptance must initialize before starting gameplay.");
        for (const auto chapter : { std::int16_t{ 0 }, std::int16_t{ -1 } })
        {
            ExpectError<std::invalid_argument>([&] { context.StartNewGame(chapter); });
        }
        context.positionalSoundsSuppressed = true;
        Check(!context.OpenInventory() && context.GetMenuPage() == MenuPage::Gameplay,
            "The original F2 gate must keep inventory closed.");
        context.positionalSoundsSuppressed = false;
        Check(context.OpenInventory() && context.GetMenuPage() == MenuPage::Inventory,
            "F2 must select the inventory page when enabled.");
        ExpectError<std::logic_error>([&] { context.OpenInventory(); });
    }

    void TestFullApplicationLoop()
    {
        TestContext context;
        context.choices = { MainMenuAction::Settings, MainMenuAction::Configuration,
            MainMenuAction::NewGame, MainMenuAction::SaveGame, MainMenuAction::LoadGame,
            MainMenuAction::ResumeGame, MainMenuAction::Quit };
        context.selectedChapter = std::int16_t{ 3 };
        context.pageDestination = MenuPage::MainMenu;
        context.inventoryDestination = MenuPage::Gameplay;
        context.gameplayDestinations = { MenuPage::Inventory, MenuPage::MainMenu, MenuPage::MainMenu };
        context.RunApplication();
        const std::vector<MenuPage> expected{
            MenuPage::MainMenu, MenuPage::Settings, MenuPage::MainMenu, MenuPage::Configuration,
            MenuPage::MainMenu, MenuPage::ChapterSelection, MenuPage::Gameplay, MenuPage::Inventory,
            MenuPage::Gameplay, MenuPage::MainMenu, MenuPage::SaveGame, MenuPage::MainMenu, MenuPage::LoadGame,
            MenuPage::MainMenu, MenuPage::Gameplay, MenuPage::MainMenu
        };
        Check(context.visits == expected && context.GetMenuPage() == MenuPage::Quit &&
            context.initializedChapter == 3, "The full member-function page loop lost a transition.");
        const auto count = context.visits.size();
        context.RunApplication();
        context.RunApplicationStep();
        Check(context.visits.size() == count, "Quitting must prevent further page callbacks.");
    }

    void TestUnimplementedBoundaries()
    {
        GameContext context;
        for (const auto page : { MenuPage::Gameplay, MenuPage::MainMenu, MenuPage::Settings,
            MenuPage::Configuration, MenuPage::ChapterSelection, MenuPage::LoadGame,
            MenuPage::SaveGame, MenuPage::Inventory, MenuPage::UnidentifiedPage5,
            MenuPage::Presentation20, MenuPage::Dialog100, MenuPage::Dialog101,
            MenuPage::ResourcePresentationFirst })
        {
            context.SetMenuPage(page);
            ExpectError<std::logic_error>([&] { context.RunApplicationStep(); });
            Check(context.GetMenuPage() == page, "An unimplemented page must not silently succeed.");
        }
        context.SetMenuPage(MenuPage::ChapterSelection);
        ExpectError<std::logic_error>([&] { context.StartNewGame(1); });
        Check(context.GetMenuPage() == MenuPage::ChapterSelection && !context.hasActiveGame,
            "The default new-game boundary must not pretend to load a level.");
    }
}

int main()
{
    try
    {
        TestMainMenu();
        TestOuterRouting();
        TestNewGameAndInventory();
        TestFullApplicationLoop();
        TestUnimplementedBoundaries();
        std::cout << "PASS: application flow, all 65536 state words, menu gates, chapter startup, "
            "inventory entry, and explicit recovery boundaries verified.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
