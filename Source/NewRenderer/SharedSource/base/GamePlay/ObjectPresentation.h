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
    std::shared_ptr<ObjectPresentationContext> CreateObjectPresentation(
        Scene& scene, int slot, const RawGameObject& object, RenderableInstance* instance);
}
