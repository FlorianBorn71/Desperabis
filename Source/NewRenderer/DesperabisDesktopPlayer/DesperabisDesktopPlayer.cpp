// DesperabisRemotePlayer.cpp : Defines the entry point for the application.
//

#include "pch.h"
#include "framework.h"
#include "DesperabisDesktopPlayer.h"

#include <DesperabisCommon.h>
#include <Asset/AssetManager.h>
#include <DX11/RendererDX11.h>
#include <IO/FileManagerBuiltIn.h>
#include <IO/BuiltInCppGenerator.h>
#include <Sample/FreeCamera.h>
#include <Sample/LevelScene.h>
#include <Sample/RemoteScene.h>
#include <Renderer/GlobalSettings.h>

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name


namespace // DX
{
    HWND hWnd = NULL;
    ComPtr<ID3D11Device3> g_d3dDevice;
    ComPtr<ID3D11DeviceContext3> g_d3dContext;
    ComPtr<IDXGISwapChain1> g_swapChain;
    ComPtr<ID3D11RenderTargetView1> g_d3dRenderTargetView;
    ComPtr<ID3D11DepthStencilView> g_d3dDepthStencilView;
    int g_outputResX = 0;
    int g_outputResY = 0;
    D3D11_VIEWPORT g_screenViewport = {};
    D3D_FEATURE_LEVEL g_d3dFeatureLevel = D3D_FEATURE_LEVEL_1_0_CORE;

    // DESP
    RendererDX11 g_RendererDX11;
    FileManagerBuiltIn g_FileManager;
    NullPhysicsModule g_Physics;
    ProfilingStats g_profilingStats;
    std::chrono::steady_clock::time_point g_startTime;
	const float g_globalScale = 1.f;
	double g_currentTime = 0.0;
    double g_previousTime = 0.0;
    int g_currentLevelMenuId = 0;
    bool g_timerRestart = true;
    bool g_connectToDOS = false;
    bool g_cameraGravity = false;

    enum class CameraMode
    {
        FreeCamera,
        FirstPerson,
        ThirdPerson,
        COUNT
    };

    std::unique_ptr<RemoteScene> g_desperabisScene;
    std::unique_ptr<FreeCamera> g_freeCam;
    CameraMode g_useFreeCam = CameraMode::FreeCamera;


    void SetCameraMode(CameraMode mode)
    {
        g_useFreeCam = mode;
        switch (g_useFreeCam)
        {
        case CameraMode::FreeCamera:
            g_desperabisScene->SetUseFirstPersonCam(false);
            break;
        case CameraMode::FirstPerson:
            g_desperabisScene->SetUseFirstPersonCam(true);
            break;
        case CameraMode::ThirdPerson:
            g_desperabisScene->SetUseFirstPersonCam(true, 2.5f);
            break;
        }

        MENUITEMINFO menuItem = { 0 };
        menuItem.cbSize = sizeof(MENUITEMINFO);
        menuItem.fMask = MIIM_STATE;
        HMENU menu = GetMenu(hWnd);
        GetMenuItemInfo(menu, ID_CAMERA_FREECAMERA, FALSE, &menuItem);

        auto SetModeCheckmark = [&](int id, CameraMode mode)
        {
            menuItem.fState = (mode == g_useFreeCam) ? MFS_CHECKED : MFS_UNCHECKED;
            SetMenuItemInfo(menu, id, FALSE, &menuItem);
        };

        SetModeCheckmark(ID_CAMERA_FREECAMERA, CameraMode::FreeCamera);
        SetModeCheckmark(ID_CAMERA_FIRSTPERSONCAMERA, CameraMode::FirstPerson);
        SetModeCheckmark(ID_CAMERA_THIRDPERSONCAMERA, CameraMode::ThirdPerson);
    }

    void UpdateGlobalSettings()
    {
		GlobalSettings& settings = GlobalSettings::Instance();
        MENUITEMINFO menuItem = { 0 };
		menuItem.cbSize = sizeof(MENUITEMINFO);
		menuItem.fMask = MIIM_STATE;
		HMENU menu = GetMenu(hWnd);
		GetMenuItemInfo(menu, ID_RENDERER_ORIGINALASSETS, FALSE, &menuItem);

		auto SetCheckState = [&](int id, bool flag)
			{
				menuItem.fState = flag ? MFS_CHECKED : MFS_UNCHECKED;
				SetMenuItemInfo(menu, id, FALSE, &menuItem);
			};
        auto SetQualityCheckmark = [&](int id, GlobalSettings::AssetQuality mode)
			{
                SetCheckState(id, mode == GlobalSettings::Instance().assetQuality);
			};


		SetQualityCheckmark(ID_RENDERER_ORIGINALASSETS, GlobalSettings::AssetQuality::Original);
		SetQualityCheckmark(ID_RENDERER_WITHHIGHRES, GlobalSettings::AssetQuality::WithHighRes);
        SetQualityCheckmark(ID_RENDERER_ONLYHIGHRES, GlobalSettings::AssetQuality::OnlyHighRes);

        SetCheckState(ID_CAMERA_GRAVITY, g_cameraGravity);
		SetCheckState(ID_RENDERER_USEFOG, settings.useFog);
		SetCheckState(ID_RENDERER_FULLBRIGHT, settings.fullbrightRendering);
		SetCheckState(ID_RENDERER_DYNAMICLIGHTS, settings.dynamicLights);
		SetCheckState(ID_RENDERER_CORONAS, settings.renderCoronas);
        SetCheckState(ID_RENDERER_GRIDVISIBILITY, settings.gridVisibility);
        SetCheckState(ID_REMOTECONNECTION_CONNECT, g_connectToDOS);
    }

    struct LevelMapping
    {
        int menuId;
        int levelIndex;
    };

    /*
        Training
        50
        The Crystal Ball
        29,30,31,32,33,36,34,35
        The Rescue Of Grant
        9,10,11,12,13,14,15,16,17,18,90,19,20,21,22,23,24,97,25,26,END
    */
    static const LevelMapping g_levelMapping[] =
    {
		{ID_LEVEL_TRAININGSLEVEL, 50},
		{ID_LEVEL_PART1_1, 30},
		{ID_LEVEL_PART1_2, 31},
		{ID_LEVEL_PART1_3, 32},
		{ID_LEVEL_PART1_4, 33},
		{ID_LEVEL_PART1_5, 36},
		{ID_LEVEL_PART1_6, 34},
		{ID_LEVEL_PART1_7, 35},

		{ID_LEVEL_PART2_1, 10},
        {ID_LEVEL_PART2_2, 11},
		{ID_LEVEL_PART2_3, 12},
		{ID_LEVEL_PART2_4, 13},
		{ID_LEVEL_PART2_5, 14},
		{ID_LEVEL_PART2_6, 15},
		{ID_LEVEL_PART2_7, 16},
		{ID_LEVEL_PART2_8, 17},
		{ID_LEVEL_PART2_9, 18},
		{ID_LEVEL_PART2_10, 90},
		{ID_LEVEL_PART2_11, 19},
		{ID_LEVEL_PART2_12, 20},
		{ID_LEVEL_PART2_13, 21},
		{ID_LEVEL_PART2_14, 22},
		{ID_LEVEL_PART2_15, 23},
		{ID_LEVEL_PART2_16, 24},
		{ID_LEVEL_PART2_17, 25},
		{ID_LEVEL_PART2_18, 26},
    };

	void LoadLevel(int menuId)
	{
        g_currentLevelMenuId = menuId;
		MENUITEMINFO menuItem = { 0 };
		menuItem.cbSize = sizeof(MENUITEMINFO);
		menuItem.fMask = MIIM_STATE;
		HMENU menu = GetMenu(hWnd);
		GetMenuItemInfo(menu, ID_LEVEL_TRAININGSLEVEL, FALSE, &menuItem);

        for (const LevelMapping& mapping : g_levelMapping)
        {
            if (mapping.menuId == menuId)
            {
				g_desperabisScene = std::make_unique<RemoteScene>(mapping.levelIndex);
                g_desperabisScene->m_createGameObjects = !g_connectToDOS;
				CO_AWAIT g_desperabisScene->CreateScene();
				SetCameraMode(g_useFreeCam);
				g_freeCam->JumpTo(g_desperabisScene->GetInitialPosition());
                menuItem.fState = MFS_CHECKED;
            }
            else
            {
				menuItem.fState = MFS_UNCHECKED;
            }
			SetMenuItemInfo(menu, mapping.menuId, FALSE, &menuItem);
        }

	}


    void RunSimulation()
    {
        // timer update
        std::chrono::steady_clock::time_point now = std::chrono::high_resolution_clock::now();
        if (g_timerRestart)
        {
            g_startTime = now;
            g_timerRestart = false;
        }
        std::chrono::duration<double> duration = now - g_startTime;
        g_currentTime = duration.count();
        double deltaTime = g_currentTime - g_previousTime;
        g_previousTime = g_currentTime;

        // update scene and camera
        if (g_desperabisScene != nullptr)
        {
            g_desperabisScene->Update(deltaTime);
        }

        if (g_useFreeCam == CameraMode::FreeCamera && g_freeCam != nullptr)
        {
            // mouse and keyboard input
            POINT p;
            float forward = 0.f;
            float sideways = 0.f;
            float speedMultiplier = 1.f;
            auto IsKeyPressed = [](int key) -> bool
            {
                SHORT keyState = GetAsyncKeyState(key);
                return (keyState & 0x8000) > 0;
            };

            if (GetCursorPos(&p))
            {
                ScreenToClient(hWnd, &p);
                g_freeCam->SetMousePos((float)p.x, (float)p.y);
            }
            if (IsKeyPressed(VK_SHIFT))
            {
                speedMultiplier = 3.f;
            }

            if (IsKeyPressed('W'))
            {
                forward += 1.0f;
            }
            if (IsKeyPressed('S'))
            {
                forward -= 1.0f;
            }
            if (IsKeyPressed('A'))
            {
                sideways -= 1.0f;
            }
            if (IsKeyPressed('D'))
            {
                sideways += 1.0f;
            }

            Level* level = (g_desperabisScene!=nullptr && g_desperabisScene->IsReady()) ? g_desperabisScene->m_level : nullptr;
            g_freeCam->SetUseGravity(g_cameraGravity);
            g_freeCam->SetSpeed(forward * speedMultiplier, sideways * speedMultiplier);
            g_freeCam->Update(deltaTime, level);
        }

    }

    FUTURE(void) Desp_Init()
    {
		GlobalSettings::SetRelativeGeometryScaling(g_globalScale);

        g_FileManager.Initialize();
        g_FileManager.AddExternalDataDir("D:\\privat\\repos\\SheetMusicOverlay\\Desperabis\\DeperabisBrowser\\Experimental\\AI.Nero");

		GlobalSettings& settings = GlobalSettings::Instance();
        settings.gridVisibility = false;

        // we need the renderer first...
        CO_AWAIT g_RendererDX11.Initialize(
            g_d3dDevice.Get(), g_d3dContext.Get(), 
            g_d3dRenderTargetView.Get(),
            g_d3dDepthStencilView.Get(),
            g_screenViewport);

        // ...before we can load assets
        CO_AWAIT AssetManager::Instance().Initialize();

        g_freeCam = std::make_unique<FreeCamera>();

		LoadLevel(ID_LEVEL_TRAININGSLEVEL);

        if (Camera* cam = Renderer::Instance().GetOrCreateCamera().get())
        {
            Matrix4 projMat;
            float aspectRt = g_screenViewport.Width / g_screenViewport.Height;
            projMat.CreateProjection(aspectRt);
            cam->SetProjectionMatrix(projMat);
        }
        if (g_freeCam != nullptr)
        {
            g_freeCam->SetWindowCenter(g_screenViewport.Width * 0.5f, g_screenViewport.Height * 0.5f);
        }
    }

    void Desp_Deinit()
    {
        g_desperabisScene.reset();
        g_freeCam.reset();

        AssetManager::Instance().Deinitialize();
        g_RendererDX11.Deinitialize();
        g_FileManager.Deinitialize();
        
        g_d3dRenderTargetView.Reset();
        g_d3dDepthStencilView.Reset();
        g_d3dContext.Reset();
        g_swapChain.Reset();
        g_d3dDevice.Reset();

    }

    inline void ThrowIfFailed(HRESULT hr)
    {
        if (FAILED(hr))
        {
            // Set a breakpoint on this line to catch Win32 API errors.
            assert(false && "Operation failed");
        }
    }

    void HandleDeviceLost()
    {

    }

    void UpdateRenderTargetSize()
    {
        RECT winRect;
        GetClientRect(hWnd, &winRect);
        g_outputResX = winRect.right - winRect.left;
        g_outputResY = winRect.bottom - winRect.top;
    }

    void CreateDXWindowResources()
    {
        // Clear the previous window size specific context.
        ID3D11RenderTargetView* nullViews[] = { nullptr };
        g_d3dContext->OMSetRenderTargets(ARRAYSIZE(nullViews), nullViews, nullptr);
        g_d3dRenderTargetView = nullptr;
        g_d3dDepthStencilView = nullptr;
        g_d3dContext->Flush1(D3D11_CONTEXT_TYPE_ALL, nullptr);

        UpdateRenderTargetSize();

        if (g_swapChain != nullptr)
        {
            // If the swap chain already exists, resize it.
            HRESULT hr = g_swapChain->ResizeBuffers(
                2, // Double-buffered swap chain.
                g_outputResX, g_outputResY,
                DXGI_FORMAT_B8G8R8A8_UNORM,
                0
            );

            if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
            {
                // If the device was removed for any reason, a new device and swap chain will need to be created.
                HandleDeviceLost();

                // Everything is set up now. Do not continue execution of this method. HandleDeviceLost will reenter this method 
                // and correctly set up the new device.
                return;
            }
            else
            {
                ThrowIfFailed(hr);
            }
        }
        else
        {
            // This sequence obtains the DXGI factory that was used to create the Direct3D device above.
            ComPtr<IDXGIDevice3> dxgiDevice;
            ThrowIfFailed(
                g_d3dDevice.As(&dxgiDevice)
            );

            ComPtr<IDXGIAdapter> dxgiAdapter;
            ThrowIfFailed(
                dxgiDevice->GetAdapter(&dxgiAdapter)
            );

            ComPtr<IDXGIFactory4> dxgiFactory;
            ThrowIfFailed(
                dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory))
            );

            ComPtr<IDXGISwapChain1> swapChain;
            {
                // Otherwise, create a new one using the same adapter as the existing Direct3D device.
                DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { 0 };

                swapChainDesc.Width = g_outputResX;		// Match the size of the window.
                swapChainDesc.Height = g_outputResY;
                swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;				// This is the most common swap chain format.
                swapChainDesc.Stereo = false;
                swapChainDesc.SampleDesc.Count = 1;								// Don't use multi-sampling.
                swapChainDesc.SampleDesc.Quality = 0;
                swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                swapChainDesc.BufferCount = 2;									// Use double-buffering to minimize latency.
                swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;	// All Microsoft Store apps must use this SwapEffect.
                swapChainDesc.Flags = 0;
                swapChainDesc.Scaling = DXGI_SCALING_NONE;
                swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
                ThrowIfFailed(dxgiFactory->CreateSwapChainForHwnd(g_d3dDevice.Get(), hWnd, &swapChainDesc, nullptr, nullptr, &swapChain));
            }

            ThrowIfFailed(
                swapChain.As(&g_swapChain)
            );

            // Ensure that DXGI does not queue more than one frame at a time. This both reduces latency and
            // ensures that the application will only render after each VSync, minimizing power consumption.
            ThrowIfFailed(
                dxgiDevice->SetMaximumFrameLatency(1)
            );
        }

        
        // Create a render target view of the swap chain back buffer.
        ComPtr<ID3D11Texture2D1> backBuffer;
        ThrowIfFailed(
            g_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))
        );

        ThrowIfFailed(
            g_d3dDevice->CreateRenderTargetView1(
                backBuffer.Get(),
                nullptr,
                &g_d3dRenderTargetView
            )
        );

        // Create a depth stencil view for use with 3D rendering if needed.
        CD3D11_TEXTURE2D_DESC1 depthStencilDesc(
            DXGI_FORMAT_D24_UNORM_S8_UINT,
            g_outputResX,
            g_outputResY,
            1, // This depth stencil view has only one texture.
            1, // Use a single mipmap level.
            D3D11_BIND_DEPTH_STENCIL
        );

        // Desperabis: Used for corona readback:
        depthStencilDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

        ComPtr<ID3D11Texture2D1> depthStencil;
        ThrowIfFailed(
            g_d3dDevice->CreateTexture2D1(
                &depthStencilDesc,
                nullptr,
                &depthStencil
            )
        );

        CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc(D3D11_DSV_DIMENSION_TEXTURE2D);
        depthStencilViewDesc.Format = DXGI_FORMAT_D32_FLOAT;

        ThrowIfFailed(
            g_d3dDevice->CreateDepthStencilView(
                depthStencil.Get(),
                &depthStencilViewDesc,
                &g_d3dDepthStencilView
            )
        );

        // Set the 3D rendering viewport to target the entire window.
        g_screenViewport = CD3D11_VIEWPORT(
            0.0f,
            0.0f,
            (float)g_outputResX,
            (float)g_outputResY
        );

        g_d3dContext->RSSetViewports(1, &g_screenViewport);
    }

    void InitDX(HWND hwnd)
    {
        hWnd = hwnd;

        // This flag adds support for surfaces with a different color channel ordering
        // than the API default. It is required for compatibility with Direct2D.
        UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#if defined(_DEBUG)
        // If the project is in a debug build, enable debugging via SDK Layers with this flag.
        creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        // This array defines the set of DirectX hardware feature levels this app will support.
        // Note the ordering should be preserved.
        // Don't forget to declare your application's minimum required feature level in its
        // description.  All applications are assumed to support 9.1 unless otherwise stated.
        D3D_FEATURE_LEVEL featureLevels[] =
        {
            D3D_FEATURE_LEVEL_12_1,
            D3D_FEATURE_LEVEL_12_0,
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
            D3D_FEATURE_LEVEL_9_3,
            D3D_FEATURE_LEVEL_9_2,
            D3D_FEATURE_LEVEL_9_1
        };

        // Create the Direct3D 11 API device object and a corresponding context.
        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> context;

        HRESULT hr = D3D11CreateDevice(
            nullptr,					// Specify nullptr to use the default adapter.
            D3D_DRIVER_TYPE_HARDWARE,	// Create a device using the hardware graphics driver.
            0,							// Should be 0 unless the driver is D3D_DRIVER_TYPE_SOFTWARE.
            creationFlags,				// Set debug and Direct2D compatibility flags.
            featureLevels,				// List of feature levels this app can support.
            ARRAYSIZE(featureLevels),	// Size of the list above.
            D3D11_SDK_VERSION,			// Always set this to D3D11_SDK_VERSION for Microsoft Store apps.
            &device,					// Returns the Direct3D device created.
            &g_d3dFeatureLevel,			// Returns feature level of device created.
            &context					// Returns the device immediate context.
        );

        if (FAILED(hr))
        {
            // If the initialization fails, fall back to the WARP device.
            // For more information on WARP, see: 
            // https://go.microsoft.com/fwlink/?LinkId=286690
            ThrowIfFailed(
                D3D11CreateDevice(
                    nullptr,
                    D3D_DRIVER_TYPE_WARP, // Create a WARP device instead of a hardware device.
                    0,
                    creationFlags,
                    featureLevels,
                    ARRAYSIZE(featureLevels),
                    D3D11_SDK_VERSION,
                    &device,
                    &g_d3dFeatureLevel,
                    &context
                )
            );
        }

        // Store pointers to the Direct3D 11.3 API device and immediate context.
        ThrowIfFailed(
            device.As(&g_d3dDevice)
        );

        ThrowIfFailed(
            context.As(&g_d3dContext)
        );

        // Enable multi thread protection as now multiple threads use the immediate context.
        ComPtr<ID3D11Multithread> contextMultithread;
        if (context.As(&contextMultithread) == S_OK)
        {
            contextMultithread->SetMultithreadProtected(true);
        }

        // Create the Direct2D device object and a corresponding context.
        ComPtr<IDXGIDevice3> dxgiDevice;
        ThrowIfFailed(
            g_d3dDevice.As(&dxgiDevice)
        );

        CreateDXWindowResources();
    }

    void RenderDX()
    {
        g_d3dContext->RSSetViewports(1, &g_screenViewport);
        ID3D11RenderTargetView* const targets[1] = { g_d3dRenderTargetView.Get() };
        g_d3dContext->OMSetRenderTargets(1, targets, g_d3dDepthStencilView.Get());

        const GlobalSettings& settings = GlobalSettings::Instance();

        // clear
		Vector4 clearCol = { 0.05f, 0.06f, 0.08f, 1.f };
		if (g_desperabisScene != nullptr && g_desperabisScene->IsReady())
		{
			clearCol = g_desperabisScene->GetClearColor();
		}
		clearCol.x = powf(clearCol.x, settings.gamma);
		clearCol.y = powf(clearCol.y, settings.gamma);
		clearCol.z = powf(clearCol.z, settings.gamma);
        g_d3dContext->ClearRenderTargetView(g_d3dRenderTargetView.Get(), &clearCol.x);
        g_d3dContext->ClearDepthStencilView(g_d3dDepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

        // render desp scene:
        Renderer::Instance().SetCurrentTime(g_currentTime);
        Renderer::Instance().RenderScene(g_desperabisScene.get(), &g_profilingStats);
        g_profilingStats.Update();

        g_swapChain->Present(1, 0);
    }
}


// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_DESPERABISREMOTEPLAYER, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_DESPERABISREMOTEPLAYER));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        RunSimulation();
        RenderDX();
    }

    Desp_Deinit();

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DESPERABISREMOTEPLAYER));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_DESPERABISREMOTEPLAYER);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);
   InitDX(hWnd);
   Desp_Init();
   SetCameraMode(CameraMode::FreeCamera);
   UpdateGlobalSettings();
   
   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	GlobalSettings& settings = GlobalSettings::Instance();
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            if (wmId >= ID_LEVEL_FIRST && wmId <= ID_LEVEL_LAST)
            {
                LoadLevel(wmId);
                break;
            }

            // Parse the menu selections:
            switch (wmId)
            {
//            case IDM_ABOUT:
//                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
//                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            case ID_CAMERA_FREECAMERA:
                SetCameraMode(CameraMode::FreeCamera);
                break;
            case ID_CAMERA_FIRSTPERSONCAMERA:
                SetCameraMode(CameraMode::FirstPerson);
                break;
            case ID_CAMERA_THIRDPERSONCAMERA:
                SetCameraMode(CameraMode::ThirdPerson);
                break;
            case ID_CAMERA_TOGGLECAMERA:
                SetCameraMode((CameraMode)(((int)g_useFreeCam+1) % (int)CameraMode::COUNT));
                break;
            case ID_CAMERA_GRAVITY:
                g_cameraGravity = !g_cameraGravity;
				UpdateGlobalSettings();
				break;
            case ID_GAME_CREATESNAPSHOT:
                RemoteScene::ScheduleSnapshot();
                break;
            case ID_RENDERER_ORIGINALASSETS:
                settings.assetQuality = GlobalSettings::AssetQuality::Original;
                UpdateGlobalSettings();
                break;
            case ID_RENDERER_WITHHIGHRES:
				settings.assetQuality = GlobalSettings::AssetQuality::WithHighRes;
				UpdateGlobalSettings();
				break;
			case ID_RENDERER_ONLYHIGHRES:
				settings.assetQuality = GlobalSettings::AssetQuality::OnlyHighRes;
				UpdateGlobalSettings();
				break;
            case ID_RENDERER_TOGGLEASSETMODE:
				settings.assetQuality = (settings.assetQuality== GlobalSettings::AssetQuality::Original) ? GlobalSettings::AssetQuality::WithHighRes : GlobalSettings::AssetQuality::Original;
				UpdateGlobalSettings();
				break;
            case ID_RENDERER_USEFOG:
                settings.useFog = !settings.useFog;
				UpdateGlobalSettings();
				break;
			case ID_RENDERER_FULLBRIGHT:
				settings.fullbrightRendering = !settings.fullbrightRendering;
				UpdateGlobalSettings();
				break;
			case ID_RENDERER_DYNAMICLIGHTS:
				settings.dynamicLights = !settings.dynamicLights;
				UpdateGlobalSettings();
				break;
			case ID_RENDERER_CORONAS:
				settings.renderCoronas = !settings.renderCoronas;
				UpdateGlobalSettings();
				break;
			case ID_RENDERER_GRIDVISIBILITY:
				settings.gridVisibility = !settings.gridVisibility;
				UpdateGlobalSettings();
				break;
            case ID_REMOTECONNECTION_CONNECT:
                g_connectToDOS = !g_connectToDOS;
                LoadLevel(g_currentLevelMenuId);
                UpdateGlobalSettings();
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
        /*
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: Add any drawing code that uses hdc here...
            EndPaint(hWnd, &ps);
            */
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
