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
        const auto calls = input.postureCalls;
        Step(host, input);
        Check(host.playerBodyHeight == 43 && input.postureCalls == calls,
            "Changing posture state must not revive a depleted player or resume posture input.");
    }

    void TestDeathCamera()
    {
        Host host;
        Input input;
        host.Load();
        host.worldObjects[1].velocityX = 100;
        host.worldObjects[1].velocityZ = 0;
        const auto initial = host.GetPlayerCamera();
        host.ApplyPlayerLifeDelta(-host.playerLife);
        Check(host.GetPlayerCamera().direction == initial.direction && host.worldObjects[1].velocityZ == 400,
            "The native death marker must not rotate the presented camera.");
        input.jump = input.crouch = input.forward = input.strafe = true;
        constexpr int collapse[]{43,40,36,31,25,18,15};
        for (std::size_t tick = 0; tick < std::size(collapse); ++tick)
        {
            Step(host, input);
            const auto camera = host.GetPlayerCamera();
            Check(host.playerBodyHeight == collapse[tick] && camera.position[1] == collapse[tick] &&
                camera.position[0] == initial.position[0] && camera.position[2] == initial.position[2] &&
                camera.direction == initial.direction && camera.lookHeight == 110 - 5 * (tick + 1) &&
                host.worldObjects[1].collisionRadiusY == 23 && host.worldObjects[1].velocityZ == 400 &&
                host.playerHitEffectTicks == 100 && input.postureCalls == 0,
                "Death must accelerate to height 15, lower the horizon without changing yaw and ignore movement input.");
        }
        Step(host, input, 13);
        Check(Eye(host) == 15 && host.playerLookHeight == 10,
            "Death must settle below crouch height at the original downward horizon.");
        Host batched;
        batched.Load();
        batched.ApplyPlayerLifeDelta(-batched.playerLife);
        Step(batched, input, 20);
        Check(Eye(batched) == Eye(host) && batched.playerLookHeight == host.playerLookHeight &&
            batched.playerPostureDelta == host.playerPostureDelta,
            "Batched and individual death ticks must follow the same collapse.");
        Step(batched, input, 32767);
        Step(batched, input, 20);
        Check(Eye(batched) == 15 && batched.playerLookHeight == 10,
            "Remaining dead must never wrap the lowering speed and raise the camera.");

        for (const std::int16_t height : std::array<std::int16_t,3>{28,45,100})
        {
            Host terrain;
            terrain.Load(height);
            terrain.playerBodyHeight = height;
            for (auto& row : terrain.level->floorHeights)
            {
                std::fill(std::begin(row), std::end(row), std::uint8_t{20});
            }
            const auto floor = terrain.GetTerrainFloorHeight(1500, height, 2500);
            terrain.playerLookHeight = 0;
            terrain.ApplyPlayerLifeDelta(-terrain.playerLife);
            Step(terrain, input, 30);
            Check(Eye(terrain) == floor + 15 && terrain.playerLookHeight == 10,
                "Crouched, standing and airborne deaths must settle relative to terrain and approach look from either side.");
        }

        for (int gate = 0; gate < 5; ++gate)
        {
            Host gated;
            gated.Load();
            gated.ApplyPlayerLifeDelta(-gated.playerLife);
            if (gate == 0) { gated.SetPaused(true); }
            if (gate == 1) { gated.SetMenuPage(MenuPage::MainMenu); }
            if (gate == 2) { gated.basicPlayerMovementEnabled = false; }
            if (gate == 3) { gated.StartLevel(12); }
            gated.Tick({20, static_cast<std::int16_t>(gate == 4 ? 0 : 20)}, input);
            Check(Eye(gated) == 45 && gated.playerLookHeight == 110 && gated.playerPostureDelta == -1,
                "Death camera must honor pause, menu, loading, disabled-controller and zero-frame gates.");
        }

        Host contact;
        contact.Load();
        std::fill(std::begin(contact.collisionMap->data), std::end(contact.collisionMap->data), std::uint8_t{192});
        Input neutral;
        Step(contact, neutral, 8);
        Check(contact.playerLife == 0 && Eye(contact) == 15 && contact.worldObjects[1].velocityZ == 400 &&
            contact.GetPlayerCamera().direction == std::array<std::int16_t,3>{0,0,100},
            "Lethal terrain mid-batch must switch remaining ticks to death, retaining the native marker and view direction.");

        host.playerLife = 360 * 65536;
        host.Load();
        Check(Eye(host) == 45 && host.playerLookHeight == 110 && host.playerPostureDelta == 0 &&
            host.GetPlayerCamera().direction == std::array<std::int16_t,3>{0,0,100},
            "A fresh installed level must clear the old death view.");
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
        TestDeathCamera();
        std::cout << "Player jump/crouch tests passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
