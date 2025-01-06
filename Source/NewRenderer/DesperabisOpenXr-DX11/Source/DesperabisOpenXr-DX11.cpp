// DesperabisOpenXr-DX11.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <conio.h>
#include "pch.h"
#include "common.h"
#include "options.h"
#include "platformdata.h"
#include "platformplugin.h"
#include "graphicsplugin.h"
#include "openxr_program.h"

#include <DesperabisCommon.h>
#include <IO/FileManagerBuiltIn.h>
#include <Physics/PhysicsInterface.h>
#include <Renderer/GlobalSettings.h>
#include <Sample/LevelScene.h>

std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin_DesperabisD3D11(const std::shared_ptr<Options>& options, std::shared_ptr<IPlatformPlugin> platformPlugin);

FileManagerBuiltIn g_FileManager;
NullPhysicsModule g_Physics;
ProfilingStats g_profilingStats;
std::unique_ptr<Scene> g_desperabisScene;
std::chrono::steady_clock::time_point g_startTime;
//XrVector3f g_playerPos = { -3.35817671f,0,18.9653301f }; // g_InitialLevel = 24 (Swamp);
XrVector3f g_playerPos = { 64.2742615f, 0.00000000f,  62.5289841f }; // g_InitialLevel = 10;

// Important global tweakables
float g_globalScale = 7.f; // only startup
//float g_globalScale = 0.5f; // only startup
float g_gamma = 1.8f; // ...because the swap chain is SRGB. Could be changed any time (currently only startup)

double g_currentTime = 0.0;
double g_previousTime = 0.0;
bool g_timerRestart = true;
// 16: forest
// 17/18: cave
// 19: graveyard
// 20: Cathedral
// 21/22: Sewer
// 24: forest/swamp
// 25: burning forest
// 50: trainingslevel
int g_InitialLevel = 10;

namespace 
{
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
	}
}


int main()
{
	GlobalSettings::SetRelativeGeometryScaling(g_globalScale);
	GlobalSettings& settings = GlobalSettings::Instance();
	settings.gamma = g_gamma;

	try {
		// Parse command-line arguments into Options.
		std::shared_ptr<Options> options = std::make_shared<Options>();
		//if (!UpdateOptionsFromCommandLine(*options, argc, argv)) {
		//	return 1;
		//}
		options->GraphicsPlugin = "D3D11";

		std::shared_ptr<PlatformData> data = std::make_shared<PlatformData>();

		g_FileManager.Initialize();

		// Spawn a thread to wait for a keypress
		static bool quitKeyPressed = false;
		auto exitPollingThread = std::thread{ [] {
			Log::Write(Log::Level::Info, "Press any key to shutdown...");
			(void)getchar();
			quitKeyPressed = true;
		} };
		exitPollingThread.detach();

		bool requestRestart = false;
		do {
			// Create platform-specific implementation.
			std::shared_ptr<IPlatformPlugin> platformPlugin = CreatePlatformPlugin(options, data);

			// Create graphics API implementation.
			//std::shared_ptr<IGraphicsPlugin> graphicsPlugin = CreateGraphicsPlugin(options, platformPlugin);
			std::shared_ptr<IGraphicsPlugin> graphicsPlugin = CreateGraphicsPlugin_DesperabisD3D11(options, platformPlugin);
			
			// Initialize the OpenXR program.
			std::shared_ptr<IOpenXrProgram> program = CreateOpenXrProgram(options, platformPlugin, graphicsPlugin);

			program->CreateInstance();
			program->InitializeSystem();

			options->SetEnvironmentBlendMode(program->GetPreferredBlendMode());
			//UpdateOptionsFromCommandLine(*options, argc, argv);
			platformPlugin->UpdateOptions(options);
			graphicsPlugin->UpdateOptions(options);

			program->InitializeDevice();
			program->InitializeSession();
			program->CreateSwapchains();

			g_desperabisScene = std::make_unique<LevelScene>(g_InitialLevel);
			CO_AWAIT g_desperabisScene->CreateScene();

			while (!quitKeyPressed) {
				bool exitRenderLoop = false;
				program->PollEvents(&exitRenderLoop, &requestRestart);
				if (exitRenderLoop) {
					break;
				}

				if (program->IsSessionRunning()) {
					program->PollActions();
					RunSimulation();
					program->RenderFrame();
				}
				else {
					// Throttle loop since xrWaitFrame won't be called.
					std::this_thread::sleep_for(std::chrono::milliseconds(250));
				}
			}

		} while (!quitKeyPressed && requestRestart);

		g_FileManager.Deinitialize();
		return 0;
	}
	catch (const std::exception& ex) {
		Log::Write(Log::Level::Error, ex.what());
		Log::Write(Log::Level::Info, "Press any key to exit...");
		_getch(); // Waits for a keypress

		return 1;
	}
	catch (...) {
		Log::Write(Log::Level::Error, "Unknown Error");
		return 2;
	}
}
