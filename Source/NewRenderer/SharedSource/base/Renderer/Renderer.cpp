#include "pch.h"
#include <Renderer/Renderer.h>
#include <Renderer/Camera.h>
#include <Renderer/Renderable.h>
#include <Renderer/RenderableInstance.h>
#include <Asset/Texture.h>
#include <Asset/ModelGeometry.h>
#include <Asset/ModelAnim.h>
#include <Scene/Scene.h>

Renderer* Renderer::g_Instance = nullptr;

void Renderer::SetCamera(std::shared_ptr<Camera> camera)
{
    m_defaultCamera = camera;
}

std::shared_ptr<Camera> Renderer::GetOrCreateCamera()
{
    if (m_defaultCamera == nullptr)
    {
        m_defaultCamera = std::make_shared<Camera>();
    }
    return m_defaultCamera;
}


std::shared_ptr<IVisibilityContext> Renderer::GetOrCreateVisibilityContext()
{
    if (m_defaultVisibilityContext == nullptr)
    {
        m_defaultVisibilityContext = std::make_shared<DefaultVisibilityContext>();
    }
    return m_defaultVisibilityContext;
}



void Renderer::RenderScene(const Scene* scene, ProfilingStats* out_stats)
{
    if (scene != nullptr && scene->IsReady())
    {
        if (out_stats == nullptr)
        {
            out_stats = &m_ownStats;
        }
        ProfilingStats& stats(*out_stats);

        {
            ProfilingScope _(stats.m_duration_visibility);
            Camera* cam = GetOrCreateCamera().get();

            // first try with a scene-specific visibility system
            IVisibilityContext* vis = scene->GetVisibilityContext().get();
            if (vis == nullptr)
            {
                vis = GetOrCreateVisibilityContext().get();
            }
            ConfigureVisibility(*vis);
            vis->ComputeResultFor(cam, scene, m_internalVisResult, *out_stats);
        }

        // complete scene rendering
        RenderSceneElements(m_internalVisResult, stats);
    }
}

void RenderContext::BeginRenderTargetScope(RenderTargetPassType newPasstype, const Camera* camera)
{
    if (camera == nullptr)
    {
        camera = m_elements.GetMainCamera();
    }
    m_renderTargetPass = newPasstype;
    m_currentCamera = camera;
    assert(m_passTypeStackIndex < 16);
    m_passTypeStackIndex++;
    m_passTypeStack[m_passTypeStackIndex] = newPasstype;
    m_cameraStack[m_passTypeStackIndex] = camera;
}

void RenderContext::EndRenderTargetScope()
{
    assert(m_passTypeStackIndex > 0);
    m_passTypeStackIndex--;
    m_renderTargetPass = m_passTypeStack[m_passTypeStackIndex];
    m_currentCamera = m_cameraStack[m_passTypeStackIndex];
}
