#pragma once
#include <DesperabisCommon.h>
#include <Asset/ModelGeometry.h>

struct DebugLine
{
    DebugLine(const Vector& pos1, const Vector& pos2, Color4 col, float width = 1.f)
        :m_p1(pos1), m_p2(pos2), m_color(col), m_width(width)
    {

    }
    Vector m_p1, m_p2;
    Color4 m_color;
    float m_width;
};
