#include "pch.h"
#include <OpenGL/RendererOGL.h>
#include <OpenGL/RenderContextOGL.h>
#include <Asset/Texture.h>


FUTURE(void) RendererOGL::Initialize(int width, int height)
{
    g_Instance = this;
    m_isReady = true;

    {
		m_mainRT = std::make_unique<RenderTargetOGL>();
        RenderTargetOGL* mainRT = (RenderTargetOGL*)m_mainRT.get();
		mainRT->m_width = width;
        mainRT->m_height = height;
		mainRT->m_performClear = false;
    }

	CO_AWAIT m_defaultShader.CreateFromFile(L"shaders\\standard.vs", L"shaders\\standard.fs");    
    //m_defaultShader.CreateDefaultVertexLayout();

    // some setup that does not change per context/mode
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT); // why?

    CO_RETURN;
}

void RendererOGL::Deinitialize()
{
    m_defaultShader.Deinitialize();

	m_globals.Deinitialize();
	m_instance.Deinitialize();
	m_instanceMaterial.Deinitialize();
}


void RendererOGL::RenderSceneElements(const VisibilityResult& elements, ProfilingStats& stats)
{
    RenderScene_Impl<RenderContextOGL>(elements, stats);
}

void RendererOGL::CreateTexture(Texture& tex, TextureLoadingContext& data)
{
	CHECK_GL_ERRORS;
    std::shared_ptr<NativeTextureObjectOGL> nativeTex = std::make_shared<NativeTextureObjectOGL>();
    nativeTex->m_Id = m_runtimeIdCounter.fetch_add(1);
	tex.m_runtime = nativeTex;
    nativeTex->Create(tex.m_width, tex.m_height, data.m_rgbaData.data());
    nativeTex->CreateAtlasDataCB(tex);
	CHECK_GL_ERRORS;
}

void RendererOGL::CloneTextureRuntime(Texture& tex, Texture& sourcetex)
{
	CHECK_GL_ERRORS;
	std::shared_ptr<NativeTextureObjectOGL> nativeTex = std::make_shared<NativeTextureObjectOGL>();
	nativeTex->m_Id = m_runtimeIdCounter.fetch_add(1);
    tex.m_runtime = nativeTex;

    NativeTextureObjectOGL* srcRuntime = sourcetex.GetNativeRuntimeObj<NativeTextureObjectOGL>();
    nativeTex->m_ownsHandle = false;
    nativeTex->m_texHandle = srcRuntime->m_texHandle;
    nativeTex->CreateAtlasDataCB(tex);
    CHECK_GL_ERRORS;
}

std::shared_ptr<IGraphicsRuntimeObject> RendererOGL::CreateModelBuffers(ModelConversionContext& data)
{
	std::shared_ptr<NativeModelMeshObjectOGL> nativeMesh = std::make_shared<NativeModelMeshObjectOGL>();
    nativeMesh->m_Id = m_runtimeIdCounter.fetch_add(1);
    nativeMesh->CreateBuffers(data);
    return nativeMesh;
}

void RendererOGL::ConfigureVisibility(IVisibilityContext& visContext)
{
    auto& limits = visContext.AccessLimits();
    limits.MaxVisibleLights = MaxVisibleLights;
}
