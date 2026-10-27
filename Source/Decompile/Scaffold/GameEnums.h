// Generated from renderer enums and original executable cases; see GenerateBehaviorScaffold.ps1.
#pragma once

#include <cstdint>

namespace Desperabis
{
    enum class BehaviorType : std::uint16_t
    {
        None = 0,
        Behavior_1 = 1,
        WeaponDamageCollider = 2,
        Behavior_3 = 3,
        Behavior_4 = 4,
        Behavior_5 = 5,
        Behavior_6 = 6,
        Behavior_7 = 7,
        Pillar = 8,
        LightSource = 9,
        Door = 10,
        Behavior_11 = 11,
        Frog = 12,
        RatAndSnake = 13,
        BatIndoor = 14,
        IdleAnimation = 15, // for vertex anims, e.g. spider web
        Flies = 16, // a few instances in both forest levels
        BatOutdoor = 17, // Unlike 'Bat' does not collide with walls?
        DriftingParticle = 18, // Shared weapon-hit and emitter particle.
        FireFly = 19,
        FireParticle = 21, // Spawned by BurningFireplace; recovered from the executable.
        BurningFireplace = 22, // e.g. lorry rails
        PulsatingLight = 23, // fireball, but also others
        SmokeFading = 24, // e.g. for fire
        Behavior_25 = 25,
        DestroyableBarrier = 26, // e.g. wood in forest
        VerticalConcreteSlab = 27, // level 23 - the one that stands and falls over
        Behavior_28 = 28,
        RollingRock = 29, // The ones that roll in submarine level 32, but also the blades in castle level
        Skeleton = 30,
        Cockroach = 31, // also brown spider
        WaterImpact = 33,
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
        Behavior_45 = 45,
        Behavior_46 = 46,
        WalkablePlanks = 47, // a bridge that you can walk over
        BreakableStoneWallFloodingWater = 48, // the one that triggers FloodingWater in level 26
        WalkableDrawBridge = 49, // Level 23 : the bridge that you can walk over after you triggered a switch
        Switch1 = 50,
        SwitchGrouped = 51, // This one is in level 12 and roof
        RopeSwitch = 52, // The one that hangs in level 23, but also used for endboss legs?! For the animation?
        Switch3 = 53, // The one that rotates in swamp level 24
        UnderwaterPlants = 54, // is this a switch?!
        Behavior_55 = 55,
        Switch4 = 56, // The one that rotates in level 31
        MetalPole = 57, // the one to open door in submarine level 32. Also used for the torchlight that you can carry.
        KeyInBucket = 58, // level 33
        Dragon = 60, // only level 15?
        Behavior_61 = 61,
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
        Behavior_82 = 82,
        BonesToGrab = 83, // the ones in level 34 that appear
        ItemHiddenInBox = 84, // an item that is shown when a box is opened in level 36
        AnancondaHead = 85,
        AnancondaTailSegment = 86,
        Behavior_87 = 87,
        AttachableDamageObject = 88, // for Kraken arms inside sewerage. But also used for attached machine parts for RotatingMachineCenter
        KrakenHead = 89, // inside sewerage
        EndBossHead = 91,
        EndBossLeg = 92,
        EndBossFeeler = 93,
        EndBossBody = 94,
        BreakableGround = 95, // In final level 26, where endboss falls down
        Behavior_96 = 96,
        Behavior_97 = 97,
        Behavior_98 = 98,
        Moon = 100,
        WaterDrop = 101,
        Behavior_102 = 102, // Something in the ending village cutscene. Multiple instances assigned to the trees
        Behavior_103 = 103,
        UnderwaterColorAnim = 104, // underwater vertex color animation
        Behavior_105 = 105,
        Behavior_106 = 106,
        Behavior_108 = 108, // could be thunder or leaf particles gen (assigned once to tree in level 10)
        LeafParticle1 = 109,
        FogParticle = 110,
        Behavior_111 = 111, // Something in the ending village cutscene. One instance assigned to the shovel
        LeafParticle2 = 112, // what is the difference? Both in Level 10
        RoofDoorHandler = 113,
        Behavior_114 = 114,
        NeonLights = 116, // in museum
        FloatingDebris = 117, // Parts floating in water near the large octopus, per author recollection; not smoke or an emitter.
        SinkingObject = 118,
        BurriedVerticalDoor = 119, // level 23
        Teleporter = 120, // various teleporter to jump between map parts in level 10 and 36
        LevelStartOverride = 121, // in cave
        DirtFromAbove = 122, // level 23 - falls from above and extinguishes fire
        DirtFromAboveShutter = 123, // level 23 - falls from above and extinguishes fire
        CeilingConcreteSlab = 124, // level 23 - the one that is carried by the wood and falls from above
        BurnableOilPuddle = 125,
        CeilingConcreteSlabStake = 126, // level 23 - the wood that holds CeilingConcreteSlab
        UnreachableKey = 127, // the key in the hut of swamp level 24
        Behavior_128 = 128,
        AttackingBull = 129,
        ExtinguishLava = 130, // in forest
        FloodingWater = 131, // the one in the final level 26
        ClimbableLadder = 133, // e.g. at Octopus
        CurtainOpens = 134, // the curtains that open on proximity in witch-level 90
        Meteor = 135, // The meteor in the meteor cutscene
        Behavior_136 = 136,
        Behavior_137 = 137,
        Behavior_138 = 138,
        SpawnInCrater = 139, // not exactly sure what it is, but it is invisble objects in the meteor crater (many of them)
        RotatingMachineCenter = 140, // e.g. level 22
        Submarine = 141,
        GrasingCow = 142,
        Behavior_143 = 143,
        CrackingSubmarineWindow = 144,
        PuzzleGameTile = 145, // level 35
        Behavior_146 = 146,
        PuzzleGameHandler = 147,
        Behavior_148 = 148, // something I could not locate in level 35
        PuzzleSpool = 149, // the ones with roman numbers in level 35
        OpeningCreviceWithExit = 150, // the one that opens in level 25
        Behavior_151 = 151,
        OpeningCrevice = 152, // the one that opens in level 25
        FlushingFishes = 153, // at the flush for flushing the fish, probably controls the water level
        SomethingWithTheFishes  = 154, // the water in the cave
        GrandfatherClock = 155, // level 35
        GrandfatherClockHand = 156, // level 35, hours and minutes
        GrandfatherClockPendulum = 157, // level 35, hours and minutes
        Behavior_158 = 158, // Something in the cathedral outdoor (level 19), assigned to tree model
        RotateKnightsInClock = 159,
        Behavior_161 = 161, // Something in the ending merlin cutscene
        KnightsCrossbow = 162, // level 31
        Behavior_163 = 163,
        RotatingBlades = 164, // level 23 on lorry track
        ActionObjectToGrab = 165, // level 34 mirror to reflect light, but also for the destroyable vase in the same level
        PaterNosterLift = 166, // level 31
        SqueezingMachine = 167, // level 31
        Behavior_168 = 168,
        RoofTopThunderHandler1 = 169,// not sure what this does
        Behavior_170 = 170, // Something in the ending village cutscene. 2 instances assigned to grates
        CutSceneCloud = 171, // in end cut scene
        CeilingBladesComingDown = 172, // In level 33, the ones that can be stopped by coffin
        PortableCoffin = 173, // In level 33
        BladesFromSide = 174, // level 33
        ExplodingMachine = 175,  // level 31
        Behavior_176 = 176,
        PressureIndicator = 177, // Used in level 31 for the exploding machine
        Behavior_178 = 178, // the barrel in the staircase, probably manages the staircase up/down teleport
        LargeFish = 180, // in level 31
        Behavior_181 = 181,
        DestroyableRoof = 182, // roof part that is destroyed by lightning in level 36
        RoofTopThunderHandler2 = 183, // not sure what this does
        RoofTopThunderHandler3 = 184, // not sure what this does
        AmbientSoundPosition = 190, // Used in various places for occasional 3d sound
        LavaSmoke = 200, // Probably turns off itself when flooded
        Behavior_201 = 201, // could be thunder or leaf particles gen (assigned once to decoration in level 10)
        TrackFireAnimation = 202,
        Behavior_204 = 204,
        TriggerObject = 205, // inside Cathedral
        Behavior_206 = 206, // could be witch-manager
        WitchHead = 207,
        BuyItem = 208,
        Behavior_209 = 209, // could be witch-manager
        TutorialTrigger = 210,
        CrystalBallTableInCutScene = 211, // for the crystal ball
        SkullInCutScene = 212, // the skull in the crystal ball
        Behavior_255 = 255, // Something in museum cutscene
        Behavior_300 = 300,
        DisabledSentinel = 65535,
    };

    enum class CollisionType : std::uint8_t
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
        Type_22 = 22, // Breakup-particle source in 10AC:10F5.
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
        Type_138 = 138, // Activation copies field_2E into the behavior.
        CrackingSubmarineWindow = 144,
        PuzzleSpool = 149, // the ones with roman numbers in level 35
        GrandfatherClockItem = 155, // level 35
        TriggerObject = 205,
        TriggerTextPrompt = 210, // in last level
        BookWithCode = 252,
    };

    enum class DisplayType : std::uint8_t
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

    enum class WeaponType : std::uint8_t
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

    static_assert(sizeof(BehaviorType) == 2);
    static_assert(sizeof(CollisionType) == 1);
    static_assert(sizeof(DisplayType) == 1);
    static_assert(sizeof(WeaponType) == 1);
}
