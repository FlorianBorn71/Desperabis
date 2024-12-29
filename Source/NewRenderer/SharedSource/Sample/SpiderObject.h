#pragma once

#include <DesperabisCommon.h>
#include <Scene/GameObject.h>

class SpiderObject : public GameObject
{
public:
    SpiderObject(Scene* scene);

private:
    std::shared_ptr<ModelAnim> m_anim;
    std::shared_ptr<AnimationSequences> m_seqSet;
    float m_pos = 0.f;
};
