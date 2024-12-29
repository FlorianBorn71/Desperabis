#include "pch.h"
#include <Asset/AnimationSequence.h>
#include <iosfwd>

void AnimationKeyframe::Read(AnimationSequence& owner, AssetStream& stream)
{
    m_animNr = stream.Read<byte>();
    m_frameNr = stream.Read<byte>();
    //Model = owner.Owner.Owner.GetModel(animNr, frameNr);
    m_duration = stream.Read<float>();
    m_blendTransition = (KeyframeTransitionType)stream.Read<int>();
    m_moveTransition = (MoveTransitionType)stream.Read<int>();
    m_positionOffset = stream.Read<Vector3>();
    m_rotationOffset = stream.Read<Vector3>();
}


void AnimationSequence::Read(AssetStream& stream)
{
    stream.Read(m_name);
    m_looped = stream.Read<bool>();
    m_speedFactor = stream.Read<float>();
    m_moveIntensity = stream.Read<float>();

    int count = stream.Read<int>();
    m_keyframes.resize(count);
    for (int i = 0; i < count; i++)
    {
        m_keyframes[i].Read(*this, stream);
    }

}

void AnimationSequences::Read(AssetStream& s)
{
    int loadingVersion = s.Read<int>();
    int count = s.Read<int>();
    m_sequences.resize(count);
    for (int i = 0; i < count; i++)
    {
        m_sequences[i].m_owner = this;
        m_sequences[i].Read(s);
    }
}

AnimationSequence* AnimationSequences::FindByName(std::string_view name)
{
    for (auto& seq : m_sequences)
    {
        if (seq.m_name == name)
            return &seq;
    }
    return nullptr;
}
