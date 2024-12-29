#include "pch.h"
#include <Asset/ModelGeometry.h>
#include <Asset/Model.h>
#include <Asset/ModelAnim.h>
#include <Asset/Palette.h>
#include <Asset/AssetManager.h>
#include <Asset/DebugGeometry.h>
#include <Renderer/DrawCall.h>

constexpr const bool g_performanceOptimizationsEnabled = true;


void Color4::ToFloat(float* storage, float multiplier) const
{
	multiplier *= (1.f / 255.f);
    storage[0] = (float)r * multiplier;
    storage[1] = (float)g * multiplier;
    storage[2] = (float)b * multiplier;
    storage[3] = (float)a * multiplier;
}


Vector4 Color4::ToFloat(float multiplier/*=1.f*/) const
{
    Vector4 v;
    ToFloat(&v.x, multiplier);
    return v;
}


void ModelVertex::SetUntextured(uint16_t v0, const Color4& color)
{
    assert(v0 < MaxAddressablePositions);
    PositionIndex = v0;
    SecondPosition = 0;
    StretchMode = (uint16_t)VertexStretchMode::None;

    //Normal = { 1,0,0 };
#ifdef USE_VERTEX_COLOR
    Color = color;
#endif
    TexCoord.x = TexCoord.x = 0;
    StretchParams = { 0,0 };
}

void ModelVertex::SetTextured(uint16_t v0, const Color4& color, const Vector2& tc)
{
    assert(v0 < MaxAddressablePositions);
	PositionIndex = v0;
    SecondPosition = 0;
    StretchMode = (uint16_t)VertexStretchMode::None;

    //Normal = { 1,0,0 };
#ifdef USE_VERTEX_COLOR
    Color = color;
#endif
    TexCoord = tc;
    StretchParams = { 0,0 };
}

void ModelVertex::SetSprite(uint16_t v0, const Color4& color, const Vector2& corner, float scale, bool flipU, bool isFlame)
{
    assert(v0 < MaxAddressablePositions);
	PositionIndex = v0;
    SecondPosition = 0;
    StretchMode = isFlame ? (uint16_t)VertexStretchMode::FlameSprite : (uint16_t)VertexStretchMode::Sprite;

    //Normal = { 1,0,0 };
#ifdef USE_VERTEX_COLOR
    Color = color;
#endif
    TexCoord = { flipU ? corner.x : (1.f - corner.x), corner.y };
    StretchParams = { corner.x, scale };
}

void ModelVertex::SetTexturedLine(uint16_t v0, uint16_t v1, const Color4& color, const Vector2& tc, float strength)
{
    assert(v0 < MaxAddressablePositions);
    assert(v1 < MaxAddressableAnimatedPositions);
	PositionIndex = v0;
    SecondPosition = v1;
    StretchMode = (uint16_t)VertexStretchMode::TexturedLine;

    //Normal = { 1,0,0 };
#ifdef USE_VERTEX_COLOR
    Color = color;
#endif
    TexCoord = tc;
    StretchParams = { strength, 0 };
}


void ModelConversionContext::AddToBounds(const VertexPosition& pos, float rad)
{
    m_visibilityBox.Add(Vector(pos.x, pos.y, pos.z), rad);
}

void ModelConversionContext::GeneratePolygonNormals(const uint16_t* firstIndex, int numIndices, int positionIndexOfs)
{
#ifdef ENABLE_ADVANCED_RENDERING
    uint16_t idx0 = firstIndex[0];
    uint16_t idx1 = firstIndex[1];
    uint16_t idx2 = firstIndex[2];
    ModelVertex& mv0 = m_vertices[idx0];
    ModelVertex& mv1 = m_vertices[idx1];
    ModelVertex& mv2 = m_vertices[idx2];

    const VertexPosition& p0 = m_input.m_vertexPos[mv0.GetPositionIndex1() + positionIndexOfs];
    const VertexPosition& p1 = m_input.m_vertexPos[mv1.GetPositionIndex1() + positionIndexOfs];
    const VertexPosition& p2 = m_input.m_vertexPos[mv2.GetPositionIndex1() + positionIndexOfs];
    Vector d1(p0.x - p1.x, p0.y - p1.y, p0.z - p1.z);
    Vector d2(p2.x - p1.x, p2.y - p1.y, p2.z - p1.z);
    Vector nrml = d2.GetCross(d1);
    nrml.Normalize();


    for (int i = 0; i < numIndices; i++)
    {
        ModelVertex& mv = m_vertices[firstIndex[i]];
        mv.Normal = nrml;
    }

    /*
    for (auto& mv : m_vertices)
    {
        mv.Normal = { 0,0,1 };
    }
    */
#endif
}

void ModelConversionContext::GeneratePerTriangleNormals(const uint16_t* firstIndex, int numIndices, int positionIndexOfs)
{
    for (int i = 0; i < numIndices; i += 3, firstIndex += 3)
    {
        GeneratePolygonNormals(firstIndex, 3, positionIndexOfs);
    }
}

void ModelConversionContext::GenerateBoundingVolume(BoundingVolume& vol) const
{
    vol = m_visibilityBox;
    vol.OnUpdated();
}


void ModelConversionContext::GeneratePhysicsGeometry(const std::vector<DrawCall>& drawCalls, TriangularShapeDesc& out_desc)
{
    if (drawCalls.size() == 0)
    {
        out_desc = TriangularShapeDesc();
        return;
    }
    if (m_numPositions == 0)
    {
		out_desc.m_numVertices = (int)m_input.m_vertexPos.size();
    }
    else
    {
		out_desc.m_numVertices = m_numPositions;
    }
    out_desc.m_vertexData = &m_input.m_vertexPos[m_modelPositionOfs].x;
    out_desc.m_vertexStride = sizeof(VertexPosition);

    m_physicsIndices.clear();
    for (const DrawCall& dc : drawCalls)
    {
        int lastIndex = dc.m_firstIndex + dc.m_primitiveCount * 3;
        for (int i = dc.m_firstIndex; i < lastIndex; i++)
        {
            const ModelVertex& mv(m_vertices[m_sortedIndices[i]]);
            if ((VertexStretchMode)mv.StretchMode != VertexStretchMode::None)
                continue;        
            m_physicsIndices.push_back((uint16_t)mv.GetPositionIndex1());
        }
    }

    out_desc.m_indexData = m_physicsIndices.data();
    out_desc.m_numTriangles = (int)m_physicsIndices.size() / 3;
}

namespace
{
    Color4 ColorFromBrightness(int indexPlusOne)
    {
        return Color4{ 255,255,255,0 }; // TODO
    }

    Color4 ColorFromBrightness(int indexPlusOne, const Color4& solidColor)
    {
        return solidColor; // TODO
    }


    void AddPoint(ModelConversionContext& context, uint16_t i0, float width, const Color4& solidCol)
    {
        const VertexPosition* vertexPositions = &context.m_input.m_vertexPos[context.m_modelPositionOfs];
        auto& vertices(context.m_vertices);
        auto& indices(context.m_indices);

        int vertexStart = (int)vertices.size();

        uint16_t p = i0 - 1;
        context.AddToBounds(vertexPositions[p], 0.f);
        Color4 col = ColorFromBrightness(i0, solidCol);

        vertices.emplace_back().SetSprite(p, col, Vector2{ 0, 1 }, width, false); // left/bottom
        vertices.emplace_back().SetSprite(p, col, Vector2{ 1, 1 }, width, false); // right/bottom
        vertices.emplace_back().SetSprite(p, col, Vector2{ 1, 0 }, width, false);// right/top
        vertices.emplace_back().SetSprite(p, col, Vector2{ 0, 0 }, width, false);// left/top

        indices.push_back((uint16_t)(vertexStart + 0));
        indices.push_back((uint16_t)(vertexStart + 1));
        indices.push_back((uint16_t)(vertexStart + 2));

        indices.push_back((uint16_t)(vertexStart + 0));
        indices.push_back((uint16_t)(vertexStart + 2));
        indices.push_back((uint16_t)(vertexStart + 3));

    }


    void AddLine(ModelConversionContext& context, uint16_t i0, uint16_t i1, float scale, const Color4& solidColor)
    {
        const VertexPosition* vertexPositions = &context.m_input.m_vertexPos[context.m_modelPositionOfs];
        auto& vertices(context.m_vertices);
        auto& indices(context.m_indices);

        int vertexStart = (int)vertices.size();

        float u0 = 0.0f;
        float u1 = 1.0f - u0;

        uint16_t p0 = i0 - 1;
        uint16_t p1 = i1 - 1;
        context.AddToBounds(vertexPositions[p0], 0.f);
        context.AddToBounds(vertexPositions[p1], 0.f);

        vertices.emplace_back().SetTexturedLine(p0, p1, ColorFromBrightness(i0, solidColor), Vector2{ u0, u0 }, -scale);
        vertices.emplace_back().SetTexturedLine(p1, p0, ColorFromBrightness(i1, solidColor), Vector2{ u0, u1 }, scale);
        vertices.emplace_back().SetTexturedLine(p1, p0, ColorFromBrightness(i1, solidColor), Vector2{ u1, u1 }, -scale);
        vertices.emplace_back().SetTexturedLine(p0, p1, ColorFromBrightness(i0, solidColor), Vector2{ u1, u0 }, scale);

        indices.push_back((uint16_t)(vertexStart + 2));
        indices.push_back((uint16_t)(vertexStart + 1));
        indices.push_back((uint16_t)vertexStart);

        indices.push_back((uint16_t)(vertexStart + 3));
        indices.push_back((uint16_t)(vertexStart + 2));
        indices.push_back((uint16_t)vertexStart);
    }


    void AddSolidColorTriangle(ModelConversionContext& context, uint16_t i0, uint16_t i1, uint16_t i2, const Color4& solidColor)
    {
        const VertexPosition* vertexPositions = &context.m_input.m_vertexPos[context.m_modelPositionOfs];
        auto& vertices(context.m_vertices);
        auto& indices(context.m_indices);

        int vertexStart = (int)vertices.size();
        uint16_t p0 = i0 - 1;
        uint16_t p1 = i1 - 1;
        uint16_t p2 = i2 - 1;
        context.AddToBounds(vertexPositions[p0], 0.f);
        context.AddToBounds(vertexPositions[p1], 0.f);
        context.AddToBounds(vertexPositions[p2], 0.f);

        vertices.emplace_back().SetUntextured(p0, ColorFromBrightness(i0, solidColor));
        vertices.emplace_back().SetUntextured(p1, ColorFromBrightness(i1, solidColor));
        vertices.emplace_back().SetUntextured(p2, ColorFromBrightness(i2, solidColor));
        //Vertex.GenerateNormals(vertices, vertexStart, 3, false);

        indices.push_back((uint16_t)(vertexStart + 2));
        indices.push_back((uint16_t)(vertexStart + 1));
        indices.push_back((uint16_t)(vertexStart + 0));

        context.GeneratePolygonNormals(&indices[indices.size() - 3], 3, context.m_modelPositionOfs);
    }

    void AddSprite(ModelConversionContext& context, uint16_t i0, float scale, bool flipU)
    {
        const VertexPosition* vertexPositions = &context.m_input.m_vertexPos[context.m_modelPositionOfs];
        auto& vertices(context.m_vertices);
        auto& indices(context.m_indices);

        int vertexStart = (int)vertices.size();

        uint16_t p = i0 - 1;
        {
            float r = scale * 100.f; // max radius extent of the sprite
            context.AddToBounds(vertexPositions[p], r);
        }

        Color4 col = ColorFromBrightness(i0);
        vertices.emplace_back().SetSprite(p, col, Vector2 { 0, 1 }, scale, flipU); // left/bottom
        vertices.emplace_back().SetSprite(p, col, Vector2 { 1, 1 }, scale, flipU); // right/bottom
        vertices.emplace_back().SetSprite(p, col, Vector2 { 1, 0 }, scale, flipU);// right/top
        vertices.emplace_back().SetSprite(p, col, Vector2 { 0, 0 }, scale, flipU);// left/top

        indices.push_back((uint16_t)(vertexStart + 0));
        indices.push_back((uint16_t)(vertexStart + 1));
        indices.push_back((uint16_t)(vertexStart + 2));

        indices.push_back((uint16_t)(vertexStart + 0));
        indices.push_back((uint16_t)(vertexStart + 2));
        indices.push_back((uint16_t)(vertexStart + 3));
    }

	void AddFlame(ModelConversionContext& context, uint16_t i0, float scale)
	{
		const VertexPosition* vertexPositions = &context.m_input.m_vertexPos[context.m_modelPositionOfs];
		auto& vertices(context.m_vertices);
		auto& indices(context.m_indices);

		int vertexStart = (int)vertices.size();

		uint16_t p = i0 - 1;
		{
			float r = scale * 100.f; // max radius extent of the sprite
			context.AddToBounds(vertexPositions[p], r);
		}

        bool flipU = (i0 & 1) > 0;
        Color4 col = { 255,255,255,255 };
		vertices.emplace_back().SetSprite(p, col, Vector2{ 0, 1 }, scale, flipU, true); // left/bottom
		vertices.emplace_back().SetSprite(p, col, Vector2{ 1, 1 }, scale, flipU, true); // right/bottom
		vertices.emplace_back().SetSprite(p, col, Vector2{ 1, 0 }, scale, flipU, true);// right/top
		vertices.emplace_back().SetSprite(p, col, Vector2{ 0, 0 }, scale, flipU, true);// left/top

		indices.push_back((uint16_t)(vertexStart + 0));
		indices.push_back((uint16_t)(vertexStart + 1));
		indices.push_back((uint16_t)(vertexStart + 2));

		indices.push_back((uint16_t)(vertexStart + 0));
		indices.push_back((uint16_t)(vertexStart + 2));
		indices.push_back((uint16_t)(vertexStart + 3));
	}

    void AddTexturedQuad(ModelConversionContext& context, uint16_t i0, uint16_t i1, uint16_t i2, uint16_t i3, const TextureTransform& textureAtlas, bool flipU, bool wall)
    {
        const VertexPosition* vertexPositions = &context.m_input.m_vertexPos[context.m_modelPositionOfs];
        auto& vertices(context.m_vertices);
        auto& indices(context.m_indices);

        int vertexStart = (int)vertices.size();
        float u0 = flipU ? 1.0f : 0.0f;
        float u1 = 1.0f - u0;
        uint16_t p0 = i0 - 1;
        uint16_t p1 = i1 - 1;
        uint16_t p2 = i2 - 1;
        uint16_t p3 = i3 - 1;
        context.AddToBounds(vertexPositions[p0], 0);
        context.AddToBounds(vertexPositions[p1], 0);
        context.AddToBounds(vertexPositions[p2], 0);
        context.AddToBounds(vertexPositions[p3], 0);

        if (wall)
        {
            float v0 = 0, v1 = 0, v2 = 1, v3 = 1;
/*
            if (true) // vertical planar mapping - does not work here
            {
                float yToV = -400 * DespToFloat;
                v0 = vertexPositions[p0].y * yToV;
                v1 = vertexPositions[p1].y * yToV;
                v2 = vertexPositions[p2].y * yToV;
                v3 = vertexPositions[p3].y * yToV;
            }
*/
            vertices.emplace_back().SetTextured(p0, ColorFromBrightness(i0), textureAtlas.Apply(v0, u0));
            vertices.emplace_back().SetTextured(p1, ColorFromBrightness(i1), textureAtlas.Apply(v1, u1));
            vertices.emplace_back().SetTextured(p2, ColorFromBrightness(i2), textureAtlas.Apply(v2, u1));
            vertices.emplace_back().SetTextured(p3, ColorFromBrightness(i3), textureAtlas.Apply(v3, u0));
        }
        else
        {
            vertices.emplace_back().SetTextured(p0, ColorFromBrightness(i0), textureAtlas.Apply(u0, 0));
            vertices.emplace_back().SetTextured(p1, ColorFromBrightness(i1), textureAtlas.Apply(u1, 0));
            vertices.emplace_back().SetTextured(p2, ColorFromBrightness(i2), textureAtlas.Apply(u1, 1));
            vertices.emplace_back().SetTextured(p3, ColorFromBrightness(i3), textureAtlas.Apply(u0, 1));
        }

        //Vertex.GenerateNormals(vertices, vertexStart, 4, false);

        indices.push_back((uint16_t)(vertexStart + 0));
        indices.push_back((uint16_t)(vertexStart + 1));
        indices.push_back((uint16_t)(vertexStart + 2));

        indices.push_back((uint16_t)(vertexStart + 0));
        indices.push_back((uint16_t)(vertexStart + 2));
        indices.push_back((uint16_t)(vertexStart + 3));

        context.GeneratePolygonNormals(&indices[indices.size() - 6], 6, context.m_modelPositionOfs);
    }


    void AddTexturedLine(ModelConversionContext& context, uint16_t i0, uint16_t i1, float width0, float width1, const TextureTransform& textureAtlas, bool flipU)
    {
        const VertexPosition* vertexPositions = &context.m_input.m_vertexPos[context.m_modelPositionOfs];
        auto& vertices(context.m_vertices);
        auto& indices(context.m_indices);

        //flipU = false;
        int vertexStart = (int)vertices.size();
        float u0 = flipU ? 1.0f : 0.0f;
        float u1 = 1.0f - u0;

        uint16_t p0 = i0 - 1;
        uint16_t p1 = i1 - 1;
        context.AddToBounds(vertexPositions[p0], width0);
        context.AddToBounds(vertexPositions[p1], width1);

        vertices.emplace_back().SetTexturedLine(p0, p1, ColorFromBrightness(i0), textureAtlas.Apply(u0, 0), -width0);
        vertices.emplace_back().SetTexturedLine(p1, p0, ColorFromBrightness(i1), textureAtlas.Apply(u0, 1), width1);
        vertices.emplace_back().SetTexturedLine(p1, p0, ColorFromBrightness(i1), textureAtlas.Apply(u1, 1), -width1);
        vertices.emplace_back().SetTexturedLine(p0, p1, ColorFromBrightness(i0), textureAtlas.Apply(u1, 0),  width0);

        indices.push_back((uint16_t)(vertexStart + 2));
        indices.push_back((uint16_t)(vertexStart + 1));
        indices.push_back((uint16_t)vertexStart);

        indices.push_back((uint16_t)(vertexStart + 3));
        indices.push_back((uint16_t)(vertexStart + 2));
        indices.push_back((uint16_t)vertexStart);
    }

} // namespace


void GeometryUtils::ConvertPolygons(class Model& model, ModelConversionContext& context, std::vector<DrawCall>& drawcalls)
{
    const Palette& palette = AssetManager::Instance().GetDefaultPalette();

    auto GetTexture = [&](int idx, TextureTransform& textureAtlas) -> Texture*
    {
        if ((model.m_owner->m_textureUsedMask & (1 << idx)) == 0)
        {
            textureAtlas = { 1,1,0,0 };
            return nullptr;
        }
        Texture* tex = &model.m_owner->m_ownTextures[idx];
        textureAtlas = tex->m_atlasData;
        return tex;
    };

    auto& vertices(context.m_vertices);
    auto& indices(context.m_indices); // input indices (unsorted)
    int indexOfs = (int)context.m_sortedIndices.size(); // output indices (resorted)

    // temporary drawcall list
    const TextureTransform identityAtlas = { 1,1,0,0 };
    TextureTransform textureAtlas = identityAtlas;

    // naive conversion
    for (int i = 0; i < model.m_numPolygons; i++)
    {
        DrawCall dc;

        const ModelPolygon& p = context.m_input.m_polygons[model.m_polygonOfs + i];

        auto AddDrawcall = [&]()
        {
            /*
            if (p.SomethingNew())
            {
                dc.m_material.m_applyLighting = false;
                dc.m_material.m_texture = AssetManager::Instance().GetPlainWhiteTexture();
            }
            */
            drawcalls.push_back(dc);
        };

        switch (p.GetPolyType())
        {
        case PolygonType::Points:
        {
            const Color4& solidColor = palette.GetEntryCustomAlpha(p.GetSolidColorIndex());
            dc.m_material.m_textureSlot = 2;
            dc.m_material.m_texture = AssetManager::Instance().GetPointTexture();
            dc.m_material.m_useAlphaTest = true;
            dc.m_material.m_applyLighting = true;

            dc.m_firstIndex = (int)indices.size();
            float scale = 6.f * DespVertToFloat;
            AddPoint(context, p.i0, scale, solidColor);
            dc.m_primitiveCount = ((int)indices.size() - dc.m_firstIndex) / 3;
            AddDrawcall();
        }
        break;
        case PolygonType::Lines:
        {
            const Color4& solidColor = palette.GetEntryCustomAlpha(p.GetSolidColorIndex());
            dc.m_material.m_textureSlot = -1;
            dc.m_material.m_texture = AssetManager::Instance().GetPlainWhiteTexture();
            dc.m_material.m_useAlphaTest = false;
            dc.m_material.m_applyLighting = true;
            dc.m_firstIndex = (int)indices.size();
            float scale = DespVertToFloat * 1.0f;
            AddLine(context, p.i0, p.i1, scale, solidColor);
            dc.m_primitiveCount = ((int)indices.size() - dc.m_firstIndex) / 3;
            AddDrawcall();
        }
        break;
        case PolygonType::SolidColorTriangle:
        {
            uint8_t colorIndex = p.GetSolidColorIndex();
            Color4 solidColor = palette.GetEntryCustomAlpha(colorIndex);
            WaterReflectionType reflType = p.GetTriangleReflectionType();
            dc.m_material.m_texture = nullptr;
            dc.m_material.m_textureSlot = -1;
            dc.m_material.m_texture = AssetManager::Instance().GetPlainWhiteTexture();
            dc.m_firstIndex = (int)indices.size();
            dc.m_material.m_useAlphaTest = false;
            dc.m_material.m_applyLighting = true;

            switch (p.GetTriangleBlendType())
            {
            case TriangleBlendType::SolidColor:break;
            case TriangleBlendType::Transparent:
                dc.m_material.m_transparencyType = TransparencyMode::AlphaBlended;
                dc.m_material.m_opacity = 0.5f;
                DrawCallMaterial::GetWaterTintColor(reflType, solidColor);
                break;
            case TriangleBlendType::Refraction:
                solidColor = { 0,255,0,255 };
                dc.m_material.m_reflectionType = reflType | WaterReflectionType::RefractionBit;
                dc.m_material.m_transparencyType = TransparencyMode::Water;
                DrawCallMaterial::GetWaterTintColor(reflType, solidColor);
                break;
            case TriangleBlendType::Reflection:
            {
                const VertexPosition* vertexPositions = &context.m_input.m_vertexPos[context.m_modelPositionOfs];
                uint16_t p0 = p.i0 - 1;
                solidColor = { 255,0,0,255 };
                dc.m_material.m_reflectionType = reflType | WaterReflectionType::ReflectionBit;
                dc.m_material.m_transparencyType = TransparencyMode::Water;
                dc.m_material.m_mirrorHeight = vertexPositions[p0].y; // take the vertex position of the first triangle position as mirror level
                DrawCallMaterial::GetWaterTintColor(reflType, solidColor);
            }   break;
            default:
                assert(!"unsupported triangle type");
                break;
            }

            // doublesided with two normals
            AddSolidColorTriangle(context, p.i0, p.i1, p.i2, solidColor);
            AddSolidColorTriangle(context, p.i2, p.i1, p.i0, solidColor);
            dc.m_primitiveCount = ((int)indices.size() - dc.m_firstIndex) / 3;
            AddDrawcall();
        }
        break;
        case PolygonType::TexturedWall:
        {
            Texture* tex = GetTexture(p.GetTextureIndex(), textureAtlas);
            if (p.IsTransparent())
            {
                dc.m_material.m_textureSlot = -1;
                dc.m_material.m_texture = AssetManager::Instance().GetPointTexture();
            }
            else
            {
                dc.m_material.m_textureSlot = (char)(p.GetTextureIndex() + 1);
                dc.m_material.m_texture = tex != nullptr ? &model.m_owner->m_textureAtlas : nullptr;
            }
            dc.m_firstIndex = (int)indices.size();
            dc.m_material.m_useAlphaTest = true;
            dc.m_material.m_applyLighting = true;
            AddTexturedQuad(context, p.i0, p.i1, p.i3, p.i2, textureAtlas, p.IsTextureFlipped(), true);
            dc.m_primitiveCount = ((int)indices.size() - dc.m_firstIndex) / 3;
            AddDrawcall();
        }
        break;
        case PolygonType::Sprite:
        {
            Texture* tex = GetTexture(p.GetTextureIndex(), textureAtlas);
            dc.m_material.m_textureSlot = (char)(p.GetTextureIndex() + 1);
            dc.m_material.m_texture = tex; // do use the single frame
            dc.m_material.m_useAlphaTest = true;
            dc.m_firstIndex = (int)indices.size();
            dc.m_material.m_applyLighting = p.i2 == 0;

            float scale = (float)p.i1 * DespSpriteToFloat;

            AddSprite(context, p.i0, scale, p.IsTextureFlipped());
            dc.m_primitiveCount = ((int)indices.size() - dc.m_firstIndex) / 3;
            AddDrawcall();
        }
        break;
        case PolygonType::TexturedQuad:
        {
            Texture* tex = GetTexture(p.GetTextureIndex(), textureAtlas);
            dc.m_material.m_textureSlot = (char)(p.GetTextureIndex() + 1);
            dc.m_material.m_texture = tex != nullptr ? &model.m_owner->m_textureAtlas : nullptr;
            dc.m_material.m_useAlphaTest = true;
            dc.m_material.m_applyLighting = true;
            dc.m_firstIndex = (int)indices.size();
            AddTexturedQuad(context, p.i0, p.i1, p.i2, p.i3, textureAtlas, p.IsTextureFlipped(), false);
            dc.m_primitiveCount = ((int)indices.size() - dc.m_firstIndex) / 3;
            AddDrawcall();
        }
        break;
        case PolygonType::TexturedLine:
        {
            Texture* tex = GetTexture(p.GetTextureIndex(), textureAtlas);
            dc.m_material.m_textureSlot = (char)(p.GetTextureIndex() + 1);
            dc.m_material.m_texture = tex != nullptr ? &model.m_owner->m_textureAtlas : nullptr;
            dc.m_material.m_useAlphaTest = true;
            dc.m_material.m_applyLighting = true;
            dc.m_firstIndex = (int)indices.size();
            float scale = DespVertToFloat;
            float width0 = (float)p.i2 * scale;
            float width1 = (float)p.i3 * scale;
            AddTexturedLine(context, p.i0, p.i1, width0, width1, textureAtlas, p.IsTextureFlipped());
            dc.m_primitiveCount = ((int)indices.size() - dc.m_firstIndex) / 3;
            AddDrawcall();
        }
        break;

        case PolygonType::DarkenVertex:
        {
            // already considered at load time
        }
        break;

        case PolygonType::AttachmentBillboard:
            model.m_numUnsupportedPrimitives++;
            // TODO
            break;
        case PolygonType::Attachment3DModel:
            // TODO
            { // real model

				int modelAndId = p.something2 - 1;
				int frame = modelAndId % 5;
				int anim = modelAndId / 5;
                if (const Model* refModel = model.m_owner->GetModelSafe(anim, frame))
                {
                    SubRenderable& sub = model.m_subRenderables.emplace_back();
                    Vector4 attachmentPos = context.m_input.m_vertexPos[model.m_positionOfs + p.i0 - 1];
					sub.m_localPos.Set(attachmentPos.x, attachmentPos.y, attachmentPos.z);
                    sub.m_localOri.Set(0, 0, 0);
                    sub.m_renderable = refModel;
                }
                else
                { // dummy sprite
                    Texture* tex = AssetManager::Instance().GetPointTexture();
                    dc.m_material.m_textureSlot = 0;
                    dc.m_material.m_texture = tex;
                    dc.m_material.m_useAlphaTest = true;
                    dc.m_firstIndex = (int)indices.size();
                    dc.m_material.m_applyLighting = 0;
                    float scale = 200.f * DespSpriteToFloat;
                    AddSprite(context, p.i0, scale, false);
                    dc.m_primitiveCount = ((int)indices.size() - dc.m_firstIndex) / 3;
                    AddDrawcall();
                }
            }
            break;
        default:
            model.m_numUnsupportedPrimitives++;
            break;
        }
    }
}


void GeometryUtils::AddModelReferences(Model& model, ModelConversionContext& context)
{
    if (model.m_subRenderables.size() > 0)
    {
        // just increase this model by the size of the submodel 
        for (const auto& subModel : model.m_subRenderables)
        {
            Matrix4 local;
            local.Create(subModel.m_localPos, subModel.m_localOri, 1.0f);
            BoundingVolume subBounds;
            subModel.m_renderable->m_localBounds.GetTransformed(subBounds, local, 1.f);
            model.m_localBounds.Add(subBounds);
        }

        model.m_localBounds.OnUpdated();
    }
}


void GeometryUtils::Convert(Model& model, ModelConversionContext& context)
{
    context.m_visibilityBox.Reset(); // reset per model

    // temporary drawcall list
    std::vector<DrawCall> dcs;
    model.m_numUnsupportedPrimitives = 0;
    GeometryUtils::ConvertPolygons(model, context, dcs);
    GeometryUtils::OptimizeDrawCalls(model.m_drawCalls, context.m_sortedIndices, context.m_indices, dcs);
    if (model.m_drawCalls.size() == 0)
    {
        model.m_drawCalls = dcs;
    }

    context.GenerateBoundingVolume(model.m_localBounds);

    TriangularShapeDesc triMesh;
    context.GeneratePhysicsGeometry(model.m_drawCalls, triMesh);
    model.m_physicsShape = PhysicsModule::Instance().CreateTriMesh(triMesh);
}

void GeometryUtils::OptimizeDrawCalls(std::vector<DrawCall>& newDrawCalls, std::vector<uint16_t>& newIndices, const std::vector<uint16_t>& indices, const std::vector<DrawCall>& drawCalls)
{
    struct SortedDrawCall
    {
        int dc_index;
        uint64_t batchingKey;
    };

    newIndices.reserve(indices.size());
    std::vector<SortedDrawCall> sortedList;
    int index = 0;
    SortedDrawCall sdc;

    for (const DrawCall& dc : drawCalls)
    {
        sdc.dc_index = index++;
        sdc.batchingKey = dc.ComputeBatchingKey();
        sortedList.push_back(sdc);
    }

    std::sort(sortedList.begin(), sortedList.end(), [](SortedDrawCall const& a, SortedDrawCall const& b) -> bool { return a.batchingKey < b.batchingKey; });

    uint64_t previousKey = 0;
    DrawCall mergedDrawCall = {};
    for (int i = 0; i < (int)sortedList.size(); i++)
    {
        if (i == 0 || previousKey != sortedList[i].batchingKey || !g_performanceOptimizationsEnabled)
        {
            if (mergedDrawCall.m_primitiveCount > 0)
            {
                mergedDrawCall.m_material.ComputeHash();
                newDrawCalls.push_back(mergedDrawCall);
            }

            previousKey = sortedList[i].batchingKey;
            mergedDrawCall = drawCalls[sortedList[i].dc_index];
            mergedDrawCall.m_primitiveCount = 0;
            mergedDrawCall.m_firstIndex = (int)newIndices.size();
        }

        const DrawCall& dc = drawCalls[sortedList[i].dc_index];
        if (!dc.IsBatchableWith(mergedDrawCall))
        {
            uint64_t k1 = dc.ComputeBatchingKey();
            uint64_t k2 = mergedDrawCall.ComputeBatchingKey();
            assert(!"hash key collision");
        }
        mergedDrawCall.m_primitiveCount += dc.m_primitiveCount;
        for (int j = 0; j < dc.m_primitiveCount; j++)
        {
            // also flip winding
            newIndices.push_back(indices[dc.m_firstIndex + j * 3 + 0]);
            newIndices.push_back(indices[dc.m_firstIndex + j * 3 + 1]);
            newIndices.push_back(indices[dc.m_firstIndex + j * 3 + 2]);
        }
    }

    if (mergedDrawCall.m_primitiveCount > 0)
    {
        mergedDrawCall.m_material.ComputeHash();
        newDrawCalls.push_back(mergedDrawCall);
    }

}


void GeometryUtils::SmoothNormals(ModelConversionContext& context, const std::vector<DrawCall>& drawCalls, int vertexOfs)
{
#ifdef ENABLE_ADVANCED_RENDERING
    const int vertexCount = (int)context.m_input.m_vertexPos.size() - vertexOfs;
    context.m_perVertexNormal.clear();
    context.m_perVertexNormal.resize(vertexCount);

    uint8_t plainGeometry = (uint8_t)VertexStretchMode::None;
    VertexPosition* pos = const_cast<VertexPosition*>(context.m_input.m_vertexPos.data()) + vertexOfs;

    for (const DrawCall& dc : drawCalls)
    {
        const uint16_t* ind = &context.m_sortedIndices[dc.m_firstIndex];
        for (int i = 0; i < dc.m_primitiveCount; i++, ind += 3)
        {
            const ModelVertex& v0 = context.m_vertices[ind[0]];
            const ModelVertex& v1 = context.m_vertices[ind[1]];
            const ModelVertex& v2 = context.m_vertices[ind[2]];
            if (v0.StretchMode != plainGeometry || v1.StretchMode != plainGeometry || v2.StretchMode != plainGeometry)
            {
                continue;
            }
            int i0 = v0.GetPositionIndex1();
            int i1 = v1.GetPositionIndex1();
            int i2 = v2.GetPositionIndex1();
            context.m_perVertexNormal[i0] += Vector(v0.Normal.x, v0.Normal.y, v0.Normal.z, 0.f);
            context.m_perVertexNormal[i1] += Vector(v1.Normal.x, v1.Normal.y, v1.Normal.z, 0.f);
            context.m_perVertexNormal[i2] += Vector(v2.Normal.x, v2.Normal.y, v2.Normal.z, 0.f);
        }
    }

    // normalize:
    for (int i = 0; i < vertexCount; i++)
    {
        //context.m_perVertexNormal[i].Set(pos[i].x, pos[i].y, -pos[i].z *0.f); // cylindrical normals
        context.m_perVertexNormal[i].Normalize(); // could be zero length
    }

    // assign back
	for (const DrawCall& dc : drawCalls)
	{
		const uint16_t* ind = &context.m_sortedIndices[dc.m_firstIndex];
		for (int i = 0; i < dc.m_primitiveCount; i++, ind += 3)
        {
            ModelVertex& v0 = context.m_vertices[ind[0]];
            ModelVertex& v1 = context.m_vertices[ind[1]];
            ModelVertex& v2 = context.m_vertices[ind[2]];
            if (v0.StretchMode != plainGeometry || v1.StretchMode != plainGeometry || v2.StretchMode != plainGeometry)
            {
                continue;
            }
            int i0 = v0.GetPositionIndex1();
            int i1 = v1.GetPositionIndex1();
            int i2 = v2.GetPositionIndex1();
            if (context.m_perVertexNormal[i0].GetLength() > 0.01f)
            {
				v0.Normal = context.m_perVertexNormal[i0];
            }
			if (context.m_perVertexNormal[i1].GetLength() > 0.01f)
			{
				v1.Normal = context.m_perVertexNormal[i1];
			}
			if (context.m_perVertexNormal[i2].GetLength() > 0.01f)
			{
				v2.Normal = context.m_perVertexNormal[i2];
			}
        }
    }
#endif
}

std::shared_ptr<Renderable> GeometryUtils::CreateSpriteMesh(bool flipU)
{
    ModelLoadingContext context;
	context.m_vertexPos.push_back(VertexPosition{ 0,0,0 });
    context.m_vertexPos.push_back(VertexPosition{ 0,0,0 }); // two positions to avoid constant buffer warning
    ModelConversionContext conversion(context);

    float scale = 64.f * DespSpriteToFloat; // norm size
    AddSprite(conversion, 1, scale, flipU);
    conversion.m_sortedIndices = conversion.m_indices;
    std::shared_ptr<Renderable> renderable = std::make_shared<Renderable>();
    renderable->m_runtime = Renderer::Instance().CreateModelBuffers(conversion);
    renderable->m_localBounds = conversion.m_visibilityBox;
    renderable->m_vertexOfs = 0;
    DrawCall& dc = renderable->m_drawCalls.emplace_back();
    dc.m_material.m_transparencyType = TransparencyMode::Opaque;
    dc.m_material.m_opacity = 1.f;
    dc.m_material.m_textureSlot = 0;
    dc.m_material.m_texture = nullptr;
    //dc.m_material.m_textureRepeat = false;
    dc.m_material.m_applyLighting = false;
    dc.m_material.m_useAlphaTest = true;
    dc.m_material.ComputeHash();

    dc.m_firstIndex = 0;
    dc.m_primitiveCount = (int)conversion.m_indices.size() / 3;
    conversion.GenerateBoundingVolume(renderable->m_localBounds);

    return renderable;
}

std::shared_ptr<Renderable> GeometryUtils::CreateCubeMesh(float radiusX, float radiusY, float radiusZ, const Color4& col, float thickness)
{
    ModelLoadingContext context;
    ModelConversionContext conversion(context);

    uint16_t v[2][2][2];
    for (int z = 0; z < 2; z++)
        for (int y = 0; y < 2; y++)
            for (int x = 0; x < 2; x++)
            {
                context.m_vertexPos.push_back(VertexPosition{ x == 0 ? -radiusX : radiusX,y == 0 ? -radiusY : radiusY,z == 0 ? -radiusZ : radiusZ });
                v[z][y][x] = (uint16_t)context.m_vertexPos.size(); // indices are 1-based
            }

    float scale = DespVertToFloat * thickness;
    AddLine(conversion, v[0][0][0], v[0][0][1], scale, col);
    AddLine(conversion, v[0][1][0], v[0][1][1], scale, col);
    AddLine(conversion, v[1][0][0], v[1][0][1], scale, col);
    AddLine(conversion, v[1][1][0], v[1][1][1], scale, col);

    AddLine(conversion, v[0][0][0], v[0][1][0], scale, col);
    AddLine(conversion, v[0][0][1], v[0][1][1], scale, col);
    AddLine(conversion, v[1][0][0], v[1][1][0], scale, col);
    AddLine(conversion, v[1][0][1], v[1][1][1], scale, col);

    AddLine(conversion, v[0][0][0], v[1][0][0], scale, col);
    AddLine(conversion, v[0][0][1], v[1][0][1], scale, col);
    AddLine(conversion, v[0][1][0], v[1][1][0], scale, col);
    AddLine(conversion, v[0][1][1], v[1][1][1], scale, col);

    conversion.m_sortedIndices = conversion.m_indices;

    std::shared_ptr<Renderable> renderable = std::make_shared<Renderable>();
    renderable->m_runtime = Renderer::Instance().CreateModelBuffers(conversion);
    renderable->m_vertexOfs = 0;
    DrawCall& dc = renderable->m_drawCalls.emplace_back();
    dc.m_material.m_transparencyType = TransparencyMode::Opaque;
    dc.m_material.m_opacity = 1.f;
    dc.m_material.m_textureSlot = -1;
    dc.m_material.m_texture = AssetManager::Instance().GetPlainWhiteTexture();
    //dc.m_material.m_textureRepeat = false;
    dc.m_material.m_applyLighting = false;
    dc.m_material.m_useAlphaTest = true;
    dc.m_material.ComputeHash();

    dc.m_firstIndex = 0;
    dc.m_primitiveCount = (int)conversion.m_indices.size() / 3;

    conversion.GenerateBoundingVolume(renderable->m_localBounds);

    return renderable;
}

std::shared_ptr<Renderable> GeometryUtils::CreateDebugGeometry(const std::vector<DebugLine>& lineBuffer)
{
    if (lineBuffer.size() == 0)
        return nullptr;

    ModelLoadingContext context;
    ModelConversionContext conversion(context);

    for (const DebugLine& line : lineBuffer)
    {
        uint16_t v0 = (uint16_t)context.m_vertexPos.size();
        context.m_vertexPos.push_back(VertexPosition{ line.m_p1.GetX(), line.m_p1.GetY(), line.m_p1.GetZ() });
        context.m_vertexPos.push_back(VertexPosition{ line.m_p2.GetX(), line.m_p2.GetY(), line.m_p2.GetZ() });
        AddLine(conversion, v0 + 1, v0 + 2, line.m_width * DespVertToFloat, line.m_color);
    }

    conversion.m_sortedIndices = std::move(conversion.m_indices);
    std::shared_ptr<Renderable> renderable = std::make_shared<Renderable>();
    renderable->m_runtime = Renderer::Instance().CreateModelBuffers(conversion);
    renderable->m_vertexOfs = 0;
    DrawCall& dc = renderable->m_drawCalls.emplace_back();
    dc.m_material.m_transparencyType = TransparencyMode::Opaque;
    dc.m_material.m_opacity = 1.f;
    dc.m_material.m_textureSlot = 0;
    dc.m_material.m_texture = AssetManager::Instance().GetPlainWhiteTexture();
    //dc.m_material.m_textureRepeat = false;
    dc.m_material.m_applyLighting = false;
    dc.m_material.m_useAlphaTest = true;
    dc.m_material.ComputeHash();

    dc.m_firstIndex = 0;
    dc.m_primitiveCount = (int)conversion.m_sortedIndices.size() / 3;
    conversion.GenerateBoundingVolume(renderable->m_localBounds);

    return renderable;

}

std::shared_ptr<Renderable> GeometryUtils::CreateBurningObjectMesh(int maxPositions)
{
	ModelLoadingContext context;
	ModelConversionContext conversion(context);

    context.m_vertexPos.resize(maxPositions);
    const float scale = DespVertToFloat * 3.f;
    for (int i = 0; i < maxPositions; i++)
    {
        AddFlame(conversion, i + 1, scale);
    }
	conversion.m_sortedIndices = conversion.m_indices;

    std::shared_ptr<Renderable> renderable = std::make_shared<Renderable>();
	renderable->m_runtime = Renderer::Instance().CreateModelBuffers(conversion);
	renderable->m_vertexOfs = 0;

    return renderable;
}

void GeometryUtils::ComputeBoundingVolume(BoundingVolume& vol, const std::vector<DrawCall>& drawCalls, const std::vector<ModelVertex>& vertexArray, const std::vector<VertexPosition>& positionArray, const std::vector<uint16_t>& indexArray)
{
    auto AddPos = [&](int idx)
    {
        if (idx >= 0)
        {
            const VertexPosition& pos(positionArray[idx]);
            vol.Add(Vector(pos.x, pos.y, pos.z));
        }
    };

    for (const auto& dc : drawCalls)
    {
        const int indexCount = dc.m_primitiveCount * 3;
        for (int i = 0; i < indexCount; i++)
        {
            const ModelVertex& mv(vertexArray[indexArray[dc.m_firstIndex + i]]);
            // does not consider the radius of stretched polygons
            AddPos(mv.GetPositionIndex1());
            AddPos(mv.GetPositionIndex2());
        }
    }

    vol.OnUpdated();
}

WaterReflectionType ModelPolygon::GetTriangleReflectionType() const
{
    WaterReflectionType reflMat = (WaterReflectionType)(something2 & 15);
    switch (GetTriangleBlendType())
    {
        case TriangleBlendType::SolidColor:
            return WaterReflectionType::Disabled;
        case TriangleBlendType::Transparent:
            return reflMat;
        case TriangleBlendType::Refraction:
            return reflMat | WaterReflectionType::RefractionBit;
        case TriangleBlendType::Reflection:
            return reflMat | WaterReflectionType::ReflectionBit;
    }
    return WaterReflectionType::Disabled;
}
