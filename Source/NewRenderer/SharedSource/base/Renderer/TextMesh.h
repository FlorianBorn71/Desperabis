#pragma once
#include <DesperabisCommon.h>
#include <Renderer/IGraphicsRuntimeObject.h>
#include <Asset/TextureAnim.h>

struct TextVertex
{
	Vector2 m_position;
	Vector2 m_texCoord;
	Color4 m_color;
};

struct TextMeshBatch
{
	const TextureAnim* m_animation = nullptr;
	const Texture* m_atlasTexture = nullptr;
	std::vector<TextVertex> m_vertices;
	std::vector<uint32_t> m_indices;
};

class D_IMPEXP TextMesh
{
public:
	const Vector2& GetExtent() const { return m_extent; }
	const std::vector<TextMeshBatch>& GetBatches() const { return m_batches; }
	bool IsValid() const { return !m_batches.empty(); }
	void CreateRuntimes(Renderer& renderer);

	template<typename T>
	T* GetNativeRuntimeObj() const { return static_cast<T*>(m_runtime.get()); }

private:
	friend class Font;
	std::vector<TextMeshBatch> m_batches;
	Vector2 m_extent = {};
	std::shared_ptr<IGraphicsRuntimeObject> m_runtime;
};

class D_IMPEXP TextMeshInstance
{
public:
	explicit TextMeshInstance(std::shared_ptr<TextMesh> mesh)
		: m_mesh(std::move(mesh))
	{
	}

	const std::shared_ptr<TextMesh>& GetMesh() const { return m_mesh; }
	void SetPosition(float x, float y) { m_position = { x, y }; }
	const Vector2& GetPosition() const { return m_position; }
	void SetScale(float scale) { m_scale = scale; }
	float GetScale() const { return m_scale; }
	void SetTintColor(const Color4& color) { m_tintColor = color; }
	const Color4& GetTintColor() const { return m_tintColor; }
	void SetVisible(bool visible) { m_visible = visible; }
	bool GetVisible() const { return m_visible; }

private:
	std::shared_ptr<TextMesh> m_mesh;
	Vector2 m_position = {};
	float m_scale = 2.f;
	Color4 m_tintColor = { 255,255,255,255 };
	bool m_visible = true;
};
