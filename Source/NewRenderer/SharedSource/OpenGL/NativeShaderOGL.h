#pragma once
#include <DesperabisCommon.h>
#include <OpenGL/OpenGL.h>
#include <Renderer/IGraphicsRuntimeObject.h>
#include <Renderer/RendererCommon.h>


class NativeShaderOGL
{
public:
    NativeShaderOGL() = default;
    virtual ~NativeShaderOGL()
    {
        Deinitialize();
    }

    void Deinitialize();

	void Bind();

    FUTURE(void) CreateFromFile(std::wstring_view vs, std::wstring_view fs, std::wstring_view gs = L"");

    //void CreateDefaultVertexLayout();
protected:
    FUTURE(GLuint) ShaderFromFile(std::wstring_view filename, GLenum shadertype);
    FUTURE(void) ResolveIncludes(const std::string& input, std::string& outSource) const;
    GLuint m_program = 0;
//    GLuint m_vertexArray = 0;
};
