#pragma once
#include <DesperabisCommon.h>
#include <Asset/ModelGeometry.h>

class AnimationPose
{
public:
    AnimationPose() = default;
    virtual ~AnimationPose() = default;

    void Update(double elapsedTime);
    void Start(ModelAnim* modelAnim, AnimationSequence* anim, float timeScale = 1.0f);

    void ApplyTo(RenderableInstance& inst) const;

protected:
    bool TryValidate();
    float EffectiveWeight(float weightIn) const;
    void ApplyMoveDelta(Vector3& moveAccum, Vector3& rotateAccum, float weight) const;
public:
    ModelAnim* m_modelAnim = nullptr;
    AnimationSequence* m_sequence = nullptr;
    float m_speedFactor = 1.0f;
    float m_moveIntensity = 1.0f;
    float m_currentTime = 0.0f;

    float m_currentWeight = 0.0f;
    int m_currentKeyframe = 0;
    int m_nextKeyframe = 0;
    bool m_isRunning = false;

};

