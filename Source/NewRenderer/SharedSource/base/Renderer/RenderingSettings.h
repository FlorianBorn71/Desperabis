#pragma once
#include <DesperabisCommon.h>

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

	/// Needs to be called before anything is loaded
	static void SetRelativeGeometryScaling(float scale);
};
