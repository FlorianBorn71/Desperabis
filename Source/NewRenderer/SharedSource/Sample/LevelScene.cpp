#include "pch.h"
#include <Sample/LevelScene.h>

#include <Asset/AssetManager.h>
#include <Asset/Level.h>
#include <Asset/Texture.h>
#include <Renderer/Renderer.h>
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

void LevelScene::CreateRevealSky()
{
    auto texture = std::make_shared<Texture>();
    texture->CreatePlainColor({135,206,235,255});
    ModelLoadingContext context;
    ModelConversionContext conversion(context);
    for (int z = -1; z <= 1; z += 2)
        for (int y = -1; y <= 1; y += 2)
            for (int x = -1; x <= 1; x += 2)
            {
                const auto index = static_cast<uint16_t>(context.m_vertexPos.size());
                context.m_vertexPos.push_back({static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), 1.f});
                conversion.m_vertices.emplace_back().SetTextured(index, {255,255,255,0}, {0,0});
            }
    const uint16_t faces[][4]{{0,4,6,2}, {1,3,7,5}, {0,1,5,4},
        {2,6,7,3}, {0,2,3,1}, {4,5,7,6}};
    for (const auto& face : faces)
    {
        conversion.m_sortedIndices.insert(conversion.m_sortedIndices.end(),
            {face[0], face[1], face[2], face[0], face[2], face[3]});
    }
    auto renderable = std::make_shared<Renderable>();
    auto& draw = renderable->m_drawCalls.emplace_back();
    draw.m_material.m_texture = texture.get();
    draw.m_material.m_textureSlot = -1;
    draw.m_material.m_applyLighting = false;
    draw.m_material.m_useAlphaTest = false;
    draw.m_material.m_transparencyType = TransparencyMode::SkyOpaque;
    draw.m_material.m_opacity = 1.f;
    draw.m_material.ComputeHash();
    draw.m_firstIndex = 0;
    draw.m_primitiveCount = static_cast<int>(conversion.m_sortedIndices.size() / 3);
    renderable->m_runtime = Renderer::Instance().CreateModelBuffers(conversion);
    conversion.GenerateBoundingVolume(renderable->m_localBounds);
    auto instance = std::make_shared<RenderableInstance>(renderable.get(),
        static_cast<uint32_t>(GamePlay::VisibilityFilter::Sky));
    AddRenderableInstance(instance);
    m_revealSkyTexture = std::move(texture);
    m_revealSkyRenderable = std::move(renderable);
    m_revealSkyInstance = std::move(instance);
}

void LevelScene::SetSkyEnabled(bool enabled)
{
    // The epilogue's native sky-off cue replaces the storm with an unobscured daytime background.
    const bool reveal = !enabled && m_levelIndex == 95;
    if (reveal && !m_revealSkyInstance) { CreateRevealSky(); }
    for (const auto& instance : GetRenderableInstances())
    {
        if (instance->HasSkyMaterials()) { instance->SetVisible(instance == m_revealSkyInstance ? reveal : enabled); }
    }
    m_skyVisible = enabled || reveal;
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
    m_revealSkyInstance.reset();
    m_revealSkyRenderable.reset();
    m_revealSkyTexture.reset();
    m_skyVisible = true;
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
