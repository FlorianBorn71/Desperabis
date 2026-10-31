#pragma once

#include <GameContext.h>
#include <Renderer/Camera.h>
#include <Renderer/LightSource.h>
#include <Renderer/RenderableInstance.h>
#include <Asset/Texture.h>

class LightningPresentation
{
public:
    void Show(Scene& scene, const Desperabis::LightningFlash& flash, const Camera& camera);
    void Hide();

private:
    Scene* m_scene = nullptr;
    std::shared_ptr<Texture> m_texture;
    std::vector<std::shared_ptr<Renderable>> m_models;
    std::vector<RenderableInstancePtr> m_instances;
    std::vector<std::shared_ptr<LightSource>> m_lights;
};
