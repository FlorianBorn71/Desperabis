#pragma once
#include <DesperabisCommon.h>
#include <Renderer/Renderer.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/NativeTextureOGL.h>
#include <OpenGL/NativeBufferOGL.h>
#include <Renderer/Renderable.h>
#include <Asset/Texture.h>
#include <OpenGL/RendererOGL.h>

class RendererOGL;

class RenderContextOGL : public RenderContext
{
public:
    RenderContextOGL(Renderer& renderer, const VisibilityResult& elements, ProfilingStats& profiling);
    ~RenderContextOGL() = default;

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
    void SubmitDrawcall(int firstIndex, int numPrimitives);

public:
    RendererOGL& m_rendererOGL;

    NativeModelMeshObjectOGL* m_previousMesh = nullptr;
    NativeTextureObjectOGL* m_previousTexture = nullptr;
    uint32_t m_lastMaterialHash = 0xffffffff;

    NativeTextureObjectOGL* m_debugTexture = nullptr;
};

#include <OpenGL/RenderContextOGL.inl>
