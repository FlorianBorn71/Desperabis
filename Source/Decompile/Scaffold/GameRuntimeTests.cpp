#include "GameContext.h"
#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <functional>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <map>
#include <regex>
#include <stdexcept>
#include <string>
#include <type_traits>
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
        Check(caught, "Expected an explicit runtime integration error.");
    }

    LevelLoadData MakeLevel()
    {
        LevelLoadData data;
        data.level = std::make_unique<Level>(CreateDummyLevel());
        data.objects[1].health = 1000;
        data.objects[1].field_07 = 1;
        data.objects[1].collisionRadiusXZ = 10;
        data.objects[1].collisionRadiusY = 10;
        return data;
    }

    class Host final : public GameContext
    {
    public:
        std::vector<LevelLoadRequest> requests;
        bool synchronous = false;
        bool fail = false;
        std::function<std::vector<std::uint8_t>(std::string_view)> files;
        std::optional<PlayerCamera> playerSpawn;
        std::array<std::int16_t, 3> initialDirection{ 0, 0, 100 };
        std::string userMessage;
        std::int16_t userMessageTicks = 0;
        int promptCalls = 0;

        void PromptUserMessage(std::string_view text, std::int16_t duration) override
        {
            userMessage = text;
            userMessageTicks = duration;
            ++promptCalls;
        }

        bool IsUserMessageActive() const override { return userMessageTicks > 0; }

    protected:
        std::vector<std::uint8_t> LoadFile(std::string_view filename) override
        {
            return files ? files(filename) : GameContext::LoadFile(filename);
        }

        void LoadLevel(const LevelLoadRequest& request) override
        {
            requests.push_back(request);
            if (fail) { throw std::runtime_error("Host asset loading failed."); }
            if (files)
            {
                LevelLoadData data;
                LoadLevelGeometry(request.levelNumber, data);
                LoadLevelObjects(request.levelNumber, data);
                if (playerSpawn) { data.playerSpawn = playerSpawn; }
                else { InitializeLevelPlayerSpawn(data, initialDirection); }
                CompleteLevelLoad(request.id, std::move(data));
            }
            else if (synchronous) { CompleteLevelLoad(request.id, MakeLevel()); }
        }
    };

    class Input final : public InputHandler
    {
    public:
        bool pressed = false;
        int calls = 0;
        std::array<bool, 6> movement{};
        int movementCalls = 0;
        bool jump = false;
        bool crouch = false;
        int postureCalls = 0;
        bool IsKeyPressed(InputKey key) override
        {
            if (key == InputKey::JumpKey || key == InputKey::CrouchKey)
            {
                ++postureCalls;
                return key == InputKey::JumpKey ? jump : crouch;
            }
            if (key == InputKey::AttackKey || key == InputKey::SelectDaggerKey || key == InputKey::SelectSwordKey ||
                (key >= InputKey::SelectCrossbowKey && key <= InputKey::SelectLentilsKey))
            {
                return false;
            }
            if (key != InputKey::InteractionKey)
            {
                ++movementCalls;
                return movement.at(static_cast<std::size_t>(key));
            }
            ++calls;
            return pressed;
        }
    };

    struct ObjectEvent
    {
        bool created;
        int slot;
        RawGameObject object;
        const Level* level;
        GameRuntimeState state;
    };

    class LifecycleHost final : public GameContext
    {
    public:
        std::vector<ObjectEvent> events;

        void OnObjectCreated(int slot) override { Record(true, slot); }
        void OnObjectDestroyed(int slot) override { Record(false, slot); }

    protected:
        void LoadLevel(const LevelLoadRequest&) override {}

    private:
        void Record(bool created, int slot)
        {
            events.push_back({ created, slot, GetWorldObjects().at(static_cast<std::size_t>(slot)),
                GetLevel(), GetRuntimeState() });
        }
    };

    bool SameObject(const RawGameObject& left, const RawGameObject& right)
    {
        return std::memcmp(&left, &right, sizeof(left)) == 0;
    }

    RawGameObject LifecycleObject(std::int16_t health = 1000)
    {
        RawGameObject object;
        object.health = health;
        object.posX = 150;
        object.posZ = 4050;
        object.modelId = 39;
        return object;
    }

    void CheckObjectEvent(const LifecycleHost& host, std::size_t index, bool created,
        int slot, const RawGameObject& object)
    {
        const auto& event = host.events.at(index);
        Check(event.created == created && event.slot == slot && SameObject(event.object, object),
            "Lifecycle hook must synchronously observe the correct slot and complete record.");
    }

    void TestWorldObjectLifecycle()
    {
        GameContext defaults;
        defaults.OnObjectCreated(2);
        defaults.OnObjectDestroyed(2);
        LifecycleHost host;
        const auto object = LifecycleObject();
        host.ReplaceWorldObject(2, object);
        CheckObjectEvent(host, 0, true, 2, object);
        host.ReplaceWorldObject(2, object);
        Check(host.events.size() == 3, "Same-slot replacement must destroy then create, even for identical bytes.");
        CheckObjectEvent(host, 1, false, 2, object);
        CheckObjectEvent(host, 2, true, 2, object);
        host.SetObjectHealth(host.worldObjects[2], 500);
        Check(host.events.size() == 3, "Positive health changes are not lifecycle changes.");
        const auto dying = host.worldObjects[2];
        host.SetObjectHealth(host.worldObjects[2], -1);
        CheckObjectEvent(host, 3, false, 2, dying);
        host.SetObjectHealth(host.worldObjects[2], 0);
        host.ReplaceWorldObject(2, {});
        Check(host.events.size() == 4, "An already inactive object must not be destroyed twice.");
        host.SetObjectHealth(host.worldObjects[2], 10);
        CheckObjectEvent(host, 4, true, 2, host.worldObjects[2]);
        host.ReplaceWorldObject(2, {});
        Check(host.events.size() == 6 && host.worldObjects[2].health == 0,
            "Replacing with an empty record must remove without creating.");

        auto scratch = object;
        host.inventory[0] = object;
        host.worldObjects[0] = object;
        host.SetObjectHealth(scratch, 0);
        host.SetObjectHealth(host.inventory[0], 0);
        host.SetObjectHealth(host.worldObjects[0], 0);
        host.InitializeDriftingParticle(scratch, 2, 150, 4050);
        Check(host.events.size() == 6, "Scratch, inventory and reserved slot zero are not world lifecycles.");
        ExpectError<std::out_of_range>([&] { host.ReplaceWorldObject(0, object); });
        ExpectError<std::out_of_range>([&] { host.ReplaceWorldObject(166, object); });
        ExpectError<std::out_of_range>([&] { host.ReplaceWorldObject(-1, object); });

        host.worldObjects[2] = object;
        Check(host.events.size() == 6, "Public raw writes deliberately bypass lifecycle notification.");
        host.ReplaceWorldObject(2, host.worldObjects[2]);
        CheckObjectEvent(host, 6, false, 2, object);
        CheckObjectEvent(host, 7, true, 2, object);
    }

    void TestLevelObjectLifecycle()
    {
        LifecycleHost host;
        auto data = MakeLevel();
        data.objects[2] = LifecycleObject();
        data.objects[165] = LifecycleObject(10);
        auto request = host.StartLevel(11);
        host.CompleteLevelLoad(request.id, std::move(data));
        Check(host.events.size() == 3 && host.events[0].slot == 1 &&
            host.events[1].slot == 2 && host.events[2].slot == 165,
            "Initial load must announce player, ordinary and fallback active slots, not inactive slots.");
        for (const auto& event : host.events)
        {
            Check(event.created && event.level == host.GetLevel() && event.state == GameRuntimeState::Ready,
                "Loaded-object creation must observe installed geometry and ready state.");
        }
        const auto* oldLevel = host.GetLevel();
        const auto oldObjects = host.worldObjects;
        host.events.clear();
        request = host.StartLevel(12);
        ExpectError<std::invalid_argument>([&] { host.CompleteLevelLoad(request.id, {}); });
        Check(host.events.empty(), "Rejected loads must not destroy the live world.");
        auto replacement = MakeLevel();
        replacement.objects[2] = LifecycleObject(20);
        replacement.objects[3] = LifecycleObject(30);
        const auto newObjects = replacement.objects;
        host.CompleteLevelLoad(request.id, std::move(replacement));
        Check(host.events.size() == 6, "Level replacement must destroy all old and create all new active slots.");
        for (std::size_t index = 0; index < 3; ++index)
        {
            const int slot = index == 2 ? 165 : static_cast<int>(index + 1);
            CheckObjectEvent(host, index, false, slot, oldObjects[static_cast<std::size_t>(slot)]);
            Check(host.events[index].level == oldLevel, "Level destruction must precede geometry replacement.");
            CheckObjectEvent(host, index + 3, true, static_cast<int>(index + 1), newObjects[index + 1]);
        }
        host.events.clear();
        request = host.StartLevel(13);
        host.FailLevelLoad(request.id, "expected");
        Check(host.events.empty(), "Load failure must preserve world lifetimes.");
    }

    void TestImmediateObjectRemoval()
    {
        LifecycleHost host;
        auto object = LifecycleObject(1);
        object.behavior = BehaviorType::IdleAnimation;
        object.itemValue = 3;
        host.ReplaceWorldObject(2, object);
        host.events.clear();
        Check(UpdateObject(host.worldObjects[2], host).status == DispatchStatus::ExecutedCase &&
            host.worldObjects[2].health == 0 && host.worldObjects[2].blendWeightOrSpriteOpacity == 3,
            "Expiry must still dispatch the native handler at zero health.");
        Check(host.events.size() == 1, "Countdown expiry must notify immediately, exactly once.");
        CheckObjectEvent(host, 0, false, 2, object);
        UpdateObject(host.worldObjects[2], host);
        Check(host.events.size() == 1, "Later updates of expired objects must not repeat destruction.");

        object = LifecycleObject();
        object.behavior = BehaviorType::DisabledSentinel;
        host.ReplaceWorldObject(2, object);
        host.events.clear();
        DispatchObjectCase(host.worldObjects[2], host);
        CheckObjectEvent(host, 0, false, 2, object);
        Check(host.events.size() == 1, "Direct gameplay dispatch must notify outside Tick.");

        object.collisionType = CollisionType::FlyingArrow;
        host.ReplaceWorldObject(2, object);
        host.events.clear();
        host.DamageObject(host.worldObjects[2], 1);
        Check(host.events.size() == 1 && !host.events[0].created && host.events[0].object.health == 1000,
            "Damage destruction must expose the live record before deactivation.");

        object.collisionType = CollisionType::Gold;
        object.itemValue = 7;
        host.ReplaceWorldObject(2, object);
        host.events.clear();
        Check(HandlePlayerObjectInteraction(host.worldObjects[2], host).status == InteractionStatus::Applied &&
            host.gold == 7, "World pickup must retain native inventory/resource behavior.");
        CheckObjectEvent(host, 0, false, 2, object);
        Check(host.events.size() == 1, "World pickup must emit only one world destruction.");

        host.ReplaceWorldObject(2, object);
        host.events.clear();
        host.discardBlockedObjects = true;
        host.HandleBlockedObject(host.worldObjects[2]);
        CheckObjectEvent(host, 0, false, 2, object);

        object = LifecycleObject(1);
        object.behavior = BehaviorType::WeaponDamageCollider;
        object.field_07 = 1;
        object.collisionRadiusXZ = object.collisionRadiusY = 10;
        host.ReplaceWorldObject(2, object);
        auto target = object;
        target.collisionType = CollisionType::PushingEnemy;
        host.ReplaceWorldObject(3, target);
        host.events.clear();
        UpdateObject(host.worldObjects[2], host);
        Check(host.events.size() == 2 && host.worldObjects[2].behavior == BehaviorType::DriftingParticle,
            "A contact expiring then becoming an impact particle must destroy then create without duplicate removal.");
        CheckObjectEvent(host, 0, false, 2, object);
        CheckObjectEvent(host, 1, true, 2, host.worldObjects[2]);
    }

    void TestObjectFactoryLifecycle()
    {
        LifecycleHost host;
        auto terrain = CreateDummyLevel();
        host.level = &terrain;
        host.collisionMap = terrain.collisionMap.get();
        auto source = LifecycleObject();
        source.collisionType = CollisionType::DamageObj;
        source.collisionRadiusY = 10;
        host.ReplaceWorldObject(2, source);
        host.events.clear();
        const auto slot = host.SpawnFireParticle(host.worldObjects[2]);
        Check(slot == 3 && host.events.size() == 1, "Factories called outside Tick must announce creation.");
        CheckObjectEvent(host, 0, true, 3, host.worldObjects[3]);
        Check(host.events[0].object.behavior == BehaviorType::FireParticle &&
            host.events[0].object.collisionRadiusY == 10, "Creation must see final factory fields.");

        host.events.clear();
        const auto oldParticle = host.worldObjects[3];
        for (std::size_t index = 4; index <= 164; ++index) { host.worldObjects[index] = LifecycleObject(); }
        Check(host.FindFreeOrLowestHealthObjectIndex() == 3, "Reclaiming allocation must preserve native selection.");
        CheckObjectEvent(host, 0, false, 3, oldParticle);
        Check(SameObject(host.worldObjects[3], RawGameObject{}), "Allocation must clear only after destruction.");
        host.InitializeSmokeParticle(host.worldObjects[3], 150, -100, 4050, 5, 1, 6);
        Check(host.events.size() == 2, "Reclaim followed by initialization must notify destroy then create.");
        CheckObjectEvent(host, 1, true, 3, host.worldObjects[3]);
        Check(host.events[1].object.posY == 0 && host.events[1].object.renderPlacementMode == 6,
            "Smoke creation must follow floor correction and placement initialization.");
        for (std::size_t index = 4; index <= 164; ++index) { host.worldObjects[index] = {}; }

        host.events.clear();
        source.collisionType = CollisionType::Skeleton;
        host.SpawnSkeletonFragment(source, 6, 1, 2, 3);
        Check(host.events.size() == 1, "Skeleton fragments must announce exactly one initialized world record.");
        CheckObjectEvent(host, 0, true, 4, host.worldObjects[4]);
        Check(host.events[0].object.collisionType == CollisionType::Weapon,
            "Skeleton weapon fragment creation must follow its special-part initialization.");
        host.events.clear();
        host.SpawnWaterImpact(150, 4050);
        Check(host.events.size() == 1, "Water allocation must announce the initialized splash.");
        CheckObjectEvent(host, 0, true, 5, host.worldObjects[5]);

        for (std::size_t index = 2; index < host.worldObjects.size(); ++index)
        {
            host.worldObjects[index] = LifecycleObject();
        }
        host.events.clear();
        const auto fallback = host.worldObjects[165];
        Check(host.FindFreeObjectIndex() == 165, "Exhausted ordinary allocation must preserve fallback slot 165.");
        CheckObjectEvent(host, 0, false, 165, fallback);
        host.InitializeDriftingParticle(host.worldObjects[165], 4, 150, 4050);
        CheckObjectEvent(host, 1, true, 165, host.worldObjects[165]);
        host.events.clear();
        const auto impact = host.worldObjects[165];
        Check(host.FindFreeOrLowestHealthObjectIndex() == 165,
            "Reclaiming allocation must preserve native fallback when all ordinary health values are 1000.");
        CheckObjectEvent(host, 0, false, 165, impact);

        LifecycleHost crater;
        crater.level = &terrain;
        crater.collisionMap = terrain.collisionMap.get();
        auto emitter = LifecycleObject(1);
        emitter.behavior = BehaviorType::SpawnInCrater;
        emitter.doorLockOrGroupBitmask = 1001;
        crater.ReplaceWorldObject(2, emitter);
        crater.events.clear();
        ExpectError<std::out_of_range>([&] { UpdateObject(crater.worldObjects[2], crater); });
        Check(crater.events.size() == 1, "Failed source-slot initialization must not announce an initialized replacement.");
        CheckObjectEvent(crater, 0, false, 2, emitter);
        Check(crater.worldObjects[2].behavior == BehaviorType::SmokeFading && crater.worldObjects[2].posX == 0,
            "Lifecycle hooks must preserve native partial initialization after source-slot clearing.");

        host.events.clear();
        host.InitializeDriftingParticle(host.worldObjects[165], 3, 150, 4050);
        host.events.clear();
        host.InitializeSmokeParticle(host.worldObjects[165], 150, 0, 4050, 5, -1, 1);
        Check(host.events.size() == 1 && !host.events[0].created && host.events[0].slot == 165 &&
            host.events[0].object.health == 50 && host.worldObjects[165].health <= 0,
            "An initializer producing nonpositive health must destroy before its health transition.");
        host.events.clear();
        host.InitializeSmokeParticle(host.worldObjects[165], 150, 0, 4050, 5, -1, 1);
        Check(host.events.empty(), "Born-inactive initializers must not publish spurious creation or destruction.");
    }

    void TestDamageFactoryLifecycle()
    {
        for (const auto type : { CollisionType::ActivateSpiders, CollisionType::Type_22,
            CollisionType::Skeleton, CollisionType::VaseToGrab })
        {
            LifecycleHost host;
            auto terrain = CreateDummyLevel();
            host.level = &terrain;
            host.collisionMap = terrain.collisionMap.get();
            host.worldObjects[1] = LifecycleObject();
            auto source = LifecycleObject();
            source.collisionType = type;
            source.groupIndexOrLock = 1;
            source.field_24 = 1;
            source.field_28 = 9;
            host.ReplaceWorldObject(2, source);
            host.events.clear();
            host.DamageObject(host.worldObjects[2], type == CollisionType::Type_22 ? 0 : 200);
            const std::size_t creations = type == CollisionType::ActivateSpiders ? 20 :
                type == CollisionType::Type_22 ? 9 : type == CollisionType::Skeleton ? 8 : 1;
            const bool destroysSource = type == CollisionType::Type_22 || type == CollisionType::Skeleton;
            Check(host.events.size() == creations + (destroysSource ? 1 : 0),
                "Damage factories must publish all fragments before source destruction, without duplicate events.");
            for (std::size_t index = 0; index < creations; ++index)
            {
                CheckObjectEvent(host, index, true, static_cast<int>(index + 3), host.worldObjects[index + 3]);
            }
            if (destroysSource)
            {
                Check(!host.events.back().created && host.events.back().slot == 2 &&
                    host.events.back().object.health == 1000 && host.worldObjects[2].health == 0,
                    "Damage-source destruction must see its old live record after fragment creation.");
            }
        }
    }

    void TestWeaponCreationLifecycle()
    {
        for (const auto weapon : { WeaponType::Dagger, WeaponType::Crossbow, WeaponType::Fireball,
            WeaponType::Stone, WeaponType::Lentils })
        {
            LifecycleHost host;
            auto terrain = CreateDummyLevel();
            host.level = &terrain;
            host.collisionMap = terrain.collisionMap.get();
            host.worldObjects[1] = LifecycleObject();
            host.worldObjects[1].velocityZ = 100;
            host.selectedWeapon = host.requestedWeapon = weapon;
            host.weaponHand = 1;
            host.inventory[0] = LifecycleObject();
            host.inventory[0].collisionType = CollisionType::Weapon;
            host.inventory[0].itemValue = static_cast<std::int16_t>(weapon);
            host.weaponAmounts[static_cast<std::size_t>(weapon)] = weapon == WeaponType::Lentils ? 8 : 10;
            Check(host.AttackWithSelectedWeapon() == WeaponActionStatus::Applied,
                "Weapon lifecycle fixture must produce its native projectile/contact.");
            const std::size_t count = weapon == WeaponType::Lentils ? 3 : 1;
            Check(host.events.size() == count, "Weapon factories must emit one event per projectile, no inventory destruction.");
            for (std::size_t index = 0; index < count; ++index)
            {
                CheckObjectEvent(host, index, true, static_cast<int>(index + 2), host.worldObjects[index + 2]);
            }
        }

        LifecycleHost host;
        auto terrain = CreateDummyLevel();
        host.level = &terrain;
        host.collisionMap = terrain.collisionMap.get();
        host.worldObjects[1] = LifecycleObject();
        host.worldObjects[1].velocityZ = 100;
        host.soundListenerX = 150;
        host.soundListenerZ = 4050;
        auto target = LifecycleObject();
        target.posX = 250;
        target.field_07 = 1;
        target.collisionType = CollisionType::FlyingArrow;
        host.ReplaceWorldObject(3, target);
        host.selectedWeapon = host.requestedWeapon = WeaponType::Electricity;
        host.weaponAmounts[7] = 100;
        host.events.clear();
        Check(host.AttackWithSelectedWeapon() == WeaponActionStatus::Applied && host.events.size() == 3,
            "Electric beam creation, target damage destruction and impact creation must notify within one call.");
        CheckObjectEvent(host, 0, true, 2, host.worldObjects[2]);
        CheckObjectEvent(host, 1, false, 3, target);
        CheckObjectEvent(host, 2, true, 3, host.worldObjects[3]);
    }

    void TestLoading()
    {
        static_assert(!std::is_copy_constructible_v<GameContext>);
        static_assert(std::is_nothrow_move_constructible_v<GameContext>);
        Host host;
        Input input;
        Check(host.GetRuntimeState() == GameRuntimeState::NoLevel && !host.GetLevel() &&
            !host.GetPendingLevelLoad() && !host.GetLoadedLevelNumber() &&
            host.Tick({}, input).status == GameTickStatus::NoLevel,
            "A host context must start idle without calling modal pages.");
        ExpectError<std::invalid_argument>([&] { host.StartLevel(0); });
        const auto request = host.StartLevel(12);
        Check(host.requests.size() == 1 && request.id != 0 && request.levelNumber == 12 &&
            host.GetPendingLevelLoad()->id == request.id &&
            host.Tick({}, input).status == GameTickStatus::Loading && input.calls == 0,
            "Starting a level must invoke the host once and suspend simulation.");
        ExpectError<std::logic_error>([&] { host.StartLevel(13); });
        ExpectError<std::logic_error>([&] { host.CompleteLevelLoad(request.id + 1, MakeLevel()); });
        ExpectError<std::invalid_argument>([&] { host.CompleteLevelLoad(request.id, {}); });
        auto missingMap = MakeLevel();
        missingMap.level->collisionMap.reset();
        ExpectError<std::invalid_argument>([&] { host.CompleteLevelLoad(request.id, std::move(missingMap)); });
        auto overflow = MakeLevel();
        overflow.objects[1].posX = 32767;
        ExpectError<std::overflow_error>([&] { host.CompleteLevelLoad(request.id, std::move(overflow)); });
        Check(!host.GetLevel() && host.GetRuntimeState() == GameRuntimeState::Loading,
            "Rejected level data must not partially install a world.");
        auto data = MakeLevel();
        const auto* owned = data.level.get();
        data.objects[2].health = 1000;
        data.objects[2].behavior = BehaviorType::PulsatingLight;
        data.floorRepeatHeight = 25;
        host.gold = 42;
        host.randomSeed = 123;
        host.userMessage = "old level";
        host.userMessageTicks = 10;
        host.switchGroupMasks[2] = 0;
        host.CompleteLevelLoad(request.id, std::move(data));
        Check(host.GetRuntimeState() == GameRuntimeState::Ready && host.GetLevel() == owned &&
            host.level == owned && host.collisionMap == owned->collisionMap.get() &&
            host.GetLoadedLevelNumber() == 12 && !host.GetPendingLevelLoad() &&
            host.GetMenuPage() == MenuPage::Gameplay && host.hasActiveGame &&
            host.GetWorldObjects()[2].behavior == BehaviorType::PulsatingLight &&
            host.floorRepeatHeight == 25 && host.gold == 42 && host.randomSeed == 123 &&
            host.userMessage.empty() && host.userMessageTicks == 0 && host.switchGroupMasks[2] == 0xff,
            "Completion must transfer ownership, install per-level state, and preserve session inventory/RNG.");
        ExpectError<std::logic_error>([&] { host.CompleteLevelLoad(request.id, MakeLevel()); });

        const auto next = host.StartLevel(13);
        Check(next.id > request.id && host.GetLevel() == owned &&
            host.Tick({}, input).status == GameTickStatus::Loading,
            "A transition must retain the previous world for display but suspend gameplay.");
        ExpectError<std::invalid_argument>([&] { host.FailLevelLoad(next.id, ""); });
        ExpectError<std::logic_error>([&] { host.FailLevelLoad(request.id, "stale"); });
        host.FailLevelLoad(next.id, "Missing level asset");
        Check(host.GetRuntimeState() == GameRuntimeState::LoadFailed &&
            host.GetLevelLoadError() == "Missing level asset" && host.GetLevel() == owned &&
            host.Tick({}, input).status == GameTickStatus::LoadFailed,
            "Async failures must be visible and must not resume the previous simulation silently.");
        const auto retry = host.StartLevel(13);
        ExpectError<std::logic_error>([&] { host.CompleteLevelLoad(next.id, MakeLevel()); });
        host.CompleteLevelLoad(retry.id, MakeLevel());
        Check(host.GetLoadedLevelNumber() == 13 && host.GetLevelLoadError().empty(),
            "Retries require a fresh request ID and clear an acknowledged error.");

        Host synchronous;
        synchronous.synchronous = true;
        synchronous.StartLevel(10);
        Check(synchronous.GetRuntimeState() == GameRuntimeState::Ready,
            "Host loading may complete synchronously.");
        Host throwing;
        throwing.fail = true;
        ExpectError<std::runtime_error>([&] { throwing.StartLevel(10); });
        Check(throwing.GetRuntimeState() == GameRuntimeState::LoadFailed &&
            throwing.GetLevelLoadError() == "Host asset loading failed.",
            "Synchronous host errors must be recorded and rethrown.");
        GameContext noHost;
        ExpectError<std::logic_error>([&] { noHost.StartLevel(10); });
        Check(noHost.GetRuntimeState() == GameRuntimeState::LoadFailed,
            "A missing loader must fail explicitly, without requiring UI virtual overrides.");
    }

    void TestLevelPlayerInitialization()
    {
        Host host;
        auto data = MakeLevel();
        data.objects[1].itemValue = 123;
        data.playerSpawn = PlayerCamera{ { 1500, 45, 2500 }, { -100, 0, 0 } };
        const auto request = host.StartLevel(10);
        host.CompleteLevelLoad(request.id, std::move(data));
        const auto& player = host.GetWorldObjects()[1];
        Check(player.health == 1000 && player.field_07 == 3 &&
            player.collisionRadiusXZ == 15 && player.collisionRadiusY == 23 &&
            player.posX == 1500 && player.posY == 20 && player.posZ == 2500 &&
            player.velocityX == -100 && player.velocityY == 0 && player.velocityZ == 0 &&
            player.itemValue == 0 &&
            host.GetPlayerCamera().position == std::array<std::int16_t, 3>{ 1500, 45, 2500 },
            "GameContext must initialize a clean normal player and convert spawn eye Y into object-center Y.");
        data = MakeLevel();
        data.playerSpawn = PlayerCamera{ { 1500, -32768, 2500 }, { 0, 0, 100 } };
        const auto wrapped = host.StartLevel(11);
        host.CompleteLevelLoad(wrapped.id, std::move(data));
        Check(host.GetWorldObjects()[1].posY == 32743 && host.GetPlayerCamera().position[1] == -32768,
            "Fresh-level eye/center conversion must wrap native signed words.");
        data = MakeLevel();
        data.objects[1].field_07 = 0x80;
        data.objects[1].collisionRadiusXZ = 7;
        data.objects[1].collisionRadiusY = 15;
        data.objects[1].health = 345;
        const auto restored = data.objects[1];
        const auto restore = host.StartLevel(12);
        host.CompleteLevelLoad(restore.id, std::move(data));
        Check(std::memcmp(&restored, &host.GetWorldObjects()[1], sizeof(restored)) == 0,
            "Loading an explicit player record without a spawn must preserve its state, including alternate collision radii.");
    }

    void TestTick()
    {
        Host host;
        host.basicPlayerMovementEnabled = false;
        const auto request = host.StartLevel(10);
        auto data = MakeLevel();
        auto& gold = data.objects[2];
        gold = data.objects[1];
        gold.collisionType = CollisionType::Gold;
        gold.itemValue = 7;
        auto& light = data.objects[3];
        light.health = 1000;
        light.behavior = BehaviorType::PulsatingLight;
        light.posX = 100;
        data.objects[4].health = 1000;
        data.objects[4].behavior = BehaviorType::Frog;
        data.objects[5].health = 1000;
        data.objects[5].displayType = static_cast<DisplayType>(60);
        host.CompleteLevelLoad(request.id, std::move(data));
        host.PromptUserMessage("message", 20);
        host.field_DC34 = 0xffff;
        host.lightingCache.contributions[0][0] = 99;
        Input input;
        input.pressed = true;
        auto expectedSeed = host.randomSeed;
        const auto nextRandom = [](std::uint32_t& seed, unsigned int range)
        {
            seed = static_cast<std::uint32_t>(static_cast<std::uint64_t>(seed) * 134775813u + 1u);
            return (seed >> 16) % range;
        };
        if (nextRandom(expectedSeed, 15) == 0) { nextRandom(expectedSeed, 10); }
        const auto expectedDisplay = 60 - nextRandom(expectedSeed, 4);
        const auto result = host.Tick({ 0, 3 }, input);
        Check(result.status == GameTickStatus::Advanced && result.interaction && result.objects &&
            result.interaction->status == InteractionStatus::Applied && input.calls == 1 &&
            host.gold == 7 && host.worldObjects[2].health == 0 &&
            (*result.objects)[0].status == DispatchStatus::InactiveObject &&
            (*result.objects)[1].status == DispatchStatus::ExecutedCase &&
            (*result.objects)[2].status == DispatchStatus::UnimplementedHandler,
            "Tick must run input/contact before objects and expose unrecovered behavior results.");
        Check(host.tickCount == 0 && host.frameTickCount == 3 && host.userMessageTicks == 20 &&
            static_cast<std::uint8_t>(host.worldObjects[5].displayType) == expectedDisplay &&
            host.randomSeed == expectedSeed &&
            host.GetLightingCache().contributions[11][10] == 30 &&
            host.GetLightingCache().contributions[0][0] == 0 && host.field_DC34 == 0,
            "Frame and behavior clocks must remain distinct; lighting/noise reset once per advanced tick.");
        const auto seed = host.randomSeed;
        host.Tick({ 0, 0 }, input);
        Check(host.randomSeed != seed && host.userMessageTicks == 20 &&
            host.GetLightingCache().contributions[11][10] == 30,
            "Zero native ticks must still update once; successive frames must not accumulate old lighting.");

        const auto before = host.GetWorldObjects();
        const auto cache = host.GetLightingCache().contributions;
        const auto pausedSeed = host.randomSeed;
        const auto calls = input.calls;
        host.SetPaused(true);
        const auto paused = host.Tick({ 10, 10 }, input);
        Check(host.IsPaused() && paused.status == GameTickStatus::Paused &&
            !paused.interaction && !paused.objects && host.userMessageTicks == 20,
            "Explicit pause must return without simulation or input.");
        host.SetPaused(false);
        host.SetMenuPage(MenuPage::Inventory);
        Check(host.Tick({}, input).status == GameTickStatus::Paused &&
            std::memcmp(before.data(), host.GetWorldObjects().data(), sizeof(before)) == 0 &&
            host.GetLightingCache().contributions == cache && host.randomSeed == pausedSeed && input.calls == calls,
            "Menus must suspend gameplay without calling modal UI hooks.");
        host.SetMenuPage(MenuPage::Gameplay);
        Check(host.Tick({}, input).status == GameTickStatus::Advanced,
            "Returning from a menu must allow gameplay to resume.");
        ExpectError<std::invalid_argument>([&] { host.Tick({ -1, 1 }, input); });
        ExpectError<std::invalid_argument>([&] { host.Tick({ 1, -1 }, input); });
        host.level = nullptr;
        ExpectError<std::logic_error>([&] { host.Tick({}, input); });
    }

    void TestPlayerCamera()
    {
        Host host;
        const GameContext& reader = host;
        Check(reader.GetPlayerCamera().position == std::array<std::int16_t, 3>{} &&
            reader.GetPlayerCamera().direction == std::array<std::int16_t, 3>{},
            "Camera getter must expose the initial player record without inventing a default orientation.");
        const auto request = host.StartLevel(10);
        auto data = MakeLevel();
        data.objects[1].posX = 150;
        data.objects[1].posY = -42;
        data.objects[1].posZ = 4050;
        data.objects[1].velocityX = -256;
        data.objects[1].velocityY = 64;
        data.objects[1].velocityZ = 128;
        data.objects[1].winX = 17;
        data.objects[1].winY = 29;
        data.objects[1].winZ = 31;
        host.CompleteLevelLoad(request.id, std::move(data));
        const auto camera = reader.GetPlayerCamera();
        Check(camera.position == std::array<std::int16_t, 3>{ 150, -30, 4050 } &&
            camera.direction == std::array<std::int16_t, 3>{ -256, 64, 128 },
            "Camera must reconstruct eye Y from the object center and read view words, not Euler angles.");
        host.SetPaused(true);
        const auto next = host.StartLevel(11);
        Check(reader.GetPlayerCamera().position == camera.position &&
            reader.GetPlayerCamera().direction == camera.direction,
            "Loading and pause must preserve access to the displayed world's camera.");
        host.FailLevelLoad(next.id, "Test failure");
        host.worldObjects[1].health = 0;
        host.worldObjects[1].posY = -32768;
        host.worldObjects[1].velocityY = 32767;
        const auto updated = reader.GetPlayerCamera();
        Check(updated.position[1] == -32756 && updated.direction[1] == 32767 &&
            camera.position[1] == -30 && camera.direction[1] == 64,
            "Camera snapshots must reflect current signed words independently of health and preserve earlier snapshots.");
    }

    void TestBasicPlayerMovement()
    {
        const auto load = [](Host& host, int heading)
        {
            const auto request = host.StartLevel(10);
            auto data = MakeLevel();
            auto& player = data.objects[1];
            player.posX = 1500;
            player.posY = 73;
            player.posZ = 2500;
            player.velocityX = static_cast<std::int16_t>(SineTable[static_cast<std::size_t>(heading)] / 655);
            player.velocityZ = static_cast<std::int16_t>(SineTable[static_cast<std::size_t>((heading + 128) % 512)] / 655);
            player.velocityY = 17;
            host.CompleteLevelLoad(request.id, std::move(data));
        };
        const auto fixed100 = [](std::int32_t sine)
        {
            const auto product = sine * 100;
            return product < 0 ? -((-product + 65535) / 65536) : product / 65536;
        };
        for (const int initialHeading : { 0, 128, 256, 384 })
        {
            Host host;
            for (unsigned int keys = 0; keys < 64; ++keys)
            {
                load(host, initialHeading);
                Input input;
                for (std::size_t key = 0; key < input.movement.size(); ++key)
                {
                    input.movement[key] = (keys & (1u << key)) != 0;
                }
                int forward = 0;
                int side = 0;
                int turn = 0;
                int heading = initialHeading;
                int x = 1500;
                int eyeY = 85;
                int verticalSpeed = 0;
                int z = 2500;
                // Exhaust all key combinations across held input, braking and normal/diagonal limits.
                for (int frame = 0; frame < 8; ++frame)
                {
                    if (frame == 4) { input.movement.fill(false); }
                    const auto axis = [&](InputKey positive, InputKey negative, int amount)
                    {
                        return (input.movement[static_cast<std::size_t>(positive)] ? amount : 0) -
                            (input.movement[static_cast<std::size_t>(negative)] ? amount : 0);
                    };
                    forward += axis(InputKey::ForwardKey, InputKey::BackwardKey, 4);
                    side += axis(InputKey::StrafeRightKey, InputKey::StrafeLeftKey, 4);
                    turn += axis(InputKey::TurnRightKey, InputKey::TurnLeftKey, 7);
                    const auto sign = [](int value) { return (value > 0) - (value < 0); };
                    forward -= sign(forward);
                    side -= sign(side);
                    turn -= sign(turn);
                    const int ticks = frame % 3 + 1;
                    for (int tick = 0; tick < ticks; ++tick)
                    {
                        const int limit = forward != 0 && side != 0 ? 3 : 5;
                        forward = std::clamp(forward, -limit, limit);
                        side = std::clamp(side, -limit, limit);
                        turn = std::clamp(turn, -6, 6);
                        heading = (heading + turn + 512) % 512;
                        const int dx = fixed100(SineTable[static_cast<std::size_t>(heading)]);
                        const int dz = fixed100(SineTable[static_cast<std::size_t>((heading + 128) % 512)]);
                        x += (dx * forward + dz * side) / 100;
                        z += (dz * forward - dx * side) / 100;
                        eyeY += --verticalSpeed;
                        if (eyeY < 45) { eyeY = 45; verticalSpeed = 0; }
                    }
                    const auto before = host.worldObjects;
                    const auto seed = host.randomSeed;
                    const auto result = host.Tick({ 0, static_cast<std::int16_t>(ticks) }, input);
                    const auto camera = host.GetPlayerCamera();
                    Check(result.playerMovement && result.playerMovement->simulatedTicks == ticks &&
                        result.playerMovement->blockedTicks == 0 && camera.position[0] == x &&
                        camera.position[2] == z && camera.position[1] == eyeY && camera.direction[1] == 17 &&
                        camera.direction[0] == fixed100(SineTable[static_cast<std::size_t>(heading)]) &&
                        camera.direction[2] == fixed100(SineTable[static_cast<std::size_t>((heading + 128) % 512)]) &&
                        host.soundListenerX == x && host.soundListenerZ == z && host.randomSeed == seed,
                        "Basic movement differs from normal keyboard/local-axis arithmetic, view direction, or listener order.");
                    auto expected = before;
                    expected[1].posX = static_cast<std::int16_t>(x);
                    expected[1].posY = static_cast<std::int16_t>(eyeY - 25);
                    expected[1].collisionRadiusY = 23;
                    expected[1].posZ = static_cast<std::int16_t>(z);
                    expected[1].velocityX = camera.direction[0];
                    expected[1].velocityZ = camera.direction[2];
                    Check(std::memcmp(expected.data(), host.worldObjects.data(), sizeof(expected)) == 0,
                        "Movement must change only position and horizontal view words, preserving unrelated record bytes.");
                }
                Check(input.movementCalls == 8 * 6, "Movement input must be sampled once per host frame, not per substep.");
            }
        }

        Host host;
        Input input;
        input.movement[static_cast<std::size_t>(InputKey::ForwardKey)] = true;
        load(host, 0);
        const auto before = host.worldObjects;
        Check(host.Tick({ 10, 0 }, input).playerMovement->simulatedTicks == 0 &&
            input.movementCalls == 0 && std::memcmp(before.data(), host.worldObjects.data(), sizeof(before)) == 0,
            "Player simulation must use frame ticks, not behavior ticks, and zero frames must not poll movement.");
        host.basicPlayerMovementEnabled = false;
        Check(!host.Tick({ 0, 3 }, input).playerMovement &&
            input.movementCalls == 0 && host.worldObjects[1].posZ == 2500,
            "Hosts with an external controller must be able to disable the basic controller entirely.");
        host.basicPlayerMovementEnabled = true;
        host.Tick({}, input);
        Check(host.worldObjects[1].posZ == 2503, "First normal forward step must move three DOS units.");
        load(host, 0);
        host.Tick({}, input);
        Check(host.worldObjects[1].posZ == 2503, "Installing a level must reset movement momentum.");

        load(host, 0);
        host.worldObjects[1].health = 0;
        const auto calls = input.movementCalls;
        Check(host.Tick({}, input).playerMovement->simulatedTicks == 0 && input.movementCalls == calls,
            "Inactive players must not move or consume movement input.");
        load(host, 0);
        host.SetPaused(true);
        Check(host.Tick({}, input).status == GameTickStatus::Paused && input.movementCalls == calls,
            "Paused simulation must not sample movement.");
        host.SetPaused(false);

        // Symmetric obstacles produce zero correction; a blocked center must still reject movement.
        std::fill(std::begin(host.collisionMap->data), std::end(host.collisionMap->data), std::uint8_t{ 0x18 });
        const auto blocked = host.Tick({}, input);
        Check(blocked.playerMovement->blockedTicks == 1 &&
            host.worldObjects[1].posX == 1500 && host.worldObjects[1].posZ == 2500,
            "Symmetric blocked centers must report rollback instead of allowing movement through a wall.");
        host.objectCollisionsEnabled = false;
        Check(host.Tick({}, input).playerMovement->blockedTicks == 0 && host.worldObjects[1].posZ == 2505,
            "Disabling object collisions must bypass map correction explicitly.");
        host.objectCollisionsEnabled = true;
        load(host, 0);
        std::fill(std::begin(host.collisionMap->data), std::end(host.collisionMap->data), std::uint8_t{ 0x20 });
        Check(host.Tick({}, input).playerMovement->blockedTicks == 0 && host.worldObjects[1].posZ == 2503,
            "Normal player collision correction must use native mask 18h, not generic object mask 38h.");
        load(host, 0);
        // A complete wall row immediately ahead should push away and terminate without crossing.
        const auto column = (1500 - 112) / 25;
        const auto row = 162 - (2500 - 38) / 25 - 1;
        for (int offset = -3; offset <= 3; ++offset)
        {
            host.collisionMap->data[row * 200 + column + offset] = 0x18;
        }
        host.Tick({ 0, 10 }, input);
        Check(host.worldObjects[1].posZ < 2525,
            "Bounded neighborhood corrections must stop forward motion at a map wall.");
        load(host, 0);
        host.worldObjects[1].collisionRadiusXZ = 1;
        host.worldObjects[1].collisionRadiusY = 1;
        auto& pickup = host.worldObjects[2];
        pickup = host.worldObjects[1];
        --pickup.posY;
        pickup.posZ = 2503;
        pickup.collisionType = CollisionType::Gold;
        pickup.itemValue = 9;
        input.pressed = true;
        const auto gold = host.gold;
        const auto contact = host.Tick({}, input);
        Check(host.gold == gold + 9 && contact.interaction->status == InteractionStatus::Applied &&
            host.worldObjects[2].health == 0,
            "Contact and interaction must use the player position after basic movement.");
        load(host, 128);
        host.worldObjects[1].posX = 1599;
        host.Tick({}, input);
        Check(host.worldObjects[1].posX == 1602 && host.lightingCache.centerX == 1602 &&
            host.soundListenerX == 1602,
            "Listener and light-grid centers must follow the newly moved player in the same tick.");
        host.collisionMap = nullptr;
        ExpectError<std::logic_error>([&] { host.Tick({}, input); });
    }

    void TestPlayerGrounding()
    {
        const auto load = [](Host& host, std::int16_t y)
        {
            auto data = MakeLevel();
            data.objects[1].posX = 1500;
            data.objects[1].posY = y;
            data.objects[1].posZ = 2500;
            data.objects[1].velocityZ = 100;
            data.objects[1].collisionRadiusY = 23;
            const auto request = host.StartLevel(10);
            auto* geometry = data.level.get();
            host.CompleteLevelLoad(request.id, std::move(data));
            return geometry;
        };
        Host host;
        Input input;
        load(host, 73);
        for (const int centerY : { 72, 70, 67, 63, 58, 52, 45, 37, 28, 20 })
        {
            host.Tick({ 0, 1 }, input);
            Check(host.worldObjects[1].posY == centerY &&
                host.GetPlayerCamera().position[1] == centerY + 25 &&
                host.GetPlayerCamera().direction[1] == 0,
                "Gravity must accelerate by one per frame, land at eye height 45, and not pitch the view.");
        }
        load(host, 73);
        host.Tick({ 0, 10 }, input);
        Check(host.worldObjects[1].posY == 20 && host.GetPlayerCamera().position[1] == 45,
            "Batched vertical substeps must land identically to single-frame ticks.");
        load(host, -90);
        host.Tick({}, input);
        Check(host.worldObjects[1].posY == 20,
            "A player below terrain must be lifted to support rather than fall through it.");

        load(host, 73);
        host.Tick({ 0, 4 }, input);
        host.SetPaused(true);
        host.Tick({ 0, 20 }, input);
        Check(host.worldObjects[1].posY == 63, "Pause must freeze a fall.");
        host.SetPaused(false);
        host.basicPlayerMovementEnabled = false;
        host.Tick({ 0, 20 }, input);
        Check(host.worldObjects[1].posY == 63, "Disabling the controller must also disable gravity.");
        host.basicPlayerMovementEnabled = true;
        host.Tick({ 0, 0 }, input);
        Check(host.worldObjects[1].posY == 63, "Zero frame ticks must not integrate gravity.");
        load(host, 73);
        host.Tick({}, input);
        Check(host.worldObjects[1].posY == 72, "Level installation must reset falling momentum.");

        auto* geometry = load(host, 33);
        // A shallow ramp: increasing raw heights lower the native floor Y.
        for (auto& row : geometry->floorHeights)
        {
            for (std::size_t column = 0; column < Level::Width; ++column)
            {
                row[column] = static_cast<std::uint8_t>(column);
            }
        }
        host.worldObjects[1].posY = static_cast<std::int16_t>(
            host.GetTerrainFloorHeight(1500, 45, 2500) + 20);
        const auto initialY = host.GetPlayerCamera().position[1];
        input.movement[static_cast<std::size_t>(InputKey::StrafeLeftKey)] = true;
        for (int tick = 0; tick < 80; ++tick)
        {
            host.Tick({}, input);
            const auto camera = host.GetPlayerCamera();
            const auto floor = host.GetTerrainFloorHeight(camera.position[0], camera.position[1], camera.position[2]);
            Check(camera.position[1] == floor + 45 && host.worldObjects[1].posY == floor + 20,
                "Walking uphill must follow interpolated terrain at the final horizontal position.");
        }
        Check(host.GetPlayerCamera().position[1] > initialY, "Uphill fixture must actually change elevation.");
        input.movement.fill(false);
        input.movement[static_cast<std::size_t>(InputKey::StrafeRightKey)] = true;
        for (int tick = 0; tick < 100; ++tick) { host.Tick({}, input); }
        input.movement.fill(false);
        host.Tick({ 0, 30 }, input);
        const auto camera = host.GetPlayerCamera();
        Check(camera.position[1] == host.GetTerrainFloorHeight(
            camera.position[0], camera.position[1], camera.position[2]) + 45 &&
            camera.position[1] < initialY,
            "Walking downhill must fall and settle onto the lower terrain, not retain the old Y.");
        // Native player support uses terrain interpolation, not the special flat-depth floor shortcut.
        for (auto& row : geometry->surfaceFlags) { std::fill(std::begin(row), std::end(row), std::uint8_t{ 1 }); }
        host.Tick({}, input);
        host.worldObjects[1].posX = 0;
        ExpectError<std::out_of_range>([&] { host.Tick({}, input); });
    }

    void TestLoadedLevelInteraction(const std::filesystem::path& levelDirectory,
        const std::filesystem::path& savePath)
    {
        const auto readBytes = [](const std::filesystem::path& path)
        {
            std::ifstream stream(path, std::ios::binary);
            if (!stream) { throw std::runtime_error("Cannot open renderer-host interaction fixture."); }
            return std::vector<std::uint8_t>(
                std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
        };
        const auto objectRecords = readBytes(levelDirectory / "OBJPOS11.DAT");
        const auto saved = readBytes(savePath);
        Check(saved.size() >= 299, "Original save fixture must contain the player record at byte 245.");
        const auto nativePlayer = DecodeGameObject(saved.data() + 245, 54);
        Check(nativePlayer.field_07 == 3 && nativePlayer.collisionRadiusXZ == 15 &&
            nativePlayer.collisionRadiusY == 23, "Original standing player collision shape changed.");
        const auto object = [&](std::size_t record)
        {
            Check(objectRecords.size() >= (record + 1) * 54, "Missing original interaction object.");
            return DecodeGameObject(objectRecords.data() + record * 54, 54);
        };
        int fileRequests = 0;
        Host host;
        host.files = [&](std::string_view filename)
        {
            ++fileRequests;
            if (filename == "LEVEL\\FELD11.256") { return readBytes(levelDirectory / "FELD11.256"); }
            Check(filename == "LEVEL\\OBJPOS11.DAT", "GameCore must choose the requested level's assets.");
            return objectRecords;
        };
        const auto worldObject = [&](int slot)
        {
            return host.GetWorldObjects().at(slot);
        };
        const auto loadAt = [&](const RawGameObject& target)
        {
            host.playerSpawn = PlayerCamera{
                { target.posX, static_cast<std::int16_t>(target.posY + 45), target.posZ }, { 0,0,100 } };
            host.StartLevel(11);
            Check(host.GetRuntimeState() == GameRuntimeState::Ready, "Real fixture level loading failed.");
            Check(fileRequests > 0, "Level loading must obtain objects through the file service.");
            for (std::size_t record = 0; record < objectRecords.size() / 54; ++record)
            {
                const auto loaded = worldObject(static_cast<int>(record + 2));
                Check(std::memcmp(&loaded, objectRecords.data() + record * 54, 54) == 0,
                    "GameCore must load every original OBJPOS record byte-for-byte before simulation.");
            }
            const auto player = worldObject(1);
            Check(player.field_07 == nativePlayer.field_07 &&
                player.collisionRadiusXZ == nativePlayer.collisionRadiusXZ &&
                player.collisionRadiusY == nativePlayer.collisionRadiusY &&
                host.GetPlayerCamera().position == host.playerSpawn->position,
                "Host must use the original player collision shape and convert eye/center position exactly once.");
        };
        const auto medicine = object(32);
        Check(medicine.collisionType == CollisionType::Medicine && medicine.posY == 5,
            "Expected original level-11 floor medicine fixture.");
        loadAt(medicine);
        Input input;
        Check(host.Tick({ 0,1 }, input).status == GameTickStatus::Advanced && worldObject(34).health == 1000,
            "Contact without Enter must not consume medicine.");
        Input enter;
        enter.pressed = true;
        const auto medicineTick = host.Tick({ 0,1 }, enter);
        Check(medicineTick.status == GameTickStatus::Advanced && worldObject(34).health == 0 &&
            medicineTick.interaction && medicineTick.interaction->status == InteractionStatus::Applied,
            "Enter through the context must pick up real floor medicine after grounding.");
        const auto door = object(0);
        loadAt(door);
        const auto doorTick = host.Tick({ 1,1 }, enter);
        Check(doorTick.status == GameTickStatus::Advanced && worldObject(2).field_26 == 3 &&
            doorTick.interaction && doorTick.interaction->status == InteractionStatus::Applied,
            "Real unlocked-door interaction must activate the native door.");
        const auto trigger = object(9);
        Check(trigger.collisionType == CollisionType::Switch, "Expected ordinary door-trigger fixture.");
        loadAt(trigger);
        Check(host.Tick({ 1,1 }, input).status == GameTickStatus::Advanced,
            "Initial ordinary-switch behavior tick failed.");
        const auto linkedDoor = worldObject(6);
        Check(linkedDoor.health == 1001 && linkedDoor.field_26 == 0,
            "Expected level-11 door encoded with switch index 1.");
        const auto switchTick = host.Tick({ 1,1 }, enter);
        Check(switchTick.status == GameTickStatus::Advanced && worldObject(11).blendWeightOrSpriteOpacity == 10 &&
            worldObject(6).field_26 == 3 && worldObject(6).blendWeightOrSpriteOpacity == 3 &&
            switchTick.interaction && switchTick.interaction->status == InteractionStatus::Applied,
            "Enter must pull the original level-11 switch and animate its linked door in the same tick.");
        for (int tick = 0; tick < 40; ++tick)
        {
            Check(host.Tick({ 1,1 }, input).status == GameTickStatus::Advanced, "Switch/door animation tick failed.");
        }
        Check(worldObject(6).blendWeightOrSpriteOpacity == 100 &&
            worldObject(11).blendWeightOrSpriteOpacity == 0,
            "Linked door must finish opening while the ordinary switch returns to rest.");
    }

    void TestPromptHooks()
    {
        std::vector<std::uint8_t> geometry(Level::GeometryStreamSize);
        geometry[0] = 49;
        geometry[2] = 39;
        RawGameObject door;
        door.health = 1000;
        door.field_07 = 3;
        door.collisionType = CollisionType::Door;
        door.doorLockOrGroupBitmask = -1;
        door.posX = 1500;
        door.posZ = 2500;
        door.collisionRadiusXZ = 100;
        door.collisionRadiusY = 100;
        std::vector<std::uint8_t> records(54);
        EncodeGameObject(door, records.data(), records.size());
        Host host;
        host.files = [&](std::string_view filename)
        {
            if (filename == "LEVEL\\FELD11.256") { return geometry; }
            Check(filename == "LEVEL\\OBJPOS11.DAT", "Unexpected prompt-test file request.");
            return records;
        };
        Check(host.promptCalls == 0 && !host.IsUserMessageActive(),
            "A new context must not emit a prompt or require polling.");
        host.playerSpawn = PlayerCamera{ { 1500,45,2500 }, { 0,0,100 } };
        host.StartLevel(11);
        Check(host.GetRuntimeState() == GameRuntimeState::Ready, "Prompt fixture level loading failed.");
        Input input;
        input.pressed = true;
        Check(host.Tick({ 0,1 }, input).status == GameTickStatus::Advanced, "Locked-door prompt tick failed.");
        const auto firstText = host.userMessage;
        const auto firstNotification = host.promptCalls;
        Check(firstText == "abgeschlossen" && host.userMessageTicks == 60 &&
            host.IsUserMessageActive() && firstNotification == 2,
            "The core must call the app's virtual hook directly for level reset and door prompts.");
        input.pressed = false;
        Check(host.Tick({ 0,59 }, input).status == GameTickStatus::Advanced &&
            host.promptCalls == firstNotification && host.userMessageTicks == 60,
            "Core updates must not own or decrement the app's prompt timer.");
        host.userMessageTicks = -1;
        input.pressed = true;
        Check(host.Tick({ 0,1 }, input).status == GameTickStatus::Advanced &&
            host.userMessage == firstText && host.userMessageTicks == 60 &&
            host.promptCalls == firstNotification + 1 && host.IsUserMessageActive(),
            "An identical message must reach the hook again after the app marks it expired.");
        Check(host.Tick({ 0,1 }, input).status == GameTickStatus::Advanced &&
            host.promptCalls == firstNotification + 1,
            "The core must query app-owned activity before requesting another locked-door prompt.");
        host.StartLevel(11);
        Check(host.userMessage.empty() && host.userMessageTicks == 0 &&
            host.promptCalls == firstNotification + 2 && !host.IsUserMessageActive(),
            "Level changes must call the same virtual hook with an empty reset request.");
    }

    void TestSimpleSwitchInteraction()
    {
        GameContext context;
        auto& lever = context.worldObjects[150];
        lever.health = 1000;
        lever.collisionType = CollisionType::Switch;
        lever.behavior = BehaviorType::Switch1;
        lever.itemValue = 1;
        lever.field_24 = 10;
        lever.field_32 = 1;
        const auto resting = lever;
        for (const int slot : { 2, 140, 141, 164 })
        {
            auto& target = context.worldObjects[static_cast<std::size_t>(slot)];
            target.health = 1001;
            target.collisionType = CollisionType::Door;
            target.modelFrame2 = 1;
            target.field_24 = 30;
            target.doorLockOrGroupBitmask = -1;
        }
        const auto before = context.worldObjects;
        Check(HandlePlayerObjectInteraction(lever, context).status == InteractionStatus::Applied &&
            lever.field_26 == 10 && lever.health == 1000 && context.inventory[0].health == 0,
            "Ordinary switch interaction must start its rate, not pick up a flagged lever.");
        for (const int slot : { 2, 140 })
        {
            const auto& target = context.worldObjects[static_cast<std::size_t>(slot)];
            Check(target.field_26 == 3 && target.itemValue == 30 && target.health == 1001 &&
                target.doorLockOrGroupBitmask == -1,
                "Switch-index matching must open all matching doors without consuming their health link or lock.");
        }
        Check(std::memcmp(&before[141], &context.worldObjects[141], sizeof(RawGameObject)) == 0 &&
            std::memcmp(&before[164], &context.worldObjects[164], sizeof(RawGameObject)) == 0,
            "Switch target dispatch must stop at native slot 140.");

        for (const auto phase : { -1, 1, 50, 100, 101 })
        {
            lever = resting;
            lever.blendWeightOrSpriteOpacity = static_cast<std::int16_t>(phase);
            const auto snapshot = context.worldObjects;
            Check(HandlePlayerObjectInteraction(lever, context).status == InteractionStatus::NoEffect &&
                std::memcmp(snapshot.data(), context.worldObjects.data(), sizeof(snapshot)) == 0,
                "Nonzero switch phase must reject reactivation without side effects.");
        }
        for (const auto health : { 999, 1001 })
        {
            lever = resting;
            lever.health = static_cast<std::int16_t>(health);
            context.worldObjects[2].field_26 = 0;
            Check(HandlePlayerObjectInteraction(lever, context).status == InteractionStatus::Applied &&
                lever.field_26 == 10 && context.worldObjects[2].field_26 == 0,
                "A non-1000 live switch must animate but not activate indexed targets.");
        }
        for (const auto index : { -1, 0 })
        {
            lever = resting;
            lever.itemValue = static_cast<std::int16_t>(index);
            Check(HandlePlayerObjectInteraction(lever, context).status == InteractionStatus::Applied &&
                lever.field_26 == 10, "A nonpositive switch index must still start the local animation.");
        }
        lever = resting;
        context.worldObjects[2].collisionType = CollisionType::Ghost;
        Check(HandlePlayerObjectInteraction(lever, context).status == InteractionStatus::Applied &&
            lever.field_26 == 10 && context.worldObjects[2].health == 1000 &&
            context.worldObjects[140].field_26 == 3,
            "Indexed activation must dispatch mixed non-door/door targets through the shared native helper.");
        context.worldObjects[2].collisionType = CollisionType::Door;
        context.worldObjects[2].health = 1001;
        context.worldObjects[2].doorLockOrGroupBitmask = 1;
        context.worldObjects[2].field_26 = 0;
        lever = resting;
        Check(HandlePlayerObjectInteraction(lever, context).status == InteractionStatus::Applied &&
            context.worldObjects[2].field_26 == 0,
            "Switch activation must preserve positive key locks.");

        lever = resting;
        lever.itemValue = 32767;
        context.worldObjects[2].health = -31769;
        context.worldObjects[2].doorLockOrGroupBitmask = 0;
        Check(HandlePlayerObjectInteraction(lever, context).status == InteractionStatus::Applied &&
            context.worldObjects[2].field_26 == 3,
            "Switch health links must use wrapped word addition, without an extra target-health gate.");
        lever = resting;
        lever.behaviorParameter = 1;
        context.tickCount = 1;
        HandlePlayerObjectInteraction(lever, context);
        for (int tick = 0; tick < 20; ++tick) { Function_Switch1(lever, context); }
        Check(lever.blendWeightOrSpriteOpacity == 100 && lever.field_26 == 0,
            "A latched ordinary switch must remain pulled and stop animating.");
        Check(HandlePlayerObjectInteraction(lever, context).status == InteractionStatus::NoEffect,
            "A fully pulled latched switch must ignore further interaction.");
        lever = resting;
        lever.itemValue = 3;
        lever.field_26 = 123;
        lever.soundEffectId = 2;
        context.soundEffectsLoaded = true;
        context.soundEffects[1].sampleCount = 1;
        context.soundEffects[1].samples[0] = 1;
        const auto seed = context.randomSeed;
        Check(HandlePlayerObjectInteraction(lever, context).status == InteractionStatus::Applied &&
            lever.field_26 == 10 && context.randomSeed == seed * 134775813u + 1u,
            "At phase zero, native activation overwrites the rate and requests sound even when no target matches.");
        lever.health = 0;
        Check(HandlePlayerObjectInteraction(lever, context).status == InteractionStatus::InactiveObject,
            "Inactive ordinary switches must reject interaction.");
    }

    void TestLevelObjectLoading()
    {
        class FileHost final : public GameContext
        {
        public:
            using GameContext::LoadLevelObjects;
            std::vector<std::uint8_t> payload;
            std::string requested;
            bool missing = false;
        protected:
            std::vector<std::uint8_t> LoadFile(std::string_view filename) override
            {
                requested = filename;
                if (missing) { throw std::runtime_error(std::string(filename) + ": file not found"); }
                return payload;
            }
        };
        FileHost host;
        auto data = MakeLevel();
        data.objects[0].itemValue = 7;
        data.objects[165].itemValue = 8;
        const auto before = data.objects;
        host.payload.resize(163 * 54);
        for (std::size_t i = 0; i < host.payload.size(); ++i)
        {
            host.payload[i] = static_cast<std::uint8_t>(i * 17 + 3);
        }
        host.LoadLevelObjects(12, data);
        Check(host.requested == "LEVEL\\OBJPOS12.DAT" &&
            std::memcmp(&data.objects[2], host.payload.data(), host.payload.size()) == 0 &&
            std::memcmp(&data.objects[0], &before[0], 2 * sizeof(RawGameObject)) == 0 &&
            std::memcmp(&data.objects[165], &before[165], sizeof(RawGameObject)) == 0,
            "OBJPOS loading must fill all 163 ordinary slots without touching reserved, player or fallback slots.");
        host.payload.resize(54);
        host.LoadLevelObjects(99, data);
        Check(data.objects[3].health == 0 && data.objects[164].health == 0,
            "Shorter object files must clear unused slots instead of retaining previous objects.");
        const auto valid = data.objects;
        for (const std::size_t badSize : { std::size_t{ 53 }, std::size_t{ 55 }, std::size_t{ 164 * 54 } })
        {
            host.payload.resize(badSize);
            ExpectError<std::invalid_argument>([&] { host.LoadLevelObjects(12, data); });
            Check(std::memcmp(valid.data(), data.objects.data(), sizeof(valid)) == 0,
                "Invalid OBJPOS data must not partially replace records.");
        }
        host.missing = true;
        ExpectError<std::runtime_error>([&] { host.LoadLevelObjects(12, data); });
        ExpectError<std::invalid_argument>([&] { host.LoadLevelObjects(0, data); });
        std::vector<std::uint8_t> geometry(Level::GeometryStreamSize);
        geometry[0] = 49;
        geometry[2] = 39;
        bool invalid = false;
        Host adapter;
        adapter.playerSpawn = PlayerCamera{ { 1500,45,2500 }, { 0,0,100 } };
        adapter.files = [&](std::string_view filename) -> std::vector<std::uint8_t>
        {
            if (filename == "LEVEL\\FELD12.256") { return geometry; }
            if (invalid) { return std::vector<std::uint8_t>(55); }
            throw std::runtime_error(std::string(filename) + ": missing");
        };
        ExpectError<std::runtime_error>([&] { adapter.StartLevel(12); });
        Check(adapter.GetRuntimeState() == GameRuntimeState::LoadFailed &&
            adapter.GetLevelLoadError().find("OBJPOS12.DAT") != std::string::npos,
            "Missing object-file errors must reach the host without installing a partial world.");
        invalid = true;
        ExpectError<std::invalid_argument>([&] { adapter.StartLevel(12); });
        Check(adapter.GetRuntimeState() == GameRuntimeState::LoadFailed &&
            adapter.GetLevelLoadError().find("54-byte") != std::string::npos,
            "Malformed object-file errors must propagate through the level lifecycle.");
    }

    void TestWallCollisionLoading(const std::filesystem::path& levelDirectory)
    {
        const auto read = [](const std::filesystem::path& path)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file) { throw std::runtime_error("Cannot read wall collision fixture."); }
            return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        };
        std::vector<std::uint8_t> icons;
        for (const auto& entry : std::filesystem::directory_iterator(levelDirectory))
        {
            const auto name = entry.path().filename().u8string();
            if (name.size() >= 8 && name.front() == 'W' && name.substr(name.size() - 7) == "NDE.AN2")
            {
                icons = read(entry.path());
            }
        }
        Check(icons.size() == 662 && icons[0] == 5 && icons[1] == 4, "Expected original twenty-frame wall AN2.");
        for (std::size_t frame = 0; frame < 20; ++frame)
        {
            const auto offset = 2 + frame * 33;
            Check(icons[offset + 4] == 4 && icons[offset + 5] == 0 &&
                icons[offset + 6] == 4 && icons[offset + 7] == 0, "Wall icons must be raw 5x5 images.");
        }
        const auto verify = [&](const std::vector<std::uint8_t>& feld)
        {
            Level level;
            level.LoadGeometry(feld.data(), feld.size());
            level.BuildCollisionMap();
            const auto width = static_cast<std::size_t>(feld[0] | (feld[1] << 8)) + 1;
            const auto height = static_cast<std::size_t>(feld[2] | (feld[3] << 8)) + 1;
            const auto planeSize = width * height;
            // Independent native oracle: draw full screen, then GetImage the native crop.
            std::array<std::uint8_t, 320 * 240> screen{};
            const std::array<int, 20> passes{
                0,16,17,18,19,27,9,10,11,12,13,14,15,20,21,22,23,24,25,26 };
            for (std::size_t pass = 0; pass < passes.size(); ++pass)
            {
                for (std::size_t row = 0; row < height; ++row)
                {
                    for (std::size_t column = 0; column < width; ++column)
                    {
                        const auto tile = row * width + column;
                        const auto selector = feld[4 + 5 * planeSize + tile] & 31;
                        for (std::size_t y = 0; y < 5; ++y)
                        {
                            for (std::size_t x = 0; x < 5; ++x)
                            {
                                const auto destination = (4 * (row + 1) - 1 + y) * 320 + 4 * (column + 1) - 1 + x;
                                if (pass == 0) { screen[destination] = feld[4 + 6 * planeSize + tile]; }
                                if (selector == passes[pass] && selector >= 9 && selector <= 27)
                                {
                                    const auto pixel = icons[2 + static_cast<std::size_t>(selector - 8) * 33 + 8 + y * 5 + x];
                                    if (pixel != 0) { screen[destination] = pixel; }
                                }
                            }
                        }
                    }
                }
            }
            const auto mapWidth = 4 * width - 2;
            const auto mapHeight = 4 * height - 1;
            Check(level.collisionMap->widthMinusOne + 1u == mapWidth &&
                level.collisionMap->heightMinusOne + 1u == mapHeight, "Native map capture dimensions differ.");
            for (std::size_t row = 0; row < mapHeight; ++row)
            {
                for (std::size_t column = 0; column < mapWidth; ++column)
                {
                    Check(level.collisionMap->data[row * mapWidth + column] == screen[(row + 3) * 320 + column + 4],
                        "Static map differs from native priority/transparency/capture oracle.");
                }
            }
            Check(std::all_of(std::begin(level.collisionMap->data) + mapWidth * mapHeight,
                std::end(level.collisionMap->data), [](std::uint8_t value) { return value == 0; }),
                "Unused collision allocation must remain deterministic.");
            if (width < Level::Width && height < Level::Height)
            {
                Check(level.floorHeights[height][width] == 255 && level.roomHeights[height][width] == 50 &&
                    level.ceilingObjects[height][width] == 0 && level.surfaceFlags[height][width] == 0 &&
                    level.collisionMaterials[height][width] == 0, "Native out-of-file plane defaults differ.");
            }
        };
        // Isolated icons test every embedded pixel, including frames with ignored -2,-2 offsets.
        std::vector<std::uint8_t> synthetic(4 + 7 * 4 * 4, 0);
        synthetic[0] = 3;
        synthetic[2] = 3;
        for (unsigned int selector = 0; selector < 32; ++selector)
        {
            std::fill(synthetic.begin() + 4 + 5 * 16, synthetic.end(), std::uint8_t{ 0 });
            synthetic[4 + 5 * 16 + 5] = static_cast<std::uint8_t>(selector | 0xe0);
            std::fill(synthetic.begin() + 4 + 6 * 16, synthetic.end(), std::uint8_t{ 7 });
            verify(synthetic);
        }
        std::vector<std::uint8_t> maximum(Level::GeometryStreamSize);
        maximum[0] = 49;
        maximum[2] = 39;
        for (std::size_t i = 4; i < maximum.size(); ++i) { maximum[i] = static_cast<std::uint8_t>(i * 13 + i / 7); }
        verify(maximum);
        verify({ 0,0,0,0, 0,0,0,0,0,15,7 });
        std::size_t originalLevels = 0;
        for (const auto& entry : std::filesystem::directory_iterator(levelDirectory))
        {
            const auto name = entry.path().filename().string();
            if (name.rfind("FELD", 0) == 0 && entry.path().extension() == ".256")
            {
                const auto feld = read(entry.path());
                if (name == "FELD9.256" || name == "FELD29.256")
                {
                    Check(feld.size() == 703, "Known truncated original FELD fixture changed.");
                    Level level;
                    ExpectError<std::invalid_argument>([&] { level.LoadGeometry(feld.data(), feld.size()); });
                }
                else { verify(feld); }
                ++originalLevels;
            }
        }
        Check(originalLevels > 0, "No original FELD files checked.");

        class FileHost final : public GameContext
        {
        public:
            using GameContext::LoadLevelGeometry;
            std::vector<std::uint8_t> payload;
            std::string requested;
            bool missing = false;
        protected:
            std::vector<std::uint8_t> LoadFile(std::string_view filename) override
            {
                requested = filename;
                if (missing) { throw std::runtime_error("Missing FELD asset."); }
                return payload;
            }
        } host;
        host.payload = read(levelDirectory / "FELD11.256");
        auto data = MakeLevel();
        host.LoadLevelGeometry(11, data);
        Check(host.requested == "LEVEL\\FELD11.256" && data.level->collisionMap->widthMinusOne == 97 &&
            data.level->collisionMap->heightMinusOne == 110, "GameCore must load and build the real variable-size map.");
        {
            const auto& map = *data.level->collisionMap;
            const auto pixel = [&](int x, int y) { return map.data[y * 98 + x] & 0x18; };
            int wallX = 0;
            int wallY = 0;
            for (int y = 6; y < 104 && wallX == 0; ++y)
            {
                for (int x = 6; x < 92 && wallX == 0; ++x)
                {
                    bool suitable = true;
                    for (int dy = -3; dy <= 3; ++dy) { suitable = suitable && pixel(x, y + dy) != 0; }
                    for (int dx = -4; dx < 0; ++dx)
                    {
                        for (int dy = -1; dy <= 1; ++dy) { suitable = suitable && pixel(x + dx, y + dy) == 0; }
                    }
                    if (suitable) { wallX = x; wallY = y; }
                }
            }
            Check(wallX != 0, "Level 11 must contain a straight wall with a clear approach.");
            auto movementData = MakeLevel();
            host.LoadLevelGeometry(11, movementData);
            movementData.playerSpawn = PlayerCamera{
                { static_cast<std::int16_t>(112 + 25 * (wallX - 3) + 12), 45,
                  static_cast<std::int16_t>(38 + 25 * (162 - wallY) + 12) }, { 100,0,0 } };
            Host movement;
            const auto request = movement.StartLevel(11);
            movement.CompleteLevelLoad(request.id, std::move(movementData));
            const auto startX = movement.worldObjects[1].posX;
            Input input;
            input.movement[0] = true;
            for (int tick = 0; tick < 120; ++tick) { movement.Tick({ 0,1 }, input); }
            Check(movement.worldObjects[1].posX > startX &&
                movement.worldObjects[1].posX < 112 + 25 * wallX,
                "The controller must approach but not cross a real FELD11 wall.");
            const auto altered = static_cast<std::size_t>(wallY * 98 + wallX);
            movement.collisionMap->data[altered] = 7;
            movement.Tick({ 0,0 }, input);
            Check(movement.collisionMap->data[altered] == 7,
                "Ticks must preserve dynamic map edits instead of rebuilding static walls.");
        }
        auto* installed = data.level.get();
        for (const auto& bad : { std::vector<std::uint8_t>{}, std::vector<std::uint8_t>{ 50,0,39,0 },
            std::vector<std::uint8_t>{ 0,0,0,0,0 } })
        {
            host.payload = bad;
            ExpectError<std::invalid_argument>([&] { host.LoadLevelGeometry(11, data); });
            Check(data.level.get() == installed, "Malformed geometry must not partially replace a level.");
        }
        ExpectError<std::invalid_argument>([&] { host.LoadLevelGeometry(0, data); });
        host.missing = true;
        ExpectError<std::runtime_error>([&] { host.LoadLevelGeometry(11, data); });
        Host adapter;
        adapter.files = [](std::string_view name) -> std::vector<std::uint8_t>
        {
            throw std::runtime_error(std::string(name) + ": missing");
        };
        ExpectError<std::runtime_error>([&] { adapter.StartLevel(11); });
        Check(adapter.GetRuntimeState() == GameRuntimeState::LoadFailed &&
            adapter.GetLevelLoadError().find("FELD11.256") != std::string::npos,
            "Geometry errors must reach the derived context.");
    }

    void TestMapPlayerStart(const std::filesystem::path& levelDirectory)
    {
        class SpawnHost final : public GameContext
        {
        public:
            using GameContext::InitializeLevelPlayerSpawn;
        } host;
        const std::array<std::int16_t, 3> direction{ -100,17,0 };
        auto data = MakeLevel();
        data.level->wallTextures[4][12] = 0xe1;
        data.level->wallTextures[1][1] = 0xe0;
        data.level->wallTextures[10][20] = 0xe2;
        data.level->wallTextures[39][49] = 0xef;
        for (auto& row : data.level->floorHeights) { std::fill(std::begin(row), std::end(row), std::uint8_t{ 10 }); }
        auto oldTerrain = CreateDummyLevel();
        for (auto& row : oldTerrain.floorHeights) { std::fill(std::begin(row), std::end(row), std::uint8_t{ 200 }); }
        host.level = &oldTerrain;
        host.floorRepeatHeight = 99;
        host.InitializeLevelPlayerSpawn(data, direction);
        Check(data.playerSpawn && data.playerSpawn->position == std::array<std::int16_t, 3>{ 1350,29,3650 } &&
            data.playerSpawn->direction == direction,
            "Exact E1 selection must use pending terrain and native biased interpolation plus 45, preserving direction.");
        data.level->wallTextures[4][12] = 0;
        data.level->wallTextures[20][7] = 0xe1;
        data.level->wallTextures[1][8] = 0xe1;
        data.level->wallTextures[2][8] = 0xe1;
        host.InitializeLevelPlayerSpawn(data, direction);
        Check(data.playerSpawn->position == std::array<std::int16_t, 3>{ 950,29,3850 },
            "Last matching column, then last matching row must win, not row-major order.");
        data.level->surfaceFlags[2][8] = 28;
        data.floorRepeatHeight = 50;
        GameContext reference;
        reference.level = data.level.get();
        reference.floorRepeatHeight = data.floorRepeatHeight;
        const auto repeatedY = WrapSignedWord(reference.GetTerrainFloorHeight(950, 0, 3850) + 45);
        host.InitializeLevelPlayerSpawn(data, direction);
        Check(data.playerSpawn->position[1] == repeatedY, "Spawn height must honor pending stacked/repeated-floor rules.");
        const auto preserved = *data.playerSpawn;
        data.level->wallTextures[20][7] = 0;
        data.level->wallTextures[1][8] = 0;
        data.level->wallTextures[2][8] = 0;
        ExpectError<std::runtime_error>([&] { host.InitializeLevelPlayerSpawn(data, direction); });
        Check(data.playerSpawn->position == preserved.position && data.playerSpawn->direction == preserved.direction,
            "Missing-marker errors must not overwrite an existing explicit spawn.");
        data.level->wallTextures[39][49] = 0xe1;
        ExpectError<std::out_of_range>([&] { host.InitializeLevelPlayerSpawn(data, direction); });
        Check(data.playerSpawn->position == preserved.position, "Invalid marker terrain must fail before changing spawn.");
        LevelLoadData noGeometry;
        ExpectError<std::invalid_argument>([&] { host.InitializeLevelPlayerSpawn(noGeometry, direction); });

        const auto read = [](const std::filesystem::path& path)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file) { throw std::runtime_error("Cannot read player start fixture."); }
            return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        };
        std::size_t levels = 0;
        for (const auto& entry : std::filesystem::directory_iterator(levelDirectory))
        {
            const auto name = entry.path().filename().u8string();
            if (name.rfind("FELD", 0) != 0 || entry.path().extension() != ".256" ||
                name == "FELD9.256" || name == "FELD29.256") { continue; }
            const auto bytes = read(entry.path());
            const std::size_t width = static_cast<std::size_t>(bytes[0] | (bytes[1] << 8)) + 1;
            const std::size_t height = static_cast<std::size_t>(bytes[2] | (bytes[3] << 8)) + 1;
            auto original = MakeLevel();
            original.level->LoadGeometry(bytes.data(), bytes.size());
            int lastColumn = -1;
            int lastRow = -1;
            for (std::size_t column = 0; column < width; ++column)
            {
                for (std::size_t row = 0; row < height; ++row)
                {
                    if (bytes[4 + row * width + column] == 0xe1)
                    {
                        lastColumn = static_cast<int>(column);
                        lastRow = static_cast<int>(row);
                    }
                }
            }
            Check(lastColumn >= 0, "Original level must have a normal start marker.");
            host.InitializeLevelPlayerSpawn(original, direction);
            Check(original.playerSpawn->position[0] == 150 + 100 * lastColumn &&
                original.playerSpawn->position[2] == 4050 - 100 * lastRow,
                "Original asset start coordinates or duplicate priority differ from native rules.");
            Host runtime;
            const auto request = runtime.StartLevel(11);
            const auto expected = *original.playerSpawn;
            if (name == "FELD17.256")
            {
                const auto objects = read(levelDirectory / "OBJPOS17.DAT");
                original.objects[46] = DecodeGameObject(objects.data() + 44 * 54, 54);
                Check(original.objects[46].behavior == BehaviorType::LevelStartOverride,
                    "Expected the original cave override in native slot 46.");
            }
            runtime.CompleteLevelLoad(request.id, std::move(original));
            const auto camera = runtime.GetPlayerCamera();
            Check(camera.position == expected.position && camera.direction == direction &&
                runtime.worldObjects[1].posY == WrapSignedWord(expected.position[1] - 25),
                "Level completion must convert selected eye position to player center exactly once.");
            if (name == "FELD17.256")
            {
                Input input;
                runtime.Tick({ 0,0 }, input);
                Check(runtime.worldObjects[46].health == 0 &&
                    runtime.GetPlayerCamera().position == std::array<std::int16_t, 3>{ 1095,91,3392 },
                    "The real cave marker must overlap its override and start the player above the cave floor.");
                runtime.Tick({ 0,1 }, input);
                Check(runtime.GetPlayerCamera().position[1] == 90 && runtime.worldObjects[46].health == 0,
                    "The consumed cave override must allow normal gravity to start the drop, not repeatedly snap upward.");
            }
            ++levels;
        }
        Check(levels == 36, "Expected every complete original FELD fixture.");
        auto geometry = read(levelDirectory / "FELD11.256");
        const auto objects = read(levelDirectory / "OBJPOS11.DAT");
        Host adapter;
        adapter.files = [&](std::string_view filename)
        {
            if (filename == "LEVEL\\FELD11.256") { return geometry; }
            Check(filename == "LEVEL\\OBJPOS11.DAT", "Unexpected player-start file request.");
            return objects;
        };
        adapter.initialDirection = direction;
        adapter.StartLevel(11);
        Check(adapter.GetRuntimeState() == GameRuntimeState::Ready,
            "The derived context must load normal starts without any position input.");
        Check(adapter.GetPlayerCamera().position[0] == 1350 && adapter.GetPlayerCamera().position[2] == 3650 &&
            adapter.GetPlayerCamera().direction == direction, "The renderer host must use the exact native level-11 marker.");
        const auto previous = adapter.GetPlayerCamera();
        for (std::size_t i = 0; i < 25 * 28; ++i) { if (geometry[4 + i] == 0xe1) { geometry[4 + i] = 0xef; } }
        ExpectError<std::runtime_error>([&] { adapter.StartLevel(11); });
        Check(adapter.GetRuntimeState() == GameRuntimeState::LoadFailed &&
            adapter.GetLevelLoadError().find("0xE1") != std::string::npos &&
            adapter.GetPlayerCamera().position == previous.position,
            "Missing normal starts must fail visibly without installing a partial world or reusing renderer guesses.");
    }

    void TestLevelStartOverride(const std::filesystem::path& levelDirectory)
    {
        GameContext context;
        auto& player = context.worldObjects[1];
        player.health = 0; // Native overlap deliberately does not test player health.
        player.field_07 = 3;
        player.collisionRadiusXZ = 15;
        player.collisionRadiusY = 23;
        player.posX = 1000;
        player.posY = 20;
        player.posZ = 2500;
        player.velocityX = -100;
        player.velocityY = 17;
        player.velocityZ = 4;
        auto& object = context.worldObjects[2];
        object.behavior = BehaviorType::LevelStartOverride;
        object.health = 1000;
        object.field_07 = 1;
        object.posX = 1007;
        object.posY = 31;
        object.posZ = 2491;
        object.collisionRadiusXZ = 10;
        object.collisionRadiusY = 10;
        const auto originalObject = object;
        const auto originalPlayer = player;
        const auto checkNoEffect = [&]()
        {
            const auto beforeObject = object;
            const auto beforePlayer = player;
            const auto result = DispatchObjectCase(object, context);
            Check((result.status == DispatchStatus::ExecutedCase || result.status == DispatchStatus::InactiveObject) &&
                std::memcmp(&object, &beforeObject, sizeof(object)) == 0 &&
                std::memcmp(&player, &beforePlayer, sizeof(player)) == 0,
                "Inactive, gated or non-overlapping start overrides must not mutate either record.");
        };
        for (const int health : { 0, -1, 999, 1001, 2000 })
        {
            object.health = static_cast<std::int16_t>(health);
            checkNoEffect();
        }
        object = originalObject;
        context.objectCollisionsEnabled = false;
        checkNoEffect();
        context.objectCollisionsEnabled = true;
        object.field_07 = 4;
        checkNoEffect();
        for (int axis = 0; axis < 3; ++axis)
        {
            for (const int sign : { -1, 1 })
            {
                object = originalObject;
                if (axis == 0) { object.posX = static_cast<std::int16_t>(player.posX + sign * 25); }
                if (axis == 1) { object.posY = static_cast<std::int16_t>(player.posY + sign * 33); }
                if (axis == 2) { object.posZ = static_cast<std::int16_t>(player.posZ + sign * 25); }
                checkNoEffect();
            }
        }
        object = originalObject;
        const auto result = DispatchObjectCase(object, context);
        auto expectedObject = originalObject;
        expectedObject.health = 0;
        auto expectedPlayer = originalPlayer;
        expectedPlayer.posX = originalObject.posX;
        expectedPlayer.posY = originalObject.posY;
        expectedPlayer.posZ = originalObject.posZ;
        Check(result.status == DispatchStatus::ExecutedCase && result.moduleOffset == 0x37dd &&
            std::memcmp(&object, &expectedObject, sizeof(object)) == 0 &&
            std::memcmp(&player, &expectedPlayer, sizeof(player)) == 0,
            "Override must copy precisely six center-coordinate bytes, preserving direction and every other field.");
        player.posX = 1200;
        checkNoEffect();

        for (const int levelNumber : { 17, 23 })
        {
            std::ifstream file(levelDirectory / ("OBJPOS" + std::to_string(levelNumber) + ".DAT"), std::ios::binary);
            if (!file) { throw std::runtime_error("Cannot read original start-override fixture."); }
            const std::vector<std::uint8_t> bytes{
                std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
            bool found = false;
            for (std::size_t offset = 0; offset + 54 <= bytes.size(); offset += 54)
            {
                const auto record = DecodeGameObject(bytes.data() + offset, 54);
                if (record.behavior != BehaviorType::LevelStartOverride) { continue; }
                found = true;
                Host runtime;
                auto levelData = MakeLevel();
                levelData.objects[2] = record;
                levelData.playerSpawn = PlayerCamera{
                    { WrapSignedWord(record.posX - 1), WrapSignedWord(record.posY + 25), record.posZ }, { 0,17,100 } };
                const auto request = runtime.StartLevel(static_cast<std::int16_t>(levelNumber));
                runtime.CompleteLevelLoad(request.id, std::move(levelData));
                const auto before = runtime.worldObjects[1];
                Input input;
                Check(runtime.Tick({ 0,0 }, input).objects->at(0).status == DispatchStatus::ExecutedCase,
                    "Original override must be dispatched in the full behavior loop.");
                Check(runtime.worldObjects[2].health == (levelNumber == 17 ? 0 : 2000) &&
                    runtime.worldObjects[1].posX == (levelNumber == 17 ? record.posX : before.posX) &&
                    runtime.worldObjects[1].posY == before.posY && runtime.worldObjects[1].velocityY == 17,
                    "Real cave override must fire once, while the initial health-2000 level-23 record must not.");
                Check(runtime.GetPlayerCamera().position[1] == record.posY + 25,
                    "Override coordinates are object-center coordinates, not eye coordinates.");
            }
            Check(found, "Expected original LevelStartOverride record.");
        }
    }

    void TestObjectBoundary()
    {
        struct RendererObject
        {
            std::uint8_t bytes[54]{};
            int rendererMetadata = 123;
        } renderer;
        for (unsigned int value = 0; value < 65536; ++value)
        {
            for (std::size_t i = 0; i < sizeof(renderer.bytes); ++i)
            {
                renderer.bytes[i] = static_cast<std::uint8_t>(value + i * 17);
            }
            renderer.bytes[8] = static_cast<std::uint8_t>(value);
            renderer.bytes[9] = static_cast<std::uint8_t>(value >> 8);
            const auto game = DecodeGameObject(renderer.bytes, sizeof(renderer.bytes));
            Check(static_cast<std::uint16_t>(game.behavior) == value &&
                game.behaviorParameter == renderer.bytes[52] && game.spriteScale == renderer.bytes[53],
                "Byte import must preserve full-width behavior IDs and both independent trailing bytes.");
            std::array<std::uint8_t, 54> saved;
            std::copy(std::begin(renderer.bytes), std::end(renderer.bytes), saved.begin());
            std::fill(std::begin(renderer.bytes), std::end(renderer.bytes), std::uint8_t{ 0 });
            EncodeGameObject(game, renderer.bytes, sizeof(renderer.bytes));
            Check(std::equal(saved.begin(), saved.end(), std::begin(renderer.bytes)) &&
                renderer.rendererMetadata == 123, "Object export must not touch renderer metadata.");
        }
        ExpectError<std::invalid_argument>([&] { DecodeGameObject(nullptr, 54); });
        ExpectError<std::invalid_argument>([&] { DecodeGameObject(renderer.bytes, 53); });
        ExpectError<std::invalid_argument>([&] { EncodeGameObject({}, nullptr, 54); });
        ExpectError<std::invalid_argument>([&] { EncodeGameObject({}, renderer.bytes, 55); });
    }

    std::string ReadText(const char* path)
    {
        std::ifstream file(path);
        if (!file) { throw std::runtime_error("Cannot open enum compatibility input."); }
        return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
    }

    std::map<std::string, unsigned long> ReadEnum(const std::string& text, const std::string& name)
    {
        const std::regex declaration("enum\\s+class\\s+" + name + "(?:\\s*:\\s*[^\\{]+)?\\s*\\{([^}]*)\\}");
        std::smatch match;
        Check(std::regex_search(text, match, declaration), "Cannot find enum declaration.");
        const auto body = std::regex_replace(match[1].str(), std::regex("//[^\\r\\n]*"), "");
        const std::regex entry("^\\s*([A-Za-z_][A-Za-z_0-9]*)\\s*(?:=\\s*(0[xX][0-9A-Fa-f]+|[0-9]+))?\\s*$");
        std::map<std::string, unsigned long> values;
        unsigned long next = 0;
        std::size_t start = 0;
        while (start < body.size())
        {
            const auto comma = body.find(',', start);
            const auto part = body.substr(start, comma == std::string::npos ? comma : comma - start);
            if (part.find_first_not_of(" \r\n\t") != std::string::npos)
            {
                std::smatch item;
                Check(std::regex_match(part, item, entry), "Unsupported enum expression; update the compatibility test.");
                if (item[2].matched)
                {
                    const auto number = item[2].str();
                    next = std::stoul(number, nullptr, number.find_first_of("xX") != std::string::npos ? 16 : 10);
                }
                Check(values.emplace(item[1].str(), next++).second, "Duplicate enum name.");
            }
            if (comma == std::string::npos) { break; }
            start = comma + 1;
        }
        Check(!values.empty(), "Empty enum compatibility input.");
        return values;
    }

    void CheckEnums(const std::string& renderer, const std::string& game)
    {
        for (const auto* type : { "BehaviorType", "CollisionType", "DisplayType", "WeaponType" })
        {
            const auto source = ReadEnum(renderer, type);
            const auto destination = ReadEnum(game, type);
            for (const auto& item : source)
            {
                const auto name = std::string(type) == "BehaviorType" && item.first == "SwimmingParticles" ?
                    "FloatingDebris" : item.first;
                const auto found = destination.find(name);
                if (found == destination.end() || found->second != item.second)
                {
                    throw std::runtime_error(std::string("Renderer/game enum mismatch: ") + type + "::" + item.first);
                }
            }
        }
    }
}

int main(int argc, char** argv)
{
    try
    {
        Check(argc == 5, "Expected enum header paths, original level directory and native player save fixture.");
        TestLoading();
        TestWorldObjectLifecycle();
        TestLevelObjectLifecycle();
        TestImmediateObjectRemoval();
        TestObjectFactoryLifecycle();
        TestDamageFactoryLifecycle();
        TestWeaponCreationLifecycle();
        TestLevelPlayerInitialization();
        TestTick();
        TestPlayerCamera();
        TestBasicPlayerMovement();
        TestPlayerGrounding();
        TestLoadedLevelInteraction(argv[3], argv[4]);
        TestPromptHooks();
        TestSimpleSwitchInteraction();
        TestLevelObjectLoading();
        TestWallCollisionLoading(argv[3]);
        TestMapPlayerStart(argv[3]);
        TestLevelStartOverride(argv[3]);
        TestObjectBoundary();
        const auto renderer = ReadText(argv[1]);
        const auto game = ReadText(argv[2]);
        CheckEnums(renderer, game);
        auto mismatch = renderer;
        const auto pillar = mismatch.find("Pillar = 8");
        Check(pillar != std::string::npos, "Expected renderer Pillar fixture.");
        mismatch.replace(pillar, std::string("Pillar = 8").size(), "Pillar = 9");
        ExpectError<std::runtime_error>([&] { CheckEnums(mismatch, game); });
        std::cout << "PASS: owned level lifecycle, native static wall loading, basic keyboard movement/collision/camera, non-blocking tick/pause/input, separate clocks, "
            "load errors, 65536 object payloads, and renderer enum compatibility.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
