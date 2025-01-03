#pragma once

#include <vector>
#include <mutex>
#include <map>
#include <ppltasks.h>
#include <string>
#include <fstream>
#include <future>
#include <array>
#include <sstream>
#include <filesystem>
#include <algorithm>

#include <Math/Math.h>


// async:
#if defined (D_ASYNC)
    #define CO_AWAIT co_await
    #define CO_RETURN co_return
    #define FUTURE(_T) std::future<_T>
    #define FORCE_WAIT(_expr) (_expr).wait()
#else
    #define CO_AWAIT
    #define CO_RETURN return
    #define FUTURE(_T) _T
    #define FORCE_WAIT(_expr) _expr
#endif


using ProfilingDuration = std::chrono::microseconds;

struct ProfilingStats
{
    void Update()
    {
        m_max_duration_visibility = __max(m_max_duration_visibility, m_duration_visibility);
        m_max_duration_rendering = __max(m_max_duration_rendering, m_duration_rendering);
        m_max_duration_update = __max(m_max_duration_update, m_duration_update);
    }

    void ResetMax()
    {
        m_max_duration_visibility = {};
        m_max_duration_rendering = {};
        m_max_duration_update = {};
    }

    void ResetRenderStats()
    {
        m_numDrawCalls = m_numTriangles = 0;
    }

    int m_numDrawCalls, m_numTriangles;
    ProfilingDuration m_duration_visibility = {};
    ProfilingDuration m_duration_rendering = {};
    ProfilingDuration m_duration_update = {};

    ProfilingDuration m_max_duration_visibility = {};
    ProfilingDuration m_max_duration_rendering = {};
    ProfilingDuration m_max_duration_update = {};

    std::wstring m_debugString;
};


struct ProfilingScope
{
    inline ProfilingScope(ProfilingDuration& out_val)
        : m_out_val(out_val)
    {
        m_profileStart = std::chrono::high_resolution_clock::now();
    }

    inline ~ProfilingScope()
    {
        auto duration = std::chrono::high_resolution_clock::now() - m_profileStart;
        m_out_val = std::chrono::duration_cast<std::chrono::microseconds>(duration);
    }

    std::chrono::steady_clock::time_point m_profileStart;
    ProfilingDuration& m_out_val;
};

// forward decl
class AnimationKeyframe;
class AnimationPose;
class AnimationSequence;
class AnimationSequences;
class AssetBase;
class AssetStream;
class Camera;
struct Color4;
struct DrawCall;
struct DrawCallMaterial;
struct Frustum;
class GameObject;
struct GridCell;
struct GridCellArray;
class LevelGameObject;
class LightSource;
class Level;
class Model;
class ModelAnim;
struct ModelLoadingContext;
struct ModelConversionContext;
class Palette;
class PhysicsModule;
class PhysicsShape;
class PhysicsObject;
struct TriangularShapeDesc;
class Renderer;
class Renderable;
class RenderableInstance;
class Scene;
class Texture;
class TextureAnim;
struct TextureLoadingContext;
class Transformable;
class IVisibilityContext;
struct VisibilityResult;
class IGraphicsRuntimeObject;
struct DebugLine;

using AssetPtr = std::shared_ptr<AssetBase>;
using RenderablePtr = std::shared_ptr<Renderable>; 
using RenderableInstancePtr = std::shared_ptr<RenderableInstance>;
using byte = unsigned char;

// See also GlobalSettings::SetRelativeGeometryScaling
extern float DespToFloat;
extern float DespVertToFloat;
extern float DespSpriteToFloat;
extern float DespRelativeScaling; // for all the distances/values that are already tweaked with the defaults below
extern float DespRelativeInvScaling; // for all the distances/values that are already tweaked with the defaults below

#define DECLARE_ENUM_FLAGS(_e) \
    inline _e operator | (_e a, _e b)\
    {\
        return static_cast<_e>(static_cast<int>(a) | static_cast<int>(b));\
    }\
    inline void operator |= (_e& a, _e b)\
    {\
        a = a | b;\
    }\
    inline _e operator & (_e a, _e b)\
    {\
        return static_cast<_e>(static_cast<int>(a) & static_cast<int>(b));\
    }\
    inline void operator &= (_e& a, _e b)\
    {\
        a = a & b; \
    }\
    inline bool operator == (_e a, int val)\
    {\
        return static_cast<int>(a) == val;\
    }\
    inline bool operator != (_e a, int val)\
    {\
        return static_cast<int>(a) != val;\
    }


//#define D_IMPEXP

// import/export
#ifdef D_EXPORT
#define D_IMPEXP __declspec(dllexport)
#else
//#define D_IMPEXP __declspec(dllimport)
  #define D_IMPEXP
#endif
