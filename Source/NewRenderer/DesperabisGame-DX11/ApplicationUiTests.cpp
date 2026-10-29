#include "pch.h"
#include "ApplicationUi.h"

#include <GameContext.h>
#include <Asset/AssetManager.h>
#include <Asset/Font.h>
#include <Asset/Level.h>
#include <DX11/RendererDX11.h>
#include <IO/FileManagerBuiltIn.h>
#include <Physics/PhysicsInterface.h>
#include <Renderer/Camera.h>
#include <d3d11sdklayers.h>
#include <cmath>
#include <cstring>
#include <iostream>
#include <fstream>
#include <stdexcept>

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition) { throw std::runtime_error(message); }
    }

    void CheckHr(HRESULT result)
    {
        if (FAILED(result)) { throw std::runtime_error("D3D11 test setup failed: " + std::to_string(result)); }
    }

    struct Target
    {
        UINT width, height;
        ComPtr<ID3D11Texture2D> color, depth;
        ComPtr<ID3D11RenderTargetView1> colorView;
        ComPtr<ID3D11DepthStencilView> depthView;
        D3D11_VIEWPORT viewport;

        Target(ID3D11Device3* device, UINT width, UINT height)
            : width(width), height(height), viewport{ 0,0,static_cast<float>(width),static_cast<float>(height),0,1 }
        {
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = width;
            desc.Height = height;
            desc.MipLevels = desc.ArraySize = 1;
            desc.SampleDesc.Count = 1;
            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.BindFlags = D3D11_BIND_RENDER_TARGET;
            CheckHr(device->CreateTexture2D(&desc, nullptr, &color));
            CheckHr(device->CreateRenderTargetView1(color.Get(), nullptr, &colorView));
            desc.Format = DXGI_FORMAT_R32_TYPELESS;
            desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
            CheckHr(device->CreateTexture2D(&desc, nullptr, &depth));
            D3D11_DEPTH_STENCIL_VIEW_DESC depthDesc{};
            depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
            depthDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
            CheckHr(device->CreateDepthStencilView(depth.Get(), &depthDesc, &depthView));
        }

        std::vector<std::uint32_t> Pixels(ID3D11Device3* device, ID3D11DeviceContext3* context)
        {
            D3D11_TEXTURE2D_DESC desc;
            color->GetDesc(&desc);
            desc.BindFlags = 0;
            desc.Usage = D3D11_USAGE_STAGING;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            ComPtr<ID3D11Texture2D> staging;
            CheckHr(device->CreateTexture2D(&desc, nullptr, &staging));
            context->CopyResource(staging.Get(), color.Get());
            D3D11_MAPPED_SUBRESOURCE mapped;
            CheckHr(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped));
            std::vector<std::uint32_t> result(static_cast<std::size_t>(width) * height);
            for (UINT y = 0; y < height; ++y)
            {
                std::memcpy(result.data() + static_cast<std::size_t>(y) * width,
                    static_cast<const byte*>(mapped.pData) + static_cast<std::size_t>(y) * mapped.RowPitch, width * 4);
            }
            context->Unmap(staging.Get(), 0);
            return result;
        }

        void SaveBitmap(const char* filename, std::vector<std::uint32_t> pixels) const
        {
            BITMAPFILEHEADER file{};
            BITMAPINFOHEADER image{};
            file.bfType = 0x4d42;
            file.bfOffBits = sizeof(file) + sizeof(image);
            file.bfSize = file.bfOffBits + width * height * 4;
            image.biSize = sizeof(image);
            image.biWidth = width;
            image.biHeight = -static_cast<LONG>(height);
            image.biPlanes = 1;
            image.biBitCount = 32;
            for (auto& pixel : pixels)
            {
                pixel = (pixel & 0xff00ff00) | ((pixel & 255) << 16) | ((pixel >> 16) & 255);
            }
            std::ofstream output(filename, std::ios::binary);
            output.exceptions(std::ios::badbit | std::ios::failbit);
            output.write(reinterpret_cast<const char*>(&file), sizeof(file));
            output.write(reinterpret_cast<const char*>(&image), sizeof(image));
            output.write(reinterpret_cast<const char*>(pixels.data()), pixels.size() * sizeof(pixels[0]));
        }
    };

    std::array<Desperabis::RawGameObject, 8> OriginalItems()
    {
        std::array<Desperabis::RawGameObject, 8> items{};
        for (const auto* file : { L"LEVEL\\OBJPOS0.DAT", L"LEVEL\\OBJPOS11.DAT", L"LEVEL\\OBJPOS10.DAT", L"LEVEL\\OBJPOS50.DAT" })
        {
            const auto bytes = FORCE_GET(FileManager::Instance().ReadDataFuture(FileManager::Instance().FilenameToNative(file)));
            Check(!bytes.empty() && bytes.size() % 54 == 0, "Original item fixtures require complete OBJPOS records.");
            for (std::size_t i = 0; i + 54 <= bytes.size(); i += 54)
            {
                const auto item = Desperabis::DecodeGameObject(bytes.data() + i, 54);
                if (item.health <= 0 || item.pickupType == Desperabis::PickupType::None) { continue; }
                if (item.collisionType == Desperabis::CollisionType::Key && item.modelId == 39) { items[0] = item; }
                if (item.collisionType == Desperabis::CollisionType::Medicine && item.modelId == 1) { items[1] = item; }
                if (item.collisionType == Desperabis::CollisionType::Weapon && item.itemValue == 1) { items[2] = item; }
                if (item.collisionType == Desperabis::CollisionType::Weapon && item.itemValue == 2) { items[3] = item; }
                if (item.collisionType == Desperabis::CollisionType::Weapon && item.itemValue == 3) { items[4] = item; }
                if (item.collisionType == Desperabis::CollisionType::Weapon && item.itemValue == 4) { items[5] = item; }
                if (item.collisionType == Desperabis::CollisionType::Weapon && item.itemValue == 5) { items[6] = item; }
                if (item.collisionType == Desperabis::CollisionType::Weapon && item.itemValue == 7) { items[7] = item; }
            }
        }
        for (const auto& item : items) { Check(item.health > 0, "Missing original UI item fixture."); }
        return items;
    }
}

int main(int argc, char* argv[])
{
    try
    {
        ComPtr<ID3D11Device> baseDevice;
        ComPtr<ID3D11DeviceContext> baseContext;
        CheckHr(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_DEBUG,
            nullptr, 0, D3D11_SDK_VERSION, &baseDevice, nullptr, &baseContext));
        ComPtr<ID3D11Device3> device;
        ComPtr<ID3D11DeviceContext3> context;
        ComPtr<ID3D11InfoQueue> info;
        CheckHr(baseDevice.As(&device));
        CheckHr(baseContext.As(&context));
        CheckHr(baseDevice.As(&info));
        Target initial(device.Get(), 360, 240);
        FileManagerBuiltIn files;
        NullPhysicsModule physics;
        files.Initialize();
        RendererDX11 renderer;
        FORCE_WAIT(renderer.Initialize(device.Get(), context.Get(), initial.colorView.Get(), initial.depthView.Get(), initial.viewport));
        FORCE_WAIT(AssetManager::Instance().Initialize());
        auto* level = FORCE_GET(AssetManager::Instance().LoadLevel(11));
        const auto hud = FORCE_GET(AssetManager::Instance().LoadAsset<TextureAnim>(L"ANIMS\\AMA_ANI.AN2"));
        Font font;
        auto palette = std::make_shared<Palette>();
        palette->BuildGrayscale();
        const std::wstring_view fontFiles[] = { L"ANIMS\\SCHRIFT0.AN2", L"ANIMS\\SCHRIFT1.AN2", L"ANIMS\\SCHRIFT2.AN2" };
        std::array<std::shared_ptr<TextureAnim>, 3> fonts;
        for (std::size_t i = 0; i < fonts.size(); ++i)
        {
            fonts[i] = FORCE_GET(AssetManager::Instance().LoadAsset<TextureAnim>(fontFiles[i], palette));
            font.AddGlyphs(fonts[i].get(), ApplicationUi::FontCharacters[i]);
        }
        const auto items = OriginalItems();
        const auto dagger = level->GetLevelInfo().GetModelAnim(9)->GetModel(1, 0);
        const auto materialBefore = dagger->m_drawCalls.at(0).m_material;
        RenderingSettings settings;
        ProfilingStats stats;
        ApplicationUi ui;
        FORCE_WAIT(ui.Initialize());
        Desperabis::GameContext core;
        core.SetMenuPage(Desperabis::MenuPage::Inventory);
        Camera camera;
        for (const auto size : { std::pair<UINT, UINT>{ 360,240 }, { 1280,720 }, { 480,800 } })
        {
            Target target(device.Get(), size.first, size.second);
            renderer.ReInitialize(target.colorView.Get(), target.depthView.Get(), target.viewport);
            const auto drawRecipe = [&](const Desperabis::ApplicationPresentation& recipe)
            {
                const float black[4]{ 0,0,0,1 };
                context->ClearRenderTargetView(target.colorView.Get(), black);
                VisibilityResult empty;
                empty.m_mainCamera = &camera;
                renderer.RenderSceneElements(empty, settings, stats);
                ui.Draw(recipe, renderer, *level, font, *hud, settings, stats,
                    target.viewport, context.Get(), target.colorView.Get(), target.depthView.Get());
                return target.Pixels(device.Get(), context.Get());
            };
            const auto render = [&] { return drawRecipe(core.GetApplicationPresentation()); };
            core.inventory = {};
            const auto baseline = render();
            const float scale = (std::min)(size.first / 360.f, size.second / 240.f);
            const float offsetX = (size.first - 360 * scale) * 0.5f;
            const float offsetY = (size.second - 240 * scale) * 0.5f;
            Desperabis::ApplicationPresentation healthLabel{ Desperabis::MenuPage::Inventory, {
                Desperabis::UiFill{ { 0,0,360,240 }, { 0,0,0 } } } };
            auto meterOnly = healthLabel;
            for (const auto& command : core.GetApplicationPresentation().commands)
            {
                if (const auto* text = std::get_if<Desperabis::UiText>(&command); text && text->text == "Gesundheit:")
                {
                    healthLabel.commands.push_back(*text);
                }
                if (const auto* meter = std::get_if<Desperabis::UiHealthMeter>(&command))
                {
                    meterOnly.commands.push_back(*meter);
                }
            }
            Check(healthLabel.commands.size() == 2 && meterOnly.commands.size() == 2,
                "The health label and meter must be present in the core recipe.");
            const auto labelPixels = drawRecipe(healthLabel);
            int visibleLabelPixels = 0;
            for (UINT y = 0; y < size.second; ++y)
            {
                for (UINT x = 0; x < size.first; ++x)
                {
                    if ((labelPixels[static_cast<std::size_t>(y) * size.first + x] & 0xffffff) == 0) { continue; }
                    Check(y < offsetY + 28 * scale,
                        "Health text must not reach the meter interior, including wide and portrait scaling.");
                    ++visibleLabelPixels;
                }
            }
            Check(visibleLabelPixels > 20 * scale * scale, "The nonoverlapping health label must remain visible.");
            const auto meterPixels = drawRecipe(meterOnly);
            // The native 'h' touches the top border; the opaque meter drawn afterward clears that final row.
            healthLabel.commands.push_back(meterOnly.commands.back());
            const auto composedPixels = drawRecipe(healthLabel);
            for (int y = static_cast<int>(std::ceil(offsetY + 27 * scale)); y < offsetY + 38 * scale; ++y)
            {
                for (int x = static_cast<int>(std::ceil(offsetX + 225 * scale)); x < offsetX + 317 * scale; ++x)
                {
                    const auto i = static_cast<std::size_t>(y) * size.first + x;
                    Check(composedPixels[i] == meterPixels[i], "The complete health meter must remain unobscured by its caption.");
                }
            }
            for (const auto& item : items)
            {
                core.inventory[13] = item;
                const auto before = core.inventory;
                const auto pixels = render();
                Check(pixels == render(), "Cached UI rendering must be stable across identical frames.");
                int changed = 0;
                for (int y = static_cast<int>(offsetY + 13 * scale); y < offsetY + 47 * scale; ++y)
                {
                    for (int x = static_cast<int>(offsetX + 13 * scale); x < offsetX + 52 * scale; ++x)
                    {
                        const auto i = static_cast<std::size_t>(y) * size.first + x;
                        if (pixels[i] != baseline[i]) { ++changed; }
                    }
                }
                std::cout << size.first << 'x' << size.second << " collision " <<
                    static_cast<int>(item.collisionType) << " value " << item.itemValue << ": " << changed << " item pixels\n";
                Check(changed > 20 * scale * scale, "Original inventory bitmap/model must be visibly rendered in its core-authored cell.");
                Check(std::memcmp(before.data(), core.inventory.data(), sizeof(before)) == 0,
                    "Drawing UI must not mutate inventory records.");
            }
            for (std::size_t i = 0; i < items.size(); ++i) { core.inventory[13 - i] = items[i]; }
            Check(render() == render(), "Mixed bitmap/model pages must preserve draw ordering and cache stability.");
            if (size.first == 360)
            {
                using namespace Desperabis;
                const auto* digit = fonts[2]->GetFrame(0, 1);
                const int advance = digit->m_width + digit->m_relX + 1;
                for (const auto alignment : { UiTextAlignment::Left, UiTextAlignment::Center, UiTextAlignment::Right })
                {
                    ApplicationPresentation native{ MenuPage::Inventory, {
                        UiFill{ { 0,0,360,240 }, { 0,0,0 } },
                        UiText{ 100,100,"11",1,{},alignment,UiTextLayout::Native } } };
                    const float x = 95.f - (alignment == UiTextAlignment::Center ? 2 * (advance / 2) :
                        alignment == UiTextAlignment::Right ? 2 * advance : 0);
                    ApplicationPresentation expected{ MenuPage::Inventory, {
                        UiFill{ { 0,0,360,240 }, { 0,0,0 } },
                        UiText{ x,100.f + digit->m_relY,"1" },
                        UiText{ x + advance,100.f + digit->m_relY,"1" } } };
                    const auto actual = drawRecipe(native);
                    Check(actual == drawRecipe(expected),
                        "Native text must use pen X minus five, original glyph Y, native advances and per-glyph centering.");
                }
                ApplicationPresentation hands{ MenuPage::Inventory, {
                    UiFill{ { 0,0,360,240 }, { 0,0,0 } },
                    UiBitmap{ { 10,10,0,0 }, UiBitmapSource::Hud, 0,0,1, {}, UiBitmapLayout::TopLeft },
                    UiBitmap{ { 60,10,0,0 }, UiBitmapSource::Hud, 0,0,1, {}, UiBitmapLayout::TopLeft, true } } };
                const auto mirrored = drawRecipe(hands);
                int handPixels = 0;
                for (int y = 10; y < 50; ++y)
                {
                    for (int x = 0; x < 45; ++x)
                    {
                        const auto left = mirrored[y * 360 + 10 + x];
                        const auto right = mirrored[y * 360 + 60 + 44 - x];
                        for (int channel = 0; channel < 3; ++channel)
                        {
                            Check(std::abs(static_cast<int>((left >> (channel * 8)) & 255) -
                                static_cast<int>((right >> (channel * 8)) & 255)) <= 1,
                                "The native right-hand selector must mirror the left-hand image exactly.");
                        }
                        if ((left & 0xffffff) != 0) { ++handPixels; }
                    }
                }
                Check(handPixels > 1200, "Original hand artwork must retain its natural 45x40 pixel footprint.");
                hands.commands.pop_back();
                const auto bright = drawRecipe(hands);
                std::get<UiBitmap>(hands.commands.back()).shade = 2;
                const auto dim = drawRecipe(hands);
                std::get<UiBitmap>(hands.commands.back()).shade = 6;
                Check(bright != dim && dim != drawRecipe(hands),
                    "Native focus and availability lookup tables must change real rendered artwork.");
                ApplicationPresentation meter{ MenuPage::Inventory, {
                    UiFill{ { 0,0,360,240 }, { 0,0,0 } }, UiHealthMeter{ 225,27,90 } } };
                const auto full = drawRecipe(meter);
                std::get<UiHealthMeter>(meter.commands.back()).filledColumns = 45;
                const auto half = drawRecipe(meter);
                std::get<UiHealthMeter>(meter.commands.back()).filledColumns = 0;
                const auto depleted = drawRecipe(meter);
                int changed = 0;
                for (std::size_t i = 0; i < full.size(); ++i)
                {
                    if (full[i] == half[i] && half[i] == depleted[i]) { continue; }
                    Check(i % 360 >= 225 && i % 360 < 317 && i / 360 >= 27 && i / 360 < 38,
                        "Health fill changes must remain inside the original 92x11 meter.");
                    ++changed;
                }
                Check(changed > 500, "Native health fill and dimming must visibly cover the original gradient.");
                if (argc == 2)
                {
                    core.gold = 42;
                    core.weaponAmounts[3] = 12;
                    core.weaponAmounts[7] = 180;
                    core.hasMap = true;
                    core.playerLife = 230 * 65536;
                    target.SaveBitmap(argv[1], render());
                }
            }
            Check(dagger->m_drawCalls.at(0).m_material.m_materialHash == materialBefore.m_materialHash &&
                dagger->m_drawCalls.at(0).m_material.m_applyLighting == materialBefore.m_applyLighting,
                "Unlit UI model pictures must not modify shared world materials.");
            core.SetMenuPage(Desperabis::MenuPage::MainMenu);
            Check(render() != baseline, "The main menu must have a distinct presentation.");
            core.SetMenuPage(Desperabis::MenuPage::Inventory);
        }
        std::vector<std::uint32_t> previousMap;
        for (const int levelNumber : { 30, 50 })
        {
            const auto* chapterLevel = FORCE_GET(AssetManager::Instance().LoadLevel(levelNumber));
            Check(chapterLevel->GetLevelInfo().GetTextureAnim(1)->IsValidFrame(5, 1),
                "Both chapter resource banks require the native map frame.");
            core.inventory = {};
            core.SetMenuPage(Desperabis::MenuPage::Inventory);
            core.hasMap = true;
            renderer.ReInitialize(initial.colorView.Get(), initial.depthView.Get(), initial.viewport);
            VisibilityResult empty;
            empty.m_mainCamera = &camera;
            renderer.RenderSceneElements(empty, settings, stats);
            ui.Clear();
            ui.Draw(core.GetApplicationPresentation(), renderer, *chapterLevel, font, *hud, settings, stats,
                initial.viewport, context.Get(), initial.colorView.Get(), initial.depthView.Get());
            const auto pixels = initial.Pixels(device.Get(), context.Get());
            std::vector<std::uint32_t> map;
            for (int y = 204; y < 231; ++y)
            {
                for (int x = 240; x < 285; ++x) { map.push_back(pixels[y * 360 + x]); }
            }
            if (!previousMap.empty())
            {
                int changed = 0;
                for (std::size_t i = 0; i < map.size(); ++i)
                {
                    if (map[i] != previousMap[i]) { ++changed; }
                }
                Check(changed > 200, "Resource art must follow the chapter's sprite bank, not a hardcoded ANIM4.");
            }
            previousMap = std::move(map);
        }
        const auto messageCount = info->GetNumStoredMessages();
        for (UINT64 i = 0; i < messageCount; ++i)
        {
            SIZE_T size = 0;
            info->GetMessage(i, nullptr, &size);
            std::vector<byte> storage(size);
            auto* message = reinterpret_cast<D3D11_MESSAGE*>(storage.data());
            CheckHr(info->GetMessage(i, message, &size));
            if (message->Severity == D3D11_MESSAGE_SEVERITY_ERROR || message->Severity == D3D11_MESSAGE_SEVERITY_CORRUPTION)
            {
                throw std::runtime_error(message->pDescription);
            }
        }
        ui.Deinitialize();
        fonts = {};
        AssetManager::Instance().Deinitialize();
        renderer.Deinitialize();
        files.Deinitialize();
        std::cout << "PASS: WARP inventory/main-menu presentation, original key/medicine and six cheat weapons, "
            "both chapter resource/model banks, native text metrics and nonoverlapping health label, "
            "native-size artwork, hand mirroring, palette shading, health gradient, "
            "wide/portrait scaling, cache stability and no D3D11 errors.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
