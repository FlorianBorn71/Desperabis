#include "pch.h"
#include <Asset/AssetManager.h>
#include <Asset/Level.h>
#include <ppltasks.h>

AssetManager AssetManager::g_Instance;

AssetManager& AssetManager::Instance()
{
    return g_Instance;
}

FUTURE(void) AssetManager::Initialize()
{
	m_defaultPalette = CO_AWAIT LoadAsset<Palette>(L"GRAFIK\\BACKGR1.PAL");

    m_plainWhiteTex.CreatePlainWhite();
    m_pointTex.CreatePointTexture();
    m_coronaTex.CreateCoronaTexture();

    CO_AWAIT CreateDefaultShapes();
                    
    m_isReady = true;
}


void AssetManager::Deinitialize()
{
    m_loadedAssets.clear();
    m_plainWhiteTex.Reset();
    m_pointTex.Reset();
    m_coronaTex.Reset();
}


void AssetManager::RegisterAsset(AssetBase& asset)
{
    std::lock_guard _(m_loadingMutex);
    asset.m_assetId = CreateNewAssetId();
}

/*
FUTURE(AssetBase*) AssetManager::LoadAssetImpl(AssetBase& asset, std::wstring_view filename)
{
    if (!asset.m_loaded)
    {
        std::wstring path = FileManager::Instance().FilenameToNative(filename);
        std::vector<byte> fileData = CO_AWAIT FileManager::Instance().ReadDataFuture(path);
        AssetStream stream(fileData);
        asset.Read(stream);
        asset.CreateRuntimes(Renderer::Instance());
        asset.m_loaded = true;
    }
    CO_RETURN &asset;
 }
 */

const Palette& AssetManager::GetDefaultPalette() const
{
    assert(m_defaultPalette!=nullptr && m_defaultPalette->IsLoaded());
    return *m_defaultPalette.get();
}

FUTURE(void) AssetManager::LoadLevelInfo()
{
    std::wstring path = FileManager::Instance().FilenameToNative(L"LEVEL\\LEVELS.INF");
    std::vector<byte> fileData = CO_AWAIT FileManager::Instance().ReadDataFuture(path);
    const int structSize = sizeof(LevelInfo::LevelInfoRaw);
    static_assert(structSize == 124);
    int count = (int)fileData.size() / structSize;
    const byte* rawData = fileData.data();
    m_levelInfo.resize(count);
    for (int i = 0; i < count; i++, rawData += structSize)
    {
        LevelInfo& info = m_levelInfo[i];
        info.Parse(i, rawData);
    }

#ifdef _DEBUG
    std::wstringstream wss;
    std::wstring sep = L"\t";
    std::wstring nl = L"\n";
    for (int line = -1; line < structSize; line++)
    {
        for (int i = 0; i < count; i++)
        {
            LevelInfo& info = m_levelInfo[i];
            info.LogLine(line, wss);
            wss << sep;
        }
        wss << nl;
    }

    // Find the smallest amount of bytes to make level raw info distinguishable:
    // turns out, it's 4!
    if (false)
    {
        for (int numB = sizeof(LevelInfo::LevelInfoRaw::m_rawData); numB > 1; numB--)
        {
            for (int i = 0; i < count; i++)
            {
                for (int j = i + 1; j < count; j++)
                {
                    if (!LevelInfo::IsUsedLevelIndex(i) || !LevelInfo::IsUsedLevelIndex(j))
                        continue;

                    if (memcmp(m_levelInfo[i].m_raw.m_rawData, m_levelInfo[j].m_raw.m_rawData, numB) == 0)
                    {
                        wss << "\n Level " << i << "is the same as " << j; // does not hit here!
                    }
                }
            }
        }
    }

	std::wstring str = wss.str();

	std::wstringstream wss1;
    {
        // byte sequence for searching in DOSbox
        const auto& museum = m_levelInfo[98].m_raw.m_rawData;
        for (int i = 0; i < sizeof(museum); i++)
        {
            wss1 << museum[i] << ",";
        }
    }
    // Level 98 raw data:
	// 4,2,8,4,8,4,2,2,18,1,20,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,1,7,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,2,14,1,27,6,28,7,30,5,25,8,1,4,7,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,196,1,0,0,0,0,100,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	str = wss1.str();
#endif
}

FUTURE(void) AssetManager::CreateDefaultShapes()
{
    m_spriteRenderable[0] = GeometryUtils::CreateSpriteMesh(false);
    m_spriteRenderable[1] = GeometryUtils::CreateSpriteMesh(true);

    m_cubeRenderable = GeometryUtils::CreateCubeMesh(0.5f, 0.5f, 0.5f);

    m_burningObjectMesh = GeometryUtils::CreateBurningObjectMesh(1024);
    
    m_weaponAnim = CO_AWAIT LoadAsset<TextureAnim>(L"ANIMS\\ANIM4.AN2");
    TextureAnimFrame* burning0 = m_weaponAnim->GetFrame(0, 0);
	TextureAnimFrame* burning1 = m_weaponAnim->GetFrame(0, 1);
}

int AssetManager::GetLevelIndexByHash(uint64_t uniqueHash) const
{
    for (int i = 0; i < m_levelInfo.size(); i++)
    {
        if (LevelInfo::IsUsedLevelIndex(i) && m_levelInfo[i].m_uniqueLevelHash == uniqueHash)
        {
            return i;
        }
    }
    return -1;
}


FUTURE(Level*) AssetManager::LoadLevel(int index)
{
    if (m_levelInfo.size() == 0)
    {
        CO_AWAIT LoadLevelInfo();
    }
    std::shared_ptr<Level> level;
    std::wstringstream wss;
    wss << L"Level" << index;
    auto iter = m_loadedAssets.find(wss.str());
    if (iter == m_loadedAssets.end())
    {
        level = std::make_shared<Level>();
        m_loadedAssets.insert(std::make_pair(wss.str(), level));
        level->m_assetId = CreateNewAssetId();
        level->SetDebugName(wss.str());
        CO_AWAIT level->Load(index, m_levelInfo[index]);
    }
    else
    {
        level = std::static_pointer_cast<Level>(iter->second);
    }
    CO_RETURN level.get();
}

FUTURE(Level*) AssetManager::LoadSaveGame(int index)
{
    if (m_levelInfo.size() == 0)
    {
        CO_AWAIT LoadLevelInfo();
    }
    std::shared_ptr<Level> level;
    std::wstringstream wss;
    wss << L"SaveGames\\STAND" << index << L".SPL";
    auto iter = m_loadedAssets.find(wss.str());
    if (iter == m_loadedAssets.end())
    {
        level = std::make_shared<Level>();
        m_loadedAssets.insert(std::make_pair(wss.str(), level));
        level->m_assetId = CreateNewAssetId();
        level->SetDebugName(wss.str());
        CO_AWAIT level->LoadFromSaveGame(wss.str(), m_levelInfo);
    }
    else
    {
        level = std::static_pointer_cast<Level>(iter->second);
    }
    CO_RETURN level.get();
}

