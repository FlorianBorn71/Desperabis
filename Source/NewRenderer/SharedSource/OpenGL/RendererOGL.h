#pragma once
#include <DesperabisCommon.h>
#include <Renderer/Renderer.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/NativeTextureOGL.h>
#include <OpenGL/NativeBufferOGL.h>
#include <OpenGL/NativeShaderOGL.h>

class RendererOGL : public Renderer
{
public:
    RendererOGL() = default;
    ~RendererOGL() = default;

    virtual FUTURE(void) Initialize(int width, int height);
    void Deinitialize();

    virtual void RenderSceneElements(const VisibilityResult& elements, ProfilingStats& stats) override final;

    virtual void CreateTexture(Texture& tex, TextureLoadingContext& data) override final;
    virtual void CloneTextureRuntime(Texture& tex, Texture& sourcetex) override final;
    std::shared_ptr<IGraphicsRuntimeObject> CreateModelBuffers(ModelConversionContext& data);
private:
    virtual void ConfigureVisibility(IVisibilityContext& visContext) override final;
	friend class RenderContextOGL;

	TypedBufferOGL<GlobalsData, false, GL_DYNAMIC_DRAW> m_globals;
	TypedBufferOGL<InstanceData, true, GL_STREAM_DRAW> m_instance;
	TypedBufferOGL<InstanceMaterialData, true, GL_STREAM_DRAW> m_instanceMaterial;
    
    NativeShaderOGL m_defaultShader;
private:
    constexpr static const int MaxVisibleLights = 64;
	std::atomic_int32_t m_runtimeIdCounter = 0;
};
