#pragma once

#include <DesperabisCommon.h>
#include <Scene/Scene.h>
#include <GamePlay/ObjectPresentation.h>
#include <array>

class PlayerLight;

class LevelScene : public Scene 
{
public:
    LevelScene(int levelIndex, std::shared_ptr<Level> loadedLevel = {});
    ~LevelScene() override;

    virtual FUTURE(void) CreateScene() override;
    virtual void ClearScene() override;

    virtual void Update(double elapsedTime) override;

    virtual const GridCellArray* GetGrid() const override;
    const std::shared_ptr<PlayerLight>& GetPlayerLight() const { return m_playerLight; }
    static constexpr std::size_t ObjectSlotCount = 166;
    void UpdateObjectPresentation(int slot, const RawGameObject& object, RenderableInstance* instance = nullptr);
    void RemoveObjectPresentation(int slot);
    void ClearObjectPresentations();
    const std::shared_ptr<ObjectPresentationContext>& GetObjectPresentation(int slot) const;
    void FlashLight(const Vector& position, const Color4& color, float fadeInSeconds, float fadeOutSeconds, float radius);
    void FireEmitted(int slot, const RawGameObject& object, const Vector& position);
    void SetElectricityLight(bool firing, const Vector& tip);
    void SetHeldTorchLight(bool burning, const Vector& tip);
    void SetSkyEnabled(bool enabled);
    bool IsSkyVisible() const { return m_skyVisible; }

public:
    int m_levelIndex = 0;
    Level* m_level = nullptr;
    bool m_createGameObjects = true;
    bool m_createMapIcons = true;
private:
    void CreateRevealSky();
    bool m_skyVisible = true;
    std::shared_ptr<Texture> m_revealSkyTexture;
    std::shared_ptr<Renderable> m_revealSkyRenderable;
    RenderableInstancePtr m_revealSkyInstance;
    std::shared_ptr<Level> m_loadedLevel;
    std::shared_ptr<PlayerLight> m_playerLight;
    std::array<std::shared_ptr<ObjectPresentationContext>, ObjectSlotCount> m_objectPresentations{};
    std::vector<std::shared_ptr<class LightFlashSource>> m_lightFlashes;
    std::shared_ptr<LightSource> m_electricityLight;
    std::shared_ptr<LightSource> m_heldTorchLight;
};
