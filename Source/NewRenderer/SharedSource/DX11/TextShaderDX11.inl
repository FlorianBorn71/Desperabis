#pragma once
#include <IO/FileManager.h>
#include <stdexcept>

inline FUTURE(void) TextShaderDX11::Create(ID3D11Device3* device)
{
	const std::vector<byte> vertexShader = CO_AWAIT FileManager::Instance().ReadDataFuture(L"Text_VS.cso");
	const std::vector<byte> pixelShader = CO_AWAIT FileManager::Instance().ReadDataFuture(L"Text_PS.cso");
	if (vertexShader.empty() || pixelShader.empty())
	{
		throw std::runtime_error("Precompiled text shader assets are missing.");
	}

	HRESULT result = device->CreateVertexShader(vertexShader.data(), vertexShader.size(), nullptr, &m_vertexShader);
	if (FAILED(result))
	{
		throw std::runtime_error("Failed to create the text vertex shader.");
	}
	result = device->CreatePixelShader(pixelShader.data(), pixelShader.size(), nullptr, &m_pixelShader);
	if (FAILED(result))
	{
		throw std::runtime_error("Failed to create the text pixel shader.");
	}

	const D3D11_INPUT_ELEMENT_DESC vertexDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(TextVertex, m_position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(TextVertex, m_texCoord), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(TextVertex, m_color), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	result = device->CreateInputLayout(vertexDesc, ARRAYSIZE(vertexDesc), vertexShader.data(), vertexShader.size(), &m_inputLayout);
	if (FAILED(result))
	{
		throw std::runtime_error("Failed to create the text input layout.");
	}

	const CD3D11_BUFFER_DESC constantBufferDesc(sizeof(TextShaderConstants), D3D11_BIND_CONSTANT_BUFFER);
	result = device->CreateBuffer(&constantBufferDesc, nullptr, &m_constantBuffer);
	if (FAILED(result))
	{
		throw std::runtime_error("Failed to create the text constant buffer.");
	}
}

inline void TextShaderDX11::Deinitialize()
{
	m_vertexShader.Reset();
	m_pixelShader.Reset();
	m_inputLayout.Reset();
	m_constantBuffer.Reset();
}

inline void TextShaderDX11::Bind(ID3D11DeviceContext3* context)
{
	context->IASetInputLayout(m_inputLayout.Get());
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
	context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
	context->VSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
	context->PSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
}

inline void TextShaderDX11::UpdateConstants(ID3D11DeviceContext3* context, float screenWidth, float screenHeight, const Vector2& position, const Color4& tint, float scale)
{
	TextShaderConstants constants = {};
	constants.m_screenAndPosition = { screenWidth, screenHeight, position.x, position.y };
	tint.ToFloat(&constants.m_tintColor.x);
	constants.m_scale = { scale, 0.f, 0.f, 0.f };
	context->UpdateSubresource(m_constantBuffer.Get(), 0, nullptr, &constants, 0, 0);
}
