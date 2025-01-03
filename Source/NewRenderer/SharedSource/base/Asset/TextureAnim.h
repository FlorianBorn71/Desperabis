#pragma once
#include <DesperabisCommon.h>
#include <Asset/Texture.h>
#include <Asset/ModelGeometry.h>

class D_IMPEXP TextureAnimFrame : public Texture
{
public:
    TextureAnimFrame() = default;
    virtual ~TextureAnimFrame() = default;

public:
    int m_relX, m_relY;
};


class D_IMPEXP TextureAnim : public Texture
{
public:
    TextureAnim() = default;
    virtual ~TextureAnim() = default;

    inline bool IsValidFrame(int animNr, int frameNr) const
    {
        return animNr >= 0 && animNr < m_countX&& frameNr >= 0 && frameNr < m_countY;
    }

    inline int GetFrameIndex(int animNr, int frameNr) const
    {
        assert(IsValidFrame(animNr, frameNr));
        return animNr * m_countY + frameNr;
    }

    TextureAnimFrame* GetFrame(int animNr, int frameNr)
    {
        return &m_frames[GetFrameIndex(animNr, frameNr)];
    }
    const TextureAnimFrame* GetFrame(int animNr, int frameNr) const
    {
        return &m_frames[GetFrameIndex(animNr, frameNr)];
    }
    TextureAnimFrame* GetFrameWithDepth(int animNr, int frameNr)
    {
        return &m_framesWithDepth[GetFrameIndex(animNr, frameNr)];
    }
    const TextureAnimFrame* GetFrameWithDepth(int animNr, int frameNr) const
    {
        return &m_framesWithDepth[GetFrameIndex(animNr, frameNr)];
    }

    virtual void Read(AssetStream& stream);
    virtual void CreateRuntimes(Renderer& renderer);

public:
    int m_countX = 0;
    int m_countY = 0;
    std::vector<TextureAnimFrame> m_frames;
    std::vector<TextureAnimFrame> m_framesWithDepth;
};

