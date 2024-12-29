#pragma once
#include <DesperabisCommon.h>
#include <Renderer/DrawCall.h>

enum class FogType : byte 
{
    Standard = 0,
    Outside = 10,
    WhiteFog = 11, // cathedral outside
    Green = 15, // sewer
    OutsideBright = 18, // cutscene clouds
    CaveMachines = 19,
    Forest = 20
};


enum class SkyTint : byte // seems to use the color same tables from WaterReflectionType
{
    NoSky = 0,
    Identity = 3,
    WhiteFog = 10,

    Dawn = 33,
    Night = 35
};


enum class AmbientSound : byte
{
    DefaultInside = 0,
    Rain = 1,
    Forest = 2,
    MorningCrows = 3,
    BurningForest = 4,

    Music1 = 100, // museum
    Music2 = 101, // cutscene magician
    Music3 = 102, // cutscene clouds
};


class D_IMPEXP LevelInfo
{
public:
    void Parse(int levelIndex, const byte* rawData);
    FUTURE(void) PreloadData();

    std::shared_ptr<ModelAnim> GetModelAnim(int id) const;
    std::shared_ptr<Texture> GetLevelTexture(int slot) const;
    std::shared_ptr<TextureAnim> GetTextureAnim(int slot) const;

    void AssignLevelTextures(RenderableInstance& inst) const;
    static bool IsUsedLevelIndex(int level);
    inline bool IsUsedLevel() const { return IsUsedLevelIndex(m_levelIndex); }

#ifdef _DEBUG
    bool LogLine(int index, std::wstringstream& wss) const;
#endif
public:
    static constexpr int NumSlots = 16;

    struct AssetSlot
    {
        byte fileIndex;
        byte targetSlot;
    };

    // there can be 3 slots of water, the tile indexes them with 1..3
    struct WaterSlot
    {
        WaterReflectionType reflectionType;
        byte height; // -height*10.f
    };

    struct LevelInfoRaw
    {
        union
        {
            struct Data
            {
                byte numTextures;
                byte numTextureAnims;
                byte numVectAnims;
                byte numSomething;

                AssetSlot textureSlots[15];
                AssetSlot textureAnimSlots[15];
                AssetSlot vectorSlots[15];

                byte clearColorIdx; // pal entry
                byte ignored_One; // always 1
                byte skyTextureFile;
                byte skylineTextureFile; // (e.g. TUR6 = forest)
                FogType fogTableType;
                SkyTint skyTint;
                AmbientSound ambientSound;
                byte unknownSky; // [1 or 2], ofs 101
                byte ignored_Zero; // always 0
                byte levelIndexInArray; // but 0 for puzzle level, ofs 103
                byte cutsceneScript; // ofs 104
                byte ignored_Ten; // almost always 10, ofs 105
                byte waterAmplitude; // ofs 106
                byte amoutOfStars; // number of stars rendered into sky, ofs 107
                WaterSlot water[3]; // ofs 108
                byte unknowns[10];
            } m_data;
            byte m_rawData[124];
        };
        inline bool operator == (const LevelInfoRaw& other) const { return memcmp(m_rawData, other.m_rawData, sizeof(m_rawData)) == 0; }
    };

    int m_levelIndex = -1;
    uint64_t m_uniqueLevelHash = 0;
    LevelInfoRaw m_raw = {};

    byte m_defaultTextureSlots[NumSlots] = {};
    byte m_loadedTexAnimSlots[NumSlots] = {};
    byte m_loadedModelSlots[NumSlots] = {};

    std::shared_ptr<Texture> m_skyTexture;
	std::shared_ptr<Texture> m_skylineTexture;
    std::shared_ptr<ModelAnim> m_loadedModel[NumSlots];
    std::shared_ptr<Texture> m_loadedTexture[NumSlots];
    std::shared_ptr<TextureAnim> m_loadedTextureAnim[NumSlots];
};

