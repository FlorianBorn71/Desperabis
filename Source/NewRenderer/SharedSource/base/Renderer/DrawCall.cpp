#include "pch.h"
#include <Renderer/DrawCall.h>
#include <Asset/Texture.h>

bool DrawCall::IsBatchableWith(const DrawCall& other) const
{
    if (m_material.m_useAlphaTest != other.m_material.m_useAlphaTest
        || m_material.m_applyLighting != other.m_material.m_applyLighting
        //|| m_material.m_textureRepeat != other.m_material.m_textureRepeat
        || m_material.m_reflectionType != other.m_material.m_reflectionType
        || m_material.m_transparencyType != other.m_material.m_transparencyType)
        return false;

    if (m_material.m_texture != nullptr && other.m_material.m_texture != nullptr)
    {
        if (m_material.m_texture->GetAssetId() != other.m_material.m_texture->GetAssetId())
            return false;
    }
    else if (m_material.m_texture != nullptr || other.m_material.m_texture != nullptr)
    {
        return false;
    }
    else if (m_material.m_textureSlot != other.m_material.m_textureSlot)
    {
        return false;
    }

    return true;
}

uint64_t DrawCall::ComputeBatchingKey() const
{
    uint64_t one = 1;
    uint64_t batchingKey = (uint64_t)m_material.m_transparencyType;
    batchingKey ^= (uint64_t)m_material.m_reflectionType << 5;
    if (m_material.m_useAlphaTest) batchingKey |= one << 61;
    if (m_material.m_applyLighting) batchingKey |= one << 60;
//    if (m_material.m_textureRepeat) batchingKey |= one << 59;
    if (m_material.m_texture != nullptr)
    {
        batchingKey ^= (((uint64_t)m_material.m_texture->GetAssetId()) << 48);
    }
    else
    {
        uint64_t slot = (uint64_t)(m_material.m_textureSlot + 1);
        uint64_t slotMask = slot << 28;
        batchingKey |= slotMask;
    }

    return batchingKey;

}

void DrawCallMaterial::ComputeHash()
{
    m_materialHash = *((uint32_t*)&m_opacity) ^ 0x55555555;
    m_materialHash ^= m_applyLighting ? 17 : 0;
}

void DrawCallMaterial::SetOpacity(float opacity)
{
    if (m_opacity != opacity)
    {
        m_opacity = opacity;
        ComputeHash();
    }
}

void DrawCallMaterial::GetWaterTintColor(WaterReflectionType refl, Color4& outCol)
{
    // ! The alpha value is important here, it defines the amount of contribution of the color itself
    refl &= WaterReflectionType::ColorMask;
    switch (refl)
    {
    case WaterReflectionType::Water:outCol = { 200, 180, 140, 10 }; return;
    case WaterReflectionType::Lava:outCol = { 220, 130, 30, 70 }; return;
    case WaterReflectionType::Oil:outCol = { 30, 50, 80, 60 }; return;
    case WaterReflectionType::Identity:outCol = { 255, 255, 255, 0 }; return;
    case WaterReflectionType::WhiteFog:outCol = { 220, 240, 255, 60 }; return;
    case WaterReflectionType::RedLava:outCol = { 255, 80, 40, 128 }; return;
    case WaterReflectionType::DarkWater:outCol = { 200, 180, 140, 0 }; return;
    }
    assert(false);
    outCol = { 255,255,255,255 };
}
