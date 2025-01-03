#include "pch.h"
#include <DX11/RenderContextDX11.h>
#include <DX11/RendererDX11.h>

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


RenderContextDX11::RenderContextDX11(Renderer& renderer, const VisibilityResult& elements, ProfilingStats& profiling)
    : RenderContext(renderer, elements, profiling)
    , m_rendererDX11((RendererDX11&)renderer)
{
    m_d3dContext = m_rendererDX11.m_d3dContext;
}


void RenderContextDX11::BeginRendering()
{
    m_previousMesh = nullptr;
    m_previousTexture = nullptr;
    m_lastMaterialHash = 0xffffffff;

    m_d3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_d3dContext->IASetInputLayout(m_rendererDX11.m_inputLayout.Get());
}

void RenderContextDX11::EndRendering()
{
#ifdef _DEBUG
    m_debugTexture = nullptr;
/*
    if (Texture* tex = m_elements.m_scene->GetCollisionFieldTexture().get())
    {
        m_debugTexture = tex->GetNativeRuntimeObj<NativeTextureObjectDX11>();
    }
    if (m_debugTexture)
    {
        RenderDebugTexture(m_debugTexture->m_nativeTexView.Get());
    }
	RenderTargetDX11* mirrorRt = (RenderTargetDX11*)m_rendererDX11.m_mirrorRT.get();
	RenderDebugTexture(mirrorRt->m_shaderResourceView.Get());
	*/
#endif

}


void RenderContextDX11::SetBlendState(TransparencyMode mode)
{
    // blend mode:
    assert(m_rendererDX11.m_blendState[(int)mode].Get() != nullptr);
    m_d3dContext->OMSetBlendState(m_rendererDX11.m_blendState[(int)mode].Get(), nullptr, 0xFFFFFFFFu);

    // depth write/read mode
    DepthWriteMode dsMode = BlendModeToDepthMode[(int)mode];
    m_d3dContext->OMSetDepthStencilState(m_rendererDX11.m_depthState[(int)dsMode].Get(), 0);
}

void RenderContextDX11::SetGeometryPassType(GeometryPassType pass)
{
    // vertex and pixel shader
    if (pass == GeometryPassType::InstancesInFlames)
    {
		m_d3dContext->VSSetShader(m_rendererDX11.m_vertexShaderFlames.Get(), nullptr, 0);
		m_d3dContext->PSSetShader(m_rendererDX11.m_pixelShaderFlames.Get(), nullptr, 0);
    }
    else if (pass != GeometryPassType::MirrorReceiver)
    {
        m_d3dContext->VSSetShader(m_rendererDX11.m_vertexShader.Get(), nullptr, 0);
        m_d3dContext->PSSetShader(m_rendererDX11.m_pixelShader.Get(), nullptr, 0);
    }

    m_d3dContext->VSSetConstantBuffers1(1, 1, m_rendererDX11.m_constantBufferInstance.GetAddressOf(), nullptr, nullptr);
    m_d3dContext->PSSetConstantBuffers1(1, 1, m_rendererDX11.m_constantBufferInstance.GetAddressOf(), nullptr, nullptr);
    m_d3dContext->VSSetConstantBuffers1(4, 1, m_rendererDX11.m_constantBufferMaterial.GetAddressOf(), nullptr, nullptr);
    m_d3dContext->PSSetConstantBuffers1(4, 1, m_rendererDX11.m_constantBufferMaterial.GetAddressOf(), nullptr, nullptr);
}


void RenderContextDX11::TrackGlobals()
{
    assert(m_currentCamera != nullptr);
    const Camera* camera = m_currentCamera;
    RenderTargetDX11* renderTarget = m_rendererDX11.GetRenderTarget<RenderTargetDX11>(m_renderTargetPass);
    assert(renderTarget != nullptr);
    m_isStereo = camera->IsStereo();

	const GlobalSettings& settings = GlobalSettings::Instance();
    Renderer::GlobalsData& globals(m_rendererDX11.m_globalsData);
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
    globals.CurrentTime = { (float)m_rendererDX11.m_currentTime, (float)fmod(m_rendererDX11.m_currentTime, 6.283185307179586),0,0 };

    // extract camera data:
    for (int eye = 0; eye < 2; eye++)
    {
        Matrix4 projectionMat = camera->GetProjectionMatrix(eye);
        Matrix4 worldViewMat = camera->GetViewMatrix(eye);
        Matrix4 view2WorldMat = camera->GetCameraTransformMatrix(eye);
        Matrix4 worldViewProj = worldViewMat * projectionMat;

        globals.worldViewMat[eye].SetTransposed(worldViewMat);
        globals.viewWorldMat[eye].SetTransposed(view2WorldMat);
        globals.projectionMat[eye].SetTransposed(projectionMat);
        globals.worldViewProjMat[eye].SetTransposed(worldViewProj);
    }

    float fRTWidth = (float)renderTarget->m_width;
    float fRTHeight = (float)renderTarget->m_height;
    globals.renderTargetSize = { fRTWidth, fRTHeight, 1.f / fRTWidth, 1.f / fRTHeight };
    float fogFalloff = m_elements.m_fogFalloff;
    globals.fogParameter = { m_elements.m_fogColor.x, m_elements.m_fogColor.y, m_elements.m_fogColor.z,  fogFalloff }; // matches clear color
    globals.ambientColor = { m_elements.m_ambientColor.x, m_elements.m_ambientColor.y, m_elements.m_ambientColor.z, m_elements.m_ambientNrmlContribution };
#ifdef ENABLE_ADVANCED_RENDERING
    assert(m_elements.m_visibileLights.size() <= RendererDX11::MaxVisibleLights);
    assert(m_elements.m_visibileCoronas.size() <= RendererDX11::MaxVisibleLights);
    globals.NumVisibleLights = (int)m_elements.m_visibileLights.size();
    globals.NumVisibleCoronas = (int)m_elements.m_visibileCoronas.size();
    // light map:
    if (m_elements.m_lightmap != nullptr)
    {
        globals.wsToLmCoords = m_elements.m_lightmap->m_atlasData;
        NativeTextureObjectDX11* nativeTex = m_elements.m_lightmap->GetNativeRuntimeObj<NativeTextureObjectDX11>();
        assert(nativeTex != nullptr);
        m_d3dContext->PSSetShaderResources(1, 1, nativeTex->m_nativeTexView.GetAddressOf());
    }
#endif

    // Prepare the constant buffer to send it to the graphics device.
    m_d3dContext->UpdateSubresource1(m_rendererDX11.m_constantBufferGlobals.Get(), 0, nullptr, &globals, 0, 0, 0);

    // Send the constant buffer to the graphics device.
    m_d3dContext->VSSetConstantBuffers1(0, 1, m_rendererDX11.m_constantBufferGlobals.GetAddressOf(), nullptr, nullptr);
    m_d3dContext->PSSetConstantBuffers1(0, 1, m_rendererDX11.m_constantBufferGlobals.GetAddressOf(), nullptr, nullptr);
    m_d3dContext->GSSetConstantBuffers1(0, 1, m_rendererDX11.m_constantBufferGlobals.GetAddressOf(), nullptr, nullptr);

#ifdef ENABLE_ADVANCED_RENDERING
    // light sources
    if (globals.NumVisibleLights > 0)
    {
        const D3D11_BOX updateRange = { 0, 0, 0, globals.NumVisibleLights * sizeof(PackedLightSourceParams), 1, 1 };
        m_d3dContext->UpdateSubresource1(m_rendererDX11.m_constantBufferLights.Get(), 0, &updateRange, m_elements.m_visibileLights.data(), 0, 0, D3D11_COPY_DISCARD);
    }
    m_d3dContext->VSSetConstantBuffers1(5, 1, m_rendererDX11.m_constantBufferLights.GetAddressOf(), nullptr, nullptr);
    m_d3dContext->PSSetConstantBuffers1(5, 1, m_rendererDX11.m_constantBufferLights.GetAddressOf(), nullptr, nullptr);
#endif

	// unbind mirror rt
    if (m_renderTargetPass != RenderTargetPassType::MainView)
    {
        ID3D11ShaderResourceView* nullView = nullptr;
        m_d3dContext->PSSetShaderResources(0, 1, &nullView);
    }

    renderTarget->BindAsTarget(true);
    float clearcolor[4] = { 
        pow(m_elements.m_fogColor.x, settings.gamma),
		pow(m_elements.m_fogColor.y, settings.gamma),
		pow(m_elements.m_fogColor.z, settings.gamma), 0 };

    renderTarget->Clear(clearcolor);

    // default sampler:
	m_d3dContext->PSSetSamplers(0, 1, m_rendererDX11.m_samplerStateWrap.GetAddressOf());
    m_d3dContext->VSSetSamplers(0, 1, m_rendererDX11.m_samplerStateWrap.GetAddressOf());
}


void RenderContextDX11::RenderDebugTexture(ID3D11ShaderResourceView* tex)
{
    SetGeometryPassType(GeometryPassType::Instances);
    SetBlendState(TransparencyMode::Opaque);
    Renderer::InstanceData& inst(m_rendererDX11.m_instanceData);
    memset(&inst, 0, sizeof(inst));
    inst.m_uniformScaling = 100.0f;
    // put in front of the camera:
    auto cam = m_rendererDX11.GetOrCreateCamera();
    Matrix4 camMat = cam->GetCameraTransformMatrix(0);
    Vector dir = camMat.TransformDirection(Vector(0, 0, -1));
    dir += camMat.GetTranslation();
    Matrix4 m;
    m.SetTranslation(dir);
    inst.m_modelToWSMat.SetTransposed(m);
    m_d3dContext->UpdateSubresource1(m_rendererDX11.m_constantBufferInstance.Get(), 0, nullptr, &inst, 0, 0, 0);

    std::shared_ptr<Renderable> renderable = AssetManager::Instance().GetSpriteRenderable(false);
    NativeModelMeshObjectDX11* nativeMesh = renderable->GetNativeRuntimeObj<NativeModelMeshObjectDX11>();
    m_d3dContext->VSSetConstantBuffers1(2, 1, nativeMesh->m_positionBuffer.GetAddressOf(), nullptr, nullptr);

    {
        UINT stride = sizeof(ModelVertex);
        UINT offset = 0;
        m_d3dContext->IASetVertexBuffers(0, 1, nativeMesh->m_vertexBuffer.GetAddressOf(), &stride, &offset);
        m_d3dContext->IASetIndexBuffer(nativeMesh->m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
        m_previousMesh = nativeMesh;
    }

    {
        // bind texture and constant buffer that holds the atlas data
        Texture* forAtlas = AssetManager::Instance().GetPlainWhiteTexture();
        NativeTextureObjectDX11* nativeTex = forAtlas->GetNativeRuntimeObj<NativeTextureObjectDX11>();
        assert(nativeTex != nullptr);
        m_d3dContext->PSSetShaderResources(0, 1, &tex);
        m_d3dContext->VSSetConstantBuffers1(3, 1, nativeTex->m_atlasDataBuffer.GetAddressOf(), nullptr, nullptr);
		m_previousTexture = nullptr;
	}

    const DrawCall& dc = renderable->m_drawCalls[0];
    m_d3dContext->DrawIndexed(dc.m_primitiveCount * 3, dc.m_firstIndex, 0);
}


void RenderContextDX11::SubmitDrawcall(int firstIndex, int numPrimitives)
{
    const int indexCount = numPrimitives * 3;
    assert(indexCount > 0);
    m_profiling.m_numDrawCalls++;
    m_profiling.m_numTriangles += numPrimitives;
    if (m_isStereo)
    {
        m_d3dContext->DrawIndexedInstanced(
            indexCount,      // Index count per instance.
            2,               // Instance count.
            firstIndex, // Start index location.
            0,               // Base vertex location.
            0                // Start instance location.
        );
    }
    else
    {
        m_d3dContext->DrawIndexed(indexCount, firstIndex, 0);
    }

}


#ifdef ENABLE_ADVANCED_RENDERING

void RenderContextDX11::BeginWaterRendering()
{

}

void RenderContextDX11::EndWaterRendering()
{
    // unbind mirror rt
    ID3D11ShaderResourceView* nullView = nullptr;
	m_d3dContext->PSSetShaderResources(0, 1, &nullView);
}


void RenderContextDX11::PrepareRefraction(bool firstTimeRefraction)
{
    if (firstTimeRefraction)
    {
        m_rendererDX11.CopyFrameBuffer();
    }
}

void RenderContextDX11::PrepareWaterRendering(const VisibilityResult::VisibleWaterLevel& water)
{
    RendererDX11::WaterShaderConstants& waterData(m_rendererDX11.m_waterConstants);

    // bind the correct texture
    if (water.m_useReflection)
    {
        RenderTargetDX11* mirrorRt = (RenderTargetDX11*)m_rendererDX11.m_mirrorRT.get();
        mirrorRt->BindAsTexture(0);
		waterData.ScreenToRTUV = { 1.f / m_rendererDX11.m_mainViewport.Width, 1.f / m_rendererDX11.m_mainViewport.Height, -1.f, 1.f }; // 1-u for the mirrored texture
    }
    else
    {
        m_d3dContext->PSSetShaderResources(0, 1, m_rendererDX11.m_frameBufferCopy.m_nativeTexView.GetAddressOf());
        waterData.ScreenToRTUV = { 1.f / m_rendererDX11.m_mainViewport.Width, 1.f / m_rendererDX11.m_mainViewport.Height, 1.f, 0.f };
    }

	m_previousTexture = nullptr;

    // shaders for rendering water instances
    m_d3dContext->VSSetShader(m_rendererDX11.m_vertexShaderMirrorReceiver.Get(), nullptr, 0);
    m_d3dContext->PSSetShader(m_rendererDX11.m_pixelShaderMirrorReceiver.Get(), nullptr, 0);
    m_d3dContext->PSSetSamplers(1, 1, m_rendererDX11.m_samplerStateClamp.GetAddressOf());

    waterData.Uv1Scroll = water.m_scrollParams1;
    waterData.Uv2Scroll = water.m_scrollParams2;
//    waterData.TintColor = { 1,1,1,1 };
    //static float strength = 1.0f;
    //waterData.DistortionStrength = 1.f;
    m_d3dContext->UpdateSubresource1(m_rendererDX11.m_constantBufferWaterParams.Get(), 0, nullptr, &waterData, 0, 0, 0);

    m_d3dContext->VSSetConstantBuffers1(6, 1, m_rendererDX11.m_constantBufferWaterParams.GetAddressOf(), nullptr, nullptr);
    m_d3dContext->PSSetConstantBuffers1(6, 1, m_rendererDX11.m_constantBufferWaterParams.GetAddressOf(), nullptr, nullptr); // also need atlas data for PS

    // normalmap
    m_d3dContext->PSSetShaderResources(1, 1, m_rendererDX11.m_mirrorNormalMap.m_nativeTexView.GetAddressOf());

}

void RenderContextDX11::RenderPostEffects()
{
    if (m_elements.m_visibileCoronas.size())
    {
        int coronaCount = (int)m_elements.m_visibileCoronas.size();

        // track corona array
        const D3D11_BOX updateRange = { 0, 0, 0, coronaCount * sizeof(PackedLightSourceParams), 1, 1 };
        m_d3dContext->UpdateSubresource1(m_rendererDX11.m_constantBufferLights.Get(), 0, &updateRange, m_elements.m_visibileCoronas.data(), 0, 0, D3D11_COPY_DISCARD);

        RenderTargetDX11* renderTarget = m_rendererDX11.GetRenderTarget<RenderTargetDX11>(m_renderTargetPass);
        renderTarget->BindAsTarget(false); // without z-buffer

        NativeModelMeshObjectDX11* nativeMesh = static_cast<NativeModelMeshObjectDX11*>(m_rendererDX11.m_coronaMesh.get());

        // blending/depth states
        m_d3dContext->OMSetDepthStencilState(m_rendererDX11.m_depthStateCorona.Get(), 1);
        SetBlendState(TransparencyMode::Additive);

        // position
        Matrix4 identity;
        Renderer::InstanceData& inst(m_rendererDX11.m_instanceData);
        inst.m_modelToWSMat.SetTransposed(identity);
        inst.m_uniformScaling = 1.f;
        inst.m_vertexOfs = inst.m_vertexOfs2 = 0;
        m_d3dContext->UpdateSubresource1(m_rendererDX11.m_constantBufferInstance.Get(), 0, nullptr, &inst, 0, 0, 0);

        // bind mesh
        UINT stride = sizeof(ModelVertex);
        UINT offset = 0;
        m_d3dContext->IASetVertexBuffers(0, 1, nativeMesh->m_vertexBuffer.GetAddressOf(), &stride, &offset);
        m_d3dContext->IASetIndexBuffer(nativeMesh->m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0); // no index buffer
        m_previousMesh = nativeMesh;

        // shader
        m_d3dContext->VSSetShader(m_rendererDX11.m_vertexShaderCorona.Get(), nullptr, 0);
        m_d3dContext->PSSetShader(m_rendererDX11.m_pixelShaderCorona.Get(), nullptr, 0);
        m_d3dContext->GSSetShader(m_rendererDX11.m_geometryShaderCorona.Get(), nullptr, 0);

        // corona texture #1 : from file
        m_d3dContext->PSSetShaderResources(0, 1, m_rendererDX11.m_coronaTexture.m_nativeTexView.GetAddressOf());

        // corona texture #2 (in fog):
        m_d3dContext->PSSetShaderResources(1, 1, m_rendererDX11.m_coronaTextureFog.m_nativeTexView.GetAddressOf());
        /*
        Texture* coronaFogTex = AssetManager::Instance().GetCoronaTexture();
        NativeTextureObjectDX11* nativeTex = coronaFogTex->GetRuntime<NativeTextureObjectDX11>();
        assert(nativeTex != nullptr);
        m_d3dContext->PSSetShaderResources(1, 1, nativeTex->m_nativeTexView.GetAddressOf());
        */
        m_d3dContext->PSSetSamplers(0, 1, m_rendererDX11.m_samplerStateClamp.GetAddressOf());
        m_previousTexture = nullptr;

        // depth texture (VS)
        m_d3dContext->VSSetShaderResources(0, 1, renderTarget->m_shaderResourceViewDS.GetAddressOf());

        // draw points
        m_d3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
        m_profiling.m_numDrawCalls++;
        m_profiling.m_numTriangles += coronaCount * 2;
        if (m_isStereo)
        {
            m_d3dContext->DrawInstanced(coronaCount, 2, 0, 0);
        }
        else
        {
            m_d3dContext->Draw(coronaCount, 0);
        }

        // set back:
        m_d3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_d3dContext->GSSetShader(nullptr, nullptr, 0);

		ID3D11ShaderResourceView* nullView = nullptr;
		m_d3dContext->VSSetShaderResources(0, 1, &nullView);
    }
}
#endif // ENABLE_ADVANCED_RENDERING

