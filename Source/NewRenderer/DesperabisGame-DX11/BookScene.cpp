#include "pch.h"
#include "BookScene.h"

#include <Asset/AssetManager.h>
#include <Renderer/Renderer.h>
#include <Renderer/Visibility.h>
#include <Renderer/LightSource.h>
#include <cmath>
#include <stdexcept>

FUTURE(void) BookScene::CreateScene()
{
    ModelLoadingContext positions;
    positions.m_vertexPos = {{-3.6f,1.2f,0,1}, {0,1.2f,0,1},
        {-3.6f,-1.2f,0,1}, {0,-1.2f,0,1}};
    ModelConversionContext geometry(positions);
    const Vector2 uv[]{{0,0}, {1,0}, {0,1}, {1,1}};
    for (int face = 0; face < 2; ++face)
    {
        for (std::uint16_t i = 0; i < 4; ++i)
        {
            ModelVertex vertex{};
            vertex.SetTextured(i, {255,255,255,0}, uv[i]);
#ifdef ENABLE_ADVANCED_RENDERING
            // The lighting shader expects inward-facing native-model normals.
            vertex.Normal = {0,0,face == 0 ? -1.f : 1.f};
#endif
            geometry.m_vertices.push_back(vertex);
            geometry.AddToBounds(positions.m_vertexPos[i], 0);
        }
    }
    geometry.m_sortedIndices = {0,1,2,2,1,3, 4,6,5,6,7,5};
    m_pageMesh = std::make_shared<Renderable>();
    m_pageMesh->m_runtime = Renderer::Instance().CreateModelBuffers(geometry);
    geometry.GenerateBoundingVolume(m_pageMesh->m_localBounds);
    for (int face = 0; face < 2; ++face)
    {
        DrawCall draw;
        draw.m_firstIndex = face * 6;
        draw.m_primitiveCount = 2;
        draw.m_material.m_useAlphaTest = false;
        draw.m_material.ComputeHash();
        m_pageMesh->m_drawCalls.push_back(draw);
    }
    for (auto& page : m_pages)
    {
        AssetManager::Instance().RegisterAsset(page);
        page.m_width = 360;
        page.m_height = 240;
        page.SetDefaultParams();
    }
    m_left = std::make_shared<RenderableInstance>(m_pageMesh.get());
    m_turning = std::make_shared<RenderableInstance>(m_pageMesh.get());
    AddRenderableInstance(m_left);
    AddRenderableInstance(m_turning);
    m_light = std::make_shared<LightSource>(this);
    m_light->SetPosition(Vector(-1.5f,2.5f,3.8f));
    m_light->SetRadius(12.f);
    m_light->SetCoronaSize(0);
    m_light->SetSpecularAmount(0);
    AddLightSource(m_light);
    auto lightmap = std::make_shared<Texture>();
    lightmap->CreatePlainWhite();
    lightmap->m_atlasData = {0,0,0.5f,0.5f};
    SetLightmap(lightmap);
    SetFogParams({0,0,0}, 0);
    SetClearColor({0,0,0,1});
    m_sceneReady = true;
    CO_RETURN;
}

void BookScene::SetPage(std::size_t slot, const std::vector<Color4>& pixels)
{
    if (slot >= m_pages.size() || pixels.size() != 360 * 240)
    {
        throw std::invalid_argument("Book pages require one of two original-size paper textures.");
    }
    TextureLoadingContext data;
    data.m_rgbaData = pixels;
    Renderer::Instance().CreateTexture(m_pages[slot], data);
}

void BookScene::Draw(const Desperabis::UiStoryBook& view, float aspect, Renderer& renderer,
    const RenderingSettings& settings, ProfilingStats& stats)
{
    if (!std::isfinite(aspect) || aspect <= 0 || !std::isfinite(view.turn) ||
        view.turn < -1 || view.turn > 1 || !std::isfinite(view.seconds) || view.seconds < 0)
    {
        throw std::invalid_argument("Invalid book camera or animation state.");
    }
    const auto setMaterials = [&](RenderableInstance& instance, Texture& front)
    {
        auto& materials = instance.GetOrCreateMaterialOverrides();
        materials.at(0).m_texture = &front;
        materials.at(1).m_texture = &front;
        for (auto& material : materials) { material.ComputeHash(); }
    };
    setMaterials(*m_left, m_pages[view.pageIndex % 2]);
    m_left->SetTransform(Vector(1.8f,0,0), Vector(0,0,0));
    m_turning->SetVisible(view.turn >= 0);
    if (view.turn >= 0)
    {
        setMaterials(*m_turning, m_pages[(view.pageIndex + 1) % 2]);
        auto& back = m_turning->GetOrCreateMaterialOverrides().at(1);
        back.m_texture = m_paper;
        back.ComputeHash();
        const float t = static_cast<float>(view.turn);
        const float eased = t * t * (3.f - 2.f * t);
        m_turning->SetTransform(Vector(1.8f,0,0.002f), Vector((1.f - eased) * Math::Pi,0,0));
    }
    m_light->SetColor({255,235,205}, 1.15f + 0.035f *
        static_cast<float>(std::sin(view.seconds * 13.7) + std::sin(view.seconds * 23.3)));
    Matrix4 projection, transform;
    const float halfHeight = (std::max)(1.2f, 1.8f / aspect);
    projection.CreateProjection(aspect, 2.f * std::atan(halfHeight / 5.f), 0.01f, 30.f);
    transform.SetTranslation(Vector(0,0,5));
    m_camera.SetProjectionMatrix(projection);
    m_camera.SetCameraTransformMatrix(transform);
    VisibilitySettings visibilitySettings;
    DefaultVisibilityContext visibility;
    VisibilityResult visible;
    auto bookSettings = settings;
    bookSettings.renderSky = false;
    bookSettings.renderCoronas = false;
    bookSettings.dynamicLights = true;
    bookSettings.specularAmount = 0;
    bookSettings.fogOverride.reset();
    bookSettings.clearColorOverride.reset();
    visibility.ComputeResultFor(&m_camera, this, visible, bookSettings, visibilitySettings, stats);
    visible.m_ambientColor = {0.35f,0.35f,0.35f};
    visible.m_ambientNrmlContribution = 0.6f;
    renderer.RenderSceneElements(visible, bookSettings, stats);
}
