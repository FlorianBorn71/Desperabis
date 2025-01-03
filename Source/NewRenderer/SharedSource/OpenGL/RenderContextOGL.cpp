#include "pch.h"
#include <OpenGL/RenderContextOGL.h>
#include <OpenGL/RendererOGL.h>

#include <Renderer/Camera.h>
#include <Renderer/Renderable.h>
#include <Renderer/RenderableInstance.h>
#include <Renderer/IGraphicsRuntimeObject.h>
#include <Asset/AssetManager.h>
#include <Asset/Texture.h>
#include <Asset/ModelGeometry.h>
#include <Asset/ModelAnim.h>
#include <Scene/Scene.h>

static const DepthWriteMode BlendModeToDepthMode[(int)TransparencyMode::TransparencyModeCount] =
{
    DepthWriteMode::DepthTestBit | DepthWriteMode::DepthWriteBit, //Opaque = 0,
    DepthWriteMode::DepthTestBit, //AlphaBlended = 1,
    DepthWriteMode::DepthTestBit, //Additive = 2,

    DepthWriteMode::None, //SkyOpaque = 3,
    DepthWriteMode::None, //SkyAlphaBlended = 4,
    DepthWriteMode::None, //SkyAdditive = 5,

    DepthWriteMode::DepthTestBit, //WaterLevel1 = 6,
};


RenderContextOGL::RenderContextOGL(Renderer& renderer, const VisibilityResult& elements, ProfilingStats& profiling)
    : RenderContext(renderer, elements, profiling)
    , m_rendererOGL((RendererOGL&)renderer)
{
}


void RenderContextOGL::BeginRendering()
{
    m_previousMesh = nullptr;
    m_previousTexture = nullptr;
    m_lastMaterialHash = 0xffffffff;
}

void RenderContextOGL::EndRendering()
{
	glDepthMask(GL_TRUE);
}


void RenderContextOGL::SetBlendState(TransparencyMode mode)
{
    // blend mode:

    // depth write/read mode
    DepthWriteMode dsMode = BlendModeToDepthMode[(int)mode];
    if ((dsMode & DepthWriteMode::DepthTestBit) != 0)
    {
        glEnable(GL_DEPTH_TEST);
    }
    else
    {
        glDisable(GL_DEPTH_TEST);
    }
    
    glDepthMask(((dsMode & DepthWriteMode::DepthWriteBit) != 0) ? GL_TRUE : GL_FALSE);
	glDepthFunc(GL_LEQUAL);
    CHECK_GL_ERRORS;
}

void RenderContextOGL::SetGeometryPassType(GeometryPassType pass)
{
    // vertex and pixel shader
    if (pass != GeometryPassType::MirrorReceiver)
    {
    }
}


void RenderContextOGL::TrackGlobals()
{
    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	CHECK_GL_ERRORS;
    assert(m_currentCamera != nullptr);
    const Camera* camera = m_currentCamera;
    RenderTargetOGL* renderTarget = m_rendererOGL.GetRenderTarget<RenderTargetOGL>(m_renderTargetPass);
    assert(renderTarget != nullptr);
    m_isStereo = camera->IsStereo();

	const GlobalSettings& settings = GlobalSettings::Instance();
	Renderer::GlobalsData& globals(m_rendererOGL.m_globalsData);

	if (m_renderTargetPass == RenderTargetPassType::MainView)
	{
		globals.gamma = settings.gamma;
	}
	else
	{
		// When we are rendering into a render target (like mirror) we render without gamma correction and rather apply gamma when we render the mittor instances
		globals.gamma = 1.f;
	}
    globals.inverseGamma = 1.f / globals.gamma;
	globals.CurrentTime = { (float)m_rendererOGL.m_currentTime, (float)fmod(m_rendererOGL.m_currentTime, 6.283185307179586),0,0 };

    // extract camera data:
    for (int eye = 0; eye < 2; eye++)
    {
        Matrix4 projectionMat = camera->GetProjectionMatrix(eye);
        Matrix4 worldViewMat = camera->GetViewMatrix(eye);
        Matrix4 view2WorldMat = camera->GetCameraTransformMatrix(eye);
        Matrix4 worldViewProj = worldViewMat * projectionMat;

        globals.worldViewMat[eye].Set(worldViewMat);
        globals.viewWorldMat[eye].Set(view2WorldMat);
        globals.projectionMat[eye].Set(projectionMat);
        globals.worldViewProjMat[eye].Set(worldViewProj);
    }

    float fRTWidth = (float)renderTarget->m_width;
    float fRTHeight = (float)renderTarget->m_height;
    globals.renderTargetSize = { fRTWidth, fRTHeight, 1.f / fRTWidth, 1.f / fRTHeight };
    float fogFalloff = m_elements.m_fogFalloff;
    globals.fogParameter = { m_elements.m_fogColor.x, m_elements.m_fogColor.y, m_elements.m_fogColor.z,  fogFalloff }; // matches clear color
    globals.ambientColor = { m_elements.m_ambientColor.x, m_elements.m_ambientColor.y, m_elements.m_ambientColor.z, m_elements.m_ambientNrmlContribution };
#ifdef ENABLE_ADVANCED_RENDERING
    assert(m_elements.m_visibileLights.size() <= RendererOGL::MaxVisibleLights);
    assert(m_elements.m_visibileCoronas.size() <= RendererOGL::MaxVisibleLights);
    globals.NumVisibleLights = (int)m_elements.m_visibileLights.size();
    globals.NumVisibleCoronas = (int)m_elements.m_visibileCoronas.size();
    // light map:
    if (m_elements.m_lightmap != nullptr)
    {
        globals.wsToLmCoords = m_elements.m_lightmap->m_atlasData;
        NativeTextureObjectOGL* nativeTex = m_elements.m_lightmap->GetRuntime<NativeTextureObjectOGL>();
        assert(nativeTex != nullptr);
        m_d3dContext->PSSetShaderResources(1, 1, nativeTex->m_nativeTexView.GetAddressOf());
    }
#endif


#ifdef ENABLE_ADVANCED_RENDERING
    // light sources
    if (globals.NumVisibleLights > 0)
    {
        const D3D11_BOX updateRange = { 0, 0, 0, globals.NumVisibleLights * sizeof(PackedLightSourceParams), 1, 1 };
        m_d3dContext->UpdateSubresource1(m_rendererOGL.m_constantBufferLights.Get(), 0, &updateRange, m_elements.m_visibileLights.data(), 0, 0, D3D11_COPY_DISCARD);
    }
    m_d3dContext->VSSetConstantBuffers1(5, 1, m_rendererOGL.m_constantBufferLights.GetAddressOf(), nullptr, nullptr);
    m_d3dContext->PSSetConstantBuffers1(5, 1, m_rendererOGL.m_constantBufferLights.GetAddressOf(), nullptr, nullptr);
#endif

    m_rendererOGL.m_globals.Update(globals);

	CHECK_GL_ERRORS;
    renderTarget->BindAsTarget(true);
    float clearcolor[4] = { m_elements.m_fogColor.x,m_elements.m_fogColor.y, m_elements.m_fogColor.z, 0 };
    renderTarget->Clear(clearcolor);

    // default shader:
	CHECK_GL_ERRORS;
    m_rendererOGL.m_defaultShader.Bind();
    m_rendererOGL.m_globals.Bind(0);
	CHECK_GL_ERRORS;
}



void RenderContextOGL::SubmitDrawcall(int firstIndex, int numPrimitives)
{
    const int indexCount = numPrimitives * 3;
    assert(indexCount > 0);
    m_profiling.m_numDrawCalls++;
    m_profiling.m_numTriangles += numPrimitives;
    if (m_isStereo)
    {
    }
    else
    {
    }

    glDrawElements(GL_TRIANGLES, numPrimitives * 3, GL_UNSIGNED_SHORT, (void*)(sizeof(short) * firstIndex));
	CHECK_GL_ERRORS;
}


#ifdef ENABLE_ADVANCED_RENDERING

void RenderContextOGL::BeginWaterRendering()
{

}

void RenderContextOGL::EndWaterRendering()
{
    // unbind mirror rt
}


void RenderContextOGL::PrepareRefraction(bool firstTimeRefraction)
{
    if (firstTimeRefraction)
    {
        m_rendererOGL.CopyFrameBuffer();
    }
}

void RenderContextOGL::PrepareWaterRendering(const VisibilityResult::VisibleWaterLevel& water)
{
    RendererOGL::WaterShaderConstants& waterData(m_rendererOGL.m_waterConstants);

    // bind the correct texture
    if (water.m_useReflection)
    {
        RenderTargetOGL* mirrorRt = (RenderTargetOGL*)m_rendererOGL.m_mirrorRT.get();
        mirrorRt->BindAsTexture(0);
        waterData.ScreenToRTUV = { 1.f / m_rendererOGL.m_mainViewport.Width, 1.f / m_rendererOGL.m_mainViewport.Height, -1.f, 1.f }; // 1-u for the mirrored texture
    }
    else
    {
        m_d3dContext->PSSetShaderResources(0, 1, m_rendererOGL.m_frameBufferCopy.m_nativeTexView.GetAddressOf());
        waterData.ScreenToRTUV = { 1.f / m_rendererOGL.m_mainViewport.Width, 1.f / m_rendererOGL.m_mainViewport.Height, 1.f, 0.f };
    }

	m_previousTexture = nullptr;

    // shaders for rendering water instances
    m_d3dContext->VSSetShader(m_rendererOGL.m_vertexShaderMirrorReceiver.Get(), nullptr, 0);
    m_d3dContext->PSSetShader(m_rendererOGL.m_pixelShaderMirrorReceiver.Get(), nullptr, 0);
    m_d3dContext->PSSetSamplers(1, 1, m_rendererOGL.m_samplerStateClamp.GetAddressOf());

    waterData.Uv1Scroll = water.m_scrollParams1;
    waterData.Uv2Scroll = water.m_scrollParams2;
//    waterData.TintColor = { 1,1,1,1 };
    //static float strength = 1.0f;
    //waterData.DistortionStrength = 1.f;
    m_d3dContext->UpdateSubresource1(m_rendererOGL.m_constantBufferWaterParams.Get(), 0, nullptr, &waterData, 0, 0, 0);

    m_d3dContext->VSSetConstantBuffers1(6, 1, m_rendererOGL.m_constantBufferWaterParams.GetAddressOf(), nullptr, nullptr);
    m_d3dContext->PSSetConstantBuffers1(6, 1, m_rendererOGL.m_constantBufferWaterParams.GetAddressOf(), nullptr, nullptr); // also need atlas data for PS

    // normalmap
    m_d3dContext->PSSetShaderResources(1, 1, m_rendererOGL.m_mirrorNormalMap.m_nativeTexView.GetAddressOf());

}

void RenderContextOGL::RenderPostEffects()
{
    if (m_elements.m_visibileCoronas.size())
    {
        int coronaCount = (int)m_elements.m_visibileCoronas.size();

    }
}
#endif // ENABLE_ADVANCED_RENDERING

