#include "pch.h"
#include <Scene/AnimationPose.h>
#include <Asset/AnimationSequence.h>
#include <Asset/ModelAnim.h>
#include <corecrt_math_defines.h>
#include <Renderer/RenderableInstance.h>

void AnimationPose::Start(ModelAnim* modelAnim, AnimationSequence* anim, float timeScale)
{
    m_modelAnim = modelAnim;
    m_sequence = anim;
    m_speedFactor = timeScale;
    m_isRunning = true;
    m_currentKeyframe = 0;
    m_nextKeyframe = 1;
    m_currentWeight = 0.0f;
    m_currentTime = 0;
}


bool AnimationPose::TryValidate()
{
    if (m_modelAnim==nullptr || m_sequence == nullptr || !m_sequence->IsValid())
        return false;

    int numKeyframes = (int)m_sequence->m_keyframes.size();
    if (m_currentKeyframe >= numKeyframes || m_nextKeyframe >= numKeyframes)
    {
        m_currentKeyframe = 0;
        m_nextKeyframe = 1;
        m_currentTime = 0.0f;
    }
    return true;
}

float AnimationPose::EffectiveWeight(float weightIn) const
{
    const AnimationKeyframe& kf = m_sequence->m_keyframes[m_currentKeyframe];
    if (kf.m_moveTransition == MoveTransitionType::Cosine)
        return (float)(0.5 - 0.5 * cos(weightIn * Math::Pi));
    return weightIn;
}

void AnimationPose::ApplyMoveDelta(Vector3& moveAccum, Vector3& rotateAccum, float weight) const
{
/* TODO
    const AnimationKeyframe& kf = m_sequence->m_keyframes[m_currentKeyframe];
    moveAccum += kf.m_positionOffset * (weight * m_moveIntensity * m_sequence->m_moveIntensity);
    rotateAccum += kf.m_rotationOffset * weight;
    */
}

void AnimationPose::Update(double elapsedTime)
{
    // sanity check -  start over if the sequence has been changed while running
    if (!TryValidate())
        return;

    int numKeyframes = (int)m_sequence->m_keyframes.size();
    float previousWeight = EffectiveWeight(m_currentWeight);
    Vector3 moveAccum = {}, rotateAccum = {};

    m_currentTime += (float)(elapsedTime * m_sequence->m_speedFactor * m_speedFactor);
    while (m_currentTime > m_sequence->m_keyframes[m_currentKeyframe].m_duration)
    {
        ApplyMoveDelta(moveAccum, rotateAccum, 1.0f - previousWeight); // finish movement of current frame
        previousWeight = 0.0f;
        m_currentTime -= m_sequence->m_keyframes[m_currentKeyframe].m_duration;
        m_currentKeyframe++;
        if (m_currentKeyframe >= numKeyframes)
        {
            if (m_sequence->m_looped)
            {
                m_currentKeyframe = 0;
            }
            else
            {
                m_currentKeyframe = m_nextKeyframe = numKeyframes - 1;
                m_currentWeight = 1.0f;
                m_isRunning = false;
                return;
            }
        }

        if (m_sequence->m_looped)
        {
            m_nextKeyframe = (m_currentKeyframe + 1) % numKeyframes;
        }
        else
        {
            m_nextKeyframe = __min(m_currentKeyframe + 1, numKeyframes - 1);
        }
    }

    m_currentWeight = m_currentTime / m_sequence->m_keyframes[m_currentKeyframe].m_duration;

    /* TODO
    if (_motionObject != null)
    {
        float diff = EffectiveWeight(_currentWeight) - previousWeight;
        ApplyMoveDelta(ref moveAccum, ref rotateAccum, diff);
        _motionObject.SetTransform(_motionObject.Position + moveAccum, _motionObject.Rotation + rotateAccum * degToRad);
    }
    */

}


void AnimationPose::ApplyTo(RenderableInstance& inst) const
{
    const AnimationKeyframe& kf = m_sequence->m_keyframes[m_currentKeyframe];
    const AnimationKeyframe& nextkf = m_sequence->m_keyframes[m_nextKeyframe];
    Model* m1 = m_modelAnim->GetModel(kf.m_animNr, kf.m_frameNr);
    Model* m2 = m_modelAnim->GetModel(nextkf.m_animNr, nextkf.m_frameNr);

    inst.m_blendWeight = m_currentWeight;
    inst.SetRenderable(m1, m2);
}

