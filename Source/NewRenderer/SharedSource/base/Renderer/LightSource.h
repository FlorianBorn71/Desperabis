#pragma once
#include <DesperabisCommon.h>
#include <Asset\ModelGeometry.h>
#include <Scene\GameObject.h>

struct PackedLightSourceParams
{
    inline void SetLight(float x, float y, float z, float radius, const Color4& color, float multiplier = 1.f)
    {
		Vector4 colF = color.ToFloat(multiplier);
		PositionAndRadius = { x, y, z, radius };
        ColorAndPadding = { colF.x, colF.y, colF.z, 1.f }; //.w yet unused
    }

    inline void SetCorona(float x, float y, float z, float radius, const Color4& color, float multiplier = 1.f)
	{
		Vector4 colF = color.ToFloat(multiplier);
		PositionAndRadius = { x, y, z, radius };
        ColorAndPadding = { colF.x, colF.y, colF.z, 1.f }; //.w yet unused
	}

    Float4Storage PositionAndRadius;
    Float4Storage ColorAndPadding;
};


class LightSource : public GameObjectBase
{
public:
    LightSource(Scene* scene);
    virtual ~LightSource() = default;

    virtual bool FillLightParams(PackedLightSourceParams& dst) const;
    virtual bool FillCoronaParams(PackedLightSourceParams& dst) const;

    const Vector& GetPosition() const
    {
        return m_pos;
    }
    float GetRadius() const
    {
        return m_radius;
    }
    float GetCoronaSize() const
    {
        return m_coronaSize;
    }
    float GetEffectiveCoronaSize() const
    {
        return m_coronaSize < 0.f ? (m_radius * 0.2f * DespRelativeInvScaling) : m_coronaSize;
    }

    void SetPosition(const Vector& pos)
    {
        m_globalBoundsDirty = true;
        m_pos = pos;
    }
    void SetRadius(float r)
    {
        m_globalBoundsDirty = true;
        m_radius = r;
    }
    void SetLightHeightOffset(float h)
    {
        m_heightOffset = h;
    }
    void SetColor(const Color4& color, float multiplier = 1.0f)
    {
        m_color = color;
        m_multiplier = multiplier;
    }

    void SetCoronaSize(float r)
    {
        m_coronaSize = r;
    }

    void SetCoronaColorMod(const Color4& color)
    {
        m_coronaColorMod = color;
    }

    const BoundingVolume& GetGlobalBounds() const { return m_globalBounds; }
    void UpdateGlobalBounds();
protected:
    Vector m_pos;
    float m_radius = 1.f;
    float m_multiplier = 1.f;
    float m_coronaSize = -1.f; // if <0, then use fraction of radius
    float m_heightOffset = 0.f;
    Color4 m_color, m_coronaColorMod;
    bool m_globalBoundsDirty = true;
    BoundingVolume m_globalBounds;
};


class FlickeringLight : public LightSource
{
public:
    FlickeringLight(Scene* scene);
    virtual ~FlickeringLight() = default;

    virtual bool FillLightParams(PackedLightSourceParams& dst) const override final;
    virtual void Update(double elapsedTime) override final;

private:
    double m_phase = 0.f;
    double m_speed = 1.0f;
};


class PulsatingLight : public LightSource
{
public:
    PulsatingLight(Scene* scene);
    virtual ~PulsatingLight() = default;

    virtual void Update(double elapsedTime) override final;
private:
    double m_phase = 0.f;
    double m_speed = 1.0f;
};


class PlayerLight : public LightSource
{
public:
    PlayerLight(Scene* scene);
    virtual ~PlayerLight() = default;

    virtual void Update(double elapsedTime) override final;
};