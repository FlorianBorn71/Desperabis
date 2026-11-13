#include "pch.h"
#include <GamePlay/ObjectPresentation.h>
#include <GamePlay/GamePlay.h>
#include <Renderer/LightSource.h>
#include <Renderer/Renderable.h>
#include <Renderer/RenderableInstance.h>
#include <Renderer/Renderer.h>
#include <Renderer/Camera.h>
#include <stdexcept>

namespace
{
    void ApplyAdditiveMaterials(RenderableInstance* instance)
    {
        if (!instance || !instance->GetRenderable()) { return; }
        for (auto& material : instance->GetOrCreateMaterialOverrides())
        {
            material.m_transparencyType = TransparencyMode::Additive;
            material.ComputeHash();
        }
        instance->ComputeTransparencyMask();
    }

    bool IsFireballSpark(const RawGameObject& object)
    {
        const auto behavior = GamePlay::PropertyAccessorBase(object).GetBehaviorType();
        // Native breakup flames retain their sprite and selector when bouncing ends.
        return (behavior == GamePlay::BehaviorType::BouncingObject || behavior == GamePlay::BehaviorType::FadingParticle) &&
            object.m_rawBytes[0] == 1 && object.m_rawBytes[1] == 5 &&
            object.m_rawBytes[3] >= 1 && object.m_rawBytes[3] <= 2 && object.m_rawUShorts[18] == 1;
    }

    bool IsDragonFireball(const RawGameObject& object)
    {
        return GamePlay::PropertyAccessorBase(object).GetBehaviorType() == GamePlay::BehaviorType::FlyingArrow &&
            object.m_rawBytes[0] == 5 && object.m_rawShorts[17] == 5;
    }

    bool IsFireParticle(const RawGameObject& object)
    {
        const auto behavior = GamePlay::PropertyAccessorBase(object).GetBehaviorType();
        return behavior == GamePlay::BehaviorType::FireParticle || IsFireballSpark(object) ||
            (behavior == GamePlay::BehaviorType::FlyingArrow &&
                ((object.m_rawBytes[0] == 5 && object.m_rawShorts[17] == 4) ||
                    (object.m_rawBytes[0] == 1 && object.m_rawShorts[17] == 7)));
    }

    class FountainWaterContext final : public ObjectPresentationContext
    {
    public:
        bool Matches(const RawGameObject& object) const override
        {
            return GamePlay::PropertyAccessorBase(object).GetCollisionType() == GamePlay::CollisionType::FountainWater;
        }
        void Update(const RawGameObject& object, RenderableInstance* instance) override
        {
            if (!instance || !instance->GetRenderable()) { return; }
            const float opacity = 0.15f * GamePlay::PropertyAccessorBase(object).GetSpriteTransparency();
            for (auto& material : instance->GetOrCreateMaterialOverrides())
            {
                material.m_transparencyType = TransparencyMode::AlphaBlended;
                material.SetOpacity(opacity);
                material.ComputeHash();
            }
            instance->ComputeTransparencyMask();
        }
        void Remove(Scene&) override {}
    };

    bool IsElectricBolt(const RawGameObject& object)
    {
        const GamePlay::PropertyAccessorBase properties(object);
        return object.m_rawBytes[0] == 31 && object.m_rawBytes[1] == 6 &&
            properties.GetBehaviorType() == GamePlay::BehaviorType::None &&
            properties.GetDisplayType() == GamePlay::DisplayType::VectorUnlit &&
            properties.GetCollisionType() == GamePlay::CollisionType::None;
    }

    bool IsElectricImpact(const RawGameObject& object)
    {
        const GamePlay::PropertyAccessorBase properties(object);
        return object.m_rawBytes[0] == 1 && object.m_rawBytes[1] == 2 && object.m_rawBytes[3] == 2 &&
            properties.GetBehaviorType() == GamePlay::BehaviorType::LightSource &&
            properties.GetDisplayType() == GamePlay::DisplayType::SpriteTorchlight;
    }

    class EmissiveRayContext final : public ObjectPresentationContext
    {
    public:
        bool Matches(const RawGameObject& object) const override
        {
            return GamePlay::PropertyAccessorBase(object).GetCollisionType() == GamePlay::CollisionType::MercuryRaySegment ||
                IsElectricBolt(object);
        }
        void Update(const RawGameObject&, RenderableInstance* instance) override
        {
            ApplyAdditiveMaterials(instance);
            if (!instance || !instance->GetRenderable()) { return; }
            for (auto& material : instance->GetOrCreateMaterialOverrides())
            {
                material.m_applyLighting = false;
                material.ComputeHash();
            }
        }
        void Remove(Scene&) override {}
    };

    class MercuryLampContext final : public ObjectPresentationContext
    {
    public:
        MercuryLampContext(std::shared_ptr<LightSource> source, std::shared_ptr<LightSource> hit)
            : m_source(std::move(source)), m_hit(std::move(hit)) {}
        bool Matches(const RawGameObject& object) const override
        {
            return GamePlay::PropertyAccessorBase(object).GetBehaviorType() == GamePlay::BehaviorType::MercuryLamp;
        }
        void Update(const RawGameObject& object, RenderableInstance*) override
        {
            const GamePlay::PropertyAccessorBase properties(object);
            const bool active = properties.GetHealth() == 1000;
            m_source->SetPosition(properties.GetPosition());
            m_source->SetRadius(active ? 160.f * DespToFloat : 0.f);
            m_source->SetCoronaSize(active ? 60.f * DespToFloat : 0.f);
            auto endpoint = object;
            endpoint.m_rawShorts[6] = object.m_rawShorts[19];
            endpoint.m_rawShorts[8] = object.m_rawShorts[20];
            m_hit->SetPosition(GamePlay::PropertyAccessorBase(endpoint).GetPosition());
            m_hit->SetRadius(active && object.m_rawShorts[18] == 1 ? 110.f * DespToFloat : 0.f);
            m_source->UpdateGlobalBounds();
            m_hit->UpdateGlobalBounds();
        }
        void Remove(Scene& scene) override
        {
            scene.RemoveLightSource(m_source);
            scene.RemoveLightSource(m_hit);
        }
    private:
        std::shared_ptr<LightSource> m_source, m_hit;
    };

    bool IsMercuryReceiver(const RawGameObject& object)
    {
        const GamePlay::PropertyAccessorBase properties(object);
        return properties.GetCollisionType() == GamePlay::CollisionType::LightSensor ||
            (properties.GetCollisionType() == GamePlay::CollisionType::None &&
                properties.GetBehaviorType() == GamePlay::BehaviorType::None &&
                object.m_rawBytes[52] == static_cast<std::uint8_t>(GamePlay::CollisionType::LightSensor));
    }

    class MercuryReceiverContext final : public ObjectPresentationContext
    {
    public:
        explicit MercuryReceiverContext(std::shared_ptr<LightSource> light) : m_light(std::move(light)) {}
        bool Matches(const RawGameObject& object) const override { return IsMercuryReceiver(object); }
        void Update(const RawGameObject& object, RenderableInstance*) override
        {
            const GamePlay::PropertyAccessorBase properties(object);
            const bool active = properties.GetHealth() == 1000 &&
                properties.GetCollisionType() == GamePlay::CollisionType::None;
            m_light->SetPosition(properties.GetPosition());
            m_light->SetRadius(active ? 180.f * DespToFloat : 0.f);
            m_light->SetCoronaSize(active ? 65.f * DespToFloat : 0.f);
            m_light->UpdateGlobalBounds();
        }
        void Remove(Scene& scene) override { scene.RemoveLightSource(m_light); }
    private:
        std::shared_ptr<LightSource> m_light;
    };

    class KnotholeDepthContext final : public ObjectPresentationContext
    {
    public:
        explicit KnotholeDepthContext(bool key) : m_key(key) {}
        bool Matches(const RawGameObject& object) const override
        {
            const GamePlay::PropertyAccessorBase properties(object);
            return properties.GetModelId() == (m_key ? 39 : 32) &&
                properties.GetModelAnim() == (m_key ? 0 : 2) &&
                properties.GetModelFrame() == (m_key ? 0 : 1);
        }
        void Update(const RawGameObject&, RenderableInstance* instance) override
        {
            if (!instance || !instance->GetRenderable()) { return; }
            for (auto& material : instance->GetOrCreateMaterialOverrides())
            {
                // The backing has priority zero; pull the recess and its key ahead of the uncut trunk together.
                material.m_depthOffset = 25.f * DespToFloat;
                material.ComputeHash();
            }
        }
        void Remove(Scene&) override {}
    private:
        bool m_key;
    };

    class SparkCorona final : public LightSource
    {
    public:
        explicit SparkCorona(Scene* scene) : LightSource(scene) { SetRadius(0.f); }
        bool FillLightParams(PackedLightSourceParams&) const override { return false; }
        bool FillCoronaParams(PackedLightSourceParams& dst) const override
        {
            dst.SetCorona(m_pos.GetX(), m_pos.GetY(), m_pos.GetZ(), GetEffectiveCoronaSize(),
                m_color, m_multiplier, GetCoronaDepthBias());
            return m_multiplier > 0.f && GetEffectiveCoronaSize() > 0.f;
        }
    };

    class FireSparkContext final : public ObjectPresentationContext
    {
    public:
        explicit FireSparkContext(std::shared_ptr<LightSource> corona) : m_corona(std::move(corona)) {}
        bool Matches(const RawGameObject& object) const override
        {
            return IsFireParticle(object) && static_cast<bool>(m_corona) == IsFireballSpark(object);
        }
        void Update(const RawGameObject& object, RenderableInstance* instance) override
        {
            ApplyAdditiveMaterials(instance);
            if (m_corona)
            {
                m_corona->SetPosition(GamePlay::PropertyAccessorBase(object).GetPosition());
                const float opacity = (std::min)(1.f, (std::max)(0.f, (object.m_rawShorts[21] + 1.f) / 6.f));
                m_corona->SetColor({255,160,50}, opacity);
                m_corona->SetCoronaSize(object.m_rawBytes[53] * 2.f * DespToFloat);
                m_corona->UpdateGlobalBounds();
            }
        }
        void Remove(Scene& scene) override { if (m_corona) { scene.RemoveLightSource(m_corona); } }
    private:
        std::shared_ptr<LightSource> m_corona;
    };

    class EmitterLightContext final : public ObjectPresentationContext
    {
    public:
        EmitterLightContext(GamePlay::BehaviorType behavior, std::shared_ptr<LightSource> light)
            : m_behavior(behavior), m_light(std::move(light)) {}
        bool Matches(const RawGameObject& object) const override
        {
            return GamePlay::PropertyAccessorBase(object).GetBehaviorType() == m_behavior;
        }
        void Update(const RawGameObject&, RenderableInstance*) override {}
        void FireEmitted(const Vector& position) override
        {
            m_remaining = 0.2;
            m_light->SetPosition(position);
            m_light->SetRadius(200.f * DespToFloat);
            m_light->SetColor({255,140,30}, 3.f);
        }
        void Advance(double elapsedTime) override
        {
            m_remaining = (std::max)(0.0, m_remaining - elapsedTime);
            m_light->SetRadius(m_remaining > 0 ? 200.f * DespToFloat : 0.f);
            m_light->SetColor({255,140,30}, static_cast<float>(3.0 * m_remaining / 0.2));
        }
        void Remove(Scene& scene) override { scene.RemoveLightSource(m_light); }
    private:
        GamePlay::BehaviorType m_behavior;
        std::shared_ptr<LightSource> m_light;
        double m_remaining = 0.0;
    };

    class GhostLightContext final : public ObjectPresentationContext
    {
    public:
        explicit GhostLightContext(std::shared_ptr<LightSource> light) : m_light(std::move(light)) {}
        bool Matches(const RawGameObject& object) const override
        {
            return GamePlay::PropertyAccessorBase(object).GetBehaviorType() == GamePlay::BehaviorType::Ghost;
        }
        void Update(const RawGameObject& object, RenderableInstance*) override
        {
            const GamePlay::PropertyAccessorBase properties(object);
            m_active = properties.GetHealth() == 1000;
            m_light->SetPosition(properties.GetPosition());
            const auto phase = object.m_rawShorts[17];
            m_dying = phase >= 20 && phase <= 23;
            const float remaining = object.m_rawShorts[19];
            m_intensity = phase == 21 ? 3.f + 6.f * std::clamp((28.f - remaining) / 40.f, 0.f, 1.f) :
                phase == 22 ? 9.f : phase == 23 ? 9.f * std::clamp(remaining / 28.f, 0.f, 1.f) :
                3.f;
            Advance(0);
        }
        void Advance(double) override
        {
            constexpr float lightScale = 0.75f;
            const auto cameraPosition = Renderer::Instance().GetOrCreateCamera()->GetCameraPosition();
            const auto delta = m_light->GetPosition() - cameraPosition;
            const float distance = std::sqrt(delta.GetX() * delta.GetX() + delta.GetZ() * delta.GetZ()) / DespToFloat;
            const float proximity = m_active ? std::clamp((220.f - distance) / 120.f, 0.f, 1.f) : 0.f;
            m_light->SetColor(m_dying ? Color4{45,80,255} : Color4{15,30,220}, m_intensity * proximity * lightScale);
            m_light->SetRadius(proximity > 0.f && m_intensity > 0.f ?
                (m_dying ? 260.f : 240.f) * DespToFloat * lightScale : 0.f);
            m_light->UpdateGlobalBounds();
        }
        void Remove(Scene& scene) override { scene.RemoveLightSource(m_light); }
    private:
        std::shared_ptr<LightSource> m_light;
        bool m_active = false, m_dying = false;
        float m_intensity = 0.f;
    };

    class ObjectLightContext final : public ObjectPresentationContext
    {
    public:
        ObjectLightContext(GamePlay::BehaviorType behavior, std::shared_ptr<LightSource> light, bool electricImpact)
            : m_behavior(behavior), m_light(std::move(light)), m_electricImpact(electricImpact) {}

        bool Matches(const RawGameObject& object) const override
        {
            return GamePlay::PropertyAccessorBase(object).GetBehaviorType() == m_behavior &&
                IsElectricImpact(object) == m_electricImpact &&
                (m_behavior != GamePlay::BehaviorType::FlyingArrow || IsDragonFireball(object));
        }

        void Update(const RawGameObject& object, RenderableInstance* instance) override
        {
            Vector position = GamePlay::PropertyAccessorBase(object).GetPosition();
            float height = 0.f;
            switch (m_behavior)
            {
            case GamePlay::BehaviorType::LightSource:
                if (m_electricImpact)
                {
                    ApplyAdditiveMaterials(instance);
                    break;
                }
                height = instance && instance->GetRenderable() ?
                    instance->GetRenderable()->m_localBounds.GetSphereRadius() * 0.20f : 5.f * DespToFloat;
                position += Vector(0, height * 0.5f, 0);
                m_light->SetLightHeightOffset(height * 0.5f);
                break;
            case GamePlay::BehaviorType::BurningFireplace:
                height = 25.f * DespToFloat;
                position += Vector(0, height * 0.5f, 0);
                m_light->SetLightHeightOffset(height * 0.5f);
                break;
            case GamePlay::BehaviorType::PulsatingLight:
            case GamePlay::BehaviorType::SpawnInCrater:
                position += Vector(0, 5.f * DespToFloat, 0);
                break;
            case GamePlay::BehaviorType::FireballProjectile:
            case GamePlay::BehaviorType::FireballImpact:
            case GamePlay::BehaviorType::FlyingArrow:
                ApplyAdditiveMaterials(instance);
                break;
            default:
                break;
            }
            m_light->SetPosition(position);
            m_light->UpdateGlobalBounds();
        }

        void Remove(Scene& scene) override { scene.RemoveLightSource(m_light); }

    private:
        GamePlay::BehaviorType m_behavior;
        std::shared_ptr<LightSource> m_light;
        bool m_electricImpact;
    };
}

std::shared_ptr<ObjectPresentationContext> GamePlay::CreateObjectPresentation(
    Scene& scene, int levelIndex, int slot, const RawGameObject& object, RenderableInstance* instance)
{
    if (slot < 1 || slot > 165) { throw std::out_of_range("Object presentation slots must be in 1..165."); }
    const PropertyAccessorBase properties(object);
    if (properties.GetHealth() <= 0) { return {}; }
    if (levelIndex == 30 && (slot == 19 || slot == 20))
    {
        auto context = std::make_shared<KnotholeDepthContext>(slot == 19);
        if (context->Matches(object))
        {
            context->Update(object, instance);
            return context;
        }
    }
    if (properties.GetCollisionType() == CollisionType::FountainWater)
    {
        auto context = std::make_shared<FountainWaterContext>();
        context->Update(object, instance);
        return context;
    }
    if (IsFireParticle(object))
    {
        std::shared_ptr<LightSource> corona;
        if (IsFireballSpark(object)) { corona = std::make_shared<SparkCorona>(&scene); }
        auto context = std::make_shared<FireSparkContext>(corona);
        context->Update(object, instance);
        if (corona) { scene.AddLightSource(corona); }
        return context;
    }
    const auto behavior = properties.GetBehaviorType();
    if (properties.GetCollisionType() == CollisionType::MercuryRaySegment || IsElectricBolt(object))
    {
        auto context = std::make_shared<EmissiveRayContext>();
        context->Update(object, instance);
        return context;
    }
    if (behavior == BehaviorType::MercuryLamp)
    {
        auto source = std::make_shared<LightSource>(&scene);
        auto hit = std::make_shared<LightSource>(&scene);
        for (const auto& light : {source, hit})
        {
            light->SetColor({0,255,255}, 2.f);
            light->SetCoronaSize(0.f);
        }
        auto context = std::make_shared<MercuryLampContext>(source, hit);
        source->SetCoronaDepthBias(30.f * DespToFloat);
        context->Update(object, instance);
        scene.AddLightSource(source);
        scene.AddLightSource(hit);
        return context;
    }
    if (IsMercuryReceiver(object))
    {
        auto light = std::make_shared<LightSource>(&scene);
        light->SetColor({255,0,0}, 2.f);
        light->SetCoronaDepthBias(30.f * DespToFloat);
        auto context = std::make_shared<MercuryReceiverContext>(light);
        context->Update(object, instance);
        scene.AddLightSource(light);
        return context;
    }
    if (behavior == BehaviorType::Ghost)
    {
        auto light = std::make_shared<LightSource>(&scene);
        light->SetCoronaSize(0.f);
        auto context = std::make_shared<GhostLightContext>(light);
        context->Update(object, instance);
        scene.AddLightSource(light);
        return context;
    }
    if (behavior == BehaviorType::FireExhauster || behavior == BehaviorType::Dragon)
    {
        auto light = std::make_shared<LightSource>(&scene);
        light->SetRadius(0.f);
        light->SetCoronaSize(0.f);
        auto context = std::make_shared<EmitterLightContext>(behavior, light);
        scene.AddLightSource(light);
        return context;
    }
    std::shared_ptr<LightSource> light;
    switch (behavior)
    {
    case BehaviorType::LightSource:
        light = std::make_shared<FlickeringLight>(&scene);
        light->SetColor(IsElectricImpact(object) ? Color4{70,210,255} : Color4{255,240,180}, 2.f);
        light->SetCoronaColorMod(IsElectricImpact(object) ? Color4{255,255,255} : Color4{255,220,140});
        light->SetRadius(140.f * DespToFloat);
        if (IsElectricImpact(object)) { light->SetCoronaSize(0.f); }
        break;
    case BehaviorType::PulsatingLight:
        light = std::make_shared<PulsatingLight>(&scene);
        break;
    case BehaviorType::SpawnInCrater:
        // One persistent emitter anchors the crater glow; do not light all 28 smoke sources.
        if (levelIndex != 25 || slot != 24) { return {}; }
        light = std::make_shared<PulsatingLight>(&scene, 2.f);
        light->SetColor({100,255,16}, 2.f);
        light->SetRadius(250.f * DespToFloat);
        light->SetCoronaSize(0.f);
        break;
    case BehaviorType::BurningFireplace:
        light = std::make_shared<FlickeringLight>(&scene);
        light->SetColor(Color4{255,230,170}, 1.5f);
        light->SetCoronaColorMod(Color4{255,220,140});
        light->SetRadius(450.f * DespToFloat);
        break;
    case BehaviorType::FireballProjectile:
    case BehaviorType::FireballImpact:
    case BehaviorType::FlyingArrow:
        if (behavior == BehaviorType::FlyingArrow && !IsDragonFireball(object)) { return {}; }
        light = std::make_shared<FlickeringLight>(&scene);
        light->SetColor(Color4{255,140,30}, 2.f);
        light->SetRadius(120.f * DespToFloat);
        light->SetCoronaSize(0.f);
        break;
    default:
        return {};
    }
    auto context = std::make_shared<ObjectLightContext>(behavior, light, IsElectricImpact(object));
    context->Update(object, instance);
    scene.AddLightSource(light);
    return context;
}
