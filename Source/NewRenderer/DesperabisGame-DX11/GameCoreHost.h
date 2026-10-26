#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Renderer-independent level input for the game context. Only plain data crosses this boundary.
struct GameCoreLevelSource
{
	int levelNumber = 0;
	std::vector<std::uint8_t> geometry;            // FELD<n>.256 file contents
	std::vector<std::uint8_t> objectRecords;       // consecutive 54-byte native records (OBJPOS<n>.DAT)
	// Native DOS eye coordinates; the controller settles onto terrain from this starting height.
	std::array<std::int16_t, 3> playerPosition{};
	std::array<std::int16_t, 3> playerDirection{}; // native view vector words
};

struct GameCoreInput
{
	bool forward = false;
	bool backward = false;
	bool strafeLeft = false;
	bool strafeRight = false;
	bool turnLeft = false;
	bool turnRight = false;
	bool interaction = false;
};

// Eye-position snapshot (see Desperabis::GameContext::GetPlayerCamera).
struct GameCorePlayerCamera
{
	std::array<std::int16_t, 3> position{};  // native DOS coordinates
	std::array<std::int16_t, 3> direction{}; // native view vector, not normalized, may be zero
};

// Owns the Desperabis-GameCore runtime. Kept in its own translation unit so that GameCore
// types (namespace Desperabis) never mix with the renderer's global types (Level, RawGameObject, ...).
class GameCoreHost
{
public:
	static constexpr int NumSlots = 166;
	static constexpr int PlayerSlot = 1;
	static constexpr int FirstObjectSlot = 2;
	static constexpr int LastObjectSlot = 164;
	static constexpr int ObjectSize = 54;
	GameCoreHost();
	~GameCoreHost();

	// Starts the level in the game context and completes the load synchronously from the given data.
	bool LoadLevel(const GameCoreLevelSource& source);
	bool IsLevelReady() const;

	// Advances the simulation. Returns true if the world advanced (object states may have changed).
	bool Tick(int behaviorTicks, int frameTicks, const GameCoreInput& input);

	// Native 54-byte payload of the given world slot.
	void GetObjectBytes(int slot, std::uint8_t (&out)[ObjectSize]) const;
	GameCorePlayerCamera GetPlayerCamera() const;

	const std::string& GetLastError() const { return m_lastError; }
	const std::string& GetLastInteractionMessage() const { return m_lastInteractionMessage; }

private:
	class HostContext;
	std::unique_ptr<HostContext> m_context;
	std::string m_lastError;
	std::string m_lastInteractionMessage;
};