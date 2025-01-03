#pragma once

#include <DesperabisCommon.h>
#include <Scene/Scene.h>

class SampleScene : public Scene 
{
public:
    SampleScene();

    virtual FUTURE(void) CreateScene() override;

    virtual void Update(double elapsedTime) override;

private:
    FUTURE(void) SampleSceneWithSpider();

    FUTURE(void) InstantiateScene();
    double m_animPhase = 0.0;
    RenderableInstancePtr m_singleObj;
};
