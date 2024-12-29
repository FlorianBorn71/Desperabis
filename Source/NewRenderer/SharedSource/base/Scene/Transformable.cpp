#include "pch.h"
#include <Scene/Transformable.h>

Transformable::Transformable()
{
}

void Transformable::CreateWSMatrix(Matrix4& outMatrix) const
{
    outMatrix.Create(m_position, m_rotation);
}
