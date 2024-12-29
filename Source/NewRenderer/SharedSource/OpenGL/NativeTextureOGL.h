#pragma once
#include <DesperabisCommon.h>
#include <OpenGL/OpenGL.h>
#include <Renderer/IGraphicsRuntimeObject.h>
#include <Renderer/RendererCommon.h>
#include <OpenGL/NativeBufferOGL.h>


class NativeTextureObjectOGL : public IGraphicsRuntimeObject
{
public:
    NativeTextureObjectOGL() = default;
    virtual ~NativeTextureObjectOGL()
    {
        Deinitialize();
    }

    void Create(int sizeX, int sizeY, const void* data);
    void CreateAtlasDataCB(Texture& tex);

    void Deinitialize()
    {
        m_atlasCB.Deinitialize();
        if (m_texHandle && m_ownsHandle)
        {
            glDeleteTextures(1, &m_texHandle);
        }
		m_texHandle = 0;
    }

	void Bind(int slot);


    FUTURE(void) LoadFromDDS(std::wstring_view filename);

    bool m_ownsHandle = true;
    GLuint m_texHandle = 0;

    TypedBufferOGL<TextureAtlasCBData, false, GL_STATIC_DRAW> m_atlasCB;
};

class RenderTargetOGL : public IRenderTarget
{
public:
    RenderTargetOGL() = default;
    ~RenderTargetOGL() = default;

    // from main target
//    void Create(ID3D11Device3* device, ID3D11DeviceContext3* deviceContext, ID3D11RenderTargetView1* colorView, ID3D11DepthStencilView* dsView, const D3D11_VIEWPORT& viewport);
#ifdef ENABLE_ADVANCED_RENDERING
    // create off-screen target
    //void Create(ID3D11Device3* device, ID3D11DeviceContext3* deviceContext, int sizeX, int sizeY);
#endif

    void Reset();
    void BindAsTarget(bool withDepth);
    void Clear(const float* clearColor);
    void BindAsTexture(int slot);

public:
    bool m_performClear = true;
};
