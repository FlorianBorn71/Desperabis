#include "GameContext.h"
#include "BehaviorDispatch.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

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
        int healthNotifications = 0;
        void OnPlayerHealthChanged(std::int32_t, std::int32_t) override { ++healthNotifications; }
        void LoadLevel(const LevelLoadRequest& request) override
        {
            requests.push_back(request);
            if (fail) { throw std::runtime_error("New-game fixture failed to load."); }
            if (reportFailure) { FailLevelLoad(request.id, "New-game fixture reported failure."); return; }
            if (synchronous) { CompleteLevelLoad(request.id, MakeLevel()); }
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
        Tap(host, input, InputKey::UiConfirmKey);
        Check(host.GetMenuPage() == MenuPage::MainMenu, "A host without a level must not resume gameplay.");
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
        Move(host, input, InputKey::UiDownKey, 2);
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
            host.weaponAnimationRecoil = 99;
            host.weaponReady = false;
            host.SetPaused(true);
            Tap(host, input, InputKey::MainMenuKey);
            Check(hasText(host, "New game"), "The main menu must expose New Game.");
            Move(host, input, InputKey::UiDownKey);
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
            Check(hasText(host, "Training level") && hasText(host, "Part 1") && hasText(host, "Part 2") &&
                hasText(host, "Starts at level " + std::to_string(starts[part])),
                "Part selection must expose exactly the playable part starts.");
            input.Set(InputKey::AttackKey, true);
            input.Set(InputKey::DebugNextLevelKey, true);
            const auto transition = Tap(host, input, InputKey::UiConfirmKey);
            Check(!transition.tick && transition.worldDeltaSeconds == 0 && transition.timing.behaviorTicks == 0 &&
                host.GetLoadedLevelNumber() == starts[part] && host.requests.size() == 2 &&
                host.GetRuntimeState() == GameRuntimeState::Ready && host.GetMenuPage() == MenuPage::Gameplay &&
                !host.IsPaused(), "Part acceptance must load once, unpause, and not simulate the transition frame.");
            Check(host.inventory[4].health == 0 && host.gold == 0 && host.arrowPoison == 0 && !host.hasMap &&
                host.playerLifeMaximum == 360 && host.playerLife == 360 * 65536 &&
                host.playerLifeDeltaSuppressed == 0 && host.playerHitEffectTicks == 0 &&
                host.playerOxygen == GameContext::PlayerOxygenMaximum && host.healthNotifications == 1 &&
                host.selectedWeapon == WeaponType::None && host.requestedWeapon == WeaponType::None &&
                host.weaponSpriteFrame == 0 && host.weaponHand == 0 && host.requestedWeaponHand == 0 &&
                host.weaponAnimationSpeed == 0 && host.weaponAnimationPosition == 0 && host.weaponAnimationRecoil == 0 &&
                host.weaponReady && !host.weaponElectricityFiring && !host.positionalSoundsSuppressed,
                "New Game must reset carried inventory, money, poison, maps, vital meters and weapon commands.");
            Check(host.DEBUG_noPlayerCollision && !host.soundEnabled &&
                !host.basicPlayerMovementEnabled && !host.behaviorDispatchEnabled,
                "New Game must preserve host debug, audio and simulation preferences.");
            const GameContext defaults;
            Check(host.weaponAmounts == defaults.weaponAmounts &&
                host.weaponInventorySlots == defaults.weaponInventorySlots,
                "New Game must restore native resource defaults, not carry ammunition from the old game.");
            Check(host.UpdateApplication(1.0 / 33, input).tick.has_value() && host.requests.size() == 2 &&
                host.GetLoadedLevelNumber() == starts[part],
                "The next frame must resume simulation without a held Tab skipping the chosen starting level.");
            input.keys.fill(false);
            host.UpdateApplication(0, input);
            if (part == 2)
            {
                for (int i = 0; i < 6; ++i) { Tap(host, input, InputKey::DebugNextLevelKey); }
                Check(host.GetLoadedLevelNumber() == 16,
                    "New Game Part 2 followed by six Tab advances must reach the forest.");
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
        Check(hasText(cancelled, "Starts at level 10"), "Part navigation must stop at Part 2.");
        Tap(cancelled, input, InputKey::UiCancelKey);
        Check(cancelled.GetMenuPage() == MenuPage::MainMenu && cancelled.requests.size() == 1 &&
            Same(cancelled.inventory[0], Item(CollisionType::Medicine, 42)),
            "Cancelling selection must return to the menu without losing progress.");
        Move(cancelled, input, InputKey::UiDownKey);
        input.Set(InputKey::UiConfirmKey, true);
        cancelled.UpdateApplication(0, input);
        cancelled.UpdateApplication(1, input);
        Check(cancelled.GetMenuPage() == MenuPage::ChapterSelection && cancelled.requests.size() == 1 &&
            hasText(cancelled, "Starts at level 50"),
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
                Check(text->text == "Gesundheit:" || text->text == "50%" || text->text == "12",
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
                Check(text->layout == UiTextLayout::TopAligned, "Main-menu text must preserve its existing metrics.");
            }
        }
    }
}

int main()
{
    try
    {
        TestFrameTimingAndRoutes();
        TestGatesLoadingAndQuit();
        TestNewGameSelection();
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
