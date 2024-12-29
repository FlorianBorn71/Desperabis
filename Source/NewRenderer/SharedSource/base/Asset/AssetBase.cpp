#include "pch.h"
#include <Asset/AssetBase.h>
#include <algorithm>

int AssetStream::Read(char* dest, int count)
{
    count = __min(count, (int)m_rawBuffer.size() - m_currentPos);
    if (count == 0)
        return 0;
    memcpy(dest, &m_rawBuffer[m_currentPos], count);
    m_currentPos += count;
    return count;
}
