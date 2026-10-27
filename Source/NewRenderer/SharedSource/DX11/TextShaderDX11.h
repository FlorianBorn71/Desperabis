#pragma once
#include <DesperabisCommon.h>
#include <DX11/DX11Decls.h>
#include <Renderer/IGraphicsRuntimeObject.h>
#include <Renderer/TextMesh.h>

struct TextShaderConstants
{
	Float4Storage m_screenAndPosition;
	Float4Storage m_tintColor;
	Float4Storage m_scale;
};

struct NativeTextMeshBatchDX11
{
	ComPtr<ID3D11Buffer> m_vertexBuffer;
	ComPtr<ID3D11Buffer> m_indexBuffer;
	const Texture* m_atlasTexture = nullptr;
	UINT m_indexCount = 0;
};

class NativeTextMeshObjectDX11 : public IGraphicsRuntimeObject
{
public:
	std::vector<NativeTextMeshBatchDX11> m_batches;
};

class TextShaderDX11
{
public:
	FUTURE(void) Create(ID3D11Device3* device);
	void Deinitialize();
	void Bind(ID3D11DeviceContext3* context);
	void UpdateConstants(ID3D11DeviceContext3* context, float screenWidth, float screenHeight, const Vector2& position, const Color4& tint, float scale);

private:
	ComPtr<ID3D11VertexShader> m_vertexShader;
	ComPtr<ID3D11PixelShader> m_pixelShader;
	ComPtr<ID3D11InputLayout> m_inputLayout;
	ComPtr<ID3D11Buffer> m_constantBuffer;
};

#include <DX11/TextShaderDX11.inl>
