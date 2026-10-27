#include "GameContext.h"

#include <cstring>
#include <stdexcept>

namespace Desperabis
{
    void Level::LoadGeometry(const std::uint8_t* data, std::size_t size)
    {
        if (!data || size < 4)
        {
            throw std::invalid_argument("Geometry input requires a FELD header.");
        }
        const auto widthMinusOne = data[0] | (static_cast<unsigned int>(data[1]) << 8);
        const auto heightMinusOne = data[2] | (static_cast<unsigned int>(data[3]) << 8);
        const std::size_t width = widthMinusOne + 1;
        const std::size_t height = heightMinusOne + 1;
        if (width > Width || height > Height || size < 4 + 7 * width * height)
        {
            throw std::invalid_argument("Geometry input requires seven complete planes fitting the native 50x40 arrays.");
        }
        // Native 1E48:1D47 clears full planes, then reads each file row into stride-50 arrays.
        std::memset(wallTextures, 0, sizeof(wallTextures));
        std::memset(ceilingObjects, 0, sizeof(ceilingObjects));
        std::memset(floorHeights, 255, sizeof(floorHeights));
        std::memset(roomHeights, 50, sizeof(roomHeights));
        std::memset(surfaceFlags, 0, sizeof(surfaceFlags));
        std::memset(collisionMaterials, 0, sizeof(collisionMaterials));
        const auto planeSize = width * height;
        for (std::size_t row = 0; row < height; ++row)
        {
            std::memcpy(wallTextures[row], data + 4 + row * width, width);
            std::memcpy(ceilingObjects[row], data + 4 + 2 * planeSize + row * width, width);
            std::memcpy(floorHeights[row], data + 4 + 3 * planeSize + row * width, width);
            std::memcpy(roomHeights[row], data + 4 + 4 * planeSize + row * width, width);
            std::memcpy(surfaceFlags[row], data + 4 + 5 * planeSize + row * width, width);
            std::memcpy(collisionMaterials[row], data + 4 + 6 * planeSize + row * width, width);
        }
        geometryWidth = width;
        geometryHeight = height;
    }
}
