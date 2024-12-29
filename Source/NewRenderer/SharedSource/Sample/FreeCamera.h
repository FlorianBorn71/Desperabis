#pragma once

#include <DesperabisCommon.h>
#include <Math/Math.h>

class FreeCamera
{
public:
    FreeCamera();
    ~FreeCamera() = default;

    void SetMousePos(float x, float y);
    void SetSpeed(float forward, float sideways);
    void SetWindowCenter(float x, float y) { m_centerX = x; m_centerY = y;}
    void SetUseGravity(bool status) { m_enableGravity = status; }
    void Update(double timeDelta, Level* level = nullptr);

    void JumpTo(const Vector& newPos);
    Vector GetPosition() const;
    Vector GetDirection() const;
private:
    bool m_enableGravity = true;
    float m_yaw = 0.f;
    float m_pitch = 0.f;

    float m_speedX = 0.f;
    float m_speedZ = 0.f;

    float m_centerX = 1000.f;
    float m_centerY = 600.f;

	Level* m_level = nullptr; // for gravity
    Matrix4 m_camMat;
    Vector m_currentPos;
    Vector m_euler;
};
