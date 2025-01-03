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
            m.m_positionOfs = (uint16_t)context.m_vertexPos.size();
            m.m_polygonOfs = (uint16_t)context.m_polygons.size();
			conversion.m_modelPositionOfs = m.m_positionOfs;
			conversion.m_numPositions = m.m_numPositions;

            m.Read(s, context);
            GeometryUtils::Convert(m, conversion);
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