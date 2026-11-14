#pragma once

#include <Scene/Scene.h>
#include <Renderer/Camera.h>
#include <Renderer/Renderer.h>
#include <Renderer/RenderableInstance.h>
#include <Asset/Texture.h>
#include "ApplicationFlow.h"

class BookScene final : public Scene
{
public:
    explicit BookScene(Texture& paper) : m_paper(&paper) {}
    FUTURE(void) CreateScene() override;
    void SetPage(std::size_t slot, const std::vector<Color4>& pixels);
    void Draw(const Desperabis::UiStoryBook& view, float aspect, Renderer& renderer,
        const RenderingSettings& settings, ProfilingStats& stats);

private:
    std::shared_ptr<Renderable> m_pageMesh;
    std::array<Texture, 2> m_pages;
    RenderableInstancePtr m_left, m_turning;
    std::shared_ptr<LightSource> m_light;
    Camera m_camera;
    Texture* m_paper;
};
