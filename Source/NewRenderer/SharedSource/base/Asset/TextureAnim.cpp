#include "pch.h"
#include <Asset/TextureAnim.h>
#include <Renderer/Renderer.h>
#include <Asset/AssetManager.h>
#include <Asset/Palette.h>

void TextureAnim::Read(AssetStream& stream)
{
    m_countX = stream.Read<byte>();
    m_countY = stream.Read<byte>();

    const int frameCount = m_countX * m_countY;
    m_frames.resize(frameCount);
    TextureAnimFrame* frame = m_frames.data();
    TextureLoadingContext tempContexts[30];
    TextureLoadingContext* context = tempContexts;

    for (int x = 0; x < m_countX; x++)
    {
        for (int y = 0; y < m_countY; y++, frame++, context++)
        {
            frame->m_relX = stream.Read<short>();
            frame->m_relY = stream.Read<short>();
            frame->Read(stream, *context);

            // reg 12 xyzw: Sprite stretch values (corners)
            frame->m_spriteData.m_topLeft.x = (float)frame->m_relX;
            frame->m_spriteData.m_topLeft.y = (float)frame->m_relY;
            frame->m_spriteData.m_bottomRight.x = frame->m_spriteData.m_topLeft.x + (float)frame->m_width;
            frame->m_spriteData.m_bottomRight.y = frame->m_spriteData.m_topLeft.y + (float)frame->m_height;
        }
    }

    CreateAsAtlas(m_frames.data(), tempContexts, frameCount, sizeof(TextureAnimFrame));

    // create distinct textures that differ by sprite data
    m_framesWithDepth.resize(frameCount);
    for (int i = 0; i < frameCount; i++)
    {
        TextureAnimFrame& df(m_framesWithDepth[i]);
        df = m_frames[i];
        // skip first row of UV atlas mapping by one pixel. Reduce by another 0.5 pixel to avoid filtering artifacts
        df.m_height--;
        df.m_atlasData = {
            (float)df.m_width / (float)m_width,
            ((float)df.m_height-0.5f) / (float)m_height,
            (float)tempContexts[i].m_atlasOfsX / (float)m_width,
            ((float)tempContexts[i].m_atlasOfsY + 1.5f) / (float)m_height,
        };

        // create new constant buffer data
        Renderer::Instance().CloneTextureRuntime(m_framesWithDepth[i], m_frames[i]);
    }
}


void TextureAnim::CreateRuntimes(Renderer& renderer)
{
    assert(this->m_runtime != nullptr); // should already be created as part of CreateAsAtlas
}
