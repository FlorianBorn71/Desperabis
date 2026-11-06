#include "pch.h"
#include <Renderer/LightSource.h>
#include <Renderer/Renderer.h>
#include <Renderer/Camera.h>
#include <Asset/GridCellArray.h>
#include <stdexcept>

LightSource::LightSource(Scene* scene)
    : GameObjectBase(scene)
{

}

bool LightSource::FillLightParams(PackedLightSourceParams& dst) const
{
    dst.SetLight(m_pos.GetX(), m_pos.GetY() + m_heightOffset, m_pos.GetZ(), m_radius, m_color, m_multiplier, m_specularAmount);
    return m_radius > 0.f;
}

void LightSource::SetSpecularAmount(float amount)
{
    if (!std::isfinite(amount) || amount < 0.f)
    {
        throw std::invalid_argument("Light specular amount must be finite and nonnegative.");
    }
    m_specularAmount = amount;
}


bool LightSource::FillCoronaParams(PackedLightSourceParams& dst) const
{
    float radius = GetEffectiveCoronaSize();
	Color4 finalCol = m_color * m_coronaColorMod;
	dst.SetCorona(m_pos.GetX(), m_pos.GetY(), m_pos.GetZ(), radius, finalCol);
    return radius > 0.f;
}

void LightSource::UpdateGlobalBounds()
{
    if (m_globalBoundsDirty)
    {
        const float boundsRadius = (std::max)(GetRadius(), GetEffectiveCoronaSize());
        m_globalBounds.m_sphere.Set(m_pos.GetX(), m_pos.GetY(), m_pos.GetZ(), boundsRadius);
        Vector rad(boundsRadius, boundsRadius, boundsRadius);
        m_globalBounds.m_box.m_min = m_pos - rad;
        m_globalBounds.m_box.m_max = m_pos + rad;

		if (m_scene != nullptr)
		{
			if (const auto* grid = m_scene->GetGrid())
			{
				grid->GetTileAssignment(m_pos, m_globalBounds);
			}
		}

        m_globalBoundsDirty = false;
    }
}

LightFlashSource::LightFlashSource(Scene* scene, float fadeInSeconds, float fadeOutSeconds)
    : LightSource(scene), m_fadeIn(fadeInSeconds), m_fadeOut(fadeOutSeconds)
{
    if (!std::isfinite(fadeInSeconds) || !std::isfinite(fadeOutSeconds) ||
        fadeInSeconds < 0.f || fadeOutSeconds < 0.f ||
        !std::isfinite(fadeInSeconds + fadeOutSeconds) || fadeInSeconds + fadeOutSeconds <= 0.f)
    {
        throw std::invalid_argument("Light flash fade times must be finite, nonnegative and have positive total duration.");
    }
    SetCoronaSize(0.f);
}

void LightFlashSource::Update(double elapsedTime)
{
    if (!std::isfinite(elapsedTime) || elapsedTime < 0.0)
    {
        throw std::invalid_argument("Light flash time must be finite and nonnegative.");
    }
    m_age += elapsedTime;
}

bool LightFlashSource::IsFinished() const { return m_age >= m_fadeIn + m_fadeOut; }

bool LightFlashSource::FillLightParams(PackedLightSourceParams& dst) const
{
    const float weight = IsFinished() ? 0.f : m_age < m_fadeIn ?
        static_cast<float>(m_age / m_fadeIn) :
        m_fadeOut > 0.f ? static_cast<float>(1.0 - (m_age - m_fadeIn) / m_fadeOut) : 0.f;
    dst.SetLight(m_pos.GetX(), m_pos.GetY(), m_pos.GetZ(), m_radius, m_color, m_multiplier * weight, m_specularAmount);
    return weight > 0.f && m_radius > 0.f;
}

FlickeringLight::FlickeringLight(Scene* scene)
    : LightSource(scene)
{
    m_phase = Math::GetRandD() * 10.0;
    m_speed = 0.8 + 0.4 * Math::GetRandD();
}

bool FlickeringLight::FillLightParams(PackedLightSourceParams& dst) const
{
    float rnd = (float)(cos(m_phase) + 0.6 * sin(m_phase * 2.41) + 0.4 * sin(m_phase * 4.17) + 0.2 * sin(m_phase * 11.821) + 0.15 * sin(m_phase * 29.198));
    float m = m_multiplier * (1.0f + rnd * 0.2f);
	dst.SetLight(m_pos.GetX(), m_pos.GetY() + sinf(rnd * 2.f) * 3.f * DespToFloat, m_pos.GetZ(), m_radius, m_color, m, m_specularAmount);

    return true;
}



void FlickeringLight::Update(double elapsedTime)
{
    m_phase += elapsedTime * m_speed;
}

/// <summary>
/// Pulsating light - fireball ammo
/// </summary>

PulsatingLight::PulsatingLight(Scene* scene)
    : LightSource(scene)
{
    SetRadius(120.f * DespToFloat);
    SetColor(Color4{ 255, 140, 30,255 }); // orange
    m_phase = Math::GetRandD() * 10.0;
    m_speed = 4.0; // 0.8 + 0.4 * Math::GetRandD();
}


void PulsatingLight::Update(double elapsedTime)
{
    m_phase += elapsedTime * m_speed;
    m_multiplier = 1.5f + (float)sin(m_phase) * 1.2f;
}


/// <summary>
/// Player light - follows the camera
/// </summary>


PlayerLight::PlayerLight(Scene* scene)
    : LightSource(scene)
{
    SetMode(Mode::Normal);
    SetCoronaSize(0); // no corona
}

void PlayerLight::SetMode(Mode mode)
{
    switch (mode)
    {
    case Mode::Off:
        SetSpecularAmount(0.f);
        SetRadius(0.f);
        break;
    case Mode::Blue:
        SetSpecularAmount(1.f);
        m_bluePhase = 0.0;
        SetColor(Color4{ 32, 80, 255, 255 }, 6.f);
        SetRadius(220.f * DespToFloat);
        break;
    case Mode::Normal:
        SetSpecularAmount(0.f);
        SetColor(Color4{ 255, 255, 255, 255 }, 1.5f);
        SetRadius(600.f * DespToFloat);
        break;
    default:
        throw std::invalid_argument("Unknown player-light mode.");
    }
    m_mode = mode;
}

void PlayerLight::Update(double elapsedTime)
{
    Vector pos = Renderer::Instance().GetOrCreateCamera()->GetCameraPosition();
    SetPosition(pos);
    if (m_mode == Mode::Blue)
    {
        m_bluePhase = fmod(m_bluePhase + elapsedTime * (2.0 * Math::Pi / 6.0), 2.0 * Math::Pi);
        m_multiplier = 6.f * (1.f + 0.3f * static_cast<float>(sin(m_bluePhase)));
    }
}
