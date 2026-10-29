#include "GameContext.h"

#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace Desperabis;

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition) { throw std::runtime_error(message); }
    }

    struct HealthEvent
    {
        std::int32_t previous;
        std::int32_t current;
        std::int32_t observed;
        bool depleted;
        std::int16_t posture;
        std::int16_t deathDirection;
    };

    class Host final : public GameContext
    {
    public:
        std::vector<HealthEvent> events;

        void OnPlayerHealthChanged(std::int32_t previous, std::int32_t current) override
        {
            events.push_back({ previous, current, playerLife, positionalSoundsSuppressed,
                playerPostureDelta, worldObjects[1].velocityZ });
        }

        void Load()
        {
            const auto request = StartLevel(11);
            LevelLoadData data;
            data.level = std::make_unique<Level>(CreateDummyLevel());
            data.playerSpawn = PlayerCamera{{1500,45,2500}, {0,0,100}};
            CompleteLevelLoad(request.id, std::move(data));
        }

    protected:
        void LoadLevel(const LevelLoadRequest&) override {}
    };

    void TestChanges()
    {
        Host host;
        constexpr auto full = 360 * 65536;
        Check(host.events.empty() && host.playerLife == full,
            "Construction must retain native maximum 360 without calling a virtual health hook.");
        host.ApplyPlayerLifeDelta(0);
        host.ApplyPlayerLifeDelta(65536);
        Check(host.events.empty(), "Unchanged or upper-clamped full health must not notify.");
        host.ApplyPlayerLifeDelta(-1);
        Check(host.events.size() == 1 && host.events[0].previous == full &&
            host.events[0].current == full - 1 && host.events[0].observed == full - 1,
            "A fractional 16.16 change must notify immediately after committing life.");
        host.ApplyPlayerLifeDelta(-65535);
        Check(host.events.size() == 2 && host.events[1].previous == full - 1 &&
            host.events[1].current == full - 65536,
            "Consecutive changes before any Tick must each issue their own immediate notification.");
        host.ApplyPlayerLifeDelta(2 * 65536);
        Check(host.events.size() == 3 && host.events.back().current == full,
            "Healing must notify the final upper-clamped value, not the overflowing requested value.");
        host.playerLifeDeltaSuppressed = 1;
        host.ApplyPlayerLifeDelta(-100 * 65536);
        Check(host.events.size() == 3 && host.playerLife == full,
            "Suppressed deltas must not manufacture a health change.");
        host.playerLifeDeltaSuppressed = 0;
        host.playerLifeMaximum = 90;
        host.ApplyPlayerLifeDelta(0);
        Check(host.events.size() == 4 && host.events.back().previous == full &&
            host.events.back().current == 90 * 65536,
            "Clamping an altered maximum must still notify when the final life changes.");
        host.SetObjectHealth(host.worldObjects[1], 0);
        Check(host.events.size() == 4,
            "The player object's independent health word must not trigger player-life notifications.");
    }

    void TestDepletion()
    {
        Host host;
        host.ApplyPlayerLifeDelta(-400 * 65536);
        Check(host.events.size() == 1 && host.events[0].current == 0 &&
            host.events[0].observed == 0 && host.events[0].depleted &&
            host.events[0].posture == -1 && host.events[0].deathDirection == 400 &&
            host.playerHitEffectTicks == 1000,
            "A lethal change must notify only after installing the complete native depletion state.");
        host.ApplyPlayerLifeDelta(-1);
        host.ApplyPlayerLifeDelta(0);
        Check(host.events.size() == 1, "Repeated depletion at zero must not emit duplicate health changes.");

        Host wrapped;
        wrapped.playerLife = (std::numeric_limits<std::int32_t>::max)();
        wrapped.ApplyPlayerLifeDelta(1);
        Check(wrapped.events.size() == 1 && wrapped.playerLife == 0 &&
            wrapped.events[0].previous == (std::numeric_limits<std::int32_t>::max)(),
            "Notifications must preserve the existing native dword-wrap then clamp semantics.");
    }

    void TestNoCollisionCheat()
    {
        Host host;
        host.Load();
        Check(!host.DEBUG_noPlayerCollision, "The player collision cheat must default to off.");
        host.ApplyPlayerLifeDelta(-10 * 65536);
        const auto life = host.playerLife;
        const auto events = host.events.size();
        host.DEBUG_noPlayerCollision = true;
        host.ApplyPlayerLifeDelta(-1);
        host.ApplyPlayerLifeDelta(-(std::numeric_limits<std::int32_t>::max)());
        RawGameObject creature;
        creature.behaviorParameter = 255;
        creature.doorLockOrGroupBitmask = 1;
        host.HandleCockroachContact(creature);
        Check(host.playerLife == life && host.events.size() == events && !host.positionalSoundsSuppressed &&
            host.playerPostureDelta == 0,
            "The cheat must suppress fractional, lethal and gameplay damage without depletion or health callbacks.");
        host.ApplyPlayerLifeDelta(65536);
        Check(host.playerLife == life + 65536 && host.events.size() == events + 1,
            "The collision cheat must still permit healing and its notification.");
        host.Load();
        Check(host.DEBUG_noPlayerCollision && host.playerLife == life + 65536,
            "The host cheat must remain enabled across level loads without altering life.");
        host.DEBUG_noPlayerCollision = false;
        host.ApplyPlayerLifeDelta(-1);
        Check(host.playerLife == life + 65535 && host.events.size() == events + 2,
            "Disabling the cheat must restore ordinary player damage immediately.");
    }

    void TestGameplayAndLoading()
    {
        Host host;
        host.Load();
        Check(host.events.empty(), "Level installation preserves life and must not invent a health change.");
        RawGameObject creature;
        creature.behaviorParameter = 2;
        creature.doorLockOrGroupBitmask = 1;
        host.HandleCockroachContact(creature);
        Check(host.events.size() == 1 && host.events[0].previous - host.events[0].current == 2 * 29491,
            "Existing gameplay damage must flow through the health notification without polling.");
        const auto life = host.playerLife;
        host.Load();
        Check(host.events.size() == 1 && host.playerLife == life,
            "Ordinary level transitions must preserve health without a spurious notification.");
        host.ApplyPlayerLifeDelta(29491);
        Check(host.events.size() == 2 && host.events.back().current == life + 29491,
            "Health changes must continue notifying after a level transition.");
    }
}

int main()
{
    try
    {
        TestChanges();
        TestDepletion();
        TestGameplayAndLoading();
        TestNoCollisionCheat();
        std::cout << "Player health notification tests passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
