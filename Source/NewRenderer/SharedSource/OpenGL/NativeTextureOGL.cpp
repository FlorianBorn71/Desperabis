#include "pch.h"
#include <OpenGL/NativeTextureOGL.h>
//#include <OpenGL/RendererOGL.h>
#include <IO/FileManager.h>
#include <Asset/Texture.h>

using DWORD = unsigned long;

struct DDS_PIXELFORMAT {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwFourCC;
    DWORD dwRGBBitCount;
    DWORD dwRBitMask;
    DWORD dwGBitMask;
    DWORD dwBBitMask;
    DWORD dwABitMask;
};

struct DDS_HEADER {
    DWORD           dwMagic;
    DWORD           dwSize;
    DWORD           dwFlags;
    DWORD           dwHeight;
    DWORD           dwWidth;
    DWORD           dwPitchOrLinearSize;
    DWORD           dwDepth;
    DWORD           dwMipMapCount;
    DWORD           dwReserved1[11];
    DDS_PIXELFORMAT ddspf;
    DWORD           dwCaps;
    DWORD           dwCaps2;
    DWORD           dwCaps3;
    DWORD           dwCaps4;
    DWORD           dwReserved2;
};

void NativeTextureObjectOGL::Create(int sizeX, int sizeY, const void* data)
{
	CHECK_GL_ERRORS;
	glGenTextures(1, &m_texHandle);
	glBindTexture(GL_TEXTURE_2D, m_texHandle);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sizeX, sizeY, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    m_ownsHandle = true;
	CHECK_GL_ERRORS;
}

void NativeTextureObjectOGL::CreateAtlasDataCB(Texture& tex)
{
	tex.m_atlasData.Store(m_atlasCB.m_data.m_atlasTransform);
	tex.m_spriteData.Store(m_atlasCB.m_data.m_spriteParams);

    m_atlasCB.Initialize();
}

void NativeTextureObjectOGL::Bind(int slot)
{
	glActiveTexture(GL_TEXTURE0 + slot);
	glBindTexture(GL_TEXTURE_2D, m_texHandle);
	//glUniform1i(slot, slot);

    m_atlasCB.Bind(3);
}

FUTURE(void) NativeTextureObjectOGL::LoadFromDDS(std::wstring_view filename)
{
    std::wstring f = FileManager::Instance().FilenameToNative(filename);
    std::vector<byte> rawData = CO_AWAIT FileManager::Instance().ReadDataFuture(f);
    byte* data = rawData.data();

    DDS_HEADER& header = *(DDS_HEADER*)data;
    data += sizeof(DDS_HEADER);

    header.dwMipMapCount = 1;
	Create(header.dwWidth, header.dwHeight, data);
}

/*
void RenderTargetOGL::Create(ID3D11Device3* device, ID3D11DeviceContext3* deviceContext, ID3D11RenderTargetView1* colorView, ID3D11DepthStencilView* dsView, const D3D11_VIEWPORT& viewport)
{
    m_deviceContext = deviceContext;
    m_width = (int)(viewport.Width + 0.1f);
    m_height = (int)(viewport.Height + 0.1f);
    m_renderTargetView = colorView;
    m_depthStencilView = dsView;
    m_viewport = viewport;

    ComPtr<ID3D11Resource> res;
    dsView->GetResource(&res);
    ThrowIfFailed(res.As(&m_depthStencilBuffer));

    colorView->GetResource(&res);
    ThrowIfFailed(res.As(&m_renderTargetTexture));
    D3D11_TEXTURE2D_DESC colorDesc;
    m_renderTargetTexture->GetDesc(&colorDesc);
    m_colorFormat = colorDesc.Format;

    // create another shader resource view for using z-buffer in shader
    CD3D11_SHADER_RESOURCE_VIEW_DESC shaderRViewDesc_DS(D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R32_FLOAT);
    ThrowIfFailed(device->CreateShaderResourceView(m_depthStencilBuffer.Get(), &shaderRViewDesc_DS, &m_shaderResourceViewDS));

}

#ifdef ENABLE_ADVANCED_RENDERING
void RenderTargetDX11::Create(ID3D11Device3* device, ID3D11DeviceContext3* deviceContext, int sizeX, int sizeY)
{
    m_deviceContext = deviceContext;
    m_width = sizeX;
    m_height = sizeY;

    // textures
    m_colorFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
    CD3D11_TEXTURE2D_DESC textureDesc(m_colorFormat, sizeX, sizeY, 1, 1, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT);
    ThrowIfFailed(device->CreateTexture2D(&textureDesc, NULL, &m_renderTargetTexture));

    CD3D11_TEXTURE2D_DESC textureDescDS = textureDesc;
    textureDescDS.Format = DXGI_FORMAT_R32_TYPELESS; // needed for corona readback
    textureDescDS.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    ThrowIfFailed(device->CreateTexture2D(&textureDescDS, NULL, &m_depthStencilBuffer));

    // color RT view
    CD3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
    renderTargetViewDesc.Format = textureDesc.Format;
    renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    renderTargetViewDesc.Texture2D.MipSlice = 0;
    ThrowIfFailed(device->CreateRenderTargetView(m_renderTargetTexture.Get(), &renderTargetViewDesc, &m_renderTargetView));
    // DS RT view
    CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc(D3D11_DSV_DIMENSION_TEXTURE2D, DXGI_FORMAT_D32_FLOAT);
    ThrowIfFailed(device->CreateDepthStencilView(m_depthStencilBuffer.Get(), &depthStencilViewDesc, &m_depthStencilView));

    // Create the shader resource view.
    CD3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
    shaderResourceViewDesc.Format = textureDesc.Format;
    shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
    shaderResourceViewDesc.Texture2D.MipLevels = 1;
    ThrowIfFailed(device->CreateShaderResourceView(m_renderTargetTexture.Get(), &shaderResourceViewDesc, &m_shaderResourceView));

    // create another shader resource view for using z-buffer in shader
    CD3D11_SHADER_RESOURCE_VIEW_DESC shaderRViewDesc_DS(D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R32_FLOAT);
    ThrowIfFailed(device->CreateShaderResourceView(m_depthStencilBuffer.Get(), &shaderRViewDesc_DS, &m_shaderResourceViewDS));

    m_viewport = CD3D11_VIEWPORT(0.f, 0.f, (float)m_width, (float)m_height);
}
#endif
*/

void RenderTargetOGL::Reset()
{
}

void RenderTargetOGL::BindAsTarget(bool withDepth)
{
}

void RenderTargetOGL::Clear(const float* clearColor)
{
    if (m_performClear)
    {
    }
}

void RenderTargetOGL::BindAsTexture(int slot)
{
}
