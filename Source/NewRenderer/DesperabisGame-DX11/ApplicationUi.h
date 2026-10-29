#pragma once

#include <ApplicationFlow.h>
#include <Renderer/TextMesh.h>
#include <Renderer/RenderingSettings.h>
#include <DX11/DX11Decls.h>
#include <map>

class RendererDX11;

// Executes the core's drawing recipe; owns no page/input/inventory decisions.
class ApplicationUi
{
public:
    static constexpr std::array<std::string_view, 3> FontCharacters{
        "abcdefghijklmnopqrstuvwxyz\x94\x81\x84\xe1",
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ\x99\x9a\x8e?",
        "0123456789.:>/-\xb3~\"!$<,+\xef^#*%" };

    FUTURE(void) Initialize();
    void Deinitialize();
    void Clear();
    void Draw(const Desperabis::ApplicationPresentation& presentation, RendererDX11& renderer,
        const Level& level, const Font& font, const TextureAnim& hud,
        const RenderingSettings& settings, ProfilingStats& stats, const D3D11_VIEWPORT& viewport,
        ID3D11DeviceContext3* deviceContext, ID3D11RenderTargetView* colorView, ID3D11DepthStencilView* depthView);

private:
    struct CachedMesh
    {
        std::string key;
        std::shared_ptr<TextMesh> mesh;
        std::shared_ptr<Texture> texture;
    };
    std::vector<CachedMesh> m_meshes;
    struct NativeGlyph
    {
        int advance, offsetY;
        std::shared_ptr<TextMesh> mesh;
    };
    std::map<char, NativeGlyph> m_nativeGlyphs;
    std::array<std::shared_ptr<TextureAnim>, 7> m_hud;
    std::array<std::shared_ptr<TextureAnim>, 7> m_mirroredHud;
    std::map<int, std::array<std::shared_ptr<TextureAnim>, 7>> m_inventoryIcons;
    std::array<std::array<byte, 256>, 8> m_shades{};
    std::array<byte, 90> m_healthGradient{};
};
