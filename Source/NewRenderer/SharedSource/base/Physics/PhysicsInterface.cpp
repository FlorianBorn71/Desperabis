#include "pch.h"
#include <Physics/PhysicsInterface.h>

PhysicsModule* PhysicsModule::g_Instance = nullptr;


class NullPhysicsWorld : public PhysicsWorld
{
public:
	virtual void CreateStaticCollider(std::shared_ptr<PhysicsShape> shape, const Matrix4& transform) override final
	{ 
	}
	virtual void CreateStaticCylinderCollider(float radius, float height, const Matrix4& transform) override final
	{ 
	}
	
	virtual void StartSimulation(double elapsedTime) override final
	{
	}
	virtual void FetchResults() override final
	{
	}

};


NullPhysicsModule::NullPhysicsModule()
{
	g_Instance = this;
}


std::unique_ptr<PhysicsWorld> NullPhysicsModule::CreateWorld()
{
	return std::make_unique<NullPhysicsWorld>();
}

std::shared_ptr<PhysicsShape> NullPhysicsModule::CreateTriMesh(const TriangularShapeDesc& desc)
{
	return nullptr;
}

