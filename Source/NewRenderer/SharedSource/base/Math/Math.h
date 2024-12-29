#pragma once

#define USE_DXMATH 

#include <DirectXMath.h>
using namespace DirectX;


#include <stdlib.h>


struct Matrix4;
struct AlignedBox;


namespace Math
{
    static const constexpr float Pi = 3.14159265358979323846f;
    static const constexpr float ToRad = Math::Pi / 180.f;

    inline float GetRandF(float range = 1.0f) { return range * (float)rand() / (float)RAND_MAX; }
    inline double GetRandD(double range = 1.0) { return range * (double)rand() / (double)RAND_MAX; }
}


struct Vector
{
public:
    Vector()
    {
#ifdef USE_DXMATH
        m_vector = XMVectorZero();
#else
        m_x = m_y = m_z = m_w = 0.f;
#endif
    }

    Vector(float x, float y, float z, float w=1.0f)
    {
        Set(x, y, z, w);
    }

    void Set(float x, float y, float z, float w = 1.0f)
    {
#ifdef USE_DXMATH
        m_vector = XMVectorSet(x, y, z, w);
#else
        m_x = x;
        m_y = y;
        m_z = z;
        m_w = w;
#endif
    }

    inline float GetX() const 
    {
#ifdef USE_DXMATH
        return XMVectorGetX(m_vector);
#else
        return m_x;
#endif
    }

    inline float GetY() const
    {
#ifdef USE_DXMATH
        return XMVectorGetY(m_vector);
#else
        return m_y;
#endif
    }

    inline float GetZ() const
    {
#ifdef USE_DXMATH
        return XMVectorGetZ(m_vector);
#else
        return m_z;
#endif
    }

    inline float GetW() const
    {
#ifdef USE_DXMATH
        return XMVectorGetW(m_vector);
#else
        return m_w;
#endif
    }

    bool IsInsideSphere(const Vector& center, float radius) const;

    Vector& operator += (const Vector& rhs)
    {
#ifdef USE_DXMATH
        m_vector = XMVectorAdd(m_vector, rhs.m_vector);
#else
        m_x += rhs.m_x;
        m_y += rhs.m_y;
        m_z += rhs.m_z;
        m_w += rhs.m_w;
#endif
        return *this;
    }

    Vector operator + (const Vector& rhs) const
    {
        Vector ret;
#ifdef USE_DXMATH
        ret.m_vector = XMVectorAdd(m_vector, rhs.m_vector);
#else
        ret.m_x = m_x + rhs.m_x;
        ret.m_y = m_y + rhs.m_y;
        ret.m_z = m_z + rhs.m_z;
        ret.m_w = m_w + rhs.m_w;
#endif
        return ret;
    }

    Vector& operator -= (const Vector& rhs)
    {
#ifdef USE_DXMATH
        m_vector = XMVectorSubtract(m_vector, rhs.m_vector);
#else
        m_x -= rhs.m_x;
        m_y -= rhs.m_y;
        m_z -= rhs.m_z;
        m_w -= rhs.m_w;
#endif
        return *this;
    }

    Vector operator - (const Vector& rhs) const
    {
        Vector ret;
#ifdef USE_DXMATH
        ret.m_vector = XMVectorSubtract(m_vector, rhs.m_vector);
#else
        ret.m_x = m_x - rhs.m_x;
        ret.m_y = m_y - rhs.m_y;
        ret.m_z = m_z - rhs.m_z;
        ret.m_w = m_w - rhs.m_w;
#endif
        return ret;
    }

    Vector& operator *= (float scalar)
    {
#ifdef USE_DXMATH
        m_vector = XMVectorScale(m_vector, scalar);
#else
        m_x *= scalar;
        m_y *= scalar;
        m_z *= scalar;
        m_w *= scalar;
#endif
        return *this;
    }

    float GetLength() const 
    {
#ifdef USE_DXMATH
        return XMVectorGetX(XMVector3Length(m_vector));
#else
        return sqrtf(m_x * m_x + m_y * m_y + m_z * m_z);
#endif
    }

    Vector GetCross(const Vector& rhs)
    {
        Vector res;
#ifdef USE_DXMATH
        res.m_vector = XMVector3Cross(m_vector, rhs.m_vector);
#else
        res.m_x = (m_y * rhs.m_z) - (m_z * rhs.m_y);
        res.m_y = (m_z * rhs.m_x) - (m_x * rhs.m_z);
        res.m_z = (m_x * rhs.m_y) - (m_y * rhs.m_x);
        res.m_w = 0.0f;
#endif

        return res;
    }

    void Normalize()
    {
#ifdef USE_DXMATH
        m_vector = XMVector3Normalize(m_vector);
#else
        float len = GetLength();
        if (len > 0.f)
        {
            *this *= (1.f / len);
        }
#endif

    }

#ifdef USE_DXMATH
    XMVECTOR m_vector;
#else
    float m_x, m_y, m_z, m_w;
#endif
};


struct Quaternion
{
public:
    Quaternion()
    {
#ifdef USE_DXMATH
        m_vector = XMVectorZero();
#else
        m_x = m_y = m_z = m_w = 0.f;
#endif
    }

#ifdef USE_DXMATH
    XMVECTOR m_vector;
#else
    float m_x, m_y, m_z, m_w;
#endif
};


struct Plane
{
public:
    Plane()
    {
#ifdef USE_DXMATH
        m_vector = XMVectorZero();
#else
        m_x = m_y = m_z = m_w = 0.f;
#endif
    }

    inline float GetDistance(const Vector& pos) const
    {
#ifdef USE_DXMATH
        float dist = XMVectorGetX(XMPlaneDotCoord(m_vector, pos.m_vector));
#else
        float dist = m_x * pos.m_x + m_y * pos.m_y + m_z * pos.m_z + m_w;
#endif
        return dist;
    }

    float GetDistance(const AlignedBox& pos) const;

#ifdef USE_DXMATH
    XMVECTOR m_vector;
#else
    float m_x, m_y, m_z, m_w;
#endif
};

const constexpr float LargeFloat = 1e20f;

struct AlignedBox
{
public:

    AlignedBox()
    {
        Reset();
    }

    inline void Reset()
    {
        m_min.Set(LargeFloat, LargeFloat, LargeFloat);
        m_max.Set(-LargeFloat, -LargeFloat, -LargeFloat);
    }

    inline bool IsValid() const 
    {
        return m_min.GetX() < m_max.GetX();
    }

    inline void Add(const Vector& pos)
    {
#ifdef USE_DXMATH
        m_max.m_vector = XMVectorMax(m_max.m_vector, pos.m_vector);
        m_min.m_vector = XMVectorMin(m_min.m_vector, pos.m_vector);
#else
        m_max.m_x = __max(m_max.m_x, pos.m_x);
        m_max.m_y = __max(m_max.m_y, pos.m_y);
        m_max.m_z = __max(m_max.m_z, pos.m_z);
        m_min.m_x = __min(m_min.m_x, pos.m_x);
        m_min.m_y = __min(m_min.m_y, pos.m_y);
        m_min.m_z = __min(m_min.m_z, pos.m_z);
#endif
    }

    inline void Add(const Vector& pos, float radius)
    {
#ifdef USE_DXMATH
        XMVECTOR r = XMVectorSet(radius, radius, radius, 0);
        m_max.m_vector = XMVectorMax(m_max.m_vector, XMVectorAdd(pos.m_vector, r));
        m_min.m_vector = XMVectorMin(m_min.m_vector, XMVectorSubtract(pos.m_vector, r));
#else
        Add({ pos.m_x - radius,pos.m_y - radius,pos.m_z - radius });
        Add({ pos.m_x + radius,pos.m_y + radius,pos.m_z + radius });
#endif
    }

    inline void Add(const AlignedBox& box)
    {
#ifdef USE_DXMATH
		m_max.m_vector = XMVectorMax(m_max.m_vector, box.m_max.m_vector);
		m_min.m_vector = XMVectorMin(m_min.m_vector, box.m_min.m_vector);
#else
		m_max.m_x = __max(m_max.m_x, box.m_max.m_x);
		m_max.m_y = __max(m_max.m_y, box.m_max.m_y);
		m_max.m_z = __max(m_max.m_z, box.m_max.m_z);
		m_min.m_x = __min(m_min.m_x, box.m_min.m_x);
		m_min.m_y = __min(m_min.m_y, box.m_min.m_y);
		m_min.m_z = __min(m_min.m_z, box.m_min.m_z);
#endif
    }

    void GetTransformed(AlignedBox& dest, const Matrix4& transform, float uniformScale) const;

    Vector m_min, m_max;
};

struct TileRange
{
    void Reset()
    {
        m_minX =  m_minZ = 127;
        m_maxX = m_maxZ = -127;
    }

    int8_t m_minX, m_minZ, m_maxX, m_maxZ;
};


struct BoundingVolume
{
    void Reset()
    {
        m_box.Reset();
        m_tileRange.Reset();
    }

    inline void Add(const Vector& pos)
    {
        m_box.Add(pos);
    }

    inline void Add(const Vector& pos, float radius)
    {
        m_box.Add(pos, radius);
    }

	inline void Add(const AlignedBox& box)
	{
		m_box.Add(box);
	}

	inline void Add(const BoundingVolume& volume)
	{
		m_box.Add(volume.m_box);
	}

    inline float GetSphereRadius() const
    {
        return m_sphere.GetW();
    }

    inline const Vector& GetCenter() const 
    {
        return m_sphere;
    }

    inline bool IsValid() const
    {
        return GetSphereRadius() > 0.f;
    }

    void GetTransformed(BoundingVolume& dest, const Matrix4& transform, float uniformScale) const;

    void OnUpdated();

    AlignedBox m_box;
    Vector m_sphere; // w==radius
    TileRange m_tileRange = {};
	uint8_t m_visibilityIsland = 0;
	int8_t m_tileAssignmentX = -1;
    int8_t m_tileAssignmentZ = -1;
};

struct Frustum
{
    bool IsSphereVisible(const Vector& pos, float radius) const;
    bool IsBoxVisible(const AlignedBox& box) const;
    bool IsVolumeVisible(const BoundingVolume& vol) const;

    Plane m_planes[6];
};


struct Matrix4
{
public:
    Matrix4()
    {
        SetIdentity();
    }
    void SetIdentity()
    {
#ifdef USE_DXMATH
        m_matrix = XMMatrixIdentity();
#else
        m_row[0].Set(1, 0, 0, 0);
        m_row[0].Set(0, 1, 0, 0);
        m_row[0].Set(0, 0, 1, 0);
        m_row[0].Set(0, 0, 0, 1);
#endif
    }
    void Create(const Vector& pos, const Vector& eulerRot, float scale = 1.0f);
    void Create(const Vector& pos, const Vector& rightDir, const Vector& upDir, const Vector& dir);
    static Matrix4 CreateTransform(const Vector& pos, const Vector& eulerRot, float scale = 1.0f)
    {
        Matrix4 m;
        m.Create(pos, eulerRot, scale);
        return m;
    }
    Vector GetRightDir() const;
    Vector GetUpDir() const;
    Vector GetDirection() const;
    Vector GetTranslation() const;
	void SetTranslation(const Vector& pos);
    void AddTranslation(const Vector& delta);

    void Decompose(Vector& out_pos, Quaternion& out_rot, Vector& out_scale) const;
    Matrix4 GetTransposed() const;
    Matrix4 GetInverse() const;
    void Store(float* dest) const;
    void Load(const float* source);

    void CreateProjection(float aspectRatio, float fovAngleY = 70.0f * Math::Pi / 180.0f, float nearClip = 0.05f, float farClip = 30.0f);
    void BuildFrustum(Frustum& frustum) const;

    Matrix4 operator* (const Matrix4& rhs) const;

    Vector TransformPosition(const Vector& pos) const;
    Vector TransformDirection(const Vector& dir) const;

#ifdef USE_DXMATH
    XMMATRIX m_matrix;
#else
    Vector m_row[4];
#endif
};

struct Matrix4Storage
{
    XMFLOAT4X4 m_matrix;

    void Set(const Matrix4& m);
    void SetTransposed(const Matrix4& m);
};

struct Float4Storage
{
    float x, y, z, w;
};

