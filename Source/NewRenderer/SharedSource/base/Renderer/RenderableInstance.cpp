#include "pch.h"
#include <Asset/GridCellArray.h>
#include <Renderer/RenderableInstance.h>
#include <Renderer/Renderable.h>
#include <Renderer/IGraphicsRuntimeObject.h>
#include <Scene/Scene.h>


void RenderableInstance::OnAddedToScene(const Scene* ownerScene)
{
    m_ownerScene = ownerScene;
    m_globalBoundsDirty = true; // a new scene means the visibility grid could be different
    ComputeTransparencyMask();
}

std::vector<DrawCallMaterial>& RenderableInstance::GetOrCreateMaterialOverrides()
{
    if (m_renderable == nullptr)
    {
        return m_materialOverride;
    }

    if (m_materialOverride.size() < m_renderable->m_drawCalls.size())
    {
        size_t oldCount = m_materialOverride.size();
        m_materialOverride.resize(m_renderable->m_drawCalls.size());
        for (size_t i = oldCount; i < m_materialOverride.size(); i++)
        {
            m_materialOverride[i] = m_renderable->m_drawCalls[i].m_material;
        }
    }

	return m_materialOverride;

}

uint32_t RenderableInstance::ComputeOpaqueSortingHash() const
{
    uint32_t thisHash = (((uint64_t)this) >> 4) & 0xffff; // lower significant part for stable sort
    uint32_t hash = thisHash | (m_renderable->m_runtime->m_Id << 16);
    return hash;
}

void RenderableInstance::SetSourcePrimitiveCount(int count)
{
    if (!m_renderable) { return; }
    bool changed = false;
    if (count >= 0)
    {
        auto& materials = GetOrCreateMaterialOverrides();
        for (std::size_t i = 0; i < m_renderable->m_drawCalls.size(); ++i)
        {
            const int triangles = m_renderable->m_drawCalls[i].GetPrimitiveCountForPrefix(count);
            changed |= materials[i].m_primitiveCountOverride != triangles;
            materials[i].m_primitiveCountOverride = triangles;
        }
    }
    else
    {
        for (auto& material : m_materialOverride)
        {
            changed |= material.m_primitiveCountOverride != -1;
            material.m_primitiveCountOverride = -1;
        }
    }
    for (std::size_t i = 0; i < m_subInstances.size(); ++i)
    {
        const int sourceIndex = m_renderable->m_subRenderables[i].m_sourcePrimitiveIndex;
        m_subInstances[i].SetSourcePrimitiveCount(count >= 0 && (count == 0 || sourceIndex >= count) ? 0 : -1);
    }
    if (changed) { ComputeTransparencyMask(); }
}


uint32_t RenderableInstance::ComputeTransparentSortingHash(const Vector& cameraPos) const
{
    Vector translate = m_transform.GetTranslation();
    translate -= cameraPos;
    float len = translate.GetLength();
    uint32_t distanceInt = (uint32_t)(len * 10000.f);
    return distanceInt;
}

uint32_t RenderableInstance::ComputeSkySortingHash() const
{
    return (int)this->m_usedTransparencies; // not relevant since Renderer::RenderSky already renders transparencies in the right order
}

void RenderableInstance::ComputeTransparencyMask()
{
    m_usedTransparencies = TransparencyModeMask::None;
    if (m_renderable != nullptr)
    {
        for (int i = 0; i < (int)m_renderable->m_drawCalls.size(); i++)
        {
            const DrawCallMaterial& material = i < m_materialOverride.size() ? m_materialOverride[i] : m_renderable->m_drawCalls[i].m_material;
            if (material.m_primitiveCountOverride == 0) { continue; }
            m_usedTransparencies |= (TransparencyModeMask)(1 << (int)material.m_transparencyType);
        }
    }
}

void RenderableInstance::UpdateGlobalBounds()
{
    if (m_globalBoundsDirty)
    {
        if (m_renderable != nullptr && m_renderable->m_localBounds.IsValid())
        {
             m_renderable->m_localBounds.GetTransformed(m_globalBounds, m_transform, m_uniformScale);
             if (m_blendToRenderable && m_blendToRenderable->m_localBounds.IsValid())
             {
                 BoundingVolume target;
                 m_blendToRenderable->m_localBounds.GetTransformed(target, m_transform, m_uniformScale);
                 m_globalBounds.Add(target);
                 m_globalBounds.OnUpdated();
             }
             if (m_ownerScene != nullptr)
             {
                 if (const auto* grid = m_ownerScene->GetGrid())
                 {
                     grid->GetTileAssignment(m_transform.GetTranslation(), m_globalBounds);
                 }
             }
        }
        else
        {
            m_globalBounds.Reset();
        }

        m_globalBoundsDirty = false;
    }
}


void RenderableInstance::SetTranslation(const Vector& pos)
{
	m_transform.SetTranslation(pos);
	m_globalBoundsDirty = true;
}

void RenderableInstance::SetTransform(const Vector& pos, const Vector& eulerRot, float scale)
{
    m_transform.Create(pos, eulerRot, scale);
	m_globalBoundsDirty = true;
}

void RenderableInstance::SetTransform(const Matrix4& transform)
{
	m_transform = transform;
	m_globalBoundsDirty = true;
}

void RenderableInstance::SetTransformAndUpdate(const Matrix4& transform)
{
    m_transform = transform;
    m_globalBoundsDirty = true;
    UpdateGlobalBounds();
}


void RenderableInstance::SetRenderable(const Renderable* renderable, const Renderable* blendTarget /*= nullptr*/)
{
    if (m_renderable == renderable && m_blendToRenderable == blendTarget)
    {
        return;
    }

    const bool primaryChanged = m_renderable != renderable;
    m_renderable = renderable;
    m_blendToRenderable = blendTarget;
	m_globalBoundsDirty = true;
    if (!primaryChanged) { return; }

    m_subInstances.clear();
    if (m_renderable != nullptr && m_renderable->m_subRenderables.size() > 0)
    {
        m_subInstances.resize(m_renderable->m_subRenderables.size());
        for (size_t i = 0; i < m_renderable->m_subRenderables.size(); i++)
        {
            const SubRenderable& src(m_renderable->m_subRenderables[i]);
            RenderableInstance& dst(m_subInstances[i]);
            dst.m_ownerScene = m_ownerScene;
            dst.m_renderable = src.m_renderable;
			dst.m_localPos = src.m_localPos;
            dst.m_localOri = src.m_localOri;
            dst.ComputeTransparencyMask();
        }
    }
}

void RenderableInstance::PrepareSubInstancesForRendering()
{
    for (std::size_t i = 0; i < m_subInstances.size(); ++i)
    {
        auto& sub = m_subInstances[i];
        auto position = sub.m_localPos;
        const int vertex = m_renderable->m_subRenderables[i].m_anchorVertex;
        if (m_blendToRenderable && m_blendWeight != 0.f && vertex >= 0)
        {
            const auto& target = m_blendToRenderable->m_sourcePositions.at(vertex);
            auto delta = Vector(target.x, target.y, target.z) - position;
            delta *= m_blendWeight;
            position += delta;
        }
        Matrix4 local;
        position *= m_uniformScale;
        local.Create(position, sub.m_localOri, 1.f);
        sub.m_uniformScale = m_uniformScale;
        sub.SetTransformAndUpdate(local * m_transform);
    }
}

bool RenderableInstance::RetrieveWaterProperties(bool& useReflection, float& waterHeight, int drawCallIndex) const
{
    if (!HasWaterMaterials())
        return false;

    for (int i = 0; i < m_renderable->m_drawCalls.size(); i++)
    {
        if (drawCallIndex >= 0 && i != drawCallIndex) { continue; }
        const DrawCallMaterial& material = i < m_materialOverride.size() ? m_materialOverride[i] : m_renderable->m_drawCalls[i].m_material;
        if (material.m_transparencyType != TransparencyMode::Water ||
            material.m_primitiveCountOverride == 0 ||
            material.m_reflectionType == WaterReflectionType::Disabled)
            continue;
        useReflection = (material.m_reflectionType & WaterReflectionType::ReflectionBit) != 0;
        waterHeight = m_transform.TransformPosition(Vector(0, material.m_mirrorHeight * m_uniformScale, 0)).GetY();
        return true;
    }

    return false;
}
