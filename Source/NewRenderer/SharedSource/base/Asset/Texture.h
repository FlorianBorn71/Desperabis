#pragma once
#include <DesperabisCommon.h>
#include <Asset/AssetBase.h>
#include <Asset/ModelGeometry.h>
#include <Renderer/GlobalSettings.h>

struct TextureLoadingContext
{
    std::wstring_view m_highResFilename;

    // after conversion
    std::vector<Color4> m_rgbaData;
    int m_atlasOfsX = 0;
    int m_atlasOfsY = 0;
};


class D_IMPEXP Texture : public AssetBase
{
public:
    Texture() = default;
    virtual ~Texture() = default;

    void Read(AssetStream& stream, TextureLoadingContext& context);
    virtual void Read(AssetStream& stream) override;
    virtual void CreateRuntimes(Renderer& renderer) override;
    virtual void Reset() override;

    void SetDefaultParams();
    void CreateAsAtlas(Texture* firstTexture, TextureLoadingContext* firstContext, int numTextures, int texStructStride = sizeof(Texture));
    void CreatePlainWhite();
    void CreatePointTexture();
    void CreateCoronaTexture();

    template<typename T>
    inline T* GetNativeRuntimeObj() const 
    { 
        if (GlobalSettings::Instance().assetQuality == GlobalSettings::AssetQuality::Original)
			return static_cast<T*>(m_runtime.get());
		if (GlobalSettings::Instance().assetQuality == GlobalSettings::AssetQuality::OnlyHighRes || m_runtimeHighres != nullptr)
			return static_cast<T*>(m_runtimeHighres.get());
		return static_cast<T*>(m_runtime.get());
    }
public:
    bool m_hasAlpha = false;
    uint16_t m_width = 0;
    uint16_t m_height = 0;
    SpriteParams m_spriteData = {};
    TextureTransform m_atlasData = {};
	std::shared_ptr<IGraphicsRuntimeObject> m_runtime;
	std::shared_ptr<IGraphicsRuntimeObject> m_runtimeHighres;
};

