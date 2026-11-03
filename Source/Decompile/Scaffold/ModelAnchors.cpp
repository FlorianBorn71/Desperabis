#include "GameContext.h"
#include "GameMath.h"

#include <cmath>
#include <stdexcept>

namespace Desperabis
{
    void ModelAnchorGeometry::Load(const std::uint8_t* data, std::size_t size)
    {
        if (!data) { throw std::invalid_argument("Model anchor input must not be null."); }
        std::size_t offset = 0;
        const auto byte = [&]()
        {
            if (offset >= size) { throw std::invalid_argument("Truncated model anchor geometry."); }
            return data[offset++];
        };
        const auto word = [&]() { const auto lo = byte(); return static_cast<std::uint16_t>(lo | (byte() << 8)); };
        const auto skip = [&](std::size_t count)
        {
            if (count > size - offset) { throw std::invalid_argument("Truncated model anchor texture."); }
            offset += count;
        };
        const auto mask = word();
        for (int texture = 0; texture < 16; ++texture)
        {
            if ((mask & (1u << texture)) == 0) { continue; }
            const std::size_t width = 1u + word(), height = 1u + word();
            if (width > 1024 || height > 1024) { throw std::invalid_argument("Invalid model anchor texture dimensions."); }
            skip(width * height);
        }
        const auto rows = byte(), columns = byte();
        if (rows == 0 || rows > 6 || columns == 0 || columns > 5)
        {
            throw std::invalid_argument("Model anchors require 1..6 animation rows and 1..5 frames.");
        }
        std::vector<std::vector<Frame>> loaded(rows, std::vector<Frame>(columns));
        for (auto& row : loaded)
        {
            for (auto& frame : row)
            {
                const auto vertexCount = word(), polygonCount = word();
                frame.linkedObjectVertices.resize(polygonCount);
                if (vertexCount > 255) { throw std::invalid_argument("Model anchor vertex count exceeds byte indices."); }
                for (int vertex = 0; vertex < vertexCount; ++vertex)
                {
                    std::array<std::int16_t, 3> position{};
                    for (auto& axis : position) { axis = WrapSignedWord(word()); }
                    frame.vertices.push_back(position);
                }
                for (int polygon = 0; polygon < polygonCount; ++polygon)
                {
                    std::array<std::uint8_t, 6> primitive{};
                    for (auto& field : primitive) { field = byte(); }
                    const auto type = primitive[0] & 15;
                    if (type != 9 && !(type == 1 && primitive[1] == 254)) { continue; }
                    if (primitive[2] == 0 || primitive[2] > vertexCount)
                    {
                        throw std::invalid_argument("Model anchor references a missing vertex.");
                    }
                    if (type == 9)
                    {
                        frame.linkedObjectVertices[polygon] = primitive[2];
                        if (!frame.billboardVertex) { frame.billboardVertex = primitive[2]; }
                    }
                    else
                    {
                        if (frame.muzzleVertex) { throw std::invalid_argument("Multiple muzzle anchors are unsupported."); }
                        frame.muzzleVertex = primitive[2];
                    }
                }
            }
        }
        if (offset != size) { throw std::invalid_argument("Unexpected trailing model anchor data."); }
        animations = std::move(loaded);
    }

    std::optional<std::array<std::int16_t, 3>> ModelAnchorGeometry::Sample(
        const RawGameObject& object, bool muzzle, std::uint16_t linkedPolygon) const
    {
        if (object.modelAnim == 0 || object.modelAnim > animations.size() || object.animFrame == 0 ||
            object.animFrame > animations[object.modelAnim - 1].size())
        {
            throw std::out_of_range("Model anchor animation/frame is unavailable.");
        }
        const auto& row = animations[object.modelAnim - 1];
        const auto& frame = row[object.animFrame - 1];
        const auto anchor = linkedPolygon != 0 ?
            std::optional<std::uint8_t>(frame.linkedObjectVertices.at(linkedPolygon - 1)) :
            (muzzle ? frame.muzzleVertex : frame.billboardVertex);
        if (anchor && *anchor == 0) { throw std::invalid_argument("Linked object requires a type-9 model polygon."); }
        if (!anchor) { return std::nullopt; }
        std::array<double, 3> position{};
        double blend = object.blendWeightOrSpriteOpacity * 0.01;
        if (object.blendWeightOrSpriteOpacity > 100) { blend = 2 - blend; }
        if (object.blendWeightOrSpriteOpacity > 200) { blend = 0; }
        for (std::size_t axis = 0; axis < position.size(); ++axis)
        {
            position[axis] = frame.vertices.at(*anchor - 1)[axis];
            if (object.modelFrame2 > 0 && object.modelFrame2 <= row.size())
            {
                position[axis] += (row[object.modelFrame2 - 1].vertices.at(*anchor - 1)[axis] - position[axis]) * blend;
            }
        }
        // Same coordinate conversion and X/Z/Y rotation order as the model renderer.
        double x = -position[0], y = -position[1], z = position[2];
        const double pitch = -object.winX * (3.14159265358979323846 / 256);
        const double roll = -object.winZ * (3.14159265358979323846 / 256);
        const double yaw = (object.winY + 256) * (3.14159265358979323846 / 256);
        const auto py = y * std::cos(pitch) - z * std::sin(pitch);
        z = y * std::sin(pitch) + z * std::cos(pitch);
        y = py;
        const auto rx = x * std::cos(roll) - y * std::sin(roll);
        y = x * std::sin(roll) + y * std::cos(roll);
        x = rx;
        const auto yx = x * std::cos(yaw) + z * std::sin(yaw);
        z = -x * std::sin(yaw) + z * std::cos(yaw);
        x = yx;
        return std::array<std::int16_t, 3>{
            WrapSignedWord(object.posX + static_cast<int>(std::lround(x / 10))),
            WrapSignedWord(object.posY + static_cast<int>(std::lround(y / 10))),
            WrapSignedWord(object.posZ - static_cast<int>(std::lround(z / 10))) };
    }

    std::optional<std::array<std::int16_t, 3>> GameContext::GetModelAnchor(
        const RawGameObject& object, bool muzzle) const
    {
        const int slot = object.modelId - 30;
        if (!level || slot < 0 || slot >= static_cast<int>(level->modelAnchors.size()) || !level->modelAnchors[slot])
        {
            throw std::runtime_error("Model attachment physics requires loaded anchor geometry.");
        }
        return level->modelAnchors[slot]->Sample(object, muzzle);
    }

    RawGameObject GameContext::GetObjectRenderState(std::size_t slot) const
    {
        auto object = worldObjects.at(slot);
        if (object.health <= 0 || object.renderPlacementMode != 20 ||
            WrapSignedWord(object.generalPurpose3) <= 0 || object.doorLockOrGroupBitmask <= 0)
        {
            return object;
        }
        const auto& parent = worldObjects.at(object.doorLockOrGroupBitmask);
        if (parent.health <= 0) { object.health = 0; return object; }
        if (parent.modelId <= 30) { return object; }
        const auto modelSlot = static_cast<std::size_t>(parent.modelId - 30);
        if (!level || modelSlot >= level->modelAnchors.size() || !level->modelAnchors[modelSlot])
        {
            throw std::runtime_error("Linked object rendering requires loaded parent geometry.");
        }
        const auto anchor = level->modelAnchors[modelSlot]->Sample(parent, false, object.generalPurpose3);
        if (!anchor) { throw std::logic_error("Linked object polygon has no model anchor."); }
        object.posX = (*anchor)[0];
        object.posY = (*anchor)[1];
        object.posZ = (*anchor)[2];
        if (static_cast<std::uint8_t>(object.displayType) > 3)
        {
            object.winY = WrapSignedWord(parent.winY + object.velocityY);
        }
        return object;
    }

    void GameContext::UpdateModelAttachments()
    {
        for (auto& head : worldObjects)
        {
            if (head.health <= 0 || head.behavior != BehaviorType::DragonHead) { continue; }
            const auto slot = head.doorLockOrGroupBitmask;
            if (slot < 1 || slot >= static_cast<int>(worldObjects.size()))
            {
                throw std::out_of_range("Dragon head requires a parent slot in 1..165.");
            }
            const auto& parent = worldObjects[slot];
            if (parent.health <= 0 || (parent.behavior != BehaviorType::Dragon && parent.behavior != BehaviorType::DragonDying))
            {
                SetObjectHealth(head, 0);
                continue;
            }
            const auto anchor = GetModelAnchor(parent, false);
            if (!anchor) { SetObjectHealth(head, 0); continue; }
            head.posX = (*anchor)[0]; head.posY = (*anchor)[1]; head.posZ = (*anchor)[2];
            head.modelAnim = static_cast<std::uint8_t>(((-GetPlayerHeading() - parent.winY + 43) & 511) / 86 + 1);
            head.animFrame = parent.spriteScale;
        }
    }
}
