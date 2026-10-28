#include "GameContext.h"

#include <algorithm>
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
        bool jump = false;
        bool crouch = false;
        bool forward = false;
        bool strafe = false;
        int postureCalls = 0;
        bool IsKeyPressed(InputKey key) override
        {
            if (key == InputKey::JumpKey) { ++postureCalls; return jump; }
            if (key == InputKey::CrouchKey) { ++postureCalls; return crouch; }
            return (key == InputKey::ForwardKey && forward) ||
                (key == InputKey::StrafeRightKey && strafe);
        }
    };

    class Host final : public GameContext
    {
    public:
        void Load(std::int16_t eyeY = 45)
        {
            const auto request = StartLevel(11);
            LevelLoadData data;
            data.level = std::make_unique<Level>(CreateDummyLevel());
            data.playerSpawn = PlayerCamera{ {1500, eyeY, 2500}, {0, 0, 100} };
            CompleteLevelLoad(request.id, std::move(data));
        }

    protected:
        void LoadLevel(const LevelLoadRequest&) override {}
    };

    int Eye(const GameContext& host) { return host.GetPlayerCamera().position[1]; }

    void Step(Host& host, Input& input, std::int16_t ticks = 1)
    {
        Check(host.Tick({0, ticks}, input).status == GameTickStatus::Advanced, "Posture tick did not advance.");
    }

    void TestJump()
    {
        Host host;
        Input input;
        host.Load();
        Check(!host.playerGrounded && host.playerBodyHeight == 45 && host.playerPostureDelta == 0,
            "Fresh load must reset grounded and posture state without snapping the camera.");
        Step(host, input);
        Check(host.playerGrounded && Eye(host) == 45, "Gravity must establish ground contact.");
        input.jump = true;
        constexpr int arc[]{51, 56, 60, 63, 65, 66, 66, 65, 63, 60, 56, 51, 45, 45};
        for (std::size_t tick = 0; tick < std::size(arc); ++tick)
        {
            Step(host, input);
            input.jump = false;
            Check(Eye(host) == arc[tick] && host.playerGrounded == (tick == std::size(arc) - 1) &&
                host.playerBodyHeight == 45 && host.worldObjects[1].collisionRadiusY == 23,
                "Jump must use impulse 7, pre-integration gravity, and strict support comparison.");
        }
        input.jump = true;
        Step(host, input, 14);
        Check(Eye(host) == 45 && host.playerGrounded && input.postureCalls == 32,
            "Batched jump must sample posture once and follow the same vertical arc.");
        Step(host, input);
        Check(Eye(host) == 51, "Held jump must repeat after landing on the next host input sample.");
        input.crouch = true;
        Step(host, input);
        Check(Eye(host) == 50 && host.playerBodyHeight == 42 && host.playerPostureDelta == -6,
            "Crouch follows jump in input order and cancels upward speed before gravity.");

        host.Load(100);
        input.crouch = false;
        Step(host, input);
        Check(Eye(host) == 99 && host.playerBodyHeight == 45,
            "Airborne jump input must not create another jump impulse.");
        host.Load();
        host.worldObjects[1].generalPurpose3 = 2;
        host.playerGrounded = true;
        host.RequestPlayerJump();
        Check(host.playerPostureDelta == 0, "Any nonzero player generalPurpose3 must block the raise/jump helper.");
    }

    void TestCrouchAndStand()
    {
        Host host;
        Input input;
        host.Load();
        Step(host, input);
        input.crouch = true;
        constexpr int heights[]{42, 39, 36, 33, 30, 28};
        constexpr int eyes[]{44, 42, 39, 35, 30, 28};
        for (std::size_t tick = 0; tick < std::size(heights); ++tick)
        {
            Step(host, input);
            input.crouch = false;
            const auto radius = heights[tick] / 2 + 1;
            Check(host.playerBodyHeight == heights[tick] && Eye(host) == eyes[tick] &&
                host.worldObjects[1].collisionRadiusY == radius &&
                host.worldObjects[1].posY + radius + 2 == Eye(host),
                "Crouch interpolation, gravity, collision radius or eye/center conversion differs.");
        }
        Step(host, input, 10);
        Check(host.playerBodyHeight == 28 && Eye(host) == 28 && host.playerPostureDelta == 0,
            "Releasing crouch must retain the crouched stance, not automatically stand.");
        input.jump = true;
        constexpr int standing[]{31, 34, 37, 40, 43, 45};
        for (const auto height : standing)
        {
            Step(host, input);
            input.jump = false;
            Check(host.playerBodyHeight == height && Eye(host) == height && host.playerGrounded,
                "Jump from crouch must raise the body before launching a jump.");
        }
        Check(host.playerPostureDelta == 0, "Raising past 45 must clear the request.");
        host.playerBodyHeight = 31;
        host.RequestPlayerCrouch();
        Step(host, input);
        Check(host.playerBodyHeight == 28 && host.playerPostureDelta == -6,
            "Reaching exactly 28 must not clear the lowering request until overshoot.");
        Step(host, input);
        Check(host.playerBodyHeight == 28 && host.playerPostureDelta == 0, "Lower clamp must clear overshoot.");
        host.playerBodyHeight = 42;
        host.playerPostureDelta = 6;
        Step(host, input);
        Check(host.playerBodyHeight == 45 && host.playerPostureDelta == 6,
            "Reaching exactly 45 must retain the raising request.");
        Step(host, input);
        Check(host.playerBodyHeight == 45 && host.playerPostureDelta == 0, "Upper clamp must clear overshoot.");
        for (const bool bodyMode : {false, true})
        {
            host.playerBodyHeight = 28;
            host.field_D500 = bodyMode;
            host.worldObjects[1].itemValue = bodyMode ? 1 : 0;
            host.RequestPlayerCrouch();
            Step(host, input);
            Check(host.playerBodyHeight == 25 && host.worldObjects[1].collisionRadiusY == 13,
                "Native posture clamps must honor both itemValue and D500 gates.");
        }
    }

    void SetRoom(Host& host, std::uint8_t raw)
    {
        for (auto& row : host.level->roomHeights) { std::fill(std::begin(row), std::end(row), raw); }
    }

    void TestClearanceAndSpeed()
    {
        Host host;
        Input input;
        for (const auto raw : {49, 50})
        {
            host.Load();
            SetRoom(host, static_cast<std::uint8_t>(raw));
            Step(host, input);
            input.jump = true;
            Step(host, input);
            input.jump = false;
            Check(Eye(host) == (raw == 49 ? 45 : 51), "Jump ceiling cutoff must be strictly below 96.");
        }
        host.Load();
        SetRoom(host, 34);
        Step(host, input, 3);
        Check(host.playerBodyHeight == 28 && host.worldObjects[1].collisionRadiusY == 15,
            "A room below 65 must force body height toward 28 at step 8.");
        input.jump = true;
        Step(host, input, 10);
        Check(host.playerBodyHeight == 28, "Standing requests must not override a low ceiling.");
        input.jump = false;
        host.Load();
        SetRoom(host, 35);
        Step(host, input);
        Check(host.playerBodyHeight == 45, "Room height 66 must not force crouching.");

        for (const bool diagonal : {false, true})
        {
            host.Load();
            host.playerBodyHeight = 28;
            input.forward = true;
            input.strafe = diagonal;
            Step(host, input);
            Check(host.worldObjects[1].posZ == 2500 + (diagonal ? 1 : 2),
                "Crouched speed must be 2, or 1 on each diagonal axis.");
            host.Load();
            host.playerBodyHeight = 28;
            host.behaviorDispatchEnabled = false;
            Step(host, input);
            Check(host.worldObjects[1].posZ == 2503,
                "Crouched speed limit must honor the original DS:04CD gate.");
            host.behaviorDispatchEnabled = true;
        }
    }

    void TestWater()
    {
        Host host;
        Input input;
        for (const auto surface : {1, 3})
        {
            host.Load();
            std::fill(std::begin(host.collisionMap->data), std::end(host.collisionMap->data),
                static_cast<std::uint8_t>(surface));
            Step(host, input);
            Check(host.worldObjects[1].generalPurpose3 == 1 && host.playerPostureDelta == -6 &&
                !host.playerUnderwater && host.playerBodyHeight == 45,
                "Water contact must force crouch before necessarily becoming submerged.");
            input.jump = true;
            Step(host, input, 8);
            Check(host.playerBodyHeight == 28 && Eye(host) == 28,
                "Water contact must prevent jump/stand from escaping the forced crouch.");
            input.jump = false;
            std::fill(std::begin(host.collisionMap->data), std::end(host.collisionMap->data), std::uint8_t{0});
            Step(host, input);
            Check(host.worldObjects[1].generalPurpose3 == 0 && host.playerPostureDelta == 6,
                "Leaving water must clear the native latch and request standing.");
            Step(host, input, 6);
            Check(host.playerBodyHeight == 45 && !host.playerUnderwater, "Water exit must restore standing.");
        }
        host.Load();
        for (auto& row : host.level->floorHeights) { std::fill(std::begin(row), std::end(row), std::uint8_t{100}); }
        for (auto& row : host.level->surfaceFlags) { std::fill(std::begin(row), std::end(row), std::uint8_t{1}); }
        host.level->heightSurfaces[0] = Level::HeightSurface{100, 9};
        std::fill(std::begin(host.collisionMap->data), std::end(host.collisionMap->data), std::uint8_t{1});
        const auto floor = host.GetTerrainFloorHeight(1500, 0, 2500);
        host.worldObjects[1].posY = static_cast<std::int16_t>(floor + 20);
        Step(host, input);
        Check(host.playerUnderwater && host.floorSurfaceValue == 9 && host.worldObjects[1].generalPurpose3 == 1,
            "Submersion must use eye plus FELD surface depth, including the native metadata side effect.");
        input.forward = true;
        Step(host, input);
        Check(host.worldObjects[1].posZ == 2503, "Underwater speed 3 must override crouched speed 2.");
        host.worldObjects[1].posY = static_cast<std::int16_t>(host.worldObjects[1].posY + 100);
        Step(host, input);
        Check(!host.playerUnderwater && host.worldObjects[1].generalPurpose3 == 1,
            "Losing water support must clear submersion without inventing a latch-release transition.");
    }

    void TestLifecycleGates()
    {
        Input input;
        input.crouch = true;
        for (int gate = 0; gate < 7; ++gate)
        {
            Host host;
            host.Load();
            if (gate == 0) { host.SetPaused(true); }
            if (gate == 1) { host.SetMenuPage(MenuPage::MainMenu); }
            if (gate == 2) { host.basicPlayerMovementEnabled = false; }
            if (gate == 3) { host.worldObjects[1].health = 0; }
            if (gate == 4) { host.playerLife = 0; }
            if (gate == 5) { host.StartLevel(12); }
            const auto calls = input.postureCalls;
            host.Tick({0, static_cast<std::int16_t>(gate == 6 ? 0 : 1)}, input);
            Check(host.playerBodyHeight == 45 && input.postureCalls == calls,
                "Inactive, loading, paused, disabled and zero-frame controllers must not poll posture.");
            host.SetPaused(false);
            host.basicPlayerMovementEnabled = true;
            host.playerLife = 360 * 65536;
        }
        Host host;
        host.Load();
        host.ApplyPlayerLifeDelta(-host.playerLife);
        Check(host.playerPostureDelta == -1, "Life depletion must still install the native lowering request.");
        host.playerPostureDelta = 6;
        Step(host, input);
        Check(host.playerBodyHeight == 45, "Changing posture state must not revive a depleted player controller.");
    }
}

int main()
{
    try
    {
        TestJump();
        TestCrouchAndStand();
        TestClearanceAndSpeed();
        TestWater();
        TestLifecycleGates();
        std::cout << "Player jump/crouch tests passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
