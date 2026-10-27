// Desperabis-Game-DX11: minimal DX11 window that loads a level and renders it.
// Game objects are owned and simulated by Desperabis-GameCore (GameContext); the renderer only
// presents the world state after each simulation step and follows the player camera.

#include "pch.h"
#include "GameCoreHost.h"

#include <DesperabisCommon.h>
#include <Asset/AssetManager.h>
#include <Asset/Level.h>
#include <DX11/RendererDX11.h>
#include <IO/FileManagerBuiltIn.h>
#include <Renderer/RenderableInstance.h>
#include <Sample/FreeCamera.h>
#include <Sample/LevelScene.h>
#include <Renderer/Camera.h>
#include <Renderer/RenderingSettings.h>
#include <Renderer/VisibilitySettings.h>
#include <GamePlay/GamePlay.h>

namespace
{
	constexpr int g_startLevelIndex = 11;

	// The host owns wall-clock conversion; the original game runs at 33 Hz.
	constexpr double g_gameTicksPerSecond = 33.0;
	// Cap catch-up after stalls to one second of game time.
	constexpr int g_maxTicksPerFrame = 33;
	// OBJPOS has no player record, so the initial view vector is a host choice (faces into level 11's start corridor).
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
	std::unique_ptr<LevelScene> g_scene;
	std::unique_ptr<FreeCamera> g_freeCam;
	std::shared_ptr<Camera> g_camera;
	std::unique_ptr<GameCoreHost> g_gameCore;
	bool g_initialized = false;

	// presentation of the game world, indexed by native slot
	RenderableInstancePtr g_objectInst[GameCoreHost::NumSlots];
	RawGameObject g_objectState[GameCoreHost::NumSlots] = {};
	RawGameObject g_presentedState[GameCoreHost::NumSlots] = {};
	bool g_objectBehaviorCreated[GameCoreHost::NumSlots] = {};
	double g_tickAccumulator = 0.0;
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

	void CreateWindowSizeDependentResources()
	{
		ID3D11RenderTargetView* nullViews[] = { nullptr };
		g_d3dContext->OMSetRenderTargets(ARRAYSIZE(nullViews), nullViews, nullptr);
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
#if defined(D_ASYNC)
		return fileManager.ReadDataFuture(path).get();
#else
		return fileManager.ReadDataFuture(path);
#endif
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

	void ReportGameCoreError(const char* context)
	{
		std::string msg = std::string("GameCore: ") + context + ": " + g_gameCore->GetLastError() + "\n";
		OutputDebugStringA(msg.c_str());
	}

	void ClearGameObjects()
	{
		for (int slot = 0; slot < GameCoreHost::NumSlots; slot++)
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

	// Copies the world state out of the game context and updates the renderables of changed slots.
	void SyncGameObjects()
	{
		Level* level = g_scene->m_level;
		for (int slot = GameCoreHost::FirstObjectSlot; slot <= GameCoreHost::LastObjectSlot; slot++)
		{
			RawGameObject& state = g_objectState[slot];
			state.m_level = level;
			state.m_objectNumber = slot;
			g_gameCore->GetObjectBytes(slot, state.m_rawBytes);
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
			level->ApplyRenderProperties(inst, state);

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

	void LoadGameLevel(int levelIndex)
	{
		ClearGameObjects();
		g_tickAccumulator = 0.0;

		GameCoreLevelSource source;
		source.levelNumber = levelIndex;

		// GameCore selects the native map start; the initial view direction remains a host choice.
		source.playerDirection = g_initialPlayerDirection;

		if (!g_gameCore->LoadLevel(source))
		{
			ReportGameCoreError("level load failed");
			g_useFreeCam = true;
			return;
		}
		SyncGameObjects();
	}

	void ApplyPlayerCamera()
	{
		const GameCorePlayerCamera playerCam = g_gameCore->GetPlayerCamera();
		const Vector pos = Level::DespToPos(playerCam.position[0], playerCam.position[1], playerCam.position[2]);

		// a zero direction is not a valid orientation: keep the previous one
		const auto& dir = playerCam.direction;
		if (dir[0] != 0 || dir[1] != 0 || dir[2] != 0)
		{
			Vector forward(-(float)dir[0], (float)dir[1], (float)dir[2], 0.f);
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
		if (!g_gameCore->IsLevelReady())
		{
			return;
		}
		g_useFreeCam = !g_useFreeCam;
		if (g_useFreeCam)
		{
			g_freeCam->JumpTo(g_camera->GetCameraPosition());
		}
	}

	void InitDesperabis()
	{
		RenderingSettings::SetRelativeGeometryScaling(1.f);
		g_fileManager.Initialize();
		g_renderSettings.gridVisibility = false;

		// the renderer must exist before assets can be loaded
		FORCE_WAIT(g_renderer.Initialize(g_d3dDevice.Get(), g_d3dContext.Get(),
			g_renderTargetView.Get(), g_depthStencilView.Get(), g_viewport));
		FORCE_WAIT(AssetManager::Instance().Initialize());

		g_gameCore = std::make_unique<GameCoreHost>([](std::string_view filename)
		{
			return ReadGameFile(std::wstring(filename.begin(), filename.end()));
		});

		g_freeCam = std::make_unique<FreeCamera>();
		g_freeCam->SetUseGravity(false);

		// static level presentation only; game objects come from the game context
		g_scene = std::make_unique<LevelScene>(g_startLevelIndex);
		g_scene->m_createGameObjects = false;
		g_scene->m_createMapIcons = false;
		FORCE_WAIT(g_scene->CreateScene());
		g_freeCam->JumpTo(g_scene->GetInitialPosition());

		g_camera = Renderer::Instance().GetOrCreateCamera();
		UpdateProjection();

		LoadGameLevel(g_startLevelIndex);

		g_startTime = std::chrono::steady_clock::now();
		g_initialized = true;
	}

	void DeinitDesperabis()
	{
		g_initialized = false;
		ClearGameObjects();
		g_scene.reset();
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
		std::chrono::duration<double> duration = std::chrono::steady_clock::now() - g_startTime;
		g_currentTime = duration.count();
		const double deltaTime = g_currentTime - g_previousTime;
		g_previousTime = g_currentTime;

		g_scene->Update(deltaTime);

		const bool hasFocus = GetForegroundWindow() == g_hWnd;
		auto IsKeyPressed = [hasFocus](int key) { return hasFocus && (GetAsyncKeyState(key) & 0x8000) != 0; };

		// advance the game simulation and present the resulting world state
		if (g_gameCore->IsLevelReady())
		{
			GameCoreInput input;
			if (!g_useFreeCam)
			{
				input.forward = IsKeyPressed('W') || IsKeyPressed(VK_UP);
				input.backward = IsKeyPressed('S') || IsKeyPressed(VK_DOWN);
				input.strafeLeft = IsKeyPressed('A');
				input.strafeRight = IsKeyPressed('D');
				input.turnLeft = IsKeyPressed(VK_LEFT);
				input.turnRight = IsKeyPressed(VK_RIGHT);
			}
			input.interaction = IsKeyPressed(VK_RETURN);

			g_tickAccumulator += deltaTime * g_gameTicksPerSecond;
			const int ticks = (int)g_tickAccumulator;
			if (ticks > 0)
			{
				g_tickAccumulator -= ticks;
				const int clampedTicks = __min(ticks, g_maxTicksPerFrame);
				if (g_gameCore->Tick(clampedTicks, clampedTicks, input))
				{
					SyncGameObjects();
					if (input.interaction)
					{
						const std::string title = "Desperabis (DX11) - " + g_gameCore->GetLastInteractionMessage();
						SetWindowTextA(g_hWnd, title.c_str());
					}
				}
				else if (!g_gameCore->GetLastError().empty())
				{
					ReportGameCoreError("tick failed");
				}
			}
		}

		if (!g_useFreeCam)
		{
			ApplyPlayerCamera();
			return;
		}

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
		g_freeCam->Update(deltaTime, level);
	}

	void Render()
	{
		g_renderer.ReInitialize(g_renderTargetView.Get(), g_depthStencilView.Get(), g_viewport);

		g_d3dContext->RSSetViewports(1, &g_viewport);
		ID3D11RenderTargetView* const targets[1] = { g_renderTargetView.Get() };
		g_d3dContext->OMSetRenderTargets(1, targets, g_depthStencilView.Get());

		Vector4 clearCol = { 0.05f, 0.06f, 0.08f, 1.f };
		if (g_scene->IsReady())
		{
			clearCol = g_scene->GetClearColor();
		}
		clearCol.x = powf(clearCol.x, g_renderSettings.gamma);
		clearCol.y = powf(clearCol.y, g_renderSettings.gamma);
		clearCol.z = powf(clearCol.z, g_renderSettings.gamma);
		g_d3dContext->ClearRenderTargetView(g_renderTargetView.Get(), &clearCol.x);
		g_d3dContext->ClearDepthStencilView(g_depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

		Renderer::Instance().SetCamera(g_camera);
		Renderer::Instance().SetCurrentTime(g_currentTime);
		Renderer::Instance().RenderScene(g_scene.get(), g_renderSettings, g_visSettings, nullptr);

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
			if (wParam == VK_ESCAPE)
			{
				DestroyWindow(hWnd);
			}
			else if (wParam == 'F' && (lParam & (1 << 30)) == 0 && g_initialized)
			{
				ToggleFreeCamera();
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
	InitDesperabis();

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
			Update();
			Render();
		}
		else
		{
			WaitMessage();
		}
	}

	DeinitDesperabis();
	return (int)msg.wParam;
}
