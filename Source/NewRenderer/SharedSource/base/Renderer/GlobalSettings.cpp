#include "pch.h"
#include <Renderer/GlobalSettings.h>

static GlobalSettings g_Settings;

float DespRelativeScaling = 1.0f; // for all the distances/values that are already tweaked with the defaults below
float DespRelativeInvScaling = 1.0f; // for all the distances/values that are already tweaked with the defaults below
float DespToFloat = 0.005f; // world positions from short->float
float DespVertToFloat = DespToFloat * 0.1f;   // vertex positions
float DespSpriteToFloat = DespToFloat / 64.f;   // sprite texture extent


GlobalSettings& GlobalSettings::Instance()
{
	return g_Settings;
}

void GlobalSettings::SetRelativeGeometryScaling(float scale)
{
	DespRelativeScaling = scale;
	DespRelativeInvScaling = 1.f / DespRelativeScaling;
	DespToFloat = 0.005f * DespRelativeScaling; // world positions from short->float
	DespVertToFloat = DespToFloat * 0.1f;   // vertex positions
	DespSpriteToFloat = DespToFloat / 64.f;   // sprite texture extent
}
