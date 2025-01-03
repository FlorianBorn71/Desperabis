#pragma once
#include <DesperabisCommon.h>

struct GlobalSettings
{
public:
	enum class AssetQuality
	{
		Original,
		WithHighRes,
		OnlyHighRes,
		COUNT
	};

	// global tweakables:

	AssetQuality assetQuality = AssetQuality::Original;
	bool useFog = true;
	bool fullbrightRendering = false;
	bool dynamicLights = true;
	bool renderCoronas = true;
	bool gridVisibility = true;
	float gamma = 1.0f;

	static GlobalSettings& Instance();

	/// Needs to be called before anything is loaded
	static void SetRelativeGeometryScaling(float scale);
};

