#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

using namespace Desperabis;

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition) { throw std::runtime_error(message); }
    }

    class Host final : public GameContext
    {
    public:
        std::filesystem::path root;
        std::string message;
        std::int16_t duration = 0;
        int calls = 0;
        int destroyed = 0;
        int lifeChanges = 0;

        LevelLoadData ReadLevel()
        {
            LevelLoadData data;
            LoadLevelGeometry(50, data);
            LoadLevelObjects(50, data);
            return data;
        }

        void PromptUserMessage(std::string_view text, std::int16_t ticks) override
        {
            message = text;
            duration = ticks;
            ++calls;
        }

        void OnObjectDestroyed(int) override { ++destroyed; }
        void OnPlayerHealthChanged(std::int32_t, std::int32_t) override { ++lifeChanges; }

    protected:
        std::vector<std::uint8_t> LoadFile(std::string_view filename) override
        {
            std::ifstream file(root / filename, std::ios::binary);
            if (!file) { throw std::runtime_error("Missing tutorial fixture."); }
            return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
        }
    };

    class NoInput final : public InputHandler
    {
    public:
        bool IsKeyPressed(InputKey) override
        {
            throw std::runtime_error("Tutorial contact must not require input.");
        }
    };

    void PlacePlayer(Host& host, const RawGameObject& trigger)
    {
        host.worldObjects[1] = trigger;
        host.worldObjects[1].collisionType = CollisionType::None;
        host.worldObjects[1].behavior = BehaviorType::None;
        host.worldObjects[1].collisionMask = 1;
        host.worldObjects[1].collisionRadiusXZ = 10;
        host.worldObjects[1].collisionRadiusY = 10;
    }

    void TestOriginalTriggers(const std::filesystem::path& root)
    {
        Host host;
        host.root = root;
        auto data = host.ReadLevel();
        Check(data.level->textureFileIndices[10] == 39, "Tutorial texture slot 10 must resolve TUR39.");
        const auto& rows = data.level->textureMessages[10];
        Check(rows.size() == 14 && rows[2] == "Nimm den Schl\x81ssel",
            "Original length-prefixed DOS text must be decoded, not rendered as pixels.");
        Check(rows[9].find('\\') != std::string::npos && rows[9].find('\xfb') != std::string::npos,
            "Native line breaks and control bytes must survive decoding.");
        host.level = data.level.get();
        int contacts = 0;
        int behaviors = 0;
        for (std::size_t slot = 2; slot <= 164; ++slot)
        {
            const auto original = data.objects[slot];
            if (original.collisionType != CollisionType::TriggerTextPrompt &&
                original.behavior != BehaviorType::TutorialTrigger) { continue; }
            host.worldObjects = {};
            host.ReplaceWorldObject(static_cast<int>(slot), original);
            auto& object = host.worldObjects[slot];
            PlacePlayer(host, object);
            const int calls = host.calls;
            if (object.behavior == BehaviorType::TutorialTrigger)
            {
                Check(DispatchObjectCase(object, host).status == DispatchStatus::ExecutedCase,
                    "Tutorial behavior must be implemented.");
                ++behaviors;
            }
            else
            {
                NoInput input;
                Check(UpdatePlayerInteraction(host, input).status == InteractionStatus::Applied,
                    "Tutorial colliders must present text without Enter.");
                ++contacts;
            }
            Check(host.calls == calls + 1 && host.message == rows[original.itemValue] &&
                host.duration == WrapSignedWord(original.generalPurpose1),
                "Every original trigger must use its own row and signed duration.");
            Check(object.health == (original.behaviorParameter ? 0 : original.health),
                "One-shot triggers must deactivate; repeatable triggers must survive.");
        }
        Check(contacts == 13 && behaviors == 3 && host.destroyed == 15,
            "All sixteen tutorial triggers and immediate one-shot lifecycle hooks must be covered.");

        host.worldObjects = {};
        host.ReplaceWorldObject(9, data.objects[9]);
        auto& trigger = host.worldObjects[9];
        PlacePlayer(host, trigger);
        auto blocker = trigger;
        blocker.behavior = BehaviorType::None;
        host.ReplaceWorldObject(8, blocker);
        int calls = host.calls;
        DispatchObjectCase(trigger, host);
        Check(host.calls == calls && trigger.health == 1000, "Nearby items must block post-pickup prompts.");
        host.SetObjectHealth(host.worldObjects[8], 0);
        host.worldObjects[1].posX += 1000;
        DispatchObjectCase(trigger, host);
        Check(host.calls == calls, "Tutorial behaviors require player overlap.");
        PlacePlayer(host, trigger);
        host.objectCollisionsEnabled = false;
        DispatchObjectCase(trigger, host);
        Check(host.calls == calls, "Disabled collision queries must suppress tutorial behaviors.");
        host.objectCollisionsEnabled = true;
        host.behaviorDispatchEnabled = false;
        host.HandleTextPromptContact(trigger);
        Check(trigger.health == 1000 && host.calls == calls + 1,
            "Native DS:04CD gates removal, not text presentation.");
        host.behaviorDispatchEnabled = true;
        DispatchObjectCase(trigger, host);
        Check(trigger.health == 0 && host.message == rows[3], "Removing the item must unlock the correct prompt.");

        trigger = data.objects[6];
        calls = host.calls;
        host.HandleTextPromptContact(trigger);
        host.HandleTextPromptContact(trigger);
        Check(host.calls == calls + 2 && trigger.health == 1000, "Repeatable triggers must refresh the prompt.");
        trigger.itemValue = 14;
        bool rejected = false;
        try { host.HandleTextPromptContact(trigger); }
        catch (const std::out_of_range&) { rejected = true; }
        Check(rejected, "An unavailable prompt row must fail explicitly.");
    }

    void TestMalformedTexture()
    {
        Level level;
        for (const auto bytes : { std::vector<std::uint8_t>{ 1,0,0,0,2,65 },
            std::vector<std::uint8_t>{ 1,0,0,0,1 } })
        {
            bool rejected = false;
            try { level.LoadTextureMessages(10, bytes.data(), bytes.size()); }
            catch (const std::invalid_argument&) { rejected = true; }
            Check(rejected && level.textureMessages[10].empty(), "Malformed message textures must not install partial rows.");
        }
    }

    void TestWebAndFireContact(const std::filesystem::path& root)
    {
        Host host;
        host.root = root;
        auto data = host.ReadLevel();
        NoInput input;
        host.ReplaceWorldObject(62, data.objects[62]);
        auto& web = host.worldObjects[62];
        PlacePlayer(host, web);
        const auto life = host.playerLife;
        host.behaviorDispatchEnabled = false;
        Check(UpdatePlayerInteraction(host, input).status == InteractionStatus::NoEffect &&
            web.collisionType == CollisionType::DestroyOnCollide, "Disabled DS:04CD must preserve walked-through webs.");
        host.behaviorDispatchEnabled = true;
        Check(UpdatePlayerInteraction(host, input).status == InteractionStatus::Applied &&
            web.collisionMask == 0 && web.collisionType == CollisionType::None && web.animFrame == 2 &&
            web.health == 1000 && host.playerLife == life,
            "Web contact must select the destroyed next frame and disable collision, without damage or fake deletion.");
        Check(UpdatePlayerInteraction(host, input).status == InteractionStatus::NoEffect,
            "Walked-through webs must not collide a second time.");
        for (int slot : { 25,26,27 })
        {
            host.worldObjects = {};
            host.ReplaceWorldObject(slot, data.objects[slot]);
            auto& fire = host.worldObjects[slot];
            PlacePlayer(host, fire);
            const auto playerHealth = host.worldObjects[1].health;
            const auto previousLife = host.playerLife;
            const int notifications = host.lifeChanges;
            host.behaviorDispatchEnabled = false;
            Check(UpdatePlayerInteraction(host, input).status == InteractionStatus::Applied &&
                host.playerLife == previousLife - 29491 * fire.behaviorParameter &&
                host.playerHitEffectTicks == 8 && fire.health == 1000 &&
                host.worldObjects[1].health == playerHealth && host.lifeChanges == notifications + 1,
                "Fire contact must damage signed 16.16 life and notify the HUD, independently of DS:04CD.");
            UpdatePlayerInteraction(host, input);
            Check(host.playerLife == previousLife - 2 * 29491 * fire.behaviorParameter,
                "Remaining in fire must apply damage on each contact update.");
            host.playerLifeDeltaSuppressed = 1;
            const auto suppressedLife = host.playerLife;
            UpdatePlayerInteraction(host, input);
            Check(host.playerLife == suppressedLife, "Fire must honor native life-delta suppression.");
            host.playerLifeDeltaSuppressed = 0;
            host.DEBUG_noPlayerCollision = true;
            UpdatePlayerInteraction(host, input);
            Check(host.playerLife == suppressedLife, "No-collision cheat must suppress fire damage.");
            host.DEBUG_noPlayerCollision = false;
            fire.behaviorParameter = 0;
            Check(UpdatePlayerInteraction(host, input).status == InteractionStatus::NoEffect,
                "Zero contact strength must not cause damage.");
            host.objectCollisionsEnabled = false;
            fire.behaviorParameter = 18;
            Check(UpdatePlayerInteraction(host, input).status == InteractionStatus::NoEffect &&
                host.playerLife == suppressedLife, "Disabled object collisions must prevent fire contact.");
            host.objectCollisionsEnabled = true;
        }
    }
}

int main(int argc, char* argv[])
{
    try
    {
        Check(argc == 2, "Expected original asset root.");
        TestOriginalTriggers(argv[1]);
        TestMalformedTexture();
        TestWebAndFireContact(argv[1]);
        std::cout << "Tutorial prompt tests passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
