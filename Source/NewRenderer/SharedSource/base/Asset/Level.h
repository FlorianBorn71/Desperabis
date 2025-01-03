#pragma once
#include <DesperabisCommon.h>
#include <Asset/AssetBase.h>
#include <Asset/LevelInfo.h>
#include <Asset/GridCellArray.h>
#include <Asset/ModelGeometry.h>
#include <Asset/RawGameObject.h>
#include <Scene/GameObject.h>
#include <GamePlay/GamePlay.h>

class D_IMPEXP Level : public AssetBase
{
public:
    Level() = default;
    virtual ~Level() = default;

    FUTURE(void) Load(int index, const LevelInfo& info);
    FUTURE(void) LoadFromSaveGame(std::wstring_view filename, const std::vector<LevelInfo>& infos);

    FUTURE(void) PopulateScene(Scene& scene, bool createGameObjects);
	void ApplyRenderProperties(RenderableInstancePtr inst, RawGameObject& rawGO) const;
    RenderableInstancePtr CreatePlayerObject();
	float GetHeightAtPos(const Vector& pos) const 
    {
        return m_cellArray.GetHeightAtPos(pos);
	}

    static Vector DespToPos(int x, int y, int z);
protected:
    virtual void Read(AssetStream& stream) override;
    void ReadCollisionMap(const byte* rawData, int dataSize);
    void ReadLevelData(const byte* rawData, int dataSize, bool readSize);
    void ReadGameObjects(const byte* rawData, int dataSize);
    FUTURE(void) ReadSaveGame(const byte* rawData, int dataSize, const std::vector<LevelInfo>& infos);
    FUTURE(void) SaveGameAnalysis();

    void InitLevelInfo(const LevelInfo& info);
    void CreateWallGeometry();
    void AddStaticCellObject(int x, int y, byte rawVal, HeightLevel atHeight);
    void CreateSkyGeometry();
    void CreateSkyCylinderMesh(std::vector<DrawCall>& drawcalls, ModelLoadingContext& context, ModelConversionContext& conversion, Texture* skyTexture, float repetitionsU, float relHeight, const Color4& col, TransparencyMode transparency);
    void CreateStars(std::vector<DrawCall>& drawcalls, ModelLoadingContext& context, ModelConversionContext& conversion, int count);
    void AddCelestialSprite(ModelLoadingContext& context, ModelConversionContext& conversion, float angle, float h, float width, const Color4& col);
    void CreateLightmap();

	static Color4 GetSkyTintColor(SkyTint mode);
    static void GetFogColorAndFalloff(FogType mode, Color4& color, float& fallOff);

public:
    constexpr static int tileCenterX = 14;
    constexpr static int tileCenterY = 14;

    const std::vector<RawGameObject>& GetRawGameObjects() const { return m_rawGameObject; }
    const GridCellArray& GetCellArray() const { return m_cellArray; };

protected:
    bool CreateObjectRenderInstance(Scene& scene, std::shared_ptr<class LevelGameObject> go, RawGameObject& rawGO) const;

    int m_levelIndex = 0;
    LevelInfo m_levelInfo;
    GridCellArray m_cellArray;
    Vector m_playerStartPos;
    std::vector<RawGameObject> m_rawGameObject;

    bool m_wallGeometryCreated = false;
    std::shared_ptr<IGraphicsRuntimeObject> m_wallGeometryRuntime;

    std::vector<std::shared_ptr<Renderable>> m_wallRenderables;
    std::vector<std::shared_ptr<RenderableInstance>> m_wallGeometry;

    std::shared_ptr<Renderable> m_skyRenderable;
    std::shared_ptr<Texture> m_lightmap;

    std::shared_ptr<Texture> m_collisionFieldTex;
};

// extends the go with behavior enum
class LevelGameObject : public GameObject
{
public:
    LevelGameObject(Scene* scene, RawGameObject& rawGO)
        : GameObject(scene)
    {
        m_rawGO = rawGO;
    }
    std::shared_ptr<Renderable> m_debugGeometry; // to prevent deletion
    RawGameObject m_rawGO;
};
