#include "pch.h"
#include <GamePlay/SimpleBehavior.h>
#include <Renderer/Renderer.h>
#include <Renderer/Camera.h>
#include <Asset/Model.h>
#include <Asset/ModelAnim.h>

/// <summary>
/// FogParticleObject
/// </summary>

FogParticleObject::FogParticleObject(Scene* scene, RawGameObject& rawGO)
    : LevelGameObject(scene, rawGO)
{
    m_phase = Math::GetRandD(10);
    m_speed = 0.3 + Math::GetRandD(0.5);
}

void FogParticleObject::Update(double elapsedTime)
{
    m_phase += elapsedTime * m_speed;
    float dh = (float)sin(m_phase) * 4.f * DespToFloat;

    // position variation
    GamePlay::PropertyAccessorBase props(m_rawGO);
    Vector pos = props.GetPosition();
    pos += Vector(0, dh, 0);
    SetPosition(pos);

    // transparency fade out by dist:
    Vector campos = Renderer::Instance().GetOrCreateCamera()->GetCameraPosition();
    pos -= campos;
    float dist = pos.GetLength();

    float opacity = __min(dist * 0.3f, 0.3f);
    DrawCallMaterial& mat(this->m_renderInst->m_materialOverride[0]);
    mat.m_opacity = opacity;
    mat.m_applyLighting = true; // looks cool, albeit not original
    mat.ComputeHash(); // it changed
}


/// <summary>
/// FliesSwarmObject
/// </summary>

FliesSwarmObject::FliesSwarmObject(Scene* scene, RawGameObject& rawGO)
    : LevelGameObject(scene, rawGO)
{

}

void FliesSwarmObject::OnAddedToScene(Scene* scene)
{
    m_rotSpeed = 1.f + Math::GetRandF(0.3f);
    Model* model = (Model *)m_renderInst->GetRenderable();
    m_anim3d = model->m_owner;
    NextFrame();
}

void FliesSwarmObject::Update(double elapsedTime)
{
    float dt = (float)elapsedTime;

    // random rotation
    float pi2 = Math::Pi * 2.f;
    m_yaw = fmodf(m_yaw + dt * 2.f * m_rotSpeed, pi2);
    m_pitch = fmodf(m_pitch + dt * 1.1f * m_rotSpeed, pi2);
    m_roll = fmodf(m_roll + dt * 1.4f * m_rotSpeed, pi2);

    Vector pos = GetPosition();
    this->m_renderInst->SetTransform(pos, Vector(m_yaw, m_pitch, m_roll));

   // blend:
    m_blendPhase += dt * m_blendSpeed;
    while (m_blendPhase > 1.f)
    {
        m_blendPhase -= 1.f;
        NextFrame();
    }
    m_renderInst->m_blendWeight = 0.5f - 0.5f * cosf(m_blendPhase * Math::Pi);
}

void FliesSwarmObject::NextFrame()
{
    GamePlay::PropertyAccessorBase props(m_rawGO);
    int anim = props.GetModelAnim();
    const int numFrames = m_anim3d->m_numFrames;
    m_currentFrame = (m_currentFrame + 1) % numFrames;
    Model* model = m_anim3d->GetModel(anim, m_currentFrame);
    Model* nextModel = m_anim3d->GetModel(anim, (m_currentFrame+1) % numFrames);

    m_renderInst->SetRenderable(model,nextModel);
    m_blendSpeed = 0.5f + Math::GetRandF(0.5f);
    m_rotSpeed = 0.5f + Math::GetRandF(0.5f);
}


/// <summary>
/// BurningFirePlaceObject
/// </summary>

BurningFireplaceObject::BurningFireplaceObject(Scene* scene, RawGameObject& rawGO)
    : LevelGameObject(scene, rawGO)
{

}

void BurningFireplaceObject::OnAddedToScene(Scene* scene)
{
    // TODO: particles

    // add a dyn. flickering light
    GamePlay::PropertyAccessorBase props(m_rawGO);
    std::shared_ptr<LightSource> light = std::make_shared<FlickeringLight>(scene);
    Vector pos = GetPosition();
    // TODO: brightness and heightOverModel should depend on bounding volume
    float heightOverModel = 25.f * DespToFloat;
    pos += Vector(0, heightOverModel * 0.5f, 0);
    light->SetLightHeightOffset(heightOverModel * 0.5f);
    light->SetPosition(pos);
    light->SetColor(Color4{ 255,230,170 }, 3.0f);
    light->SetCoronaColorMod(Color4{ 255,220,140 });
    light->SetRadius(450.f * DespToFloat);
    scene->AddLightSource(light);
}
