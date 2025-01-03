#pragma once
#include <DesperabisCommon.h>
#include <Renderer/Renderer.h>
#include <DX11/DX11Decls.h>
#include <DX11/NativeTextureDX11.h>


inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        // Set a breakpoint on this line to catch Win32 API errors.
        assert(false && "Operation failed");
    }
}

class RendererDX11 : public Renderer
{
public:
    RendererDX11() = default;
    ~RendererDX11() = default;

    FUTURE(void) Initialize(ID3D11Device3* device, ID3D11DeviceContext3* context, ID3D11RenderTargetView1* colorView, ID3D11DepthStencilView* dsView, const D3D11_VIEWPORT& viewport);
    void Deinitialize();
	void ReInitialize(ID3D11RenderTargetView1* colorView, ID3D11DepthStencilView* dsView, const D3D11_VIEWPORT& viewport);

    virtual void RenderSceneElements(const VisibilityResult& elements, ProfilingStats& stats) override final;

    virtual void CreateTexture(Texture& tex, TextureLoadingContext& data) override final;
    virtual void CloneTextureRuntime(Texture& tex, Texture& sourcetex) override final;
    std::shared_ptr<IGraphicsRuntimeObject> CreateModelBuffers(ModelConversionContext& data);
private:
    friend class RenderContextDX11;

    virtual void ConfigureVisibility(IVisibilityContext& visContext) override final;
    void CreateAtlasDataCB(Texture& tex, NativeTextureObjectDX11& runtime);
#ifdef ENABLE_ADVANCED_RENDERING
    void CreateCoronaMesh();
    void CopyFrameBuffer();
#endif

private:
    constexpr static const int MaxVisibleLights = 64;

    D3D11_VIEWPORT m_mainViewport = {};
    ComPtr<ID3D11Device3> m_d3dDevice;
    ComPtr<ID3D11DeviceContext3> m_d3dContext;
    ComPtr<ID3D11InputLayout> m_inputLayout;
    ComPtr<ID3D11Buffer> m_constantBufferGlobals;
    ComPtr<ID3D11Buffer> m_constantBufferInstance;
    ComPtr<ID3D11Buffer> m_constantBufferMaterial;
#ifdef ENABLE_ADVANCED_RENDERING
    struct WaterShaderConstants
    {
        Float4Storage ScreenToRTUV; // xy=1/rt size, z=scale u, w= ofs u (so that zw = -1,1 mirrors u coordinate)
        // normalmap:
        Float4Storage Uv1Scroll;
        Float4Storage Uv2Scroll;
    };
    WaterShaderConstants m_waterConstants;
    ComPtr<ID3D11Buffer> m_constantBufferLights;
    ComPtr<ID3D11Buffer> m_constantBufferWaterParams;
    ComPtr<ID3D11PixelShader> m_pixelShaderMirrorReceiver;
    ComPtr<ID3D11VertexShader> m_vertexShaderMirrorReceiver;
    ComPtr<ID3D11PixelShader> m_pixelShaderCorona;
    ComPtr<ID3D11VertexShader> m_vertexShaderCorona;
    ComPtr<ID3D11GeometryShader> m_geometryShaderCorona;
    NativeTextureObjectDX11 m_mirrorNormalMap;
    std::shared_ptr<IGraphicsRuntimeObject> m_coronaMesh;
    ComPtr<ID3D11DepthStencilState> m_depthStateCorona;
    NativeTextureObjectDX11 m_coronaTexture;
    NativeTextureObjectDX11 m_coronaTextureFog;
    NativeTextureObjectDX11 m_frameBufferCopy;
#endif
    ComPtr<ID3D11VertexShader> m_vertexShader;
    ComPtr<ID3D11PixelShader> m_pixelShader;
	ComPtr<ID3D11VertexShader> m_vertexShaderFlames;
	ComPtr<ID3D11PixelShader> m_pixelShaderFlames;
    ComPtr<ID3D11SamplerState> m_samplerStateWrap;
    ComPtr<ID3D11SamplerState> m_samplerStateClamp;
    ComPtr<ID3D11BlendState> m_blendState[(int)TransparencyMode::TransparencyModeCount];
    ComPtr<ID3D11DepthStencilState> m_depthState[(int)DepthWriteMode::DepthModeCount];

    std::atomic_int32_t m_runtimeIdCounter = 0;
    bool m_isStereo = false;
};
