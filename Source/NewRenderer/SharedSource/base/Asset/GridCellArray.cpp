#include "pch.h"
#include <Asset/GridCellArray.h>
#include <Asset/Level.h>

float GridCellArray::GridPosX(int tileX) const
{
    float d = 100.0f * DespToFloat;
    return (tileX - Level::tileCenterX) * d;
}

float GridCellArray::GridPosY(int tileY) const
{
    float d = 100.0f * DespToFloat;
    return (tileY - Level::tileCenterY) * d;
}

int GridCellArray::GetCellX(float posX) const
{
    float d = 1.f / (100.0f * DespToFloat);
    int tileX = (int)(floorf(posX * d)) + Level::tileCenterX;
    return tileX;
}

int GridCellArray::GetCellY(float posZ) const
{
    float d = 1.f / (100.0f * DespToFloat);
    int tileY = (int)(floorf(posZ * d)) + Level::tileCenterY;
    return tileY;
}


bool GridCellArray::GetCellIndices(const Vector& pos, int& tileX, int& tileY) const
{
    tileX = GetCellX(pos.GetX());
	tileY = GetCellY(pos.GetZ());
    return IsInside(tileX, tileY);
}

void GridCellArray::GetTileAssignment(const Vector& pos, BoundingVolume& volume) const
{
    const AlignedBox& box = volume.m_box;
    const float eps = 2.f * DespToFloat;
    volume.m_tileRange.m_minX = (int8_t)GetCellX(box.m_min.GetX() + eps);
    volume.m_tileRange.m_minZ = (int8_t)GetCellY(box.m_min.GetZ() + eps);
    volume.m_tileRange.m_maxX = (int8_t)GetCellX(box.m_max.GetX() - eps);
    volume.m_tileRange.m_maxZ = (int8_t)GetCellY(box.m_max.GetZ() - eps);

    int x, y;
    if (GetCellIndices(pos, x, y))
    {
        const GridCell& cell(GetCell(x, y));
        volume.m_visibilityIsland = cell.m_visibilityIsland;
		volume.m_tileAssignmentX = (int8_t)x;
        volume.m_tileAssignmentZ = (int8_t)y;
    }
    else
    {
        volume.m_visibilityIsland = 0;
        volume.m_tileAssignmentX = volume.m_tileAssignmentZ = -1;
    }
}

uint8_t GridCellArray::GetVisibilityIsland(const Vector& pos) const
{
    if (const GridCell* cell = GetCellOrNull(GetCellX(pos.GetX()), GetCellY(pos.GetZ())))
    {
        return cell->m_visibilityIsland;
    }
    return 0;
}

const GridCell& GridCellArray::GetCell(int tileX, int tileY) const
{
    const GridCell& cell = m_cell[tileY * m_sizeX + tileX];
    return cell;
}

GridCell& GridCellArray::GetCell(int tileX, int tileY)
{
	GridCell& cell = m_cell[tileY * m_sizeX + tileX];
	return cell;
}


const GridCell* GridCellArray::GetCellOrNull(int tileX, int tileY) const
{
    if (tileX >= 0 && tileX < m_sizeX && tileY >= 0 && tileY < m_sizeZ)
    {
        return &GetCell(tileX, tileY);
    }
    return nullptr;
}

GridCell* GridCellArray::GetCellOrNull(int tileX, int tileY)
{
	if (tileX >= 0 && tileX < m_sizeX && tileY >= 0 && tileY < m_sizeZ)
	{
		return &GetCell(tileX, tileY);
	}
	return nullptr;
}


const GridCell& GridCellArray::GetCellClamped(int tileX, int tileY) const
{
    if (tileX < 0) 
        tileX = 0;
    else if (tileX >= m_sizeX)
        tileX = m_sizeX - 1;
    if (tileY < 0)
        tileY = 0;
    else if (tileY >= m_sizeZ)
        tileY = m_sizeZ - 1;

    return GetCell(tileX, tileY);
}

const GridCell* GridCellArray::GetCellAtPos(const Vector3& pos) const
{
    return GetCellOrNull(GetCellX(pos.x), GetCellY(pos.z));
}

float GridCellArray::GetHeightAtPos(const Vector& pos) const
{
	float d = 1.f / (100.0f * DespToFloat);

	float xofs = (float)Level::tileCenterX;
	float yofs = (float)Level::tileCenterY;

	float tx = (pos.GetX() * d) + xofs;
	float ty = (pos.GetZ() * d) + yofs;
	float fx = tx - floorf(tx);
	float fy = ty - floorf(ty);
	int tileX0 = (int)(tx);
	int tileY0 = (int)(ty);

	const float outside = 0.0f;
	if (tileX0 < 0)
		return outside;
	else if (tileX0 > m_sizeX - 2)
		return outside;
	if (tileY0 < 0)
		return outside;
	else if (tileY0 > m_sizeZ - 2)
		return outside;

    // bilerp
    const float h00 = GetCell(tileX0, tileY0).m_floorHeight;
	const float h01 = GetCell(tileX0+1, tileY0).m_floorHeight;
	const float h10 = GetCell(tileX0, tileY0+1).m_floorHeight;
	const float h11 = GetCell(tileX0+1, tileY0+1).m_floorHeight;

	float row0 = h00 * (1.0f - fx) + h01 * fx;
	float row1 = h10 * (1.0f - fx) + h11 * fx;

	return row0 * (1.0f - fy) + row1 * fy;
}

Vector3 GridCellArray::GetGridPos(int tileX, int tileY, HeightLevel atHeight) const
{
    const GridCell& cell(GetCell(tileX, tileY));

    float h;
    if (atHeight == HeightLevel::Floor)
    {
        h = cell.m_floorHeight;
    }
    else if (atHeight == HeightLevel::Ceiling)
    {
        h = cell.m_ceilingHeight;
    }
    else
    {
        assert((int)atHeight >= (int)HeightLevel::WaterLevel1 && (int)atHeight <= (int)HeightLevel::WaterLevel3);
        h = m_waterHeights[(int)atHeight - (int)HeightLevel::WaterLevel1];
    }

    Vector3 pos = { GridPosX(tileX), h, GridPosY(tileY) };
    return pos;
}

Vector3 GridCellArray::GetCenterPos(int tileX, int tileY, HeightLevel atHeight) const
{
    float cx = (GridPosX(tileX) + GridPosX(tileX + 1)) * 0.5f;
    float cy = (GridPosY(tileY) + GridPosY(tileY + 1)) * 0.5f;
    const GridCell& c0(GetCell(tileX, tileY));
    const GridCell& c1(GetCell(tileX + 1, tileY));
    const GridCell& c2(GetCell(tileX + 1, tileY + 1));
    const GridCell& c3(GetCell(tileX, tileY + 1));
    float h;

    if (atHeight == HeightLevel::Floor)
    {
        h = __min(__min(c0.m_floorHeight, c1.m_floorHeight), __min(c2.m_floorHeight, c3.m_floorHeight));
    }
    else if (atHeight == HeightLevel::Ceiling)
    {
        h = __max(__max(c0.m_ceilingHeight, c1.m_ceilingHeight), __max(c2.m_ceilingHeight, c3.m_ceilingHeight));
    }
    else
    {
        assert((int)atHeight >= (int)HeightLevel::WaterLevel1 && (int)atHeight <= (int)HeightLevel::WaterLevel3);
        h = m_waterHeights[(int)atHeight - (int)HeightLevel::WaterLevel1];
    }

    Vector3 pos = { cx, h, cy };
    return pos;
}


void GridCellArray::DetermineVisibilityIslands(bool IsOutdoor)
{
    byte currentIslandId = 0;

    auto IsRelevant = [this](int x, int y) -> bool {
        if (x < 0 || y < 0 || x >= m_sizeX || y >= m_sizeZ)
            return false;
        const GridCell& cell(m_cell[y * m_sizeX + x]);
        return cell.m_floor != 0 || cell.m_ceiling != 0 || cell.IsWaterSurface();
    };

    // #1 set the m_relevantForVisibility flag for each tile
	for (int i = 0, y = 0; y < m_sizeZ; y++)
    {
        for (int x = 0; x < m_sizeX; x++, i++)
        {
			GridCell& cell(m_cell[i]);
            cell.m_relevantForVisibility = IsRelevant(x, y);
            if (!cell.m_relevantForVisibility && IsOutdoor)
            {
                // also look in close environment, otherwise we'll miss for instance the small houses in level 10
				cell.m_relevantForVisibility = IsRelevant(x - 1, y) || IsRelevant(x + 1, y) || IsRelevant(x, y - 1) || IsRelevant(x, y + 1);
				//cell.m_relevantForVisibility = IsRelevant(x - 1, y) || IsRelevant(x, y - 1); // one-cell environment is enough
            }
        }
    }

    // #2 flood fill visibility islands
    for (int i = 0, y = 0; y < m_sizeZ; y++)
    {
        for (int x = 0; x < m_sizeX; x++, i++)
        {
            GridCell& cell(m_cell[i]);
            if (!cell.IsRelevantForVisibility() || cell.m_visibilityIsland > 0)
            {
                continue;
            }
            currentIslandId++;
            FloodFillWithIdRecursive(x, y, currentIslandId);
        }
    }

    // #3 fill the gaps (a bit), so that walls from neighbor cells have the right island Id
    for (int i = 0, y = 0; y < m_sizeZ; y++)
    {
        for (int x = 0; x < m_sizeX; x++, i++)
        {
            GridCell& cell(m_cell[i]);
			if (!cell.IsRelevantForVisibility() || cell.m_visibilityIsland == 0)
			{
				continue;
			}

            for (int yy = y - 1; yy <= y + 1; yy++)
            {
                for (int xx = x - 1; xx <= x + 1; xx++)
                {
                    if (GridCell* other = GetCellOrNull(xx, yy))
                    {
                        if (other->m_visibilityIsland == 0)
                        {
                            other->m_visibilityIsland = cell.m_visibilityIsland;
                            yy = 10000; // break;
                            break;
                        }
                    }
                }
            }
        }
    }
}

void GridCellArray::FloodFillWithIdRecursive(int x, int y, byte Id)
{
    if (x < 0 || y < 0 || x >= m_sizeX || y >= m_sizeZ)
    {
		return;
    }
    GridCell& cell(m_cell[y * m_sizeX + x]);
    if (!cell.IsRelevantForVisibility() || cell.m_visibilityIsland > 0)
    {
		return;
    }
    cell.m_visibilityIsland = Id;
	FloodFillWithIdRecursive(x - 1, y, Id);
	FloodFillWithIdRecursive(x + 1, y, Id);
	FloodFillWithIdRecursive(x, y - 1, Id);
    FloodFillWithIdRecursive(x, y + 1, Id);
}

