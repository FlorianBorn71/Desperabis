#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <charconv>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

using namespace Desperabis;

namespace
{
    struct ConsoleInputHandler final : InputHandler
    {
        bool interactionKeyPressed = false;
        bool IsInteractionKeyPressed() override { return interactionKeyPressed; }
    };

    struct ConsoleAudioHandler final : AudioHandler
    {
        std::array<bool, SoundChannelCount> playing{};
        std::uint16_t GetSampleCount() const override { return 129; }
        bool IsChannelPlaying(std::size_t channel) const override { return playing.at(channel); }
        void PlaySample(std::size_t channel, std::uint8_t sample, std::uint8_t volume) override
        {
            std::cout << "Sound request: channel " << channel + 1 << ", sample "
                << static_cast<unsigned int>(sample) << ", volume " << static_cast<unsigned int>(volume)
                << " (logging only; no audio device)\n";
            playing.at(channel) = true;
        }
    };

    void PrintUsage()
    {
        std::cout << "Usage:\n"
            "  desperabis-recovery --list\n"
            "  desperabis-recovery --behavior BatIndoor|14|0xE [--ticks 3] [--dispatch-disabled]\n"
            "  desperabis-recovery --object object54.bin [--ticks 3] [--dispatch-disabled]\n"
            "  Add --interact to request an ENTER-key interaction instead of a behavior update.\n"
            "  Add --player player54.bin to select the --object target through player collision first.\n"
            "  With --player but without --interact, only collision selection is performed.\n"
            "  Default: CreateDummyLevel() supplies an empty 200x200 collision map.\n"
            "  Add --collision-map map.bin to replace the dummy map with a buffer dump.\n"
            "  Add --geometry FELD.256 to load 50x40 geometry (header plus seven byte planes).\n"
            "  Add --sound-table S_TAB.PRB to log sample playback through a console AudioHandler.\n"
            "  Add --collision-map-output updated.bin to save the 40004-byte dummy map allocation.\n"
            "Exit 2: partial recovery only, not a complete gameplay simulation. Exit 1: invalid input/error.\n";
    }

    std::uint16_t ParseWord(std::string_view text)
    {
        int base = 10;
        if (text.size() > 2 && text.substr(0, 2) == "0x")
        {
            base = 16;
            text.remove_prefix(2);
        }
        unsigned int value = 0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value, base);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
            value > std::numeric_limits<std::uint16_t>::max())
        {
            throw std::invalid_argument("Expected an unsigned 16-bit integer, got: " + std::string(text));
        }
        return static_cast<std::uint16_t>(value);
    }

    RawGameObject ReadObject(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            throw std::runtime_error("Cannot open object file: " + path);
        }
        RawGameObject object;
        file.read(reinterpret_cast<char*>(&object), static_cast<std::streamsize>(sizeof(object)));
        if (file.gcount() != static_cast<std::streamsize>(sizeof(object)) ||
            file.peek() != std::char_traits<char>::eof() || file.bad())
        {
            throw std::runtime_error("Object file must contain exactly 54 bytes: " + path);
        }
        return object;
    }

    int Run(int argc, char** argv)
    {
        if (argc == 2 && std::string_view(argv[1]) == "--help")
        {
            PrintUsage();
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--list")
        {
            for (const auto& entry : BehaviorCatalog())
            {
                std::cout << static_cast<std::uint16_t>(entry.type) << ' ' << entry.name << " 0000:"
                    << std::hex << std::setw(4) << std::setfill('0') << entry.moduleOffset << std::dec
                    << (entry.type == BehaviorType::Pillar || entry.type == BehaviorType::IdleAnimation ||
                        entry.type == BehaviorType::SmokeFading || entry.type == BehaviorType::Switch1 ||
                        entry.type == BehaviorType::SwitchGrouped || entry.type == BehaviorType::WaterDrop ?
                        " implemented case (common processing missing)" :
                        entry.hasSpecificCase ? " unrecovered handler" : " common path only") << '\n';
            }
            return 0;
        }
        GameState context;
        RawGameObject& object = context.worldObjects[2];
        ConsoleAudioHandler audio;
        auto level = CreateDummyLevel();
        context.collisionMap = level.collisionMap.get();
        context.level = &level;
        std::string collisionMapOutput;
        bool haveCollisionMap = false;
        bool haveGeometry = false;
        bool haveObjectFile = false;
        bool haveObject = false;
        bool haveTicks = false;
        bool haveDispatchFlag = false;
        bool interact = false;
        bool havePlayer = false;
        for (int i = 1; i < argc; ++i)
        {
            const std::string_view option = argv[i];
            if (option == "--interact")
            {
                if (interact) { throw std::invalid_argument("Repeated interaction request."); }
                interact = true;
            }
            else if (option == "--dispatch-disabled")
            {
                if (haveDispatchFlag) { throw std::invalid_argument("Repeated dispatch flag."); }
                haveDispatchFlag = true;
                context.behaviorDispatchEnabled = false;
            }
            else if (option == "--player")
            {
                if (havePlayer) { throw std::invalid_argument("Repeated player input."); }
                if (++i == argc) { throw std::invalid_argument("Missing value for --player."); }
                context.worldObjects[1] = ReadObject(argv[i]);
                havePlayer = true;
            }
            else if (option == "--geometry")
            {
                if (haveGeometry) { throw std::invalid_argument("Repeated geometry input."); }
                if (++i == argc) { throw std::invalid_argument("Missing value for --geometry."); }
                std::ifstream file(argv[i], std::ios::binary);
                if (!file) { throw std::runtime_error("Cannot open geometry input."); }
                std::array<std::uint8_t, Level::GeometryStreamSize> data{};
                file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
                if (file.gcount() != static_cast<std::streamsize>(data.size()) || file.bad())
                {
                    throw std::runtime_error("Cannot read complete 50x40 geometry input.");
                }
                level.LoadGeometry(data.data(), data.size());
                haveGeometry = true;
            }
            else if (option == "--sound-table")
            {
                if (context.soundEffectsLoaded) { throw std::invalid_argument("Repeated sound-table input."); }
                if (++i == argc) { throw std::invalid_argument("Missing value for --sound-table."); }
                std::ifstream file(argv[i], std::ios::binary);
                if (!file) { throw std::runtime_error("Cannot open sound-effect table."); }
                std::array<std::uint8_t, SoundEffectTableSize> data{};
                file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
                if (file.gcount() != static_cast<std::streamsize>(data.size()) ||
                    file.peek() != std::char_traits<char>::eof() || file.bad())
                {
                    throw std::runtime_error("Sound-effect table must contain exactly 649 bytes.");
                }
                context.LoadSoundEffects(data.data(), data.size());
                context.audioHandler = &audio;
            }
            else if (option == "--collision-map" || option == "--collision-map-output")
            {
                if (++i == argc) { throw std::invalid_argument("Missing value for " + std::string(option)); }
                if (option == "--collision-map-output")
                {
                    if (!collisionMapOutput.empty()) { throw std::invalid_argument("Repeated collision-map output."); }
                    collisionMapOutput = argv[i];
                }
                else
                {
                    if (haveCollisionMap) { throw std::invalid_argument("Repeated collision-map input."); }
                    std::ifstream file(argv[i], std::ios::binary);
                    if (!file) { throw std::runtime_error("Cannot open collision-map input."); }
                    const auto size = file.seekg(0, std::ios::end).tellg();
                    if (size < 4 || size > static_cast<std::streamoff>(sizeof(CollisionMap)))
                    {
                        throw std::invalid_argument("Collision-map input must contain 4..40004 bytes.");
                    }
                    file.seekg(0);
                    *level.collisionMap = {};
                    file.read(reinterpret_cast<char*>(level.collisionMap.get()), static_cast<std::streamsize>(size));
                    if (!file) { throw std::runtime_error("Cannot read collision-map input."); }
                    const auto pixelCount = (static_cast<std::uint64_t>(level.collisionMap->widthMinusOne) + 1) *
                        (static_cast<std::uint64_t>(level.collisionMap->heightMinusOne) + 1);
                    if (pixelCount > sizeof(level.collisionMap->data) ||
                        pixelCount + offsetof(CollisionMap, data) > static_cast<std::uint64_t>(size))
                    {
                        throw std::invalid_argument("Collision-map input does not contain a supported complete raster.");
                    }
                    haveCollisionMap = true;
                }
            }
            else if (option == "--behavior" || option == "--object" || option == "--ticks")
            {
                if (++i == argc) { throw std::invalid_argument("Missing value for " + std::string(option)); }
                const std::string_view value = argv[i];
                if (option == "--ticks")
                {
                    if (haveTicks) { throw std::invalid_argument("Repeated tick count."); }
                    haveTicks = true;
                    const auto ticks = ParseWord(value);
                    if (ticks > std::numeric_limits<std::int16_t>::max())
                    {
                        throw std::invalid_argument("Tick count must be in 0..32767.");
                    }
                    context.tickCount = static_cast<std::int16_t>(ticks);
                }
                else
                {
                    if (haveObject) { throw std::invalid_argument("Choose exactly one --behavior or --object."); }
                    haveObject = true;
                    if (option == "--object")
                    {
                        object = ReadObject(std::string(value));
                        haveObjectFile = true;
                    }
                    else
                    {
                        const auto named = FindBehavior(value);
                        const auto behavior = named ? named->type : static_cast<BehaviorType>(ParseWord(value));
                        object.behavior = behavior;
                        object.health = 1;
                    }
                }
            }
            else
            {
                throw std::invalid_argument("Unknown option: " + std::string(option));
            }
        }
        if (!haveObject)
        {
            throw std::invalid_argument("Choose --behavior or --object. Use --help for usage.");
        }
        if (!haveObjectFile && object.behavior == BehaviorType::WaterDrop && !interact && !havePlayer &&
            context.behaviorDispatchEnabled)
        {
            object.posX = 150;
            object.posZ = 4050;
            object.posY = WrapSignedWord(context.GetFloorHeight(object.posX, 0, object.posZ) +
                context.GetRoomHeight(object.posX, object.posZ));
        }
        std::cout << "PARTIAL RECOVERY ONLY: shared pre/postprocessing and most handlers are NOT IMPLEMENTED.\n";
        if (!haveCollisionMap)
        {
            std::cout << "Using dummy level: 200x200 collision pixels, initially zero.\n";
        }
        std::cout << (haveGeometry ? "Loaded 50x40 geometry streams.\n" :
            "Using dummy geometry: raw floor 0, room height 50, ceiling present, no special surfaces.\n");
        if (context.audioHandler)
        {
            context.soundListenerX = havePlayer ? context.worldObjects[1].posX : object.posX;
            context.soundListenerZ = havePlayer ? context.worldObjects[1].posZ : object.posZ;
            std::cout << "Console audio logging enabled; listener at "
                << (havePlayer ? "player" : "the test object") << " X/Z.\n";
        }
        if (interact || havePlayer)
        {
            InteractionResult interaction{ InteractionStatus::NoEffect };
            if (havePlayer)
            {
                context.worldObjects[2] = object;
                ConsoleInputHandler input;
                input.interactionKeyPressed = interact;
                interaction = UpdatePlayerInteraction(context, input);
                object = context.worldObjects[2];
                std::cout << "Collision-selected world slot: " << context.playerCollisionSlot << '\n';
            }
            else
            {
                interaction = HandlePlayerObjectInteraction(object, context);
            }
            std::cout << (interact ? "Explicit interaction request (ENTER): " : "No interaction key request: ");
            switch (interaction.status)
            {
            case InteractionStatus::Applied:
                std::cout << "applied implemented gameplay interaction";
                break;
            case InteractionStatus::NoEffect:
                std::cout << "no effect (not pickable or interaction not currently available)";
                break;
            case InteractionStatus::InventoryFull:
                std::cout << "inventory full; no new slot allocated";
                break;
            case InteractionStatus::InactiveObject:
                std::cout << "skipped: object health must be positive";
                break;
            case InteractionStatus::UnimplementedHandler:
                std::cout << "collision-specific interaction NOT IMPLEMENTED";
                break;
            }
            std::cout << "\nGold: " << context.gold << ", arrow poison: " << context.arrowPoison
                << ", map: " << (context.hasMap ? "yes" : "no")
                << ", world object health: " << object.health
                << ", inventory result slot: " << interaction.inventorySlot << '\n';
            for (std::size_t i = 0; i < context.inventory.size(); ++i)
            {
                const auto& stored = context.inventory[i];
                if (stored.health != 0)
                {
                    std::cout << "Inventory slot " << i + 1 << ": collision type "
                        << static_cast<unsigned int>(stored.collisionType) << ", value "
                        << stored.itemValue << ", health " << stored.health << '\n';
                }
            }
            for (std::size_t i = 0; i < context.weaponAmounts.size(); ++i)
            {
                if (context.weaponInventorySlots[i] != 0 || context.weaponAmounts[i] != 0)
                {
                    std::cout << "Weapon/resource " << i << ": amount " << context.weaponAmounts[i]
                        << ", inventory slot " << context.weaponInventorySlots[i] << '\n';
                }
            }
            std::cout << "Interaction UI/audio, inventory use, and unsupported interactions are NOT IMPLEMENTED.\n";
        }
        else
        {
            const auto result = DispatchObjectCase(object, context);
            const auto descriptor = FindBehavior(result.behavior);
            const char* behaviorName = descriptor ? descriptor->name : "UnmappedWord";
            std::cout << "Behavior word: " << static_cast<std::uint16_t>(object.behavior) << " (" << behaviorName
                << "), health: " << object.health << ", ticks: " << context.tickCount << '\n'
                << "Raw position: " << object.posX << ", " << object.posY
                << ", " << object.posZ << " (DOS integer units)\n";
            if (object.behavior == BehaviorType::IdleAnimation)
            {
                std::cout << "Animation blend phase: " << object.blendWeightOrSpriteOpacity << '\n';
            }
            if (object.behavior == BehaviorType::SmokeFading)
            {
                std::cout << "Smoke fraction: " << object.itemValue << ", vertical velocity: " << object.velocityY
                    << ", opacity: " << object.blendWeightOrSpriteOpacity << ", scale: " << object.spriteScale << '\n';
            }
            if (object.behavior == BehaviorType::Switch1 || object.behavior == BehaviorType::SwitchGrouped)
            {
                std::cout << "Switch phase: " << object.blendWeightOrSpriteOpacity << ", rate word: "
                    << object.field_26 << ", timer/return-speed word: " << object.field_24 << '\n';
                if (object.behavior == BehaviorType::SwitchGrouped &&
                    result.status == DispatchStatus::ExecutedCase)
                {
                    std::cout << "Switch group: " << object.groupIndexOrLock << ", mask: "
                        << static_cast<unsigned int>(context.switchGroupMasks[object.groupIndexOrLock]) << '\n';
                }
                std::cout << "Use --interact to request grouped-switch activation; ordinary switch targets "
                    "and door interactions are NOT IMPLEMENTED.\n";
            }
            if (object.behavior == BehaviorType::WaterDrop)
            {
                std::cout << "Drop velocity Y: " << object.velocityY << ", scale high byte: "
                    << (object.spriteScale >> 8) << ", wait timer: " << object.field_24
                    << ", splash updates remaining: " << object.field_26
                    << ", animation frame: " << static_cast<unsigned int>(object.animFrame)
                    << "\nWaterDrop sound is routed through AudioHandler; "
                    << (context.audioHandler ? "console logging only, no audio device.\n" :
                        "audio unavailable (no --sound-table supplied).\n");
            }
            switch (result.status)
            {
            case DispatchStatus::ExecutedCase:
                std::cout << "Executed Function_" << behaviorName
                    << ": recovered case only; shared pre/postprocessing is NOT IMPLEMENTED";
                break;
            case DispatchStatus::UnimplementedHandler:
                std::cout << "Selected Function_" << behaviorName << ": NOT IMPLEMENTED";
                break;
            case DispatchStatus::NoSpecificCase:
                std::cout << "No specialized case; common postprocessing is NOT IMPLEMENTED";
                break;
            case DispatchStatus::InactiveObject:
                std::cout << "Skipped: original loop requires health > 0";
                break;
            case DispatchStatus::DispatchDisabled:
                std::cout << "Skipped behavior cascade: modeled DS:04CD flag is clear";
                break;
            }
            std::cout << " (module 0000:" << std::hex << std::setw(4) << std::setfill('0')
                << result.moduleOffset << std::dec << ")\nFull gameplay simulation is NOT IMPLEMENTED.\n";
        }
        if (!collisionMapOutput.empty())
        {
            std::ofstream file(collisionMapOutput, std::ios::binary);
            if (!file) { throw std::runtime_error("Cannot open collision-map output."); }
            file.write(reinterpret_cast<const char*>(level.collisionMap.get()),
                static_cast<std::streamsize>(sizeof(CollisionMap)));
            file.close();
            if (!file) { throw std::runtime_error("Cannot write collision-map output."); }
        }
        return 2;
    }
}

int main(int argc, char** argv)
{
    try
    {
        return Run(argc, argv);
    }
    catch (const std::invalid_argument& error)
    {
        std::cerr << "Invalid input: " << error.what() << '\n';
        return 1;
    }
    catch (const std::runtime_error& error)
    {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
    catch (const std::out_of_range& error)
    {
        std::cerr << "Memory error: " << error.what() << '\n';
        return 1;
    }
}
