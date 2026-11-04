#pragma once
#include <DesperabisCommon.h>
#include <Asset/ModelGeometry.h>
#include <optional>

struct FogParameters
{
	Vector3 color = {0,0,0};
	float falloff = 0.f;
	float distanceOffset = 0.f;
	bool affectUnlit = false;
};

/// <summary>
/// Tweakables that control how a scene is rendered.
/// The application owns the instances and passes them into the renderer,
/// so different views (e.g. main view and map view) can use different settings.
/// </summary>
struct RenderingSettings
{
public:
	bool useFog = true;
	bool fullbrightRendering = false;
	bool dynamicLights = true;
	bool renderCoronas = true;
	bool gridVisibility = true;
	float gamma = 1.0f;
	std::optional<FogParameters> fogOverride;
	bool renderSky = true;
	std::optional<Vector4> clearColorOverride;

	/// Needs to be called before anything is loaded
	static void SetRelativeGeometryScaling(float scale);
};
