#include "pch.h"
#include "ApplicationUi.h"
#include "LightningPresentation.h"

#include <GameContext.h>
#include <BehaviorDispatch.h>
#include <Asset/AssetManager.h>
#include <Asset/Font.h>
#include <Asset/Level.h>
#include <DX11/RendererDX11.h>
#include <DX11/RenderContextDX11.h>
#include <DX11/NativeTextureDX11.h>
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

    template<typename T>
    std::vector<T> ReadBuffer(ID3D11Device3* device, ID3D11DeviceContext3* context, ID3D11Buffer* buffer)
    {
        Check(buffer != nullptr, "Sword geometry must bind an actual GPU buffer.");
        D3D11_BUFFER_DESC desc{};
        buffer->GetDesc(&desc);
        Check(desc.ByteWidth % sizeof(T) == 0, "Geometry buffer has an unexpected element stride.");
        desc.BindFlags = desc.MiscFlags = desc.StructureByteStride = 0;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        ComPtr<ID3D11Buffer> copy;
        CheckHr(device->CreateBuffer(&desc, nullptr, &copy));
        context->CopyResource(copy.Get(), buffer);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        CheckHr(context->Map(copy.Get(), 0, D3D11_MAP_READ, 0, &mapped));
        std::vector<T> result(desc.ByteWidth / sizeof(T));
        std::memcpy(result.data(), mapped.pData, desc.ByteWidth);
        context->Unmap(copy.Get(), 0);
        return result;
    }

    void CheckSwordFacing(ID3D11Device3* device, ID3D11DeviceContext3* context,
        const Desperabis::WeaponView& view, const Model& model, bool checkGripClipping = false)
    {
        ComPtr<ID3D11Buffer> buffer;
        context->VSGetConstantBuffers(2, 1, &buffer);
        Check(buffer.Get() == model.GetNativeRuntimeObj<NativeModelMeshObjectDX11>()->m_positionBuffer.Get(),
            "The sword must use its original shared geometry, not a generated replacement.");
        const auto positions = ReadBuffer<VertexPosition>(device, context, buffer.Get());
        Check(positions.size() >= static_cast<std::size_t>(model.m_vertexOfs) + model.m_numPositions,
            "Native sword positions must be available.");
        for (int i = 0; i < model.m_numPositions; ++i)
        {
            Check(positions[model.m_vertexOfs + i].y == 0, "The original sword must remain a flat polygon.");
        }
        buffer.Reset();
        context->VSGetConstantBuffers(1, 1, &buffer);
        const auto instance = ReadBuffer<float>(device, context, buffer.Get());
        Check(instance.size() >= 16, "Sword instance buffer must contain its transform.");
        Matrix4 rendered;
        rendered.Load(instance.data());
        rendered = rendered.GetTransposed();
        RawGameObject raw;
        Desperabis::EncodeGameObject(view.object, raw.m_rawBytes, sizeof(raw.m_rawBytes));
        auto native = GamePlay::PropertyAccessorBase(raw).GetTransform();
        native.SetTranslation(Vector(view.object.posX * DespToFloat, -(view.object.posY + 6) * DespToFloat,
            -view.object.posZ * DespToFloat));
        Check((rendered.GetTranslation() - native.GetTranslation()).GetLength() < 0.000001f &&
            (rendered.GetDirection() - native.GetDirection()).GetLength() < 0.00001f,
            "The sword must retain its six-unit downward offset and native long-axis direction.");
        auto ray = Vector(0,0,0) - rendered.TransformPosition(model.m_localBounds.GetCenter());
        ray.Normalize();
        const auto axis = rendered.GetDirection();
        const auto normal = rendered.GetUpDir();
        const auto dot = [](const Vector& a, const Vector& b)
        {
            return a.GetX()*b.GetX() + a.GetY()*b.GetY() + a.GetZ()*b.GetZ();
        };
        const float along = dot(axis, ray);
        const float maximumFacing = std::sqrt((std::max)(0.f, 1.f - along*along));
        Check(std::fabs(dot(normal, ray) - maximumFacing) < 0.0001f,
            "The flat sword must attain the maximum camera-facing angle allowed by its long axis.");
        if (checkGripClipping)
        {
            buffer.Reset();
            context->VSGetConstantBuffers(0, 1, &buffer);
            const auto globals = ReadBuffer<float>(device, context, buffer.Get());
            Check(globals.size() >= 80, "Weapon globals must contain the projection matrix.");
            Matrix4 projection;
            // Two view matrices and two inverse-view matrices precede the projection.
            projection.Load(globals.data() + 64);
            projection = projection.GetTransposed();
            const auto& box = model.m_localBounds.m_box;
            const auto pommel = (rendered * projection).TransformPosition(
                Vector(model.m_localBounds.GetCenter().GetX(), 0, box.m_min.GetZ()));
            Check(pommel.GetY() < -1.05f, "The idle sword's pommel must lie below the screen with clipping margin.");
        }
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

    void CheckUnderwater(RendererDX11& renderer, ID3D11Device3* device, ID3D11DeviceContext3* context)
    {
        const auto& white = *AssetManager::Instance().GetPlainWhiteTexture();
        RenderingSettings settings;
        ProfilingStats stats;
        Camera camera;
        for (const auto size : {std::pair<UINT, UINT>{360,240}, {240,360}, {720,480}})
        {
            Target target(device, size.first, size.second);
            renderer.ReInitialize(target.colorView.Get(), target.depthView.Get(), target.viewport);
            TextMeshInstance view(TextMesh::CreateTexturedQuad(white, static_cast<float>(size.first),
                static_cast<float>(size.second)));
            view.SetScale(1.f);
            view.SetTintColor(Color4{200,100,50,255});
            VisibilityResult visible;
            visible.m_mainCamera = &camera;
            const auto render = [&]
            {
                renderer.SetWeaponDraw([&](const RenderingSettings& drawSettings, ProfilingStats& drawStats)
                {
                    VisibilityResult empty;
                    RenderContextDX11 draw(renderer, empty, drawSettings, drawStats);
                    draw.RenderTextMeshes({&view});
                });
                renderer.RenderSceneElements(visible, settings, stats);
                return target.Pixels(device, context);
            };
            const auto pixelAt = [&](const auto& pixels, UINT x, UINT y)
                { return pixels[static_cast<std::size_t>(y) * size.first + x] & 0xffffff; };
            renderer.SetUnderwaterTint(false);
            const auto normal = render();
            Check(pixelAt(normal, 10, 10) == 0x3264c8, "Untinted view must preserve its original RGB.");
            renderer.SetUnderwaterTint(true);
            const auto tinted = render();
            const auto rgb = pixelAt(tinted, 10, 10);
            Check(std::abs(static_cast<int>(rgb & 255) - 75) <= 1 &&
                std::abs(static_cast<int>((rgb >> 8) & 255) - 63) <= 1 && (rgb >> 16) == 50,
                "Underwater must multiply the world/weapon RGB, not cover it with an alpha-blended blue sheet.");
            Check(render() == tinted, "Underwater tint must remain active until the exit callback.");
            const float height = 20.f;
            auto meters = ApplicationUi::CreateVitalMeters(white,
                {0.f, static_cast<float>(size.second) - height, static_cast<float>(size.first), height}, 0.5f, 0.25f);
            Check(meters.size() == 4, "Submerged vitals must contain separate half-height health/oxygen strips.");
            for (const auto& meter : meters)
            {
                Check(meter->GetMesh()->GetExtent().y == height * 0.5f,
                    "Both submerged meters must use exactly half the normal health height.");
                visible.m_visibleTextMeshInstances.push_back(meter.get());
            }
            const auto hud = render();
            Check(pixelAt(hud, size.first / 8, size.second - 15) == 0xff8030 &&
                pixelAt(hud, size.first / 2, size.second - 15) == 0xffffff &&
                pixelAt(hud, size.first / 8, size.second - 5) == 0xffffff &&
                pixelAt(hud, 3 * size.first / 4, size.second - 5) == 0x181818 &&
                pixelAt(hud, size.first / 2, size.second - 21) == rgb,
                "Blue/white oxygen must sit above health within its old bounds, with HUD colors unaffected by tint.");
            visible.m_visibleTextMeshInstances.clear();
            auto dry = ApplicationUi::CreateVitalMeters(white,
                {0.f, static_cast<float>(size.second) - height, static_cast<float>(size.first), height}, 0.5f, std::nullopt);
            Check(dry.size() == 2 && dry[0]->GetMesh()->GetExtent().y == height,
                "Outside water the existing health meter must retain its full height.");
            auto empty = ApplicationUi::CreateVitalMeters(white, {0,0,100,20}, 0.f, 0.f);
            Check(empty.size() == 2, "Depleted meters must show backgrounds without zero-width geometry.");
            renderer.SetUnderwaterTint(false);
            Check(render() == normal, "Exit must remove the tint completely.");
        }
    }

    void CheckUnderwaterSurface(RendererDX11& renderer, ID3D11Device3* device, ID3D11DeviceContext3* context)
    {
        ModelLoadingContext positions;
        positions.m_vertexPos = {{-2,0,-2,1}, {2,0,-2,1}, {2,0,2,1}, {-2,0,2,1}};
        ModelConversionContext mesh(positions);
        mesh.m_numPositions = 4;
        for (uint16_t i = 0; i < 4; ++i)
        {
            mesh.AddToBounds(positions.m_vertexPos[i], 0.f);
            mesh.m_vertices.emplace_back().SetTextured(i, Color4{255,255,255,255}, {0,0});
        }
        mesh.m_sortedIndices = {0,2,1,0,3,2};
        mesh.GeneratePerTriangleNormals(mesh.m_sortedIndices.data(), 6, 0);
        Renderable model;
        model.m_runtime = renderer.CreateModelBuffers(mesh);
        model.m_numPositions = 4;
        model.m_numPolygons = 1;
        mesh.GenerateBoundingVolume(model.m_localBounds);
        auto& draw = model.m_drawCalls.emplace_back();
        draw.m_primitiveCount = 2;
        draw.m_material.m_transparencyType = TransparencyMode::Water;
        draw.m_material.m_reflectionType = WaterReflectionType::ReflectionBit | WaterReflectionType::Water;
        draw.m_material.m_texture = AssetManager::Instance().GetPlainWhiteTexture();
        draw.m_material.m_applyLighting = false;
        draw.m_material.m_useAlphaTest = false;
        draw.m_material.ComputeHash();
        auto instance = std::make_shared<RenderableInstance>(&model, 1);
        instance->SetTransform(Vector(0,0,0), Vector(0,0,0));
        instance->UpdateGlobalBounds();
        FlameScene scene;
        scene.AddRenderableInstance(instance);
        class Visibility final : public DefaultVisibilityContext
        {
        public:
            void Collect(VisibilityResult& result, const Scene& scene, float cameraY)
            {
                m_performFrustumClipping = false;
                VisibilitySettings settings;
                settings.filterMask = 1;
                Frustum frustum;
                DetermineVisibleInstances(result, result.m_visibleInstances, &scene,
                    Vector(0,cameraY,0), frustum, nullptr, settings, true);
            }
        } visibility;
        for (const float y : {1.f, 0.f, -1.f})
        {
            VisibilityResult result;
            visibility.Collect(result, scene, y);
            Check(result.m_visibleWater.size() == 1 && result.m_visibleWater[0].m_useReflection == (y >= 0.f),
                "Reflecting water must use refraction only when the camera is below its surface.");
        }
        auto& override = instance->GetOrCreateMaterialOverrides().at(0);
        override.m_reflectionType = WaterReflectionType::RefractionBit | WaterReflectionType::Water;
        override.ComputeHash();
        for (const float y : {-1.f, 1.f})
        {
            VisibilityResult result;
            visibility.Collect(result, scene, y);
            Check(!result.m_visibleWater.at(0).m_useReflection, "Explicit refraction must remain refraction on both sides.");
        }
        override = draw.m_material;
        Target target(device, 360, 240);
        renderer.ReInitialize(target.colorView.Get(), target.depthView.Get(), target.viewport);
        Camera camera;
        Matrix4 projection;
        projection.CreateProjection(1.5f, Math::Pi / 2.f, 0.05f, 10.f);
        camera.SetProjectionMatrix(projection);
        Matrix4 cameraTransform;
        cameraTransform.Create(Vector(0,-1,0,1), Vector(1,0,0,0), Vector(0,0,1,0), Vector(0,-1,0,0));
        camera.SetCameraTransformMatrix(cameraTransform);
        VisibilityResult result;
        visibility.Collect(result, scene, -1.f);
        result.m_mainCamera = &camera;
        result.m_scene = &scene;
        result.m_fogColor = {0,0,0};
        result.m_ambientColor = {1,1,1};
        result.m_fogFalloff = 0;
        result.m_lightmap = AssetManager::Instance().GetPlainWhiteTexture();
        auto& water = result.m_visibleWater.at(0);
        water.m_scrollParams1 = water.m_scrollParams2 = {0,0,0,0};
        RenderingSettings settings;
        ProfilingStats stats;
        stats.ResetRenderStats();
        RenderContextDX11 render(renderer, result, settings, stats);
        render.BeginRendering();
        render.BeginRenderTargetScope(RenderTargetPassType::MainView);
        const float gray[]{0.8f,0.8f,0.8f,1.f};
        context->ClearRenderTargetView(target.colorView.Get(), gray);
        context->ClearDepthStencilView(target.depthView.Get(), D3D11_CLEAR_DEPTH, 1.f, 0);
        render.TrackGlobals();
        render.BeginWaterRendering();
        render.PrepareRefraction(true);
        render.SetGeometryPassType(GeometryPassType::MirrorReceiver);
        render.SetBlendState(TransparencyMode::Water);
        ComPtr<ID3D11RasterizerState> before;
        context->RSGetState(&before);
        render.PrepareWaterRendering(water);
        ComPtr<ID3D11RasterizerState> during;
        context->RSGetState(&during);
        D3D11_RASTERIZER_DESC state{};
        during->GetDesc(&state);
        Check(state.CullMode == D3D11_CULL_NONE, "Water receivers must actually draw their underwater backfaces.");
        render.RenderInstance<GeometryPassType::MirrorReceiver>(instance.get(), TransparencyMode::Water);
        render.EndWaterRendering();
        ComPtr<ID3D11RasterizerState> after;
        context->RSGetState(&after);
        Check(after.Get() == before.Get(), "Water rendering must restore the original rasterizer state.");
        render.EndRenderTargetScope();
        render.EndRendering();
        const auto pixels = target.Pixels(device, context);
        Check(std::any_of(pixels.begin(), pixels.end(), [](auto pixel)
            { return (pixel & 0xffffff) != 0xcccccc; }), "The underside water receiver must produce refraction pixels.");
        scene.RemoveRenderableInstance(instance);
    }

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

    void CheckMummyFade(RendererDX11& renderer, ID3D11Device3* device, ID3D11DeviceContext3* context)
    {
        const struct
        {
            GamePlay::DisplayType display;
            Desperabis::DisplayType coreDisplay;
            Color4 tint;
            const char* name;
        } materials[]{
            { GamePlay::DisplayType::VectorWaterTint, Desperabis::DisplayType::VectorWaterTint,
                { 200, 180, 140, 255 }, "VectorWaterTint" },
            { GamePlay::DisplayType::VectorLavaTint, Desperabis::DisplayType::VectorLavaTint,
                { 220, 130, 30, 255 }, "VectorLavaTint" },
            { GamePlay::DisplayType::VectorOilTint, Desperabis::DisplayType::VectorOilTint,
                { 30, 50, 80, 255 }, "VectorOilTint" },
            { GamePlay::DisplayType::VectorFullbright, Desperabis::DisplayType::VectorFullbright,
                { 255, 255, 255, 255 }, "VectorFullbright" },
            { GamePlay::DisplayType::VectorBlueTint, Desperabis::DisplayType::VectorBlueTint,
                { 220, 240, 255, 255 }, "VectorBlueTint" },
            { GamePlay::DisplayType::VectorRedLavaTint, Desperabis::DisplayType::VectorRedLavaTint,
                { 255, 80, 40, 255 }, "VectorRedLavaTint" },
        };
        for (int index = 0; index < 6; ++index)
        {
            const auto& material = materials[index];
            Color4 tint;
            float fire = 1.f;
            Check(static_cast<int>(material.display) == 20 + index &&
                static_cast<int>(material.coreDisplay) == 20 + index &&
                GamePlay::IsVectorDisplayType(material.display, tint, fire) &&
                tint.r == material.tint.r && tint.g == material.tint.g &&
                tint.b == material.tint.b && tint.a == 255 && fire == 0.f &&
                !GamePlay::IsSpriteDisplayType(material.display) &&
                std::string(GamePlay::DisplayTypeToString(material.display)) == material.name,
                "Special model modes must preserve native IDs and expose named opaque material tints.");
        }
        const GamePlay::DisplayType shadeModes[]{
            GamePlay::DisplayType::VectorBrightness0, GamePlay::DisplayType::VectorBrightness1,
            GamePlay::DisplayType::VectorBrightness2, GamePlay::DisplayType::VectorBrightness3,
            GamePlay::DisplayType::VectorBrightness4, GamePlay::DisplayType::VectorBrightness5,
            GamePlay::DisplayType::VectorBrightness6,
        };
        const Desperabis::DisplayType coreShadeModes[]{
            Desperabis::DisplayType::VectorBrightness0, Desperabis::DisplayType::VectorBrightness1,
            Desperabis::DisplayType::VectorBrightness2, Desperabis::DisplayType::VectorBrightness3,
            Desperabis::DisplayType::VectorBrightness4, Desperabis::DisplayType::VectorBrightness5,
            Desperabis::DisplayType::VectorBrightness6,
        };
        const byte brightness[]{ 255, 223, 191, 159, 127, 95, 63 };
        for (int shade = 0; shade < 7; ++shade)
        {
            const auto mode = shadeModes[shade];
            Color4 tint;
            float fire = 1.f;
            Check(static_cast<int>(mode) == 30 + shade && static_cast<int>(coreShadeModes[shade]) == 30 + shade &&
                std::string(GamePlay::DisplayTypeToString(mode)) == "VectorBrightness" + std::to_string(shade) &&
                GamePlay::IsVectorDisplayType(mode, tint, fire) &&
                tint.r == brightness[shade] && tint.g == tint.r && tint.b == tint.r &&
                tint.a == 255 && fire == 0.f && !GamePlay::IsSpriteDisplayType(mode),
                "Vector brightness modes must progressively darken opaque models without flames.");
        }
        for (const auto mode : { GamePlay::DisplayType::VectorNormal,
            GamePlay::DisplayType::Type27, GamePlay::DisplayType::Type37 })
        {
            Color4 tint;
            float fire = 1.f;
            Check(GamePlay::IsVectorDisplayType(mode, tint, fire) && tint.r == 255 &&
                tint.g == 255 && tint.b == 255 && tint.a == 255 && fire == 0.f,
                "Ordinary models and the separate mode 37 must retain their original tint.");
        }
        {
            Color4 tint;
            float fire = 1.f;
            Check(GamePlay::IsVectorDisplayType(GamePlay::DisplayType::Type26, tint, fire) &&
                tint.r == 218 && tint.g == 218 && tint.b == 218 && tint.a == 255 && fire == 0.f,
                "Unrecovered mode 26 must retain its previous RGB approximation.");
        }
        for (const int mode : { 50, 85, 120 })
        {
            Color4 tint;
            float fire = -1.f;
            Check(GamePlay::IsVectorDisplayType(static_cast<GamePlay::DisplayType>(mode), tint, fire) &&
                tint.r == 72 && tint.g == 72 && tint.b == 72 && tint.a == 255 &&
                std::fabs(fire - (mode - 50) / 70.f) < 0.00001f,
                "Renaming shade modes must not alter the existing fire-overlay tint or intensity.");
        }

        auto* level = FORCE_GET(AssetManager::Instance().LoadLevel(12));
        const auto records = FORCE_GET(FileManager::Instance().ReadDataFuture(
            FileManager::Instance().FilenameToNative(L"LEVEL\\OBJPOS12.DAT")));
        auto terrain = Desperabis::CreateDummyLevel();
        Desperabis::GameContext core;
        core.level = &terrain;
        core.collisionMap = terrain.collisionMap.get();
        core.objectCollisionsEnabled = true;
        core.tickCount = 1;
        auto& mummy = core.worldObjects[2];
        for (std::size_t offset = 0; offset + 54 <= records.size(); offset += 54)
        {
            const auto object = Desperabis::DecodeGameObject(records.data() + offset, 54);
            if (object.behavior == Desperabis::BehaviorType::Mummie) { mummy = object; break; }
        }
        Check(mummy.behavior == Desperabis::BehaviorType::Mummie, "Expected an original level-12 mummy.");
        mummy.health = 250;
        mummy.generalPurpose1 = 2;
        mummy.generalPurpose2 = 0;
        mummy.modelAnim = mummy.animFrame = 3;
        mummy.modelFrame2 = 0;
        mummy.blendWeightOrSpriteOpacity = 0;
        mummy.collisionMask = 0;
        mummy.posX = 150; mummy.posY = 0; mummy.posZ = 4050;
        mummy.displayType = Desperabis::DisplayType::VectorBrightness0;
        ::RawGameObject raw{};
        raw.m_level = level;
        auto instance = std::make_shared<RenderableInstance>(nullptr);
        Desperabis::EncodeGameObject(mummy, raw.m_rawBytes, 54);
        level->ApplyRenderProperties(instance, raw);
        Check(instance->IsValid() && instance->GetVisible(), "The original mummy corpse must resolve a real mesh.");
        const auto* corpseMesh = instance->GetRenderable();
        instance->UpdateGlobalBounds();
        const auto center = instance->GetGlobalBounds().GetCenter();
        const float span = (std::max)(instance->GetGlobalBounds().GetSphereRadius() * 3.f, 0.2f);
        Matrix4 projection, transform;
        projection.CreateOrthographic(span * 1.5f, span, 0.1f, 20.f);
        transform.SetTranslation(Vector(center.GetX(), center.GetY(), center.GetZ() + 10.f));
        Camera camera;
        camera.SetProjectionMatrix(projection);
        camera.SetCameraTransformMatrix(transform);
        Texture lightmap;
        lightmap.CreatePlainWhite();
        lightmap.m_atlasData = { 0, 0, 0.5f, 0.5f };
        VisibilityResult visible;
        visible.m_mainCamera = &camera;
        visible.m_visibleInstances.m_opaqueInstances.push_back({ instance.get(), 0 });
        visible.m_visibleInstances.m_visibleTransparencyTypes = instance->GetUsedTransparencyMask();
        visible.m_ambientColor = { 1.f, 1.f, 1.f };
        visible.m_fogColor = { 0.f, 0.f, 0.f };
        visible.m_fogFalloff = 0.f;
        visible.m_lightmap = &lightmap;
        Target target(device, 360, 240);
        renderer.ReInitialize(target.colorView.Get(), target.depthView.Get(), target.viewport);
        RenderingSettings settings;
        ProfilingStats stats;
        const auto renderEnergy = [&](const Desperabis::RawGameObject& object)
        {
            Desperabis::EncodeGameObject(object, raw.m_rawBytes, 54);
            level->ApplyRenderProperties(instance, raw);
            Check(instance->GetRenderable() == corpseMesh && instance->GetBlendTargetRenderable() == nullptr &&
                instance->m_tintColor.a == 255 && !instance->HasTransparentMaterials(),
                "Corpse darkening must update tint even when the opaque model mesh is unchanged.");
            const float black[]{ 0, 0, 0, 1 };
            context->ClearRenderTargetView(target.colorView.Get(), black);
            context->ClearDepthStencilView(target.depthView.Get(), D3D11_CLEAR_DEPTH, 1.f, 0);
            renderer.RenderSceneElements(visible, settings, stats);
            std::uint64_t energy = 0;
            int pixels = 0;
            for (const auto pixel : target.Pixels(device, context))
            {
                energy += (pixel & 255) + ((pixel >> 8) & 255) + ((pixel >> 16) & 255);
                if ((pixel & 0xffffff) != 0) { ++pixels; }
            }
            Check(pixels >= 100, "Every mummy fade shade must render actual visible corpse pixels.");
            return energy;
        };
        auto previousEnergy = renderEnergy(mummy);
        for (const auto& material : materials)
        {
            auto tinted = mummy;
            tinted.displayType = material.coreDisplay;
            const auto energy = renderEnergy(tinted);
            Check(instance->m_tintColor.r == material.tint.r &&
                instance->m_tintColor.g == material.tint.g && instance->m_tintColor.b == material.tint.b &&
                (material.display == GamePlay::DisplayType::VectorFullbright ?
                    energy == previousEnergy : energy < previousEnergy),
                "Every special tint must reach and visibly modulate the same real opaque model.");
        }
        previousEnergy = renderEnergy(mummy);
        int previousMode = 30;
        bool smoke = false;
        for (int tick = 0; tick < 250; ++tick)
        {
            Desperabis::UpdateObjects(core);
            smoke |= std::any_of(core.worldObjects.begin() + 3, core.worldObjects.end(),
                [](const auto& object) { return object.behavior == Desperabis::BehaviorType::SmokeFading; });
            if (mummy.health == 0) { break; }
            const int mode = static_cast<int>(mummy.displayType);
            if (mode != previousMode)
            {
                const auto energy = renderEnergy(mummy);
                Check(mode == previousMode + 1 && energy < previousEnergy * 0.97,
                    "Core-driven mummy fade stages must visibly darken before removal.");
                previousEnergy = energy;
                previousMode = mode;
            }
        }
        Check(previousMode == 35 && mummy.health == 0 && smoke,
            "Mummy lifetime and smoke emission must remain intact through all six native corpse shades.");
        auto darkest = mummy;
        darkest.health = 1000;
        darkest.displayType = Desperabis::DisplayType::VectorBrightness6;
        Check(renderEnergy(darkest) < previousEnergy * 0.97,
            "The seventh fade mode used by other model particles must also darken.");
        std::cout << "Mummy: six core-driven corpse fade shades, smoke/removal and seventh model shade passed.\n";
        std::cout << "Model display: six named material tints, seven brightness shades and unchanged fire/unknown modes passed.\n";
    }

    void CheckMaps(ApplicationUi& ui, RendererDX11& renderer, const Level& level,
        const Font& font, const TextureAnim& hud, ID3D11Device3* device, ID3D11DeviceContext3* context,
        const char* screenshot = nullptr)
    {
        auto geometry = Desperabis::CreateDummyLevel();
        geometry.collisionMap->widthMinusOne = 23;
        geometry.collisionMap->heightMinusOne = 19;
        geometry.geometryWidth = 6;
        geometry.geometryHeight = 5;
        std::memset(geometry.ceilingObjects, 0, sizeof(geometry.ceilingObjects));
        for (int row = 0; row < 5; ++row) { geometry.wallTextures[row][0] = 193; }
        for (int row = 1; row <= 3; ++row)
        {
            for (int column = 1; column <= 4; ++column)
            {
                geometry.floorObjects[row][column] = 1;
                geometry.ceilingObjects[row][column] = 1;
            }
        }
        geometry.surfaceFlags[2][2] = 1;
        for (int row = 4; row < 16; ++row) { geometry.collisionMap->data[row * 24 + 5] = 56; }
        for (int row = 8; row < 12; ++row)
        {
            for (int column = 8; column < 12; ++column) { geometry.collisionMap->data[row * 24 + column] = 1; }
        }
        Desperabis::GameContext core;
        core.level = &geometry;
        core.collisionMap = geometry.collisionMap.get();
        Check(ui.UploadCollisionMap(core, renderer) && !core.collisionmapUploadDirty &&
            !ui.UploadCollisionMap(core, renderer), "The first map upload must be lazy and cached.");
        Check(ui.UploadGeometryMap(core, renderer) && !ui.UploadGeometryMap(core, renderer),
            "The geometry map must have its own lazy texture cache.");
        const auto texturePixels = [&]
        {
            Target capture(device, 240, 360);
            context->CopyResource(capture.color.Get(), ui.GetGeometryMapTexture()->
                GetNativeRuntimeObj<NativeTextureObjectDX11>()->m_nativeTex.Get());
            const auto uploaded = capture.Pixels(device, context);
            std::vector<std::uint32_t> logical(360 * 240);
            for (int y = 0; y < 240; ++y)
            {
                for (int x = 0; x < 360; ++x) { logical[y * 360 + x] = uploaded[x * 240 + y]; }
            }
            return logical;
        };
        const auto shaded = texturePixels();
        const auto blueAt = [&](int x, int y)
        {
            const auto pixel = shaded[y * 360 + x];
            return static_cast<int>((pixel >> 16) & 255) > static_cast<int>(pixel & 255) + 20;
        };
        Check(blueAt(179, 110) && !blueAt(180, 110) && !blueAt(150, 135),
            "Water shading must stop exactly before the neighbouring tile in both axes.");
        for (int shape = 0; shape < 8; ++shape)
        {
            geometry.wallTextures[1][2] = static_cast<byte>((shape << 5) | 1);
            ++geometry.geometryRevision;
            Check(ui.UploadGeometryMap(core, renderer), "Wall changes must invalidate the geometry map.");
            const auto pixels = texturePixels();
            const auto inkAt = [&](int x, int y) { return pixels[y * 360 + x] == 0xff0c1114; };
            Check(inkAt(140, 75) == (shape == 0 || shape == 5) &&
                inkAt(180, 75) == (shape == 2 || shape == 5) &&
                inkAt(160, 55) == (shape == 1 || shape == 4) &&
                inkAt(160, 95) == (shape == 3 || shape == 4),
                "The modal must draw exactly the native one-pixel wall segments for all eight shape selectors.");
        }
        ui.UploadCollisionMap(core, renderer);
        Camera camera;
        RenderingSettings settings;
        ProfilingStats stats;
        Desperabis::UiMap map{ { 80, 50, 120, 100 }, false, 10, 10, 0, 4 };
        for (const auto size : { std::pair<UINT, UINT>{ 360,240 }, { 1280,720 }, { 480,800 } })
        {
            Target target(device, size.first, size.second);
            renderer.ReInitialize(target.colorView.Get(), target.depthView.Get(), target.viewport);
            const auto render = [&](const Desperabis::ApplicationPresentation& recipe)
            {
                const float background[]{ 0.1f, 0.2f, 0.3f, 1.f };
                context->ClearRenderTargetView(target.colorView.Get(), background);
                VisibilityResult empty;
                empty.m_mainCamera = &camera;
                renderer.RenderSceneElements(empty, settings, stats);
                ui.Draw(recipe, renderer, level, font, hud, settings, stats, target.viewport,
                    context, target.colorView.Get(), target.depthView.Get());
                return target.Pixels(device, context);
            };
            const Desperabis::ApplicationPresentation empty{ Desperabis::MenuPage::Gameplay, {} };
            const auto baseline = render(empty);
            map.heading = 0;
            const auto first = render({ Desperabis::MenuPage::Gameplay, { map } });
            map.heading = 128;
            const auto rotated = render({ Desperabis::MenuPage::Gameplay, { map } });
            Check(!ui.UploadCollisionMap(core, renderer), "Rotating the map must not upload its pixels again.");
            const float scale = (std::min)(size.first / 360.f, size.second / 240.f);
            const float ox = size.first - 360 * scale;
            const float oy = size.second - 240 * scale;
            int changed = 0, blue = 0, brown = 0, gray = 0, rotatedPixels = 0;
            for (UINT y = 0; y < size.second; ++y)
            {
                for (UINT x = 0; x < size.first; ++x)
                {
                    const auto i = y * size.first + x;
                    const bool inside = x + 0.5f >= ox + map.bounds.x * scale &&
                        x + 0.5f < ox + (map.bounds.x + map.bounds.width) * scale &&
                        y + 0.5f >= oy + map.bounds.y * scale &&
                        y + 0.5f < oy + (map.bounds.y + map.bounds.height) * scale;
                    if (!inside)
                    {
                        Check(first[i] == baseline[i] && rotated[i] == baseline[i],
                            "Rotating map sampling must never draw outside the clipped viewport.");
                    }
                    else
                    {
                        changed += first[i] != baseline[i];
                        rotatedPixels += first[i] != rotated[i];
                        const int r = first[i] & 255, g = (first[i] >> 8) & 255, b = (first[i] >> 16) & 255;
                        if (first[i] != baseline[i])
                        {
                            blue += b > r + 20;
                            brown += r > b + 15;
                            gray += r > 140 && std::abs(r - g) < 10 && std::abs(r - b) < 15;
                        }
                    }
                }
            }
            Check(changed > 100 && rotatedPixels > 100 && blue > 20 && brown > 20 && gray > 20,
                "The map must visibly rotate and distinguish blue water, brown floor and gray walls.");
            core.SetCollisionMapPixelAtWorldXZ(312, 3888, 56);
            core.SetCollisionMapPixelAtWorldXZ(337, 3888, 56);
            Check(ui.UploadCollisionMap(core, renderer) && !ui.UploadCollisionMap(core, renderer),
                "Multiple collision edits must coalesce into exactly one lazy upload.");
            Check(!ui.UploadGeometryMap(core, renderer),
                "Collision-only edits must not rebuild the geometry map.");
            const auto edited = render({ Desperabis::MenuPage::Gameplay, { map } });
            Check(edited != rotated, "Collision edits must change the actual rendered map pixels.");
            core.SetCollisionMapPixelAtWorldXZ(312, 3888, 1);
            core.SetCollisionMapPixelAtWorldXZ(337, 3888, 1);
            ui.UploadCollisionMap(core, renderer);
            core.SetMenuPage(Desperabis::MenuPage::Map);
            const auto paper = render({ Desperabis::MenuPage::Map, {
                Desperabis::UiBitmap{ { 0,0,360,240 }, Desperabis::UiBitmapSource::MapPaper, 0,0,0,{} } } });
            const auto modal = render(core.GetApplicationPresentation());
            Check(modal != paper && modal != baseline, "The modal map must render on the original parchment.");
            int waterPixels = 0, paperPixels = 0;
            for (std::size_t i = 0; i < modal.size(); ++i)
            {
                const int r = modal[i] & 255, b = (modal[i] >> 16) & 255;
                waterPixels += b > r + 20;
                paperPixels += modal[i] == paper[i] && paper[i] != baseline[i];
            }
            Check(waterPixels > 100 && paperPixels > 1000,
                "The full map must retain blue water and show parchment through its unoccupied area.");
        }
        ++geometry.geometryRevision;
        Check(ui.UploadCollisionMap(core, renderer), "Geometry coverage changes must invalidate map presentation.");
        Check(ui.UploadGeometryMap(core, renderer), "Geometry revisions must rebuild the separate map texture.");
        ui.Clear();
        Check(ui.UploadCollisionMap(core, renderer), "Clearing renderer caches must require a fresh upload.");
        core.collisionMap->widthMinusOne = 200;
        core.collisionmapUploadDirty = true;
        bool rejected = false;
        try { ui.UploadCollisionMap(core, renderer); }
        catch (const std::runtime_error&) { rejected = true; }
        Check(rejected && core.collisionmapUploadDirty, "Failed uploads must leave the map dirty and report an error.");
        ui.Clear();
        class OriginalMapHost final : public Desperabis::GameContext
        {
            std::vector<std::uint8_t> LoadFile(std::string_view filename) override
            {
                const std::wstring name(filename.begin(), filename.end());
                return FORCE_GET(FileManager::Instance().ReadDataFuture(
                    FileManager::Instance().FilenameToNative(name)));
            }
            void LoadLevel(const Desperabis::LevelLoadRequest& request) override
            {
                Desperabis::LevelLoadData data;
                LoadLevelGeometry(request.levelNumber, data);
                LoadLevelObjects(request.levelNumber, data);
                InitializeLevelPlayerSpawn(data, { 0, 0, 100 });
                CompleteLevelLoad(request.id, std::move(data));
            }
        } original;
        Target realTarget(device, 360, 240);
        for (const int number : { 11, 12, 14 })
        {
            original.StartLevel(static_cast<std::int16_t>(number));
            auto picture = std::find_if(original.worldObjects.begin() + 2, original.worldObjects.end(),
                [](const auto& object) { return object.health > 0 &&
                    object.collisionType == Desperabis::CollisionType::PictureMap; });
            Check(picture != original.worldObjects.end(), "Expected an original level-11/12/14 wall map.");
            const auto before = *picture;
            Check(Desperabis::HandlePlayerObjectInteraction(*picture, original).status ==
                Desperabis::InteractionStatus::Applied && original.GetMenuPage() == Desperabis::MenuPage::Map &&
                std::memcmp(&before, &*picture, sizeof(before)) == 0,
                "Original wall maps must open the modal without altering their baked preview/model.");
            Check(ui.UploadGeometryMap(original, renderer), "A different real level must replace the geometry-map texture.");
            auto* realLevel = FORCE_GET(AssetManager::Instance().LoadLevel(number));
            const auto model = realLevel->GetLevelInfo().GetModelAnim(picture->modelId - 30);
            Check(model != nullptr, "The original wall-map model must be loaded.");
            auto instance = std::make_shared<RenderableInstance>(nullptr);
            ::RawGameObject raw{};
            raw.m_level = realLevel;
            Desperabis::EncodeGameObject(*picture, raw.m_rawBytes, 54);
            realLevel->ApplyRenderProperties(instance, raw);
            const auto* mesh = instance->GetRenderable();
            Check(mesh != nullptr && model->m_textureUsedMask == 3327,
                "The original VEKT14 wall-map geometry and textures must remain intact.");
            ui.ApplyMapMaterial(*instance, *model, renderer);
            int replaced = 0, preserved = 0;
            const auto* mapRuntime = ui.GetGeometryMapTexture()->GetNativeRuntimeObj<NativeTextureObjectDX11>();
            for (std::size_t i = 0; i < mesh->m_drawCalls.size(); ++i)
            {
                const auto& originalMaterial = mesh->m_drawCalls[i].m_material;
                const auto& material = instance->m_materialOverride[i];
                if (originalMaterial.m_textureSlot == 6)
                {
                    const auto* view = material.m_texture->GetNativeRuntimeObj<NativeTextureObjectDX11>();
                    Check(view->m_nativeTex.Get() == mapRuntime->m_nativeTex.Get() &&
                        view->m_nativeTexView.Get() == mapRuntime->m_nativeTexView.Get(),
                        "The model preview must share the modal texture, not a second upload.");
                    const auto source = model->m_ownTextures[5].m_atlasData;
                    const auto remap = material.m_texture->m_atlasData;
                    const auto uv0 = source.Apply(0, 0), uv1 = source.Apply(1, 1);
                    const auto mapped0 = remap.Apply(uv0.x, uv0.y), mapped1 = remap.Apply(uv1.x, uv1.y);
                    Check(std::abs(mapped0.x) < 0.00001f && std::abs(mapped0.y) < 0.00001f &&
                        std::abs(mapped1.x - 1) < 0.00001f && std::abs(mapped1.y - 1) < 0.00001f,
                        "The preview override must undo baked atlas UVs without changing shared mesh data.");
                    ++replaced;
                }
                else
                {
                    Check(material.m_texture == originalMaterial.m_texture &&
                        material.m_materialHash == originalMaterial.m_materialHash,
                        "The frame and all non-map materials must be unchanged.");
                    ++preserved;
                }
            }
            Check(replaced == 1 && preserved > 0 && instance->GetRenderable() == mesh,
                "Only the original map panel may receive the generated texture.");
            renderer.ReInitialize(realTarget.colorView.Get(), realTarget.depthView.Get(), realTarget.viewport);
            VisibilityResult empty;
            empty.m_mainCamera = &camera;
            renderer.RenderSceneElements(empty, settings, stats);
            ui.Draw(original.GetApplicationPresentation(), renderer, level, font, hud, settings, stats,
                realTarget.viewport, context, realTarget.colorView.Get(), realTarget.depthView.Get());
            const auto pixels = realTarget.Pixels(device, context);
            int blue = 0, brown = 0;
            for (const auto pixel : pixels)
            {
                const int r = pixel & 255, b = (pixel >> 16) & 255;
                blue += b > r + 20;
                brown += r > b + 15;
            }
            Check(brown > 1000 && (number != 12 || blue > 20),
                "Real maps must show parchment/floors and level 12's blue water.");
            if (screenshot && number == 12) { realTarget.SaveBitmap(screenshot, pixels); }
            auto unmodified = std::make_shared<RenderableInstance>(nullptr);
            Texture lightmap;
            lightmap.CreatePlainWhite();
            lightmap.m_atlasData = { 0, 0, 0.5f, 0.5f };
            int mostChanged = 0, mostBlue = 0;
            for (const int heading : { 0, 128, 256, 384 })
            {
                auto object = *picture;
                object.posX = 150; object.posY = 0; object.posZ = 4050;
                object.winY = static_cast<std::int16_t>(heading);
                Desperabis::EncodeGameObject(object, raw.m_rawBytes, 54);
                realLevel->ApplyRenderProperties(unmodified, raw);
                realLevel->ApplyRenderProperties(instance, raw);
                ui.ApplyMapMaterial(*instance, *model, renderer);
                instance->UpdateGlobalBounds();
                const auto center = instance->GetGlobalBounds().GetCenter();
                const float span = (std::max)(instance->GetGlobalBounds().GetSphereRadius() * 3.f, 0.2f);
                Matrix4 projection, transform;
                projection.CreateOrthographic(span * 1.5f, span, 0.1f, 20.f);
                transform.SetTranslation(Vector(center.GetX(), center.GetY(), center.GetZ() + 10.f));
                Camera modelCamera;
                modelCamera.SetProjectionMatrix(projection);
                modelCamera.SetCameraTransformMatrix(transform);
                const auto renderModel = [&](const RenderableInstance* drawn)
                {
                    VisibilityResult visible;
                    visible.m_mainCamera = &modelCamera;
                    visible.m_visibleInstances.m_opaqueInstances.push_back({ drawn, 0 });
                    visible.m_visibleInstances.m_visibleTransparencyTypes = drawn->GetUsedTransparencyMask();
                    visible.m_ambientColor = { 1.f, 1.f, 1.f };
                    visible.m_fogColor = { 0.f, 0.f, 0.f };
                    visible.m_fogFalloff = 0.f;
                    visible.m_lightmap = &lightmap;
                    const float black[]{ 0,0,0,1 };
                    context->ClearRenderTargetView(realTarget.colorView.Get(), black);
                    context->ClearDepthStencilView(realTarget.depthView.Get(), D3D11_CLEAR_DEPTH, 1.f, 0);
                    renderer.RenderSceneElements(visible, settings, stats);
                    return realTarget.Pixels(device, context);
                };
                const auto baseline = renderModel(unmodified.get());
                const auto preview = renderModel(instance.get());
                int changed = 0, water = 0, waterX = 0, waterY = 0;
                int left = 360, right = 0, top = 240, bottom = 0;
                for (std::size_t i = 0; i < preview.size(); ++i)
                {
                    const int x = static_cast<int>(i % 360), y = static_cast<int>(i / 360);
                    if (preview[i] != baseline[i])
                    {
                        ++changed;
                        left = (std::min)(left, x); right = (std::max)(right, x);
                        top = (std::min)(top, y); bottom = (std::max)(bottom, y);
                    }
                    if (static_cast<int>((preview[i] >> 16) & 255) >
                        static_cast<int>(preview[i] & 255) + 20)
                    {
                        ++water;
                        waterX += x; waterY += y;
                    }
                }
                if (number == 12 && changed > 100)
                {
                    Check(water > 20 && waterX * 2 > (left + right) * water &&
                        waterY * 2 < (top + bottom) * water,
                        "The 3D panel must show level 12's water upper-right, matching the modal orientation.");
                }
                if (screenshot && number == 12 && changed > mostChanged)
                {
                    realTarget.SaveBitmap((std::string(screenshot) + ".panel.bmp").c_str(), preview);
                }
                mostChanged = (std::max)(mostChanged, changed);
                mostBlue = (std::max)(mostBlue, water);
            }
            Check(mostChanged > 100 && (number != 12 || mostBlue > 20),
                "The actual 3D panel must visibly render the new map, including level 12's water.");
            const auto* mapTexture = ui.GetGeometryMapTexture();
            ComPtr<ID3D11Texture2D> oldTexture = mapTexture->GetNativeRuntimeObj<NativeTextureObjectDX11>()->m_nativeTex;
            ++original.level->geometryRevision;
            Check(ui.UploadGeometryMap(original, renderer) && ui.GetGeometryMapTexture() == mapTexture &&
                !ui.UploadGeometryMap(original, renderer),
                "Geometry edits must refresh one stable shared map texture exactly once.");
            for (std::size_t i = 0; i < mesh->m_drawCalls.size(); ++i)
            {
                if (mesh->m_drawCalls[i].m_material.m_textureSlot != 6) { continue; }
                const auto* refreshed = instance->m_materialOverride[i].m_texture->
                    GetNativeRuntimeObj<NativeTextureObjectDX11>();
                Check(refreshed->m_nativeTex.Get() != oldTexture.Get() &&
                    refreshed->m_nativeTex.Get() == mapTexture->
                        GetNativeRuntimeObj<NativeTextureObjectDX11>()->m_nativeTex.Get(),
                    "Existing panel overrides must follow regenerated map pixels without another assignment.");
            }
            original.SetMenuPage(Desperabis::MenuPage::Gameplay);
            if (number == 11)
            {
                auto pickup = std::find_if(original.worldObjects.begin() + 2, original.worldObjects.end(),
                    [](const auto& object) { return object.health > 0 &&
                        object.collisionType == Desperabis::CollisionType::PickableMap; });
                Check(pickup != original.worldObjects.end() &&
                    Desperabis::HandlePlayerObjectInteraction(*pickup, original).status ==
                        Desperabis::InteractionStatus::Applied &&
                    original.hasMap && pickup->health == 0 &&
                    std::holds_alternative<Desperabis::UiMap>(original.GetApplicationPresentation().commands.back()),
                    "The actual level-11 pickup must enable the core's corner-map recipe.");
            }
        }
        ui.Clear();
        std::cout << "Maps: independent collision/geometry caches, nonoverlapping tiles, native wall shapes, "
            "shared GPU preview and unchanged frame, original level-11/12/14 panels, three aspect ratios passed.\n";
    }

    void CheckDragon(RendererDX11& renderer, ID3D11Device3* device, ID3D11DeviceContext3* context)
    {
        auto* level = FORCE_GET(AssetManager::Instance().LoadLevel(15));
        const auto records = FORCE_GET(FileManager::Instance().ReadDataFuture(
            FileManager::Instance().FilenameToNative(L"LEVEL\\OBJPOS15.DAT")));
        const auto bytes = FORCE_GET(FileManager::Instance().ReadDataFuture(
            FileManager::Instance().FilenameToNative(L"VEKT\\VEKT19.3DA")));
        auto terrain = Desperabis::CreateDummyLevel();
        terrain.modelAnchors[6] = std::make_shared<Desperabis::ModelAnchorGeometry>();
        terrain.modelAnchors[6]->Load(bytes.data(), bytes.size());
        Desperabis::GameContext core;
        core.level = &terrain;
        core.collisionMap = terrain.collisionMap.get();
        core.worldObjects[12] = Desperabis::DecodeGameObject(records.data() + 10 * 54, 54);
        core.worldObjects[5] = Desperabis::DecodeGameObject(records.data() + 3 * 54, 54);
        auto& dragon = core.worldObjects[12];
        auto& head = core.worldObjects[5];
        auto bodyInstance = std::make_shared<RenderableInstance>(nullptr);
        auto headInstance = std::make_shared<RenderableInstance>(nullptr);
        ::RawGameObject raw{};
        raw.m_level = level;
        int anchorComparisons = 0;
        for (int bank = 1; bank <= 2; ++bank)
        {
            dragon.modelAnim = static_cast<std::uint8_t>(bank);
            for (int frame = 1; frame <= 4; ++frame)
            {
                dragon.animFrame = static_cast<std::uint8_t>(frame);
                dragon.modelFrame2 = static_cast<std::uint8_t>(frame % 4 + 1);
                for (int weight : { 0, 50, 100, 150, 201 })
                {
                    dragon.blendWeightOrSpriteOpacity = static_cast<std::int16_t>(weight);
                    dragon.winX = static_cast<std::int16_t>(frame * 19);
                    dragon.winY = static_cast<std::int16_t>(frame * 113);
                    dragon.winZ = static_cast<std::int16_t>(frame * -17);
                    core.UpdateModelAttachments();
                    Desperabis::EncodeGameObject(dragon, raw.m_rawBytes, 54);
                    level->ApplyRenderProperties(bodyInstance, raw);
                    const auto& frames = terrain.modelAnchors[6]->animations[bank - 1];
                    const auto& current = frames[frame - 1];
                    const float blend = weight > 200 ? 0.f : (weight > 100 ? (200 - weight) * 0.01f : weight * 0.01f);
                    for (bool muzzle : { false, true })
                    {
                        const auto anchor = muzzle ? current.muzzleVertex : current.billboardVertex;
                        if (!anchor)
                        {
                            Check(muzzle && bank == 2, "Every live pose must contain both head and muzzle anchors.");
                            continue;
                        }
                        const auto index = *anchor - 1;
                        float vertex[3]{};
                        for (int axis = 0; axis < 3; ++axis)
                        {
                            vertex[axis] = current.vertices[index][axis] +
                                (frames[dragon.modelFrame2 - 1].vertices[index][axis] - current.vertices[index][axis]) * blend;
                        }
                        const auto rendered = bodyInstance->GetTransform().TransformPosition(
                            Vector(-vertex[0] * DespVertToFloat, -vertex[1] * DespVertToFloat, vertex[2] * DespVertToFloat));
                        const auto point = core.GetModelAnchor(dragon, muzzle);
                        const auto anchored = ::Level::DespToPos((*point)[0], (*point)[1], (*point)[2]);
                        Check(std::fabs(rendered.GetX() - anchored.GetX()) <= DespToFloat &&
                            std::fabs(rendered.GetY() - anchored.GetY()) <= DespToFloat &&
                            std::fabs(rendered.GetZ() - anchored.GetZ()) <= DespToFloat,
                            "Core head/muzzle anchors must match the renderer's actual blended X/Z/Y matrix within one native unit.");
                        ++anchorComparisons;
                    }
                    Check(bodyInstance->GetVisible() && bodyInstance->GetRenderable() != nullptr,
                        "All live/dying dragon model frames must resolve real meshes.");
                }
            }
        }
        dragon.modelAnim = dragon.animFrame = 1; dragon.modelFrame2 = 2;
        dragon.winX = dragon.winY = dragon.winZ = 0; dragon.blendWeightOrSpriteOpacity = 0;
        const auto heads = level->GetLevelInfo().GetTextureAnim(5);
        Check(heads != nullptr, "Dragon head and fire particles must resolve original ANIM5.");
        RenderingSettings settings;
        ProfilingStats stats;
        Target target(device, 360, 240);
        renderer.ReInitialize(target.colorView.Get(), target.depthView.Get(), target.viewport);
        int minimumPixels = 360 * 240;
        Texture lightmap;
        lightmap.CreatePlainWhite();
        lightmap.m_atlasData = { 0, 0, 0.5f, 0.5f };
        for (int bank = 1; bank <= 6; ++bank)
        {
            dragon.winY = static_cast<std::int16_t>((43 - (bank - 1) * 86) & 511);
            for (int mouth = 1; mouth <= 2; ++mouth)
            {
                dragon.spriteScale = static_cast<std::uint8_t>(mouth);
                core.UpdateModelAttachments();
                Check(head.modelAnim == bank && head.animFrame == mouth, "Core must select all six view banks and both mouth phases.");
                Desperabis::EncodeGameObject(head, raw.m_rawBytes, 54);
                level->ApplyRenderProperties(headInstance, raw);
                Check(headInstance->IsValid() && headInstance->GetVisible() && heads->IsValidFrame(bank - 1, mouth - 1) &&
                    headInstance->m_materialOverride[0].m_texture == heads->GetFrame(bank - 1, mouth - 1) &&
                    std::fabs(headInstance->m_uniformScale - 0.5f) < 0.00001f,
                    "Attached head must use the original direction/mouth texture at native scale 32/64.");
                Desperabis::EncodeGameObject(dragon, raw.m_rawBytes, 54);
                level->ApplyRenderProperties(bodyInstance, raw);
                bodyInstance->UpdateGlobalBounds();
                headInstance->UpdateGlobalBounds();
                const auto center = bodyInstance->GetGlobalBounds().GetCenter();
                const float span = (std::max)(bodyInstance->GetGlobalBounds().GetSphereRadius() * 3.f, 0.2f);
                Matrix4 projection, transform;
                projection.CreateOrthographic(span * 1.5f, span, 0.1f, 20.f);
                transform.SetTranslation(Vector(center.GetX(), center.GetY(), center.GetZ() + 10.f));
                Camera camera;
                camera.SetProjectionMatrix(projection);
                camera.SetCameraTransformMatrix(transform);
                VisibilityResult visible;
                visible.m_mainCamera = &camera;
                visible.m_visibleInstances.m_opaqueInstances.push_back({ bodyInstance.get(), 0 });
                visible.m_visibleInstances.m_opaqueInstances.push_back({ headInstance.get(), 0 });
                visible.m_visibleInstances.m_visibleTransparencyTypes =
                    bodyInstance->GetUsedTransparencyMask() | headInstance->GetUsedTransparencyMask();
                visible.m_ambientColor = { 1.f, 1.f, 1.f }; visible.m_fogFalloff = 0.f;
                visible.m_fogColor = { 0.f, 0.f, 0.f };
                visible.m_lightmap = &lightmap;
                const float black[4]{ 0, 0, 0, 1 };
                context->ClearRenderTargetView(target.colorView.Get(), black);
                context->ClearDepthStencilView(target.depthView.Get(), D3D11_CLEAR_DEPTH, 1.f, 0);
                renderer.RenderSceneElements(visible, settings, stats);
                const auto pixels = target.Pixels(device, context);
                const auto count = static_cast<int>(std::count_if(pixels.begin(), pixels.end(),
                    [](std::uint32_t pixel) { return (pixel & 0xffffff) != 0; }));
                minimumPixels = (std::min)(minimumPixels, count);
                Check(count >= 100, "Complete original dragon body/head must render visible pixels for all view/mouth phases.");
                for (auto* part : { bodyInstance.get(), headInstance.get() })
                {
                    const auto partCenter = part->GetGlobalBounds().GetCenter();
                    const float partSpan = (std::max)(part->GetGlobalBounds().GetSphereRadius() * 3.f, 0.2f);
                    projection.CreateOrthographic(partSpan * 1.5f, partSpan, 0.1f, 20.f);
                    transform.SetTranslation(Vector(partCenter.GetX(), partCenter.GetY(), partCenter.GetZ() + 3.f));
                    camera.SetProjectionMatrix(projection);
                    camera.SetCameraTransformMatrix(transform);
                    visible.m_visibleInstances.m_opaqueInstances.clear();
                    visible.m_visibleInstances.m_opaqueInstances.push_back({ part, 0 });
                    context->ClearRenderTargetView(target.colorView.Get(), black);
                    context->ClearDepthStencilView(target.depthView.Get(), D3D11_CLEAR_DEPTH, 1.f, 0);
                    renderer.RenderSceneElements(visible, settings, stats);
                    const auto partPixels = target.Pixels(device, context);
                    const auto partCount = static_cast<int>(std::count_if(partPixels.begin(), partPixels.end(),
                        [](std::uint32_t pixel) { return (pixel & 0xffffff) != 0; }));
                    Check(partCount >= 8, part == bodyInstance.get() ?
                        "Real dragon body must draw independently of its head." :
                        "Every original head direction/mouth frame must draw independently of the body.");
                }
            }
            auto fireball = head;
            fireball.animFrame = 3; fireball.health = 500; fireball.spriteScale = 40;
            fireball.behavior = Desperabis::BehaviorType::FlyingArrow;
            fireball.displayType = Desperabis::DisplayType::SpriteTorchlight;
            Desperabis::EncodeGameObject(fireball, raw.m_rawBytes, 54);
            level->ApplyRenderProperties(headInstance, raw);
            Check(headInstance->GetVisible() && headInstance->m_materialOverride[0].m_texture == heads->GetFrame(bank - 1, 2) &&
                !headInstance->m_materialOverride[0].m_applyLighting,
                "Model-bank-5 fireballs must use ANIM5 frame three, not bank-six FACKEL slots.");
            headInstance->UpdateGlobalBounds();
            const auto center = headInstance->GetGlobalBounds().GetCenter();
            const float span = (std::max)(headInstance->GetGlobalBounds().GetSphereRadius() * 3.f, 0.2f);
            Matrix4 projection, transform;
            projection.CreateOrthographic(span * 1.5f, span, 0.1f, 10.f);
            transform.SetTranslation(Vector(center.GetX(), center.GetY(), center.GetZ() + 3.f));
            Camera camera;
            camera.SetProjectionMatrix(projection); camera.SetCameraTransformMatrix(transform);
            VisibilityResult visible;
            visible.m_mainCamera = &camera;
            visible.m_visibleInstances.m_opaqueInstances.push_back({ headInstance.get(), 0 });
            visible.m_visibleInstances.m_visibleTransparencyTypes = headInstance->GetUsedTransparencyMask();
            visible.m_ambientColor = { 1, 1, 1 }; visible.m_fogColor = { 0, 0, 0 }; visible.m_fogFalloff = 0;
            visible.m_lightmap = &lightmap;
            const float black[4]{ 0, 0, 0, 1 };
            context->ClearRenderTargetView(target.colorView.Get(), black);
            context->ClearDepthStencilView(target.depthView.Get(), D3D11_CLEAR_DEPTH, 1, 0);
            renderer.RenderSceneElements(visible, settings, stats);
            const auto pixels = target.Pixels(device, context);
            Check(std::count_if(pixels.begin(), pixels.end(),
                [](std::uint32_t pixel) { return (pixel & 0xffffff) != 0; }) >= 8,
                "All six original fireball sprites must draw actual pixels.");
            fireball.behavior = Desperabis::BehaviorType::FireballImpact;
            fireball.health = 10; fireball.displayType = Desperabis::DisplayType::SpriteTransparent;
            fireball.blendWeightOrSpriteOpacity = 4;
            Desperabis::EncodeGameObject(fireball, raw.m_rawBytes, 54);
            level->ApplyRenderProperties(headInstance, raw);
            Check(headInstance->GetUsedTransparencyMask() == TransparencyModeMask::AlphaBlendedBit &&
                std::fabs(headInstance->m_materialOverride[0].m_opacity - 5.f / 6.f) < 0.00001f,
                "Fireball impacts must refresh alpha-pass classification even when the sprite mesh is unchanged.");
            visible.m_visibleInstances.m_opaqueInstances.clear();
            visible.m_visibleInstances.m_transparentInstances.push_back({ headInstance.get(), 0 });
            visible.m_visibleInstances.m_visibleTransparencyTypes = headInstance->GetUsedTransparencyMask();
            context->ClearRenderTargetView(target.colorView.Get(), black);
            context->ClearDepthStencilView(target.depthView.Get(), D3D11_CLEAR_DEPTH, 1, 0);
            renderer.RenderSceneElements(visible, settings, stats);
            const auto flashPixels = target.Pixels(device, context);
            Check(std::count_if(flashPixels.begin(), flashPixels.end(),
                [](std::uint32_t pixel) { return (pixel & 0xffffff) != 0; }) >= 8,
                "Fading fireball impacts must remain visible in the alpha pass.");
        }
        std::cout << "Dragon: " << anchorComparisons << " head/muzzle matrix comparisons, 12 full body/head renders, minimum " <<
            minimumPixels << " pixels; all fireball/impact textures and alpha transitions passed.\n";
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
        CheckMummyFade(renderer, device.Get(), context.Get());
        CheckDragon(renderer, device.Get(), context.Get());
        CheckRainSprites(renderer, device.Get(), context.Get());
        CheckLightning(renderer, device.Get(), context.Get());
        CheckUnderwater(renderer, device.Get(), context.Get());
        CheckUnderwaterSurface(renderer, device.Get(), context.Get());
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
        CheckMaps(ui, renderer, *level, font, *hud, device.Get(), context.Get(), argc >= 4 ? argv[3] : nullptr);
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
                if (argc >= 2)
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
        for (const auto levelNumber : { 10, 11, 50 })
        {
            const auto* weaponLevel = FORCE_GET(AssetManager::Instance().LoadLevel(levelNumber));
            const auto* swordModel = weaponLevel->GetLevelInfo().GetModelAnim(1)->GetModel(1, 0);
            const auto originalMaterial = swordModel->m_drawCalls[0].m_material;
            const auto placeableData = FORCE_GET(FileManager::Instance().ReadDataFuture(
                FileManager::Instance().FilenameToNative(L"LEVEL\\OBJPOS10.DAT")));
            const auto placeable = Desperabis::DecodeGameObject(placeableData.data() + 73 * 54, 54);
            Check(placeable.behavior == Desperabis::BehaviorType::BearTrapAmmo, "Weapon view needs the original bear-trap fixture.");
            for (const auto size : { std::pair<UINT, UINT>{ 360,240 }, { 1280,720 }, { 480,800 } })
            {
                Target target(device.Get(), size.first, size.second);
                renderer.ReInitialize(target.colorView.Get(), target.depthView.Get(), target.viewport);
                Desperabis::GameContext weaponCore;
                weaponCore.SetMenuPage(Desperabis::MenuPage::Gameplay);
                weaponCore.weaponHand = 2;
                weaponCore.weaponSpriteFrame = 1;
                weaponCore.playerWalkPhase = 128;
                weaponCore.inventory[1] = items[6];
                for (const auto type : { Desperabis::WeaponType::None, Desperabis::WeaponType::Dagger,
                    Desperabis::WeaponType::Sword, Desperabis::WeaponType::Crossbow, Desperabis::WeaponType::Fireball,
                    Desperabis::WeaponType::Stone, Desperabis::WeaponType::TrapOrPole, Desperabis::WeaponType::Electricity,
                    Desperabis::WeaponType::Showel, Desperabis::WeaponType::Lentils, Desperabis::WeaponType::Scythe })
                {
                    weaponCore.selectedWeapon = type;
                    switch (type)
                    {
                    case Desperabis::WeaponType::Sword: weaponCore.weaponAnimationPosition = 20; break;
                    case Desperabis::WeaponType::Crossbow: weaponCore.weaponAnimationPosition = 30; break;
                    case Desperabis::WeaponType::Fireball:
                    case Desperabis::WeaponType::Lentils: weaponCore.weaponAnimationPosition = 60; break;
                    case Desperabis::WeaponType::Electricity: weaponCore.weaponAnimationPosition = 15; break;
                    default: weaponCore.weaponAnimationPosition = 10; break;
                    }
                    if (type == Desperabis::WeaponType::TrapOrPole) { weaponCore.inventory[1] = placeable; }
                    else { weaponCore.inventory[1] = items[6]; }
                    const auto weapon = weaponCore.GetWeaponView();
                    Check(weapon.has_value(), "Every weapon must produce a render recipe.");
                    const float black[4]{ 0,0,0,1 };
                    context->ClearRenderTargetView(target.colorView.Get(), black);
                    context->ClearDepthStencilView(target.depthView.Get(), D3D11_CLEAR_DEPTH, 0, 0);
                    ui.DrawWeapon(*weapon, renderer, *weaponLevel, settings, stats, target.viewport,
                        context.Get(), target.colorView.Get(), target.depthView.Get());
                    const auto pixels = target.Pixels(device.Get(), context.Get());
                    ComPtr<ID3D11Buffer> swordBuffer;
                    if (type == Desperabis::WeaponType::Sword)
                    {
                        CheckSwordFacing(device.Get(), context.Get(), *weapon, *swordModel, true);
                        context->VSGetConstantBuffers(2, 1, &swordBuffer);
                        if (argc >= 3 && levelNumber == 10 && size.first == 1280)
                        {
                            target.SaveBitmap(argv[2], pixels);
                        }
                    }
                    int count = 0;
                    for (const auto pixel : pixels) { if ((pixel & 0xffffff) != 0) { ++count; } }
                    std::cout << "Weapon " << static_cast<int>(type) << " at " << size.first << "x" << size.second
                        << ": " << count << " pixels\n";
                    Check(count > 5, "Every original weapon must render actual pixels despite occluding world depth.");
                    context->ClearRenderTargetView(target.colorView.Get(), black);
                    ui.DrawWeapon(*weapon, renderer, *weaponLevel, settings, stats, target.viewport,
                        context.Get(), target.colorView.Get(), target.depthView.Get());
                    const auto repeated = target.Pixels(device.Get(), context.Get());
                    Check(pixels == repeated, "Cached weapon meshes must render identically without rebuilding assets.");
                    if (type == Desperabis::WeaponType::Sword)
                    {
                        ComPtr<ID3D11Buffer> repeatedBuffer;
                        context->VSGetConstantBuffers(2, 1, &repeatedBuffer);
                        Check(swordBuffer.Get() == repeatedBuffer.Get(), "The flat sword must reuse its original GPU geometry.");
                        for (const int hand : {1,2})
                        {
                            weaponCore.weaponHand = static_cast<std::uint8_t>(hand);
                            context->ClearRenderTargetView(target.colorView.Get(), black);
                            ui.DrawWeapon(*weaponCore.GetWeaponView(), renderer, *weaponLevel, settings, stats, target.viewport,
                                context.Get(), target.colorView.Get(), target.depthView.Get());
                            const auto handed = target.Pixels(device.Get(), context.Get());
                            int visible = 0, correctSide = 0;
                            for (std::size_t i = 0; i < handed.size(); ++i)
                            {
                                if ((handed[i] & 0xffffff) == 0) { continue; }
                                ++visible;
                                if ((i % size.first < size.first / 2) == (hand == 1)) { ++correctSide; }
                            }
                            Check(visible > 100 && correctSide > visible * 0.8f,
                                "The camera-facing sword must remain visible on the selected hand's side.");
                            CheckSwordFacing(device.Get(), context.Get(), *weaponCore.GetWeaponView(), *swordModel, true);
                            context->VSGetConstantBuffers(2, 1, &repeatedBuffer);
                            Check(swordBuffer.Get() == repeatedBuffer.Get(), "Changing sword hands must not rebuild the mesh.");
                            if (levelNumber == 10 && size.first == 360)
                            {
                                for (const int pose : {-10,0,10,20,25,30,35,40})
                                {
                                    weaponCore.weaponAnimationPosition = static_cast<std::int16_t>(pose);
                                    const auto animated = *weaponCore.GetWeaponView();
                                    ui.DrawWeapon(animated, renderer, *weaponLevel, settings, stats, target.viewport,
                                        context.Get(), target.colorView.Get(), target.depthView.Get());
                                    CheckSwordFacing(device.Get(), context.Get(), animated, *swordModel);
                                }
                                weaponCore.weaponAnimationPosition = 20;
                                auto endOn = *weaponCore.GetWeaponView();
                                endOn.object.posX = endOn.object.posY = 0;
                                endOn.object.posY = -6;
                                endOn.object.winX = endOn.object.winY = endOn.object.winZ = 0;
                                ui.DrawWeapon(endOn, renderer, *weaponLevel, settings, stats, target.viewport,
                                    context.Get(), target.colorView.Get(), target.depthView.Get());
                                CheckSwordFacing(device.Get(), context.Get(), endOn, *swordModel);
                            }
                        }
                        weaponCore.weaponHand = 2;
                    }
                    weaponCore.playerWalkPhase = 0;
                    context->ClearRenderTargetView(target.colorView.Get(), black);
                    ui.DrawWeapon(*weaponCore.GetWeaponView(), renderer, *weaponLevel, settings, stats, target.viewport,
                        context.Get(), target.colorView.Get(), target.depthView.Get());
                    const auto walking = target.Pixels(device.Get(), context.Get());
                    if (type == Desperabis::WeaponType::Sword)
                    {
                        CheckSwordFacing(device.Get(), context.Get(), *weaponCore.GetWeaponView(), *swordModel, true);
                    }
                    Check((walking == pixels) == (type == Desperabis::WeaponType::Dagger),
                        "Walking must visibly move every native bobbing weapon, but not the dagger.");
                    weaponCore.playerWalkPhase = 128;
                    if (type != Desperabis::WeaponType::Stone)
                    {
                        weaponCore.weaponAnimationPosition += 5;
                        if (type == Desperabis::WeaponType::Fireball || type == Desperabis::WeaponType::Lentils)
                        {
                            weaponCore.weaponSpriteFrame = 3;
                        }
                        context->ClearRenderTargetView(target.colorView.Get(), black);
                        ui.DrawWeapon(*weaponCore.GetWeaponView(), renderer, *weaponLevel, settings, stats, target.viewport,
                            context.Get(), target.colorView.Get(), target.depthView.Get());
                        Check(pixels != target.Pixels(device.Get(), context.Get()),
                            "Attack/draw poses and casting frames must visibly differ from rest.");
                        weaponCore.weaponSpriteFrame = 1;
                    }
                    if (type == Desperabis::WeaponType::Electricity)
                    {
                        weaponCore.weaponAnimationPosition = 15;
                        weaponCore.weaponAmounts[7] = 100;
                        std::vector<std::uint32_t> chargedPixels;
                        for (const bool firing : { false, true })
                        {
                            weaponCore.weaponElectricityFiring = firing;
                            context->ClearRenderTargetView(target.colorView.Get(), black);
                            ui.DrawWeapon(*weaponCore.GetWeaponView(), renderer, *weaponLevel, settings, stats, target.viewport,
                                context.Get(), target.colorView.Get(), target.depthView.Get());
                            const auto energyPixels = target.Pixels(device.Get(), context.Get());
                            Check(energyPixels != pixels, "Charged electricity must differ from its depleted view.");
                            if (!firing) { chargedPixels = energyPixels; }
                            else { Check(energyPixels != chargedPixels, "Electricity firing must render its animated alternate model."); }
                        }
                        weaponCore.weaponElectricityFiring = false;
                        weaponCore.weaponAmounts[7] = 0;
                    }
                }
                Check(swordModel->m_drawCalls[0].m_material.m_texture == originalMaterial.m_texture &&
                    swordModel->m_drawCalls[0].m_material.m_materialHash == originalMaterial.m_materialHash,
                    "Weapon texture-slot rebinding must not mutate the shared model material.");
            }
            {
                Target target(device.Get(), 360, 240);
                renderer.ReInitialize(target.colorView.Get(), target.depthView.Get(), target.viewport);
                Desperabis::GameContext weaponCore;
                weaponCore.SetMenuPage(Desperabis::MenuPage::Gameplay);
                weaponCore.selectedWeapon = Desperabis::WeaponType::Fireball;
                weaponCore.weaponHand = 2;
                weaponCore.weaponSpriteFrame = 1;
                weaponCore.weaponAnimationPosition = 60;
                weaponCore.playerWalkPhase = 128;
                int weaponCalls = 0;
                renderer.SetWeaponDraw([&](const RenderingSettings& drawSettings, ProfilingStats& drawStats)
                {
                    ++weaponCalls;
                    ui.DrawWeapon(*weaponCore.GetWeaponView(), renderer, *level, drawSettings, drawStats, target.viewport,
                        context.Get(), target.colorView.Get(), target.depthView.Get());
                });
                TextMeshInstance hudOverlay(TextMesh::CreateTexturedQuad(*AssetManager::Instance().GetPlainWhiteTexture(), 360,240));
                hudOverlay.SetScale(1);
                VisibilityResult visible;
                visible.m_mainCamera = &camera;
                visible.m_visibleTextMeshInstances.push_back(&hudOverlay);
                renderer.RenderSceneElements(visible, settings, stats);
                const auto pixels = target.Pixels(device.Get(), context.Get());
                Check(weaponCalls == 1 && std::all_of(pixels.begin(), pixels.end(), [](std::uint32_t pixel)
                    { return (pixel & 0xffffff) == 0xffffff; }),
                    "HUD must render after the one-shot weapon callback.");
                renderer.RenderSceneElements(visible, settings, stats);
                Check(weaponCalls == 1, "Weapon callbacks must be consumed, not retained across renders or menu pages.");
            }
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
            "wide/portrait scaling, all named first-person weapon poses, cached art, HUD ordering and no D3D11 errors.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
