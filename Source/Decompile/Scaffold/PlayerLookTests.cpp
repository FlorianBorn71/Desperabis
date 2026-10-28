#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <iterator>
#include <stdexcept>

using namespace Desperabis;

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition) { throw std::runtime_error(message); }
    }

    class Input final : public InputHandler
    {
    public:
        bool up = false;
        bool down = false;
        bool center = false;
        bool forward = false;
        bool attack = false;
        int lookCalls = 0;
        bool IsKeyPressed(InputKey key) override
        {
            if (key == InputKey::LookUpKey) { ++lookCalls; return up; }
            if (key == InputKey::LookDownKey) { ++lookCalls; return down; }
            if (key == InputKey::CenterLookKey) { ++lookCalls; return center; }
            return (key == InputKey::ForwardKey && forward) || (key == InputKey::AttackKey && attack);
        }
    };

    class Host final : public GameContext
    {
    public:
        void Load(std::array<std::int16_t, 3> direction = { 0,0,100 })
        {
            const auto request = StartLevel(11);
            LevelLoadData data;
            data.level = std::make_unique<Level>(CreateDummyLevel());
            data.playerSpawn = PlayerCamera{ {1500,45,2500}, direction };
            CompleteLevelLoad(request.id, std::move(data));
            soundEnabled = false;
        }

    protected:
        void LoadLevel(const LevelLoadRequest&) override {}
    };

    void Step(Host& host, Input& input, std::int16_t frames = 1, std::int16_t behaviors = 0)
    {
        Check(host.Tick({ behaviors,frames }, input).status == GameTickStatus::Advanced,
            "Look fixture tick failed.");
    }

    void TestKeysAndTiming()
    {
        Host host;
        Input input;
        host.Load();
        input.up = true;
        Step(host, input);
        Check(host.playerTargetLookHeight == 120 && host.playerLookHeight == 114 &&
            host.GetPlayerCamera().lookHeight == 114 && input.lookCalls == 3,
            "Look up must request +10 once per input sample and approach by 5 with even-word quantization.");
        input.up = false;
        Step(host, input);
        Check(host.playerLookHeight == 118, "Upward approach must quantize 119 down to 118.");
        Step(host, input);
        Check(host.playerLookHeight == 120, "Look must settle at the requested horizon.");
        input.center = true;
        Step(host, input);
        Check(host.playerTargetLookHeight == 110 && host.playerLookHeight == 114,
            "Center requests 110 through smoothing rather than snapping the applied horizon.");
        Step(host, input);
        Check(host.playerLookHeight == 110, "Center must finish at the native neutral horizon.");
        input.center = false;
        input.down = true;
        Step(host, input);
        Check(host.playerTargetLookHeight == 100 && host.playerLookHeight == 104,
            "Downward approach must preserve asymmetric even-word rounding.");
        for (int tick = 0; tick < 40; ++tick) { Step(host, input); }
        Check(host.playerTargetLookHeight == 50 && host.playerLookHeight == 50,
            "Held look-down must stop requesting changes at 50.");
        input.down = false;
        input.up = true;
        for (int tick = 0; tick < 40; ++tick) { Step(host, input); }
        Check(host.playerTargetLookHeight == 170 && host.playerLookHeight == 170,
            "Held look-up must stop requesting changes at 170.");
        input.down = true;
        Step(host, input);
        Check(host.playerTargetLookHeight == 170, "Look down must be sampled before look up at the upper limit.");
        input.center = true;
        Step(host, input);
        Check(host.playerTargetLookHeight == 110, "Center must win when all three keys are pressed.");

        host.Load();
        input = {};
        input.up = true;
        Step(host, input, 8);
        Check(host.playerTargetLookHeight == 120 && host.playerLookHeight == 120 && input.lookCalls == 3,
            "Batched frame ticks must smooth repeatedly without repeating key acceleration.");
        const auto calls = input.lookCalls;
        Step(host, input, 0, 8);
        Check(host.playerTargetLookHeight == 120 && host.playerLookHeight == 120 && input.lookCalls == calls,
            "Behavior ticks alone must not change player look or poll look keys.");
        host.Load();
        Check(host.playerTargetLookHeight == 110 && host.playerLookHeight == 110 &&
            host.GetPlayerCamera().lookHeight == 110, "Level installation must reset both native look words.");
    }

    void TestWordOracle()
    {
        Host host;
        Input input;
        host.Load();
        for (int bits = 0; bits < 65536; ++bits)
        {
            const auto applied = WrapSignedWord(bits);
            const auto requested = WrapSignedWord(bits * 73 + 111);
            host.playerLookHeight = applied;
            host.playerTargetLookHeight = requested;
            const int difference = WrapSignedWord(static_cast<int>(requested) - applied);
            const int magnitude = WrapSignedWord(difference < 0 ? -difference : difference);
            const int delta = magnitude <= 5 ? difference : ((difference > 0) - (difference < 0)) * 5;
            const auto expected = WrapSignedWord(static_cast<std::uint16_t>(
                WrapSignedWord(applied + delta)) & 0xfffeu);
            Step(host, input);
            Check(host.playerLookHeight == expected && host.GetPlayerCamera().lookHeight == expected,
                "Look smoothing must preserve signed-word delta/ABS wrapping and the low-bit clear.");
        }
    }

    void SetSlope(Level& level, int sign, bool alongX)
    {
        for (std::size_t row = 0; row < Level::Height; ++row)
        {
            for (std::size_t column = 0; column < Level::Width; ++column)
            {
                const int coordinate = alongX ? static_cast<int>(column) : 39 - static_cast<int>(row);
                level.floorHeights[row][column] = static_cast<std::uint8_t>(100 - sign * coordinate);
            }
        }
    }

    void EquipCrossbow(Host& host)
    {
        auto& item = host.inventory[1];
        item = {};
        item.health = 1000;
        item.collisionType = CollisionType::Weapon;
        item.itemValue = static_cast<std::int16_t>(WeaponType::Crossbow);
        host.weaponAmounts[3] = 10;
        host.RefreshWeaponInventory();
        Check(host.SelectWeapon(WeaponType::Crossbow), "Cannot equip crossbow.");
        host.selectedWeapon = host.requestedWeapon = WeaponType::Crossbow;
        host.weaponHand = host.requestedWeaponHand = 2;
        host.weaponAnimationSpeed = 0;
        host.weaponReady = true;
    }

    void TestSlopesAndArrows()
    {
        for (const bool alongX : { false, true })
        {
            for (const int sign : { -1, 0, 1 })
            {
                for (const bool collisions : { false, true })
                {
                    Host host;
                    Input input;
                    host.Load(alongX ? std::array<std::int16_t, 3>{100,0,0} :
                        std::array<std::int16_t, 3>{0,0,100});
                    SetSlope(*host.level, sign, alongX);
                    host.objectCollisionsEnabled = collisions;
                    const auto floor = host.GetTerrainFloorHeight(1500, 45, 2500);
                    host.worldObjects[1].posY = WrapSignedWord(floor + 20);
                    const auto ahead = host.GetTerrainFloorHeight(alongX ? 1600 : 1500, floor,
                        alongX ? 2500 : 2600);
                    const auto target = WrapSignedWord(110 - WrapSignedWord(floor - ahead));
                    Step(host, input, 20);
                    const auto expected = WrapSignedWord(static_cast<std::uint16_t>(target) & 0xfffeu);
                    Check(host.playerTargetLookHeight == 110 && host.playerLookHeight == expected &&
                        (sign == 0 ? expected == 110 : sign > 0 ? expected > 110 : expected < 110),
                        "A floor rise ahead must raise aim, a descent lower it, without modifying manual look.");
                    EquipCrossbow(host);
                    input.attack = true;
                    Step(host, input);
                    const auto found = std::find_if(host.worldObjects.begin() + 2, host.worldObjects.end(),
                        [](const RawGameObject& object) { return object.health > 0 && object.behavior == BehaviorType::CrossbowBolt; });
                    Check(found != host.worldObjects.end(), "Held attack must create a crossbow bolt.");
                    auto& bolt = *found;
                    Check(bolt.winX == 11 + WrapSignedWord(expected - 90) / 2,
                        "Attack in the same Tick must use the slope-adjusted horizon shared with the camera.");
                    Check(bolt.velocityY == 0, "Crossbow initial vertical velocity must remain zero.");
                    Check(host.GetPlayerCamera().lookHeight == expected, "Shooting must not change the applied camera horizon.");
                    host.tickCount = 1;
                    Function_CrossbowBolt(bolt, host);
                    Check(bolt.velocityY == (10 + WrapSignedWord(expected - 90) / 2) / 4,
                        "The first arrow substep must derive its vertical velocity from the adjusted launch pitch.");
                }
            }
        }
        Host host;
        Input input;
        host.Load();
        SetSlope(*host.level, 1, false);
        input.up = true;
        Step(host, input, 20);
        const auto floor = host.GetTerrainFloorHeight(1500, 45, 2500);
        const auto ahead = host.GetTerrainFloorHeight(1500, 45, 2600);
        Check(host.playerTargetLookHeight == 120 &&
            host.playerLookHeight == ((120 - (floor - ahead)) & ~1),
            "Manual look and automatic slope look must combine, not replace each other.");
    }

    void TestManualShots()
    {
        for (const bool up : { false, true })
        {
            Host host;
            Input input;
            host.Load({0,17,100});
            EquipCrossbow(host);
            input.up = up;
            input.down = !up;
            input.attack = true;
            Step(host, input, 8);
            const auto& bolt = host.worldObjects[2];
            const int expectedHorizon = up ? 120 : 100;
            Check(host.GetPlayerCamera().lookHeight == expectedHorizon &&
                host.GetPlayerCamera().direction[1] == 17 && host.GetPlayerCamera().position[1] == 45 &&
                bolt.behavior == BehaviorType::CrossbowBolt && bolt.winX == (up ? 26 : 16),
                "Look and attack in one Tick must alter arrow pitch without changing view Y or physical eye height.");
        }
    }

    void TestGates()
    {
        for (int gate = 0; gate < 7; ++gate)
        {
            Host host;
            Input input;
            host.Load();
            input.up = true;
            if (gate == 0) { host.SetPaused(true); }
            if (gate == 1) { host.SetMenuPage(MenuPage::MainMenu); }
            if (gate == 2) { host.basicPlayerMovementEnabled = false; }
            if (gate == 3) { host.SetObjectHealth(host.worldObjects[1], 0); }
            if (gate == 4) { host.playerLife = 0; }
            if (gate == 5) { host.StartLevel(12); }
            host.Tick({ 0,static_cast<std::int16_t>(gate == 6 ? 0 : 1) }, input);
            Check(input.lookCalls == 0 && host.playerLookHeight == 110 && host.playerTargetLookHeight == 110,
                "Inactive, paused, loading, disabled or zero-frame controllers must not change look.");
        }

    }

    void TestProbeOrdering()
    {
        Host host;
        Input input;
        host.Load();
        host.worldObjects[1].posZ = 2549;
        for (auto& row : host.level->floorHeights)
        {
            std::fill(std::begin(row), std::end(row), std::uint8_t{100});
        }
        std::fill(std::begin(host.level->floorHeights[14]), std::end(host.level->floorHeights[14]), std::uint8_t{50});
        std::fill(std::begin(host.collisionMap->data), std::end(host.collisionMap->data), std::uint8_t{0x18});
        const auto targetAt = [&](int z)
        {
            return 110 - (host.GetTerrainFloorHeight(1500, 0, static_cast<std::int16_t>(z)) -
                host.GetTerrainFloorHeight(1500, 0, static_cast<std::int16_t>(z + 100)));
        };
        const auto proposedTarget = targetAt(2552);
        Check(proposedTarget != targetAt(2549), "Nonlinear floor fixture must distinguish proposed and blocked positions.");
        host.playerLookHeight = static_cast<std::int16_t>(proposedTarget & ~1);
        input.forward = true;
        Step(host, input);
        Check(host.worldObjects[1].posZ == 2549 && host.playerLookHeight == (proposedTarget & ~1),
            "Slope must sample the moved, pre-wall-correction position and the full 100-unit ahead vector.");
    }
}

int main()
{
    try
    {
        TestKeysAndTiming();
        TestWordOracle();
        TestSlopesAndArrows();
        TestManualShots();
        TestProbeOrdering();
        TestGates();
        std::cout << "Player look tests passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
