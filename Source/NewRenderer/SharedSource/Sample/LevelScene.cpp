#include "pch.h"
#include <Sample/LevelScene.h>

#include <Asset/AssetManager.h>
#include <Asset/Level.h>

LevelScene::LevelScene(int levelIndex)
    : m_levelIndex(levelIndex)
{
    m_renderInstances.reserve(512);
    m_gameObjects.reserve(512);
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
	RenderableInstancePtr inst = std::make_shared<RenderableInstance>(model);
	this->AddRenderableInstance(inst);
#else
    if (isSaveGame)
    { 
        m_level = CO_AWAIT assets.LoadSaveGame(1);
    }
    else
    {
        m_level = CO_AWAIT assets.LoadLevel(m_levelIndex);
    }
    CO_AWAIT m_level->PopulateScene(*this, m_createGameObjects);
#endif
    m_sceneReady = true;
}

void LevelScene::Update(double elapsedTime)
{
    if (!m_sceneReady)
    {
        return;
    }

    Scene::Update(elapsedTime);
}

const GridCellArray* LevelScene::GetGrid() const
{
    if (m_level != nullptr)
    {
        return &m_level->GetCellArray();
    }
    return nullptr;

}
