#pragma once
#include <DesperabisCommon.h>
#include <Renderer/DrawCall.h>
#include <Asset/ModelGeometry.h>

class Renderable;

struct SubRenderable
{
    Vector m_localPos, m_localOri;
    const Renderable* m_renderable = nullptr;
};

class Renderable
{
public:
    inline bool IsValid() const
    {
        return m_drawCalls.size() > 0 || m_subRenderables.size() > 0;
    }

    inline bool HasOwnDrawcalls() const 
    {
        return m_drawCalls.size() > 0;
    }

    template<typename T>
    inline T* GetNativeRuntimeObj() const { return static_cast<T*>(m_runtime.get()); }

public:
    std::vector<SubRenderable> m_subRenderables;
    std::shared_ptr<IGraphicsRuntimeObject> m_runtime;
    std::shared_ptr<PhysicsShape> m_physicsShape;

    std::vector<DrawCall> m_drawCalls;
    int m_vertexOfs = 0;
	uint16_t m_positionOfs = 0;
	uint16_t m_polygonOfs = 0;
	uint16_t m_numPositions = 0;
	uint16_t m_numPolygons = 0;

    BoundingVolume m_localBounds;
};

