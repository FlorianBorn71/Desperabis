#pragma once
#include <DesperabisCommon.h>

class IGraphicsRuntimeObject
{
public:
    uint32_t m_Id = 0; // assigned by renderer for draw call sorting
};

class IRenderTarget
{
public:

    int m_width, m_height;
};
