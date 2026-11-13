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
    RenderContextDX11(Renderer& renderer, const VisibilityResult& elements, const RenderingSettings& settings, ProfilingStats& profiling);
    ~RenderContextDX11() = default;

    template<GeometryPassType PASSTYPE>
    void RenderInstance(const RenderableInstance* instance, TransparencyMode transparencyFilter, int drawCallIndex = -1);

    void TrackGlobals();
    void BeginRendering();
    void EndRendering();
    void RenderHudGeometry();
    void SetBlendState(TransparencyMode mode);
    void SetGeometryPassType(GeometryPassType pass);
    void BeginScissorRect(const ScissorRect& rect);
    void EndScissorRect();
    void RenderTextMeshes(const std::vector<const TextMeshInstance*>& instances);
#ifdef ENABLE_ADVANCED_RENDERING
    void RenderPostEffects();
    void BeginWaterRendering();
	void EndWaterRendering();
    void PrepareRefraction(bool firstTimeRefraction);
    // Draw a spherical lens from a radial mask after opaque geometry, before foreground effects.
    void RenderDistortedSphere(const RenderableInstance& instance, int drawCallIndex);
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
    ComPtr<ID3D11RasterizerState> m_savedWaterRasterizer;
    bool m_waterCullDisabled = false;
    ComPtr<ID3D11RasterizerState> m_savedScissorRasterizer;
    D3D11_RECT m_savedScissorRects[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
    UINT m_savedScissorRectCount = 0;
};

#include <DX11/RenderContextDX11.inl>
