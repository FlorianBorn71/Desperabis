#include "pch.h"
#include <Renderer/Visibility.h>
#include <Scene/Scene.h>
#include <Renderer/RenderableInstance.h>
#include <Renderer/Renderable.h>
#include <Renderer/Camera.h>
#include <Renderer/LightSource.h>
#include <Renderer/GlobalSettings.h>
#include <Asset/GridCellArray.h>



void VisibilityResult::Reset()
{
    m_mainCamera = nullptr;
    m_visibleInstances.Reset();
#ifdef ENABLE_ADVANCED_RENDERING
    m_visibileLights.clear();
    m_visibileCoronas.clear();
    m_visibleWater.clear();
    m_anyWaterVisible = false;
#endif
}

const Camera* VisibilityResult::GetMainCamera() const
{
    return m_mainCamera;
}

bool WaterLevelEquals(float l1, float l2)
{
    return fabsf(l1 - l2) < 10.f * DespToFloat;
}

#ifdef ENABLE_ADVANCED_RENDERING
VisibilityResult::VisibleWaterLevel& VisibilityResult::CreateWaterReceiver(bool useReflection, float waterHeight)
{
    for (VisibleWaterLevel& level : m_visibleWater)
    {
        if (level.m_useReflection != useReflection)
            continue;
        if (useReflection && !WaterLevelEquals(level.m_waterHeight, waterHeight))
            continue;
        return level;
    }
    VisibilityResult::VisibleWaterLevel& newInst = m_visibleWater.emplace_back();
    newInst.m_waterHeight = waterHeight;
    newInst.m_useReflection = useReflection;
    return newInst;
}
#endif

void VisibilityResult::VisibleInstances::Reset()
{
    m_visibleTransparencyTypes = TransparencyModeMask::None;
    m_skyInstances.clear();
    m_opaqueInstances.clear();
    m_transparentInstances.clear();
    m_burningInstances.clear();
}

void VisibilityResult::VisibleWaterLevel::Reset()
{
    m_visibleInstancesInMirror.Reset();
    m_waterReceiver.clear();
}

void IVisibilityContext::SortCollection(VisibilityResult::InstanceCollection& list)
{
    if (list.size() > 1)
    {
        // sort list from large hash to small hash
        std::sort(list.begin(), list.end(), [](const VisibilityResult::InstanceAndHash& instA, const VisibilityResult::InstanceAndHash& instB)-> bool
            {
                return instA.m_sortingHash > instB.m_sortingHash;
            });
    }
}


/// <summary>
/// DefaultVisibilityContext
/// </summary>

extern float g_debugValue;

void DefaultVisibilityContext::DetermineVisibleInstances(VisibilityResult& result, VisibilityResult::VisibleInstances& target, const Scene* scene, const Vector& cameraPos, const Frustum& camFrustum, const VisibilityTileGrid* grid, bool collectWaterReceiver)
{
    target.m_visibleTransparencyTypes = TransparencyModeMask::None;

    // instance visibility
    VisibilityResult::InstanceCollection& list_o(target.m_opaqueInstances);
    VisibilityResult::InstanceCollection& list_t(target.m_transparentInstances);
    VisibilityResult::InstanceCollection& list_s(target.m_skyInstances);
    VisibilityResult::InstanceCollection& list_f(target.m_burningInstances);

	auto AddBurningLight = [&](const RenderableInstance* inst, bool performLightVisibleTest)
	{
		if (result.m_visibileLights.size() < m_limits.MaxVisibleLights)
		{
			const BoundingVolume& bounds = inst->GetGlobalBounds();
			Vector center = bounds.GetCenter();
			float radius = bounds.GetSphereRadius() * 2.f;
			if (performLightVisibleTest && !camFrustum.IsSphereVisible(center, radius))
			{
				return;
			}
			Color4 col = { 255,230,180,255 };
			float f = inst->m_fireIntensity;
			float intensity = f * 2.f * (0.7f + 0.4f * sinf(f * 111.f) + 0.15f * sinf(f * 261.f) + 0.07f * cosf(f * 391.f));
			PackedLightSourceParams& lightEntry = result.m_visibileLights.emplace_back();
			lightEntry.SetLight(center.GetX(), center.GetY(), center.GetZ(), radius, col, intensity);
		}
	};


    auto AddInstanceToBuckets = [&](const RenderableInstance* inst)
    {
		target.m_visibleTransparencyTypes |= inst->GetUsedTransparencyMask();
		// sort into buckets
		if (inst->HasOpaqueMaterials())
		{
			VisibilityResult::InstanceAndHash& entry = list_o.emplace_back();
			entry.m_instance = inst;
			entry.m_sortingHash = inst->ComputeOpaqueSortingHash();
		}
		if (inst->HasTransparentMaterials())
		{
			VisibilityResult::InstanceAndHash& entry = list_t.emplace_back();
			entry.m_instance = inst;
			entry.m_sortingHash = inst->ComputeTransparentSortingHash(cameraPos);
		}
		if (inst->HasSkyMaterials())
		{
			VisibilityResult::InstanceAndHash& entry = list_s.emplace_back();
			entry.m_instance = inst;
			entry.m_sortingHash = inst->ComputeSkySortingHash();
		}
		if (inst->m_fireIntensity > 0.f)
		{
			VisibilityResult::InstanceAndHash& entry = list_f.emplace_back();
			entry.m_instance = inst;
			entry.m_sortingHash = inst->ComputeOpaqueSortingHash(); // since flames are all additively rendered, we can as well sort by renderstate
			// add a light source implicitly:
			AddBurningLight(inst, false); //  no need to test visibility since the object itself is visible
		}
#ifdef ENABLE_ADVANCED_RENDERING
		bool useReflection;
		float waterHeight;

		if (collectWaterReceiver && inst->RetrieveWaterProperties(useReflection, waterHeight))
		{
			VisibilityResult::VisibleWaterLevel& level = result.CreateWaterReceiver(useReflection, waterHeight);
			VisibilityResult::InstanceAndHash& entry = level.m_waterReceiver.emplace_back();
			entry.m_instance = inst;
			entry.m_sortingHash = inst->ComputeOpaqueSortingHash();
		}
#endif
    };

    const auto& instances = scene->GetRenderableInstances();
    for (const auto inst : instances)
    {
        if (!inst->IsValid() || !inst->GetVisible())
        {
            continue;
        }

        TransparencyModeMask transpMask = inst->GetUsedTransparencyMask();
        if (m_performFrustumClipping && (transpMask & TransparencyModeMask::AllSky) == TransparencyModeMask::None)
        {
            inst->UpdateGlobalBounds();
            const BoundingVolume& bounds = inst->GetGlobalBounds();
            
            if ((grid != nullptr && !grid->IsVisible(bounds)) || !camFrustum.IsVolumeVisible(bounds))
            {
                if (inst->m_fireIntensity > 0.f) // even when not visible, fire light may need to be added
                {
                    AddBurningLight(inst.get(), true); // ...with visibility check
                }
                continue;
            }
        }

        // instance is visible...add itself and the sub instances (without further checks)

        if (inst->GetRenderable()->HasOwnDrawcalls())
        {
            AddInstanceToBuckets(inst.get());
        }
        inst->PrepareSubInstancesForRendering();

        // sub instances:
        for (const RenderableInstance& subInst : inst->m_subInstances)
        {
			AddInstanceToBuckets(&subInst);
        }
    }

    SortCollection(list_o);
    SortCollection(list_t);
    SortCollection(list_s);
#ifdef ENABLE_ADVANCED_RENDERING
    if (collectWaterReceiver)
    {
        for (VisibilityResult::VisibleWaterLevel& level : result.m_visibleWater)
        {
            SortCollection(level.m_waterReceiver);
        }
    }
#endif
}

void DefaultVisibilityContext::ComputeResultFor(const Camera* camera, const Scene* scene, VisibilityResult& result, ProfilingStats& out_stats)
{
    result.Reset();
    result.m_scene = scene;
    result.m_mainCamera = camera;
    if (scene == nullptr || !scene->IsReady())
    {
        return;
    }

    if (m_performFrustumClipping)
    {
        camera->BuildFrustum(m_frustum);
    }

    const GlobalSettings& settings = GlobalSettings::Instance();

    double currentTime = scene->GetSceneTime();
    Vector cameraPos = camera->GetCameraPosition();

	const VisibilityTileGrid* grid = nullptr;
	
    if (settings.gridVisibility)
    {
        if (const GridCellArray* srcGrid = scene->GetGrid())
        {
            grid = &result.m_grid;
            result.m_grid.Initialize(*srcGrid);
            result.m_grid.Run(cameraPos, m_frustum, out_stats);
        }
    }

#ifdef ENABLE_ADVANCED_RENDERING
    // set lightmap:
    result.m_lightmap = scene->GetLightmap().get();

    if (settings.dynamicLights || settings.renderCoronas)
    {

        // light sources
        const std::vector<std::shared_ptr<LightSource>>& lights = scene->GetLightSources();
        for (auto& light : lights)
        {
            light->UpdateGlobalBounds();
            if (settings.dynamicLights
                && result.m_visibileLights.size() < m_limits.MaxVisibleLights
                && m_frustum.IsSphereVisible(light->GetPosition(), light->GetRadius())
                && ((grid == nullptr) || grid->IsVisible(light->GetGlobalBounds())) // for light sources we need to check full bounds
                )
            {
                PackedLightSourceParams& lightEntry = result.m_visibileLights.emplace_back();
                if (!light->FillLightParams(lightEntry))
                {
                    result.m_visibileLights.pop_back();
                }
            }

            float coronaRadius = light->GetEffectiveCoronaSize();
            if (settings.renderCoronas
                && coronaRadius > 0.f
                && result.m_visibileCoronas.size() < m_limits.MaxVisibleLights
                && m_frustum.IsSphereVisible(light->GetPosition(), coronaRadius)
                && ((grid == nullptr) || grid->IsPivotVisible(light->GetGlobalBounds())) // for coronas we need to check the actual tile at pivot
                )
            {
                PackedLightSourceParams& lightEntry = result.m_visibileCoronas.emplace_back();
                if (!light->FillCoronaParams(lightEntry))
                {
                    result.m_visibileCoronas.pop_back();
                }
            }
        }
    }
#endif
   
    // determine visible instances in normal view
    DetermineVisibleInstances(result, result.m_visibleInstances, scene, cameraPos, m_frustum, grid, true);

#ifdef ENABLE_ADVANCED_RENDERING
    // water rendering
    result.m_anyWaterVisible = (result.m_visibleInstances.m_visibleTransparencyTypes & TransparencyModeMask::AllWaterReceiver) != TransparencyModeMask::None;
    for (VisibilityResult::VisibleWaterLevel& water : result.m_visibleWater)
    {
        Float4Storage& scroll1(water.m_scrollParams1);
        Float4Storage& scroll2(water.m_scrollParams2);
        float scale = 2.f * DespRelativeInvScaling;
        scroll1.x = scale;
        scroll1.y = scale * 1.7f;
        scroll2.x = scale * 1.31f;
        scroll2.y = scale * 1.91f;

        // scroll ofs:
        scroll1.z = (float)fmod(currentTime * 0.051, 1.0);
        scroll1.w = (float)fmod(currentTime * 0.0191, 1.0);
        scroll2.z = (float)fmod(currentTime * -0.0314, 1.0);
        scroll2.w = (float)fmod(currentTime * 0.04941, 1.0);

        if (water.m_useReflection)
        {
            water.m_mirrorCamera.CreateMirrored(*camera, water.m_waterHeight);
            water.m_mirrorCamera.BuildFrustum(m_mirrorFrustum);
            DetermineVisibleInstances(result, water.m_visibleInstancesInMirror, scene, water.m_mirrorCamera.GetCameraPosition(), m_mirrorFrustum, grid, false);
        }
    }
#endif

    static float ambientTweak = 3.f;

    if (settings.fullbrightRendering)
    {
		result.m_ambientColor = { 1,1,1 };
		result.m_ambientNrmlContribution = 0.0f;

    }
    else
    {
        if ((result.m_visibleInstances.m_visibleTransparencyTypes & TransparencyModeMask::SkyOpaqueBit) != TransparencyModeMask::None)
        {
            // outdoor
            result.m_ambientColor = { 0.10f * ambientTweak, 0.14f * ambientTweak, 0.18f * ambientTweak };
            result.m_ambientNrmlContribution = 0.5f;
        }
        else
        {
            // indoor
            result.m_ambientColor = { 0.07f * ambientTweak, 0.06f * ambientTweak, 0.03f * ambientTweak };
            result.m_ambientNrmlContribution = 0.1f;
        }
    }
    if (settings.useFog)
    {
        scene->GetFogParams(result.m_fogColor, result.m_fogFalloff);
    }
    else
    {
        result.m_fogFalloff = 0.f;
    }
}
