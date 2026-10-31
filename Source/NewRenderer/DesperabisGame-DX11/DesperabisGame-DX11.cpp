// Desperabis-Game-DX11: minimal DX11 window that loads a level and renders it.
// Game objects are owned and simulated by Desperabis-GameCore (GameContext); the renderer only
// presents the world state after each simulation step and follows the player camera.

#include "pch.h"
#include <GameContext.h>
#include <GameMath.h>
#include "AudioPlayer.h"
#include "ApplicationUi.h"
#include "LightningPresentation.h"

#include <DesperabisCommon.h>
#include <Asset/AssetManager.h>
#include <Asset/Level.h>
#include <Asset/Font.h>
#include <Asset/Palette.h>
#include <DX11/RendererDX11.h>
#include <IO/FileManagerBuiltIn.h>
#include <Renderer/RenderableInstance.h>
#include <Renderer/TextMesh.h>
#include <Sample/FreeCamera.h>
#include <Sample/LevelScene.h>
#include <Renderer/Camera.h>
#include <Renderer/RenderingSettings.h>
#include <Renderer/VisibilitySettings.h>
#include <GamePlay/GamePlay.h>
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace
{
	constexpr int g_startLevelIndex = 10;
	bool g_cheatMode = true;

	constexpr int g_numObjectSlots = static_cast<int>(std::tuple_size_v<decltype(Desperabis::GameContext::worldObjects)>);
	// OBJPOS has no player record, so the initial view vector is a host choice.
	// GameContext seeds the player heading from it at load and rewrites velocityX/Z from the heading every tick.
	constexpr std::array<int16_t, 3> g_initialPlayerDirection = { -256, 0, 0 };
	constexpr int g_initialWidth = 1280;
	constexpr int g_initialHeight = 800;
	const wchar_t* g_windowClass = L"DesperabisGameDX11Window";
	const wchar_t* g_windowTitle = L"Desperabis (DX11)";

	HWND g_hWnd = nullptr;

	// DX
	ComPtr<ID3D11Device3> g_d3dDevice;
	ComPtr<ID3D11DeviceContext3> g_d3dContext;
	ComPtr<IDXGISwapChain1> g_swapChain;
	ComPtr<ID3D11RenderTargetView1> g_renderTargetView;
	ComPtr<ID3D11DepthStencilView> g_depthStencilView;
	D3D11_VIEWPORT g_viewport = {};

	// Desperabis
	RendererDX11 g_renderer;
	FileManagerBuiltIn g_fileManager;
	NullPhysicsModule g_physics;
	RenderingSettings g_renderSettings;
	VisibilitySettings g_visSettings{ (uint32_t)GamePlay::VisibilityFilter::NormalRendering };
	ProfilingStats g_profilingStats;
	std::unique_ptr<LevelScene> g_scene;
	std::shared_ptr<TextureAnim> g_fireballFrames;
	std::shared_ptr<TextureAnim> g_hudAnimations;
	std::unique_ptr<FreeCamera> g_freeCam;
	std::shared_ptr<Camera> g_camera;
	class GameAppContext;
	std::unique_ptr<GameAppContext> g_gameCore;
	std::unique_ptr<AudioPlayer> g_audioPlayer;
	std::array<std::shared_ptr<TextureAnim>, 3> g_fontAnimations;
	std::unique_ptr<Font> g_promptFont;
	ApplicationUi g_applicationUi;
	bool g_initialized = false;

	// presentation of the game world, indexed by native slot
	RenderableInstancePtr g_objectInst[g_numObjectSlots];
	RawGameObject g_objectState[g_numObjectSlots] = {};
	RawGameObject g_presentedState[g_numObjectSlots] = {};
	bool g_objectBehaviorCreated[g_numObjectSlots] = {};
	double g_worldTime = 0.0;
	bool g_useFreeCam = false;
	Vector g_playerForward(0.f, 0.f, 1.f, 0.f);

	std::chrono::steady_clock::time_point g_startTime;
	double g_currentTime = 0.0;
	double g_previousTime = 0.0;

	inline void ThrowIfFailed(HRESULT hr)
	{
		if (FAILED(hr))
		{
			assert(false && "Operation failed");
		}
	}

	std::vector<byte> ReadGameFile(const std::wstring& filename);
	void ClearGameObjects();

	class GameAppContext final : public Desperabis::GameContext
	{
	public:
		void OnLightningShown(const Desperabis::LightningFlash& flash) override
		{
			if (!g_scene || !g_camera) { throw std::logic_error("Lightning requires an active scene and camera."); }
			m_lightning.Show(*g_scene, flash, *g_camera);
		}

		void OnLightningHidden() override
		{
			m_lightning.Hide();
		}

		void OnPlayerHealthChanged(std::int32_t, std::int32_t) override
		{
			UpdateHealthBar();
		}

		float GetHealthBarHeight() const
		{
			const auto* frame = g_hudAnimations->GetFrame(1, 0);
			return (std::min)(g_viewport.Height,
				g_viewport.Width * static_cast<float>(frame->m_height) / frame->m_width) * 0.25f;
		}

		void UpdateHealthBar()
		{
			if (!g_scene || GetRuntimeState() != Desperabis::GameRuntimeState::Ready) { return; }
			if (playerLifeMaximum <= 0) { throw std::logic_error("Health bar requires a positive player-life maximum."); }
			const auto* frame = g_hudAnimations->GetFrame(1, 0);
			const float height = GetHealthBarHeight();
			const float fraction = std::clamp(
				static_cast<float>(playerLife) / (static_cast<float>(playerLifeMaximum) * 65536.f), 0.f, 1.f);
			auto background = std::make_shared<TextMeshInstance>(TextMesh::CreateTexturedQuad(
				*AssetManager::Instance().GetPlainWhiteTexture(), g_viewport.Width, height));
			background->SetScale(1.f);
			background->SetPosition(0.f, g_viewport.Height - height);
			background->SetTintColor(Color4{ 24,24,24,255 });
			std::shared_ptr<TextMeshInstance> fill;
			if (fraction > 0.f)
			{
				fill = std::make_shared<TextMeshInstance>(TextMesh::CreateTexturedQuad(
					*frame, g_viewport.Width * fraction, height, fraction, 0.5f));
				fill->SetScale(1.f);
				fill->SetPosition(0.f, g_viewport.Height - height);
			}
			ClearHealthBar();
			g_scene->AddTextMeshInstance(background);
			m_healthBackground = std::move(background);
			if (fill)
			{
				g_scene->AddTextMeshInstance(fill);
				m_healthFill = std::move(fill);
			}
		}

		void ClearHealthBar()
		{
			if (m_healthBackground) { g_scene->RemoveTextMeshInstance(m_healthBackground); m_healthBackground.reset(); }
			if (m_healthFill) { g_scene->RemoveTextMeshInstance(m_healthFill); m_healthFill.reset(); }
		}

		void PromptUserMessage(std::string_view text, std::int16_t duration) override
		{
			const auto ticks = text.empty() ? std::int16_t{ 0 } :
				duration < 0 ? Desperabis::WrapSignedWord(-static_cast<std::int32_t>(duration)) : duration;
			if (ticks > 0 && (!m_promptInstance || m_promptText != text))
			{
				auto mesh = g_promptFont->CreateGlyphMesh(text, Color4{ 204,102,0,255 },
					Font::HorizontalAlignment::Center, Font::VerticalAlignment::Center);
				auto instance = std::make_shared<TextMeshInstance>(std::move(mesh));
				RemovePromptMesh();
				g_scene->AddTextMeshInstance(instance);
				m_promptInstance = std::move(instance);
			}
			m_promptText = text;
			m_promptTicks = ticks;
			m_promptBottomAnchored = duration < 0;
			if (IsUserMessageActive()) { UpdatePromptPosition(); }
			else { RemovePromptMesh(); }
		}

		bool IsUserMessageActive() const override { return m_promptTicks > 0; }

		std::int16_t GetSpriteBottom(const Desperabis::RawGameObject& object) const override
		{
			if (!g_scene || !g_scene->m_level) { throw std::runtime_error("Sprite physics requires loaded level assets."); }
			const auto animation = g_scene->m_level->GetLevelInfo().GetTextureAnim(object.modelId);
			const int row = static_cast<int>(object.modelAnim) - 1;
			const int frame = static_cast<int>(object.animFrame) - 1;
			if (!animation || !animation->IsValidFrame(row, frame))
			{
				throw std::runtime_error("Sprite physics references an unavailable animation frame.");
			}
			const auto* image = animation->GetFrame(row, frame);
			return Desperabis::WrapSignedWord(image->m_relY + image->m_height - 1);
		}

		void UpdateUserMessage(std::int16_t ticks)
		{
			if (ticks < 0) { throw std::invalid_argument("Message updates require nonnegative ticks."); }
			if (m_promptTicks > 0)
			{
				m_promptTicks = Desperabis::WrapSignedWord(m_promptTicks - ticks);
				if (m_promptTicks <= 0) { RemovePromptMesh(); }
			}
		}

		void ClearPrompt()
		{
			RemovePromptMesh();
			m_promptText.clear();
			m_promptTicks = 0;
		}

		void UpdatePromptPosition()
		{
			if (m_promptInstance)
			{
				const float scale = (std::min)(2.f, g_viewport.Width /
					(std::max)(1.f, m_promptInstance->GetMesh()->GetExtent().x + 16.f));
				m_promptInstance->SetScale(scale);
				const float y = m_promptBottomAnchored ?
					g_viewport.Height - GetHealthBarHeight() - 16.f -
						m_promptInstance->GetMesh()->GetExtent().y * scale * 0.5f :
					g_viewport.Height * 0.5f;
				m_promptInstance->SetPosition(g_viewport.Width * 0.5f, y);
			}
		}

		void ApplyCheatLoadout()
		{
			using Desperabis::CollisionType;
			using Desperabis::WeaponType;
			struct CheatWeapon
			{
				WeaponType type;
				std::int16_t amount;
			};
			const CheatWeapon weapons[] =
			{
				{ WeaponType::Dagger, 360 },
				{ WeaponType::Sword, 360 },
				{ WeaponType::Crossbow, 10 },
				{ WeaponType::Fireball, (std::numeric_limits<std::int16_t>::max)() },
				{ WeaponType::Electricity, 360 },
				{ WeaponType::Stone, 360 }
			};
			auto items = inventory;
			auto amounts = weaponAmounts;
			std::array<Desperabis::RawGameObject, 32> templates{};
			for (const auto* filename : { "LEVEL\\OBJPOS0.DAT", "LEVEL\\OBJPOS10.DAT", "LEVEL\\OBJPOS50.DAT" })
			{
				const auto bytes = LoadFile(filename);
				constexpr auto recordSize = sizeof(Desperabis::RawGameObject);
				if (bytes.empty() || bytes.size() % recordSize != 0)
				{
					throw std::runtime_error(std::string(filename) + ": invalid cheat weapon templates.");
				}
				for (size_t offset = 0; offset < bytes.size(); offset += recordSize)
				{
					const auto item = Desperabis::DecodeGameObject(bytes.data() + offset, recordSize);
					if (item.health == 1000 && item.collisionType == CollisionType::Weapon &&
						item.itemValue > 0 && static_cast<size_t>(item.itemValue) < templates.size() &&
						templates[item.itemValue].health == 0)
					{
						templates[item.itemValue] = item;
					}
				}
			}
			for (const auto& weapon : weapons)
			{
				const auto id = static_cast<size_t>(weapon.type);
				auto slot = std::find_if(items.begin(), items.end(), [id](const auto& item)
				{
					return item.health == 1000 && item.collisionType == CollisionType::Weapon &&
						item.itemValue == static_cast<std::int16_t>(id);
				});
				if (slot == items.end())
				{
					slot = std::find_if(items.begin(), items.end(), [](const auto& item) { return item.health == 0; });
					if (slot == items.end()) { throw std::runtime_error("Cheat loadout requires six weapon inventory slots."); }
					if (templates[id].health == 0) { throw std::runtime_error("Missing original cheat weapon template."); }
					*slot = templates[id];
				}
				if (weapon.type == WeaponType::Fireball) { slot->generalPurpose1 = static_cast<std::uint16_t>(weapon.amount); }
				amounts[id] = weapon.amount;
			}
			inventory = items;
			weaponAmounts = amounts;
			RefreshWeaponInventory();
		}

	protected:
		std::vector<std::uint8_t> LoadFile(std::string_view filename) override
		{
			return ReadGameFile(std::wstring(filename.begin(), filename.end()));
		}

		void LoadLevel(const Desperabis::LevelLoadRequest& request) override
		{
			Desperabis::LevelLoadData data;
			LoadLevelGeometry(request.levelNumber, data);
			LoadLevelObjects(request.levelNumber, data);
			InitializeLevelPlayerSpawn(data, g_initialPlayerDirection);
			auto scene = std::make_unique<LevelScene>(request.levelNumber);
			scene->m_createGameObjects = false;
			scene->m_createMapIcons = false;
			FORCE_WAIT(scene->CreateScene());
			ClearPrompt();
			ClearHealthBar();
			ClearGameObjects();
			g_applicationUi.Clear();
			g_scene = std::move(scene);
			g_worldTime = 0;
			g_freeCam->JumpTo(g_scene->GetInitialPosition());
			if (g_cheatMode) { ApplyCheatLoadout(); }
			const auto title = "Desperabis (DX11) - level " + std::to_string(request.levelNumber);
			SetWindowTextA(g_hWnd, title.c_str());
			if (g_initialized)
			{
				g_previousTime = std::chrono::duration<double>(
					std::chrono::steady_clock::now() - g_startTime).count();
			}
			CompleteLevelLoad(request.id, std::move(data));
		}

	private:
		LightningPresentation m_lightning;
		void RemovePromptMesh()
		{
			if (m_promptInstance)
			{
				g_scene->RemoveTextMeshInstance(m_promptInstance);
				m_promptInstance.reset();
			}
		}

		std::shared_ptr<TextMeshInstance> m_promptInstance;
		std::shared_ptr<TextMeshInstance> m_healthBackground;
		std::shared_ptr<TextMeshInstance> m_healthFill;
		std::string m_promptText;
		std::int16_t m_promptTicks = 0;
		bool m_promptBottomAnchored = false;
	};

	class GameInput final : public Desperabis::InputHandler
	{
	public:
		bool forward = false;
		bool backward = false;
		bool strafeLeft = false;
		bool strafeRight = false;
		bool turnLeft = false;
		bool turnRight = false;
		bool interaction = false;
		bool attack = false;
		bool selectDagger = false;
		bool selectSword = false;
		bool jump = false;
		bool crouch = false;
		bool lookUp = false;
		bool lookDown = false;
		bool centerLook = false;
		std::array<bool, 6> selectOtherWeapons{};
		bool selectScythe = false;
		bool selectLentils = false;
		bool mainMenu = false;
		bool inventory = false;
		bool uiUp = false;
		bool uiDown = false;
		bool uiLeft = false;
		bool uiRight = false;
		bool uiConfirm = false;
		bool uiCancel = false;
		bool debugNextLevel = false;
		bool debugNextGamePart = false;

		bool IsKeyPressed(Desperabis::InputKey key) override
		{
			switch (key)
			{
			case Desperabis::InputKey::ForwardKey: return forward;
			case Desperabis::InputKey::BackwardKey: return backward;
			case Desperabis::InputKey::StrafeLeftKey: return strafeLeft;
			case Desperabis::InputKey::StrafeRightKey: return strafeRight;
			case Desperabis::InputKey::TurnLeftKey: return turnLeft;
			case Desperabis::InputKey::TurnRightKey: return turnRight;
			case Desperabis::InputKey::InteractionKey: return interaction;
			case Desperabis::InputKey::AttackKey: return attack;
			case Desperabis::InputKey::SelectDaggerKey: return selectDagger;
			case Desperabis::InputKey::SelectSwordKey: return selectSword;
			case Desperabis::InputKey::JumpKey: return jump;
			case Desperabis::InputKey::CrouchKey: return crouch;
			case Desperabis::InputKey::LookUpKey: return lookUp;
			case Desperabis::InputKey::LookDownKey: return lookDown;
			case Desperabis::InputKey::CenterLookKey: return centerLook;
			case Desperabis::InputKey::SelectCrossbowKey: return selectOtherWeapons[0];
			case Desperabis::InputKey::SelectFireballKey: return selectOtherWeapons[1];
			case Desperabis::InputKey::SelectStoneKey: return selectOtherWeapons[2];
			case Desperabis::InputKey::SelectPlaceableKey: return selectOtherWeapons[3];
			case Desperabis::InputKey::SelectElectricityKey: return selectOtherWeapons[4];
			case Desperabis::InputKey::SelectShovelKey: return selectOtherWeapons[5];
			case Desperabis::InputKey::SelectScytheKey: return selectScythe;
			case Desperabis::InputKey::SelectLentilsKey: return selectLentils;
			case Desperabis::InputKey::MainMenuKey: return mainMenu;
			case Desperabis::InputKey::InventoryKey: return inventory;
			case Desperabis::InputKey::UiUpKey: return uiUp;
			case Desperabis::InputKey::UiDownKey: return uiDown;
			case Desperabis::InputKey::UiLeftKey: return uiLeft;
			case Desperabis::InputKey::UiRightKey: return uiRight;
			case Desperabis::InputKey::UiConfirmKey: return uiConfirm;
			case Desperabis::InputKey::UiCancelKey: return uiCancel;
			case Desperabis::InputKey::DebugNextLevelKey: return debugNextLevel;
			case Desperabis::InputKey::DebugNextGamePartKey: return debugNextGamePart;
			case Desperabis::InputKey::Count: break;
			}
			throw std::invalid_argument("Unknown game input key.");
		}
	};

	void CreateWindowSizeDependentResources()
	{
		ID3D11RenderTargetView* nullViews[] = { nullptr };
		g_d3dContext->OMSetRenderTargets(ARRAYSIZE(nullViews), nullViews, nullptr);
		g_renderer.ReleaseWindowSizeDependentResources();
		g_renderTargetView.Reset();
		g_depthStencilView.Reset();
		g_d3dContext->Flush();

		RECT rect;
		GetClientRect(g_hWnd, &rect);
		const UINT width = (UINT)__max(1, rect.right - rect.left);
		const UINT height = (UINT)__max(1, rect.bottom - rect.top);

		if (g_swapChain != nullptr)
		{
			ThrowIfFailed(g_swapChain->ResizeBuffers(2, width, height, DXGI_FORMAT_B8G8R8A8_UNORM, 0));
		}
		else
		{
			ComPtr<IDXGIDevice3> dxgiDevice;
			ThrowIfFailed(g_d3dDevice.As(&dxgiDevice));
			ComPtr<IDXGIAdapter> dxgiAdapter;
			ThrowIfFailed(dxgiDevice->GetAdapter(&dxgiAdapter));
			ComPtr<IDXGIFactory2> dxgiFactory;
			ThrowIfFailed(dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory)));

			DXGI_SWAP_CHAIN_DESC1 desc = {};
			desc.Width = width;
			desc.Height = height;
			desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
			desc.SampleDesc.Count = 1;
			desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			desc.BufferCount = 2;
			desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
			desc.Scaling = DXGI_SCALING_NONE;
			desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
			ThrowIfFailed(dxgiFactory->CreateSwapChainForHwnd(g_d3dDevice.Get(), g_hWnd, &desc, nullptr, nullptr, &g_swapChain));
			ThrowIfFailed(dxgiDevice->SetMaximumFrameLatency(1));
		}

		ComPtr<ID3D11Texture2D1> backBuffer;
		ThrowIfFailed(g_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)));
		ThrowIfFailed(g_d3dDevice->CreateRenderTargetView1(backBuffer.Get(), nullptr, &g_renderTargetView));

		// typeless + shader resource: the renderer reads depth back for coronas
		CD3D11_TEXTURE2D_DESC1 depthDesc(DXGI_FORMAT_R32_TYPELESS, width, height, 1, 1,
			D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE);
		ComPtr<ID3D11Texture2D1> depthStencil;
		ThrowIfFailed(g_d3dDevice->CreateTexture2D1(&depthDesc, nullptr, &depthStencil));

		CD3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc(D3D11_DSV_DIMENSION_TEXTURE2D, DXGI_FORMAT_D32_FLOAT);
		ThrowIfFailed(g_d3dDevice->CreateDepthStencilView(depthStencil.Get(), &dsvDesc, &g_depthStencilView));

		g_viewport = CD3D11_VIEWPORT(0.0f, 0.0f, (float)width, (float)height);
		g_d3dContext->RSSetViewports(1, &g_viewport);
		if (g_gameCore)
		{
			g_gameCore->UpdatePromptPosition();
			g_gameCore->UpdateHealthBar();
		}
	}

	void UpdateProjection()
	{
		if (g_camera != nullptr)
		{
			Matrix4 projMat;
			projMat.CreateProjection(g_viewport.Width / g_viewport.Height);
			g_camera->SetProjectionMatrix(projMat);
		}
		if (g_freeCam != nullptr)
		{
			g_freeCam->SetWindowCenter(g_viewport.Width * 0.5f, g_viewport.Height * 0.5f);
		}
	}

	bool InitDX()
	{
		UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
		creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
		D3D_FEATURE_LEVEL featureLevels[] =
		{
			D3D_FEATURE_LEVEL_12_1,
			D3D_FEATURE_LEVEL_12_0,
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0,
		};

		ComPtr<ID3D11Device> device;
		ComPtr<ID3D11DeviceContext> context;
		D3D_FEATURE_LEVEL featureLevel;
		HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, 0, creationFlags,
			featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION, &device, &featureLevel, &context);
		if (FAILED(hr) && (creationFlags & D3D11_CREATE_DEVICE_DEBUG))
		{
			// SDK layers might not be installed
			creationFlags &= ~D3D11_CREATE_DEVICE_DEBUG;
			hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, 0, creationFlags,
				featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION, &device, &featureLevel, &context);
		}
		if (FAILED(hr))
		{
			hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, 0, creationFlags,
				featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION, &device, &featureLevel, &context);
		}
		if (FAILED(hr) || FAILED(device.As(&g_d3dDevice)) || FAILED(context.As(&g_d3dContext)))
		{
			return false;
		}

		ComPtr<ID3D11Multithread> contextMultithread;
		if (SUCCEEDED(context.As(&contextMultithread)))
		{
			contextMultithread->SetMultithreadProtected(true);
		}

		CreateWindowSizeDependentResources();
		return true;
	}

	std::vector<byte> ReadGameFile(const std::wstring& filename)
	{
		FileManager& fileManager = FileManager::Instance();
		const std::wstring path = fileManager.FilenameToNative(filename);
		return FORCE_GET(fileManager.ReadDataFuture(path));
	}

	int16_t ToNativeWord(float value)
	{
		return (int16_t)std::clamp(std::lround(value), -32768L, 32767L);
	}

	// inverse of Level::DespToPos
	std::array<int16_t, 3> PosToDesp(const Vector& pos)
	{
		const float invScale = 1.f / DespToFloat;
		const float xofs = (float)Level::tileCenterX * 100.0f + 100.0f;
		const float zofs = 41 * 100.0f - (float)Level::tileCenterY * 100.0f;
		return { ToNativeWord(pos.GetX() * invScale + xofs), ToNativeWord(pos.GetY() * invScale), ToNativeWord(zofs - pos.GetZ() * invScale) };
	}

	void ReportGameCoreError(const char* context, const std::string& error)
	{
		std::string msg = std::string("GameCore: ") + context + ": " + error + "\n";
		OutputDebugStringA(msg.c_str());
	}

	std::string DescribeInteraction(const Desperabis::InteractionResult& interaction)
	{
		std::string message;
		switch (interaction.status)
		{
		case Desperabis::InteractionStatus::Applied: message = "Interaction applied"; break;
		case Desperabis::InteractionStatus::NoEffect:
			message = g_gameCore->playerCollisionSlot == 0 ?
				"No interaction target in reach" : "Interaction had no effect (locked, busy, or not pickable)";
			break;
		case Desperabis::InteractionStatus::InventoryFull: message = "Inventory full"; break;
		case Desperabis::InteractionStatus::InactiveObject: message = "Interaction target is inactive"; break;
		case Desperabis::InteractionStatus::UnimplementedHandler: message = "Interaction not implemented"; break;
		}
		const auto slot = g_gameCore->playerCollisionSlot;
		if (slot != 0)
		{
			message += " [slot " + std::to_string(slot) + ", collision type " +
				std::to_string(static_cast<unsigned int>(g_gameCore->GetWorldObjects()[slot].collisionType)) + "]";
		}
		return message;
	}

	void ClearGameObjects()
	{
		g_renderer.SetItemPreview(nullptr, false, {});
		for (int slot = 0; slot < g_numObjectSlots; slot++)
		{
			if (g_objectInst[slot] != nullptr && g_scene != nullptr)
			{
				g_scene->RemoveRenderableInstance(g_objectInst[slot]);
			}
			g_objectInst[slot].reset();
			g_objectState[slot] = {};
			g_presentedState[slot] = {};
			g_objectBehaviorCreated[slot] = false;
		}
	}

	void UpdateItemPreview()
	{
		g_renderer.SetItemPreview(nullptr, false, {});
		if (g_useFreeCam || g_gameCore->GetRuntimeState() != Desperabis::GameRuntimeState::Ready ||
			g_gameCore->playerLife <= 0) { return; }
		const auto slot = g_gameCore->playerCollisionSlot;
		if (slot < 2 || slot > 164) { return; }
		const auto& object = g_gameCore->GetWorldObjects().at(slot);
		using Desperabis::CollisionType;
		const bool pickable = object.pickupType != Desperabis::PickupType::None ||
			object.collisionType == CollisionType::Poison || object.collisionType == CollisionType::Medicine ||
			object.collisionType == CollisionType::Gold || object.collisionType == CollisionType::PickableMap;
		const auto& instance = g_objectInst[slot];
		if (object.health <= 0 || !pickable || !instance || !instance->IsValid() || !instance->GetVisible()) { return; }
		const float margin = (std::min)(g_viewport.Width, g_viewport.Height) * 0.02f;
		const float side = (std::min)(g_viewport.Width - margin * 2.f,
			g_viewport.Height - g_gameCore->GetHealthBarHeight() - margin * 2.f) * 0.16f;
		if (side <= 0.f) { return; }
		const D3D11_VIEWPORT viewport = {
			g_viewport.Width - margin - side,
			g_viewport.Height - g_gameCore->GetHealthBarHeight() - margin - side,
			side,side,0.f,1.f };
		const GamePlay::PropertyAccessorBase properties(g_objectState[slot]);
		g_renderer.SetItemPreview(instance, GamePlay::IsSpriteDisplayType(properties.GetDisplayType()), viewport);
	}

	// Copies the world state out of the game context and updates the renderables of changed slots.
	void SyncGameObjects()
	{
		Level* level = g_scene->m_level;
		for (int slot = 2; slot <= 164; slot++)
		{
			RawGameObject& state = g_objectState[slot];
			state.m_level = level;
			state.m_objectNumber = slot;
			Desperabis::EncodeGameObject(g_gameCore->GetWorldObjects().at(slot), state.m_rawBytes, sizeof(state.m_rawBytes));
			state.m_rawBytes[48] = 0; // render-list scratch, rewritten every tick
			state.m_rawBytes[49] = 0;
			if (state == g_presentedState[slot])
			{
				continue;
			}
			g_presentedState[slot] = state;

			RenderableInstancePtr& inst = g_objectInst[slot];
			const GamePlay::PropertyAccessorBase prop(state);
			if (!prop.Isvalid())
			{
				if (inst != nullptr)
				{
					g_scene->RemoveRenderableInstance(inst);
					inst.reset();
				}
				continue;
			}

			const bool firstAssignment = inst == nullptr;
			if (firstAssignment)
			{
				inst = std::make_shared<RenderableInstance>(nullptr, (uint32_t)GamePlay::VisibilityFilter::GameObjects);
			}
			Texture* spriteOverride = nullptr;
			const auto& coreObject = g_gameCore->GetWorldObjects().at(slot);
			if (coreObject.behavior == Desperabis::BehaviorType::FireballProjectile)
			{
				const auto frame = coreObject.doorLockOrGroupBitmask == 0 ? 0 : coreObject.doorLockOrGroupBitmask - 1;
				if (!g_fireballFrames || frame < 0 || frame >= static_cast<int>(g_fireballFrames->m_frames.size()))
				{
					throw std::runtime_error("Fireball animation frame is unavailable.");
				}
				spriteOverride = &g_fireballFrames->m_frames[frame];
			}
			level->ApplyRenderProperties(inst, state, spriteOverride);

			const GamePlay::BehaviorType behavior = prop.GetBehaviorType();
			if (behavior == GamePlay::BehaviorType::Moon && inst->GetRenderable() != nullptr && inst->m_materialOverride.size() == 1)
			{
				GamePlay::ConvertToMoon(*inst, state);
			}

			if (firstAssignment)
			{
				g_scene->AddRenderableInstance(inst);
				inst->UpdateGlobalBounds();
			}
			else { g_scene->AssignFireAnimations(*inst); }

			// Presentation-only extras (lights, fire texture animations), once per slot. The renderer-side
			// game object is not added to the scene, so it does not simulate anything itself.
			if (!g_objectBehaviorCreated[slot] && behavior != GamePlay::BehaviorType::Moon)
			{
				g_objectBehaviorCreated[slot] = true;
				LevelGameObject presentation(g_scene.get(), state);
				presentation.m_renderInst = inst;
				FORCE_WAIT(GamePlay::CreateObjectBehavior(*g_scene, presentation, state));
			}
		}
	}

	void ApplyPlayerCamera()
	{
		const auto playerCam = g_gameCore->GetPlayerCamera();
		const Vector pos = Level::DespToPos(playerCam.position[0], playerCam.position[1], playerCam.position[2]);

		// a zero direction is not a valid orientation: keep the previous one
		const auto& dir = playerCam.direction;
		if (dir[0] != 0 || dir[1] != 0 || dir[2] != 0)
		{
			// Present the DOS horizon shift as pitch in the modern perspective camera.
			const float horizontalLength = Vector((float)dir[0], 0.f, (float)dir[2], 0.f).GetLength();
			Vector forward(-(float)dir[0],
				(float)dir[1] + (110.f - playerCam.lookHeight) * horizontalLength / 100.f, (float)dir[2], 0.f);
			forward.Normalize();
			Vector worldUp(0.f, 1.f, 0.f, 0.f);
			if (worldUp.GetCross(forward).GetLength() > 1e-4f) // looking straight up/down has no defined right vector
			{
				g_playerForward = forward;
			}
		}

		// Same basis as GamePlay::PropertyAccessorPlayer::GetCameraTransform. With the right-handed projection the
		// camera looks along -forward, i.e. along native (velocityX, -velocityZ): the direction the player walks.
		Vector forward = g_playerForward;
		Vector up(0.f, 1.f, 0.f, 0.f);
		Vector right = up.GetCross(forward);
		right.Normalize();
		up = forward.GetCross(right);
		Matrix4 cameraTransform;
		cameraTransform.Create(pos, right, up, forward);
		g_camera->SetCameraTransformMatrix(cameraTransform);
	}

	void ToggleFreeCamera()
	{
		if (g_gameCore->GetRuntimeState() != Desperabis::GameRuntimeState::Ready)
		{
			return;
		}
		g_useFreeCam = !g_useFreeCam;
		if (g_useFreeCam)
		{
			g_freeCam->JumpTo(g_camera->GetCameraPosition());
		}
	}

	FUTURE(void) InitDesperabis()
	{
		RenderingSettings::SetRelativeGeometryScaling(1.f);
		g_fileManager.Initialize();
		g_renderSettings.gridVisibility = false;

		// the renderer must exist before assets can be loaded
		CO_AWAIT g_renderer.Initialize(g_d3dDevice.Get(), g_d3dContext.Get(),
			g_renderTargetView.Get(), g_depthStencilView.Get(), g_viewport);
		CO_AWAIT AssetManager::Instance().Initialize();

		g_promptFont = std::make_unique<Font>();
		g_promptFont->SetSpaceWidth(6.f);
		const std::wstring_view fontFiles[] =
		{
			L"ANIMS\\SCHRIFT0.AN2", L"ANIMS\\SCHRIFT1.AN2", L"ANIMS\\SCHRIFT2.AN2"
		};
		auto fontPalette = std::make_shared<Palette>();
		fontPalette->BuildGrayscale();
		for (size_t i = 0; i < g_fontAnimations.size(); ++i)
		{
			g_fontAnimations[i] = CO_AWAIT AssetManager::Instance().LoadAsset<TextureAnim>(fontFiles[i], fontPalette);
			g_promptFont->AddGlyphs(g_fontAnimations[i].get(), ApplicationUi::FontCharacters[i]);
		}

		g_gameCore = std::make_unique<GameAppContext>();
		g_fireballFrames = CO_AWAIT AssetManager::Instance().LoadAsset<TextureAnim>(L"ANIMS\\SCHUSS1.AN2");
		g_hudAnimations = CO_AWAIT AssetManager::Instance().LoadAsset<TextureAnim>(L"ANIMS\\AMA_ANI.AN2");
		CO_AWAIT g_applicationUi.Initialize();
		std::vector<std::shared_ptr<const SoundSample>> samples;
		for (int sample = 1; sample <= 129; ++sample)
		{
			samples.push_back(CO_AWAIT AssetManager::Instance().LoadAsset<SoundSample>(
				L"SAMP\\SOUND" + std::to_wstring(sample) + L".SMP"));
		}
		const auto soundTable = CO_AWAIT FileManager::Instance().ReadDataFuture(
			FileManager::Instance().FilenameToNative(L"SAMP\\S_TAB.PRB"));
		g_gameCore->LoadSoundEffects(soundTable.data(), soundTable.size());
		g_audioPlayer = std::make_unique<AudioPlayer>(std::move(samples));
		g_gameCore->audioHandler = g_audioPlayer.get();
		if (!g_fireballFrames || g_fireballFrames->m_frames.size() != 10)
		{
			throw std::runtime_error("Expected the original ten-frame SCHUSS1 fireball animation.");
		}
		if (!g_hudAnimations || !g_hudAnimations->IsValidFrame(1, 0) ||
			g_hudAnimations->GetFrame(1, 0)->m_width != 90 || g_hudAnimations->GetFrame(1, 0)->m_height != 2)
		{
			throw std::runtime_error("Expected the original 90x2 AMA_ANI health gradient.");
		}

		g_freeCam = std::make_unique<FreeCamera>();
		g_freeCam->SetUseGravity(false);

		g_camera = Renderer::Instance().GetOrCreateCamera();
		UpdateProjection();

		g_gameCore->StartLevel(g_startLevelIndex);
		SyncGameObjects();
		g_gameCore->UpdateHealthBar();

		g_startTime = std::chrono::steady_clock::now();
		g_initialized = true;
	}

	void DeinitDesperabis()
	{
		g_initialized = false;
		g_gameCore->StopAllSoundEffects();
		g_audioPlayer->StopAmbientPlayback();
		g_gameCore->audioHandler = nullptr;
		g_audioPlayer.reset();
		g_gameCore->ClearPrompt();
		g_gameCore->ClearHealthBar();
		g_gameCore->ClearLightningFlashes();
		g_applicationUi.Deinitialize();
		ClearGameObjects();
		g_scene.reset();
		g_promptFont.reset();
		g_fontAnimations = {};
		g_hudAnimations.reset();
		g_freeCam.reset();
		g_camera.reset();
		g_gameCore.reset();

		AssetManager::Instance().Deinitialize();
		g_renderer.Deinitialize();
		g_fileManager.Deinitialize();

		g_renderTargetView.Reset();
		g_depthStencilView.Reset();
		g_swapChain.Reset();
		g_d3dContext.Reset();
		g_d3dDevice.Reset();
	}

	void Update()
	{
		g_profilingStats.m_duration_game = {};
		std::chrono::duration<double> duration = std::chrono::steady_clock::now() - g_startTime;
		g_currentTime = duration.count();
		const double deltaTime = g_currentTime - g_previousTime;
		g_previousTime = g_currentTime;

		const bool hasFocus = GetForegroundWindow() == g_hWnd;
		auto IsKeyPressed = [hasFocus](int key) { return hasFocus && (GetAsyncKeyState(key) & 0x8000) != 0; };

		GameInput input;
		if (!g_useFreeCam)
		{
			input.forward = IsKeyPressed('W') || IsKeyPressed(VK_UP);
			input.backward = IsKeyPressed('S') || IsKeyPressed(VK_DOWN);
			input.strafeLeft = IsKeyPressed('A');
			input.strafeRight = IsKeyPressed('D');
			input.turnLeft = IsKeyPressed(VK_LEFT);
			input.turnRight = IsKeyPressed(VK_RIGHT);
			input.lookUp = IsKeyPressed(VK_PRIOR);
			input.lookDown = IsKeyPressed(VK_NEXT);
			input.centerLook = IsKeyPressed(VK_HOME);
			input.attack = IsKeyPressed(VK_CONTROL) || IsKeyPressed(VK_LBUTTON);
			input.selectDagger = IsKeyPressed('1');
			input.selectSword = IsKeyPressed('2');
			input.selectScythe = IsKeyPressed('9');
			input.selectLentils = IsKeyPressed('0');
			for (size_t weapon = 0; weapon < input.selectOtherWeapons.size(); ++weapon)
			{
				input.selectOtherWeapons[weapon] = IsKeyPressed('3' + static_cast<int>(weapon));
			}
			input.jump = IsKeyPressed(VK_SPACE);
			input.crouch = IsKeyPressed('C');
		}
		input.interaction = IsKeyPressed(VK_RETURN);
		input.mainMenu = IsKeyPressed(VK_F1);
		input.inventory = IsKeyPressed(VK_F2);
		input.uiUp = IsKeyPressed(VK_UP);
		input.uiDown = IsKeyPressed(VK_DOWN);
		input.uiLeft = IsKeyPressed(VK_LEFT);
		input.uiRight = IsKeyPressed(VK_RIGHT);
		input.uiConfirm = IsKeyPressed(VK_RETURN);
		input.uiCancel = IsKeyPressed(VK_ESCAPE);
		input.debugNextLevel = IsKeyPressed(VK_TAB) && !IsKeyPressed(VK_SHIFT);
		input.debugNextGamePart = IsKeyPressed(VK_TAB) && IsKeyPressed(VK_SHIFT);

		Desperabis::ApplicationUpdateResult application;
		const auto levelBeforeUpdate = g_gameCore->GetLoadedLevelNumber();
		try
		{
			{
				ProfilingScope _(g_profilingStats.m_duration_game);
				application = g_gameCore->UpdateApplication(deltaTime, input);
			}
			if (g_gameCore->GetLoadedLevelNumber() != levelBeforeUpdate)
			{
				if (g_gameCore->GetRuntimeState() == Desperabis::GameRuntimeState::Ready)
				{
					SyncGameObjects();
					g_gameCore->UpdateHealthBar();
				}
				else
				{
					g_gameCore->ClearPrompt();
					g_gameCore->ClearHealthBar();
				}
			}
			if (application.tick && application.tick->status == Desperabis::GameTickStatus::Advanced)
			{
				const auto& result = *application.tick;
				g_gameCore->UpdateUserMessage(application.timing.frameTicks);
				SyncGameObjects();
				if (input.attack && result.weapon &&
					result.weapon->attack == Desperabis::WeaponActionStatus::UnimplementedWeapon)
				{
					SetWindowTextA(g_hWnd, "Desperabis (DX11) - this weapon's attack is not recovered yet");
				}
				if (input.interaction && result.interaction)
				{
					const std::string title = "Desperabis (DX11) - " + DescribeInteraction(*result.interaction);
					SetWindowTextA(g_hWnd, title.c_str());
				}
			}
		}
		catch (const std::exception& error)
		{
			application = {};
			ReportGameCoreError("application update failed", error.what());
			const std::string title = std::string("Desperabis (DX11) - ") + error.what();
			SetWindowTextA(g_hWnd, title.c_str());
		}
		g_worldTime += application.worldDeltaSeconds;
		if (application.worldDeltaSeconds > 0) { g_scene->Update(application.worldDeltaSeconds); }

		if (!g_useFreeCam && g_gameCore->GetRuntimeState() == Desperabis::GameRuntimeState::Ready)
		{
			ApplyPlayerCamera();
			return;
		}
		if (application.worldDeltaSeconds == 0) { return; }

		// free camera input
		float forward = 0.f;
		float sideways = 0.f;
		if (hasFocus)
		{
			POINT p;
			if (GetCursorPos(&p) && ScreenToClient(g_hWnd, &p))
			{
				g_freeCam->SetMousePos((float)p.x, (float)p.y);
			}

			const float speedMultiplier = IsKeyPressed(VK_SHIFT) ? 3.f : 1.f;
			if (IsKeyPressed('W')) forward += speedMultiplier;
			if (IsKeyPressed('S')) forward -= speedMultiplier;
			if (IsKeyPressed('A')) sideways -= speedMultiplier;
			if (IsKeyPressed('D')) sideways += speedMultiplier;
		}

		Level* level = g_scene->IsReady() ? g_scene->m_level : nullptr;
		g_freeCam->SetSpeed(forward, sideways);
		g_freeCam->Update(application.worldDeltaSeconds, level);
	}

	void Render()
	{
		const auto presentation = g_gameCore->GetApplicationPresentation();
		g_renderer.ReInitialize(g_renderTargetView.Get(), g_depthStencilView.Get(), g_viewport);

		g_d3dContext->RSSetViewports(1, &g_viewport);
		ID3D11RenderTargetView* const targets[1] = { g_renderTargetView.Get() };
		g_d3dContext->OMSetRenderTargets(1, targets, g_depthStencilView.Get());

		Vector4 clearCol = { 0.05f, 0.06f, 0.08f, 1.f };
		if (presentation.commands.empty() && g_scene->IsReady())
		{
			clearCol = g_scene->GetClearColor();
		}
		clearCol.x = powf(clearCol.x, g_renderSettings.gamma);
		clearCol.y = powf(clearCol.y, g_renderSettings.gamma);
		clearCol.z = powf(clearCol.z, g_renderSettings.gamma);
		g_d3dContext->ClearRenderTargetView(g_renderTargetView.Get(), &clearCol.x);
		g_d3dContext->ClearDepthStencilView(g_depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

		Renderer::Instance().SetCamera(g_camera);
		Renderer::Instance().SetCurrentTime(g_worldTime);
		if (presentation.commands.empty())
		{
			UpdateItemPreview();
			Renderer::Instance().RenderScene(g_scene.get(), g_renderSettings, g_visSettings, &g_profilingStats);
		}
		else
		{
			g_renderer.SetItemPreview(nullptr, false, {});
			VisibilityResult elements;
			elements.m_mainCamera = g_camera.get();
			g_renderer.RenderSceneElements(elements, g_renderSettings, g_profilingStats);
			g_applicationUi.Draw(presentation, g_renderer, *g_scene->m_level, *g_promptFont, *g_hudAnimations,
				g_renderSettings, g_profilingStats, g_viewport, g_d3dContext.Get(),
				g_renderTargetView.Get(), g_depthStencilView.Get());
		}
		g_profilingStats.Update();

		g_swapChain->Present(1, 0);
	}

	LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		switch (message)
		{
		case WM_SIZE:
			if (g_d3dDevice != nullptr && wParam != SIZE_MINIMIZED)
			{
				CreateWindowSizeDependentResources();
				UpdateProjection();
			}
			return 0;
		case WM_KEYDOWN:
			if (wParam == 'F' && (lParam & (1 << 30)) == 0 && g_initialized &&
				g_gameCore->GetMenuPage() == Desperabis::MenuPage::Gameplay)
			{
				ToggleFreeCamera();
			}
			else if (wParam == 'N' && (lParam & (1 << 30)) == 0 && g_initialized &&
				g_gameCore->GetMenuPage() == Desperabis::MenuPage::Gameplay)
			{
				g_gameCore->DEBUG_noPlayerCollision = !g_gameCore->DEBUG_noPlayerCollision;
				g_gameCore->PromptUserMessage(g_gameCore->DEBUG_noPlayerCollision ?
					"Player collision: off" : "Player collision: on", 90);
			}
			return 0;
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int nCmdShow)
{
	WNDCLASSEXW wcex = {};
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.hInstance = hInstance;
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.lpszClassName = g_windowClass;
	RegisterClassExW(&wcex);

	RECT rect = { 0, 0, g_initialWidth, g_initialHeight };
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
	g_hWnd = CreateWindowW(g_windowClass, g_windowTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top,
		nullptr, nullptr, hInstance, nullptr);
	if (g_hWnd == nullptr)
	{
		return 1;
	}

	ShowWindow(g_hWnd, nCmdShow);
	UpdateWindow(g_hWnd);

	if (!InitDX())
	{
		MessageBoxW(g_hWnd, L"Failed to create a D3D11 device.", g_windowTitle, MB_ICONERROR);
		return 1;
	}
	try
	{
		FORCE_WAIT(InitDesperabis());
	}
	catch (const std::exception& error)
	{
		MessageBoxA(g_hWnd, error.what(), "Desperabis initialization failed", MB_ICONERROR);
		return 1;
	}

	MSG msg = {};
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			continue;
		}

		if (g_initialized && !IsIconic(g_hWnd))
		{
			try
			{
				Update();
				if (g_gameCore->GetMenuPage() == Desperabis::MenuPage::Quit) { DestroyWindow(g_hWnd); }
				else { Render(); }
			}
			catch (const std::exception& error)
			{
				ReportGameCoreError("frame failed", error.what());
				MessageBoxA(g_hWnd, error.what(), "Desperabis frame failed", MB_ICONERROR);
				DestroyWindow(g_hWnd);
			}
		}
		else
		{
			WaitMessage();
		}
	}

	DeinitDesperabis();
	return (int)msg.wParam;
}
