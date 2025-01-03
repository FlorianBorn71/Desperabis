#include "pch.h"
#include <Scene/Scene.h>
#include <Scene/AnimationPose.h>
#include <Scene/GameObject.h>
#include <Renderer/Renderable.h>
#include <Renderer/RenderableInstance.h>
#include <Asset/AssetManager.h>
#include <Asset/TextureAnim.h>

Scene::Scene()
{
    m_physicsWorld = PhysicsModule::Instance().CreateWorld();
}

Scene::~Scene()
{
    m_physicsWorld.reset();
}


void Scene::ClearScene()
{
	m_physicsWorld = PhysicsModule::Instance().CreateWorld();
	m_updateFunctions.clear();

	m_lightmap.reset();
	m_collisionFieldTex.reset();

	m_renderInstances.clear();
	m_animPoses.clear();
	m_gameObjects.clear();
	m_lightSources.clear();
	m_debugGeometry.clear();

	FireAnimationSlot m_fireAnimations[16];
	m_fireAnimationInstances.clear();
}


void Scene::Update(double elapsedTime)
{
    m_currentTime += elapsedTime;
    m_physicsWorld->StartSimulation(elapsedTime);

    HandleFireAnimations(elapsedTime);


    for (auto go : m_gameObjects)
    {
        go->Update(elapsedTime);
    }
    for (auto l : m_lightSources)
    {
        l->Update(elapsedTime);
    }
    for (auto& fn : m_updateFunctions)
    {
        fn(elapsedTime);
    }
    for (auto pose : m_animPoses)
    {
        pose->Update(elapsedTime);
    }
    for (auto go : m_gameObjects)
    {
        go->ApplyAnimation();
    }
    m_physicsWorld->FetchResults();
}

void Scene::SetLightmap(std::shared_ptr<Texture> lightmap)
{
    m_lightmap = lightmap;
}

void Scene::RegisterUpdateFunction(SceneUpdateFunction fn)
{
    m_updateFunctions.push_back(std::move(fn));
}

void Scene::AddRenderableInstance(RenderableInstancePtr inst)
{
    m_renderInstances.push_back(inst);

    AssignFireAnimations(*inst.get());
    inst->OnAddedToScene(this);
}


void Scene::RemoveRenderableInstance(RenderableInstancePtr inst)
{
    m_renderInstances.erase(std::remove(m_renderInstances.begin(), m_renderInstances.end(), inst), m_renderInstances.end());
}

void Scene::AddGameObject(std::shared_ptr<GameObject> gameObject)
{
    m_gameObjects.push_back(gameObject);
    gameObject->OnAddedToScene(this);
}

void Scene::RemoveGameObject(std::shared_ptr<GameObject> gameObject)
{
    m_gameObjects.erase(std::remove(m_gameObjects.begin(), m_gameObjects.end(), gameObject), m_gameObjects.end());
}

void Scene::AddLightSource(std::shared_ptr<LightSource> light)
{
    m_lightSources.push_back(light);
    light->OnAddedToScene(this);
}

void Scene::RemoveLightSource(std::shared_ptr<LightSource> light)
{
    m_lightSources.erase(std::remove(m_lightSources.begin(), m_lightSources.end(), light), m_lightSources.end());
}


void Scene::AddAnimationPose(std::shared_ptr<AnimationPose> pose)
{
    m_animPoses.push_back(pose);
}

void Scene::AddDebugGeometry(const std::vector<DebugLine>& lineBuffer)
{
    if (std::shared_ptr<Renderable> renderable = GeometryUtils::CreateDebugGeometry(lineBuffer))
    {
        m_debugGeometry.push_back(renderable);
        RenderableInstancePtr inst = std::make_shared<RenderableInstance>(renderable.get());
        AddRenderableInstance(inst);
        inst->SetDebugName("Debug geometry");
    }
}

void Scene::AddFireAnimation(std::shared_ptr<TextureAnim> torchAnim, int targetSlot, float speed)
{
    assert(targetSlot >= 0 && targetSlot < 16);
    m_fireAnimations[targetSlot].m_torchAnim = torchAnim;
    m_fireAnimations[targetSlot].m_speed = speed;
}

void Scene::AddFireAnimationInstance(DrawCallMaterial& targetMaterial)
{
    targetMaterial.m_transparencyType = TransparencyMode::Additive; // fire animation = additive

    FireAnimationPhase& anim = m_fireAnimationInstances.emplace_back();
    anim.m_targetMaterial = &targetMaterial;
    anim.m_phase = (float)rand() * 0.001f;
    anim.m_speedMultiplier = 0.8f + fmodf((float)rand() * 0.12527f, 1.0f);
}

void Scene::AssignFireAnimations(RenderableInstance& inst)
{
    if (inst.GetRenderable() == nullptr)
    {
        return;
    }
    const int numDC = (int)inst.GetRenderable()->m_drawCalls.size();

    bool needsOverride = false;
    for (int i = 0; i < numDC; i++)
    {
        const DrawCallMaterial& mat = (i < (int)inst.m_materialOverride.size()) ? inst.m_materialOverride[i] : inst.GetRenderable()->m_drawCalls[i].m_material;
        if (mat.m_texture == nullptr && mat.m_textureSlot >= 0)
        {
            needsOverride = true;
            break;
        }
    }
    if (!needsOverride)
    {
        return;
    }

    // allocate override materials
    if ((int)inst.m_materialOverride.size() < numDC)
    {
        int oldCount = (int)inst.m_materialOverride.size();
        inst.m_materialOverride.resize(numDC);
        for (int i = oldCount; i < numDC; i++)
        {
            inst.m_materialOverride[i] = inst.GetRenderable()->m_drawCalls[i].m_material;
        }
    }

    for (int i = 0; i < numDC; i++)
    {
        DrawCallMaterial& mat = inst.m_materialOverride[i];
        if (mat.m_texture != nullptr || mat.m_textureSlot < 0)
            continue;

        AddFireAnimationInstance(mat);
        mat.ComputeHash();
    }
    inst.ComputeTransparencyMask();
}

void Scene::HandleFireAnimations(double elapsedTime)
{
    float dt = (float)elapsedTime;
    for (FireAnimationPhase& anim : m_fireAnimationInstances)
    {
        DrawCallMaterial& mat(*anim.m_targetMaterial);
        const FireAnimationSlot& slot = m_fireAnimations[mat.m_textureSlot];
        if (slot.m_torchAnim == nullptr)
        {
            // gets here from some level geometry. Assign null to prevent rendering
            mat.m_texture = nullptr;// AssetManager::Instance().GetPlainWhiteTexture();
            continue;
        }

        if ((rand() % 300) == 0)
        {
            anim.m_speedMultiplier = 0.8f + fmodf((float)rand()*0.12527f, 1.0f);
        }
        const float phaseLen = (float)slot.m_torchAnim->m_frames.size() - 0.001f;

        anim.m_phase = fmodf(anim.m_phase + dt * (slot.m_speed * anim.m_speedMultiplier), phaseLen);
        mat.m_texture = &slot.m_torchAnim->m_frames[(int)anim.m_phase];
    }
}
