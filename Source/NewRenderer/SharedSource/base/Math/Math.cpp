#include "pch.h"
#include <Math/Math.h>
#include <DirectXMath.h>



#define EARLY_OUT_SPHERE(_i, _s) if (m_planes[_i].GetDistance(_s) > sphereRad) return false;
#define EARLY_OUT_BOX(_i, _b) if (m_planes[_i].GetDistance(_b) > 0) return false;

bool Frustum::IsSphereVisible(const Vector& sphereCenter, float sphereRad) const
{
    EARLY_OUT_SPHERE(0, sphereCenter);
    EARLY_OUT_SPHERE(1, sphereCenter);
    EARLY_OUT_SPHERE(2, sphereCenter);
    EARLY_OUT_SPHERE(3, sphereCenter);
    EARLY_OUT_SPHERE(4, sphereCenter);
    EARLY_OUT_SPHERE(5, sphereCenter);
    return true;
}


bool Frustum::IsBoxVisible(const AlignedBox& box) const
{
    EARLY_OUT_BOX(0, box);
    EARLY_OUT_BOX(1, box);
    EARLY_OUT_BOX(2, box);
    EARLY_OUT_BOX(3, box);
    EARLY_OUT_BOX(4, box);
    EARLY_OUT_BOX(5, box);

    return true;
}


bool Frustum::IsVolumeVisible(const BoundingVolume& vol) const
{
    const float sphereRad = vol.GetSphereRadius();
    EARLY_OUT_SPHERE(0, vol.m_sphere);
    EARLY_OUT_SPHERE(1, vol.m_sphere);
    EARLY_OUT_SPHERE(2, vol.m_sphere);
    EARLY_OUT_SPHERE(3, vol.m_sphere);
    EARLY_OUT_SPHERE(4, vol.m_sphere);
    EARLY_OUT_SPHERE(5, vol.m_sphere);

    EARLY_OUT_BOX(0, vol.m_box);
    EARLY_OUT_BOX(1, vol.m_box);
    EARLY_OUT_BOX(2, vol.m_box);
    EARLY_OUT_BOX(3, vol.m_box);
    EARLY_OUT_BOX(4, vol.m_box);
    EARLY_OUT_BOX(5, vol.m_box);

    return true;
}

void Matrix4::Create(const Vector& pos, const Vector& eulerRot, float scale)
{
     XMVECTOR vecPos, vecRot;
#ifdef USE_DXMATH
     vecPos = pos.m_vector;
     vecRot = eulerRot.m_vector;
#else
     vecPos = XMLoadFloat4((const XMFLOAT4*)&pos.m_x);
     vecRot = XMLoadFloat4((const XMFLOAT4*)&eulerRot.m_x);
#endif
    XMMATRIX mTranslation = XMMatrixTranslationFromVector(vecPos);
    // after testing:
    XMMATRIX mRot = XMMatrixRotationX(XMVectorGetY(vecRot)) * XMMatrixRotationZ(XMVectorGetZ(vecRot)) * XMMatrixRotationY(XMVectorGetX(vecRot));
    XMMATRIX mScale = XMMatrixScaling(scale, scale, scale);
    m_matrix = mScale * mRot * mTranslation;
}

void Matrix4::Create(const Vector& pos, const Vector& rightDir, const Vector& upDir, const Vector& dir)
{
#ifdef USE_DXMATH
    m_matrix.r[0] = rightDir.m_vector;
    m_matrix.r[1] = upDir.m_vector;
    m_matrix.r[2] = dir.m_vector;
    m_matrix.r[3] = pos.m_vector;
#else
    m_matrix.r[0] = XMVectorSet(rightDir.m_x, rightDir.m_y, rightDir.m_z, rightDir.m_w);
    m_matrix.r[1] = XMVectorSet(upDir.m_x, upDir.m_y, upDir.m_z, upDir.m_w);
    m_matrix.r[2] = XMVectorSet(dir.m_x, dir.m_y, dir.m_z, dir.m_w);
    m_matrix.r[3] = XMVectorSet(pos.m_x, pos.m_y, pos.m_z, pos.m_w);
#endif
}

Vector Matrix4::GetRightDir() const
{
    Vector out_pos;
#ifdef USE_DXMATH
    out_pos.m_vector = m_matrix.r[0];
#else
    XMStoreFloat4((XMFLOAT4 *)&out_pos.m_x, m_matrix.r[0]);
#endif

    return out_pos;
}

Vector Matrix4::GetUpDir() const
{
    Vector out_pos;
#ifdef USE_DXMATH
    out_pos.m_vector = m_matrix.r[1];
#else
    XMStoreFloat4((XMFLOAT4*)&out_pos.m_x, m_matrix.r[1]);
#endif
    return out_pos;
}

Vector Matrix4::GetDirection() const
{
    Vector out_pos;
#ifdef USE_DXMATH
    out_pos.m_vector = m_matrix.r[2];
#else
    XMStoreFloat4((XMFLOAT4*)&out_pos.m_x, m_matrix.r[2]);
#endif
    return out_pos;
}

Vector Matrix4::GetTranslation() const
{
    Vector out_pos;
#ifdef USE_DXMATH
    out_pos.m_vector = m_matrix.r[3];
#else
    XMStoreFloat4((XMFLOAT4*)&out_pos.m_x, m_matrix.r[3]);
#endif    
    return out_pos;
}

void Matrix4::SetTranslation(const Vector& pos)
{
#ifdef USE_DXMATH
    m_matrix.r[3] = pos.m_vector;
#else
    m_matrix.r[3] = XMVectorSet(pos.m_x, pos.m_y, pos.m_z, pos.m_w);
#endif    
}

void Matrix4::AddTranslation(const Vector& delta)
{
#ifdef USE_DXMATH
	m_matrix.r[3] += delta.m_vector;
#else
	m_matrix.r[3] += XMVectorSet(delta.m_x, delta.m_y, delta.m_z, delta.m_w);
#endif    

}

void Matrix4::Decompose(Vector& out_pos, Quaternion& out_rot, Vector& out_scale) const
{
#ifdef USE_DXMATH
    XMMatrixDecompose(&out_scale.m_vector, &out_rot.m_vector, &out_pos.m_vector, m_matrix);
#else
    XMVECTOR scale, rot, pos;
    XMMatrixDecompose(&scale, &rot, &pos, m_matrix);
    XMStoreFloat4((XMFLOAT4*)&out_pos.m_x, pos);
    XMStoreFloat4((XMFLOAT4*)&out_rot.m_x, rot);
    XMStoreFloat4((XMFLOAT4*)&out_scale.m_x, scale);
#endif
}

Matrix4 Matrix4::GetTransposed() const
{
    Matrix4 m;
    m.m_matrix = XMMatrixTranspose(m_matrix);
    return m;
}

Matrix4 Matrix4::GetInverse() const
{
    Matrix4 m;
    m.m_matrix = XMMatrixInverse(nullptr, m_matrix);
    return m;
}

void Matrix4::Store(float* dest) const
{
    XMStoreFloat4x4((XMFLOAT4X4*)dest, m_matrix);
}

void Matrix4::Load(const float* source)
{
	m_matrix = XMLoadFloat4x4((const XMFLOAT4X4*)source);
}

void Matrix4::CreateProjection(float aspectRatio, float fovAngleY /*= 70.0f * Math::Pi / 180.0f*/, float nearClip /*= 0.05f*/, float farClip /*= 30.0f*/)
{
    m_matrix = XMMatrixPerspectiveFovRH(fovAngleY, aspectRatio, nearClip, farClip);
}

Matrix4 Matrix4::operator* (const Matrix4& rhs) const
{
    Matrix4 res;
    res.m_matrix = XMMatrixMultiply(m_matrix, rhs.m_matrix);
    return res;
}


void Matrix4::BuildFrustum(Frustum& frustum) const
{
    XMFLOAT4X4 mat;
    XMStoreFloat4x4(&mat, m_matrix);

    auto MakePlane = [&](int idx, float sign) -> Plane
    {
        Plane result;

        // https://stackoverflow.com/questions/12836967/extracting-view-frustum-planes-gribb-hartmann-method
        XMFLOAT4 p0 = { sign * mat.m[0][idx] - mat.m[0][3], sign * mat.m[1][idx] - mat.m[1][3], sign * mat.m[2][idx] - mat.m[2][3], sign * mat.m[3][idx] - mat.m[3][3] };
        XMVECTOR plane = XMLoadFloat4(&p0);
#ifdef USE_DXMATH
        result.m_vector = XMPlaneNormalize(plane);
#else
        XMStoreFloat4((XMFLOAT4*)&result.m_x, XMPlaneNormalize(plane));
#endif
        return result;
    };

    frustum.m_planes[0] = MakePlane(0, 1.f); // right
    frustum.m_planes[1] = MakePlane(0, -1.f); // left
    frustum.m_planes[2] = MakePlane(1, 1.f);  // top
    frustum.m_planes[3] = MakePlane(1, -1.f); // bottom
    frustum.m_planes[4] = MakePlane(2, 1.f);  // far
    frustum.m_planes[5] = MakePlane(2, -1.f); // near
}



Vector Matrix4::TransformPosition(const Vector& pos) const
{
    Vector r;
#ifdef USE_DXMATH
    r.m_vector = XMVector3TransformCoord(pos.m_vector, m_matrix);
#else
    XMStoreFloat4((XMFLOAT4*)&r.m_x, XMVector3TransformCoord(XMLoadFloat4((XMFLOAT4*)&pos.m_x), m_matrix));
#endif
    return r;

}

Vector Matrix4::TransformDirection(const Vector& dir) const
{
    Vector r;
#ifdef USE_DXMATH
    r.m_vector = XMVector3TransformNormal(dir.m_vector, m_matrix);
#else
    XMStoreFloat4((XMFLOAT4*)&r.m_x, XMVector3TransformNormal(XMLoadFloat4((XMFLOAT4*)&dir.m_x), m_matrix));
#endif
    return r;
}

void Matrix4Storage::Set(const Matrix4& m)
{
    XMStoreFloat4x4(&m_matrix, m.m_matrix);
}

void Matrix4Storage::SetTransposed(const Matrix4& m)
{
    XMStoreFloat4x4(&m_matrix, XMMatrixTranspose(m.m_matrix));
}

bool Vector::IsInsideSphere(const Vector& center, float radius) const
{
#ifdef USE_DXMATH
    float lenSqr = XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(m_vector, center.m_vector)));
#else
    Vector diff = *this - center;
    float lenSqr = diff.m_x * diff.m_x + diff.m_y * diff.m_y + diff.m_z * diff.m_z;
#endif
    return lenSqr <= radius * radius;
}


float Plane::GetDistance(const AlignedBox& box) const
{
    Vector testPoint;
    /*
    testPoint.x = plane.x >= 0 ? mins.x : maxs.x;
    testPoint.y = plane.y >= 0 ? mins.y : maxs.y;
    testPoint.z = plane.z >= 0 ? mins.z : maxs.z;
    */
#ifdef USE_DXMATH
    XMVECTOR ctrl = XMVectorSelectControl(XMVectorGetX(m_vector) >= 0 ? 0 : 1, XMVectorGetY(m_vector) >= 0 ? 0 : 1, XMVectorGetZ(m_vector) >= 0 ? 0 : 1, 0);
    testPoint.m_vector = XMVectorSelect(box.m_min.m_vector, box.m_max.m_vector, ctrl);
#else
    testPoint.m_x = m_x >= 0 ? box.m_min.m_x : box.m_max.m_x;
    testPoint.m_y = m_y >= 0 ? box.m_min.m_y : box.m_max.m_y;
    testPoint.m_z = m_z >= 0 ? box.m_min.m_z : box.m_max.m_z;
#endif
    return GetDistance(testPoint);
}


void AlignedBox::GetTransformed(AlignedBox& dest, const Matrix4& transform, float uniformScale) const
{
#ifdef USE_DXMATH
    XMVECTOR nmin = XMVector3TransformCoord(XMVectorScale(m_min.m_vector, uniformScale), transform.m_matrix);
    XMVECTOR nmax = XMVector3TransformCoord(XMVectorScale(m_max.m_vector, uniformScale), transform.m_matrix);
    dest.m_min.m_vector = XMVectorMin(nmin, nmax);
    dest.m_max.m_vector = XMVectorMax(nmin, nmax);
#else
    XMMATRIX t = XMLoadFloat4x4((const XMFLOAT4X4*)transform.m_row);
    XMVECTOR nmin = XMVector3TransformCoord(XMVectorScale(XMLoadFloat4((XMFLOAT4*)&m_min.m_x), uniformScale), t);
    XMVECTOR nmax = XMVector3TransformCoord(XMVectorScale(XMLoadFloat4((XMFLOAT4*)&m_max.m_x), uniformScale), t);
    XMStoreFloat4((XMFLOAT4*)&dest.m_min.m_x, XMVectorMin(nmin, nmax));
    XMStoreFloat4((XMFLOAT4*)&dest.m_max.m_x, XMVectorMax(nmin, nmax));
#endif
}


void BoundingVolume::GetTransformed(BoundingVolume& dest, const Matrix4& transform, float uniformScale) const
{
    m_box.GetTransformed(dest.m_box, transform, uniformScale);
    dest.OnUpdated();

}

void BoundingVolume::OnUpdated()
{
#ifdef USE_DXMATH
    XMVECTOR half = XMVectorSubtract(m_box.m_max.m_vector, m_box.m_min.m_vector) * 0.5f;

    // sphere = center of box and half diagonal
    float len = XMVectorGetX(XMVector3Length(half));
    m_sphere.m_vector = XMVectorSetW(XMVectorAdd(m_box.m_min.m_vector, half), len);
#else
    XMVECTOR bmin = XMLoadFloat4((XMFLOAT4*)&m_box.m_min.m_x);
    XMVECTOR bmax = XMLoadFloat4((XMFLOAT4*)&m_box.m_max.m_x);
    XMVECTOR half = XMVectorSubtract(bmax, bmin) * 0.5f;

    // sphere = center of box and half diagonal
    float len = XMVectorGetX(XMVector3Length(half));
    XMStoreFloat4((XMFLOAT4*)&m_sphere.m_x, XMVectorSetW(XMVectorAdd(bmin, half), len));
#endif
}
