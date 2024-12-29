#include "pch.h"
#include <Asset/LevelInfo.h>
#include <Asset/AssetManager.h>
#include <Asset/ModelAnim.h>
#include <Asset/Texture.h>
#include <Renderer/RenderableInstance.h>


bool LevelInfo::IsUsedLevelIndex(int level)
{
    const int SupportedLevels[] = {
        10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26, 29,30,31,32,33,36,34,35, 50,51, 60, 90,91,92,95,96,97,98,99

    };

    for (int i = 0; i < sizeof(SupportedLevels) / sizeof(SupportedLevels[0]); i++)
    {
        if (level == SupportedLevels[i])
            return true;
    }
    return false;
}

void LevelInfo::Parse(int levelIndex, const byte* rawData)
{
    m_levelIndex = levelIndex;
    memcpy(&m_raw, rawData, sizeof(LevelInfoRaw));
    memcpy(&m_uniqueLevelHash, rawData, sizeof(m_uniqueLevelHash)); // turns out even the first 4 bytes are significant
    for (int i = 0; i < 15; i++)
    {
        { // textures
            const AssetSlot& slot(m_raw.m_data.textureSlots[i]);
            if (slot.fileIndex > 0 && i < m_raw.m_data.numTextures)
            {
                assert(slot.targetSlot >= 0 && slot.targetSlot < NumSlots);
                m_defaultTextureSlots[slot.targetSlot] = slot.fileIndex;
            }
        }
        { // texanims
            const AssetSlot& slot(m_raw.m_data.textureAnimSlots[i]);
            if (slot.fileIndex > 0 && i < m_raw.m_data.numTextureAnims)
            {
                assert(slot.targetSlot >= 0 && slot.targetSlot < NumSlots);
                m_loadedTexAnimSlots[slot.targetSlot] = slot.fileIndex;
            }
        }
        { // vector anims
            const AssetSlot& slot(m_raw.m_data.vectorSlots[i]);
            if (slot.fileIndex > 0 && i < m_raw.m_data.numVectAnims)
            {
                assert(slot.targetSlot >= 0 && slot.targetSlot < NumSlots);
                m_loadedModelSlots[slot.targetSlot] = slot.fileIndex;
            }
        }
    }
}



FUTURE(void) LevelInfo::PreloadData()
{
    AssetManager& assets = AssetManager::Instance();

    for (int slot = 0; slot < NumSlots; slot++)
    {
        if (m_loadedModelSlots[slot] > 0)
        {
            std::wstringstream wss;
            wss << L"VEKT\\VEKT" << m_loadedModelSlots[slot] << ".3DA";
            m_loadedModel[slot] = CO_AWAIT assets.LoadAsset<ModelAnim>(wss.str());
        }
        if (m_loadedTexAnimSlots[slot] > 0)
        {
            std::wstringstream wss;
            wss << L"ANIMS\\ANIM" << m_loadedTexAnimSlots[slot] << ".AN2";
            m_loadedTextureAnim[slot] = CO_AWAIT assets.LoadAsset<TextureAnim>(wss.str());
        }
        if (m_defaultTextureSlots[slot] > 0)
        {
            std::wstringstream wss;
            wss << L"GRAFIK\\TUR" << m_defaultTextureSlots[slot] << ".256";
            m_loadedTexture[slot] = CO_AWAIT assets.LoadAsset<Texture>(wss.str());
        }
    }

    if (m_raw.m_data.skyTextureFile > 0)
    {
        std::wstringstream wss;
        wss << L"GRAFIK\\TUR" << m_raw.m_data.skyTextureFile << ".256";
        m_skyTexture = CO_AWAIT assets.LoadAsset<Texture>(wss.str());
    }
	if (m_raw.m_data.skylineTextureFile > 0)
	{
		std::wstringstream wss;
		wss << L"GRAFIK\\TUR" << m_raw.m_data.skylineTextureFile << ".256";
        m_skylineTexture = CO_AWAIT assets.LoadAsset<Texture>(wss.str());
	}    
}

std::shared_ptr<ModelAnim> LevelInfo::GetModelAnim(int id) const
{
    if (id >= 0 && id < NumSlots)
    {
        return m_loadedModel[id];
    }

    return nullptr;
}

std::shared_ptr<Texture> LevelInfo::GetLevelTexture(int slot) const
{
    if (slot >= 0 && slot < NumSlots)
    {
        return m_loadedTexture[slot];
    }

    return nullptr;

}

std::shared_ptr<TextureAnim> LevelInfo::GetTextureAnim(int slot) const
{
    if (slot >= 0 && slot < NumSlots)
    {
        return m_loadedTextureAnim[slot];
    }

    return nullptr;

}

// Assign per instance overrides for the wall textures. These are level specific, not asset specific, so use material override feature
void LevelInfo::AssignLevelTextures(RenderableInstance& inst) const
{
    if (inst.GetRenderable() == nullptr)
    {
        inst.m_materialOverride.clear();
        return;
    }
    const int numDC = (int)inst.GetRenderable()->m_drawCalls.size();

    bool needsOverride = false;
    for (const DrawCall& dc : inst.GetRenderable()->m_drawCalls)
    {
        if (dc.m_material.m_texture == nullptr)
        {
            needsOverride = true;
            break;
        }
    }
    if (needsOverride)
    {
        if ((int)inst.m_materialOverride.size() < numDC)
        {
            inst.m_materialOverride.resize(numDC);
        }

        for (int i = 0; i < numDC; i++)
        {
            const DrawCall& dc = inst.GetRenderable()->m_drawCalls[i];
            inst.m_materialOverride[i] = dc.m_material;
            if (dc.m_material.m_texture != nullptr)
                continue;

            inst.m_materialOverride[i].m_texture = GetLevelTexture(dc.m_material.m_textureSlot).get();
            if (inst.m_materialOverride[i].m_texture == nullptr)
            {
                continue;
            }
            inst.m_materialOverride[i].ComputeHash();
        }
    }

    // recurse on sub-instances:
    for (auto& subInst : inst.m_subInstances)
    {
        AssignLevelTextures(subInst);
    }
}


#ifdef _DEBUG
bool LevelInfo::LogLine(int index, std::wstringstream& wss) const
{
    if (index < 0)
    {
        wss << L"Level " << m_levelIndex;
        return true;
    }
    if (index < sizeof(LevelInfoRaw))
    {
        wss << (int)m_raw.m_rawData[index];
        return true;
    }
    return false;
}
#endif