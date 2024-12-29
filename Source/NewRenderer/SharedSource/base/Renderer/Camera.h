#pragma once
#include <DesperabisCommon.h>
#include <Scene\Transformable.h>

class Camera
    //: public Transformable
{
public:
    Camera() = default;
    ~Camera() = default;

    const Matrix4& GetProjectionMatrix(int eye) const
    {
        return m_projectionMatrix[eye];
    }

    const Matrix4& GetViewMatrix(int eye) const
    {
        return m_viewMatrix[eye];
    }

    const Matrix4& GetCameraTransformMatrix(int eye) const
    {
        return m_cameraTransform[eye];
    }

    const Vector& GetCameraPosition() const
    {
        return m_cameraPosition;
    }

    void SetProjectionMatrix(const Matrix4& mat)
    {
        m_projectionMatrix[0] = m_projectionMatrix[1] = mat;
        m_stereo = false;
    }

    void SetProjectionMatrices(const Matrix4& left, const Matrix4& right)
    {
        m_projectionMatrix[0] = left;
        m_projectionMatrix[1] = right;
        m_stereo = true;
    }

    void SetCameraTransformMatrix(const Matrix4& mat)
    {
        m_cameraTransform[0] = m_cameraTransform[1] = mat;
        m_viewMatrix[0] = m_viewMatrix[1] = mat.GetInverse();
        m_cameraPosition = m_cameraTransform[0].GetTranslation();
        m_stereo = false;
    }

    void SetCameraTransformMatrices(const Matrix4& left, const Matrix4& right)
    {
        m_cameraTransform[0] = left;
        m_cameraTransform[1] = right;
        m_viewMatrix[0] = left.GetInverse();
        m_viewMatrix[1] = right.GetInverse();
        m_cameraPosition = m_cameraTransform[0].GetTranslation();
        m_stereo = true;
    }

    void SetViewMatrix(const Matrix4& mat)
    {
        m_viewMatrix[0] = m_viewMatrix[1] = mat;
        m_cameraTransform[0] = m_cameraTransform[1] = mat.GetInverse();
        m_cameraPosition = m_cameraTransform[0].GetTranslation();
        m_stereo = false;
    }

    void SetViewMatrices(const Matrix4& left, const Matrix4& right)
    {
        m_viewMatrix[0] = left;
        m_viewMatrix[1] = right;
        m_cameraTransform[0] = left.GetInverse();
        m_cameraTransform[1] = right.GetInverse();
        m_cameraPosition = m_cameraTransform[0].GetTranslation();
        m_stereo = true;
    }

    void Set(const Camera& other);
    void CreateMirrored(const Camera& other, float mirrorHeight);

    inline bool IsStereo() const { return m_stereo; }

    void BuildFrustum(Frustum& frustum) const;


private:
    bool m_stereo = false;
    Matrix4 m_projectionMatrix[2];
    Matrix4 m_cameraTransform[2];
    Matrix4 m_viewMatrix[2];
    Vector m_cameraPosition;
};

