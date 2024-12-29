#pragma once
#include <DesperabisCommon.h>
#include <Scene/Scene.h>
#include <Scene/AnimationPose.h>

class GameObjectBase
{
public:
    GameObjectBase(Scene* scene);
    virtual ~GameObjectBase() = default;

    virtual void OnAddedToScene(Scene* scene) {}
    virtual void Update(double elapsedTime) {}
public:
    Scene* m_scene = nullptr;
};


class GameObject : public GameObjectBase
{
public:
    GameObject(Scene* scene);
    virtual ~GameObject() = default;

    void SetPosition(const Vector& pos);
    Vector GetPosition() const;

    void ApplyAnimation();

public:
    std::shared_ptr<AnimationPose> m_animPose;
    RenderableInstancePtr m_renderInst;
};

