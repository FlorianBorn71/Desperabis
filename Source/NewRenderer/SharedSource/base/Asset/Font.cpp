#include "pch.h"
#include <Asset/Font.h>
#include <Renderer/Renderer.h>
#include <cmath>
#include <stdexcept>

void Font::AddGlyphs(TextureAnim* glyphs, std::string_view characterMapping)
{
	if (glyphs == nullptr)
	{
		throw std::invalid_argument("Glyph animation must not be null.");
	}

	if (characterMapping.size() > glyphs->m_frames.size())
	{
		throw std::invalid_argument("Character mapping has more entries than the glyph animation.");
	}

	for (size_t i = 0; i < characterMapping.size(); ++i)
	{
		const int animNr = static_cast<int>(i / glyphs->m_countY);
		const int frameNr = static_cast<int>(i % glyphs->m_countY);
		m_glyphs[characterMapping[i]] = { glyphs, glyphs->GetFrame(animNr, frameNr) };
	}
}

void Font::SetSpaceWidth(float width)
{
	if (!std::isfinite(width) || width < 0.f)
	{
		throw std::invalid_argument("Space width must be finite and non-negative.");
	}
	m_spaceWidth = width;
}

std::shared_ptr<TextMesh> Font::CreateGlyphMesh(std::string_view text, const Color4& color,
	HorizontalAlignment horizontalAlignment, VerticalAlignment verticalAlignment) const
{
	auto mesh = std::make_shared<TextMesh>();
	float penX = 0.f;
	float minX = 0.f;
	float maxX = 0.f;
	float minY = 0.f;
	float maxY = 0.f;
	bool hasGlyphBounds = false;

	for (char character : text)
	{
		const auto glyphIt = m_glyphs.find(character);
		if (glyphIt == m_glyphs.end())
		{
			if (character == ' ')
			{
				penX += m_spaceWidth;
				maxX = __max(maxX, penX);
			}
			continue;
		}

		const Glyph& glyph = glyphIt->second;
		const TextureAnimFrame& frame = *glyph.m_frame;
		if (frame.m_width > 0 && frame.m_height > 0)
		{
			auto batchIt = std::find_if(mesh->m_batches.begin(), mesh->m_batches.end(), [&glyph](const TextMeshBatch& batch)
			{
				return batch.m_animation == glyph.m_animation;
			});
			if (batchIt == mesh->m_batches.end())
			{
				TextMeshBatch& batch = mesh->m_batches.emplace_back();
				batch.m_animation = glyph.m_animation;
				batch.m_atlasTexture = &frame;
				batchIt = mesh->m_batches.end() - 1;
			}

			const float left = penX + static_cast<float>(frame.m_relX);
			const float top = static_cast<float>(frame.m_relY);
			const float right = left + static_cast<float>(frame.m_width);
			const float bottom = top + static_cast<float>(frame.m_height);
			const uint32_t firstVertex = static_cast<uint32_t>(batchIt->m_vertices.size());
			const Vector2 uvTopLeft = frame.m_atlasData.Apply(0.f, 0.f);
			const Vector2 uvBottomRight = frame.m_atlasData.Apply(1.f, 1.f);

			batchIt->m_vertices.push_back({ { left, top }, uvTopLeft, color });
			batchIt->m_vertices.push_back({ { right, top }, { uvBottomRight.x, uvTopLeft.y }, color });
			batchIt->m_vertices.push_back({ { left, bottom }, { uvTopLeft.x, uvBottomRight.y }, color });
			batchIt->m_vertices.push_back({ { right, bottom }, uvBottomRight, color });
			batchIt->m_indices.insert(batchIt->m_indices.end(),
				{ firstVertex, firstVertex + 1, firstVertex + 2, firstVertex + 2, firstVertex + 1, firstVertex + 3 });

			if (!hasGlyphBounds)
			{
				minX = left;
				maxX = right;
				minY = top;
				maxY = bottom;
				hasGlyphBounds = true;
			}
			else
			{
				minX = __min(minX, left);
				maxX = __max(maxX, right);
				minY = __min(minY, top);
				maxY = __max(maxY, bottom);
			}
		}

		penX += static_cast<float>(frame.m_width);
		minX = __min(minX, penX);
		maxX = __max(maxX, penX);
	}

	float anchorX;
	switch (horizontalAlignment)
	{
	case HorizontalAlignment::Left: anchorX = minX; break;
	case HorizontalAlignment::Center: anchorX = (minX + maxX) * 0.5f; break;
	case HorizontalAlignment::Right: anchorX = maxX; break;
	default: throw std::invalid_argument("Invalid horizontal text alignment.");
	}

	float anchorY;
	switch (verticalAlignment)
	{
	case VerticalAlignment::Top: anchorY = minY; break;
	case VerticalAlignment::Center: anchorY = (minY + maxY) * 0.5f; break;
	case VerticalAlignment::Bottom: anchorY = maxY; break;
	default: throw std::invalid_argument("Invalid vertical text alignment.");
	}

	if (hasGlyphBounds || penX > 0.f)
	{
		mesh->m_extent = { maxX - minX, maxY - minY };
		for (TextMeshBatch& batch : mesh->m_batches)
		{
			for (TextVertex& vertex : batch.m_vertices)
			{
				vertex.m_position.x -= anchorX;
				vertex.m_position.y -= anchorY;
			}
		}
	}

	if (Renderer::IsReady())
	{
		mesh->CreateRuntimes(Renderer::Instance());
	}
	return mesh;
}
