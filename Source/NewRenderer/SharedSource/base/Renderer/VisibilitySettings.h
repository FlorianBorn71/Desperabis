#pragma once
#include <DesperabisCommon.h>

/// <summary>
/// Controls which parts of a scene are considered for rendering.
/// The application owns the instances and passes them into the renderer.
/// </summary>
struct VisibilitySettings
{
public:
	/// Only instances with a matching GamePlay::VisibilityFilter bit are rendered
	uint32_t filterMask = 0xffffffff;
};
