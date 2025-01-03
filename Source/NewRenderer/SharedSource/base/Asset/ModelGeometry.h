#pragma once
#include <DesperabisCommon.h>
#include <Asset/LevelInfo.h>

struct VertexPositionI16
{
    short x, y, z;
};

struct Vector2
{
    float x, y;

    bool operator == (const Vector2& rhs) const { return x == rhs.x && y == rhs.y; }
    bool operator != (const Vector2& rhs) const { return !(*this == rhs); }
};

struct Vector3
{
    float x, y, z;
    
    inline Vector3& operator = (const Vector& rhs)
    {
        x = rhs.GetX();
        y = rhs.GetY();
        z = rhs.GetZ();
        return *this;
    }


    bool operator == (const Vector3& rhs) const {return x == rhs.x && y == rhs.y && z == rhs.z;}
    bool operator != (const Vector3& rhs) const { return !(*this == rhs); }
};

struct Vector4
{
    float x, y, z, w;

    inline Vector4& operator = (const VertexPositionI16& rhs)
    {
        const float scale = 1.0f * DespVertToFloat;
        x = (float)rhs.x * -scale;
        y = (float)rhs.y * -scale;
        z = (float)rhs.z * scale;
        w = 1.0f;
        return *this;
    }

    inline Vector4& operator = (const Vector3& rhs)
    {
        x = rhs.x;
        y = rhs.y;
        z = rhs.z;
        w = 1.0f;
        return *this;
    }

	inline operator Vector3()
	{
        return { x,y,z };
	}

    bool operator == (const Vector4& rhs) const { return x == rhs.x && y == rhs.y && z == rhs.z && w == rhs.w;}
    bool operator != (const Vector4& rhs) const { return !(*this == rhs); }

};

using VertexPosition = Vector4;

struct SpriteParams
{
    Vector2 m_topLeft;
    Vector2 m_bottomRight;

    inline void Store(Float4Storage& dst) const 
    {
        dst = { m_topLeft.x, m_topLeft.y, m_bottomRight.x, m_bottomRight.y };
    }
};

struct TextureTransform
{
    float scaleU, scaleV, ofsU, ofsV;
    void Reset()
    {
        scaleU = scaleV = 1.0f;
        ofsU = ofsV = 0.0f;
    }
    Vector2 Apply(float u, float v) const 
    {
        return { u * scaleU + ofsU, v * scaleV + ofsV };
    }
    inline void Store(Float4Storage& dst) const
    {
        dst = { scaleU, scaleV, ofsU, ofsV };
    }
};

struct Color4
{
    void Set(byte R, byte G, byte B, byte A = 255)
    {
        r = R;
        g = G;
        b = B;
        a = A;
    }

    Color4 operator* (const Color4& rhs) const
    {
        Color4 res;
        res.r = (int)r * (int)rhs.r / 255;
        res.g = (int)g * (int)rhs.g / 255;
        res.b = (int)b * (int)rhs.b / 255;
        res.a = (int)a * (int)rhs.a / 255;
        return res;
    }

    Vector4 ToFloat(float multiplier=1.f) const;
    void ToFloat(float* storage, float multiplier = 1.f) const;

    byte r, g, b, a;

};


enum class VertexStretchMode
{
    None = 0,
    Sprite = 1,
    TexturedLine = 2,
    FlameSprite = 3,
};

enum class ShaderType
{
    Normal,
    Mirror
};

#define USE_VERTEX_COLOR

struct ModelVertex
{
    void SetUntextured(uint16_t v0, const Color4& color);
    void SetTextured(uint16_t v0, const Color4& color, const Vector2& tc);
    void SetSprite(uint16_t v0, const Color4& color, const Vector2& corner, float scale, bool flipU, bool isFlame = false);
    void SetTexturedLine(uint16_t v0, uint16_t v1, const Color4& color, const Vector2& tc, float strength);

    inline int GetPositionIndex1() const
    {
        return (int)PositionIndex;
    }
    inline int GetPositionIndex2() const
    {
        return (int)SecondPosition;
    }
    //   Vector3 Normal;
#ifdef USE_VERTEX_COLOR
    Color4 Color;
#endif
    uint16_t PositionIndex;
    uint16_t SecondPosition;
	uint16_t StretchMode;
    uint16_t Padding;

    Vector2 TexCoord;
    Vector2 StretchParams;
#ifdef ENABLE_ADVANCED_RENDERING
    Vector3 Normal;
#endif
};


enum class PolygonType
{
    Unknown = 0,
    Points = 1,
    Lines = 2,
    SolidColorTriangle = 3,
    TexturedWall = 4,
    Sprite = 5,
    TexturedQuad = 6,
    TexturedLine = 7,
    DarkenVertex = 8,
    AttachmentBillboard = 9,
    Attachment3DModel = 10,
};

enum class TriangleBlendType : uint8_t
{
    SolidColor = 0,
    Transparent = 1,
    Refraction = 2,
    Reflection = 3
};

struct ModelPolygon
{
    uint8_t something1;
    uint8_t something2;
    uint8_t i0, i1, i2, i3;

    PolygonType GetPolyType() const
    {
        return (PolygonType)(something1 & 15);
    }

    uint8_t GetTextureIndex() const
    {
        return (uint8_t)(something2 & 15);
    }

    uint8_t GetSolidColorIndex() const
    {
        return something2;
    }

    TriangleBlendType GetTriangleBlendType() const 
    {
        return (TriangleBlendType)i3;
    }

    // only when GetTriangleBlendType != solidcolor
    WaterReflectionType GetTriangleReflectionType() const;

    bool IsTextureFlipped() const
    {
        return (something2 & 128) > 0;
    }

    bool IsTransparent() const
    {
        return (something2 & 127) > 32; 
    }

    // ?
    int BrightnessModifier() const 
    {
        return (int)something1 >> 4; // 0..15
    }

    // cannot find any of these
    bool SomethingNew() const
    {
        return (something2 & 16) > 0;
    }

};


struct ModelLoadingContext
{
    // before conversion
    std::vector<ModelPolygon> m_polygons;
    std::vector<VertexPosition> m_vertexPos;
};

struct ModelConversionContext
{
    ModelConversionContext(const ModelLoadingContext& input)
        : m_input(input)
    {

    }
    void AddToBounds(const VertexPosition& pos, float rad);
    void GeneratePolygonNormals(const uint16_t* firstIndex, int numIndices, int positionIndexOfs);
    void GeneratePerTriangleNormals(const uint16_t* firstIndex, int numIndices, int positionIndexOfs);
    void GenerateBoundingVolume(BoundingVolume& vol) const;
    void GeneratePhysicsGeometry(const std::vector<DrawCall>& drawCalls, TriangularShapeDesc& out_desc);

    const ModelLoadingContext& m_input;
    // after conversion
    std::vector<ModelVertex> m_vertices;
    std::vector<uint16_t> m_indices;
    std::vector<uint16_t> m_sortedIndices;
    int m_modelPositionOfs = 0;
    int m_numPositions = 0;
    BoundingVolume m_visibilityBox;
    std::vector<Vector> m_perVertexNormal;
    std::vector<uint16_t> m_physicsIndices;
};


constexpr const int MaxAddressablePositions = (1 << 16) / sizeof(VertexPosition); // size of constant buffer
constexpr const int MaxAddressableAnimatedPositions = 256; //  byte representation

namespace GeometryUtils
{
	void Convert(class Model& model, ModelConversionContext& context);
	void ConvertPolygons(class Model& model, ModelConversionContext& context, std::vector<DrawCall>& drawcalls);
	void AddModelReferences(class Model& model, ModelConversionContext& context);
    void OptimizeDrawCalls(std::vector<DrawCall>& newDrawCalls, std::vector<uint16_t>& newIndices, const std::vector<uint16_t>& indices, const std::vector<DrawCall>& drawCalls);
    void SmoothNormals(ModelConversionContext& context, const std::vector<DrawCall>& drawCalls, int vertexOfs);

    std::shared_ptr<Renderable> CreateSpriteMesh(bool flipU);
    std::shared_ptr<Renderable> CreateCubeMesh(float radiusX, float radiusY, float radiusZ, const Color4& col = { 255,255,255,255 }, float thickness = 0.75f);
    std::shared_ptr<Renderable> CreateDebugGeometry(const std::vector<DebugLine>& lineBuffer);
    std::shared_ptr<Renderable> CreateBurningObjectMesh(int maxPositions);

    // returns the radius of bounding sphere
    void ComputeBoundingVolume(BoundingVolume& vol, const std::vector<DrawCall>& drawCalls, const std::vector<ModelVertex>& vertexArray, const std::vector<VertexPosition>& positionArray, const std::vector<uint16_t>& indexArray);
};
