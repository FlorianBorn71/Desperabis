#include "pch.h"
#include <OpenGL/NativeShaderOGL.h>
#include <IO/FileManager.h>
#include <Asset/AssetManager.h>
#include <Asset/ModelGeometry.h>
#include <Asset/TextInclude.h>

void NativeShaderOGL::Deinitialize()
{
	if (m_program)
	{
		glDeleteProgram(m_program);
		m_program = 0;
	}
/*
	if (m_vertexArray)
	{
		glDeleteVertexArrays(1, &m_vertexArray);
		m_vertexArray = 0;
	}
	*/
}

void NativeShaderOGL::Bind()
{
	CHECK_GL_ERRORS;
	glUseProgram(m_program);
//	glBindVertexArray(m_vertexArray);

	GLuint instBlockIdx;

	instBlockIdx = glGetUniformBlockIndex(m_program, "Globals");
	glUniformBlockBinding(m_program, instBlockIdx, 0);

	instBlockIdx = glGetUniformBlockIndex(m_program, "InstanceConstantBuffer");
	glUniformBlockBinding(m_program, instBlockIdx, 1);

	instBlockIdx = glGetUniformBlockIndex(m_program, "PositionArray");
	glUniformBlockBinding(m_program, instBlockIdx, 2);

	instBlockIdx = glGetUniformBlockIndex(m_program, "TextureAtlasConstantBuffer");
	glUniformBlockBinding(m_program, instBlockIdx, 3);

	instBlockIdx = glGetUniformBlockIndex(m_program, "MaterialConstantBuffer");
	glUniformBlockBinding(m_program, instBlockIdx, 4);

	GLuint TextureID = glGetUniformLocation(m_program, "baseTexture");

	CHECK_GL_ERRORS;
}

FUTURE(void) NativeShaderOGL::CreateFromFile(std::wstring_view vs, std::wstring_view fs, std::wstring_view gs)
{
	std::vector<GLuint> shaderIDs;
	shaderIDs.push_back(CO_AWAIT ShaderFromFile(vs, GL_VERTEX_SHADER));
	shaderIDs.push_back(CO_AWAIT ShaderFromFile(fs, GL_FRAGMENT_SHADER));
	if (!gs.empty())
	{
		shaderIDs.push_back(CO_AWAIT ShaderFromFile(gs, GL_GEOMETRY_SHADER));
	}

	// Link the program
	CHECK_GL_ERRORS;
	m_program = glCreateProgram();
	for (GLuint s : shaderIDs)
	{
		glAttachShader(m_program, s);
	}
	glLinkProgram(m_program);
	CHECK_GL_ERRORS;

	// Check the program
	GLint linkResult = GL_FALSE;
	glGetProgramiv(m_program, GL_LINK_STATUS, &linkResult);
	if (linkResult != GL_TRUE)
	{
		GLint infoLogLen;
		glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &infoLogLen);
		std::vector<GLchar> message(infoLogLen + 1);
		glGetProgramInfoLog(m_program, infoLogLen, NULL, &message[0]);
		const char* msg = message.data();
		printf(msg);
		assert(msg == nullptr); // assert here
	}

	for (GLuint s : shaderIDs)
	{
		glDetachShader(m_program, s);
		glDeleteShader(s);
	}
	CHECK_GL_ERRORS;
}

FUTURE(GLuint) NativeShaderOGL::ShaderFromFile(std::wstring_view filename, GLenum shadertype)
{
	FileManager& fm = FileManager::Instance();
	std::wstring path = fm.FilenameToNative(filename);
	std::vector<byte> fileData = CO_AWAIT fm.ReadDataFuture(path);
	std::string resolvedSource;
	std::string text((const char*)fileData.data(), fileData.size());
	this->ResolveIncludes(text, resolvedSource);

	const GLchar* sources[] =
	{
		resolvedSource.data(),
	};
	GLint sourceLengths[] =
	{
		(GLint)resolvedSource.size()
	};

	CHECK_GL_ERRORS;
	GLuint shaderId = glCreateShader(shadertype);
	glShaderSource(shaderId, sizeof(sourceLengths) / sizeof(sourceLengths[0]), sources, sourceLengths);
	glCompileShader(shaderId);
	CHECK_GL_ERRORS;

	// Check shader status
	GLint compileResult = GL_FALSE;
	glGetShaderiv(shaderId, GL_COMPILE_STATUS, &compileResult);
	if (compileResult != GL_TRUE)
	{
		GLint infoLogLen;
		glGetShaderiv(shaderId, GL_INFO_LOG_LENGTH, &infoLogLen);
		if (infoLogLen > 0)
		{
			std::vector<GLchar> message(infoLogLen + 1);
			glGetShaderInfoLog(shaderId, infoLogLen, NULL, &message[0]);
			const char* msg = message.data();
			printf(msg);
			assert(msg == nullptr); // assert here
		}
	}
	return shaderId;
}

FUTURE(void) NativeShaderOGL::ResolveIncludes(const std::string& input, std::string& outSource) const
{
	std::stringstream stream(input);
	std::stringstream out;
	std::string line;
	AssetManager& assets(AssetManager::Instance());

	while (std::getline(stream, line, '\n')) 
	{
		if (line._Starts_with("#include \""))
		{
			std::string_view incl = line;
			incl = incl.substr(10);
			size_t last = incl.find_last_of('"');
			incl = incl.substr(0, last);
			std::wstring path(incl.begin(), incl.end());
			std::replace(path.begin(), path.end(), '/', '\\');
			std::shared_ptr<TextInclude> includeContent = CO_AWAIT assets.LoadAsset<TextInclude>(path);
			out << includeContent->m_text;
		}
		else
		{
			out << line << '\n';
		}
	}
	outSource = out.str();
}

