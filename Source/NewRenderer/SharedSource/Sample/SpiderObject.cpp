#include "pch.h"
#include <Sample/SpiderObject.h>
#include <Asset/AssetManager.h>
#include <Asset/AnimationSequence.h>
#include <Asset/ModelAnim.h>
#include <Renderer/RenderableInstance.h>

SpiderObject::SpiderObject(Scene* scene):
    GameObject(scene)
{
    AssetManager& assets = AssetManager::Instance();

    m_anim = assets.GetAsset<ModelAnim>(L"VEKT\\VEKT10.3DA");
    Model* model = m_anim->GetModel(0, 3);

    m_renderInst = std::make_shared<RenderableInstance>(model);
    //m_renderInst->m_blendWeight = 0.5f;
    //m_renderInst->m_blendToRenderable = anim->GetModel(0, 1);
    Vector p(0, 0, -2);
    Vector r(0, 0, 0);
    this->m_renderInst->SetTransform(p, r, 1.0f);

    // register renderable
    scene->AddRenderableInstance(m_renderInst);

    m_seqSet = assets.GetAsset<AnimationSequences>(L"VEKT\\vekt10.animSeq");
    AnimationSequence* seq = &m_seqSet->m_sequences[0];

    
    // start animation
    m_animPose = std::make_shared<AnimationPose>();
    m_animPose->Start(m_anim.get(), seq, 1.0f);
    scene->AddAnimationPose(m_animPose);
}
