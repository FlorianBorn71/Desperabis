#include "pch.h"
#include <Renderer/TextMesh.h>
#include <Renderer/Renderer.h>
#include <cmath>
#include <stdexcept>

std::shared_ptr<TextMesh> TextMesh::CreateTexturedQuad(
	const Texture& texture, float width, float height, float sourceWidthFraction, float sourceHeightFraction)
{
	if (texture.m_width == 0 || texture.m_height == 0 ||
		!std::isfinite(width) || !std::isfinite(height) || width <= 0.f || height <= 0.f ||
		!std::isfinite(sourceWidthFraction) || sourceWidthFraction <= 0.f || sourceWidthFraction > 1.f ||
		!std::isfinite(sourceHeightFraction) || sourceHeightFraction <= 0.f || sourceHeightFraction > 1.f)
	{
		throw std::invalid_argument("Screen-space quads require a nonempty texture, positive finite dimensions and source fractions in (0,1].");
	}
	auto mesh = std::make_shared<TextMesh>();
	mesh->m_extent = { width, height };
	auto& batch = mesh->m_batches.emplace_back();
	batch.m_atlasTexture = &texture;
	batch.m_useTextureColor = true;
	const float halfTexelU = 0.5f / texture.m_width;
	const float halfTexelV = 0.5f / texture.m_height;
	const Vector2 uvTopLeft = texture.m_atlasData.Apply(halfTexelU, halfTexelV);
	const Vector2 uvBottomRight = texture.m_atlasData.Apply(
		(std::max)(halfTexelU, sourceWidthFraction - halfTexelU),
		(std::max)(halfTexelV, sourceHeightFraction - halfTexelV));
	const Color4 white{ 255,255,255,255 };
	batch.m_vertices = {
		{ {0.f,0.f}, uvTopLeft, white },
		{ {width,0.f}, {uvBottomRight.x,uvTopLeft.y}, white },
		{ {0.f,height}, {uvTopLeft.x,uvBottomRight.y}, white },
		{ {width,height}, uvBottomRight, white } };
	batch.m_indices = { 0,1,2,2,1,3 };
	return mesh;
}

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
