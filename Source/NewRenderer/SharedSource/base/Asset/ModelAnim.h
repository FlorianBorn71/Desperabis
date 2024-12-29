#pragma once
#include <DesperabisCommon.h>
#include <Asset/AssetBase.h>
#include <Asset/Model.h>
#include <Asset/Texture.h>
#include <Asset/ModelGeometry.h>

class D_IMPEXP ModelAnim : public AssetBase
{
public:
    ModelAnim();
    virtual ~ModelAnim() = default;

    virtual void Read(AssetStream& s) override;
    virtual void CreateRuntimes(Renderer& renderer) override;

    template<typename T>
    inline T* GetRuntime() const { return static_cast<T*>(m_runtime.get()); }

    Model* GetModel(int animNr, int frameNr) 
    { 
        assert(animNr >= 0 && animNr < m_numAnims&& frameNr >= 0 && frameNr < m_numFrames);
        return &m_model[animNr][frameNr]; 
    }
    const Model* GetModel(int animNr, int frameNr) const 
    { 
        assert(animNr >= 0 && animNr < m_numAnims&& frameNr >= 0 && frameNr < m_numFrames);
        return &m_model[animNr][frameNr];
    }
    Model* GetModelSafe(int animNr, int frameNr)
    {
        if (animNr >= 0 && animNr < m_numAnims && frameNr >= 0 && frameNr < m_numFrames)
        {
            return &m_model[animNr][frameNr];
        }
        return nullptr;
    }
    const Model* GetModelSafe(int animNr, int frameNr) const
    {
        if (animNr >= 0 && animNr < m_numAnims&& frameNr >= 0 && frameNr < m_numFrames)
        { 
            return &m_model[animNr][frameNr];
        }
        return nullptr;
    }

public:
    uint16_t m_textureUsedMask = 0;
    Texture m_ownTextures[16];
    Texture m_textureAtlas;

    int m_numAnims = 0;
    int m_numFrames = 0;
    Model m_model[6][5];

    std::shared_ptr<IGraphicsRuntimeObject> m_runtime;
};

