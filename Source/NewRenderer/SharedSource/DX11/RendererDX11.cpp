#include "pch.h"
#include <DX11/RendererDX11.h>
#include <DX11/RenderContextDX11.h>
#include <IO/FileManager.h>
#include <Renderer/Camera.h>
#include <Renderer/Renderable.h>
#include <Renderer/RenderableInstance.h>
#include <Renderer/IGraphicsRuntimeObject.h>
#include <Asset/AssetManager.h>
#include <Asset/Texture.h>
#include <Asset/ModelGeometry.h>
#include <Asset/ModelAnim.h>
#include <Scene/Scene.h>
#include <Renderer/TextMesh.h>
#include <cmath>
#include <stdexcept>

void RendererDX11::ReleaseWindowSizeDependentResources()
{
    if (m_mainRT != nullptr)
    {
        static_cast<RenderTargetDX11*>(m_mainRT.get())->Reset();
    }
#ifdef ENABLE_ADVANCED_RENDERING
    m_frameBufferCopy.Reset();
#endif
}

void RendererDX11::ReInitialize(ID3D11RenderTargetView1* colorView, ID3D11DepthStencilView* dsView, const D3D11_VIEWPORT& viewport)
{
	RenderTargetDX11* mainRT = (RenderTargetDX11*)m_mainRT.get();
#ifdef ENABLE_ADVANCED_RENDERING
	if (mainRT->m_width != static_cast<int>(viewport.Width) || mainRT->m_height != static_cast<int>(viewport.Height))
	{
		m_frameBufferCopy.Reset();
	}
#endif
	mainRT->Create(m_d3dDevice.Get(), m_d3dContext.Get(), colorView, dsView, viewport);
	m_mainViewport = viewport;

#ifdef ENABLE_ADVANCED_RENDERING
    if (mainRT->m_width > 0 && m_frameBufferCopy.m_nativeTex == nullptr)
    {
        CD3D11_TEXTURE2D_DESC descCopy(mainRT->m_colorFormat, m_mainRT->m_width, m_mainRT->m_height, 1, 1, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0);
		if (descCopy.Format == DXGI_FORMAT_R8G8B8A8_TYPELESS)
		{
            descCopy.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		}
        ThrowIfFailed(m_d3dDevice->CreateTexture2D(&descCopy, nullptr, &m_frameBufferCopy.m_nativeTex));
        CD3D11_SHADER_RESOURCE_VIEW_DESC viewDesc(D3D11_SRV_DIMENSION_TEXTURE2D, descCopy.Format);
        ThrowIfFailed(m_d3dDevice->CreateShaderResourceView(m_frameBufferCopy.m_nativeTex.Get(), &viewDesc, &m_frameBufferCopy.m_nativeTexView));
    }
#endif
}


FUTURE(void) RendererDX11::Initialize(ID3D11Device3* device, ID3D11DeviceContext3* context, ID3D11RenderTargetView1* colorView, ID3D11DepthStencilView* dsView, const D3D11_VIEWPORT& viewport)
{
    g_Instance = this;

    m_d3dDevice = device;
    m_d3dContext = context;
    m_mainViewport = viewport;

    // retrieve depth texture and create another resource view
    {
        m_mainRT = std::make_unique<RenderTargetDX11>();
        RenderTargetDX11* mainRT = (RenderTargetDX11 *)m_mainRT.get();
        mainRT->Create(device, context, colorView, dsView, viewport);
        mainRT->m_performClear = false;
    }

    RenderTargetDX11* mainRT = this->GetRenderTarget<RenderTargetDX11>(RenderTargetPassType::MainView);

    // default buffers:
    CD3D11_BUFFER_DESC cbGlobalsDesc(sizeof(GlobalsData), D3D11_BIND_CONSTANT_BUFFER);
    ThrowIfFailed(device->CreateBuffer(&cbGlobalsDesc, nullptr, &m_constantBufferGlobals));

    CD3D11_BUFFER_DESC cbInstanceDesc(sizeof(InstanceData), D3D11_BIND_CONSTANT_BUFFER);
    ThrowIfFailed(device->CreateBuffer(&cbInstanceDesc, nullptr, &m_constantBufferInstance));

    CD3D11_BUFFER_DESC cbMaterialDesc(sizeof(InstanceMaterialData), D3D11_BIND_CONSTANT_BUFFER);
    ThrowIfFailed(device->CreateBuffer(&cbMaterialDesc, nullptr, &m_constantBufferMaterial));

#ifdef ENABLE_ADVANCED_RENDERING
    {
        CD3D11_BUFFER_DESC cbLightListDesc(sizeof(PackedLightSourceParams) * MaxVisibleLights, D3D11_BIND_CONSTANT_BUFFER);
        ThrowIfFailed(device->CreateBuffer(&cbLightListDesc, nullptr, &m_constantBufferLights));

        // fill with data initially:
        PackedLightSourceParams params[MaxVisibleLights] = {};
        m_d3dContext->UpdateSubresource1(m_constantBufferLights.Get(), 0, nullptr, params, 0, 0, 0);

        CD3D11_BUFFER_DESC cbWaterDesc(sizeof(WaterShaderConstants), D3D11_BIND_CONSTANT_BUFFER);
        ThrowIfFailed(device->CreateBuffer(&cbWaterDesc, nullptr, &m_constantBufferWaterParams));
        
        m_mirrorRT = std::make_unique<RenderTargetDX11>();
        RenderTargetDX11* mirrorRt = (RenderTargetDX11*)m_mirrorRT.get();
        int sizeX = 2048;
        int sizeY = 2048;
        mirrorRt->Create(m_d3dDevice.Get(), m_d3dContext.Get(), sizeX, sizeY);

        CO_AWAIT m_mirrorNormalMap.LoadFromDDS(device, L"textures\\water-normal.dds");
        CO_AWAIT m_coronaTexture.LoadFromDDS(device, L"textures\\mainlens.dds");
        CO_AWAIT m_coronaTextureFog.LoadFromDDS(device, L"textures\\flare13.dds");

        // framebuffer copy for refraction effects
        if (m_mainRT->m_width > 0)
        {
            CD3D11_TEXTURE2D_DESC descCopy(mainRT->m_colorFormat, m_mainRT->m_width, m_mainRT->m_height, 1, 1, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0);
			if (descCopy.Format == DXGI_FORMAT_R8G8B8A8_TYPELESS)
			{
				descCopy.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			}
            ThrowIfFailed(m_d3dDevice->CreateTexture2D(&descCopy, nullptr, &m_frameBufferCopy.m_nativeTex));
            CD3D11_SHADER_RESOURCE_VIEW_DESC viewDesc(D3D11_SRV_DIMENSION_TEXTURE2D, descCopy.Format);
            ThrowIfFailed(m_d3dDevice->CreateShaderResourceView(m_frameBufferCopy.m_nativeTex.Get(), &viewDesc, &m_frameBufferCopy.m_nativeTexView));
        }
    }
#endif

    CD3D11_SAMPLER_DESC samplerDescWrap(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_WRAP, 0.0f, 16, D3D11_COMPARISON_ALWAYS, nullptr, 0.f, D3D11_FLOAT32_MAX);
    ThrowIfFailed(device->CreateSamplerState(&samplerDescWrap, &m_samplerStateWrap));
    CD3D11_SAMPLER_DESC samplerDescClamp(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_CLAMP, 0.0f, 16, D3D11_COMPARISON_ALWAYS, nullptr, 0.f, D3D11_FLOAT32_MAX);
    ThrowIfFailed(device->CreateSamplerState(&samplerDescClamp, &m_samplerStateClamp));

    // Load shaders asynchronously. vsData is needed later for layout
    std::vector<byte> vsData = CO_AWAIT FileManager::Instance().ReadDataFuture(L"StandardGeometry_VS.cso");
    std::vector<byte> psData = CO_AWAIT FileManager::Instance().ReadDataFuture(L"StandardGeometry_PS.cso");
    ThrowIfFailed(device->CreateVertexShader(&vsData[0], vsData.size(), nullptr, &m_vertexShader));
    ThrowIfFailed(device->CreatePixelShader(&psData[0], psData.size(), nullptr, &m_pixelShader));

    {
        std::vector<byte> vsDataF = CO_AWAIT FileManager::Instance().ReadDataFuture(L"InFlames_VS.cso");
        std::vector<byte> psDataF = CO_AWAIT FileManager::Instance().ReadDataFuture(L"InFlames_PS.cso");
        ThrowIfFailed(device->CreateVertexShader(&vsDataF[0], vsDataF.size(), nullptr, &m_vertexShaderFlames));
        ThrowIfFailed(device->CreatePixelShader(&psDataF[0], psDataF.size(), nullptr, &m_pixelShaderFlames));
    }
    // create additional shader
#ifdef ENABLE_ADVANCED_RENDERING
    std::vector<byte> vsDataMR = CO_AWAIT FileManager::Instance().ReadDataFuture(L"MirrorReceiver_VS.cso");
    std::vector<byte> psDataMR = CO_AWAIT FileManager::Instance().ReadDataFuture(L"MirrorReceiver_PS.cso");
    std::vector<byte> vsDataCp = CO_AWAIT FileManager::Instance().ReadDataFuture(L"Corona_VS.cso");
    std::vector<byte> psDataCo = CO_AWAIT FileManager::Instance().ReadDataFuture(L"Corona_PS.cso");
    std::vector<byte> gsDataCo = CO_AWAIT FileManager::Instance().ReadDataFuture(L"Corona_GS.cso");

    ThrowIfFailed(device->CreateVertexShader(&vsDataMR[0], vsDataMR.size(), nullptr, &m_vertexShaderMirrorReceiver));
    ThrowIfFailed(device->CreatePixelShader(&psDataMR[0], psDataMR.size(), nullptr, &m_pixelShaderMirrorReceiver));
    ThrowIfFailed(device->CreatePixelShader(&psDataCo[0], psDataCo.size(), nullptr, &m_pixelShaderCorona));
    ThrowIfFailed(device->CreateVertexShader(&vsDataCp[0], vsDataCp.size(), nullptr, &m_vertexShaderCorona));
    ThrowIfFailed(device->CreateGeometryShader(&gsDataCo[0], gsDataCo.size(), nullptr, &m_geometryShaderCorona));
#endif

    static const D3D11_INPUT_ELEMENT_DESC vertexDesc[] =
    {
#ifdef ENABLE_ADVANCED_RENDERING
        { "NORMAL"  , 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,    offsetof(ModelVertex,Normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },  // float3 Normal
#endif
#ifdef USE_VERTEX_COLOR
        { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0,        offsetof(ModelVertex,Color), D3D11_INPUT_PER_VERTEX_DATA, 0 },   // Color4 Color
#endif
        { "TEXCOORD", 0, DXGI_FORMAT_R16G16B16A16_UINT, 0,  offsetof(ModelVertex,PositionIndex), D3D11_INPUT_PER_VERTEX_DATA, 0 },   // 4x uint16
        { "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0,       offsetof(ModelVertex,TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0 },   // float2 TexCoord
        { "TEXCOORD", 2, DXGI_FORMAT_R32G32_FLOAT, 0,       offsetof(ModelVertex,StretchParams), D3D11_INPUT_PER_VERTEX_DATA, 0 },   // float2 StretchParams
    };

    ThrowIfFailed(device->CreateInputLayout(vertexDesc, ARRAYSIZE(vertexDesc), &vsData[0], vsData.size(), &m_inputLayout));
    CO_AWAIT m_textShader.Create(device);

    // create blend states:
    CD3D11_BLEND_DESC blendDesc(D3D11_DEFAULT);
    D3D11_RENDER_TARGET_BLEND_DESC& rt = blendDesc.RenderTarget[0];
    rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    rt.BlendOp = rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    rt.SrcBlend = D3D11_BLEND_ONE;
    rt.DestBlend = D3D11_BLEND_ZERO;

    ThrowIfFailed(device->CreateBlendState(&blendDesc, &m_blendState[(int)TransparencyMode::Opaque]));
    rt.BlendEnable = TRUE;
    rt.SrcBlend = rt.SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
    rt.DestBlend = rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    ThrowIfFailed(device->CreateBlendState(&blendDesc, &m_blendState[(int)TransparencyMode::AlphaBlended]));
	rt.SrcBlend = rt.SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
    rt.DestBlend = rt.DestBlendAlpha = D3D11_BLEND_ONE;
    ThrowIfFailed(device->CreateBlendState(&blendDesc, &m_blendState[(int)TransparencyMode::Additive]));

    m_blendState[(int)TransparencyMode::SkyOpaque] = m_blendState[(int)TransparencyMode::Opaque];
    m_blendState[(int)TransparencyMode::SkyAlphaBlended] = m_blendState[(int)TransparencyMode::AlphaBlended];
    m_blendState[(int)TransparencyMode::SkyAdditive] = m_blendState[(int)TransparencyMode::Additive];

    // they are all opaque:
    m_blendState[(int)TransparencyMode::Water] = m_blendState[(int)TransparencyMode::Opaque];

    // create depth write states:
    for (int i = 0; i < (int)DepthWriteMode::DepthModeCount; i++)
    {
        CD3D11_DEPTH_STENCIL_DESC dsDesc(D3D11_DEFAULT);
        dsDesc.DepthWriteMask = ((i & (int)DepthWriteMode::DepthWriteBit) > 0) ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
        dsDesc.DepthFunc = ((i & (int)DepthWriteMode::DepthTestBit) > 0) ? D3D11_COMPARISON_LESS_EQUAL : D3D11_COMPARISON_ALWAYS;
        ThrowIfFailed(device->CreateDepthStencilState(&dsDesc, &m_depthState[i]));
    }

#ifdef ENABLE_ADVANCED_RENDERING
    CD3D11_DEPTH_STENCIL_DESC dsDescCorona(D3D11_DEFAULT);
    dsDescCorona.DepthEnable = FALSE;
    dsDescCorona.DepthFunc = D3D11_COMPARISON_ALWAYS;
    dsDescCorona.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    ThrowIfFailed(device->CreateDepthStencilState(&dsDescCorona, &m_depthStateCorona)); // disabled

    // mesh for coronas
    CreateCoronaMesh();
#endif
    // renderer is ready when everything is compiled
    m_isReady = true;
}

void RendererDX11::Deinitialize()
{
    m_itemPreview.reset();
    m_d3dDevice.Reset();
    m_d3dContext.Reset();
    m_inputLayout.Reset();
    m_constantBufferGlobals.Reset();
    m_constantBufferInstance.Reset();
    m_constantBufferMaterial.Reset();
	m_vertexShader.Reset();
	m_pixelShader.Reset();
	m_textShader.Deinitialize();
	m_vertexShaderFlames.Reset();
	m_pixelShaderFlames.Reset();
    m_samplerStateWrap.Reset();
    m_samplerStateClamp.Reset();
    for (int i = 0; i < (int)DepthWriteMode::DepthModeCount; i++)
        m_depthState[i].Reset();

    m_mirrorRT = nullptr;
    m_mainRT = nullptr;
#ifdef ENABLE_ADVANCED_RENDERING
    m_vertexShaderMirrorReceiver.Reset();
    m_pixelShaderMirrorReceiver.Reset();
    m_vertexShaderCorona.Reset();
    m_pixelShaderCorona.Reset();
    m_geometryShaderCorona.Reset();
    m_mirrorNormalMap.Reset();
    m_depthStateCorona.Reset();
    m_coronaTexture.Reset();
    m_coronaTextureFog.Reset();

    m_coronaMesh = nullptr;
#endif
}


void RendererDX11::RenderSceneElements(const VisibilityResult& elements, const RenderingSettings& renderSettings, ProfilingStats& stats)
{
    RenderScene_Impl<RenderContextDX11>(elements, renderSettings, stats);
}

void RendererDX11::SetItemPreview(std::shared_ptr<const RenderableInstance> instance, bool isSprite, const D3D11_VIEWPORT& viewport)
{
    if (instance != nullptr && (!instance->IsValid() ||
        !std::isfinite(viewport.TopLeftX) || !std::isfinite(viewport.TopLeftY) ||
        !std::isfinite(viewport.Width) || !std::isfinite(viewport.Height) ||
        viewport.TopLeftX < m_mainViewport.TopLeftX || viewport.TopLeftY < m_mainViewport.TopLeftY ||
        viewport.Width <= 0.f || viewport.Height <= 0.f ||
        viewport.TopLeftX + viewport.Width > m_mainViewport.TopLeftX + m_mainViewport.Width ||
        viewport.TopLeftY + viewport.Height > m_mainViewport.TopLeftY + m_mainViewport.Height))
    {
        throw std::invalid_argument("Item preview requires a renderable and a finite rectangle inside the main viewport.");
    }
    m_itemPreview = std::move(instance);
    m_itemPreviewIsSprite = isSprite;
    m_itemPreviewViewport = viewport;
    m_itemPreviewViewport.MinDepth = 0.f;
    m_itemPreviewViewport.MaxDepth = 1.f;
}

void RendererDX11::RenderHudGeometry(const RenderingSettings& settings, ProfilingStats& stats)
{
    const auto item = std::move(m_itemPreview);
    if (item == nullptr) { return; }
    const auto& viewport = m_itemPreviewViewport;
    VisibilityResult elements;
    if (m_itemPreviewIsSprite)
    {
        const auto& materials = item->GetMaterialOverrides();
        const Texture* texture = materials.empty() ?
            item->GetRenderable()->m_drawCalls.at(0).m_material.m_texture : materials[0].m_texture;
        if (texture == nullptr || texture->m_width == 0 || texture->m_height == 0)
        {
            throw std::runtime_error("Pickup sprite preview has no texture.");
        }
        const float scale = (std::min)(viewport.Width / texture->m_width, viewport.Height / texture->m_height);
        const float width = texture->m_width * scale;
        const float height = texture->m_height * scale;
        TextMeshInstance sprite(TextMesh::CreateTexturedQuad(*texture, width, height));
        sprite.SetScale(1.f);
        sprite.SetPosition(viewport.TopLeftX + (viewport.Width - width) * 0.5f,
            viewport.TopLeftY + (viewport.Height - height) * 0.5f);
        sprite.SetTintColor(item->m_tintColor);
        RenderContextDX11 context(*this, elements, settings, stats);
        context.RenderTextMeshes({ &sprite });
        return;
    }

    RenderableInstance preview = *item;
    const auto setPreviewLighting = [](RenderableInstance& instance)
    {
        instance.m_tintColor = { 255,255,255,instance.m_tintColor.a };
        for (auto& material : instance.GetOrCreateMaterialOverrides())
        {
            material.m_applyLighting = false;
            material.ComputeHash();
        }
    };
    setPreviewLighting(preview);
    for (auto& sub : preview.m_subInstances) { setPreviewLighting(sub); }
    BoundingVolume bounds = preview.GetRenderable()->m_localBounds;
    if (const auto* blendTarget = preview.GetBlendTargetRenderable()) { bounds.Add(blendTarget->m_localBounds); }
    bounds.OnUpdated();
    const float radius = bounds.GetSphereRadius();
    if (!std::isfinite(radius) || radius <= 0.f)
    {
        throw std::runtime_error("Pickup model preview requires nonempty finite model bounds.");
    }
    preview.m_uniformScale = 1.f / radius;
    for (auto& sub : preview.m_subInstances) { sub.m_uniformScale /= radius; }
    Matrix4 center, rotation, tilt, placement;
    const Vector& pivot = bounds.GetCenter();
    center.SetTranslation(Vector(-pivot.GetX(), -pivot.GetY(), -pivot.GetZ()));
    rotation.Create(Vector(), Vector(static_cast<float>(std::fmod(m_currentTime * 0.8, 2.0 * Math::Pi)), 0.f, 0.f), 1.f / radius);
    tilt.Create(Vector(), Vector(0.f, 0.3f, 0.f));
    placement.SetTranslation(Vector(0.f, 0.f, -3.f));
    preview.SetTransform(center * rotation * tilt * placement);
    preview.PrepareSubInstancesForRendering();
    const auto addInstance = [&elements](const RenderableInstance& instance)
    {
        if (!instance.GetRenderable()->HasOwnDrawcalls()) { return; }
        if (instance.HasOpaqueMaterials()) { elements.m_visibleInstances.m_opaqueInstances.push_back({ &instance,0 }); }
        if (instance.HasTransparentMaterials()) { elements.m_visibleInstances.m_transparentInstances.push_back({ &instance,0 }); }
        elements.m_visibleInstances.m_visibleTransparencyTypes |= instance.GetUsedTransparencyMask();
    };
    addInstance(preview);
    for (const auto& sub : preview.m_subInstances) { addInstance(sub); }
    Camera camera;
    Matrix4 projection;
    const float aspect = viewport.Width / viewport.Height;
    projection.CreateOrthographic(2.4f * (std::max)(1.f, aspect), 2.4f * (std::max)(1.f, 1.f / aspect), 0.1f, 10.f);
    camera.SetProjectionMatrix(projection);
    elements.m_mainCamera = &camera;
    elements.m_ambientColor = { 1.f,1.f,1.f };
    elements.m_ambientNrmlContribution = 0.f;
    elements.m_fogFalloff = 0.f;
    elements.m_lightmap = AssetManager::Instance().GetPlainWhiteTexture();
    RenderContextDX11 context(*this, elements, settings, stats);
    context.BeginRendering();
    context.BeginRenderTargetScope(RenderTargetPassType::MainView);
    context.TrackGlobals();
    auto* target = GetRenderTarget<RenderTargetDX11>(RenderTargetPassType::MainView);
    // No world-depth consumers remain. Keep the world color and use fresh depth for self-occlusion.
    m_d3dContext->ClearDepthStencilView(target->m_depthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.f, 0);
    m_d3dContext->RSSetViewports(1, &viewport);
    RenderOpaqueGeometry(context, elements.m_visibleInstances);
    RenderTransparentGeometry(context, elements.m_visibleInstances);
    context.EndRenderTargetScope();
    context.EndRendering();
    target->BindAsTarget(false);
}

void RendererDX11::CreateAtlasDataCB(Texture& tex, NativeTextureObjectDX11& runtime)
{
    static_assert(sizeof(tex.m_atlasData) + sizeof(tex.m_spriteData) == sizeof(TextureAtlasCBData));
    CD3D11_BUFFER_DESC cbAtlasDataDesc(sizeof(TextureAtlasCBData), D3D11_BIND_CONSTANT_BUFFER);
    TextureAtlasCBData data;
    tex.m_atlasData.Store(data.m_atlasTransform);
    tex.m_spriteData.Store(data.m_spriteParams);

    ThrowIfFailed(m_d3dDevice->CreateBuffer(&cbAtlasDataDesc, nullptr, &runtime.m_atlasDataBuffer));
    m_d3dContext->UpdateSubresource1(runtime.m_atlasDataBuffer.Get(), 0, nullptr, &data, 0, 0, 0);

}

#ifdef ENABLE_ADVANCED_RENDERING
void RendererDX11::CreateCoronaMesh()
{
    ModelLoadingContext context;
    ModelConversionContext conversion(context);

    const int maxLights = MaxVisibleLights;
    conversion.m_vertices.resize(maxLights);
//    conversion.m_sortedIndices.resize(maxLights * 6);
    ModelVertex* v = conversion.m_vertices.data();
//    uint16_t* idx = conversion.m_sortedIndices.data();
    float width = 1.f;
    Color4 col = { 255,255,255,255 };
    for (int i = 0; i < maxLights; i++, v ++)
    {
        uint16_t vertexStart = i * 4;
        v[0].SetSprite(i, col, Vector2{ 0, 0 }, width, false); // left/bottom
    }

    m_coronaMesh = CreateModelBuffers(conversion);
}

void RendererDX11::CopyFrameBuffer()
{
    RenderTargetDX11* mainRT = GetRenderTarget<RenderTargetDX11>(RenderTargetPassType::MainView);
    assert(m_frameBufferCopy.m_nativeTex != nullptr && mainRT != nullptr && mainRT->m_renderTargetTexture != nullptr);
    m_d3dContext->CopyResource(m_frameBufferCopy.m_nativeTex.Get(), mainRT->m_renderTargetTexture.Get());
}

#endif

void RendererDX11::CreateTexture(Texture& tex, TextureLoadingContext& data)
{
    CD3D11_TEXTURE2D_DESC desc(DXGI_FORMAT_R8G8B8A8_UNORM, tex.m_width, tex.m_height, 1, 1, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, D3D11_CPU_ACCESS_WRITE);

    std::shared_ptr<NativeTextureObjectDX11> native = std::make_shared<NativeTextureObjectDX11>();
    native->m_Id = m_runtimeIdCounter.fetch_add(1);
    tex.m_runtime = native;

    D3D11_SUBRESOURCE_DATA initialData;
    memset(&initialData, 0, sizeof(initialData));
    initialData.pSysMem = data.m_rgbaData.data();
    initialData.SysMemPitch = tex.m_width * 4;
    initialData.SysMemSlicePitch = initialData.SysMemPitch * tex.m_height;

    ThrowIfFailed(m_d3dDevice->CreateTexture2D(&desc, &initialData, &native->m_nativeTex));

    CD3D11_SHADER_RESOURCE_VIEW_DESC viewDesc(D3D11_SRV_DIMENSION_TEXTURE2D, desc.Format);
    ThrowIfFailed(m_d3dDevice->CreateShaderResourceView(native->m_nativeTex.Get(), &viewDesc, &native->m_nativeTexView));

    CreateAtlasDataCB(tex, *native);

    if (!data.m_highResFilename.empty())
    {
		std::shared_ptr<NativeTextureObjectDX11> hrnative = std::make_shared<NativeTextureObjectDX11>();
        hrnative->m_Id = m_runtimeIdCounter.fetch_add(1);
        hrnative->LoadFromDDS(m_d3dDevice.Get(), data.m_highResFilename);
        hrnative->m_atlasDataBuffer = native->m_atlasDataBuffer; // share atlas data
		tex.m_runtimeHighres = hrnative;
    }
}

void RendererDX11::CloneTextureRuntime(Texture& tex, Texture& sourcetex)
{
    std::shared_ptr<NativeTextureObjectDX11> runtime = std::make_shared<NativeTextureObjectDX11>();
    runtime->m_Id = m_runtimeIdCounter.fetch_add(1);
    tex.m_runtime = runtime;

    NativeTextureObjectDX11* srcRuntime = sourcetex.GetNativeRuntimeObj<NativeTextureObjectDX11>();
    assert(srcRuntime != nullptr);
    runtime->m_nativeTex = srcRuntime->m_nativeTex;
    runtime->m_nativeTexView = srcRuntime->m_nativeTexView;

    CreateAtlasDataCB(tex, *runtime);
}

std::shared_ptr<IGraphicsRuntimeObject> RendererDX11::CreateModelBuffers(ModelConversionContext& data)
{
    std::shared_ptr<NativeModelMeshObjectDX11> native = std::make_shared<NativeModelMeshObjectDX11>();
    native->m_Id = m_runtimeIdCounter.fetch_add(1);
#ifdef USE_STRUCTURED_BUFFER
    // structured buffer for positions only
    D3D11_SUBRESOURCE_DATA positionBufferData = { 0 };
    positionBufferData.pSysMem = data.m_input.m_vertexPos.data();
    positionBufferData.SysMemPitch = 0;
    positionBufferData.SysMemSlicePitch = 0;
    CD3D11_BUFFER_DESC positionBufferDesc(sizeof(Vector4) * (int)data.m_input.m_vertexPos.size(), D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE);
    ThrowIfFailed(m_d3dDevice->CreateBuffer(&positionBufferDesc, &positionBufferData, &native->m_positionBuffer));

    D3D11_SHADER_RESOURCE_VIEW_DESC positionViewDesc = { };
    positionViewDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
    positionViewDesc.BufferEx.FirstElement = 0;
    positionViewDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
    positionViewDesc.BufferEx.NumElements = (int)data.m_input.m_vertexPos.size();
    ThrowIfFailed(m_d3dDevice->CreateShaderResourceView(native->m_positionBuffer.Get(), &positionViewDesc, &native->m_positionBufferView));
#else
    if (data.m_input.m_vertexPos.size() > 0)
    {
		if (data.m_input.m_vertexPos.size() == 1) // just avoid debug warnings
		{
            assert(false);
		}

        CD3D11_BUFFER_DESC cbPositionDesc(sizeof(data.m_input.m_vertexPos[0]) * (int)data.m_input.m_vertexPos.size(), D3D11_BIND_CONSTANT_BUFFER);
        ThrowIfFailed(m_d3dDevice->CreateBuffer(&cbPositionDesc, nullptr, &native->m_positionBuffer));
        m_d3dContext->UpdateSubresource1(native->m_positionBuffer.Get(), 0, nullptr, data.m_input.m_vertexPos.data(), 0, 0, 0);
    }
#endif

    // render vertices
    D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
    vertexBufferData.pSysMem = data.m_vertices.data();
    vertexBufferData.SysMemPitch = 0;
    vertexBufferData.SysMemSlicePitch = 0;
    CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(data.m_vertices[0]) * (int)data.m_vertices.size(), D3D11_BIND_VERTEX_BUFFER);
    ThrowIfFailed(m_d3dDevice->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &native->m_vertexBuffer));

    // index buffer
    if (data.m_sortedIndices.size() > 0)
    {
        D3D11_SUBRESOURCE_DATA indexBufferData = { 0 };
        indexBufferData.pSysMem = data.m_sortedIndices.data();
        indexBufferData.SysMemPitch = 0;
        indexBufferData.SysMemSlicePitch = 0;
        CD3D11_BUFFER_DESC indexBufferDesc(sizeof(data.m_sortedIndices[0]) * (int)data.m_sortedIndices.size(), D3D11_BIND_INDEX_BUFFER);
        ThrowIfFailed(m_d3dDevice->CreateBuffer(&indexBufferDesc, &indexBufferData, &native->m_indexBuffer));
    }
    return native;
}

std::shared_ptr<IGraphicsRuntimeObject> RendererDX11::CreateTextMeshBuffers(const TextMesh& textMesh)
{
    auto native = std::make_shared<NativeTextMeshObjectDX11>();
    native->m_Id = m_runtimeIdCounter.fetch_add(1);

    for (const TextMeshBatch& batch : textMesh.GetBatches())
    {
        if (batch.m_vertices.empty() || batch.m_indices.empty())
        {
            continue;
        }

        NativeTextMeshBatchDX11& nativeBatch = native->m_batches.emplace_back();
        nativeBatch.m_atlasTexture = batch.m_atlasTexture;
        nativeBatch.m_useTextureColor = batch.m_useTextureColor;
        nativeBatch.m_indexCount = static_cast<UINT>(batch.m_indices.size());

        D3D11_SUBRESOURCE_DATA vertexData = {};
        vertexData.pSysMem = batch.m_vertices.data();
        const CD3D11_BUFFER_DESC vertexDesc(static_cast<UINT>(batch.m_vertices.size() * sizeof(TextVertex)), D3D11_BIND_VERTEX_BUFFER);
        ThrowIfFailed(m_d3dDevice->CreateBuffer(&vertexDesc, &vertexData, &nativeBatch.m_vertexBuffer));

        D3D11_SUBRESOURCE_DATA indexData = {};
        indexData.pSysMem = batch.m_indices.data();
        const CD3D11_BUFFER_DESC indexDesc(static_cast<UINT>(batch.m_indices.size() * sizeof(uint32_t)), D3D11_BIND_INDEX_BUFFER);
        ThrowIfFailed(m_d3dDevice->CreateBuffer(&indexDesc, &indexData, &nativeBatch.m_indexBuffer));
    }

    return native;
}

void RendererDX11::ConfigureVisibility(IVisibilityContext& visContext)
{
    auto& limits = visContext.AccessLimits();
    limits.MaxVisibleLights = MaxVisibleLights;
}
