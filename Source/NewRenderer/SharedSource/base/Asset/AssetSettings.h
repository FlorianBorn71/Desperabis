#pragma once
#include <DesperabisCommon.h>

/// <summary>
/// Global settings that affect how assets are loaded/selected.
/// </summary>
struct AssetSettings
{
public:
	enum class AssetQuality
	{
		Original,
		WithHighRes,
		OnlyHighRes,
		COUNT
	};

	AssetQuality assetQuality = AssetQuality::Original;

	static AssetSettings& Instance();
};
