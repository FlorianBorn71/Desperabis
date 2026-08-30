#include "pch.h"
#include <Asset/AssetSettings.h>

static AssetSettings g_AssetSettings;

AssetSettings& AssetSettings::Instance()
{
	return g_AssetSettings;
}
