#include "pch.h"
#include <Scene/GameObject.h>
#include <Renderer/RenderableInstance.h>



GameObjectBase::GameObjectBase(Scene* scene) : m_scene(scene)
{

}


GameObject::GameObject(Scene* scene) : GameObjectBase(scene)
{
}


void GameObject::SetPosition(const Vector& pos)
{
    if (m_renderInst != nullptr)
    {
        m_renderInst->SetTranslation(pos);
    }
}

Vector GameObject::GetPosition() const
{
    if (m_renderInst != nullptr)
    {
        return m_renderInst->GetTransform().GetTranslation();
    }
    return Vector();
}

void GameObject::ApplyAnimation()
{
    if (m_animPose != nullptr && m_renderInst != nullptr)
    {
        m_animPose->ApplyTo(*m_renderInst);
    }
}
