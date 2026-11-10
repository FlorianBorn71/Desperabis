#include "pch.h"
#include "ApplicationUi.h"
#include "LightningPresentation.h"

#include <GameContext.h>
#include <GameMath.h>
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
#include <Renderer/LightSource.h>
#include <Scene/Scene.h>
#include <Sample/LevelScene.h>
#include <Sample/FreeCamera.h>
#include <d3d11sdklayers.h>
#include <cmath>
#include <cstring>
#include <iostream>
#include <fstream>
#include <limits>
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

    template<typename Exception, typename Action>
    void CheckRecoverableError(Action action)
    {
        bool rejected = false;
        try { action(); }
        catch (const Exception&) { rejected = true; }
#ifndef NDEBUG
        Check(rejected, "Recoverable errors must throw their original exception type in Debug.");
#else
        Check(!rejected, "Recoverable errors must log and take their safe recovery path in Release.");
#endif
    }

    void CheckPresentationRecovery()
    {
        class RecoveryScene final : public Scene
        {
            FUTURE(void) CreateScene() override { CO_RETURN; }
        } scene;
        const auto invalidAnimation = [&] { scene.AddFireAnimation(nullptr, 13, 1.f); };
        CheckRecoverableError<std::invalid_argument>(invalidAnimation);
        CheckRecoverableError<std::out_of_range>([&] { scene.SetFireAnimationFrame(13, 0); });
        RenderableInstance material;
        material.m_materialOverride.resize(1);
        material.m_materialOverride[0].m_textureSlot = 17;
        CheckRecoverableError<std::out_of_range>([&] { scene.AddFireAnimationInstance(material, 0); });
        CheckRecoverableError<std::out_of_range>([&] { scene.AddFireAnimationInstance(material, 1); });
        scene.HandleFireAnimations(0.1);
        Check(scene.GetRenderableInstances().empty(),
            "Rejected optional animations must not register renderables or corrupt subsequent animation updates.");

        Desperabis::GameContext core;
        core.worldObjects[2].health = 1000;
        core.worldObjects[2].renderPlacementMode = 20;
        core.worldObjects[2].generalPurpose3 = 1;
        core.worldObjects[2].doorLockOrGroupBitmask = 166;
        std::array<int, 166> updates{};
        int failures = 0;
        for (int tick = 0; tick < 2; ++tick)
        {
            ApplicationUi::SynchronizeObjectPresentations([&](int slot)
            {
                core.GetObjectRenderState(slot);
                ++updates[slot];
            }, [&](int slot, const char* message)
            {
                Check(slot == 2 && message && *message,
                    "Presentation failures must identify the offending slot and provide a diagnostic.");
                ++failures;
            });
        }
        Check(failures == 2 && updates[2] == 0 && updates[3] == 2 && updates[164] == 2,
            "Failed object presentation must be retried without starving subsequent slots or later frames.");
        core.worldObjects[2].renderPlacementMode = 0;
        ApplicationUi::SynchronizeObjectPresentations([&](int slot)
        {
            core.GetObjectRenderState(slot);
            ++updates[slot];
        }, [&](int, const char*) { ++failures; });
        Check(updates[2] == 1 && updates[164] == 3 && failures == 2,
            "A repaired object's presentation must recover on its very next synchronization.");
        std::cout << "Recovery policy: optional animation skips, per-slot presentation failure isolation, retries and recovery passed.\n";
    }

    std::vector<RenderableInstancePtr> PopulatedGeometry(const std::vector<RenderableInstancePtr>& instances)
    {
        std::vector<RenderableInstancePtr> result;
        for (const auto& instance : instances)
        {
            if (instance->GetRenderable()->IsValid()) { result.push_back(instance); }
        }
        return result;
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

        Target(ID3D11Device3* device, UINT width, UINT height, UINT layers = 1)
            : width(width), height(height), viewport{ 0,0,static_cast<float>(width),static_cast<float>(height),0,1 }
        {
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = width;
            desc.Height = height;
            desc.MipLevels = 1;
            desc.ArraySize = layers;
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
            depthDesc.ViewDimension = layers == 1 ? D3D11_DSV_DIMENSION_TEXTURE2D : D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
            if (layers > 1) { depthDesc.Texture2DArray.ArraySize = layers; }
            CheckHr(device->CreateDepthStencilView(depth.Get(), &depthDesc, &depthView));
        }

        std::vector<std::uint32_t> Pixels(ID3D11Device3* device, ID3D11DeviceContext3* context, UINT layer = 0)
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
            CheckHr(context->Map(staging.Get(), layer, D3D11_MAP_READ, 0, &mapped));
            std::vector<std::uint32_t> result(static_cast<std::size_t>(width) * height);
            for (UINT y = 0; y < height; ++y)
            {
                std::memcpy(result.data() + static_cast<std::size_t>(y) * width,
                    static_cast<const byte*>(mapped.pData) + static_cast<std::size_t>(y) * mapped.RowPitch, width * 4);
            }
            context->Unmap(staging.Get(), layer);
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
        FUTURE(void) CreateScene() override { m_sceneReady = true; CO_RETURN; }
        std::size_t BindingCount() const { return m_fireAnimationInstances.size(); }
        bool HasFlameSlot(int slot) const { return m_fireAnimations[slot].m_torchAnim != nullptr; }
    };

    void ConfigureUnderwaterView(RenderingSettings& settings, bool enabled)
    {
        settings.renderSky = !enabled;
        if (enabled)
        {
            settings.fogOverride = FogParameters{
                {0.045f,0.065f,0.055f}, 1.f / (400.f * DespToFloat), 160.f * DespToFloat, true};
            settings.clearColorOverride = Vector4{0.015f,0.015f,0.015f,0};
        }
        else
        {
            settings.fogOverride.reset();
            settings.clearColorOverride.reset();
        }
    }

    void CheckUnderwater(RendererDX11& renderer, ID3D11Device3* device, ID3D11DeviceContext3* context)
    {
        const auto& white = *AssetManager::Instance().GetPlainWhiteTexture();
        RenderingSettings settings;
        ProfilingStats stats;
        Camera camera;
        ModelLoadingContext positions;
        positions.m_vertexPos = {{-1,-1,0,1}, {1,-1,0,1}, {1,1,0,1}, {-1,1,0,1}};
        ModelConversionContext mesh(positions);
        mesh.m_numPositions = 4;
        for (uint16_t i = 0; i < 4; ++i)
        {
            mesh.AddToBounds(positions.m_vertexPos[i], 0.f);
            mesh.m_vertices.emplace_back().SetTextured(i, Color4{200,100,50,0}, {0,0});
        }
        mesh.m_sortedIndices = {0,1,2,0,2,3,0,2,1,0,3,2};
        mesh.GeneratePerTriangleNormals(mesh.m_sortedIndices.data(), 12, 0);
        // Isolate fog/light color from which side of this two-sided test quad is visible.
        for (auto& vertex : mesh.m_vertices) { vertex.Normal = {0,0,1}; }
        Renderable model;
        model.m_runtime = renderer.CreateModelBuffers(mesh);
        model.m_numPositions = 4;
        model.m_numPolygons = 2;
        mesh.GenerateBoundingVolume(model.m_localBounds);
        auto& draw = model.m_drawCalls.emplace_back();
        draw.m_primitiveCount = 4;
        draw.m_material.m_texture = AssetManager::Instance().GetPlainWhiteTexture();
        draw.m_material.m_transparencyType = TransparencyMode::Opaque;
        draw.m_material.m_applyLighting = false;
        draw.m_material.ComputeHash();
        RenderableInstance instance(&model);
        instance.ComputeTransparencyMask();
        instance.UpdateGlobalBounds();
        for (const auto size : {std::pair<UINT, UINT>{360,240}, {240,360}, {720,480}})
        {
            Target target(device, size.first, size.second);
            renderer.ReInitialize(target.colorView.Get(), target.depthView.Get(), target.viewport);
            Matrix4 projection;
            projection.CreateOrthographic(2.f, 2.f, 0.001f, 20.f);
            camera.SetProjectionMatrix(projection);
            VisibilityResult visible;
            visible.m_mainCamera = &camera;
            visible.m_visibleInstances.m_opaqueInstances.push_back({&instance, 0});
            visible.m_visibleInstances.m_visibleTransparencyTypes = instance.GetUsedTransparencyMask();
            visible.m_ambientColor = {1,1,1};
            visible.m_ambientNrmlContribution = 0;
            visible.m_fogColor = {0.6f,0.2f,0.1f};
            visible.m_fogFalloff = 0.1f;
            visible.m_lightmap = AssetManager::Instance().GetPlainWhiteTexture();
            float distance = 0.01f * DespRelativeScaling;
            const auto render = [&]
            {
                Matrix4 transform;
                transform.SetTranslation(Vector(0,0,distance));
                camera.SetCameraTransformMatrix(transform);
                context->ClearDepthStencilView(target.depthView.Get(), D3D11_CLEAR_DEPTH, 1, 0);
                renderer.RenderSceneElements(visible, settings, stats);
                return target.Pixels(device, context);
            };
            const auto pixelAt = [&](const auto& pixels, UINT x, UINT y)
                { return pixels[static_cast<std::size_t>(y) * size.first + x] & 0xffffff; };
            ConfigureUnderwaterView(settings, false);
            const auto normal = render();
            Check(pixelAt(normal, 10, 10) == 0x3264c8, "Dry fullbright geometry must preserve its original RGB.");
            ConfigureUnderwaterView(settings, true);
            const auto submerged = render();
            const auto rgb = pixelAt(submerged, 10, 10);
            const auto checkFog = [&](std::uint32_t pixel, float depth)
            {
                const float weight = std::exp(-(depth + 160.f * DespToFloat) / (400.f * DespToFloat));
                const float source[] = {200.f / 255, 100.f / 255, 50.f / 255};
                const float fog[] = {0.045f, 0.065f, 0.055f};
                for (int channel = 0; channel < 3; ++channel)
                {
                    const int expected = static_cast<int>(std::lround(255 * (fog[channel] +
                        (source[channel] - fog[channel]) * weight)));
                    Check(std::abs(static_cast<int>((pixel >> (8 * channel)) & 255) - expected) <= 1,
                        "Underwater pixels must match muted gray-green fog with a farther-behind-camera start.");
                }
            };
            checkFog(rgb, distance);
            Check(render() == submerged, "Underwater fog must remain active until the exit callback.");
            ConfigureUnderwaterView(settings, false);
            renderer.SetItemPreview(std::make_shared<RenderableInstance>(instance), false, target.viewport);
            const auto dryPreview = render();
            Check(stats.m_numDrawCalls > 1, "The fog-isolation fixture must draw actual model-preview geometry.");
            ConfigureUnderwaterView(settings, true);
            renderer.SetItemPreview(std::make_shared<RenderableInstance>(instance), false, target.viewport);
            const auto submergedPreview = render();
            Check(pixelAt(dryPreview, size.first / 2, size.second / 2) ==
                pixelAt(submergedPreview, size.first / 2, size.second / 2),
                "Pickup model previews must not inherit the world's fog override.");
            renderer.SetWeaponDraw([&](const VisibilityResult&, const RenderingSettings& drawSettings, ProfilingStats& drawStats)
            {
                RenderContextDX11 weapon(renderer, visible, drawSettings, drawStats);
                weapon.BeginRendering();
                weapon.BeginRenderTargetScope(RenderTargetPassType::MainView);
                weapon.TrackGlobals();
                context->ClearDepthStencilView(target.depthView.Get(), D3D11_CLEAR_DEPTH, 1, 0);
                weapon.SetGeometryPassType(GeometryPassType::Instances);
                weapon.SetBlendState(TransparencyMode::Opaque);
                weapon.RenderInstance<GeometryPassType::Instances>(&instance, TransparencyMode::Opaque);
                weapon.EndRenderTargetScope();
                weapon.EndRendering();
            });
            Check(render() == submerged, "Camera-space weapon geometry must use the same near underwater fog.");
            for (const float depth : {0.8f, 8.f})
            {
                distance = depth * DespRelativeScaling;
                const auto distant = render();
                checkFog(pixelAt(distant, 10, 10), distance);
                Check(pixelAt(distant, 10, 10) != rgb, "World depth must change attenuation, unlike a fullscreen tint.");
            }
            settings.useFog = false;
            checkFog(pixelAt(render(), 10, 10), distance);
            settings.useFog = true;
            distance = 0.01f * DespRelativeScaling;
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
                "Blue/white oxygen must sit above health within its old bounds, with HUD colors unaffected by fog.");
            visible.m_visibleTextMeshInstances.clear();
            auto dry = ApplicationUi::CreateVitalMeters(white,
                {0.f, static_cast<float>(size.second) - height, static_cast<float>(size.first), height}, 0.5f, std::nullopt);
            Check(dry.size() == 2 && dry[0]->GetMesh()->GetExtent().y == height,
                "Outside water the existing health meter must retain its full height.");
            auto empty = ApplicationUi::CreateVitalMeters(white, {0,0,100,20}, 0.f, 0.f);
            Check(empty.size() == 2, "Depleted meters must show backgrounds without zero-width geometry.");
            ConfigureUnderwaterView(settings, false);
            Check(render() == normal, "Exit must restore dry rendering completely.");
            distance = 1.f * DespRelativeScaling;
            settings.fogOverride = FogParameters{{0.4f,0.03f,0.08f}, 0.7f / DespRelativeScaling, 0.f, true};
            const auto explicitUnlitFog = render();
            const auto checkCustomFog = [&](std::uint32_t pixel, float offset)
            {
                const float weight = std::exp(0.7f / DespRelativeScaling * (-distance - offset));
                const float color[] = {0.4f,0.03f,0.08f};
                const float source[] = {200.f / 255,100.f / 255,50.f / 255};
                for (int channel = 0; channel < 3; ++channel)
                {
                    const int expected = static_cast<int>(std::lround(255 * (color[channel] +
                        (source[channel] - color[channel]) * weight)));
                    Check(std::abs(static_cast<int>((pixel >> (8 * channel)) & 255) - expected) <= 1,
                        "A caller-supplied non-underwater fog override must control the exact pixel color.");
                }
            };
            checkCustomFog(pixelAt(explicitUnlitFog, 10, 10), 0.f);
            settings.fogOverride->distanceOffset = 0.4f * DespRelativeScaling;
            settings.fogOverride->affectUnlit = false;
            Check(render() == normal, "Positive distance offset must not implicitly fog unlit materials.");
            settings.fogOverride->affectUnlit = true;
            checkCustomFog(pixelAt(render(), 10, 10), settings.fogOverride->distanceOffset);
            settings.fogOverride.reset();
            distance = 0.01f * DespRelativeScaling;
            auto& override = instance.GetOrCreateMaterialOverrides().at(0);
            override.m_applyLighting = true;
            override.ComputeHash();
            const auto dryLit = render();
            ConfigureUnderwaterView(settings, true);
            checkFog(pixelAt(render(), 10, 10), distance);
            ConfigureUnderwaterView(settings, false);
            Check(render() == dryLit, "Surfacing must restore the level's original fog color and falloff.");
            PlayerLight playerLight(nullptr);
            PackedLightSourceParams normalLight;
            Check(playerLight.FillLightParams(normalLight) &&
                std::abs(normalLight.PositionAndRadius.w - 600.f * DespToFloat) < 0.00001f &&
                std::abs(normalLight.ColorAndPadding.x - 1.5f) < 0.00001f &&
                std::abs(normalLight.ColorAndPadding.y - 1.5f) < 0.00001f &&
                std::abs(normalLight.ColorAndPadding.z - 1.5f) < 0.00001f && playerLight.GetCoronaSize() == 0.f &&
                playerLight.GetSpecularAmount() == 0.f && normalLight.ColorAndPadding.w == 0.f,
                "The default above-water player light must retain its white color, radius, intensity and no corona.");
            const auto& normalVector = mesh.m_vertices.front().Normal;
            const float lightDistance = 0.25f * DespRelativeScaling;
            playerLight.SetPosition(Vector(-normalVector.x * lightDistance,
                -normalVector.y * lightDistance, -normalVector.z * lightDistance));
            visible.m_ambientColor = {0,0,0};
            ConfigureUnderwaterView(settings, true);
            const auto renderLight = [&](PlayerLight::Mode mode)
            {
                playerLight.SetMode(mode);
                visible.m_visibileLights.clear();
                PackedLightSourceParams packed;
                const bool enabled = playerLight.FillLightParams(packed);
                Check(enabled == (mode != PlayerLight::Mode::Off),
                    "Off player light must be omitted from GPU lighting, not just recolored.");
                Check(packed.ColorAndPadding.w == (mode == PlayerLight::Mode::Blue ? 1.f : 0.f),
                    "Only blue player-light mode must enable specularity.");
                if (enabled)
                {
                    Check(std::abs(packed.PositionAndRadius.w -
                        (mode == PlayerLight::Mode::Blue ? 220.f : 600.f) * DespToFloat) < 0.00001f,
                        "Blue player light must retain its tuned radius; white must retain its original radius.");
                    visible.m_visibileLights.push_back(packed);
                }
                return render();
            };
            const auto unlit = renderLight(PlayerLight::Mode::Off);
            const auto blueLight = renderLight(PlayerLight::Mode::Blue);
            Check(visible.m_visibileLights.front().ColorAndPadding.z >
                visible.m_visibileLights.front().ColorAndPadding.x * 2.f,
                "Blue player light must upload a cool color rather than change the whole fog.");
            const auto whiteLight = renderLight(PlayerLight::Mode::Normal);
            const auto lightPixel = [&](const auto& pixels, int channel)
            {
                return static_cast<int>((pixelAt(pixels, size.first / 2, size.second / 2) >> (8 * channel)) & 255);
            };
            const int blueRed = lightPixel(blueLight, 0) - lightPixel(unlit, 0);
            const int blueBlue = lightPixel(blueLight, 2) - lightPixel(unlit, 2);
            const int whiteRed = lightPixel(whiteLight, 0) - lightPixel(unlit, 0);
            const int whiteBlue = lightPixel(whiteLight, 2) - lightPixel(unlit, 2);
            Check(blueRed > 0 && blueBlue > 0 && whiteRed > blueRed &&
                blueBlue * whiteRed > whiteBlue * blueRed,
                ("Underwater light differences (blue R/B, white R/B): " + std::to_string(blueRed) + "/" +
                    std::to_string(blueBlue) + ", " + std::to_string(whiteRed) + "/" +
                    std::to_string(whiteBlue)).c_str());
            Check(renderLight(PlayerLight::Mode::Off) == unlit &&
                renderLight(PlayerLight::Mode::Normal) == whiteLight,
                "Repeated player-light mode changes must restore identical off/normal pixels.");
            playerLight.SetMode(PlayerLight::Mode::Blue);
            const auto checkPulse = [&](float multiplier)
            {
                PackedLightSourceParams packed;
                Check(playerLight.FillLightParams(packed), "Blue light must remain enabled throughout its pulse.");
                const auto expected = Color4{32,80,255,255}.ToFloat(multiplier);
                Check(std::abs(packed.ColorAndPadding.x - expected.x) < 0.00001f &&
                    std::abs(packed.ColorAndPadding.y - expected.y) < 0.00001f &&
                    std::abs(packed.ColorAndPadding.z - expected.z) < 0.00001f &&
                    std::abs(packed.PositionAndRadius.w - 220.f * DespToFloat) < 0.00001f,
                    "Blue light must pulse only its intensity, between 4.2 and 7.8, over six seconds.");
            };
            checkPulse(6.f);
            playerLight.Update(1.5);
            checkPulse(7.8f);
            playerLight.Update(3.0);
            checkPulse(4.2f);
            playerLight.Update(1.5);
            checkPulse(6.f);
            playerLight.SetMode(PlayerLight::Mode::Blue);
            for (int step = 0; step < 15; ++step) { playerLight.Update(0.1); }
            checkPulse(7.8f);
            playerLight.SetMode(PlayerLight::Mode::Normal);
            playerLight.Update(1.5);
            PackedLightSourceParams steadyWhite;
            Check(playerLight.FillLightParams(steadyWhite) &&
                std::abs(steadyWhite.ColorAndPadding.x - normalLight.ColorAndPadding.x) < 0.00001f &&
                steadyWhite.PositionAndRadius.w == normalLight.PositionAndRadius.w,
                "Returning to normal must restore steady white intensity and radius without oscillation.");
            playerLight.SetMode(PlayerLight::Mode::Off);
            playerLight.Update(1.5);
            Check(!playerLight.FillLightParams(steadyWhite), "Updating an off player light must not re-enable it.");
            visible.m_visibileLights.clear();
            visible.m_ambientColor = {1,1,1};
            ConfigureUnderwaterView(settings, false);
            override.m_applyLighting = false;
            override.ComputeHash();
            visible.m_visibleInstances.m_opaqueInstances.clear();
            visible.m_visibleInstances.m_skyInstances.push_back({&instance, 0});
            visible.m_visibleInstances.m_visibleTransparencyTypes = TransparencyModeMask::SkyOpaqueBit;
            override.m_transparencyType = TransparencyMode::SkyOpaque;
            override.ComputeHash();
            instance.SetTransform(Vector(0,0,-1), Vector(0,0,0));
            const auto drySky = render();
            Check(pixelAt(drySky, 10, 10) == 0x3264c8, "Sky fixture must be visible outside water.");
            ConfigureUnderwaterView(settings, true);
            Check(pixelAt(render(), 10, 10) == 0x040404,
                "Underwater must hide the dry sky against a near-black neutral background, independently of fog.");
            ConfigureUnderwaterView(settings, false);
            override.m_transparencyType = TransparencyMode::Opaque;
            override.ComputeHash();
            instance.SetTransform(Vector(0,0,0), Vector(0,0,0));
        }
        Target offsetTarget(device, 360, 240);
        auto viewport = offsetTarget.viewport;
        viewport.TopLeftX = 20;
        viewport.TopLeftY = 30;
        viewport.Width = 300;
        viewport.Height = 180;
        renderer.ReInitialize(offsetTarget.colorView.Get(), offsetTarget.depthView.Get(), viewport);
        const float red[] = {1,0,0,1};
        context->ClearRenderTargetView(offsetTarget.colorView.Get(), red);
        ConfigureUnderwaterView(settings, true);
        VisibilityResult emptyView;
        emptyView.m_mainCamera = &camera;
        renderer.RenderSceneElements(emptyView, settings, stats);
        const auto offsetPixels = offsetTarget.Pixels(device, context);
        Check((offsetPixels[10 * 360 + 10] & 0xffffff) == 0x0000ff &&
            (offsetPixels[40 * 360 + 30] & 0xffffff) == 0x040404 &&
            (offsetPixels[220 * 360 + 330] & 0xffffff) == 0x0000ff,
            "Underwater background must respect a nonzero viewport origin and preserve pixels outside it.");
        ConfigureUnderwaterView(settings, false);
        std::cout << "Underwater fog: muted gray-green near/far RGB, off/blue/normal player-light pixels, six-second blue pulse, weapon, sky/background, dry restoration and untouched HUD passed.\n";
    }

    void CheckUnderwaterLightPool(RendererDX11& renderer, ID3D11Device3* device, ID3D11DeviceContext3* context)
    {
        const float extent = 400.f * DespToFloat;
        ModelLoadingContext positions;
        positions.m_vertexPos = {{-extent,0,-extent,1}, {extent,0,-extent,1},
            {extent,0,extent,1}, {-extent,0,extent,1}};
        ModelConversionContext mesh(positions);
        mesh.m_numPositions = 4;
        for (uint16_t i = 0; i < 4; ++i)
        {
            mesh.AddToBounds(positions.m_vertexPos[i], 0.f);
            mesh.m_vertices.emplace_back().SetTextured(i, Color4{255,255,255,0}, {0,0});
        }
        // Same winding and generated normals as a flat terrain floor, without synthetic normal overrides.
        mesh.m_sortedIndices = {0,1,2,0,2,3};
        mesh.GeneratePerTriangleNormals(mesh.m_sortedIndices.data(), 6, 0);
        Renderable model;
        model.m_runtime = renderer.CreateModelBuffers(mesh);
        model.m_numPositions = 4;
        model.m_numPolygons = 2;
        mesh.GenerateBoundingVolume(model.m_localBounds);
        auto& draw = model.m_drawCalls.emplace_back();
        draw.m_primitiveCount = 2;
        draw.m_material.m_texture = AssetManager::Instance().GetPlainWhiteTexture();
        draw.m_material.m_transparencyType = TransparencyMode::Opaque;
        draw.m_material.m_applyLighting = true;
        draw.m_material.ComputeHash();
        auto instance = std::make_shared<RenderableInstance>(&model);
        instance->ComputeTransparencyMask();
        instance->UpdateGlobalBounds();
        FlameScene scene;
        FORCE_WAIT(scene.CreateScene());
        scene.AddRenderableInstance(instance);
        auto lightmap = std::make_shared<Texture>();
        lightmap->CreatePlainWhite();
        lightmap->m_atlasData = {0,0,0.5f,0.5f};
        scene.SetLightmap(lightmap);
        auto playerLight = std::make_shared<PlayerLight>(&scene);
        scene.AddLightSource(playerLight);

        const auto previousCamera = renderer.GetOrCreateCamera();
        auto camera = std::make_shared<Camera>();
        Matrix4 projection, transform;
        projection.CreateOrthographic(extent * 2, extent * 2, 0.001f, 20.f);
        transform.Create(Vector(0,45.f * DespToFloat,0,1), Vector(1,0,0,0),
            Vector(0,0,-1,0), Vector(0,1,0,0));
        camera->SetProjectionMatrix(projection);
        camera->SetCameraTransformMatrix(transform);
        renderer.SetCamera(camera);
        Target target(device, 360, 240);
        renderer.ReInitialize(target.colorView.Get(), target.depthView.Get(), target.viewport);
        RenderingSettings settings;
        ConfigureUnderwaterView(settings, true);
        VisibilitySettings visibilitySettings;
        DefaultVisibilityContext visibility;
        ProfilingStats stats;
        std::size_t extraLights = 0;
        const auto render = [&](PlayerLight::Mode mode)
        {
            playerLight->SetMode(mode);
            playerLight->Update(0);
            VisibilityResult visible;
            visibility.ComputeResultFor(camera.get(), &scene, visible, settings, visibilitySettings, stats);
            Check(visible.m_visibileLights.size() == extraLights + (mode == PlayerLight::Mode::Off ? 0u : 1u) &&
                visible.m_visibleInstances.m_opaqueInstances.size() == 1,
                "The camera-following player light and terrain must pass normal visibility collection.");
            context->ClearDepthStencilView(target.depthView.Get(), D3D11_CLEAR_DEPTH, 1, 0);
            renderer.RenderSceneElements(visible, settings, stats);
            return target.Pixels(device, context);
        };
        const auto off = render(PlayerLight::Mode::Off);
        const auto blue = render(PlayerLight::Mode::Blue);
        const auto channel = [](const auto& pixels, int x, int component)
        {
            return static_cast<int>((pixels[120 * 360 + x] >> (8 * component)) & 255);
        };
        Check(channel(blue,225,2) >= channel(off,225,2) + 60 &&
            channel(blue,248,2) >= channel(off,248,2) + 25 &&
            channel(blue,248,2) >= channel(blue,248,0) + 20 &&
            channel(blue,248,2) >= channel(blue,248,1) + 15,
            "The blue player light must create a visibly blue pool at 100 and 150 native units on real-facing terrain.");
        Check(blue[120 * 360 + 338] == off[120 * 360 + 338],
            "Terrain outside the 220-unit light radius must remain unchanged, not receive a global blue tint.");
        ConfigureUnderwaterView(settings, false);
        settings.fogOverride = FogParameters{{0,0,0}, 0.f, 0.f, false};
        instance->m_tintColor = {80,80,80,255};
        ::RawGameObject ghost{};
        ghost.m_rawBytes[8] = static_cast<byte>(GamePlay::BehaviorType::Ghost);
        ghost.m_rawShorts[5] = 1040;
        ghost.m_rawShorts[6] = Level::tileCenterX * 100 + 100;
        ghost.m_rawShorts[7] = 32;
        ghost.m_rawShorts[8] = 4100 - Level::tileCenterY * 100;
        auto ghostContext = GamePlay::CreateObjectPresentation(scene, 0, 2, ghost, nullptr);
        const auto dormant = render(PlayerLight::Mode::Normal);
        ghost.m_rawShorts[5] = 1000;
        ghostContext->Update(ghost, nullptr);
        extraLights = 1;
        const auto nearbyGhost = render(PlayerLight::Mode::Normal);
        std::cout << "Ghost pool at 100 units: baseline RGB " << channel(dormant,225,0) << ','
            << channel(dormant,225,1) << ',' << channel(dormant,225,2) << ", ghost RGB "
            << channel(nearbyGhost,225,0) << ',' << channel(nearbyGhost,225,1) << ','
            << channel(nearbyGhost,225,2) << "; 150-unit blue "
            << channel(dormant,248,2) << " -> " << channel(nearbyGhost,248,2) << ".\n";
        Check(channel(nearbyGhost,225,2) >= channel(dormant,225,2) + 10 &&
            channel(nearbyGhost,225,2) <= channel(dormant,225,2) + 25 &&
            channel(nearbyGhost,248,2) >= channel(dormant,248,2) + 2 &&
            channel(nearbyGhost,248,2) <= channel(dormant,248,2) + 6 &&
            channel(nearbyGhost,225,2) >= channel(nearbyGhost,225,0) + 5 &&
            channel(nearbyGhost,225,0) <= channel(dormant,225,0) + 4,
            "Nearby ghosts must cast a subtler blue pool at 100/150 native units with the normal white player light enabled.");
        Check(nearbyGhost[120 * 360 + 338] == dormant[120 * 360 + 338],
            "Ghost illumination must remain local rather than tinting the whole floor.");
        ghost.m_rawShorts[17] = 22;
        ghostContext->Update(ghost, nullptr);
        const auto dyingGhost = render(PlayerLight::Mode::Normal);
        Check(channel(dyingGhost,225,2) >= channel(nearbyGhost,225,2) + 30,
            "Dying ghosts must visibly brighten their blue illumination above the quiet nearby glow.");
        ghostContext->Remove(scene);
        renderer.SetCamera(previousCamera);
        std::cout << "Underwater blue pool: camera-following light, generated terrain normals, bright blue at 100/150 units and unchanged outside radius passed.\n";
        std::cout << "Ghost blue pool: visible 100/150-unit illumination against the white player light, stronger death glow and local falloff passed.\n";
    }

    void CheckDynamicSpecularity(RendererDX11& renderer, ID3D11Device3* device, ID3D11DeviceContext3* context)
    {
        const std::array<std::shared_ptr<LightSource>, 4> sources{
            std::make_shared<LightSource>(nullptr), std::make_shared<FlickeringLight>(nullptr),
            std::make_shared<PulsatingLight>(nullptr), std::make_shared<LightFlashSource>(nullptr, 1.f, 1.f)};
        for (const auto& source : sources)
        {
            source->SetPosition(Vector(0,1,0));
            source->SetRadius(1.f);
            source->SetColor({128,192,255,255});
            source->SetCoronaColorMod({255,255,255,255});
            source->SetCoronaSize(0.2f);
            source->Update(0.5);
            PackedLightSourceParams enabled{}, scaled{}, coronaBefore{}, coronaAfter{};
            Check(source->GetSpecularAmount() == 1.f && source->FillLightParams(enabled) && enabled.ColorAndPadding.w == 1.f,
                "Ordinary, flickering, pulsating and fading lights must default to specular amount one.");
            source->FillCoronaParams(coronaBefore);
            for (const auto amount : {0.f, 0.25f, 2.f})
            {
                source->SetSpecularAmount(amount);
                Check(source->GetSpecularAmount() == amount && source->FillLightParams(scaled) &&
                    scaled.ColorAndPadding.w == amount &&
                    scaled.PositionAndRadius.x == enabled.PositionAndRadius.x &&
                    scaled.PositionAndRadius.y == enabled.PositionAndRadius.y &&
                    scaled.PositionAndRadius.z == enabled.PositionAndRadius.z &&
                    scaled.PositionAndRadius.w == enabled.PositionAndRadius.w &&
                    scaled.ColorAndPadding.x == enabled.ColorAndPadding.x &&
                    scaled.ColorAndPadding.y == enabled.ColorAndPadding.y &&
                    scaled.ColorAndPadding.z == enabled.ColorAndPadding.z,
                    "Every light type must pack zero, fractional and boosted specularity without changing diffuse lighting.");
                source->FillCoronaParams(coronaAfter);
                Check(std::memcmp(&coronaBefore, &coronaAfter, sizeof(coronaBefore)) == 0,
                    "Changing specular amount must not change corona parameters.");
            }
            for (const auto invalid : {-1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
            {
                CheckRecoverableError<std::invalid_argument>([&] { source->SetSpecularAmount(invalid); });
                Check(source->GetSpecularAmount() == 2.f,
                    "Invalid specular amounts must leave the previous value unchanged.");
            }
            source->SetSpecularAmount(1.f);
            Check(source->FillLightParams(enabled) && enabled.ColorAndPadding.w == 1.f,
                "Each light type must support restoring default specularity.");
        }
        RenderingSettings settings;
        Check(settings.specularAmount == 2.f && settings.specularExponent == 16.f,
            "Dynamic specularity must default to amount 2 and exponent 16.");
        settings.fogOverride = FogParameters{{0,0,0}, 0.f, 0.f, false};
        ModelLoadingContext positions;
        positions.m_vertexPos = {{-2,0,-2,1}, {2,0,-2,1}, {2,0,2,1}, {-2,0,2,1}};
        ModelConversionContext mesh(positions);
        mesh.m_numPositions = 4;
        for (uint16_t index = 0; index < 4; ++index)
        {
            mesh.AddToBounds(positions.m_vertexPos[index], 0.f);
            mesh.m_vertices.emplace_back().SetTextured(index, Color4{255,255,255,0}, {0,0});
        }
        mesh.m_sortedIndices = {0,1,2,0,2,3};
        mesh.GeneratePerTriangleNormals(mesh.m_sortedIndices.data(), 6, 0);
        Renderable model;
        model.m_runtime = renderer.CreateModelBuffers(mesh);
        model.m_numPositions = 4;
        model.m_numPolygons = 2;
        mesh.GenerateBoundingVolume(model.m_localBounds);
        auto& draw = model.m_drawCalls.emplace_back();
        draw.m_primitiveCount = 2;
        draw.m_material.m_texture = AssetManager::Instance().GetPlainWhiteTexture();
        draw.m_material.m_applyLighting = true;
        draw.m_material.ComputeHash();
        auto instance = std::make_shared<RenderableInstance>(&model);
        instance->ComputeTransparencyMask();
        instance->UpdateGlobalBounds();
        Texture lightmap;
        lightmap.CreatePlainWhite();
        lightmap.m_atlasData = {0,0,0.5f,0.5f};
        Camera camera;
        Matrix4 projection, transform;
        projection.CreateOrthographic(4.f, 4.f, 0.001f, 20.f);
        transform.Create(Vector(0,1,0,1), Vector(1,0,0,0), Vector(0,0,-1,0), Vector(0,1,0,0));
        camera.SetProjectionMatrix(projection);
        camera.SetCameraTransformMatrix(transform);
        Target target(device, 256, 256);
        renderer.ReInitialize(target.colorView.Get(), target.depthView.Get(), target.viewport);
        VisibilityResult visible;
        visible.m_mainCamera = &camera;
        visible.m_visibleInstances.m_opaqueInstances.push_back({instance.get(),0});
        visible.m_visibleInstances.m_visibleTransparencyTypes = TransparencyModeMask::OpaqueBit;
        visible.m_lightmap = &lightmap;
        visible.m_ambientColor = {0,0,0};
        PackedLightSourceParams light{};
        light.PositionAndRadius = {0,1,0,5};
        light.ColorAndPadding = {0.05f,0.1f,0.15f,1};
        visible.m_visibileLights.push_back(light);
        ProfilingStats stats;
        const auto render = [&](bool enabled)
        {
            auto renderSettings = settings;
            if (!enabled) { renderSettings.specularAmount = 0.f; }
            const float black[4]{0,0,0,1};
            context->ClearRenderTargetView(target.colorView.Get(), black);
            context->ClearDepthStencilView(target.depthView.Get(), D3D11_CLEAR_DEPTH, 1, 0);
            renderer.RenderSceneElements(visible, renderSettings, stats);
            return target.Pixels(device, context);
        };
        const auto channel = [](const auto& pixels, int x, int component)
        {
            return static_cast<int>((pixels[128 * 256 + x] >> (component * 8)) & 255);
        };
        const auto off = render(false);
        const auto defaults = render(true);
        ComPtr<ID3D11Buffer> globalsBuffer;
        context->PSGetConstantBuffers(0, 1, &globalsBuffer);
        auto globals = ReadBuffer<float>(device, context, globalsBuffer.Get());
        Check(globals[globals.size() - 2] == 2.f && globals.back() == 16.f,
            "The default amount 2 and exponent 16 must reach the pixel shader.");
        Check(channel(defaults,128,2) >= channel(off,128,2) + 150,
            "Default amount 2/exponent 16 must produce highlights with the unnormalized squared-albedo mask.");
        settings.specularAmount = 1.f;
        settings.specularExponent = 32.f;
        const auto on = render(true);
        const int peak = channel(on,128,2) - channel(off,128,2);
        std::cout << "Specular center RGB: " << channel(off,128,0) << ',' << channel(off,128,1) << ','
            << channel(off,128,2) << " -> " << channel(on,128,0) << ',' << channel(on,128,1) << ','
            << channel(on,128,2) << ".\n";
        globals = ReadBuffer<float>(device, context, globalsBuffer.Get());
        Check(peak >= 80 && peak <= 100 && channel(on,128,0) - channel(off,128,0) >= 25 &&
            std::abs(peak - 3 * (channel(on,128,0) - channel(off,128,0))) <= 3,
            "Dynamic highlights must add a visible, light-colored half-vector lobe on real-facing terrain.");
        Check(globals[globals.size() - 2] == 1.f && globals.back() == 32.f,
            "The pixel shader must receive the selected global amount and exponent.");
        visible.m_visibileLights[0].ColorAndPadding.w = 0.f;
        Check(render(true) == off, "Disabling a light's specularity must preserve its exact diffuse contribution.");
        for (const auto amount : {0.25f, 2.f})
        {
            visible.m_visibileLights[0].ColorAndPadding.w = amount;
            Check(render(false) == off,
                "Changing per-light specular amount must leave diffuse-only pixels identical.");
            const auto scaled = render(true);
            Check(std::abs(channel(scaled,128,2) - channel(off,128,2) - peak * amount) <= 2.f,
                "A light's fractional and boosted specular amounts must linearly scale its pixel highlight.");
        }
        visible.m_visibileLights[0] = light;
        Check(render(true) == on, "Re-enabling a light's specularity must restore its highlight.");
        settings.specularAmount = 0.f;
        Check(render(true) == off, "Enabled specularity with amount zero must exactly match disabled rendering.");
        settings.specularAmount = 1.f;
        Check(render(false) == off, "Zeroing the rendering multiplier must restore identical diffuse-only pixels.");
        globals = ReadBuffer<float>(device, context, globalsBuffer.Get());
        Check(globals[globals.size() - 2] == 0.f && globals.back() == 32.f &&
            settings.specularAmount == 1.f, "Disabling must upload zero without losing the selected strength.");
        settings.specularAmount = 0.5f;
        const auto half = render(true);
        Check(std::abs(2 * (channel(half,128,2) - channel(off,128,2)) - peak) <= 2,
            "Specular amount must linearly scale the highlight contribution.");
        settings.specularAmount = 1.f;
        settings.specularExponent = 8.f;
        const auto broad = render(true);
        settings.specularExponent = 128.f;
        const auto tight = render(true);
        int broadPixels = 0, tightPixels = 0;
        for (std::size_t index = 0; index < off.size(); ++index)
        {
            const int baseline = (off[index] >> 16) & 255;
            broadPixels += static_cast<int>((broad[index] >> 16) & 255) > baseline + 4;
            tightPixels += static_cast<int>((tight[index] >> 16) & 255) > baseline + 4;
        }
        Check(tightPixels > 0 && broadPixels > tightPixels * 4,
            "Increasing the exponent must visibly narrow the highlight across pixels, not merely scale vertex lighting.");
        settings.specularExponent = 32.f;
        visible.m_visibileLights.push_back(light);
        const auto twoOff = render(false);
        const auto twoOn = render(true);
        Check(std::abs(channel(twoOn,128,2) - channel(twoOff,128,2) - peak * 2) <= 2,
            "Each visible dynamic light must contribute its own specular highlight.");
        visible.m_visibileLights[0].ColorAndPadding.w = 0.f;
        const auto mixed = render(true);
        Check(std::abs(channel(mixed,128,2) - channel(twoOff,128,2) - peak) <= 1,
            "One diffuse-only light must not suppress the other light's specular highlight.");
        visible.m_visibileLights.pop_back();
        visible.m_visibileLights[0] = light;
        PlayerLight playerLight(nullptr);
        playerLight.SetPosition(Vector(0,1,0));
        instance->m_tintColor = {64,64,64,255};
        for (const auto mode : {PlayerLight::Mode::Normal, PlayerLight::Mode::Blue, PlayerLight::Mode::Normal})
        {
            playerLight.SetMode(mode);
            PackedLightSourceParams player{};
            Check(playerLight.FillLightParams(player) &&
                player.ColorAndPadding.w == (mode == PlayerLight::Mode::Blue ? 1.f : 0.f),
                "Player-light mode changes must enable specularity only for blue mode.");
            visible.m_visibileLights[0] = player;
            const auto playerOff = render(false);
            const auto playerOn = render(true);
            Check(channel(playerOff,128,2) > 0 &&
                (mode == PlayerLight::Mode::Blue ?
                    channel(playerOn,128,2) >= channel(playerOff,128,2) + 30 : playerOn == playerOff),
                "Blue player light must add a visible highlight; returning to normal must restore diffuse-only illumination.");
        }
        instance->m_tintColor = {255,255,255,255};
        visible.m_visibileLights[0] = light;
        for (const Color4 color : {Color4{128,128,128,255}, Color4{0,0,0,255},
            Color4{255,0,0,255}, Color4{0,255,0,255}})
        {
            Texture albedo;
            albedo.SetDefaultParams();
            albedo.m_width = albedo.m_height = 2;
            TextureLoadingContext data;
            data.m_rgbaData.assign(4, color);
            renderer.CreateTexture(albedo, data);
            draw.m_material.m_texture = &albedo;
            const auto maskedOff = render(false);
            const auto maskedOn = render(true);
            const float brightness = (float(color.r) * color.r + float(color.g) * color.g + float(color.b) * color.b) /
                (3.f * 255.f * 255.f);
            Check(std::abs(channel(maskedOn,128,2) - channel(maskedOff,128,2) - peak * brightness) <= 2.f,
                "Specular masking must use dot(albedo, albedo): half-gray is quarter-strength and pure colors are equal.");
            draw.m_material.m_texture = AssetManager::Instance().GetPlainWhiteTexture();
        }
        transform.SetTranslation(Vector(0.8f,1,0,1));
        camera.SetCameraTransformMatrix(transform);
        const auto movedOff = render(false);
        const auto movedOn = render(true);
        Check(channel(movedOn,77,2) - channel(movedOff,77,2) < peak / 2,
            "Moving the camera must change the highlight at the same world-space surface point.");
        transform.SetTranslation(Vector(0,1,0,1));
        camera.SetCameraTransformMatrix(transform);
        draw.m_material.m_applyLighting = false;
        draw.m_material.ComputeHash();
        const auto unlit = render(false);
        Check(render(true) == unlit, "Unlit materials must not receive dynamic specularity.");
        draw.m_material.m_applyLighting = true;
        draw.m_material.ComputeHash();
        visible.m_visibileLights.clear();
        visible.m_ambientColor = {0.05f,0.1f,0.15f};
        const auto ambient = render(false);
        Check(render(true) == ambient, "Ambient and baked light must not produce specular highlights.");
        visible.m_ambientColor = {0,0,0};
        visible.m_visibileLights.push_back(light);
        visible.m_visibileLights[0].PositionAndRadius.w = 0.5f;
        const auto outside = render(false);
        Check(render(true) == outside, "Specular highlights must obey the dynamic light's radius.");
        visible.m_visibileLights[0] = light;
        visible.m_visibileLights[0].PositionAndRadius.y = -1.f;
        const auto backlit = render(false);
        Check(render(true) == backlit, "Lights behind the surface must not generate specular highlights.");
        visible.m_visibileLights[0] = light;
        settings.fogOverride->falloff = 1.f;
        const auto fogOff = render(false);
        const auto fogOn = render(true);
        Check(std::abs(channel(fogOn,128,2) - channel(fogOff,128,2) - peak * std::exp(-1.f)) <= 2.f,
            "Specular and diffuse contributions must be fogged together.");
        settings.fogOverride->falloff = 0.f;
        Target stereoTarget(device, 256, 256, 2);
        Matrix4 rightEye = transform;
        rightEye.SetTranslation(Vector(0.8f,1,0,1));
        camera.SetCameraTransformMatrices(transform, rightEye);
        renderer.ReInitialize(stereoTarget.colorView.Get(), nullptr, stereoTarget.viewport);
        const auto renderStereo = [&](bool enabled)
        {
            auto renderSettings = settings;
            if (!enabled) { renderSettings.specularAmount = 0.f; }
            const float black[4]{0,0,0,1};
            context->ClearRenderTargetView(stereoTarget.colorView.Get(), black);
            renderer.RenderSceneElements(visible, renderSettings, stats);
            return std::array<std::vector<std::uint32_t>, 2>{
                stereoTarget.Pixels(device, context, 0), stereoTarget.Pixels(device, context, 1)};
        };
        const auto stereoOff = renderStereo(false);
        const auto stereoOn = renderStereo(true);
        Check(std::abs(channel(stereoOn[0],128,2) - channel(stereoOff[0],128,2) - peak) <= 1 &&
            channel(stereoOff[1],77,2) >= 25 &&
            channel(stereoOn[1],77,2) - channel(stereoOff[1],77,2) < peak / 2,
            "Stereo highlights must use each eye's camera, not a shared left-eye view direction.");
        camera.SetCameraTransformMatrix(transform);
        renderer.ReInitialize(target.colorView.Get(), target.depthView.Get(), target.viewport);
        settings.specularExponent = 0.f;
        bool rejected = false;
        try { render(true); }
        catch (const std::invalid_argument&) { rejected = true; }
        Check(rejected, "Invalid specular exponents must report an explicit error before uploading shader constants.");
        settings.specularExponent = 32.f;
        std::cout << "Dynamic specularity: " << peak << " peak blue increment; " << broadPixels << '/'
            << tightPixels << " broad/tight highlight pixels; defaults, zero equivalence, squared albedo, camera/stereo, multiple lights, unlit, radius and fog passed.\n";
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
        model.m_drawCalls[0].m_primitiveCount = 1;
        auto secondDraw = model.m_drawCalls[0];
        secondDraw.m_firstIndex = 3;
        secondDraw.m_material.m_reflectionType = WaterReflectionType::RefractionBit | WaterReflectionType::Water;
        secondDraw.m_material.ComputeHash();
        model.m_drawCalls.push_back(secondDraw);
        auto& materials = instance->GetOrCreateMaterialOverrides();
        materials[0] = model.m_drawCalls[0].m_material;
        materials[1] = secondDraw.m_material;
        instance->ComputeTransparencyMask();
        VisibilityResult mixed;
        visibility.Collect(mixed, scene, 1.f);
        Check(mixed.m_visibleWater.size() == 2,
            "One model with reflecting and refracting triangles must enter both water passes.");
        for (const auto& group : mixed.m_visibleWater)
        {
            Check(group.m_waterReceiver.size() == 1 &&
                group.m_waterReceiver[0].m_drawCallIndex == (group.m_useReflection ? 0 : 1),
                "Each water pass must retain the exact source draw range, not re-render all water materials.");
        }
        VisibilityResult submerged;
        visibility.Collect(submerged, scene, -1.f);
        Check(submerged.m_visibleWater.size() == 1 &&
            submerged.m_visibleWater[0].m_waterReceiver.size() == 2,
            "Submersion must convert both individual water ranges to refraction without losing either.");
        const auto renderRange = [&](int drawIndex)
        {
            RenderContextDX11 rangeRender(renderer, result, settings, stats);
            rangeRender.BeginRendering();
            rangeRender.BeginRenderTargetScope(RenderTargetPassType::MainView);
            context->ClearRenderTargetView(target.colorView.Get(), gray);
            context->ClearDepthStencilView(target.depthView.Get(), D3D11_CLEAR_DEPTH, 1.f, 0);
            rangeRender.TrackGlobals();
            rangeRender.BeginWaterRendering();
            rangeRender.PrepareRefraction(true);
            rangeRender.SetGeometryPassType(GeometryPassType::MirrorReceiver);
            rangeRender.SetBlendState(TransparencyMode::Water);
            rangeRender.PrepareWaterRendering(water);
            rangeRender.RenderInstance<GeometryPassType::MirrorReceiver>(instance.get(), TransparencyMode::Water, drawIndex);
            rangeRender.EndWaterRendering();
            rangeRender.EndRenderTargetScope();
            rangeRender.EndRendering();
            return target.Pixels(device, context);
        };
        const auto firstPixels = renderRange(0), secondPixels = renderRange(1), allPixels = renderRange(-1);
        int firstOnly = 0, secondOnly = 0;
        for (std::size_t i = 0; i < allPixels.size(); ++i)
        {
            const bool first = (firstPixels[i] & 0xffffff) != 0xcccccc;
            const bool second = (secondPixels[i] & 0xffffff) != 0xcccccc;
            if (first && !second) { ++firstOnly; }
            if (second && !first) { ++secondOnly; }
            Check(((allPixels[i] & 0xffffff) != 0xcccccc) == (first || second),
                "Filtered water passes must cover precisely the union of their source triangles.");
        }
        Check(firstOnly > 100 && secondOnly > 100,
            "Selecting a water draw range must visibly omit the other triangle rather than redraw the entire model.");
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
        const auto menu = font.CreateGlyphMesh("aaa", base, Font::HorizontalAlignment::Left,
            Font::VerticalAlignment::Top, 0.5f);
        Check(menu->GetExtent().x == plain->GetExtent().x && menu->GetExtent().y == plain->GetExtent().y * 0.5f,
            "Original menu text must preserve horizontal advances while halving the 480-line glyph height.");
        for (std::size_t vertex = 0; vertex < plain->GetBatches().at(0).m_vertices.size(); ++vertex)
        {
            Check(menu->GetBatches()[0].m_vertices[vertex].m_position.x == plain->GetBatches()[0].m_vertices[vertex].m_position.x &&
                menu->GetBatches()[0].m_vertices[vertex].m_position.y == plain->GetBatches()[0].m_vertices[vertex].m_position.y * 0.5f,
                "Menu font aspect correction must scale glyph geometry rather than changing texture sampling or inventory text.");
        }
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

    void CheckNormalGenerationPolicies()
    {
        Check(NormalGenerationSettings{}.mode == NormalGenerationMode::SlopedFaces,
            "3D models must default to smoothing only sloped faces.");
        const auto generate = [](const ModelLoadingContext& input, const std::vector<char>& materialSlots,
            NormalGenerationMode mode = NormalGenerationMode::ConnectedMaterial)
        {
            Model model;
            model.m_numPositions = static_cast<uint16_t>(input.m_vertexPos.size());
            model.m_numPolygons = static_cast<uint16_t>(input.m_polygons.size());
            ModelConversionContext conversion(input);
            std::vector<DrawCall> draws;
            GeometryUtils::ConvertPolygons(model, conversion, draws);
            Check(draws.size() == materialSlots.size(), "Normal-policy fixtures must retain one draw range per source face.");
            for (std::size_t i = 0; i < draws.size(); ++i) { draws[i].m_material.m_textureSlot = materialSlots[i]; }
            GeometryUtils::OptimizeDrawCalls(model.m_drawCalls, conversion.m_sortedIndices, conversion.m_indices, draws);
            GeometryUtils::SmoothNormals(conversion, model.m_drawCalls, 0, model.m_numPositions, {mode});
            return std::move(conversion.m_vertices);
        };
        const auto difference = [](const ModelVertex& a, const ModelVertex& b)
        {
            return Vector(a.Normal.x - b.Normal.x, a.Normal.y - b.Normal.y, a.Normal.z - b.Normal.z).GetLength();
        };
        ModelLoadingContext patch;
        patch.m_vertexPos = {{0,0,0}, {1,0,0}, {0,1,0}, {-1,0,0.5f}, {0,1,4}};
        patch.m_polygons = {{3,120,1,2,3,0}, {3,120,1,3,4,0}, {3,120,1,4,5,0}};
        const auto promoted = generate(patch, {0,0,0});
        const auto smooth = generate(patch, {0,0,0}, NormalGenerationMode::Smooth);
        const auto crease = generate(patch, {0,0,0}, NormalGenerationMode::CreaseAngle);
        const auto flat = generate(patch, {0,0,0}, NormalGenerationMode::Flat);
        int sharpCorners = 0;
        for (std::size_t i = 0; i < promoted.size(); ++i)
        {
            Check(difference(promoted[i], smooth[i]) < 0.00001f,
                "A qualifying bend must promote the whole connected material patch to full smoothing.");
            if (promoted[i].PositionIndex == 3 && promoted[i].NormalGroup == 0 &&
                difference(promoted[i], crease[i]) > 0.1f) { ++sharpCorners; }
        }
        Check(sharpCorners == 2, "Patch promotion must smooth both sides of the connected edge sharper than 90 degrees.");

        const auto boundary = generate(patch, {0,0,1});
        const auto isolatedTrigger = generate(patch, {0,1,1});
        for (std::size_t i = 0; i < promoted.size(); ++i)
        {
            Check(difference(boundary[i], crease[i]) < 0.00001f,
                "Distinct source slots in the same texture must stop promotion at the material boundary.");
            Check(difference(isolatedTrigger[i], flat[i]) < 0.00001f,
                "A bent edge between different materials must not trigger smoothing within either material.");
        }
        auto disconnected = patch;
        disconnected.m_vertexPos.insert(disconnected.m_vertexPos.end(), {{3,0,0}, {4,0,0}, {3,1,0}, {3,0,1}});
        disconnected.m_polygons.insert(disconnected.m_polygons.end(), {{3,120,6,7,8,0}, {3,120,6,8,9,0}});
        const auto separate = generate(disconnected, {0,0,0,0,0});
        const auto separateFlat = generate(disconnected, {0,0,0,0,0}, NormalGenerationMode::Flat);
        for (std::size_t i = promoted.size(); i < separate.size(); ++i)
        {
            Check(difference(separate[i], separateFlat[i]) < 0.00001f,
                "A disconnected right-angle patch must remain flat even when the same material is smooth elsewhere.");
        }
        auto slopes = patch;
        for (auto& p : slopes.m_vertexPos)
        {
            const float y = p.y * 0.8660254f - p.z * 0.5f;
            const float z = p.y * 0.5f + p.z * 0.8660254f;
            p = {p.x * 0.9396926f - y * 0.34202014f, p.x * 0.34202014f + y * 0.9396926f, z};
        }
        const auto sloped = generate(slopes, {0,1,2}, NormalGenerationMode::SlopedFaces);
        const auto fullySmooth = generate(slopes, {0,1,2}, NormalGenerationMode::Smooth);
        const auto slopedCrease = generate(slopes, {0,1,2}, NormalGenerationMode::CreaseAngle);
        int sharpened = 0;
        for (std::size_t i = 0; i < sloped.size(); ++i)
        {
            Check(difference(sloped[i], fullySmooth[i]) < 0.00001f,
                "Sloped faces must fully smooth across material boundaries, including edges sharper than 90 degrees.");
            sharpened += difference(sloped[i], slopedCrease[i]) > 0.1f;
        }
        Check(sharpened > 0, "The sloped fixture must exercise sharp edges beyond the old crease threshold.");
        for (int fixture = 0; fixture < 5; ++fixture)
        {
            ModelLoadingContext pair;
            pair.m_polygons = {{3,120,1,2,3,0}, {3,120,2,1,4,0}};
            if (fixture == 0) { pair.m_vertexPos = {{0,0,0}, {1,0,0}, {0,0,1}, {0,1,1}}; }
            if (fixture == 1) { pair.m_vertexPos = {{0,0,0}, {1,0,1}, {0,1,0}, {0,-1,0.5f}}; }
            if (fixture == 2) { pair.m_vertexPos = {{0,0,0}, {0,1,0}, {1,0,1}, {-1,0,2}}; }
            if (fixture == 3) { pair.m_vertexPos = {{0,0,0}, {1,0,0}, {0,0.0001f,1}, {0,1,1}}; }
            if (fixture == 4) { pair.m_vertexPos = {{0,0,0}, {1,0,1}, {0,1,0.0001f}, {0,-1,0.5f}}; }
            const auto actual = generate(pair, {0,1}, NormalGenerationMode::SlopedFaces);
            const auto expected = generate(pair, {0,1}, fixture < 3 ? NormalGenerationMode::Flat : NormalGenerationMode::Smooth);
            const auto unsmoothed = generate(pair, {0,1}, NormalGenerationMode::Flat);
            int smoothed = 0;
            for (std::size_t i = 0; i < actual.size(); ++i)
            {
                Check(difference(actual[i], expected[i]) < 0.00001f,
                    "Only exactly horizontal/vertical faces must stay flat, including vertical faces at arbitrary yaw.");
                smoothed += difference(actual[i], unsmoothed[i]) > 0.01f;
            }
            Check(fixture < 3 || smoothed > 0,
                "Almost horizontal/vertical faces must exercise real smoothing, not a broad angular tolerance.");
        }
        std::cout << "Connected material normals: bent-edge promotion past 90 degrees, source-slot boundaries, "
            "isolated triggers and disconnected same-material patches passed; default sloped faces smooth across materials, "
            "horizontal/vertical faces stay flat.\n";
    }

    int CountPromotedModelCorners(const Model& model, ID3D11Device3* device, ID3D11DeviceContext3* context)
    {
        const auto* mesh = model.GetNativeRuntimeObj<NativeModelMeshObjectDX11>();
        const auto vertices = ReadBuffer<ModelVertex>(device, context, mesh->m_vertexBuffer.Get());
        const auto indices = ReadBuffer<uint16_t>(device, context, mesh->m_indexBuffer.Get());
        ModelLoadingContext input;
        input.m_vertexPos = ReadBuffer<VertexPosition>(device, context, mesh->m_positionBuffer.Get());
        ModelConversionContext crease(input);
        crease.m_vertices = vertices;
        crease.m_sortedIndices = indices;
        GeometryUtils::SmoothNormals(crease, model.m_drawCalls, model.m_positionOfs, model.m_numPositions,
            {NormalGenerationMode::CreaseAngle});
        int promoted = 0;
        for (const auto& dc : model.m_drawCalls)
        {
            if (!dc.m_material.m_applyLighting || dc.m_material.m_transparencyType == TransparencyMode::Water) { continue; }
            for (int i = dc.m_firstIndex; i < dc.m_firstIndex + dc.m_primitiveCount * 3; i += 3)
            {
                if (vertices[indices[i]].StretchMode != (uint16_t)VertexStretchMode::None ||
                    vertices[indices[i + 1]].StretchMode != (uint16_t)VertexStretchMode::None ||
                    vertices[indices[i + 2]].StretchMode != (uint16_t)VertexStretchMode::None) { continue; }
                Vector triangle[3];
                for (int corner = 0; corner < 3; ++corner)
                {
                    const auto& p = input.m_vertexPos[model.m_positionOfs + vertices[indices[i + corner]].PositionIndex];
                    triangle[corner].Set(p.x, p.y, p.z, 0.f);
                }
                auto face = (triangle[1] - triangle[0]).GetCross(triangle[2] - triangle[0]);
                face.Normalize();
                for (int corner = 0; corner < 3; ++corner)
                {
                    const auto& before = crease.m_vertices[crease.m_sortedIndices[i + corner]].Normal;
                    const auto& after = vertices[indices[i + corner]].Normal;
                    if ((face - Vector(before.x, before.y, before.z)).GetLength() < 0.00001f &&
                        (face - Vector(after.x, after.y, after.z)).GetLength() > 0.01f) { ++promoted; }
                }
            }
        }
        return promoted;
    }

    void CheckSewerCeilingNormals(ID3D11Device3* device, ID3D11DeviceContext3* context)
    {
        for (const int number : {21,22})
        {
            const auto* level = FORCE_GET(AssetManager::Instance().LoadLevel(number));
            std::vector<const Model*> checked;
            int previousFlipped = 0, corners = 0;
            for (const auto& cell : level->GetCellArray().m_cell)
            {
                for (const int raw : {cell.m_floor, cell.m_ceiling})
                {
                    if (raw <= 31) { continue; }
                    const auto animation = level->GetLevelInfo().GetModelAnim(raw >= 160 ? 3 : 2);
                    Check(animation != nullptr, "Sewer tunnel tiles must resolve their original model bank.");
                    const auto* model = animation->GetModelSafe((raw & 31) / 5, (raw & 31) % 5);
                    Check(model != nullptr, "Sewer tunnel tiles must resolve their original model frame.");
                    if (std::find(checked.begin(), checked.end(), model) != checked.end()) { continue; }
                    checked.push_back(model);
                    const auto* mesh = model->GetNativeRuntimeObj<NativeModelMeshObjectDX11>();
                    const auto vertices = ReadBuffer<ModelVertex>(device, context, mesh->m_vertexBuffer.Get());
                    const auto indices = ReadBuffer<uint16_t>(device, context, mesh->m_indexBuffer.Get());
                    ModelLoadingContext input;
                    input.m_vertexPos = ReadBuffer<VertexPosition>(device, context, mesh->m_positionBuffer.Get());
                    ModelConversionContext previous(input);
                    previous.m_vertices = vertices;
                    previous.m_sortedIndices = indices;
                    GeometryUtils::SmoothNormals(previous, model->m_drawCalls, model->m_positionOfs, model->m_numPositions,
                        {NormalGenerationMode::ConnectedMaterial});
                    for (const auto& dc : model->m_drawCalls)
                    {
                        if (!dc.m_material.m_applyLighting || dc.m_material.m_transparencyType == TransparencyMode::Water) { continue; }
                        for (int i = dc.m_firstIndex; i < dc.m_firstIndex + dc.m_primitiveCount * 3; i += 3)
                        {
                            if (vertices[indices[i]].StretchMode != (uint16_t)VertexStretchMode::None ||
                                vertices[indices[i + 1]].StretchMode != (uint16_t)VertexStretchMode::None ||
                                vertices[indices[i + 2]].StretchMode != (uint16_t)VertexStretchMode::None) { continue; }
                            Vector triangle[3];
                            for (int c = 0; c < 3; ++c)
                            {
                                const auto& p = input.m_vertexPos[model->m_positionOfs + vertices[indices[i + c]].PositionIndex];
                                triangle[c].Set(p.x, p.y, p.z, 0.f);
                            }
                            auto face = (triangle[1] - triangle[0]).GetCross(triangle[2] - triangle[0]);
                            if (face.GetLength() == 0.f) { continue; }
                            face.Normalize();
                            for (int c = 0; c < 3; ++c)
                            {
                                const auto& n = vertices[indices[i + c]].Normal;
                                const auto& old = previous.m_vertices[previous.m_sortedIndices[i + c]].Normal;
                                const float dot = face.GetX() * n.x + face.GetY() * n.y + face.GetZ() * n.z;
                                previousFlipped += face.GetX() * old.x + face.GetY() * old.y + face.GetZ() * old.z < -0.00001f;
                                Check(std::abs(Vector(n.x, n.y, n.z).GetLength() - 1.f) < 0.00001f && dot > 0.000001f,
                                    "Original sewer tunnel normals must be unit length and face into each triangle's own hemisphere.");
                                ++corners;
                            }
                        }
                    }
                }
            }
            Check(!checked.empty() && corners > 0, "Sewer fixtures must exercise real polygonal ceiling tiles.");
            Check(previousFlipped > 0, "Original sewer models must reproduce opposing normals with the previous patch policy.");
            std::cout << "Sewer " << number << ": " << checked.size() << " tunnel models, " << corners
                << " lit polygon corners; opposing normals reduced from " << previousFlipped << " to zero.\n";
        }
    }

    void CheckSmoothPolygonNormals(RendererDX11& renderer, ID3D11Device3* device, ID3D11DeviceContext3* context)
    {
        CheckSewerCeilingNormals(device, context);
        CheckNormalGenerationPolicies();
        ModelLoadingContext input;
        input.m_vertexPos = { {9,9,9}, {8,8,8}, {-1,0,0}, {1,0,0}, {0,1,0.35f}, {0,-1,0.35f} };
        input.m_polygons = { {3,120,1,2,3,0}, {3,121,2,1,4,0} };
        Model model;
        model.m_vertexOfs = model.m_positionOfs = 2;
        model.m_numPositions = 4;
        model.m_numPolygons = 2;
        ModelConversionContext conversion(input);
        conversion.m_modelPositionOfs = 2;
        conversion.m_numPositions = 4;
        GeometryUtils::Convert(model, conversion);
        Check(conversion.m_vertices.size() == 12, "Both folded triangles must retain their generated reverse faces.");
        for (const auto& vertex : conversion.m_vertices)
        {
            const auto& smooth = vertex.Normal;
            const float length = Vector(smooth.x, smooth.y, smooth.z).GetLength();
            Check(std::abs(length - 1.f) < 0.00001f, "Smooth polygon normals must be finite unit vectors.");
            if (vertex.PositionIndex < 2)
            {
                Check(std::abs(smooth.x) < 0.00001f && std::abs(smooth.y) < 0.00001f &&
                    std::abs(smooth.z - (vertex.NormalGroup == 0 ? -1.f : 1.f)) < 0.00001f,
                    "Shared corners across split render vertices must smooth, with opposite reverse-face normals.");
            }
        }

        const auto firstVertices = conversion.m_vertices;
        input.m_vertexPos.insert(input.m_vertexPos.end(), { {-1,0,0}, {1,0,0}, {0,-1,0.35f}, {0,1,0.35f} });
        Model second;
        second.m_vertexOfs = second.m_positionOfs = 6;
        second.m_numPositions = 4;
        second.m_numPolygons = 2;
        conversion.m_modelPositionOfs = 6;
        GeometryUtils::Convert(second, conversion);
        for (std::size_t i = 0; i < firstVertices.size(); ++i)
        {
            const auto& a = firstVertices[i].Normal;
            const auto& b = conversion.m_vertices[i].Normal;
            Check(a.x == b.x && a.y == b.y && a.z == b.z,
                "Later animation frames must neither mix with nor overwrite earlier frames' normals.");
        }
        Check(conversion.m_vertices[firstVertices.size()].Normal.z > 0.999f,
            "Later frames must calculate normals from their own nonzero position offset.");
        for (auto& vertex : conversion.m_vertices) { vertex.Color = {200,200,200,0}; }
        model.m_runtime = renderer.CreateModelBuffers(conversion);

        Model flatModel, narrowCreaseModel;
        ModelConversionContext flatConversion(input), narrowConversion(input);
        for (auto* fixture : {&flatModel, &narrowCreaseModel})
        {
            fixture->m_vertexOfs = fixture->m_positionOfs = 2;
            fixture->m_numPositions = 4;
            fixture->m_numPolygons = 2;
        }
        flatModel.m_normalGeneration.mode = NormalGenerationMode::Flat;
        narrowCreaseModel.m_normalGeneration.mode = NormalGenerationMode::CreaseAngle;
        narrowCreaseModel.m_normalGeneration.creaseAngleDegrees = 30.f;
        for (auto* fixture : {&flatConversion, &narrowConversion})
        {
            fixture->m_modelPositionOfs = 2;
            fixture->m_numPositions = 4;
        }
        GeometryUtils::Convert(flatModel, flatConversion);
        GeometryUtils::Convert(narrowCreaseModel, narrowConversion);
        for (std::size_t i = 0; i < flatConversion.m_vertices.size(); ++i)
        {
            const auto& a = flatConversion.m_vertices[i].Normal;
            const auto& b = narrowConversion.m_vertices[i].Normal;
            Check(Vector(a.x - b.x, a.y - b.y, a.z - b.z).GetLength() < 0.00001f,
                "A configurable 30-degree crease must keep the 39-degree fold hard.");
            flatConversion.m_vertices[i].Color = narrowConversion.m_vertices[i].Color = {200,200,200,0};
        }
        flatModel.m_runtime = renderer.CreateModelBuffers(flatConversion);
        narrowCreaseModel.m_runtime = renderer.CreateModelBuffers(narrowConversion);

        ModelLoadingContext quads;
        quads.m_vertexPos = { {0,0,0}, {1,0,0}, {1,1,0}, {0,1,0}, {0,0,1}, {1,0,1} };
        quads.m_polygons = { {6,0,1,2,3,4}, {6,0,5,6,2,1} };
        ModelAnim owner;
        Model corner;
        corner.m_owner = &owner;
        corner.m_numPositions = 6;
        corner.m_numPolygons = 2;
        corner.m_normalGeneration.mode = NormalGenerationMode::Smooth;
        ModelConversionContext quadConversion(quads);
        GeometryUtils::Convert(corner, quadConversion);
        for (const auto& vertex : quadConversion.m_vertices)
        {
            if (vertex.PositionIndex != 0) { continue; }
            Check(std::abs(vertex.Normal.y - std::sqrt(0.5f)) < 0.00001f &&
                std::abs(vertex.Normal.z - std::sqrt(0.5f)) < 0.00001f,
                "A quad diagonal must not bias smooth normals relative to an adjacent unsplit corner.");
        }
        for (std::size_t i = 0; i < quadConversion.m_vertices.size(); ++i)
        {
            quadConversion.m_vertices[i].NormalGroup = i < 4 ? 2 : 0;
        }
        quadConversion.GeneratePerTriangleNormals(quadConversion.m_sortedIndices.data(),
            static_cast<int>(quadConversion.m_sortedIndices.size()), 0);
        GeometryUtils::SmoothNormals(quadConversion, corner.m_drawCalls, 0, 6, {NormalGenerationMode::Smooth});
        for (std::size_t i = 0; i < quadConversion.m_vertices.size(); ++i)
        {
            const auto& normal = quadConversion.m_vertices[i].Normal;
            Check(i < 4 ? std::abs(normal.z - 1.f) < 0.00001f : std::abs(normal.y - 1.f) < 0.00001f,
                "Fully smooth heightmap surfaces must not inherit normals from a connected wall group.");
        }
        Model hardCorner;
        hardCorner.m_owner = &owner;
        hardCorner.m_numPositions = 6;
        hardCorner.m_numPolygons = 2;
        ModelConversionContext hardConversion(quads);
        GeometryUtils::Convert(hardCorner, hardConversion);
        for (const auto& vertex : hardConversion.m_vertices)
        {
            Check((std::abs(vertex.Normal.y - 1.f) < 0.00001f && std::abs(vertex.Normal.z) < 0.00001f) ||
                (std::abs(vertex.Normal.z - 1.f) < 0.00001f && std::abs(vertex.Normal.y) < 0.00001f),
                "Default sloped-face smoothing must preserve horizontal/vertical stair and box faces.");
        }
        ModelLoadingContext foldedQuad;
        foldedQuad.m_vertexPos = { {0,0,0}, {1,0,0}, {0,1,0}, {0,0,1} };
        foldedQuad.m_polygons = { {6,0,1,2,3,4} };
        Model splitCorner;
        splitCorner.m_owner = &owner;
        splitCorner.m_numPositions = 4;
        splitCorner.m_numPolygons = 1;
        ModelConversionContext splitConversion(foldedQuad);
        GeometryUtils::Convert(splitCorner, splitConversion);
        Check(splitConversion.m_vertices.size() == 6 &&
            splitConversion.m_sortedIndices[0] != splitConversion.m_sortedIndices[3] &&
            splitConversion.m_sortedIndices[2] != splitConversion.m_sortedIndices[4],
            "Hard edges must split previously shared render vertices into independent normal fans.");
        ModelConversionContext flatTriangles(foldedQuad);
        for (uint16_t i = 0; i < 4; ++i)
        {
            flatTriangles.m_vertices.emplace_back().SetTextured(i, Color4{255,255,255,0}, {0,0});
        }
        flatTriangles.m_sortedIndices = {0,1,2,0,2,3,0,0,0};
        flatTriangles.GeneratePerTriangleNormals(flatTriangles.m_sortedIndices.data(), 9, 0);
        for (int i = 0; i < 9; ++i)
        {
            const auto& normal = flatTriangles.m_vertices[flatTriangles.m_sortedIndices[i]].Normal;
            Check(i < 3 ? std::abs(normal.z - 1.f) < 0.00001f :
                i < 6 ? std::abs(normal.x - 1.f) < 0.00001f :
                Vector(normal.x, normal.y, normal.z).GetLength() == 0.f,
                "Flat normals must remain independent across shared corners, including degenerate neighboring triangles.");
        }

        ModelLoadingContext touching;
        touching.m_vertexPos = { {0,0,0}, {1,0,0}, {0,1,0}, {-1,0,0}, {0,0,1} };
        touching.m_polygons = { {3,120,1,2,3,0}, {3,120,1,4,5,0} };
        Model disconnected;
        disconnected.m_numPositions = 5;
        disconnected.m_numPolygons = 2;
        disconnected.m_normalGeneration.mode = NormalGenerationMode::Smooth;
        ModelConversionContext touchingConversion(touching);
        GeometryUtils::Convert(disconnected, touchingConversion);
        for (const auto& vertex : touchingConversion.m_vertices)
        {
            Check(std::abs(vertex.Normal.y * vertex.Normal.z) < 0.00001f,
                "Full smoothing must not weld disconnected faces that only share a vertex.");
        }

        ModelAnim animated;
        animated.m_numAnims = 1;
        animated.m_numFrames = 3;
        ModelLoadingContext frames;
        frames.m_vertexPos = { {-1,0,0}, {1,0,0}, {0,1,0.35f}, {0,-1,0.35f},
            {-1,0,0}, {1,0,0}, {0,1,2}, {0,-1,2},
            {-1,0,0}, {1,0,0}, {0,1,0}, {0,-1,0.35f} };
        frames.m_polygons = { {3,120,1,2,3,0}, {3,121,2,1,4,0},
            {3,120,1,2,3,0}, {3,121,2,1,4,0},
            {3,120,1,2,3,0}, {3,121,2,1,4,0} };
        ModelConversionContext frameConversion(frames);
        for (int frame = 0; frame < 3; ++frame)
        {
            auto* mesh = animated.GetModel(0, frame);
            mesh->m_numPositions = 4;
            mesh->m_numPolygons = 2;
            mesh->m_positionOfs = frame * 4;
            mesh->m_polygonOfs = frame * 2;
            frameConversion.m_modelPositionOfs = mesh->m_positionOfs;
            GeometryUtils::Convert(*mesh, frameConversion);
        }
        Check(frameConversion.m_vertices[12].Normal.z < -0.999f &&
            std::abs(frameConversion.m_vertices[12].Normal.y) < 0.00001f,
            "A later frame bent past 90 degrees must retain the first frame's smooth edge classification.");
        Check(std::abs(frameConversion.m_vertices[24].Normal.y) > 0.01f,
            "A later frame becoming exactly vertical must retain the first frame's sloped-face classification.");

        Target target(device, 360, 240);
        renderer.ReInitialize(target.colorView.Get(), target.depthView.Get(), target.viewport);
        Texture lightmap;
        lightmap.CreatePlainWhite();
        lightmap.m_atlasData = {0,0,0.5f,0.5f};
        Camera camera;
        Matrix4 projection, transform;
        projection.CreateOrthographic(4.5f, 3.f, 0.01f, 20.f);
        transform.SetTranslation(Vector(0,0,5));
        camera.SetProjectionMatrix(projection);
        camera.SetCameraTransformMatrix(transform);
        RenderingSettings settings;
        ProfilingStats stats;
        const auto render = [&](const Renderable& mesh, float scale = 1.f, float fogFalloff = 0.f,
            Vector3 fogColor = {0,0,0}, float cameraDistance = 5.f)
        {
            Matrix4 cameraTransform;
            cameraTransform.SetTranslation(Vector(0,0,cameraDistance));
            camera.SetCameraTransformMatrix(cameraTransform);
            RenderableInstance instance(&mesh);
            instance.m_uniformScale = scale;
            instance.ComputeTransparencyMask();
            instance.UpdateGlobalBounds();
            VisibilityResult visible;
            visible.m_mainCamera = &camera;
            visible.m_visibleInstances.m_opaqueInstances.push_back({&instance, 0});
            visible.m_visibleInstances.m_visibleTransparencyTypes = instance.GetUsedTransparencyMask();
            visible.m_ambientColor = {0.6f,0.6f,0.6f};
            visible.m_ambientNrmlContribution = 0.8f;
            visible.m_fogColor = fogColor;
            visible.m_fogFalloff = fogFalloff;
            visible.m_lightmap = &lightmap;
            const float black[4]{0,0,0,1};
            context->ClearRenderTargetView(target.colorView.Get(), black);
            context->ClearDepthStencilView(target.depthView.Get(), D3D11_CLEAR_DEPTH, 1, 0);
            renderer.RenderSceneElements(visible, settings, stats);
            Check(stats.m_numDrawCalls > 0, "Smooth-normal lighting fixtures must submit actual GPU draw calls.");
            return target.Pixels(device, context);
        };
        const auto flat = render(flatModel);
        const auto smooth = render(model);
        int changed = 0, visible = 0;
        for (std::size_t i = 0; i < flat.size(); ++i)
        {
            changed += flat[i] != smooth[i];
            visible += (flat[i] & 0xffffff) != 0;
        }
        std::cout << "Folded polygon lighting: " << visible << " visible pixels, " << changed << " changed pixels.\n";
        Check(visible > 100 && changed > 100, "Smooth normals must change actual pixel lighting on the folded mesh.");
        Check(render(narrowCreaseModel) == flat, "Crease-preserved edges must match the baked flat-mode pixels.");
        Check(render(model) == smooth, "Baked normals must render identically without rebuilding geometry.");
        for (auto billboard : { GeometryUtils::CreateSpriteMesh(false),
            GeometryUtils::CreateCameraBillboardQuadMesh(1,1,Color4{255,255,255,0}) })
        {
            auto& material = billboard->m_drawCalls[0].m_material;
            material.m_texture = AssetManager::Instance().GetPlainWhiteTexture();
            material.m_applyLighting = true;
            material.ComputeHash();
            const auto original = render(*billboard, 200.f);
            Check(std::count_if(original.begin(), original.end(),
                [](std::uint32_t pixel) { return (pixel & 0xffffff) != 0; }) > 5,
                "Billboard regression checks must draw actual lit pixels.");
            Check(render(*billboard, 200.f) == original, "Lit billboard rendering must remain stable.");
        }
        ModelAnim colorOwner;
        const auto colorModel = [&](const std::vector<ModelPolygon>& modifiers, bool modifiersFirst = false,
            TextureLightingMode lightingMode = TextureLightingMode::VertexLighting)
        {
            ModelLoadingContext source;
            source.m_vertexPos = {{99,99,99}, {98,98,98}, {-1,-1,0}, {1,-1,0}, {1,1,0}, {-1,1,0}};
            const auto flags = static_cast<uint8_t>(static_cast<uint8_t>(lightingMode) << 5);
            source.m_polygons = {{6,flags,1,2,3,4}, {6,flags,4,3,2,1}};
            source.m_polygons.insert(modifiersFirst ? source.m_polygons.begin() : source.m_polygons.end(),
                modifiers.begin(), modifiers.end());
            auto fixture = std::make_shared<Model>();
            fixture->m_owner = &colorOwner;
            fixture->m_vertexOfs = fixture->m_positionOfs = 2;
            fixture->m_numPositions = 4;
            fixture->m_numPolygons = static_cast<uint16_t>(source.m_polygons.size());
            ModelConversionContext converted(source);
            converted.m_modelPositionOfs = 2;
            converted.m_numPositions = 4;
            GeometryUtils::Convert(*fixture, converted);
            for (const auto& vertex : converted.m_vertices)
            {
                Check(vertex.Color.a == 0, "Brightness modifiers must leave alpha/emissive channels untouched.");
            }
            for (auto& draw : fixture->m_drawCalls)
            {
                draw.m_material.m_texture = AssetManager::Instance().GetPlainWhiteTexture();
                draw.m_material.ComputeHash();
            }
            fixture->m_runtime = renderer.CreateModelBuffers(converted);
            return fixture;
        };
        const auto uniformColorModel = [&](uint8_t code)
        {
            std::vector<ModelPolygon> modifiers;
            for (uint8_t position = 1; position <= 4; ++position) { modifiers.push_back({8,code,position,0,0,0}); }
            return colorModel(modifiers);
        };
        const auto plainColor = colorModel({});
        const auto relativeColor = uniformColorModel(198);
        const auto absoluteColor = uniformColorModel(241);
        const Vector3 coloredFog{0.1f,0.2f,0.05f};
        const auto baselinePixels = render(*plainColor);
        const auto relativePixels = render(*relativeColor);
        const auto absolutePixels = render(*absoluteColor);
        const auto center = 120 * 360 + 180;
        const float baselineRed = static_cast<float>(baselinePixels[center] & 255) / 255.f;
        const float relativeRed = static_cast<float>(relativePixels[center] & 255) / 255.f;
        const float absoluteRed = static_cast<float>(absolutePixels[center] & 255) / 255.f;
        Check(std::abs(baselineRed - 0.6f) < 0.01f &&
            std::abs(relativeRed - 0.6f * 127.f / 255.f) < 0.01f &&
            std::abs(absoluteRed - 0.6f * 63.f / 255.f) < 0.01f,
            "Actual shader pixels must match baked RGB multipliers even with fog disabled.");
        const auto coloredFogPixels = render(*relativeColor, 1.f, 0.25f, coloredFog);
        const float foggedRed = static_cast<float>(coloredFogPixels[center] & 255) / 255.f;
        Check(std::abs(foggedRed - (0.1f + (0.6f * 127.f / 255.f - 0.1f) * expf(-1.25f))) < 0.01f &&
            render(*absoluteColor, 1.f, 0.25f, coloredFog, 8.f) != render(*absoluteColor, 1.f, 0.25f, coloredFog),
            "Authored RGB darkening must not replace or offset ordinary distance fog.");
        const auto dark = colorModel({}, false, TextureLightingMode::Dark);
        const auto fullbright = colorModel({}, false, TextureLightingMode::Fullbright);
        Check((render(*dark)[center] & 0xffffff) == 0 && (render(*fullbright)[center] & 0xffffff) == 0xffffff,
            "Native fixed-dark and fullbright textured primitives must remain distinct from normal lighting.");
        const std::vector<ModelPolygon> mixedModifiers{{8,192,1,0,0,0}, {8,240,2,0,0,0},
            {8,201,3,0,0,0}, {8,244,4,0,0,0}, {40,198,1,0,0,0}};
        const auto mixed = colorModel(mixedModifiers);
        const auto mixedFirst = colorModel(mixedModifiers, true);
        Check(render(*mixed, 1.f, 0.25f, coloredFog) == render(*mixedFirst, 1.f, 0.25f, coloredFog),
            "Modifier placement before or after visible primitives must not change the result.");
        const auto* mixedMesh = mixed->GetNativeRuntimeObj<NativeModelMeshObjectDX11>();
        const auto mixedVertices = ReadBuffer<ModelVertex>(device, context, mixedMesh->m_vertexBuffer.Get());
        int checkedCopies = 0;
        for (const auto& vertex : mixedVertices)
        {
            const byte expected[]{127,0,255,255};
            const auto brightness = expected[vertex.PositionIndex];
            Check(vertex.Color.r == brightness && vertex.Color.g == brightness && vertex.Color.b == brightness &&
                vertex.Color.a == 0,
                "GPU vertex colors must follow source positions across split/reverse vertices, with the last modifier winning.");
            ++checkedCopies;
        }
        Check(checkedCopies == 12, "Vertex-color tests must inspect all flat-split triangle corners of both quads.");
        for (const uint8_t invalidPosition : {uint8_t(0), uint8_t(5)})
        {
            bool rejected = false;
            try { colorModel({{8,192,invalidPosition,0,0,0}}); }
            catch (const std::invalid_argument&) { rejected = true; }
            Check(rejected, "Out-of-model brightness positions must report an explicit conversion error.");
        }
        class LightmapLevel final : public Level
        {
        public:
            const Texture& Build()
            {
                std::vector<byte> planes(7 * 50 * 40, 0);
                for (int i = 0; i < 50 * 40; ++i) { planes[5 * 50 * 40 + i] = static_cast<byte>(i); }
                ReadLevelData(planes.data(), static_cast<int>(planes.size()), false);
                CreateLightmap();
                return *m_lightmap;
            }
        } lightmapLevel;
        const auto& gridLightmap = lightmapLevel.Build();
        Target lightmapCapture(device, 50, 40);
        context->CopyResource(lightmapCapture.color.Get(),
            gridLightmap.GetNativeRuntimeObj<NativeTextureObjectDX11>()->m_nativeTex.Get());
        const auto lightmapPixels = lightmapCapture.Pixels(device, context);
        for (int i = 0; i < 50 * 40; ++i)
        {
            const auto brightness = static_cast<byte>(100 + ((i & 255) >> 5) * 155 / 7);
            const auto& cell = lightmapLevel.GetCellArray().m_cell[i];
            Check(cell.m_brightness.r == brightness && cell.m_brightness.g == brightness &&
                cell.m_brightness.b == brightness && (lightmapPixels[i] & 0xffffff) == brightness * 0x010101,
                "The uploaded grid lightmap must use only the upper three surface bits, independent of all 32 surface selectors.");
            const auto position = lightmapLevel.GetCellArray().GetCenterPos(i % 50, i / 50, HeightLevel::Floor);
            Check(std::abs(position.x * gridLightmap.m_atlasData.scaleU + gridLightmap.m_atlasData.ofsU -
                    (i % 50 + 0.5f) / 50.f) < 0.00001f &&
                std::abs(position.z * gridLightmap.m_atlasData.scaleV + gridLightmap.m_atlasData.ofsV -
                    (i / 50 + 0.5f) / 40.f) < 0.00001f,
                "Each world-space tile center must sample its own lightmap texel center.");
        }
        const auto* burningForest = FORCE_GET(AssetManager::Instance().LoadLevel(25));
        int darkForestCorners = 0, clearForestCorners = 0;
        for (int frame = 0; frame < 5; ++frame)
        {
            const auto* tile = burningForest->GetLevelInfo().GetModelAnim(2)->GetModel(4, frame);
            const auto* mesh = tile->GetNativeRuntimeObj<NativeModelMeshObjectDX11>();
            const auto vertices = ReadBuffer<ModelVertex>(device, context, mesh->m_vertexBuffer.Get());
            const auto indices = ReadBuffer<uint16_t>(device, context, mesh->m_indexBuffer.Get());
            for (const auto& dc : tile->m_drawCalls)
            {
                for (int i = dc.m_firstIndex; i < dc.m_firstIndex + dc.m_primitiveCount * 3; ++i)
                {
                    const auto& v = vertices[indices[i]];
                    darkForestCorners += v.Color.r == 0 && v.Color.g == 0 && v.Color.b == 0;
                    clearForestCorners += v.Color.r == 255 && v.Color.g == 255 && v.Color.b == 255;
                }
            }
            const auto* forestTile = burningForest->GetLevelInfo().GetModelAnim(2)->GetModel(3, frame);
            const auto* forestMesh = forestTile->GetNativeRuntimeObj<NativeModelMeshObjectDX11>();
            const auto forestVertices = ReadBuffer<ModelVertex>(device, context, forestMesh->m_vertexBuffer.Get());
            const auto forestIndices = ReadBuffer<uint16_t>(device, context, forestMesh->m_indexBuffer.Get());
            int fixedDarkCorners = 0;
            for (const auto& dc : forestTile->m_drawCalls)
            {
                if (dc.m_material.m_textureSlot != 5) { continue; }
                for (int i = dc.m_firstIndex; i < dc.m_firstIndex + dc.m_primitiveCount * 3; ++i)
                {
                    const auto& color = forestVertices[forestIndices[i]].Color;
                    Check(color.r == 0 && color.g == 0 && color.b == 0 && color.a == 0,
                        "Original burning-forest ground quads must honor their fixed-dark texture mode, not just path-edge modifiers.");
                    ++fixedDarkCorners;
                }
            }
            Check(frame == 4 || fixedDarkCorners > 0,
                "The four original forest variants with ground quads must exercise their fixed-dark texture mode.");
        }
        Check(darkForestCorners > 0 && clearForestCorners > 0,
            "Original burning-forest path tiles must upload both darkened and unmodified colors across their frames.");
        std::cout << "Authored vertex colors: exact RGB and ordinary-fog pixels, fixed dark/fullbright texture modes, "
            "record order and split vertices passed; original forest has " << darkForestCorners << " dark and "
            << clearForestCorners << " clear corners.\n";
        std::cout << "Baked normals: 60/30-degree creases, smooth/flat overrides, hard vertex splits, disconnected fans, "
            "stable animation creases, reverse faces, quad weighting and actual lighting pixels passed.\n";
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

    void CheckParkAttachments(RendererDX11& renderer, ID3D11Device3* device, ID3D11DeviceContext3* context)
    {
        auto* park = FORCE_GET(AssetManager::Instance().LoadLevel(30));
        const auto records = FORCE_GET(FileManager::Instance().ReadDataFuture(
            FileManager::Instance().FilenameToNative(L"LEVEL\\OBJPOS30.DAT")));
        const auto record = [&](int slot)
        {
            ::RawGameObject object{};
            std::memcpy(object.m_rawBytes, records.data() + (slot - 2) * sizeof(object.m_rawBytes), sizeof(object.m_rawBytes));
            return object;
        };
        for (const int slot : {33,34,35,50,75,65})
        {
            auto raw = record(slot);
            auto instance = std::make_shared<RenderableInstance>();
            for (const int weight : {0,25,50,100,50,0})
            {
                raw.m_rawShorts[21] = static_cast<short>(weight);
                park->ApplyRenderProperties(instance, raw);
                instance->PrepareSubInstancesForRendering();
                const auto* primary = instance->GetRenderable();
                const auto* target = instance->GetBlendTargetRenderable();
                Check(primary && target && !instance->m_subInstances.empty(),
                    "Real cow bodies and the tree door must preserve their embedded mesh attachments.");
                for (std::size_t i = 0; i < instance->m_subInstances.size(); ++i)
                {
                    const auto& attachment = primary->m_subRenderables[i];
                    const auto& from = primary->m_sourcePositions.at(attachment.m_anchorVertex);
                    const auto& to = target->m_sourcePositions.at(attachment.m_anchorVertex);
                    const float blend = weight * 0.01f;
                    const Vector local(from.x + (to.x - from.x) * blend,
                        from.y + (to.y - from.y) * blend, from.z + (to.z - from.z) * blend);
                    const auto& child = instance->m_subInstances[i];
                    Check((child.GetTransform().GetTranslation() -
                        instance->GetTransform().TransformPosition(local)).GetLength() < 0.00001f &&
                        child.GetGlobalBounds().IsValid(),
                        "Embedded heads and doors must follow the actual morphed parent vertex, with valid world bounds.");
                    for (std::size_t draw = 0; draw < child.GetRenderable()->m_drawCalls.size(); ++draw)
                    {
                        const auto& overrides = child.GetMaterialOverrides();
                        const auto& material = draw < overrides.size() ? overrides[draw] :
                            child.GetRenderable()->m_drawCalls[draw].m_material;
                        Check(material.m_texture != nullptr,
                            "Repeated same-frame updates must retain level textures on embedded door meshes.");
                    }
                }
            }
        }

        auto raw = record(65);
        auto tree = std::make_shared<RenderableInstance>();
        park->ApplyRenderProperties(tree, raw);
        park->ApplyRenderProperties(tree, raw);
        tree->PrepareSubInstancesForRendering();
        const auto* door = &tree->m_subInstances.at(1);
        class MeshScene final : public Scene
        {
        public:
            FUTURE(void) CreateScene() override { m_sceneReady = true; CO_RETURN; }
        } scene;
        FORCE_WAIT(scene.CreateScene());
        scene.AddRenderableInstance(tree);
        Target image(device, 360, 240);
        renderer.ReInitialize(image.colorView.Get(), image.depthView.Get(), image.viewport);
        Camera camera;
        Matrix4 projection, transform;
        projection.CreateProjection(1.5f, Math::Pi / 3.f, 0.001f, 10.f);
        camera.SetProjectionMatrix(projection);
        auto direction = tree->GetTransform().GetDirection();
        auto right = tree->GetTransform().GetRightDir();
        direction *= -1.f; right *= -1.f;
        auto distance = direction; distance *= 0.15f;
        const auto cameraPosition = door->GetGlobalBounds().GetCenter() + distance;
        transform.Create(Vector(cameraPosition.GetX(), cameraPosition.GetY(), cameraPosition.GetZ()), right,
            tree->GetTransform().GetUpDir(), direction);
        camera.SetCameraTransformMatrix(transform);
        RenderingSettings settings;
        settings.fullbrightRendering = true;
        ProfilingStats stats;
        VisibilityResult visible;
        DefaultVisibilityContext visibility;
        visibility.ComputeResultFor(&camera, &scene, visible, settings, VisibilitySettings{}, stats);
        const float black[]{0,0,0,1};
        const auto render = [&]
        {
            context->ClearRenderTargetView(image.colorView.Get(), black);
            context->ClearDepthStencilView(image.depthView.Get(), D3D11_CLEAR_DEPTH, 1.f, 0);
            renderer.RenderSceneElements(visible, settings, stats);
            return image.Pixels(device, context);
        };
        const auto withDoor = render();
        auto& opaque = visible.m_visibleInstances.m_opaqueInstances;
        const auto count = opaque.size();
        opaque.erase(std::remove_if(opaque.begin(), opaque.end(),
            [door](const auto& entry) { return entry.m_instance == door; }), opaque.end());
        Check(opaque.size() + 1 == count, "The real tree door must enter the visible opaque mesh list.");
        const auto withoutDoor = render();
        int changed = 0;
        for (std::size_t i = 0; i < withDoor.size(); ++i) { changed += withDoor[i] != withoutDoor[i]; }
        Check(changed > 100, "The actual tree-door geometry must contribute visible pixels, not just exist in a child list.");

        LevelScene waterScene(30);
        Desperabis::RawGameObject drop{};
        drop.health = 1000;
        drop.modelId = 2; drop.modelAnim = 2; drop.animFrame = 1; drop.spriteScale = 64;
        drop.displayType = Desperabis::DisplayType::SpriteNormal;
        drop.collisionType = Desperabis::CollisionType::FountainWater;
        ::RawGameObject water{};
        Desperabis::EncodeGameObject(drop, water.m_rawBytes, sizeof(water.m_rawBytes));
        auto particle = std::make_shared<RenderableInstance>();
        for (int tick = 0; tick < 3; ++tick)
        {
            park->ApplyRenderProperties(particle, water);
            waterScene.UpdateObjectPresentation(100, water, particle.get());
            Check(particle->GetVisible() && std::abs(particle->GetMaterialOverrides().at(0).m_opacity -
                    0.15f * GamePlay::PropertyAccessorBase(water).GetSpriteTransparency()) < 0.00001f &&
                particle->GetUsedTransparencyMask() == TransparencyModeMask::AlphaBlendedBit,
                "Fountain drops must retain 15 percent of authored opacity after repeated sprite updates.");
        }

        const auto playerCamera = renderer.GetOrCreateCamera();
        const auto saved = playerCamera->GetCameraTransformMatrix(0);
        transform.Create(Vector(1,2,3), Vector(0.7f,-0.3f,0));
        FreeCamera free;
        free.SetUseGravity(false);
        free.JumpTo(transform);
        free.SetWindowCenter(120,80);
        free.SetMousePos(120,80);
        free.Update(0, park);
        Check((playerCamera->GetCameraTransformMatrix(0).GetDirection() - transform.GetDirection()).GetLength() < 0.00001f,
            "Entering free camera must preserve the player camera's heading and pitch.");
        free.SetSpeed(0,0,1);
        free.Update(0.5, park);
        Check(std::abs(free.GetPosition().GetY() - 3.5f) < 0.00001f,
            "Free-camera elevation must be independent of the actual park terrain.");
        free.JumpTo(transform);
        free.Update(0.5, park);
        Check((free.GetPosition() - transform.GetTranslation()).GetLength() < 0.00001f,
            "Re-entering free camera must clear stale movement and preserve the player location.");
        playerCamera->SetCameraTransformMatrix(saved);
        std::cout << "Park: five morphed cow heads, textured tree door (" << changed <<
            " pixels), transparent fountain and free flight.\n";
    }

    void CheckFinalLevelRuntimePresentation(RendererDX11& renderer, ID3D11Device3* device, ID3D11DeviceContext3* context)
    {
        class Host final : public Desperabis::GameContext
        {
        public:
            LevelScene* scene = nullptr;
            void OnObjectDestroyed(int slot) override { if (scene) { scene->RemoveObjectPresentation(slot); } }
        private:
            std::vector<std::uint8_t> LoadFile(std::string_view filename) override
            {
                return FORCE_GET(FileManager::Instance().ReadDataFuture(FileManager::Instance().FilenameToNative(
                    std::wstring(filename.begin(), filename.end()))));
            }
            void LoadLevel(const Desperabis::LevelLoadRequest& request) override
            {
                Desperabis::LevelLoadData data;
                LoadLevelGeometry(request.levelNumber, data);
                LoadLevelObjects(request.levelNumber, data);
                InitializeLevelPlayerSpawn(data, {0, 0, 100});
                CompleteLevelLoad(request.id, std::move(data));
            }
        } core;
        core.StartLevel(26);
        LevelScene scene(26);
        scene.m_createGameObjects = false;
        FORCE_WAIT(scene.CreateScene());
        core.scene = &scene;
        scene.ClearObjectPresentations();
        scene.GetPlayerLight()->SetMode(PlayerLight::Mode::Off);
        const float extent = 20.f * DespToFloat;
        ModelLoadingContext positions;
        positions.m_vertexPos = {{-extent,-extent,0,1}, {extent,-extent,0,1},
            {extent,extent,0,1}, {-extent,extent,0,1}};
        ModelConversionContext mesh(positions);
        mesh.m_numPositions = 4;
        for (uint16_t vertex = 0; vertex < 4; ++vertex)
        {
            mesh.AddToBounds(positions.m_vertexPos[vertex], 0.f);
            mesh.m_vertices.emplace_back().SetTextured(vertex, {255,255,255,0}, {0,0});
        }
        mesh.m_sortedIndices = {0,1,2,0,2,3,0,2,1,0,3,2};
        mesh.GeneratePerTriangleNormals(mesh.m_sortedIndices.data(), 12, 0);
        for (auto& vertex : mesh.m_vertices) { vertex.Normal = {0,0,-1}; }
        Renderable receiver;
        receiver.m_runtime = renderer.CreateModelBuffers(mesh);
        receiver.m_numPositions = 4;
        receiver.m_numPolygons = 2;
        mesh.GenerateBoundingVolume(receiver.m_localBounds);
        auto& draw = receiver.m_drawCalls.emplace_back();
        draw.m_primitiveCount = 4;
        draw.m_material.m_texture = AssetManager::Instance().GetPlainWhiteTexture();
        draw.m_material.m_transparencyType = TransparencyMode::Opaque;
        draw.m_material.m_applyLighting = true;
        draw.m_material.ComputeHash();
        auto cube = std::make_shared<RenderableInstance>(&receiver);
        const auto& gold = core.worldObjects[5];
        const auto position = Level::DespToPos(gold.posX, gold.posY, gold.posZ);
        cube->SetTranslation(Vector(position.GetX(), position.GetY(), position.GetZ() - 30.f * DespToFloat));
        for (auto& material : cube->GetOrCreateMaterialOverrides())
        {
            material.m_texture = AssetManager::Instance().GetPlainWhiteTexture();
            material.m_applyLighting = true;
            material.ComputeHash();
        }
        cube->ComputeTransparencyMask();
        Matrix4 projection, transform;
        projection.CreateOrthographic(80.f * DespToFloat, 60.f * DespToFloat, 0.01f, 20.f);
        transform.SetTranslation(Vector(position.GetX(), position.GetY(), position.GetZ() - 30.f * DespToFloat + 5.f));
        Camera camera;
        camera.SetProjectionMatrix(projection);
        camera.SetCameraTransformMatrix(transform);
        VisibilityResult visible;
        visible.m_mainCamera = &camera;
        visible.m_visibleInstances.m_opaqueInstances.push_back({cube.get(), ~uint32_t{0}});
        visible.m_visibleInstances.m_visibleTransparencyTypes = cube->GetUsedTransparencyMask();
        visible.m_ambientColor = {0, 0, 0};
        visible.m_ambientNrmlContribution = 0;
        visible.m_fogColor = {0, 0, 0};
        visible.m_fogFalloff = 0;
        visible.m_lightmap = AssetManager::Instance().GetPlainWhiteTexture();
        Target target(device, 320, 240);
        renderer.ReInitialize(target.colorView.Get(), target.depthView.Get(), target.viewport);
        RenderingSettings settings;
        ProfilingStats stats;
        const auto renderLight = [&]
        {
            visible.m_visibileLights.clear();
            for (const auto& source : scene.GetLightSources())
            {
                PackedLightSourceParams packed;
                if (source->FillLightParams(packed)) { visible.m_visibileLights.push_back(packed); }
            }
            const float black[]{0, 0, 0, 1};
            context->ClearRenderTargetView(target.colorView.Get(), black);
            context->ClearDepthStencilView(target.depthView.Get(), D3D11_CLEAR_DEPTH, 1.f, 0);
            renderer.RenderSceneElements(visible, settings, stats);
            return target.Pixels(device, context);
        };
        visible.m_ambientColor = {1,1,1};
        const auto receiverPixels = renderLight();
        Check(std::any_of(receiverPixels.begin(), receiverPixels.end(),
            [](auto pixel) { return (pixel & 0xffffff) != 0; }),
            "The GPU light-lifetime receiver must render visible geometry.");
        visible.m_ambientColor = {0,0,0};
        const auto dark = renderLight();
        ::RawGameObject raw{};
        Desperabis::EncodeGameObject(gold, raw.m_rawBytes, 54);
        scene.UpdateObjectPresentation(5, raw);
        scene.Update(0.1);
        const auto lit = renderLight();
        Check(lit != dark, "Original gold must illuminate actual pixels before pickup.");
        Desperabis::HandlePlayerObjectInteraction(core.worldObjects[5], core);
        Check(renderLight() == dark && !scene.GetObjectPresentation(5),
            "Picking up original gold must remove its GPU lighting on the very next frame.");
        ApplicationUi ui;
        class Input final : public Desperabis::InputHandler
        {
            bool IsKeyPressed(Desperabis::InputKey) override { return false; }
        } input;
        std::vector<std::pair<std::string, std::string>> synchronizationErrors;
        const auto reportSynchronizationError = [&](const char* operation, const char* message)
        {
            synchronizationErrors.emplace_back(operation, message);
        };
        Check(ApplicationUi::SynchronizeLevelGeometry(*scene.m_level, *core.level, reportSynchronizationError) &&
            synchronizationErrors.empty(), "Original final-level geometry must synchronize without errors.");
        const auto originalSurface = core.level->surfaceFlags[3][21];
        Check((originalSurface & 31) == 0, "The native flood regression must start on dry ground.");
        core.level->surfaceFlags[3][21] = 225;
        core.basicPlayerMovementEnabled = false;
        core.UpdateApplication(0, input);
        Check(Desperabis::HandlePlayerObjectInteraction(core.worldObjects[20], core).status ==
            Desperabis::InteractionStatus::Applied && core.worldObjects[18].generalPurpose2 == 3,
            "The original prison switch must activate its door before testing presentation fault isolation.");
        auto ripple = core.worldObjects[6];
        ripple.health = 40;
        ripple.behavior = Desperabis::BehaviorType::DriftingParticle;
        ripple.animFrame = 5;
        ripple.modelFrame2 = 4;
        ripple.displayType = Desperabis::DisplayType::VectorNormal;
        ripple.renderPlacementMode = 3;
        ripple.blendWeightOrSpriteOpacity = 80;
        ripple.posX = ripple.posZ = 150;
        ripple.posY = ripple.velocityY = ripple.velocityZ = 0;
        ripple.velocityX = 2;
        core.ReplaceWorldObject(100, ripple);
        auto doorInstance = std::make_shared<RenderableInstance>();
        auto rippleInstance = std::make_shared<RenderableInstance>();
        const auto sceneTimeBeforeFailure = scene.GetSceneTime();
        float previousDoorBlend = 0.f;
        for (int frame = 1; frame <= 3; ++frame)
        {
            const auto application = core.UpdateApplication(1.0 / 33, input);
            Check(application.tick && application.tick->status == Desperabis::GameTickStatus::Advanced &&
                application.worldDeltaSeconds > 0, "Flooding must not stop successful core ticks.");
            Check(ApplicationUi::SynchronizeLevelGeometry(*scene.m_level, *core.level, reportSynchronizationError) &&
                synchronizationErrors.empty() &&
                scene.m_level->GetCellArray().GetCell(21, 3).m_waterSlot == WaterSlotIndex::WaterSlot1,
                "The native flood transition must update water geometry without reporting a synchronization error.");
            for (const int slot : {18, 100})
            {
                raw.m_level = scene.m_level;
                Desperabis::EncodeGameObject(core.GetObjectRenderState(slot), raw.m_rawBytes, 54);
                scene.m_level->ApplyRenderProperties(slot == 18 ? doorInstance : rippleInstance, raw);
            }
            Check(doorInstance->m_blendWeight > previousDoorBlend &&
                std::abs(doorInstance->m_blendWeight -
                    core.worldObjects[18].blendWeightOrSpriteOpacity * 0.01f) < 0.00001f,
                "The switch-operated prison door must keep receiving advancing morphs while the level floods.");
            Check(core.worldObjects[100].posX == 150 + frame * 2 && core.worldObjects[100].health == 40 - frame &&
                rippleInstance->GetRenderable() &&
                std::abs(rippleInstance->m_blendWeight - (80 - frame * 5) * 0.01f) < 0.00001f,
                "Ripple movement, lifetime and presentation must continue while the level floods.");
            previousDoorBlend = doorInstance->m_blendWeight;
            const auto previousSceneTime = scene.GetSceneTime();
            scene.Update(application.worldDeltaSeconds);
            Check(scene.GetSceneTime() > previousSceneTime,
                "Flood synchronization must preserve the successful core tick's scene-time delta.");
        }
        Check(scene.GetSceneTime() > sceneTimeBeforeFailure,
            "Scene animations must advance during flood synchronization.");
        core.level->surfaceFlags[3][21] = originalSurface;
        Check(ApplicationUi::SynchronizeLevelGeometry(*scene.m_level, *core.level, reportSynchronizationError) &&
            synchronizationErrors.empty(), "Water removal must synchronize without errors.");
        std::cout << "Final-level flooding: water synchronization, switch-operated prison door, "
            "ripple movement/presentation and scene time advance without errors.\n";
        core.BeginBossFall(23, 46);
        const auto renderFall = [&](bool bossOnly = false)
        {
            auto view = core.GetBossFallView();
            if (bossOnly) { view.erase(view.begin(), view.begin() + 7); }
            ui.DrawBossFall(view, renderer, *scene.m_level, settings, stats,
                target.viewport, context, target.colorView.Get(), target.depthView.Get());
            return target.Pixels(device, context);
        };
        const auto start = renderFall();
        core.Tick({60,60}, input);
        const auto falling = renderFall();
        int changed = 0, pixels = 0;
        for (std::size_t pixel = 0; pixel < start.size(); ++pixel)
        {
            if (start[pixel] != falling[pixel]) { ++changed; }
            if ((falling[pixel] & 0xffffff) != 0) { ++pixels; }
        }
        Check(pixels > 200 && changed > 200,
            "The native staged assembly and lava shaft must visibly animate in the isolated GPU view.");
        const auto visibleBossPixels = [&]
        {
            const auto body = renderFall(true);
            return std::count_if(body.begin(), body.end(),
                [](auto pixel) { return (pixel & 0xffffff) != 0; });
        };
        const auto nearBossPixels = visibleBossPixels();
        core.Tick({60,60}, input);
        const auto farBossPixels = visibleBossPixels();
        Check(farBossPixels > 0 && farBossPixels < nearBossPixels,
            "The staged boss itself must remain visible and shrink as it falls away, independently of the shaft.");
        std::cout << "Final-level runtime: gold pickup clears next-frame GPU illumination; isolated boss fall has "
            << pixels << " visible pixels and " << changed << " changing pixels; boss shrinks from "
            << nearBossPixels << " to " << farBossPixels << " pixels.\n";
        core.scene = nullptr;
    }

    void CheckEndingActorVisibility()
    {
        static_assert(static_cast<int>(Desperabis::CollisionType::RenderSuppressed) == 110);
        static_assert(static_cast<int>(GamePlay::CollisionType::RenderSuppressed) == 110);
        Check(std::string(GamePlay::CollisionTypeToString(GamePlay::CollisionType::RenderSuppressed)) ==
            "RenderSuppressed", "Collision diagnostics must use the recovered render-suppression name.");
        auto* level = FORCE_GET(AssetManager::Instance().LoadLevel(96));
        const auto originals = level->GetRawGameObjects();
        const int templates[]{4, 6, 7, 8, 9, 10, 12, 13, 14, 15, 23};
        auto instance = std::make_shared<RenderableInstance>();
        auto actor = originals.at(11 - 2);
        level->ApplyRenderProperties(instance, actor);
        Check(instance->GetVisible() && instance->IsValid(),
            "The original ending's live magician must render before its first pose transition.");
        const auto* actorModel = instance->GetRenderable();
        for (const int slot : templates)
        {
            auto pose = originals.at(slot - 2);
            const auto originalPose = pose;
            const GamePlay::PropertyAccessorBase properties(pose);
            Check(properties.Isvalid() && properties.IsRenderSuppressed() && properties.GetHealth() == 1000,
                "Ending pose templates must remain active records despite native render suppression.");
            level->ApplyRenderProperties(instance, pose);
            Check(!instance->GetVisible() && !instance->IsValid() && pose == originalPose,
                "Switching a visible instance to a pose template must clear its mesh without changing gameplay data.");
            auto fresh = std::make_shared<RenderableInstance>();
            level->ApplyRenderProperties(fresh, pose);
            Check(!fresh->GetVisible() && !fresh->IsValid(),
                "Fresh pose-template instances must also remain hidden.");
            level->ApplyRenderProperties(instance, actor);
            Check(instance->GetVisible() && instance->GetRenderable() == actorModel,
                "Clearing render suppression must restore a reused actor instance.");
        }
        for (const byte collision : {byte{109}, byte{111}})
        {
            auto neighbor = actor;
            neighbor.m_rawBytes[6] = collision;
            level->ApplyRenderProperties(instance, neighbor);
            Check(instance->GetVisible() && instance->IsValid(),
                "Only collision 110, not neighboring collision types, must suppress rendering.");
        }
        auto sprite = originals.at(0);
        sprite.m_rawBytes[6] = 110;
        level->ApplyRenderProperties(instance, sprite, AssetManager::Instance().GetPlainWhiteTexture());
        Check(!instance->GetVisible() && !instance->IsValid(),
            "Native suppression must also apply to sprites, including explicit texture overrides.");

        LevelScene legacy(96);
        legacy.m_createMapIcons = false;
        FORCE_WAIT(legacy.CreateScene());
        int liveActors = 0;
        for (const auto& object : legacy.GetGameObjects())
        {
            const auto levelObject = std::dynamic_pointer_cast<LevelGameObject>(object);
            if (!levelObject) { continue; }
            const GamePlay::PropertyAccessorBase properties(levelObject->m_rawGO);
            Check(!properties.IsRenderSuppressed(),
                "Initial scene population must not replace suppressed templates with debug fallback sprites.");
            if (properties.GetBehaviorType() == GamePlay::BehaviorType::ObjectPoseSequence)
            {
                Check(levelObject->m_renderInst && levelObject->m_renderInst->GetVisible(),
                    "Initial scene population must retain the live pose-sequence actor.");
                ++liveActors;
            }
        }
        Check(liveActors == 1, "The original ending must populate exactly one live magician pose-sequence actor.");
        for (int slot = 16; slot <= 22; ++slot)
        {
            auto effect = originals.at(slot - 2);
            level->ApplyRenderProperties(instance, effect);
            Check(instance->GetVisible() && instance->IsValid(),
                "The magician's attachment and lightning-generator meshes must not be suppressed.");
        }
        auto emitter = actor;
        emitter.m_rawShorts[4] = static_cast<short>(GamePlay::BehaviorType::LightSource);
        const auto lightCount = legacy.GetLightSources().size();
        legacy.UpdateObjectPresentation(11, emitter);
        Check(legacy.GetObjectPresentation(11) && legacy.GetLightSources().size() == lightCount + 1,
            "An ordinary live emitter must own its presentation light.");
        emitter.m_rawBytes[6] = 110;
        legacy.UpdateObjectPresentation(11, emitter);
        Check(!legacy.GetObjectPresentation(11) && legacy.GetLightSources().size() == lightCount,
            "Render suppression must also remove an existing object's presentation extras.");
        legacy.UpdateObjectPresentation(11, emitter);
        Check(!legacy.GetObjectPresentation(11) && legacy.GetLightSources().size() == lightCount,
            "Suppressed source records must not create presentation extras on subsequent frames.");
        emitter.m_rawBytes[6] = 0;
        legacy.UpdateObjectPresentation(11, emitter);
        Check(legacy.GetObjectPresentation(11) && legacy.GetLightSources().size() == lightCount + 1,
            "Clearing render suppression must restore presentation extras.");
        legacy.RemoveObjectPresentation(11);

        Desperabis::GameContext core;
        for (std::size_t index = 0; index < originals.size(); ++index)
        {
            core.ReplaceWorldObject(static_cast<int>(index + 2),
                Desperabis::DecodeGameObject(originals[index].m_rawBytes, 54));
        }
        core.tickCount = 1;
        std::array<bool, 166> visited{};
        for (int tick = 0; tick < 1000; ++tick)
        {
            core.UpdateObjectPoseSequence(core.worldObjects[11]);
            const auto& live = core.worldObjects[11];
            Check(live.collisionType == Desperabis::CollisionType::None &&
                live.behavior == Desperabis::BehaviorType::ObjectPoseSequence,
                "Every copied ending pose must remain a drawable live actor, not a suppressed template.");
            visited.at(live.generalPurpose1) = true;
            Desperabis::EncodeGameObject(live, actor.m_rawBytes, 54);
            level->ApplyRenderProperties(instance, actor);
            Check(instance->GetVisible() && instance->IsValid(),
                "The live magician must remain visible throughout the original pose chain.");
        }
        for (const int slot : templates)
        {
            Check(visited.at(slot), "The original pose sequence must visit every ending template.");
            auto pose = originals.at(slot - 2);
            Desperabis::EncodeGameObject(core.worldObjects[slot], pose.m_rawBytes, 54);
            Check(pose == originals.at(slot - 2),
                "Rendering and pose playback must preserve each original source template.");
        }
        Check(level->GetRawGameObjects() == originals,
            "Ending presentation must not mutate cached authored records.");
        std::cout << "Ending visibility: 11 hidden templates, initial population, hide/show, sprite overrides, "
            "presentation lights and 1000 live pose-chain ticks passed.\n";
    }

    void CheckVillageCloudReveal(RendererDX11& renderer, ID3D11Device3* device, ID3D11DeviceContext3* context)
    {
        class Host final : public Desperabis::GameContext
        {
            std::vector<std::uint8_t> LoadFile(std::string_view filename) override
            {
                return FORCE_GET(FileManager::Instance().ReadDataFuture(FileManager::Instance().FilenameToNative(
                    std::wstring(filename.begin(), filename.end()))));
            }
            void LoadLevel(const Desperabis::LevelLoadRequest& request) override
            {
                Desperabis::LevelLoadData data;
                LoadLevelGeometry(request.levelNumber, data);
                LoadLevelObjects(request.levelNumber, data);
                InitializeLevelPlayerSpawn(data, {0,0,100});
                CompleteLevelLoad(request.id, std::move(data));
            }
        } core;
        class Input final : public Desperabis::InputHandler
        {
            bool IsKeyPressed(Desperabis::InputKey) override { return false; }
        } input;
        core.StartLevel(95);
        auto* level = FORCE_GET(AssetManager::Instance().LoadLevel(95));
        LevelScene scene(95);
        scene.m_createGameObjects = false;
        scene.m_createMapIcons = false;
        FORCE_WAIT(scene.CreateScene());
        std::vector<RenderableInstancePtr> stormSky;
        for (const auto& instance : scene.GetRenderableInstances())
        {
            if (instance->HasSkyMaterials()) { stormSky.push_back(instance); }
            else { instance->SetVisible(false); }
        }
        Check(!stormSky.empty(), "The village must initially provide its authored storm sky.");
        scene.SetSkyEnabled(true);
        Check(scene.IsSkyVisible() && stormSky.front()->GetVisible(),
            "The original storm sky must remain visible before the reveal cue.");
        std::array<RenderableInstancePtr, 3> instances;
        const int slots[]{2, 25, 26};
        for (auto& instance : instances) { instance = std::make_shared<RenderableInstance>(); }
        const auto synchronize = [&]
        {
            for (std::size_t index = 0; index < instances.size(); ++index)
            {
                RawGameObject raw{};
                raw.m_level = level;
                Desperabis::EncodeGameObject(core.GetObjectRenderState(slots[index]), raw.m_rawBytes, 54);
                level->ApplyRenderProperties(instances[index], raw);
                instances[index]->UpdateGlobalBounds();
                Check(instances[index]->GetVisible() && instances[index]->IsValid(),
                    "The native reveal must provide a visible sun and two drawable cloud actors.");
            }
        };
        for (int tick = 0; tick < 231; ++tick) { core.Tick({1,1}, input); }
        synchronize();
        const auto view = core.GetPlayerCamera();
        const auto position = Level::DespToPos(view.position[0], view.position[1], view.position[2]);
        Vector forward(-static_cast<float>(view.direction[0]),
            static_cast<float>(view.direction[1]) + (110.f - view.lookHeight), static_cast<float>(view.direction[2]), 0.f);
        forward.Normalize();
        Vector up(0,1,0,0);
        auto right = up.GetCross(forward);
        right.Normalize();
        up = forward.GetCross(right);
        Matrix4 transform, projection;
        transform.Create(position, right, up, forward);
        projection.CreateProjection(1.5f, 70.f * Math::Pi / 180.f, 0.05f, 100.f);
        Camera camera;
        camera.SetProjectionMatrix(projection);
        camera.SetCameraTransformMatrix(transform);
        Target target(device, 360, 240);
        renderer.ReInitialize(target.colorView.Get(), target.depthView.Get(), target.viewport);
        RenderingSettings settings;
        Check(!core.skyEnabled, "The native reveal must disable the authored storm sky.");
        scene.SetSkyEnabled(core.skyEnabled);
        settings.renderSky = scene.IsSkyVisible();
        Check(settings.renderSky && !stormSky.front()->GetVisible(),
            "The reveal cue must replace the storm sky, rather than disable all sky rendering.");
        VisibilitySettings visibilitySettings;
        DefaultVisibilityContext visibility;
        ProfilingStats stats;
        const auto render = [&](bool clouds)
        {
            instances[1]->SetVisible(clouds);
            instances[2]->SetVisible(clouds);
            VisibilityResult visible;
            visibility.ComputeResultFor(&camera, &scene, visible, settings, visibilitySettings, stats);
            Check(visible.HasResults() && !visible.m_visibleInstances.m_skyInstances.empty(),
                "The reveal sky must reach the renderer's sky visibility bucket.");
            visible.m_ambientColor = {1,1,1};
            visible.m_ambientNrmlContribution = 0;
            visible.m_fogColor = {0,0,0};
            visible.m_fogFalloff = 0;
            visible.m_lightmap = AssetManager::Instance().GetPlainWhiteTexture();
            const float black[]{0,0,0,1};
            context->ClearRenderTargetView(target.colorView.Get(), black);
            context->ClearDepthStencilView(target.depthView.Get(), D3D11_CLEAR_DEPTH, 1.f, 0);
            renderer.RenderSceneElements(visible, settings, stats);
            return target.Pixels(device, context);
        };
        constexpr std::uint32_t skyBlue = 0xebce87;
        const Vector directions[]{Vector(1,0,0,0), Vector(-1,0,0,0), Vector(0,1,0,0),
            Vector(0,-1,0,0), Vector(0,0,1,0), Vector(0,0,-1,0)};
        for (auto direction : directions)
        {
            Vector skyUp = std::abs(direction.GetY()) > 0.9f ? Vector(0,0,1,0) : Vector(0,1,0,0);
            auto skyRight = skyUp.GetCross(direction);
            skyUp = direction.GetCross(skyRight);
            Matrix4 skyTransform;
            skyTransform.Create(Vector(43,27,-19), skyRight, skyUp, direction);
            camera.SetCameraTransformMatrix(skyTransform);
            const auto background = render(false);
            Check(std::all_of(background.begin(), background.end(),
                [skyBlue](std::uint32_t pixel) { return (pixel & 0xffffff) == skyBlue; }),
                ("The procedural skybox must cover every pixel with constant blue in all six view directions; " +
                    std::to_string(direction.GetX()) + "," + std::to_string(direction.GetY()) + "," +
                    std::to_string(direction.GetZ()) + ", first pixel " + std::to_string(background.front() & 0xffffff) +
                    ", draw calls " + std::to_string(stats.m_numDrawCalls) +
                    ", triangles " + std::to_string(stats.m_numTriangles) +
                    ", blue pixels " + std::to_string(std::count_if(background.begin(), background.end(),
                        [skyBlue](std::uint32_t pixel) { return (pixel & 0xffffff) == skyBlue; }))).c_str());
        }
        scene.SetSkyEnabled(true);
        Check(stormSky.front()->GetVisible(), "Restoring the original sky must restore storm visibility.");
        scene.SetSkyEnabled(false);
        Check(!stormSky.front()->GetVisible(), "Returning to the reveal must hide the original storm again.");
        camera.SetCameraTransformMatrix(transform);
        for (const auto& instance : instances) { scene.AddRenderableInstance(instance); }
        const auto sun = render(false);
        const auto closed = render(true);
        int sunPixels = 0, closedSunPixels = 0;
        for (std::size_t pixel = 0; pixel < sun.size(); ++pixel)
        {
            if ((sun[pixel] & 0xffffff) == skyBlue) { continue; }
            ++sunPixels;
            if (sun[pixel] == closed[pixel]) { ++closedSunPixels; }
        }
        Check(sunPixels > 200, "The original sun must project visible pixels from the selected native camera.");
        for (int tick = 231; tick < 629; ++tick) { core.Tick({1,1}, input); }
        synchronize();
        const auto open = render(true);
        int openSunPixels = 0;
        for (std::size_t pixel = 0; pixel < sun.size(); ++pixel)
        {
            if ((sun[pixel] & 0xffffff) != skyBlue && sun[pixel] == open[pixel]) { ++openSunPixels; }
        }
        std::cout << "Village reveal: " << sunPixels << " sun pixels, " << closedSunPixels <<
            " visible with closed clouds, " << openSunPixels << " after opening.\n";
        Check(openSunPixels > closedSunPixels + 200 && openSunPixels > sunPixels / 2,
            "Opening the original cloud actors must visibly uncover the sun from the native reveal camera.");
        scene.ClearScene();
        Check(scene.GetRenderableInstances().empty() && scene.IsSkyVisible(),
            "Clearing the scene must release the reveal sky and reset its presentation state.");
        LevelScene ordinaryScene(26);
        ordinaryScene.SetSkyEnabled(false);
        Check(!ordinaryScene.IsSkyVisible() && ordinaryScene.GetRenderableInstances().empty(),
            "Disabling sky outside the epilogue must not introduce a blue background over the dark clear color.");
    }

    void CheckFinalLevelMorphs(RendererDX11& renderer, ID3D11Device3* device, ID3D11DeviceContext3* context)
    {
        auto* level = FORCE_GET(AssetManager::Instance().LoadLevel(26));
        const auto records = FORCE_GET(FileManager::Instance().ReadDataFuture(
            FileManager::Instance().FilenameToNative(L"LEVEL\\OBJPOS26.DAT")));
        const auto geometryBytes = FORCE_GET(FileManager::Instance().ReadDataFuture(
            FileManager::Instance().FilenameToNative(L"LEVEL\\FELD26.256")));
        Desperabis::Level floodedGeometry;
        floodedGeometry.LoadGeometry(geometryBytes.data(), geometryBytes.size());
        floodedGeometry.surfaceFlags[3][21] = 225;
        LevelScene scene(26);
        scene.m_createGameObjects = false;
        FORCE_WAIT(scene.CreateScene());
        ::RawGameObject head{};
        head.m_level = level;
        std::memcpy(head.m_rawBytes, records.data() + (28 - 2) * 54, 54);
        head.m_rawBytes[3] = 4;
        auto headInstance = std::make_shared<RenderableInstance>();
        level->ApplyRenderProperties(headInstance, head);
        const auto headMaterials = headInstance->GetOrCreateMaterialOverrides();
        scene.AddRenderableInstance(headInstance);
        scene.Update(0.03);
        for (std::size_t material = 0; material < headMaterials.size(); ++material)
        {
            Check(headInstance->m_materialOverride.at(material).m_texture == headMaterials[material].m_texture,
                "Scene animation binding must preserve the boss head's owned billboard subtextures.");
        }
        Texture lightmap;
        lightmap.CreatePlainWhite();
        lightmap.m_atlasData = { 0, 0, 0.5f, 0.5f };
        Target target(device, 360, 240);
        renderer.ReInitialize(target.colorView.Get(), target.depthView.Get(), target.viewport);
        RenderingSettings settings;
        ProfilingStats stats;
        head.m_rawShorts[6] = head.m_rawShorts[7] = head.m_rawShorts[8] = 0;
        head.m_rawShorts[14] = head.m_rawShorts[15] = head.m_rawShorts[16] = 0;
        const auto headAnimation = level->GetLevelInfo().GetModelAnim(5);
        const auto* headMesh = headAnimation->GetModel(3, 2)->GetNativeRuntimeObj<NativeModelMeshObjectDX11>();
        const auto headVertices = ReadBuffer<ModelVertex>(device, context, headMesh->m_vertexBuffer.Get());
        const auto headIndices = ReadBuffer<uint16_t>(device, context, headMesh->m_indexBuffer.Get());
        int atlasToSpriteChanges = 0;
        for (const int frame : {3, 3, 4, 4, 3})
        {
            const auto previousMaterials = headInstance->GetOrCreateMaterialOverrides();
            head.m_rawBytes[3] = static_cast<byte>(frame);
            level->ApplyRenderProperties(headInstance, head);
            scene.AssignFireAnimations(*headInstance);
            scene.Update(0.03);
            auto freshHead = std::make_shared<RenderableInstance>();
            level->ApplyRenderProperties(freshHead, head);
            freshHead->UpdateGlobalBounds();
            const auto center = freshHead->GetGlobalBounds().GetCenter();
            const float span = (std::max)(freshHead->GetGlobalBounds().GetSphereRadius() * 3.f, 0.2f);
            Matrix4 projection, transform;
            projection.CreateOrthographic(span * 1.5f, span, 0.1f, 20.f);
            transform.SetTranslation(Vector(center.GetX(), center.GetY(), center.GetZ() + 10.f));
            Camera camera;
            camera.SetProjectionMatrix(projection);
            camera.SetCameraTransformMatrix(transform);
            VisibilityResult visible;
            visible.m_mainCamera = &camera;
            visible.m_ambientColor = {1, 1, 1};
            visible.m_ambientNrmlContribution = 0;
            visible.m_fogColor = {0, 0, 0};
            visible.m_fogFalloff = 0;
            visible.m_lightmap = &lightmap;
            const auto renderHead = [&](RenderableInstance* instance)
            {
                visible.m_visibleInstances.m_opaqueInstances = {{instance, ~uint32_t{0}}};
                visible.m_visibleInstances.m_visibleTransparencyTypes = instance->GetUsedTransparencyMask();
                const float black[]{0, 0, 0, 1};
                context->ClearRenderTargetView(target.colorView.Get(), black);
                context->ClearDepthStencilView(target.depthView.Get(), D3D11_CLEAR_DEPTH, 1.f, 0);
                renderer.RenderSceneElements(visible, settings, stats);
                return target.Pixels(device, context);
            };
            const auto expected = renderHead(freshHead.get());
            const auto& drawCalls = headInstance->GetRenderable()->m_drawCalls;
            int spriteTriangles = 0;
            for (const auto& drawCall : drawCalls)
            {
                for (int triangle = 0; triangle < drawCall.m_primitiveCount; ++triangle)
                {
                    if (headVertices[headIndices[drawCall.m_firstIndex + triangle * 3]].StretchMode ==
                        static_cast<uint16_t>(VertexStretchMode::Sprite))
                    {
                        ++spriteTriangles;
                        Check(drawCall.m_material.m_texture != &headAnimation->m_textureAtlas,
                            "Boss-head sprites must bind owned subtextures, not the complete model atlas.");
                        const auto material = static_cast<std::size_t>(&drawCall - drawCalls.data());
                        if (material < previousMaterials.size() &&
                            previousMaterials[material].m_texture == &headAnimation->m_textureAtlas)
                        {
                            ++atlasToSpriteChanges;
                        }
                    }
                }
            }
            std::cout << "Final-level boss head frame " << frame << ": " << spriteTriangles << " sprite triangles.\n";
            Check(std::any_of(expected.begin(), expected.end(),
                [](auto pixel) { return (pixel & 0xffffff) != 0; }),
                "Both original boss-head animation frames must render visible reference pixels.");
            Check(renderHead(headInstance.get()) == expected,
                "Switching boss-head frames must render exactly like a fresh instance, without stale atlas materials.");
            const auto& materials = headInstance->GetOrCreateMaterialOverrides();
            for (std::size_t material = 0; material < drawCalls.size(); ++material)
            {
                Check(materials.at(material).m_texture == drawCalls[material].m_material.m_texture,
                    "Switching boss-head frames must replace old atlas/subtexture material overrides.");
            }
        }
        Check(atlasToSpriteChanges > 0,
            "Original boss-head transitions must exercise an old atlas-material slot becoming a sprite subtexture.");
        headInstance->GetOrCreateMaterialOverrides().front().m_depthOffset = 0.125f;
        level->ApplyRenderProperties(headInstance, head);
        Check(headInstance->GetMaterialOverrides().front().m_depthOffset == 0.125f,
            "Updates within the same model frame must preserve instance-specific material overrides.");
        std::cout << "Final-level boss head: repeated frames 4/3 match fresh-instance GPU pixels and materials.\n";
        for (const int slot : {9, 18, 6})
        {
            ::RawGameObject raw{};
            raw.m_level = level;
            std::memcpy(raw.m_rawBytes, records.data() + (slot - 2) * 54, 54);
            if (slot == 6)
            {
                raw.m_rawBytes[3] = 5;
                raw.m_rawBytes[2] = 4;
            }
            raw.m_rawShorts[6] = raw.m_rawShorts[7] = raw.m_rawShorts[8] = 0;
            raw.m_rawShorts[14] = raw.m_rawShorts[15] = raw.m_rawShorts[16] = 0;
            raw.m_rawShorts[21] = 0;
            auto instance = std::make_shared<RenderableInstance>();
            level->ApplyRenderProperties(instance, raw);
            Check(instance->GetRenderable() && instance->GetBlendTargetRenderable(),
                "Original dam, prison door and flood surface must resolve both morph frames.");
            instance->UpdateGlobalBounds();
            const auto center = instance->GetGlobalBounds().GetCenter();
            const float span = (std::max)(instance->GetGlobalBounds().GetSphereRadius() * 3.f, 0.2f);
            Matrix4 projection, transform;
            projection.CreateOrthographic(span * 1.5f, span, 0.1f, 20.f);
            transform.SetTranslation(Vector(center.GetX(), center.GetY(), center.GetZ() + 10.f));
            Camera camera;
            camera.SetProjectionMatrix(projection);
            camera.SetCameraTransformMatrix(transform);
            VisibilityResult visible;
            visible.m_mainCamera = &camera;
            visible.m_visibleInstances.m_opaqueInstances.push_back({instance.get(), ~uint32_t{0}});
            visible.m_visibleInstances.m_visibleTransparencyTypes = instance->GetUsedTransparencyMask();
            visible.m_ambientColor = {1.f, 1.f, 1.f};
            visible.m_fogColor = {0.f, 0.f, 0.f};
            visible.m_fogFalloff = 0.f;
            visible.m_lightmap = &lightmap;
            const auto render = [&](short blend)
            {
                Check(ApplicationUi::SynchronizeLevelGeometry(*level, floodedGeometry,
                    [](const char* operation, const char* message)
                    {
                        throw std::runtime_error(std::string(operation) + ": " + message);
                    }), "Water synchronization must succeed before GPU morph updates.");
                raw.m_rawShorts[21] = blend;
                level->ApplyRenderProperties(instance, raw);
                Check(std::abs(instance->m_blendWeight - blend * 0.01f) < 0.00001f,
                    "Final-level native morph weights must reach their render instances.");
                const float black[]{0, 0, 0, 1};
                context->ClearRenderTargetView(target.colorView.Get(), black);
                context->ClearDepthStencilView(target.depthView.Get(), D3D11_CLEAR_DEPTH, 1.f, 0);
                renderer.RenderSceneElements(visible, settings, stats);
                return target.Pixels(device, context);
            };
            const auto closed = render(0);
            const auto half = render(50);
            const auto open = render(100);
            int halfwayChanges = 0, endpointChanges = 0, visiblePixels = 0;
            for (std::size_t pixel = 0; pixel < closed.size(); ++pixel)
            {
                if ((closed[pixel] & 0xffffff) != 0) { ++visiblePixels; }
                if (closed[pixel] != half[pixel]) { ++halfwayChanges; }
                if (closed[pixel] != open[pixel]) { ++endpointChanges; }
            }
            std::cout << "Final-level slot " << slot << ": " << visiblePixels
                << " visible pixels, " << halfwayChanges << " halfway changes, "
                << endpointChanges << " endpoint changes.\n";
            Check(visiblePixels > 20 && halfwayChanges > 20 && endpointChanges > 20,
                "Original final-level morphs must visibly animate on the GPU, not just update CPU properties.");
        }
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

    void CheckMutableWalls(RendererDX11& renderer, ID3D11Device3* device, ID3D11DeviceContext3* context)
    {
        class WallLevel final : public Level
        {
        public:
            void Build(const LevelInfo& info, const Desperabis::Level& geometry, const std::vector<byte>& objects)
            {
                InitLevelInfo(info);
                std::vector<byte> planes;
                for (const auto* plane : { &geometry.wallTextures[0][0], &geometry.floorObjects[0][0],
                    &geometry.ceilingObjects[0][0], &geometry.floorHeights[0][0], &geometry.roomHeights[0][0],
                    &geometry.surfaceFlags[0][0], &geometry.collisionMaterials[0][0] })
                {
                    planes.insert(planes.end(), plane, plane + 50 * 40);
                }
                // Savegames store headerless 50x40 planes and the current object records.
                ReadLevelData(planes.data(), static_cast<int>(planes.size()), false);
                ReadGameObjects(objects.data(), static_cast<int>(objects.size()));
                CreateWallGeometry();
                for (const auto& instance : m_wallGeometry) { m_scene.AddRenderableInstance(instance); }
            }
            auto Walls() const { return PopulatedGeometry(m_wallGeometry); }
            const auto& GeneratedWalls() const { return m_wallRenderables; }
            const auto& MutableWalls() const { return m_mutableWalls; }
            const auto& MutableFloors() const { return m_mutableFloors; }
            const auto& FloorModels() const { return m_floorModels; }
            const auto& WaterSurfaces() const { return m_waterSurfaces; }
            RenderableInstancePtr InvalidStaticTile() { return AddStaticCellObject(1, 1, 63, HeightLevel::Floor); }
            Texture& Lightmap() { CreateLightmap(); return *m_lightmap; }
            const Scene& WallScene() const { return m_scene; }
            RenderableInstancePtr UpdateFront(const Desperabis::RawGameObject& object)
            {
                if (!m_front)
                {
                    FORCE_WAIT(m_scene.CreateScene());
                    m_front = std::make_shared<RenderableInstance>();
                    m_scene.AddRenderableInstance(m_front);
                }
                RawGameObject raw{};
                raw.m_level = this;
                Desperabis::EncodeGameObject(object, raw.m_rawBytes, sizeof(raw.m_rawBytes));
                ApplyRenderProperties(m_front, raw);
                m_front->SetVisible(object.health > 0);
                return m_front;
            }
            void ReadGrid(const LevelInfo& info, const std::vector<byte>& geometry, bool readHeader)
            {
                InitLevelInfo(info);
                ReadLevelData(geometry.data(), static_cast<int>(geometry.size()), readHeader);
            }
            void BuildOriginal(const LevelInfo& info, const std::vector<byte>& geometry,
                const std::vector<byte>& objects = {})
            {
                ReadGrid(info, geometry, true);
                if (!objects.empty()) { ReadGameObjects(objects.data(), static_cast<int>(objects.size())); }
                CreateWallGeometry();
                for (const auto& instance : m_wallGeometry) { m_scene.AddRenderableInstance(instance); }
            }
        private:
            FlameScene m_scene;
            RenderableInstancePtr m_front;
        };
        class WallVisibility final : public DefaultVisibilityContext
        {
        public:
            void Collect(VisibilityResult& result, const Scene& scene, const Vector& position)
            {
                m_performFrustumClipping = false;
                VisibilitySettings settings;
                settings.filterMask = static_cast<uint32_t>(GamePlay::VisibilityFilter::Walls);
                Frustum frustum;
                DetermineVisibleInstances(result, result.m_visibleInstances, &scene,
                    position, frustum, nullptr, settings, true);
            }
        } visibility;
        auto* assets = FORCE_GET(AssetManager::Instance().LoadLevel(11));
        const auto read = [](std::wstring_view filename)
        {
            return FORCE_GET(FileManager::Instance().ReadDataFuture(
                FileManager::Instance().FilenameToNative(filename)));
        };
        int promotedCaveCorners = 0;
        for (const int number : {17,18})
        {
            auto* caveAssets = FORCE_GET(AssetManager::Instance().LoadLevel(number));
            WallLevel cave;
            cave.BuildOriginal(caveAssets->GetLevelInfo(), read(L"LEVEL\\FELD" + std::to_wstring(number) + L".256"));
            std::vector<ID3D11Buffer*> inspected;
            int changed = 0, wallCorners = 0;
            for (const auto& renderable : cave.GeneratedWalls())
            {
                const auto* mesh = renderable->GetNativeRuntimeObj<NativeModelMeshObjectDX11>();
                auto* buffer = mesh->m_vertexBuffer.Get();
                if (std::find(inspected.begin(), inspected.end(), buffer) != inspected.end()) { continue; }
                inspected.push_back(buffer);
                const auto vertices = ReadBuffer<ModelVertex>(device, context, buffer);
                const auto positions = ReadBuffer<VertexPosition>(device, context, mesh->m_positionBuffer.Get());
                const auto indices = ReadBuffer<uint16_t>(device, context, mesh->m_indexBuffer.Get());
                for (const auto& vertex : vertices)
                {
                    if (vertex.StretchMode != (uint16_t)VertexStretchMode::None) { continue; }
                    const Vector normal(vertex.Normal.x, vertex.Normal.y, vertex.Normal.z);
                    const float length = normal.GetLength();
                    Check(std::isfinite(length) && (length < 0.00001f || std::abs(length - 1.f) < 0.0001f),
                        "Original cave geometry buffers must contain finite, normalized normals.");
                }
                for (std::size_t i = 0; i < indices.size(); i += 3)
                {
                    Vector triangle[3];
                    for (int corner = 0; corner < 3; ++corner)
                    {
                        const auto& p = positions[vertices[indices[i + corner]].PositionIndex];
                        triangle[corner].Set(p.x, p.y, p.z, 0.f);
                    }
                    auto face = (triangle[1] - triangle[0]).GetCross(triangle[2] - triangle[0]);
                    face.Normalize();
                    for (int corner = 0; corner < 3; ++corner)
                    {
                        const auto& vertex = vertices[indices[i + corner]];
                        const auto& n = vertex.Normal;
                        const float difference = (face - Vector(n.x, n.y, n.z)).GetLength();
                        if (vertex.NormalGroup == 0)
                        {
                            ++wallCorners;
                            Check(difference < 0.00001f,
                                "Every generated wall corner must retain its triangle's face normal, regardless of angle.");
                        }
                        else { changed += difference > 0.01f; }
                    }
                }
            }
            Check(wallCorners > 0 && changed > 0,
                "Original caves must have flat generated walls alongside genuinely smoothed heightmap surfaces.");
            std::cout << "Cave " << number << ": " << wallCorners << " flat wall corners, "
                << changed << " smoothed heightmap corners.\n";
            int promoted = 0;
            bool hasModelBank = false;
            for (const int bank : {2,3})
            {
                const auto animation = caveAssets->GetLevelInfo().GetModelAnim(bank);
                if (animation == nullptr) { continue; }
                hasModelBank = true;
                for (int row = 0; row < animation->m_numAnims; ++row)
                {
                    const auto* model = animation->GetModel(row, 0);
                    if (!model->HasOwnDrawcalls()) { continue; }
                    promoted += CountPromotedModelCorners(*model, device, context);
                }
            }
            Check(hasModelBank, "Original caves must provide at least one floor/ceiling model bank.");
            promotedCaveCorners += promoted;
            std::cout << "Cave " << number << ": " << promoted << " original model corners promoted beyond per-edge shading.\n";
        }
        Check(promotedCaveCorners > 0, "Connected-material promotion must affect the real cave floor/ceiling models.");
        const auto feld = read(L"LEVEL\\FELD11.256");
        auto objects = read(L"LEVEL\\OBJPOS11.DAT");
        auto controls = objects;
        auto badMarker = Desperabis::DecodeGameObject(controls.data() + (49 - 2) * 54, 54);
        badMarker.posX = -100;
        Desperabis::EncodeGameObject(badMarker, controls.data(), 54);
        WallLevel invalidControl;
        CheckRecoverableError<std::runtime_error>([&]
        {
            invalidControl.BuildOriginal(assets->GetLevelInfo(), feld, controls);
        });
#ifdef NDEBUG
        Check(invalidControl.MutableWalls().size() == 2,
            "Release must skip a bad wall marker while still preparing both subsequent valid wall triggers.");
#endif
        LevelInfo missingFrameInfo = assets->GetLevelInfo();
        missingFrameInfo.m_loadedModel[2] = std::make_shared<ModelAnim>();
        WallLevel missingFrame;
        missingFrame.ReadGrid(missingFrameInfo, feld, true);
        RenderableInstancePtr missingTile;
        CheckRecoverableError<std::runtime_error>([&] { missingTile = missingFrame.InvalidStaticTile(); });
        Check(!missingTile && missingFrame.FloorModels().empty(),
            "An unavailable optional static tile must not publish a render instance or floor presentation.");
        Desperabis::Level geometry;
        geometry.LoadGeometry(feld.data(), feld.size());
        geometry.BuildCollisionMap();
        Desperabis::GameContext core;
        core.level = &geometry;
        core.collisionMap = geometry.collisionMap.get();
        for (std::size_t offset = 0; offset < objects.size(); offset += 54)
        {
            core.worldObjects[offset / 54 + 2] = Desperabis::DecodeGameObject(objects.data() + offset, 54);
        }
        WallLevel level;
        level.Build(assets->GetLevelInfo(), geometry, objects);
        Check(level.MutableWalls().size() == 2 &&
            level.MutableWalls()[0].column == 19 && level.MutableWalls()[0].row == 8 &&
            level.MutableWalls()[1].column == 19 && level.MutableWalls()[1].row == 9,
            "Original level 11 must isolate exactly its two authored wall-destruction tiles.");
        const auto synchronize = [&]
        {
            return level.SynchronizeWallTiles(&geometry.wallTextures[0][0], 50, 40, 50);
        };
        Check(synchronize() == 0, "Intact wall synchronization must not change any render instance.");
        const auto originalInstances = level.Walls();
        std::vector<std::shared_ptr<IGraphicsRuntimeObject>> runtimes;
        std::size_t triangles = 0, removableTriangles = 0;
        for (const auto& instance : originalInstances)
        {
            const auto* renderable = instance->GetRenderable();
            runtimes.push_back(renderable->m_runtime);
            triangles += renderable->m_drawCalls[0].m_primitiveCount;
        }
        for (const auto& wall : level.MutableWalls())
        {
            removableTriangles += wall.instance->GetRenderable()->m_drawCalls[0].m_primitiveCount;
            Check(std::count_if(originalInstances.begin(), originalInstances.end(), [&](const auto& other)
                { return other != wall.instance &&
                    other->GetRenderable()->m_runtime == wall.instance->GetRenderable()->m_runtime; }) > 0,
                "Mutable wall ranges must share existing island GPU buffers with the static batches.");
        }
        auto staticObjects = objects;
        for (std::size_t offset = 0; offset < staticObjects.size(); offset += 54)
        {
            if (staticObjects[offset + 6] == 8) { staticObjects[offset + 6] = 0; }
        }
        WallLevel unsplit;
        unsplit.Build(assets->GetLevelInfo(), geometry, staticObjects);
        std::size_t unsplitTriangles = 0;
        for (const auto& instance : unsplit.Walls())
        {
            unsplitTriangles += instance->GetRenderable()->m_drawCalls[0].m_primitiveCount;
        }
        Check(unsplit.MutableWalls().empty() && triangles == unsplitTriangles,
            "Extraction must neither duplicate nor omit any wall/floor/ceiling triangles.");

        Target target(device, 360, 240);
        renderer.ReInitialize(target.colorView.Get(), target.depthView.Get(), target.viewport);
        Texture lightmap;
        lightmap.CreatePlainWhite();
        lightmap.m_atlasData = { 0,0,0.5f,0.5f };
        RenderingSettings settings;
        ProfilingStats stats;
        const Vector center((20 - Level::tileCenterX) * 100.f * DespToFloat,
            (geometry.roomHeights[8][19] - 2 * geometry.floorHeights[8][19]) * DespToFloat,
            (8.5f - Level::tileCenterY) * 100.f * DespToFloat);
        const auto render = [&](const WallLevel& drawn, int direction, const Vector& viewCenter)
        {
            Camera camera;
            Matrix4 projection, transform;
            projection.CreateOrthographic(150.f * DespToFloat, 100.f * DespToFloat, 0.01f, 30.f);
            const float yaw = direction * static_cast<float>(Math::Pi * 0.5);
            const float distance = 40.f * DespToFloat;
            transform.Create(Vector(viewCenter.GetX() + std::sin(yaw) * distance, viewCenter.GetY(),
                viewCenter.GetZ() + std::cos(yaw) * distance), Vector(yaw, 0, 0));
            camera.SetProjectionMatrix(projection);
            camera.SetCameraTransformMatrix(transform);
            VisibilityResult visible;
            visible.m_mainCamera = &camera;
            visible.m_lightmap = &lightmap;
            visible.m_ambientColor = { 1.f,1.f,1.f };
            visible.m_fogColor = { 0,0,0 };
            visible.m_fogFalloff = 0;
            visibility.Collect(visible, drawn.WallScene(), camera.GetCameraPosition());
            const float black[]{ 0,0,0,1 };
            context->ClearRenderTargetView(target.colorView.Get(), black);
            context->ClearDepthStencilView(target.depthView.Get(), D3D11_CLEAR_DEPTH, 1.f, 0);
            renderer.RenderSceneElements(visible, settings, stats);
            return target.Pixels(device, context);
        };
        std::array<std::vector<std::uint32_t>, 4> before;
        for (int direction = 0; direction < 4; ++direction) { before[direction] = render(level, direction, center); }
        core.DamageObject(core.worldObjects[49], 0);
        Check(synchronize() == 1 && !level.MutableWalls()[0].instance->GetVisible() &&
            level.MutableWalls()[1].instance->GetVisible() && synchronize() == 0,
            "Core destruction must hide only its tile and remain idempotent after object coordinates change.");
        core.DamageObject(core.worldObjects[50], 0);
        Check(synchronize() == 1 && !level.MutableWalls()[1].instance->GetVisible(),
            "The second trigger must independently remove the neighbouring wall.");
        for (std::size_t i = 0; i < originalInstances.size(); ++i)
        {
            Check(level.Walls()[i] == originalInstances[i] &&
                level.Walls()[i]->GetRenderable()->m_runtime == runtimes[i],
                "Destruction must not recreate instances or upload/rebuild the level GPU meshes.");
            const bool mutableWall = std::any_of(level.MutableWalls().begin(), level.MutableWalls().end(),
                [&](const auto& wall) { return wall.instance == originalInstances[i]; });
            Check(originalInstances[i]->GetVisible() != mutableWall,
                "Static walls, floors and ceilings must remain visible after destruction.");
        }
        for (std::size_t offset = 0; offset < objects.size(); offset += 54)
        {
            Desperabis::EncodeGameObject(core.worldObjects[offset / 54 + 2], objects.data() + offset, 54);
        }
        WallLevel restored;
        restored.Build(assets->GetLevelInfo(), geometry, objects);
        std::size_t restoredTriangles = 0;
        for (const auto& instance : restored.Walls())
        {
            restoredTriangles += instance->GetRenderable()->m_drawCalls[0].m_primitiveCount;
        }
        Check(restored.MutableWalls().empty() && restoredTriangles + removableTriangles == triangles,
            "Saved cleared tiles and dead/repositioned triggers must never resurrect removed wall faces.");
        const auto& invalidWall = level.MutableWalls().front();
        const auto originalWall = geometry.wallTextures[invalidWall.row][invalidWall.column];
        const auto originalBrightness = geometry.surfaceFlags[0][0];
        geometry.wallTextures[invalidWall.row][invalidWall.column] = static_cast<byte>(invalidWall.originalWall ^ 16);
        geometry.surfaceFlags[0][0] ^= 0xe0;
        std::vector<std::pair<std::string, std::string>> geometryErrors;
        const auto reportGeometryError = [&](const char* operation, const char* message)
        {
            geometryErrors.emplace_back(operation, message);
        };
        Check(!ApplicationUi::SynchronizeLevelGeometry(level, geometry, reportGeometryError) &&
            geometryErrors.size() == 1 && geometryErrors[0].first == "wall synchronization failed" &&
            !geometryErrors[0].second.empty() &&
            level.GetCellArray().GetCell(0, 0).m_walls == geometry.surfaceFlags[0][0],
            "A genuinely unsupported wall replacement must be reported without preventing later surface synchronization.");
        geometry.wallTextures[invalidWall.row][invalidWall.column] = originalWall;
        geometry.surfaceFlags[0][0] = originalBrightness;
        Check(ApplicationUi::SynchronizeLevelGeometry(level, geometry, reportGeometryError) && geometryErrors.size() == 1,
            "Restored geometry must synchronize normally after an independently reported wall failure.");
        int changedPixels = 0;
        for (int direction = 0; direction < 4; ++direction)
        {
            const auto after = render(level, direction, center);
            const auto saved = render(restored, direction, center);
            Check(after == saved, "A fresh saved-geometry build must render identically to runtime wall removal.");
            for (std::size_t i = 0; i < after.size(); ++i) { changedPixels += before[direction][i] != after[i]; }
        }
        Check(changedPixels > 100, "The original secret-room walls must visibly disappear from the rendered scene.");
        Desperabis::Level isolated;
        for (auto& row : isolated.roomHeights) { std::fill(std::begin(row), std::end(row), byte(50)); }
        auto trigger = core.worldObjects[49];
        trigger.health = 1000;
        trigger.posX = 300;
        trigger.posZ = 3800;
        std::vector<byte> duplicateTriggers(108);
        Desperabis::EncodeGameObject(trigger, duplicateTriggers.data(), 54);
        Desperabis::EncodeGameObject(trigger, duplicateTriggers.data() + 54, 54);
        for (int shape = 0; shape <= 6; ++shape)
        {
            const auto tile = static_cast<byte>((shape << 5) | 2);
            isolated.wallTextures[2][2] = tile;
            WallLevel onlyWall;
            onlyWall.Build(assets->GetLevelInfo(), isolated, duplicateTriggers);
            Check(onlyWall.Walls().size() == 1 && onlyWall.MutableWalls().size() == 1 &&
                onlyWall.Walls()[0]->GetRenderable()->m_drawCalls[0].m_primitiveCount ==
                    (shape < 4 ? 2 : shape < 6 ? 4 : 8),
                "Duplicate triggers and all wall shapes must produce one complete range, even without static batches.");
            std::vector<byte> stridedTiles(40 * 53, 0);
            Check(onlyWall.SynchronizeWallTiles(stridedTiles.data(), 50, 40, 53) == 1 &&
                !onlyWall.Walls()[0]->GetVisible() &&
                onlyWall.GetCellArray().GetCell(2, 2).m_wallTextures == 0,
                "Headerless saved planes and padded core strides must hide every face of a mutable-only island.");
            stridedTiles[2 * 53 + 2] = tile;
            Check(onlyWall.SynchronizeWallTiles(stridedTiles.data(), 50, 40, 53) == 1 &&
                onlyWall.Walls()[0]->GetVisible(),
                "Restoring the original tile must restore its prebuilt range without rebuilding.");
        }
        const auto tutorial = read(L"LEVEL\\FELD50.256");
        Desperabis::Level tutorialGeometry;
        tutorialGeometry.LoadGeometry(tutorial.data(), tutorial.size());
        Check(tutorialGeometry.geometryWidth == 36 && tutorialGeometry.geometryHeight == 31 &&
            tutorialGeometry.wallTextures[30][9] == 49,
            "The original tutorial must retain its inward-facing wall on the final row.");
        auto boundaryWall = tutorial;
        constexpr int tutorialPlaneSize = 36 * 31;
        for (const int plane : { 0, 1, 2, 5, 6 })
        {
            std::fill_n(boundaryWall.begin() + 4 + plane * tutorialPlaneSize, tutorialPlaneSize, byte(0));
        }
        boundaryWall[4 + 30 * 36 + 9] = tutorialGeometry.wallTextures[30][9];
        WallLevel tutorialWall;
        tutorialWall.BuildOriginal(assets->GetLevelInfo(), boundaryWall);
        Check(tutorialWall.Walls().size() == 1 &&
            tutorialWall.Walls()[0]->GetRenderable()->m_drawCalls[0].m_primitiveCount == 2,
            "The tutorial's last-row inward-facing wall must generate its complete quad.");
        const auto& tutorialBounds = tutorialWall.Walls()[0]->GetGlobalBounds().m_box;
        const auto& tutorialGrid = tutorialWall.GetCellArray();
        Check(std::abs(tutorialBounds.m_min.GetX() - tutorialGrid.GridPosX(9)) < 0.0001f &&
            std::abs(tutorialBounds.m_max.GetX() - tutorialGrid.GridPosX(10)) < 0.0001f &&
            std::abs(tutorialBounds.m_min.GetZ() - tutorialGrid.GridPosY(30)) < 0.0001f &&
            std::abs(tutorialBounds.m_max.GetZ() - tutorialGrid.GridPosY(30)) < 0.0001f,
            "The restored tutorial wall must lie on its original final-row segment, not a shifted or duplicated edge.");
        const Vector tutorialCenter((tutorialBounds.m_min.GetX() + tutorialBounds.m_max.GetX()) * 0.5f,
            (tutorialBounds.m_min.GetY() + tutorialBounds.m_max.GetY()) * 0.5f, tutorialGrid.GridPosY(30));
        int tutorialPixels[2]{};
        for (int side = 0; side < 2; ++side)
        {
            const auto pixels = render(tutorialWall, side * 2, tutorialCenter);
            tutorialPixels[side] = static_cast<int>(std::count_if(pixels.begin(), pixels.end(),
                [](std::uint32_t pixel) { return (pixel & 0x00ffffff) != 0; }));
        }
        Check((std::max)(tutorialPixels[0], tutorialPixels[1]) > 100 &&
            (std::min)(tutorialPixels[0], tutorialPixels[1]) == 0,
            "The original tutorial boundary wall must render visible pixels while retaining one-sided face culling.");
        std::vector<byte> boundaryGeometry(4 + 7 * 9, 0);
        boundaryGeometry[0] = boundaryGeometry[2] = 2;
        std::fill_n(boundaryGeometry.begin() + 4 + 4 * 9, 9, byte(50));
        for (int y = 0; y < 3; ++y)
        {
            for (int x = 0; x < 3; ++x)
            {
                for (int shape = 0; shape <= 6; ++shape)
                {
                    for (int flipped : { 0, 16 })
                    {
                        boundaryGeometry[4 + y * 3 + x] = static_cast<byte>((shape << 5) | flipped | 2);
                        WallLevel edge;
                        edge.BuildOriginal(assets->GetLevelInfo(), boundaryGeometry);
                        const int faces = shape < 4 ? 1 : shape < 6 ? 2 : 4;
                        std::size_t triangleCount = 0;
                        for (const auto& instance : edge.Walls())
                        {
                            triangleCount += instance->GetRenderable()->m_drawCalls[0].m_primitiveCount;
                            const auto& bounds = instance->GetGlobalBounds().m_box;
                            Check(bounds.m_min.GetX() >= edge.GetCellArray().GridPosX(0) - 0.0001f &&
                                bounds.m_max.GetX() <= edge.GetCellArray().GridPosX(3) + 0.0001f &&
                                bounds.m_min.GetZ() >= edge.GetCellArray().GridPosY(0) - 0.0001f &&
                                bounds.m_max.GetZ() <= edge.GetCellArray().GridPosY(3) + 0.0001f,
                                "Authored boundary walls must retain their original tile footprint using native padding.");
                        }
                        Check(triangleCount == static_cast<std::size_t>(faces * 2),
                            "Every wall shape and facing must retain every face on authored edges and corners.");
                    }
                }
                boundaryGeometry[4 + y * 3 + x] = 0;
            }
        }
        std::fill_n(boundaryGeometry.begin() + 4 + 9, 2 * 9, byte(2));
        WallLevel boundarySurfaces;
        boundarySurfaces.BuildOriginal(assets->GetLevelInfo(), boundaryGeometry);
        std::size_t surfaceTriangles = 0;
        for (const auto& instance : boundarySurfaces.Walls())
        {
            surfaceTriangles += instance->GetRenderable()->m_drawCalls[0].m_primitiveCount;
        }
        Check(surfaceTriangles == 36,
            "All nine authored tiles must retain floor and ceiling quads, including the final row and column.");
        for (const int number : { 10, 11, 17, 50 })
        {
            const auto original = read(L"LEVEL\\FELD" + std::to_wstring(number) + L".256");
            Desperabis::Level native;
            native.LoadGeometry(original.data(), original.size());
            WallLevel originalGrid;
            originalGrid.ReadGrid(assets->GetLevelInfo(), original, true);
            const auto& grid = originalGrid.GetCellArray();
            Check(grid.m_sizeX == 50 && grid.m_sizeZ == 40 && grid.m_cell.size() == 2000 &&
                grid.m_authoredSizeX == static_cast<int>(native.geometryWidth) &&
                grid.m_authoredSizeZ == static_cast<int>(native.geometryHeight),
                "Original files must retain authored dimensions separately from the fixed native grid.");
            for (int y = -1; y <= 40; ++y)
            {
                for (int x = -1; x <= 50; ++x)
                {
                    const bool inside = x >= 0 && x < 50 && y >= 0 && y < 40;
                    const float floor = inside ? -2.f * native.floorHeights[y][x] * DespToFloat : 0.f;
                    const float ceiling = inside ?
                        2.f * (native.roomHeights[y][x] - native.floorHeights[y][x]) * DespToFloat :
                        80.f * DespToFloat;
                    Check(std::abs(grid.GetGridPos(x, y, HeightLevel::Floor).y - floor) < 0.0001f &&
                        std::abs(grid.GetGridPos(x, y, HeightLevel::Ceiling).y - ceiling) < 0.0001f,
                        "Render vertices must match native padding and the explicit outside-grid 0/80 rule, not gameplay aliases.");
                    if (!inside) { continue; }
                    const auto& cell = grid.GetCell(x, y);
                    Check(cell.m_wallTextures == native.wallTextures[y][x] &&
                        cell.m_floor == native.floorObjects[y][x] && cell.m_ceiling == native.ceilingObjects[y][x] &&
                        cell.m_walls == native.surfaceFlags[y][x] && cell.m_rawMaterial == native.collisionMaterials[y][x],
                        "All original geometry planes must use native stride 50 with cleared padding.");
                }
            }
            std::vector<byte> saved(7 * 2000);
            int planeIndex = 0;
            for (const auto* plane : { &native.wallTextures, &native.floorObjects, &native.ceilingObjects,
                &native.floorHeights, &native.roomHeights, &native.surfaceFlags, &native.collisionMaterials })
            {
                std::memcpy(saved.data() + planeIndex++ * 2000, plane, 2000);
            }
            WallLevel savedGrid;
            savedGrid.ReadGrid(assets->GetLevelInfo(), saved, false);
            Check(savedGrid.GetCellArray().m_authoredSizeX == 50 && savedGrid.GetCellArray().m_authoredSizeZ == 40,
                "Headerless saved geometry must retain its full native dimensions.");
            for (int y = 0; y <= 40; ++y)
            {
                for (int x = 0; x <= 50; ++x)
                {
                    for (const auto height : { HeightLevel::Floor, HeightLevel::Ceiling })
                    {
                        Check(savedGrid.GetCellArray().GetGridPos(x, y, height).y == grid.GetGridPos(x, y, height).y,
                            "Original and saved geometry must produce identical padded and outside-grid vertex heights.");
                    }
                }
            }
            if (number == 11)
            {
                const auto originalObjects = read(L"LEVEL\\OBJPOS11.DAT");
                WallLevel linked;
                linked.BuildOriginal(assets->GetLevelInfo(), original, originalObjects);
                WallLevel savedLevel;
                savedLevel.Build(assets->GetLevelInfo(), native, originalObjects);
                Check(linked.MutableWalls().size() == 2 &&
                    linked.SynchronizeWallTiles(&native.wallTextures[0][0], 50, 40, 50) == 0,
                    "Original-file mutable walls must accept the same native backing dimensions as live core synchronization.");
                for (int direction = 0; direction < 4; ++direction)
                {
                    Check(render(linked, direction, center) == render(savedLevel, direction, center),
                        "Original-file and saved native planes must render identical walls, floors and static models.");
                }
                native.wallTextures[8][19] = 0;
                Check(linked.SynchronizeWallTiles(&native.wallTextures[0][0], 50, 40, 50) == 1,
                    "A compact original file must retain immediate selective wall removal after native-grid expansion.");
            }
        }
        std::vector<byte> nativeBoundary(4 + 7 * 2000, 0);
        nativeBoundary[0] = 49;
        nativeBoundary[2] = 39;
        std::fill_n(nativeBoundary.begin() + 4 + 4 * 2000, 2000, byte(50));
        for (const auto& cell : { std::pair<int, int>{ 49, 1 }, { 1, 39 }, { 49, 39 } })
        {
            for (int shape = 0; shape <= 6; ++shape)
            {
                for (const int flipped : { 0, 16 })
                {
                    nativeBoundary[4 + cell.second * 50 + cell.first] = static_cast<byte>((shape << 5) | flipped | 2);
                    WallLevel edge;
                    edge.BuildOriginal(assets->GetLevelInfo(), nativeBoundary);
                    Check(edge.Walls().size() == 1 &&
                        edge.Walls()[0]->GetRenderable()->m_drawCalls[0].m_primitiveCount ==
                            (shape < 4 ? 2 : shape < 6 ? 4 : 8),
                        "Physical 50x40 boundary tiles must retain all wall faces using the native outside-grid vertices.");
                }
            }
            nativeBoundary[4 + cell.second * 50 + cell.first] = 0;
        }
        nativeBoundary[4 + 2000 + 1999] = nativeBoundary[4 + 2 * 2000 + 1999] = 2;
        WallLevel physicalSurfaces;
        physicalSurfaces.BuildOriginal(assets->GetLevelInfo(), nativeBoundary);
        std::size_t physicalTriangles = 0;
        for (const auto& instance : physicalSurfaces.Walls())
        {
            physicalTriangles += instance->GetRenderable()->m_drawCalls[0].m_primitiveCount;
            const auto& bounds = instance->GetGlobalBounds().m_box;
            Check(std::abs(bounds.m_max.GetX() - physicalSurfaces.GetCellArray().GridPosX(50)) < 0.0001f &&
                std::abs(bounds.m_max.GetZ() - physicalSurfaces.GetCellArray().GridPosY(40)) < 0.0001f,
                "The final physical tile must generate its complete footprint, not clip at the last stored height.");
        }
        Check(physicalTriangles == 4, "The final physical tile must retain both floor and ceiling quads.");
        std::fill_n(nativeBoundary.begin() + 4 + 2000, 2 * 2000, byte(0));
        auto waterInfo = assets->GetLevelInfo();
        waterInfo.m_raw.m_data.water[0] = { WaterReflectionType::Water, 10 };
        for (const auto& cell : { std::pair<int, int>{ 49, 1 }, { 1, 39 }, { 49, 39 } })
        {
            nativeBoundary[4 + 5 * 2000 + cell.second * 50 + cell.first] = 1;
            WallLevel water;
            water.BuildOriginal(waterInfo, nativeBoundary);
            Check(water.Walls().size() == 1 &&
                water.Walls()[0]->GetRenderable()->m_drawCalls[0].m_material.m_transparencyType == TransparencyMode::Water &&
                water.Walls()[0]->GetRenderable()->m_drawCalls[0].m_primitiveCount == 2,
                "Water must retain a complete quad on the last native row, column and corner.");
            const auto& bounds = water.Walls()[0]->GetGlobalBounds().m_box;
            Check(std::abs(bounds.m_min.GetY() + 100.f * DespToFloat) < 0.0001f &&
                std::abs(bounds.m_max.GetY() + 100.f * DespToFloat) < 0.0001f &&
                std::abs(bounds.m_max.GetX() - water.GetCellArray().GridPosX(cell.first + 1)) < 0.0001f &&
                std::abs(bounds.m_max.GetZ() - water.GetCellArray().GridPosY(cell.second + 1)) < 0.0001f,
                "Boundary water vertices must use the surface depth and full footprint, not the outside-grid floor/ceiling rule.");
            nativeBoundary[4 + 5 * 2000 + cell.second * 50 + cell.first] = 0;
        }
        auto* shoreline = FORCE_GET(AssetManager::Instance().LoadLevel(17));
        for (const bool physical : { false, true })
        {
            auto modelGeometry = physical ? nativeBoundary : boundaryGeometry;
            const int width = physical ? 50 : 3;
            const int height = physical ? 40 : 3;
            const int count = width * height;
            std::fill_n(modelGeometry.begin() + 4 + count, 2 * count, byte(0));
            for (const bool ceiling : { false, true })
            {
                const int plane = ceiling ? 2 : 1;
                modelGeometry[4 + plane * count + count - 1] = 172;
                WallLevel model;
                model.BuildOriginal(shoreline->GetLevelInfo(), modelGeometry);
                Check(model.Walls().size() == 1, "The final authored/native tile must retain its static floor/ceiling model.");
                const auto& position = model.Walls()[0]->GetTransform().GetTranslation();
                float expectedY = ceiling ? 100.f * DespToFloat : physical ? 0.f : -510.f * DespToFloat;
                const auto& local = model.Walls()[0]->GetRenderable()->m_localBounds.m_box;
                const float floorY = model.GetHeightAtPos(Vector(position.GetX(), expectedY, position.GetZ()));
                if (!ceiling && (model.Walls()[0]->HasWaterMaterials() ||
                    (local.m_max.GetY() - local.m_min.GetY() <= 2.f * DespToFloat &&
                        std::abs(expectedY - floorY) <= 8.f * DespToFloat)))
                {
                    expectedY = floorY + 2.f * DespToFloat;
                }
                Check(std::abs(position.GetX() - (model.GetCellArray().GridPosX(width - 1) + 50.f * DespToFloat)) < 0.0001f &&
                    std::abs(position.GetZ() - (model.GetCellArray().GridPosY(height - 1) + 50.f * DespToFloat)) < 0.0001f &&
                    std::abs(position.GetY() - expectedY) < 0.0001f,
                    "Edge models must preserve native corner anchors, except the fresh thin-floor offset; ceilings stay unchanged.");
                modelGeometry[4 + plane * count + count - 1] = 0;
            }
        }
        for (const bool header : { false, true })
        {
            auto truncated = header ? boundaryGeometry : std::vector<byte>(7 * 2000, 0);
            truncated.pop_back();
            bool rejected = false;
            try { WallLevel invalid; invalid.ReadGrid(assets->GetLevelInfo(), truncated, header); }
            catch (const std::invalid_argument&) { rejected = true; }
            Check(rejected, "Truncated original and saved geometry must fail explicitly.");
        }
        auto oversized = boundaryGeometry;
        oversized[0] = 50;
        bool rejected = false;
        try { WallLevel invalid; invalid.ReadGrid(assets->GetLevelInfo(), oversized, true); }
        catch (const std::invalid_argument&) { rejected = true; }
        Check(rejected, "Original dimensions exceeding the native grid must fail before copying plane data.");
        auto stale = nativeBoundary;
        for (const int plane : { 0, 1, 2, 5, 6 }) { stale[4 + plane * 2000 + 1999] = 7; }
        WallLevel reloaded;
        reloaded.ReadGrid(assets->GetLevelInfo(), stale, true);
        reloaded.ReadGrid(assets->GetLevelInfo(), boundaryGeometry, true);
        const auto& padding = reloaded.GetCellArray().GetCell(49, 39);
        Check(padding.m_wallTextures == 0 && padding.m_floor == 0 && padding.m_ceiling == 0 &&
            padding.m_walls == 0 && padding.m_rawMaterial == 0 &&
            std::abs(padding.m_floorHeight + 510.f * DespToFloat) < 0.0001f &&
            std::abs(padding.m_ceilingHeight + 410.f * DespToFloat) < 0.0001f,
            "Loading a smaller file must reset all native padding instead of retaining the previous level's cells.");

        auto* forestAssets = FORCE_GET(AssetManager::Instance().LoadLevel(16));
        const auto forestFeld = read(L"LEVEL\\FELD16.256");
        auto forestObjects = read(L"LEVEL\\OBJPOS16.DAT");
        Desperabis::Level forestGeometry;
        forestGeometry.LoadGeometry(forestFeld.data(), forestFeld.size());
        forestGeometry.BuildCollisionMap();
        Desperabis::GameContext forestCore;
        forestCore.level = &forestGeometry;
        forestCore.collisionMap = forestGeometry.collisionMap.get();
        int frontSlot = 0;
        for (std::size_t offset = 0; offset < forestObjects.size(); offset += 54)
        {
            const int slot = static_cast<int>(offset / 54) + 2;
            forestCore.ReplaceWorldObject(slot, Desperabis::DecodeGameObject(forestObjects.data() + offset, 54));
            if (!frontSlot && forestCore.worldObjects[slot].behavior == Desperabis::BehaviorType::ExtinguishLava)
            {
                frontSlot = slot;
            }
        }
        Check(frontSlot != 0, "Original forest must provide a water-front fixture.");
        WallLevel forest;
        forest.Build(forestAssets->GetLevelInfo(), forestGeometry, forestObjects);
        Check(forest.MutableFloors().size() == 127 && level.MutableFloors().empty(),
            "Only the forest's 127 authored river-path tiles must acquire switchable floor variants.");
        for (const auto& floor : forest.MutableFloors())
        {
            Check(floor.original && floor.replacement && floor.original->GetVisible() &&
                !floor.replacement->GetVisible() &&
                (floor.originalFloor & 0xe0) == (floor.replacementFloor & 0xe0) &&
                (floor.original->GetTransform().GetTranslation() -
                    floor.replacement->GetTransform().GetTranslation()).GetLength() < 0.00001f,
                "River variants must retain native model placement/bank/rotation and show only the original initially.");
        }
        const auto forestInstances = forest.Walls();
        std::vector<std::shared_ptr<IGraphicsRuntimeObject>> forestBuffers;
        for (const auto& instance : forestInstances) { forestBuffers.push_back(instance->GetRenderable()->m_runtime); }
        const auto syncForest = [&]
        {
            return forest.SynchronizeFloorTiles(&forestGeometry.floorObjects[0][0], 50, 40, 50);
        };
        Check(syncForest() == 0, "Dormant river synchronization must be idempotent.");
        forestCore.ActivateObjectGroup(1);
        std::size_t completed = 0;
        std::size_t savedRemaining = 0;
        std::unique_ptr<WallLevel> midwaySave;
        for (int tick = 0; tick < 800; ++tick)
        {
            forestCore.tickCount = 1;
            for (std::size_t slot = 2; slot < forestCore.worldObjects.size(); ++slot)
            {
                if (forestCore.worldObjects[slot].behavior == Desperabis::BehaviorType::ExtinguishLava)
                {
                    Desperabis::UpdateObject(forestCore.worldObjects[slot], forestCore);
                }
            }
            completed += syncForest();
            if (tick == 100)
            {
                auto savedObjects = forestObjects;
                for (std::size_t offset = 0; offset < savedObjects.size(); offset += 54)
                {
                    Desperabis::EncodeGameObject(forestCore.worldObjects[offset / 54 + 2], savedObjects.data() + offset, 54);
                }
                midwaySave = std::make_unique<WallLevel>();
                midwaySave->Build(forestAssets->GetLevelInfo(), forestGeometry, savedObjects);
                savedRemaining = 127 - completed;
                Check(completed > 0 && savedRemaining > 0 && midwaySave->MutableFloors().size() == savedRemaining &&
                    midwaySave->SynchronizeFloorTiles(&forestGeometry.floorObjects[0][0], 50, 40, 50) == 0,
                    "Midway loading must preserve completed tiles and prepare only each front's remaining path.");
            }
        }
        Check(completed == 127 && syncForest() == 0,
            "Four original fronts must switch every authored river tile exactly once.");
        Check(midwaySave->SynchronizeFloorTiles(&forestGeometry.floorObjects[0][0], 50, 40, 50) == savedRemaining,
            "A midway-loaded river must continue through all its remaining prepared tiles.");
        for (const auto& floor : forest.MutableFloors())
        {
            Check(!floor.original->GetVisible() && floor.replacement->GetVisible(),
                "Completed river tiles must replace the lava model, not overlay it.");
        }
        for (std::size_t i = 0; i < forestInstances.size(); ++i)
        {
            Check(forest.Walls()[i] == forestInstances[i] &&
                forest.Walls()[i]->GetRenderable()->m_runtime == forestBuffers[i],
                "Flooding must not allocate instances or rebuild/upload terrain buffers at runtime.");
        }
        for (std::size_t offset = 0; offset < forestObjects.size(); offset += 54)
        {
            Desperabis::EncodeGameObject(forestCore.worldObjects[offset / 54 + 2], forestObjects.data() + offset, 54);
        }
        WallLevel completedSave;
        completedSave.Build(forestAssets->GetLevelInfo(), forestGeometry, forestObjects);
        Check(completedSave.MutableFloors().empty(),
            "A completed saved river must load the final floor models without reviving dead front paths.");

        auto originalFront = Desperabis::DecodeGameObject(
            read(L"LEVEL\\OBJPOS16.DAT").data() + (frontSlot - 2) * 54, 54);
        const int riverColumn = originalFront.posX / 100 - 1;
        const int riverRow = 40 - originalFront.posZ / 100;
        Desperabis::Level riverGeometry;
        for (auto& row : riverGeometry.floorHeights) { std::fill(std::begin(row), std::end(row), byte(100)); }
        for (auto& row : riverGeometry.roomHeights) { std::fill(std::begin(row), std::end(row), byte(70)); }
        riverGeometry.floorObjects[riverRow][riverColumn] = 138;
        riverGeometry.BuildCollisionMap();
        Desperabis::GameContext riverCore;
        riverCore.level = &riverGeometry;
        riverCore.collisionMap = riverGeometry.collisionMap.get();
        originalFront.itemValue = 1;
        originalFront.blendWeightOrSpriteOpacity = 0;
        originalFront.posY = -204;
        originalFront.health = 1000;
        riverCore.ReplaceWorldObject(2, originalFront);
        std::vector<byte> riverObjects(54);
        Desperabis::EncodeGameObject(originalFront, riverObjects.data(), 54);
        WallLevel river;
        river.Build(forestAssets->GetLevelInfo(), riverGeometry, riverObjects);
        Vector riverCenter = Level::DespToPos(100 * riverColumn + 150, -200, 4050 - 100 * riverRow);
        const auto renderRiver = [&](WallLevel& drawn, const Desperabis::RawGameObject& front)
        {
            drawn.UpdateFront(front);
            Camera camera;
            Matrix4 projection, transform;
            projection.CreateOrthographic(130.f * DespToFloat, 130.f * DespToFloat, 0.01f, 30.f);
            transform.Create(Vector(riverCenter.GetX(), riverCenter.GetY() + 300.f * DespToFloat, riverCenter.GetZ()),
                Vector(0, -static_cast<float>(Math::Pi * 0.5), 0));
            camera.SetProjectionMatrix(projection);
            camera.SetCameraTransformMatrix(transform);
            RenderingSettings waterSettings;
            waterSettings.gridVisibility = false;
            waterSettings.fullbrightRendering = true;
            VisibilitySettings viewSettings;
            VisibilityResult visible;
            visibility.ComputeResultFor(&camera, &drawn.WallScene(), visible, waterSettings, viewSettings, stats);
            visible.m_lightmap = &lightmap;
            visible.m_fogFalloff = 0;
            const float black[]{ 0,0,0,1 };
            context->ClearRenderTargetView(target.colorView.Get(), black);
            context->ClearDepthStencilView(target.depthView.Get(), D3D11_CLEAR_DEPTH, 1.f, 0);
            renderer.RenderSceneElements(visible, waterSettings, stats);
            for (const auto& water : visible.m_visibleWater)
            {
                for (const auto& entry : water.m_waterReceiver)
                {
                    Check(entry.m_instance->GetVisible(),
                        "Water passes must never submit the hidden riverbed variant or expired front.");
                }
            }
            return target.Pixels(device, context);
        };
        const auto riverStart = renderRiver(river, riverCore.worldObjects[2]);
        riverCore.tickCount = 12;
        Desperabis::UpdateObject(riverCore.worldObjects[2], riverCore);
        const auto riverPartial = renderRiver(river, riverCore.worldObjects[2]);
        const auto frontInstance = river.UpdateFront(riverCore.worldObjects[2]);
        const Vector frontPosition = frontInstance->GetTransform().GetTranslation();
        Check(std::fabs(frontPosition.GetY() -
            (river.GetCellArray().GetHeightAtPos(frontPosition) + 2.f * DespToFloat)) < 0.00001f &&
            riverCore.worldObjects[2].posY == -204,
            "Flood rendering must sit above the riverbed without changing the native core object's height.");
        std::size_t morphPixels = 0;
        for (std::size_t i = 0; i < riverStart.size(); ++i) { morphPixels += riverStart[i] != riverPartial[i]; }
        Check(std::abs(frontInstance->m_blendWeight - 0.48f) < 0.00001f &&
            frontInstance->GetBlendTargetRenderable() && morphPixels > 100,
            "The original frame-4/5 water front must visibly morph at core-authored partial progress.");
        Desperabis::EncodeGameObject(riverCore.worldObjects[2], riverObjects.data(), 54);
        WallLevel partialSave;
        partialSave.Build(forestAssets->GetLevelInfo(), riverGeometry, riverObjects);
        Check(renderRiver(partialSave, riverCore.worldObjects[2]) == riverPartial,
            "A saved partially flooded tile must render pixel-identically with its saved morph weight.");
        riverCore.tickCount = 14;
        Desperabis::UpdateObject(riverCore.worldObjects[2], riverCore);
        Check(river.SynchronizeFloorTiles(&riverGeometry.floorObjects[0][0], 50, 40, 50) == 1 &&
            riverCore.worldObjects[2].health == 0, "The terminal front must reveal its prepared final riverbed.");
        const auto riverEnd = renderRiver(river, riverCore.worldObjects[2]);
        Desperabis::EncodeGameObject(riverCore.worldObjects[2], riverObjects.data(), 54);
        WallLevel riverSave;
        riverSave.Build(forestAssets->GetLevelInfo(), riverGeometry, riverObjects);
        Check(riverEnd == renderRiver(riverSave, riverCore.worldObjects[2]) && riverEnd != riverStart,
            "Completed runtime river geometry must visibly change and match freshly loaded saved floors.");
        Check(renderRiver(forest, forestCore.worldObjects[frontSlot]) ==
            renderRiver(completedSave, forestCore.worldObjects[frontSlot]) &&
            renderRiver(forest, forestCore.worldObjects[frontSlot]) ==
            renderRiver(*midwaySave, forestCore.worldObjects[frontSlot]),
            "Original forest runtime, completed save and continued midway save must render pixel-identically.");
        riverGeometry.floorObjects[riverRow][riverColumn] = 138;
        Check(river.SynchronizeFloorTiles(&riverGeometry.floorObjects[0][0], 50, 40, 50) == 1,
            "Prepared river variants must support restoration without a rebuild.");
        std::vector<byte> stridedFloors(53 * 40, 0);
        stridedFloors[riverRow * 53 + riverColumn] = 139;
        Check(river.SynchronizeFloorTiles(stridedFloors.data(), 50, 40, 53) == 1 &&
            river.SynchronizeFloorTiles(stridedFloors.data(), 50, 40, 53) == 0,
            "Floor synchronization must honor row stride and avoid repeated changes.");
        stridedFloors[riverRow * 53 + riverColumn] = 140;
        rejected = false;
        try { river.SynchronizeFloorTiles(stridedFloors.data(), 50, 40, 53); }
        catch (const std::runtime_error&) { rejected = true; }
        Check(rejected, "Unprepared river floor variants must fail explicitly.");
        rejected = false;
        try { river.SynchronizeFloorTiles(stridedFloors.data(), 49, 40, 53); }
        catch (const std::invalid_argument&) { rejected = true; }
        Check(rejected, "Floor synchronization must reject mismatched backing dimensions.");
        riverGeometry.floorObjects[riverRow][riverColumn] = 138;
        auto invalidFront = originalFront;
        invalidFront.behaviorParameter = 31;
        std::vector<byte> invalidObjects(54);
        Desperabis::EncodeGameObject(invalidFront, invalidObjects.data(), 54);
        WallLevel invalidRiverModel;
        CheckRecoverableError<std::runtime_error>([&]
        {
            invalidRiverModel.Build(forestAssets->GetLevelInfo(), riverGeometry, invalidObjects);
        });
#ifdef NDEBUG
        Check(invalidRiverModel.MutableFloors().size() == 1 &&
            invalidRiverModel.MutableFloors()[0].original &&
            invalidRiverModel.MutableFloors()[0].original->GetVisible() &&
            !invalidRiverModel.MutableFloors()[0].replacement,
            "An unavailable replacement model must retain the original prepared riverbed.");
        riverGeometry.floorObjects[riverRow][riverColumn] = 159;
        Check(invalidRiverModel.SynchronizeFloorTiles(&riverGeometry.floorObjects[0][0], 50, 40, 50) == 0 &&
            invalidRiverModel.GetCellArray().GetCell(riverColumn, riverRow).m_floor == 138 &&
            invalidRiverModel.MutableFloors()[0].original->GetVisible(),
            "Release must not hide a valid floor or claim synchronization succeeded when its replacement asset is missing.");
        riverGeometry.floorObjects[riverRow][riverColumn] = 138;
#endif
        invalidFront = originalFront;
        invalidFront.posX = 0;
        Desperabis::EncodeGameObject(invalidFront, invalidObjects.data(), 54);
        WallLevel invalidRiverPath;
        CheckRecoverableError<std::runtime_error>([&]
        {
            invalidRiverPath.Build(forestAssets->GetLevelInfo(), riverGeometry, invalidObjects);
        });
#ifdef NDEBUG
        Check(invalidRiverPath.MutableFloors().empty(),
            "Release must stop an out-of-grid river path before indexing tile storage.");
#endif
        for (const auto values : { std::pair<byte, byte>{ 1, 3 }, { 0, 3 }, { 16, 3 }, { 1, 0 } })
        {
            riverGeometry.floorObjects[riverRow][riverColumn] = values.first;
            auto quadFront = originalFront;
            quadFront.behaviorParameter = values.second;
            std::vector<byte> duplicateFronts(108);
            Desperabis::EncodeGameObject(quadFront, duplicateFronts.data(), 54);
            Desperabis::EncodeGameObject(quadFront, duplicateFronts.data() + 54, 54);
            WallLevel quad;
            quad.Build(forestAssets->GetLevelInfo(), riverGeometry, duplicateFronts);
            Check(quad.MutableFloors().size() == 1,
                "Duplicate river fronts must share one tile binding, including textured and empty floors.");
            const auto& floor = quad.MutableFloors()[0];
            if (floor.original && floor.replacement)
            {
                Check(floor.original->GetRenderable()->m_runtime == floor.replacement->GetRenderable()->m_runtime,
                    "Textured river variants must share the island's immutable buffers, just like mutable walls.");
            }
            riverGeometry.floorObjects[riverRow][riverColumn] = values.second;
            Check(quad.SynchronizeFloorTiles(&riverGeometry.floorObjects[0][0], 50, 40, 50) == 1 &&
                (!floor.original || !floor.original->GetVisible()) &&
                (!floor.replacement || floor.replacement->GetVisible()),
                "Textured and empty floor variants must switch visibility without duplicate or missing faces.");
        }
        auto* meteorAssets = FORCE_GET(AssetManager::Instance().LoadLevel(97));
        auto fireGeometry = Desperabis::CreateDummyLevel();
        for (auto& row : fireGeometry.floorHeights) { std::fill_n(row, 50, byte(102)); }
        fireGeometry.floorObjects[riverRow][riverColumn] = 47;
        fireGeometry.floorObjects[riverRow][riverColumn + 1] = 47;
        fireGeometry.surfaceFlags[riverRow][riverColumn] = 0x80;
        fireGeometry.surfaceFlags[riverRow][riverColumn + 1] = 0x80;
        WallLevel fire;
        fire.Build(meteorAssets->GetLevelInfo(), fireGeometry, std::vector<byte>(54));
        Check(fire.FloorModels().size() == 2, "Fire fixture must expose two independent static model tiles.");
        const auto fireInstances = fire.Walls();
        std::vector<std::shared_ptr<IGraphicsRuntimeObject>> fireBuffers;
        for (const auto& instance : fireInstances) { fireBuffers.push_back(instance->GetRenderable()->m_runtime); }
        Texture* fireLightmap = &fire.Lightmap();
        const auto lightmapBefore = fireLightmap->m_runtime;
        const auto unburnt = renderRiver(fire, {});
        Desperabis::GameContext fireCore;
        fireCore.level = &fireGeometry;
        Check(fireCore.ReplaceSurfaceSelector(originalFront.posX, originalFront.posZ, 0, 29),
            "The recovered native surface helper must burn the rendered fixture cell.");
        Check(fire.SynchronizeSurfaceTiles(&fireGeometry.surfaceFlags[0][0], 50, 40, 50) == 1 &&
            fire.SynchronizeSurfaceTiles(&fireGeometry.surfaceFlags[0][0], 50, 40, 50) == 0,
            "Surface synchronization must update one cell exactly once.");
        const auto& burningTile = fire.FloorModels()[0].instance;
        const auto& untouchedTile = fire.FloorModels()[1].instance;
        Check(std::abs(burningTile->m_fireIntensity - 30.f / 70.f) < 0.00001f &&
            burningTile->m_tintColor.r == 72 && untouchedTile->m_fireIntensity == 0 &&
            untouchedTile->m_tintColor.r == 255 && fireLightmap == &fire.Lightmap() &&
            fireLightmap->m_runtime != lightmapBefore &&
            fire.GetCellArray().GetCell(riverColumn, riverRow).m_brightness.r == 100,
            "Surface 29 must apply native table 80's fire presentation only to its tile and refresh cleared brightness.");
        const auto burnt = renderRiver(fire, {});
        std::size_t firePixels = 0;
        for (std::size_t i = 0; i < burnt.size(); ++i) { firePixels += burnt[i] != unburnt[i]; }
        Check(firePixels > 100, "A core-driven surface change must visibly affect the static model.");
        WallLevel fireSave;
        fireSave.Build(meteorAssets->GetLevelInfo(), fireGeometry, std::vector<byte>(54));
        Check(renderRiver(fireSave, {}) == burnt,
            "Saved burnt geometry must render identically to a runtime-updated surface.");
        for (std::size_t i = 0; i < fireInstances.size(); ++i)
        {
            Check(fire.Walls()[i] == fireInstances[i] &&
                fireInstances[i]->GetRenderable()->m_runtime == fireBuffers[i],
                "Burning surfaces must retain their original static instances and GPU model buffers.");
        }
        std::vector<byte> stridedSurfaces(53 * 40, 0);
        for (std::size_t row = 0; row < 40; ++row)
        {
            std::copy_n(&fireGeometry.surfaceFlags[row][0], 50, stridedSurfaces.data() + row * 53);
        }
        stridedSurfaces[riverRow * 53 + riverColumn] = 0x80;
        Check(fire.SynchronizeSurfaceTiles(stridedSurfaces.data(), 50, 40, 53) == 1 &&
            renderRiver(fire, {}) == unburnt,
            "Strided surface restoration must remove the fire overlay and restore original pixels.");
        for (const byte selector : { byte(30), byte(31) })
        {
            stridedSurfaces[riverRow * 53 + riverColumn] = selector;
            Check(fire.SynchronizeSurfaceTiles(stridedSurfaces.data(), 50, 40, 53) == 1 &&
                burningTile->m_fireIntensity == 0 && burningTile->m_tintColor.r < 255,
                "Native surface tables 30/31 must use a charred non-burning remap.");
        }
        rejected = false;
        try { fire.SynchronizeSurfaceTiles(nullptr, 50, 40, 50); }
        catch (const std::invalid_argument&) { rejected = true; }
        Check(rejected, "Surface synchronization must reject missing storage explicitly.");
        stridedSurfaces[riverRow * 53 + riverColumn] = 1;
        Check(fire.SynchronizeSurfaceTiles(stridedSurfaces.data(), 50, 40, 53) == 1 &&
            fire.GetCellArray().GetCell(riverColumn, riverRow).m_waterSlot == WaterSlotIndex::WaterSlot1,
            "Strided surface updates must support replacing charred ground with water.");
        stridedSurfaces[riverRow * 53 + riverColumn] = 31;
        Check(fire.SynchronizeSurfaceTiles(stridedSurfaces.data(), 50, 40, 53) == 1 &&
            !fire.GetCellArray().GetCell(riverColumn, riverRow).IsWaterSurface(),
            "Replacing water with charred ground must remove the water slot.");

        auto* floodAssets = FORCE_GET(AssetManager::Instance().LoadLevel(26));
        Desperabis::Level floodGeometry;
        for (auto& row : floodGeometry.floorHeights) { std::fill(std::begin(row), std::end(row), byte(220)); }
        for (auto& row : floodGeometry.roomHeights) { std::fill(std::begin(row), std::end(row), byte(200)); }
        floodGeometry.floorObjects[3][20] = floodGeometry.floorObjects[3][21] = 1;
        floodGeometry.BuildCollisionMap();
        Desperabis::GameContext floodCore;
        floodCore.level = &floodGeometry;
        floodCore.collisionMap = floodGeometry.collisionMap.get();
        auto waterFront = Desperabis::DecodeGameObject(read(L"LEVEL\\OBJPOS26.DAT").data() + (6 - 2) * 54, 54);
        waterFront.posX = 2256;
        waterFront.posY = -160;
        waterFront.posZ = 3750;
        waterFront.velocityX = -6;
        waterFront.velocityZ = 0;
        floodCore.ReplaceWorldObject(6, waterFront);
        floodCore.SetObjectHealth(floodCore.worldObjects[6], 1000);
        floodCore.tickCount = 1;
        WallLevel flood;
        flood.Build(floodAssets->GetLevelInfo(), floodGeometry, std::vector<byte>(54));
        riverCenter = Level::DespToPos(2250, -160, 3750);
        const auto dryFlood = renderRiver(flood, {});
        const auto registeredFloodInstances = flood.WallScene().GetRenderableInstances();
        const auto floodTerrain = flood.Walls();
        std::vector<std::shared_ptr<IGraphicsRuntimeObject>> floodTerrainBuffers;
        for (const auto& instance : floodTerrain) { floodTerrainBuffers.push_back(instance->GetRenderable()->m_runtime); }
        const auto island = flood.GetCellArray().GetCell(21, 3).m_visibilityIsland;
        const auto findWater = [&](WaterSlotIndex slot)
        {
            const auto batch = std::find_if(flood.WaterSurfaces().begin(), flood.WaterSurfaces().end(),
                [&](const auto& water) { return water.island == island && water.slot == slot; });
            Check(batch != flood.WaterSurfaces().end(), "Enabled water slots need stable registered batches, even when dry.");
            return &*batch;
        };
        const auto* water1 = findWater(WaterSlotIndex::WaterSlot1);
        const auto* water2 = findWater(WaterSlotIndex::WaterSlot2);
        const auto originalWater2 = water2->geometry;
        Check(!water1->instance->GetVisible() && !water2->instance->GetVisible(),
            "Dry water batches must not emit receiver passes or cover the original floor.");
        Desperabis::UpdateObject(floodCore.worldObjects[6], floodCore);
        Check(floodGeometry.surfaceFlags[3][21] == 225 &&
            flood.SynchronizeSurfaceTiles(&floodGeometry.surfaceFlags[0][0], 50, 40, 50) == 1 &&
            flood.SynchronizeSurfaceTiles(&floodGeometry.surfaceFlags[0][0], 50, 40, 50) == 0 &&
            water1->instance->GetVisible() && water1->instance->HasWaterMaterials() &&
            water1->geometry->m_drawCalls.at(0).m_primitiveCount == 2 &&
            water2->geometry == originalWater2,
            "The original flood behavior must add precisely one water quad without rebuilding other water slots.");
        bool reflects = false;
        float waterHeight = 0.f;
        Check(water1->instance->RetrieveWaterProperties(reflects, waterHeight) &&
            std::abs(waterHeight + 160.f * DespToFloat) < 0.00001f &&
            water1->geometry->m_drawCalls.at(0).m_material.m_reflectionType ==
                floodAssets->GetLevelInfo().m_raw.m_data.water[0].reflectionType,
            "New flood geometry must use the final level's native height and reflection/refraction mode.");
        const auto wetFlood = renderRiver(flood, {});
        std::size_t floodPixels = 0;
        for (std::size_t pixel = 0; pixel < wetFlood.size(); ++pixel) { floodPixels += wetFlood[pixel] != dryFlood[pixel]; }
        Check(floodPixels > 100, "A native dry-to-water transition must create visibly rendered water, not just flags.");
        WallLevel floodSave;
        floodSave.Build(floodAssets->GetLevelInfo(), floodGeometry, std::vector<byte>(54));
        Check(renderRiver(floodSave, {}) == wetFlood,
            "Newly flooded runtime water must render pixel-identically to saved flooded geometry.");
        const auto firstWaterBuffer = water1->geometry->m_runtime;
        floodCore.worldObjects[6].posX = 2156;
        Desperabis::UpdateObject(floodCore.worldObjects[6], floodCore);
        Check(flood.SynchronizeSurfaceTiles(&floodGeometry.surfaceFlags[0][0], 50, 40, 50) == 1 &&
            water1->geometry->m_drawCalls.at(0).m_primitiveCount == 4 &&
            water1->geometry->m_runtime != firstWaterBuffer && water2->geometry == originalWater2 &&
            water1->instance->GetGlobalBounds().m_tileRange.m_minX == 20 &&
            water1->instance->GetGlobalBounds().m_tileRange.m_maxX == 21,
            "Flood growth must rebuild only its water batch and expand visibility bounds to the newly flooded tile.");
        std::vector<byte> floodSurfaces(53 * 40, 0);
        for (int row = 0; row < 40; ++row)
        {
            std::copy_n(&floodGeometry.surfaceFlags[row][0], 50, floodSurfaces.data() + row * 53);
        }
        floodSurfaces[3 * 53 + 21] = 0;
        Check(flood.SynchronizeSurfaceTiles(floodSurfaces.data(), 50, 40, 53) == 1 &&
            water1->geometry->m_drawCalls.at(0).m_primitiveCount == 2 &&
            !flood.GetCellArray().GetCell(21, 3).IsWaterSurface(),
            "Strided water removal must remove only the selected tile's quad and cached water slot.");
        floodSurfaces[3 * 53 + 20] = 226;
        Check(flood.SynchronizeSurfaceTiles(floodSurfaces.data(), 50, 40, 53) == 1 &&
            !water1->instance->GetVisible() && water1->geometry->m_drawCalls.empty() &&
            water2->instance->GetVisible() && water2->geometry->m_drawCalls.at(0).m_primitiveCount == 2 &&
            water2->instance->RetrieveWaterProperties(reflects, waterHeight) &&
            std::abs(waterHeight + 390.f * DespToFloat) < 0.00001f &&
            flood.GetCellArray().GetCell(20, 3).m_waterSlot == WaterSlotIndex::WaterSlot2,
            "Switching water slots must remove the old receiver and use the new native plane.");
        floodSurfaces[3 * 53 + 20] = 3;
        Check(flood.SynchronizeSurfaceTiles(floodSurfaces.data(), 50, 40, 53) == 1 &&
            !water2->instance->GetVisible() &&
            flood.GetCellArray().GetCell(20, 3).m_waterSlot == WaterSlotIndex::WaterSlot3,
            "A zero-depth water slot must update collision metadata without emitting flat water geometry.");
        floodSurfaces[3 * 53 + 20] = 0;
        Check(flood.SynchronizeSurfaceTiles(floodSurfaces.data(), 50, 40, 53) == 1 &&
            renderRiver(flood, {}) == dryFlood &&
            flood.WallScene().GetRenderableInstances() == registeredFloodInstances,
            "Removing all water must restore original pixels without replacing registered scene instances.");
        for (std::size_t index = 0; index < floodTerrain.size(); ++index)
        {
            Check(floodTerrain[index]->GetRenderable()->m_runtime == floodTerrainBuffers[index],
                "Flood growth, slot replacement and removal must preserve wall/floor/ceiling GPU buffers.");
        }
        std::cout << "Dynamic water: native flood behavior, " << floodPixels
            << " changed GPU pixels, saved-pixel equivalence, stable terrain/instances, growth bounds, "
            "slot heights, removal and stride passed.\n";
        std::cout << "Meteor surfaces: core-driven burning pixels, saved pixels, stable static buffers, "
            "lightmap refresh, charred remaps, restoration and stride passed (" << firePixels << " changed pixels).\n";
        std::cout << "Forest flooding: 127 original river tiles, unchanged GPU buffers, core-driven water-front morph pixels, "
            "partial/completed save pixels, midway continuation, visibility, restoration and stride passed ("
            << morphPixels << " morph pixels).\n";
        std::cout << "Native grid: original tutorial pixels, four original padded grids/save planes, "
            "all wall shapes/facings at authored and physical edges, full floor/ceiling/water/model footprints, "
            "live wall sync and 0/80 vertices passed.\n";
        std::cout << "Mutable walls: original level-11 triggers, shared immutable buffers, selective removal, "
            "identical saved-geometry rendering, duplicate targets and all seven shapes passed ("
            << changedPixels << " changed pixels).\n";
    }

    void CheckWaterSurfaceDefinitions()
    {
        class SurfaceLevel final : public Level
        {
        public:
            void Build(const LevelInfo& info)
            {
                InitLevelInfo(info);
                constexpr int width = 8, height = 3, count = width * height;
                std::vector<byte> geometry(4 + 7 * count, 0);
                geometry[0] = width - 1;
                geometry[2] = height - 1;
                const byte selectors[]{ 1,2,3,4,17,20 };
                std::copy(std::begin(selectors), std::end(selectors), geometry.begin() + 4 + 5 * count + width + 1);
                ReadLevelData(geometry.data(), static_cast<int>(geometry.size()), true);
                CreateWallGeometry();
                for (const auto& instance : m_wallGeometry) { instance->ComputeTransparencyMask(); }
            }
            auto Walls() const { return PopulatedGeometry(m_wallGeometry); }
        };
        LevelInfo fixture;
        fixture.m_raw.m_data.water[0] = { WaterReflectionType::Water, 10 };
        fixture.m_raw.m_data.water[1] = { WaterReflectionType::Disabled, 30 };
        fixture.m_raw.m_data.water[2] = { WaterReflectionType::ReflectionBit | WaterReflectionType::Water, 30 };
        fixture.m_raw.m_rawData[114] = static_cast<byte>(WaterReflectionType::Lava);
        fixture.m_raw.m_rawData[115] = 17;
        Check(sizeof(LevelInfo::LevelInfoRaw) == 124 &&
            fixture.m_raw.m_data.water[3].reflectionType == WaterReflectionType::Lava &&
            fixture.m_raw.m_data.water[3].height == 17,
            "The fourth native surface pair must retain LEVELS.INF offsets 114/115 and the 124-byte record layout.");
        SurfaceLevel surfaces;
        surfaces.Build(fixture);
        Check(surfaces.Walls().size() == 3 &&
            surfaces.GetCellArray().GetCell(4, 1).m_waterSlot == WaterSlotIndex::WaterSlot4 &&
            !surfaces.GetCellArray().GetCell(5, 1).IsWaterSurface() &&
            !surfaces.GetCellArray().GetCell(6, 1).IsWaterSurface(),
            "Native selectors use five bits and accept 1..4; 17/20 must not alias water slots 1/4.");
        bool fourthSurface = false;
        for (const auto& instance : surfaces.Walls())
        {
            const auto& dc = instance->GetRenderable()->m_drawCalls.at(0);
            Check(dc.m_material.m_transparencyType == TransparencyMode::Water &&
                dc.m_material.m_reflectionType != WaterReflectionType::Disabled && dc.m_primitiveCount == 2,
                "Only enabled nonzero-depth surface definitions may emit flat water quads.");
            if (dc.m_material.m_reflectionType == WaterReflectionType::Lava)
            {
                fourthSurface = true;
                Check(std::fabs(dc.m_material.m_mirrorHeight + 170.f * DespToFloat) < 0.00001f &&
                    std::fabs(surfaces.GetCellArray().GetGridPos(4, 1, HeightLevel::WaterLevel4).y +
                        170.f * DespToFloat) < 0.00001f,
                    "Fourth-slot vertices and receiver metadata must use the native ten-unit depth.");
            }
        }
        Check(fourthSurface, "The fourth native special surface must generate geometry.");
        std::vector<byte> changedSurfaces(50 * 40, 0);
        for (int row = 0; row < 40; ++row)
        {
            for (int column = 0; column < 50; ++column)
            {
                changedSurfaces[row * 50 + column] = surfaces.GetCellArray().GetCell(column, row).m_walls;
            }
        }
        changedSurfaces[51] = 2;
        Check(surfaces.SynchronizeSurfaceTiles(changedSurfaces.data(), 50, 40, 50) == 1 &&
            surfaces.Walls().size() == 2 &&
            surfaces.GetCellArray().GetCell(1, 1).m_waterSlot == WaterSlotIndex::WaterSlot2,
            "Switching to a disabled nonzero-depth slot must remove the old plane without emitting a replacement.");
        changedSurfaces[51] = 1;
        Check(surfaces.SynchronizeSurfaceTiles(changedSurfaces.data(), 50, 40, 50) == 1 &&
            surfaces.Walls().size() == 3, "Restoring an enabled slot must restore its water geometry.");
        for (auto& slot : fixture.m_raw.m_data.water) { slot.height = 0; }
        SurfaceLevel zeroDepth;
        zeroDepth.Build(fixture);
        Check(zeroDepth.Walls().empty(), "Native depth zero must suppress a flat special-surface polygon.");
        auto* level = FORCE_GET(AssetManager::Instance().LoadLevel(17));
        FlameScene scene;
        FORCE_WAIT(level->PopulateScene(scene, false, false));
        std::size_t shorelineReceivers = 0, flatTriangles = 0;
        for (const auto& instance : scene.GetRenderableInstances())
        {
            if (!instance->HasWaterMaterials()) { continue; }
            bool reflection;
            float height;
            instance->RetrieveWaterProperties(reflection, height);
            const auto& range = instance->GetGlobalBounds().m_tileRange;
            const bool shoreline = instance->HasOpaqueMaterials() &&
                range.m_minX == range.m_maxX && range.m_minZ == range.m_maxZ &&
                level->GetCellArray().IsInside(range.m_minX, range.m_minZ) &&
                (level->GetCellArray().GetCell(range.m_minX, range.m_minZ).m_walls & 31) == 2;
            if (shoreline)
            {
                ++shorelineReceivers;
                const auto& draws = instance->GetRenderable()->m_drawCalls;
                for (int drawIndex = 0; drawIndex < static_cast<int>(draws.size()); ++drawIndex)
                {
                    if (!instance->RetrieveWaterProperties(reflection, height, drawIndex)) { continue; }
                    Check(std::fabs(height + 300.f * DespToFloat) < 0.00001f,
                        "Every modelled shoreline water range must join the flat depth-300 water plane.");
                }
            }
            else if (!instance->HasOpaqueMaterials())
            {
                for (const auto& dc : instance->GetRenderable()->m_drawCalls)
                {
                    if (dc.m_material.m_transparencyType == TransparencyMode::Water) { flatTriangles += dc.m_primitiveCount; }
                }
            }
        }
        Check(shorelineReceivers == 63 && flatTriangles == 38 * 2,
            "Level 17's 63 selector-2 tiles use modelled shoreline water; only its 38 selector-3 tiles emit flat quads.");
        class SurfaceVisibility final : public DefaultVisibilityContext
        {
        public:
            void Collect(VisibilityResult& result, const Scene& scene)
            {
                m_performFrustumClipping = false;
                VisibilitySettings settings;
                settings.filterMask = static_cast<uint32_t>(GamePlay::VisibilityFilter::Walls);
                Frustum frustum;
                DetermineVisibleInstances(result, result.m_visibleInstances, &scene,
                    Vector(0, 10.f, 0), frustum, nullptr, settings, true);
            }
        } visibility;
        VisibilityResult visible;
        visibility.Collect(visible, scene);
        std::size_t receiverCount = 0, expectedRanges = 0;
        for (const auto& instance : scene.GetRenderableInstances())
        {
            for (const auto& dc : instance->GetRenderable()->m_drawCalls)
            {
                if (dc.m_material.m_transparencyType == TransparencyMode::Water) { ++expectedRanges; }
            }
        }
        for (const auto& group : visible.m_visibleWater)
        {
            for (const auto& entry : group.m_waterReceiver)
            {
                ++receiverCount;
                bool reflection;
                float height;
                Check(entry.m_instance->RetrieveWaterProperties(reflection, height, entry.m_drawCallIndex) &&
                    reflection == group.m_useReflection && std::fabs(height - group.m_waterHeight) < 0.05f,
                    "Original level-17 reflection/refraction ranges must retain their own pass and physical plane.");
            }
        }
        Check(receiverCount == expectedRanges, "Every original modelled and flat water range must be submitted exactly once.");
        std::cout << "Water surfaces: four native slots, five-bit selectors, zero-depth/255 suppression, "
            "63 modelled shoreline tiles, 38 flat tiles and per-material reflection/refraction passed.\n";
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
        for (const auto number : { 11, 16, 35 })
        {
            original.StartLevel(static_cast<std::int16_t>(number));
            original.level->wallTextures[0][0] = 0;
            if (number == 35) { original.level->nativeInfo[103] = 0; }
            const auto snapshot = original.CaptureSaveGame("WARP snapshot");
            auto savedLevel = std::make_shared<Level>();
            FORCE_WAIT(savedLevel->LoadSaveGameData(snapshot.data(), static_cast<int>(snapshot.size())));
            Check(savedLevel->GetLevelInfo().m_levelIndex == number && savedLevel->GetRawGameObjects().size() == 163,
                "Saved renderer levels must resolve native ordinals and exclude player, fallback and inventory records.");
            LevelScene savedScene(number, savedLevel);
            savedScene.m_createGameObjects = savedScene.m_createMapIcons = false;
            FORCE_WAIT(savedScene.CreateScene());
            Check(savedScene.m_level == savedLevel.get(),
                "Restored scenes must retain their owned snapshot, not reload cached original geometry.");
            for (int row = 0; row < 40; ++row)
            {
                for (int column = 0; column < 50; ++column)
                {
                    const auto& cell = savedLevel->GetCellArray().GetCell(column, row);
                    Check(cell.m_wallTextures == original.level->wallTextures[row][column] &&
                        std::fabs(cell.m_floorHeight -
                            original.level->floorHeights[row][column] * -2.f * DespToFloat) < 0.00001f,
                        "Snapshot scenes must rebuild the full saved wall and height planes.");
                }
            }
            original.LoadGame(snapshot);
            Check(original.CaptureSaveGame("WARP snapshot") == snapshot,
                "Core and renderer must agree on native snapshot state and resources.");
            savedScene.ClearScene();
            auto corrupt = snapshot;
            corrupt.back() ^= 1;
            bool rejectedSave = false;
            try { FORCE_WAIT(savedLevel->LoadSaveGameData(corrupt.data(), static_cast<int>(corrupt.size()))); }
            catch (const std::runtime_error&) { rejectedSave = true; }
            Check(rejectedSave, "The renderer must reject corrupt snapshots before changing its assets.");
        }
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

    void CheckVisualLightEffects()
    {
        LevelScene scene(23);
        scene.m_createGameObjects = false;
        FORCE_WAIT(scene.CreateScene());
        const auto count = scene.GetLightSources().size();
        class Context final : public Desperabis::GameContext
        {
        public:
            explicit Context(LevelScene& scene) : scene(scene) {}
            void OnLightFlash(const Desperabis::LightFlash& flash) override
            {
                scene.FlashLight(Level::DespToPos(flash.position[0], flash.position[1], flash.position[2]),
                    {flash.color[0],flash.color[1],flash.color[2]}, flash.fadeInSeconds, flash.fadeOutSeconds,
                    flash.radius * DespToFloat);
            }
            void OnFireEmitted(int slot, const std::array<std::int16_t,3>& position) override
            {
                ::RawGameObject raw{};
                Desperabis::EncodeGameObject(worldObjects.at(slot), raw.m_rawBytes, sizeof(raw.m_rawBytes));
                scene.FireEmitted(slot, raw, Level::DespToPos(position[0],position[1],position[2]));
            }
            LevelScene& scene;
        } core(scene);
        const auto position = Level::DespToPos(1000,100,1500);
        core.FlashLight({1000,100,1500}, {255,100,30}, 0.1f, 0.2f, 300.f);
        auto light = scene.GetLightSources().back();
        PackedLightSourceParams params;
        Check(!light->FillLightParams(params) && (light->GetPosition() - position).GetLength() < 0.00001f &&
            std::abs(light->GetRadius() - 300.f * DespToFloat) < 0.00001f,
            "GameContext flash parameters must reach the renderer with correct native position/radius conversion.");
        scene.Update(0.05);
        Check(light->FillLightParams(params), "Flash must fade in before contributing.");
        const float half = params.ColorAndPadding.x;
        scene.Update(0.05);
        light->FillLightParams(params);
        Check(std::abs(params.ColorAndPadding.x - half * 2.f) < 0.00001f, "Flash must reach full brightness at the fade-in boundary.");
        scene.Update(0.1);
        light->FillLightParams(params);
        Check(std::abs(params.ColorAndPadding.x - half) < 0.00001f, "Flash must fade out linearly.");
        std::weak_ptr<LightSource> expired = light;
        light.reset();
        scene.Update(0.11);
        Check(expired.expired() && scene.GetLightSources().size() == count, "Finished flashes must remove themselves and release scene ownership.");
        core.FlashLight({1000,100,1500}, {255,255,255}, 0.f, 0.1f);
        Check(scene.GetLightSources().back()->FillLightParams(params), "Zero fade-in must begin at full brightness.");
        scene.Update(1.0);
        Check(scene.GetLightSources().size() == count, "Large updates must expire flashes without leaving lights behind.");
        core.FlashLight({1000,100,1500}, {255,255,255}, 0.1f, 0.f);
        scene.Update(0.05);
        Check(scene.GetLightSources().back()->FillLightParams(params) &&
            std::abs(params.ColorAndPadding.x - 2.f) < 0.00001f, "Zero fade-out must still support a linear fade-in.");
        scene.Update(1.0);
        for (const auto invalid : { std::array<float,3>{-1.f,1.f,200.f}, {0.f,0.f,200.f},
            {1.f,-1.f,200.f}, {std::nanf(""),1.f,200.f}, {1.f,HUGE_VALF,200.f},
            {0.f,1.f,0.f}, {0.f,1.f,-1.f}, {0.f,1.f,std::nanf("")} })
        {
            CheckRecoverableError<std::invalid_argument>([&]
            {
                core.FlashLight({0,0,0}, {255,255,255}, invalid[0], invalid[1], invalid[2]);
            });
            Check(scene.GetLightSources().size() == count, "Invalid flash parameters must never create lights.");
        }

        auto level = Desperabis::CreateDummyLevel();
        core.level = &level; core.collisionMap = level.collisionMap.get(); core.tickCount = 1;
        auto& emitter = core.worldObjects[2];
        emitter.health = 1000; emitter.behavior = Desperabis::BehaviorType::FireExhauster;
        emitter.posX = 1000; emitter.posY = 100; emitter.posZ = 1500;
        emitter.generalPurpose1 = 1; emitter.groupIndexOrLock = 3; emitter.itemValue = 10; emitter.behaviorParameter = 5;
        Desperabis::DispatchObjectCase(emitter, core);
        Check(scene.GetLightSources().size() == count + 1 &&
            scene.GetLightSources().back()->FillLightParams(params),
            "Actual pipe emission must pulse one owner light, not create one light per fire particle.");
        emitter.generalPurpose1 = 1;
        Desperabis::DispatchObjectCase(emitter, core);
        Check(scene.GetLightSources().size() == count + 1, "Repeated pipe emissions must reuse their owner light.");
        scene.Update(0.21);
        Check(!scene.GetLightSources().back()->FillLightParams(params), "Pipe glow must fade away after emission stops.");

        Model model;
        model.m_drawCalls.emplace_back();
        auto instance = std::make_shared<RenderableInstance>(&model);
        for (int index = 0; index < 3; ++index)
        {
            Desperabis::RawGameObject fireball{};
            fireball.health = 500; fireball.behavior = Desperabis::BehaviorType::FlyingArrow;
            fireball.modelId = 5; fireball.itemValue = 5;
            ::RawGameObject raw{};
            Desperabis::EncodeGameObject(fireball, raw.m_rawBytes, sizeof(raw.m_rawBytes));
            scene.UpdateObjectPresentation(20 + index, raw, instance.get());
            Check(scene.GetLightSources().back()->FillLightParams(params) &&
                instance->GetUsedTransparencyMask() == TransparencyModeMask::AdditiveBit,
                "Each dragon volley fireball must have a dynamic light and additive sprite.");
        }
        Check(scene.GetLightSources().size() == count + 4, "Dragon volley must create precisely three lights.");
        for (const auto behavior : {Desperabis::BehaviorType::FireParticle, Desperabis::BehaviorType::FlyingArrow})
        {
            Desperabis::RawGameObject fire{};
            fire.health = 20; fire.behavior = behavior; fire.modelId = 1; fire.modelAnim = 5; fire.itemValue = 7;
            ::RawGameObject raw{};
            Desperabis::EncodeGameObject(fire, raw.m_rawBytes, sizeof(raw.m_rawBytes));
            scene.UpdateObjectPresentation(30, raw, instance.get());
            Check(instance->GetUsedTransparencyMask() == TransparencyModeMask::AdditiveBit &&
                scene.GetLightSources().size() == count + 4, "Fire particles must be additive without expensive per-particle lights.");
        }
        Desperabis::RawGameObject spark{};
        spark.health = 50; spark.behavior = Desperabis::BehaviorType::BouncingObject;
        spark.modelId = 1; spark.modelAnim = 5; spark.animFrame = 1; spark.generalPurpose1 = 1;
        spark.spriteScale = 24; spark.blendWeightOrSpriteOpacity = 10;
        ::RawGameObject sparkRaw{};
        Desperabis::EncodeGameObject(spark, sparkRaw.m_rawBytes, sizeof(sparkRaw.m_rawBytes));
        scene.UpdateObjectPresentation(31, sparkRaw, instance.get());
        auto corona = scene.GetLightSources().back();
        Check(!corona->FillLightParams(params) && corona->FillCoronaParams(params) &&
            corona->GetGlobalBounds().GetSphereRadius() >= corona->GetCoronaSize(),
            "Corona-only sparks must have nonempty visibility bounds without contributing dynamic lights.");
        const float fullCorona = params.ColorAndPadding.x;
        spark.behavior = Desperabis::BehaviorType::FadingParticle;
        spark.blendWeightOrSpriteOpacity = 2; spark.posX = 1500; spark.spriteScale = 12;
        Desperabis::EncodeGameObject(spark, sparkRaw.m_rawBytes, sizeof(sparkRaw.m_rawBytes));
        scene.UpdateObjectPresentation(31, sparkRaw, instance.get());
        corona->FillCoronaParams(params);
        Check(scene.GetLightSources().back() == corona &&
            std::abs(params.ColorAndPadding.x - fullCorona * 0.5f) < 0.00001f &&
            std::abs(corona->GetCoronaSize() - 24.f * DespToFloat) < 0.00001f &&
            (corona->GetPosition() - Level::DespToPos(1500,0,0)).GetLength() < 0.00001f,
            "Breakup coronas must follow, shrink and fade with their original particle without reallocation.");
        scene.RemoveObjectPresentation(31);
        Desperabis::RawGameObject smoke{};
        smoke.health = 50; smoke.behavior = Desperabis::BehaviorType::FlyingArrow;
        smoke.modelId = 5; smoke.itemValue = 6;
        ::RawGameObject smokeRaw{};
        Desperabis::EncodeGameObject(smoke, smokeRaw.m_rawBytes, sizeof(smokeRaw.m_rawBytes));
        scene.UpdateObjectPresentation(32, smokeRaw);
        Check(!scene.GetObjectPresentation(32) && scene.GetLightSources().size() == count + 4,
            "Dragon death clouds must not accidentally acquire fireball lights or fire-particle overrides.");
        scene.SetElectricityLight(true, position);
        auto electricity = scene.GetLightSources().back();
        Check(electricity->FillLightParams(params) && params.ColorAndPadding.z > params.ColorAndPadding.x &&
            electricity->FillCoronaParams(params), "Electricity must provide one cyan light and tip corona.");
        scene.SetElectricityLight(true, position + Vector(1,0,0));
        Check(scene.GetLightSources().back() == electricity &&
            (electricity->GetPosition() - position - Vector(1,0,0)).GetLength() < 0.00001f,
            "Electricity must follow its tip without allocating a new light every frame.");
        scene.SetElectricityLight(false, position);
        Check(scene.GetLightSources().size() == count + 4, "Releasing electricity must remove its light and corona immediately.");
        scene.ClearScene();
        Check(scene.GetLightSources().empty(), "Scene changes must clear emitter, projectile and transient lights.");
    }

    void CheckGhostPresentations()
    {
        LevelScene scene(20);
        scene.m_createGameObjects = false;
        FORCE_WAIT(scene.CreateScene());
        const auto& objects = scene.m_level->GetRawGameObjects();
        const auto found = std::find_if(objects.begin(), objects.end(), [](const auto& object)
        {
            return GamePlay::PropertyAccessorBase(object).GetBehaviorType() == GamePlay::BehaviorType::Ghost;
        });
        Check(found != objects.end(), "Cathedral regression requires an original ghost fixture.");
        auto raw = *found;
        raw.m_rawShorts[5] = 1040;
        raw.m_rawShorts[17] = 0;
        raw.m_rawShorts[21] = 8;
        auto instance = std::make_shared<RenderableInstance>();
        auto camera = Renderer::Instance().GetOrCreateCamera();
        const auto oldCamera = camera->GetCameraTransformMatrix(0);
        Matrix4 nearCamera;
        const auto position = GamePlay::PropertyAccessorBase(raw).GetPosition();
        nearCamera.SetTranslation(position + Vector(50.f * DespToFloat,0,0));
        camera->SetCameraTransformMatrix(nearCamera);
        scene.m_level->ApplyRenderProperties(instance, raw);
        scene.UpdateObjectPresentation(2, raw, instance.get());
        auto light = scene.GetLightSources().back();
        PackedLightSourceParams params;
        Check(instance->IsValid() && !instance->GetVisible() && !light->FillLightParams(params) &&
            GamePlay::PropertyAccessorBase(raw).GetSpriteTransparency() == 0.f,
            "Dormant original ghosts must be completely invisible and emit no light even beside the player.");
        raw.m_rawShorts[5] = 1000; raw.m_rawShorts[21] = 0;
        scene.m_level->ApplyRenderProperties(instance, raw);
        scene.UpdateObjectPresentation(2, raw, instance.get());
        Check(!instance->GetVisible() && instance->m_materialOverride[0].m_opacity == 0.f,
            "An active ghost at native opacity zero must not retain the generic one-sixth sprite visibility.");
        raw.m_rawShorts[21] = 8;
        scene.m_level->ApplyRenderProperties(instance, raw);
        scene.UpdateObjectPresentation(2, raw, instance.get());
        Check(instance->GetVisible() && light->FillLightParams(params) &&
            params.ColorAndPadding.z > params.ColorAndPadding.x * 4.f &&
            std::abs(params.ColorAndPadding.z - 220.f / 255.f * 2.25f) < 0.00001f &&
            std::abs(params.PositionAndRadius.w - 180.f * DespToFloat) < 0.00001f &&
            !light->FillCoronaParams(params),
            "A nearby active ghost must regain native visibility with a restrained dark-blue dynamic glow.");
        light->FillLightParams(params);
        const float quietBlue = params.ColorAndPadding.z;
        raw.m_rawShorts[17] = 21; raw.m_rawShorts[19] = -10;
        scene.UpdateObjectPresentation(2, raw, instance.get());
        light->FillLightParams(params);
        Check(params.ColorAndPadding.z > quietBlue * 3.f &&
            std::abs(params.ColorAndPadding.z - 6.525f) < 0.00001f &&
            std::abs(params.PositionAndRadius.w - 195.f * DespToFloat) < 0.00001f &&
            scene.GetLightSources().back() == light, "Ghost death must brighten the existing blue light without allocating another.");
        raw.m_rawShorts[17] = 22;
        scene.UpdateObjectPresentation(2, raw, instance.get());
        Check(light->FillLightParams(params) && std::abs(params.ColorAndPadding.z - 6.75f) < 0.00001f &&
            std::abs(params.PositionAndRadius.w - 195.f * DespToFloat) < 0.00001f,
            "The peak ghost death glow must use 75 percent of its original intensity and radius.");
        raw.m_rawShorts[17] = 23; raw.m_rawShorts[19] = 14;
        scene.UpdateObjectPresentation(2, raw, instance.get());
        Check(light->FillLightParams(params) && std::abs(params.ColorAndPadding.z - 3.375f) < 0.00001f &&
            std::abs(params.PositionAndRadius.w - 195.f * DespToFloat) < 0.00001f,
            "The subtler ghost death glow must preserve its final fade curve.");
        nearCamera.SetTranslation(position + Vector(300.f * DespToFloat,0,0));
        camera->SetCameraTransformMatrix(nearCamera);
        scene.Update(0.01);
        Check(!light->FillLightParams(params), "Ghost glow must turn off beyond proximity range even while dying.");
        scene.RemoveObjectPresentation(2);
        Check(scene.GetLightSources().size() == 1, "Ghost removal must remove its light, leaving only the player light.");
        camera->SetCameraTransformMatrix(oldCamera);
        scene.ClearScene();
    }

    void CheckFreshObjectPlacement()
    {
        int raisedKeys = 0, wallMaps = 0;
        for (const auto number : {50, 11})
        {
            LevelScene scene(number);
            scene.m_createGameObjects = false;
            FORCE_WAIT(scene.CreateScene());
            for (const auto& source : scene.m_level->GetRawGameObjects())
            {
                const GamePlay::PropertyAccessorBase properties(source);
                if (properties.GetHealth() <= 0 || (properties.GetCollisionType() != GamePlay::CollisionType::Key &&
                    properties.GetCollisionType() != GamePlay::CollisionType::PictureMap)) { continue; }
                auto raw = source;
                scene.m_level->CorrectInitialObjectPlacement(raw);
                auto instance = std::make_shared<RenderableInstance>();
                scene.m_level->ApplyRenderProperties(instance, raw);
                Check(instance->IsValid(), "Original keys and wall maps must have measurable converted model bounds.");
                const auto& box = instance->GetGlobalBounds().m_box;
                if (properties.GetCollisionType() == GamePlay::CollisionType::Key)
                {
                    const auto floor = std::lround(scene.m_level->GetHeightAtPos(properties.GetPosition()) / DespToFloat);
                    Check(raw.m_rawShorts[7] == floor + 4 &&
                        raw.m_rawShorts[6] == source.m_rawShorts[6] && raw.m_rawShorts[8] == source.m_rawShorts[8],
                        "Fresh ground keys must sit exactly four DOS units above the native-rounded floor at their origin.");
                    raisedKeys += raw.m_rawShorts[7] > source.m_rawShorts[7];
                }
                else
                {
                    Check(number == 11 && raw.m_rawShorts[6] < source.m_rawShorts[6] &&
                        box.m_max.GetX() <= scene.m_level->GetCellArray().GridPosX(23) - 2.f * DespToFloat + 0.00001f &&
                        raw.m_rawShorts[7] == source.m_rawShorts[7] && raw.m_rawShorts[8] == source.m_rawShorts[8],
                        "The original level-11 map frame must remain on its wall side with at least two units of model clearance.");
                    ++wallMaps;
                }
                const auto once = raw;
                scene.m_level->CorrectInitialObjectPlacement(raw);
                Check(raw == once, "Fresh placement correction must be idempotent, not accumulate offsets.");
                auto attached = source;
                attached.m_rawBytes[4] = 20;
                const auto originalAttached = attached;
                scene.m_level->CorrectInitialObjectPlacement(attached);
                Check(attached == originalAttached, "Attached objects must retain their native parent-relative coordinates.");
                auto inactive = source;
                inactive.m_rawShorts[5] = 0;
                const auto originalInactive = inactive;
                scene.m_level->CorrectInitialObjectPlacement(inactive);
                Check(inactive == originalInactive, "Inactive objects must not receive placement corrections.");
                if (properties.GetCollisionType() == GamePlay::CollisionType::Key)
                {
                    auto raised = once;
                    raised.m_rawShorts[7] += 32;
                    scene.m_level->CorrectInitialObjectPlacement(raised);
                    Check(raised.m_rawShorts[7] == once.m_rawShorts[7] + 32,
                        "Elevated keys must retain their authored height rather than being moved down to terrain.");
                    for (const auto collision : {GamePlay::CollisionType::BurnableOilPuddle, GamePlay::CollisionType::ActivateSpiders})
                    {
                        auto puddle = source;
                        puddle.m_rawBytes[6] = static_cast<byte>(collision);
                        puddle.m_rawShorts[7] = 32000;
                        scene.m_level->CorrectInitialObjectPlacement(puddle);
                        Check(puddle.m_rawShorts[7] == once.m_rawShorts[7] - 2 &&
                            puddle.m_rawShorts[6] == source.m_rawShorts[6] && puddle.m_rawShorts[8] == source.m_rawShorts[8],
                            "Explicit polygonal puddles must use origin-floor plus two units, regardless of bounds or previous Y.");
                    }
                }
            }
        }
        Check(raisedKeys >= 2 && wallMaps == 1, "Placement tests must exercise the buried tutorial keys and original wall map.");
        {
            LevelScene swamp(24);
            swamp.m_createGameObjects = false;
            FORCE_WAIT(swamp.CreateScene());
            auto windmillKey = swamp.m_level->GetRawGameObjects().at(39);
            const auto original = windmillKey;
            Check(original.m_rawShorts[7] == 29 && original.m_rawShorts[5] == 1000 &&
                GamePlay::PropertyAccessorBase(original).GetBehaviorType() == GamePlay::BehaviorType::None,
                "The windmill key must be the active, elevated original slot 41.");
            swamp.m_level->CorrectInitialObjectPlacement(windmillKey);
            auto instance = std::make_shared<RenderableInstance>();
            swamp.m_level->ApplyRenderProperties(instance, windmillKey);
            Check(windmillKey == original && instance->IsValid() && instance->GetVisible() &&
                windmillKey.m_rawShorts[7] > swamp.m_level->GetHeightAtPos(
                    GamePlay::PropertyAccessorBase(windmillKey).GetPosition()) / DespToFloat + 100.f,
                "Fresh swamp loading must preserve and render the ladder-height key, not drop it under the windmill.");
            auto shovelKey = swamp.m_level->GetRawGameObjects().at(15);
            const auto originalShovelKey = shovelKey;
            swamp.m_level->CorrectInitialObjectPlacement(shovelKey);
            Check(shovelKey == originalShovelKey, "The separate shovel-retrieved key must remain unchanged.");
        }

        const auto tutorial = FORCE_GET(AssetManager::Instance().LoadLevel(50));
        const auto originalRecords = tutorial->GetRawGameObjects();
        const auto keyTemplate = std::find_if(originalRecords.begin(), originalRecords.end(), [](const RawGameObject& raw)
        {
            return GamePlay::PropertyAccessorBase(raw).GetCollisionType() == GamePlay::CollisionType::Key;
        });
        Check(keyTemplate != originalRecords.end(), "The original tutorial must supply a polygonal key record.");
        LevelScene legacy(50);
        legacy.m_createMapIcons = false;
        FORCE_WAIT(legacy.CreateScene());
        int legacyKeys = 0;
        for (const auto& object : legacy.GetGameObjects())
        {
            const auto levelObject = std::dynamic_pointer_cast<LevelGameObject>(object);
            if (!levelObject || GamePlay::PropertyAccessorBase(levelObject->m_rawGO).GetCollisionType() !=
                GamePlay::CollisionType::Key) { continue; }
            Check(levelObject->m_renderInst && levelObject->m_rawGO.m_rawShorts[7] ==
                std::lround(legacy.m_level->GetHeightAtPos(levelObject->GetPosition()) / DespToFloat) + 4,
                "Renderer-driven level creation must also place tutorial key origins at floor plus four DOS units.");
            ++legacyKeys;
        }
        Check(legacyKeys == 3 && tutorial->GetRawGameObjects() == originalRecords,
            "Legacy scene placement must correct all tutorial keys without mutating cached authored object records.");

        class FloorLevel final : public Level
        {
        public:
            void Initialize(const Level& source)
            {
                InitLevelInfo(source.GetLevelInfo());
                m_cellArray = source.GetCellArray();
            }
            using Level::AddStaticCellObject;
        };
        class SaveHost final : public Desperabis::GameContext
        {
            std::vector<std::uint8_t> LoadFile(std::string_view filename) override
            {
                const std::wstring name(filename.begin(), filename.end());
                return FORCE_GET(FileManager::Instance().ReadDataFuture(FileManager::Instance().FilenameToNative(name)));
            }
            void LoadLevel(const Desperabis::LevelLoadRequest& request) override
            {
                Desperabis::LevelLoadData data;
                LoadLevelGeometry(request.levelNumber, data);
                LoadLevelObjects(request.levelNumber, data);
                InitializeLevelPlayerSpawn(data, {0,0,100});
                CompleteLevelLoad(request.id, std::move(data));
            }
        } host;
        host.StartLevel(10);
        const auto bytes = host.CaptureSaveGame("Placement regression");
        FloorLevel saved;
        FORCE_WAIT(saved.LoadSaveGameData(bytes.data(), static_cast<int>(bytes.size())));
        for (const auto& source : saved.GetRawGameObjects())
        {
            auto raw = source;
            saved.CorrectInitialObjectPlacement(raw);
            Check(raw == source, "Actual SPL-loaded world objects must remain byte-for-byte unchanged by placement policy.");
        }
        LevelScene scene(10);
        scene.m_createGameObjects = false;
        FORCE_WAIT(scene.CreateScene());
        FloorLevel fresh;
        fresh.Initialize(*scene.m_level);
        int puddles = 0, reflectiveTiles = 0, thickReflectiveTiles = 0;
        const auto& cells = scene.m_level->GetCellArray();
        for (int z = 0; z < cells.m_sizeZ; ++z)
        {
            for (int x = 0; x < cells.m_sizeX; ++x)
            {
                const auto raw = cells.GetCell(x, z).m_floor;
                if (raw <= 31) { continue; }
                const auto restored = saved.AddStaticCellObject(x, z, raw, HeightLevel::Floor);
                const auto corrected = fresh.AddStaticCellObject(x, z, raw, HeightLevel::Floor);
                Check(restored && corrected, "Authored polygonal floor models must exist in both fresh and saved scenes.");
                if (restored->HasWaterMaterials())
                {
                    Check(!cells.GetCell(x, z).IsWaterSurface(),
                        "Level-10 reflective puddle models must be distinct from grid water surfaces.");
                    const auto position = cells.GetCenterPos(x, z, HeightLevel::Floor);
                    const auto origin = corrected->GetTransform().GetTranslation();
                    const auto controlled = fresh.AddStaticCellObject(x, z, raw, HeightLevel::Floor, false);
                    Check(std::abs(restored->GetTransform().GetTranslation().GetY() - position.y) < 0.00001f &&
                        controlled && std::abs(controlled->GetTransform().GetTranslation().GetY() - position.y) < 0.00001f &&
                        std::abs(origin.GetY() - fresh.GetHeightAtPos(origin) - 2.f * DespToFloat) < 0.00001f,
                        "Every actual level-10 mirror-receiver puddle must use floor plus two units; saved and controlled-terrain anchors stay unchanged.");
                    const auto& bounds = restored->GetGlobalBounds().m_box;
                    thickReflectiveTiles += bounds.m_max.GetY() - bounds.m_min.GetY() > 2.f * DespToFloat;

                    auto object = *keyTemplate;
                    object.m_rawBytes[0] = raw >= 160 ? 33 : 32;
                    object.m_rawBytes[1] = (raw & 31) / 5 + 1;
                    object.m_rawBytes[2] = 0;
                    object.m_rawBytes[3] = (raw & 31) % 5 + 1;
                    object.m_rawBytes[6] = static_cast<byte>(GamePlay::CollisionType::None);
                    object.m_rawBytes[8] = static_cast<byte>(GamePlay::BehaviorType::None);
                    const auto nativeZero = Level::DespToPos(0, 0, 0);
                    object.m_rawShorts[6] = static_cast<short>(std::lround((origin.GetX() - nativeZero.GetX()) / DespToFloat));
                    object.m_rawShorts[7] = 32000;
                    object.m_rawShorts[8] = static_cast<short>(std::lround((nativeZero.GetZ() - origin.GetZ()) / DespToFloat));
                    const auto originalObject = object;
                    auto instance = std::make_shared<RenderableInstance>();
                    scene.m_level->ApplyRenderProperties(instance, object);
                    Check(instance->IsValid() && instance->HasWaterMaterials(),
                        "The original reflective puddle model must retain mirror-receiver materials when instantiated as a behaviorless object.");
                    scene.m_level->CorrectInitialObjectPlacement(object);
                    Check(object.m_rawShorts[7] == std::lround(fresh.GetHeightAtPos(origin) / DespToFloat) + 2 &&
                        object.m_rawShorts[6] == originalObject.m_rawShorts[6] &&
                        object.m_rawShorts[8] == originalObject.m_rawShorts[8],
                        "Behaviorless world objects must use model mirror-receiver materials to reset their origins to floor plus two units.");
                    const auto once = object;
                    scene.m_level->CorrectInitialObjectPlacement(object);
                    Check(object == once, "Material-identified puddle placement must be idempotent.");
                    auto savedObject = originalObject;
                    saved.CorrectInitialObjectPlacement(savedObject);
                    Check(savedObject == originalObject, "Saved material-identified puddles must not receive floor correction.");
                    ++reflectiveTiles;
                }
                if (cells.GetCell(x, z).IsWaterSurface()) { continue; }
                const auto& box = restored->GetGlobalBounds().m_box;
                if (box.m_max.GetY() - box.m_min.GetY() > 2.f * DespToFloat) { continue; }
                const auto position = cells.GetCenterPos(x, z, HeightLevel::Floor);
                Check(std::abs(restored->GetTransform().GetTranslation().GetY() - position.y) < 0.00001f &&
                    std::abs(corrected->GetTransform().GetTranslation().GetY() -
                        fresh.GetHeightAtPos(corrected->GetTransform().GetTranslation()) - 2.f * DespToFloat) < 0.00001f,
                    "Fresh polygonal puddle origins must equal floor plus two units; SPL-loaded tile models retain their saved placement.");
                ++puddles;
            }
        }
        Check(puddles > 0, "The placement regression must cover actual level-10 polygonal puddles, not water quads or sprites.");
        Check(reflectiveTiles > 0 && thickReflectiveTiles > 0,
            "Placement tests must cover actual behaviorless level-10 mirror-receiver models exceeding the old two-unit thickness limit.");
        std::cout << "Fresh placement: " << raisedKeys << " raised keys, level-11 wall frame and " << puddles <<
            " thin polygonal floor decals, " << reflectiveTiles << " mirror-receiver puddles (" << thickReflectiveTiles <<
            " thicker than two units); behaviorless world-object classification, idempotence and SPL preservation passed.\n";
    }

    void CheckNativeObjectRotations()
    {
        const auto nativeOffset = [](const RawGameObject& object, const std::array<int16_t, 3>& vertex)
        {
            const double radians = 3.14159265358979323846 / 256.0;
            const double pitch = (object.m_rawShorts[14] & 511) * radians;
            const double yaw = (object.m_rawShorts[15] & 511) * radians;
            const double roll = (object.m_rawShorts[16] & 511) * radians;
            // Direct transcription of 1E48:9E02..9E9F and A136..A291,
            // before projection, translation and the uniform vertex scale.
            const double y1 = vertex[1] * std::cos(pitch) - vertex[2] * std::sin(pitch);
            const double z1 = vertex[2] * std::cos(pitch) + vertex[1] * std::sin(pitch);
            const double x2 = vertex[0] * std::cos(roll) - y1 * std::sin(roll);
            const double y2 = y1 * std::cos(roll) + vertex[0] * std::sin(roll);
            return std::array<double, 3>{ x2 * std::cos(yaw) - z1 * std::sin(yaw),
                -y2, -(z1 * std::cos(yaw) + x2 * std::sin(yaw)) };
        };
        const std::array<std::array<int16_t, 3>, 4> vertices{{
            {{1000,0,0}}, {{0,1000,0}}, {{0,0,1000}}, {{-700,430,210}}
        }};
        Desperabis::ModelAnchorGeometry anchors;
        anchors.animations.resize(1);
        anchors.animations[0].resize(1);
        auto& frame = anchors.animations[0][0];
        frame.vertices.assign(vertices.begin(), vertices.end());
        frame.linkedObjectVertices = {1,2,3,4};
        const auto check = [&](const RawGameObject& raw)
        {
            const auto before = raw;
            const GamePlay::PropertyAccessorBase props(raw);
            const auto unscaled = props.GetTransform();
            const auto scaled = props.GetScaledTransform();
            auto object = Desperabis::DecodeGameObject(raw.m_rawBytes, 54);
            object.modelAnim = object.animFrame = 1;
            object.modelFrame2 = 0;
            for (std::size_t i = 0; i < vertices.size(); ++i)
            {
                const auto& vertex = vertices[i];
                const auto expected = nativeOffset(raw, vertex);
                const Vector local(-vertex[0] * DespVertToFloat, -vertex[1] * DespVertToFloat,
                    vertex[2] * DespVertToFloat);
                const auto offset = Vector(static_cast<float>(expected[0]) * DespVertToFloat,
                    static_cast<float>(expected[1]) * DespVertToFloat,
                    static_cast<float>(expected[2]) * DespVertToFloat);
                auto scaledOffset = offset;
                scaledOffset *= props.GetScale();
                Check((unscaled.TransformPosition(local) - (props.GetPosition() + offset)).GetLength() < 0.0001f &&
                    (scaled.TransformPosition(local) - (props.GetPosition() + scaledOffset)).GetLength() < 0.0001f,
                    "Both world-object matrices must match native X/Z/Y arithmetic for every combined angle.");
                const auto point = anchors.Sample(object, false, static_cast<uint16_t>(i + 1));
                Check(point && (*point)[0] == Desperabis::WrapSignedWord(object.posX +
                        static_cast<int>(std::lround(expected[0] / 10))) &&
                    (*point)[1] == Desperabis::WrapSignedWord(object.posY +
                        static_cast<int>(std::lround(expected[1] / 10))) &&
                    (*point)[2] == Desperabis::WrapSignedWord(object.posZ -
                        static_cast<int>(std::lround(expected[2] / 10))),
                    ("Core attachments must match native rotation: angles " + std::to_string(object.winX) + "," +
                        std::to_string(object.winY) + "," + std::to_string(object.winZ) + " vertex " +
                        std::to_string(i) + " actual " + (point ? std::to_string((*point)[0]) + "," +
                        std::to_string((*point)[1]) + "," + std::to_string((*point)[2]) : "missing") +
                        " offset " + std::to_string(expected[0]) + "," + std::to_string(expected[1]) + "," +
                        std::to_string(expected[2])).c_str());
            }
            Check(std::memcmp(before.m_rawBytes, raw.m_rawBytes, 54) == 0,
                "Rotation conversion must not rewrite native angles or positions.");
        };
        RawGameObject raw{};
        raw.m_rawBytes[1] = raw.m_rawBytes[3] = 1;
        raw.m_rawShorts[6] = 1200;
        raw.m_rawShorts[7] = 40;
        raw.m_rawShorts[8] = 2300;
        raw.m_rawUShorts[26] = 40960;
        const int16_t angles[] = {0,32,105,128,255,256,384,496,511,-16,-128,-32768,32767};
        for (auto pitch : angles)
        {
            for (auto yaw : angles)
            {
                for (auto roll : angles)
                {
                    raw.m_rawShorts[14] = pitch;
                    raw.m_rawShorts[15] = yaw;
                    raw.m_rawShorts[16] = roll;
                    check(raw);
                }
            }
        }
        for (int level : {23,97})
        {
            const auto records = FORCE_GET(FileManager::Instance().ReadDataFuture(
                FileManager::Instance().FilenameToNative(L"LEVEL\\OBJPOS" + std::to_wstring(level) + L".DAT")));
            const int slot = level == 23 ? 29 : 2;
            std::memcpy(raw.m_rawBytes, records.data() + (slot - 2) * 54, 54);
            Check(raw.m_rawShorts[14] == 0 && raw.m_rawShorts[15] == (level == 23 ? 256 : 105) &&
                raw.m_rawShorts[16] == (level == 23 ? 128 : 496),
                "Rotation regressions must use the authored mine slab and meteor angle triples.");
            check(raw);
            if (level != 23) { continue; }
            for (int16_t roll = 128; roll >= 0; --roll)
            {
                raw.m_rawShorts[16] = roll;
                check(raw);
                const auto raisedEnd = GamePlay::PropertyAccessorBase(raw).GetTransform().TransformPosition(
                    Vector(1000 * DespVertToFloat, 0, 0));
                Check(raisedEnd.GetY() >= GamePlay::PropertyAccessorBase(raw).GetPosition().GetY() - 0.00001f,
                    "The mine slab's raised end must fall toward the ground, never rotate underneath it.");
            }
        }
        const auto witchRecords = FORCE_GET(FileManager::Instance().ReadDataFuture(
            FileManager::Instance().FilenameToNative(L"LEVEL\\OBJPOS90.DAT")));
        std::memcpy(raw.m_rawBytes, witchRecords.data() + (5 - 2) * 54, 54);
        Check(raw.m_rawShorts[15] == 310 &&
            GamePlay::PropertyAccessorBase(raw).GetBehaviorType() == GamePlay::BehaviorType::WitchHead,
            "The facing regression must use the original witch head, not a symmetric test mesh.");
        const std::array<std::array<int16_t, 2>, 4> listenerOffsets{{
            {{100,0}}, {{0,100}}, {{-100,0}}, {{0,-100}}
        }};
        for (const auto& target : listenerOffsets)
        {
            raw.m_rawShorts[15] = Desperabis::GetVectorAngle(target[0], target[1]);
            const GamePlay::PropertyAccessorBase head(raw);
            auto facing = head.GetTransform().TransformPosition(Vector(0,0,-1)) - head.GetPosition();
            facing.Set(facing.GetX(), 0, facing.GetZ(), 0);
            facing.Normalize();
            Check(std::abs(facing.GetX() - target[0] / 100.f) < 0.00001f &&
                std::abs(facing.GetZ() + target[1] / 100.f) < 0.00001f,
                "The witch's rendered face must point toward the listener selected by native heading math in all four directions.");
        }
        std::cout << "Object rotations: 2197 combined/signed angle triples, native basis/attachment agreement, "
            "authored meteor, every mine-slab roll and witch/listener cardinal facing passed.\n";
    }

    void CheckObjectPresentations()
    {
        CheckNativeObjectRotations();
        static_assert(LevelScene::ObjectSlotCount == std::tuple_size_v<decltype(Desperabis::GameContext::worldObjects)>);
        {
            RawGameObject head{};
            head.m_rawShorts[4] = static_cast<int16_t>(GamePlay::BehaviorType::WitchHead);
            head.m_rawShorts[14] = 3;
            const float attentive = GamePlay::PropertyAccessorBase(head).GetEuler().GetY();
            head.m_rawShorts[14] = 9;
            const float resting = GamePlay::PropertyAccessorBase(head).GetEuler().GetY();
            Check(std::abs(attentive - resting - 6.f * Math::Pi / 256.f) < 0.00001f &&
                head.m_rawShorts[14] == 9,
                "Euler conversion must preserve the six-unit nod supplied by the witch behavior.");
            head.m_rawShorts[4] = 0;
            Check(GamePlay::PropertyAccessorBase(head).GetEuler().GetY() == resting,
                "Euler conversion must be identical for witch heads and all other object behaviors.");
        }
        {
            LevelScene forest(25);
            FORCE_WAIT(forest.CreateScene());
            const auto& objects = forest.m_level->GetRawGameObjects();
            RawGameObject anchor = objects.at(22);
            Check(anchor.m_objectNumber + 2 == 24 && anchor.m_rawShorts[6] == 1258 &&
                anchor.m_rawShorts[7] == -104 && anchor.m_rawShorts[8] == 2723,
                "The toxic-green crater glow must use the original persistent level-25 emitter in native slot 24.");
            int emitters = 0;
            for (const auto& object : objects)
            {
                if (GamePlay::PropertyAccessorBase(object).GetBehaviorType() != GamePlay::BehaviorType::SpawnInCrater) { continue; }
                const int slot = object.m_objectNumber + 2;
                Check(static_cast<bool>(forest.GetObjectPresentation(slot)) == (slot == 24),
                    "Only the selected crater emitter may own a toxic-green light presentation.");
                ++emitters;
            }
            Check(emitters == 28, "Crater presentation coverage must include all 28 original smoke emitters.");
            const auto originalContext = forest.GetObjectPresentation(24);
            std::shared_ptr<PulsatingLight> glow;
            int pulses = 0;
            for (const auto& light : forest.GetLightSources())
            {
                if (auto pulse = std::dynamic_pointer_cast<PulsatingLight>(light)) { glow = pulse; ++pulses; }
            }
            Check(originalContext && glow && pulses == 1,
                "The crater must reuse the existing PulsatingLight and per-object context, exactly once.");
            const auto position = GamePlay::PropertyAccessorBase(anchor).GetPosition() + Vector(0, 5.f * DespToFloat, 0);
            Check(glow->GetRadius() == 250.f * DespToFloat && glow->GetCoronaSize() == 0.f &&
                std::abs(glow->GetPosition().GetX() - position.GetX()) < 0.00001f &&
                std::abs(glow->GetPosition().GetY() - position.GetY()) < 0.00001f &&
                std::abs(glow->GetPosition().GetZ() - position.GetZ()) < 0.00001f,
                "The smaller green glow must remain local, inside the crater, without a floating corona.");
            float minimum = (std::numeric_limits<float>::max)(), maximum = 0.f;
            for (int step = 0; step < 64; ++step)
            {
                glow->Update(0.1);
                PackedLightSourceParams packed{};
                Check(glow->FillLightParams(packed) && packed.ColorAndPadding.y > packed.ColorAndPadding.x * 2.f &&
                    packed.ColorAndPadding.x > packed.ColorAndPadding.z * 4.f,
                    "Every pulse phase must remain toxic green, with enough red to affect the crater albedo.");
                minimum = (std::min)(minimum, packed.ColorAndPadding.y);
                maximum = (std::max)(maximum, packed.ColorAndPadding.y);
            }
            Check(minimum >= 0.5999f && minimum < 0.7f && maximum > 5.3f && maximum <= 5.4001f,
                "The crater must retain its reduced twofold brightness throughout pulse updates, spanning 0.6..5.4.");
            const auto lightCount = forest.GetLightSources().size();
            for (int frame = 0; frame < 20; ++frame) { forest.UpdateObjectPresentation(24, anchor); }
            Check(forest.GetObjectPresentation(24) == originalContext && forest.GetLightSources().size() == lightCount,
                "Object synchronization must retain the pulse phase and never duplicate the crater light.");
            LevelScene unrelated(24);
            unrelated.UpdateObjectPresentation(24, anchor);
            Check(!unrelated.GetObjectPresentation(24) && unrelated.GetLightSources().empty(),
                "The crater decoration must not attach to the same slot or behavior in another level.");
            anchor.m_rawShorts[5] = 0;
            forest.UpdateObjectPresentation(24, anchor);
            Check(!forest.GetObjectPresentation(24) && forest.GetLightSources().size() == lightCount - 1,
                "Removing the emitter must remove its light through the existing presentation lifecycle.");
            anchor = objects.at(22);
            forest.UpdateObjectPresentation(24, anchor);
            Check(forest.GetObjectPresentation(24) != originalContext && forest.GetLightSources().size() == lightCount,
                "Restoring the emitter must create exactly one new crater light.");
            forest.ClearScene();
            Check(!forest.GetObjectPresentation(24) && forest.GetLightSources().empty(),
                "Scene cleanup must release the crater presentation and light.");
        }
        {
            LevelScene swamp(24);
            swamp.m_createGameObjects = false;
            FORCE_WAIT(swamp.CreateScene());
            Desperabis::GameContext core;
            auto geometry = Desperabis::CreateDummyLevel();
            core.level = &geometry;
            int clouds = 0;
            int hiddenClouds = 0;
            for (const auto& authored : swamp.m_level->GetRawGameObjects())
            {
                const GamePlay::PropertyAccessorBase properties(authored);
                if (properties.GetBehaviorType() != GamePlay::BehaviorType::FogParticle || properties.GetHealth() <= 0) { continue; }
                if (properties.GetScale() == 0.f)
                {
                    ++hiddenClouds;
                    continue;
                }
                Desperabis::RawGameObject native;
                std::memcpy(&native, authored.m_rawBytes, sizeof(native));
                auto instance = std::make_shared<RenderableInstance>();
                float previousOpacity = 0.f;
                for (int distance = 0; distance <= 500; ++distance)
                {
                    core.soundListenerX = static_cast<std::int16_t>(native.posX + distance);
                    core.soundListenerZ = native.posZ;
                    Desperabis::DispatchObjectCase(native, core);
                    RawGameObject raw = authored;
                    std::memcpy(raw.m_rawBytes, &native, sizeof(native));
                    swamp.m_level->ApplyRenderProperties(instance, raw);
                    Check(instance->IsValid() && !instance->m_materialOverride.empty(),
                        "Every original swamp cloud must have a render instance.");
                    const auto& material = instance->m_materialOverride[0];
                    const float expected = distance <= 50 ? 0.f : distance >= 370 ? 0.4f : (distance - 50) * 0.00125f;
                    Check(material.m_transparencyType == TransparencyMode::AlphaBlended &&
                        material.m_opacity >= 0.f && material.m_opacity <= 0.4f &&
                        std::abs(material.m_opacity - expected) < 0.00001f &&
                        (distance <= 50 || distance > 370 || material.m_opacity > previousOpacity),
                        "Swamp clouds must fade at every distance unit, remaining within zero to 40% opacity.");
                    previousOpacity = material.m_opacity;
                }
                ++clouds;
            }
            Check(clouds == 28 && hiddenClouds == 1,
                "Renderer opacity coverage must include all 28 visible swamp clouds, preserving the zero-size dummy.");
            RawGameObject ordinary{};
            ordinary.m_rawBytes[51] = static_cast<byte>(GamePlay::DisplayType::SpriteTransparent);
            ordinary.m_rawShorts[21] = 3;
            Check(GamePlay::PropertyAccessorBase(ordinary).GetSpriteTransparency() == 4.f / 6.f,
                "The full-resolution fog override must not change other sprites' native transparency bands.");
        }
        {
            LevelScene scene(23);
            FORCE_WAIT(scene.CreateScene());
            std::size_t lights = 1;
            int firstLightSlot = 0;
            int fireplaces = 0;
            for (const auto& object : scene.m_level->GetRawGameObjects())
            {
                const GamePlay::PropertyAccessorBase properties(object);
                const auto behavior = properties.GetBehaviorType();
                const bool hasLight = properties.GetHealth() > 0 &&
                    (behavior == GamePlay::BehaviorType::LightSource ||
                        behavior == GamePlay::BehaviorType::PulsatingLight ||
                        behavior == GamePlay::BehaviorType::BurningFireplace ||
                        behavior == GamePlay::BehaviorType::FireExhauster);
                const int slot = object.m_objectNumber + 2;
                if (behavior == GamePlay::BehaviorType::BurningFireplace && properties.GetHealth() > 0)
                {
                    FlameScene fireplace;
                    const auto presentation = GamePlay::CreateObjectPresentation(fireplace, 23, slot, object, nullptr);
                    Check(presentation && fireplace.GetLightSources().size() == 1,
                        "Each original mine fireplace must own exactly one light.");
                    const auto light = fireplace.GetLightSources().front();
                    light->Update(0.1);
                    PackedLightSourceParams dimmed{}, original{};
                    Check(light->FillLightParams(dimmed) &&
                        dimmed.PositionAndRadius.w == 450.f * DespToFloat && dimmed.ColorAndPadding.w == 1.f,
                        "Dimmed mine fireplaces must retain their radius, flicker and specularity.");
                    light->SetColor({255,230,170}, 3.f);
                    Check(light->FillLightParams(original) &&
                        std::abs(original.ColorAndPadding.x - dimmed.ColorAndPadding.x * 2.f) < 0.00001f &&
                        std::abs(original.ColorAndPadding.y - dimmed.ColorAndPadding.y * 2.f) < 0.00001f &&
                        std::abs(original.ColorAndPadding.z - dimmed.ColorAndPadding.z * 2.f) < 0.00001f,
                        "Every original mine fireplace must emit exactly half its previous light at the same flicker phase.");
                    ++fireplaces;
                }
                Check(static_cast<bool>(scene.GetObjectPresentation(slot)) == hasLight,
                    "Level conversion must associate each existing object light with its native slot.");
                if (hasLight) { ++lights; firstLightSlot = slot; }
            }
            Check(lights > 1 && scene.GetLightSources().size() == lights,
                "Converted lights must retain their appearance without duplicate fireplace lights.");
            Check(fireplaces == 5, "The brightness regression must cover all five original mine fireplaces.");
            std::weak_ptr<ObjectPresentationContext> removed = scene.GetObjectPresentation(firstLightSlot);
            scene.RemoveObjectPresentation(firstLightSlot);
            Check(removed.expired() && scene.GetLightSources().size() == lights - 1,
                "Removing a converted object must release both its context and scene light.");
            scene.ClearScene();
            Check(scene.GetLightSources().empty() && !scene.GetObjectPresentation(firstLightSlot),
                "Scene reset must clear every object presentation and light.");
        }

        LevelScene scene(23);
        class Context final : public Desperabis::GameContext
        {
        public:
            explicit Context(LevelScene& scene) : scene(scene) {}
            void Sync(int slot)
            {
                ::RawGameObject object{};
                Desperabis::EncodeGameObject(GetObjectRenderState(slot), object.m_rawBytes, sizeof(object.m_rawBytes));
                scene.UpdateObjectPresentation(slot, object);
            }
            void OnObjectCreated(int slot) override { Sync(slot); }
            void OnObjectDestroyed(int slot) override { scene.RemoveObjectPresentation(slot); }
            LevelScene& scene;
        } host(scene);
        auto level = Desperabis::CreateDummyLevel();
        host.level = &level;
        host.collisionMap = level.collisionMap.get();
        level.heightSurfaces[0] = Desperabis::Level::HeightSurface{0,0};
        std::memset(level.ceilingObjects, 0, sizeof(level.ceilingObjects));
        auto& player = host.worldObjects[1];
        player.health = 1000;
        player.posX = player.posZ = 1000;
        player.posY = 120;
        player.velocityX = 100;
        host.selectedWeapon = host.requestedWeapon = Desperabis::WeaponType::Fireball;
        host.weaponHand = host.requestedWeaponHand = 2;
        host.weaponReady = true;
        auto& held = host.inventory[1];
        held.health = 1000;
        held.collisionType = Desperabis::CollisionType::Weapon;
        held.itemValue = static_cast<std::int16_t>(Desperabis::WeaponType::Fireball);
        held.generalPurpose1 = 2;
        host.weaponAmounts[4] = 2;
        host.RefreshWeaponInventory();
        auto* renderLevel = FORCE_GET(AssetManager::Instance().LoadLevel(11));
        const auto fireballFrames = FORCE_GET(AssetManager::Instance().LoadAsset<TextureAnim>(L"ANIMS\\SCHUSS1.AN2"));
        auto present = [&](int slot, const RenderableInstancePtr& instance, Texture* texture = nullptr)
        {
            ::RawGameObject object{};
            Desperabis::EncodeGameObject(host.GetObjectRenderState(slot), object.m_rawBytes, sizeof(object.m_rawBytes));
            object.m_level = renderLevel;
            renderLevel->ApplyRenderProperties(instance, object, texture);
            scene.UpdateObjectPresentation(slot, object, instance.get());
        };
        Check(host.AttackWithSelectedWeapon() == Desperabis::WeaponActionStatus::Applied &&
            scene.GetObjectPresentation(2) && scene.GetLightSources().size() == 1,
            "The actual fireball weapon factory must create its renderer light through the lifecycle hook.");
        std::weak_ptr<ObjectPresentationContext> original = scene.GetObjectPresentation(2);
        std::weak_ptr<LightSource> originalLight = scene.GetLightSources()[0];
        auto& ball = host.worldObjects[2];
        auto ballInstance = std::make_shared<RenderableInstance>(nullptr);
        present(2, ballInstance, &fireballFrames->m_frames[0]);
        Check(ballInstance->GetUsedTransparencyMask() == TransparencyModeMask::AdditiveBit &&
            ballInstance->m_materialOverride[0].m_texture == &fireballFrames->m_frames[0],
            "The creation context must apply additive blending when the fireball render instance becomes available.");
        host.tickCount = 12;
        Desperabis::DispatchObjectCase(ball, host);
        host.tickCount = 1;
        Desperabis::DispatchObjectCase(ball, host);
        host.Sync(2);
        present(2, ballInstance, &fireballFrames->m_frames[1]);
        Check(ballInstance->GetUsedTransparencyMask() == TransparencyModeMask::AdditiveBit &&
            ballInstance->m_materialOverride[0].m_texture == &fireballFrames->m_frames[1],
            "Fireball frame/material refreshes must preserve additive blending and the selected texture.");
        const auto position = Level::DespToPos(ball.posX, ball.posY, ball.posZ);
        Check(scene.GetLightSources().size() == 1 &&
            (scene.GetLightSources()[0]->GetPosition() - position).GetLength() < 0.00001f &&
            scene.GetObjectPresentation(2) == original.lock(),
            "A flying fireball must move its existing light, not allocate one per update.");
        Check(std::abs(scene.GetLightSources()[0]->GetGlobalBounds().m_box.m_min.GetX() -
            (position.GetX() - scene.GetLightSources()[0]->GetRadius())) < 0.00001f,
            "Moving fireball lights must update visibility bounds.");
        for (int step = 0; step < 20; ++step) { Desperabis::DispatchObjectCase(ball, host); }
        Check(ball.health == 0 && !scene.GetObjectPresentation(2) && original.expired() && originalLight.expired() &&
            scene.GetLightSources().empty(),
            "Native projectile expiry must immediately remove its light without a presentation-sync pass.");

        auto exploding = ball;
        exploding.health = 38;
        host.ReplaceWorldObject(2, exploding);
        host.DamageObject(host.worldObjects[2], 0);
        Check(scene.GetLightSources().size() == 9, "Explosion sparks must each create only a corona source.");
        for (const auto& light : scene.GetLightSources())
        {
            PackedLightSourceParams params;
            Check(!light->FillLightParams(params) && light->FillCoronaParams(params),
                "Explosion spark coronas must never enter the expensive dynamic-light list.");
        }
        for (int slot = 3; slot <= 11; ++slot)
        {
            auto& spark = host.worldObjects[slot];
            Check(spark.behavior == Desperabis::BehaviorType::BouncingObject && scene.GetObjectPresentation(slot),
                "Each actual fireball-breakup spark must receive its material context from the creation hook.");
            auto instance = std::make_shared<RenderableInstance>(nullptr);
            present(slot, instance);
            Check(instance->GetUsedTransparencyMask() == TransparencyModeMask::AdditiveBit &&
                instance->m_materialOverride[0].m_opacity == 1.f &&
                instance->GetRenderable()->m_drawCalls[0].m_material.m_transparencyType != TransparencyMode::Additive,
                "Explosion sparks must use additive instance overrides without modifying shared sprite materials or opacity.");
            const auto context = scene.GetObjectPresentation(slot);
            spark.behavior = Desperabis::BehaviorType::FadingParticle;
            spark.health = 12;
            Desperabis::DispatchObjectCase(spark, host);
            present(slot, instance);
            Check(scene.GetObjectPresentation(slot) == context &&
                instance->GetUsedTransparencyMask() == TransparencyModeMask::AdditiveBit &&
                std::abs(instance->m_materialOverride[0].m_opacity - 4.f / 6.f) < 0.00001f,
                "Spark bounce-to-fade transitions must retain additive blending and native opacity fading.");
            spark.behavior = Desperabis::BehaviorType::BouncingObject;
            spark.generalPurpose1 = 0;
            present(slot, instance);
            Check(!scene.GetObjectPresentation(slot) &&
                instance->GetUsedTransparencyMask() == TransparencyModeMask::AlphaBlendedBit,
                "Unrelated bouncing debris must keep its normal alpha blending.");
            host.SetObjectHealth(spark, 0);
        }

        Desperabis::RawGameObject replacement{};
        replacement.health = 1000;
        replacement.behavior = Desperabis::BehaviorType::FireballProjectile;
        host.ReplaceWorldObject(165, replacement);
        Check(scene.GetObjectPresentation(165) && scene.GetLightSources().size() == 1,
            "Presentation storage must include the last native slot 165.");
        std::weak_ptr<ObjectPresentationContext> replaced = scene.GetObjectPresentation(165);
        replacement.behavior = Desperabis::BehaviorType::FireballImpact;
        host.ReplaceWorldObject(165, replacement);
        Check(replaced.expired() && scene.GetObjectPresentation(165) && scene.GetLightSources().size() == 1,
            "Replacing a live slot must release the previous context and create only one impact light.");
        auto& impact = host.worldObjects[165];
        impact.modelId = 1; impact.modelAnim = 5; impact.animFrame = 1;
        impact.spriteScale = 24;
        impact.displayType = Desperabis::DisplayType::SpriteTransparent;
        impact.blendWeightOrSpriteOpacity = 3;
        auto impactInstance = std::make_shared<RenderableInstance>(nullptr);
        present(165, impactInstance);
        Check(impactInstance->GetUsedTransparencyMask() == TransparencyModeMask::AdditiveBit &&
            std::abs(impactInstance->m_materialOverride[0].m_opacity - 4.f / 6.f) < 0.00001f,
            "Fireball impact flashes must also use additive blending without losing native opacity.");
        host.worldObjects[165].behavior = Desperabis::BehaviorType::None;
        host.Sync(165);
        Check(!scene.GetObjectPresentation(165) && scene.GetLightSources().empty(),
            "In-place behavior changes must discard obsolete effect contexts.");
        replacement.behavior = Desperabis::BehaviorType::PulsatingLight;
        host.ReplaceWorldObject(165, replacement);
        host.SetObjectHealth(host.worldObjects[165], 0);
        Check(scene.GetLightSources().empty(), "Pickup/depletion must remove pulsating item lights immediately.");
        host.SetObjectHealth(host.worldObjects[165], 1000);
        Check(scene.GetLightSources().size() == 1, "Reactivating an object must recreate its light once.");
        std::weak_ptr<LightSource> cleared = scene.GetLightSources()[0];
        scene.ClearScene();
        Check(cleared.expired() && !scene.GetObjectPresentation(165), "Level changes must not retain old object lights.");
    }

    void CheckStaticFireTracking()
    {
        auto* level = FORCE_GET(AssetManager::Instance().LoadLevel(23));
        const auto bytes = FORCE_GET(FileManager::Instance().ReadDataFuture(
            FileManager::Instance().FilenameToNative(L"LEVEL\\OBJPOS23.DAT")));
        const auto animation = FORCE_GET(AssetManager::Instance().LoadAsset<TextureAnim>(L"ANIMS\\FACKEL1.AN2"));
        for (const bool markersFirst : { false, true })
        {
            FlameScene scene;
            Model model;
            model.m_drawCalls.resize(3);
            for (int index = 0; index < 3; ++index)
            {
                auto& material = model.m_drawCalls[index].m_material;
                material.m_textureSlot = 13 + index;
                material.m_texture = level->GetLevelInfo().GetLevelTexture(13 + index).get();
                Check(material.m_texture != nullptr, "Level 23 must supply static textures in slots 13..15.");
            }
            auto instance = std::make_shared<RenderableInstance>(&model);
            Model terrain;
            terrain.m_drawCalls = model.m_drawCalls;
            for (auto& draw : terrain.m_drawCalls)
            {
                draw.m_material.m_textureSlotSource = TextureSlotSource::Level;
                draw.m_material.ComputeHash();
            }
            Check(!model.m_drawCalls[0].IsBatchableWith(terrain.m_drawCalls[0]) &&
                model.m_drawCalls[0].ComputeBatchingKey() != terrain.m_drawCalls[0].ComputeBatchingKey(),
                "Equal texture slots and pointers from different namespaces must not be batched together.");
            auto terrainInstance = std::make_shared<RenderableInstance>(&terrain);
            auto loadMarkers = [&]()
            {
                for (const int slot : { 4, 101 })
                {
                    ::RawGameObject marker{};
                    Desperabis::EncodeGameObject(Desperabis::DecodeGameObject(bytes.data() + (slot - 2) * 54, 54),
                        marker.m_rawBytes, 54);
                    marker.m_level = level;
                    LevelGameObject controller(&scene, marker);
                    FORCE_WAIT(GamePlay::CreateObjectBehavior(scene, controller, marker));
                }
            };
            if (markersFirst) { loadMarkers(); }
            scene.AddRenderableInstance(instance);
            scene.AddRenderableInstance(terrainInstance);
            if (!markersFirst)
            {
                Check(scene.BindingCount() == 0, "Resolved model textures must remain untracked until a marker claims their slot.");
                loadMarkers();
            }
            Check(scene.BindingCount() == 2,
                "Markers must bind model texture slots, not equal level slots, regardless of marker/geometry load order.");
            scene.SetFireAnimationFrame(13, 0);
            scene.SetFireAnimationFrame(14, 1);
            scene.HandleFireAnimations(0);
            Check(instance->m_materialOverride[0].m_texture == &animation->m_frames[0] &&
                instance->m_materialOverride[1].m_texture == &animation->m_frames[1] &&
                instance->m_materialOverride[0].m_transparencyType == TransparencyMode::Additive &&
                instance->m_materialOverride[1].m_transparencyType == TransparencyMode::Additive,
                "Registered fire slots must replace resolved model textures with their selected additive frames.");
            Check(terrainInstance->m_materialOverride.empty(),
                "Tracked model slots must not allocate animation overrides on flat terrain with equal slot numbers.");
            terrainInstance->m_materialOverride.push_back(terrain.m_drawCalls[0].m_material);
            scene.AssignFireAnimations(*terrainInstance);
            scene.HandleFireAnimations(0);
            Check(scene.BindingCount() == 2 &&
                terrainInstance->m_materialOverride[0].m_texture == level->GetLevelInfo().GetLevelTexture(13).get() &&
                terrainInstance->m_materialOverride[0].m_transparencyType == TransparencyMode::Opaque,
                "Existing level-texture overrides must keep their static texture and opaque blending.");
            Check(instance->m_materialOverride[2].m_texture == model.m_drawCalls[2].m_material.m_texture &&
                instance->m_materialOverride[2].m_transparencyType == model.m_drawCalls[2].m_material.m_transparencyType &&
                model.m_drawCalls[0].m_material.m_texture == level->GetLevelInfo().GetLevelTexture(13).get(),
                "Untracked static slots and shared source materials must remain unchanged.");
            instance->m_materialOverride.clear();
            scene.AssignFireAnimations(*instance);
            scene.AssignFireAnimations(*instance);
            scene.SetFireAnimationFrame(13, 2);
            scene.HandleFireAnimations(0);
            Check(scene.BindingCount() == 2 && instance->m_materialOverride[0].m_texture == &animation->m_frames[2],
                "Static material refreshes must rebind the current frame without duplicating animation bindings.");
            instance->m_materialOverride[0] = terrain.m_drawCalls[0].m_material;
            scene.AssignFireAnimations(*instance);
            scene.HandleFireAnimations(0);
            Check(instance->m_materialOverride[0].m_texture == level->GetLevelInfo().GetLevelTexture(13).get() &&
                instance->m_materialOverride[0].m_transparencyType == TransparencyMode::Opaque,
                "An old animation binding must not overwrite a refreshed material from the level-texture namespace.");
            scene.RemoveRenderableInstance(instance);
            Check(scene.BindingCount() == 0, "Removing static animated geometry must discard its bindings.");
            scene.RemoveRenderableInstance(terrainInstance);

            FlameScene converted;
            FORCE_GET(level->PopulateScene(converted, false, false));
            for (const int slot : { 13, 14 })
            {
                converted.AddFireAnimation(animation, slot, 25.1f);
                converted.SetFireAnimationFrame(slot, 2);
            }
            converted.HandleFireAnimations(0);
            int staticSurfaces = 0;
            int collidingSlots = 0;
            for (const auto& surface : converted.GetRenderableInstances())
            {
                for (std::size_t index = 0; index < surface->GetRenderable()->m_drawCalls.size(); ++index)
                {
                    const auto& original = surface->GetRenderable()->m_drawCalls[index].m_material;
                    if (original.m_textureSlotSource != TextureSlotSource::Level) { continue; }
                    const auto& current = index < surface->m_materialOverride.size() ?
                        surface->m_materialOverride[index] : original;
                    Check(current.m_textureSlotSource == TextureSlotSource::Level &&
                        current.m_texture == level->GetLevelInfo().GetLevelTexture(original.m_textureSlot).get() &&
                        current.m_transparencyType == original.m_transparencyType,
                        "Converted level 23 walls, floors and ceilings must retain their static TUR textures and blending.");
                    ++staticSurfaces;
                    if (original.m_textureSlot == 13 || original.m_textureSlot == 14) { ++collidingSlots; }
                }
            }
            Check(staticSurfaces > 0 && collidingSlots > 0,
                "The original level 23 regression must exercise flat surfaces sharing the fireplace's slot numbers.");
        }
    }

    void CheckWitchMouth(RendererDX11& renderer, ID3D11Device3* device, ID3D11DeviceContext3* context)
    {
        auto* level = FORCE_GET(AssetManager::Instance().LoadLevel(90));
        const auto bytes = FORCE_GET(FileManager::Instance().ReadDataFuture(
            FileManager::Instance().FilenameToNative(L"LEVEL\\OBJPOS90.DAT")));
        FlameScene scene;
        ::RawGameObject marker{};
        Desperabis::EncodeGameObject(Desperabis::DecodeGameObject(bytes.data(), 54), marker.m_rawBytes, 54);
        marker.m_level = level;
        LevelGameObject controller(&scene, marker);
        FORCE_WAIT(GamePlay::CreateObjectBehavior(scene, controller, marker));
        Check(scene.HasFlameSlot(13), "Behavior 209 must bind the five-frame witch mouth into reserved slot 13.");
        ::RawGameObject head{};
        Desperabis::EncodeGameObject(Desperabis::DecodeGameObject(bytes.data() + 3 * 54, 54), head.m_rawBytes, 54);
        head.m_level = level;
        auto instance = std::make_shared<RenderableInstance>(nullptr);
        level->ApplyRenderProperties(instance, head);
        scene.AddRenderableInstance(instance);
        Check(scene.BindingCount() > 0, "The original witch head must actually consume its animated mouth material.");
        scene.SetFireAnimationFrame(13, 0);
        scene.HandleFireAnimations(0);
        auto mouth = std::find_if(instance->m_materialOverride.begin(), instance->m_materialOverride.end(),
            [](const DrawCallMaterial& material) { return material.m_textureSlot == 13; });
        Check(mouth != instance->m_materialOverride.end() && mouth->m_texture != nullptr &&
            mouth->m_transparencyType == TransparencyMode::Opaque,
            "The witch mouth must use a real, non-additive frame rather than fire blending.");
        const auto* closed = mouth->m_texture;
        scene.HandleFireAnimations(2);
        Check(mouth->m_texture == closed, "The renderer must not autonomously talk while the core holds a mouth frame.");
        scene.SetFireAnimationFrame(13, 4);
        scene.HandleFireAnimations(0);
        Check(mouth->m_texture != closed, "Native mouth frame selection must update the rendered head material.");
        level->ApplyRenderProperties(instance, head);
        scene.AssignFireAnimations(*instance);
        scene.HandleFireAnimations(0);
        mouth = std::find_if(instance->m_materialOverride.begin(), instance->m_materialOverride.end(),
            [](const DrawCallMaterial& material) { return material.m_textureSlot == 13; });
        Check(mouth != instance->m_materialOverride.end() && mouth->m_texture != closed &&
            mouth->m_transparencyType == TransparencyMode::Opaque,
            "Core head-pose refreshes must preserve the mouth binding and native frame.");
        Target target(device, 360, 240);
        renderer.ReInitialize(target.colorView.Get(), target.depthView.Get(), target.viewport);
        RenderingSettings settings;
        ProfilingStats stats;
        Texture lightmap;
        lightmap.CreatePlainWhite();
        lightmap.m_atlasData = {0, 0, 0.5f, 0.5f};
        const auto render = [&](const RenderableInstancePtr& object)
        {
            object->UpdateGlobalBounds();
            const auto& bounds = object->GetGlobalBounds();
            const auto center = bounds.GetCenter();
            const float span = (std::max)(bounds.GetSphereRadius() * 3.f, 0.2f);
            Matrix4 projection, transform;
            projection.CreateOrthographic(span * 1.5f, span, 0.01f, span * 10.f);
            transform.SetTranslation(Vector(center.GetX(), center.GetY(), center.GetZ() + span));
            Camera camera;
            camera.SetProjectionMatrix(projection);
            camera.SetCameraTransformMatrix(transform);
            VisibilityResult visible;
            visible.m_mainCamera = &camera;
            visible.m_visibleInstances.m_opaqueInstances.push_back({object.get(), 0});
            visible.m_visibleInstances.m_visibleTransparencyTypes = object->GetUsedTransparencyMask();
            visible.m_ambientColor = {1, 1, 1};
            visible.m_ambientNrmlContribution = 0.6f;
            visible.m_fogColor = {0, 0, 0};
            visible.m_fogFalloff = 0;
            visible.m_lightmap = &lightmap;
            const float black[4]{0, 0, 0, 1};
            context->ClearRenderTargetView(target.colorView.Get(), black);
            context->ClearDepthStencilView(target.depthView.Get(), D3D11_CLEAR_DEPTH, 1, 0);
            renderer.RenderSceneElements(visible, settings, stats);
            return target.Pixels(device, context);
        };
        bool allVisible = true;
        for (const int slot : {5, 14, 15})
        {
            ::RawGameObject raw{};
            Desperabis::EncodeGameObject(Desperabis::DecodeGameObject(bytes.data() + (slot - 2) * 54, 54),
                raw.m_rawBytes, 54);
            raw.m_level = level;
            auto object = std::make_shared<RenderableInstance>(nullptr);
            level->ApplyRenderProperties(object, raw);
            scene.AddRenderableInstance(object);
            scene.HandleFireAnimations(0);
            const auto pixels = render(object);
            const auto count = std::count_if(pixels.begin(), pixels.end(),
                [](std::uint32_t pixel) { return (pixel & 0xffffff) != 0; });
            if (slot == 5)
            {
                const auto* mesh = object->GetRenderable()->GetNativeRuntimeObj<NativeModelMeshObjectDX11>();
                const auto vertices = ReadBuffer<ModelVertex>(device, context, mesh->m_vertexBuffer.Get());
                const auto indices = ReadBuffer<uint16_t>(device, context, mesh->m_indexBuffer.Get());
                int changed = 0;
                for (const auto& dc : object->GetRenderable()->m_drawCalls)
                {
                    for (int i = dc.m_firstIndex; i < dc.m_firstIndex + dc.m_primitiveCount * 3; i += 3)
                    {
                        const auto& a = vertices[indices[i]].Normal;
                        const auto& b = vertices[indices[i + 1]].Normal;
                        changed += Vector(a.x - b.x, a.y - b.y, a.z - b.z).GetLength() > 0.01f;
                    }
                }
                Check(changed > 0, "The original witch head must contain baked interpolated polygon normals.");
                Check(render(object) == pixels, "The witch's baked shading must remain stable across repeated draws.");
                std::cout << "Witch baked lighting: " << changed << " triangles have interpolated normals.\n";
            }
            std::cout << "Witch slot " << slot << ": " << count << " rendered pixels, "
                << object->GetRenderable()->m_drawCalls.size() << " draw calls.\n";
            allVisible &= count >= 8;
        }
        Check(allVisible, "Original witch head, animated arm and dog must draw actual pixels.");
        scene.ClearScene();
        class Host final : public Desperabis::GameContext
        {
            std::vector<std::uint8_t> LoadFile(std::string_view filename) override
            {
                return FORCE_GET(FileManager::Instance().ReadDataFuture(FileManager::Instance().FilenameToNative(
                    std::wstring(filename.begin(), filename.end()))));
            }
            void LoadLevel(const Desperabis::LevelLoadRequest& request) override
            {
                Desperabis::LevelLoadData data;
                LoadLevelGeometry(request.levelNumber, data);
                LoadLevelObjects(request.levelNumber, data);
                InitializeLevelPlayerSpawn(data, {0, 0, 100});
                CompleteLevelLoad(request.id, std::move(data));
            }
        } core;
        core.StartLevel(90);
        core.worldObjects[5].winY = 256;
        core.worldObjects[5].winX = 3;
        LevelScene room(90);
        room.m_createGameObjects = false;
        room.m_createMapIcons = false;
        FORCE_WAIT(room.CreateScene());
        std::weak_ptr<PlayerLight> playerLight = room.GetPlayerLight();
        Check(!playerLight.expired() && std::any_of(room.GetLightSources().begin(), room.GetLightSources().end(),
            [&](const auto& source) { return source.get() == room.GetPlayerLight().get(); }),
            "LevelScene must retain the typed player light registered during population, without RTTI lookup.");
        std::array<RenderableInstancePtr, 166> objects;
        for (std::size_t offset = 0; offset < bytes.size(); offset += 54)
        {
            ::RawGameObject raw{};
            raw.m_level = level;
            raw.m_objectNumber = static_cast<int>(offset / 54 + 2);
            Desperabis::EncodeGameObject(core.GetObjectRenderState(raw.m_objectNumber), raw.m_rawBytes, 54);
            if (!GamePlay::PropertyAccessorBase(raw).Isvalid()) { continue; }
            auto object = std::make_shared<RenderableInstance>(nullptr,
                static_cast<uint32_t>(GamePlay::VisibilityFilter::GameObjects));
            level->ApplyRenderProperties(object, raw);
            room.AddRenderableInstance(object);
            objects[raw.m_objectNumber] = object;
            LevelGameObject presentation(&room, raw);
            presentation.m_renderInst = object;
            FORCE_WAIT(GamePlay::CreateObjectBehavior(room, presentation, raw));
        }
        room.HandleFireAnimations(0);
        Camera camera;
        Matrix4 projection, transform;
        projection.CreateProjection(1.5f, 70.f * static_cast<float>(Math::Pi / 180), 0.01f, 30.f);
        transform.Create(Level::DespToPos(1430, -140, 3420), Vector(static_cast<float>(Math::Pi), 0, 0));
        camera.SetProjectionMatrix(projection);
        camera.SetCameraTransformMatrix(transform);
        DefaultVisibilityContext visibility;
        VisibilitySettings visibilitySettings;
        const auto renderRoom = [&]
        {
            VisibilityResult visible;
            visibility.ComputeResultFor(&camera, &room, visible, settings, visibilitySettings, stats);
            const float black[4]{0, 0, 0, 1};
            context->ClearRenderTargetView(target.colorView.Get(), black);
            context->ClearDepthStencilView(target.depthView.Get(), D3D11_CLEAR_DEPTH, 1, 0);
            renderer.RenderSceneElements(visible, settings, stats);
            return target.Pixels(device, context);
        };
        const auto roomPixels = renderRoom();
        for (int slot : {5, 14, 15})
        {
            objects[slot]->SetVisible(false);
            const auto omitted = renderRoom();
            int changed = 0;
            for (std::size_t i = 0; i < omitted.size(); ++i) { changed += omitted[i] != roomPixels[i]; }
            std::cout << "Witch room slot " << slot << ": " << changed << " visible pixels.\n";
            Check(changed >= 8, "Linked witch components must be visible in the actual room, not behind its wall.");
            objects[slot]->SetVisible(true);
        }
        room.SetFireAnimationFrame(13, 4);
        room.HandleFireAnimations(0);
        const auto talking = renderRoom();
        int mouthPixels = 0;
        for (std::size_t i = 0; i < talking.size(); ++i) { mouthPixels += talking[i] != roomPixels[i]; }
        std::cout << "Witch room mouth: " << mouthPixels << " changed pixels.\n";
        Check(mouthPixels >= 8, "Native mouth frames must visibly change the witch's face in the actual room.");
        const auto mouthOverride = std::find_if(objects[5]->m_materialOverride.begin(), objects[5]->m_materialOverride.end(),
            [](const DrawCallMaterial& material) { return material.m_textureSlot == 13; });
        Check(mouthOverride != objects[5]->m_materialOverride.end(), "The real scene must retain the mouth override.");
        auto& mouthMaterial = *mouthOverride;
        room.SetFireAnimationFrame(13, 0);
        room.HandleFireAnimations(0);
        mouthMaterial.m_depthOffset = 0;
        mouthMaterial.ComputeHash();
        const auto unoffsetClosed = renderRoom();
        room.SetFireAnimationFrame(13, 4);
        room.HandleFireAnimations(0);
        mouthMaterial.m_depthOffset = 0;
        mouthMaterial.ComputeHash();
        const auto unoffsetTalking = renderRoom();
        int unoffsetPixels = 0;
        for (std::size_t i = 0; i < talking.size(); ++i) { unoffsetPixels += unoffsetClosed[i] != unoffsetTalking[i]; }
        std::cout << "Witch mouth without overlay offset: " << unoffsetPixels << " changed pixels.\n";
        Check(unoffsetPixels == 0, "The original base-face quad must reproduce the missing-mouth depth occlusion.");
        room.HandleFireAnimations(0);
        for (const int slot : {14, 15})
        {
            std::array<std::vector<std::uint32_t>, 2> poses;
            for (int phase = 0; phase < 2; ++phase)
            {
                auto pose = core.GetObjectRenderState(slot);
                pose.blendWeightOrSpriteOpacity = static_cast<std::int16_t>(phase * 100);
                ::RawGameObject raw{};
                raw.m_level = level;
                Desperabis::EncodeGameObject(pose, raw.m_rawBytes, 54);
                level->ApplyRenderProperties(objects[slot], raw);
                room.AssignFireAnimations(*objects[slot]);
                room.HandleFireAnimations(0);
                poses[phase] = renderRoom();
            }
            int changed = 0;
            for (std::size_t i = 0; i < poses[0].size(); ++i) { changed += poses[0][i] != poses[1][i]; }
            Check(changed >= 8, "Attached arm and dog morphs must visibly animate in the room after pose refreshes.");
        }
        room.ClearScene();
        Check(!room.GetPlayerLight() && playerLight.expired(),
            "Clearing a level scene must release the retained player light as well as the scene's light list.");
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
        CheckSmoothPolygonNormals(renderer, device.Get(), context.Get());
        CheckTutorialFlames();
        CheckStaticFireTracking();
        CheckFreshObjectPlacement();
        CheckObjectPresentations();
        CheckVisualLightEffects();
        CheckGhostPresentations();
        CheckWitchMouth(renderer, device.Get(), context.Get());
        CheckMutableWalls(renderer, device.Get(), context.Get());
        CheckWaterSurfaceDefinitions();
        CheckEnemyModels();
        CheckPresentationRecovery();
        CheckEndingActorVisibility();
        CheckVillageCloudReveal(renderer, device.Get(), context.Get());
        CheckFinalLevelMorphs(renderer, device.Get(), context.Get());
        CheckFinalLevelRuntimePresentation(renderer, device.Get(), context.Get());
        CheckParkAttachments(renderer, device.Get(), context.Get());
        CheckMummyFade(renderer, device.Get(), context.Get());
        CheckDragon(renderer, device.Get(), context.Get());
        CheckRainSprites(renderer, device.Get(), context.Get());
        CheckLightning(renderer, device.Get(), context.Get());
        CheckUnderwater(renderer, device.Get(), context.Get());
        CheckUnderwaterLightPool(renderer, device.Get(), context.Get());
        CheckDynamicSpecularity(renderer, device.Get(), context.Get());
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
                using namespace Desperabis;
                ApplicationPresentation cursor{ MenuPage::MainMenu, {
                    UiFill{ { 0,0,360,240 }, { 0,0,0 } },
                    UiBitmap{ { 48,37,0,0 }, UiBitmapSource::MenuCursor, 0,0,0,
                        { 204,153,85 }, UiBitmapLayout::SpriteAnchor } } };
                std::vector<std::uint32_t> previous;
                for (std::uint8_t frame = 0; frame < 10; ++frame)
                {
                    auto& sword = std::get<UiBitmap>(cursor.commands.back());
                    sword.animation = frame / 5;
                    sword.frame = frame % 5;
                    const auto pixels = drawRecipe(cursor);
                    int goldPixels = 0;
                    for (auto pixel : pixels)
                    {
                        const int r = pixel & 255, g = (pixel >> 8) & 255, b = (pixel >> 16) & 255;
                        goldPixels += r > g && g > b && g > 10;
                    }
                    Check(goldPixels > 0 && pixels != previous,
                        "All ten sword frames must render distinctly in the font palette, not the world palette.");
                    previous = pixels;
                }
            }
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
            const auto herz = FORCE_GET(files.ReadDataFuture(files.FilenameToNative(L"GRAFIK\\HERZ.256")));
            core.SetGameParts(herz.data(), herz.size());
            core.SetMenuPage(Desperabis::MenuPage::ChapterSelection);
            const auto partSelection = render();
            Check(partSelection != baseline && partSelection == render(),
                "Part selection must render a stable core-owned presentation.");
            struct SelectionInput final : Desperabis::InputHandler
            {
                bool down = false;
                bool IsKeyPressed(Desperabis::InputKey key) override
                {
                    return down && key == Desperabis::InputKey::UiDownKey;
                }
            } selectionInput;
            core.UpdateApplication(0, selectionInput);
            selectionInput.down = true;
            core.UpdateApplication(0, selectionInput);
            Check(render() != partSelection, "Part selection must visibly move focus to the next part.");
            core.SetMenuPage(Desperabis::MenuPage::LoadGame);
            const auto loadScreen = render();
            core.SetMenuPage(Desperabis::MenuPage::SaveGame);
            const auto saveScreen = render();
            Check(loadScreen != baseline && saveScreen != loadScreen,
                "Native load/save slot screens must render distinctly at every tested aspect ratio.");
            selectionInput.down = false;
            core.UpdateApplication(0, selectionInput);
            core.UpdateApplication(0.11, selectionInput);
            const auto rotated = render();
            Check(rotated != saveScreen, "The native sword must rotate using the UI clock while gameplay is paused.");
            for (std::size_t pixel = 0; pixel < saveScreen.size(); ++pixel)
            {
                if (saveScreen[pixel] == rotated[pixel]) { continue; }
                const float x = (pixel % size.first - offsetX) / scale;
                const float y = (pixel / size.first - offsetY) / scale;
                Check(x >= 32 && x <= 68 && y >= 29 && y <= 44,
                    "Sword animation must affect only its original anchored, half-height sprite footprint.");
            }
            auto editor = core.GetApplicationPresentation();
            const auto firstRow = std::find_if(editor.commands.begin(), editor.commands.end(), [](const auto& command)
            {
                const auto* text = std::get_if<Desperabis::UiText>(&command);
                return text && text->x == 80 && text->y == 32;
            });
            Check(firstRow != editor.commands.end(), "Save editing requires its original inline row.");
            auto& name = std::get<Desperabis::UiText>(*firstRow);
            const auto emptyEditor = drawRecipe(editor);
            name.caret = true;
            const auto emptyCaret = drawRecipe(editor);
            Check(emptyCaret != emptyEditor, "An entirely erased name must render a visible inline caret without crashing.");
            name.text = "Level 12";
            const auto typedCaret = drawRecipe(editor);
            name.caret = false;
            const auto typed = drawRecipe(editor);
            Check(typed != emptyEditor && typedCaret != typed, "The caret must blink after the actual typed name.");
            const auto textMesh = font.CreateGlyphMesh(name.text, { 255,255,255,255 });
            const float caretX = 80 + textMesh->GetExtent().x * name.scale + 1;
            int caretPixels = 0;
            for (std::size_t pixel = 0; pixel < typed.size(); ++pixel)
            {
                if (typed[pixel] == typedCaret[pixel]) { continue; }
                const float x = (pixel % size.first - offsetX) / scale;
                const float y = (pixel / size.first - offsetY) / scale;
                Check(x >= caretX - 1 && x <= caretX + 2 && y >= 31 && y <= 46,
                    "Blinking must stay immediately after the name, on the selected row.");
                ++caretPixels;
            }
            Check(caretPixels > 0, "Inline caret checks must cover actual rendered pixels.");
            for (const auto value : { "", "   ", "_" })
            {
                name.text = value;
                name.caret = false;
                Check(drawRecipe(editor) == emptyEditor,
                    "Empty, whitespace-only and glyphless text meshes must be harmless in the text renderer.");
            }
            for (const auto page : { Desperabis::MenuPage::MainMenu, Desperabis::MenuPage::AlternateMainMenu,
                Desperabis::MenuPage::ChapterSelection, Desperabis::MenuPage::LoadGame, Desperabis::MenuPage::SaveGame })
            {
                core.SetMenuPage(page);
                int backdrops = 0, cursors = 0;
                for (const auto& command : core.GetApplicationPresentation().commands)
                {
                    const auto* bitmap = std::get_if<Desperabis::UiBitmap>(&command);
                    if (!bitmap) { continue; }
                    backdrops += bitmap->source == Desperabis::UiBitmapSource::MenuBackground &&
                        bitmap->layout == Desperabis::UiBitmapLayout::Stretch;
                    cursors += bitmap->source == Desperabis::UiBitmapSource::MenuCursor;
                }
                Check(backdrops == 1 && cursors == 1, "Every menu-family page must use the native background and sword.");
                const auto screen = render();
                for (std::size_t pixel = 0; pixel < screen.size(); ++pixel)
                {
                    const float y = (pixel / size.first - offsetY) / scale;
                    if (y >= 205 && y < 225)
                    {
                        Check(screen[pixel] == saveScreen[pixel], "Menu pages must share the same full-width native backdrop.");
                    }
                }
            }
            if (argc >= 5 && size.first == 360)
            {
                name.text = "Level 12";
                name.caret = true;
                target.SaveBitmap(argv[4], drawRecipe(editor));
            }
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
        const auto scytheAsset = FORCE_GET(AssetManager::Instance().LoadAsset<ModelAnim>(L"VEKT\\VEKT67.3DA"));
        const auto* scytheModel = scytheAsset->GetModel(1, 1);
        Check(scytheModel->m_numPositions == 14,
            "The complete scythe frame must contain all fourteen authored vertices, including the blade tip.");
        for (const auto levelNumber : { 10, 11, 50 })
        {
            const auto* weaponLevel = FORCE_GET(AssetManager::Instance().LoadLevel(levelNumber));
            const auto* swordModel = weaponLevel->GetLevelInfo().GetModelAnim(1)->GetModel(1, 0);
            const auto originalMaterial = swordModel->m_drawCalls[0].m_material;
            const auto electricityBytes = FORCE_GET(files.ReadDataFuture(files.FilenameToNative(
                L"VEKT\\VEKT" + std::to_wstring(weaponLevel->GetLevelInfo().m_loadedModelSlots[1]) + L".3DA")));
            Desperabis::ModelAnchorGeometry electricityGeometry;
            electricityGeometry.Load(electricityBytes.data(), electricityBytes.size());
            const auto& electricityFrames = electricityGeometry.animations.at(1);
            Check(electricityFrames.at(3).vertices.at(2) == std::array<std::int16_t,3>{0,0,53} &&
                electricityFrames.at(3).vertices.at(2) == electricityFrames.at(4).vertices.at(2),
                "Both chapter electricity bolts must originate at the same vertex 3 in both firing frames.");
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
                    if (type == Desperabis::WeaponType::Scythe)
                    {
                        ComPtr<ID3D11Buffer> boundPositions;
                        context->VSGetConstantBuffers(2, 1, &boundPositions);
                        Check(weapon->modelSource == Desperabis::WeaponView::ModelSource::Scythe &&
                            boundPositions.Get() == scytheModel->GetNativeRuntimeObj<NativeModelMeshObjectDX11>()->m_positionBuffer.Get(),
                            "First-person scythes must bind the complete VEKT67 geometry in both chapter banks.");
                    }
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
                    if (type == Desperabis::WeaponType::Electricity)
                    {
                        weaponCore.weaponAmounts[7] = 100;
                        weaponCore.weaponElectricityFiring = true;
                        Camera worldCamera;
                        Matrix4 projection;
                        projection.CreateProjection(target.viewport.Width / target.viewport.Height, 1.1f, 0.001f, 100.f);
                        worldCamera.SetProjectionMatrix(projection);
                        ::RawGameObject cameraState{};
                        cameraState.m_rawShorts[9] = 30;
                        cameraState.m_rawShorts[10] = 100;
                        auto worldTransform = GamePlay::PropertyAccessorBase(cameraState).GetTransform();
                        worldTransform.SetTranslation(Vector(2,3,4));
                        worldCamera.SetCameraTransformMatrix(worldTransform);
                        for (const int hand : {1,2})
                        {
                            weaponCore.weaponHand = static_cast<std::uint8_t>(hand);
                            for (const int pose : {-10,0,15,30,50})
                            {
                                weaponCore.weaponAnimationPosition = static_cast<std::int16_t>(pose);
                                weaponCore.playerWalkPhase = pose * 7 & 511;
                                const auto charged = *weaponCore.GetWeaponView();
                                ui.DrawWeapon(charged, renderer, *weaponLevel, settings, stats, target.viewport,
                                    context.Get(), target.colorView.Get(), target.depthView.Get());
                                ComPtr<ID3D11Buffer> instanceBuffer, globalsBuffer;
                                context->VSGetConstantBuffers(1, 1, &instanceBuffer);
                                context->VSGetConstantBuffers(0, 1, &globalsBuffer);
                                const auto instanceData = ReadBuffer<float>(device.Get(), context.Get(), instanceBuffer.Get());
                                const auto globals = ReadBuffer<float>(device.Get(), context.Get(), globalsBuffer.Get());
                                Matrix4 actualModel, actualProjection;
                                actualModel.Load(instanceData.data()); actualModel = actualModel.GetTransposed();
                                actualProjection.Load(globals.data() + 64); actualProjection = actualProjection.GetTransposed();
                                const auto gpuTip = (actualModel * actualProjection).TransformPosition(Vector(0,0,53 * DespVertToFloat));
                                const float scale = (std::min)(target.viewport.Width / Desperabis::WeaponView::Width,
                                    target.viewport.Height / Desperabis::WeaponView::Height);
                                const float expectedX = target.viewport.Width * 0.5f +
                                    gpuTip.GetX() * Desperabis::WeaponView::Width * scale * 0.5f;
                                const float expectedY = target.viewport.Height -
                                    (gpuTip.GetY() + 1.f) * Desperabis::WeaponView::Height * scale * 0.5f;
                                const auto worldTip = ApplicationUi::ElectricityTipWorld(charged, electricityGeometry, worldCamera, target.viewport);
                                const auto projected = (worldCamera.GetViewMatrix(0) * projection).TransformPosition(worldTip);
                                const float actualX = (projected.GetX() + 1.f) * target.viewport.Width * 0.5f;
                                const float actualY = (1.f - projected.GetY()) * target.viewport.Height * 0.5f;
                                Check(std::abs(actualX - expectedX) < 0.25f && std::abs(actualY - expectedY) < 0.25f,
                                    "World electricity corona must project onto the actual GPU-rendered bolt tip within a quarter pixel.");
                            }
                        }
                        weaponCore.weaponHand = 2; weaponCore.weaponAnimationPosition = 15;
                        weaponCore.playerWalkPhase = 128;
                        weaponCore.weaponAmounts[7] = 0; weaponCore.weaponElectricityFiring = false;
                    }
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
                renderer.SetWeaponDraw([&](const VisibilityResult&, const RenderingSettings& drawSettings, ProfilingStats& drawStats)
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
        {
            const auto* scytheLevel = FORCE_GET(AssetManager::Instance().LoadLevel(30));
            Target target(device.Get(), 1280, 720);
            renderer.ReInitialize(target.colorView.Get(), target.depthView.Get(), target.viewport);
            Desperabis::GameContext core;
            core.SetMenuPage(Desperabis::MenuPage::Gameplay);
            core.selectedWeapon = Desperabis::WeaponType::Scythe;
            core.weaponHand = 2;
            core.weaponSpriteFrame = 1;
            for (const int pose : {10,11})
            for (const int look : {95,110})
            for (const int phase : {0,128,256,384})
            {
                core.weaponAnimationPosition = static_cast<std::int16_t>(pose);
                core.playerLookHeight = static_cast<std::int16_t>(look);
                core.playerWalkPhase = static_cast<std::int16_t>(phase);
                const auto view = core.GetWeaponView();
                const float black[]{0,0,0,1};
                context->ClearRenderTargetView(target.colorView.Get(), black);
                ui.DrawWeapon(*view, renderer, *scytheLevel, settings, stats, target.viewport,
                    context.Get(), target.colorView.Get(), target.depthView.Get());
                ComPtr<ID3D11Buffer> boundPositions;
                context->VSGetConstantBuffers(2, 1, &boundPositions);
                const auto pixels = target.Pixels(device.Get(), context.Get());
                Check(boundPositions.Get() == scytheModel->GetNativeRuntimeObj<NativeModelMeshObjectDX11>()->m_positionBuffer.Get() &&
                    std::count_if(pixels.begin(), pixels.end(), [](std::uint32_t pixel) { return (pixel & 0xffffff) != 0; }) > 100,
                    "The first-part scythe must retain its complete, visibly rendered blade.");
                const auto positions = ReadBuffer<VertexPosition>(device.Get(), context.Get(), boundPositions.Get());
                ComPtr<ID3D11Buffer> instanceBuffer;
                context->VSGetConstantBuffers(1, 1, &instanceBuffer);
                const auto transformData = ReadBuffer<float>(device.Get(), context.Get(), instanceBuffer.Get());
                Matrix4 rendered;
                rendered.Load(transformData.data());
                rendered = rendered.GetTransposed();
                RawGameObject raw;
                Desperabis::EncodeGameObject(view->object, raw.m_rawBytes, sizeof(raw.m_rawBytes));
                auto native = GamePlay::PropertyAccessorBase(raw).GetTransform();
                native.SetTranslation(Vector(view->object.posX * DespToFloat,
                    -(view->object.posY + 8) * DespToFloat, -view->object.posZ * DespToFloat));
                Check((rendered.GetTranslation() - native.GetTranslation()).GetLength() < 0.000001f &&
                    (rendered.GetDirection() - native.GetDirection()).GetLength() < 0.00001f,
                    "Scythe framing must only lower the rendered model by eight units, preserving native poses.");
                const auto project = [&](int vertex)
                {
                    const auto& position = positions.at(scytheModel->m_vertexOfs + vertex);
                    const auto point = rendered.TransformPosition(Vector(position.x, position.y, position.z));
                    Check(point.GetZ() < 0, "The scythe must remain in front of its first-person camera.");
                    return Vector2{ Desperabis::WeaponView::Width / 2 + 128.f * point.GetX() / -point.GetZ(),
                        view->projectionCenterY - 128.f * point.GetY() / -point.GetZ() };
                };
                Check(project(0).y > Desperabis::WeaponView::Height,
                    "The shaft's rear endpoint must stay below the viewport throughout idle walking bob.");
                const auto bladeTip = project(9);
                Check(bladeTip.x > 0 && bladeTip.x < Desperabis::WeaponView::Width &&
                    bladeTip.y > 0 && bladeTip.y < Desperabis::WeaponView::Height,
                    "Lowering the scythe must not clip its blade tip out of the first-person view.");
            }
        }
        {
            const auto* park = FORCE_GET(AssetManager::Instance().LoadLevel(30));
            const auto* bucket = park->GetLevelInfo().GetModelAnim(3)->GetModel(2, 0);
            class ReflectionScene final : public Scene
            {
            public:
                FUTURE(void) CreateScene() override { m_sceneReady = true; CO_RETURN; }
            } reflectionScene;
            FORCE_WAIT(reflectionScene.CreateScene());
            for (const auto size : {std::pair<UINT, UINT>{360,240}, {1280,720}, {480,800}})
            {
                Target target(device.Get(), size.first, size.second);
                renderer.ReInitialize(target.colorView.Get(), target.depthView.Get(), target.viewport);
                Desperabis::GameContext core;
                core.SetMenuPage(Desperabis::MenuPage::Gameplay);
                core.selectedWeapon = core.requestedWeapon = Desperabis::WeaponType::Bucket;
                core.weaponHand = 2;
                core.weaponSpriteFrame = 1;
                std::vector<std::uint32_t> previous;
                for (const int amount : {0,15,20})
                {
                    core.weaponAmounts[9] = static_cast<std::int16_t>(amount);
                    const auto view = core.GetWeaponView();
                    Check(view && view->object.modelFrame2 == 2 &&
                        view->object.blendWeightOrSpriteOpacity == amount * 5,
                        "Bucket recipes must retain the actual empty/partial/full water morph.");
                    const float black[]{0,0,0,1};
                    context->ClearRenderTargetView(target.colorView.Get(), black);
                    ui.DrawWeapon(*view, renderer, *park, settings, stats, target.viewport,
                        context.Get(), target.colorView.Get(), target.depthView.Get());
                    ComPtr<ID3D11Buffer> positions;
                    context->VSGetConstantBuffers(2, 1, &positions);
                    const auto pixels = target.Pixels(device.Get(), context.Get());
                    std::size_t firstRow = size.second;
                    for (std::size_t i = 0; i < pixels.size(); ++i)
                    {
                        if ((pixels[i] & 0xffffff) != 0) { firstRow = (std::min)(firstRow, i / size.first); }
                    }
                    Check(firstRow > size.second * 0.75f &&
                        std::count_if(pixels.end() - size.first, pixels.end(),
                            [](std::uint32_t pixel) { return (pixel & 0xffffff) != 0; }) > 3,
                        "The held bucket must enter through the bottom edge, not float fully visible above it.");
                    Check(positions.Get() == bucket->GetNativeRuntimeObj<NativeModelMeshObjectDX11>()->m_positionBuffer.Get() &&
                        std::count_if(pixels.begin(), pixels.end(),
                            [](std::uint32_t pixel) { return (pixel & 0xffffff) != 0; }) > 100,
                        "The held park bucket must render original level-model geometry and textures.");
                    ComPtr<ID3D11Buffer> instance;
                    context->VSGetConstantBuffers(1, 1, &instance);
                    const auto constants = ReadBuffer<float>(device.Get(), context.Get(), instance.Get());
                    Check(constants.size() > 19 && std::abs(constants[19] - amount / 20.f) < 0.00001f,
                        "Bucket fill must reach the actual GPU morph constant.");
                    std::cout << "Bucket at " << size.first << "x" << size.second << ", amount " << amount
                        << ", vertices " << bucket->m_numPositions << ", pixels "
                        << std::count_if(pixels.begin(), pixels.end(),
                            [](std::uint32_t pixel) { return (pixel & 0xffffff) != 0; }) << '\n';
                    Check(previous.empty() || pixels != previous,
                        "Empty, partial and full bucket water levels must produce distinct rendered images.");
                    previous = pixels;
                }
                Camera worldCamera;
                Matrix4 worldTransform;
                worldTransform.SetTranslation(Vector(0,1,0));
                worldCamera.SetCameraTransformMatrix(worldTransform);
                VisibilityResult world;
                world.m_mainCamera = &worldCamera;
                world.m_scene = &reflectionScene;
                auto& cached = world.m_visibleWater.emplace_back();
                cached.m_useReflection = true;
                cached.m_waterHeight = -0.2f;
                cached.m_scrollParams1 = cached.m_scrollParams2 = {0,0,0,0};
                previous.clear();
                for (const auto color : {std::array<float,4>{1,0,0,1}, {0,0,1,1}})
                {
                    RenderContextDX11 mirror(renderer, world, settings, stats);
                    mirror.BeginRendering();
                    mirror.BeginRenderTargetScope(RenderTargetPassType::Mirror, &worldCamera);
                    mirror.TrackGlobals();
                    ComPtr<ID3D11RenderTargetView> mirrorTarget;
                    context->OMGetRenderTargets(1, &mirrorTarget, nullptr);
                    context->ClearRenderTargetView(mirrorTarget.Get(), color.data());
                    mirror.EndRenderTargetScope();
                    mirror.EndRendering();
                    const float black[]{0,0,0,1};
                    context->ClearRenderTargetView(target.colorView.Get(), black);
                    ui.DrawWeapon(*core.GetWeaponView(), renderer, *park, settings, stats, target.viewport,
                        context.Get(), target.colorView.Get(), target.depthView.Get(), &world);
                    const auto pixels = target.Pixels(device.Get(), context.Get());
                    if (!previous.empty())
                    {
                        int changed = 0;
                        for (std::size_t i = 0; i < pixels.size(); ++i) { changed += pixels[i] != previous[i]; }
                        Check(changed > 20, "Held bucket water must sample the shared puddle reflection target, not a white fallback.");
                    }
                    previous = pixels;
                }
                world.m_visibleWater.clear();
                ui.DrawWeapon(*core.GetWeaponView(), renderer, *park, settings, stats, target.viewport,
                    context.Get(), target.colorView.Get(), target.depthView.Get(), &world);
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
