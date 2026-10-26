#pragma once

#include "ApplicationFlow.h"
#include "GameEnums.h"
#include "Sound.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

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
        std::uint8_t field_07 = 0;
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
        std::uint16_t field_24 = 0;
        std::uint16_t field_26 = 0;
        std::uint16_t field_28 = 0;
        std::int16_t blendWeightOrSpriteOpacity = 0;
        std::int16_t groupIndexOrLock = 0;
        // Door lock/group mask; fireplaces use emission intensity, fire particles use vertical acceleration.
        std::int16_t doorLockOrGroupBitmask = 0;
        std::int16_t renderSortKey = 0; // Original render-list scratch, overwritten after active-object dispatch.
        std::uint8_t field_32 = 0;
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
    static_assert(offsetof(RawGameObject, field_07) == 7);
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
    static_assert(offsetof(RawGameObject, field_24) == 36);
    static_assert(offsetof(RawGameObject, field_26) == 38);
    static_assert(offsetof(RawGameObject, field_28) == 40);
    static_assert(offsetof(RawGameObject, blendWeightOrSpriteOpacity) == 42);
    static_assert(offsetof(RawGameObject, groupIndexOrLock) == 44);
    static_assert(offsetof(RawGameObject, doorLockOrGroupBitmask) == 46);
    static_assert(offsetof(RawGameObject, renderSortKey) == 48);
    static_assert(offsetof(RawGameObject, field_32) == 50);
    static_assert(offsetof(RawGameObject, displayType) == 51);
    static_assert(offsetof(RawGameObject, behaviorParameter) == 52);
    static_assert(offsetof(RawGameObject, spriteScale) == 53);

    static_assert(std::is_trivially_copyable_v<RawGameObject>);

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
        std::uint8_t floorHeights[Height][Width]{};
        std::uint8_t roomHeights[Height][Width]{};
        std::uint8_t ceilingObjects[Height][Width]{};
        std::uint8_t surfaceFlags[Height][Width]{};

        struct HeightSurface
        {
            std::int16_t depth;
            std::uint8_t value;
        };
        // DS:D494..D49A and the byte lookup used by 1E48:1AF9; not in the geometry streams.
        std::array<std::optional<HeightSurface>, 4> heightSurfaces{};

        void LoadGeometry(const std::uint8_t* data, std::size_t size);
    };

    Level CreateDummyLevel();

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
        InteractionKey
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
        virtual ~GameContext();

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
        const Level* level = nullptr;
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
        std::int16_t frameTickCount = 1; // DS:D4CA; burning uses this rather than the behavior argument.
        LightingCache lightingCache;
        std::uint16_t field_DC34 = 0; // Near lights add random values; also used as a timer RNG range.
        std::string userMessage;
        std::int16_t userMessageTicks = 0;

        // Store a UTF-8 prompt and its lifetime; drawing and screen placement belong to the host.
        void PromptUserMessage(std::string_view text, std::int16_t duration);
        void UpdateUserMessage(std::int16_t ticks);
        std::uint16_t Random(std::uint16_t range);
        // Native signed random helper replaces a zero result with the positive radius.
        std::int16_t RandomSigned(std::uint16_t radius);
        void LoadSoundEffects(const std::uint8_t* data, std::size_t size);
        std::int16_t GetSoundDistanceAt(std::int16_t x, std::int16_t z) const;
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
        MenuPage menuPage = MenuPage::MainMenu;
    };
}
