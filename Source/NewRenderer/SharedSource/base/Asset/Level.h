#pragma once
#include <DesperabisCommon.h>
#include <Asset/AssetBase.h>
#include <Asset/LevelInfo.h>
#include <Asset/GridCellArray.h>
#include <Asset/ModelGeometry.h>
#include <Asset/RawGameObject.h>
#include <Scene/GameObject.h>
#include <GamePlay/GamePlay.h>

class PlayerLight;

class D_IMPEXP Level : public AssetBase
{
public:
    Level() = default;
    virtual ~Level() = default;

    FUTURE(void) Load(int index, const LevelInfo& info);
    FUTURE(void) LoadFromSaveGame(std::wstring_view filename, const std::vector<LevelInfo>& infos);
    FUTURE(void) LoadSaveGameData(const byte* rawData, int dataSize);

    using ObjectPresentationCallback = std::function<void(int, const RawGameObject&, RenderableInstance*)>;
    FUTURE(std::shared_ptr<PlayerLight>) PopulateScene(Scene& scene, bool createGameObjects, bool addMapIcons,
        ObjectPresentationCallback objectPresentation = {});
	void ApplyRenderProperties(RenderableInstancePtr inst, RawGameObject& rawGO, Texture* spriteOverride = nullptr) const;
    RenderableInstancePtr CreatePlayerObject();
    std::size_t SynchronizeWallTiles(const byte* tiles, std::size_t width, std::size_t height, std::size_t rowStride);
    std::size_t SynchronizeFloorTiles(const byte* tiles, std::size_t width, std::size_t height, std::size_t rowStride);
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
    FUTURE(void) SaveGameAnalysis();

    void InitLevelInfo(const LevelInfo& info);
    void CreateWallGeometry();
    RenderableInstancePtr AddStaticCellObject(int x, int y, byte rawVal, HeightLevel atHeight);
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
    const LevelInfo& GetLevelInfo() const { return m_levelInfo; }

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
    struct MutableWall
    {
        int column, row;
        byte originalWall;
        RenderableInstancePtr instance;
    };
    std::vector<MutableWall> m_mutableWalls;
    struct MutableFloor
    {
        int column, row;
        byte originalFloor, replacementFloor;
        RenderableInstancePtr original, replacement;
    };
    std::vector<MutableFloor> m_mutableFloors;

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
