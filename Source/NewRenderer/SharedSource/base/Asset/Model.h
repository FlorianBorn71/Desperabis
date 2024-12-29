#pragma once
#include <DesperabisCommon.h>
#include <Renderer/DrawCall.h>
#include <Renderer/Renderable.h>


class D_IMPEXP Model : public Renderable
{
public:
    Model() = default;
    virtual ~Model() = default;

public:
    friend class ModelAnim;
    void Read(AssetStream& s, ModelLoadingContext& context);

    ModelAnim* m_owner = nullptr;
    int m_animNr = 0;
    int m_frameNr = 0;
    int m_numUnsupportedPrimitives = 0;
};
