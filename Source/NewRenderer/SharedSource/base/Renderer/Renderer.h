#pragma once
#include <DesperabisCommon.h>
#include <Renderer/RendererCommon.h>
#include <Renderer/Visibility.h>
#include <Renderer/DrawCall.h>
#include <Renderer/IGraphicsRuntimeObject.h>


class RenderContext
{
public:
    RenderContext(Renderer& renderer, const VisibilityResult& elements, ProfilingStats& profiling)
        : m_baseRenderer(renderer)
        , m_elements(elements)
        , m_profiling(profiling)
    {
        m_passTypeStack[0] = RenderTargetPassType::MainView;
    }

    ~RenderContext() = default;

    void BeginRenderTargetScope(RenderTargetPassType newPasstype, const Camera* camera = nullptr);
    void EndRenderTargetScope();

public:
    Renderer& m_baseRenderer;
    const VisibilityResult& m_elements;
    ProfilingStats& m_profiling;
    bool m_isStereo = false;
    RenderTargetPassType m_renderTargetPass = RenderTargetPassType::MainView; // current pass type
    const Camera* m_currentCamera = nullptr;
    RenderTargetPassType m_passTypeStack[16];
    const Camera* m_cameraStack[16] = {};
    int m_passTypeStackIndex = 0;
};


//template <typename CONTEXT_TYPE>
class D_IMPEXP Renderer
{
public:
    Renderer() = default;
    ~Renderer() = default;

    static Renderer& Instance() { return *g_Instance; }

    static bool IsReady() { return g_Instance != nullptr && g_Instance->m_isReady; }

    void SetCamera(std::shared_ptr<Camera> camera);
    std::shared_ptr<Camera> GetOrCreateCamera();

    std::shared_ptr<IVisibilityContext> GetOrCreateVisibilityContext();

    void SetCurrentTime(double sec) { m_currentTime = sec; }

    template <typename CONTEXT_TYPE>
    inline void RenderScene_Impl(const VisibilityResult& elements, ProfilingStats& stats);

    void RenderScene(const Scene* scene, ProfilingStats* out_stats = nullptr); // helper function, uses default camera and visibility context

    virtual void RenderSceneElements(const VisibilityResult& elements, ProfilingStats& out_stats) = 0;
    virtual void CreateTexture(Texture& tex, TextureLoadingContext& data) = 0;
    virtual void CloneTextureRuntime(Texture& tex, Texture& sourcetex) = 0;
    virtual std::shared_ptr<IGraphicsRuntimeObject> CreateModelBuffers(ModelConversionContext& data) = 0;
    virtual void ConfigureVisibility(IVisibilityContext& visContext) = 0;

protected:

    template <typename CONTEXT_TYPE>
    static inline void RenderSky(CONTEXT_TYPE& context, const VisibilityResult::VisibleInstances& instances);
 
    template <typename CONTEXT_TYPE>
    static inline void RenderOpaqueGeometry(CONTEXT_TYPE& context, const VisibilityResult::VisibleInstances& instances);
    
    template <typename CONTEXT_TYPE>
    static inline void RenderTransparentGeometry(CONTEXT_TYPE& context, const VisibilityResult::VisibleInstances& instances);

#ifdef ENABLE_ADVANCED_RENDERING
    template <typename CONTEXT_TYPE>
    static inline void RenderWaterInstances(CONTEXT_TYPE& context);
#endif


    template<typename T>
    inline T* GetRenderTarget(RenderTargetPassType pass)
    {
        if (pass == RenderTargetPassType::MainView)
        {
            return (T*)m_mainRT.get();
        }
        return (T*)m_mirrorRT.get();
    }

protected:
    static Renderer* g_Instance;
    bool m_isReady = false;

    struct GlobalsData
    {
        Matrix4Storage worldViewMat[2]; // world -> view
        Matrix4Storage viewWorldMat[2]; // view -> world
        Matrix4Storage projectionMat[2];
        Matrix4Storage worldViewProjMat[2];
        float gamma, inverseGamma, fPadding1, fPadding2;
        Float4Storage renderTargetSize; // xy=size, zw=1/size
        Float4Storage ambientColor; // rgb, normalmap contribution
        Float4Storage fogParameter; // rgb, falloff
        TextureTransform wsToLmCoords; // world to lm coord transform
        Float4Storage CurrentTime;
        int NumVisibleLights;
        int NumVisibleCoronas;
        int padding1, padding2;
    };

    struct InstanceData
    {
        Matrix4Storage m_modelToWSMat;
        float m_uniformScaling;
        int m_vertexOfs;  // vertex offset of primary model
        int m_vertexOfs2; // vertex offset of secondary model
        float m_blendWeight; // blend factor between two models
        Float4Storage m_objectTint; // per object tint color

        float debugColorTint;
        float padding1, padding2, padding3;
    };

    struct InstanceMaterialData
    {
        float m_transparency; // alpha modulo for transparent objects
        float m_applyLighting; // apply lighting and fog
        float padding1 = 0, padding2 = 0;
    };

    ProfilingStats m_ownStats;
    double m_currentTime = 0.0;
    std::shared_ptr<Camera> m_defaultCamera;
    std::shared_ptr<IVisibilityContext> m_defaultVisibilityContext;
    VisibilityResult m_internalVisResult;

    GlobalsData m_globalsData = {};
    InstanceData m_instanceData = {};
    InstanceMaterialData m_instanceMaterialData = {};
    std::unique_ptr<IRenderTarget> m_mainRT;
    std::unique_ptr<IRenderTarget> m_mirrorRT;
};


#include <Renderer/Renderer.inl>