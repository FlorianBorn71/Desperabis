#pragma once
#include <DesperabisCommon.h>

#include <Asset/RawGameObject.h>

namespace GamePlay
{

    enum class DisplayType
    {
        SpriteNormal = 0,
        SpriteTransparent,
        SpriteTorchlight,
        Type3,
        VectorNormal,
        VectorUnlit,
        VectorTorchlight,
        Type7,
        Type8,
        Type9,
        SpritePulsate,
        SpriteTorchlightTransparent,
        DepthSprite,

        // vector shaded?
        VectorBrightness0 = 20,
        VectorBrightness1 = 21,
        VectorBrightness2 = 22, // after burning
        VectorBrightness3 = 23,
        VectorBrightness4 = 24,
        VectorBrightness5 = 25,
        VectorBrightness6 = 26,
        VectorBrightness7 = 27,

        // vector shaded?
        Type30 = 30,
        Type31 = 31,
        Type32 = 32,
        Type33 = 33,
        Type34 = 34,
        Type35 = 35,
        Type36 = 36,
        Type37 = 37, // submarine window

		FirstVectorBurning = 50,
        LastVectorBurning = 120, // ?

        SpriteScissorRect = 250,
        VectorUnderWater = 254, // submarine level - what is this?
        Hidden = 255
    };

	const char* DisplayTypeToString(DisplayType t);
	const char* DisplayTypeToString(DisplayType t, char* buffer);
    bool IsVectorDisplayType(DisplayType display, Color4& tintColor, float& fireIntensity);
    bool IsSpriteDisplayType(DisplayType display);
    bool IsLitSpriteDisplayType(DisplayType display);

    enum class BehaviorType
    {
        None = 0,

        Pillar = 8,
        LightSource = 9,
        Door = 10,
        Frog = 12,
        RatAndSnake = 13,
        BatIndoor = 14,
        IdleAnimation = 15, // for vertex anims, e.g. spider web
        Flies = 16, // a few instances in both forest levels
        BatOutdoor = 17, // Unlike 'Bat' does not collide with walls?
        FireFly = 19,
        BurningFireplace = 22, // e.g. lorry rails
        PulsatingLight = 23, // fireball, but also others
        SmokeFading = 24, // e.g. for fire
        DestroyableBarrier = 26, // e.g. wood in forest
        VerticalConcreteSlab = 27, // level 23 - the one that stands and falls over
        RollingRock = 29, // The ones that roll in submarine level 32, but also the blades in castle level
        Skeleton = 30,
        Cockroach = 31, // also brown spider
        RollingRockSlope = 34,
        Mummie = 35,
        SpiderLarge = 36, // both the black/red and the white ones
        Harvestman = 37, // long legged spiders in level 14
        Devil = 38,
        Gorilla = 39,
        ShootArrows = 40, // level with dragon, but also particle emitter for fountain
        FlyingArrow = 41, // level with dragon, also used for the falling rock debris in boss fight
        MercuryLamp = 42, // level 34
        Scorpion = 43,
        SpearOnGroundTrap = 44, // level with dragon
        WalkablePlanks = 47, // a bridge that you can walk over
        BreakableStoneWallFloodingWater = 48, // the one that triggers FloodingWater in level 26
        WalkableDrawBridge = 49, // Level 23 : the bridge that you can walk over after you triggered a switch
        Switch1 = 50,
        SwitchGrouped = 51, // This one is in level 12 and roof
        RopeSwitch = 52, // The one that hangs in level 23, but also used for endboss legs?! For the animation?
        Switch3 = 53, // The one that rotates in swamp level 24
        UnderwaterPlants = 54, // is this a switch?!
        Switch4 = 56, // The one that rotates in level 31
        MetalPole = 57, // the one to open door in submarine level 32. Also used for the torchlight that you can carry.
        KeyInBucket = 58, // level 33
        Dragon = 60, // only level 15?
        DragonHead = 62, // only level 15?
        DiggableGround = 63,
        Buried = 64,
        Shark = 65,
        Crab = 66,
        DecoFish = 67, // in submarine level 32
        Dog = 68,
        DogLeash = 69, // level 19
        TriggerDebris = 70, // in end level
        FlipTRexHead = 71, // just the deco object in museum
        Ghost = 72,
        LightRayKillsGhost = 73,
        ActivatableLadder = 74, // the one in the hut of swamp level 24
        LiftUp = 76,
        RechargeElectricity = 77,
        Lorry = 78,
        MummieGuard = 79, // final level 26
        BearTrapAmmo = 80,
        FireExhauster = 81,
        BonesToGrab = 83, // the ones in level 34 that appear
        ItemHiddenInBox = 84, // an item that is shown when a box is opened in level 36
        AnancondaHead = 85,
        AnancondaTailSegment = 86,
        AttachableDamageObject = 88, // for Kraken arms inside sewerage. But also used for attached machine parts for RotatingMachineCenter
        KrakenHead = 89, // inside sewerage
        EndBossHead = 91,
        EndBossLeg = 92,
        EndBossFeeler = 93,
        EndBossBody = 94,
        BreakableGround = 95, // In final level 26, where endboss falls down
        Moon = 100,
        WaterDrop = 101,
        UnderwaterColorAnim = 104, // underwater vertex color animation
        LeafParticle1 = 109,
        FogParticle = 110,
        LeafParticle2 = 112, // what is the difference? Both in Level 10
        RoofDoorHandler = 113,
        NeonLights = 116, // in museum
        SwimmingParticles = 117, // underwater with large Octopus
        BurriedVerticalDoor = 119, // level 23
        Teleporter = 120, // various teleporter to jump between map parts in level 10 and 36
        LevelStartOverride = 121, // in cave
        DirtFromAbove = 122, // level 23 - falls from above and extinguishes fire
        DirtFromAboveShutter = 123, // level 23 - falls from above and extinguishes fire
        CeilingConcreteSlab = 124, // level 23 - the one that is carried by the wood and falls from above
        BurnableOilPuddle = 125,
        CeilingConcreteSlabStake = 126, // level 23 - the wood that holds CeilingConcreteSlab
        UnreachableKey = 127, // the key in the hut of swamp level 24
        AttackingBull = 129,
        ExtinguishLava = 130, // in forest
        FloodingWater = 131, // the one in the final level 26
        ClimbableLadder = 133, // e.g. at Octopus
        CurtainOpens = 134, // the curtains that open on proximity in witch-level 90
        Meteor = 135, // The meteor in the meteor cutscene
        SpawnInCrater = 139, // not exactly sure what it is, but it is invisble objects in the meteor crater (many of them)
        RotatingMachineCenter = 140, // e.g. level 22
        Submarine = 141,
        GrasingCow = 142,
        CrackingSubmarineWindow = 144,
        PuzzleGameTile = 145, // level 35
        PuzzleGameHandler = 147,
        PuzzleSpool = 149, // the ones with roman numbers in level 35
        OpeningCreviceWithExit = 150, // the one that opens in level 25
        OpeningCrevice = 152, // the one that opens in level 25
        FlushingFishes = 153, // at the flush for flushing the fish, probably controls the water level
        SomethingWithTheFishes  = 154, // the water in the cave
        GrandfatherClock = 155, // level 35
        GrandfatherClockHand = 156, // level 35, hours and minutes
        GrandfatherClockPendulum = 157, // level 35, hours and minutes
        RotateKnightsInClock = 159,
        KnightsCrossbow = 162, // level 31
        RotatingBlades = 164, // level 23 on lorry track
        ActionObjectToGrab = 165, // level 34 mirror to reflect light, but also for the destroyable vase in the same level
        PaterNosterLift = 166, // level 31
        SqueezingMachine = 167, // level 31
        RoofTopThunderHandler1 = 169,// not sure what this does
        CutSceneCloud = 171, // in end cut scene
        CeilingBladesComingDown = 172, // In level 33, the ones that can be stopped by coffin
        PortableCoffin = 173, // In level 33
        BladesFromSide = 174, // level 33
        ExplodingMachine = 175,  // level 31
        PressureIndicator = 177, // Used in level 31 for the exploding machine
        LargeFish = 180, // in level 31
        DestroyableRoof = 182, // roof part that is destroyed by lightning in level 36
        RoofTopThunderHandler2 = 183, // not sure what this does
        RoofTopThunderHandler3 = 184, // not sure what this does
        AmbientSoundPosition = 190, // Used in various places for occasional 3d sound
        LavaSmoke = 200, // Probably turns off itself when flooded
        TrackFireAnimation = 202,
        TriggerObject = 205, // inside Cathedral
        WitchHead = 207,
        BuyItem = 208,
        TutorialTrigger = 210,
        CrystalBallTableInCutScene = 211, // for the crystal ball
        SkullInCutScene = 212, // the skull in the crystal ball
        // unknowns:
        Behavior_102 = 102, // Something in the ending village cutscene. Multiple instances assigned to the trees
        Behavior_111 = 111, // Something in the ending village cutscene. One instance assigned to the shovel
        Behavior_108 = 108, // could be thunder or leaf particles gen (assigned once to tree in level 10)
        Behavior_148 = 148, // something I could not locate in level 35
        Behavior_158 = 158, // Something in the cathedral outdoor (level 19), assigned to tree model
        Behavior_161 = 161, // Something in the ending merlin cutscene
        Behavior_170 = 170, // Something in the ending village cutscene. 2 instances assigned to grates
        Behavior_178 = 178, // the barrel in the staircase, probably manages the staircase up/down teleport
        Behavior_201 = 201, // could be thunder or leaf particles gen (assigned once to decoration in level 10)
        Behavior_206 = 206, // could be witch-manager
        Behavior_209 = 209, // could be witch-manager
        Behavior_255 = 255, // Something in museum cutscene
    };

    enum class CollisionType
    {
        None = 0,
        Poison = 1,
        Medicine = 2,
        Gold = 3,
        Switch = 4,
        Door = 5,
        Key = 6,
        TriggerActivation = 7,
        WallDestruction = 8,
        PushPlayer = 9, // unknown, two trees in level 10, but no behavior, Maybe pushes away player, also used by rolling rocks in cave
        DestroyOnCollide = 10,
        CurtainOpens = 11, // the curtains that open on proximity in witch-level 90
        DeadBullHead = 12,
        RatAndSnake = 13,
        BatIndoor = 14,
        Flies = 16, // a few instances in both forest levels
        BuyItem = 17,
        Ammo = 20,
        Weapon = 21,
        DestroyableBarrier = 26, // e.g. wood in forest
        Skeleton = 30,
        Cockroach = 31, // also brown spider
        AnacondaTail = 33,
        AnacondaHead = 34,
        PushingEnemy = 35,
        SpiderLarge = 36,
        Harvestman = 37,
        Devil = 38,
        FlyingArrow = 41,
        PickableMap = 45,
        PictureMap = 46,
        WalkablePlanks = 47, // a bridge that you can walk over
        ClimbableLadder = 48, // e.g. at Octopus
        DamageObj = 49, // e.g. burning fire
        GroupedSwitch = 51,
        PickableTorchlight = 56,
        SwitchForMetalPole = 57,
        WheelToLiftRope = 58,
        RopeSwitch = 59,
        Dragon = 60,
        ActivateSpiders = 61, // in cathedral (at puddles)
        DiggableGround = 63,
        WashableMud = 64, // in park level
        Shark = 65,
        Crab = 66,
        Ghost = 67,
        Dog = 68,
        TriggerDebris = 70, // in end level
        LightRayKillsGhost = 73,
        LiftUp = 76,
        RechargeElectricity = 77,
        Lorry = 78,
        AttachableDamageObject = 88, // for Kraken arms inside sewerage. But also used for attached machine parts for RotatingMachineCenter
        KrakenHead = 89, // inside sewerage
        PuzzleGameTile = 90,
        KnightShootsArrow = 91,
        VaseToGrab = 92,
        MirrorToGrab = 93,
        LightSensor = 94, // for the reflected light
        LevelEnd = 100,
        RotatingBlades = 103,
        WaterRecharge = 105, // at fountain
        Type_107 = 107, // unknown, two trees in level 10, but no behavior
        CutSceneCloud = 110, // in end cut scene
        SwimmingParticles = 117, // underwater with large Octopus
        Teleporter = 120, // various teleporter to jump between map parts in level 10 and 36
        SarcophagusWithBones = 121, // reveals bones to feed dogs
        BurnableOilPuddle = 125,
        CeilingConcreteSlabStake = 126, // level 23 - the wood that holds CeilingConcreteSlab
        CrackingSubmarineWindow = 144,
        PuzzleSpool = 149, // the ones with roman numbers in level 35
        GrandfatherClockItem = 155, // level 35
        TriggerObject = 205,
        TriggerTextPrompt = 210, // in last level
        BookWithCode = 252,
    };


    enum class WeaponType
    {
        None = 0,
        Dagger = 1,
        Sword = 2,
        Crossbow = 3,
        Fireball = 4,
        Stone = 5,
        TrapOrPole = 6,
        Electricity = 7,
        Showel = 8
    };

    const char* BehaviorTypeToString(BehaviorType t);
    const char* CollisionTypeToString(CollisionType t);
    const char* WeaponTypeTypeToString(WeaponType t);

    void SortRawGameObjects(std::vector<const RawGameObject*>& list);
	std::wstring LogRawGameObjects(const RawGameObject** objList, int count, std::wstring_view sep = L"\t", std::wstring_view nl = L"\n");
	std::wstring GameObjectToRawCpp(const RawGameObject& obj);
    FUTURE(void) CreateObjectBehavior(Scene& scene, GameObject& go, RawGameObject& rawGo);
    std::shared_ptr<LevelGameObject> CreateGameObject(Scene& scene, RawGameObject& rawGO);
    void ConvertToMoon(RenderableInstance& inst, const RawGameObject& rawGo);


    class D_IMPEXP PropertyAccessorBase
    {
    public:
        PropertyAccessorBase(const RawGameObject& go)
            : m_go(go)
        {

        }

        int LogLineBestMatch(int line, std::wstringstream& wss) const;

        inline bool Isvalid() const { return GetHealth() != 0; }
        inline DisplayType GetDisplayType() const { return (DisplayType)m_go.m_rawBytes[51]; }

        inline int GetModelId() const { return (int)m_go.m_rawBytes[0]; }
        inline int GetModelAnim() const { return (int)m_go.m_rawBytes[1] - 1; }
 		inline int GetModelAnim2() const { return (int)m_go.m_rawBytes[1] - 1; } // same as anim
        inline int GetModelFrame() const { return (int)m_go.m_rawBytes[3] - 1; }
		inline int GetModelFrame2() const { return (int)m_go.m_rawBytes[2] - 1; }
        float GetAnimBlendWeight() const;
		float GetSpriteTransparency() const;

        Vector GetPosition() const;
        Vector GetEuler() const;
        Vector GetDirection() const;

        inline float GetScale() const;
        inline Matrix4 GetScaledTransform() const { return Matrix4::CreateTransform(GetPosition(), GetEuler(), GetScale()); }
        inline Matrix4 GetTransform() const { return Matrix4::CreateTransform(GetPosition(), GetEuler(), 1.f); }
        inline bool FlipSpriteU() const { return ((m_go.m_rawShorts[15] & 1) > 0) && GetBehaviorType()!= BehaviorType::Moon; } // moon is never flipped
        inline GamePlay::BehaviorType GetBehaviorType() const { return (GamePlay::BehaviorType)m_go.m_rawBytes[8]; }
        inline int GetHealth() const { return (int)m_go.m_rawShorts[5]; }

        inline float CollisionRadiusXZ() const {return (float)m_go.m_rawShorts[12] * DespToFloat;}
        inline float CollisionRadiusY() const { return (float)m_go.m_rawShorts[13] * DespToFloat;}

        inline GamePlay::CollisionType GetCollisionType() const {return (GamePlay::CollisionType)m_go.m_rawBytes[6];}

        virtual int LogLine(int line, std::wstringstream& wss) const;
    protected:
        const RawGameObject& m_go;
    };


    class D_IMPEXP PropertyAccessorWeapon : public PropertyAccessorBase
    {
    public:
        PropertyAccessorWeapon(const RawGameObject& go)
            : PropertyAccessorBase(go)
        {
        }

        inline GamePlay::WeaponType GetWeaponType() const { return (GamePlay::WeaponType)m_go.m_rawBytes[34]; }

        virtual int LogLine(int line, std::wstringstream& wss) const override;
    };

    class D_IMPEXP PropertyAccessorGold : public PropertyAccessorBase
    {
    public:
        PropertyAccessorGold(const RawGameObject& go)
            : PropertyAccessorBase(go)
        {
        }

        inline int GetGold() const { return (int)m_go.m_rawShorts[17]; }

        virtual int LogLine(int line, std::wstringstream& wss) const override;
    };

    class D_IMPEXP PropertyAccessorMedicine : public PropertyAccessorBase
    {
    public:
        PropertyAccessorMedicine(const RawGameObject& go)
            : PropertyAccessorBase(go)
        {
        }

        inline int GetHealingPercentage() const { return (int)m_go.m_rawShorts[17] / 8; } // 800 = 100%

        virtual int LogLine(int line, std::wstringstream& wss) const override;
    };

    class D_IMPEXP PropertyAccessorKey : public PropertyAccessorBase
    {
    public:
        PropertyAccessorKey(const RawGameObject& go)
            : PropertyAccessorBase(go)
        {
        }

        inline int GetKeyIndex() const { return (int)m_go.m_rawShorts[17]; } 

        virtual int LogLine(int line, std::wstringstream& wss) const override;
    };

    class D_IMPEXP PropertyAccessorSwitch : public PropertyAccessorBase
    {
    public:
        PropertyAccessorSwitch(const RawGameObject& go)
            : PropertyAccessorBase(go)
        {
        }

        inline int GetSwitchIndex() const { return (int)m_go.m_rawShorts[17]; }

        virtual int LogLine(int line, std::wstringstream& wss) const override;
    };

    class D_IMPEXP PropertyAccessorGroupedSwitch : public PropertyAccessorBase
    {
    public:
        PropertyAccessorGroupedSwitch(const RawGameObject& go)
            : PropertyAccessorBase(go)
        {
        }

        inline int GetGroupIndex() const { return (int)m_go.m_rawShorts[22]; }
        inline int GetGroupBitmask() const { return (int)m_go.m_rawShorts[23]; }

        virtual int LogLine(int line, std::wstringstream& wss) const override;
    };

    class D_IMPEXP PropertyAccessorDoor : public PropertyAccessorBase
    {
    public:
        PropertyAccessorDoor(const RawGameObject& go)
            : PropertyAccessorBase(go)
        {
        }

        inline int GetKeyLock() const { return (int)m_go.m_rawShorts[23]; }
        inline int GetSwitchLock() const { int h = GetHealth(); return (h > 1000) ? (h - 1000) : 0; }
        inline int GetGroupLock() const { return (int)m_go.m_rawShorts[22]; }
        inline bool IsFree() const { return GetKeyLock() > 0 && GetSwitchLock() > 0; }

        virtual int LogLine(int line, std::wstringstream& wss) const override;
    };

    class D_IMPEXP PropertyAccessorPlayer : public PropertyAccessorBase
    {
    public:
        PropertyAccessorPlayer(const RawGameObject& go)
            : PropertyAccessorBase(go)
        {
        }

        Matrix4 GetCameraTransform(float hoverDistance = 0.f) const;
    };
} // namespace
