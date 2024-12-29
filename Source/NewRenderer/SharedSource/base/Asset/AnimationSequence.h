#pragma once
#include <DesperabisCommon.h>
#include <Asset/AssetBase.h>
#include <Asset/ModelGeometry.h>

enum class KeyframeTransitionType
{
    Linear,
    Cosine,
    Random,
    RandomPerVertex
};

enum class MoveTransitionType
{
    Linear,
    Cosine
};


class AnimationKeyframe
{
public:
    void Read(AnimationSequence& owner, AssetStream& stream);


public:
    byte m_animNr = 0;
    byte m_frameNr = 0;
    float m_duration = 1.f;
    KeyframeTransitionType m_blendTransition = KeyframeTransitionType::Linear;
    MoveTransitionType m_moveTransition = MoveTransitionType::Linear;
    Vector3 m_positionOffset = {};
    Vector3 m_rotationOffset = {};

};


class AnimationSequence
{
public:
    AnimationSequence() = default;
    ~AnimationSequence() = default;

    void Read(AssetStream& stream);

    bool IsValid() const
    {
        return m_keyframes.size() > 0;
    }

public:
    AnimationSequences* m_owner = nullptr;
    std::string m_name;
    bool m_looped = true;
    float m_speedFactor = 1.f;
    float m_moveIntensity = 1.f;

    std::vector<AnimationKeyframe> m_keyframes;
};


class AnimationSequences : public AssetBase
{
public:
    AnimationSequences() = default;
    virtual ~AnimationSequences() = default;

    virtual void Read(AssetStream& stream);

    AnimationSequence* FindByName(std::string_view name);

public:
    std::vector<AnimationSequence> m_sequences;
};

