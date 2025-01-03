#pragma once
#include <DesperabisCommon.h>
#include <Renderer/VisibilityTileGrid.h>
#include <Renderer/RendererCommon.h>
#include <Renderer/RenderableInstance.h>
#include <Renderer/LightSource.h>
#include <Renderer/Camera.h>


/// <summary>
/// The visibility result is given to the renderer for rendering
/// </summary>
struct VisibilityResult
{
    struct InstanceAndHash
    {
        const RenderableInstance* m_instance;
        uint32_t m_sortingHash;
    };

    void Reset();

    bool HasResults() const 
    {
        return m_mainCamera != nullptr && (m_visibleInstances.m_visibleTransparencyTypes != TransparencyModeMask::None);
    }

    const Camera* GetMainCamera() const;

    const Scene* m_scene = nullptr;
    const Camera* m_mainCamera = nullptr;

    // Visible instances:
    using InstanceCollection = std::vector<InstanceAndHash>;
    struct VisibleInstances
    {
        void Reset();
        InstanceCollection m_skyInstances;
        InstanceCollection m_opaqueInstances;
        InstanceCollection m_transparentInstances;
        TransparencyModeMask m_visibleTransparencyTypes = TransparencyModeMask::None;
		InstanceCollection m_burningInstances;
    };

    struct VisibleWaterLevel
    {
        void Reset();
        inline bool HasReceiver() const 
        {
            return m_waterReceiver.size() > 0;
        }
        bool m_useReflection = false;
        float m_waterHeight = 0.f;
        VisibleInstances m_visibleInstancesInMirror;
        InstanceCollection m_waterReceiver;
        Float4Storage m_scrollParams1;
        Float4Storage m_scrollParams2;
        Camera m_mirrorCamera;
    };

    VisibleInstances m_visibleInstances;

#ifdef ENABLE_ADVANCED_RENDERING
    bool m_anyWaterVisible = false;
    std::vector<VisibleWaterLevel> m_visibleWater;
    VisibleWaterLevel& CreateWaterReceiver(bool useReflection, float waterHeight);

    // relevant lightsources:
    std::vector<PackedLightSourceParams> m_visibileLights;
    std::vector<PackedLightSourceParams> m_visibileCoronas;
    Texture* m_lightmap = nullptr;
#endif

    float m_fogFalloff = 0.3f;
    Vector3 m_fogColor;
    Vector3 m_ambientColor;
    float m_ambientNrmlContribution = 0.01f;
	VisibilityTileGrid m_grid;

};


/// <summary>
/// Produces a VisibilityResult
/// </summary>
class IVisibilityContext
{
public:
    struct Limits
    {
        int MaxVisibleLights = 64;
    };
    IVisibilityContext() = default;
    virtual ~IVisibilityContext() = default;

    virtual void ComputeResultFor(const Camera* camera, const Scene* scene, VisibilityResult& result, ProfilingStats& out_stats) = 0;

    Limits& AccessLimits() { return m_limits; }
protected:
    static void SortCollection(VisibilityResult::InstanceCollection& list);
    Limits m_limits;
};


class DefaultVisibilityContext : public IVisibilityContext
{
public:
    DefaultVisibilityContext() = default;
    virtual ~DefaultVisibilityContext() = default;

    virtual void ComputeResultFor(const Camera* camera, const Scene* scene, VisibilityResult& result, ProfilingStats& out_stats) override;

protected:
    void DetermineVisibleInstances(VisibilityResult& result, VisibilityResult::VisibleInstances& target, const Scene* scene, const Vector& cameraPos, const Frustum& camFrustum, const VisibilityTileGrid* grid, bool collectWaterReceiver);
    bool m_performFrustumClipping = true;
    Frustum m_frustum;
    Frustum m_mirrorFrustum;
};
