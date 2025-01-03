#pragma once
#include <DesperabisCommon.h>
#include <OpenGL/OpenGL.h>
#include <Renderer/IGraphicsRuntimeObject.h>

template<typename TYPE, bool CHECK_FOR_CHANGES, GLuint USAGE_HINT>
struct TypedBufferOGL
{
public:
	TypedBufferOGL() = default;
    ~TypedBufferOGL() { Deinitialize(); }

    const GLuint BUFFER_TYPE = GL_UNIFORM_BUFFER;
    inline void Initialize()
    {
		CHECK_GL_ERRORS;
        if (m_handle == 0)
        {
            glGenBuffers(1, &m_handle);
        }
		glBindBuffer(BUFFER_TYPE, m_handle);
		glBufferData(BUFFER_TYPE, sizeof(TYPE), &m_data, USAGE_HINT);
		CHECK_GL_ERRORS;
    }
    inline void Deinitialize()
    {
        if (m_handle)
        {
            CHECK_GL_ERRORS;
            glDeleteBuffers(1, &m_handle);
            m_handle = 0;
			CHECK_GL_ERRORS;
        }
    }

    inline void Update(const TYPE& data)
    {
		CHECK_GL_ERRORS;
        if (m_handle == 0)
        {
            m_data = data;
            Initialize();
        }
        else if (CHECK_FOR_CHANGES)
        {
            if (memcmp(&data, &m_data, sizeof(TYPE)) == 0)
                return;
            m_data = data;
			glBindBuffer(BUFFER_TYPE, m_handle);
			glBufferData(BUFFER_TYPE, sizeof(TYPE), &m_data, USAGE_HINT);
        }
        else
        {
			glBindBuffer(BUFFER_TYPE, m_handle);
			glBufferData(BUFFER_TYPE, sizeof(TYPE), &data, USAGE_HINT);
        }
		CHECK_GL_ERRORS;
	}

    inline void Bind(int slot)
    {
		CHECK_GL_ERRORS;
        glBindBufferBase(BUFFER_TYPE, slot, m_handle);
		CHECK_GL_ERRORS;
    }

    GLuint m_handle = 0;
    TYPE m_data = {};
};


class NativeModelMeshObjectOGL : public IGraphicsRuntimeObject
{
public:
    NativeModelMeshObjectOGL() = default;

    virtual ~NativeModelMeshObjectOGL()
    {
        Deinitialize();
    }

    void Deinitialize();
    void Bind();
    void CreateBuffers(ModelConversionContext& data);
	void CreateDefaultVertexLayout();

	GLuint m_vertexArray = 0;

	GLuint m_positionBuffer = 0;
	GLuint m_vertexBuffer = 0;
	GLuint m_indexBuffer = 0;

};
