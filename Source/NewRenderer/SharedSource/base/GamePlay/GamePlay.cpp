#include "pch.h"
#include <GamePlay/GamePlay.h>
#include <Asset/AssetManager.h>
#include <Asset/Level.h>
#include <GamePlay/SimpleBehavior.h>
#include <algorithm>

extern float SkyVerticalScale;
extern float SkyVerticalOffset;
extern float SkyRotationOffset;

namespace GamePlay
{

    bool IsVectorDisplayType(DisplayType display, Color4& tintColor, float& fireIntensity)
    {
        auto TintColorForBrightness = [](DisplayType d) -> Color4
        {
			byte lum = (byte)((int)d - (int)DisplayType::VectorBrightness0) * 255 / ((int)DisplayType::VectorBrightness7 - (int)DisplayType::VectorBrightness0);
            return { lum, lum, lum, 255 };
        };

        switch (display)
        {
        case DisplayType::VectorNormal:
        case DisplayType::VectorUnlit:
        case DisplayType::VectorTorchlight:
        case DisplayType::VectorUnderWater:
            fireIntensity = 0.f;
            tintColor = { 255, 255,255, 255 };
            return true;
        }

        if (display >= DisplayType::VectorBrightness0 && display <= DisplayType::VectorBrightness7)
        {
            tintColor = TintColorForBrightness(display);
			fireIntensity = 0.f;
			return true;
        }
        if (display >= DisplayType::Type30 && display <= DisplayType::Type37)
        {
			fireIntensity = 0.f;
			tintColor = { 255, 255,255, 255 };
            return true;
        }
		if (display >= DisplayType::FirstVectorBurning && display <= DisplayType::LastVectorBurning)
		{
			fireIntensity = ((float)display - (float)DisplayType::FirstVectorBurning) / ((float)DisplayType::LastVectorBurning - (float)DisplayType::FirstVectorBurning);
			tintColor = TintColorForBrightness(DisplayType::VectorBrightness2); // color during burning(?)
			return true;
		}

        return false;
    }

    bool IsSpriteDisplayType(DisplayType display)
    {
        switch (display)
        {
        case DisplayType::SpriteNormal:
        case DisplayType::SpriteTransparent:
        case DisplayType::SpriteTorchlight:
        case DisplayType::SpritePulsate:
        case DisplayType::SpriteTorchlightTransparent:
        case DisplayType::DepthSprite:
        case DisplayType::SpriteScissorRect:
            return true;
        }

        return false;
    }

    bool IsLitSpriteDisplayType(DisplayType display)
    {
        switch (display)
        {
        case DisplayType::SpriteNormal:
        case DisplayType::DepthSprite:
            return true;
        }

        return false;
    }

    char tempBuffer[128];
    const char* ReturnUnknown(int val)
    {
        sprintf_s(tempBuffer, "<%i>", val);
        return tempBuffer;
    }

#define DT_ENUM2STRING(_e) if (t==DisplayType::_e) return #_e;

    const char* DisplayTypeToString(DisplayType t)
    {
        DT_ENUM2STRING(SpriteNormal);
        DT_ENUM2STRING(SpriteTransparent);
        DT_ENUM2STRING(SpriteTorchlight);
        DT_ENUM2STRING(Type3);
        DT_ENUM2STRING(VectorNormal);
        DT_ENUM2STRING(VectorUnlit);
        DT_ENUM2STRING(VectorTorchlight);
        DT_ENUM2STRING(Type7);
        DT_ENUM2STRING(Type8);
        DT_ENUM2STRING(Type9);
        DT_ENUM2STRING(SpritePulsate);
        DT_ENUM2STRING(SpriteTorchlightTransparent);
        DT_ENUM2STRING(DepthSprite);
        DT_ENUM2STRING(VectorBrightness0);
        DT_ENUM2STRING(VectorBrightness1);
        DT_ENUM2STRING(VectorBrightness2);
        DT_ENUM2STRING(VectorBrightness3);
        DT_ENUM2STRING(VectorBrightness4);
        DT_ENUM2STRING(VectorBrightness5);
        DT_ENUM2STRING(VectorBrightness6);
        DT_ENUM2STRING(VectorBrightness7);
        DT_ENUM2STRING(Type30);
        DT_ENUM2STRING(Type31);
        DT_ENUM2STRING(Type32);
        DT_ENUM2STRING(Type33);
        DT_ENUM2STRING(Type34);
        DT_ENUM2STRING(Type35);
        DT_ENUM2STRING(Type36);
        DT_ENUM2STRING(Type37);
        DT_ENUM2STRING(VectorUnderWater);
        DT_ENUM2STRING(Hidden);
        DT_ENUM2STRING(SpriteScissorRect);
        return ReturnUnknown((int)t);
    }

    const char* DisplayTypeToString(DisplayType t, char *buffer)
    {
        if (t >= DisplayType::FirstVectorBurning && t <= DisplayType::LastVectorBurning)
        {
            sprintf_s(buffer, 1024, "burning:%i", (int)t - (int)DisplayType::FirstVectorBurning);
            return buffer;
        }
        return DisplayTypeToString(t);
    }


#define BT_ENUM2STRING(_e) if (t==BehaviorType::_e) return #_e;

    const char* BehaviorTypeToString(BehaviorType t)
    {
        BT_ENUM2STRING(None);
        BT_ENUM2STRING(Pillar);
        BT_ENUM2STRING(LightSource);
        BT_ENUM2STRING(PulsatingLight);
        BT_ENUM2STRING(WaterDrop);
        BT_ENUM2STRING(FogParticle);
        BT_ENUM2STRING(TrackFireAnimation);
        BT_ENUM2STRING(Door);
        BT_ENUM2STRING(Switch1);
        BT_ENUM2STRING(SwitchGrouped);
        BT_ENUM2STRING(Skeleton);
        BT_ENUM2STRING(Cockroach);
        BT_ENUM2STRING(IdleAnimation);
        BT_ENUM2STRING(Mummie);
        BT_ENUM2STRING(Behavior_108);
        BT_ENUM2STRING(LeafParticle1);
        BT_ENUM2STRING(LeafParticle2);
        BT_ENUM2STRING(Behavior_201);
        BT_ENUM2STRING(BearTrapAmmo);
        BT_ENUM2STRING(Moon);
        BT_ENUM2STRING(Crab);
        BT_ENUM2STRING(RatAndSnake);
        BT_ENUM2STRING(SpiderLarge);
        BT_ENUM2STRING(BurnableOilPuddle);
        BT_ENUM2STRING(BatIndoor);
        BT_ENUM2STRING(BatOutdoor);
        BT_ENUM2STRING(Devil);
        BT_ENUM2STRING(Harvestman);
        BT_ENUM2STRING(ShootArrows);
        BT_ENUM2STRING(Dragon);
        BT_ENUM2STRING(DragonHead);
        BT_ENUM2STRING(FlyingArrow);
        BT_ENUM2STRING(SpearOnGroundTrap);
        BT_ENUM2STRING(ExtinguishLava);
        BT_ENUM2STRING(Frog);
        BT_ENUM2STRING(SmokeFading);
        BT_ENUM2STRING(DestroyableBarrier);
        BT_ENUM2STRING(Flies);
        BT_ENUM2STRING(Shark);
        BT_ENUM2STRING(RollingRockSlope);
        BT_ENUM2STRING(Gorilla);
        BT_ENUM2STRING(AmbientSoundPosition);
        BT_ENUM2STRING(AnancondaHead);
        BT_ENUM2STRING(WalkablePlanks);
        BT_ENUM2STRING(AnancondaTailSegment);
        BT_ENUM2STRING(LiftUp);
        BT_ENUM2STRING(FireExhauster);
        BT_ENUM2STRING(RechargeElectricity);
        BT_ENUM2STRING(Dog);
        BT_ENUM2STRING(DogLeash);
        BT_ENUM2STRING(Behavior_158);
        BT_ENUM2STRING(DiggableGround);
        BT_ENUM2STRING(TriggerObject);
        BT_ENUM2STRING(Ghost);
        BT_ENUM2STRING(LightRayKillsGhost);
        BT_ENUM2STRING(KrakenHead);
        BT_ENUM2STRING(AttachableDamageObject);
        BT_ENUM2STRING(SwimmingParticles);
        BT_ENUM2STRING(RotatingMachineCenter);
        BT_ENUM2STRING(ClimbableLadder);
        BT_ENUM2STRING(Scorpion);
        BT_ENUM2STRING(BurningFireplace);
        BT_ENUM2STRING(BurriedVerticalDoor);
        BT_ENUM2STRING(DirtFromAbove);
        BT_ENUM2STRING(DirtFromAboveShutter);
        BT_ENUM2STRING(CeilingConcreteSlab);
        BT_ENUM2STRING(CeilingConcreteSlabStake);
        BT_ENUM2STRING(VerticalConcreteSlab);
        BT_ENUM2STRING(WalkableDrawBridge);
        BT_ENUM2STRING(RopeSwitch);
        BT_ENUM2STRING(Lorry);
        BT_ENUM2STRING(RotatingBlades);
        BT_ENUM2STRING(UnreachableKey);
        BT_ENUM2STRING(ActivatableLadder);
        BT_ENUM2STRING(Switch3);
        BT_ENUM2STRING(FireFly);
        BT_ENUM2STRING(OpeningCrevice);
        BT_ENUM2STRING(OpeningCreviceWithExit);
        BT_ENUM2STRING(FloodingWater);
        BT_ENUM2STRING(BreakableStoneWallFloodingWater);
        BT_ENUM2STRING(EndBossBody);
        BT_ENUM2STRING(EndBossHead);
        BT_ENUM2STRING(EndBossFeeler);
        BT_ENUM2STRING(EndBossLeg);
        BT_ENUM2STRING(BreakableGround);
        BT_ENUM2STRING(MummieGuard);
        BT_ENUM2STRING(GrasingCow);
        BT_ENUM2STRING(AttackingBull);
        BT_ENUM2STRING(PaterNosterLift);
        BT_ENUM2STRING(Switch4);
        BT_ENUM2STRING(SqueezingMachine);
        BT_ENUM2STRING(ExplodingMachine);
        BT_ENUM2STRING(LargeFish);
        BT_ENUM2STRING(PressureIndicator);
        BT_ENUM2STRING(RollingRock);
        BT_ENUM2STRING(DecoFish);
        BT_ENUM2STRING(Submarine);
        BT_ENUM2STRING(UnderwaterPlants);
        BT_ENUM2STRING(MetalPole);
        BT_ENUM2STRING(CrackingSubmarineWindow);
        BT_ENUM2STRING(PortableCoffin);
        BT_ENUM2STRING(Behavior_178);
        BT_ENUM2STRING(CeilingBladesComingDown);
        BT_ENUM2STRING(BladesFromSide);
        BT_ENUM2STRING(KnightsCrossbow);
        BT_ENUM2STRING(KeyInBucket);
        BT_ENUM2STRING(BonesToGrab);
        BT_ENUM2STRING(ActionObjectToGrab);
        BT_ENUM2STRING(MercuryLamp);
        BT_ENUM2STRING(PuzzleGameTile);
        BT_ENUM2STRING(PuzzleGameHandler);
        BT_ENUM2STRING(GrandfatherClock);
        BT_ENUM2STRING(GrandfatherClockHand);
        BT_ENUM2STRING(GrandfatherClockPendulum);
        BT_ENUM2STRING(PuzzleSpool);
        BT_ENUM2STRING(Behavior_148);
        BT_ENUM2STRING(DestroyableRoof);
        BT_ENUM2STRING(Teleporter);
        BT_ENUM2STRING(ItemHiddenInBox);
        BT_ENUM2STRING(WitchHead);
        BT_ENUM2STRING(CurtainOpens);
        BT_ENUM2STRING(SkullInCutScene);
        BT_ENUM2STRING(CrystalBallTableInCutScene);
        BT_ENUM2STRING(Behavior_102);
        BT_ENUM2STRING(Behavior_170);
        BT_ENUM2STRING(Behavior_111);
        BT_ENUM2STRING(Behavior_161);
        BT_ENUM2STRING(Meteor);
        BT_ENUM2STRING(Behavior_255);
        BT_ENUM2STRING(FlipTRexHead);
        BT_ENUM2STRING(NeonLights);
        BT_ENUM2STRING(SpawnInCrater);
        BT_ENUM2STRING(TriggerDebris);
        BT_ENUM2STRING(LavaSmoke);
        BT_ENUM2STRING(LevelStartOverride);
        BT_ENUM2STRING(Buried);
        BT_ENUM2STRING(UnderwaterColorAnim);
        BT_ENUM2STRING(FlushingFishes);
        BT_ENUM2STRING(SomethingWithTheFishes);
        BT_ENUM2STRING(RotateKnightsInClock);
        BT_ENUM2STRING(RoofDoorHandler);
        BT_ENUM2STRING(RoofTopThunderHandler1);
        BT_ENUM2STRING(RoofTopThunderHandler2);
        BT_ENUM2STRING(RoofTopThunderHandler3);
        BT_ENUM2STRING(TutorialTrigger);
        BT_ENUM2STRING(Behavior_206);
        BT_ENUM2STRING(Behavior_209);
        BT_ENUM2STRING(BuyItem);
        BT_ENUM2STRING(CutSceneCloud);

        return ReturnUnknown((int)t);
    }

#define PT_ENUM2STRING(_e) if (t==CollisionType::_e) return #_e;

    const char* CollisionTypeToString(CollisionType t)
    {
        PT_ENUM2STRING(None);
        PT_ENUM2STRING(Poison);
        PT_ENUM2STRING(Medicine);
        PT_ENUM2STRING(Gold);
        PT_ENUM2STRING(Switch);
        PT_ENUM2STRING(Door);
        PT_ENUM2STRING(Key);
        PT_ENUM2STRING(Ammo);
        PT_ENUM2STRING(Weapon);
        PT_ENUM2STRING(RatAndSnake);
        PT_ENUM2STRING(Ammo);
        PT_ENUM2STRING(Weapon);
        PT_ENUM2STRING(Skeleton);
        PT_ENUM2STRING(PushingEnemy);
        PT_ENUM2STRING(PickableMap);
        PT_ENUM2STRING(PictureMap);
        PT_ENUM2STRING(GroupedSwitch);
        PT_ENUM2STRING(Crab);
        PT_ENUM2STRING(TriggerObject);
        PT_ENUM2STRING(TriggerActivation);
        PT_ENUM2STRING(WallDestruction);
        PT_ENUM2STRING(LevelEnd);
        PT_ENUM2STRING(SpiderLarge);
        PT_ENUM2STRING(BurnableOilPuddle);
        PT_ENUM2STRING(BatIndoor);
        PT_ENUM2STRING(Harvestman);
        PT_ENUM2STRING(Devil);
        PT_ENUM2STRING(DestroyOnCollide);
        PT_ENUM2STRING(FlyingArrow);
        PT_ENUM2STRING(Dragon);
        PT_ENUM2STRING(PushPlayer);
        PT_ENUM2STRING(Type_107);
        PT_ENUM2STRING(Teleporter);
        PT_ENUM2STRING(DestroyableBarrier);
        PT_ENUM2STRING(Flies);
        PT_ENUM2STRING(Shark);
        PT_ENUM2STRING(LiftUp);
        PT_ENUM2STRING(RechargeElectricity);
        PT_ENUM2STRING(WalkablePlanks);
        PT_ENUM2STRING(Cockroach);
        PT_ENUM2STRING(AnacondaTail);
        PT_ENUM2STRING(AnacondaHead);
        PT_ENUM2STRING(DiggableGround);
        PT_ENUM2STRING(Dog);
        PT_ENUM2STRING(LightRayKillsGhost);
        PT_ENUM2STRING(Ghost);
        PT_ENUM2STRING(ActivateSpiders);
        PT_ENUM2STRING(AttachableDamageObject);
        PT_ENUM2STRING(KrakenHead);
        PT_ENUM2STRING(SwimmingParticles);
        PT_ENUM2STRING(ClimbableLadder);
        PT_ENUM2STRING(CeilingConcreteSlabStake);
        PT_ENUM2STRING(RotatingBlades);
        PT_ENUM2STRING(Lorry);
        PT_ENUM2STRING(DamageObj);
        PT_ENUM2STRING(TriggerTextPrompt);
        PT_ENUM2STRING(TriggerDebris);
        PT_ENUM2STRING(WaterRecharge);
        PT_ENUM2STRING(WashableMud);
        PT_ENUM2STRING(DeadBullHead);
        PT_ENUM2STRING(CrackingSubmarineWindow);
        PT_ENUM2STRING(SwitchForMetalPole);
        PT_ENUM2STRING(RopeSwitch);
        PT_ENUM2STRING(WheelToLiftRope);
        PT_ENUM2STRING(PickableTorchlight);
        PT_ENUM2STRING(SarcophagusWithBones);
        PT_ENUM2STRING(MirrorToGrab);
        PT_ENUM2STRING(VaseToGrab);
        PT_ENUM2STRING(LightSensor);
        PT_ENUM2STRING(GrandfatherClockItem);
        PT_ENUM2STRING(PuzzleSpool);
        PT_ENUM2STRING(PuzzleGameTile);
        PT_ENUM2STRING(BookWithCode);
        PT_ENUM2STRING(KnightShootsArrow);
        PT_ENUM2STRING(CurtainOpens);
        PT_ENUM2STRING(BuyItem);
        PT_ENUM2STRING(CutSceneCloud);
        
        return ReturnUnknown((int)t);
    }

#define WT_ENUM2STRING(_e) if (t==WeaponType::_e) return #_e;

    const char* WeaponTypeTypeToString(WeaponType t)
    {
        WT_ENUM2STRING(None);
        WT_ENUM2STRING(Dagger);
        WT_ENUM2STRING(Sword);
        WT_ENUM2STRING(Crossbow);
        WT_ENUM2STRING(Fireball);
        WT_ENUM2STRING(Stone);
        WT_ENUM2STRING(TrapOrPole);
        WT_ENUM2STRING(Electricity);
        WT_ENUM2STRING(Showel);

        return ReturnUnknown((int)t);
    }

    void SortRawGameObjects(std::vector<const RawGameObject*>& list)
    {
        std::sort(list.begin(), list.end(), [](const RawGameObject* instA, const RawGameObject* instB)-> bool
            {
                PropertyAccessorBase pA(*instA);
                PropertyAccessorBase pB(*instB);
                if (pA.GetCollisionType() > pB.GetCollisionType()) return true;
                if (pA.GetCollisionType() < pB.GetCollisionType()) return false;
                if (pA.GetBehaviorType() > pB.GetBehaviorType()) return true;
                if (pA.GetBehaviorType() < pB.GetBehaviorType()) return false;
                return instA->m_objectNumber < instB->m_objectNumber; // small to large
            });

    }

    std::wstring LogRawGameObjects(const RawGameObject** objList, int count, std::wstring_view sep, std::wstring_view nl)
    {
        std::wstringstream wss;
        if (count == 0)
            return L"";
        int line = -1;
        int size = 1;
        for (int line = -1; size > 0; line += size)
        {
            for (int i = 0; i < count; i++)
            {
                if (i > 0)
                {
                    wss << sep;
                }
                const RawGameObject* go = objList[i];
                PropertyAccessorBase props(*go);
                if (line < 0)
                {
                    wss << "Obj#" << go->m_objectNumber;
                    size = 1;
                }
                else
                {
                    size = props.LogLineBestMatch(line, wss);
                }
                assert(line < (int)sizeof(RawGameObjectData) || size == 0);
            }

            for (int i = 0; i < size; i++)
            {
                wss << nl;
            }
        }

        return wss.str();
    }


    std::wstring GameObjectToRawCpp(const RawGameObject& obj)
    {
		std::wstringstream wss;
        wss << "const uint8_t gameobjSeq[] = {";
        for (int i = 0; i < 54; i++)
        {
            if (i > 0)
                wss << ",";
		    wss << (int)obj.m_rawBytes[i];
        }
        wss << "};";
	    return wss.str();
	}


    FUTURE(void) CreateObjectBehavior(Scene& scene, GameObject& go, RawGameObject& rawGo)
    {
        AssetManager& assets = AssetManager::Instance();

        PropertyAccessorBase props(rawGo);
        BehaviorType btype = props.GetBehaviorType();

        if (btype == BehaviorType::TrackFireAnimation)
        {
            int torchlightTexAnim = rawGo.m_rawBytes[34];
            int targetTexSlot = rawGo.m_rawBytes[44] + 12;
            std::wstringstream wss;
            wss << L"ANIMS\\FACKEL" << torchlightTexAnim << ".AN2";
            std::shared_ptr<TextureAnim> torchAnim = CO_AWAIT assets.LoadAsset<TextureAnim>(wss.str());
            float speed = 25.1f;
            switch (torchlightTexAnim)
            {
            case 1:speed = 25.1f; break; // torch
            case 2:speed = 19.1f; break; // big fire
            case 3:speed = 16.1f; break; // lantern
            case 4:speed = 11.1f; break; // smoke
            case 5:speed = 8.1f; break;  // water bubbles
            case 6:speed = 8.1f; break;  // crystal ball
            case 7:speed = 4.1f; break;  // mouth anim
            default:
                assert(false);
            }
            scene.AddFireAnimation(torchAnim, targetTexSlot, speed);

        }
        else if (btype == BehaviorType::LightSource)
        {
            float lightRadius = 140.f;
            if (lightRadius > 0.f)
            {
                std::shared_ptr<LightSource> light = std::make_shared<FlickeringLight>(&scene);
                Vector pos = props.GetPosition();
                float heightOverModel = 5.f * DespToFloat; // depends on the model:
                if (go.m_renderInst != nullptr && go.m_renderInst->GetRenderable() != nullptr)
                {
                    heightOverModel = go.m_renderInst->GetRenderable()->m_localBounds.GetSphereRadius() * 0.20f; // heuristic
                }

                // slightly move the corona up...
                pos += Vector(0, heightOverModel * 0.5f, 0);
                // ..but the light even more
                light->SetLightHeightOffset(heightOverModel * 0.5f);
                light->SetPosition(pos);
                light->SetColor(Color4{ 255,240,180 }, 2.0f);
                light->SetCoronaColorMod(Color4{ 255,220,140 });
                light->SetRadius(lightRadius * DespToFloat);
                scene.AddLightSource(light);
                light->UpdateGlobalBounds();
            }

        }
        else if (btype == BehaviorType::PulsatingLight)
        {
            std::shared_ptr<LightSource> light = std::make_shared<PulsatingLight>(&scene); // manages color and radius
            Vector pos = props.GetPosition();
            float heightOverModel = 5.f * DespToFloat;
            pos += Vector(0, heightOverModel, 0);
            light->SetPosition(pos);
            scene.AddLightSource(light);
        }
        else if (btype == BehaviorType::Moon)
        {
            ConvertToMoon(*go.m_renderInst, rawGo);
			go.SetPosition(go.m_renderInst->GetTransform().GetTranslation());
        }
    }

    std::shared_ptr<LevelGameObject> CreateGameObject(Scene& scene, RawGameObject& rawGO)
    {
        PropertyAccessorBase props(rawGO);
        BehaviorType btype = props.GetBehaviorType();

        switch (btype)
        {
        case BehaviorType::FogParticle: return std::make_shared<FogParticleObject>(&scene, rawGO);
        case BehaviorType::Flies: return std::make_shared<FliesSwarmObject>(&scene, rawGO);
        case BehaviorType::BurningFireplace: return std::make_shared<BurningFireplaceObject>(&scene, rawGO);
        }

        return std::make_shared<LevelGameObject>(&scene, rawGO);
    }


	void ConvertToMoon(RenderableInstance& inst, const RawGameObject& rawGo)
	{
        float winX = (float)rawGo.m_rawShorts[14];
		float winY = (float)rawGo.m_rawShorts[15] / 130.f; // tweaked, probably 10*skytex.height, which is 130 or 160
        float h = winY * SkyVerticalScale + SkyVerticalOffset;
        float angle = winX / 3180.f * Math::Pi; // tweaked, probably 10*skytex.width
        angle += SkyRotationOffset - 0.03f; // compensate a bit for alignment
		Vector pos(-cosf(angle) * DespRelativeScaling, h * DespRelativeScaling, -sin(angle) * DespRelativeScaling);
        inst.SetTranslation(pos);

		std::vector<DrawCallMaterial>& materials = inst.GetOrCreateMaterialOverrides();
		assert(materials.size() == 1);
		DrawCallMaterial& mat(materials[0]);
		mat.m_applyLighting = false;
		mat.m_transparencyType = TransparencyMode::SkyAlphaBlended;
		mat.ComputeHash();
		inst.ComputeTransparencyMask();
	}

	/// <summary>
    /// Game object property accessor
    /// </summary>
    /// <returns></returns>

    Vector PropertyAccessorBase::GetPosition() const
    {
        return Level::DespToPos(m_go.m_rawShorts[6], m_go.m_rawShorts[7], m_go.m_rawShorts[8]);
    }

    Vector PropertyAccessorBase::GetEuler() const
    {
        const float DespToRad = (float)(Math::Pi / 256.0);
        float winX = (float)(m_go.m_rawShorts[14]) * DespToRad;       // pitch
        float winY = (float)(m_go.m_rawShorts[15] + 256) * DespToRad; // yaw
        float winZ = (float)(m_go.m_rawShorts[16]) * DespToRad;       // roll

        // correct for coffin, but not for switch in sewer
        winX = (float)(512 - m_go.m_rawShorts[14]) * DespToRad;       // pitch
        winY = (float)(m_go.m_rawShorts[15] + 256) * DespToRad; // yaw
        winZ = (float)(m_go.m_rawShorts[16]) * DespToRad;       // roll

        // testing
        winX = (float)(512 - m_go.m_rawShorts[14]) * DespToRad;       // pitch
        winY = (float)(m_go.m_rawShorts[15] + 256) * DespToRad; // yaw
        winZ = (float)(512 - m_go.m_rawShorts[16]) * DespToRad;       // roll

        return Vector(winY, winX, winZ);

    }

    Vector PropertyAccessorBase::GetDirection() const
    {
        const float scale = 1.f / 256.f;
        Vector v((float)m_go.m_rawShorts[9] * -scale, (float)m_go.m_rawShorts[10] * scale, (float)m_go.m_rawShorts[11] * scale, 0);
        v.Normalize();
        return v;
    }



    float PropertyAccessorBase::GetScale() const
    {
        return (float)m_go.m_rawUShorts[26] / (64.f * 256.f);
    }


    int PropertyAccessorBase::LogLineBestMatch(int line, std::wstringstream& wss) const
    {
        auto ct = GetCollisionType();
        if (ct == CollisionType::Weapon || ct == CollisionType::Ammo)
        {
            PropertyAccessorWeapon props(m_go);
            return props.LogLine(line, wss);
        }
        if (ct == CollisionType::Gold)
        {
            PropertyAccessorGold props(m_go);
            return props.LogLine(line, wss);
        }
        if (ct == CollisionType::Medicine)
        {
            PropertyAccessorMedicine props(m_go);
            return props.LogLine(line, wss);
        }
        if (ct == CollisionType::Key)
        {
            PropertyAccessorKey props(m_go);
            return props.LogLine(line, wss);
        }
        if (ct == CollisionType::Door)
        {
            PropertyAccessorDoor props(m_go);
            return props.LogLine(line, wss);
        }
        if (ct == CollisionType::Switch)
        {
            PropertyAccessorSwitch props(m_go);
            return props.LogLine(line, wss);
        }
        if (ct == CollisionType::GroupedSwitch)
        {
            PropertyAccessorGroupedSwitch props(m_go);
            return props.LogLine(line, wss);
        }

        return LogLine(line, wss);
    }

#define LOG_LINE(ln, param , size) \
    if (ln == line)\
    {\
        wss << param;\
        return size;\
    }

#define LOG_NEXT_LINE(param , size) \
    if (currentLine == line)\
    {\
        wss << param;\
        return size;\
    }\
    currentLine+=size;

    inline static int DespToDeg(int d)
    {
        return d * 360 / 512;
    }

    int PropertyAccessorBase::LogLine(int line, std::wstringstream& wss) const
    {
        int currentLine = 0;
        // anim
        LOG_NEXT_LINE((int)m_go.m_rawBytes[0], 1);
        LOG_NEXT_LINE((int)m_go.m_rawBytes[1], 1);
        LOG_NEXT_LINE((int)m_go.m_rawBytes[2], 1);
        LOG_NEXT_LINE((int)m_go.m_rawBytes[3], 1);

        LOG_NEXT_LINE((int)m_go.m_rawBytes[4], 1);
        LOG_NEXT_LINE((int)m_go.m_rawBytes[5], 1);
        LOG_NEXT_LINE(GamePlay::CollisionTypeToString(GetCollisionType()), 1);
        LOG_NEXT_LINE((int)m_go.m_rawBytes[7], 1);

        LOG_NEXT_LINE(GamePlay::BehaviorTypeToString(GetBehaviorType()), 1);
        LOG_NEXT_LINE((int)m_go.m_rawBytes[9], 1);

        LOG_NEXT_LINE((int)m_go.m_rawShorts[5], 2); // health
        LOG_NEXT_LINE((int)m_go.m_rawShorts[6], 2); // x
        LOG_NEXT_LINE((int)m_go.m_rawShorts[7], 2); // y
        LOG_NEXT_LINE((int)m_go.m_rawShorts[8], 2); // z

        LOG_NEXT_LINE((int)m_go.m_rawShorts[9], 2); // velocity x
        LOG_NEXT_LINE((int)m_go.m_rawShorts[10], 2); // velocity y
        LOG_NEXT_LINE((int)m_go.m_rawShorts[11], 2); // velocity z

        LOG_NEXT_LINE((int)m_go.m_rawShorts[12], 2); // col rad xz
        LOG_NEXT_LINE((int)m_go.m_rawShorts[13], 2); // col rad y

        if (IsSpriteDisplayType(GetDisplayType()))
        {
            LOG_NEXT_LINE(m_go.m_rawShorts[14], 2); // win x
            LOG_NEXT_LINE(((m_go.m_rawShorts[15]&1) ? "flipped" : "not flipped"), 2); // win y
            LOG_NEXT_LINE(m_go.m_rawShorts[16], 2); // win z
        }
        else
        {
            LOG_NEXT_LINE(DespToDeg(m_go.m_rawShorts[14]), 2); // win x
            LOG_NEXT_LINE(DespToDeg(m_go.m_rawShorts[15]), 2); // win y
            LOG_NEXT_LINE(DespToDeg(m_go.m_rawShorts[16]), 2); // win z
        }

        LOG_NEXT_LINE(m_go.m_rawShorts[17], 2); // item value (gold, medicine...)

        LOG_NEXT_LINE((int)m_go.m_rawBytes[36], 1);
        LOG_NEXT_LINE((int)m_go.m_rawBytes[37], 1);
        LOG_NEXT_LINE((int)m_go.m_rawBytes[38], 1);
        LOG_NEXT_LINE((int)m_go.m_rawBytes[39], 1);
        LOG_NEXT_LINE((int)m_go.m_rawBytes[40], 1);
        LOG_NEXT_LINE((int)m_go.m_rawBytes[41], 1);
		LOG_NEXT_LINE((int)m_go.m_rawShorts[21], 2); // blend weight
        LOG_NEXT_LINE((int)m_go.m_rawBytes[44], 1);
        LOG_NEXT_LINE((int)m_go.m_rawBytes[45], 1);

        LOG_NEXT_LINE(m_go.m_rawShorts[23], 2); // door lock

        LOG_NEXT_LINE((int)m_go.m_rawBytes[48], 1);
        LOG_NEXT_LINE((int)m_go.m_rawBytes[49], 1);
        LOG_NEXT_LINE((int)m_go.m_rawBytes[50], 1);

        char buffer[1024];
        LOG_NEXT_LINE(GamePlay::DisplayTypeToString(GetDisplayType(), buffer), 1);
        LOG_NEXT_LINE((int)m_go.m_rawUShorts[26], 2);

        return 0; // stop
    }

	float PropertyAccessorBase::GetAnimBlendWeight() const
	{
        short w = m_go.m_rawShorts[21];
        if (w <= 100) // this also covers values <0
        {
            return (float)w * 0.01f;
        }
		if (w <= 200)
		{
            return 1.f - (float)(w - 100) * 0.01f;
		}
        return 0.f;
	}

    float PropertyAccessorBase::GetSpriteTransparency() const
    {
        if (GetDisplayType() != DisplayType::SpriteTransparent)
        {
            return 1.f;
        }
        short w = __min(m_go.m_rawShorts[21] + 1, (short)6);
        return (float)w / 6.f;
    }


    int PropertyAccessorWeapon::LogLine(int line, std::wstringstream& wss) const
    {
        LOG_LINE(34, WeaponTypeTypeToString(GetWeaponType()), 2);
        return PropertyAccessorBase::LogLine(line, wss);
    }

    int PropertyAccessorGold::LogLine(int line, std::wstringstream& wss) const
    {
        LOG_LINE(34, GetGold() << " Gold", 2);
        return PropertyAccessorBase::LogLine(line, wss);
    }

    int PropertyAccessorMedicine::LogLine(int line, std::wstringstream& wss) const
    {
        LOG_LINE(34, GetHealingPercentage() << " Percent", 2);
        return PropertyAccessorBase::LogLine(line, wss);
    }

    int PropertyAccessorKey::LogLine(int line, std::wstringstream& wss) const
    {
        LOG_LINE(34, "Key #" << GetKeyIndex(), 2);
        return PropertyAccessorBase::LogLine(line, wss);
    }

    int PropertyAccessorSwitch::LogLine(int line, std::wstringstream& wss) const
    {
        LOG_LINE(34, "Switch #" << GetSwitchIndex(), 2);
        return PropertyAccessorBase::LogLine(line, wss);
    }

    int PropertyAccessorDoor::LogLine(int line, std::wstringstream& wss) const
    {
        int key = GetKeyLock();
        int switchLock = GetSwitchLock();
        int groupIdx = GetGroupLock();

        if (key > 0)
        {
            LOG_LINE(46, "key #" << key, 2);
        }
        else if (switchLock > 0)
        {
            LOG_LINE(46, "switch #" << switchLock, 2);
        }
        else if (groupIdx > 0)
        {
            LOG_LINE(46, "group #" << groupIdx, 2);
        }
        else
        {
            LOG_LINE(46, "free", 2);
        }

        return PropertyAccessorBase::LogLine(line, wss);
    }

    int PropertyAccessorGroupedSwitch::LogLine(int line, std::wstringstream& wss) const
    {
        LOG_LINE(44, "group #" << GetGroupIndex(), 2);
        LOG_LINE(46, "bitmask " << GetGroupBitmask(), 2);
        return PropertyAccessorBase::LogLine(line, wss);
    }

    Matrix4 PropertyAccessorPlayer::GetCameraTransform(float hoverDistance) const
    {
        // the player does not provide proper euler angles, but fortunately a view direction
        Vector pos = GetPosition();
        Vector up(0, 1, 0, 0);
        Vector dir = GetDirection();

        if (hoverDistance > 0.f)
        {
            Vector ofs = dir + Vector(0, 0.6f, 0.f, 0.f);
            ofs.Normalize();
            dir = ofs;
            ofs *= hoverDistance;

            pos += ofs;
        }

        Vector right = up.GetCross(dir);
        up = dir.GetCross(right);
        Matrix4 m;
        m.Create(pos, right, up, dir);
        return m;
    }


} // namespace