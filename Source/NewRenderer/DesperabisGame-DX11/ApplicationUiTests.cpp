#include "pch.h"
#include "ApplicationUi.h"
#include "LightningPresentation.h"

#include <GameContext.h>
#include <Asset/AssetManager.h>
#include <Asset/Font.h>
#include <Asset/Level.h>
#include <DX11/RendererDX11.h>
#include <IO/FileManagerBuiltIn.h>
#include <Physics/PhysicsInterface.h>
#include <Renderer/Camera.h>
#include <Scene/Scene.h>
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

    class FlameScene final : public Scene
    {
    public:
        FUTURE(void) CreateScene() override { CO_RETURN; }
        std::size_t BindingCount() const { return m_fireAnimationInstances.size(); }
        bool HasFlameSlot(int slot) const { return m_fireAnimations[slot].m_torchAnim != nullptr; }
    };

    void CheckLightning(RendererDX11& renderer, ID3D11Device3* device, ID3D11DeviceContext3* context)
    {
        FlameScene scene;
        Camera camera;
        Matrix4 projection;
        projection.CreateProjection(1.5f, 70.f * Math::Pi / 180.f, 0.05f, 100.f);
        camera.SetProjectionMatrix(projection);
        Desperabis::GameContext core;
        core.randomSeed = 1;
        core.GenerateLightningFlash(180);
        const auto flash = core.GetLightningFlashes().front();
        LightningPresentation presentation;
        presentation.Show(scene, flash, camera);
        Check(!scene.GetRenderableInstances().empty() && scene.GetLightSources().size() == 1,
            "Lightning show must register a dynamic model and distant light.");
        const auto light = scene.GetLightSources().front();
        Check(light->GetPosition().GetLength() > light->GetRadius() && light->GetRadius() > 0.f,
            "Lightning light must concentrate illumination in the distance, not at the player.");
        PackedLightSourceParams packed;
        Check(light->FillLightParams(packed) && packed.ColorAndPadding.x > 1.f && light->GetCoronaSize() == 0.f,
            "Lightning must use a bright, cool dynamic light without an unrelated corona.");
        Check(std::fabs(light->GetRadius() - 1700.f * DespToFloat) < 0.0001f &&
            std::fabs(packed.ColorAndPadding.x - 80.f * 210.f / 255.f) < 0.0001f,
            "Lightning must use the exaggerated 80x multiplier and increased 1700-unit radius.");
        Target target(device, 360, 240);
        renderer.ReInitialize(target.colorView.Get(), target.depthView.Get(), target.viewport);
        VisibilityResult visible;
        visible.m_mainCamera = &camera;
        visible.m_scene = &scene;
        visible.m_fogFalloff = 0.f;
        visible.m_lightmap = AssetManager::Instance().GetPlainWhiteTexture();
        visible.m_visibileLights.push_back(packed);
        for (const auto& instance : scene.GetRenderableInstances())
        {
            visible.m_visibleInstances.m_transparentInstances.push_back({instance.get(), 0});
            visible.m_visibleInstances.m_visibleTransparencyTypes |= instance->GetUsedTransparencyMask();
            Check(instance->IsValid() && instance->HasTransparentMaterials() &&
                !instance->GetRenderable()->m_drawCalls[0].m_material.m_applyLighting,
                "Lightning strips must be additive and self-lit.");
        }
        RenderingSettings settings;
        ProfilingStats stats;
        const float black[4]{0, 0, 0, 1};
        context->ClearRenderTargetView(target.colorView.Get(), black);
        context->ClearDepthStencilView(target.depthView.Get(), D3D11_CLEAR_DEPTH, 1.f, 0);
        renderer.RenderSceneElements(visible, settings, stats);
        const auto pixels = target.Pixels(device, context);
        const auto count = std::count_if(pixels.begin(), pixels.end(),
            [](std::uint32_t pixel) { return (pixel & 0xffffff) != 0; });
        Check(count >= 100, "Tapered textured lightning must render actual visible world-space pixels.");
        const auto receiverModel = GeometryUtils::CreateCameraBillboardQuadMesh(2.f, 2.f);
        auto receiver = std::make_shared<RenderableInstance>(receiverModel.get());
        auto& material = receiver->GetOrCreateMaterialOverrides().front();
        material.m_texture = AssetManager::Instance().GetPlainWhiteTexture();
        material.m_textureSlot = -1;
        material.m_applyLighting = true;
        material.ComputeHash();
        receiver->ComputeTransparencyMask();
        const auto litPixels = [&](const Vector& position, bool enabled)
        {
            Matrix4 transform;
            transform.SetTranslation(position);
            receiver->SetTransform(transform);
            VisibilityResult lighting;
            lighting.m_mainCamera = &camera;
            lighting.m_ambientColor = {0.f, 0.f, 0.f};
            lighting.m_ambientNrmlContribution = 0.f;
            lighting.m_fogFalloff = 0.f;
            lighting.m_lightmap = AssetManager::Instance().GetPlainWhiteTexture();
            lighting.m_visibleInstances.m_opaqueInstances.push_back({receiver.get(), 0});
            lighting.m_visibleInstances.m_visibleTransparencyTypes = receiver->GetUsedTransparencyMask();
            if (enabled) { lighting.m_visibileLights.push_back(packed); }
            context->ClearRenderTargetView(target.colorView.Get(), black);
            context->ClearDepthStencilView(target.depthView.Get(), D3D11_CLEAR_DEPTH, 1.f, 0);
            renderer.RenderSceneElements(lighting, settings, stats);
            const auto result = target.Pixels(device, context);
            return std::count_if(result.begin(), result.end(),
                [](std::uint32_t pixel) { return (pixel & 0xffffff) != 0; });
        };
        const auto distantPosition = light->GetPosition() + Vector(0.f, 0.f, 1.f);
        const auto unlitCount = litPixels(distantPosition, false);
        const auto distantCount = litPixels(distantPosition, true);
        const auto nearCount = litPixels(Vector(0.f, 0.f, -0.5f), true);
        std::cout << "Lightning receiver pixels: baseline " << unlitCount << ", distant "
            << distantCount << ", foreground " << nearCount << ".\n";
        Check(unlitCount == 0 && distantCount >= 100 && nearCount == 0,
            "The lightning light must illuminate distant surfaces while leaving the immediate foreground dark.");
        std::weak_ptr<RenderableInstance> instance = scene.GetRenderableInstances().front();
        presentation.Show(scene, flash, camera);
        Check(scene.GetLightSources().size() == 2, "Multiple flashes in a batch must retain their own lights.");
        presentation.Hide();
        Check(scene.GetRenderableInstances().empty() && scene.GetLightSources().empty() && instance.expired(),
            "Hide must remove every flash model and light and release its instances.");
        presentation.Hide();
        Desperabis::LightningFlash large;
        large.lines.assign(260, {170, 20, 180, 40, 4.f, 0.f});
        presentation.Show(scene, large, camera);
        int segments = 0;
        for (const auto& part : scene.GetRenderableInstances())
        {
            Check(part->GetRenderable()->m_numPositions <= 256, "Lightning chunks must fit byte-addressed second positions.");
            segments += part->GetRenderable()->m_numPolygons;
        }
        Check(scene.GetRenderableInstances().size() == 3 && segments == 260,
            "Large bolts must split into models without truncating segments.");
        presentation.Hide();
        std::cout << "Tapered world-space lightning: " << count << " visible pixels; distant light and batch cleanup passed.\n";
    }

    void CheckTextControls(const Font& font)
    {
        const Color4 base{ 204,102,0,190 };
        const auto plain = font.CreateGlyphMesh("aaa", base);
        const auto styled = font.CreateGlyphMesh("a\xfb" "a\xfa" "a", base);
        Check(styled->GetExtent().x == plain->GetExtent().x &&
            styled->GetBatches().size() == 1, "Color controls must occupy no glyph width or extra batches.");
        const auto& vertices = styled->GetBatches()[0].m_vertices;
        Check(vertices.size() == 12, "Text controls must not emit glyphs.");
        for (std::size_t i = 0; i < vertices.size(); ++i)
        {
            const auto expected = i >= 4 && i < 8 ? Color4{ 128,0,0,190 } : base;
            Check(vertices[i].m_color.r == expected.r && vertices[i].m_color.g == expected.g &&
                vertices[i].m_color.b == expected.b && vertices[i].m_color.a == expected.a &&
                vertices[i].m_position.x == plain->GetBatches()[0].m_vertices[i].m_position.x,
                "0xFB must select dark red and 0xFA restore the caller color, preserving alpha and spacing.");
        }
        const auto multiline = font.CreateGlyphMesh("\xfb" "a\\a\xfa" "a", base);
        Check(multiline->GetBatches().size() == 2 &&
            multiline->GetBatches()[1].m_vertices[0].m_color.r == 128 &&
            multiline->GetBatches()[1].m_vertices[4].m_color.g == base.g,
            "Highlight state must carry over line breaks and reset within the next line.");
        const auto next = font.CreateGlyphMesh("a", base);
        Check(next->GetBatches()[0].m_vertices[0].m_color.g == base.g,
            "Color controls must not leak into subsequent messages.");
    }

    void CheckEnemyModels()
    {
        auto* gorillaLevel = FORCE_GET(AssetManager::Instance().LoadLevel(16));
        const auto gorillaBytes = FORCE_GET(FileManager::Instance().ReadDataFuture(
            FileManager::Instance().FilenameToNative(L"LEVEL\\OBJPOS16.DAT")));
        auto gorilla = Desperabis::DecodeGameObject(gorillaBytes.data(), 54);
        Check(gorilla.behavior == Desperabis::BehaviorType::Gorilla, "Expected original gorilla fixture.");
        gorilla.health = 1000;
        gorilla.displayType = Desperabis::DisplayType::VectorNormal;
        gorilla.modelFrame2 = 16;
        gorilla.blendWeightOrSpriteOpacity = 50;
        ::RawGameObject raw{};
        Desperabis::EncodeGameObject(gorilla, raw.m_rawBytes, 54);
        raw.m_level = gorillaLevel;
        GamePlay::PropertyAccessorBase properties(raw);
        Check(properties.GetModelAnim2() == 1 && properties.GetModelFrame2() == 0,
            "Gorilla packed target 16 must decode to bank 1/frame 0, like mummy.");
        auto instance = std::make_shared<RenderableInstance>(nullptr);
        gorillaLevel->ApplyRenderProperties(instance, raw);
        auto model = gorillaLevel->GetLevelInfo().GetModelAnim(gorilla.modelId - 30);
        Check(instance->GetVisible() && instance->GetBlendTargetRenderable() == model->GetModelSafe(1, 0) &&
            instance->GetBlendTargetRenderable() != nullptr && instance->m_blendWeight == 0.5f,
            "Gorilla attack blending must resolve the real target-bank mesh.");

        auto* spiderLevel = FORCE_GET(AssetManager::Instance().LoadLevel(50));
        const auto spiderBytes = FORCE_GET(FileManager::Instance().ReadDataFuture(
            FileManager::Instance().FilenameToNative(L"LEVEL\\OBJPOS50.DAT")));
        auto slime = Desperabis::DecodeGameObject(spiderBytes.data() + (44 - 2) * 54, 54);
        Check(slime.behavior == Desperabis::BehaviorType::SpiderLarge, "Expected original white spider fixture.");
        slime.behavior = Desperabis::BehaviorType::FlyingArrow;
        slime.health = 100;
        slime.modelAnim = 2;
        slime.animFrame = 1;
        slime.modelFrame2 = 0;
        slime.blendWeightOrSpriteOpacity = 0;
        slime.displayType = Desperabis::DisplayType::VectorNormal;
        Desperabis::EncodeGameObject(slime, raw.m_rawBytes, 54);
        raw.m_level = spiderLevel;
        spiderLevel->ApplyRenderProperties(instance, raw);
        model = spiderLevel->GetLevelInfo().GetModelAnim(slime.modelId - 30);
        Check(instance->GetVisible() && instance->GetRenderable() == model->GetModelSafe(1, 0) &&
            instance->GetRenderable() != nullptr, "Airborne slime must use the spider's real second animation bank.");
        slime.animFrame = 3;
        slime.modelFrame2 = 2;
        slime.blendWeightOrSpriteOpacity = 60;
        Desperabis::EncodeGameObject(slime, raw.m_rawBytes, 54);
        spiderLevel->ApplyRenderProperties(instance, raw);
        Check(instance->GetRenderable() == model->GetModelSafe(1, 2) &&
            instance->GetBlendTargetRenderable() == model->GetModelSafe(1, 1) &&
            instance->GetRenderable() != nullptr && instance->GetBlendTargetRenderable() != nullptr,
            "Slime impact/dissolve must resolve both real puddle morph targets.");
        Check(std::fabs(instance->m_blendWeight - 0.6f) < 0.00001f,
            "Slime dissolve must forward the native blend weight.");
    }

    void CheckRainSprites(RendererDX11& renderer, ID3D11Device3* device, ID3D11DeviceContext3* context)
    {
        auto* level = FORCE_GET(AssetManager::Instance().LoadLevel(10));
        const auto animations = level->GetLevelInfo().GetTextureAnim(1);
        Check(animations != nullptr && animations->IsValidFrame(3, 0) && animations->IsValidFrame(3, 1),
            "Level 10 must supply the original rain and splash frames.");
        Desperabis::GameContext core;
        auto terrain = Desperabis::CreateDummyLevel();
        core.level = &terrain;
        core.collisionMap = terrain.collisionMap.get();
        core.worldObjects[1].posX = core.worldObjects[1].posZ = 1500;
        core.playerMapCollisionsEnabled = false;
        std::fill(std::begin(core.collisionMap->data), std::end(core.collisionMap->data), 0);
        const auto slot = core.FindFreeObjectIndex();
        core.InitializeRainDrop(slot, 1500, 200, 1500);
        const auto initial = core.worldObjects[slot];
        Check(initial.health > 0, "Rain fixture must be admitted.");
        auto instance = std::make_shared<RenderableInstance>(nullptr);
        ::RawGameObject raw{};
        raw.m_level = level;
        raw.m_objectNumber = slot;
        RenderingSettings settings;
        ProfilingStats stats;
        Target target(device, 360, 240);
        renderer.ReInitialize(target.colorView.Get(), target.depthView.Get(), target.viewport);
        std::array<int, 2> visiblePixels{};
        for (int frame = 0; frame < 2; ++frame)
        {
            auto rain = initial;
            rain.animFrame = static_cast<std::uint8_t>(frame + 1);
            rain.behavior = frame == 0 ? Desperabis::BehaviorType::RainDrop : Desperabis::BehaviorType::None;
            rain.health = frame == 0 ? 60 : 3;
            Desperabis::EncodeGameObject(rain, raw.m_rawBytes, 54);
            level->ApplyRenderProperties(instance, raw);
            Check(instance->IsValid() && instance->GetVisible() && instance->m_materialOverride.size() == 1 &&
                instance->m_materialOverride[0].m_texture == animations->GetFrame(3, frame) &&
                !instance->m_materialOverride[0].m_applyLighting &&
                std::fabs(instance->m_uniformScale - 29.f / 64.f) < 0.00001f,
                "Mode 13 must resolve the real rain/splash sprite, scale and unlit material.");
            instance->UpdateGlobalBounds();
            const auto& bounds = instance->GetGlobalBounds();
            const auto center = bounds.GetCenter();
            const float span = (std::max)(bounds.GetSphereRadius() * 3.f, 0.2f);
            Matrix4 projection, transform;
            projection.CreateOrthographic(span * 1.5f, span, 0.1f, 10.f);
            transform.SetTranslation(Vector(center.GetX(), center.GetY(), center.GetZ() + 3.f));
            Camera camera;
            camera.SetProjectionMatrix(projection);
            camera.SetCameraTransformMatrix(transform);
            VisibilityResult visible;
            visible.m_mainCamera = &camera;
            visible.m_visibleInstances.m_opaqueInstances.push_back({instance.get(), 0});
            visible.m_visibleInstances.m_visibleTransparencyTypes = instance->GetUsedTransparencyMask();
            visible.m_ambientColor = {0.f, 0.f, 0.f};
            visible.m_fogFalloff = 0.f;
            visible.m_lightmap = AssetManager::Instance().GetPlainWhiteTexture();
            const float black[4]{0, 0, 0, 1};
            context->ClearRenderTargetView(target.colorView.Get(), black);
            context->ClearDepthStencilView(target.depthView.Get(), D3D11_CLEAR_DEPTH, 1.f, 0);
            renderer.RenderSceneElements(visible, settings, stats);
            const auto pixels = target.Pixels(device, context);
            visiblePixels[frame] = static_cast<int>(std::count_if(pixels.begin(), pixels.end(),
                [](std::uint32_t pixel) { return (pixel & 0xffffff) != 0; }));
            Check(visiblePixels[frame] >= 8, "Actual rain and splash sprites must draw visible pixels, even without ambient light.");
        }
        std::cout << "Mode-13 world sprites: " << visiblePixels[0] << " rain pixels, "
            << visiblePixels[1] << " splash pixels (zero ambient light).\n";
    }

    void CheckTutorialFlames()
    {
        auto* level = FORCE_GET(AssetManager::Instance().LoadLevel(50));
        const auto bytes = FORCE_GET(FileManager::Instance().ReadDataFuture(
            FileManager::Instance().FilenameToNative(L"LEVEL\\OBJPOS50.DAT")));
        {
            Desperabis::GameContext core;
            auto web = Desperabis::DecodeGameObject(bytes.data() + 60 * 54, 54);
            ::RawGameObject object{};
            Desperabis::EncodeGameObject(web, object.m_rawBytes, 54);
            object.m_level = level;
            auto instance = std::make_shared<RenderableInstance>(nullptr);
            level->ApplyRenderProperties(instance, object);
            Check(instance->GetVisible(), "Original spider web must initially render.");
            const auto* intact = instance->GetRenderable();
            core.DamageObject(web, 0);
            Desperabis::EncodeGameObject(web, object.m_rawBytes, 54);
            level->ApplyRenderProperties(instance, object);
            const auto* destroyed = level->GetLevelInfo().GetModelAnim(web.modelId - 30)->GetModelSafe(
                web.modelAnim - 1, web.animFrame - 1);
            Check(instance->GetRenderable() == destroyed && destroyed != intact &&
                destroyed->m_localBounds.GetSphereRadius() < intact->m_localBounds.GetSphereRadius(),
                "Walked-through spider web must select the native torn-web geometry.");
        }
        FlameScene scene;
        std::array<RenderableInstancePtr, 3> flames;
        for (int index = 0; index < 3; ++index)
        {
            ::RawGameObject object{};
            const auto source = Desperabis::DecodeGameObject(bytes.data() + (23 + index) * 54, 54);
            Desperabis::EncodeGameObject(source, object.m_rawBytes, 54);
            object.m_level = level;
            flames[index] = std::make_shared<RenderableInstance>(nullptr);
            level->ApplyRenderProperties(flames[index], object);
            if (!flames[index]->GetVisible())
            {
                throw std::runtime_error("Tutorial flame fixture is invisible: display " +
                    std::to_string(static_cast<int>(source.displayType)) + ", model " +
                    std::to_string(source.modelId) + ", animation " + std::to_string(source.modelAnim));
            }
            if (flames[index]->m_materialOverride[0].m_textureSlot != 14)
            {
                throw std::runtime_error("Tutorial flame texture slot: " +
                    std::to_string(flames[index]->m_materialOverride[0].m_textureSlot) +
                    ", source animation: " + std::to_string(source.modelAnim));
            }
            scene.AddRenderableInstance(flames[index]);
        }
        Check(scene.BindingCount() == 3, "Each visible fire sprite must have an animation binding.");
        scene.HandleFireAnimations(0.1);
        Check(flames[0]->m_materialOverride[0].m_texture == nullptr,
            "Reserved texture slots are initially unbound until the invisible marker is processed.");
        for (int slot : { 24,70 })
        {
            ::RawGameObject marker{};
            Desperabis::EncodeGameObject(Desperabis::DecodeGameObject(bytes.data() + (slot - 2) * 54, 54),
                marker.m_rawBytes, 54);
            marker.m_level = level;
            Check(GamePlay::PropertyAccessorBase(marker).GetDisplayType() == GamePlay::DisplayType::Hidden,
                "Original flame-slot markers are intentionally hidden.");
            LevelGameObject object(&scene, marker);
            FORCE_WAIT(GamePlay::CreateObjectBehavior(scene, object, marker));
        }
        Check(scene.HasFlameSlot(13) && scene.HasFlameSlot(14),
            "Hidden markers must load FACKEL1/FACKEL2 into slots 13/14.");
        scene.HandleFireAnimations(0.1);
        const auto* firstFrame = flames[0]->m_materialOverride[0].m_texture;
        Check(firstFrame != nullptr && firstFrame->m_width > 0, "Tutorial flames must resolve an actual animation frame.");
        bool changed = false;
        for (int step = 0; step < 10; ++step)
        {
            scene.HandleFireAnimations(0.1);
            changed |= flames[0]->m_materialOverride[0].m_texture != firstFrame;
        }
        Check(changed, "Flame frames must advance over time.");
        for (int index = 0; index < 3; ++index)
        {
            ::RawGameObject object{};
            auto source = Desperabis::DecodeGameObject(bytes.data() + (23 + index) * 54, 54);
            source.winY += 20;
            Desperabis::EncodeGameObject(source, object.m_rawBytes, 54);
            object.m_level = level;
            level->ApplyRenderProperties(flames[index], object);
            scene.AssignFireAnimations(*flames[index]);
        }
        scene.HandleFireAnimations(0.1);
        Check(scene.BindingCount() == 3 && flames[0]->m_materialOverride[0].m_texture != nullptr,
            "Repeated core presentation updates must preserve and rebind native flame sprites.");
        std::vector<DrawCallMaterial> replacement = flames[0]->m_materialOverride;
        replacement[0].m_texture = nullptr;
        flames[0]->m_materialOverride.swap(replacement);
        scene.AssignFireAnimations(*flames[0]);
        Check(scene.BindingCount() == 3, "Refreshing a flame material must not duplicate its animation binding.");
        scene.HandleFireAnimations(0.1);
        Check(flames[0]->m_materialOverride[0].m_texture != nullptr,
            "Animation must target the current material after its vector storage is replaced.");
        scene.RemoveRenderableInstance(flames[0]);
        flames[0].reset();
        Check(scene.BindingCount() == 2, "Removing a flame must discard its material binding.");
        scene.HandleFireAnimations(0.1);
        scene.ClearScene();
        Check(scene.BindingCount() == 0 && !scene.HasFlameSlot(13) && !scene.HasFlameSlot(14),
            "Scene reset must release both flame slots and animation bindings.");
    }

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
        CheckTextControls(font);
        CheckTutorialFlames();
        CheckEnemyModels();
        CheckRainSprites(renderer, device.Get(), context.Get());
        CheckLightning(renderer, device.Get(), context.Get());
        const auto singleLine = font.CreateGlyphMesh("Nimm den Schl\x81ssel", Color4{ 204,102,0,255 },
            Font::HorizontalAlignment::Center, Font::VerticalAlignment::Center);
        const auto multiline = font.CreateGlyphMesh("Nimm den Schl\x81ssel\\Wechsle mit F2 ins \xfbInventar",
            Color4{ 204,102,0,255 }, Font::HorizontalAlignment::Center, Font::VerticalAlignment::Center);
        Check(multiline->GetExtent().y == singleLine->GetExtent().y + 20.f,
            "Tutorial backslash separators must create distinct lines with native 20-pixel spacing.");
        std::size_t batchIndex = 0;
        float lineY = 0.f;
        for (const auto text : { "Nimm den Schl\x81ssel", "Wechsle mit F2 ins \xfbInventar" })
        {
            const auto lineMesh = font.CreateGlyphMesh(text, Color4{ 204,102,0,255 },
                Font::HorizontalAlignment::Center, Font::VerticalAlignment::Top);
            for (const auto& source : lineMesh->GetBatches())
            {
                const auto& actual = multiline->GetBatches().at(batchIndex++);
                Check(actual.m_vertices.size() == source.m_vertices.size(), "Multiline prompt must preserve every glyph.");
                for (std::size_t i = 0; i < source.m_vertices.size(); ++i)
                {
                    Check(actual.m_vertices[i].m_position.x == source.m_vertices[i].m_position.x &&
                        actual.m_vertices[i].m_position.y ==
                            source.m_vertices[i].m_position.y + lineY - multiline->GetExtent().y * 0.5f,
                        "Each tutorial line must retain independent centering and native vertical spacing.");
                }
            }
            lineY += 20.f;
        }
        Check(batchIndex == multiline->GetBatches().size(), "Multiline prompts must not add separator glyphs.");
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
            {
                const float black[4]{ 0,0,0,1 };
                context->ClearRenderTargetView(target.colorView.Get(), black);
                TextMeshInstance prompt(multiline);
                prompt.SetScale(1.f);
                prompt.SetPosition(size.first * 0.5f, size.second * 0.5f);
                VisibilityResult visible;
                visible.m_mainCamera = &camera;
                visible.m_visibleTextMeshInstances.push_back(&prompt);
                renderer.RenderSceneElements(visible, settings, stats);
                const auto pixels = target.Pixels(device.Get(), context.Get());
                std::array<int, 2> linePixels{};
                int redPixels = 0;
                const float secondLineTop = size.second * 0.5f - multiline->GetExtent().y * 0.5f + 20.f;
                for (UINT y = 0; y < size.second; ++y)
                {
                    for (UINT x = 0; x < size.first; ++x)
                    {
                        if ((pixels[static_cast<std::size_t>(y) * size.first + x] & 0xffffff) != 0)
                        {
                            ++linePixels[y < secondLineTop ? 0 : 1];
                            const auto pixel = pixels[static_cast<std::size_t>(y) * size.first + x];
                            if ((pixel & 255) > 4 && (pixel & 0xffff00) == 0) { ++redPixels; }
                        }
                    }
                }
                Check(linePixels[0] > 20 && linePixels[1] > 20,
                    "Both tutorial prompt lines must render visible glyphs on wide and portrait targets.");
                Check(redPixels > 20, "Highlighted tutorial text must render actual red pixels.");
            }
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
