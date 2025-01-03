#pragma once
#include <DesperabisCommon.h>
#include <Asset/ModelGeometry.h>


enum class GroundMaterial : byte
{
    None = 0,
    Water = 1,
    Forest = 2,
    Lava = 3,
    Damage = 4,
    Carpet = 5,
    Oil = 6,
    Wood = 7,
    Level33 = 8,
};

enum class WaterSlotIndex : byte
{
    None = 0,
    WaterSlot1 = 1,
    WaterSlot2 = 2,
    WaterSlot3 = 3,
};

enum class HeightLevel : int
{
    Floor = 0,
    Ceiling = 1,
    WaterLevel1 = 2,
    WaterLevel2 = 3,
    WaterLevel3 = 4,
    Count = 5
};


struct GridCell
{
    float m_floorHeight, m_ceilingHeight;
    byte m_wallTextures, m_walls, m_floor, m_ceiling, m_rawMaterial, m_visibilityIsland;
    bool m_relevantForVisibility;
    GroundMaterial m_groundMaterial;
    WaterSlotIndex m_waterSlot;

    Color4 m_brightness;

    inline bool IsWaterSurface() const
    {
        return m_waterSlot != WaterSlotIndex::None;
    }

    inline bool IsRelevantForVisibility() const
    {
        return m_relevantForVisibility;
    }
};

struct GridCellArray
{
    float GridPosX(int tileX) const;
    float GridPosY(int tileY) const;
    Vector3 GetGridPos(int tileX, int tileY, HeightLevel atHeight) const;
    Vector3 GetCenterPos(int tileX, int tileY, HeightLevel atHeight) const;
    int GetCellX(float posX) const;
    int GetCellY(float posZ) const;
    bool GetCellIndices(const Vector& pos, int& tileX, int& tileY) const;
	void GetTileAssignment(const Vector& pos, BoundingVolume& volume) const;
    inline  bool IsInside(int tileX, int tileY) const
    {
        return tileX >= 0 && tileY >= 0 && tileX < m_sizeX && tileY < m_sizeZ;
    }
    uint8_t GetVisibilityIsland(const Vector& pos) const;
	GridCell& GetCell(int tileX, int tileY);
	const GridCell& GetCell(int tileX, int tileY) const;
	const GridCell* GetCellOrNull(int tileX, int tileY) const;
    GridCell* GetCellOrNull(int tileX, int tileY);
    const GridCell& GetCellClamped(int tileX, int tileY) const;
    const GridCell* GetCellAtPos(const Vector3& pos) const;
    float GetHeightAtPos(const Vector& pos) const;

	void DetermineVisibilityIslands(bool IsOutdoor);
	void FloodFillWithIdRecursive(int x, int y, byte Id);

    std::vector<GridCell> m_cell;
    int m_sizeX, m_sizeZ;
    float m_waterHeights[3] = {};
};
