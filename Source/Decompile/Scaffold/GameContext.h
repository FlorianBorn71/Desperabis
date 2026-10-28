#pragma once

#include "ApplicationFlow.h"
#include "GameEnums.h"
#include "GameRuntime.h"
#include "Sound.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace Desperabis
{
    struct RawGameObject
    {
        std::uint8_t modelId = 0;
        std::uint8_t modelAnim = 0;
        std::uint8_t modelFrame2 = 0;
        std::uint8_t animFrame = 0;
        // Reserved for render-list placement/culling configuration, not free behavior storage.
        // Selects tile-assignment coordinates, sort-key biases, and visibility paths; full mode meanings remain provisional.
        std::uint8_t renderPlacementMode = 0;
        std::uint8_t soundEffectId = 0;
        CollisionType collisionType = CollisionType::None;
        std::uint8_t collisionMask = 0;
        BehaviorType behavior = BehaviorType::None;
        std::int16_t health = 0;
        std::int16_t posX = 0;
        std::int16_t posY = 0;
        std::int16_t posZ = 0;
        std::int16_t velocityX = 0;
        std::int16_t velocityY = 0;
        std::int16_t velocityZ = 0;
        std::int16_t collisionRadiusXZ = 0;
        std::int16_t collisionRadiusY = 0;
        std::int16_t winX = 0;
        std::int16_t winY = 0;
        std::int16_t winZ = 0;
        std::int16_t itemValue = 0;
        // Behavior-specific words: counters, rates, subtypes, links, or other parameters.
        std::uint16_t generalPurpose1 = 0;
        std::uint16_t generalPurpose2 = 0;
        std::uint16_t generalPurpose3 = 0;
        std::int16_t blendWeightOrSpriteOpacity = 0;
        std::int16_t groupIndexOrLock = 0;
        // Door lock/group mask; fireplaces use emission intensity, fire particles use vertical acceleration.
        std::int16_t doorLockOrGroupBitmask = 0;
        std::int16_t renderSortKey = 0; // Original render-list scratch, overwritten after active-object dispatch.
        // Provisional: nonzero enables pickup/contact priority; individual values remain unresolved.
        PickupType pickupType = PickupType::None;
        DisplayType displayType = DisplayType::SpriteNormal;
        // Contact damage factor, door occupancy, switch latch, or other behavior-specific parameter.
        std::uint8_t behaviorParameter = 0;
        std::uint8_t spriteScale = 0;
    };

    static_assert(std::is_standard_layout_v<RawGameObject>);
    static_assert(sizeof(RawGameObject) == 54);
    static_assert(offsetof(RawGameObject, modelId) == 0);
    static_assert(offsetof(RawGameObject, modelAnim) == 1);
    static_assert(offsetof(RawGameObject, modelFrame2) == 2);
    static_assert(offsetof(RawGameObject, animFrame) == 3);
    static_assert(offsetof(RawGameObject, renderPlacementMode) == 4);
    static_assert(offsetof(RawGameObject, soundEffectId) == 5);
    static_assert(offsetof(RawGameObject, collisionType) == 6);
    static_assert(offsetof(RawGameObject, collisionMask) == 7);
    static_assert(offsetof(RawGameObject, behavior) == 8);
    static_assert(offsetof(RawGameObject, health) == 10);
    static_assert(offsetof(RawGameObject, posX) == 12);
    static_assert(offsetof(RawGameObject, posY) == 14);
    static_assert(offsetof(RawGameObject, posZ) == 16);
    static_assert(offsetof(RawGameObject, velocityX) == 18);
    static_assert(offsetof(RawGameObject, velocityY) == 20);
    static_assert(offsetof(RawGameObject, velocityZ) == 22);
    static_assert(offsetof(RawGameObject, collisionRadiusXZ) == 24);
    static_assert(offsetof(RawGameObject, collisionRadiusY) == 26);
    static_assert(offsetof(RawGameObject, winX) == 28);
    static_assert(offsetof(RawGameObject, winY) == 30);
    static_assert(offsetof(RawGameObject, winZ) == 32);
    static_assert(offsetof(RawGameObject, itemValue) == 34);
    static_assert(offsetof(RawGameObject, generalPurpose1) == 36);
    static_assert(offsetof(RawGameObject, generalPurpose2) == 38);
    static_assert(offsetof(RawGameObject, generalPurpose3) == 40);
    static_assert(offsetof(RawGameObject, blendWeightOrSpriteOpacity) == 42);
    static_assert(offsetof(RawGameObject, groupIndexOrLock) == 44);
    static_assert(offsetof(RawGameObject, doorLockOrGroupBitmask) == 46);
    static_assert(offsetof(RawGameObject, renderSortKey) == 48);
    static_assert(offsetof(RawGameObject, pickupType) == 50);
    static_assert(offsetof(RawGameObject, displayType) == 51);
    static_assert(offsetof(RawGameObject, behaviorParameter) == 52);
    static_assert(offsetof(RawGameObject, spriteScale) == 53);

    static_assert(std::is_trivially_copyable_v<RawGameObject>);

    // Copy only the native payload, never a renderer wrapper or an aliased struct.
    RawGameObject DecodeGameObject(const std::uint8_t* bytes, std::size_t size);
    void EncodeGameObject(const RawGameObject& object, std::uint8_t* bytes, std::size_t size);

    inline constexpr std::uint16_t CollisionMapValue_BlockingWall = 0x38;

    struct CollisionMap
    {
        static constexpr std::size_t VARIABLE_SIZE = 200 * 200;

        std::uint16_t widthMinusOne = 0;
        std::uint16_t heightMinusOne = 0;
        std::uint8_t data[VARIABLE_SIZE]{};
    };

    static_assert(std::is_standard_layout_v<CollisionMap>);
    static_assert(std::is_trivially_copyable_v<CollisionMap>);
    static_assert(offsetof(CollisionMap, widthMinusOne) == 0);
    static_assert(offsetof(CollisionMap, heightMinusOne) == 2);
    static_assert(offsetof(CollisionMap, data) == 4);
    static_assert(sizeof(CollisionMap) == 200 * 200 + 4);

    struct Level
    {
        static constexpr std::size_t Width = 50;
        static constexpr std::size_t Height = 40;
        static constexpr std::size_t GeometryStreamSize = 4 + 7 * Width * Height;

        std::unique_ptr<CollisionMap> collisionMap;
        std::uint8_t wallTextures[Height][Width]{};
        std::uint8_t floorObjects[Height][Width]{};
        std::uint8_t floorHeights[Height][Width]{};
        std::uint8_t roomHeights[Height][Width]{};
        std::uint8_t ceilingObjects[Height][Width]{};
        std::uint8_t surfaceFlags[Height][Width]{};
        std::uint8_t collisionMaterials[Height][Width]{};
        std::size_t geometryWidth = Width;
        std::size_t geometryHeight = Height;
        std::uint64_t geometryRevision = 0;

        struct HeightSurface
        {
            std::int16_t depth;
            std::uint8_t value;
        };
        // DS:D494..D49A and the byte lookup used by 1E48:1AF9; not in the geometry streams.
        std::array<std::optional<HeightSurface>, 4> heightSurfaces{};

        void LoadGeometry(const std::uint8_t* data, std::size_t size);
        void BuildCollisionMap();
    };

    Level CreateDummyLevel();

    struct LevelLoadData
    {
        std::unique_ptr<Level> level;
        // Native indexing: reserved 0, player 1, ordinary objects 2..164, fallback 165.
        std::array<RawGameObject, 166> objects{};
        std::int16_t floorRepeatHeight = 0;
        // Fresh level: GameContext creates slot 1 from this eye position/view direction.
        // Omit to preserve the supplied slot-1 record (e.g. a save or recovery fixture).
        std::optional<PlayerCamera> playerSpawn;
    };

    struct LightingCache
    {
        static constexpr std::size_t Size = 21;
        static constexpr std::uint8_t MaxRadius = 10;

        std::int16_t centerX = 0;
        std::int16_t centerZ = 0;
        std::uint8_t activeRadius = MaxRadius;
        // X first, then positive world Z; native record offset 10h.
        std::array<std::array<std::int16_t, Size>, Size> contributions{};

        void Reset(std::int16_t x, std::int16_t z, std::uint8_t radius = MaxRadius);
        // Returns false for a source outside the active window; contributions wrap as signed words.
        bool AddAtWorldXZ(std::int16_t x, std::int16_t z, std::int16_t amount);
    };

    enum class InputKey
    {
        ForwardKey,
        BackwardKey,
        StrafeLeftKey,
        StrafeRightKey,
        TurnLeftKey,
        TurnRightKey,
        InteractionKey,
        AttackKey,
        SelectDaggerKey,
        SelectSwordKey,
        JumpKey,
        CrouchKey,
        SelectCrossbowKey,
        SelectFireballKey,
        SelectStoneKey,
        SelectPlaceableKey,
        SelectElectricityKey,
        SelectShovelKey,
        SelectScytheKey,
        SelectLentilsKey,
        LookUpKey,
        LookDownKey,
        CenterLookKey
    };

    class InputHandler
    {
    public:
        virtual ~InputHandler() = default;
        virtual bool IsKeyPressed(InputKey key) = 0;
    };

    class GameContext
    {
    public:
        GameContext();
        virtual ~GameContext();
        GameContext(const GameContext&) = delete;
        GameContext& operator=(const GameContext&) = delete;
        GameContext(GameContext&&) noexcept;
        GameContext& operator=(GameContext&&) noexcept;

        // Host-driven API. All calls, including load completion, must run on the game thread.
        LevelLoadRequest StartLevel(std::int16_t levelNumber);
        void CompleteLevelLoad(std::uint64_t requestId, LevelLoadData data);
        void FailLevelLoad(std::uint64_t requestId, std::string_view error);
        GameRuntimeState GetRuntimeState() const;
        std::optional<LevelLoadRequest> GetPendingLevelLoad() const;
        std::optional<std::int16_t> GetLoadedLevelNumber() const;
        const std::string& GetLevelLoadError() const;
        void SetPaused(bool paused);
        bool IsPaused() const;
        GameTickResult Tick(GameTickTiming timing, InputHandler& input);
        const std::array<RawGameObject, 166>& GetWorldObjects() const;
        // Immediate game-thread hooks for active (health > 0) world slots 1..165.
        // Destruction sees the old record; creation sees the initialized replacement.
        // Hooks must not throw or mutate the world/reenter gameplay.
        virtual void OnObjectCreated(int slot);
        virtual void OnObjectDestroyed(int slot);
        // Immediate after clamping/depletion; both arguments are signed 16.16 life values.
        // Only actual changes through ApplyPlayerLifeDelta notify; do not mutate/reenter gameplay.
        virtual void OnPlayerHealthChanged(std::int32_t previousLife, std::int32_t currentLife);
        // Use these for external lifecycle changes; raw worldObjects writes bypass notifications.
        void ReplaceWorldObject(int slot, RawGameObject object);
        void SetObjectHealth(RawGameObject& object, std::int16_t health);
        const Level* GetLevel() const;
        const LightingCache& GetLightingCache() const;
        // Eye-position snapshot, including while paused/loading; check loaded-level availability separately.
        PlayerCamera GetPlayerCamera() const;

        MenuPage GetMenuPage() const;
        void SetMenuPage(MenuPage page);
        // Runs modal pages until Quit. Unrecovered page implementations throw explicitly.
        void RunApplication();
        void RunApplicationStep();
        bool CanResumeGame() const;
        // Returns false for a disabled choice, leaving the current page unchanged.
        bool SelectMainMenuAction(MainMenuAction action);
        bool OpenInventory();
        void StartNewGame(std::int16_t chapter);

        bool hasActiveGame = false; // Original resume/save availability flag, separate from page selection.
        // Partial normal keyboard/terrain controller, not the complete native player simulation.
        bool basicPlayerMovementEnabled = true;
        std::int16_t tickCount = 1;
        bool behaviorDispatchEnabled = true;
        CollisionMap* collisionMap = nullptr;
        // Original DS:EC70, initialized to five FF bytes.
        std::uint8_t switchGroupMasks[5] = { 0xff, 0xff, 0xff, 0xff, 0xff };
        // Original inventory slots 1..14: two hands followed by twelve backpack slots.
        std::array<RawGameObject, 14> inventory{};
        std::int16_t gold = 0;
        std::int16_t arrowPoison = 0;
        bool hasMap = false;
        // DS:DD36; includes ammunition/condition and still-unidentified resource entries.
        std::array<std::int16_t, 32> weaponAmounts = {
            0, 0, 0, 0, 0, 0, 0, 0,
            100, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 1000, 0, 0, 100
        };
        // DS:DD76; zero means absent, otherwise an original 1-based inventory slot.
        std::array<std::uint16_t, 32> weaponInventorySlots{};
        // Original slot 1 is the player, 2..164 are query candidates; slot 0 is reserved.
        // CE9E's post-test loop also reads slot 165 when the initial hit is 164.
        std::array<RawGameObject, 166> worldObjects{};
        bool objectCollisionsEnabled = true;
        bool discardBlockedObjects = false; // DS:D501; false exposes the native WC-Error diagnostic path.
        std::uint16_t playerCollisionSlot = 0;
        Level* level = nullptr;
        std::int16_t floorSurfaceValue = -1; // DS:D4CC, meaning not fully established.
        std::int16_t floorRepeatHeight = 0; // DS:D4CE, in raw heightfield units.
        AudioHandler* audioHandler = nullptr;
        bool soundEnabled = true;
        std::array<SoundEffectDefinition, SoundEffectCount> soundEffects{};
        bool soundEffectsLoaded = false;
        std::array<std::int16_t, SoundChannelCount> soundChannelSources{};
        std::uint32_t randomSeed = 0; // Turbo Pascal RandSeed, DS:04C2.
        std::int16_t soundListenerX = 0; // DS:D4D2.
        std::int16_t soundListenerZ = 0; // DS:D4D6.
        bool positionalSoundsSuppressed = false; // DS:D4FA forces distance 10000.
        // DS:2DBA meter: maximum at +2, signed 16.16 current value at +4.
        std::int16_t playerLifeMaximum = 360;
        std::int32_t playerLife = 360 * 65536;
        std::int16_t playerLifeDeltaSuppressed = 0; // DS:2DDD.
        std::int16_t playerHitEffectTicks = 0; // DS:D514; presentation consumer not yet ported.
        std::int16_t playerBodyHeight = 45; // DS:D4B2: standing 45, crouching 28.
        std::int16_t playerPostureDelta = 0; // DS:D4B4: raise/lower request; also -1 on depletion.
        bool playerGrounded = false; // DS:05D4.
        WeaponType selectedWeapon = WeaponType::None; // DS:DDB6.
        WeaponType requestedWeapon = WeaponType::None; // DS:DDB7.
        std::uint8_t weaponSpriteFrame = 0; // DS:DDB8.
        std::uint8_t weaponHand = 0; // DS:DDB9, original inventory slot 1/2.
        std::uint8_t requestedWeaponHand = 0; // DS:DDBA.
        bool weaponReady = true; // DS:DDC5; resource/strike latch, not the cooldown counter.
        // Historical name: DS:DDBB is a phase/countdown, including negative command sentinels.
        std::int16_t weaponAnimationSpeed = 0; // DS:DDBB.
        std::int16_t weaponAnimationPosition = 0; // DS:DDBD.
        std::int16_t weaponAnimationRecoil = 0; // DS:DDBF, set by digging contacts.
        std::int16_t playerLookHeight = 110; // DS:D4A4, screen horizon rather than an angle.
        std::int16_t playerTargetLookHeight = 110; // DS:D4B6, keyboard-requested horizon.
        bool playerUnderwater = false; // DS:D4FC; swimming controller remains separate.
        bool field_D500 = true; // Native body-mode gate, also used by the unarmed weapon branch.
        bool debugWeaponColliders = false; // DS:D502.
        void RefreshWeaponInventory();
        bool SelectWeapon(WeaponType weapon);
        WeaponActionStatus AttackWithSelectedWeapon();
        WeaponActionStatus UpdateWeaponAnimation(std::int16_t ticks);
        std::int16_t GetPlayerHeading() const;
        // Native 10AC:CF23/CF4B; raising from crouch precedes jumping.
        void RequestPlayerJump();
        void RequestPlayerCrouch();
        // Native 0000:DA3C: selected cockroach contact uses forward speed, not an interaction key.
        InteractionResult HandleCockroachContact(RawGameObject& object);
        // Authoritative player-life update, including suppression, clamping, death and notification.
        void ApplyPlayerLifeDelta(std::int32_t delta);
        // Native 10AC:10F5: zero activates, positive values damage, -1 selects special destruction.
        void DamageObject(RawGameObject& object, std::int16_t amount);
        void ActivateObjectGroup(std::int16_t group);
        void StopAllSoundEffects();
        // Recovered collision-type-30 branch of 10AC:10F5.
        void DamageSkeleton(RawGameObject& object, std::int16_t amount);
        void SpawnSkeletonFragment(const RawGameObject& source, std::uint16_t part,
            std::int16_t x, std::int16_t y, std::int16_t z);
        void SpawnWaterImpact(std::int16_t x, std::int16_t z);
        // Native offsetY + encoded height (pixel count minus one), before spriteScale / 64.
        virtual std::int16_t GetSpriteBottom(const RawGameObject& object) const;
        std::int16_t frameTickCount = 1; // DS:D4CA; burning uses this rather than the behavior argument.
        LightingCache lightingCache;
        std::uint16_t field_DC34 = 0; // Near lights add random values; also used as a timer RNG range.
        // The app owns prompt bytes, lifetime and presentation. Empty text requests a reset on level completion.
        virtual void PromptUserMessage(std::string_view text, std::int16_t duration);
        virtual bool IsUserMessageActive() const;
        std::uint16_t Random(std::uint16_t range);
        // Native signed random helper replaces a zero result with the positive radius.
        std::int16_t RandomSigned(std::uint16_t radius);
        void LoadSoundEffects(const std::uint8_t* data, std::size_t size);
        std::int16_t GetSoundDistanceAt(std::int16_t x, std::int16_t z) const;
        static std::uint8_t GetSoundVolume(std::int16_t distance);
        std::uint8_t GetSoundVolumeAt(std::int16_t x, std::int16_t z, std::int16_t distanceScale = 1) const;
        SoundPlaybackStatus PlaySoundEffect(SoundEffect effect, std::uint8_t volume,
            std::int16_t sourceId, std::int16_t instanceLimit);
        SoundPlaybackStatus PlaySoundEffect(SoundEffect effect, std::uint8_t volume,
            const RawGameObject& source, std::int16_t instanceLimit);

        // Original 1E48:1B45 and 1E48:1BA6; room height is relative, not absolute Y.
        std::int16_t GetFloorHeight(std::int16_t x, std::int16_t y, std::int16_t z);
        std::int16_t GetRoomHeight(std::int16_t x, std::int16_t z) const;
        // Original 1E48:18CF: terrain interpolation without the flat-surface shortcut.
        std::int16_t GetTerrainFloorHeight(std::int16_t x, std::int16_t y, std::int16_t z) const;
        // Original 1E48:1AF9, including the floorSurfaceValue side effect.
        std::int16_t GetHeightSurfaceDepth(std::uint16_t surface);

        // Original 10AC:387B: three word slots, X/Z/value push order, RETF 6.
        void SetCollisionMapPixelAtWorldXZ(std::int16_t x, std::int16_t z, std::uint16_t value);
        void SetDoorCollisionMapAtWorldXZ(std::int16_t x, std::int16_t z, std::uint16_t value);
        void SetBarrierCollisionMapAtWorldXZ(std::int16_t x, std::int16_t z,
            std::int16_t heading, std::uint8_t value);

        // Original 1E48:CF49: X/Z word arguments, RETF 4; returns a byte in AL.
        std::uint8_t GetCollisionMapPixelAtWorldXZ(std::int16_t x, std::int16_t z) const;
        // Original 1E48:0E8F and 1E48:2A64; corrections are in/out words.
        std::uint8_t GetCollisionMapPixel(std::int16_t column, std::int16_t row) const;
        void QueryCollisionMapNeighborhood(std::int16_t x, std::int16_t z, std::int16_t step,
            std::int16_t& correctionX, std::int16_t& correctionZ, std::uint16_t& center, std::uint8_t mask) const;
        // Original 10AC:1F5A: discard or disable and report a stuck object.
        void HandleBlockedObject(RawGameObject& object);

        // Original 1E48:2B33, one source-slot word; returns a slot or zero.
        std::uint16_t FindCollidingObject(std::int16_t sourceSlot) const;
        std::uint16_t FindCollidingObject(const RawGameObject& source) const;
        // Original 1E48:2D63: no player-health test.
        bool OverlapsPlayer(const RawGameObject& source) const;
        // Original 0000:CE9E, including its later-slot preference rule.
        std::uint16_t FindPlayerCollision() const;
        // Both native allocators clear the selected record, including fallback slot 165.
        std::uint16_t FindFreeObjectIndex();
        std::uint16_t FindFreeOrLowestHealthObjectIndex();
        // Recovered fire branch of the native fragment factory; zero means no particle was created.
        std::uint16_t SpawnFireParticle(const RawGameObject& source);
        // Native supplied-object initializers: neither allocates nor clears the record.
        // Behavior 18 starts at the supplied X/Z and drifts away from its previous X/Z.
        void InitializeDriftingParticle(RawGameObject& object, std::uint8_t frame,
            std::int16_t x, std::int16_t z);
        // Placement is renderer-owned configuration; riseFactor controls lifetime and vertical speed.
        void InitializeSmokeParticle(RawGameObject& object, std::int16_t x, std::int16_t y,
            std::int16_t z, std::int16_t radiusXZ, std::int16_t riseFactor, std::uint8_t placement);

    protected:
        // Logical asset names, e.g. LEVEL\OBJPOS11.DAT; failures must throw.
        virtual std::vector<std::uint8_t> LoadFile(std::string_view filename);
        // Loads FELD geometry, LEVELS.INF surface metadata and the initial static collision map.
        void LoadLevelGeometry(std::int16_t levelNumber, LevelLoadData& data);
        // Selects the exact normal map marker and derives eye Y from the pending level's terrain.
        void InitializeLevelPlayerSpawn(LevelLoadData& data, const std::array<std::int16_t, 3>& direction);
        // Loads ordinary slots 2..164, preserving reserved/player/fallback records.
        void LoadLevelObjects(std::int16_t levelNumber, LevelLoadData& data);
        // May complete synchronously or start host-side async work. Default reports an explicit error.
        virtual void LoadLevel(const LevelLoadRequest& request);
        // Each modal page owns its input/drawing loop and returns its next application page.
        virtual MainMenuAction ShowMainMenu();
        virtual std::optional<std::int16_t> ShowChapterSelection();
        virtual MenuPage RunGameplay();
        virtual MenuPage ShowSettings();
        virtual MenuPage ShowConfiguration();
        virtual MenuPage ShowLoadGame();
        virtual MenuPage ShowSaveGame();
        virtual MenuPage ShowInventory();
        virtual MenuPage ShowUnrecoveredPage(MenuPage page);
        // Native chapter acceptance calls the level-start routine with (1, chapter).
        virtual void InitializeNewGame(std::int16_t chapter);

    private:
        int FindWorldObjectSlot(const RawGameObject& object) const;
        void NotifyObjectCreated(const RawGameObject& object);
        void UpdateWeaponSelection();
        void CommitWeaponSelection();
        void InitializeWeaponCollider(std::uint16_t slot, std::int16_t reach, std::int16_t yOffset,
            std::uint8_t damage, std::int16_t radiusXZ, std::int16_t radiusY, std::int16_t lifetime);
        void ApplyObjectKnockback(RawGameObject& object, std::int16_t amount);
        void SpawnBreakupParticle(const RawGameObject& source, std::int16_t selector,
            std::int16_t radiusX, std::int16_t radiusY, std::int16_t radiusZ);
        void SpawnActivationParticle(std::int16_t x, std::int16_t y, std::int16_t z,
            std::int16_t angle, std::int16_t speed);
        static RawGameObject InitializeLevelPlayer(const PlayerCamera& spawn);
        BasicPlayerMovementResult UpdateBasicPlayerMovement(InputHandler& input);
        std::int16_t playerForwardSpeed = 0;
        std::int16_t playerSidewaysSpeed = 0;
        std::int16_t playerTurnSpeed = 0;
        std::int16_t playerVerticalSpeed = 0;
        std::int16_t playerHeading = 0;
        std::int16_t playerTargetHeading = 0;
        MenuPage menuPage = MenuPage::MainMenu;
        GameRuntimeState runtimeState = GameRuntimeState::NoLevel;
        std::uint64_t nextLoadRequestId = 0;
        std::optional<LevelLoadRequest> pendingLevelLoad;
        std::optional<std::int16_t> loadedLevelNumber;
        std::string levelLoadError;
        std::unique_ptr<Level> ownedLevel;
        bool paused = false;
    };
}
