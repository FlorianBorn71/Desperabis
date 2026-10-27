#include "GameCoreHost.h"

#include <GameContext.h>

#include <algorithm>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <utility>

using namespace Desperabis;

namespace
{
	class HostInput final : public InputHandler
	{
	public:
		explicit HostInput(const GameCoreInput& input) : m_input(input) {}

		bool IsKeyPressed(InputKey key) override
		{
			switch (key)
			{
			case InputKey::ForwardKey: return m_input.forward;
			case InputKey::BackwardKey: return m_input.backward;
			case InputKey::StrafeLeftKey: return m_input.strafeLeft;
			case InputKey::StrafeRightKey: return m_input.strafeRight;
			case InputKey::TurnLeftKey: return m_input.turnLeft;
			case InputKey::TurnRightKey: return m_input.turnRight;
			case InputKey::InteractionKey: return m_input.interaction;
			}
			return false;
		}

	private:
		const GameCoreInput& m_input;
	};

	// GameCore requires the native 50x40 planes; variable level dimensions are not recovered yet.
	// Assumption: smaller FELD levels occupy the top-left of the native arrays (row stride 50),
	// which matches the renderer's size-independent tile coordinates (Level::DespToPos).
	std::vector<std::uint8_t> ToNativeGeometry(const std::vector<std::uint8_t>& feld)
	{
		constexpr std::size_t planeCount = 7;
		if (feld.size() < 4)
		{
			throw std::invalid_argument("Level geometry has no header.");
		}
		const std::size_t width = (std::size_t)(feld[0] | (feld[1] << 8)) + 1;
		const std::size_t height = (std::size_t)(feld[2] | (feld[3] << 8)) + 1;
		if (width > Level::Width || height > Level::Height || feld.size() < 4 + planeCount * width * height)
		{
			throw std::invalid_argument("Level geometry does not fit the native 50x40 arrays.");
		}
		if (width == Level::Width && height == Level::Height)
		{
			return feld;
		}

		std::vector<std::uint8_t> native(Level::GeometryStreamSize, 0);
		native[0] = (std::uint8_t)(Level::Width - 1);
		native[2] = (std::uint8_t)(Level::Height - 1);
		for (std::size_t plane = 0; plane < planeCount; plane++)
		{
			const std::uint8_t* src = feld.data() + 4 + plane * width * height;
			std::uint8_t* dst = native.data() + 4 + plane * Level::Width * Level::Height;
			for (std::size_t row = 0; row < height; row++)
			{
				std::memcpy(dst + row * Level::Width, src + row * width, width);
			}
		}
		return native;
	}

	LevelLoadData BuildLevelData(const GameCoreLevelSource& source)
	{
		LevelLoadData data;

		// No collision-map loader exists for fresh levels yet: start from the dummy (empty) map and real geometry.
		data.level = std::make_unique<Level>(CreateDummyLevel());
		const std::vector<std::uint8_t> geometry = ToNativeGeometry(source.geometry);
		data.level->LoadGeometry(geometry.data(), geometry.size());

		data.playerSpawn = PlayerCamera{ source.playerPosition, source.playerDirection };
		return data;
	}
}

class GameCoreHost::HostContext final : public GameContext
{
public:
	explicit HostContext(GameCoreHost::FileLoader fileLoader) : m_fileLoader(std::move(fileLoader))
	{
		if (!m_fileLoader) { throw std::invalid_argument("GameCoreHost requires a file loader."); }
	}
	const GameCoreLevelSource* m_source = nullptr;

protected:
	std::vector<std::uint8_t> LoadFile(std::string_view filename) override
	{
		return m_fileLoader(filename);
	}

	void LoadLevel(const LevelLoadRequest& request) override
	{
		if (m_source == nullptr || m_source->levelNumber != request.levelNumber)
		{
			FailLevelLoad(request.id, "No level data provided by the host.");
			return;
		}
		auto data = BuildLevelData(*m_source);
		LoadLevelObjects(request.levelNumber, data);
		CompleteLevelLoad(request.id, std::move(data));
	}

private:
	GameCoreHost::FileLoader m_fileLoader;
};

GameCoreHost::GameCoreHost(FileLoader fileLoader)
	: m_context(std::make_unique<HostContext>(std::move(fileLoader)))
{
}

GameCoreHost::~GameCoreHost() = default;

bool GameCoreHost::LoadLevel(const GameCoreLevelSource& source)
{
	m_lastError.clear();
	m_lastInteractionMessage.clear();
	m_context->m_source = &source;
	try
	{
		m_context->StartLevel((std::int16_t)source.levelNumber);
	}
	catch (const std::exception& e)
	{
		m_lastError = e.what();
	}
	m_context->m_source = nullptr;

	if (!IsLevelReady())
	{
		if (m_lastError.empty())
		{
			m_lastError = m_context->GetLevelLoadError();
		}
		return false;
	}

	return true;
}

bool GameCoreHost::IsLevelReady() const
{
	return m_context->GetRuntimeState() == GameRuntimeState::Ready;
}

bool GameCoreHost::Tick(int behaviorTicks, int frameTicks, const GameCoreInput& input)
{
	HostInput inputHandler(input);
	const GameTickTiming timing{ (std::int16_t)std::clamp(behaviorTicks, 0, 32767), (std::int16_t)std::clamp(frameTicks, 0, 32767) };
	try
	{
		const auto result = m_context->Tick(timing, inputHandler);
		if (input.interaction && result.interaction)
		{
			switch (result.interaction->status)
			{
			case InteractionStatus::Applied: m_lastInteractionMessage = "Interaction applied"; break;
			case InteractionStatus::NoEffect:
				m_lastInteractionMessage = m_context->playerCollisionSlot == 0 ?
					"No interaction target in reach" : "Interaction had no effect (locked, busy, or not pickable)";
				break;
			case InteractionStatus::InventoryFull: m_lastInteractionMessage = "Inventory full"; break;
			case InteractionStatus::InactiveObject: m_lastInteractionMessage = "Interaction target is inactive"; break;
			case InteractionStatus::UnimplementedHandler: m_lastInteractionMessage = "Interaction not implemented"; break;
			}
			const auto slot = m_context->playerCollisionSlot;
			if (slot != 0)
			{
				m_lastInteractionMessage += " [slot " + std::to_string(slot) + ", collision type " +
					std::to_string(static_cast<unsigned int>(m_context->GetWorldObjects()[slot].collisionType)) + "]";
			}
		}
		return result.status == GameTickStatus::Advanced;
	}
	catch (const std::exception& e)
	{
		m_lastError = e.what();
		return false;
	}
}

void GameCoreHost::GetObjectBytes(int slot, std::uint8_t (&out)[ObjectSize]) const
{
	EncodeGameObject(m_context->GetWorldObjects().at((std::size_t)slot), out, ObjectSize);
}

GameCorePlayerCamera GameCoreHost::GetPlayerCamera() const
{
	const PlayerCamera camera = m_context->GetPlayerCamera();
	return { camera.position, camera.direction };
}