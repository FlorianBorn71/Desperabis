#pragma once

#include <ApplicationFlow.h>
#include <GameContext.h>
#include <Renderer/TextMesh.h>
#include <Renderer/RenderingSettings.h>
#include <DX11/DX11Decls.h>
#include <functional>
#include <map>

class RendererDX11;
class ModelAnim;
class RenderableInstance;

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
    // Called only when a map is visible. Clears the core's dirty flag after a successful upload.
    bool UploadCollisionMap(Desperabis::GameContext& core, Renderer& renderer);
    bool UploadGeometryMap(const Desperabis::GameContext& core, Renderer& renderer);
    // Reports each failed plane independently; false does not prevent subsequent object presentation.
    static bool SynchronizeLevelGeometry(Level& level, const Desperabis::Level& geometry,
        const std::function<void(const char*, const char*)>& reportError);
    static void SynchronizeObjectPresentations(const std::function<void(int)>& update,
        const std::function<void(int, const char*)>& reportError);
    void ApplyMapMaterial(RenderableInstance& instance, const ModelAnim& model, Renderer& renderer);
    const Texture* GetGeometryMapTexture() const { return m_geometryMap.get(); }
    static std::vector<std::shared_ptr<TextMeshInstance>> CreateVitalMeters(
        const Texture& healthTexture, const Desperabis::UiRectangle& bounds,
        float healthFraction, std::optional<float> oxygenFraction);
    void DrawWeapon(const Desperabis::WeaponView& view, RendererDX11& renderer, const Level& level,
        const RenderingSettings& settings, ProfilingStats& stats, const D3D11_VIEWPORT& viewport,
        ID3D11DeviceContext3* deviceContext, ID3D11RenderTargetView* colorView, ID3D11DepthStencilView* depthView,
        const VisibilityResult* world = nullptr);
    static Vector ElectricityTipWorld(const Desperabis::WeaponView& view,
        const Desperabis::ModelAnchorGeometry& geometry, const Camera& camera, const D3D11_VIEWPORT& viewport);
    void DrawBossFall(const std::vector<Desperabis::RawGameObject>& objects, RendererDX11& renderer,
        const Level& level, const RenderingSettings& settings, ProfilingStats& stats,
        const D3D11_VIEWPORT& viewport, ID3D11DeviceContext3* deviceContext,
        ID3D11RenderTargetView* colorView, ID3D11DepthStencilView* depthView);
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
    std::shared_ptr<TextureAnim> m_weapons;
    std::shared_ptr<ModelAnim> m_scytheModel;
    std::array<std::array<std::shared_ptr<TextMesh>, 2>, 12> m_weaponMeshes;
    CachedMesh m_heldWeaponMesh;
    const Texture* m_heldWeaponTexture = nullptr;
    std::shared_ptr<Texture> m_mapPaper, m_collisionMap;
    std::shared_ptr<Texture> m_menuBackground;
    std::shared_ptr<TextureAnim> m_menuCursor;
    std::shared_ptr<TextMesh> m_textCaret;
    std::vector<Color4> m_mapPaperPixels;
    std::shared_ptr<Texture> m_geometryMap;
    const Desperabis::Level* m_geometryMapSource = nullptr;
    std::optional<std::int16_t> m_geometryMapLevelNumber;
    std::uint64_t m_geometryMapRevision = 0, m_geometryMapGeneration = 0;
    std::map<const ModelAnim*, std::shared_ptr<Texture>> m_mapMaterialViews;
    const Desperabis::CollisionMap* m_mapSource = nullptr;
    std::uint64_t m_mapGeometryRevision = 0, m_mapRevision = 0;
};
