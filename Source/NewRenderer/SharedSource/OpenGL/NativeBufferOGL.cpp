#include "pch.h"
#include <OpenGL/NativeBufferOGL.h>
#include <Asset/ModelGeometry.h>


void NativeModelMeshObjectOGL::Deinitialize()
{
	if (m_positionBuffer) { glDeleteBuffers(1, &m_positionBuffer); m_positionBuffer = 0;}
	if (m_vertexBuffer) { glDeleteBuffers(1, &m_vertexBuffer); m_vertexBuffer = 0; }
	if (m_indexBuffer) { glDeleteBuffers(1, &m_indexBuffer); m_indexBuffer = 0; }
	if (m_vertexArray) { glDeleteVertexArrays(1, &m_vertexArray); m_vertexArray = 0;}
}


void NativeModelMeshObjectOGL::Bind()
{
	CHECK_GL_ERRORS;
	glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
	CHECK_GL_ERRORS;
	glBindBufferBase(GL_UNIFORM_BUFFER, 2, m_positionBuffer);
	CHECK_GL_ERRORS;
	glBindVertexArray(m_vertexArray);
	CHECK_GL_ERRORS;
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
	CHECK_GL_ERRORS;
}

void NativeModelMeshObjectOGL::CreateBuffers(ModelConversionContext& data)
{
	CHECK_GL_ERRORS;

	// position data
	if (data.m_input.m_vertexPos.size() > 0)
	{
		int posBufferSize = sizeof(data.m_input.m_vertexPos[0]) * (int)data.m_input.m_vertexPos.size();
		glGenBuffers(1, &m_positionBuffer);
		glBindBuffer(GL_UNIFORM_BUFFER, m_positionBuffer);
		glBufferData(GL_UNIFORM_BUFFER, posBufferSize, data.m_input.m_vertexPos.data(), GL_STATIC_DRAW);
	}

	// vertex data
	int vertexBufferSize = sizeof(data.m_vertices[0]) * (int)data.m_vertices.size();
	glGenBuffers(1, &m_vertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, vertexBufferSize, data.m_vertices.data(), GL_STATIC_DRAW);
	CHECK_GL_ERRORS;

	// index data
	if (data.m_sortedIndices.size() > 0)
	{
		glGenBuffers(1, &m_indexBuffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, data.m_sortedIndices.size() * sizeof(unsigned short), data.m_sortedIndices.data(), GL_STATIC_DRAW);
	}
	CHECK_GL_ERRORS;

	CreateDefaultVertexLayout();
}

void NativeModelMeshObjectOGL::CreateDefaultVertexLayout()
{
	CHECK_GL_ERRORS;
	glGenVertexArrays(1, &m_vertexArray);
	CHECK_GL_ERRORS;

	CHECK_GL_ERRORS;
	glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
	CHECK_GL_ERRORS;
	glBindVertexArray(m_vertexArray);
	CHECK_GL_ERRORS;

#ifdef USE_VERTEX_COLOR
	// 0 = color
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ModelVertex), (void*)offsetof(ModelVertex, Color));
	CHECK_GL_ERRORS;
#endif
	// 1 = Position index and mode
	glEnableVertexAttribArray(1);
	glVertexAttribIPointer(1, 4, GL_UNSIGNED_BYTE, sizeof(ModelVertex), (void*)offsetof(ModelVertex, PositionIndex));
	CHECK_GL_ERRORS;
	// 2 = UV
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), (void*)offsetof(ModelVertex, TexCoord));
	CHECK_GL_ERRORS;
	// 3 = Stretch params
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), (void*)offsetof(ModelVertex, StretchParams));
	CHECK_GL_ERRORS;
}
