#include "pch.h"
#include "LightningPresentation.h"

#include <Asset/Level.h>
#include <Renderer/Renderer.h>
#include <Renderer/Renderable.h>
#include <Scene/Scene.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

void LightningPresentation::Show(Scene& scene, const Desperabis::LightningFlash& flash, const Camera& camera)
{
    if (m_scene && m_scene != &scene) { throw std::logic_error("Lightning must be hidden before replacing its scene."); }
    if (flash.lines.empty()) { return; }
    if (!m_texture)
    {
        m_texture = std::make_shared<Texture>();
        m_texture->m_width = 32;
        m_texture->m_height = 2;
        m_texture->SetDefaultParams();
        TextureLoadingContext pixels;
        for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 32; ++x)
        {
            const float edge = std::fabs((x + 0.5f) / 16.f - 1.f);
            const float glow = std::exp(-edge * edge * 8.f);
            pixels.m_rgbaData.push_back({ static_cast<byte>(glow * 210.f),
                static_cast<byte>(glow * 230.f), static_cast<byte>(glow * 255.f), 255 });
        }
        Renderer::Instance().CreateTexture(*m_texture, pixels);
    }

    const float distance = 1800.f * DespToFloat;
    const auto inverseProjection = camera.GetProjectionMatrix(0).GetInverse();
    const auto toView = [&](float x, float y)
    {
        Vector ray = inverseProjection.TransformPosition(Vector(x / 180.f - 1.f, 1.f - y / 120.f, 0.5f));
        if (!std::isfinite(ray.GetZ()) || std::fabs(ray.GetZ()) < 0.000001f)
        {
            throw std::invalid_argument("Lightning requires a finite perspective camera.");
        }
        ray *= distance / std::fabs(ray.GetZ());
        return ray;
    };
    const float pixelScale = (toView(181.f, 120.f) - toView(180.f, 120.f)).GetLength();
    const auto& toWorld = camera.GetCameraTransformMatrix(0);
    std::vector<std::shared_ptr<Renderable>> models;
    std::vector<RenderableInstancePtr> instances;
    // TexturedLine's second-position index is a byte: split, never truncate, large bolts.
    constexpr std::size_t linesPerModel = MaxAddressableAnimatedPositions / 2;
    for (std::size_t first = 0; first < flash.lines.size(); first += linesPerModel)
    {
        ModelLoadingContext positions;
        ModelConversionContext mesh(positions);
        const auto end = (std::min)(first + linesPerModel, flash.lines.size());
        for (auto i = first; i < end; ++i)
        {
            const auto& line = flash.lines[i];
            if (!std::isfinite(line.thickness1) || !std::isfinite(line.thickness2) ||
                line.thickness1 < 0.f || line.thickness2 < 0.f)
            {
                throw std::invalid_argument("Lightning widths must be finite and nonnegative.");
            }
            const auto p0 = static_cast<uint16_t>(positions.m_vertexPos.size());
            const auto p1 = static_cast<uint16_t>(p0 + 1);
            for (const auto point : {toView(line.x1, line.y1), toView(line.x2, line.y2)})
            {
                positions.m_vertexPos.push_back({point.GetX(), point.GetY(), point.GetZ(), 1.f});
            }
            const float w0 = line.thickness1 * pixelScale * 0.5f;
            const float w1 = line.thickness2 * pixelScale * 0.5f;
            mesh.AddToBounds(positions.m_vertexPos[p0], w0);
            mesh.AddToBounds(positions.m_vertexPos[p1], w1);
            const auto v = static_cast<uint16_t>(mesh.m_vertices.size());
            const Color4 white{255, 255, 255, 255};
            mesh.m_vertices.emplace_back().SetTexturedLine(p0, p1, white, {0.f, 0.f}, -w0);
            mesh.m_vertices.emplace_back().SetTexturedLine(p1, p0, white, {0.f, 1.f}, w1);
            mesh.m_vertices.emplace_back().SetTexturedLine(p1, p0, white, {1.f, 1.f}, -w1);
            mesh.m_vertices.emplace_back().SetTexturedLine(p0, p1, white, {1.f, 0.f}, w0);
            for (int offset : {2, 1, 0, 3, 2, 0}) { mesh.m_sortedIndices.push_back(static_cast<uint16_t>(v + offset)); }
        }
        mesh.m_numPositions = static_cast<int>(positions.m_vertexPos.size());
        auto model = std::make_shared<Renderable>();
        model->m_runtime = Renderer::Instance().CreateModelBuffers(mesh);
        model->m_numPositions = static_cast<uint16_t>(mesh.m_numPositions);
        model->m_numPolygons = static_cast<uint16_t>(end - first);
        mesh.GenerateBoundingVolume(model->m_localBounds);
        auto& draw = model->m_drawCalls.emplace_back();
        draw.m_primitiveCount = static_cast<int>(mesh.m_sortedIndices.size() / 3);
        draw.m_material.m_texture = m_texture.get();
        draw.m_material.m_textureSlot = -1;
        draw.m_material.m_transparencyType = TransparencyMode::Additive;
        draw.m_material.m_opacity = 1.f;
        draw.m_material.m_applyLighting = false;
        draw.m_material.m_useAlphaTest = false;
        draw.m_material.ComputeHash();
        auto instance = std::make_shared<RenderableInstance>(model.get(),
            static_cast<uint32_t>(GamePlay::VisibilityFilter::GameObjects));
        instance->SetTransform(toWorld);
        models.push_back(std::move(model));
        instances.push_back(std::move(instance));
    }
    const auto lowest = std::max_element(flash.lines.begin(), flash.lines.end(),
        [](const auto& a, const auto& b) { return a.y2 < b.y2; });
    auto light = std::make_shared<LightSource>(&scene);
    light->SetPosition(toWorld.TransformPosition(toView(flash.lines.front().x1, lowest->y2)));
    // Concentrate the flash around the distant bolt, outside the player's immediate radius.
    light->SetRadius(1700.f * DespToFloat);
    light->SetColor({210, 230, 255, 255}, 80.f);
    light->SetCoronaSize(0.f);
    m_scene = &scene;
    m_models.insert(m_models.end(), models.begin(), models.end());
    m_instances.insert(m_instances.end(), instances.begin(), instances.end());
    m_lights.push_back(light);
    for (const auto& instance : instances)
    {
        scene.AddRenderableInstance(instance);
        instance->UpdateGlobalBounds();
    }
    scene.AddLightSource(light);
    light->UpdateGlobalBounds();
}

void LightningPresentation::Hide()
{
    if (m_scene)
    {
        for (const auto& instance : m_instances) { m_scene->RemoveRenderableInstance(instance); }
        for (const auto& light : m_lights) { m_scene->RemoveLightSource(light); }
    }
    m_instances.clear();
    m_models.clear();
    m_lights.clear();
    m_scene = nullptr;
}
