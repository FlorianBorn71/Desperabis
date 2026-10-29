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
#include <algorithm>
#include <cmath>
#include <stdexcept>

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
        const TextureAnim& hud, const TextureAnim& icons)
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
        const TextureAnim* animation = command.source == UiBitmapSource::InventoryIcons ? &icons : &hud;
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
        ID3D11DeviceContext3* deviceContext, ID3D11RenderTargetView* colorView, ID3D11DepthStencilView* depthView)
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
        level.ApplyRenderProperties(instance, raw);
        if (!instance->IsValid() || !instance->GetVisible())
        {
            throw std::runtime_error("Application UI references an unavailable model frame.");
        }
        const auto& bounds = instance->GetRenderable()->m_localBounds;
        const auto center = bounds.GetCenter();
        Matrix4 centering;
        centering.SetTranslation(Vector(-center.GetX(), -center.GetY(), -center.GetZ()));
        auto transform = GamePlay::PropertyAccessorBase(raw).GetTransform();
        transform.SetTranslation(Vector(0, 0, -command.depth * DespToFloat));
        instance->SetTransform(centering * transform);
        instance->PrepareSubInstancesForRendering();
        const auto unlit = [&command](RenderableInstance& item)
        {
            item.m_tintColor = { command.color.r,command.color.g,command.color.b,item.m_tintColor.a };
            for (auto& material : item.GetOrCreateMaterialOverrides())
            {
                material.m_applyLighting = false;
                material.ComputeHash();
            }
        };
        unlit(*instance);
        for (auto& sub : instance->m_subInstances) { unlit(sub); }
        instance->ComputeTransparencyMask();
        Camera camera;
        Matrix4 projection;
        projection.CreateProjection(command.bounds.width / command.bounds.height,
            2.f * std::atan(command.bounds.height / (2.f * command.focalLength)), 0.001f, 100.f);
        camera.SetProjectionMatrix(projection);
        VisibilityResult elements;
        elements.m_mainCamera = &camera;
        elements.m_ambientColor = { 1,1,1 };
        elements.m_lightmap = AssetManager::Instance().GetPlainWhiteTexture();
        RenderContextDX11 context(renderer, elements, settings, stats);
        context.BeginRendering();
        context.BeginRenderTargetScope(RenderTargetPassType::MainView);
        context.TrackGlobals();
        context.SetGeometryPassType(GeometryPassType::Instances);
        // Model pictures need private depth, but must retain earlier UI color.
        deviceContext->ClearDepthStencilView(depthView, D3D11_CLEAR_DEPTH, 1.f, 0);
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

void ApplicationUi::Deinitialize()
{
    Clear();
    m_hud = {};
    m_mirroredHud = {};
    m_inventoryIcons = {};
    m_nativeGlyphs.clear();
}

void ApplicationUi::Clear()
{
    m_meshes.clear();
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
    const float offsetX = viewport.TopLeftX + (viewport.Width - ApplicationPresentation::Width * canvasScale) * 0.5f;
    const float offsetY = viewport.TopLeftY + (viewport.Height - ApplicationPresentation::Height * canvasScale) * 0.5f;
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
        if (const auto* fill = std::get_if<UiFill>(&command))
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
            key = "text:" + std::to_string(static_cast<int>(text->alignment)) + ":" + text->text;
            if (key != cached.key)
            {
                const auto alignment = text->alignment == UiTextAlignment::Center ? Font::HorizontalAlignment::Center :
                    text->alignment == UiTextAlignment::Right ? Font::HorizontalAlignment::Right : Font::HorizontalAlignment::Left;
                cached.mesh = font.CreateGlyphMesh(text->text, { 255,255,255,255 }, alignment);
            }
        }
        else if (const auto* bitmap = std::get_if<UiBitmap>(&command))
        {
            const auto& selectedHud = bitmap->flipHorizontal ? m_mirroredHud.at(bitmap->shade) : m_hud.at(bitmap->shade);
            const auto iconFile = level.GetLevelInfo().m_loadedTexAnimSlots[1];
            const auto bank = m_inventoryIcons.find(iconFile);
            if (bank == m_inventoryIcons.end())
            {
                throw std::runtime_error("Unavailable native inventory resource bank " + std::to_string(iconFile) + ".");
            }
            const auto frame = ResolveBitmap(*bitmap, level, *selectedHud, *bank->second.at(bitmap->shade));
            const auto& texture = frame.texture;
            if (texture.m_width == 0 || texture.m_height == 0)
            {
                throw std::runtime_error("Application UI bitmap is empty.");
            }
            const float fit = bitmap->layout == UiBitmapLayout::Fit ?
                (std::min)(bitmap->bounds.width / texture.m_width, bitmap->bounds.height / texture.m_height) : 1.f;
            const float width = texture.m_width * fit, height = texture.m_height * fit;
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
                y += frame.anchorY;
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
            if (key != cached.key) { cached.mesh = TextMesh::CreateTexturedQuad(texture, width, height); }
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
        auto& instance = instances.emplace_back(cached.mesh);
        instance.SetPosition(offsetX + x * canvasScale, offsetY + y * canvasScale);
        instance.SetScale(scale);
        instance.SetTintColor({ color.r,color.g,color.b,color.a });
        batch.push_back(&instance);
    }
    flush();
    deviceContext->RSSetViewports(1, &viewport);
}
