#pragma once
#include <DesperabisCommon.h>
#include <Renderer/IGraphicsRuntimeObject.h>
#include <DX11/DX11Decls.h>

class NativeTextureObjectDX11 : public IGraphicsRuntimeObject
{
public:
    NativeTextureObjectDX11() = default;
    virtual ~NativeTextureObjectDX11() = default;

    void Reset()
    {
        m_nativeTex.Reset();
        m_nativeTexView.Reset();
        m_atlasDataBuffer.Reset();
    }

    FUTURE(void) LoadFromDDS(ID3D11Device3* device, std::wstring_view filename);

    ComPtr<ID3D11Texture2D> m_nativeTex;
    ComPtr<ID3D11ShaderResourceView> m_nativeTexView;
    ComPtr<ID3D11Buffer> m_atlasDataBuffer; // TextureAtlasCBData
};

class RenderTargetDX11 : public IRenderTarget
{
public:
    RenderTargetDX11() = default;
    ~RenderTargetDX11() = default;

    // from main target
    void Create(ID3D11Device3* device, ID3D11DeviceContext3* deviceContext, ID3D11RenderTargetView1* colorView, ID3D11DepthStencilView* dsView, const D3D11_VIEWPORT& viewport);
#ifdef ENABLE_ADVANCED_RENDERING
    // create off-screen target
    void Create(ID3D11Device3* device, ID3D11DeviceContext3* deviceContext, int sizeX, int sizeY);
#endif

    void Reset();
    void BindAsTarget(bool withDepth);
    void Clear(const float* clearColor);
    void BindAsTexture(int slot);

public:
    bool m_performClear = true;
    D3D11_VIEWPORT m_viewport;
    ComPtr<ID3D11DeviceContext3> m_deviceContext;
    ComPtr<ID3D11Texture2D> m_renderTargetTexture;
    ComPtr<ID3D11RenderTargetView> m_renderTargetView;
    ComPtr<ID3D11Texture2D> m_depthStencilBuffer;
    ComPtr<ID3D11DepthStencilView> m_depthStencilView;

    ComPtr<ID3D11ShaderResourceView> m_shaderResourceView;
    ComPtr<ID3D11ShaderResourceView> m_shaderResourceViewDS;

    DXGI_FORMAT m_colorFormat = DXGI_FORMAT_UNKNOWN;
};


class NativeModelMeshObjectDX11 : public IGraphicsRuntimeObject
{
public:
    NativeModelMeshObjectDX11() = default;
    virtual ~NativeModelMeshObjectDX11() = default;

    ComPtr<ID3D11Buffer> m_positionBuffer;
    // ComPtr<ID3D11ShaderResourceView> m_positionBufferView;
    ComPtr<ID3D11Buffer> m_vertexBuffer;
    ComPtr<ID3D11Buffer> m_indexBuffer;
};
