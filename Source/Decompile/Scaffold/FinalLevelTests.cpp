#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

using namespace Desperabis;

namespace
{
    void Check(bool value, const char* message)
    {
        if (!value) { throw std::runtime_error(message); }
    }

    template<typename Exception, typename Action>
    void Reject(Action action)
    {
        try { action(); }
        catch (const Exception&) { return; }
        throw std::runtime_error("Expected rejection of malformed ending input.");
    }

    struct Input final : InputHandler
    {
        bool IsKeyPressed(InputKey) override { return false; }
    };

    struct Audio final : AudioHandler
    {
        int creations = 0, starts = 0, stops = 0;
        bool looping = true;
        std::vector<std::uint8_t> music;
        std::uint16_t GetSampleCount() const override { return 129; }
        bool IsChannelPlaying(std::size_t) const override { return false; }
        void PlaySample(std::size_t, std::uint8_t, std::uint8_t) override {}
        void StopAllSamples() override {}
        void CreateMusicPlayback(const std::vector<std::uint8_t>& pcm, bool loop) override
        {
            music = pcm;
            looping = loop;
            ++creations;
        }
        void StartAmbientPlayback() override { ++starts; }
        void StopAmbientPlayback() override { ++stops; }
    };

    struct Host final : GameContext
    {
        std::filesystem::path assets;
        std::optional<std::vector<std::uint8_t>> musicOverride, recordingOverride;
        int flashes = 0, destroyed = 0, paths = 0;
        explicit Host(const std::filesystem::path& path) : assets(path) {}
        void OnObjectDestroyed(int) override { ++destroyed; }
        void OnLightFlash(const LightFlash&) override { ++flashes; }
        std::vector<std::uint8_t> Read(std::string_view name)
        {
            std::ifstream file(assets / name, std::ios::binary);
            if (!file) { throw std::runtime_error("Missing final-level fixture: " + std::string(name)); }
            return {std::istreambuf_iterator<char>(file), {}};
        }
    protected:
        std::vector<std::uint8_t> LoadFile(std::string_view name) override
        {
            if (std::filesystem::path(name).extension() == ".KOO") { ++paths; }
            if (name == "SAMP\\MUSIC1.WAV" && musicOverride) { return *musicOverride; }
            if (name == "LEVEL\\DEMO6.KOO" && recordingOverride) { return *recordingOverride; }
            return Read(name);
        }
        void LoadLevel(const LevelLoadRequest& request) override
        {
            LevelLoadData data;
            LoadLevelGeometry(request.levelNumber, data);
            LoadLevelObjects(request.levelNumber, data);
            InitializeLevelPlayerSpawn(data, {0,0,100});
            CompleteLevelLoad(request.id, std::move(data));
        }
    };

    void CheckSupported(const std::array<DispatchResult, 163>& results)
    {
        for (const auto& result : results)
        {
            if (result.status == DispatchStatus::UnimplementedHandler)
            {
                const auto* behavior = FindBehavior(result.behavior);
                throw std::runtime_error(std::string("Unsupported authored behavior: ") +
                    (behavior ? behavior->name : "unknown"));
            }

        }
    }

    void CompleteBossFall(Host& core)
    {
        Check(core.IsBossFallPlaying() && core.IsCutscenePlaying(),
            "Pit contact must enter a blocking, isolated staged-boss presentation.");
        const auto player = core.worldObjects[1];
        const auto boss = core.worldObjects[23];
        Check(core.GetBossFallView().size() == 18 && boss.health > 0 &&
            core.worldObjects[59].generalPurpose2 == 0,
            "The native background, five strips and eleven boss parts must precede route activation.");
        Input input;
        core.SetPaused(true);
        core.Tick({10,10}, input);
        Check(core.GetBossFallView()[7].posZ == -100, "Pausing must freeze the staged fall.");
        core.SetPaused(false);
        core.Tick({0,0}, input);
        Check(core.GetBossFallView()[7].posZ == -100, "Zero ticks must not advance the staged fall.");
        core.Tick({139,139}, input);
        Check(core.IsBossFallPlaying() && core.worldObjects[23].health == boss.health &&
            std::memcmp(&core.worldObjects[1], &player, sizeof(player)) == 0,
            "The fall must freeze ordinary simulation and never move or pitch the actual player.");
        core.Tick({1,1}, input);
        Check(!core.IsBossFallPlaying() && !core.IsCutscenePlaying() && core.GetBossFallView().empty(),
            "The native post-draw depth limit must finish the fall on tick 140.");
    }

    void TestBossAndFloor(const std::filesystem::path& assets)
    {
        Host core(assets);
        core.StartLevel(26);
        Check(core.GetRuntimeState() == GameRuntimeState::Ready, "Original final level must load.");
        auto& boss = core.worldObjects[23];
        const auto dormant = boss;
        core.tickCount = 1;
        DispatchObjectCase(boss, core);
        Check(boss.health == 1020 && boss.posY == dormant.posY,
            "Boss must remain dormant until native activation group 20.");
        CheckSupported(UpdateObjects(core));
        core.HandleActivationTriggerContact(core.worldObjects[60]);
        Check(boss.health == 1000, "The original entrance trigger must activate the boss.");
        boss.posX = 2550;
        boss.posZ = 2550;
        boss.posY = -50;
        boss.winY = 0;
        boss.generalPurpose1 = 0;
        boss.velocityY = 0;
        boss.velocityX = 150;
        core.soundListenerX = 2550;
        core.soundListenerZ = 2550;
        auto& plank = core.worldObjects[40];
        Check(plank.groupIndexOrLock == 20, "The central original slab must have strength 20.");
        for (int tick = 0; tick < 10; ++tick) { DispatchObjectCase(boss, core); }
        Check(plank.groupIndexOrLock == 0, "Ten boss weight ticks must exhaust the weak slab.");
        DispatchObjectCase(plank, core);
        Check(plank.behavior == BehaviorType::FallingGroundFragment && plank.health == 30 &&
            plank.collisionMask == 0, "Broken ground must lose support and enter a thirty-tick fall.");
        Check(std::any_of(core.worldObjects.begin() + 34, core.worldObjects.begin() + 46,
            [](const RawGameObject& object) { return object.behaviorParameter == 1; }),
            "The floor collapse must propagate to adjacent slabs.");
        const auto y = plank.posY;
        core.tickCount = 4;
        DispatchObjectCase(plank, core);
        Check(plank.velocityY == 4 && plank.posY == y - 4,
            "Falling ground must preserve per-tick signed-half gravity.");
        core.tickCount = 1;
        boss.velocityX = 179;
        DispatchObjectCase(core.worldObjects[28], core);
        Check(std::any_of(core.worldObjects.begin(), core.worldObjects.end(),
            [](const RawGameObject& object)
            {
                return object.behavior == BehaviorType::FlyingArrow && object.itemValue == 7 &&
                    object.spriteScale == 21 && object.collisionRadiusXZ == 40 &&
                    object.collisionRadiusY == 32 && object.behaviorParameter == 30;
            }), "The boss must emit the native large subtype-nine fireball.");
        for (int tick = 0; tick < 1000 && boss.health > 0 && !core.IsBossFallPlaying(); ++tick)
        {
            CheckSupported(UpdateObjects(core));
        }
        CompleteBossFall(core);
        Check(boss.health == 0 && core.flashes == 0 && core.worldObjects[28].health == 0,
            "The staged fall must remove the boss and its emitters only after playback finishes.");
        Check(core.worldObjects[58].health == 1000 && core.worldObjects[59].generalPurpose2 == 3,
            "Boss defeat must activate the original group-two rescue route.");
        core.worldObjects[1].posX = core.worldObjects[46].posX;
        core.worldObjects[1].posY = core.worldObjects[46].posY;
        core.worldObjects[1].posZ = core.worldObjects[46].posZ;
        DispatchObjectCase(core.worldObjects[46], core);
        Check(core.playerLife <= 0, "Falling into the boss pit must kill the player, not complete the game.");
    }

    void TestAuthoredBossChase(const std::filesystem::path& assets)
    {
        Host core(assets);
        Audio audio;
        core.StartLevel(26);
        const auto sounds = core.Read("SAMP\\S_TAB.PRB");
        core.LoadSoundEffects(sounds.data(), sounds.size());
        core.audioHandler = &audio;
        core.HandleActivationTriggerContact(core.worldObjects[60]);
        core.tickCount = 1;
        core.soundListenerX = 2550;
        core.soundListenerZ = 2550;
        for (int tick = 0; tick < 3000 && core.worldObjects[23].health > 0 && !core.IsBossFallPlaying(); ++tick)
        {
            CheckSupported(UpdateObjects(core));
        }
        const auto& boss = core.worldObjects[23];
        CompleteBossFall(core);
        if (boss.health != 0)
        {
            throw std::runtime_error("Authored boss did not reach the weak floor: " +
                std::to_string(boss.posX) + "," + std::to_string(boss.posY) + "," + std::to_string(boss.posZ));
        }
        Check(core.flashes == 0 && core.worldObjects[59].generalPurpose2 == 3,
            "Luring the original boss into the trap room must defeat it and open the magician's route.");
    }

    void TestDamAndDebris(const std::filesystem::path& assets)
    {
        Host core(assets);
        core.StartLevel(26);
        core.tickCount = 1;
        auto& dam = core.worldObjects[9];
        auto& water = core.worldObjects[6];
        core.DamageObject(dam, 60);
        Check(dam.generalPurpose2 == 10 && water.health == 1000,
            "Hitting the original dam must start its hinge and activate water group one.");
        for (int tick = 0; tick < 11; ++tick) { DispatchObjectCase(dam, core); }
        Check(dam.blendWeightOrSpriteOpacity == 100 && dam.behavior == BehaviorType::None &&
            core.GetCollisionMapPixelAtWorldXZ(dam.posX, dam.posZ) == 0,
            "The dam must finish opening and release its collision-map gate.");
        std::memset(core.collisionMap->data, 0, sizeof(core.collisionMap->data));
        core.level->surfaceFlags[3][21] = 0;
        RawGameObject debris;
        debris.health = 1000;
        debris.posX = static_cast<std::int16_t>(water.posX + water.velocityX);
        debris.posY = water.posY;
        debris.posZ = water.posZ;
        debris.collisionMask = 0x80;
        debris.collisionRadiusXZ = debris.collisionRadiusY = 5;
        core.ReplaceWorldObject(100, debris);
        const auto x = water.posX;
        const auto revision = core.level->geometryRevision;
        DispatchObjectCase(water, core);
        Check(water.posX == x - 6 && water.generalPurpose2 == 6 &&
            core.GetCollisionMapPixelAtWorldXZ(water.posX, water.posZ) == 1 &&
            core.level->surfaceFlags[3][21] == 225 && core.level->geometryRevision > revision &&
            core.collisionmapUploadDirty, "Water must advance and update collision and rendered surface maps.");
        Check(core.worldObjects[100].behavior == BehaviorType::FloatingDebris &&
            core.worldObjects[100].collisionType == CollisionType::SwimmingParticles,
            "The flood must convert contacted fragments into floating debris.");
        Check(std::any_of(core.worldObjects.begin(), core.worldObjects.end(), [](const RawGameObject& object)
            {
                return object.health == 40 && object.behavior == BehaviorType::DriftingParticle &&
                    object.modelId == 32 && object.modelAnim == 5 && object.animFrame == 5 &&
                    object.modelFrame2 == 4 && object.displayType == DisplayType::VectorNormal;
            }), "The original flood front must emit a renderable native water-ripple morph every tick.");
        core.PatchCollisionMapFootprint(static_cast<std::int16_t>(water.posX + water.velocityX),
            water.posZ, 0, 0, 0, 0, 0, 0, 0x38);
        DispatchObjectCase(water, core);
        Check(water.behavior != BehaviorType::FloodingWater,
            "The advancing flood must stop at the next wall, allowing its slot to become the final ripple.");
        debris.behavior = BehaviorType::FlyingArrow;
        debris.itemValue = 8;
        debris.collisionMask = 1;
        debris.posX = 2550;
        debris.posZ = 2550;
        debris.posY = 100;
        debris.velocityY = 0;
        core.soundListenerX = debris.posX;
        core.soundListenerZ = debris.posZ;
        core.randomSeed = 0;
        Function_FlyingArrow(debris, core);
        Check(debris.itemValue == 9 && debris.velocityY == 0,
            "Nearby authored debris must leave its standby phase before beginning to fall.");
        const auto floor = core.GetFloorHeight(debris.posX, debris.posY, debris.posZ);
        const auto support = floor < -300 ? -1000 : floor;
        debris.posY = static_cast<std::int16_t>(support - 1);
        debris.velocityY = -12;
        Function_FlyingArrow(debris, core);
        Check(debris.posY == support && debris.velocityY == 3 && debris.behavior == BehaviorType::None &&
            debris.collisionType == CollisionType::None && debris.behaviorParameter == 12,
            "Falling debris must bounce at quarter speed and settle when its rebound is below four.");
    }

    void TestRecordedBatching(const std::filesystem::path& assets)
    {
        Host single(assets), batched(assets);
        Input input;
        single.StartLevel(96);
        batched.StartLevel(96);
        const auto initial = batched.worldObjects;
        batched.Tick({0,0}, input);
        Check(std::memcmp(initial.data(), batched.worldObjects.data(), sizeof(initial)) == 0,
            "A zero-tick ending update must not consume a frame or mutate actors.");
        for (int tick = 0; tick < 24; ++tick) { single.Tick({1,1}, input); }
        const auto result = batched.Tick({24,24}, input);
        Check(std::memcmp(single.worldObjects.data(), batched.worldObjects.data(),
            sizeof(single.worldObjects)) == 0 && single.randomSeed == batched.randomSeed &&
            result.playerMovement && result.playerMovement->simulatedTicks == 24,
            "Batched recorded playback must preserve actor, camera, RNG and movement-result equivalence.");
    }

    void TestEndingInputs(const std::filesystem::path& assets)
    {
        Host core(assets);
        Audio audio;
        core.audioHandler = &audio;
        const std::vector<std::uint8_t> wave{
            'R','I','F','F',40,0,0,0,'W','A','V','E',
            'f','m','t',' ',16,0,0,0,1,0,1,0,
            0x11,0x2b,0,0,0x11,0x2b,0,0,1,0,8,0,
            'd','a','t','a',4,0,0,0,0,64,128,255
        };
        core.musicOverride = wave;
        core.CreateAmbientLoop(101);
        Check(audio.music == std::vector<std::uint8_t>({0,64,128,255}) && !audio.looping,
            "Music decoding must pass only the exact PCM data, without RIFF headers.");
        core.CreateAmbientLoop(102);
        Check(audio.creations == 1, "Music reuse must not allocate a second track.");
        core.CreateAmbientLoop(100);
        Check(audio.looping && audio.creations == 2, "Music selector100 must retain native looping.");
        core.musicOverride->pop_back();
        Reject<std::invalid_argument>([&] { core.CreateAmbientLoop(101); });
        core.musicOverride = wave;
        (*core.musicOverride)[22] = 2;
        Reject<std::invalid_argument>([&] { core.CreateAmbientLoop(101); });
        Check(audio.creations == 2, "Invalid music must not reach the audio backend.");
        Host recording(assets);
        recording.recordingOverride = recording.Read("LEVEL\\DEMO6.KOO");
        (*recording.recordingOverride)[8] = 1;
        Reject<std::invalid_argument>([&] { recording.StartLevel(96); });
        Check(recording.GetRuntimeState() == GameRuntimeState::LoadFailed,
            "Unrecovered recorded action bits must surface as a failed level load.");
        recording.recordingOverride = recording.Read("LEVEL\\DEMO6.KOO");
        recording.recordingOverride->pop_back();
        Reject<std::invalid_argument>([&] { recording.StartLevel(96); });
        Check(recording.GetRuntimeState() == GameRuntimeState::LoadFailed,
            "A partial recorded camera frame must surface as a failed level load.");
    }

    void TestEnding(const std::filesystem::path& assets)
    {
        Host core(assets);
        Audio audio;
        Input input;
        core.StartLevel(26);
        const auto table = core.Read("SAMP\\S_TAB.PRB");
        core.LoadSoundEffects(table.data(), table.size());
        core.audioHandler = &audio;
        core.HandleLevelEndContact(core.worldObjects[19], false);
        Check(core.GetLoadedLevelNumber() == 96 && core.IsCutscenePlaying() && core.paths == 1,
            "Magician rescue must load DEMO6 in asset96, not meteor97 or the main menu.");
        Check(audio.creations == 1 && audio.starts == 1 && !audio.looping && audio.music.size() > 800000,
            "Ending music101 must decode MUSIC1's RIFF chunks as non-looping PCM.");
        const auto path = core.Read("LEVEL\\DEMO6.KOO");
        for (int frame = 0; frame < 498; ++frame)
        {
            if (frame == 100)
            {
                core.SetPaused(true);
                core.Tick({10,10}, input);
                core.SetPaused(false);
            }
            const auto result = core.Tick({1,1}, input);
            if (result.objects) { CheckSupported(*result.objects); }
            if (frame == 497)
            {
                Check(result.status == GameTickStatus::LevelEnded && core.GetLoadedLevelNumber() == 95,
                    "Exactly 498 recorded frames must transition into the native village epilogue.");
                break;
            }
            const auto offset = 3 + frame * 6;
            const auto x = WrapSignedWord(path[offset] | (path[offset + 1] << 8));
            const auto z = WrapSignedWord(path[offset + 2] | (path[offset + 3] << 8));
            Check(core.worldObjects[1].posX == x && core.worldObjects[1].posZ == z,
                "Recorded coordinates must match six-byte frames exactly, including after pause.");
        }
        Check(audio.creations == 1 && audio.starts == 1 && audio.stops == 0 &&
            core.worldObjects[1].itemValue == 630,
            "The 630-tick epilogue must reuse, not restart or interrupt, MUSIC1.");
        for (int tick = 0; tick < 630; ++tick)
        {
            const auto result = core.Tick({1,1}, input);
            if (result.objects) { CheckSupported(*result.objects); }
        }
        Check(core.GetRuntimeState() == GameRuntimeState::NoLevel && audio.stops == 1 &&
            core.GetMenuPage() == MenuPage::MainMenu, "The complete ending must return cleanly to the main menu.");
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc != 2) { throw std::invalid_argument("Pass the original asset directory."); }
        TestBossAndFloor(argv[1]);
        TestAuthoredBossChase(argv[1]);
        TestDamAndDebris(argv[1]);
        TestRecordedBatching(argv[1]);
        TestEndingInputs(argv[1]);
        TestEnding(argv[1]);
        std::cout << "Final boss, breakable floor, rescue, recorded ending and music passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
