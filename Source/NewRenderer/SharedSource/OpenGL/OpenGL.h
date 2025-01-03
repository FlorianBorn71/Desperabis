#pragma once

#include <GL/glew.h>

#ifdef _DEBUG

	inline void CheckForGlErrors(bool assertion = true)
	{
		GLenum err; 
		while ((err = glGetError()) != GL_NO_ERROR)
		{
			if (assertion)
			{
				assert(false);
			}
		}
	}

#define CHECK_GL_ERRORS CheckForGlErrors()
#define FLUSH_GL_ERRORS CheckForGlErrors(false)
#else
#define CHECK_GL_ERRORS
#define FLUSH_GL_ERRORS
#endif // _DEBUG
