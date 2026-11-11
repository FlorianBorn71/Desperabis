#include "pch.h"
#include "ApplicationUi.h"

#include <GameContext.h>
#include <Asset/AssetManager.h>
#include <Asset/Font.h>
#include <Asset/Level.h>
#include <Asset/ModelAnim.h>
#include <Asset/Palette.h>
#include <DX11/RendererDX11.h>
#include <DX11/RenderContextDX11.h>
#include <Renderer/Camera.h>
#include <Renderer/RenderableInstance.h>
#include <Sample/LevelScene.h>
#include <Renderer/LightSource.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

void ApplicationUi::SynchronizeSubmarineView(const Desperabis::GameContext& core, LevelScene& scene,
    const std::array<std::shared_ptr<RenderableInstance>, 166>& instances, bool firstPerson)
{
    const auto& objects = core.GetWorldObjects();
    const int slot = core.controlledVehicleSlot;
    const bool occupied = firstPerson && slot >= 2 && slot <= 164 && objects[slot].health > 0 &&
        objects[slot].behavior == Desperabis::BehaviorType::Submarine && objects[slot].doorLockOrGroupBitmask > 0;
    for (std::size_t index = 0; index < instances.size(); ++index)
    {
        if (!instances[index]) { continue; }
        const auto& object = objects[index];
        instances[index]->m_foreground = occupied && object.health > 0 &&
            (index == slot || (object.renderPlacementMode == 20 && object.doorLockOrGroupBitmask == slot));
    }
    if (const auto& light = scene.GetPlayerLight())
    {
        const auto mode = occupied ? PlayerLight::Mode::Submarine :
            (core.playerUnderwater ? PlayerLight::Mode::Blue : PlayerLight::Mode::Normal);
        if (light->GetMode() != mode) { light->SetMode(mode); }
    }
}

void ApplicationUi::SynchronizeObjectPresentations(const std::function<void(int)>& update,
    const std::function<void(int, const char*)>& reportError)
{
    if (!update || !reportError) { throw std::invalid_argument("Object synchronization requires update and error callbacks."); }
    for (int slot = 2; slot <= 164; ++slot)
    {
        try { update(slot); }
        catch (const std::exception& error) { reportError(slot, error.what()); }
    }
}

bool ApplicationUi::SynchronizeLevelGeometry(Level& level, const Desperabis::Level& geometry,
    const std::function<void(const char*, const char*)>& reportError)
{
    if (!reportError) { throw std::invalid_argument("Geometry synchronization requires an error reporter."); }
    bool succeeded = true;
    const auto synchronize = [&](const char* operation, auto method, const auto& tiles)
    {
        try
        {
            (level.*method)(&tiles[0][0],
                Desperabis::Level::Width, Desperabis::Level::Height, Desperabis::Level::Width);
        }
        catch (const std::exception& error)
        {
            succeeded = false;
            reportError(operation, error.what());
        }
    };
    synchronize("wall synchronization failed", &Level::SynchronizeWallTiles, geometry.wallTextures);
    synchronize("floor synchronization failed", &Level::SynchronizeFloorTiles, geometry.floorObjects);
    try
    {
        for (std::size_t slot = 0; slot < geometry.heightSurfaces.size(); ++slot)
        {
            if (const auto& surface = geometry.heightSurfaces[slot])
            {
                level.SynchronizeWaterHeight(static_cast<WaterSlotIndex>(slot + 1), -surface->depth * DespToFloat);
            }
        }
    }
    catch (const std::exception& error)
    {
        succeeded = false;
        reportError("water height synchronization failed", error.what());
    }
    synchronize("surface synchronization failed", &Level::SynchronizeSurfaceTiles, geometry.surfaceFlags);
    for (std::size_t slot = 0; slot < geometry.texturePixels.size(); ++slot)
    {
        const auto& texture = geometry.texturePixels[slot];
        if (texture.revision == 0) { continue; }
        try { level.SynchronizeTexturePixels(slot, texture.width, texture.height, texture.pixels, texture.revision); }
        catch (const std::exception& error)
        {
            succeeded = false;
            const auto operation = "texture synchronization failed [slot " + std::to_string(slot) + "]";
            reportError(operation.c_str(), error.what());
        }
    }
    return succeeded;
}

std::vector<std::shared_ptr<TextMeshInstance>> ApplicationUi::CreateVitalMeters(
    const Texture& healthTexture, const Desperabis::UiRectangle& bounds,
    float healthFraction, std::optional<float> oxygenFraction)
{
    if (!std::isfinite(bounds.x) || !std::isfinite(bounds.y) ||
        !std::isfinite(bounds.width) || !std::isfinite(bounds.height) ||
        bounds.width <= 0.f || bounds.height <= 0.f ||
        !std::isfinite(healthFraction) || healthFraction < 0.f || healthFraction > 1.f ||
        (oxygenFraction && (!std::isfinite(*oxygenFraction) || *oxygenFraction < 0.f || *oxygenFraction > 1.f)))
    {
        throw std::invalid_argument("Vital meters require positive bounds and fractions in 0..1.");
    }
    std::vector<std::shared_ptr<TextMeshInstance>> meters;
    const auto& white = *AssetManager::Instance().GetPlainWhiteTexture();
    const float height = oxygenFraction ? bounds.height * 0.5f : bounds.height;
    const float healthY = bounds.y + bounds.height - height;
    const auto quad = [&](const Texture& texture, float width, float y, Color4 tint,
        float uMax = 1.f, float vMax = 1.f)
    {
        auto instance = std::make_shared<TextMeshInstance>(
            TextMesh::CreateTexturedQuad(texture, width, height, uMax, vMax));
        instance->SetScale(1.f);
        instance->SetPosition(bounds.x, y);
        instance->SetTintColor(tint);
        meters.push_back(std::move(instance));
    };
    quad(white, bounds.width, healthY, Color4{ 24,24,24,255 });
    if (healthFraction > 0.f)
    {
        quad(healthTexture, bounds.width * healthFraction, healthY,
            Color4{ 255,255,255,255 }, healthFraction, 0.5f);
    }
    if (oxygenFraction)
    {
        quad(white, bounds.width, bounds.y, Color4{ 255,255,255,255 });
        if (*oxygenFraction > 0.f)
        {
            quad(white, bounds.width * *oxygenFraction, bounds.y, Color4{ 48,128,255,255 });
        }
    }
    return meters;
}

namespace
{
    class InventoryPalette final : public Palette
    {
    public:
        InventoryPalette(const Palette& original, const std::array<byte, 256>& lookup)
        {
            for (std::size_t i = 0; i < lookup.size(); ++i)
            {
                m_palette[i] = original.GetEntry(lookup[i]);
                m_palette[i].a = i == 0 ? 0 : 255;
            }
        }
    };

    struct BitmapFrame
    {
        const Texture& texture;
        float anchorX = 0, anchorY = 0;
    };

    BitmapFrame ResolveBitmap(const Desperabis::UiBitmap& command, const Level& level,
        const TextureAnim& hud, const TextureAnim* icons, const TextureAnim& cursor)
    {
        using Desperabis::UiBitmapSource;
        if (command.flipHorizontal && command.source != UiBitmapSource::Hud)
        {
            throw std::logic_error("Only the native HUD inventory art has mirrored variants.");
        }
        if (command.source == UiBitmapSource::ModelTexture)
        {
            const auto model = level.GetLevelInfo().GetModelAnim(command.assetSlot - 30);
            if (!model || command.frame >= 16 || (model->m_textureUsedMask & (1u << command.frame)) == 0)
            {
                throw std::runtime_error("Application UI references a missing model bitmap.");
            }
            return { model->m_ownTextures[command.frame] };
        }
        const TextureAnim* animation = command.source == UiBitmapSource::InventoryIcons ? icons :
            command.source == UiBitmapSource::MenuCursor ? &cursor : &hud;
        if (command.source == UiBitmapSource::LevelSprite)
        {
            if (command.shade != 0) { throw std::logic_error("Level-sprite UI pictures require their original palette."); }
            animation = level.GetLevelInfo().GetTextureAnim(command.assetSlot).get();
        }
        if (!animation || !animation->IsValidFrame(command.animation, command.frame))
        {
            throw std::runtime_error("Application UI references a missing bitmap frame.");
        }
        const auto* frame = animation->GetFrame(command.animation, command.frame);
        return { *frame, static_cast<float>(frame->m_relX), static_cast<float>(frame->m_relY) };
    }

    void DrawModel(const Desperabis::UiModel& command, const Level& level, RendererDX11& renderer,
        const RenderingSettings& settings, ProfilingStats& stats, const D3D11_VIEWPORT& viewport,
        ID3D11DeviceContext3* deviceContext, ID3D11RenderTargetView* colorView, ID3D11DepthStencilView* depthView,
        Texture* weaponTexture = nullptr, bool faceSwordCamera = false, Model* modelOverride = nullptr,
        bool clearDepth = true, const VisibilityResult::VisibleWaterLevel* water = nullptr,
        const Scene* animationScene = nullptr)
    {
        Desperabis::RawGameObject object;
        object.health = 1000;
        object.modelId = command.modelId;
        object.modelAnim = command.animation + 1;
        object.animFrame = command.frame + 1;
        object.modelFrame2 = command.blendFrame;
        object.blendWeightOrSpriteOpacity = command.blendWeight;
        object.displayType = command.displayType;
        object.winX = command.angles[0];
        object.winY = command.angles[1];
        object.winZ = command.angles[2];
        RawGameObject raw;
        Desperabis::EncodeGameObject(object, raw.m_rawBytes, sizeof(raw.m_rawBytes));
        auto instance = std::make_shared<RenderableInstance>();
        if (modelOverride)
        {
            instance->SetRenderable(modelOverride, nullptr);
            level.GetLevelInfo().AssignLevelTextures(*instance);
            instance->ComputeTransparencyMask();
        }
        else { level.ApplyRenderProperties(instance, raw); }
        if (!instance->IsValid() || !instance->GetVisible())
        {
            throw std::runtime_error("Application UI references an unavailable model frame: " +
                std::to_string(command.modelId) + "/" + std::to_string(command.animation + 1) +
                "/" + std::to_string(command.frame + 1) + ".");
        }
        const auto& bounds = instance->GetRenderable()->m_localBounds;
        const auto center = bounds.GetCenter();
        Matrix4 centering;
        centering.SetTranslation(Vector(-center.GetX(), -center.GetY(), -center.GetZ()));
        auto transform = GamePlay::PropertyAccessorBase(raw).GetTransform();
        transform.SetTranslation(Vector(0, 0, -command.depth * DespToFloat));
        if (command.cameraSpacePosition)
        {
            const auto& position = *command.cameraSpacePosition;
            transform.SetTranslation(Vector(position[0] * DespToFloat, -position[1] * DespToFloat, -position[2] * DespToFloat));
            if (faceSwordCamera)
            {
                auto axis = transform.GetDirection();
                axis.Normalize();
                auto toCamera = Vector(0,0,0) - transform.TransformPosition(center);
                auto right = toCamera.GetCross(axis);
                // An end-on sword has no preferred axial rotation.
                if (right.GetLength() > 0.000001f)
                {
                    right.Normalize();
                    const auto up = axis.GetCross(right);
                    transform.Create(transform.GetTranslation(), right, up, axis);
                }
            }
            instance->SetTransform(transform);
        }
        else { instance->SetTransform(centering * transform); }
        instance->PrepareSubInstancesForRendering();
        const bool heldWater = command.cameraSpacePosition.has_value();
        const auto configureMaterials = [&command, weaponTexture, heldWater](RenderableInstance& item)
        {
            item.m_tintColor = { command.color.r,command.color.g,command.color.b,item.m_tintColor.a };
            for (auto& material : item.GetOrCreateMaterialOverrides())
            {
                if (weaponTexture && material.m_textureSlot == 9)
                {
                    material.m_texture = weaponTexture;
                }
                if (material.m_transparencyType == TransparencyMode::Water && !heldWater)
                {
                    // UI cameras have no world reflection pass; retain the polygon's authored tint.
                    material.m_transparencyType = TransparencyMode::Opaque;
                    material.m_reflectionType = WaterReflectionType::Disabled;
                }
                material.m_applyLighting = false;
                material.ComputeHash();
            }
        };
        configureMaterials(*instance);
        for (auto& sub : instance->m_subInstances) { configureMaterials(sub); }
        if (animationScene) { animationScene->ApplyFireAnimationFrames(*instance); }
        instance->ComputeTransparencyMask();
        Camera camera;
        Matrix4 projection;
        projection.CreateProjection(command.bounds.width / command.bounds.height,
            2.f * std::atan(command.bounds.height / (2.f * command.focalLength)), 0.001f, 100.f);
        if (command.projectionCenterY)
        {
            float matrix[16];
            projection.Store(matrix);
            matrix[9] = 2.f * *command.projectionCenterY / command.bounds.height - 1.f;
            projection.Load(matrix);
        }
        camera.SetProjectionMatrix(projection);
        VisibilityResult elements;
        elements.m_mainCamera = &camera;
        elements.m_ambientColor = { 1,1,1 };
        elements.m_lightmap = AssetManager::Instance().GetPlainWhiteTexture();
        elements.m_fogFalloff = 0.f;
        elements.m_fogColor = {0,0,0};
        RenderContextDX11 context(renderer, elements, settings, stats);
        context.BeginRendering();
        context.BeginRenderTargetScope(RenderTargetPassType::MainView);
        context.TrackGlobals();
        context.SetGeometryPassType(GeometryPassType::Instances);
        // Model pictures need private depth, but must retain earlier UI color.
        if (clearDepth) { deviceContext->ClearDepthStencilView(depthView, D3D11_CLEAR_DEPTH, 1.f, 0); }
        deviceContext->OMSetRenderTargets(1, &colorView, depthView);
        deviceContext->RSSetViewports(1, &viewport);
        const auto draw = [&context](const RenderableInstance& item)
        {
            if (!item.GetRenderable()->HasOwnDrawcalls()) { return; }
            for (const auto mode : { TransparencyMode::Opaque, TransparencyMode::AlphaBlended, TransparencyMode::Additive })
            {
                context.SetBlendState(mode);
                context.RenderInstance<GeometryPassType::Instances>(&item, mode);
            }
        };
        draw(*instance);
        for (const auto& sub : instance->m_subInstances) { draw(sub); }
        if (heldWater && instance->HasWaterMaterials())
        {
            VisibilityResult::VisibleWaterLevel refracted;
            refracted.m_scrollParams1 = { 2,3.4f,0,0 };
            refracted.m_scrollParams2 = { 2.62f,3.82f,0,0 };
            if (!water) { context.PrepareRefraction(true); }
            context.BeginWaterRendering();
            context.SetGeometryPassType(GeometryPassType::MirrorReceiver);
            context.SetBlendState(TransparencyMode::Water);
            context.PrepareWaterRendering(water ? *water : refracted);
            context.RenderInstance<GeometryPassType::MirrorReceiver>(instance.get(), TransparencyMode::Water);
            context.EndWaterRendering();
        }
        context.EndRenderTargetScope();
        context.EndRendering();
    }
}

FUTURE(void) ApplicationUi::Initialize()
{
    const auto read = [](std::wstring_view name)
    {
        return FileManager::Instance().ReadDataFuture(FileManager::Instance().FilenameToNative(name));
    };
    const auto shades = CO_AWAIT read(L"LEVEL\\OPSET10.256");
    const auto hudData = CO_AWAIT read(L"ANIMS\\AMA_ANI.AN2");
    const auto paperPalette = CO_AWAIT AssetManager::Instance().LoadAsset<Palette>(L"GRAFIK\\BACKGR3.PAL");
    m_mapPaper = CO_AWAIT AssetManager::Instance().LoadAsset<Texture>(L"GRAFIK\\BACKGR3.256", paperPalette);
    if (m_mapPaper->m_width != 360 || m_mapPaper->m_height != 240)
    {
        throw std::runtime_error("Expected the original 360x240 map parchment.");
    }
    const auto paper = CO_AWAIT read(L"GRAFIK\\BACKGR3.256");
    if (paper.size() != 4 + 360 * 240)
    {
        throw std::runtime_error("Invalid original map parchment pixels.");
    }
    m_mapPaperPixels.resize(360 * 240);
    for (std::size_t i = 0; i < m_mapPaperPixels.size(); ++i)
    {
        m_mapPaperPixels[i] = paperPalette->GetEntryCustomAlpha(paper[i + 4], 255);
    }
    m_weapons = CO_AWAIT AssetManager::Instance().LoadAsset<TextureAnim>(L"ANIMS\\WAFFEN.AN2");
    m_scytheModel = CO_AWAIT AssetManager::Instance().LoadAsset<ModelAnim>(L"VEKT\\VEKT67.3DA");
    if (m_weapons->m_countX != 4 || m_weapons->m_countY != 3)
    {
        throw std::runtime_error("Expected the original 4x3 first-person weapon bitmap bank.");
    }
    if (shades.size() != 2052 || shades[0] != 255 || shades[1] != 0 || shades[2] != 7 || shades[3] != 0 ||
        hudData.size() < 2 || hudData[0] != 2 || hudData[1] != 5)
    {
        throw std::runtime_error("Missing or invalid original inventory art/palette tables.");
    }
    for (std::size_t i = 0; i < m_shades.size(); ++i)
    {
        std::copy_n(shades.begin() + 4 + i * 256, 256, m_shades[i].begin());
    }
    auto mirrored = hudData;
    std::size_t offset = 2;
    for (int frame = 0; frame < 10; ++frame)
    {
        if (offset + 8 > mirrored.size()) { throw std::runtime_error("Truncated inventory HUD frame header."); }
        const auto width = 1u + mirrored[offset + 4] + (mirrored[offset + 5] << 8);
        const auto height = 1u + mirrored[offset + 6] + (mirrored[offset + 7] << 8);
        offset += 8;
        if (width > 1024 || height > 1024 || offset + width * height > mirrored.size())
        {
            throw std::runtime_error("Invalid inventory HUD frame dimensions.");
        }
        if (frame == 5)
        {
            if (width != 90 || height != 2) { throw std::runtime_error("Expected the original 90x2 health gradient."); }
            std::copy_n(hudData.begin() + offset, 90, m_healthGradient.begin());
        }
        for (std::size_t y = 0; y < height; ++y)
        {
            auto row = mirrored.begin() + offset + y * width;
            std::reverse(row, row + width);
        }
        offset += width * height;
    }
    const auto animation = [](const std::vector<byte>& bytes, std::shared_ptr<const Palette> palette)
    {
        auto asset = std::make_shared<TextureAnim>();
        AssetStream stream(bytes, std::move(palette));
        asset->Read(stream);
        return asset;
    };
    std::array<std::shared_ptr<InventoryPalette>, 7> palettes;
    for (std::size_t shade = 0; shade < m_hud.size(); ++shade)
    {
        palettes[shade] = std::make_shared<InventoryPalette>(AssetManager::Instance().GetDefaultPalette(), m_shades[shade]);
        m_hud[shade] = animation(hudData, palettes[shade]);
        m_mirroredHud[shade] = animation(mirrored, palettes[shade]);
    }
    for (const int file : { 4, 12 })
    {
        const auto icons = CO_AWAIT read(L"ANIMS\\ANIM" + std::to_wstring(file) + L".AN2");
        if (icons.size() < 2 || icons[0] != 6 || icons[1] != 3)
        {
            throw std::runtime_error("Invalid native inventory resource bank " + std::to_string(file) + ".");
        }
        auto& variants = m_inventoryIcons[file];
        for (std::size_t shade = 0; shade < variants.size(); ++shade)
        {
            variants[shade] = animation(icons, palettes[shade]);
        }
    }
    auto fontPalette = std::make_shared<Palette>();
    fontPalette->BuildGrayscale();
    const auto menuPalette = CO_AWAIT AssetManager::Instance().LoadAsset<Palette>(L"GRAFIK\\BACKGR2.PAL");
    m_menuBackground = CO_AWAIT AssetManager::Instance().LoadAsset<Texture>(L"GRAFIK\\BACKGR2.256", menuPalette);
    m_menuCursor = CO_AWAIT AssetManager::Instance().LoadAsset<TextureAnim>(L"ANIMS\\CURSOR.AN2", fontPalette);
    if (m_menuBackground->m_width != 360 || m_menuBackground->m_height != 480 ||
        m_menuCursor->m_countX != 2 || m_menuCursor->m_countY != 5)
    {
        throw std::runtime_error("Expected the original 360x480 menu background and ten sword cursor frames.");
    }
    m_textCaret = TextMesh::CreateTexturedQuad(*AssetManager::Instance().GetPlainWhiteTexture(), 1, 10);
    for (std::size_t bank = 0; bank < FontCharacters.size(); ++bank)
    {
        const auto font = CO_AWAIT AssetManager::Instance().LoadAsset<TextureAnim>(
            L"ANIMS\\SCHRIFT" + std::to_wstring(bank) + L".AN2", fontPalette);
        if (FontCharacters[bank].size() > font->m_frames.size())
        {
            throw std::runtime_error("Incomplete native inventory font bank.");
        }
        for (std::size_t character = 0; character < FontCharacters[bank].size(); ++character)
        {
            const auto& frame = font->m_frames[character];
            m_nativeGlyphs.emplace(FontCharacters[bank][character],
                NativeGlyph{ frame.m_width + frame.m_relX + 1, frame.m_relY, nullptr });
        }
    }
    CO_RETURN;
}

namespace
{
Vector WeaponPointWorld(const Desperabis::WeaponView& view, const Vector& point,
    const Camera& camera, const D3D11_VIEWPORT& viewport)
{
    const auto& object = view.object;
    ::RawGameObject raw{};
    Desperabis::EncodeGameObject(object, raw.m_rawBytes, sizeof(raw.m_rawBytes));
    auto transform = GamePlay::PropertyAccessorBase(raw).GetTransform();
    transform.SetTranslation(Vector(object.posX * DespToFloat, -object.posY * DespToFloat, -object.posZ * DespToFloat));
    const auto tip = transform.TransformPosition(point);
    if (tip.GetZ() >= 0.f || viewport.Width <= 0.f || viewport.Height <= 0.f)
    {
        throw std::invalid_argument("Weapon tip requires a visible weapon and positive viewport.");
    }
    const float scale = (std::min)(viewport.Width / Desperabis::WeaponView::Width,
        viewport.Height / Desperabis::WeaponView::Height);
    const float px = viewport.Width * 0.5f + view.focalLength * tip.GetX() / -tip.GetZ() * scale;
    const float py = viewport.Height - Desperabis::WeaponView::Height * scale +
        (view.projectionCenterY - view.focalLength * tip.GetY() / -tip.GetZ()) * scale;
    auto ray = camera.GetProjectionMatrix(0).GetInverse().TransformPosition(
        Vector(px / viewport.Width * 2.f - 1.f, 1.f - py / viewport.Height * 2.f, 0.5f));
    ray *= -tip.GetZ() / std::fabs(ray.GetZ());
    return camera.GetCameraTransformMatrix(0).TransformPosition(ray);
}
}

Vector ApplicationUi::ElectricityTipWorld(const Desperabis::WeaponView& view,
    const Desperabis::ModelAnchorGeometry& geometry, const Camera& camera, const D3D11_VIEWPORT& viewport)
{
    // Native electricity bolts branch from vertex 3 in both firing frames.
    const auto& vertex = geometry.animations.at(view.object.modelAnim - 1).at(view.object.animFrame - 1).vertices.at(2);
    return WeaponPointWorld(view, Vector(-vertex[0] * DespVertToFloat,
        -vertex[1] * DespVertToFloat, vertex[2] * DespVertToFloat), camera, viewport);
}

Vector ApplicationUi::HeldTorchTipWorld(const Desperabis::WeaponView& view,
    const Level& level, const Camera& camera, const D3D11_VIEWPORT& viewport)
{
    const auto animation = level.GetLevelInfo().GetModelAnim(view.object.modelId - 30);
    const auto* model = animation ? animation->GetModelSafe(view.object.modelAnim - 1, view.object.animFrame - 1) : nullptr;
    if (!model) { throw std::runtime_error("Held torch requires its original model geometry."); }
    // VEKT54 4/5 vertex 24 anchors the largest of the four authored flame sprites.
    const auto& vertex = model->m_sourcePositions.at(23);
    return WeaponPointWorld(view, Vector(vertex.x, vertex.y, vertex.z), camera, viewport);
}

void ApplicationUi::DrawBossFall(const std::vector<Desperabis::RawGameObject>& objects,
    RendererDX11& renderer, const Level& level, const RenderingSettings& settings,
    ProfilingStats& stats, const D3D11_VIEWPORT& viewport, ID3D11DeviceContext3* deviceContext,
    ID3D11RenderTargetView* colorView, ID3D11DepthStencilView* depthView)
{
    const float black[]{0, 0, 0, 1};
    deviceContext->ClearRenderTargetView(colorView, black);
    deviceContext->ClearDepthStencilView(depthView, D3D11_CLEAR_DEPTH, 1.f, 0);
    for (const auto& object : objects)
    {
        const auto animation = level.GetLevelInfo().GetModelAnim(object.modelId - 30);
        const auto* model = animation ? animation->GetModelSafe(object.modelAnim - 1, object.animFrame - 1) : nullptr;
        if (!model) { throw std::runtime_error("Boss presentation references a missing model frame."); }
        if (!model->HasOwnDrawcalls() && model->m_subRenderables.empty())
        {
            if (model->m_numPositions == 0 || model->m_numUnsupportedPrimitives != 0)
            {
                throw std::runtime_error("Boss presentation references an unsupported model frame.");
            }
            // The staged root is an anchor rig; its linked world objects provide the visible body.
            continue;
        }
        Desperabis::UiModel command{};
        command.bounds = {0, 0, 320, 240};
        command.modelId = object.modelId;
        command.animation = object.modelAnim - 1;
        command.frame = object.animFrame - 1;
        command.blendFrame = object.modelFrame2;
        command.blendWeight = object.blendWeightOrSpriteOpacity;
        command.displayType = object.displayType;
        command.angles = {object.winX, object.winY, object.winZ};
        command.focalLength = 128;
        command.color = {255, 255, 255};
        command.cameraSpacePosition = std::array<std::int16_t, 3>{
            object.posX, static_cast<std::int16_t>(-object.posY), object.posZ};
        command.projectionCenterY = 90.f;
        DrawModel(command, level, renderer, settings, stats, viewport,
            deviceContext, colorView, depthView, nullptr, false, nullptr, false);
    }
}

void ApplicationUi::DrawWeapon(const Desperabis::WeaponView& view, RendererDX11& renderer, const Level& level,
    const RenderingSettings& settings, ProfilingStats& stats, const D3D11_VIEWPORT& viewport,
    ID3D11DeviceContext3* deviceContext, ID3D11RenderTargetView* colorView, ID3D11DepthStencilView* depthView,
    const VisibilityResult* world, const Scene* animationScene)
{
    using Desperabis::WeaponView;
    if (!m_weapons) { throw std::logic_error("First-person weapon assets have not been initialized."); }
    const float scale = (std::min)(viewport.Width / WeaponView::Width, viewport.Height / WeaponView::Height);
    const float bottom = viewport.TopLeftY + viewport.Height;
    if (view.kind == WeaponView::Kind::Model)
    {
        const auto& object = view.object;
        Desperabis::UiModel command{ { 0,0,WeaponView::Width,WeaponView::Height },
            object.modelId, static_cast<byte>(object.modelAnim - 1), static_cast<byte>(object.animFrame - 1),
            object.modelFrame2, object.blendWeightOrSpriteOpacity, object.displayType,
            { object.winX,object.winY,object.winZ }, object.posZ };
        command.cameraSpacePosition = { object.posX,object.posY,object.posZ };
        command.projectionCenterY = view.projectionCenterY;
        command.focalLength = view.focalLength;
        const bool faceSwordCamera = view.bindWeaponBitmap && object.modelAnim == 2 && object.animFrame == 1;
        if (faceSwordCamera) { (*command.cameraSpacePosition)[1] += 6; }
        const D3D11_VIEWPORT modelViewport{ viewport.TopLeftX + (viewport.Width - WeaponView::Width * scale) * 0.5f,
            bottom - WeaponView::Height * scale, WeaponView::Width * scale, WeaponView::Height * scale, 0,1 };
        auto* weaponTexture = view.bindWeaponBitmap ?
            &m_weapons->m_frames.at(view.bitmapFrame) : nullptr;
        Model* modelOverride = nullptr;
        if (view.modelSource == WeaponView::ModelSource::Scythe)
        {
            if (!m_scytheModel) { throw std::logic_error("Scythe presentation requires its model asset."); }
            modelOverride = m_scytheModel->GetModelSafe(object.modelAnim - 1, object.animFrame - 1);
            if (!modelOverride) { throw std::runtime_error("Scythe presentation references an unavailable frame."); }
            (*command.cameraSpacePosition)[1] += 8;
        }
        std::optional<VisibilityResult::VisibleWaterLevel> water;
        if (view.object.modelId == 33 && view.object.modelAnim == 3 && !view.bindWeaponBitmap)
        {
            (*command.cameraSpacePosition)[1] += 10;
            if (world)
            {
                water = renderer.PrepareWeaponReflection(*world,
                    level.GetHeightAtPos(world->GetMainCamera()->GetCameraPosition()), settings, stats);
            }
        }
        DrawModel(command, level, renderer, settings, stats, modelViewport, deviceContext, colorView, depthView,
            weaponTexture, faceSwordCamera, modelOverride, true, water ? &*water : nullptr, animationScene);
        return;
    }
    const Texture* texture = nullptr;
    float offsetX = view.offsetX, offsetY = view.offsetY;
    std::shared_ptr<TextMesh>* mesh = nullptr;
    if (view.kind == WeaponView::Kind::Sprite)
    {
        if (!m_weapons || view.bitmapFrame >= m_weaponMeshes.size())
        {
            throw std::runtime_error("Missing first-person weapon bitmap.");
        }
        const auto& frame = m_weapons->m_frames.at(view.bitmapFrame);
        texture = &frame;
        if (view.useBitmapOffsetX) { offsetX += frame.m_relX; }
        if (view.centerBitmapX) { offsetX -= static_cast<float>((frame.m_width - 1) / 2); }
        if (view.flipHorizontal) { offsetX = -static_cast<float>(frame.m_width - 1) - offsetX; }
        mesh = &m_weaponMeshes[view.bitmapFrame][view.flipHorizontal ? 1 : 0];
    }
    else
    {
        const auto& object = view.object;
        if (object.modelId < 30)
        {
            const auto animation = level.GetLevelInfo().GetTextureAnim(object.modelId);
            if (!animation || !animation->IsValidFrame(object.modelAnim - 1, object.animFrame - 1))
            {
                throw std::runtime_error("Held stone references an unavailable level sprite.");
            }
            const auto* frame = animation->GetFrame(object.modelAnim - 1, object.animFrame - 1);
            texture = frame;
            offsetX += frame->m_relX;
            offsetY += frame->m_relY;
        }
        else
        {
            const auto animation = level.GetLevelInfo().GetModelAnim(object.modelId - 30);
            const auto slot = static_cast<unsigned int>(object.pickupType);
            if (!animation || slot < 1 || slot > 14 || !(animation->m_textureUsedMask & (1u << slot)))
            {
                throw std::runtime_error("Held stone requires its original model bitmap slot in 1..14.");
            }
            texture = &animation->m_ownTextures[slot];
            offsetX -= static_cast<float>((texture->m_width - 1) / 2);
            offsetY -= static_cast<float>(texture->m_height - 1);
        }
        if (m_heldWeaponTexture != texture || m_heldWeaponMesh.mesh == nullptr)
        {
            m_heldWeaponTexture = texture;
            m_heldWeaponMesh.mesh = TextMesh::CreateTexturedQuad(*texture, static_cast<float>(texture->m_width),
                static_cast<float>(texture->m_height));
        }
        mesh = &m_heldWeaponMesh.mesh;
    }
    if (!*mesh)
    {
        *mesh = TextMesh::CreateTexturedQuad(*texture, static_cast<float>(texture->m_width),
            static_cast<float>(texture->m_height), 1, 1, view.flipHorizontal);
    }
    const float bitmapScale = scale * view.scale;
    float anchorY = bottom + (view.anchorY - WeaponView::Height) * scale;
    if (view.kind == WeaponView::Kind::Sprite)
    {
        anchorY = (std::max)(anchorY, bottom - (texture->m_height - 1 + offsetY) * bitmapScale);
    }
    TextMeshInstance instance(*mesh);
    instance.SetScale(bitmapScale);
    instance.SetPosition(viewport.TopLeftX + view.anchorX * viewport.Width + offsetX * bitmapScale,
        anchorY + offsetY * bitmapScale);
    VisibilityResult elements;
    RenderContextDX11 context(renderer, elements, settings, stats);
    context.RenderTextMeshes({ &instance });
}

void ApplicationUi::Deinitialize()
{
    Clear();
    m_hud = {};
    m_mirroredHud = {};
    m_inventoryIcons = {};
    m_nativeGlyphs.clear();
    m_weaponMeshes = {};
    m_heldWeaponMesh = {};
    m_heldWeaponTexture = nullptr;
    m_weapons.reset();
    m_scytheModel.reset();
    m_mapPaper.reset();
    m_menuBackground.reset();
    m_menuCursor.reset();
    m_textCaret.reset();
    m_mapPaperPixels.clear();
}

void ApplicationUi::Clear()
{
    m_meshes.clear();
    m_collisionMap.reset();
    m_mapSource = nullptr;
    m_mapMaterialViews.clear();
    m_geometryMap.reset();
    m_geometryMapSource = nullptr;
    m_geometryMapLevelNumber.reset();
}

bool ApplicationUi::UploadGeometryMap(const Desperabis::GameContext& core, Renderer& renderer)
{
    const auto* geometry = core.GetLevel();
    if (!geometry) { throw std::logic_error("Map presentation requires loaded level geometry."); }
    if (m_geometryMap && m_geometryMapSource == geometry &&
        m_geometryMapLevelNumber == core.GetLoadedLevelNumber() &&
        m_geometryMapRevision == geometry->geometryRevision) { return false; }
    if (m_mapPaperPixels.size() != 360 * 240)
    {
        throw std::logic_error("Map parchment must be initialized before generating its texture.");
    }
    if (geometry->geometryWidth == 0 || geometry->geometryWidth > Desperabis::Level::Width ||
        geometry->geometryHeight == 0 || geometry->geometryHeight > Desperabis::Level::Height)
    {
        throw std::runtime_error("Map geometry dimensions must fit 50x40.");
    }
    TextureLoadingContext pixels;
    pixels.m_rgbaData = m_mapPaperPixels;
    const auto occupied = [&](std::size_t row)
    {
        return std::any_of(std::begin(geometry->wallTextures[row]),
            std::begin(geometry->wallTextures[row]) + geometry->geometryWidth,
            [](auto value) { return value != 0; });
    };
    std::size_t first = 0;
    while (first < geometry->geometryHeight && !occupied(first)) { ++first; }
    if (first < geometry->geometryHeight)
    {
        std::size_t last = first;
        while (last + 1 < geometry->geometryHeight && occupied(last + 1)) { ++last; }
        const int scale = last > first ? 160 / static_cast<int>(last - first) : 1;
        const auto xAt = [scale](int column) { return 20 + (column + 1) * scale; };
        const auto yAt = [scale, first](int row)
        {
            return 16 + (row * 100 - static_cast<int>(first + 1)) * scale / 100;
        };
        const auto put = [&](int x, int y, Color4 color)
        {
            if (x >= 0 && x < 360 && y >= 0 && y < 240)
            {
                pixels.m_rgbaData[y * 360 + x] = color;
            }
        };
        for (std::size_t row = first; row <= last; ++row)
        {
            for (std::size_t column = 0; column < (std::min)(geometry->geometryWidth, std::size_t{49}); ++column)
            {
                if (geometry->ceilingObjects[row][column] == 0) { continue; }
                const auto surface = geometry->surfaceFlags[row][column] & 31;
                const bool water = surface >= 1 && surface <= 4;
                for (int y = (std::max)(0, yAt(static_cast<int>(row)));
                    y < (std::min)(240, yAt(static_cast<int>(row + 1))); ++y)
                {
                    for (int x = (std::max)(0, xAt(static_cast<int>(column)));
                        x < (std::min)(360, xAt(static_cast<int>(column + 1))); ++x)
                    {
                        const auto paper = m_mapPaperPixels[y * 360 + x];
                        // Approximate the native indexed paper remaps while retaining its grain.
                        const int grain = (paper.r + paper.g + paper.b) / 3;
                        const Color4 shaded = water ?
                            Color4{ static_cast<byte>(grain * 45 / 100), static_cast<byte>(grain * 70 / 100),
                                static_cast<byte>((std::min)(255, grain * 110 / 100)), 255 } :
                            Color4{ static_cast<byte>(paper.r * 60 / 100), static_cast<byte>(paper.g * 60 / 100),
                                static_cast<byte>(paper.b * 60 / 100), 255 };
                        put(x, y, shaded);
                    }
                }
            }
        }
        const Color4 ink{ 20, 17, 12, 255 };
        for (std::size_t row = first; row <= last; ++row)
        {
            for (std::size_t column = 0; column < (std::min)(geometry->geometryWidth, std::size_t{49}); ++column)
            {
                const auto wall = geometry->wallTextures[row][column];
                if ((wall & 15) == 0) { continue; }
                const int shape = wall >> 5;
                const int left = xAt(static_cast<int>(column)), right = xAt(static_cast<int>(column + 1));
                const int top = yAt(static_cast<int>(row)), bottom = yAt(static_cast<int>(row + 1));
                if (shape == 0 || shape == 5)
                {
                    for (int y = top; y <= bottom; ++y) { put(left, y, ink); }
                }
                if (shape == 2 || shape == 5)
                {
                    for (int y = top; y <= bottom; ++y) { put(right, y, ink); }
                }
                if (shape == 1 || shape == 4)
                {
                    for (int x = left; x <= right; ++x) { put(x, top, ink); }
                }
                if (shape == 3 || shape == 4)
                {
                    for (int x = left; x <= right; ++x) { put(x, bottom, ink); }
                }
            }
        }
    }
    if (!m_geometryMap)
    {
        m_geometryMap = std::make_shared<Texture>();
        AssetManager::Instance().RegisterAsset(*m_geometryMap);
    }
    // Native textured-wall UVs transpose the preview. Store once in that orientation;
    // the modal quad reverses the transpose without a second texture upload.
    auto logicalPixels = std::move(pixels.m_rgbaData);
    pixels.m_rgbaData.resize(logicalPixels.size());
    for (int y = 0; y < 240; ++y)
    {
        for (int x = 0; x < 360; ++x)
        {
            pixels.m_rgbaData[x * 240 + y] = logicalPixels[y * 360 + x];
        }
    }
    m_geometryMap->m_width = 240;
    m_geometryMap->m_height = 360;
    m_geometryMap->m_hasAlpha = false;
    m_geometryMap->SetDefaultParams();
    renderer.CreateTexture(*m_geometryMap, pixels);
    for (auto& [model, view] : m_mapMaterialViews) { renderer.CloneTextureRuntime(*view, *m_geometryMap); }
    m_geometryMapSource = geometry;
    m_geometryMapLevelNumber = core.GetLoadedLevelNumber();
    m_geometryMapRevision = geometry->geometryRevision;
    ++m_geometryMapGeneration;
    return true;
}

void ApplicationUi::ApplyMapMaterial(RenderableInstance& instance, const ModelAnim& model, Renderer& renderer)
{
    if (!m_geometryMap || !instance.GetRenderable())
    {
        throw std::logic_error("Wall-map materials require an uploaded geometry map and model.");
    }
    // VEKT14 bank 3 uses embedded slot 5 for its map panel; slot 4 is the wooden frame.
    constexpr int previewSlot = 5;
    if ((model.m_textureUsedMask & (1 << previewSlot)) == 0)
    {
        throw std::runtime_error("Wall-map model has no map-preview texture.");
    }
    const auto& calls = instance.GetRenderable()->m_drawCalls;
    const auto isPreview = [&](const DrawCall& call)
    {
        return call.m_material.m_textureSlot == previewSlot + 1 &&
            call.m_material.m_texture == &model.m_textureAtlas;
    };
    if (std::none_of(calls.begin(), calls.end(), isPreview))
    {
        throw std::runtime_error("Wall-map model has no map-preview material.");
    }
    auto& view = m_mapMaterialViews[&model];
    if (!view)
    {
        const auto atlas = model.m_ownTextures[previewSlot].m_atlasData;
        if (atlas.scaleU <= 0 || atlas.scaleV <= 0)
        {
            throw std::runtime_error("Wall-map preview has invalid atlas coordinates.");
        }
        auto texture = std::make_shared<Texture>();
        texture->m_width = m_geometryMap->m_width;
        texture->m_height = m_geometryMap->m_height;
        texture->SetDefaultParams();
        texture->m_atlasData = { 1 / atlas.scaleU, 1 / atlas.scaleV,
            -atlas.ofsU / atlas.scaleU, -atlas.ofsV / atlas.scaleV };
        renderer.CloneTextureRuntime(*texture, *m_geometryMap);
        AssetManager::Instance().RegisterAsset(*texture);
        view = std::move(texture);
    }
    const auto oldSize = instance.m_materialOverride.size();
    instance.m_materialOverride.resize(calls.size());
    for (std::size_t i = 0; i < calls.size(); ++i)
    {
        if (i >= oldSize) { instance.m_materialOverride[i] = calls[i].m_material; }
        if (!isPreview(calls[i])) { continue; }
        instance.m_materialOverride[i].m_texture = view.get();
        instance.m_materialOverride[i].ComputeHash();
    }
    instance.ComputeTransparencyMask();
}

bool ApplicationUi::UploadCollisionMap(Desperabis::GameContext& core, Renderer& renderer)
{
    const auto* map = core.collisionMap;
    const auto* geometry = core.GetLevel();
    if (!map || !geometry) { throw std::logic_error("Map presentation requires loaded collision geometry."); }
    if (m_collisionMap && m_mapSource == map && !core.collisionmapUploadDirty &&
        m_mapGeometryRevision == geometry->geometryRevision) { return false; }
    const std::size_t width = static_cast<std::size_t>(map->widthMinusOne) + 1;
    const std::size_t height = static_cast<std::size_t>(map->heightMinusOne) + 1;
    if (width > 200 || height > 200 || width * height > Desperabis::CollisionMap::VARIABLE_SIZE)
    {
        throw std::runtime_error("Collision-map dimensions exceed the supported 200x200 texture.");
    }
    // RGB approximations of map materials, not the scene's model/sprite palette.
    const std::array<Color4, 8> floors{{
        { 82, 69, 45, 220 }, { 60, 100, 160, 220 }, { 120, 88, 48, 220 }, { 55, 94, 148, 220 },
        { 72, 100, 54, 220 }, { 102, 91, 67, 220 }, { 128, 120, 101, 220 }, { 133, 111, 72, 220 }
    }};
    std::array<Color4, 256> palette;
    for (std::size_t i = 0; i < palette.size(); ++i)
    {
        palette[i] = (i & 0x38) != 0 ? Color4{ 192, 192, 184, 255 } : floors[i & 7];
    }
    TextureLoadingContext pixels;
    const auto stride = width + 2;
    pixels.m_rgbaData.resize(stride * (height + 2), Color4{ 0, 0, 0, 0 });
    for (std::size_t y = 0; y < height; ++y)
    {
        for (std::size_t x = 0; x < width; ++x)
        {
            const auto value = map->data[y * width + x];
            const auto row = y / 4, column = (x + 1) / 4;
            const bool floor = row < geometry->geometryHeight && column < geometry->geometryWidth &&
                geometry->floorObjects[row][column] != 0;
            if (value == 0 && !floor) { continue; }
            pixels.m_rgbaData[(y + 1) * stride + x + 1] = palette[value];
        }
    }
    if (!m_collisionMap)
    {
        m_collisionMap = std::make_shared<Texture>();
        AssetManager::Instance().RegisterAsset(*m_collisionMap);
    }
    m_collisionMap->m_width = static_cast<std::uint16_t>(stride);
    m_collisionMap->m_height = static_cast<std::uint16_t>(height + 2);
    m_collisionMap->m_hasAlpha = true;
    m_collisionMap->SetDefaultParams();
    renderer.CreateTexture(*m_collisionMap, pixels);
    m_mapSource = map;
    m_mapGeometryRevision = geometry->geometryRevision;
    ++m_mapRevision;
    core.collisionmapUploadDirty = false;
    return true;
}

void ApplicationUi::Draw(const Desperabis::ApplicationPresentation& presentation, RendererDX11& renderer,
    const Level& level, const Font& font, const TextureAnim& hud,
    const RenderingSettings& settings, ProfilingStats& stats, const D3D11_VIEWPORT& viewport,
    ID3D11DeviceContext3* deviceContext, ID3D11RenderTargetView* colorView, ID3D11DepthStencilView* depthView)
{
    using namespace Desperabis;
    if (!m_hud[0]) { throw std::logic_error("Application UI assets have not been initialized."); }
    if (!hud.IsValidFrame(1, 0)) { throw std::runtime_error("Application UI requires the original HUD bank."); }
    const float canvasScale = (std::min)(viewport.Width / ApplicationPresentation::Width,
        viewport.Height / ApplicationPresentation::Height);
    const float anchor = presentation.page == MenuPage::Gameplay ? 1.f : 0.5f;
    const float offsetX = viewport.TopLeftX + (viewport.Width - ApplicationPresentation::Width * canvasScale) * anchor;
    const float offsetY = viewport.TopLeftY + (viewport.Height - ApplicationPresentation::Height * canvasScale) * anchor;
    m_meshes.resize(presentation.commands.size());
    VisibilityResult elements;
    RenderContextDX11 context(renderer, elements, settings, stats);
    std::vector<TextMeshInstance> instances;
    std::size_t instanceCount = presentation.commands.size();
    for (const auto& command : presentation.commands)
    {
        if (const auto* text = std::get_if<UiText>(&command); text && text->layout == UiTextLayout::Native)
        {
            instanceCount += text->text.size();
        }
        if (const auto* text = std::get_if<UiText>(&command); text && text->caret) { ++instanceCount; }
    }
    instances.reserve(instanceCount);
    std::vector<const TextMeshInstance*> batch;
    const auto flush = [&]
    {
        context.RenderTextMeshes(batch);
        batch.clear();
    };
    for (std::size_t i = 0; i < presentation.commands.size(); ++i)
    {
        const auto& command = presentation.commands[i];
        auto& cached = m_meshes[i];
        std::string key;
        float x = 0, y = 0, scale = canvasScale;
        UiColor color;
        if (const auto* model = std::get_if<UiModel>(&command))
        {
            flush();
            const D3D11_VIEWPORT modelViewport{
                offsetX + model->bounds.x * canvasScale, offsetY + model->bounds.y * canvasScale,
                model->bounds.width * canvasScale, model->bounds.height * canvasScale, 0, 1 };
            DrawModel(*model, level, renderer, settings, stats, modelViewport, deviceContext, colorView, depthView);
            continue;
        }
        if (const auto* map = std::get_if<UiMap>(&command))
        {
            if (map->overview)
            {
                if (!m_geometryMap) { throw std::logic_error("Geometry map must be uploaded before drawing."); }
                x = map->bounds.x;
                y = map->bounds.y;
                key = "geometry-map:" + std::to_string(m_geometryMapGeneration) + ":" +
                    std::to_string(map->bounds.width) + ":" + std::to_string(map->bounds.height);
                if (key != cached.key)
                {
                    const std::array<Vector2, 4> uv{{ { 0,0 }, { 0,1 }, { 1,0 }, { 1,1 } }};
                    cached.mesh = TextMesh::CreateTexturedQuad(*m_geometryMap,
                        map->bounds.width, map->bounds.height, uv);
                }
            }
            else
            {
                if (!m_collisionMap) { throw std::logic_error("Collision map must be uploaded before drawing."); }
                x = map->bounds.x;
                y = map->bounds.y;
                const float zoom = map->pixelsPerMapPixel;
                const float centerX = map->centerColumn, centerY = map->centerRow;
                const float angle = map->heading * (6.28318530718f / 512.f);
                const float cosine = std::cos(angle), sine = std::sin(angle);
                std::array<Vector2, 4> uv;
                for (std::size_t corner = 0; corner < uv.size(); ++corner)
                {
                    const float dx = (corner % 2 == 0 ? -0.5f : 0.5f) * map->bounds.width / zoom;
                    const float dy = (corner < 2 ? -0.5f : 0.5f) * map->bounds.height / zoom;
                    uv[corner] = {
                        (1.5f + centerX + dx * cosine - dy * sine) / m_collisionMap->m_width,
                        (1.5f + centerY + dx * sine + dy * cosine) / m_collisionMap->m_height };
                }
                key = "map:" + std::to_string(m_mapRevision) + ":" + std::to_string(centerX) + ":" +
                    std::to_string(centerY) + ":" + std::to_string(angle) + ":" +
                    std::to_string(map->bounds.width) + ":" + std::to_string(map->bounds.height) + ":" +
                    std::to_string(zoom);
                if (key != cached.key)
                {
                    cached.mesh = TextMesh::CreateTexturedQuad(*m_collisionMap,
                        map->bounds.width, map->bounds.height, uv);
                }
            }
        }
        else if (const auto* fill = std::get_if<UiFill>(&command))
        {
            x = fill->bounds.x;
            y = fill->bounds.y;
            color = fill->color;
            key = "fill:" + std::to_string(fill->bounds.width) + ":" + std::to_string(fill->bounds.height);
            if (key != cached.key)
            {
                cached.mesh = TextMesh::CreateTexturedQuad(*AssetManager::Instance().GetPlainWhiteTexture(),
                    fill->bounds.width, fill->bounds.height);
            }
        }
        else if (const auto* text = std::get_if<UiText>(&command))
        {
            if (text->layout == UiTextLayout::Native)
            {
                const auto advance = [this](char character)
                {
                    if (character == ' ') { return 8; }
                    const auto found = m_nativeGlyphs.find(character);
                    if (found == m_nativeGlyphs.end())
                    {
                        throw std::runtime_error("Unavailable native inventory font character " +
                            std::to_string(static_cast<unsigned char>(character)) + ".");
                    }
                    return found->second.advance;
                };
                float penX = text->x - 5 * text->scale;
                for (const char character : text->text)
                {
                    const auto width = advance(character);
                    if (text->alignment == UiTextAlignment::Right) { penX -= width * text->scale; }
                    else if (text->alignment == UiTextAlignment::Center) { penX -= (width / 2) * text->scale; }
                }
                for (const char character : text->text)
                {
                    const auto width = advance(character);
                    if (character != ' ')
                    {
                        auto& glyph = m_nativeGlyphs.at(character);
                        if (!glyph.mesh)
                        {
                            glyph.mesh = font.CreateGlyphMesh(std::string(1, character), { 255,255,255,255 });
                            if (!glyph.mesh->IsValid()) { throw std::runtime_error("Missing native inventory glyph mesh."); }
                        }
                        auto& instance = instances.emplace_back(glyph.mesh);
                        instance.SetPosition(offsetX + penX * canvasScale,
                            offsetY + (text->y + glyph.offsetY * text->scale) * canvasScale);
                        instance.SetScale(canvasScale * text->scale);
                        instance.SetTintColor({ text->color.r,text->color.g,text->color.b,text->color.a });
                        batch.push_back(&instance);
                    }
                    penX += width * text->scale;
                }
                continue;
            }
            x = text->x;
            y = text->y;
            scale *= text->scale;
            color = text->color;
            key = "text:" + std::to_string(static_cast<int>(text->alignment)) + ":" +
                std::to_string(static_cast<int>(text->layout)) + ":" + text->text;
            if (key != cached.key)
            {
                const auto alignment = text->alignment == UiTextAlignment::Center ? Font::HorizontalAlignment::Center :
                    text->alignment == UiTextAlignment::Right ? Font::HorizontalAlignment::Right : Font::HorizontalAlignment::Left;
                cached.mesh = font.CreateGlyphMesh(text->text, { 255,255,255,255 }, alignment,
                    Font::VerticalAlignment::Top, text->layout == UiTextLayout::Menu ? 0.5f : 1.f);
            }
        }
        else if (const auto* bitmap = std::get_if<UiBitmap>(&command))
        {
            const auto& selectedHud = bitmap->flipHorizontal ? m_mirroredHud.at(bitmap->shade) : m_hud.at(bitmap->shade);
            const auto iconFile = level.GetLevelInfo().m_loadedTexAnimSlots[1];
            const auto bank = m_inventoryIcons.find(iconFile);
            if (bitmap->source == UiBitmapSource::InventoryIcons && bank == m_inventoryIcons.end())
            {
                throw std::runtime_error("Unavailable native inventory resource bank " + std::to_string(iconFile) + ".");
            }
            const auto frame = bitmap->source == UiBitmapSource::MapPaper ? BitmapFrame{ *m_mapPaper } :
                bitmap->source == UiBitmapSource::MenuBackground ? BitmapFrame{ *m_menuBackground } :
                    ResolveBitmap(*bitmap, level, *selectedHud,
                        bank == m_inventoryIcons.end() ? nullptr : bank->second.at(bitmap->shade).get(), *m_menuCursor);
            const auto& texture = frame.texture;
            if (texture.m_width == 0 || texture.m_height == 0)
            {
                throw std::runtime_error("Application UI bitmap is empty.");
            }
            const float fit = bitmap->layout == UiBitmapLayout::Fit ?
                (std::min)(bitmap->bounds.width / texture.m_width, bitmap->bounds.height / texture.m_height) : 1.f;
            // Native menus use 360x480 artwork on the same 360x240 logical canvas.
            const float verticalScale = bitmap->source == UiBitmapSource::MenuCursor ? 0.5f : 1.f;
            const float width = bitmap->layout == UiBitmapLayout::Stretch ? bitmap->bounds.width : texture.m_width * fit;
            const float height = bitmap->layout == UiBitmapLayout::Stretch ? bitmap->bounds.height :
                texture.m_height * fit * verticalScale;
            x = bitmap->bounds.x;
            y = bitmap->bounds.y;
            if (bitmap->layout == UiBitmapLayout::Fit)
            {
                x += (bitmap->bounds.width - width) * 0.5f;
                y += (bitmap->bounds.height - height) * 0.5f;
            }
            else if (bitmap->layout == UiBitmapLayout::SpriteAnchor)
            {
                x += frame.anchorX;
                y += frame.anchorY * verticalScale;
            }
            else if (bitmap->layout == UiBitmapLayout::Center)
            {
                x -= (texture.m_width - 1) / 2;
                y -= (texture.m_height - 1) / 2;
            }
            color = bitmap->color;
            key = "bitmap:" + std::to_string(static_cast<int>(bitmap->source)) + ":" +
                std::to_string(bitmap->assetSlot) + ":" + std::to_string(bitmap->animation) + ":" +
                std::to_string(bitmap->frame) + ":" + std::to_string(width) + ":" + std::to_string(height) + ":" +
                std::to_string(bitmap->flipHorizontal) + ":" + std::to_string(bitmap->shade);
            if (key != cached.key)
            {
                if (bitmap->source == UiBitmapSource::MapPaper || bitmap->source == UiBitmapSource::MenuBackground)
                {
                    const std::array<Vector2, 4> uv{{ { 0,0 }, { 1,0 }, { 0,1 }, { 1,1 } }};
                    cached.mesh = TextMesh::CreateTexturedQuad(texture, width, height, uv);
                }
                else { cached.mesh = TextMesh::CreateTexturedQuad(texture, width, height); }
            }
        }
        else if (const auto* meter = std::get_if<UiHealthMeter>(&command))
        {
            if (meter->filledColumns < 0 || meter->filledColumns > 90)
            {
                throw std::invalid_argument("Inventory health meter requires 0..90 filled columns.");
            }
            x = meter->x;
            y = meter->y;
            key = "meter:" + std::to_string(meter->filledColumns);
            if (key != cached.key)
            {
                TextureLoadingContext pixels;
                const auto& palette = AssetManager::Instance().GetDefaultPalette();
                pixels.m_rgbaData.resize(92 * 11, palette.GetEntryCustomAlpha(0, 255));
                for (int row = 1; row <= 9; ++row)
                {
                    for (int column = 0; column < 90; ++column)
                    {
                        byte index = m_healthGradient[column];
                        if (column > meter->filledColumns) { index = m_shades[6][index]; }
                        else if (column == meter->filledColumns) { index = m_shades[3][index]; }
                        index = m_shades[std::abs(row - 5)][index];
                        pixels.m_rgbaData[row * 92 + column + 1] = palette.GetEntryCustomAlpha(index, 255);
                    }
                }
                cached.texture = std::make_shared<Texture>();
                cached.texture->m_width = 92;
                cached.texture->m_height = 11;
                cached.texture->SetDefaultParams();
                AssetManager::Instance().RegisterAsset(*cached.texture);
                renderer.CreateTexture(*cached.texture, pixels);
                cached.mesh = TextMesh::CreateTexturedQuad(*cached.texture, 92, 11);
            }
        }
        cached.key = std::move(key);
        if (const auto* text = std::get_if<UiText>(&command); text && text->caret)
        {
            const float alignment = text->alignment == UiTextAlignment::Left ? 1.f :
                text->alignment == UiTextAlignment::Center ? 0.5f : 0.f;
            auto& caret = instances.emplace_back(m_textCaret);
            caret.SetPosition(offsetX + (text->x + cached.mesh->GetExtent().x * text->scale * alignment + 1) * canvasScale,
                offsetY + text->y * canvasScale);
            caret.SetScale(canvasScale * text->scale);
            caret.SetTintColor({ text->color.r,text->color.g,text->color.b,text->color.a });
            batch.push_back(&caret);
        }
        auto& instance = instances.emplace_back(cached.mesh);
        instance.SetPosition(offsetX + x * canvasScale, offsetY + y * canvasScale);
        instance.SetScale(scale);
        instance.SetTintColor({ color.r,color.g,color.b,color.a });
        batch.push_back(&instance);
    }
    flush();
    deviceContext->RSSetViewports(1, &viewport);
}
