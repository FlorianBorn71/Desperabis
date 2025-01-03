#include "pch.h"
#include <Asset/Model.h>
#include <Asset/AssetBase.h>
#include <Asset/ModelGeometry.h>

void Model::Read(AssetStream& s, ModelLoadingContext& context)
{
    m_numPositions = s.Read<uint16_t>();
    m_numPolygons = s.Read<uint16_t>();

    auto& vertices(context.m_vertexPos);
    m_vertexOfs = (int)vertices.size();
    vertices.resize(m_vertexOfs + m_numPositions);
    VertexPositionI16 rawV;
    const int r = 30;
    const int edge = 500;
    for (int i = 0; i < m_numPositions; i++)
    {
        s.Read((char *)&rawV, sizeof(VertexPositionI16));

        // clamp ground tiles to edge to avoid overlap with z-fighting
        if (rawV.x >= (edge - r) && rawV.x <= (edge + r)) rawV.x = edge;
        if (rawV.x >= (-edge - r) && rawV.x <= (-edge + r)) rawV.x = -edge;
        if (rawV.z >= (edge - r) && rawV.z <= (edge + r)) rawV.z = edge;
        if (rawV.z >= (-edge - r) && rawV.z <= (-edge + r)) rawV.z = -edge;

        vertices[m_vertexOfs + i] = rawV;
    }
    s.Read(context.m_polygons, m_numPolygons);
}
