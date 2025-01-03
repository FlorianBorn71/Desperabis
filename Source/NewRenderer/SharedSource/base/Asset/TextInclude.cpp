#include "pch.h"
#include <Asset/TextInclude.h>

void TextInclude::Read(AssetStream& stream)
{
	stream.ReadText(m_text);
}
