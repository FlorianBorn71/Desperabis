#include "pch.h"
#include <Asset/Texture.h>
#include <Renderer/Renderer.h>
#include <Asset/AssetManager.h>
#include <Asset/Palette.h>

void Texture::Read(AssetStream& stream)
{
    TextureLoadingContext context;
    Read(stream, context);

    if (m_width > 0 && m_height > 0)
    {
#ifdef _DEBUG
        std::wstring highResFile = this->m_debugName;
		highResFile += L".dds";
        if (FileManager::Instance().Exists(highResFile))
        {
            context.m_highResFilename = highResFile;
        }
#endif
        Renderer::Instance().CreateTexture(*this, context);
    }
}

void Texture::Read(AssetStream& stream, TextureLoadingContext& context)
{
    const Palette& palette = AssetManager::Instance().GetDefaultPalette();
    m_width = stream.Read<uint16_t>();
    m_height = stream.Read<uint16_t>();
    m_width++;
    m_height++;
    assert(m_width < 1024 && m_height < 1024);
    int count = (int)m_width * (int)m_height;
    context.m_rgbaData.resize(count);
    for (int i = 0; i < count; i++)
    {
        byte idx = stream.Read<byte>();
        m_hasAlpha |= idx == 0;
        Color4& dest = context.m_rgbaData[i];
        dest = palette.GetEntry(idx);
    }

    SetDefaultParams();
}

void Texture::CreateRuntimes(Renderer& renderer)
{
}

void Texture::Reset()
{
    m_runtime = nullptr;
}

void Texture::SetDefaultParams()
{
    // centered sprite
    m_spriteData.m_bottomRight = { (float)m_width * 0.5f, (float)m_height * 0.5f};
    m_spriteData.m_topLeft = { -m_spriteData.m_bottomRight.x, -m_spriteData.m_bottomRight.y };

    m_atlasData = { 1,1,0,0 };
}

void Texture::CreateAsAtlas(Texture* firstTexture, TextureLoadingContext* firstContext, int numTextures, int texStructStride)
{
    AssetManager::Instance().RegisterAsset(*this); // assign a new Id

    const int border = 1;
    int atlasHeight = 0;
    int atlasWidth = 0;
    char* texbasePtr = (char*)firstTexture;
    for (int i = 0; i < numTextures; i++, texbasePtr += texStructStride)
    {
        Texture& t(*((Texture * )texbasePtr));
        if (t.m_width == 0)
            continue;
        firstContext[i].m_atlasOfsX = atlasWidth + border;
        firstContext[i].m_atlasOfsY = border;
        atlasWidth += t.m_width + border * 2;
        atlasHeight = __max(atlasHeight, t.m_height + border * 2);
    }

    if (atlasHeight == 0 || atlasWidth == 0)
        return;

    m_width = atlasWidth;
    m_height = atlasHeight;
    TextureLoadingContext thisData;
    thisData.m_rgbaData.resize(atlasWidth * atlasHeight);
    Color4* destBase = thisData.m_rgbaData.data();
    memset(destBase, 0, atlasWidth * atlasHeight * sizeof(destBase[0]));
#ifdef _DEBUG
    // fill yellow to recognize UV errors
    for (int i = 0; i < atlasWidth * atlasHeight; i++)
        destBase[i].Set(255, 255, 0, 255);
#endif
    texbasePtr = (char*)firstTexture;
    for (int i = 0; i < numTextures; i++, texbasePtr += texStructStride)
    {
        Texture& t(*((Texture*)texbasePtr));
        if (t.m_width == 0)
            continue;
        TextureLoadingContext& d(firstContext[i]);
        const Color4* srcBase = d.m_rgbaData.data();
        const int widthPlusBorder = t.m_width + 2 * border;
        const int heightPlusBorder = t.m_height + 2 * border;

        // blit each subimage, but duplicate edge pixels
        for (int y = 0; y < heightPlusBorder; y++)
        {
            int clampedY = __min(__max(y - border, 0), t.m_height - 1);
            Color4* dst = destBase + y * atlasWidth + d.m_atlasOfsX - border;
            for (int x = 0; x < widthPlusBorder; x++, dst++)
            {
                int clampedX = __min(__max(x - border, 0), t.m_width - 1);
                const Color4* src = srcBase + clampedY * t.m_width + clampedX;
                *dst = *src;
            }
        }

        t.m_atlasData = {
            (float)t.m_width / (float)atlasWidth,
            (float)t.m_height / (float)atlasHeight,
            (float)d.m_atlasOfsX / (float)atlasWidth,
            (float)d.m_atlasOfsY / (float)atlasHeight,
        };
    }

    SetDefaultParams();
    Renderer::Instance().CreateTexture(*this, thisData);

    // share the native DX resources
    texbasePtr = (char*)firstTexture;
    for (int i = 0; i < numTextures; i++, texbasePtr += texStructStride)
    {
        Texture& t(*((Texture*)texbasePtr));
        Renderer::Instance().CloneTextureRuntime(t, *this); // cannot share the full runtime object since atlas data differs
//        t.m_assetId = this->m_assetId; // also share the asset ID, so it works with batching
        AssetManager::Instance().RegisterAsset(t); // cannot batch sub-textures because they have distinct atlas data
    }
}

void Texture::CreatePlainWhite()
{
    AssetManager::Instance().RegisterAsset(*this);

    m_width = m_height = 1; // pretend 1 pixel texture for the sprite corner generation:
    SetDefaultParams();

    m_width = m_height = 4;
    TextureLoadingContext thisData;
    thisData.m_rgbaData.resize(m_width * m_height);
    Color4* dest = thisData.m_rgbaData.data();
    for (int i = 0; i < (int)thisData.m_rgbaData.size(); i++)
        dest[i] = { 255,255,255,255 };

    Renderer::Instance().CreateTexture(*this, thisData);
}

void Texture::CreatePointTexture()
{
    AssetManager::Instance().RegisterAsset(*this);

    m_width = m_height = 1; // pretend 1 pixel texture for the sprite corner generation:
    SetDefaultParams();

    m_width = m_height = 16;
    TextureLoadingContext thisData;
    thisData.m_rgbaData.resize(m_width * m_height);
    Color4* dest = thisData.m_rgbaData.data();
    float centerX = (float)m_width * 0.5f;
    float centerY = (float)m_height * 0.5f;
    for (int y = 0; y < m_height; y++)
        for (int x = 0; x < m_width; x++, dest++)
        {
            float dx = (float)x - centerX;
            float dy = (float)y - centerY;
            int d = (int)((centerX - sqrtf(dx * dx + dy * dy)) * 128.0f);
            if (d < 0) d = 0; else if (d > 255) d = 255;
            *dest = { 255, 255, 255, (byte)d};
        }

    Renderer::Instance().CreateTexture(*this, thisData);
}

void Texture::CreateCoronaTexture()
{
    AssetManager::Instance().RegisterAsset(*this);

    m_width = m_height = 1; // pretend 1 pixel texture for the sprite corner generation:
    SetDefaultParams();

    m_width = m_height = 128;
    TextureLoadingContext thisData;
    thisData.m_rgbaData.resize(m_width * m_height);
    Color4* dest = thisData.m_rgbaData.data();
    float centerX = (float)m_width * 0.5f;
    float centerY = (float)m_height * 0.5f;
    for (int y = 0; y < m_height; y++)
        for (int x = 0; x < m_width; x++, dest++)
        {
            float dx = (float)x - centerX;
            float dy = (float)y - centerY;
            int d = (int)((centerX - sqrtf(dx * dx + dy * dy)));
            if (d < 0) d = 0; else if (d > 255) d = 255;
            *dest = { (byte)d, (byte)d, (byte)d, 255 };
        }

    Renderer::Instance().CreateTexture(*this, thisData);
}
