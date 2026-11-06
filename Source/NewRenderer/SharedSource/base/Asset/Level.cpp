#include "pch.h"
#include <Asset/Level.h>

#include <Asset/AssetManager.h>
#include <Asset/ModelAnim.h>
#include <Asset/DebugGeometry.h>
#include <IO/FileManager.h>
#include <Scene/Scene.h>
#include <Scene/GameObject.h>
#include <Renderer/RenderableInstance.h>
#include <GamePlay/GamePlay.h>
#include <cmath>
#include <limits>
#include <stdexcept>


// gameplay level index to level file
constexpr const static int LevelToLevelFile[] = {
    10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26, // part 2
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, // padding
    30,31,32,33,34,35,36, // part 1
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, // padding
    50 // tutorial
};

constexpr const int LevelCount = sizeof(LevelToLevelFile) / sizeof(LevelToLevelFile[0]);


static_assert(LevelToLevelFile[30] == 30);
static_assert(LevelToLevelFile[50] == 50);

void Level::Read(AssetStream& stream)
{
    assert(!"Should not get here");
}


Color4 Level::GetSkyTintColor(SkyTint mode)
{
	switch (mode)
	{
	case SkyTint::NoSky: return { 255,255,255,0 };
	case SkyTint::Identity: return { 255,255,255,0 };
	case SkyTint::WhiteFog: return { 255,255,255,130 };
	case SkyTint::Dawn: return { 100,100,100,0 };
	case SkyTint::Night: return { 40,40,40,0 };
	}
	return { 255,255,255,0 };
}

void Level::GetFogColorAndFalloff(FogType mode, Color4& color, float& fallOff)
{
	switch (mode)
	{
    case FogType::Standard:
        color = { 10,12,16,0 }; 
        fallOff = 0.25f;
        break;
	case FogType::Outside:
		color = { 110,100,75,0 };
		fallOff = 0.10f;
		break;
	case FogType::WhiteFog:
		color = { 230,240,255,0 };
		fallOff = 0.05f;
		break;
	case FogType::Green:
		color = { 10,35,16,0 };
		fallOff = 0.25f;
		break;
	case FogType::OutsideBright:
		color = { 110,120,140,0 };
		fallOff = 0.05f;
		break;
	case FogType::CaveMachines:
		color = { 20,12,8,0 };
		fallOff = 0.3f;
		break;
	case FogType::Forest:
		color = { 10,12,16,0 };
		fallOff = 0.25f;
		break;
	}
}

Vector Level::DespToPos(int x, int y, int z)
{
    float scale = DespToFloat;
    float xofs = (float)tileCenterX * 100.0f + 100.0f;
    float yofs = 41 * 100.0f - (float)tileCenterY * 100.0f;

    return Vector(((float)x - xofs) * scale, (float)y * scale, (yofs - (float)z) * scale);
}

void Level::InitLevelInfo(const LevelInfo& info)
{
    m_levelInfo = info;

    for (int i = 0; i < 4; i++)
    {
        const LevelInfo::WaterSlot& src(m_levelInfo.m_raw.m_data.water[i]);
        m_cellArray.m_waterHeights[i] = (float)src.height * -10.f * DespToFloat;
    }

}


FUTURE(void) Level::Load(int index, const LevelInfo& info)
{
    m_isSaveGame = false;
    m_levelIndex = index;
    InitLevelInfo(info);

    {
        // pre-load level data. Needs to be done first, so level textures are available
        CO_AWAIT m_levelInfo.PreloadData();
    }

    {
        // level geometry
        std::wstringstream wss;
        wss << L"LEVEL\\FELD" << index << L".256";
        std::wstring path = FileManager::Instance().FilenameToNative(wss.str());
        std::vector<byte> fileData = CO_AWAIT FileManager::Instance().ReadDataFuture(path);
        ReadLevelData(fileData.data(), (int)fileData.size(), true);
    }

    {
        // game objects
        std::wstringstream wss;
        wss << L"LEVEL\\OBJPOS" << index << L".DAT";
        std::wstring path = FileManager::Instance().FilenameToNative(wss.str());
        std::vector<byte> fileData = CO_AWAIT FileManager::Instance().ReadDataFuture(path);
        ReadGameObjects(fileData.data(), (int)fileData.size());
    }
}




FUTURE(void) Level::LoadFromSaveGame(std::wstring_view filename, const std::vector<LevelInfo>&)
{
//    CO_AWAIT SaveGameAnalysis();

    std::wstring path = FileManager::Instance().FilenameToNative(filename);
    std::vector<byte> fileData = CO_AWAIT FileManager::Instance().ReadDataFuture(path);
    CO_AWAIT LoadSaveGameData(fileData.data(), (int)fileData.size());

    /*
    Windows::Storage::StorageFolder^ storageFolder = ApplicationData::Current->LocalFolder;
    create_task(storageFolder->GetFileAsync("sample.txt")).then([](StorageFile^ sampleFile)
        {
            // Process file
        });
        */
}


namespace
{
    short ReadShort(const byte*& rawData)
    {
        short val = *((short*)rawData);
        rawData += 2;
        return val;
    }

    const byte* Advance(const byte*& rawData, int count)
    {
        const byte* start = rawData;
        rawData += count;
        return start;
    }

    std::string ByteArrayToStr(const byte* rawData, int count)
    {
        std::stringstream s;
        for (int i = 0; i < count; i++)
        {
            if ((i & 63) == 0) s << "\n";
            if (i > 0) s << ",";
            s << (int)rawData[i];
        }
        return s.str();
    }
}

void Level::ReadLevelData(const byte* rawData, int dataSize, bool readSize)
{
    const int headerSize = readSize ? 4 : 0;
    if (!rawData || dataSize < headerSize)
    {
        throw std::invalid_argument("Level geometry requires complete dimensions and seven native planes.");
    }
    int width = GridCellArray::Width;
    int height = GridCellArray::Height;
    if (readSize)
    {
        width = ReadShort(rawData) + 1;
        height = ReadShort(rawData) + 1;
    }
    if (width < 1 || height < 1 || width > GridCellArray::Width || height > GridCellArray::Height ||
        dataSize - headerSize < 7 * width * height)
    {
        throw std::invalid_argument("Level geometry requires seven complete planes fitting the native 50x40 grid.");
    }
    const int count = width * height;
    
    const byte* wallTextures = Advance(rawData, count);
    const byte* rawDataFloor = Advance(rawData, count);
    const byte* rawDataCeiling = Advance(rawData, count);
    const byte* rawDataHeight = Advance(rawData, count);
    const byte* rawDataCeilingHeight = Advance(rawData, count);
    const byte* rawDataWalls = Advance(rawData, count);
    const byte* rawDataMaterials = Advance(rawData, count);

    float scale = DespToFloat * -2.0f;
    GridCell padding{};
    padding.m_floorHeight = 255.f * scale;
    padding.m_ceilingHeight = padding.m_floorHeight - 50.f * scale;
    padding.m_brightness.Set(100, 100, 100, 0);
    m_cellArray.m_sizeX = GridCellArray::Width;
    m_cellArray.m_sizeZ = GridCellArray::Height;
    m_cellArray.m_authoredSizeX = width;
    m_cellArray.m_authoredSizeZ = height;
    m_cellArray.m_cell.assign(GridCellArray::Width * GridCellArray::Height, padding);
    int i = 0;
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++, i++)
        {
            GridCell& cell(m_cellArray.GetCell(x, y));
            cell.m_floorHeight = (float)rawDataHeight[i] * scale;
            cell.m_ceilingHeight = cell.m_floorHeight - (float)rawDataCeilingHeight[i] * scale;
            cell.m_wallTextures = wallTextures[i];
            cell.m_walls = rawDataWalls[i];
            byte collision = cell.m_walls & 31;
            cell.m_waterSlot = (WaterSlotIndex)collision;
            if (cell.m_waterSlot<WaterSlotIndex::WaterSlot1 || cell.m_waterSlot>WaterSlotIndex::WaterSlot4)
                cell.m_waterSlot = WaterSlotIndex::None;
            cell.m_floor = rawDataFloor[i];
            cell.m_ceiling = rawDataCeiling[i];
            cell.m_rawMaterial = rawDataMaterials[i];
            cell.m_groundMaterial = (GroundMaterial)(cell.m_rawMaterial & 15);
            cell.m_visibilityIsland = 0;
            cell.m_relevantForVisibility = false;
//            cell.m_relevantForVisibility = cell.m_ceiling != 0 || cell.m_floor != 0 || cell.m_wallTextures != 0;
            int c = rawDataWalls[i];
            int brightness = (int)(c >> 5) * 155 / 7 + 100;
            cell.m_brightness.Set(brightness, brightness, brightness, 0);
        }
    }

    bool isOutdoor = m_levelInfo.m_skyTexture != nullptr;

    m_cellArray.DetermineVisibilityIslands(isOutdoor);
}

void Level::ReadGameObjects(const byte* rawData, int dataSize)
{
#ifdef _DEBUG
    std::string str0 = ByteArrayToStr(rawData, 54); // DEBUG
#endif

    const int objSize = sizeof(RawGameObject::m_rawBytes);
    static_assert(objSize == 54);
    assert(dataSize % objSize == 0 && dataSize > 0);
    const int count = dataSize / objSize;
    m_rawGameObject.resize(count);
    RawGameObject* go = m_rawGameObject.data();
    for (int i = 0; i < count; i++, go++, rawData+= objSize)
    {
        go->m_level = this;
        go->m_objectNumber = i; // +1 for one-base?

//        ((byte*)rawData)[8] = 0; // turn off behavior in savegame
        memcpy(go->m_rawBytes, rawData, objSize);
    }
}


void Level::ReadCollisionMap(const byte* rawData, int dataSize)
{
    TextureLoadingContext context;

    std::vector<byte> dataCopy;
    dataCopy.assign(rawData, rawData + dataSize);
    AssetStream stream(dataCopy);
    m_collisionFieldTex = std::make_shared<Texture>();
    m_collisionFieldTex->Read(stream, context);

    Renderer::Instance().CreateTexture(*m_collisionFieldTex, context);

}


namespace 
{
    // generates only one position entry for every grid cell corner, to reduce size of the position buffer
    class LevelVertexCache
    {
    public:
        LevelVertexCache(const Level& level, std::vector<VertexPosition>& posBuffer)
            : m_level(level), m_vertexPos(posBuffer)
        {
            for (int y = 0; y < MaxLevelSize; y++)
                for (int x = 0; x < MaxLevelSize; x++)
                    for (int f = 0; f < (int)HeightLevel::Count; f++)
                        m_cachedIndex[y][x][f] = -1;

        }

        uint16_t GetGridPos(int tileX, int tileY, HeightLevel atHeight)
        {
            int& cache(m_cachedIndex[tileY][tileX][(int)atHeight]);
            if (cache >= 0)
            {
                return (uint16_t)cache;
            }
            cache = (int)m_vertexPos.size();
            Vector3 pos = m_level.GetCellArray().GetGridPos(tileX, tileY, atHeight);
            m_vertexPos.emplace_back() = pos;
            return cache;
        }

        const Level& m_level;
        std::vector<VertexPosition>& m_vertexPos;
        static constexpr int MaxLevelSize = 64;
        int m_cachedIndex[MaxLevelSize][MaxLevelSize][(int)HeightLevel::Count];
    };


    void AddWall(LevelVertexCache &levelVerts, std::vector<ModelVertex>& vertices, std::vector<uint16_t>& indices, int x1, int y1, int x2, int y2, bool flippedFace, Color4 col, bool flippedU)
    {
//        if (flippedU) 
//            col = { 255, 0, 0, 255 };
        float u0 = flippedU ? 1.0f : 0.0f;
        float u1 = 1.0f - u0;

        // positions (shared):
        int v0 = levelVerts.GetGridPos(x1, y1, HeightLevel::Ceiling);
        int v1 = levelVerts.GetGridPos(x2, y2, HeightLevel::Ceiling);
        int v2 = levelVerts.GetGridPos(x2, y2, HeightLevel::Floor);
        int v3 = levelVerts.GetGridPos(x1, y1, HeightLevel::Floor);

        // vertices:
        int vertexStart = (int)vertices.size();

        if (true) // planar vertical wall mapping
        {
            const auto& pos = levelVerts.m_vertexPos;
            float yToV = -400 * 0.005f * DespRelativeInvScaling;
            vertices.emplace_back().SetTextured(v0, col, { pos[v0].y * yToV, u0 });
            vertices.emplace_back().SetTextured(v1, col, { pos[v1].y * yToV, u1 });
            vertices.emplace_back().SetTextured(v2, col, { pos[v2].y * yToV, u1 });
            vertices.emplace_back().SetTextured(v3, col, { pos[v3].y * yToV, u0 });

        }
        else
        {
            vertices.emplace_back().SetTextured(v0, col, { 0, u0 });
            vertices.emplace_back().SetTextured(v1, col, { 0, u1 });
            vertices.emplace_back().SetTextured(v2, col, { 3, u1 });
            vertices.emplace_back().SetTextured(v3, col, { 3, u0 });
        }

        // indices
        if (flippedFace)
        {
            indices.push_back((uint16_t)(vertexStart + 2));
            indices.push_back((uint16_t)(vertexStart + 1));
            indices.push_back((uint16_t)(vertexStart + 0));
            indices.push_back((uint16_t)(vertexStart + 3));
            indices.push_back((uint16_t)(vertexStart + 2));
            indices.push_back((uint16_t)(vertexStart + 0));
        }
        else
        {
            indices.push_back((uint16_t)(vertexStart + 0));
            indices.push_back((uint16_t)(vertexStart + 1));
            indices.push_back((uint16_t)(vertexStart + 2));
            indices.push_back((uint16_t)(vertexStart + 0));
            indices.push_back((uint16_t)(vertexStart + 2));
            indices.push_back((uint16_t)(vertexStart + 3));
        }
    }

    void AddFloorOrCeiling(HeightLevel heightAt, LevelVertexCache& levelVerts, std::vector<ModelVertex>& vertices, std::vector<uint16_t>& indices, int x1, int y1, bool flipped, Color4 col = { 255,255,255,0 })
    {
        int x2 = x1 + 1;
        int y2 = y1 + 1;
        float u0 = flipped ? 1.0f : 0.0f;
        float u1 = 1.0f - u0;

        // positions (shared):
        int v0 = levelVerts.GetGridPos(x1, y1, heightAt);
        int v1 = levelVerts.GetGridPos(x2, y1, heightAt);
        int v2 = levelVerts.GetGridPos(x2, y2, heightAt);
        int v3 = levelVerts.GetGridPos(x1, y2, heightAt);

        int vertexStart = (int)vertices.size();
        vertices.emplace_back().SetTextured(v0, col, { u0,0 });
        vertices.emplace_back().SetTextured(v1, col, { u1,0 });
        vertices.emplace_back().SetTextured(v2, col, { u1,1 });
        vertices.emplace_back().SetTextured(v3, col, { u0,1 });
        for (int i = vertexStart; i < vertexStart + 4; ++i)
        {
            vertices[i].NormalGroup = 2 + static_cast<uint16_t>(heightAt);
        }
        //Vertex.GenerateNormals(vertices, vertexStart, 4, !isFloor);

        if (heightAt != HeightLevel::Ceiling)
        {
            // floor and water
            indices.push_back((uint16_t)(vertexStart + 0));
            indices.push_back((uint16_t)(vertexStart + 1));
            indices.push_back((uint16_t)(vertexStart + 2));
            indices.push_back((uint16_t)(vertexStart + 0));
            indices.push_back((uint16_t)(vertexStart + 2));
            indices.push_back((uint16_t)(vertexStart + 3));
        }
        else
        {
            indices.push_back((uint16_t)(vertexStart + 2));
            indices.push_back((uint16_t)(vertexStart + 1));
            indices.push_back((uint16_t)(vertexStart + 0));
            indices.push_back((uint16_t)(vertexStart + 3));
            indices.push_back((uint16_t)(vertexStart + 2));
            indices.push_back((uint16_t)(vertexStart + 0));
        }
    }

	class IslandWallConversion
	{
	public:
		IslandWallConversion(Level* level)
			: m_conversion(m_context),
			m_vertexCache(*level, m_context.m_vertexPos)
		{

		}
		~IslandWallConversion() = default;

		ModelLoadingContext m_context;
		ModelConversionContext m_conversion;
		LevelVertexCache m_vertexCache;
		std::vector<DrawCall> m_dcs;
        struct MutableDrawCall
        {
            int column, row;
            DrawCall drawCall;
        };
        std::vector<MutableDrawCall> m_mutableWalls;
        struct MutableFloorDrawCall
        {
            std::size_t floorIndex;
            bool replacement;
            DrawCall drawCall;
        };
        std::vector<MutableFloorDrawCall> m_mutableFloors;
	};


    RenderableInstancePtr CreateSpriteInstance(GamePlay::DisplayType display, Texture* texture, char textureSlot, float scale, bool flipU, GamePlay::VisibilityFilter visibilityFilterMask)
    {
        AssetManager& assets = AssetManager::Instance();
        RenderablePtr spriteMesh = assets.GetSpriteRenderable(flipU);
        RenderableInstancePtr inst = std::make_shared<RenderableInstance>(spriteMesh.get(), (uint32_t)visibilityFilterMask);
        inst->m_uniformScale = scale;
        if (scale == 0.f)
        {
            inst->SetVisible(false); // for debugging, no idea what this means, e.g. for a fog particle. Maybe it's invisible switches
        }
        inst->m_tintColor = { 255,255,255,255 };
		inst->m_fireIntensity = 0.f;

        DrawCallMaterial& overrideMat = inst->m_materialOverride.emplace_back();
        overrideMat = spriteMesh->m_drawCalls[0].m_material;
        overrideMat.m_transparencyType = TransparencyMode::Opaque;
        overrideMat.m_opacity = 1.f;
        overrideMat.m_applyLighting = GamePlay::IsLitSpriteDisplayType(display);

        switch (display)
        {
        case GamePlay::DisplayType::SpriteTransparent:
            overrideMat.m_transparencyType = TransparencyMode::AlphaBlended;
            overrideMat.m_opacity = 0.75f;
            break;
        case GamePlay::DisplayType::SpriteTorchlightTransparent:
            overrideMat.m_transparencyType = TransparencyMode::Additive;
            overrideMat.m_opacity = 1.0f;
            break;
        //case GamePlay::DisplayType::SpriteTorchlight: this is opaque
        case GamePlay::DisplayType::SpriteScissorRect:
            overrideMat.m_transparencyType = TransparencyMode::Additive;
            overrideMat.m_opacity = 1.f;
            break;
        }
        overrideMat.m_texture = texture;
        overrideMat.m_textureSlot = textureSlot;
        overrideMat.ComputeHash();

        return inst;
    }

    Texture* GetTextureForMapIcon(const RawGameObject& rawGO)
    {
        GamePlay::PropertyAccessorBase props(rawGO);
        switch (props.GetCollisionType())
        {
        case GamePlay::CollisionType::Key:
            return AssetManager::Instance().GetMapIconTexture(MapIconType::Key);
        case GamePlay::CollisionType::Medicine:
            return AssetManager::Instance().GetMapIconTexture(MapIconType::Medicine);
        case GamePlay::CollisionType::Gold:
            return AssetManager::Instance().GetMapIconTexture(MapIconType::Gold);
        case GamePlay::CollisionType::Weapon:
        case GamePlay::CollisionType::Ammo:
            switch (GamePlay::PropertyAccessorWeapon(rawGO).GetWeaponType())
            {
            case GamePlay::WeaponType::Fireball:
                return AssetManager::Instance().GetMapIconTexture(MapIconType::Fireball);
            case GamePlay::WeaponType::Stone:
                return AssetManager::Instance().GetMapIconTexture(MapIconType::Stone);
            default:
                return nullptr;
            }
        default:
            return nullptr;
        }
    }

} // anonymous namespace


RenderableInstancePtr Level::AddStaticCellObject(int x, int y, byte rawVal, HeightLevel atHeight, bool correctInitialPlacement)
{
    auto staticGeom0 = m_levelInfo.GetModelAnim(2);
    auto staticGeom1 = m_levelInfo.GetModelAnim(3);
    Vector center, euler;

    int c = rawVal;
    const bool isFloor = atHeight == HeightLevel::Floor;
    if (c > 31)
    {
        int yawSteps = isFloor ? 1 : 3;
        float pitch = isFloor ? 0.f : Math::Pi;
        ModelAnim* modelAnim = (c >= 160) ? staticGeom1.get() : staticGeom0.get();
        if (modelAnim == nullptr)
            return nullptr;
        int frame = c & 31;
        int anim = frame / 5;
        int animFrame = frame % 5;
        if (anim < modelAnim->m_numAnims && animFrame < modelAnim->m_numFrames)
        {
            Model* tile = modelAnim->GetModel(anim, animFrame);
            float yaw = (float)(((c >> 5) & 3) + yawSteps) * Math::Pi * 0.5f;
            euler.Set(yaw, pitch, 0);
            Vector3 p = m_cellArray.GetCenterPos(x, y, atHeight);
            center.Set(p.x, p.y, p.z);
            Matrix4 matrix = Matrix4::CreateTransform(center, euler);
            std::shared_ptr<RenderableInstance> inst = std::make_shared<RenderableInstance>(tile, (uint32_t)GamePlay::VisibilityFilter::Walls);
            m_levelInfo.AssignLevelTextures(*inst);
            inst->ComputeTransparencyMask();
            inst->SetTransform(matrix);
            inst->UpdateGlobalBounds();
            // Fresh polygonal floor decals need depth-buffer clearance; saved scenes keep their existing placement.
            if (correctInitialPlacement && isFloor && !m_isSaveGame && !m_cellArray.GetCell(x, y).IsWaterSurface() &&
                inst->GetGlobalBounds().IsValid())
            {
                const auto& box = inst->GetGlobalBounds().m_box;
                if (inst->HasWaterMaterials() ||
                    (box.m_max.GetY() - box.m_min.GetY() <= 2.f * DespToFloat &&
                        std::abs(center.GetY() - GetHeightAtPos(center)) <= 8.f * DespToFloat))
                {
                    center.Set(center.GetX(), GetHeightAtPos(center) + 2.f * DespToFloat, center.GetZ());
                    inst->SetTranslation(center);
                    inst->UpdateGlobalBounds();
                }
            }
            // no scene yet, but we want to set the tile assignment manually anyway:
            BoundingVolume& vol = inst->ModifyGlobalBounds();
            vol.m_tileRange.m_minX = vol.m_tileRange.m_maxX = (int8_t)x;
			vol.m_tileRange.m_minZ = vol.m_tileRange.m_maxZ = (int8_t)y;
            vol.m_visibilityIsland = m_cellArray.GetCell(x, y).m_visibilityIsland;
            m_wallGeometry.push_back(inst);
            return inst;
        }
        else
        {
            throw std::runtime_error("Static floor/ceiling tile references an unavailable model frame.");
        }
    }
    return nullptr;
}


float SkyVerticalScale = 0.86f;
float SkyVerticalOffset = -0.06f; // move full skybox slightly below horizon
float SkyRotationOffset = -0.48f; // angle in rad

void Level::CreateSkyCylinderMesh(std::vector<DrawCall>& drawcalls, ModelLoadingContext& context, ModelConversionContext& conversion, Texture* skyTexture, float repetitionsU, float relHeight, const Color4& col, TransparencyMode transparency)
{
	auto& indices(conversion.m_sortedIndices);
	auto& positions(context.m_vertexPos);
	auto& vertices(conversion.m_vertices);
    const int firstIndex = (int)indices.size();
    const int firstVertex = (int)vertices.size();

	float r = 1.0f;
    float height = r * relHeight;

	Vector pos0(0, 0, 0);
	Vector euler(0, 0, 0);

	const int subDiv = 36;
	const float scaleAngle = (float)(2.0 * Math::Pi / subDiv);
	const float scaleU = repetitionsU / (float)subDiv; // sky fits 4 times
	float bottomV = 1.f - (0.5f / (float)skyTexture->m_height); // border clamp

	for (int yaw = 0; yaw <= subDiv; yaw++)
	{
		float a0 = (float)yaw * scaleAngle + SkyRotationOffset;
		float u0 = (float)yaw * scaleU;

		auto AddVertex = [&](float a, float h, const Vector2& tc)
		{
			uint16_t idx = (uint16_t)positions.size();
			positions.emplace_back() = { cosf(a) * r, h * SkyVerticalScale + SkyVerticalOffset, sinf(a) * r, 1.0f };
			vertices.emplace_back().SetTextured(idx, col, tc);
		};

		// top:
		AddVertex(a0, height, Vector2{ u0, 0 });
		// bottom:
		AddVertex(a0, 0, Vector2{ u0, bottomV });

		if (yaw > 0)
		{
			uint16_t vertexStart = (uint16_t)(firstVertex + (yaw - 1) * 2);
			indices.push_back(vertexStart + 2);
			indices.push_back(vertexStart + 3);
			indices.push_back(vertexStart + 0);
			indices.push_back(vertexStart + 3);
			indices.push_back(vertexStart + 1);
			indices.push_back(vertexStart + 0);
		}
	}

	DrawCall& dc = drawcalls.emplace_back();
	dc.m_material.m_texture = skyTexture;
	dc.m_material.m_applyLighting = false;
	dc.m_material.m_textureSlot = -1;
	dc.m_material.m_useAlphaTest = false;
	dc.m_material.m_transparencyType = transparency;
	dc.m_material.m_opacity = 1.f;
	dc.m_material.ComputeHash();

	dc.m_firstIndex = firstIndex;
	dc.m_primitiveCount = ((int)indices.size() - dc.m_firstIndex) / 3;
}

void Level::AddCelestialSprite(ModelLoadingContext& context, ModelConversionContext& conversion, float angle, float h, float width, const Color4& col)
{
	auto& indices(conversion.m_sortedIndices);
	auto& positions(context.m_vertexPos);
	auto& vertices(conversion.m_vertices);

	float r = 1.0f;
	uint16_t p = (uint16_t)positions.size();
	int vertexStart = (int)vertices.size();
	positions.emplace_back() = { cosf(angle) * r, h + SkyVerticalOffset, sinf(angle) * r, 1.0f };
	vertices.emplace_back().SetSprite(p, col, Vector2{ 0, 1 }, width, false); // left/bottom
	vertices.emplace_back().SetSprite(p, col, Vector2{ 1, 1 }, width, false); // right/bottom
	vertices.emplace_back().SetSprite(p, col, Vector2{ 1, 0 }, width, false);// right/top
	vertices.emplace_back().SetSprite(p, col, Vector2{ 0, 0 }, width, false);// left/top
	conversion.AddToBounds(positions[p], 0.f);

	indices.push_back((uint16_t)(vertexStart + 0));
	indices.push_back((uint16_t)(vertexStart + 1));
	indices.push_back((uint16_t)(vertexStart + 2));
	indices.push_back((uint16_t)(vertexStart + 0));
	indices.push_back((uint16_t)(vertexStart + 2));
	indices.push_back((uint16_t)(vertexStart + 3));

}



void Level::CreateStars(std::vector<DrawCall>& drawcalls, ModelLoadingContext& context, ModelConversionContext& conversion, int count)
{
	auto& indices(conversion.m_sortedIndices);
	auto& positions(context.m_vertexPos);
	auto& vertices(conversion.m_vertices);
	const int firstIndex = (int)indices.size();
	const int firstVertex = (int)vertices.size();


	float r = 1.0f;
	for (int i = 0; i < count; i++)
    {
        float a = Math::GetRandF(2.0 * Math::Pi);
        float h = (0.3f + Math::GetRandF(0.7f)) * r; // put them higher up in the sky so they don't intersect with the skyline billboard
        float width = 120.f * DespSpriteToFloat * (0.2f + Math::GetRandF(1.f));
        width *= DespRelativeInvScaling; // Turns out this must be independent of global scaling
        uint8_t lum = (int)(Math::GetRandF(155.9f) + 100.f);
		Color4 col = { lum, lum, lum, 0 };
        AddCelestialSprite(context, conversion, a, h, width, col);
    }

    Texture* star = AssetManager::Instance().GetCoronaTexture();
	DrawCall& dc = drawcalls.emplace_back();
	dc.m_material.m_texture = star;
	dc.m_material.m_applyLighting = false;
	dc.m_material.m_textureSlot = -1;
	dc.m_material.m_useAlphaTest = false;
	dc.m_material.m_transparencyType = TransparencyMode::SkyAdditive;
	dc.m_material.m_opacity = 1.f;
	dc.m_material.ComputeHash();
	dc.m_firstIndex = firstIndex;
	dc.m_primitiveCount = ((int)indices.size() - dc.m_firstIndex) / 3;
}

void Level::CreateSkyGeometry()
{
    if (m_levelInfo.m_skyTexture == nullptr || m_skyRenderable != nullptr)
        return;

    m_skyRenderable = std::make_shared<Renderable>();
    ModelLoadingContext context;
    ModelConversionContext conversion(context);

    // sky backdrop:
    Color4 skyTint = GetSkyTintColor(m_levelInfo.m_raw.m_data.skyTint);
    const float baseRepetitions = 4.f;
    Texture* skyTexture = m_levelInfo.m_skyTexture.get();
    CreateSkyCylinderMesh(m_skyRenderable->m_drawCalls, context, conversion, skyTexture, baseRepetitions, 1.f, skyTint, TransparencyMode::SkyOpaque);

    // Stars
    if (m_levelInfo.m_raw.m_data.amoutOfStars > 0)
    { 
        int count = (int)m_levelInfo.m_raw.m_data.amoutOfStars * 20;
        CreateStars(m_skyRenderable->m_drawCalls, context, conversion, count);
    }
    
    // skyline:
    if (Texture* skyline = m_levelInfo.m_skylineTexture.get())
    {
        float repeatU = floorf(baseRepetitions * (float)skyTexture->m_width / (float)skyline->m_width);
        float relHeight = baseRepetitions / repeatU; // (float)skyline->m_height / (float)skyTexture->m_height;
		CreateSkyCylinderMesh(m_skyRenderable->m_drawCalls, context, conversion, skyline, repeatU, relHeight, skyTint, TransparencyMode::SkyOpaque);
    }
    
    m_skyRenderable->m_runtime = Renderer::Instance().CreateModelBuffers(conversion);
}

void Level::CreateLightmap()
{
    if (m_lightmap != nullptr)
    {
        return;
    }

    m_lightmap = std::make_shared<Texture>();
    m_lightmap->m_width = m_cellArray.m_sizeX;
    m_lightmap->m_height = m_cellArray.m_sizeZ;

    TextureLoadingContext thisData;
    thisData.m_rgbaData.resize(m_cellArray.m_sizeX * m_cellArray.m_sizeZ);
    Color4* dest = thisData.m_rgbaData.data();

    for (int y = 0; y < m_cellArray.m_sizeZ; y++)
        for (int x = 0; x < m_cellArray.m_sizeX; x++, dest++)
        {
            const GridCell& cell(m_cellArray.GetCell(x, y));
            *dest = cell.m_brightness;
        }


    // scale ofs value:
    float d = 100.0f * DespToFloat;
    float w = (float)m_lightmap->m_width;
    float h = (float)m_lightmap->m_height;
    float xofs = (float)tileCenterX / w;
    float yofs = (float)tileCenterY / h;

    m_lightmap->m_atlasData = { 1.0f / (d * w),  1.0f / (d * h), xofs, yofs };

    Renderer::Instance().CreateTexture(*m_lightmap, thisData);
    m_lightmap->m_runtimeHighres = m_lightmap->m_runtime;
}



void Level::CreateWallGeometry()
{
    // geometry already created?
    if (m_wallGeometryCreated)
    {
        return;
    }

    m_wallGeometryCreated = true;
    bool addWalls = true;
    bool addFloor = true;
    bool addCeiling = true;
    bool addWater = true;

    std::unique_ptr<IslandWallConversion> perIslandConversion[256];
    int maxIslands = 0;
    std::vector<bool> mutableTiles(m_cellArray.m_cell.size(), false);
    for (const auto& object : m_rawGameObject)
    {
        const GamePlay::PropertyAccessorBase props(object);
        if (!props.Isvalid() || props.GetCollisionType() != GamePlay::CollisionType::WallDestruction) { continue; }
        // Same one-based DOS tile conversion as GameCore's wall-destruction path.
        const int column = object.m_rawShorts[6] / 100 - 1;
        const int row = 40 - object.m_rawShorts[8] / 100;
        if (!m_cellArray.IsInside(column, row))
        {
            throw std::runtime_error("Wall-destruction object is outside level geometry.");
        }
        mutableTiles[row * m_cellArray.m_sizeX + column] = true;
    }
    std::vector<int> mutableFloorIndices(m_cellArray.m_cell.size(), -1);
    for (const auto& object : m_rawGameObject)
    {
        const GamePlay::PropertyAccessorBase props(object);
        if (!props.Isvalid() || props.GetBehaviorType() != GamePlay::BehaviorType::ExtinguishLava) { continue; }
        int x = object.m_rawShorts[6];
        int z = object.m_rawShorts[8];
        const int dx = (object.m_rawShorts[9] > 0) - (object.m_rawShorts[9] < 0);
        const int dz = (object.m_rawShorts[11] > 0) - (object.m_rawShorts[11] < 0);
        for (int remaining = object.m_rawShorts[17]; remaining > 0; --remaining, x += dx * 100, z += dz * 100)
        {
            const int column = x / 100 - 1;
            const int row = 40 - z / 100;
            if (!m_cellArray.IsInside(column, row))
            {
                throw std::runtime_error("River flooding path is outside level geometry.");
            }
            const auto index = row * m_cellArray.m_sizeX + column;
            const byte original = m_cellArray.GetCell(column, row).m_floor;
            const byte replacement = (original & 0xe0) | (object.m_rawBytes[52] & 31);
            if (mutableFloorIndices[index] >= 0)
            {
                if (m_mutableFloors[mutableFloorIndices[index]].replacementFloor != replacement)
                {
                    throw std::runtime_error("Overlapping river flooding paths require conflicting floor variants.");
                }
            }
            else if (original != replacement)
            {
                mutableFloorIndices[index] = static_cast<int>(m_mutableFloors.size());
                m_mutableFloors.push_back({ column, row, original, replacement, nullptr, nullptr });
            }
        }
    }

    for (int y = 0; y < m_cellArray.m_sizeZ; y++)
    {
        for (int x = 0; x < m_cellArray.m_sizeX; x++)
        {
            int i = y * m_cellArray.m_sizeX + x;
            const GridCell& cell(m_cellArray.m_cell[i]);
            if (perIslandConversion[cell.m_visibilityIsland] == nullptr)
            {
                perIslandConversion[cell.m_visibilityIsland] = std::make_unique<IslandWallConversion>(this);
                maxIslands = __max((int)cell.m_visibilityIsland, maxIslands);
            }
			IslandWallConversion& islandWalls = *perIslandConversion[cell.m_visibilityIsland].get();
			std::vector<uint16_t>& indices(islandWalls.m_conversion.m_indices);

            if (addWalls)
            {
                int firstIndex = (int)indices.size();

                bool flippedFace = (cell.m_wallTextures & 16) > 0;
                int texIndex = (cell.m_wallTextures & 15);
                int wallIndex = cell.m_wallTextures >> 5;
                bool flippedU = false;

                if (texIndex > 0)
                {
                    Color4 col = {255, 255, 255, 0};
                    switch (wallIndex)
                    {
                    case 0:
                        AddWall(islandWalls.m_vertexCache, islandWalls.m_conversion.m_vertices, indices, x, y + 1, x, y, flippedFace, col, true); // UV correct
                        break;
                    case 1:
                        AddWall(islandWalls.m_vertexCache, islandWalls.m_conversion.m_vertices, indices, x, y, x + 1, y, flippedFace, col, true);// UV correct
                        break;
                    case 2:
                        AddWall(islandWalls.m_vertexCache, islandWalls.m_conversion.m_vertices, indices, x + 1, y, x + 1, y + 1, flippedFace, col, true);// UV correct
                        break;
                    case 3:
                        AddWall(islandWalls.m_vertexCache, islandWalls.m_conversion.m_vertices, indices, x + 1, y + 1, x, y + 1, flippedFace, col, true);// UV correct
                        break;
                    case 4:
                        AddWall(islandWalls.m_vertexCache, islandWalls.m_conversion.m_vertices, indices, x, y, x + 1, y, false, col, false);
                        AddWall(islandWalls.m_vertexCache, islandWalls.m_conversion.m_vertices, indices, x + 1, y + 1, x, y + 1, false, col, false);
                        break;
                    case 5:
                        AddWall(islandWalls.m_vertexCache, islandWalls.m_conversion.m_vertices, indices, x, y + 1, x, y, false, col, false);
                        AddWall(islandWalls.m_vertexCache, islandWalls.m_conversion.m_vertices, indices, x + 1, y, x + 1, y + 1, false, col, false);
                        break;
                    case 6:
                        AddWall(islandWalls.m_vertexCache, islandWalls.m_conversion.m_vertices, indices, x + 1, y, x, y, false, col, false);
                        AddWall(islandWalls.m_vertexCache, islandWalls.m_conversion.m_vertices, indices, x, y + 1, x + 1, y + 1, false, col, false);
                        AddWall(islandWalls.m_vertexCache, islandWalls.m_conversion.m_vertices, indices, x, y, x, y + 1, false, col, false);
                        AddWall(islandWalls.m_vertexCache, islandWalls.m_conversion.m_vertices, indices, x + 1, y + 1, x + 1, y, false, col, false);
                        break;
                    case 7:
                        // marks player start position
                        {
                            Vector3 campos = m_cellArray.GetCenterPos(x, y, HeightLevel::Floor);
                            campos.y += 50.f * DespToFloat;
                            m_playerStartPos.Set(campos.x, campos.y, campos.z); 
                            break;
                        }
                    }
                }
                DrawCall dc;
                //dc.shaderType = ShaderType.Normal;
                dc.m_material.m_transparencyType = TransparencyMode::Opaque;
                dc.m_material.m_opacity = 1.f;
                dc.m_material.m_textureSlot = (char)texIndex;
                dc.m_material.m_textureSlotSource = TextureSlotSource::Level;
                //dc.m_material.m_textureRepeat = true;
                dc.m_firstIndex = firstIndex;
                dc.m_primitiveCount = ((int)indices.size() - firstIndex) / 3;
                dc.m_material.m_useAlphaTest = false;
                dc.m_material.m_applyLighting = true;
                dc.m_material.ComputeHash();

                if (dc.m_primitiveCount > 0)
                {
                    dc.m_material.m_texture = m_levelInfo.GetLevelTexture(texIndex).get(); // prevent it from using the torchlight tex
                    //dc.m_material.m_texture = AssetManager::Instance().GetPlainWhiteTexture(); // REMOVE
                    if (mutableTiles[i]) { islandWalls.m_mutableWalls.push_back({ x, y, dc }); }
                    else { islandWalls.m_dcs.emplace_back(dc); }
                }

            } // add walls

            if (addFloor)
            {
                const int floorIndex = mutableFloorIndices[i];
                const auto addVariant = [&](byte c, bool replacement)
                {
                    if (c > 0 && c < 32 && c != 16)
                    {
                        const int firstIndex = static_cast<int>(indices.size());
                        const int texIndex = c & 15;
                        AddFloorOrCeiling(HeightLevel::Floor, islandWalls.m_vertexCache,
                            islandWalls.m_conversion.m_vertices, indices, x, y, false);
                        DrawCall dc;
                        dc.m_material.m_transparencyType = TransparencyMode::Opaque;
                        dc.m_material.m_opacity = 1.f;
                        dc.m_material.m_textureSlot = static_cast<char>(texIndex);
                        dc.m_material.m_textureSlotSource = TextureSlotSource::Level;
                        dc.m_firstIndex = firstIndex;
                        dc.m_primitiveCount = (static_cast<int>(indices.size()) - firstIndex) / 3;
                        dc.m_material.m_useAlphaTest = false;
                        dc.m_material.m_applyLighting = true;
                        dc.m_material.m_texture = m_levelInfo.GetLevelTexture(texIndex).get();
                        dc.m_material.ComputeHash();
                        if (dc.m_primitiveCount > 0)
                        {
                            if (floorIndex < 0) { islandWalls.m_dcs.push_back(dc); }
                            else
                            {
                                islandWalls.m_mutableFloors.push_back(
                                    { static_cast<std::size_t>(floorIndex), replacement, dc });
                            }
                        }
                    }
                    else
                    {
                        // Controlled river variants retain one shared terrain anchor, not puddle-decal placement.
                        auto inst = AddStaticCellObject(x, y, c, HeightLevel::Floor, floorIndex < 0);
                        if (floorIndex >= 0)
                        {
                            if (c > 31 && !inst)
                            {
                                throw std::runtime_error("River flooding requires a valid floor model variant.");
                            }
                            auto& floor = m_mutableFloors[floorIndex];
                            (replacement ? floor.replacement : floor.original) = inst;
                            if (inst) { inst->SetVisible(!replacement); }
                        }
                    }
                };
                addVariant(cell.m_floor, false);
                if (floorIndex >= 0)
                {
                    addVariant(m_mutableFloors[floorIndex].replacementFloor, true);
                }
            } // add floor
            
            if (addCeiling)
            {
                int firstIndex = (int)indices.size();
                int c = cell.m_ceiling;
                if (c > 0 && c < 32 && c != 16)
                {
                    int texIndex = c & 15;
                    AddFloorOrCeiling(HeightLevel::Ceiling, islandWalls.m_vertexCache, islandWalls.m_conversion.m_vertices, indices, x, y, false);
                    DrawCall dc;
                    dc.m_material.m_transparencyType = TransparencyMode::Opaque;
                    dc.m_material.m_opacity = 1.f;
                    dc.m_material.m_textureSlot = (char)(texIndex);
                    dc.m_material.m_textureSlotSource = TextureSlotSource::Level;
                    //dc.m_material.m_textureRepeat = true;
                    dc.m_firstIndex = firstIndex;
                    dc.m_primitiveCount = ((int)indices.size() - firstIndex) / 3;
                    dc.m_material.m_useAlphaTest = false;
                    dc.m_material.m_applyLighting = true;
                    dc.m_material.ComputeHash();
                    if (dc.m_primitiveCount > 0)
                    {
                        dc.m_material.m_texture = m_levelInfo.GetLevelTexture(texIndex).get(); // prevent it from using the torchlight tex
                        islandWalls.m_dcs.emplace_back(dc);
                    }
                }
                else
                {
                    AddStaticCellObject(x, y, cell.m_ceiling, HeightLevel::Ceiling);
                }
            } // add ceiling
            if (addWater)
            {
                if (cell.IsWaterSurface())
                {
                    int slotIndex = (int)cell.m_waterSlot - (int)WaterSlotIndex::WaterSlot1; // 0..3
                    assert(slotIndex >= 0 && slotIndex < 4);
                    const LevelInfo::WaterSlot& waterSlot(m_levelInfo.m_raw.m_data.water[slotIndex]);
                    if (waterSlot.height != 0 && waterSlot.reflectionType != WaterReflectionType::Disabled)
                    {
                        HeightLevel level = (HeightLevel)(slotIndex + (int)HeightLevel::WaterLevel1); // translate into height level
                        int firstIndex = (int)indices.size();
                        Color4 waterCol = { 255,255,255,0 };
                        DrawCallMaterial::GetWaterTintColor(waterSlot.reflectionType, waterCol);
                        AddFloorOrCeiling(level, islandWalls.m_vertexCache, islandWalls.m_conversion.m_vertices, indices, x, y, false, waterCol);
                        DrawCall dc;
                        dc.m_material.m_transparencyType = TransparencyMode::Water;
                        dc.m_material.m_reflectionType = waterSlot.reflectionType;
                        dc.m_material.m_mirrorHeight = m_cellArray.m_waterHeights[slotIndex];
                        dc.m_material.m_opacity = 1.f;
                        dc.m_material.m_textureSlot = -1;
                        //dc.m_material.m_textureRepeat = true;
                        dc.m_firstIndex = firstIndex;
                        dc.m_primitiveCount = ((int)indices.size() - firstIndex) / 3;
                        dc.m_material.m_useAlphaTest = false;
                        dc.m_material.m_applyLighting = false;
                        dc.m_material.ComputeHash();
                        if (dc.m_primitiveCount > 0)
                        {
                            dc.m_material.m_texture = nullptr;
                            islandWalls.m_dcs.emplace_back(dc);
                        }
                    }
                }
            } // add mirror
        }
    }

   
    // now convert
    for (int islandId = 0; islandId <= maxIslands; islandId++)
    {
		if (perIslandConversion[islandId] == nullptr ||
            (perIslandConversion[islandId]->m_dcs.empty() && perIslandConversion[islandId]->m_mutableWalls.empty() &&
                perIslandConversion[islandId]->m_mutableFloors.empty()))
		{
            continue;
		}
		IslandWallConversion& islandWalls = *perIslandConversion[islandId].get();
		std::vector<uint16_t>& indices(islandWalls.m_conversion.m_indices);

        // turn everything into optimized mesh
        std::vector<DrawCall> newdcs;
        GeometryUtils::OptimizeDrawCalls(newdcs, islandWalls.m_conversion.m_sortedIndices, indices, islandWalls.m_dcs);
        const auto staticDrawCallCount = newdcs.size();
        for (const auto& wall : islandWalls.m_mutableWalls)
        {
            auto dc = wall.drawCall;
            dc.m_firstIndex = static_cast<int>(islandWalls.m_conversion.m_sortedIndices.size());
            const auto first = indices.begin() + wall.drawCall.m_firstIndex;
            islandWalls.m_conversion.m_sortedIndices.insert(islandWalls.m_conversion.m_sortedIndices.end(),
                first, first + wall.drawCall.m_primitiveCount * 3);
            newdcs.push_back(dc);
        }
        const auto mutableFloorStart = newdcs.size();
        for (const auto& floor : islandWalls.m_mutableFloors)
        {
            auto dc = floor.drawCall;
            dc.m_firstIndex = static_cast<int>(islandWalls.m_conversion.m_sortedIndices.size());
            const auto first = indices.begin() + floor.drawCall.m_firstIndex;
            islandWalls.m_conversion.m_sortedIndices.insert(islandWalls.m_conversion.m_sortedIndices.end(),
                first, first + floor.drawCall.m_primitiveCount * 3);
            newdcs.push_back(dc);
        }
        islandWalls.m_conversion.GeneratePerTriangleNormals(islandWalls.m_conversion.m_sortedIndices.data(), (int)islandWalls.m_conversion.m_sortedIndices.size(), 0);
        std::vector<DrawCall> smoothSurfaces;
        for (const auto& dc : newdcs)
        {
            const int end = dc.m_firstIndex + dc.m_primitiveCount * 3;
            for (int i = dc.m_firstIndex; i < end; i += 3)
            {
                const auto vertex = islandWalls.m_conversion.m_sortedIndices[i];
                // Group zero is generated wall geometry: retain its face normals at every angle.
                if (islandWalls.m_conversion.m_vertices[vertex].NormalGroup == 0) { continue; }
                if (smoothSurfaces.empty() ||
                    smoothSurfaces.back().m_firstIndex + smoothSurfaces.back().m_primitiveCount * 3 != i)
                {
                    DrawCall surface;
                    surface.m_firstIndex = i;
                    smoothSurfaces.push_back(surface);
                }
                ++smoothSurfaces.back().m_primitiveCount;
            }
        }
        GeometryUtils::SmoothNormals(islandWalls.m_conversion, smoothSurfaces, 0,
            static_cast<int>(islandWalls.m_context.m_vertexPos.size()), {NormalGenerationMode::Smooth});

        m_wallGeometryRuntime = Renderer::Instance().CreateModelBuffers(islandWalls.m_conversion);

        std::shared_ptr<PhysicsShape> wallPhysics;
        {
            TriangularShapeDesc triMesh;
            islandWalls.m_conversion.GeneratePhysicsGeometry(newdcs, triMesh); // spans all draw calls
            wallPhysics = PhysicsModule::Instance().CreateTriMesh(triMesh);
        }

        // one renderable instance per draw call for better visibility clipping:
        for (std::size_t drawCallIndex = 0; drawCallIndex < newdcs.size(); ++drawCallIndex)
        {
            const auto& dc = newdcs[drawCallIndex];
            RenderablePtr r = std::make_shared<Renderable>();
            r->m_physicsShape = wallPhysics;
            wallPhysics = nullptr; // assign physics only to the first instance as it holds all draw calls
            r->m_drawCalls.push_back(dc);
            r->m_runtime = m_wallGeometryRuntime;
            r->m_vertexOfs = 0;
            GeometryUtils::ComputeBoundingVolume(r->m_localBounds, r->m_drawCalls, islandWalls.m_conversion.m_vertices, islandWalls.m_conversion.m_input.m_vertexPos, islandWalls.m_conversion.m_sortedIndices);
            m_wallRenderables.push_back(r);
            std::shared_ptr<RenderableInstance> inst = std::make_shared<RenderableInstance>(r.get(), (uint32_t)GamePlay::VisibilityFilter::Walls);
            m_levelInfo.AssignLevelTextures(*inst);
            // transform stays at (0,0,0)
            // 
            // pre-bake tile assignment
            inst->UpdateGlobalBounds();
            BoundingVolume& bounds = inst->ModifyGlobalBounds();
            m_cellArray.GetTileAssignment(inst->GetTransform().GetTranslation(), bounds);
            bounds.m_visibilityIsland = islandId;
            m_wallGeometry.push_back(inst);
            if (drawCallIndex >= mutableFloorStart)
            {
                const auto& variant = islandWalls.m_mutableFloors[drawCallIndex - mutableFloorStart];
                auto& floor = m_mutableFloors[variant.floorIndex];
                (variant.replacement ? floor.replacement : floor.original) = inst;
                inst->SetVisible(!variant.replacement);
            }
            else if (drawCallIndex >= staticDrawCallCount)
            {
                const auto& wall = islandWalls.m_mutableWalls[drawCallIndex - staticDrawCallCount];
                m_mutableWalls.push_back({ wall.column, wall.row,
                    m_cellArray.GetCell(wall.column, wall.row).m_wallTextures, inst });
            }

            /*if (false) // visibility bounding boxes for wall geometry
            {
                float radius = r->m_boundingRadius;
                std::shared_ptr<Renderable> cube = GeometryUtils::CreateCubeMesh(radius, radius, radius, { 255,255,0,255 }, 3.f);
                m_wallRenderables.push_back(cube);
                RenderableInstancePtr boxInst = std::make_shared<RenderableInstance>(cube.get(),  (uint32_t)GamePlay::VisibilityFilter::Walls);
                boxInst->m_transform.Create(Vector(0, 0, 0), Vector(0, 0, 0)); // unrotated, unscaled box
                m_wallGeometry.push_back(boxInst);
            }
            */
        }
    }
}

std::size_t Level::SynchronizeWallTiles(
    const byte* tiles, std::size_t width, std::size_t height, std::size_t rowStride)
{
    if (!tiles || width != static_cast<std::size_t>(m_cellArray.m_sizeX) ||
        height != static_cast<std::size_t>(m_cellArray.m_sizeZ) || rowStride < width)
    {
        throw std::invalid_argument("Wall synchronization requires matching level dimensions and tile storage.");
    }
    std::size_t changed = 0;
    for (const auto& wall : m_mutableWalls)
    {
        const auto value = tiles[wall.row * rowStride + wall.column];
        if (value != 0 && value != wall.originalWall)
        {
            throw std::runtime_error("Mutable wall synchronization supports removal or restoration, not replacement.");
        }
        const bool visible = value != 0;
        if (wall.instance->GetVisible() != visible)
        {
            wall.instance->SetVisible(visible);
            ++changed;
        }
        m_cellArray.GetCell(wall.column, wall.row).m_wallTextures = value;
    }
    return changed;
}

std::size_t Level::SynchronizeFloorTiles(
    const byte* tiles, std::size_t width, std::size_t height, std::size_t rowStride)
{
    if (!tiles || width != static_cast<std::size_t>(m_cellArray.m_sizeX) ||
        height != static_cast<std::size_t>(m_cellArray.m_sizeZ) || rowStride < width)
    {
        throw std::invalid_argument("Floor synchronization requires matching level dimensions and tile storage.");
    }
    std::size_t changed = 0;
    for (const auto& floor : m_mutableFloors)
    {
        const byte value = tiles[floor.row * rowStride + floor.column];
        if (value != floor.originalFloor && value != floor.replacementFloor)
        {
            throw std::runtime_error("River floor synchronization requires a prepared tile variant.");
        }
        auto& cell = m_cellArray.GetCell(floor.column, floor.row);
        if (cell.m_floor == value) { continue; }
        if (floor.original) { floor.original->SetVisible(value == floor.originalFloor); }
        if (floor.replacement) { floor.replacement->SetVisible(value == floor.replacementFloor); }
        cell.m_floor = value;
        ++changed;
    }
    return changed;
}


bool Level::CreateObjectRenderInstance(Scene& scene, std::shared_ptr<LevelGameObject> go, RawGameObject& rawGO) const
{
    AssetManager& assets = AssetManager::Instance();
	PhysicsWorld* physicsWorld = scene.GetPhysicsWorld();
	go->m_rawGO = rawGO;
    GamePlay::PropertyAccessorBase prop(rawGO);
    GamePlay::BehaviorType behavior = prop.GetBehaviorType();
    GamePlay::DisplayType display = prop.GetDisplayType();
    int modelId = prop.GetModelId();
    Color4 tintColor = { 255,255,255,255 };
    float fireIntensity = 0.f;

    // model & animation:
    if (GamePlay::IsVectorDisplayType(display, tintColor, fireIntensity))
    {
        auto anim3d = m_levelInfo.GetModelAnim(modelId - 30);
        if (anim3d == nullptr)
        {
            return false;
        }
        int animNr = prop.GetModelAnim();
        int frameNr = prop.GetModelFrame();
        Model* model = anim3d->GetModelSafe(animNr, frameNr);
        if (model == nullptr || !model->IsValid())
        {
            return false;
        }

        assert(model->m_drawCalls.size() < 16);
        RenderableInstancePtr inst = std::make_shared<RenderableInstance>(model, (uint32_t)GamePlay::VisibilityFilter::GameObjects);
        go->m_renderInst = inst;
        inst->m_uniformScale = 1.f; // not scalable  rawGO.GetScale();
        inst->SetTransform(prop.GetTransform());
        inst->m_tintColor = tintColor;
        inst->m_fireIntensity = fireIntensity;
        m_levelInfo.AssignLevelTextures(*go->m_renderInst);

		int animNr2 = prop.GetModelAnim2();
		int frameNr2 = prop.GetModelFrame2();
        Model* model2 = anim3d->GetModelSafe(animNr2, frameNr2);
        if (model2 != nullptr)
        {
            inst->m_blendWeight = prop.GetAnimBlendWeight();
        }
        inst->SetRenderable(model, model2);

        // register renderable
        scene.AddRenderableInstance(go->m_renderInst);
        return true;
    }
    // alternatively, sprite model?
    else if (GamePlay::IsSpriteDisplayType(display))
    {
        char textureSlot = -1;
        Texture* spriteTex = nullptr;
        float scale = prop.GetScale();
        bool flipped = prop.FlipSpriteU();

        // Native sprite bank 6 is populated by TrackFireAnimation, even for ordinary sprite display modes.
        if (modelId == 6)
        {
            textureSlot = prop.GetModelAnim() + 13; // it already subtracts 1, we need to add 12 to rawByte[1]
        }
        else
        {
            auto texAnim = m_levelInfo.GetTextureAnim(modelId);
            if (texAnim == nullptr)
            {
                return false;
            }
            int anim = prop.GetModelAnim();
            int frame = prop.GetModelFrame();
            if (anim < 0 || frame < 0)
                return false;

            const bool isDepthSprite = display == GamePlay::DisplayType::DepthSprite;
            spriteTex = isDepthSprite ? texAnim->GetFrameWithDepth(anim, frame) : texAnim->GetFrame(anim, frame);
        }
        RenderableInstancePtr inst = CreateSpriteInstance(display, spriteTex, textureSlot, scale, flipped, GamePlay::VisibilityFilter::GameObjects);
        go->m_renderInst = inst;
        inst->SetTransform(prop.GetScaledTransform());
		inst->m_materialOverride[0].SetOpacity(prop.GetSpriteTransparency());
        if (behavior == GamePlay::BehaviorType::Ghost && prop.GetSpriteTransparency() == 0.f)
        {
            inst->SetVisible(false);
        }

        // register renderable
        scene.AddRenderableInstance(go->m_renderInst);

        // create a physics pillar
        if (behavior == GamePlay::BehaviorType::Pillar && physicsWorld != nullptr && spriteTex != nullptr)
        {
            Vector pivot = prop.GetPosition();
            float y0 = pivot.GetY() + spriteTex->m_spriteData.m_topLeft.y * DespToFloat * scale;
            float y1 = pivot.GetY() + spriteTex->m_spriteData.m_bottomRight.y * DespToFloat * scale;
            pivot.Set(pivot.GetX(), (y0 + y1) * 0.5f, pivot.GetZ());
            float height = fabsf(y1 - y0);
            float radius = fabsf(spriteTex->m_spriteData.m_topLeft.x - spriteTex->m_spriteData.m_bottomRight.x) * 0.5f * DespToFloat * scale;
            Matrix4 capsuleTransform;
            Vector euler(0, 0, Math::Pi * 0.5f);
            capsuleTransform.Create(pivot, euler);
            physicsWorld->CreateStaticCylinderCollider(radius, height, capsuleTransform);
        }


        return true;
    }
    else
    {
        return false;
    }

    return false;
}


void Level::CorrectInitialObjectPlacement(RawGameObject& rawGO) const
{
    GamePlay::PropertyAccessorBase props(rawGO);
    if (m_isSaveGame || !props.Isvalid() || props.GetModelId() <= 30 || rawGO.m_rawBytes[4] == 20) { return; }
    const auto behavior = props.GetBehaviorType();
    const auto collision = props.GetCollisionType();
    const bool key = collision == GamePlay::CollisionType::Key && behavior == GamePlay::BehaviorType::None;
    const bool map = collision == GamePlay::CollisionType::PictureMap && behavior == GamePlay::BehaviorType::None;
    const bool puddle = collision == GamePlay::CollisionType::BurnableOilPuddle ||
        collision == GamePlay::CollisionType::ActivateSpiders || behavior == GamePlay::BehaviorType::BurnableOilPuddle;
    const bool staticDecoration = collision == GamePlay::CollisionType::None && behavior == GamePlay::BehaviorType::None;
    if (!key && !map && !puddle && behavior != GamePlay::BehaviorType::None) { return; }

    const auto position = props.GetPosition();
    const float floor = GetHeightAtPos(position);
    const auto placeOnFloor = [&](int clearance)
    {
        // Model bounds include oversized animation extents; ground placement uses only the object's origin.
        const auto height = std::lround(floor / DespToFloat) + clearance;
        if (height < std::numeric_limits<int16_t>::min() || height > std::numeric_limits<int16_t>::max())
        {
            throw std::out_of_range("Initial floor placement exceeds native coordinate range.");
        }
        rawGO.m_rawShorts[7] = static_cast<int16_t>(height);
    };
    if (key || puddle)
    {
        placeOnFloor(key ? 4 : 2);
        return;
    }

    auto instance = std::make_shared<RenderableInstance>();
    ApplyRenderProperties(instance, rawGO);
    if (!instance->IsValid()) { return; }
    // Mirror-receiver materials identify reflective puddles even when their native behavior/collision is absent.
    if (!map && instance->HasWaterMaterials())
    {
        placeOnFloor(2);
        return;
    }
    if (!instance->GetGlobalBounds().IsValid()) { return; }
    const auto& box = instance->GetGlobalBounds().m_box;
    const float margin = 2.f * DespToFloat;
    const auto move = [](int16_t& coordinate, float offset)
    {
        const float units = offset / DespToFloat;
        const int delta = static_cast<int>(units >= 0.f ? std::ceil(units) : std::floor(units));
        const int adjusted = coordinate + delta;
        if (adjusted < std::numeric_limits<int16_t>::min() || adjusted > std::numeric_limits<int16_t>::max())
        {
            throw std::out_of_range("Initial object clearance exceeds native coordinate range.");
        }
        coordinate = static_cast<int16_t>(adjusted);
    };

    // Flat, floor-level decoration includes nonflammable polygonal puddles; never lift water tiles or sprites.
    const bool floorOverlay = staticDecoration && box.m_max.GetY() - box.m_min.GetY() <= margin &&
        std::abs(position.GetY() - floor) <= 8.f * DespToFloat;
    if (floorOverlay)
    {
        placeOnFloor(2);
        return;
    }
    if (!map) { return; }

    bool foundWall = false, wallAlongX = false;
    float wallPlane = 0.f, nearest = std::numeric_limits<float>::max();
    int wallSide = 0;
    const auto considerWall = [&](bool alongX, float plane, float start, float end, int ownerSide)
    {
        const float min = alongX ? box.m_min.GetZ() : box.m_min.GetX();
        const float max = alongX ? box.m_max.GetZ() : box.m_max.GetX();
        if (max < start || min > end) { return; }
        const float signedDistance = (alongX ? position.GetX() : position.GetZ()) - plane;
        const float distance = std::abs(signedDistance);
        if (distance >= nearest || distance > 20.f * DespToFloat) { return; }
        foundWall = true;
        nearest = distance;
        wallAlongX = alongX;
        wallPlane = plane;
        wallSide = signedDistance < 0.f ? -1 : signedDistance > 0.f ? 1 : ownerSide;
    };
    for (int z = m_cellArray.GetCellY(box.m_min.GetZ() - margin); z <= m_cellArray.GetCellY(box.m_max.GetZ() + margin); ++z)
    {
        for (int x = m_cellArray.GetCellX(box.m_min.GetX() - margin); x <= m_cellArray.GetCellX(box.m_max.GetX() + margin); ++x)
        {
            const auto* cell = m_cellArray.GetCellOrNull(x, z);
            if (!cell || (cell->m_wallTextures & 15) == 0) { continue; }
            const int wall = cell->m_wallTextures >> 5;
            const float x0 = m_cellArray.GridPosX(x), x1 = m_cellArray.GridPosX(x + 1);
            const float z0 = m_cellArray.GridPosY(z), z1 = m_cellArray.GridPosY(z + 1);
            if (wall == 0 || wall == 5 || wall == 6) { considerWall(true, x0, z0, z1, 1); }
            if (wall == 2 || wall == 5 || wall == 6) { considerWall(true, x1, z0, z1, -1); }
            if (wall == 1 || wall == 4 || wall == 6) { considerWall(false, z0, x0, x1, 1); }
            if (wall == 3 || wall == 4 || wall == 6) { considerWall(false, z1, x0, x1, -1); }
        }
    }
    if (foundWall)
    {
        const float edge = wallAlongX ?
            (wallSide < 0 ? box.m_max.GetX() : box.m_min.GetX()) :
            (wallSide < 0 ? box.m_max.GetZ() : box.m_min.GetZ());
        const float offset = wallPlane + wallSide * margin - edge;
        if (offset * wallSide > 0.f) { move(rawGO.m_rawShorts[wallAlongX ? 6 : 8], offset); }
    }
}

void Level::ApplyRenderProperties(RenderableInstancePtr inst, RawGameObject& rawGO, Texture* spriteOverride) const
{
    inst->SetVisible(false);
	GamePlay::PropertyAccessorBase prop(rawGO);
	GamePlay::DisplayType display = prop.GetDisplayType();
	int modelId = prop.GetModelId();
    const Renderable* oldRenderable = inst->GetRenderable();
    inst->SetRenderable(nullptr, nullptr);
    bool assignLevelTextures = true;

	// model & animation:
    if (GamePlay::IsVectorDisplayType(display, inst->m_tintColor, inst->m_fireIntensity))
    {
        auto anim3d = m_levelInfo.GetModelAnim(modelId - 30);
        if (anim3d == nullptr)
        {
            return;
        }
		int animNr = prop.GetModelAnim();
		int frameNr = prop.GetModelFrame();

		Model* model = anim3d->GetModelSafe(animNr, frameNr);
		if (model == nullptr || !model->IsValid())
		{
			return;
		}

		int animNr2 = prop.GetModelAnim2();
		int frameNr2 = prop.GetModelFrame2();
        Model* model2 = anim3d->GetModelSafe(animNr2, frameNr2);
        if (model2 != nullptr)
        {
            inst->m_blendWeight = prop.GetAnimBlendWeight();
        }
        inst->SetRenderable(model, model2);

		assert(model->m_drawCalls.size() > 0 && model->m_drawCalls.size() < 16);
		inst->m_uniformScale = 1.f; // not scalable  rawGO.GetScale();
        auto transform = prop.GetTransform();
        if (prop.GetBehaviorType() == GamePlay::BehaviorType::ExtinguishLava)
        {
            const Vector pos = transform.GetTranslation();
            // Keep the morphing flood surface two DOS units above the floor to avoid riverbed z-fighting.
            transform.SetTranslation(Vector(pos.GetX(), m_cellArray.GetHeightAtPos(pos) + 2.f * DespToFloat, pos.GetZ()));
        }
        inst->SetTransformAndUpdate(transform);
        inst->SetVisible(true);
	}
    else if (GamePlay::IsSpriteDisplayType(display))
    {
        char textureSlot = -1;
        Texture* spriteTex = spriteOverride;
        float scale = prop.GetScale();
        bool flipped = prop.FlipSpriteU();

        if (spriteOverride == nullptr && modelId == 6)
        {
            textureSlot = prop.GetModelAnim() + 13; // it already subtracts 1, we need to add 12 to rawByte[1]
        }
        else if (spriteOverride == nullptr)
        {
            auto texAnim = m_levelInfo.GetTextureAnim(modelId);
            if (texAnim == nullptr)
            {
                return;
            }
            int anim = prop.GetModelAnim();
            int frame = prop.GetModelFrame();
            if (!texAnim->IsValidFrame(anim, frame))
            {
                return;
            }
            const bool isDepthSprite = display == GamePlay::DisplayType::DepthSprite;
            spriteTex = isDepthSprite ? texAnim->GetFrameWithDepth(anim, frame) : texAnim->GetFrame(anim, frame);
        }
        // TODO: This is super inefficient as it allocates a new sprite every frame
        RenderableInstancePtr inst2 = CreateSpriteInstance(display, spriteTex, textureSlot, scale, flipped, GamePlay::VisibilityFilter::GameObjects);
        inst->m_materialOverride = inst2->m_materialOverride;
        inst->SetRenderable(inst2->GetRenderable(), inst2->GetBlendTargetRenderable());
        inst->m_uniformScale = inst2->m_uniformScale;
        inst->SetTransformAndUpdate(prop.GetScaledTransform());
        assignLevelTextures = false;
        inst->SetVisible(true);
		inst->m_tintColor = inst2->m_tintColor;
		inst->m_fireIntensity = inst2->m_fireIntensity;
        inst->m_materialOverride[0].SetOpacity(prop.GetSpriteTransparency());
        if (prop.GetBehaviorType() == GamePlay::BehaviorType::Ghost && prop.GetSpriteTransparency() == 0.f)
        {
            inst->SetVisible(false);
        }

        // TODO: Sprites are still not visible!
    }

    if (inst->GetRenderable() == nullptr)
    {
		inst->SetVisible(false);
        return;
    }

    if (oldRenderable != inst->GetRenderable())
    {
        if (assignLevelTextures)
        { 
            m_levelInfo.AssignLevelTextures(*inst);
        }
    }
    if (!assignLevelTextures || oldRenderable != inst->GetRenderable())
    {
        inst->ComputeTransparencyMask();
    }
    // TODO: fire animations...

}

RenderableInstancePtr Level::CreatePlayerObject()
{
    // create a white sprite
    float scale = 25.f;
    AssetManager& assets = AssetManager::Instance();
    RenderableInstancePtr inst = CreateSpriteInstance(GamePlay::DisplayType::SpriteNormal, assets.GetPointTexture(), -1, scale, false, GamePlay::VisibilityFilter::GameObjects);
    return inst;
}


FUTURE(std::shared_ptr<PlayerLight>) Level::PopulateScene(Scene& scene, bool createGameObjects, bool addMapIcons,
    ObjectPresentationCallback objectPresentation)
{
    if (createGameObjects && !objectPresentation)
    {
        throw std::invalid_argument("Level object conversion requires a slot-owned presentation callback.");
    }
    AssetManager& assets = AssetManager::Instance();

    const bool addStaticGeometry = true;
    const bool addGameObjects = createGameObjects;
    const bool showDebugGeometry = false; // object bounding boxes
    const bool showDoorConnections = false;

    std::vector<DebugLine> debugGeometry;

    Color4 clearCol = assets.GetDefaultPalette().GetEntry(m_levelInfo.m_raw.m_data.clearColorIdx);
    Vector4 clearColF = clearCol.ToFloat();
 
    Color4 fogColor;
   
    float fogFallOff = 0.f;
    GetFogColorAndFalloff(m_levelInfo.m_raw.m_data.fogTableType, fogColor, fogFallOff);
	Vector4 fogColF = fogColor.ToFloat();
	float w1 = 0.6f;

    CreateSkyGeometry();
    if (m_skyRenderable != nullptr)
    {
        RenderableInstancePtr skyInst = std::make_shared<RenderableInstance>(m_skyRenderable.get(), (uint32_t)GamePlay::VisibilityFilter::Sky);
        scene.AddRenderableInstance(skyInst);

        // global sunlight
        std::shared_ptr<LightSource> sunLight = std::make_shared<LightSource>(&scene);
		float radius = 500.f * DespRelativeScaling;
        sunLight->SetPosition(Vector(radius*1.5f, radius, radius));
        sunLight->SetRadius(radius * 10);
        sunLight->SetColor(Color4{ 255,230,220 }, 0.3f);
        sunLight->SetCoronaSize(0); // important
        scene.AddLightSource(sunLight);

        fogFallOff = 0.06f; // outdoor should use large fog range
        w1 = 0.4f;
    }

	float w0 = 1.f - w1;
	clearColF = { fogColF.x * w1 + clearColF.x * w0, fogColF.y * w1 + clearColF.y * w0, fogColF.z * w1 + clearColF.z * w0, 1.f }; // better fit

    fogFallOff *= DespRelativeInvScaling;
    fogFallOff *= 0.75f; // Still, on VR it was too extreme, so scaling it down
	scene.SetFogParams(fogColF, fogFallOff);
	scene.SetClearColor(clearColF);

    // player light:
    auto playerLight = std::make_shared<PlayerLight>(&scene);
    scene.AddLightSource(playerLight);

    // lightmap:
    CreateLightmap();
    scene.SetLightmap(m_lightmap);
    scene.SetCollisionFieldTexture(m_collisionFieldTex);

	PhysicsWorld* physicsWorld = scene.GetPhysicsWorld();

    // create unique wall/floor/ceiling geometry
    if (addStaticGeometry)
    {
        CreateWallGeometry();
        for (auto inst : m_wallGeometry)
        {
            scene.AddRenderableInstance(inst);

            // also add physics
            if (physicsWorld != nullptr && inst->GetRenderable()->m_physicsShape != nullptr)
            {
                physicsWorld->CreateStaticCollider(inst->GetRenderable()->m_physicsShape, inst->GetTransform());
            }
        }

    }

    if (addGameObjects)
    {
        // turn raw game objects into renderable game objects
        for (const RawGameObject& original : m_rawGameObject)
        {
            auto rawGO = original;
            CorrectInitialObjectPlacement(rawGO);
            GamePlay::PropertyAccessorBase props(rawGO);
            if (!props.Isvalid())
                continue;

            GamePlay::BehaviorType behavior = props.GetBehaviorType();
            GamePlay::DisplayType display = props.GetDisplayType();
            int modelId = props.GetModelId();

            std::shared_ptr<LevelGameObject> go = GamePlay::CreateGameObject(scene, rawGO);

            // trigger objects etc.:
            if (display == GamePlay::DisplayType::Hidden)
            {
                float scale = 5.f;
                RenderableInstancePtr inst = CreateSpriteInstance(GamePlay::DisplayType::SpriteTransparent, assets.GetPointTexture(), -1, scale, false, GamePlay::VisibilityFilter::GameObjects);
                go->m_renderInst = inst;
                inst->SetTransform(props.GetTransform());
                // register renderable
                scene.AddRenderableInstance(go->m_renderInst);
            }
            else if (!CreateObjectRenderInstance(scene, go, rawGO))
            {
                // create a white sprite
                float scale = 16.f;
                RenderableInstancePtr inst = CreateSpriteInstance(GamePlay::DisplayType::SpriteNormal, assets.GetPointTexture(), -1, scale, false, GamePlay::VisibilityFilter::GameObjects);
                go->m_renderInst = inst;
                inst->SetTransform(props.GetTransform());
                // register renderable
                scene.AddRenderableInstance(go->m_renderInst);
            }

            // additional debug geometry
            if (showDebugGeometry)
            {
                if (props.GetCollisionType() != GamePlay::CollisionType::None)
                {
                    float cx = props.CollisionRadiusXZ();
                    float cy = props.CollisionRadiusY();
                    std::shared_ptr<Renderable> cube = GeometryUtils::CreateCubeMesh(cx, cy, cx, { 255,255,0,0 });
                    go->m_debugGeometry = cube;
                    RenderableInstancePtr boxInst = std::make_shared<RenderableInstance>(cube.get(), (uint32_t)GamePlay::VisibilityFilter::DebugGeometry);
                    Vector center = props.GetTransform().GetTranslation();
                    boxInst->SetTransform(center, Vector(0,0,0)); // unrotated, unscaled box
                    scene.AddRenderableInstance(boxInst);
                }
            }

            // items for map
            if (addMapIcons)
            {
                if (Texture* mapIconTexture = GetTextureForMapIcon(rawGO))
                {
                    RenderablePtr quadMesh = AssetManager::Instance().GetCameraFacingQuadRenderable();
                    RenderableInstancePtr mapInst = std::make_shared<RenderableInstance>(quadMesh.get(), (uint32_t)GamePlay::VisibilityFilter::MapIcons);
                    mapInst->SetDebugName("ItemHighlight");
                    mapInst->m_tintColor = { 255, 255, 255, 255 };
                    Vector pos = go->GetPosition();
                    pos.Set(pos.GetX(), pos.GetY() + 200.f * DespToFloat, pos.GetZ());
                    mapInst->SetTransform(pos, Vector(0, 0, 0));
                    mapInst->m_uniformScale = 100.f * DespToFloat;
                    DrawCallMaterial& overrideMat = mapInst->m_materialOverride.emplace_back();
                    overrideMat = quadMesh->m_drawCalls[0].m_material;
                    overrideMat.m_transparencyType = TransparencyMode::AlphaBlended;
                    overrideMat.m_texture = mapIconTexture;
                    overrideMat.m_opacity = 1.f;
                    overrideMat.m_applyLighting = false;
                    overrideMat.ComputeHash();

                    scene.AddRenderableInstance(mapInst);
                }
            }

            // draw lines between doors and keys/switches
            if (showDoorConnections)
            {
                if (props.GetCollisionType() == GamePlay::CollisionType::Door)
                {
                    GamePlay::PropertyAccessorDoor door(rawGO);
                    if (!door.IsFree())
                    {
                        int sw = door.GetSwitchLock();
                        int key = door.GetKeyLock();
                        int grp = door.GetGroupLock();
                        for (RawGameObject& other : m_rawGameObject)
                        {
                            GamePlay::PropertyAccessorBase otherProp(other);
                            if (sw > 0 && otherProp.GetCollisionType() == GamePlay::CollisionType::Switch)
                            {
                                GamePlay::PropertyAccessorSwitch switchProp(other);
                                if (switchProp.GetSwitchIndex() == sw)
                                {
                                    debugGeometry.emplace_back(props.GetPosition(), otherProp.GetPosition(), Color4{ 100,100,255,0 });
                                }
                            }
                            if (grp > 0 && otherProp.GetCollisionType() == GamePlay::CollisionType::GroupedSwitch)
                            {
                                GamePlay::PropertyAccessorGroupedSwitch switchProp(other);
                                if (switchProp.GetGroupIndex() == grp)
                                {
                                    debugGeometry.emplace_back(props.GetPosition(), otherProp.GetPosition(), Color4{ 100,240,255,0 });
                                }
                            }
                            if (key > 0 && otherProp.GetCollisionType() == GamePlay::CollisionType::Key)
                            {
                                GamePlay::PropertyAccessorKey keyProp(other);
                                if (keyProp.GetKeyIndex() == key)
                                {
                                    debugGeometry.emplace_back(props.GetPosition(), otherProp.GetPosition(), Color4{ 255,255,50,0 });
                                }
                            }

                        }
                    }

                }
            }
            CO_AWAIT GamePlay::CreateObjectBehavior(scene, *go, rawGO);
            if (objectPresentation) { objectPresentation(rawGO.m_objectNumber + 2, rawGO, go->m_renderInst.get()); }
            scene.AddGameObject(go);
        }

        scene.AddDebugGeometry(debugGeometry);
    }

    scene.SetInitialPosition(m_playerStartPos);
    CO_RETURN playerLight;
}


FUTURE(void) Level::SaveGameAnalysis()
{
    std::vector<byte> fileData[8];
    fileData[0] = CO_AWAIT FileManager::Instance().ReadDataFuture(FileManager::Instance().FilenameToNative(L"SaveGames\\ByLevel\\2-2.SPL"));
    fileData[1] = CO_AWAIT FileManager::Instance().ReadDataFuture(FileManager::Instance().FilenameToNative(L"SaveGames\\ByLevel\\2-3.SPL"));
    fileData[2] = CO_AWAIT FileManager::Instance().ReadDataFuture(FileManager::Instance().FilenameToNative(L"SaveGames\\ByLevel\\2-4.SPL"));
    fileData[3] = CO_AWAIT FileManager::Instance().ReadDataFuture(FileManager::Instance().FilenameToNative(L"SaveGames\\ByLevel\\2-5.SPL"));
    fileData[4] = CO_AWAIT FileManager::Instance().ReadDataFuture(FileManager::Instance().FilenameToNative(L"SaveGames\\ByLevel\\2-6.SPL"));
    fileData[5] = CO_AWAIT FileManager::Instance().ReadDataFuture(FileManager::Instance().FilenameToNative(L"SaveGames\\ByLevel\\2-7.SPL"));

    const byte* rawBytes = fileData[0].data();
    // first test: find the level index. It must be an ascending byte
    int firstHit = -1;

    int pageSize = 50 * 40;
    byte sequenceToFind[] = {
        //33,1,2,1,2,4,5,1,10,0,232,3,170,5,0,0,122,13,0,0,0,0,0,0 // obj 0 , level 11
        33,1,2,1,2,4,5,3,10,0,232,3,114,6,156,255, // obj0, level 12

// Feld:
//0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,114,0,0,0,0,0 // str0, 10164
//0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 // str1
 //0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,25,25,25,25,25,25,25 // str3, 16164
// 50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50 // str4
 //128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,140,128,128,128,128,0 // str5 // 20164
// 128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,140,128,128,128,128,0 // str6


    };
    int sequenceLength = sizeof(sequenceToFind) / sizeof(sequenceToFind[0]);

    for (int i = 0; i < (int)fileData[0].size() - sequenceLength; i++)
    {
/*
        byte ref = fileData[0].at(i);
        bool fits = true;
        for (int j = 1; j < 6; j++)
        {
            if (fileData[j].at(i) != ref + j)
            {
                fits = false;
                break;
            }
        }
        if (fits)
        {
            firstHit = i;
        }
        */
        //const short* asShort = (const short*)rawBytes[0];
        if (memcmp(&rawBytes[i], sequenceToFind, sequenceLength) == 0)
        {
//            std::string actual = ByteArrayToStr(&rawBytes[i], 700);
            firstHit = i;
            RawGameObject playerO;
            memcpy(playerO.m_rawBytes, &rawBytes[i - 54], 54);
            GamePlay::PropertyAccessorBase props(playerO);
            const RawGameObject* list[] = { &playerO };
            std::wstring pl = GamePlay::LogRawGameObjects(list, 1);
            const byte* objCnt = &rawBytes[i - 56];
            int cnt1 = objCnt[0];
            int cnt2 = objCnt[1];
        }

    }



    // level index is at pos 1e:
    // part 1 : 30...
    // part 2 : 0...

}


FUTURE(void) Level::LoadSaveGameData(const byte* rawData, int dataSize)
{
    constexpr int geometry = 10164, map = geometry + 14000;
    if (!rawData || dataSize < map + 10 || dataSize > map + 40009)
    {
        throw std::runtime_error("Invalid SPL size for renderer loading.");
    }
    const auto word = [rawData](int offset)
    {
        return static_cast<unsigned int>(rawData[offset] | (rawData[offset + 1] << 8));
    };
    const auto ordinal = word(227);
    const auto width = word(map) + 1, height = word(map + 2) + 1;
    if (rawData[0] > 20 || rawData[23] == 0 || rawData[23] > 62 ||
        ordinal == 0 || ordinal > rawData[23] || word(243) != 165 ||
        width > 200 || height > 200 || dataSize != map + 4 + width * height + 5)
    {
        throw std::runtime_error("Invalid SPL header or collision map for renderer loading.");
    }
    std::uint32_t checksum = 0;
    for (int i = 0; i < dataSize - 4; ++i) { checksum += rawData[i]; }
    if (checksum != (word(dataSize - 4) | (word(dataSize - 2) << 16)))
    {
        throw std::runtime_error("SPL checksum mismatch.");
    }
    const int levelFile = rawData[23 + ordinal];
    if (levelFile == 0 || levelFile == 96) { throw std::runtime_error("Invalid saved level index."); }
    m_isSaveGame = true;
    m_levelInfo.Parse(levelFile, rawData + 103);
    InitLevelInfo(m_levelInfo);
    CO_AWAIT m_levelInfo.PreloadData();
    ReadGameObjects(rawData + 299, 163 * 54);
    ReadLevelData(rawData + geometry, 14000, false);
    ReadCollisionMap(rawData + map, static_cast<int>(4 + width * height));
    RawGameObject player{};
    memcpy(player.m_rawBytes, rawData + 245, 54);
    m_playerStartPos = GamePlay::PropertyAccessorBase(player).GetPosition();
}
