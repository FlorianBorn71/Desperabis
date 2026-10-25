#pragma once

#include "GameEnums.h"
#include "Sound.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
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
        std::uint16_t field_04 = 0;
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
        std::int16_t doorLockOrGroupBitmask = 0;
        std::uint16_t candidate_tileAssignment = 0;
        std::uint8_t field_32 = 0;
        DisplayType displayType = DisplayType::SpriteNormal;
        std::uint16_t spriteScale = 0;

        std::array<std::uint8_t, 54> ReadBytes() const;
        std::array<std::int16_t, 27> ReadSignedWords() const;
        std::array<std::uint16_t, 27> ReadUnsignedWords() const;
        std::uint16_t ReadWord(std::size_t offset) const;
        std::int16_t ReadSignedWord(std::size_t offset) const;
        void WriteWord(std::size_t offset, std::uint16_t value);
    };

    static_assert(std::is_standard_layout_v<RawGameObject>);
    static_assert(sizeof(RawGameObject) == 54);
    static_assert(offsetof(RawGameObject, modelId) == 0);
    static_assert(offsetof(RawGameObject, modelAnim) == 1);
    static_assert(offsetof(RawGameObject, modelFrame2) == 2);
    static_assert(offsetof(RawGameObject, animFrame) == 3);
    static_assert(offsetof(RawGameObject, field_04) == 4);
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
    static_assert(offsetof(RawGameObject, candidate_tileAssignment) == 48);
    static_assert(offsetof(RawGameObject, field_32) == 50);
    static_assert(offsetof(RawGameObject, displayType) == 51);
    static_assert(offsetof(RawGameObject, spriteScale) == 52);

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

    class InputHandler
    {
    public:
        virtual ~InputHandler() = default;
        virtual bool IsInteractionKeyPressed() = 0;
    };

    struct GameState
    {
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

        void LoadSoundEffects(const std::uint8_t* data, std::size_t size);
        std::uint8_t GetSoundVolumeAt(std::int16_t x, std::int16_t z, std::int16_t distanceScale = 1) const;
        SoundPlaybackStatus PlaySoundEffect(SoundEffect effect, std::uint8_t volume,
            std::int16_t sourceId, std::int16_t instanceLimit);
        SoundPlaybackStatus PlaySoundEffect(SoundEffect effect, std::uint8_t volume,
            const RawGameObject& source, std::int16_t instanceLimit);

        // Original 1E48:1B45 and 1E48:1BA6; room height is relative, not absolute Y.
        std::int16_t GetFloorHeight(std::int16_t x, std::int16_t y, std::int16_t z);
        std::int16_t GetRoomHeight(std::int16_t x, std::int16_t z) const;

        // Original 10AC:387B: three word slots, X/Z/value push order, RETF 6.
        void SetCollisionMapPixelAtWorldXZ(std::int16_t x, std::int16_t z, std::uint16_t value);

        // Original 1E48:CF49: X/Z word arguments, RETF 4; returns a byte in AL.
        std::uint8_t GetCollisionMapPixelAtWorldXZ(std::int16_t x, std::int16_t z) const;

        // Original 1E48:2B33, one source-slot word; returns a slot or zero.
        std::uint16_t FindCollidingObject(std::int16_t sourceSlot) const;
        // Original 0000:CE9E, including its later-slot preference rule.
        std::uint16_t FindPlayerCollision() const;
    };

    enum class InteractionStatus
    {
        Applied,
        NoEffect,
        InventoryFull,
        InactiveObject,
        UnimplementedHandler
    };

    struct InteractionResult
    {
        InteractionStatus status;
        std::uint16_t inventorySlot = 0;
    };

    // Gameplay cases of 0EF0:09F8; call on explicit interaction input, not on contact.
    // The host passes the selected object directly; UI/audio processing is not implemented.
    InteractionResult HandlePlayerObjectInteraction(RawGameObject& object, GameState& context);
    InteractionResult UpdatePlayerInteraction(GameState& context, InputHandler& input);

    enum class DispatchStatus
    {
        ExecutedCase,
        UnimplementedHandler,
        NoSpecificCase,
        InactiveObject,
        DispatchDisabled
    };

    struct DispatchResult
    {
        DispatchStatus status;
        BehaviorType behavior;
        std::uint16_t moduleOffset;
    };

    struct BehaviorDescriptor
    {
        BehaviorType type;
        const char* name;
        std::uint16_t moduleOffset;
        bool hasSpecificCase;
    };

    const std::vector<BehaviorDescriptor>& BehaviorCatalog();
    const BehaviorDescriptor* FindBehavior(BehaviorType type);
    const BehaviorDescriptor* FindBehavior(std::string_view name);
    DispatchResult DispatchObjectCase(RawGameObject& object, GameState& context);

#define BEHAVIOR_CASE(name, entry) DispatchResult Function_##name(RawGameObject&, GameState&);
#define BEHAVIOR_COMMON(name)
#include "BehaviorCases.inc"
#undef BEHAVIOR_COMMON
#undef BEHAVIOR_CASE
}
