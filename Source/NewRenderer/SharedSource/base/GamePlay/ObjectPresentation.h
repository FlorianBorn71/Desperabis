#pragma once
#include <DesperabisCommon.h>
#include <Asset/RawGameObject.h>

class ObjectPresentationContext
{
public:
    virtual ~ObjectPresentationContext() = default;
    virtual bool Matches(const RawGameObject& object) const = 0;
    virtual void Update(const RawGameObject& object, RenderableInstance* instance) = 0;
    virtual void Remove(Scene& scene) = 0;
    virtual void Advance(double elapsedTime) {}
    virtual void FireEmitted(const Vector& position) {}
};

namespace GamePlay
{
    bool IsElectricBolt(const RawGameObject& object);
    std::shared_ptr<ObjectPresentationContext> CreateObjectPresentation(
        Scene& scene, int levelIndex, int slot, const RawGameObject& object, RenderableInstance* instance);
}
