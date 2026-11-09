#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <algorithm>
#include <cmath>
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

    std::vector<std::uint8_t> Read(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file) { throw std::runtime_error("Missing fixture: " + path.string()); }
        return {std::istreambuf_iterator<char>(file), {}};
    }

    struct Input final : InputHandler
    {
        bool interact = false, attack = false;
        bool IsKeyPressed(InputKey key) override
        {
            return (interact && key == InputKey::InteractionKey) || (attack && key == InputKey::AttackKey);
        }
    };

    struct Audio final : AudioHandler
    {
        int calls = 0;
        std::uint8_t sample = 0, volume = 0;
        std::uint16_t GetSampleCount() const override { return 129; }
        bool IsChannelPlaying(std::size_t) const override { return false; }
        void PlaySample(std::size_t, std::uint8_t id, std::uint8_t gain) override
        {
            ++calls; sample = id; volume = gain;
        }
        void StopAllSamples() override {}
    };

    struct Host final : GameContext
    {
        std::filesystem::path assets;
        std::string message;
        std::int16_t duration = 0;
        int prompts = 0, destroyed = 0;
        explicit Host(const std::filesystem::path& path) : assets(path) {}
        void Load()
        {
            inventory = {};
            weaponAmounts = {};
            weaponInventorySlots = {};
            arrowPoison = 0;
            StartLevel(90);
            Check(GetRuntimeState() == GameRuntimeState::Ready, "Original witch level must load.");
        }
        void PromptUserMessage(std::string_view text, std::int16_t ticks) override
        {
            message = text; duration = ticks; ++prompts;
        }
        bool IsUserMessageActive() const override { return !message.empty() && duration != 0; }
        bool IsBottomUserMessageActive() const override { return IsUserMessageActive() && duration < 0; }
        void OnObjectDestroyed(int) override { ++destroyed; }
    protected:
        std::vector<std::uint8_t> LoadFile(std::string_view name) override { return Read(assets / name); }
        void LoadLevel(const LevelLoadRequest& request) override
        {
            LevelLoadData data;
            LoadLevelGeometry(request.levelNumber, data);
            LoadLevelObjects(request.levelNumber, data);
            InitializeLevelPlayerSpawn(data, {0, 0, 100});
            CompleteLevelLoad(request.id, std::move(data));
        }
    };

    void TestOriginalAudit(const std::filesystem::path& assets)
    {
        Host host(assets);
        host.Load();
        Check(host.level->objectDescriptions[134] == "(Die Medizin kostet = Goldst\x81" "cke" &&
            host.level->trackedTextureFrameCounts[7] == 5,
            "Native indexed descriptions and all five mouth frames must load.");
        for (int step = 0; step < 100; ++step)
        {
            const auto results = UpdateObjects(host);
            for (const auto& result : results)
            {
                Check(result.status != DispatchStatus::UnimplementedHandler,
                    "Every live behavior in original OBJPOS90 must be implemented.");
            }
            ++host.levelElapsedTicks;
        }
        Check(host.selectedWeapon == WeaponType::ShopGold && host.requestedWeapon == host.selectedWeapon,
            "The original manager must force weapon selector 10.");
    }

    void TestPurchases(const std::filesystem::path& assets)
    {
        for (const int slot : {11, 13, 27, 29, 30, 31, 32, 33, 34, 35})
        {
            Host host(assets);
            host.Load();
            auto& proxy = host.worldObjects[slot];
            const auto stockSlot = proxy.itemValue;
            const auto price = proxy.doorLockOrGroupBitmask;
            host.gold = price;
            Check(HandlePlayerObjectInteraction(proxy, host).status == InteractionStatus::Applied,
                "Every original shop offering must delegate to the linked pickup.");
            Check(host.gold == 0 && proxy.health == 0 && host.destroyed >= 1,
                "Exact funds must pay the original price and remove the proxy through lifecycle hooks.");
            const auto inventory = host.inventory;
            Check(HandlePlayerObjectInteraction(proxy, host).status == InteractionStatus::InactiveObject &&
                host.gold == 0 && std::memcmp(inventory.data(), host.inventory.data(), sizeof(inventory)) == 0,
                "Sold proxies must not charge or grant an item twice.");
            Check(host.worldObjects[stockSlot].health == 0, "Original stock must be consumed by its pickup response.");
        }
        Host host(assets);
        host.Load();
        auto& medicine = host.worldObjects[11];
        host.gold = medicine.doorLockOrGroupBitmask - 1;
        const auto stock = host.worldObjects[medicine.itemValue];
        Check(HandlePlayerObjectInteraction(medicine, host).status == InteractionStatus::NoEffect &&
            host.gold == medicine.doorLockOrGroupBitmask - 1 && medicine.health == 1000 &&
            std::memcmp(&stock, &host.worldObjects[medicine.itemValue], sizeof(stock)) == 0 &&
            host.message == "Nicht genug Gold",
            "Insufficient funds must report failure without consuming gold, proxy or stock.");

        host.gold = 100;
        for (auto& item : host.inventory) { item.health = 1000; }
        Check(HandlePlayerObjectInteraction(medicine, host).status == InteractionStatus::InventoryFull &&
            host.gold == 80 && medicine.health == 0 &&
            host.worldObjects[medicine.itemValue].health == 1000 &&
            host.worldObjects[medicine.itemValue].collisionMask == 255,
            "Native full-inventory ordering must charge, release the stock and consume the proxy.");

        host.Load();
        auto& stacked = host.inventory[0];
        stacked = host.worldObjects[10];
        const auto amount = stacked.itemValue;
        for (std::size_t i = 1; i < host.inventory.size(); ++i) { host.inventory[i].health = 1000; }
        host.gold = 100;
        Check(HandlePlayerObjectInteraction(host.worldObjects[11], host).status == InteractionStatus::Applied &&
            stacked.itemValue == WrapSignedWord(amount + host.worldObjects[10].itemValue) && host.gold == 80,
            "Medicine must stack even with a full backpack.");

        host.Load();
        auto& dagger = host.worldObjects[30];
        auto& blade = host.worldObjects[dagger.itemValue];
        host.inventory[0] = blade;
        host.gold = 100;
        Check(HandlePlayerObjectInteraction(dagger, host).status == InteractionStatus::Applied &&
            host.gold == 70 && blade.health == 1000 && blade.collisionType == CollisionType::None &&
            blade.pickupType == PickupType::None && dagger.health == 0,
            "Duplicate blades must retain the native disabled-stock response.");

        host.Load();
        auto& fireballs = host.worldObjects[35];
        const auto payload = host.worldObjects[fireballs.itemValue];
        host.inventory[0] = payload;
        const auto original = payload.generalPurpose1;
        host.gold = 100;
        Check(HandlePlayerObjectInteraction(fireballs, host).status == InteractionStatus::Applied &&
            host.inventory[0].generalPurpose1 == static_cast<std::uint16_t>(original * 2) &&
            host.weaponAmounts[payload.itemValue] == WrapSignedWord(original * 2) && host.gold == 50,
            "Purchased fireballs must stack their native quantity and synchronize ammunition.");

        host.Load();
        host.gold = 0;
        host.worldObjects[11].doorLockOrGroupBitmask = -1;
        Check(HandlePlayerObjectInteraction(host.worldObjects[11], host).status == InteractionStatus::Applied &&
            host.gold == 0, "Nonpositive prices must be free, not gold rewards.");
    }

    void TestDescriptions(const std::filesystem::path& assets)
    {
        Host host(assets);
        host.Load();
        Audio audio;
        const auto table = Read(assets / "SAMP\\S_TAB.PRB");
        host.LoadSoundEffects(table.data(), table.size());
        host.audioHandler = &audio;
        auto& item = host.worldObjects[11];
        host.playerCollisionSlot = 11;
        Function_WitchHead(host.worldObjects[5], host);
        const auto& samples = host.soundEffects[item.soundEffectId - 1];
        Check(host.message == "Die Medizin kostet 20 Goldst\x81" "cke" &&
            host.duration == -3 * static_cast<int>(host.message.size()) && item.generalPurpose3 == 40 &&
            audio.calls == 1 && audio.volume == 255 &&
            std::find(samples.samples.begin(), samples.samples.end(), audio.sample) != samples.samples.end() &&
            std::find(host.soundChannelSources.begin(), host.soundChannelSources.end(), 514) != host.soundChannelSources.end(),
            "Selecting an offering must speak its own original sample and format its localized price.");
        const auto prompts = host.prompts;
        Function_WitchHead(host.worldObjects[5], host);
        Check(host.prompts == prompts && audio.calls == 1, "The same selection must not repeatedly restart speech.");
        host.soundListenerX = item.posX + 50; host.soundListenerZ = item.posZ;
        Function_BuyItem(item, host);
        Check(item.generalPurpose3 == 40, "Description cooldown must freeze within native distance 50.");
        host.soundListenerX = item.posX + 51;
        Function_BuyItem(item, host);
        Check(item.generalPurpose3 == 39, "Leaving the offering must advance its per-object cooldown.");
        host.tickCount = 40;
        Function_BuyItem(item, host);
        Check(WrapSignedWord(item.generalPurpose3) == -1, "Cooldown must use wrapped signed native words.");
        host.tickCount = 1;
        Function_WitchHead(host.worldObjects[5], host);
        Check(audio.calls == 2, "Returning after cooldown must permit another spoken selection.");
        host.audioHandler = nullptr;

        const std::string custom = " \r\n(Prix = et =\r\n@\r\n";
        host.level->LoadObjectDescriptions(reinterpret_cast<const std::uint8_t*>(custom.data()), custom.size());
        item.generalPurpose1 = 2; item.generalPurpose3 = 0; item.doorLockOrGroupBitmask = -7;
        host.DescribeShopItem(item);
        Check(host.message == "Prix -7 et =", "Blank lines and only the first price placeholder must be preserved.");
        host.level->objectDescriptions[2] = "(Not for sale";
        item.generalPurpose3 = 0;
        host.DescribeShopItem(item);
        Check(host.message == "Not for sale" && item.generalPurpose3 == 40,
            "Descriptions without price placeholders still need a cooldown.");
    }

    void TestCurtains(const std::filesystem::path& assets)
    {
        for (const int slot : {17, 46})
        {
            Host host(assets);
            host.Load();
            const auto original = host.worldObjects[slot];
            for (int other = 2; other <= 165; ++other) { host.SetObjectHealth(host.worldObjects[other], 0); }
            host.ReplaceWorldObject(slot, original);
            auto& curtain = host.worldObjects[slot];
            auto& player = host.worldObjects[1];
            player.posX = curtain.posX; player.posY = curtain.posY; player.posZ = curtain.posZ;
            Input input;
            Check(UpdatePlayerInteraction(host, input).status == InteractionStatus::Applied &&
                curtain.collisionMask == 0 && curtain.generalPurpose3 == curtain.velocityZ,
                "Both original curtains must start opening automatically, without an interaction key.");
            const auto seed = host.randomSeed;
            Check(UpdatePlayerInteraction(host, input).status == InteractionStatus::NoEffect && host.randomSeed == seed,
                "A curtain must not retrigger its sound after its collision mask is cleared.");
            host.tickCount = 0;
            const auto initial = curtain.blendWeightOrSpriteOpacity;
            Function_CurtainOpens(curtain, host);
            Check(curtain.blendWeightOrSpriteOpacity == initial, "Zero ticks must freeze curtain interpolation.");
            host.tickCount = 3;
            Function_CurtainOpens(curtain, host);
            Check(curtain.blendWeightOrSpriteOpacity == initial + 15, "Curtain opening must scale by behavior ticks.");
            host.tickCount = 100;
            Function_CurtainOpens(curtain, host);
            Check(curtain.blendWeightOrSpriteOpacity == 95, "Curtains must clamp exactly at their authored target.");
            curtain.generalPurpose1 = 10;
            Function_CurtainOpens(curtain, host);
            Check(curtain.blendWeightOrSpriteOpacity == 10, "The signed step helper must also support closing.");
        }
    }

    void TestAnimationAndOwnership(const std::filesystem::path& assets)
    {
        Host host(assets);
        host.Load();
        auto& mouth = host.worldObjects[2];
        mouth.generalPurpose2 = 4;
        host.levelElapsedTicks = 1;
        Function_WitchMouthAnimation(mouth, host);
        Check(host.level->trackedTextureFrames[13] == 2, "Texture frame advancement must retain native period/remainder.");
        mouth.generalPurpose2 = 4;
        host.PromptUserMessage("speech", -60);
        bool changed = false;
        for (int i = 0; i < 50; ++i)
        {
            mouth.generalPurpose2 = mouth.behaviorParameter;
            Function_WitchMouthAnimation(mouth, host);
            const auto frame = host.level->trackedTextureFrames[13];
            Check(frame >= 1 && frame <= 5, "Speaking must wrap random mouth phases into five native frames.");
            changed |= frame != 1;
        }
        Check(changed, "The witch must visibly articulate her spoken descriptions.");
        host.PromptUserMessage("centered", 60);
        mouth.generalPurpose2 = 0;
        Function_WitchMouthAnimation(mouth, host);
        Check(mouth.generalPurpose1 == 1, "Centered notifications must not make the witch speak.");

        auto& head = host.worldObjects[5];
        head.generalPurpose2 = 256; head.winX = 3; head.winY = 250; head.velocityY = 2;
        host.soundListenerX = head.posX; host.soundListenerZ = head.posZ + 100;
        host.playerCollisionSlot = 11;
        host.worldObjects[11].generalPurpose3 = 40;
        Function_WitchHead(head, host);
        Check(head.winY > 250 && head.winX == 3, "An attentive witch head must turn toward the player.");
        host.playerCollisionSlot = 0;
        host.tickCount = 30;
        Function_WitchHead(head, host);
        Check(head.generalPurpose2 == 0 && head.winX == 9, "Leaving contact must return the head to its subtle resting tilt.");
        const auto restingYaw = head.winY;
        host.soundListenerX = head.posX + 100;
        host.soundListenerZ = head.posZ;
        Function_WitchHead(head, host);
        Check(head.winY == restingYaw && head.winX == 9,
            "The reduced pitch must not allow a resting witch to turn when the native nod phase blocks yaw.");
        host.tickCount = 0;
        for (std::uint16_t phase = 0; phase < 512; ++phase)
        {
            head.generalPurpose2 = phase;
            Function_WitchHead(head, host);
            const auto originalPitch = WrapSignedWord(static_cast<std::int32_t>(
                static_cast<std::uint32_t>(Cosine(WrapSignedWord(phase)) * 27) >> 16) + 30);
            Check(head.winX == 3 + (originalPitch - 3) / 9 && head.winX >= 3 && head.winX <= 9,
                "Every native witch nod phase must produce the reduced 3..9 angle range in the behavior.");
        }

        auto& petting = host.worldObjects[14];
        petting.velocityZ = 127; petting.velocityX = 5;
        const auto previous = petting.modelFrame2;
        host.tickCount = 1;
        Function_UnderwaterPlants(petting, host);
        Check(petting.velocityZ == -128 && petting.blendWeightOrSpriteOpacity == 0 &&
            petting.animFrame == previous && petting.modelFrame2 != previous &&
            petting.modelFrame2 >= 1 && petting.modelFrame2 <= 4,
            "The dog-petting animation must select a different allowed pose at the morph boundary.");

        auto& offer = host.worldObjects[29];
        host.weaponInventorySlots[offer.behaviorParameter] = 1;
        const auto linked = offer.itemValue;
        Function_BuyItem(offer, host);
        Check(offer.health == 0 && host.worldObjects[linked].collisionType == CollisionType::None,
            "Already-owned unique weapons must disappear from the shop without deleting decorative stock.");
    }

    void TestLinkedRenderState(const std::filesystem::path& assets)
    {
        Host host(assets);
        host.Load();
        const auto snapshot = host.CaptureSaveGame("Linked");
        const auto& frame = host.level->modelAnchors[4]->animations[1][0];
        Check(frame.linkedObjectVertices[29] == 31 && frame.linkedObjectVertices[30] == 34,
            "The witch body must link polygon 30 to her arm and polygon 31 to the dog.");
        for (const int slot : {7, 8, 14, 15})
        {
            const auto pose = host.GetObjectRenderState(slot);
            Check(pose.health == host.worldObjects[slot].health &&
                (pose.posX != host.worldObjects[slot].posX || pose.posZ != host.worldObjects[slot].posZ),
                "Placement-20 children must render at the parent anchor rather than their stored editor position.");
        }
        Check(host.CaptureSaveGame("Linked") == snapshot,
            "Resolving linked presentation must not change native save bytes.");
        auto& parent = host.worldObjects[9];
        parent.winX = parent.winY = parent.winZ = 0;
        host.worldObjects[14].velocityY = 17;
        const auto pose = host.GetObjectRenderState(14);
        const auto& vertex = frame.vertices[30];
        Check(pose.posX == parent.posX + static_cast<int>(std::lround(vertex[0] / 10.0)) &&
            pose.posY == parent.posY - static_cast<int>(std::lround(vertex[1] / 10.0)) &&
            pose.posZ == parent.posZ + static_cast<int>(std::lround(vertex[2] / 10.0)) &&
            pose.winY == 17,
            "Linked models must use the polygon vertex, parent transform and velocity-Y heading offset.");
        parent.posX += 100;
        Check(host.GetObjectRenderState(14).posX == pose.posX + 100,
            "Parent movement alone must invalidate the child's presented pose.");
        host.SetObjectHealth(parent, 0);
        Check(host.GetObjectRenderState(14).health == 0 && host.GetObjectRenderState(15).health == 0,
            "Dead parents must not leave visible attached parts.");
    }

    void TestSaveContinuation(const std::filesystem::path& assets)
    {
        Host host(assets);
        host.Load();
        host.gold = 100;
        HandlePlayerObjectInteraction(host.worldObjects[11], host);
        auto& curtain = host.worldObjects[17];
        curtain.collisionMask = 0; curtain.generalPurpose3 = curtain.velocityZ;
        host.tickCount = 3;
        Function_CurtainOpens(curtain, host);
        const auto saved = host.CaptureSaveGame("Witch");
        Host restored(assets);
        restored.LoadGame(saved);
        Check(restored.gold == 80 && restored.worldObjects[11].health == 0 &&
            restored.worldObjects[10].health == 0 &&
            std::memcmp(&curtain, &restored.worldObjects[17], sizeof(curtain)) == 0,
            "Native SPL must retain purchases, gold and partially opened curtain state.");
        restored.tickCount = 3;
        Function_CurtainOpens(restored.worldObjects[17], restored);
        Check(restored.worldObjects[17].blendWeightOrSpriteOpacity == curtain.blendWeightOrSpriteOpacity + 15,
            "A restored curtain must continue from its saved morph weight.");
    }

    void TestGameplayTick(const std::filesystem::path& assets)
    {
        Host host(assets);
        host.Load();
        host.selectedWeapon = host.requestedWeapon = WeaponType::Dagger;
        host.weaponReady = true; host.weaponSpriteFrame = 1; host.weaponAnimationSpeed = 0;
        host.gold = 123;
        Input input;
        input.attack = true;
        const auto result = host.Tick({1, 1}, input);
        Check(result.status == GameTickStatus::Advanced && result.weapon &&
            result.weapon->animation == WeaponActionStatus::Applied &&
            result.weapon->attack == WeaponActionStatus::NoEffect &&
            host.selectedWeapon == WeaponType::ShopGold && host.weaponAnimationSpeed == 100 &&
            !host.GetWeaponView(),
            "Entering the shop with a drawn weapon must show gold, never crash or permit attacks.");
        const auto view = host.GetApplicationPresentation();
        Check(std::any_of(view.commands.begin(), view.commands.end(), [](const UiDrawCommand& command)
            {
                const auto* text = std::get_if<UiText>(&command);
                return text && text->text == "Gold: 123";
            }), "The shop must present the current gold balance instead of a first-person weapon.");
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc != 2) { throw std::runtime_error("Expected original asset directory."); }
        const std::filesystem::path assets(argv[1]);
        TestOriginalAudit(assets);
        TestPurchases(assets);
        TestDescriptions(assets);
        TestCurtains(assets);
        TestAnimationAndOwnership(assets);
        TestLinkedRenderState(assets);
        TestSaveContinuation(assets);
        TestGameplayTick(assets);
        std::cout << "Witch-level behaviors, shop purchases, speech, curtains and saves passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
