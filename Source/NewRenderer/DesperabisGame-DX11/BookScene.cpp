#include "pch.h"
#include "BookScene.h"

#include <Asset/AssetManager.h>
#include <Renderer/Renderer.h>
#include <Renderer/Visibility.h>
#include <Renderer/LightSource.h>
#include <cmath>
#include <stdexcept>

void BookScene::CreatePages(bool rigid)
{
    ModelLoadingContext positions;
    ModelConversionContext geometry(positions);
    const int strips = rigid ? 1 : PageStrips;
    const int positionsPerFrame = (strips + 1) * 2;
    static_assert(TurnFrames * (PageStrips + 1) * 2 * sizeof(VertexPosition) <= 64 * 1024,
        "Cached book poses must fit the DX11 position constant buffer.");
    auto& meshes = rigid ? m_coverMeshes : m_pageMeshes;
    std::array<float, PageStrips + 1> textureUs;
    for (int column = 0; column <= strips; ++column)
    {
        const double u = static_cast<double>(column) / strips;
        // Keep extra strips near the free edge as the rolling curl shrinks.
        textureUs[column] = static_cast<float>(rigid ? u : u * u);
    }
    for (int frame = 0; frame < TurnFrames; ++frame)
    {
        const double t = static_cast<double>(frame) / (TurnFrames - 1);
        double eased = t * t * (3 - 2 * t);
        if (t > 0.5)
        {
            // Match the curl's initial speed, then settle with zero speed and acceleration.
            const double remaining = 2 * (1 - t);
            eased = 1 - 0.5 * remaining * remaining * remaining * (2.5 - 1.5 * remaining);
        }
        const double curl = static_cast<double>(Math::Pi) * (std::min)(2 * eased, 1.0);
        const double laidLength = 3.6 * (std::max)(2 * eased - 1, 0.0);
        const double radius = curl > 0 ? (3.6 - laidLength) / curl : 0;
        const double rootAngle = static_cast<double>(Math::Pi) - curl;
        std::array<double, PageStrips + 1> tangents;
        auto mesh = std::make_shared<Renderable>();
        mesh->m_vertexOfs = frame * positionsPerFrame;
        mesh->m_numPositions = positionsPerFrame;
        for (int column = 0; column <= strips; ++column)
        {
            const double distance = 3.6 * (1 - static_cast<double>(textureUs[column]));
            double x = -distance, z = 0, tangent = rootAngle;
            if (rigid)
            {
                const double angle = static_cast<double>(Math::Pi) * eased;
                x = distance * std::cos(angle);
                z = distance * std::sin(angle);
                tangent = static_cast<double>(Math::Pi) - angle;
            }
            else if (frame == 0) { x = distance; }
            else if (distance > laidLength)
            {
                tangent += (distance - laidLength) / radius;
                x = -laidLength - radius * (std::sin(tangent) - std::sin(rootAngle));
                z = radius * (std::cos(rootAngle) - std::cos(tangent));
            }
            tangents[column] = tangent;
            for (int row = 0; row < 2; ++row)
            {
                const VertexPosition position{static_cast<float>(x),row == 0 ? 1.2f : -1.2f,
                    static_cast<float>(z),1};
                positions.m_vertexPos.push_back(position);
                mesh->m_sourcePositions.push_back(position);
                geometry.AddToBounds(position, 0);
            }
        }
        for (int face = 0; face < 2; ++face)
        {
            const auto firstVertex = static_cast<std::uint16_t>(geometry.m_vertices.size());
            for (std::uint16_t i = 0; i < positionsPerFrame; ++i)
            {
                ModelVertex vertex{};
                vertex.SetTextured(i, {255,255,255,0}, {textureUs[i / 2],static_cast<float>(i % 2)});
#ifdef ENABLE_ADVANCED_RENDERING
                const double tangent = tangents[i / 2];
                const float side = face == 0 ? -1.f : 1.f;
                // The lighting shader expects inward-facing native-model normals.
                vertex.Normal = {side * static_cast<float>(std::sin(tangent)),0,
                    side * static_cast<float>(std::cos(tangent))};
#endif
                geometry.m_vertices.push_back(vertex);
            }
            DrawCall draw;
            draw.m_firstIndex = static_cast<int>(geometry.m_sortedIndices.size());
            draw.m_primitiveCount = strips * 2;
            draw.m_material.m_useAlphaTest = false;
            draw.m_material.ComputeHash();
            mesh->m_drawCalls.push_back(draw);
            for (int column = 0; column < strips; ++column)
            {
                const auto a = static_cast<std::uint16_t>(firstVertex + column * 2);
                const auto b = static_cast<std::uint16_t>(a + 2);
                const auto c = static_cast<std::uint16_t>(a + 1);
                const auto d = static_cast<std::uint16_t>(a + 3);
                if (face == 0) { geometry.m_sortedIndices.insert(geometry.m_sortedIndices.end(), {a,b,c,c,b,d}); }
                else { geometry.m_sortedIndices.insert(geometry.m_sortedIndices.end(), {a,c,b,c,d,b}); }
            }
        }
        meshes[frame] = std::move(mesh);
    }
    const auto runtime = Renderer::Instance().CreateModelBuffers(geometry);
    for (auto& mesh : meshes)
    {
        mesh->m_runtime = runtime;
        geometry.GenerateBoundingVolume(mesh->m_localBounds);
    }
}

FUTURE(void) BookScene::CreateScene()
{
    CreatePages(false);
    CreatePages(true);
    for (auto& page : m_pages)
    {
        AssetManager::Instance().RegisterAsset(page);
        page.m_width = 360;
        page.m_height = 240;
        page.SetDefaultParams();
    }
    m_left = std::make_shared<RenderableInstance>(m_pageMeshes.back().get());
    m_turning = std::make_shared<RenderableInstance>(m_pageMeshes.front().get());
    AddRenderableInstance(m_left);
    AddRenderableInstance(m_turning);
    m_light = std::make_shared<LightSource>(this);
    m_light->SetPosition(Vector(3.0f,2.5f,3.8f));
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
    const auto* currentBitmap = view.current.size() == 1 ?
        std::get_if<Desperabis::UiBitmap>(&view.current.front()) : nullptr;
    m_left->SetRenderable(currentBitmap && currentBitmap->source == Desperabis::UiBitmapSource::BookCover ?
        m_coverMeshes.back().get() : m_pageMeshes.back().get());
    setMaterials(*m_left, m_pages[view.pageIndex % 2]);
    m_left->SetTransform(Vector(1.8f,0,0), Vector(0,0,0));
    m_turning->SetVisible(view.turn >= 0);
    if (view.turn >= 0)
    {
        const float pose = static_cast<float>(view.turn) * (TurnFrames - 1);
        const int frame = static_cast<int>(pose);
        const auto& meshes = view.rigidTurn ? m_coverMeshes : m_pageMeshes;
        m_turning->SetRenderable(meshes[frame].get(),
            frame + 1 < TurnFrames ? meshes[frame + 1].get() : nullptr);
        m_turning->m_blendWeight = pose - frame;
        setMaterials(*m_turning, m_pages[(view.pageIndex + 1) % 2]);
        auto& back = m_turning->GetOrCreateMaterialOverrides().at(1);
        back.m_texture = m_paper;
        back.ComputeHash();
        m_turning->SetTransform(Vector(1.8f,0,0.002f), Vector(0,0,0));
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
