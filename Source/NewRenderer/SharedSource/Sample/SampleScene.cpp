#include "pch.h"
#include <Sample/SampleScene.h>
#include <Asset/AssetManager.h>
#include <Asset/ModelAnim.h>
#include <Asset/AnimationSequence.h>
#include <Asset/Level.h>
#include <Asset/Model.h>
#include <Renderer/RenderableInstance.h>
#include <Renderer/Renderer.h>
#include <Renderer/Camera.h>
#include <Sample/SpiderObject.h>

SampleScene::SampleScene()
{
}

bool useSingleO = true;
std::wstring singleO = L"VEKT\\VEKT10.3DA";
std::wstring singleA = L"VEKT\\vekt10.animSeq";

static const int vektModels[] = { 2, 3, 4, 7, 8, 10,11,12,13,15,21,24,25,27,32,36,38,43,72 };
static const int vektCount = ARRAYSIZE(vektModels);


FUTURE(void) SampleScene::CreateScene()
{
    return SampleSceneWithSpider();
}


FUTURE(void) SampleScene::SampleSceneWithSpider()
{
    AssetManager& assets = AssetManager::Instance();

    if (useSingleO)
    {
        CO_AWAIT assets.LoadAsset<ModelAnim>(singleO);
        if (!singleA.empty())
        {
            CO_AWAIT assets.LoadAsset<AnimationSequences>(singleA);
        }
    }
    else
    {
        // preload data:
        for (int i = 0; i < vektCount; i++)
        {
            std::wstring name = L"VEKT\\VEKT" + std::to_wstring(vektModels[i]) + L".3DA";
            CO_AWAIT assets.LoadAsset<ModelAnim>(name);
        }
    }

    InstantiateScene();
}

FUTURE(void) SampleScene::InstantiateScene()
{
    AssetManager& assets = AssetManager::Instance();

    // 2 rat/bat
    // 7 Krabbe/Pflanze
    // 24: Wald -> 3,3

    if (useSingleO)
    {
        std::shared_ptr<SpiderObject> spider =  std::make_shared<SpiderObject>(this);
        this->m_gameObjects.push_back(spider);
/*
        ModelAnim* anim = assets.GetAsset<ModelAnim>(singleO);
        Model* model = anim->GetModel(0, 0);

        m_singleObj = std::make_shared<RenderableInstance>(model);
        m_singleObj->m_blendWeight = 0.5f;
        m_singleObj->m_blendToRenderable = anim->GetModel(0, 1);

        this->AddRenderableInstance(m_singleObj);
        if (!singleA.empty())
        {
            AnimationSequences* seqSet = assets.GetAsset<AnimationSequences>(singleA);
            AnimationSequence* seq = &seqSet->m_sequences[0];
            //AnimationPose
        }
        */
    }
    else
    {
        for (int i = 0; i < vektCount; i++)
        {
            int vnr = vektModels[i];
            std::wstring name = L"VEKT\\VEKT" + std::to_wstring(vnr) + L".3DA";
            std::shared_ptr<ModelAnim> anim = assets.GetAsset<ModelAnim>(name);
            Model* model = anim->GetModel(0, 0);

            if (vnr == 24)
            {
                model = anim->GetModel(0, 2); // forest
            }

            RenderableInstancePtr inst = std::make_shared<RenderableInstance>(model);
            this->AddRenderableInstance(inst);

            float scale = 0.5f;
            inst->m_uniformScale = scale;
            float x = ((float)(i % 5) - 2.5f) * 0.5f;
            float y = 0.f;
            float z = ((float)(i / 5) - 2.5f) * 0.5f;
            inst->SetTransform(Vector(x, y, z), Vector(0, 0, 0), scale);
        }
    }

    m_sceneReady = true;
    CO_RETURN;
}


void SampleScene::Update(double elapsedTime)
{
    if (!m_sceneReady)
    {
        return;
    }

    m_animPhase += elapsedTime;

    Scene::Update(elapsedTime);

    if (m_singleObj)
    {
        float x = (float)(2.0 * cos(m_animPhase));
        float y = 0;
        float z = 0.f;// (float)sin(m_animPhase);;
        x = y = z = 0.f;

        float yaw = 3.1f; // (float)m_animPhase;
        float scale = 1.5f;
        //m_singleObj->m_blendWeight = 0.5f + cosf((float)(m_animPhase*20.0)) * 0.5f;

        m_singleObj->m_uniformScale = scale;
        
        m_singleObj->SetTransform(Vector(x,y,z), Vector(0, yaw, 0), scale);
    }
}
