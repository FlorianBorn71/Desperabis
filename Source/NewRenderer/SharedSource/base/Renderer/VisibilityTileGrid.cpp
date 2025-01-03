#include "pch.h"
#include <Renderer/VisibilityTileGrid.h>
#include <Asset/GridCellArray.h>




std::wstring VisibilityTileGrid::ToString() const
{
	std::wstringstream s;

	for (int z = 0; z < m_sizeZ; z++)
	{
		for (int x = 0; x < m_sizeX; x++)
		{
			VisitedFlags dst = m_result[z * m_sizeX + x];
			if ((dst & VisitedFlags::Camera) != 0)
			{
				s << "c";
			}
			else if ((dst & VisitedFlags::Visible) != 0)
			{
				s << "v";
			}
			else
			{
				const GridCell& cell(m_srcGrid->GetCell(x, z));
				byte Id = cell.m_visibilityIsland;
				if (Id == 0)
					s << ".";
				else
					s << char(Id + '0');
			}

		}
		s << "\n";
	}
	return s.str();
}


void VisibilityTileGrid::Initialize(const GridCellArray& src)
{
    m_srcGrid = &src;
	m_sizeX = src.m_sizeX;
	m_sizeZ = src.m_sizeZ;

    const int count = m_sizeX * m_sizeZ;
    m_result.resize(count);
    Clear(VisitedFlags::None);
}

void VisibilityTileGrid::Clear(VisitedFlags val)
{
    const int count = m_sizeX * m_sizeZ;
    for (int i = 0; i < count; i++)
        m_result[i] = val;
}



bool VisibilityTileGrid::IsVisible(const TileRange& range) const
{
	int x1 = __max(range.m_minX, 0);
	int z1 = __max(range.m_minZ, 0);
	int x2 = __min(range.m_maxX, m_sizeX - 1);
	int z2 = __min(range.m_maxZ, m_sizeZ - 1);

	// do not check too big areas - assume they are visible
	const int threshold = 4;
	if ((x2 - x1) > threshold || (z2 - z1) > threshold)
	{
		return true;
	}

	for (int z = z1; z <= z2; z++)
	{
		int ofs = z * m_sizeX + x1;
		const VisitedFlags* row = &m_result[ofs];
		for (int x = x1; x <= x2; x++, row++)
		{
			if ((*row & VisitedFlags::Visible) != 0)
			{
				return true;
			}
		}
	}

	return false;
}

bool VisibilityTileGrid::IsVisible(int tileX, int tileZ) const
{
	if (tileX < 0 || tileZ < 0 || tileX >= m_sizeX || tileZ >= m_sizeZ)
	{
		return false;
	}

	return (m_result[tileZ * m_sizeX + tileX] & VisitedFlags::Visible) != 0;
}



bool VisibilityTileGrid::IsVisible(const BoundingVolume& volume) const
{
	if (!IsIslandVisible(volume.m_visibilityIsland))
		return false;
	return IsVisible(volume.m_tileRange);
}

bool VisibilityTileGrid::IsPivotVisible(const BoundingVolume& volume) const
{
	return IsIslandVisible(volume.m_visibilityIsland) && IsVisible(volume.m_tileAssignmentX, volume.m_tileAssignmentZ);
}

void VisibilityTileGrid::IterateRecursive(int x, int z, DirectionFlags directionMask)
{
    if (x < 0 || z < 0 || x >= m_sizeX || z >= m_sizeZ || directionMask == DirectionFlags::None)
        return;

    VisitedFlags& dst = m_result[z * m_sizeX + x];
	const GridCell& srcCell(m_srcGrid->GetCell(x, z));
    if (dst != VisitedFlags::None || !srcCell.IsRelevantForVisibility() || srcCell.m_visibilityIsland != m_cameraIsland)
    {
		dst |= VisitedFlags::Visited;
        return;
    }
	dst |= VisitedFlags::Visited | VisitedFlags::Visible;
    if (m_srcGrid->GetCell(x, z).IsRelevantForVisibility())
    {
        // the idea is that the traversal must not flip into opposite direction
        if ((directionMask & DirectionFlags::Left) != 0) IterateRecursive(x - 1, z, directionMask & DirectionFlags::NotRight);
        if ((directionMask & DirectionFlags::Right) != 0) IterateRecursive(x + 1, z, directionMask & DirectionFlags::NotLeft);
        if ((directionMask & DirectionFlags::Up) != 0) IterateRecursive(x, z - 1, directionMask & DirectionFlags::NotDown);
        if ((directionMask & DirectionFlags::Down) != 0) IterateRecursive(x, z + 1, directionMask & DirectionFlags::NotUp);
    }
}


void VisibilityTileGrid::Run(const Vector& cameraPos, const Frustum& frustum, ProfilingStats& out_stats)
{
	m_cameraIsland = 0;
    int camTileX, camTileY;
    if (!m_srcGrid->GetCellIndices(cameraPos, camTileX, camTileY))
    {
        Clear(VisitedFlags::Visible | VisitedFlags::Visited); // camera outside -> all visible
		out_stats.m_debugString = ToString();
        return;
    }

	const GridCell& camCell(m_srcGrid->GetCell(camTileX, camTileY));
    if (!camCell.IsRelevantForVisibility())
    {
		Clear(VisitedFlags::Visible | VisitedFlags::Visited); // camera outside -> all visible
		out_stats.m_debugString = ToString();
		return;
    }

	m_cameraIsland = camCell.m_visibilityIsland;
    IterateRecursive(camTileX, camTileY, DirectionFlags::All);
	m_result[camTileY * m_sizeX + camTileX] |= VisitedFlags::Camera;

    out_stats.m_debugString = ToString();
}

