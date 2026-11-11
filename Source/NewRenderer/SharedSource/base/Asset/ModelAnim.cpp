#include "pch.h"
#include <Asset/ModelAnim.h>
#include <Renderer/Renderer.h>


ModelAnim::ModelAnim()
{
    for (int anim = 0; anim < 6; anim++)
        for (int frame = 0; frame < 5; frame++)
        {
            Model& m(m_model[anim][frame]);
            m.m_owner = this;
            m.m_animNr = anim;
            m.m_frameNr = frame;
        }
}

void ModelAnim::Read(AssetStream& s)
{
    static constexpr struct
    {
        std::wstring_view filename;
        int animation, frame; // One-based; frame zero selects the whole animation.
        float scale;
        float depthOffsetDos = 0.f;
    } depthBiasOverrides[]{
        {L"VEKT\\VEKT20.3DA", 1, 1, 0.5f}, // Smaller windmill body variants.
        {L"VEKT\\VEKT20.3DA", 1, 2, 0.5f},
        {L"VEKT\\VEKT20.3DA", 1, 3, 0.5f},
        {L"VEKT\\VEKT20.3DA", 1, 4, 0.5f},
        {L"VEKT\\VEKT20.3DA", 3, 0, 2.f}, // Windmill body variants.
        {L"VEKT\\VEKT60.3DA", 3, 2, 1.f}, // Recessed knothole.
        {L"VEKT\\VEKT66.3DA", 3, 1, 0.f}, // Machine casing and wreck stay at geometric depth.
        {L"VEKT\\VEKT66.3DA", 3, 2, 0.f},
        {L"VEKT\\VEKT66.3DA", 3, 3, 0.002f, 2.f}, // Entire pressure display, including priority-zero faces.
        {L"VEKT\\VEKT66.3DA", 3, 4, 0.f},
        {L"VEKT\\VEKT66.3DA", 3, 5, 0.f, 4.f}, // Pointer remains above the display.
        {L"VEKT\\VEKT68.3DA", 1, 0, 0.02f}, // Witch head's recessed mouth.
    };

    m_textureUsedMask = s.Read<uint16_t>();

    TextureLoadingContext textureData[16];
    for (int i = 0; i < 16; i++)
    {
        if ((m_textureUsedMask & (1 << i)) > 0)
        {
            Texture& t(m_ownTextures[i]);
            t.Read(s, textureData[i]);
        }
    }

    m_textureAtlas.CreateAsAtlas(m_ownTextures, textureData, 16);

    // load raw data:
    m_numAnims = s.Read<uint8_t>();
    m_numFrames = s.Read<uint8_t>();

    // Load and convert single models
    ModelLoadingContext context;

    // First step of the conversion
	ModelConversionContext conversion(context);
    for (int anim = 0; anim < m_numAnims; anim++)
    {
        for (int frame = 0; frame < m_numFrames; frame++)
        {
            Model& m(m_model[anim][frame]);
            float depthOffset = 0.f;
            for (const auto& override : depthBiasOverrides)
            {
                if (s.GetSourceFilename() == override.filename && anim + 1 == override.animation &&
                    (override.frame == 0 || frame + 1 == override.frame))
                {
                    m.m_sortingDepthBiasScale = override.scale;
                    depthOffset = override.depthOffsetDos * DespToFloat;
                }
            }
            m.m_positionOfs = (uint16_t)context.m_vertexPos.size();
            m.m_polygonOfs = (uint16_t)context.m_polygons.size();
			conversion.m_modelPositionOfs = m.m_positionOfs;
			conversion.m_numPositions = m.m_numPositions;

            m.Read(s, context);
            GeometryUtils::Convert(m, conversion);
            if (depthOffset != 0.f)
            {
                for (auto& draw : m.m_drawCalls)
                {
                    draw.m_material.m_depthOffset = depthOffset;
                    draw.m_material.ComputeHash();
                }
            }
        }
    }

    // Second step of the conversion
    for (int anim = 0; anim < m_numAnims; anim++)
	{
		for (int frame = 0; frame < m_numFrames; frame++)
		{
			Model& m(m_model[anim][frame]);
 			conversion.m_modelPositionOfs = m.m_positionOfs;
			conversion.m_numPositions = m.m_numPositions;
            GeometryUtils::AddModelReferences(m, conversion);
        }
	}
    m_runtime = Renderer::Instance().CreateModelBuffers(conversion);
}

void ModelAnim::CreateRuntimes(Renderer& renderer)
{
    for (int i = 0; i < 16; i++)
    {
        m_ownTextures[i].CreateRuntimes(renderer);
    }

    // assign shared buffers:
    for (int anim = 0; anim < m_numAnims; anim++)
    {
        for (int frame = 0; frame < m_numFrames; frame++)
        {
            Model& m(m_model[anim][frame]);

            // share the runtime across all models
            m.m_runtime = m_runtime;

        }
    }
}