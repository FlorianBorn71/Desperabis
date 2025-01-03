#pragma once

#include <DesperabisCommon.h>

class Transformable
{
public:
    Transformable();
    void CreateWSMatrix(Matrix4& outMatrix) const;

public:
    Vector m_position;
    Vector m_rotation; // Euler
};

