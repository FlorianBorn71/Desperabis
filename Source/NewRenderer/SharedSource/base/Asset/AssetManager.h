#pragma once

#include <DesperabisCommon.h>
#include <Asset/Palette.h>
#include <Asset/Texture.h>
#include <Asset/TextureAnim.h>
#include <Asset/ModelAnim.h>
#include <Asset/LevelInfo.h>
#include <IO/FileManager.h>
#include <Renderer/Renderer.h>

class D_IMPEXP AssetManager
{
public:
    AssetManager() = default;
    ~AssetManager() = default;

    static AssetManager& Instance();

    FUTURE(void) Initialize();
    void Deinitialize();

    bool IsReady() const {return m_isReady;}

    template<typename ASSET_TYPE>
    inline FUTURE(std::shared_ptr<ASSET_TYPE>) LoadAsset(std::wstring_view filename);

    template<typename ASSET_TYPE>
    std::shared_ptr<ASSET_TYPE> GetAsset(std::wstring_view filename);

    FUTURE(Level*) LoadLevel(int index);
    FUTURE(Level*) LoadSaveGame(int index);
    int GetLevelIndexByHash(uint64_t uniqueHash) const;

    const Palette& GetDefaultPalette() const;
    Texture* GetPlainWhiteTexture() { return &m_plainWhiteTex; }
    Texture* GetPointTexture() { return &m_pointTex; }
    Texture* GetCoronaTexture() { return &m_coronaTex; }
	Texture* GetFlameTexture(int frame) { return m_weaponAnim->GetFrame(0, frame);}
    int GetNumFlameFrames() const { return 2; }
    std::shared_ptr<Renderable> GetSpriteRenderable(bool flipped) { return m_spriteRenderable[flipped ? 1 : 0]; }
    std::shared_ptr<Renderable> GetCubeRenderable() { return m_cubeRenderable; }
    std::shared_ptr<Renderable> GetBurningObjectRenderable() { return m_burningObjectMesh; }
    uint32_t CreateNewAssetId() { return m_assetIdCounter++; }
    void RegisterAsset(AssetBase& asset);

private:
    static AssetManager g_Instance;

    template<typename ASSET_TYPE>
    FUTURE(std::shared_ptr<ASSET_TYPE>) LoadAssetImpl(std::shared_ptr<ASSET_TYPE> asset, std::wstring_view filename);
    FUTURE(void) LoadLevelInfo();
    FUTURE(void) CreateDefaultShapes();

    std::mutex m_loadingMutex;
    std::shared_ptr<Palette> m_defaultPalette;
    Texture m_plainWhiteTex;
    Texture m_pointTex;
    Texture m_coronaTex;
    std::shared_ptr<Renderable> m_spriteRenderable[2]; // flipped or not
    std::shared_ptr<Renderable> m_cubeRenderable;
	std::shared_ptr<Renderable> m_burningObjectMesh; // a list of quads, each referencing a position index
    std::shared_ptr<TextureAnim> m_weaponAnim;

    std::map<std::wstring, AssetPtr> m_loadedAssets;
    uint32_t m_assetIdCounter = 1;

    // level
    std::vector<LevelInfo> m_levelInfo;

    bool m_isReady = false;
};


template<typename ASSET_TYPE>
inline FUTURE(std::shared_ptr<ASSET_TYPE>) AssetManager::LoadAssetImpl(std::shared_ptr<ASSET_TYPE> asset, std::wstring_view filename)
{
    assert(asset.get() != nullptr);
    if (!asset->m_loaded)
    {
        std::wstring path = FileManager::Instance().FilenameToNative(filename);
        std::vector<byte> fileData = CO_AWAIT FileManager::Instance().ReadDataFuture(path);
        AssetStream stream(fileData);
		asset->Read(stream);
        asset->CreateRuntimes(Renderer::Instance());
        asset->m_loaded = true;
    }
    CO_RETURN asset;
}


template<typename ASSET_TYPE>
inline FUTURE(std::shared_ptr<ASSET_TYPE>) AssetManager::LoadAsset(std::wstring_view filename)
{
    std::shared_ptr<ASSET_TYPE> ptr;
    {
        std::lock_guard _(m_loadingMutex);
        AssetPtr foundPtr;
        auto iter = m_loadedAssets.find(std::wstring(filename));
        if (iter == m_loadedAssets.end())
        {
            ptr = std::make_shared<ASSET_TYPE>();
            ptr->SetDebugName(filename);
            m_loadedAssets.insert(std::make_pair(filename, ptr));
            ptr->m_assetId = CreateNewAssetId();
        }
        else
        {
            ptr = std::static_pointer_cast<ASSET_TYPE>(iter->second);
        }
    }

    return LoadAssetImpl<ASSET_TYPE>(ptr, filename);
}

template<typename ASSET_TYPE>
std::shared_ptr<ASSET_TYPE> AssetManager::GetAsset(std::wstring_view filename)
{
    std::lock_guard _(m_loadingMutex);

    AssetPtr ptr = m_loadedAssets.at(std::wstring(filename));
    
    return std::static_pointer_cast<ASSET_TYPE>(ptr);
}
