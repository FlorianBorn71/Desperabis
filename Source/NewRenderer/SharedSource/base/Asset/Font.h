#pragma once
#include <DesperabisCommon.h>
#include <Renderer/TextMesh.h>
#include <string_view>
#include <unordered_map>

class D_IMPEXP Font
{
public:
	enum class HorizontalAlignment
	{
		Left,
		Center,
		Right
	};

	enum class VerticalAlignment
	{
		Top,
		Center,
		Bottom
	};

	void AddGlyphs(TextureAnim* glyphs, std::string_view characterMapping);
	void SetSpaceWidth(float width);
	std::shared_ptr<TextMesh> CreateGlyphMesh(std::string_view text, const Color4& color,
		HorizontalAlignment horizontalAlignment = HorizontalAlignment::Left,
		VerticalAlignment verticalAlignment = VerticalAlignment::Top) const;

private:
	struct Glyph
	{
		const TextureAnim* m_animation;
		const TextureAnimFrame* m_frame;
	};

	std::unordered_map<char, Glyph> m_glyphs;
	float m_spaceWidth = 4.f;
};
