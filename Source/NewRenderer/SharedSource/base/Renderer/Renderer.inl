#pragma once

template <typename CONTEXT_TYPE>
inline void Renderer::RenderScene_Impl(const VisibilityResult& elements, ProfilingStats& stats)
{
    CONTEXT_TYPE context(*this, elements, stats);

    stats.ResetRenderStats();
    ProfilingScope _(stats.m_duration_rendering);

    context.BeginRendering();
    context.BeginRenderTargetScope(RenderTargetPassType::MainView);

    if (elements.HasResults())
    {
        // normal instances:
        context.TrackGlobals();
        RenderSky(context, elements.m_visibleInstances);

        RenderOpaqueGeometry(context, elements.m_visibleInstances);
#ifdef ENABLE_ADVANCED_RENDERING
        if (elements.m_anyWaterVisible)
        {
            RenderWaterInstances(context);
        }
#endif
        RenderTransparentGeometry(context, elements.m_visibleInstances);

#ifdef ENABLE_ADVANCED_RENDERING
        context.RenderPostEffects();
#endif
    }

    context.EndRenderTargetScope();
    assert(context.m_renderTargetPass == RenderTargetPassType::MainView);

    context.EndRendering();
}



template <typename CONTEXT_TYPE>
inline void Renderer::RenderSky(CONTEXT_TYPE& context, const VisibilityResult::VisibleInstances& instances)
{
    if (instances.m_skyInstances.size() > 0)
    {
        context.SetGeometryPassType(GeometryPassType::Sky);

        // render additive before alpha-blended, because stars are additive and they should be covered
        const TransparencyMode skyModes[] = { TransparencyMode::SkyOpaque, TransparencyMode::SkyAdditive, TransparencyMode::SkyAlphaBlended };
        for (TransparencyMode mode : skyModes)
        {
            TransparencyModeMask modeMask = (TransparencyModeMask)(1 << (int)mode);
            if ((instances.m_visibleTransparencyTypes & modeMask) != TransparencyModeMask::None)
            {
                context.SetBlendState(mode);
                for (const auto& entry : instances.m_skyInstances)
                {
                    context.RenderInstance<GeometryPassType::Sky>(entry.m_instance, mode);
                }
            }
        }
    }
}

template <typename CONTEXT_TYPE>
inline void Renderer::RenderOpaqueGeometry(CONTEXT_TYPE& context, const VisibilityResult::VisibleInstances& instances)
{
    if (instances.m_opaqueInstances.size() > 0)
    {
        context.SetGeometryPassType(GeometryPassType::Instances);
        context.SetBlendState(TransparencyMode::Opaque);
        for (const auto& entry : instances.m_opaqueInstances)
        {
            context.RenderInstance<GeometryPassType::Instances>(entry.m_instance, TransparencyMode::Opaque);
        }
    }
}


template <typename CONTEXT_TYPE>
inline void Renderer::RenderTransparentGeometry(CONTEXT_TYPE& context, const VisibilityResult::VisibleInstances& instances)
{
    if (instances.m_transparentInstances.size() > 0)
    {
        context.SetGeometryPassType(GeometryPassType::Instances);

        static const TransparencyMode transpModes[] = { TransparencyMode::AlphaBlended, TransparencyMode::Additive};

        for (TransparencyMode mode : transpModes)
        {
            if ((instances.m_visibleTransparencyTypes & (TransparencyModeMask)(1 << (int)mode)) != TransparencyModeMask::None)
            {
                context.SetBlendState(mode);
                for (const auto& entry : instances.m_transparentInstances)
                {
                    context.RenderInstance<GeometryPassType::Instances>(entry.m_instance, mode);
                }
            }
        }
    }

    // objects in flames
    if (instances.m_burningInstances.size() > 0)
    {
        context.SetGeometryPassType(GeometryPassType::InstancesInFlames);
        context.SetBlendState(TransparencyMode::Additive);
        for (const auto& entry : instances.m_burningInstances)
        {
            context.RenderInstance<GeometryPassType::InstancesInFlames>(entry.m_instance, TransparencyMode::Additive);
        }
    }
}

#ifdef ENABLE_ADVANCED_RENDERING
template <typename CONTEXT_TYPE>
inline void Renderer::RenderWaterInstances(CONTEXT_TYPE& context)
{
    bool firstTimeRefraction = true;
	context.BeginWaterRendering();

    for (const VisibilityResult::VisibleWaterLevel& water : context.m_elements.m_visibleWater)
    {
        assert(water.m_waterReceiver.size() > 0);

        // mirror rendering:
        if (water.m_useReflection)
        {
            context.BeginRenderTargetScope((RenderTargetPassType)RenderTargetPassType::Mirror, &water.m_mirrorCamera);
            context.TrackGlobals();
            RenderSky(context, water.m_visibleInstancesInMirror);
            RenderOpaqueGeometry(context, water.m_visibleInstancesInMirror);
            RenderTransparentGeometry(context, water.m_visibleInstancesInMirror);
            context.RenderPostEffects();
            context.EndRenderTargetScope();
            context.TrackGlobals(); // track for main view again
        }
        else 
        {
            // refraction: typically 1x frame buffer copy
            context.PrepareRefraction(firstTimeRefraction);
            firstTimeRefraction = false;
        }

        // render instances:
        context.SetGeometryPassType(GeometryPassType::MirrorReceiver);
        context.SetBlendState(TransparencyMode::Water);
        context.PrepareWaterRendering(water);
        for (const auto& entry : water.m_waterReceiver)
        {
            context.RenderInstance<GeometryPassType::MirrorReceiver>(entry.m_instance, TransparencyMode::Water);
        }

    }
    context.EndWaterRendering();
}
#endif
