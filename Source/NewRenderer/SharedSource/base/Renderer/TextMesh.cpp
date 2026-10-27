#include "pch.h"
#include <Renderer/TextMesh.h>
#include <Renderer/Renderer.h>
#include <stdexcept>

void TextMesh::CreateRuntimes(Renderer& renderer)
{
	if (!IsValid() || m_runtime != nullptr)
	{
		return;
	}

	m_runtime = renderer.CreateTextMeshBuffers(*this);
	if (m_runtime == nullptr)
	{
		throw std::runtime_error("The active renderer could not create text mesh buffers.");
	}
}
