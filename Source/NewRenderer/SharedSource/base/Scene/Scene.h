#pragma once
#include <DesperabisCommon.h>
#include <Asset/ModelGeometry.h>
#include <Physics/PhysicsInterface.h>


using SceneUpdateFunction = std::function<void(double)>;

class Scene
{
public:
    Scene();
    virtual ~Scene();

    virtual FUTURE(void) CreateScene() = 0;
    virtual void ClearScene();

    void SetInitialPosition(const Vector& pos) { m_initialPos = pos; }
    const Vector& GetInitialPosition() const { return m_initialPos; }

    bool IsReady() const { return m_sceneReady; }

    virtual void Update(double elapsedTime);

    // the visibility collector is overridable. If null, the renderer uses a default one
    virtual std::shared_ptr<IVisibilityContext> GetVisibilityContext() const
    {
        return nullptr;
    }

    void SetLightmap(std::shared_ptr<Texture> lightmap);
    std::shared_ptr<Texture> GetLightmap() const { return m_lightmap; }

    void SetCollisionFieldTexture(std::shared_ptr<Texture> tex) { m_collisionFieldTex = tex; }
    std::shared_ptr<Texture> GetCollisionFieldTexture() const { return m_collisionFieldTex; }

    void RegisterUpdateFunction(SceneUpdateFunction fn);
	void AddRenderableInstance(RenderableInstancePtr inst);
    void RemoveRenderableInstance(RenderableInstancePtr inst);
    void AddGameObject(std::shared_ptr<GameObject> gameObject);
	void RemoveGameObject(std::shared_ptr<GameObject> gameObject);
	void AddLightSource(std::shared_ptr<LightSource> light);
    void RemoveLightSource(std::shared_ptr<LightSource> light);
    void AddAnimationPose(std::shared_ptr<AnimationPose> pose);
    void AddDebugGeometry(const std::vector<DebugLine>& lineBuffer);

    void AddFireAnimation(std::shared_ptr<TextureAnim> torchAnim, int targetSlot, float speed);
    void AddFireAnimationInstance(DrawCallMaterial& targetMaterial);
    void AssignFireAnimations(RenderableInstance& inst);
    void HandleFireAnimations(double elapsedTime);

    inline double GetSceneTime() const 
    {
        return m_currentTime;
    }

    const std::vector<RenderableInstancePtr>& GetRenderableInstances() const 
    {
        return m_renderInstances;
    }
    const std::vector<std::shared_ptr<GameObject>>& GetGameObjects() const 
    {
        return m_gameObjects;
    }

    const std::vector<std::shared_ptr<LightSource>>& GetLightSources() const
    {
        return m_lightSources;
    }

    void SetClearColor(const Vector4& color) { m_clearColor = color; }
    const Vector4& GetClearColor() const { return m_clearColor; }

    void SetFogParams(const Vector3& fogColor, float falloff) {
        m_fogColor = fogColor;
        m_fogFalloff = falloff;
    }

	void GetFogParams(Vector3& fogColor, float& falloff) const {
		fogColor = m_fogColor;
		falloff = m_fogFalloff;
	}

    virtual const GridCellArray* GetGrid() const { return nullptr; }

    PhysicsWorld* GetPhysicsWorld() { return m_physicsWorld.get(); }
protected:
    struct FireAnimationSlot
    {
        std::shared_ptr<TextureAnim> m_torchAnim;
        float m_speed = 0.f;
    };

    struct FireAnimationPhase
    {
        float m_phase;
        float m_speedMultiplier;
        DrawCallMaterial* m_targetMaterial;
    };

    std::unique_ptr<PhysicsWorld> m_physicsWorld;

    Vector4 m_clearColor = { 0,0,0,0 };
    Vector3 m_fogColor = { 0,0,0 };
    float m_fogFalloff = 0.1f;

    Vector m_initialPos;
    std::vector<SceneUpdateFunction> m_updateFunctions;

    std::shared_ptr<Texture> m_lightmap;
    std::shared_ptr<Texture> m_collisionFieldTex;

    std::vector<RenderableInstancePtr> m_renderInstances;
    std::vector<std::shared_ptr<AnimationPose>> m_animPoses;
    std::vector<std::shared_ptr<GameObject>> m_gameObjects;
    std::vector<std::shared_ptr<LightSource>> m_lightSources;
    std::vector <std::shared_ptr<Renderable>> m_debugGeometry;

    FireAnimationSlot m_fireAnimations[16];
    std::vector<FireAnimationPhase> m_fireAnimationInstances;

    double m_currentTime = 0.0;
    float m_waterLevel = 0.f;

    bool m_sceneReady = false;
};

