#include "pch.h"
#include <Renderer/Camera.h>

void Camera::BuildFrustum(Frustum& frustum) const
{
    const Matrix4& projectionMat = GetProjectionMatrix(0);
    const Matrix4& worldViewMat = GetViewMatrix(0);
    Matrix4 worldViewProj = worldViewMat * projectionMat;

    worldViewProj.BuildFrustum(frustum);
}

void Camera::Set(const Camera& other)
{
    m_stereo = other.m_stereo;
    m_cameraPosition = other.m_cameraPosition;

    for (int eye = 0; eye < 2; eye++)
    {
        m_projectionMatrix[eye] = other.m_projectionMatrix[eye];
        m_cameraTransform[eye] = other.m_cameraTransform[eye];
        m_viewMatrix[eye] = other.m_viewMatrix[eye];
    }
}

void Camera::CreateMirrored(const Camera& other, float mirrorHeight)
{
    Set(other);

	// create slightly modified projection matrix
	{
        // we need to swap/negate ViewLeft and ViewRight, because the shader lookup uses 1-u
        float pm[16];
        other.m_projectionMatrix[0].Store(pm);
        // The following flip actually performs this:
        //  XMMatrixPerspectiveOffCenterRH( ViewLeft, ViewRight, ViewBottom, ViewTop, NearZ, FarZ); // main view
        //	XMMatrixPerspectiveOffCenterRH(-ViewRight, -ViewLeft, ViewBottom, ViewTop, NearZ, FarZ); // mirror rendering

		pm[8] = -pm[8];
        m_projectionMatrix[0].Load(pm);
	}

    Vector pos = other.GetCameraPosition();
    float newY = pos.GetY() - mirrorHeight;
    newY = mirrorHeight - newY;
    Matrix4 camTransform = other.GetCameraTransformMatrix(0);
    Vector newPos(pos.GetX(), newY, pos.GetZ());
    Vector newUp = camTransform.GetUpDir();
    Vector newRight = camTransform.GetRightDir();
    Vector newDir = camTransform.GetDirection();

    // mirror the CS:
    newUp.Set(newUp.GetX(), -newUp.GetY(), newUp.GetZ(), newUp.GetW());
    newRight.Set(-newRight.GetX(), newRight.GetY(), -newRight.GetZ(), newRight.GetW()); // flip right vector to preserve handedness (use 1-u in the shader)
    newDir.Set(newDir.GetX(), -newDir.GetY(), newDir.GetZ(), newDir.GetW());

    camTransform.Create(newPos, newRight, newUp, newDir);
    SetCameraTransformMatrix(camTransform);
    m_stereo = false;
}
