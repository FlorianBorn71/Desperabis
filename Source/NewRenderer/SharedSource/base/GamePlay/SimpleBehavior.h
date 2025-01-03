#pragma once
#include <DesperabisCommon.h>
#include <Scene\GameObject.h>
#include <Asset\Level.h>


class FogParticleObject : public LevelGameObject
{
public:
    FogParticleObject(Scene* scene, RawGameObject& rawGO);
    virtual ~FogParticleObject() = default;
    virtual void Update(double elapsedTime) override final;

private:
    double m_phase = 0.0;
    double m_speed = 1.0;
};



class FliesSwarmObject : public LevelGameObject
{
public:
    FliesSwarmObject(Scene* scene, RawGameObject& rawGO);
    virtual ~FliesSwarmObject() = default;

    virtual void OnAddedToScene(Scene* scene) override final;
    virtual void Update(double elapsedTime) override final;

private:
    void NextFrame();
    float m_yaw = 0.0;
    float m_pitch = 0.0;
    float m_roll = 0.0;
    float m_rotSpeed = 1.f;
    float m_blendPhase = 0.f;
    float m_blendSpeed = 2.f;
    int m_currentFrame = 0;
    ModelAnim* m_anim3d = nullptr;
};


class BurningFireplaceObject : public LevelGameObject
{
public:
    BurningFireplaceObject(Scene* scene, RawGameObject& rawGO);
    virtual ~BurningFireplaceObject() = default;

    virtual void OnAddedToScene(Scene* scene) override final;

private:
};


