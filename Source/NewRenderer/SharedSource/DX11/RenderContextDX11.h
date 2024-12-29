#pragma once
#include <DesperabisCommon.h>
#include <Renderer/Renderer.h>
#include <Renderer/Renderable.h>
#include <Asset/Texture.h>
#include <DX11/NativeTextureDX11.h>

class RendererDX11;

class RenderContextDX11 : public RenderContext
{
public:
    RenderContextDX11(Renderer& renderer, const VisibilityResult& elements, ProfilingStats& profiling);
    ~RenderContextDX11() = default;

    template<GeometryPassType PASSTYPE>
    void RenderInstance(const RenderableInstance* instance, TransparencyMode transparencyFilter);

    void TrackGlobals();
    void BeginRendering();
    void EndRendering();
    void SetBlendState(TransparencyMode mode);
    void SetGeometryPassType(GeometryPassType pass);
#ifdef ENABLE_ADVANCED_RENDERING
    void RenderPostEffects();
    void BeginWaterRendering();
	void EndWaterRendering();
    void PrepareRefraction(bool firstTimeRefraction);
    void PrepareWaterRendering(const VisibilityResult::VisibleWaterLevel& water);
#endif

protected:
    void RenderDebugTexture(ID3D11ShaderResourceView* tex);
    void SubmitDrawcall(int firstIndex, int numPrimitives);

public:
    RendererDX11& m_rendererDX11;
    ComPtr<ID3D11DeviceContext3> m_d3dContext;

    class NativeModelMeshObjectDX11* m_previousMesh = nullptr;
    NativeTextureObjectDX11* m_previousTexture = nullptr;
    uint32_t m_lastMaterialHash = 0xffffffff;

    NativeTextureObjectDX11* m_debugTexture = nullptr;
};

#include <DX11/RenderContextDX11.inl>
