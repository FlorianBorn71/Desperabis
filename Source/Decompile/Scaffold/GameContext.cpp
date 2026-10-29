#include "GameContext.h"
#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>

namespace Desperabis
{
    namespace
    {
        std::int16_t AbsoluteWord(std::int16_t value)
        {
            return value < 0 ? WrapSignedWord(-static_cast<std::int32_t>(value)) : value;
        }

        struct HeightPosition
        {
            std::size_t column;
            std::size_t row;
            std::uint16_t fractionX;
            std::uint16_t fractionZ;
        };

        HeightPosition GetHeightPosition(std::int16_t x, std::int16_t z)
        {
            const int column = x / 100;
            const int zTile = z / 100;
            if (column < -128 || column > 127 || zTile < -128 || zTile > 127)
            {
                throw std::overflow_error("Height lookup would overflow the DOS signed-byte division.");
            }
            const int row = 41 - zTile;
            if (column < 1 || column > static_cast<int>(Level::Width) ||
                row < 1 || row > static_cast<int>(Level::Height))
            {
                throw std::out_of_range("Height lookup is outside the 50x40 level.");
            }
            // The original arrays are one-based; their instruction displacements are biased by 51.
            return { static_cast<std::size_t>(column - 1), static_cast<std::size_t>(row - 1),
                static_cast<std::uint16_t>((x % 100) * 661),
                static_cast<std::uint16_t>((z % 100) * 661) };
        }

        void CheckHeightCorners(const HeightPosition& position)
        {
            if (position.column + 1 >= Level::Width || position.row + 1 >= Level::Height)
            {
                throw std::out_of_range("Height interpolation requires four samples inside the level.");
            }
        }

        std::uint16_t Interpolate(std::uint16_t first, std::uint16_t second, std::uint16_t fraction)
        {
            const auto sum = static_cast<std::uint32_t>(first) * (65535u - fraction) +
                static_cast<std::uint32_t>(second) * fraction;
            return static_cast<std::uint16_t>(sum >> 16);
        }

        std::uint16_t InterpolateHeight(const std::array<std::int16_t, 4>& corners,
            const HeightPosition& position)
        {
            const auto upper = Interpolate(static_cast<std::uint16_t>(corners[0]),
                static_cast<std::uint16_t>(corners[1]), position.fractionX);
            const auto lower = Interpolate(static_cast<std::uint16_t>(corners[2]),
                static_cast<std::uint16_t>(corners[3]), position.fractionX);
            return Interpolate(lower, upper, position.fractionZ);
        }

        std::array<std::int16_t, 4> GetCorners(
            const std::uint8_t (&plane)[Level::Height][Level::Width], const HeightPosition& position)
        {
            CheckHeightCorners(position);
            const auto row = position.row;
            const auto column = position.column;
            return { plane[row][column], plane[row][column + 1],
                plane[row + 1][column], plane[row + 1][column + 1] };
        }

        std::int16_t RepeatFloor(std::int16_t corner, std::int16_t queryHeight, std::int16_t step)
        {
            corner = WrapSignedWord(corner % step - step);
            const auto threshold = WrapSignedWord(queryHeight - 28);
            for (unsigned int iterations = 0; threshold > corner; ++iterations)
            {
                if (iterations == 65536u)
                {
                    throw std::runtime_error("Repeated-floor lookup would loop forever in the DOS game.");
                }
                corner = WrapSignedWord(corner + step);
            }
            return std::clamp<std::int16_t>(corner, 0, 255);
        }

        std::int16_t GetTerrainHeight(const Level& terrain, std::int16_t repeatHeight,
            std::int16_t x, std::int16_t y, std::int16_t z)
        {
            const auto position = GetHeightPosition(x, z);
            const auto surface = terrain.surfaceFlags[position.row][position.column] & 31;
            auto corners = GetCorners(terrain.floorHeights, position);
            const auto halfY = y >= 0 ? y / 2 : (static_cast<int>(y) - 1) / 2;
            const auto queryHeight = WrapSignedWord(-halfY);
            constexpr int stackedFloorSurface = 28;
            if (surface == stackedFloorSurface)
            {
                const auto room = GetCorners(terrain.roomHeights, position);
                for (std::size_t i = 0; i < corners.size(); ++i)
                {
                    corners[i] = WrapSignedWord(corners[i] - room[i]);
                }
                if (WrapSignedWord(*std::max_element(corners.begin(), corners.end()) - 5) < queryHeight)
                {
                    for (std::size_t i = 0; i < corners.size(); ++i)
                    {
                        corners[i] = WrapSignedWord(corners[i] + room[i]);
                    }
                }
            }
            if (repeatHeight > 0 && surface != 0)
            {
                for (auto& corner : corners) { corner = RepeatFloor(corner, queryHeight, repeatHeight); }
            }
            return WrapSignedWord(-2 * static_cast<std::int32_t>(InterpolateHeight(corners, position)));
        }

        bool AxisOverlaps(std::int16_t first, std::int16_t second,
            std::int16_t firstRadius, std::int16_t secondRadius)
        {
            const auto difference = WrapSignedWord(first - second);
            const auto distance = difference < 0 ? WrapSignedWord(-difference) : difference;
            return distance < WrapSignedWord(firstRadius + secondRadius);
        }

        bool ObjectsOverlap(const RawGameObject& source, const RawGameObject& candidate)
        {
            return (source.collisionMask & candidate.collisionMask) != 0 &&
                AxisOverlaps(source.posX, candidate.posX, source.collisionRadiusXZ, candidate.collisionRadiusXZ) &&
                AxisOverlaps(source.posY, candidate.posY, source.collisionRadiusY, candidate.collisionRadiusY) &&
                AxisOverlaps(source.posZ, candidate.posZ, source.collisionRadiusXZ, candidate.collisionRadiusXZ);
        }
    }

    GameContext::GameContext() = default;
    GameContext::~GameContext() = default;
    GameContext::GameContext(GameContext&&) noexcept = default;
    GameContext& GameContext::operator=(GameContext&&) noexcept = default;

    void GameContext::OnObjectCreated(int) {}
    void GameContext::OnObjectDestroyed(int) {}
    void GameContext::OnPlayerHealthChanged(std::int32_t, std::int32_t) {}

    int GameContext::FindWorldObjectSlot(const RawGameObject& object) const
    {
        for (std::size_t slot = 1; slot < worldObjects.size(); ++slot)
        {
            if (&worldObjects[slot] == &object) { return static_cast<int>(slot); }
        }
        return -1;
    }

    void GameContext::NotifyObjectCreated(const RawGameObject& object)
    {
        const auto slot = FindWorldObjectSlot(object);
        if (slot != -1 && object.health > 0) { OnObjectCreated(slot); }
    }

    void GameContext::SetObjectHealth(RawGameObject& object, std::int16_t health)
    {
        const bool active = object.health > 0;
        if (active == (health > 0))
        {
            object.health = health;
            return;
        }
        const auto slot = FindWorldObjectSlot(object);
        if (slot != -1 && active && health <= 0) { OnObjectDestroyed(slot); }
        object.health = health;
        if (slot != -1 && !active && health > 0) { OnObjectCreated(slot); }
    }

    void GameContext::ReplaceWorldObject(int slot, RawGameObject object)
    {
        if (slot < 1 || slot >= static_cast<int>(worldObjects.size()))
        {
            throw std::out_of_range("World-object lifecycle slots must be in 1..165.");
        }
        auto& current = worldObjects[static_cast<std::size_t>(slot)];
        if (current.health > 0) { OnObjectDestroyed(slot); }
        current = object;
        NotifyObjectCreated(current);
    }

    RawGameObject DecodeGameObject(const std::uint8_t* bytes, std::size_t size)
    {
        if (bytes == nullptr || size != sizeof(RawGameObject))
        {
            throw std::invalid_argument("An object payload must contain exactly 54 bytes.");
        }
        RawGameObject object;
        std::memcpy(&object, bytes, sizeof(object));
        return object;
    }

    void EncodeGameObject(const RawGameObject& object, std::uint8_t* bytes, std::size_t size)
    {
        if (bytes == nullptr || size != sizeof(RawGameObject))
        {
            throw std::invalid_argument("An object payload must contain exactly 54 bytes.");
        }
        std::memcpy(bytes, &object, sizeof(object));
    }

    LevelLoadRequest GameContext::StartLevel(std::int16_t levelNumber)
    {
        if (levelNumber <= 0) { throw std::invalid_argument("Level numbers must be positive."); }
        if (pendingLevelLoad) { throw std::logic_error("A level load is already in progress."); }
        if (nextLoadRequestId == std::numeric_limits<std::uint64_t>::max())
        {
            throw std::overflow_error("Level load request IDs are exhausted.");
        }
        const LevelLoadRequest request{ ++nextLoadRequestId, levelNumber };
        pendingLevelLoad = request;
        levelLoadError.clear();
        runtimeState = GameRuntimeState::Loading;
        try
        {
            LoadLevel(request);
        }
        catch (const std::exception& error)
        {
            if (pendingLevelLoad && pendingLevelLoad->id == request.id)
            {
                FailLevelLoad(request.id, error.what());
            }
            throw;
        }
        return request;
    }

    RawGameObject GameContext::InitializeLevelPlayer(const PlayerCamera& spawn)
    {
        RawGameObject player;
        player.health = 1000;
        // Save-derived normal shape, not yet a traced EXE initialization routine; see BehaviorRecovery.md.
        player.collisionMask = 3;
        player.collisionRadiusXZ = 15;
        player.collisionRadiusY = 23;
        player.posX = spawn.position[0];
        player.posY = WrapSignedWord(spawn.position[1] - player.collisionRadiusY - 2);
        player.posZ = spawn.position[2];
        player.velocityX = spawn.direction[0];
        player.velocityY = spawn.direction[1];
        player.velocityZ = spawn.direction[2];
        return player;
    }

    void GameContext::CompleteLevelLoad(std::uint64_t requestId, LevelLoadData data)
    {
        if (!pendingLevelLoad || pendingLevelLoad->id != requestId)
        {
            throw std::logic_error("Level completion does not match the pending request.");
        }
        if (!data.level || !data.level->collisionMap)
        {
            throw std::invalid_argument("A loaded level requires owned geometry and a collision map.");
        }
        if (data.playerSpawn) { data.objects[1] = InitializeLevelPlayer(*data.playerSpawn); }
        LightingCache newLighting;
        newLighting.Reset(data.objects[1].posX, data.objects[1].posZ);

        if (audioHandler && data.ambientSound)
        {
            audioHandler->StopAmbientPlayback();
            if (soundEnabled)
            {
                CreateAmbientLoop(*data.ambientSound);
                audioHandler->StartAmbientPlayback();
            }
        }

        for (std::size_t slot = 1; slot < worldObjects.size(); ++slot)
        {
            SetObjectHealth(worldObjects[slot], 0);
        }
        ownedLevel = std::move(data.level);
        level = ownedLevel.get();
        collisionMap = ownedLevel->collisionMap.get();
        worldObjects = data.objects;
        playerForwardSpeed = 0;
        playerSidewaysSpeed = 0;
        playerTurnSpeed = 0;
        playerVerticalSpeed = 0;
        playerWalkPhase = 0;
        playerSurfaceSpeedLimit = 5;
        playerMapCollisionsEnabled = true;
        playerLadderMovement = false;
        playerRenderTable = 0;
        field_D503 = false;
        playerLookHeight = 110;
        playerTargetLookHeight = 110;
        playerBodyHeight = 45;
        playerPostureDelta = 0;
        playerGrounded = false;
        playerUnderwater = false;
        // Host policy: seed normal heading from its supplied view vector; zero means face +Z.
        // TODO: Load native DS:D4D0/DC4C and the working camera position from recovered level/save initialization.
        playerHeading = worldObjects[1].velocityX == 0 && worldObjects[1].velocityZ == 0 ? 0 :
            GetVectorAngle(worldObjects[1].velocityX, WrapSignedWord(-static_cast<int>(worldObjects[1].velocityZ)));
        playerTargetHeading = playerHeading;
        floorRepeatHeight = data.floorRepeatHeight;
        floorSurfaceValue = -1;
        playerCollisionSlot = 0;
        std::fill(std::begin(switchGroupMasks), std::end(switchGroupMasks), std::uint8_t{ 0xff });
        PromptUserMessage({}, 0);
        soundListenerX = worldObjects[1].posX;
        soundListenerZ = worldObjects[1].posZ;
        lightingCache = newLighting;
        field_DC34 = 0;
        loadedLevelNumber = pendingLevelLoad->levelNumber;
        pendingLevelLoad.reset();
        levelLoadError.clear();
        hasActiveGame = true;
        runtimeState = GameRuntimeState::Ready;
        SetMenuPage(MenuPage::Gameplay);
        applicationTickAccumulator = 0;
        for (std::size_t slot = 1; slot < worldObjects.size(); ++slot)
        {
            NotifyObjectCreated(worldObjects[slot]);
        }
    }

    void GameContext::FailLevelLoad(std::uint64_t requestId, std::string_view error)
    {
        if (!pendingLevelLoad || pendingLevelLoad->id != requestId)
        {
            throw std::logic_error("Level failure does not match the pending request.");
        }
        if (error.empty()) { throw std::invalid_argument("Level loading failures require an error message."); }
        levelLoadError = error;
        pendingLevelLoad.reset();
        runtimeState = GameRuntimeState::LoadFailed;
    }

    GameRuntimeState GameContext::GetRuntimeState() const { return runtimeState; }
    std::optional<LevelLoadRequest> GameContext::GetPendingLevelLoad() const { return pendingLevelLoad; }
    std::optional<std::int16_t> GameContext::GetLoadedLevelNumber() const { return loadedLevelNumber; }
    const std::string& GameContext::GetLevelLoadError() const { return levelLoadError; }
    void GameContext::SetPaused(bool value)
    {
        paused = value;
        if (value) { applicationTickAccumulator = 0; }
    }
    bool GameContext::IsPaused() const { return paused; }
    const std::array<RawGameObject, 166>& GameContext::GetWorldObjects() const { return worldObjects; }
    const Level* GameContext::GetLevel() const { return level; }
    const LightingCache& GameContext::GetLightingCache() const { return lightingCache; }

    PlayerCamera GameContext::GetPlayerCamera() const
    {
        const auto& player = worldObjects[1];
        // E1F4..E210: object-center Y = working/view Y - vertical radius - 2.
        const auto eyeY = loadedLevelNumber ?
            WrapSignedWord(player.posY + player.collisionRadiusY + 2) : player.posY;
        return { { player.posX, eyeY, player.posZ },
            { player.velocityX, player.velocityY, player.velocityZ }, playerLookHeight };
    }

    void GameContext::RequestPlayerJump()
    {
        if (worldObjects[1].generalPurpose3 != 0) { return; }
        if (playerBodyHeight == 45 && playerGrounded) { playerVerticalSpeed = 7; }
        else { playerPostureDelta = 6; }
    }

    void GameContext::RequestPlayerCrouch()
    {
        playerPostureDelta = -6;
        if (playerVerticalSpeed > 0) { playerVerticalSpeed = 0; }
    }

    BasicPlayerMovementResult GameContext::UpdateBasicPlayerMovement(InputHandler& input)
    {
        BasicPlayerMovementResult result;
        auto& player = worldObjects[1];
        if (frameTickCount == 0 || player.health <= 0 || playerLife <= 0) { return result; }

        // Normal keyboard mode, C603..C679 and C74E..C774; sample once per host tick, not per substep.
        if (input.IsKeyPressed(InputKey::TurnLeftKey)) { playerTurnSpeed = WrapSignedWord(playerTurnSpeed - 7); }
        if (input.IsKeyPressed(InputKey::TurnRightKey)) { playerTurnSpeed = WrapSignedWord(playerTurnSpeed + 7); }
        if (input.IsKeyPressed(InputKey::ForwardKey)) { playerForwardSpeed = WrapSignedWord(playerForwardSpeed + 4); }
        if (input.IsKeyPressed(InputKey::BackwardKey)) { playerForwardSpeed = WrapSignedWord(playerForwardSpeed - 4); }
        if (input.IsKeyPressed(InputKey::StrafeRightKey)) { playerSidewaysSpeed = WrapSignedWord(playerSidewaysSpeed + 4); }
        if (input.IsKeyPressed(InputKey::StrafeLeftKey)) { playerSidewaysSpeed = WrapSignedWord(playerSidewaysSpeed - 4); }
        if (input.IsKeyPressed(InputKey::JumpKey)) { RequestPlayerJump(); }
        if (input.IsKeyPressed(InputKey::CrouchKey)) { RequestPlayerCrouch(); }
        if (input.IsKeyPressed(InputKey::LookDownKey) && playerTargetLookHeight > 50)
        {
            playerTargetLookHeight = WrapSignedWord(playerTargetLookHeight - 10);
        }
        if (input.IsKeyPressed(InputKey::LookUpKey) && playerTargetLookHeight < 170)
        {
            playerTargetLookHeight = WrapSignedWord(playerTargetLookHeight + 10);
        }
        if (input.IsKeyPressed(InputKey::CenterLookKey)) { playerTargetLookHeight = 110; }
        playerForwardSpeed = WrapSignedWord(playerForwardSpeed - SignWord(playerForwardSpeed));
        playerSidewaysSpeed = WrapSignedWord(playerSidewaysSpeed - SignWord(playerSidewaysSpeed));
        playerTurnSpeed = WrapSignedWord(playerTurnSpeed - SignWord(playerTurnSpeed));

        // TODO: Recover mouse controls and vehicle/attached-object movement.
        for (int tick = 0; tick < frameTickCount; ++tick)
        {
            int limit = playerBodyHeight < 45 && behaviorDispatchEnabled ? 2 : playerSurfaceSpeedLimit;
            if (playerUnderwater) { limit = 3; }
            if (playerForwardSpeed != 0 && playerSidewaysSpeed != 0) { limit = 2 * limit / 3; }
            playerForwardSpeed = static_cast<std::int16_t>(std::clamp<int>(playerForwardSpeed, -limit, limit));
            playerSidewaysSpeed = static_cast<std::int16_t>(std::clamp<int>(playerSidewaysSpeed, -limit, limit));
            playerTurnSpeed = static_cast<std::int16_t>(std::clamp<int>(playerTurnSpeed, -6, 6));
            playerTargetHeading = static_cast<std::int16_t>((playerTargetHeading + playerTurnSpeed) & 511);
            auto target = playerTargetHeading;
            NormalizeAnglePair(target, playerHeading);
            playerHeading = static_cast<std::int16_t>(
                (playerHeading + GetStepToward(playerHeading, target, 6)) & 511);

            std::int16_t directionX;
            std::int16_t negativeDirectionZ;
            GetDirectionDisplacement(directionX, negativeDirectionZ, playerHeading, 100);
            const auto directionZ = WrapSignedWord(-static_cast<int>(negativeDirectionZ));
            const auto signedBob = WrapSignedWord(static_cast<std::int32_t>(
                static_cast<std::uint32_t>(6 * Sine(playerWalkPhase)) >> 16));
            const auto bob = std::abs(static_cast<int>(signedBob));
            auto eyeY = WrapSignedWord(player.posY + player.collisionRadiusY + 2);
            bool bridgeSupported = false;
            bool terrainSlopeEnabled = true;
            std::uint8_t footstepEffect = 0;
            if (field_D500)
            {
                int contacts = 0;
                for (std::size_t slot = 2; slot <= 164 && contacts < 50; ++slot)
                {
                    auto& candidate = worldObjects[slot];
                    if (candidate.health <= 999 || (player.collisionMask & candidate.collisionMask) == 0) { continue; }
                    const auto radiusXZ = ArithmeticShiftRight(WrapSignedWord(
                        3 * WrapSignedWord(player.collisionRadiusXZ + candidate.collisionRadiusXZ)), 1);
                    if (!AxisOverlaps(player.posX, candidate.posX, radiusXZ, 0) ||
                        !AxisOverlaps(player.posZ, candidate.posZ, radiusXZ, 0) ||
                        !AxisOverlaps(player.posY, candidate.posY,
                            player.collisionRadiusY, candidate.collisionRadiusY)) { continue; }
                    ++contacts;
                    if (candidate.collisionType == CollisionType::ClimbableLadder && candidate.health == 1000)
                    {
                        terrainSlopeEnabled = false;
                    }
                    if (candidate.collisionType != CollisionType::WalkablePlanks ||
                        candidate.blendWeightOrSpriteOpacity != 0 || eyeY <= candidate.posY) { continue; }
                    eyeY = WrapSignedWord(eyeY + GetStepToward(eyeY,
                        WrapSignedWord(candidate.posY + playerBodyHeight + bob), 4));
                    playerVerticalSpeed = 1;
                    candidate.itemValue = 7;
                    footstepEffect = candidate.soundEffectId;
                    bridgeSupported = true;
                    terrainSlopeEnabled = false;
                }
            }
            playerBodyHeight = WrapSignedWord(playerBodyHeight + GetStepToward(playerBodyHeight,
                WrapSignedWord(playerBodyHeight + playerPostureDelta), 3));
            if (player.itemValue == 0 && field_D500)
            {
                if (playerBodyHeight < 28) { playerBodyHeight = 28; playerPostureDelta = 0; }
                if (playerBodyHeight > 45) { playerBodyHeight = 45; playerPostureDelta = 0; }
            }
            // D6FA..D745 samples ahead using arithmetic shifts, including negative directions.
            const auto shift3 = [](std::int16_t value)
            {
                return value < 0 ? -((-static_cast<int>(value) + 7) / 8) : value / 8;
            };
            const auto room = GetRoomHeight(WrapSignedWord(player.posX + shift3(directionX)),
                WrapSignedWord(player.posZ + shift3(directionZ)));
            // Keep physical speed separate from the host's view-vector Y.
            playerVerticalSpeed = playerLadderMovement ?
                SignWord(WrapSignedWord(playerVerticalSpeed + playerPostureDelta)) :
                WrapSignedWord(playerVerticalSpeed - 1);
            if (room < 96 && playerVerticalSpeed > 0) { playerVerticalSpeed = 0; }
            if (room < 65)
            {
                playerBodyHeight = WrapSignedWord(
                    playerBodyHeight + GetStepToward(playerBodyHeight, 28, 8));
            }
            if (player.blendWeightOrSpriteOpacity > 10) { playerForwardSpeed = SignWord(playerForwardSpeed); }
            if (player.blendWeightOrSpriteOpacity > 0) { --player.blendWeightOrSpriteOpacity; }
            const auto deltaX = WrapSignedWord(directionX * playerForwardSpeed +
                directionZ * playerSidewaysSpeed) / 100;
            const auto deltaZ = WrapSignedWord(directionZ * playerForwardSpeed -
                directionX * playerSidewaysSpeed) / 100;
            auto x = WrapSignedWord(player.posX + deltaX);
            auto z = WrapSignedWord(player.posZ + deltaZ);
            eyeY = WrapSignedWord(eyeY + playerVerticalSpeed);
            const auto walkingSpeed = std::abs(static_cast<int>(playerForwardSpeed)) +
                std::abs(static_cast<int>(playerSidewaysSpeed));
            if (!playerUnderwater && walkingSpeed > 1)
            {
                playerWalkPhase = WrapSignedWord(playerWalkPhase + 3 * walkingSpeed);
            }
            bool footstep = playerWalkPhase > 256;
            if (footstep) { playerWalkPhase = WrapSignedWord(playerWalkPhase - 256); }
            const auto aheadFloor = GetTerrainFloorHeight(
                WrapSignedWord(x + directionX), eyeY, WrapSignedWord(z + directionZ));
            std::int16_t slopeX = 0;
            std::int16_t slopeZ = 0;
            const auto currentFloor = QueryTerrainSupport(x, eyeY, z, slopeX, slopeZ);
            const auto slopeDifference = terrainSlopeEnabled ? WrapSignedWord(currentFloor - aheadFloor) : 0;
            if (field_D500 && terrainSlopeEnabled)
            {
                x = WrapSignedWord(x + slopeX);
                z = WrapSignedWord(z + slopeZ);
            }
            playerLookHeight = WrapSignedWord(playerLookHeight + GetStepToward(playerLookHeight,
                WrapSignedWord(playerTargetLookHeight - slopeDifference), 5));
            playerLookHeight = WrapSignedWord(static_cast<std::uint16_t>(playerLookHeight) & 0xfffeu);

            const bool collideWithMap = !DEBUG_noPlayerCollision &&
                objectCollisionsEnabled && field_D500 && playerMapCollisionsEnabled;
            bool resolved = !collideWithMap;
            if (collideWithMap)
            {
                // DD97..DE08 uses step 5, mask 18h and repeated correction. Bound retries for the host.
                // The native loop is unbounded; reject unresolved/symmetric blocking instead of hanging the host.
                for (int attempt = 0; attempt < 16; ++attempt)
                {
                    std::int16_t correctionX = 0;
                    std::int16_t correctionZ = 0;
                    std::uint16_t center = 0;
                    QueryCollisionMapNeighborhood(x, z, 5, correctionX, correctionZ, center, 0x18);
                    if (correctionX == 0 && correctionZ == 0)
                    {
                        resolved = center == 0;
                        break;
                    }
                    x = WrapSignedWord(x + correctionX);
                    z = WrapSignedWord(z + correctionZ);
                    playerForwardSpeed = WrapSignedWord(playerForwardSpeed - SignWord(playerForwardSpeed));
                }
            }
            if (resolved)
            {
                player.posX = x;
                player.posZ = z;
            }
            else
            {
                ++result.blockedTicks;
            }
            const auto supportY = WrapSignedWord(
                GetTerrainFloorHeight(player.posX, eyeY, player.posZ) + playerBodyHeight + bob);
            playerGrounded = bridgeSupported || playerLadderMovement;
            if (supportY > eyeY)
            {
                if (playerVerticalSpeed < -4)
                {
                    footstep = true;
                    playerWalkPhase = 0;
                }
                eyeY = supportY;
                playerVerticalSpeed = 0;
                playerGrounded = true;
            }
            // DE45..DED4: the map byte controls contact; FELD surfaceFlags supplies depth.
            const auto surface = field_D500 ? GetCollisionMapPixelAtWorldXZ(player.posX, player.posZ) : 0;
            static constexpr std::array<std::uint8_t, 8> terrainFootsteps = { 1,10,26,10,26,97,10,105 };
            if (footstepEffect == 0) { footstepEffect = terrainFootsteps[surface & 7]; }
            playerSurfaceSpeedLimit = 5;
            if ((surface & 7) == 4)
            {
                playerSurfaceSpeedLimit = 2;
                if (Random(5) == 0) { footstepEffect = 52; }
            }
            else if ((surface & 7) == 6) { playerSurfaceSpeedLimit = 3; }
            if (surface != 1 && player.generalPurpose3 == 1)
            {
                player.generalPurpose3 = 0;
                RequestPlayerJump();
            }
            playerUnderwater = false;
            if (((surface & 7) == 1 || (surface & 7) == 3) && WrapSignedWord(eyeY - supportY) < 5)
            {
                RequestPlayerCrouch();
                player.generalPurpose3 = 1;
                const auto position = GetHeightPosition(player.posX, player.posZ);
                const auto depth = GetHeightSurfaceDepth(level->surfaceFlags[position.row][position.column]);
                playerUnderwater = WrapSignedWord(eyeY + depth) < 0;
            }
            if (playerUnderwater)
            {
                footstep = false;
                playerWalkPhase = WrapSignedWord(playerWalkPhase + Random(6) + 2);
            }
            if (footstep)
            {
                const auto volume = GetSoundVolume(WrapSignedWord(430 + Random(150)));
                PlaySoundEffect(static_cast<SoundEffect>(footstepEffect), volume, -6, 2);
            }
            const auto halfHeight = playerBodyHeight < 0 ?
                -((-static_cast<int>(playerBodyHeight) + 1) / 2) : playerBodyHeight / 2;
            player.collisionRadiusY = WrapSignedWord(halfHeight + 1);
            player.posY = WrapSignedWord(eyeY - player.collisionRadiusY - 2);
            player.velocityX = directionX;
            player.velocityZ = directionZ;
            ++result.simulatedTicks;
        }
        // TODO: Port remaining damage/contact modes; selected interactions run in Tick after movement.
        return result;
    }

    GameTickResult GameContext::Tick(GameTickTiming timing, InputHandler& input)
    {
        if (timing.behaviorTicks < 0 || timing.frameTicks < 0)
        {
            throw std::invalid_argument("Game ticks require nonnegative behavior and frame counts.");
        }
        switch (runtimeState)
        {
        case GameRuntimeState::NoLevel: return { GameTickStatus::NoLevel, {}, {} };
        case GameRuntimeState::Loading: return { GameTickStatus::Loading, {}, {} };
        case GameRuntimeState::LoadFailed: return { GameTickStatus::LoadFailed, {}, {} };
        case GameRuntimeState::Ready: break;
        }
        if (paused || menuPage != MenuPage::Gameplay)
        {
            return { GameTickStatus::Paused, {}, {} };
        }
        if (!ownedLevel || level != ownedLevel.get() || collisionMap != ownedLevel->collisionMap.get())
        {
            throw std::logic_error("Host-driven gameplay requires the installed owned level.");
        }
        tickCount = timing.behaviorTicks;
        frameTickCount = timing.frameTicks;
        std::optional<BasicPlayerMovementResult> movement;
        if (basicPlayerMovementEnabled) { movement = UpdateBasicPlayerMovement(input); }
        soundListenerX = worldObjects[1].posX;
        soundListenerZ = worldObjects[1].posZ;
        lightingCache.Reset(soundListenerX, soundListenerZ, lightingCache.activeRadius);
        field_DC34 = 0;
        const auto interaction = UpdatePlayerInteraction(*this, input);
        soundListenerX = worldObjects[1].posX;
        soundListenerZ = worldObjects[1].posZ;
        UpdateWeaponSelection();
        const auto results = UpdateObjects(*this);
        PlayerWeaponResult weapon;
        weapon.animation = UpdateWeaponAnimation(frameTickCount);
        if (input.IsKeyPressed(InputKey::SelectDaggerKey)) { SelectWeapon(WeaponType::Dagger); }
        if (input.IsKeyPressed(InputKey::SelectSwordKey)) { SelectWeapon(WeaponType::Sword); }
        if (input.IsKeyPressed(InputKey::SelectCrossbowKey)) { SelectWeapon(WeaponType::Crossbow); }
        if (input.IsKeyPressed(InputKey::SelectFireballKey)) { SelectWeapon(WeaponType::Fireball); }
        if (input.IsKeyPressed(InputKey::SelectStoneKey)) { SelectWeapon(WeaponType::Stone); }
        if (input.IsKeyPressed(InputKey::SelectPlaceableKey)) { SelectWeapon(WeaponType::TrapOrPole); }
        if (input.IsKeyPressed(InputKey::SelectElectricityKey)) { SelectWeapon(WeaponType::Electricity); }
        if (input.IsKeyPressed(InputKey::SelectShovelKey)) { SelectWeapon(WeaponType::Showel); }
        if (input.IsKeyPressed(InputKey::SelectScytheKey)) { SelectWeapon(WeaponType::Scythe); }
        if (input.IsKeyPressed(InputKey::SelectLentilsKey)) { SelectWeapon(WeaponType::Lentils); }
        if (input.IsKeyPressed(InputKey::AttackKey)) { weapon.attack = AttackWithSelectedWeapon(); }
        return { GameTickStatus::Advanced, interaction, results, movement, weapon };
    }

    std::vector<std::uint8_t> GameContext::LoadFile(std::string_view filename)
    {
        throw std::logic_error("The host must implement LoadFile to read " + std::string(filename));
    }

    void GameContext::LoadLevelGeometry(std::int16_t levelNumber, LevelLoadData& data)
    {
        if (levelNumber <= 0) { throw std::invalid_argument("Geometry loading requires a positive level number."); }
        const auto filename = "LEVEL\\FELD" + std::to_string(levelNumber) + ".256";
        const auto payload = LoadFile(filename);
        auto loadedLevel = std::make_unique<Level>();
        loadedLevel->LoadGeometry(payload.data(), payload.size());
        loadedLevel->BuildCollisionMap();
        const auto info = LoadFile("LEVEL\\LEVELS.INF");
        constexpr std::size_t recordSize = 124;
        const auto recordOffset = static_cast<std::size_t>(levelNumber) * recordSize;
        if (info.size() % recordSize != 0 || info.size() < recordOffset + recordSize)
        {
            throw std::invalid_argument("LEVEL\\LEVELS.INF: expected complete 124-byte records including the requested level.");
        }
        // 0840..086B initializes all four depth words; 1E48:1AF9 reads the paired value bytes.
        for (std::size_t slot = 0; slot < loadedLevel->heightSurfaces.size(); ++slot)
        {
            const auto offset = recordOffset + 108 + 2 * slot;
            loadedLevel->heightSurfaces[slot] = Level::HeightSurface{
                static_cast<std::int16_t>(info[offset + 1] * 10), info[offset] };
        }
        const auto repeatHeight = static_cast<std::int16_t>(
            info[recordOffset + 116] > 0 ? info[recordOffset + 117] * 5 : 0);
        data.level = std::move(loadedLevel);
        data.floorRepeatHeight = repeatHeight;
        data.ambientSound = info[recordOffset + 100];
    }

    void GameContext::LoadLevelObjects(std::int16_t levelNumber, LevelLoadData& data)
    {
        if (levelNumber <= 0) { throw std::invalid_argument("Object loading requires a positive level number."); }
        const auto filename = "LEVEL\\OBJPOS" + std::to_string(levelNumber) + ".DAT";
        const auto payload = LoadFile(filename);
        constexpr std::size_t firstSlot = 2;
        constexpr std::size_t lastSlot = 164;
        constexpr auto recordSize = sizeof(RawGameObject);
        if (payload.size() % recordSize != 0 ||
            payload.size() / recordSize > lastSlot - firstSlot + 1)
        {
            throw std::invalid_argument(filename + ": expected at most 163 complete 54-byte object records.");
        }
        // Retain the current OBJPOS record-i -> native-slot-i+2 mapping; native initialization is not fully traced.
        for (std::size_t slot = firstSlot; slot <= lastSlot; ++slot)
        {
            const auto offset = (slot - firstSlot) * recordSize;
            data.objects[slot] = offset < payload.size() ?
                DecodeGameObject(payload.data() + offset, recordSize) : RawGameObject{};
        }
    }

    void GameContext::InitializeLevelPlayerSpawn(LevelLoadData& data,
        const std::array<std::int16_t, 3>& direction)
    {
        if (!data.level) { throw std::invalid_argument("Player start selection requires loaded level geometry."); }
        std::optional<PlayerCamera> spawn;
        // 1E48:1CC6 scans columns first, retaining the last exact E0+1 match.
        for (std::size_t column = 0; column < Level::Width; ++column)
        {
            for (std::size_t row = 0; row < Level::Height; ++row)
            {
                if (data.level->wallTextures[row][column] == 0xe1)
                {
                    const auto x = static_cast<std::int16_t>(150 + 100 * column);
                    const auto z = static_cast<std::int16_t>(4050 - 100 * row);
                    const auto y = WrapSignedWord(
                        GetTerrainHeight(*data.level, data.floorRepeatHeight, x, 0, z) + 45);
                    spawn = PlayerCamera{ { x, y, z }, direction };
                }
            }
        }
        if (!spawn) { throw std::runtime_error("Level geometry has no normal player start marker (0xE1)."); }
        data.playerSpawn = *spawn;
    }

    void GameContext::LoadLevel(const LevelLoadRequest&)
    {
        throw std::logic_error("The host must implement level loading.");
    }

    MenuPage GameContext::GetMenuPage() const
    {
        return menuPage;
    }

    void GameContext::SetMenuPage(MenuPage page)
    {
        if (page == menuPage) { return; }
        menuPage = page;
        applicationPageChanged = true;
        applicationTickAccumulator = 0;
        navigationRepeatKey.reset();
        navigationRepeatSeconds = 0;
        if (page == MenuPage::Inventory)
        {
            inventoryFocus = 0;
            inventorySource.reset();
            inventoryPulsePhase = 0;
            inventoryPulseAccumulator = 0;
        }
        else { inventorySource.reset(); }
        if (page == MenuPage::MainMenu || page == MenuPage::AlternateMainMenu) { mainMenuFocus = 0; }
    }

    bool GameContext::CanResumeGame() const
    {
        return hasActiveGame && !positionalSoundsSuppressed;
    }

    bool GameContext::SelectMainMenuAction(MainMenuAction action)
    {
        if (menuPage != MenuPage::MainMenu && menuPage != MenuPage::AlternateMainMenu)
        {
            throw std::logic_error("Main-menu choices require an active main menu.");
        }
        switch (action)
        {
        case MainMenuAction::Cancel:
            if (positionalSoundsSuppressed) { return false; }
            SetMenuPage(MenuPage::Gameplay);
            break;
        case MainMenuAction::ResumeGame:
            if (!CanResumeGame()) { return false; }
            SetMenuPage(MenuPage::Gameplay);
            break;
        case MainMenuAction::NewGame: SetMenuPage(MenuPage::ChapterSelection); break;
        case MainMenuAction::LoadGame: SetMenuPage(MenuPage::LoadGame); break;
        case MainMenuAction::SaveGame:
            if (!CanResumeGame()) { return false; }
            SetMenuPage(MenuPage::SaveGame);
            break;
        case MainMenuAction::Settings: SetMenuPage(MenuPage::Settings); break;
        case MainMenuAction::Configuration: SetMenuPage(MenuPage::Configuration); break;
        case MainMenuAction::Quit: SetMenuPage(MenuPage::Quit); break;
        default: throw std::invalid_argument("Unknown main-menu action.");
        }
        return true;
    }

    bool GameContext::OpenInventory()
    {
        if (menuPage != MenuPage::Gameplay)
        {
            throw std::logic_error("Inventory can only be opened from gameplay.");
        }
        if (positionalSoundsSuppressed) { return false; }
        SetMenuPage(MenuPage::Inventory);
        return true;
    }

    void GameContext::StartNewGame(std::int16_t chapter)
    {
        if (chapter <= 0)
        {
            throw std::invalid_argument("A new game requires a positive chapter number.");
        }
        InitializeNewGame(chapter);
        hasActiveGame = true;
        menuPage = MenuPage::Gameplay;
    }

    void GameContext::RunApplication()
    {
        while (menuPage != MenuPage::Quit)
        {
            RunApplicationStep();
        }
    }

    void GameContext::RunApplicationStep()
    {
        switch (menuPage)
        {
        case MenuPage::Gameplay: menuPage = RunGameplay(); break;
        case MenuPage::MainMenu:
        case MenuPage::AlternateMainMenu:
            SelectMainMenuAction(ShowMainMenu());
            break;
        case MenuPage::Settings: menuPage = ShowSettings(); break;
        case MenuPage::Configuration: menuPage = ShowConfiguration(); break;
        case MenuPage::ChapterSelection:
        {
            const auto chapter = ShowChapterSelection();
            if (chapter) { StartNewGame(*chapter); }
            else { menuPage = MenuPage::MainMenu; }
            break;
        }
        case MenuPage::LoadGame: menuPage = ShowLoadGame(); break;
        case MenuPage::SaveGame:
            // The native dispatcher rechecks this shared gate even when a page is selected directly.
            menuPage = positionalSoundsSuppressed ? MenuPage::MainMenu : ShowSaveGame();
            break;
        case MenuPage::Inventory: menuPage = ShowInventory(); break;
        case MenuPage::ReturnToGameplay: menuPage = MenuPage::Gameplay; break;
        case MenuPage::Presentation20:
            menuPage = ShowUnrecoveredPage(menuPage);
            positionalSoundsSuppressed = true;
            break;
        case MenuPage::Presentation26:
        case MenuPage::Presentation32:
            ShowUnrecoveredPage(menuPage);
            menuPage = MenuPage::Gameplay;
            break;
        case MenuPage::Presentation31:
        case MenuPage::Presentation33:
        case MenuPage::Dialog101:
            ShowUnrecoveredPage(menuPage);
            menuPage = MenuPage::MainMenu;
            break;
        case MenuPage::UnidentifiedPage5:
        case MenuPage::UnidentifiedPage8:
        case MenuPage::UnidentifiedPage9:
        case MenuPage::UnidentifiedPage11:
        case MenuPage::Presentation22:
        case MenuPage::Presentation23:
        case MenuPage::Presentation25:
        case MenuPage::Dialog100:
            menuPage = ShowUnrecoveredPage(menuPage);
            break;
        case MenuPage::Quit: break;
        default:
        {
            const auto value = static_cast<std::uint16_t>(menuPage);
            if (value >= 1 && value <= 20)
            {
                // Unassigned states in this native range return to gameplay.
                menuPage = MenuPage::Gameplay;
            }
            else if (value >= 200 && value <= 299)
            {
                menuPage = ShowUnrecoveredPage(menuPage);
            }
            else
            {
                // The original dispatcher's default branch requests application shutdown.
                menuPage = MenuPage::Quit;
            }
            break;
        }
        }
    }

    MainMenuAction GameContext::ShowMainMenu()
    {
        throw std::logic_error("Main-menu input and drawing are not implemented.");
    }

    std::optional<std::int16_t> GameContext::ShowChapterSelection()
    {
        throw std::logic_error("Chapter-selection input and drawing are not implemented.");
    }

    MenuPage GameContext::RunGameplay()
    {
        throw std::logic_error("The complete gameplay frame loop is not implemented.");
    }

    MenuPage GameContext::ShowSettings()
    {
        throw std::logic_error("The settings page is not implemented.");
    }

    MenuPage GameContext::ShowConfiguration()
    {
        throw std::logic_error("The configuration page is not implemented.");
    }

    MenuPage GameContext::ShowLoadGame()
    {
        throw std::logic_error("The load-game page and loading are not implemented.");
    }

    MenuPage GameContext::ShowSaveGame()
    {
        throw std::logic_error("The save-game page and saving are not implemented.");
    }

    MenuPage GameContext::ShowInventory()
    {
        throw std::logic_error("The inventory page loop is not implemented.");
    }

    MenuPage GameContext::ShowUnrecoveredPage(MenuPage page)
    {
        throw std::logic_error("Application page " +
            std::to_string(static_cast<std::uint16_t>(page)) + " is not implemented.");
    }

    void GameContext::InitializeNewGame(std::int16_t)
    {
        throw std::logic_error("New-game resource loading and initialization are not implemented.");
    }

    std::int16_t GameContext::RandomSigned(std::uint16_t radius)
    {
        if (radius > 32767)
        {
            throw std::invalid_argument("Signed random radius must fit a nonnegative signed word.");
        }
        const auto value = static_cast<std::int16_t>(
            static_cast<int>(Random(static_cast<std::uint16_t>(radius * 2u + 1u))) - radius);
        return value == 0 ? static_cast<std::int16_t>(radius) : value;
    }

    std::int16_t GameContext::GetPlayerHeading() const
    {
        return playerHeading;
    }

    void GameContext::TeleportPlayer(std::int16_t destinationId)
    {
        std::array<std::uint16_t, 140> destinations{};
        std::size_t count = 0;
        for (std::uint16_t slot = 2; slot <= 141; ++slot)
        {
            const auto& object = worldObjects[slot];
            if (object.health == 1000 && object.collisionType == CollisionType::Teleporter &&
                object.itemValue == destinationId)
            {
                destinations[count++] = slot;
            }
        }
        if (count == 0) { throw std::runtime_error("Teleporter destination not found: " + std::to_string(destinationId)); }
        auto& player = worldObjects[1];
        auto eyeY = WrapSignedWord(player.posY + player.collisionRadiusY + 2);
        for (std::size_t index = 0; index < count; ++index)
        {
            auto& destination = worldObjects[destinations[index]];
            eyeY = destination.velocityY == 0 ? WrapSignedWord(
                GetTerrainFloorHeight(destination.posX, eyeY, destination.posZ) + playerBodyHeight) : destination.posY;
            player.posX = destination.posX;
            player.posZ = destination.posZ;
            player.posY = WrapSignedWord(eyeY - player.collisionRadiusY - 2);
            if (destination.winY >= 0) { playerTargetHeading = destination.winY; }
            playerHeading = playerTargetHeading;
            std::int16_t negativeZ;
            GetDirectionDisplacement(player.velocityX, negativeZ, playerHeading, 100);
            player.velocityZ = WrapSignedWord(-static_cast<int>(negativeZ));
            if (destination.behaviorParameter != 0) { playerRenderTable = destination.behaviorParameter; }
            field_D503 = destination.velocityZ > 0;
            destination.generalPurpose3 = 0;
        }
        soundListenerX = player.posX;
        soundListenerZ = player.posZ;
        lightingCache.Reset(soundListenerX, soundListenerZ, lightingCache.activeRadius);
    }

    InteractionResult GameContext::HandleTeleporterContact(RawGameObject& object, bool interactionPressed)
    {
        if (object.collisionType != CollisionType::Teleporter)
        {
            throw std::invalid_argument("Teleporter contact requires collision type 120.");
        }
        if (object.health <= 0) { return { InteractionStatus::InactiveObject }; }
        if (!interactionPressed)
        {
            // DBC3: the automatic gate is word 46, not the manual path's word-40 cooldown.
            if (object.doorLockOrGroupBitmask <= 10 || object.groupIndexOrLock != 0)
            {
                return { InteractionStatus::NoEffect };
            }
        }
        else
        {
            if (object.doorLockOrGroupBitmask > 0) { return HandleLockedObjectInteraction(object, *this); }
            if (object.velocityX != 0)
            {
                if (object.soundEffectId != 0)
                {
                    PlaySoundEffect(static_cast<SoundEffect>(object.soundEffectId), GetSoundVolume(0), object, 1);
                }
                object.blendWeightOrSpriteOpacity = WrapSignedWord(
                    object.blendWeightOrSpriteOpacity + object.velocityX * frameTickCount);
                if (object.blendWeightOrSpriteOpacity > 100)
                {
                    object.blendWeightOrSpriteOpacity = 100;
                    object.velocityX = 0;
                }
                return { InteractionStatus::Applied };
            }
            if (WrapSignedWord(object.generalPurpose3) <= 10) { return { InteractionStatus::NoEffect }; }
        }
        TeleportPlayer(WrapSignedWord(object.generalPurpose1));
        return { InteractionStatus::Applied };
    }

    InteractionResult GameContext::HandleCockroachContact(RawGameObject& object)
    {
        if (!behaviorDispatchEnabled) { return { InteractionStatus::NoEffect }; }
        if (AbsoluteWord(playerForwardSpeed) > object.doorLockOrGroupBitmask)
        {
            DamageObject(object, 1);
            return { InteractionStatus::Applied };
        }
        if (object.behaviorParameter == 0) { return { InteractionStatus::NoEffect }; }
        object.velocityX = 0;
        object.velocityZ = 0;
        ApplyPlayerLifeDelta(-29491 * static_cast<std::int32_t>(object.behaviorParameter));
        playerHitEffectTicks = 8;
        return { InteractionStatus::Applied };
    }

    void GameContext::ApplyPlayerLifeDelta(std::int32_t delta)
    {
        if (DEBUG_noPlayerCollision && delta < 0) { return; }
        const auto previousLife = playerLife;
        if (playerLifeDeltaSuppressed == 0)
        {
            const auto bits = static_cast<std::uint32_t>(playerLife) + static_cast<std::uint32_t>(delta);
            playerLife = bits <= 0x7fffffffu ? static_cast<std::int32_t>(bits) :
                static_cast<std::int32_t>(static_cast<std::int64_t>(bits) - 0x100000000LL);
        }
        if (playerLife >= static_cast<std::int32_t>(playerLifeMaximum) * 65536)
        {
            playerLife = static_cast<std::int32_t>(playerLifeMaximum) * 65536;
        }
        if (playerLife <= 0)
        {
            playerLife = 0;
            if (!positionalSoundsSuppressed)
            {
                playerHitEffectTicks = 1000;
                playerPostureDelta = -1;
                positionalSoundsSuppressed = true;
                worldObjects[1].velocityZ = 400;
            }
        }
        if (playerLife != previousLife) { OnPlayerHealthChanged(previousLife, playerLife); }
    }

    void GameContext::StopAllSoundEffects()
    {
        if (audioHandler) { audioHandler->StopAllSamples(); }
        soundChannelSources.fill(0);
    }

    void GameContext::ActivateObjectGroup(std::int16_t group)
    {
        const auto health = WrapSignedWord(group + 1000);
        for (std::size_t slot = 2; slot <= 140; ++slot)
        {
            if (worldObjects[slot].health == health) { DamageObject(worldObjects[slot], 0); }
        }
    }

    InteractionResult GameContext::HandleActivationTriggerContact(RawGameObject& object)
    {
        if (!behaviorDispatchEnabled) { return { InteractionStatus::NoEffect }; }
        const auto health = WrapSignedWord(object.itemValue + 1000);
        if (object.collisionType == CollisionType::DeadBullHead)
        {
            object.collisionType = CollisionType::None;
        }
        else { SetObjectHealth(object, 0); }
        for (std::size_t slot = 2; slot <= 140; ++slot)
        {
            auto& target = worldObjects[slot];
            if (target.health != health) { continue; }
            DamageObject(target, 0);
            SetObjectHealth(target, 1000);
        }
        return { InteractionStatus::Applied };
    }

    void GameContext::ApplyObjectKnockback(RawGameObject& object, std::int16_t amount)
    {
        if (object.health != 1000) { return; }
        amount = std::min<std::int16_t>(amount, 30);
        const auto dx = WrapSignedWord(soundListenerX - object.posX);
        const auto dz = WrapSignedWord(soundListenerZ - object.posZ);
        const auto distance = WrapSignedWord(AbsoluteWord(dx) + AbsoluteWord(dz));
        if (distance == 0) { throw std::runtime_error("Native object knockback would divide by zero."); }
        const auto numeratorX = WrapSignedWord(-static_cast<std::int32_t>(dx) * amount);
        const auto numeratorZ = WrapSignedWord(-static_cast<std::int32_t>(dz) * amount);
        if (distance == -1 && (numeratorX == -32768 || numeratorZ == -32768))
        {
            throw std::overflow_error("Native object knockback would overflow signed-word division.");
        }
        const auto x = numeratorX / distance;
        object.itemValue = WrapSignedWord(numeratorZ / distance);
        std::int16_t correctionX = 0;
        std::int16_t correctionZ = 0;
        std::uint16_t center = 0;
        QueryCollisionMapNeighborhood(WrapSignedWord(object.posX + x),
            WrapSignedWord(object.posZ + object.itemValue), 1, correctionX, correctionZ, center, 0x38);
        if (correctionX == 0 && correctionZ == 0)
        {
            object.posX = WrapSignedWord(object.posX + x);
            object.posZ = WrapSignedWord(object.posZ + object.itemValue);
        }
    }

    void GameContext::SpawnBreakupParticle(const RawGameObject& source, std::int16_t selector,
        std::int16_t radiusX, std::int16_t radiusY, std::int16_t radiusZ)
    {
        auto& particle = worldObjects[FindFreeObjectIndex()];
        if (source.collisionType != CollisionType::Type_22) { return; }
        const auto linkedSlot = source.generalPurpose1;
        particle.health = static_cast<std::int16_t>(Random(50) + 50);
        particle.modelId = 1;
        particle.modelAnim = 5;
        particle.animFrame = static_cast<std::uint8_t>(Random(2) + 1);
        particle.displayType = DisplayType::SpriteTransparent;
        particle.blendWeightOrSpriteOpacity = 10;
        particle.winY = static_cast<std::int16_t>(Random(2));
        particle.spriteScale = 24;
        particle.groupIndexOrLock = 5;
        particle.itemValue = 75;
        particle.generalPurpose1 = 1;
        if (selector == 0)
        {
            particle.posX = WrapSignedWord(source.posX + RandomSigned(static_cast<std::uint16_t>(radiusX)));
            particle.posY = WrapSignedWord(source.posY + RandomSigned(static_cast<std::uint16_t>(radiusY)));
            particle.posZ = WrapSignedWord(source.posZ + RandomSigned(static_cast<std::uint16_t>(radiusZ)));
            std::int16_t x;
            std::int16_t z;
            std::uint16_t center;
            do
            {
                x = 0;
                z = 0;
                center = 0;
                QueryCollisionMapNeighborhood(particle.posX, particle.posZ, 5, x, z, center, 8);
                particle.posX = WrapSignedWord(particle.posX + x);
                particle.posZ = WrapSignedWord(particle.posZ + z);
            } while ((x != 0 || z != 0) && center != 8);
            particle.velocityX = RandomSigned(4);
            particle.velocityY = static_cast<std::int16_t>(Random(6) + 2);
            particle.velocityZ = RandomSigned(4);
            particle.behavior = BehaviorType::BouncingObject;
        }
        else
        {
            if (linkedSlot >= worldObjects.size())
            {
                throw std::out_of_range("Breakup particle references a world-object slot outside 0..165.");
            }
            const auto& linked = worldObjects[linkedSlot];
            const auto radius = static_cast<std::uint16_t>(linked.collisionRadiusXZ);
            const auto offsetX = RandomSigned(radius >> 1);
            const auto x = WrapSignedWord(linked.posX +
                GetStepToward(linked.posX, soundListenerX, radius >> 2) + offsetX);
            const auto halfY = linked.collisionRadiusY >= 0 ? linked.collisionRadiusY / 2 :
                -((-static_cast<int>(linked.collisionRadiusY) + 1) / 2);
            const auto y = WrapSignedWord(linked.posY + RandomSigned(static_cast<std::uint16_t>(halfY)));
            const auto offsetZ = RandomSigned(radius >> 1);
            const auto z = WrapSignedWord(linked.posZ +
                GetStepToward(linked.posZ, soundListenerZ, radius >> 2) + offsetZ);
            InitializeSmokeParticle(particle, x, y, z, linked.collisionRadiusXZ, 1, linked.renderPlacementMode);
        }
        NotifyObjectCreated(particle);
    }

    void GameContext::SpawnActivationParticle(std::int16_t x, std::int16_t y, std::int16_t z,
        std::int16_t angle, std::int16_t speed)
    {
        auto& particle = worldObjects[FindFreeOrLowestHealthObjectIndex()];
        particle.health = 100;
        particle.collisionMask = 1;
        particle.spriteScale = 35;
        particle.behavior = BehaviorType::FlyingArrow;
        particle.collisionType = CollisionType::FlyingArrow;
        particle.posX = x;
        particle.posY = y;
        particle.posZ = z;
        GetClampedDirectionDisplacement(particle.velocityX, particle.velocityZ,
            static_cast<std::int16_t>(angle & 511), speed);
        particle.modelId = 1;
        particle.animFrame = static_cast<std::uint8_t>(Random(2) + 1);
        particle.modelAnim = 5;
        particle.displayType = DisplayType::SpriteTransparent;
        particle.velocityY = static_cast<std::int16_t>(Random(6));
        particle.itemValue = 6;
        particle.behaviorParameter = 70;
        particle.collisionRadiusXZ = 55;
        particle.collisionRadiusY = 25;
        particle.spriteScale = static_cast<std::uint8_t>(Random(20) + 40);
        particle.health = static_cast<std::int16_t>(Random(10) + 25);
        particle.renderPlacementMode = 3;
        particle.blendWeightOrSpriteOpacity = 20;
        const auto floor = WrapSignedWord(GetTerrainFloorHeight(x, y, z) + 8);
        if (particle.posY < floor) { particle.posY = floor; }
        NotifyObjectCreated(particle);
    }

    void GameContext::DamageObject(RawGameObject& object, std::int16_t amount)
    {
        const auto volume = GetSoundVolumeAt(object.posX, object.posZ);
        const auto originalStrength = object.groupIndexOrLock;
        const auto play = [&](std::uint8_t effect, std::uint8_t loudness, std::int16_t limit)
        {
            if (effect != 0) { PlaySoundEffect(static_cast<SoundEffect>(effect), loudness, object, limit); }
        };
        if (amount == 0 && object.health > 1000 && object.health < 2000)
        {
            switch (object.collisionType)
            {
            case CollisionType::BurnableOilPuddle:
                if (object.displayType == DisplayType::VectorNormal)
                {
                    object.displayType = DisplayType::LastVectorBurning;
                }
                break;
            case CollisionType::None:
            case CollisionType::RotatingBlades:
            case CollisionType::Ghost:
            case CollisionType::ClimbableLadder:
            case CollisionType::AttachableDamageObject:
            case CollisionType::Key:
            case CollisionType::Medicine:
            case CollisionType::LiftUp:
            case CollisionType::WalkablePlanks:
            case CollisionType::Weapon:
            case CollisionType::PushPlayer:
                object.health = 1000;
                break;
            default: break;
            }
        }
        // Keep the existing skeleton helper's subtraction and fragment ordering in one place.
        if (object.collisionType == CollisionType::Skeleton) { DamageSkeleton(object, amount); return; }
        if (object.groupIndexOrLock > 0)
        {
            object.groupIndexOrLock = WrapSignedWord(object.groupIndexOrLock - amount);
        }
        switch (object.collisionType)
        {
        case CollisionType::CeilingConcreteSlabStake: return;
        case CollisionType::RopeSwitch:
            object.posY = WrapSignedWord(object.posY + amount);
            return;
        case CollisionType::MirrorToGrab:
            if (amount > 100) { object.winX = 128; }
            return;
        case CollisionType::KnightShootsArrow:
            play(object.soundEffectId, volume, 2);
            object.groupIndexOrLock = originalStrength;
            return;
        case CollisionType::VaseToGrab:
            if (amount < 200) { object.groupIndexOrLock = originalStrength; return; }
            if (object.groupIndexOrLock > 0) { return; }
            StopAllSoundEffects();
            play(object.soundEffectId, volume, 2);
            object.collisionType = CollisionType::None;
            object.collisionMask = 0;
            object.behavior = BehaviorType::None;
            if (object.modelFrame2 > 0) { object.animFrame = object.modelFrame2; }
            if (WrapSignedWord(object.generalPurpose3) > 0)
            {
                const auto slot = FindFreeOrLowestHealthObjectIndex();
                ReplaceWorldObject(slot, object);
                object.collisionType = CollisionType::Key;
                object.pickupType = PickupType::Type_4;
                object.collisionMask = 3;
                object.displayType = DisplayType::VectorUnlit;
                object.renderPlacementMode = 5;
                object.modelId = 39;
                object.modelAnim = 1;
                object.animFrame = 1;
                object.itemValue = WrapSignedWord(object.generalPurpose3);
            }
            return;
        case CollisionType::Weapon:
        case CollisionType::Ammo:
            if (amount == -1) { object.collisionMask = 0; object.health = 10; }
            else { object.groupIndexOrLock = originalStrength; }
            return;
        case CollisionType::Type_138:
            object.behavior = static_cast<BehaviorType>(static_cast<std::uint16_t>(object.doorLockOrGroupBitmask));
            object.collisionType = CollisionType::None;
            object.displayType = DisplayType::SpriteTransparent;
            return;
        case CollisionType::Door:
            if (object.doorLockOrGroupBitmask < 1 && amount == 0)
            {
                if (object.blendWeightOrSpriteOpacity < 100)
                {
                    object.generalPurpose2 = 3;
                    if (object.modelFrame2 == 0) { object.displayType = DisplayType::Hidden; }
                    play(object.soundEffectId, volume, 1);
                }
                object.itemValue = WrapSignedWord(object.generalPurpose1);
            }
            return;
        case CollisionType::WallDestruction:
        {
            if (!level || !collisionMap) { throw std::runtime_error("Wall destruction requires level geometry and collision memory."); }
            const auto cell = GetHeightPosition(object.posX, object.posZ);
            const auto column = WrapSignedWord(object.posX - 112) / 25;
            const auto row = WrapSignedWord(162 - WrapSignedWord(object.posZ - 38) / 25);
            const auto stride = static_cast<std::size_t>(collisionMap->widthMinusOne) + 1;
            for (int dx = -1; dx <= 2; ++dx)
            {
                for (int dz = -1; dz <= 2; ++dz)
                {
                    const int px = column + dx;
                    const int pz = row + dz;
                    if (px < 0 || pz < 0 || px > collisionMap->widthMinusOne || pz > collisionMap->heightMinusOne) { continue; }
                    const auto index = static_cast<std::size_t>(pz) * stride + px;
                    if (index >= CollisionMap::VARIABLE_SIZE) { throw std::out_of_range("Wall destruction exceeds collision-map storage."); }
                    auto& pixel = collisionMap->data[index];
                    if ((pixel & 0x38) == 0x38) { pixel = 0; }
                }
            }
            SetObjectHealth(object, 0);
            object.posX = WrapSignedWord(object.posX / 100);
            object.posZ = WrapSignedWord(41 - object.posZ / 100);
            level->wallTextures[cell.row][cell.column] = 0;
            ++level->geometryRevision;
            return;
        }
        case CollisionType::WaterRecharge:
        {
            if (!level) { throw std::runtime_error("Water-recharge activation requires level geometry."); }
            const auto cell = GetHeightPosition(object.posX, object.posZ);
            object.collisionType = CollisionType::None;
            level->floorObjects[cell.row][cell.column] = static_cast<std::uint8_t>(object.itemValue);
            ++level->geometryRevision;
            return;
        }
        case CollisionType::PushingEnemy:
            if (object.groupIndexOrLock < 1)
            {
                object.animFrame = 1; object.modelFrame2 = 2; object.modelAnim = 3;
                object.collisionType = CollisionType::None; object.collisionMask = 0;
                object.generalPurpose1 = 1; object.generalPurpose2 = 0;
                object.blendWeightOrSpriteOpacity = 0; object.itemValue = 0;
                object.renderPlacementMode = 3;
                play(12, volume, 2);
            }
            else if (amount > 1)
            {
                play(11, volume, 1);
                ApplyObjectKnockback(object, amount);
            }
            return;
        case CollisionType::SpiderLarge:
        {
            constexpr std::array<std::array<std::uint8_t, 2>, 32> sounds{{ {16,13}, {26,26} }};
            if (object.soundEffectId >= sounds.size()) { throw std::out_of_range("Spider sound-pair index must be in 0..31."); }
            if (object.groupIndexOrLock < 1)
            {
                object.animFrame = 5; object.modelFrame2 = 4;
                object.collisionType = CollisionType::None; object.collisionMask = 0;
                object.velocityY = 10; object.posY = WrapSignedWord(object.posY + object.velocityY);
                object.generalPurpose1 = 1; object.blendWeightOrSpriteOpacity = -100;
                StopAllSoundEffects();
                play(sounds[object.soundEffectId][0], volume, 2);
            }
            else if (amount > 1) { play(sounds[object.soundEffectId][1], volume, 1); }
            return;
        }
        case CollisionType::Harvestman:
            if (object.groupIndexOrLock < 1)
            {
                object.modelFrame2 = 15; object.collisionType = CollisionType::None; object.collisionMask = 0;
                object.generalPurpose1 = 1; object.blendWeightOrSpriteOpacity = 0; object.generalPurpose2 = 0;
            }
            return;
        case CollisionType::Devil:
            if (object.groupIndexOrLock < 1)
            {
                object.modelFrame2 = object.animFrame; object.modelAnim = 1; object.animFrame = 4;
                object.collisionType = CollisionType::None; object.collisionMask = 0;
                object.generalPurpose1 = 1; object.blendWeightOrSpriteOpacity = 100; object.generalPurpose2 = 0;
                play(12, volume, 2);
            }
            else if (amount > 1) { play(11, volume, 1); }
            return;
        case CollisionType::Dragon:
            if (object.groupIndexOrLock < 1 && object.behavior != BehaviorType::Behavior_61)
            {
                object.doorLockOrGroupBitmask = 0; object.behavior = BehaviorType::Behavior_61;
                object.blendWeightOrSpriteOpacity = 0; object.animFrame = 1; object.modelFrame2 = 2;
                object.modelAnim = 2; object.generalPurpose2 = 0; object.itemValue = 4; object.generalPurpose1 = 250;
            }
            else
            {
                if (originalStrength / 1000 != object.groupIndexOrLock / 1000 && Random(3) == 0)
                {
                    object.itemValue = -130;
                    play(42, GetSoundVolume(static_cast<std::int16_t>(
                        GetSoundDistanceAt(object.posX, object.posZ) / 2)), 3);
                }
                object.velocityY = 100;
            }
            return;
        case CollisionType::Cockroach:
            if (object.groupIndexOrLock < 1)
            {
                if (amount < 100) { play(object.soundEffectId, volume, 1); }
                if (object.itemValue < 7)
                {
                    object.collisionType = CollisionType::None;
                    if (object.collisionMask != 131) { object.collisionMask = 0; }
                    object.itemValue = 7; object.winX = 0; object.winZ = 0;
                }
                if (object.itemValue == 15) { object.itemValue = 16; object.blendWeightOrSpriteOpacity = -100; }
                if (object.displayType == DisplayType::Hidden) { SetObjectHealth(object, 0); }
            }
            else { play(26, volume, 1); }
            return;
        case CollisionType::Shark:
            if (object.groupIndexOrLock < 1 && object.health == 1000)
            {
                object.modelAnim = 3; object.velocityY = 3; object.health = 2000;
                object.animFrame = 1; object.modelFrame2 = 2; object.itemValue = 200;
            }
            SpawnWaterImpact(object.posX, object.posZ);
            return;
        case CollisionType::Crab:
            if (object.groupIndexOrLock <= 0)
            {
                object.collisionMask = 0; object.behavior = BehaviorType::None;
                object.modelAnim = 2; object.animFrame = 1; object.modelFrame2 = 0;
                play(26, volume, 1);
            }
            return;
        case CollisionType::Switch:
            if (object.itemValue > 0 && object.blendWeightOrSpriteOpacity == 0)
            {
                if (object.health == 1000) { ActivateObjectGroup(object.itemValue); }
                object.groupIndexOrLock = 1; object.generalPurpose2 = 10;
                play(object.soundEffectId, volume, 1);
            }
            return;
        case CollisionType::BatIndoor:
            if (object.groupIndexOrLock < 1)
            {
                object.velocityY = 2; object.collisionType = CollisionType::None;
                object.health = 2000; object.blendWeightOrSpriteOpacity = -100;
            }
            return;
        case CollisionType::RatAndSnake:
            if (object.health == 1000)
            {
                if (object.modelAnim == 1) { play(17, GetSoundVolume(0), 1); }
                else if (object.modelAnim == 2) { play(20, GetSoundVolume(200), 1); }
                object.modelFrame2 = object.animFrame; object.animFrame = 3;
                object.collisionMask = 0; object.collisionType = CollisionType::None; object.health = 1500;
                object.velocityY = 8; object.posY = WrapSignedWord(object.posY + object.velocityY);
                object.blendWeightOrSpriteOpacity = -100;
            }
            return;
        case CollisionType::FlyingArrow:
            if (object.health == 1000) { SetObjectHealth(object, 0); }
            return;
        case CollisionType::Type_22:
            object.posX = WrapSignedWord(object.posX - 2 * object.velocityX);
            object.posZ = WrapSignedWord(object.posZ - 2 * object.velocityZ);
            PlaySoundEffect(static_cast<SoundEffect>(37), volume, 467, 1);
            for (int i = 0; i < 9; ++i) { SpawnBreakupParticle(object, amount, 10, 10, 10); }
            SetObjectHealth(object, 0);
            return;
        case CollisionType::DestroyOnCollide:
            object.collisionMask = 0; object.collisionType = CollisionType::None;
            object.animFrame = static_cast<std::uint8_t>(object.animFrame + 1);
            return;
        case CollisionType::DestroyableBarrier:
            if (object.soundEffectId == 0) { play(52, GetSoundVolume(0), 1); }
            object.generalPurpose1 = static_cast<std::uint16_t>(object.generalPurpose1 + object.behaviorParameter * amount);
            if (static_cast<std::int16_t>(object.generalPurpose1 >> 8) >= object.doorLockOrGroupBitmask)
            {
                object.collisionMask = 0; object.generalPurpose1 = 32000;
            }
            return;
        case CollisionType::DiggableGround:
            if (amount == 5)
            {
                if (selectedWeapon == WeaponType::Showel) { weaponAnimationRecoil = 2; }
                play(54, GetSoundVolume(0), 1);
                object.itemValue = WrapSignedWord(object.itemValue + object.behaviorParameter);
            }
            else { object.groupIndexOrLock = originalStrength; }
            return;
        case CollisionType::Dog:
            if (object.groupIndexOrLock < 1)
            {
                play(50, volume, 2);
                object.itemValue = 100; object.modelAnim = 1; object.animFrame = 3; object.modelFrame2 = 4;
                object.blendWeightOrSpriteOpacity = 0; object.generalPurpose2 = 0;
                object.collisionMask = 0; object.collisionType = CollisionType::None;
            }
            else if (object.itemValue == 1) { object.itemValue = -2; }
            return;
        case CollisionType::AnacondaHead:
            if (object.groupIndexOrLock < 1)
            {
                play(48, GetSoundVolume(0), 1);
                object.behavior = BehaviorType::Behavior_87; object.health = 2000;
                object.modelAnim = 3; object.animFrame = 3; object.modelFrame2 = 0; object.renderPlacementMode = 1;
            }
            else
            {
                object.generalPurpose1 = 10;
                if (amount > 1) { play(object.soundEffectId, volume, 1); }
            }
            return;
        case CollisionType::AnacondaTail:
            if (object.groupIndexOrLock < 1)
            {
                object.health = 2000; play(3, volume, 1); object.modelAnim = 3; object.animFrame = 1;
            }
            return;
        case CollisionType::AttachableDamageObject:
        case CollisionType::KrakenHead:
            if (object.groupIndexOrLock < 1 && object.health == 1000)
            {
                if (object.collisionType == CollisionType::AttachableDamageObject)
                {
                    object.modelFrame2 = 5;
                    if (object.doorLockOrGroupBitmask > 0)
                    {
                        const auto slot = static_cast<std::size_t>(object.doorLockOrGroupBitmask);
                        if (slot >= worldObjects.size()) { throw std::out_of_range("Attached damage target is outside 0..165."); }
                        worldObjects[slot].generalPurpose3 = static_cast<std::uint16_t>(worldObjects[slot].generalPurpose3 - object.generalPurpose3);
                    }
                }
                else { object.modelFrame2 = 2; }
                play(16, volume, 1);
                object.collisionMask = 0; object.collisionType = CollisionType::None; object.health = 2000;
                object.blendWeightOrSpriteOpacity = 0; object.behavior = BehaviorType::AttachableDamageObject;
            }
            return;
        case CollisionType::ActivateSpiders:
            if (object.groupIndexOrLock < 1 && object.health == 1000)
            {
                if (amount < 200) { object.groupIndexOrLock = originalStrength; return; }
                for (int i = 1; i <= 20; ++i)
                {
                    const auto x = WrapSignedWord(object.posX + RandomSigned(15));
                    const auto heightOffset = Random(10);
                    const auto y = WrapSignedWord(GetTerrainFloorHeight(object.posX, object.posY, object.posZ) + heightOffset);
                    const auto z = WrapSignedWord(object.posZ + RandomSigned(15));
                    const auto angle = static_cast<std::int16_t>(Random(512));
                    const auto speed = static_cast<std::int16_t>(Random(
                        static_cast<std::uint16_t>(WrapSignedWord(object.doorLockOrGroupBitmask + 1))));
                    SpawnActivationParticle(x, y, z, angle, speed);
                }
                object.health = 35;
                play(object.soundEffectId, GetSoundVolume(0), 1);
            }
            return;
        case CollisionType::DamageObj:
            if (amount == -1)
            {
                object.behavior = BehaviorType::WalkableDrawBridge;
                std::swap(object.animFrame, object.modelFrame2);
                object.blendWeightOrSpriteOpacity = 100; object.velocityZ = 3;
                object.displayType = static_cast<DisplayType>(80); object.collisionMask = 0;
                play(20, GetSoundVolume(0), 1);
                for (int i = -1; i <= 3; ++i)
                {
                    auto& particle = worldObjects[FindFreeObjectIndex()];
                    const auto x = WrapSignedWord(object.posX + RandomSigned(
                        static_cast<std::uint16_t>(object.collisionRadiusXZ)));
                    const auto y = WrapSignedWord(object.posY + i * 3);
                    const auto z = WrapSignedWord(object.posZ + RandomSigned(
                        static_cast<std::uint16_t>(object.collisionRadiusXZ)));
                    InitializeSmokeParticle(particle, x, y, z, object.collisionRadiusXZ, 1, object.renderPlacementMode);
                }
            }
            return;
        default:
            object.groupIndexOrLock = originalStrength;
            return;
        }
    }

    void GameContext::SpawnSkeletonFragment(const RawGameObject& source, std::uint16_t part,
        std::int16_t x, std::int16_t y, std::int16_t z)
    {
        const auto slot = FindFreeObjectIndex();
        // The native allocator can clear the source itself; read it after allocation.
        if (source.collisionType == CollisionType::None) { return; }
        if (source.collisionType != CollisionType::Skeleton)
        {
            throw std::invalid_argument("SpawnSkeletonFragment requires collision type Skeleton.");
        }
        auto& fragment = worldObjects[slot];
        fragment.modelId = source.modelId;
        fragment.modelAnim = static_cast<std::uint8_t>(part);
        fragment.displayType = source.displayType;
        fragment.animFrame = 5;
        fragment.winY = WrapSignedWord(source.winY + part);
        part &= 255;
        fragment.health = 1000;
        fragment.soundEffectId = 6;
        fragment.behavior = BehaviorType::BouncingObject;
        fragment.itemValue = 85;
        fragment.generalPurpose1 = 8;
        fragment.winX = 128;
        fragment.velocityX = RandomSigned(2);
        fragment.velocityZ = RandomSigned(2);
        fragment.velocityY = WrapSignedWord(5 - part);
        if (part == 1 || part == 2) { fragment.itemValue = 40; }
        else if (part == 4 || part == 5) { fragment.modelFrame2 = 35; }
        else if (part == 6)
        {
            fragment.modelId = 39;
            fragment.modelAnim = 2;
            fragment.animFrame = 2;
            fragment.doorLockOrGroupBitmask = 6;
            fragment.spriteScale = 41;
            fragment.collisionMask = 1;
            fragment.winX = 0;
            fragment.itemValue = 2;
            fragment.collisionType = CollisionType::Weapon;
            fragment.displayType = DisplayType::VectorNormal;
            fragment.generalPurpose1 = 0;
            fragment.collisionRadiusXZ = 15;
            fragment.collisionRadiusY = 10;
            fragment.pickupType = PickupType::Type_40;
            fragment.soundEffectId = 39;
        }
        fragment.posX = WrapSignedWord(source.posX + x);
        fragment.posY = WrapSignedWord(source.posY - y);
        fragment.posZ = WrapSignedWord(source.posZ + z);
        NotifyObjectCreated(fragment);
    }

    void GameContext::DamageSkeleton(RawGameObject& object, std::int16_t amount)
    {
        if (object.collisionType != CollisionType::Skeleton)
        {
            throw std::invalid_argument("DamageSkeleton requires collision type Skeleton.");
        }
        const auto volume = GetSoundVolumeAt(object.posX, object.posZ);
        if (object.groupIndexOrLock > 0)
        {
            object.groupIndexOrLock = WrapSignedWord(object.groupIndexOrLock - amount);
        }
        if (object.groupIndexOrLock < 1)
        {
            std::int16_t x;
            std::int16_t z;
            GetClampedDirectionDisplacement(x, z, WrapSignedWord(object.winY - 128), 3);
            SpawnSkeletonFragment(object, 1, 0, -28, 0);
            SpawnSkeletonFragment(object, 2, 0, -12, 0);
            SpawnSkeletonFragment(object, 3, 0, 0, 0);
            SpawnSkeletonFragment(object, 6, WrapSignedWord(x * 4), 5, WrapSignedWord(z * 4));
            SpawnSkeletonFragment(object, 4, x, -8, z);
            SpawnSkeletonFragment(object, 260, WrapSignedWord(-x), -8, WrapSignedWord(-z));
            SpawnSkeletonFragment(object, 4, x, 14, z);
            SpawnSkeletonFragment(object, 260, WrapSignedWord(-x), 14, WrapSignedWord(-z));
            SetObjectHealth(object, 0);
        }
        else if (amount > 1)
        {
            PlaySoundEffect(static_cast<SoundEffect>(11), volume, object, 1);
        }
    }

    void GameContext::SpawnWaterImpact(std::int16_t x, std::int16_t z)
    {
        auto& impact = worldObjects[FindFreeOrLowestHealthObjectIndex()];
        impact.posX = x;
        impact.posZ = z;
        const auto isWater = [this](std::int16_t px, std::int16_t pz)
        {
            const auto material = GetCollisionMapPixelAtWorldXZ(px, pz) & 7;
            return material == 1 || material == 3;
        };
        for (int dx = -1; dx <= 1; ++dx)
        {
            for (int dz = -1; dz <= 1; ++dz)
            {
                if (!isWater(WrapSignedWord(impact.posX + dx * 25), WrapSignedWord(impact.posZ + dz * 25)))
                {
                    impact.posX = WrapSignedWord(impact.posX - dx * 25);
                    impact.posZ = WrapSignedWord(impact.posZ - dz * 25);
                }
            }
        }
        PlaySoundEffect(static_cast<SoundEffect>(43), GetSoundVolumeAt(x, z), -7, 2);
        impact.modelId = 39;
        impact.posY = GetFloorHeight(impact.posX, 0, impact.posZ);
        impact.modelAnim = 4;
        impact.animFrame = 1;
        impact.modelFrame2 = 2;
        impact.health = 1000;
        impact.displayType = DisplayType::VectorNormal;
        impact.winY = static_cast<std::int16_t>(Random(512));
        impact.behavior = BehaviorType::WaterImpact;
        impact.itemValue = 3;
        impact.renderPlacementMode = 3;
        if ((GetCollisionMapPixelAtWorldXZ(impact.posX, impact.posZ) & 7) == 3)
        {
            impact.displayType = DisplayType::VectorBrightness1;
        }
        NotifyObjectCreated(impact);
    }

    std::uint16_t GameContext::SpawnFireParticle(const RawGameObject& source)
    {
        const auto x = source.posX;
        const auto y = WrapSignedWord(source.posY + source.collisionRadiusY);
        const auto z = source.posZ;
        const auto slot = FindFreeObjectIndex();
        // Allocation can clear the source itself after its lifetime reaches zero.
        if (source.collisionType == CollisionType::None) { return 0; }
        if (source.collisionType != CollisionType::DamageObj)
        {
            throw std::logic_error("Only the fire branch of the native fragment factory is implemented.");
        }
        auto& particle = worldObjects[slot];
        particle.modelId = 39;
        particle.modelAnim = 3;
        particle.animFrame = 5;
        particle.modelFrame2 = 2;
        particle.blendWeightOrSpriteOpacity = 100;
        particle.renderPlacementMode = 130;
        particle.velocityY = static_cast<std::int16_t>(Random(500) + 600);
        particle.behavior = BehaviorType::FireParticle;
        particle.generalPurpose1 = Random(4);
        particle.health = static_cast<std::int16_t>(Random(60) + 50);
        particle.displayType = DisplayType::VectorUnlit;
        const auto acceleration = RandomSigned(30) + particle.velocityY / particle.health;
        // Arithmetic right shift rounds negative odd values down, unlike signed division.
        const auto half = acceleration >= 0 ? acceleration / 2 : -((-acceleration + 1) / 2);
        particle.doorLockOrGroupBitmask = static_cast<std::int16_t>(std::clamp(-half, -30, 1));
        particle.posX = x;
        particle.posY = y;
        particle.posZ = z;
        particle.collisionRadiusXZ = 10;
        particle.collisionRadiusY = 10;
        particle.winX = static_cast<std::int16_t>(Random(512));
        particle.winY = static_cast<std::int16_t>(Random(512));
        NotifyObjectCreated(particle);
        return slot;
    }

    void GameContext::InitializeDriftingParticle(RawGameObject& object, std::uint8_t frame,
        std::int16_t x, std::int16_t z)
    {
        const bool active = object.health > 0;
        object.health = 50;
        object.modelId = 39;
        object.modelAnim = 3;
        object.animFrame = frame;
        object.modelFrame2 = 1;
        object.blendWeightOrSpriteOpacity = 100;
        object.displayType = DisplayType::Type30;
        object.behavior = BehaviorType::DriftingParticle;
        object.collisionType = CollisionType::None;
        object.velocityY = 1;
        // The old position chooses the drift direction before X/Z are replaced.
        object.velocityX = SignWord(WrapSignedWord(object.posX - x));
        object.velocityZ = SignWord(WrapSignedWord(object.posZ - z));
        object.posX = x;
        object.posZ = z;
        object.winX = static_cast<std::int16_t>(Random(512));
        object.winY = static_cast<std::int16_t>(Random(512));
        if (!active) { NotifyObjectCreated(object); }
    }

    void GameContext::InitializeSmokeParticle(RawGameObject& object, std::int16_t x, std::int16_t y,
        std::int16_t z, std::int16_t radiusXZ, std::int16_t riseFactor, std::uint8_t placement)
    {
        const bool active = object.health > 0;
        object.modelId = 1;
        object.modelAnim = 2;
        object.animFrame = 3;
        object.behavior = BehaviorType::SmokeFading;
        object.displayType = DisplayType::SpriteTransparent;
        object.blendWeightOrSpriteOpacity = 10;
        object.winY = static_cast<std::int16_t>(Random(2));
        object.collisionRadiusXZ = radiusXZ;
        object.collisionRadiusY = 5;
        object.velocityZ = 2;
        const auto health = WrapSignedWord(riseFactor * 50 + Random(50));
        if (active && health <= 0) { SetObjectHealth(object, health); }
        else { object.health = health; }
        const auto size = static_cast<std::uint8_t>(Random(
            static_cast<std::uint16_t>(object.collisionRadiusXZ)) + 10);
        object.spriteScale = size;
        object.velocityY = WrapSignedWord(riseFactor * 200 + Random(130));
        object.posX = x;
        object.posY = y;
        object.posZ = z;
        object.renderPlacementMode = placement;
        const auto floor = GetFloorHeight(object.posX, object.posY, object.posZ);
        if (object.posY < floor) { object.posY = floor; }
        if (!active) { NotifyObjectCreated(object); }
    }

    void GameContext::RefreshWeaponInventory()
    {
        if (inventory[1].collisionType == CollisionType::Weapon && inventory[1].health == 1000)
        {
            requestedWeaponHand = 2;
        }
        else if (inventory[0].collisionType == CollisionType::Weapon && inventory[0].health == 1000)
        {
            requestedWeaponHand = 1;
        }
        weaponInventorySlots.fill(0);
        for (std::size_t index = 0; index < inventory.size(); ++index)
        {
            const auto& item = inventory[index];
            if (item.collisionType != CollisionType::Weapon || item.health != 1000 || item.itemValue <= 0)
            {
                continue;
            }
            if (static_cast<std::size_t>(item.itemValue) >= weaponInventorySlots.size())
            {
                throw std::out_of_range("Inventory weapon ID must fit the native 32-entry registry.");
            }
            weaponInventorySlots[static_cast<std::size_t>(item.itemValue)] =
                static_cast<std::uint16_t>(index + 1);
        }
    }

    void GameContext::UpdateWeaponSelection()
    {
        if (positionalSoundsSuppressed) { return; }
        requestedWeaponHand = 0;
        for (int hand = 2; hand >= 1; --hand)
        {
            const auto& item = inventory[static_cast<std::size_t>(hand - 1)];
            if (item.collisionType != CollisionType::Weapon || item.health != 1000) { continue; }
            if (item.itemValue < 0 || static_cast<std::size_t>(item.itemValue) >= weaponAmounts.size())
            {
                throw std::out_of_range("Held weapon ID must fit the native 32-entry amount table.");
            }
            requestedWeaponHand = static_cast<std::uint8_t>(hand);
            break;
        }
        const auto next = requestedWeaponHand == 0 ? WeaponType::None :
            static_cast<WeaponType>(inventory[requestedWeaponHand - 1].itemValue);
        if (requestedWeapon != next)
        {
            weaponAnimationSpeed = -1;
            requestedWeapon = next;
        }
    }

    void GameContext::CommitWeaponSelection()
    {
        const auto index = static_cast<std::size_t>(requestedWeapon);
        if (index >= weaponAmounts.size()) { throw std::out_of_range("Invalid requested weapon ID."); }
        weaponAnimationSpeed = -2;
        selectedWeapon = requestedWeapon;
        weaponSpriteFrame = 0;
        weaponHand = requestedWeaponHand;
        weaponReady = weaponAmounts[index] > 0;
    }

    bool GameContext::SelectWeapon(WeaponType weapon)
    {
        const auto index = static_cast<std::size_t>(weapon);
        if (index >= weaponAmounts.size()) { throw std::out_of_range("Invalid weapon selection ID."); }
        if (weaponAnimationSpeed != 0 || selectedWeapon == weapon ||
            weaponInventorySlots[index] == 0 ||
            (weaponAmounts[index] <= 0 && weapon != WeaponType::Scythe))
        {
            return false;
        }
        const auto slot = weaponInventorySlots[index];
        if (slot > inventory.size()) { throw std::out_of_range("Weapon registry slot must be in 1..14."); }
        SwapInventoryItems(1, slot - 1);
        requestedWeapon = weapon;
        requestedWeaponHand = 2;
        weaponAnimationSpeed = -1;
        return true;
    }

    void GameContext::InitializeWeaponCollider(std::uint16_t slot, std::int16_t reach,
        std::int16_t yOffset, std::uint8_t damage, std::int16_t radiusXZ,
        std::int16_t radiusY, std::int16_t lifetime)
    {
        if (slot == 0) { return; }
        if (slot >= worldObjects.size()) { throw std::out_of_range("Weapon collider slot exceeds 165."); }
        auto& object = worldObjects[slot];
        object.health = lifetime;
        object.collisionRadiusXZ = radiusXZ;
        object.collisionRadiusY = radiusY;
        object.collisionMask = 254;
        object.behaviorParameter = damage;
        object.behavior = BehaviorType::WeaponDamageCollider;
        const auto displacement = [reach](std::int16_t direction)
        {
            const auto result = WrapSignedWord(direction * reach) / 100;
            if (result < -128 || result > 127)
            {
                throw std::overflow_error("Weapon contact displacement overflows native signed-byte division.");
            }
            return result;
        };
        const auto camera = GetPlayerCamera();
        object.posX = WrapSignedWord(camera.position[0] + displacement(worldObjects[1].velocityX));
        const auto look = WrapSignedWord(playerLookHeight - 90);
        const auto pitchOffset = look >= 0 ? look / 4 : (static_cast<int>(look) - 3) / 4;
        object.posY = WrapSignedWord(camera.position[1] + yOffset + pitchOffset);
        object.posZ = WrapSignedWord(camera.position[2] + displacement(worldObjects[1].velocityZ));
        if (debugWeaponColliders)
        {
            object.modelId = 1;
            object.modelAnim = 1;
            object.displayType = DisplayType::SpriteTransparent;
            object.renderPlacementMode = 64;
        }
        NotifyObjectCreated(object);
    }

    WeaponActionStatus GameContext::AttackWithSelectedWeapon()
    {
        if (weaponAnimationSpeed != 0 || !weaponReady || positionalSoundsSuppressed)
        {
            return WeaponActionStatus::NoEffect;
        }
        if (selectedWeapon > WeaponType::Showel &&
            selectedWeapon != WeaponType::Lentils && selectedWeapon != WeaponType::Scythe)
        {
            return WeaponActionStatus::UnimplementedWeapon;
        }
        const auto index = static_cast<std::size_t>(selectedWeapon);
        weaponReady = weaponAmounts[index] > 0;
        if (playerUnderwater && selectedWeapon > WeaponType::Dagger) { return WeaponActionStatus::NoEffect; }
        const bool usesHeldItem = selectedWeapon == WeaponType::Fireball ||
            selectedWeapon == WeaponType::Stone || selectedWeapon == WeaponType::TrapOrPole ||
            selectedWeapon == WeaponType::Lentils;
        if (usesHeldItem && (weaponHand == 0 || weaponHand > inventory.size()))
        {
            throw std::out_of_range("This weapon requires an inventory hand in 1..14.");
        }
        if (selectedWeapon == WeaponType::TrapOrPole &&
            inventory[weaponHand - 1].behavior != BehaviorType::BearTrapAmmo &&
            inventory[weaponHand - 1].behavior != BehaviorType::MetalPole)
        {
            return WeaponActionStatus::UnimplementedWeapon;
        }
        if (selectedWeapon == WeaponType::Stone && inventory[weaponHand - 1].modelId < 30)
        {
            GetSpriteBottom(inventory[weaponHand - 1]);
        }
        if (selectedWeapon == WeaponType::Lentils && inventory[weaponHand - 1].modelId < 30)
        {
            auto thrown = inventory[weaponHand - 1];
            thrown.animFrame = 2;
            GetSpriteBottom(thrown);
        }
        const auto oldAmount = weaponAmounts[index];
        weaponAmounts[index] = WrapSignedWord(weaponAmounts[index] - 1);
        weaponReady = weaponAmounts[index] > 0;
        const auto slot = FindFreeOrLowestHealthObjectIndex();
        weaponAnimationSpeed = -3;
        if (selectedWeapon == WeaponType::Dagger)
        {
            InitializeWeaponCollider(slot, 40, -12, 8, 18, 8, 10);
        }
        auto& object = worldObjects[slot];
        const auto camera = GetPlayerCamera();
        const auto& player = worldObjects[1];
        if (selectedWeapon == WeaponType::Crossbow)
        {
            PlaySoundEffect(static_cast<SoundEffect>(7), GetSoundVolume(0), object, 1);
            object.displayType = DisplayType::VectorNormal;
            object.groupIndexOrLock = object.doorLockOrGroupBitmask = 1;
            object.collisionType = CollisionType::Ammo;
            object.itemValue = 3;
            object.collisionRadiusXZ = object.collisionRadiusY = 1;
            object.behavior = BehaviorType::CrossbowBolt;
            object.collisionMask = 126;
            object.renderPlacementMode = 1;
            object.modelId = 31; object.animFrame = 2; object.modelAnim = 1;
            object.winY = WrapSignedWord(512 - GetPlayerHeading());
            object.health = 1000;
            object.velocityX = static_cast<std::int16_t>(player.velocityX / 4);
            object.velocityZ = static_cast<std::int16_t>(player.velocityZ / 4);
            object.winX = WrapSignedWord(11 + WrapSignedWord(playerLookHeight - 90) / 2);
            object.posX = camera.position[0];
            object.posY = WrapSignedWord(camera.position[1] - 20);
            object.posZ = camera.position[2];
            for (int step = 1; step <= 6; ++step)
            {
                if ((GetCollisionMapPixelAtWorldXZ(
                    WrapSignedWord(object.posX + ArithmeticShiftRight(WrapSignedWord(step * object.velocityX), 1)),
                    WrapSignedWord(object.posZ + ArithmeticShiftRight(WrapSignedWord(step * object.velocityZ), 1))) & 8) != 0)
                {
                    object.velocityX = object.velocityZ = 0;
                    object.generalPurpose1 = 1;
                }
            }
        }
        else if (selectedWeapon == WeaponType::Fireball)
        {
            inventory[weaponHand - 1].generalPurpose1 = static_cast<std::uint16_t>(weaponAmounts[4]);
            object.displayType = DisplayType::SpriteTransparent;
            object.blendWeightOrSpriteOpacity = 10;
            object.generalPurpose2 = 100;
            object.collisionRadiusXZ = object.collisionRadiusY = 8;
            object.collisionType = CollisionType::Type_22;
            object.behavior = BehaviorType::FireballProjectile;
            object.collisionMask = 126; object.renderPlacementMode = 1;
            object.modelId = 6; object.animFrame = 1; object.modelAnim = 3;
            object.health = 38; object.spriteScale = 45; object.behaviorParameter = 50;
            object.velocityX = static_cast<std::int16_t>(player.velocityX / 4);
            object.velocityZ = static_cast<std::int16_t>(player.velocityZ / 4);
            object.velocityY = static_cast<std::int16_t>(std::max(-4, WrapSignedWord(playerLookHeight - 110) / 7));
            object.posX = camera.position[0]; object.posY = WrapSignedWord(camera.position[1] - 20);
            object.posZ = camera.position[2];
        }
        else if (selectedWeapon == WeaponType::Stone || selectedWeapon == WeaponType::TrapOrPole)
        {
            if (selectedWeapon == WeaponType::TrapOrPole &&
                (GetCollisionMapPixelAtWorldXZ(camera.position[0], camera.position[2]) & 7) == 1)
            {
                weaponAnimationSpeed = 0;
            }
            else
            {
                auto& held = inventory[weaponHand - 1];
                object = held;
                held.health = 0;
                if (selectedWeapon == WeaponType::Stone)
                {
                    for (std::size_t next = 0; next < inventory.size(); ++next)
                    {
                        auto& candidate = inventory[next];
                        if (next != static_cast<std::size_t>(weaponHand - 1) && held.health == 0 &&
                            candidate.health == 1000 && candidate.collisionType == CollisionType::Weapon &&
                            candidate.itemValue == 5)
                        {
                            held = candidate;
                            candidate.health = 0;
                        }
                    }
                    object.winY = playerTargetHeading;
                    object.collisionRadiusXZ = 10;
                    object.behavior = BehaviorType::BouncingObject;
                    object.itemValue = 60; object.groupIndexOrLock = 300;
                    object.pickupType = PickupType::None; object.generalPurpose1 = 6; object.collisionMask = 126;
                    object.soundEffectId = 41;
                    object.velocityX = static_cast<std::int16_t>(player.velocityX / 6);
                    object.velocityZ = static_cast<std::int16_t>(player.velocityZ / 6);
                    object.velocityY = WrapSignedWord(5 + WrapSignedWord(playerLookHeight - 90) / 12);
                    const auto sign = weaponHand == 1 ? 1 : -1;
                    object.posX = WrapSignedWord(camera.position[0] - object.velocityZ * sign);
                    object.posY = WrapSignedWord(camera.position[1] + 10);
                    object.posZ = WrapSignedWord(camera.position[2] + object.velocityX * sign);
                    object.health = 1000;
                }
                else
                {
                    object.posX = camera.position[0]; object.posY = camera.position[1]; object.posZ = camera.position[2];
                    object.winY = WrapSignedWord(-playerTargetHeading);
                    object.velocityY = -10;
                    object.winX = object.winZ = object.blendWeightOrSpriteOpacity = 0;
                    object.generalPurpose1 = 0;
                }
            }
        }
        else if (selectedWeapon == WeaponType::Lentils)
        {
            auto* thrown = &object;
            for (;;)
            {
                *thrown = inventory[weaponHand - 1];
                thrown->blendWeightOrSpriteOpacity = 10;
                thrown->generalPurpose2 = 100;
                thrown->collisionRadiusXZ = 11;
                thrown->collisionRadiusY = 6;
                thrown->behavior = BehaviorType::BouncingObject;
                thrown->itemValue = 50;
                thrown->generalPurpose1 = 7;
                thrown->groupIndexOrLock = 0;
                thrown->renderPlacementMode = 6;
                thrown->pickupType = PickupType::None;
                thrown->animFrame = 2;
                thrown->health = 200;
                const auto angle = WrapSignedWord(GetPlayerHeading() + RandomSigned(20));
                const auto speed = static_cast<std::int16_t>(Random(5) + 4);
                GetClampedDirectionDisplacement(thrown->velocityX, thrown->velocityZ, angle, speed);
                thrown->velocityZ = WrapSignedWord(-thrown->velocityZ);
                thrown->velocityY = static_cast<std::int16_t>(Random(3) + 3);
                thrown->posX = WrapSignedWord(camera.position[0] + RandomSigned(5));
                thrown->posY = WrapSignedWord(camera.position[1] - 20);
                thrown->posZ = WrapSignedWord(camera.position[2] + RandomSigned(5));
                if (weaponAmounts[20] <= 5)
                {
                    thrown->soundEffectId = 101;
                    NotifyObjectCreated(*thrown);
                    break;
                }
                NotifyObjectCreated(*thrown);
                // Native recurses into A8F7; iterate to preserve large counters without stack growth.
                weaponAmounts[20] = WrapSignedWord(weaponAmounts[20] - 1);
                weaponReady = weaponAmounts[20] > 0;
                thrown = &worldObjects[FindFreeOrLowestHealthObjectIndex()];
            }
        }
        else if (selectedWeapon == WeaponType::Electricity && weaponReady)
        {
            weaponAmounts[7] = static_cast<std::int16_t>(std::clamp(
                static_cast<int>(WrapSignedWord(weaponAmounts[7] - 2 * frameTickCount)), 0, 360));
            std::uint16_t targetSlot = 0;
            int best = 21;
            for (std::uint16_t candidate = 2; candidate <= 165; ++candidate)
            {
                const auto& target = worldObjects[candidate];
                if (target.health <= 0 || target.collisionMask == 0 || target.collisionType <= CollisionType::WallDestruction) { continue; }
                const auto score = std::abs(static_cast<int>(WrapSignedWord(GetSoundDistanceAt(target.posX, target.posZ) - 100)));
                if (score < best) { best = score; targetSlot = candidate; }
            }
            if (targetSlot == 0)
            {
                weaponAnimationSpeed = -4;
                weaponAmounts[7] = oldAmount;
            }
            else
            {
                PlaySoundEffect(static_cast<SoundEffect>(45),
                    GetSoundVolume(WrapSignedWord(200 + Random(200))), object, 1);
                object.posX = WrapSignedWord(camera.position[0] + ArithmeticShiftRight(player.velocityX, 1));
                object.posY = WrapSignedWord(camera.position[1] - 24 +
                    ArithmeticShiftRight(WrapSignedWord(playerLookHeight - 90), 2));
                object.posZ = WrapSignedWord(camera.position[2] + ArithmeticShiftRight(player.velocityZ, 1));
                object.displayType = DisplayType::VectorUnlit; object.renderPlacementMode = 1;
                object.modelId = 31; object.modelAnim = 6;
                object.animFrame = static_cast<std::uint8_t>(Random(3) + 1);
                do { object.modelFrame2 = static_cast<std::uint8_t>(Random(3) + 1); }
                while (object.modelFrame2 == object.animFrame);
                object.blendWeightOrSpriteOpacity = -40;
                object.health = WrapSignedWord(frameTickCount + 2);
                auto& target = worldObjects[targetSlot];
                const auto dx = WrapSignedWord(object.posX - target.posX);
                const auto dz = WrapSignedWord(object.posZ - target.posZ);
                const auto squared = WrapSignedWord(WrapSignedWord(dx * dx) + WrapSignedWord(dz * dz));
                if (squared < 0) { throw std::domain_error("Electric beam squared distance overflows the native square-root domain."); }
                object.winY = GetVectorAngle(dx, dz);
                object.winX = GetVectorAngle(WrapSignedWord(target.posY - object.posY),
                    WrapSignedWord(-static_cast<int>(std::lround(std::sqrt(static_cast<double>(squared))))));
                NotifyObjectCreated(object);
                DamageObject(target, WrapSignedWord(frameTickCount + 1));
                auto& impact = worldObjects[FindFreeObjectIndex()];
                impact.posX = target.posX; impact.posY = target.posY; impact.posZ = target.posZ;
                impact.modelId = 1; impact.modelAnim = 2; impact.animFrame = 2;
                impact.displayType = DisplayType::SpriteTorchlight;
                impact.spriteScale = static_cast<std::uint8_t>(Random(30) + 10);
                impact.health = WrapSignedWord(frameTickCount + 1);
                impact.behavior = BehaviorType::LightSource;
                NotifyObjectCreated(impact);
            }
        }
        if (selectedWeapon != WeaponType::Dagger && selectedWeapon != WeaponType::Lentils &&
            selectedWeapon != WeaponType::Electricity)
        {
            NotifyObjectCreated(object);
        }
        // Sword/unarmed input still clears a reclaiming allocation; their contact is delayed.
        RefreshWeaponInventory();
        return WeaponActionStatus::Applied;
    }

    std::int16_t GameContext::GetSpriteBottom(const RawGameObject&) const
    {
        throw std::runtime_error("Sprite projectile physics requires offsetY and imageHeight from the host's sprite assets.");
    }

    WeaponActionStatus GameContext::UpdateWeaponAnimation(std::int16_t ticks)
    {
        if (ticks < 0) { throw std::invalid_argument("Weapon animation ticks must be nonnegative."); }
        if (worldObjects[1].itemValue > 0) { return WeaponActionStatus::NoEffect; }
        const auto decay = [&]
        {
            if (weaponAnimationSpeed > 0)
            {
                weaponAnimationSpeed = WrapSignedWord(weaponAnimationSpeed +
                    GetStepToward(weaponAnimationSpeed, 0, ticks));
            }
        };
        if (selectedWeapon == WeaponType::None)
        {
            if (field_D500)
            {
                weaponSpriteFrame = 1;
                weaponAmounts[0] = 100;
                if (weaponAnimationSpeed == -1 || weaponAnimationSpeed == -2) { weaponAnimationSpeed = 0; }
                else if (weaponAnimationSpeed == -3) { weaponAnimationSpeed = 14; }
                decay();
                if (weaponAnimationSpeed >= 11 && weaponAnimationSpeed <= 20)
                {
                    weaponAnimationPosition = WrapSignedWord(70 - (weaponAnimationSpeed - 11) * 22);
                }
                else if (weaponAnimationSpeed >= 0 && weaponAnimationSpeed <= 10)
                {
                    weaponAnimationPosition = WrapSignedWord(weaponAnimationSpeed * 6 + 10);
                }
                else if (selectedWeapon != requestedWeapon)
                {
                    CommitWeaponSelection();
                    return WeaponActionStatus::Applied;
                }
                if (weaponAnimationSpeed <= 10 && WrapSignedWord(weaponAnimationSpeed + ticks) > 10)
                {
                    InitializeWeaponCollider(FindFreeObjectIndex(), 30, -10, 4, 10, 10, 3);
                }
            }
        }
        else if (selectedWeapon == WeaponType::Dagger)
        {
            weaponSpriteFrame = 1;
            weaponAmounts[1] = 100;
            weaponReady = true;
            const auto phase = weaponAnimationSpeed;
            if (phase == -3) { weaponAnimationSpeed = 40; }
            else if (phase == -1) { weaponAnimationSpeed = 20; }
            else if (phase == -2) { weaponAnimationPosition = 1; weaponAnimationSpeed = 6; }
            else if (phase == 0) { weaponAnimationPosition = 10; }
            else if (phase >= 1 && phase <= 5) { weaponAnimationPosition = WrapSignedWord(10 - phase * 5); }
            else if (phase >= 15 && phase <= 20) { weaponAnimationPosition = WrapSignedWord((phase - 15) * 5); }
            else if (phase >= 21 && phase <= 30) { weaponAnimationSpeed = 0; }
            else if (phase >= 31 && phase <= 40) { weaponAnimationPosition = WrapSignedWord((phase - 31) * 6 + 10); }
            else
            {
                weaponAnimationPosition = 1;
                if (selectedWeapon != requestedWeapon)
                {
                    CommitWeaponSelection();
                    return WeaponActionStatus::Applied;
                }
            }
            decay();
        }
        else if (selectedWeapon == WeaponType::Sword)
        {
            for (int tick = 0; tick < ticks; ++tick)
            {
                weaponAmounts[2] = 100;
                weaponReady = true;
                weaponSpriteFrame = 1;
                switch (weaponAnimationSpeed)
                {
                case 0: weaponAnimationPosition = 20; break;
                case -3: weaponAnimationPosition = 21; weaponAnimationSpeed = 20; break;
                case -2: weaponAnimationPosition = 10; weaponAnimationSpeed = 10; break;
                case -1: weaponAnimationPosition = 0; weaponAnimationSpeed = 1010; break;
                case -4: weaponAnimationPosition = 0; weaponAnimationSpeed = 0; break;
                default:
                    if (weaponAnimationSpeed >= 900 && weaponAnimationSpeed <= 1000 &&
                        selectedWeapon != requestedWeapon)
                    {
                        CommitWeaponSelection();
                        return WeaponActionStatus::Applied;
                    }
                    break;
                }
                if (weaponAnimationSpeed > 0) { --weaponAnimationSpeed; }
                if (weaponAnimationPosition != 20) { weaponAnimationPosition = WrapSignedWord(weaponAnimationPosition + 1); }
                if (weaponAnimationPosition == 32 && weaponReady)
                {
                    PlaySoundEffect(static_cast<SoundEffect>(19), GetSoundVolume(0), -3, 2);
                    InitializeWeaponCollider(FindFreeObjectIndex(), 45, -10, 20, 22, 12, 10);
                    weaponReady = false;
                }
                if (weaponAnimationPosition >= 40)
                {
                    weaponAnimationSpeed = -2;
                    weaponAnimationPosition = 10;
                }
            }
        }
        else if (selectedWeapon == WeaponType::Crossbow)
        {
            const auto phase = weaponAnimationSpeed;
            if (phase == -3) { weaponAnimationSpeed = weaponAmounts[3] > 0 ? 50 : 0; weaponReady = false; }
            else if (phase == -1) { weaponAnimationSpeed = 50; }
            else if (phase == -2)
            {
                weaponAnimationPosition = 0; weaponAnimationSpeed = 20;
                weaponReady = weaponAmounts[3] > 0;
            }
            else if (phase == 0)
            {
                if (!weaponReady && weaponAmounts[3] > 0) { weaponAnimationSpeed = 50; }
                weaponAnimationPosition = 30;
            }
            else if (phase >= 1 && phase <= 10) { weaponAnimationPosition = WrapSignedWord(30 - 3 * phase); }
            else if (phase >= 30 && phase <= 50) { weaponAnimationPosition = WrapSignedWord(3 * (phase - 40)); }
            else if (phase >= 11 && phase <= 29)
            {
                weaponReady = weaponAmounts[3] > 0;
                weaponAnimationPosition = 1;
                if (selectedWeapon != requestedWeapon) { CommitWeaponSelection(); return WeaponActionStatus::Applied; }
            }
            weaponSpriteFrame = weaponReady ? 2 : 1;
            decay();
        }
        else if (selectedWeapon == WeaponType::Fireball || selectedWeapon == WeaponType::Lentils)
        {
            if (weaponHand == 0 || weaponHand > inventory.size())
            {
                throw std::out_of_range("Casting animation requires an inventory hand in 1..14.");
            }
            auto& held = inventory[weaponHand - 1];
            const auto index = static_cast<std::size_t>(selectedWeapon);
            if (held.itemValue == 4) { weaponAmounts[index] = WrapSignedWord(held.generalPurpose1); }
            weaponReady = weaponAmounts[index] > 0 || held.itemValue != static_cast<std::int16_t>(selectedWeapon);
            if (!weaponReady) { held.health = 0; RefreshWeaponInventory(); }
            if (weaponAnimationSpeed == -1) { weaponAnimationSpeed = 220; }
            else if (weaponAnimationSpeed == -2) { weaponAnimationSpeed = 20; }
            else if (weaponAnimationSpeed == -3) { weaponAnimationSpeed = 120; }
            const auto phase = weaponAnimationSpeed;
            weaponSpriteFrame = 1;
            if (phase >= 201 && phase <= 250) { weaponAnimationPosition = WrapSignedWord(3 * (phase - 201)); }
            else if (phase >= 0 && phase <= 20) { weaponAnimationPosition = WrapSignedWord(3 * (20 - phase)); }
            else if (phase >= 150 && phase <= 200)
            {
                if (selectedWeapon != requestedWeapon) { CommitWeaponSelection(); return WeaponActionStatus::Applied; }
                weaponAnimationSpeed = 0;
            }
            else if (phase >= 30 && phase <= 60) { weaponAnimationSpeed = 0; }
            else if ((phase >= 71 && phase <= 80) || (phase >= 91 && phase <= 100)) { weaponSpriteFrame = 2; }
            else if (phase >= 81 && phase <= 90) { weaponSpriteFrame = 3; }
            weaponAmounts[20] = 12;
            if (weaponAnimationSpeed > 0)
            {
                weaponAnimationSpeed = WrapSignedWord(weaponAnimationSpeed +
                    GetStepToward(weaponAnimationSpeed, 0, WrapSignedWord(2 * ticks)));
            }
        }
        else if (selectedWeapon == WeaponType::Stone)
        {
            if (weaponAnimationSpeed < 0) { weaponAnimationSpeed = 20; }
            decay();
            weaponAnimationPosition = weaponHand == 1 ? 1 : 7;
            weaponAmounts[5] = 100;
        }
        else if (selectedWeapon == WeaponType::TrapOrPole)
        {
            const auto phase = weaponAnimationSpeed;
            if (phase == 0)
            {
                if (selectedWeapon != requestedWeapon) { CommitWeaponSelection(); return WeaponActionStatus::Applied; }
            }
            else if (phase == -2) { weaponAnimationSpeed = 20; }
            else if (phase == -3 || phase == -1) { weaponAnimationSpeed = 110; }
            else if (phase >= 50 && phase <= 100) { weaponAnimationSpeed = 0; }
            else { weaponAnimationSpeed = WrapSignedWord(phase + GetStepToward(phase, 0, ticks)); }
            if (weaponAnimationSpeed >= 1 && weaponAnimationSpeed <= 20) { weaponAnimationPosition = weaponAnimationSpeed; }
            else if (weaponAnimationSpeed >= 99 && weaponAnimationSpeed <= 120)
            {
                weaponAnimationPosition = WrapSignedWord(2 * (110 - weaponAnimationSpeed));
            }
            weaponAmounts[6] = 100;
            weaponReady = true;
        }
        else if (selectedWeapon == WeaponType::Electricity)
        {
            weaponSpriteFrame = 1; weaponReady = true; weaponAmounts[23] = 100;
            weaponAnimationPosition = 0;
            if (weaponAnimationSpeed == -4 || weaponAnimationSpeed == -3) { weaponAnimationSpeed = 0; }
            else if (weaponAnimationSpeed == -2) { weaponAnimationSpeed = 200; }
            else if (weaponAnimationSpeed == -1) { weaponAnimationSpeed = 100; }
            const auto phase = weaponAnimationSpeed;
            if (phase >= 85 && phase <= 100) { weaponAnimationPosition = WrapSignedWord(phase - 85); }
            else if (phase >= 2 && phase <= 100)
            {
                if (selectedWeapon != requestedWeapon) { CommitWeaponSelection(); return WeaponActionStatus::Applied; }
            }
            else if (phase >= 185 && phase <= 200) { weaponAnimationPosition = WrapSignedWord(200 - phase); }
            else if (phase >= 102 && phase <= 200) { weaponAnimationSpeed = 0; }
            decay();
            if (weaponAnimationSpeed == 0) { weaponAnimationPosition = 15; }
            weaponAmounts[7] = static_cast<std::int16_t>(std::clamp(static_cast<int>(weaponAmounts[7]), 0, 360));
        }
        else if (selectedWeapon == WeaponType::Showel || selectedWeapon == WeaponType::Scythe)
        {
            for (int tick = 0; tick < ticks; ++tick)
            {
                weaponAmounts[8] = 100; weaponReady = true;
                weaponSpriteFrame = weaponAnimationRecoil == 0 ? 1 : 2;
                switch (weaponAnimationSpeed)
                {
                case 0: weaponAnimationPosition = 10; break;
                case -3: weaponAnimationRecoil = 0; weaponAnimationPosition = 11; weaponAnimationSpeed = 30; break;
                case -2: weaponAnimationRecoil = 0; weaponAnimationPosition = 0; weaponAnimationSpeed = 10; break;
                case -1: weaponAnimationRecoil = 0; weaponAnimationPosition = -10; weaponAnimationSpeed = 1010; break;
                default:
                    if (weaponAnimationSpeed >= 900 && weaponAnimationSpeed <= 1000 && selectedWeapon != requestedWeapon)
                    {
                        CommitWeaponSelection(); return WeaponActionStatus::Applied;
                    }
                    break;
                }
                if (weaponAnimationSpeed > 0) { --weaponAnimationSpeed; }
                weaponAnimationPosition = WrapSignedWord(weaponAnimationPosition + 1);
                if (weaponAnimationPosition >= 38) { weaponAnimationSpeed = -2; weaponAnimationPosition = 0; }
                if (weaponAnimationPosition > 30) { weaponAnimationRecoil = 0; }
                if (weaponAnimationPosition == 17 && weaponReady)
                {
                    PlaySoundEffect(static_cast<SoundEffect>(19), GetSoundVolume(500), -3, 2);
                    InitializeWeaponCollider(FindFreeObjectIndex(), 55, -35, 5, 15, 6, 10);
                    weaponReady = false;
                }
            }
        }
        else { return WeaponActionStatus::UnimplementedWeapon; }
        if (weaponAnimationSpeed == 0 &&
            (weaponHand != requestedWeaponHand || selectedWeapon != requestedWeapon))
        {
            CommitWeaponSelection();
        }
        return WeaponActionStatus::Applied;
    }

    std::uint16_t GameContext::FindFreeObjectIndex()
    {
        std::uint16_t slot = 2;
        while (slot < 165 && worldObjects[slot].health != 0) { ++slot; }
        ReplaceWorldObject(slot, {});
        return slot;
    }

    std::uint16_t GameContext::FindFreeOrLowestHealthObjectIndex()
    {
        std::uint16_t lowestSlot = 165;
        std::int16_t lowestHealth = 1000;
        std::uint16_t slot = 2;
        for (; slot <= 164; ++slot)
        {
            const auto health = worldObjects[slot].health;
            if (health < lowestHealth)
            {
                lowestHealth = health;
                lowestSlot = slot;
            }
            if (health == 0) { break; }
        }
        if (slot > 164) { slot = lowestSlot; }
        ReplaceWorldObject(slot, {});
        return slot;
    }

    std::uint8_t GameContext::GetCollisionMapPixelAtWorldXZ(std::int16_t x, std::int16_t z) const
    {
        const auto column = static_cast<std::int16_t>(WrapSignedWord(static_cast<std::int32_t>(x) - 112) / 25);
        const auto row = WrapSignedWord(162 - WrapSignedWord(static_cast<std::int32_t>(z) - 38) / 25);
        return GetCollisionMapPixel(column, row);
    }

    std::uint8_t GameContext::GetCollisionMapPixel(std::int16_t column, std::int16_t row) const
    {
        if (!collisionMap)
        {
            throw std::runtime_error("GetCollisionMapPixelAtWorldXZ requires collision-map memory (DS:69C4).");
        }
        if (column < 0 || column > 319 || row < 0 || row > 239)
        {
            return 0;
        }
        const auto& map = *collisionMap;
        const auto stride = static_cast<std::uint16_t>(map.widthMinusOne + 1u);
        const auto address = static_cast<std::uint16_t>(offsetof(CollisionMap, data) + row * stride + column);
        if (address >= sizeof(CollisionMap))
        {
            throw std::out_of_range("Collision-map read is outside the allocated dummy map.");
        }
        // Wrapped offsets can address the header as well as pixel data.
        const auto* bytes = reinterpret_cast<const unsigned char*>(&map);
        return bytes[address];
    }

    void GameContext::QueryCollisionMapNeighborhood(std::int16_t x, std::int16_t z, std::int16_t step,
        std::int16_t& correctionX, std::int16_t& correctionZ, std::uint16_t& center, std::uint8_t mask) const
    {
        const auto column = WrapSignedWord(x - 112) / 25;
        const auto zPixel = WrapSignedWord(z - 38) / 25;
        for (int dx = -1; dx <= 1; ++dx)
        {
            for (int dz = -1; dz <= 1; ++dz)
            {
                const auto sample = GetCollisionMapPixel(WrapSignedWord(column + dx),
                    WrapSignedWord(162 - WrapSignedWord(zPixel + dz))) & mask;
                if (dx == 0 && dz == 0) { center = static_cast<std::uint16_t>(sample); }
                if (sample > 0)
                {
                    correctionX = WrapSignedWord(correctionX - dx * step);
                    correctionZ = WrapSignedWord(correctionZ - dz * step);
                }
            }
        }
        correctionX = WrapSignedWord(SignWord(correctionX) * step);
        correctionZ = WrapSignedWord(SignWord(correctionZ) * step);
    }

    void GameContext::HandleBlockedObject(RawGameObject& object)
    {
        if (discardBlockedObjects)
        {
            SetObjectHealth(object, 0);
            return;
        }
        const auto found = std::find_if(worldObjects.begin(), worldObjects.end(),
            [&object](const RawGameObject& candidate) { return &candidate == &object; });
        if (found == worldObjects.end())
        {
            throw std::invalid_argument("Blocked-object diagnostics require a world-object slot.");
        }
        object.behavior = BehaviorType::None;
        object.blendWeightOrSpriteOpacity = -100;
        object.posY = WrapSignedWord(object.posY + 50);
        PromptUserMessage("WC-Error Obj " + std::to_string(found - worldObjects.begin()), 50);
    }

    void GameContext::SetCollisionMapPixelAtWorldXZ(
        std::int16_t x, std::int16_t z, std::uint16_t value)
    {
        if (!collisionMap)
        {
            throw std::runtime_error("SetCollisionMapPixelAtWorldXZ requires collision-map memory (DS:69C4).");
        }
        auto& map = *collisionMap;
        const auto stride = static_cast<std::uint16_t>(map.widthMinusOne + 1u);
        const auto column = WrapSignedWord(static_cast<std::int32_t>(x) - 112) / 25;
        const auto row = WrapSignedWord(162 - WrapSignedWord(static_cast<std::int32_t>(z) - 38) / 25);
        const auto index = static_cast<std::uint16_t>(row * stride + column);
        constexpr auto dataOffset = offsetof(CollisionMap, data);
        const auto address = static_cast<std::uint16_t>(dataOffset + index);
        if (address >= sizeof(CollisionMap))
        {
            throw std::out_of_range("Collision-map write is outside the allocated dummy map.");
        }
        if (address >= dataOffset)
        {
            map.data[address - dataOffset] = static_cast<std::uint8_t>(value);
        }
        else
        {
            // Normalize the far pointer to offset zero; wrapped writes can still hit the header.
            auto* bytes = reinterpret_cast<unsigned char*>(&map);
            bytes[address] = static_cast<std::uint8_t>(value);
        }
    }

    void GameContext::SetDoorCollisionMapAtWorldXZ(
        std::int16_t x, std::int16_t z, std::uint16_t value)
    {
        if (!collisionMap)
        {
            throw std::runtime_error("Door collision updates require collision-map memory.");
        }
        auto& map = *collisionMap;
        auto* bytes = reinterpret_cast<unsigned char*>(&map);
        const int column = WrapSignedWord(static_cast<std::int32_t>(x) - 137) / 25;
        const int centerRow = 162 - WrapSignedWord(static_cast<std::int32_t>(z) - 38) / 25;
        for (int offset = -1; offset <= 1; ++offset)
        {
            // A wrapped write can touch the header, changing the stride for the next row.
            const auto stride = static_cast<std::uint16_t>(map.widthMinusOne + 1u);
            const auto row = WrapSignedWord(centerRow + offset);
            std::array<std::uint16_t, 3> addresses{};
            for (int pixel = 0; pixel < 3; ++pixel)
            {
                addresses[pixel] = static_cast<std::uint16_t>(
                    offsetof(CollisionMap, data) + row * stride + column + pixel);
                if (addresses[pixel] >= sizeof(CollisionMap))
                {
                    throw std::out_of_range("Door collision write is outside the allocated map.");
                }
            }
            for (const auto address : addresses) { bytes[address] = static_cast<std::uint8_t>(value); }
        }
    }

    void GameContext::SetBarrierCollisionMapAtWorldXZ(
        std::int16_t x, std::int16_t z, std::int16_t heading, std::uint8_t value)
    {
        if (!collisionMap) { throw std::runtime_error("Barrier collision updates require collision-map memory."); }
        auto& map = *collisionMap;
        const auto column = WrapSignedWord(x - 112) / 25;
        const auto row = WrapSignedWord(162 - WrapSignedWord(z - 38) / 25);
        const bool horizontal = ((static_cast<std::uint16_t>(heading - 64) & 511u) & 128u) != 0;
        for (int offset = -1; offset <= 1; ++offset)
        {
            const auto start = WrapSignedWord(column + (horizontal ? -1 : 0));
            const auto targetRow = WrapSignedWord(row + (horizontal ? 0 : offset));
            const int count = horizontal ? 3 : 1;
            // 10AC:00DE rejects whole spans at the native right/bottom bounds.
            if (start >= 0 && targetRow >= 0 && targetRow < map.heightMinusOne &&
                WrapSignedWord(start + count) <= map.widthMinusOne)
            {
                const auto address = static_cast<std::size_t>(targetRow) * (map.widthMinusOne + 1u) + start;
                if (address + count > std::size(map.data))
                {
                    throw std::out_of_range("Barrier collision span exceeds the allocated map.");
                }
                std::fill_n(map.data + address, count, value);
            }
            if (horizontal) { break; }
        }
    }

    std::int16_t GameContext::GetHeightSurfaceDepth(std::uint16_t surface)
    {
        surface &= 31;
        floorSurfaceValue = -1;
        std::int16_t depth = 1000;
        if (surface >= 1 && surface <= 4)
        {
            if (!level) { throw std::runtime_error("Surface depth requires level information."); }
            const auto& entry = level->heightSurfaces[surface - 1];
            if (!entry)
            {
                throw std::runtime_error("Floor surface slot requires height/value metadata from level information.");
            }
            depth = entry->depth;
            floorSurfaceValue = entry->value;
        }
        return depth;
    }

    std::int16_t GameContext::GetFloorHeight(std::int16_t x, std::int16_t y, std::int16_t z)
    {
        if (!level) { throw std::runtime_error("GetFloorHeight requires level geometry."); }
        const auto position = GetHeightPosition(x, z);
        const auto depth = GetHeightSurfaceDepth(level->surfaceFlags[position.row][position.column]);
        const auto flatFloor = WrapSignedWord(-static_cast<std::int32_t>(depth));
        if (flatFloor >= -900) { return flatFloor; }
        return GetTerrainFloorHeight(x, y, z);
    }

    std::int16_t GameContext::GetTerrainFloorHeight(std::int16_t x, std::int16_t y, std::int16_t z) const
    {
        if (!level) { throw std::runtime_error("Terrain floor lookup requires level geometry."); }
        return GetTerrainHeight(*level, floorRepeatHeight, x, y, z);
    }

    std::int16_t GameContext::QueryTerrainSupport(std::int16_t x, std::int16_t y, std::int16_t z,
        std::int16_t& correctionX, std::int16_t& correctionZ) const
    {
        const auto center = GetTerrainFloorHeight(x, y, z);
        correctionX = 0;
        correctionZ = 0;
        for (int dx = -1; dx <= 1; ++dx)
        {
            for (int dz = -1; dz <= 1; ++dz)
            {
                const auto neighbor = GetTerrainFloorHeight(
                    WrapSignedWord(x + 30 * dx), y, WrapSignedWord(z + 30 * dz));
                const auto difference = WrapSignedWord(center - neighbor);
                if (AbsoluteWord(difference) <= 16) { continue; }
                correctionX = WrapSignedWord(correctionX + SignWord(difference) * dx);
                correctionZ = WrapSignedWord(correctionZ + SignWord(difference) * dz);
            }
        }
        return center;
    }

    std::int16_t GameContext::GetRoomHeight(std::int16_t x, std::int16_t z) const
    {
        if (!level) { throw std::runtime_error("GetRoomHeight requires level geometry."); }
        const auto position = GetHeightPosition(x, z);
        if (level->ceilingObjects[position.row][position.column] == 0) { return 20000; }
        const auto corners = GetCorners(level->roomHeights, position);
        return WrapSignedWord(2 * static_cast<std::int32_t>(InterpolateHeight(corners, position)));
    }

    std::uint16_t GameContext::FindCollidingObject(std::int16_t sourceSlot) const
    {
        if (!objectCollisionsEnabled)
        {
            return 0;
        }
        if (sourceSlot < 0 || static_cast<std::size_t>(sourceSlot) >= worldObjects.size())
        {
            throw std::out_of_range("Collision source slot must be in 0..165.");
        }
        const auto& source = worldObjects[sourceSlot];
        return FindCollidingObject(source);
    }

    std::uint16_t GameContext::FindCollidingObject(const RawGameObject& source) const
    {
        if (!objectCollisionsEnabled) { return 0; }
        for (std::uint16_t slot = 2; slot <= 164; ++slot)
        {
            const auto& candidate = worldObjects[slot];
            if (&candidate != &source && candidate.health > 0 && ObjectsOverlap(source, candidate))
            {
                return slot;
            }
        }
        return 0;
    }

    bool GameContext::OverlapsPlayer(const RawGameObject& source) const
    {
        return objectCollisionsEnabled && ObjectsOverlap(source, worldObjects[1]);
    }

    std::uint16_t GameContext::FindPlayerCollision() const
    {
        const auto first = FindCollidingObject(1);
        if (first == 0)
        {
            return 0;
        }
        auto slot = first;
        do
        {
            ++slot;
            const auto& candidate = worldObjects[slot];
            if (candidate.health > 999 && candidate.pickupType != PickupType::None &&
                ObjectsOverlap(worldObjects[1], candidate))
            {
                return slot;
            }
        } while (slot <= 163);
        return first;
    }

    std::uint16_t GameContext::Random(std::uint16_t range)
    {
        // 2CE4:170A uses the high seed word modulo the range, not multiply-high scaling.
        randomSeed = randomSeed * 0x08088405u + 1u;
        return range == 0 ? 0 : static_cast<std::uint16_t>((randomSeed >> 16) % range);
    }

    double GameContext::RandomReal()
    {
        Random(0);
        // 2CE4:173D takes ABS of the signed seed, then scales by 2^-31.
        const auto signedSeed = randomSeed <= 0x7fffffffu ?
            static_cast<std::int64_t>(randomSeed) : static_cast<std::int64_t>(randomSeed) - 0x100000000ll;
        return std::abs(static_cast<double>(signedSeed)) / 2147483648.0;
    }

    void GameContext::CreateAmbientLoop(std::uint8_t environment)
    {
        if (!audioHandler) { throw std::logic_error("Ambient creation requires an audio handler."); }
        if (environment >= 100 && environment <= 150)
        {
            throw std::runtime_error("Music/cutscene ambient selectors are not implemented.");
        }
        struct Overlay
        {
            std::uint8_t sample;
            std::uint16_t count;
        };
        struct Recipe
        {
            std::uint8_t base;
            std::uint16_t gapChance;
            std::array<Overlay, 3> overlays;
        };
        static constexpr std::array<Recipe, 5> recipes{{
            {46, 0, {{{5, 50}, {38, 50}, {0, 0}}}},
            {30, 0, {{{31, 20}, {0, 0}, {0, 0}}}},
            {28, 13, {{{82, 100}, {29, 10}, {40, 4}}}},
            {28, 4, {{{82, 100}, {44, 4}, {73, 8}}}},
            {62, 0, {{{62, 140}, {63, 140}, {64, 140}}}}
        }};
        const auto& recipe = recipes[environment < recipes.size() ? environment : 0];
        constexpr std::size_t capacity = 1800000;
        constexpr std::size_t targetLength = 1499904;
        const auto sampleLength = [&](std::uint8_t sample)
        {
            if (sample == 0 || sample > audioHandler->GetSampleCount())
            {
                throw std::out_of_range("Ambient recipe references a missing sample.");
            }
            const auto length = audioHandler->GetSampleLength(sample);
            if (length == 0 || length > 64000)
            {
                throw std::invalid_argument("Ambient recipe samples must contain 1..64000 PCM frames.");
            }
            return length;
        };
        const auto baseLength = sampleLength(recipe.base);
        std::array<std::size_t, 3> overlayLengths{};
        for (std::size_t i = 0; i < recipe.overlays.size(); ++i)
        {
            if (recipe.overlays[i].count != 0) { overlayLengths[i] = sampleLength(recipe.overlays[i].sample); }
        }

        audioHandler->BeginAmbientLoopCreation(capacity);
        std::size_t cursor = 0;
        do
        {
            if (recipe.gapChance != 0 && Random(recipe.gapChance) == 0)
            {
                cursor += 4u * Random(10000);
            }
            audioHandler->CopyAmbient(recipe.base, cursor);
            cursor += baseLength;
        } while (cursor < targetLength);
        const auto loopLength = cursor - baseLength;
        for (std::size_t i = 0; i < recipe.overlays.size(); ++i)
        {
            const auto& overlay = recipe.overlays[i];
            if (overlay.count == 0) { continue; }
            const auto maximumOffset = loopLength - overlayLengths[i] - 100;
            for (std::uint16_t occurrence = 0; occurrence < overlay.count; ++occurrence)
            {
                const auto offset = static_cast<std::size_t>(
                    std::nearbyint(RandomReal() * static_cast<double>(maximumOffset))) & ~std::size_t{1};
                const auto weight = static_cast<std::uint8_t>(200 - Random(150));
                audioHandler->MixAmbient(overlay.sample, offset, weight);
            }
        }
        audioHandler->EndAmbientLoopCreation(loopLength);
    }

    void GameContext::LoadSoundEffects(const std::uint8_t* data, std::size_t size)
    {
        if (!data || size != SoundEffectTableSize)
        {
            throw std::invalid_argument("S_TAB.PRB must contain its four-byte header and 129 five-byte records.");
        }
        if (data[0] != SoundEffectCount || data[1] != 0 || data[2] != 5 || data[3] != 0)
        {
            throw std::invalid_argument("Unsupported sound-effect table dimensions.");
        }
        for (std::size_t i = 0; i < SoundEffectCount; ++i)
        {
            if (data[4 + i * 5] > 4)
            {
                throw std::invalid_argument("Sound effects support at most four sample references.");
            }
        }
        std::memcpy(soundEffects.data(), data + 4, SoundEffectCount * sizeof(SoundEffectDefinition));
        soundEffectsLoaded = true;
    }

    std::int16_t GameContext::GetSoundDistanceAt(std::int16_t x, std::int16_t z) const
    {
        return positionalSoundsSuppressed ? static_cast<std::int16_t>(10000) :
            WrapSignedWord(AbsoluteWord(WrapSignedWord(soundListenerX - x)) +
                AbsoluteWord(WrapSignedWord(soundListenerZ - z)));
    }

    std::uint8_t GameContext::GetSoundVolumeAt(
        std::int16_t x, std::int16_t z, std::int16_t distanceScale) const
    {
        const auto distance = GetSoundDistanceAt(x, z);
        const auto scaled = WrapSignedWord(static_cast<std::int32_t>(distance) * distanceScale);
        return GetSoundVolume(scaled);
    }

    std::uint8_t GameContext::GetSoundVolume(std::int16_t distance)
    {
        const auto magnitude = static_cast<std::uint16_t>(AbsoluteWord(distance));
        return static_cast<std::uint8_t>(std::clamp(355 - static_cast<int>(magnitude >> 1), 0, 255));
    }

    SoundPlaybackStatus GameContext::PlaySoundEffect(
        SoundEffect effect, std::uint8_t volume, std::int16_t sourceId, std::int16_t instanceLimit)
    {
        const auto effectId = static_cast<std::uint16_t>(effect);
        if (effectId < 1 || effectId > SoundEffectCount)
        {
            throw std::out_of_range("Sound-effect ID must be in the native range 1..129.");
        }
        if (!soundEffectsLoaded)
        {
            if (!audioHandler) { return SoundPlaybackStatus::AudioUnavailable; }
            throw std::runtime_error("PlaySoundEffect requires the S_TAB.PRB effect table.");
        }
        const auto& definition = soundEffects[effectId - 1];
        if (definition.sampleCount == 0) { return SoundPlaybackStatus::NoSample; }
        if (definition.sampleCount > definition.samples.size())
        {
            throw std::invalid_argument("Sound-effect sample count exceeds its four-reference record.");
        }
        const auto sample = definition.samples[Random(definition.sampleCount)];
        if (!audioHandler) { return SoundPlaybackStatus::AudioUnavailable; }
        if (sample == 0 || sample > audioHandler->GetSampleCount()) { return SoundPlaybackStatus::NoSample; }
        if (volume == 0) { return SoundPlaybackStatus::Inaudible; }
        if (!soundEnabled) { return SoundPlaybackStatus::Disabled; }

        std::size_t freeChannel = SoundChannelCount;
        int freeCount = 0;
        int sameSourceCount = 0;
        for (std::size_t channel = 0; channel < SoundChannelCount; ++channel)
        {
            if (!audioHandler->IsChannelPlaying(channel))
            {
                freeChannel = channel;
                ++freeCount;
            }
            else if (soundChannelSources[channel] == sourceId)
            {
                ++sameSourceCount;
            }
        }
        if (instanceLimit < 0 && freeCount < 2) { return SoundPlaybackStatus::ReservedChannel; }
        if (freeCount == 0) { return SoundPlaybackStatus::NoFreeChannel; }
        if (AbsoluteWord(instanceLimit) <= sameSourceCount) { return SoundPlaybackStatus::SourceLimitReached; }
        audioHandler->PlaySample(freeChannel, sample, volume);
        soundChannelSources[freeChannel] = sourceId;
        return SoundPlaybackStatus::Started;
    }

    SoundPlaybackStatus GameContext::PlaySoundEffect(
        SoundEffect effect, std::uint8_t volume, const RawGameObject& source, std::int16_t instanceLimit)
    {
        for (std::size_t slot = 0; slot < worldObjects.size(); ++slot)
        {
            if (&source == &worldObjects[slot])
            {
                return PlaySoundEffect(effect, volume, static_cast<std::int16_t>(slot), instanceLimit);
            }
        }
        if (!audioHandler) { return PlaySoundEffect(effect, volume, 0, instanceLimit); }
        throw std::invalid_argument("Object sound sources must belong to GameContext::worldObjects.");
    }

    void GameContext::PromptUserMessage(std::string_view, std::int16_t)
    {
    }

    bool GameContext::IsUserMessageActive() const
    {
        return false;
    }
}
