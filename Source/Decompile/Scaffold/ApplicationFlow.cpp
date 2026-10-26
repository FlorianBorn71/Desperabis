#include "GameContext.h"

#include <stdexcept>
#include <string>

namespace Desperabis
{
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
}
