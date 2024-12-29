#pragma once
#include <DesperabisCommon.h>

enum class DirectionFlags : uint8_t
{
	None = 0,
	Up = 1 << 0,
	Down = 1 << 1,
	Left = 1 << 2,
	Right = 1 << 3,

	NotUp = (uint8_t)~Up,
	NotDown = (uint8_t)~Down,
	NotLeft = (uint8_t)~Left,
	NotRight = (uint8_t)~Right,

	All = Up | Down | Left | Right
};
DECLARE_ENUM_FLAGS(DirectionFlags);

enum class VisitedFlags : uint8_t
{
	None = 0,
	Visited = 1 << 0,
	Visible = 1 << 1,
	Camera = 1 << 2, // mainly for debugging purposes
};
DECLARE_ENUM_FLAGS(VisitedFlags);


class VisibilityTileGrid
{
public:
    void Initialize(const GridCellArray& src);
	void Run(const Vector& cameraPos, const Frustum& frustum, ProfilingStats& out_stats);

	bool IsVisible(const BoundingVolume& volume) const;
	bool IsPivotVisible(const BoundingVolume& volume) const;
	bool IsVisible(const TileRange& range) const;
	bool IsVisible(int tileX, int tileZ) const;
	bool IsIslandVisible(uint8_t islandId) const { return m_cameraIsland == 0 || m_cameraIsland == islandId || islandId == 0; }
	std::wstring ToString() const;
protected:
    void Clear(VisitedFlags val = VisitedFlags::None);
    void IterateRecursive(int x, int z, DirectionFlags directionMask);
    const GridCellArray* m_srcGrid = nullptr;
    int m_sizeX = 0;
    int m_sizeZ = 0;
    std::vector<VisitedFlags> m_result;
	uint8_t m_cameraIsland = 0;
};
