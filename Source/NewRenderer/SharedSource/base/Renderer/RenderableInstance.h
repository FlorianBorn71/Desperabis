#pragma once
#include <DesperabisCommon.h>
#include <Scene/Transformable.h>
#include <Renderer/DrawCall.h>
#include <Asset/ModelGeometry.h>

class D_IMPEXP RenderableInstance
{
public:
    RenderableInstance(const Renderable* renderable = nullptr)
    {
        SetRenderable(renderable, nullptr);
    }
    ~RenderableInstance() = default;

    inline bool IsValid() const {return m_renderable != nullptr;}

    inline bool HasOpaqueMaterials() const 
    {
        return (m_usedTransparencies & TransparencyModeMask::OpaqueBit) != TransparencyModeMask::None;
    }

    inline bool HasTransparentMaterials() const
    {
        return (m_usedTransparencies & TransparencyModeMask::AllTransparent) != TransparencyModeMask::None;
    }

    inline bool HasSkyMaterials() const
    {
        return (m_usedTransparencies & TransparencyModeMask::AllSky) != TransparencyModeMask::None;
    }

    inline bool HasWaterMaterials() const
    {
        return (m_usedTransparencies & TransparencyModeMask::AllWaterReceiver) != TransparencyModeMask::None;
    }

    bool RetrieveWaterProperties(bool& useReflection, float& waterHeight) const;

    void SetDebugSelection(bool status)
    {
#ifdef _DEBUG
        m_debugColorTint = status;
#endif
    }

    void SetDebugName(const char* name)
    {
#ifdef _DEBUG
        m_debugName = name;
#endif
    }
    void OnAddedToScene(const Scene* ownerScene);

    // render state
	void ComputeTransparencyMask();
    inline TransparencyModeMask GetUsedTransparencyMask() const {return m_usedTransparencies;}

    // visible
    void SetVisible(bool status) { m_visible = status; }
    bool GetVisible() const { return m_visible; }

    // transform
	const BoundingVolume& GetGlobalBounds() const { return m_globalBounds; }
	BoundingVolume& ModifyGlobalBounds() { return m_globalBounds; }

    void UpdateGlobalBounds();
    inline const Matrix4& GetTransform() const { return m_transform; }
	void SetTranslation(const Vector& pos);
	void SetTransform(const Vector& pos, const Vector& eulerRot, float scale = 1.f);
	void SetTransform(const Matrix4& transform);
	void SetTransformAndUpdate(const Matrix4& transform);

    // renderable
    const Renderable* GetRenderable() const {return m_renderable;}
    const Renderable* GetBlendTargetRenderable() const {return m_blendToRenderable;}
    void SetRenderable(const Renderable* renderable, const Renderable* blendTarget = nullptr);
    void PrepareSubInstancesForRendering();

    // material
	std::vector<DrawCallMaterial>& GetMaterialOverrides() { return m_materialOverride; }
    const std::vector<DrawCallMaterial>& GetMaterialOverrides() const { return m_materialOverride; }
    std::vector<DrawCallMaterial>& GetOrCreateMaterialOverrides();

    // sorting/visibility
	uint32_t ComputeOpaqueSortingHash() const;
	uint32_t ComputeTransparentSortingHash(const Vector& cameraPos) const;
	uint32_t ComputeSkySortingHash() const;

protected:

    const Scene* m_ownerScene = nullptr;
    bool m_visible = true;
	bool m_globalBoundsDirty = true;
	bool m_subInstancesDirty = true;
    TransparencyModeMask m_usedTransparencies = TransparencyModeMask::None; // used transparencies across all draw calls
	Vector m_localPos, m_localOri; // only relevant for sub instances
    Matrix4 m_transform;
    BoundingVolume m_globalBounds;

    const Renderable* m_renderable = nullptr;
    const Renderable* m_blendToRenderable = nullptr;
public:
    std::vector<RenderableInstance> m_subInstances;
    std::vector<DrawCallMaterial> m_materialOverride; // one entry for each draw call
    float m_blendWeight = 0.f;
    float m_uniformScale = 1.f;
    float m_fireIntensity = 0.f;
    Color4 m_tintColor = { 255,255,255,255 };
#ifdef _DEBUG
    std::string m_debugName;
    bool m_debugColorTint = false;
#endif
};

