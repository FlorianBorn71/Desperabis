#include "BehaviorDispatch.h"

#include <cstring>
#include <stdexcept>

namespace Desperabis
{
    std::array<std::uint8_t, 54> RawGameObject::ReadBytes() const
    {
        std::array<std::uint8_t, 54> result;
        std::memcpy(result.data(), this, sizeof(result));
        return result;
    }

    std::array<std::int16_t, 27> RawGameObject::ReadSignedWords() const
    {
        std::array<std::int16_t, 27> result;
        std::memcpy(result.data(), this, sizeof(result));
        return result;
    }

    std::array<std::uint16_t, 27> RawGameObject::ReadUnsignedWords() const
    {
        std::array<std::uint16_t, 27> result;
        std::memcpy(result.data(), this, sizeof(result));
        return result;
    }

    std::uint16_t RawGameObject::ReadWord(std::size_t offset) const
    {
        if (offset >= sizeof(RawGameObject) - 1)
        {
            throw std::out_of_range("A word read must fit inside the 54-byte object.");
        }
        const auto* raw = reinterpret_cast<const unsigned char*>(this);
        return static_cast<std::uint16_t>(raw[offset] |
            (static_cast<std::uint16_t>(raw[offset + 1]) << 8));
    }

    std::int16_t RawGameObject::ReadSignedWord(std::size_t offset) const
    {
        const auto word = ReadWord(offset);
        const std::int32_t value = word >= 0x8000 ? static_cast<std::int32_t>(word) - 0x10000 : word;
        return static_cast<std::int16_t>(value);
    }

    void RawGameObject::WriteWord(std::size_t offset, std::uint16_t value)
    {
        if (offset >= sizeof(RawGameObject) - 1)
        {
            throw std::out_of_range("A word write must fit inside the 54-byte object.");
        }
        auto* raw = reinterpret_cast<unsigned char*>(this);
        raw[offset] = static_cast<std::uint8_t>(value & 0xff);
        raw[offset + 1] = static_cast<std::uint8_t>(value >> 8);
    }

    const std::vector<BehaviorDescriptor>& BehaviorCatalog()
    {
        static const std::vector<BehaviorDescriptor> catalog{
#define BEHAVIOR_CASE(name, entry) { BehaviorType::name, #name, entry, true },
#define BEHAVIOR_COMMON(name) { BehaviorType::name, #name, 0x5110, false },
#include "BehaviorCases.inc"
#undef BEHAVIOR_COMMON
#undef BEHAVIOR_CASE
        };
        return catalog;
    }

    const BehaviorDescriptor* FindBehavior(BehaviorType type)
    {
        for (const auto& descriptor : BehaviorCatalog())
        {
            if (descriptor.type == type)
            {
                return &descriptor;
            }
        }
        return nullptr;
    }

    const BehaviorDescriptor* FindBehavior(std::string_view name)
    {
        for (const auto& descriptor : BehaviorCatalog())
        {
            if (name == descriptor.name)
            {
                return &descriptor;
            }
        }
        return nullptr;
    }

    DispatchResult DispatchObjectCase(RawGameObject& object, GameState& context)
    {
        if (context.tickCount < 0)
        {
            throw std::invalid_argument("The scaffold requires a nonnegative tick count.");
        }
        if (object.health <= 0)
        {
            return { DispatchStatus::InactiveObject, object.behavior, 0x5511 };
        }
        if (!context.behaviorDispatchEnabled)
        {
            return { DispatchStatus::DispatchDisabled, object.behavior, 0x5110 };
        }
        switch (object.behavior)
        {
#define BEHAVIOR_CASE(name, entry) case BehaviorType::name: return Function_##name(object, context);
#define BEHAVIOR_COMMON(name) case BehaviorType::name: return { DispatchStatus::NoSpecificCase, BehaviorType::name, 0x5110 };
#include "BehaviorCases.inc"
#undef BEHAVIOR_COMMON
#undef BEHAVIOR_CASE
        default:
            return { DispatchStatus::NoSpecificCase, object.behavior, 0x5110 };
        }
    }
}
