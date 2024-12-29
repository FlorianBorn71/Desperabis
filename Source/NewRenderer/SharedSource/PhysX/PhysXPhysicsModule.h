#pragma once

#include <Common.h>
#include <Physics/PhysicsInterface.h>


class D_IMPEXP PhysXPhysicsModule : public PhysicsModule
{
public:
    PhysXPhysicsModule();
    void Initialize();
    void Deinitialize();

    virtual std::unique_ptr<PhysicsWorld> CreateWorld() override final;
    virtual std::shared_ptr<PhysicsShape> CreateTriMesh(const TriangularShapeDesc& desc) override final;

private:
};