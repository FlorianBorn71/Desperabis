#include "pch.h"
#include <PhysX/PhysXPhysicsModule.h>

#include "PxPhysicsAPI.h"

using namespace physx;
#define SAFE_RELEASE(_p) if (_p!=nullptr) {_p->release();_p=nullptr;}

static PxDefaultErrorCallback gDefaultErrorCallback;
static PxDefaultAllocator gDefaultAllocatorCallback;
static PxDefaultCpuDispatcher* gCpuDispatcher = nullptr;
static PxFoundation* gFoundation = nullptr;
static PxPvd* gPvd = nullptr;
static PxPhysics* gPhysics = nullptr;
static PxCooking* gCooking = nullptr;


PhysXPhysicsModule::PhysXPhysicsModule()
{
	g_Instance = this;
}

void PhysXPhysicsModule::Initialize()
{
	gFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, gDefaultAllocatorCallback, gDefaultErrorCallback);
	assert(gFoundation != nullptr);

	gPvd = PxCreatePvd(*gFoundation);
	assert(gPvd != nullptr);
	PxPvdTransport* transport = PxDefaultPvdSocketTransportCreate("localhost", 5425, 10000);
	gPvd->connect(*transport, PxPvdInstrumentationFlag::eALL);

	bool recordMemoryAllocations = false;
	gPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *gFoundation, PxTolerancesScale(), recordMemoryAllocations, gPvd);
	assert(gPhysics != nullptr);

	gCpuDispatcher = PxDefaultCpuDispatcherCreate(2);
	assert(gCpuDispatcher != nullptr);

	PxTolerancesScale scale;
	scale.length = DespToFloat * 100.f;
	scale.speed = DespToFloat * 100.f;
	gCooking = PxCreateCooking(PX_PHYSICS_VERSION, *gFoundation, PxCookingParams(scale));
	assert(gCooking != nullptr);

}


void PhysXPhysicsModule::Deinitialize()
{
	SAFE_RELEASE(gCpuDispatcher)
	SAFE_RELEASE(gPhysics);
	SAFE_RELEASE(gPvd);
	SAFE_RELEASE(gCooking);
	SAFE_RELEASE(gFoundation);
}

class PhysXBaseShape : public PhysicsShape
{
public:
	PhysXBaseShape() = default;
	virtual ~PhysXBaseShape()
	{
		SAFE_RELEASE(m_triMesh);
	}

	PxTriangleMesh* m_triMesh = nullptr;
};


class PhysXPhysicsWorld : public PhysicsWorld
{
public:
	PhysXPhysicsWorld()
	{
		PxSceneDesc sceneDesc(gPhysics->getTolerancesScale());
		sceneDesc.gravity = PxVec3(0.0f, -9.81f * DespToFloat, 0.0f);
		sceneDesc.cpuDispatcher = gCpuDispatcher;
		sceneDesc.filterShader = PxDefaultSimulationFilterShader;
		m_Scene = gPhysics->createScene(sceneDesc);
		m_defaultMaterial = gPhysics->createMaterial(0.5f, 0.5f, 0.f);
	}

	virtual ~PhysXPhysicsWorld() final
	{
		SAFE_RELEASE(m_defaultMaterial);
		SAFE_RELEASE(m_Scene);
	}

	virtual void CreateStaticCollider(std::shared_ptr<PhysicsShape> shape, const Matrix4& transform) override final
	{
		PhysXBaseShape* ps = (PhysXBaseShape *)shape.get();
		if (ps->m_triMesh == nullptr)
		{
			return;
		}
		Vector pos = transform.GetTranslation();
		PxMat44 pxmat;
		transform.Store(&pxmat.column0.x);
		PxTransform t(pxmat);
		PxRigidStatic* actor = gPhysics->createRigidStatic(t);
		PxShape* pxshape = PxRigidActorExt::createExclusiveShape(*actor, PxTriangleMeshGeometry(ps->m_triMesh), *m_defaultMaterial);
		m_Scene->addActor(*actor);
	}

	virtual void CreateStaticCylinderCollider(float radius, float height, const Matrix4& transform) override final
	{
		PxMat44 pxmat;
		transform.Store(&pxmat.column0.x);
		PxTransform t(pxmat);
		PxRigidStatic* actor = gPhysics->createRigidStatic(t);
		PxShape* pxshape = PxRigidActorExt::createExclusiveShape(*actor, PxCapsuleGeometry(radius, height * 0.5f), *m_defaultMaterial);

		m_Scene->addActor(*actor);
	};

	virtual void StartSimulation(double elapsedTime) override final
	{
		m_Scene->simulate((float)elapsedTime);
	}

	virtual void FetchResults() override final
	{
		m_Scene->fetchResults(true);
	}

	PxScene* m_Scene = nullptr;
	PxMaterial* m_defaultMaterial = nullptr;
};


std::unique_ptr<PhysicsWorld> PhysXPhysicsModule::CreateWorld()
{
	return std::make_unique<PhysXPhysicsWorld>();
}

std::shared_ptr<PhysicsShape> PhysXPhysicsModule::CreateTriMesh(const TriangularShapeDesc& desc)
{
	if (desc.m_numTriangles == 0)
	{
		return nullptr;
	}
	std::shared_ptr<PhysXBaseShape> shape = std::make_shared<PhysXBaseShape>();
	PxTriangleMeshDesc meshDesc;
	meshDesc.flags = PxMeshFlag::e16_BIT_INDICES | PxMeshFlag::eFLIPNORMALS;
	meshDesc.points.data = desc.m_vertexData;
	meshDesc.points.count = desc.m_numVertices;
	meshDesc.points.stride = desc.m_vertexStride;
	meshDesc.triangles.data = desc.m_indexData;
	meshDesc.triangles.count = desc.m_numTriangles;
	meshDesc.triangles.stride = 3 * sizeof(uint16_t);
	shape->m_triMesh = gCooking->createTriangleMesh(meshDesc, gPhysics->getPhysicsInsertionCallback());
	return shape;
}

