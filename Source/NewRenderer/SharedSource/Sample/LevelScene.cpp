#include "pch.h"
#include <Sample/LevelScene.h>

#include <Asset/AssetManager.h>
#include <Asset/Level.h>
#include <Renderer/RenderableInstance.h>
#include <Scene/GameObject.h>
#include <GamePlay/GamePlay.h>
#include <stdexcept>
#include <Renderer/LightSource.h>
#include <cmath>

LevelScene::LevelScene(int levelIndex, std::shared_ptr<Level> loadedLevel)
    : m_levelIndex(levelIndex), m_loadedLevel(std::move(loadedLevel))
{
    m_renderInstances.reserve(512);
    m_gameObjects.reserve(512);
}

LevelScene::~LevelScene()
{
    ClearObjectPresentations();
}

void LevelScene::FlashLight(const Vector& position, const Color4& color, float fadeInSeconds, float fadeOutSeconds, float radius)
{
    if (!std::isfinite(radius) || radius <= 0.f ||
        !std::isfinite(fadeInSeconds) || !std::isfinite(fadeOutSeconds) ||
        fadeInSeconds < 0.f || fadeOutSeconds < 0.f ||
        !std::isfinite(fadeInSeconds + fadeOutSeconds) || fadeInSeconds + fadeOutSeconds <= 0.f)
    {
        GAME_ERROR_AND_RETURN(std::invalid_argument("Flash requires finite positive radius and duration, and nonnegative fade times."));
    }
    auto light = std::make_shared<LightFlashSource>(this, fadeInSeconds, fadeOutSeconds);
    light->SetPosition(position);
    light->SetColor(color, 4.f);
    light->SetRadius(radius);
    m_lightFlashes.push_back(light);
    AddLightSource(light);
}

void LevelScene::FireEmitted(int slot, const RawGameObject& object, const Vector& position)
{
    UpdateObjectPresentation(slot, object);
    if (auto& context = m_objectPresentations.at(slot)) { context->FireEmitted(position); }
}

void LevelScene::SetElectricityLight(bool firing, const Vector& tip)
{
    if (!firing)
    {
        if (m_electricityLight) { RemoveLightSource(m_electricityLight); m_electricityLight.reset(); }
        return;
    }
    if (!m_electricityLight)
    {
        m_electricityLight = std::make_shared<LightSource>(this);
        m_electricityLight->SetColor({70,210,255}, 2.f);
        m_electricityLight->SetCoronaColorMod({255,255,255});
        m_electricityLight->SetRadius(160.f * DespToFloat);
        m_electricityLight->SetCoronaSize(4.f * DespToFloat);
        AddLightSource(m_electricityLight);
    }
    m_electricityLight->SetPosition(tip);
}

const std::shared_ptr<ObjectPresentationContext>& LevelScene::GetObjectPresentation(int slot) const
{
    if (slot < 1 || slot >= static_cast<int>(ObjectSlotCount))
    {
        throw std::out_of_range("Object presentation slots must be in 1..165.");
    }
    return m_objectPresentations.at(slot);
}

void LevelScene::RemoveObjectPresentation(int slot)
{
    GetObjectPresentation(slot);
    auto& context = m_objectPresentations.at(slot);
    if (context) { context->Remove(*this); context.reset(); }
}

void LevelScene::ClearObjectPresentations()
{
    for (int slot = 1; slot < static_cast<int>(ObjectSlotCount); ++slot) { RemoveObjectPresentation(slot); }
}

void LevelScene::UpdateObjectPresentation(int slot, const RawGameObject& object, RenderableInstance* instance)
{
    GetObjectPresentation(slot);
    auto& context = m_objectPresentations.at(slot);
    const GamePlay::PropertyAccessorBase properties(object);
    if (properties.GetHealth() <= 0 || properties.IsRenderSuppressed())
    {
        RemoveObjectPresentation(slot);
        return;
    }
    if (context && !context->Matches(object)) { RemoveObjectPresentation(slot); }
    if (!context) { context = GamePlay::CreateObjectPresentation(*this, m_levelIndex, slot, object, instance); }
    else { context->Update(object, instance); }
}

//#define SINGLE_OBJECT

FUTURE(void) LevelScene::CreateScene()
{
    bool isSaveGame = false;
    AssetManager& assets = AssetManager::Instance();
#ifdef SINGLE_OBJECT
	// test one debug model:
	std::shared_ptr<ModelAnim> modelAnim = CO_AWAIT assets.LoadAsset<ModelAnim>(L"VEKT\\VEKT62.3DA");
	Model* model = modelAnim->GetModel(0, 0);
	RenderableInstancePtr inst = std::make_shared<RenderableInstance>(model, (uint32_t)GamePlay::VisibilityFilter::GameObjects);
	this->AddRenderableInstance(inst);
#else
    if (m_loadedLevel)
    {
        m_level = m_loadedLevel.get();
    }
    else if (isSaveGame)
    { 
        m_level = CO_AWAIT assets.LoadSaveGame(1);
    }
    else
    {
        m_level = CO_AWAIT assets.LoadLevel(m_levelIndex);
    }
    m_playerLight = CO_AWAIT m_level->PopulateScene(*this, m_createGameObjects, m_createMapIcons,
        [this](int slot, const RawGameObject& object, RenderableInstance* instance)
        {
            UpdateObjectPresentation(slot, object, instance);
        });
#endif
    m_sceneReady = true;
}

void LevelScene::ClearScene()
{
    ClearObjectPresentations();
    Scene::ClearScene();
    m_playerLight.reset();
    m_lightFlashes.clear();
    m_electricityLight.reset();
}

void LevelScene::Update(double elapsedTime)
{
    if (!std::isfinite(elapsedTime) || elapsedTime < 0.0)
    {
        throw std::invalid_argument("Scene time must be finite and nonnegative.");
    }
    if (!m_sceneReady)
    {
        return;
    }

    for (const auto& context : m_objectPresentations) { if (context) { context->Advance(elapsedTime); } }
    Scene::Update(elapsedTime);
    m_lightFlashes.erase(std::remove_if(m_lightFlashes.begin(), m_lightFlashes.end(),
        [this](const auto& light)
        {
            if (!light->IsFinished()) { return false; }
            RemoveLightSource(light);
            return true;
        }), m_lightFlashes.end());
}

const GridCellArray* LevelScene::GetGrid() const
{
    if (m_level != nullptr)
    {
        return &m_level->GetCellArray();
    }
    return nullptr;

}
