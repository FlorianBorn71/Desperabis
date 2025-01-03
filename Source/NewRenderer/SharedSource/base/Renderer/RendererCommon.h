#pragma once
#include <DesperabisCommon.h>

class Renderer;

enum class GeometryPassType
{
    Sky,
    Instances,
    MirrorReceiver,
    InstancesInFlames
};

enum class RenderTargetPassType
{
    MainView,
    Mirror,
};

// find a better place
struct TextureAtlasCBData
{
	Float4Storage m_atlasTransform;
	Float4Storage m_spriteParams;
};