#pragma once

#include <DesperabisCommon.h>


struct TriangularShapeDesc
{
    const float* m_vertexData = nullptr;
    int m_numVertices = 0;
    int m_vertexStride = 0;
    const uint16_t* m_indexData = nullptr;
    int m_numTriangles = 0;
};


class PhysicsShape
{

};


class PhysicsObject
{

};


class PhysicsWorld
{
public:
	PhysicsWorld() = default;
    virtual ~PhysicsWorld() = default;

    virtual void CreateStaticCollider(std::shared_ptr<PhysicsShape> shape, const Matrix4& transform) = 0;
    virtual void CreateStaticCylinderCollider(float radius, float height, const Matrix4& transform) = 0;

    virtual void StartSimulation(double elapsedTime) = 0;
    virtual void FetchResults() = 0;
};

class D_IMPEXP PhysicsModule
{
public:
    static PhysicsModule& Instance()
    {
        return *g_Instance;
    }

    virtual std::unique_ptr<PhysicsWorld> CreateWorld() = 0;

    virtual std::shared_ptr<PhysicsShape> CreateTriMesh(const TriangularShapeDesc& desc) = 0;


protected:
    static PhysicsModule* g_Instance;
};


/////////////////////////////////////////////////////////////////////////////////////////////


class D_IMPEXP NullPhysicsModule : public PhysicsModule
{
public:
    NullPhysicsModule();
    void Initialize() {};
    void Deinitialize() {};

    virtual std::unique_ptr<PhysicsWorld> CreateWorld() override final;
    virtual std::shared_ptr<PhysicsShape> CreateTriMesh(const TriangularShapeDesc& desc) override final;

};