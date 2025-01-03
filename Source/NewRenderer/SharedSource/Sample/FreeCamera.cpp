#include "pch.h"
#include <Sample/FreeCamera.h>
#include <Renderer/Renderer.h>
#include <Renderer/Camera.h>
#include <Asset/GridCellArray.h>
#include <Asset/Level.h>

FreeCamera::FreeCamera()
{
}

void FreeCamera::SetMousePos(float x, float y)
{
    m_euler.Set(
        (x - m_centerX) * -0.005f, // yaw
        (y - m_centerY) * -0.0025f, // pitch
        0.0f
    );
}

void FreeCamera::SetSpeed(float forward, float sideways)
{
    m_speedX = sideways * 3.f;
    m_speedZ = -forward * 3.f;
}

void FreeCamera::Update(double timeDelta, Level* level)
{
    Camera* cam = Renderer::Instance().GetOrCreateCamera().get();

    // generate rotation matrix
    m_camMat = Matrix4::CreateTransform(m_currentPos, m_euler, 1.0f);

    // inc position along view direction
    Vector speed((float)(m_speedX * timeDelta), 0, (float)(m_speedZ * timeDelta));
    speed = m_camMat.TransformDirection(speed);
    m_currentPos += speed;

    if (m_enableGravity && level != nullptr)
    {
        Vector3 p;
        p = m_currentPos;
        p.y = level->GetHeightAtPos(m_currentPos) + 55.f * DespToFloat;
        m_currentPos.Set(p.x, p.y, p.z);
    }

    // re-generate again with proper translation
    m_camMat = Matrix4::CreateTransform(m_currentPos, m_euler, 1.0f);


    cam->SetCameraTransformMatrix(m_camMat); // computes the view matrix
}

void FreeCamera::JumpTo(const Vector& newPos)
{
    m_currentPos = newPos;
}

Vector FreeCamera::GetPosition() const
{
    return m_currentPos;
}

Vector FreeCamera::GetDirection() const
{
    Vector dir(0, 0, -1);
    dir = m_camMat.TransformDirection(dir);
    return dir;
}
