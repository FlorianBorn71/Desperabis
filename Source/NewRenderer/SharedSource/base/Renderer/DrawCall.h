#pragma once
#include <DesperabisCommon.h>

enum class TransparencyMode : uint8_t
{
    Opaque = 0,
    AlphaBlended = 1,
    Additive = 2,

    SkyOpaque = 3,
    SkyAlphaBlended = 4,
    SkyAdditive = 5,

    Water = 6,

    TransparencyModeCount
};

// do not change values, they come from level info
enum class WaterReflectionType : byte
{
    // materials:
    Water = 0,
    Lava = 1,
    Oil = 2,
    Identity = 3,
    WhiteFog = 4,
    RedLava = 5,
    DarkWater = 10,

    // reflection/refraction
    ColorTintBit = 0,  // only transparent
    ColorMask = 15,
    RefractionBit = 16, // applied with 4-block distortion
    ReflectionBit = 32, // screenspace refl. with 4-block distortion

    Disabled = 255 // to hide the tile
};


DECLARE_ENUM_FLAGS(WaterReflectionType);


enum class DepthWriteMode : uint8_t
{
    None = 0,
    DepthWriteBit = 1,
    DepthTestBit = 2,

    DepthModeCount = DepthTestBit << 1
};
DECLARE_ENUM_FLAGS(DepthWriteMode);

enum class TransparencyModeMask : uint16_t
{
    None = 0,
    OpaqueBit = 1 << (int)TransparencyMode::Opaque,
    AlphaBlendedBit = 1 << (int)TransparencyMode::AlphaBlended,
    AdditiveBit = 1 << (int)TransparencyMode::Additive,

    SkyOpaqueBit = 1 << (int)TransparencyMode::SkyOpaque,
    SkyAlphaBlendedBit = 1 << (int)TransparencyMode::SkyAlphaBlended,
    SkyAdditiveBit = 1 << (int)TransparencyMode::SkyAdditive,

    WaterBit = 1 << (int)TransparencyMode::Water,

    All = OpaqueBit | AlphaBlendedBit | AdditiveBit | WaterBit,
    AllTransparent = AlphaBlendedBit | AdditiveBit,
    AllSky = SkyOpaqueBit | SkyAlphaBlendedBit | SkyAdditiveBit,
    AllWaterReceiver = WaterBit,
};
DECLARE_ENUM_FLAGS(TransparencyModeMask);


struct DrawCallMaterial
{
    void ComputeHash();
    void SetOpacity(float opacity);
    static void GetWaterTintColor(WaterReflectionType refl, Color4& outCol);

    TransparencyMode m_transparencyType = TransparencyMode::Opaque;
    WaterReflectionType m_reflectionType = WaterReflectionType::Disabled;
    bool m_useAlphaTest = true;
    bool m_applyLighting = true;
    //bool m_textureRepeat = true;
    char m_textureSlot = -1;
    float m_opacity = 1.0f;
    float m_mirrorHeight = 0.f; // for mirror objects
    Texture* m_texture = nullptr;

    uint32_t m_materialHash = 0;
};

struct DrawCall
{
    int m_firstIndex = 0, m_primitiveCount = 0;
    DrawCallMaterial m_material;

    bool IsBatchableWith(const DrawCall& other) const;
    uint64_t ComputeBatchingKey() const;
};

