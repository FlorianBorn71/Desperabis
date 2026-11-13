#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

using namespace Desperabis;

namespace
{
    void Check(bool value, const char* message)
    {
        if (!value) { throw std::runtime_error(message); }
    }

    struct Input final : InputHandler
    {
        bool confirm = false;
        bool IsKeyPressed(InputKey key) override { return confirm && key == InputKey::UiConfirmKey; }
    };

    struct Host final : GameContext
    {
        std::filesystem::path assets;
        explicit Host(const std::filesystem::path& path) : assets(path)
        {
            StartNewGame(2);
            StartLevel(35);
            Check(GetRuntimeState() == GameRuntimeState::Ready, "Original castle fixture must load.");
            basicPlayerMovementEnabled = false;
        }
    protected:
        std::vector<std::uint8_t> LoadFile(std::string_view name) override
        {
            std::ifstream file(assets / name, std::ios::binary);
            if (!file) { throw std::runtime_error("Missing castle fixture: " + std::string(name)); }
            return {std::istreambuf_iterator<char>(file), {}};
        }
        void LoadLevel(const LevelLoadRequest& request) override
        {
            LevelLoadData data;
            LoadLevelGeometry(request.levelNumber, data);
            LoadLevelObjects(request.levelNumber, data);
            InitializeLevelPlayerSpawn(data, {0, 0, 100});
            CompleteLevelLoad(request.id, std::move(data));
        }
    };

    void FinishTile(RawGameObject& tile, GameContext& host)
    {
        for (int step = 0; step < 13; ++step) { DispatchObjectCase(tile, host); }
        Check(tile.behavior == BehaviorType::PuzzleGameTile && tile.velocityX == 0 && tile.velocityZ == 0,
            "A tile must finish its 100-unit slide without overshooting.");
    }

    void TestTiles(const std::filesystem::path& assets)
    {
        Host host(assets);
        auto& tile = host.worldObjects[3]; // Authored cell 2, next to the gap at cell 3.
        const auto x = tile.posX;
        HandlePlayerObjectInteraction(tile, host);
        DispatchObjectCase(tile, host);
        Check(tile.itemValue == 3 && tile.generalPurpose1 == 2 &&
            tile.behavior == BehaviorType::PuzzleGameTileMoving && tile.velocityX == 100,
            "The original tile must reserve the gap and enter the recovered moving behavior.");
        auto& neighbor = host.worldObjects[10]; // Cell 1 cannot enter the still-occupied source cell 2.
        HandlePlayerObjectInteraction(neighbor, host);
        DispatchObjectCase(neighbor, host);
        Check(neighbor.itemValue == 1 && neighbor.behavior == BehaviorType::PuzzleGameTile,
            "Another tile must not enter a source cell while its owner is still sliding.");
        DispatchObjectCase(tile, host);
        Check(tile.posX == x + 8 && tile.velocityX == 92 && tile.renderPlacementMode == 3,
            "Moving tiles must advance by eight units per native tick.");
        const auto save = host.CaptureSaveGame("Moving tile");
        Host restored(assets);
        restored.LoadGame(save);
        auto& loaded = restored.worldObjects[3];
        Check(loaded.itemValue == 3 && loaded.generalPurpose1 == 2 && loaded.velocityX == 92,
            "Native saves must preserve an in-flight tile and both reserved cells.");
        HandlePlayerObjectInteraction(restored.worldObjects[10], restored);
        DispatchObjectCase(restored.worldObjects[10], restored);
        Check(restored.worldObjects[10].itemValue == 1, "Reload must rebuild the moving tile's source reservation.");
        FinishTile(loaded, restored);
        Check(loaded.posX == x + 100, "A loaded slide must finish at the authored neighboring cell.");
        HandlePlayerObjectInteraction(restored.worldObjects[10], restored);
        DispatchObjectCase(restored.worldObjects[10], restored);
        Check(restored.worldObjects[10].itemValue == 2,
            "The old cell must become available immediately after a tile finishes sliding.");

        Host solved(assets);
        for (int slot = 2; slot <= 16; ++slot) { solved.worldObjects[slot].itemValue = slot - 1; }
        auto& last = solved.worldObjects[16];
        last.itemValue = 16;
        HandlePlayerObjectInteraction(last, solved);
        DispatchObjectCase(last, solved);
        DispatchObjectCase(solved.worldObjects[70], solved);
        Check(solved.worldObjects[70].health == 1000 && solved.worldObjects[17].health == 1021,
            "The completion controller must not accept a tile still in transit.");
        FinishTile(last, solved);
        DispatchObjectCase(solved.worldObjects[70], solved);
        Check(solved.worldObjects[70].health == 0 && solved.worldObjects[17].generalPurpose2 == 3,
            "Ordered I..XV tiles must consume the controller and activate its authored door group.");

        Host edge(assets);
        for (int slot = 2; slot <= 16; ++slot) { edge.worldObjects[slot].itemValue = slot - 1; }
        edge.worldObjects[5].itemValue = 16; // Gap at 4; cell 5 is not its horizontal neighbor.
        auto& acrossRow = edge.worldObjects[6];
        HandlePlayerObjectInteraction(acrossRow, edge);
        DispatchObjectCase(acrossRow, edge);
        Check(acrossRow.itemValue == 5 && acrossRow.behavior == BehaviorType::PuzzleGameTile,
            "Horizontal neighbors must not wrap between rows of the 4x4 grid.");

        Host vertical(assets);
        auto& above = vertical.worldObjects[5];
        const auto z = above.posZ;
        HandlePlayerObjectInteraction(above, vertical);
        DispatchObjectCase(above, vertical);
        Check(above.itemValue == 3 && above.velocityZ == 100,
            "The original vertical neighbor must move toward the same gap.");
        vertical.tickCount = 20;
        DispatchObjectCase(above, vertical);
        Check(above.posZ == z + 100 && above.behavior == BehaviorType::PuzzleGameTile,
            "Batched tile ticks must stop exactly at the neighboring cell.");
    }

    void TestTileTriggers(const std::filesystem::path& assets)
    {
        Host host(assets);
        auto& trigger = host.worldObjects[69];
        auto& tile = host.worldObjects[3];
        tile.posX = trigger.posX;
        tile.posY = trigger.posY;
        tile.posZ = trigger.posZ;
        DispatchObjectCase(trigger, host);
        Check(trigger.behavior == BehaviorType::PuzzleTileTrigger && trigger.collisionMask == 0,
            "A numbered-tile trigger must ignore the wrong tile.");
        tile.doorLockOrGroupBitmask = trigger.doorLockOrGroupBitmask;
        DispatchObjectCase(trigger, host);
        Check(trigger.behavior == BehaviorType::None && trigger.collisionType == CollisionType::None &&
            trigger.collisionMask == 0 && host.worldObjects[18].generalPurpose2 == 3,
            "Behavior 148 must activate its group once when the required tile overlaps it.");
    }

    void TurnWheel(RawGameObject& wheel, GameContext& host)
    {
        const auto angle = wheel.winX;
        HandlePlayerObjectInteraction(wheel, host);
        DispatchObjectCase(wheel, host);
        HandlePlayerObjectInteraction(wheel, host);
        for (int frame = 1; frame < 13; ++frame) { DispatchObjectCase(wheel, host); }
        Check(wheel.generalPurpose2 == 0 && wheel.winX == ((angle - 64) & 511),
            "A wheel click must rotate exactly one eighth-turn, ignoring extra clicks during motion.");
    }

    void TestWheels(const std::filesystem::path& assets)
    {
        Host host(assets);
        for (int slot : {38, 74, 75, 76})
        {
            auto& wheel = host.worldObjects[slot];
            for (int turns = 0; turns < 8 && wheel.winX != WrapSignedWord(wheel.generalPurpose1); ++turns)
            {
                TurnWheel(wheel, host);
            }
            Check(wheel.winX == WrapSignedWord(wheel.generalPurpose1),
                "Each authored wheel target must be reachable in eight turns.");
            DispatchObjectCase(wheel, host);
        }
        Check(host.switchGroupMasks[1] == 0, "All four correct wheels must clear their shared door mask.");
        DispatchObjectCase(host.worldObjects[55], host);
        Check(host.worldObjects[55].generalPurpose2 == 3,
            "The wheel mask must open the original combination door through the shared door handler.");
        auto& wheel = host.worldObjects[38];
        TurnWheel(wheel, host);
        Check(host.switchGroupMasks[1] == 1, "Turning one correct wheel away must relock only its own bit.");
        const auto save = host.CaptureSaveGame("Combination");
        Host restored(assets);
        restored.LoadGame(save);
        Check(restored.switchGroupMasks[1] == 1 &&
            restored.worldObjects[38].winX == wheel.winX,
            "Wheel positions and the shared combination mask must survive a native save.");
    }

    void TestClock(const std::filesystem::path& assets)
    {
        Host host(assets);
        auto& clock = host.worldObjects[33];
        const auto minute = clock.itemValue;
        clock.velocityX = clock.velocityY = 0;
        HandlePlayerObjectInteraction(clock, host);
        DispatchObjectCase(clock, host);
        Check(clock.itemValue == minute && clock.velocityY == 59 && clock.velocityX == 34,
            "The authored adjustment advances almost one minute through the native 35-tick accumulator.");
        DispatchObjectCase(clock, host);
        Check(clock.itemValue == minute + 1 && clock.velocityY == 0 && clock.velocityX == 0,
            "The clock adjustment must carry seconds into the minute counter.");
        clock.itemValue = 719;
        clock.velocityX = 34;
        clock.velocityY = 59;
        clock.doorLockOrGroupBitmask = -1;
        DispatchObjectCase(clock, host);
        Check(clock.itemValue == 0 && clock.doorLockOrGroupBitmask == 11 &&
            host.worldObjects[60].health == 1000 && host.worldObjects[64].generalPurpose2 == 3,
            "Midnight must start twelve chimes, the knights and the clock doors.");
        clock.itemValue = 15;
        clock.velocityX = 34;
        clock.doorLockOrGroupBitmask = -1;
        DispatchObjectCase(clock, host);
        Check(clock.doorLockOrGroupBitmask == 0, "Quarter past must produce one chime.");
        clock.velocityX = 34;
        DispatchObjectCase(clock, host);
        Check(clock.doorLockOrGroupBitmask == 0, "Quarter-hour chimes must not retrigger within the same minute.");
        clock.itemValue = 16;
        clock.velocityX = 34;
        DispatchObjectCase(clock, host);
        Check(clock.doorLockOrGroupBitmask == -1, "An ordinary minute must rearm the chime latch.");
        clock.itemValue = 30;
        clock.velocityX = 34;
        DispatchObjectCase(clock, host);
        Check(clock.doorLockOrGroupBitmask == 1, "Half past must start two chimes.");
        auto& minuteHand = host.worldObjects[34];
        auto& hourHand = host.worldObjects[35];
        clock.itemValue = 90;
        DispatchObjectCase(minuteHand, host);
        DispatchObjectCase(hourHand, host);
        Check(minuteHand.winZ == 768 && hourHand.winZ == 64,
            "Both attached hands must derive their angle from the parent clock's minutes.");
        auto& pendulum = host.worldObjects[36];
        const auto phase = pendulum.velocityZ;
        DispatchObjectCase(pendulum, host);
        Check(pendulum.velocityZ == ((phase + 11) & 511) &&
            pendulum.winZ == (Sine(pendulum.velocityZ) * 16 >> 16),
            "The pendulum must use the native sine phase and amplitude.");
        auto& knights = host.worldObjects[60];
        const auto angle = knights.winY;
        host.tickCount = 510;
        DispatchObjectCase(knights, host);
        Check(knights.health == 1000 && knights.generalPurpose2 == 1,
            "The rotating mechanism must stay active until its authored countdown expires.");
        host.tickCount = 1;
        DispatchObjectCase(knights, host);
        Check(knights.health == 1001 && knights.generalPurpose2 == 512 &&
            knights.winY == ((angle + 1022) & 511),
            "The knights must stop and rearm after their first native rotation.");
        const auto stopped = knights.winY;
        DispatchObjectCase(knights, host);
        Check(knights.winY == stopped, "Inactive clock knights must not keep rotating.");
        host.DamageObject(knights, 0);
        DispatchObjectCase(knights, host);
        Check(knights.health == 1000 && knights.generalPurpose2 == 511,
            "The existing restart lever's activation path must restart the mechanism.");
    }

    void TestCrossbow(const std::filesystem::path& assets)
    {
        struct Audio final : AudioHandler
        {
            int plays = 0;
            std::uint16_t GetSampleCount() const override { return 129; }
            bool IsChannelPlaying(std::size_t) const override { return false; }
            void PlaySample(std::size_t, std::uint8_t, std::uint8_t) override { ++plays; }
            void StopAllSamples() override {}
            void StopAmbientPlayback() override {}
        } audio;
        Host host(assets);
        host.audioHandler = &audio;
        host.soundEffectsLoaded = true;
        for (auto& effect : host.soundEffects)
        {
            effect.sampleCount = 1;
            effect.samples[0] = 1;
        }
        auto& crossbow = host.worldObjects[67];
        Check(crossbow.health == 1030 && crossbow.generalPurpose1 == 1 && crossbow.itemValue == 0,
            "The original knight's projectile selector is GP1, not itemValue.");
        host.worldObjects[1].posX = crossbow.posX;
        host.worldObjects[1].posZ = crossbow.posZ;
        host.soundListenerX = crossbow.posX;
        host.soundListenerZ = crossbow.posZ;
        host.DamageObject(crossbow, 0);
        host.tickCount = 1;
        const auto countArrows = [&]
        {
            return std::count_if(host.worldObjects.begin(), host.worldObjects.end(), [](const auto& object)
            {
                return object.health > 0 && object.behavior == BehaviorType::FlyingArrow &&
                    object.collisionType == CollisionType::FlyingArrow;
            });
        };
        const auto arrows = countArrows();
        const auto plays = audio.plays;
        DispatchObjectCase(crossbow, host);
        Check(crossbow.behavior == BehaviorType::None && crossbow.animFrame == crossbow.modelFrame2,
            "The activated castle knight must finish its one-shot animation.");
        Check(countArrows() == arrows + 1, "The castle knight must spawn one real flying arrow.");
        Check(audio.plays == plays + 1, "The nearby castle knight must play one firing sound.");
        DispatchObjectCase(crossbow, host);
        Check(countArrows() == arrows + 1 && audio.plays == plays + 1,
            "The completed crossbow must not repeat its shot or firing sound.");
    }

    void TestFoil(const std::filesystem::path& assets)
    {
        Host host(assets);
        auto& foil = host.worldObjects[79];
        Check(foil.itemValue == static_cast<std::int16_t>(WeaponType::Foil) &&
            foil.modelId == 31 && foil.modelAnim == 5 && foil.animFrame == 5,
            "The original castle foil must retain its authored pickup model and weapon selector.");
        const auto pickup = HandlePlayerObjectInteraction(foil, host);
        Check(pickup.status == InteractionStatus::Applied && pickup.inventorySlot == 1 && foil.health == 0,
            "The original foil must be collectible through normal weapon pickup.");
        Input input;
        for (int tick = 0; tick < 30; ++tick) { host.Tick({1,1}, input); }
        const auto view = host.GetWeaponView();
        Check(host.selectedWeapon == WeaponType::Foil && view && view->bitmapFrame == 11 &&
            view->object.modelId == 31 && view->object.modelAnim == 5 && view->object.animFrame == 4 &&
            host.weaponAnimationSpeed == 0,
            "Equipping the collected foil must switch from its world pickup to the original held sheet and hand.");
    }

    void TestBook(const std::filesystem::path& assets)
    {
        Host host(assets);
        auto& book = host.worldObjects[98];
        const auto health = book.health;
        Check(HandlePlayerObjectInteraction(book, host).status == InteractionStatus::Applied &&
            host.GetMenuPage() == MenuPage::CodeBook && book.health == health,
            "Reading the original code book must open a modal page without consuming it.");
        const auto page = host.GetApplicationPresentation();
        int paper = 0, illustrations = 0;
        for (const auto& command : page.commands)
        {
            const auto* bitmap = std::get_if<UiBitmap>(&command);
            if (!bitmap) { continue; }
            paper += bitmap->source == UiBitmapSource::MapPaper;
            if (bitmap->source == UiBitmapSource::TextIllustration)
            {
                ++illustrations;
                Check(bitmap->frame == 13 && bitmap->bounds.x == 76 && bitmap->bounds.y == 103,
                    "Section 101 must select the four-wheel illustration at its native text coordinates.");
            }
        }
        Check(paper == 1 && illustrations == 1, "The book must put its original illustration on parchment.");
        Input input;
        const auto ticks = host.levelElapsedTicks;
        host.UpdateApplication(1, input);
        Check(host.levelElapsedTicks == ticks, "Reading the book must pause gameplay.");
        input.confirm = true;
        host.UpdateApplication(0, input);
        Check(host.GetMenuPage() == MenuPage::Gameplay, "Confirm must close the book.");
        bool rejected = false;
        try { host.OpenCodeBook(30000); }
        catch (const std::runtime_error&) { rejected = true; }
        Check(rejected && host.GetMenuPage() == MenuPage::Gameplay,
            "Missing book sections must report an error without opening a blank successful page.");
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc != 2) { throw std::runtime_error("Expected original asset directory."); }
        TestTiles(argv[1]);
        TestTileTriggers(argv[1]);
        TestWheels(argv[1]);
        TestClock(argv[1]);
        TestCrossbow(argv[1]);
        TestFoil(argv[1]);
        TestBook(argv[1]);
        std::cout << "Castle sliding tiles, numbered triggers, wheels, clock and code book passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
