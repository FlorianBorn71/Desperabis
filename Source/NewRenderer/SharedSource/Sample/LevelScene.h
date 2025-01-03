#pragma once

#include <DesperabisCommon.h>
#include <Scene/Scene.h>

class LevelScene : public Scene 
{
public:
    LevelScene(int levelIndex);

    virtual FUTURE(void) CreateScene() override;

    virtual void Update(double elapsedTime) override;

    virtual const GridCellArray* GetGrid() const override;

public:
    int m_levelIndex = 0;
    Level* m_level = nullptr;
    bool m_createGameObjects = true;
};
