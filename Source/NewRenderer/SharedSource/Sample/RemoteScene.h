#pragma once

#include <DesperabisCommon.h>
#include <Sample/LevelScene.h>
#include <Asset/RawGameObject.h>
#include <Renderer/RenderableInstance.h>

#include "Windows.h"

#define SHAREDMEM_MAPPEDFILE

const int g_numGameObjects = 140;

struct SharedLevelMem
{
    RawGameObjectData m_playerObj;
    RawGameObjectData m_objects[g_numGameObjects];
    short m_padding; // uneven number of gameobj
	uint64_t m_uniqueLevelHash;
    int m_beginWrite, m_endWrite;
};


class RemoteScene : public LevelScene
{
public:
    RemoteScene(int levelIndex);
    virtual ~RemoteScene();

    virtual FUTURE(void) CreateScene() override;
	virtual void ClearScene() override;

    virtual void Update(double elapsedTime) override;

	void TryConnect();
	void Disconnect();
    void SetUseFirstPersonCam(bool status, float hoverDist = 0.f) 
    {
        m_useFirstPersonCam = status; 
        m_hoverDistance = hoverDist;
    }

    static void ScheduleSnapshot() { g_pendingSnapshot = true; }
public:
#ifdef SHAREDMEM_MAPPEDFILE
	HANDLE m_hMapFile;
#endif
    bool m_isConnected = false;
    bool m_useFirstPersonCam = true;
    float m_hoverDistance = 0.f;
    SharedLevelMem* m_sharedMemory = nullptr;
    SharedLevelMem m_snapshotMem;

	RawGameObject m_currentPlayerPos;
    RawGameObject m_currentObjState[g_numGameObjects];
    RawGameObject m_lastUpdatedState[g_numGameObjects];
    RenderableInstancePtr m_renderableInst[g_numGameObjects];
    std::shared_ptr<LevelGameObject> m_gameObj[g_numGameObjects];

    RenderableInstancePtr m_playerModelInst;
protected:
    FUTURE(void) LoadNewLevel(int levelIndex);
    bool GatherSnapshot(int level, SharedLevelMem& out_data);
    void MakeSnapshot();
    static bool g_pendingSnapshot;
};
