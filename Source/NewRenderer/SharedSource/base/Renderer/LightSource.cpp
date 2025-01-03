#include "pch.h"
#include <Renderer/LightSource.h>
#include <Renderer/Renderer.h>
#include <Renderer/Camera.h>
#include <Asset/GridCellArray.h>

LightSource::LightSource(Scene* scene)
    : GameObjectBase(scene)
{

}

bool LightSource::FillLightParams(PackedLightSourceParams& dst) const
{
    dst.SetLight(m_pos.GetX(), m_pos.GetY() + m_heightOffset, m_pos.GetZ(), m_radius, m_color, m_multiplier);
    return m_radius > 0.f;
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
        m_globalBounds.m_sphere.Set(m_pos.GetX(), m_pos.GetY(), m_pos.GetZ(), GetRadius());
        Vector rad(GetRadius(), GetRadius(), GetRadius());
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

/// <summary>
/// Flickering light
/// </summary>
/// <param name="scene"></param>


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
	dst.SetLight(m_pos.GetX(), m_pos.GetY() + sinf(rnd * 2.f) * 3.f * DespToFloat, m_pos.GetZ(), m_radius, m_color, m);

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
    SetRadius(600.f * DespToFloat);
    SetColor(Color4{ 255, 255, 255, 255 }, 1.5f);
    SetCoronaSize(0); // no corona
}

void PlayerLight::Update(double elapsedTime)
{
    Vector pos = Renderer::Instance().GetOrCreateCamera()->GetCameraPosition();
    SetPosition(pos);
}
