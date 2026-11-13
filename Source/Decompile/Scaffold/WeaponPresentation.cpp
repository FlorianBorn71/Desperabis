#include "GameContext.h"
#include "GameMath.h"

#include <algorithm>
#include <cstdlib>
#include <stdexcept>

namespace Desperabis
{
    std::optional<WeaponView> GameContext::GetWeaponView() const
    {
        if (GetMenuPage() != MenuPage::Gameplay || IsCutscenePlaying() || worldObjects[1].itemValue > 0 ||
            weaponAnimationSpeed == -2 || weaponAnimationSpeed == -4 ||
            (selectedWeapon == WeaponType::None && !field_D500) ||
            selectedWeapon == WeaponType::ShopGold ||
            (weaponSpriteFrame == 0 && selectedWeapon != WeaponType::TrapOrPole &&
                selectedWeapon != WeaponType::Bucket && selectedWeapon != WeaponType::Stone))
        {
            return std::nullopt;
        }
        const int bob = static_cast<int>((static_cast<std::int64_t>(SineTable[(playerWalkPhase + 128) & 511]) * 5) >> 16);
        const int bounce = std::abs(bob);
        const int pose = weaponAnimationPosition;
        WeaponView view;
        view.flipHorizontal = weaponHand != 1;
        view.anchorX = weaponHand == 1 ? 0.25f : 0.75f;
        const auto sprite = [&](int base)
        {
            if (weaponSpriteFrame < 1 || weaponSpriteFrame > 3)
            {
                throw std::logic_error("First-person weapon requires a sprite frame in 1..3.");
            }
            view.bitmapFrame = static_cast<std::uint8_t>(base + weaponSpriteFrame - 1);
        };
        const auto model = [&](int animation, int frame, const std::array<int, 6>& transform)
        {
            view.kind = WeaponView::Kind::Model;
            view.object.health = 1000;
            view.object.modelId = 31; // DS:7066 is level model slot 1.
            view.bindWeaponBitmap = true;
            view.object.modelAnim = static_cast<std::uint8_t>(animation);
            view.object.animFrame = static_cast<std::uint8_t>(frame);
            view.object.displayType = DisplayType::VectorNormal;
            view.object.posX = WrapSignedWord(transform[0]);
            view.object.posY = WrapSignedWord(transform[1]);
            view.object.posZ = WrapSignedWord(transform[2]);
            view.object.winX = WrapSignedWord(transform[3]);
            view.object.winY = WrapSignedWord(transform[4]);
            view.object.winZ = WrapSignedWord(transform[5]);
        };
        const auto tableModel = [&](const std::array<std::array<int, 6>, 5>& table,
            int frame, int position, bool mirror, int centerY)
        {
            const int segment = std::clamp(position / 10 + 1, 1, 4);
            const int fraction = (position % 10) * 65536 / 10;
            std::array<int, 6> transform{};
            for (std::size_t i = 0; i < transform.size(); ++i)
            {
                transform[i] = static_cast<int>((static_cast<std::int64_t>(table[segment - 1][i]) * (65536 - fraction) +
                    static_cast<std::int64_t>(table[segment][i]) * fraction) >> 16);
            }
            if (mirror) { transform[0] = -transform[0]; transform[4] = -transform[4]; transform[5] = -transform[5]; }
            transform[1] += bounce;
            model(2, frame, transform);
            view.projectionCenterY = static_cast<float>(std::max(95, centerY));
        };
        switch (selectedWeapon)
        {
        case WeaponType::None:
            sprite(0);
            view.useBitmapOffsetX = true;
            view.offsetX = static_cast<float>(pose - std::abs(2 * bob));
            view.offsetY = -view.offsetX;
            break;
        case WeaponType::Dagger:
            sprite(1);
            view.flipHorizontal = weaponHand == 1;
            view.offsetX = view.offsetY = static_cast<float>(-pose);
            break;
        case WeaponType::Crossbow:
            sprite(3);
            view.flipHorizontal = false;
            view.anchorX = 0.5f;
            view.anchorY -= pose;
            view.centerBitmapX = true;
            view.offsetY = static_cast<float>(bounce);
            break;
        case WeaponType::Fireball:
        case WeaponType::Lentils:
            sprite(6);
            view.anchorX = 0.5f;
            view.anchorY += playerLookHeight - 90 - pose;
            view.useBitmapOffsetX = true;
            view.offsetX = static_cast<float>(-std::abs(2 * bob));
            view.offsetY = -view.offsetX;
            break;
        case WeaponType::Sword:
            sprite(2);
            tableModel({ {{ -19,19,30,9,4,27 }, { -27,30,30,-17,0,0 }, { -19,19,30,9,4,27 },
                { -20,17,30,52,76,0 }, { 4,25,30,13,-90,0 }} }, 1, pose, weaponHand != 1, 210 - playerLookHeight);
            break;
        case WeaponType::Showel:
            sprite(9);
            tableModel({ {{ 5,33,20,-48,0,0 }, { 5,23,20,-14,0,0 }, { 5,32,51,-22,0,0 },
                { 36,33,51,60,-59,61 }, { 12,31,21,-53,0,0 }} }, 2, std::abs(pose), weaponHand == 1, playerLookHeight);
            break;
        case WeaponType::Scythe:
            sprite(0);
            view.modelSource = WeaponView::ModelSource::Scythe;
            tableModel({ {{ 10,35,32,-32,-60,0 }, { 15,25,32,-22,-70,0 }, { 0,25,32,-22,60,0 },
                { 0,27,32,-30,70,0 }, { 15,35,32,-32,-60,0 }} }, 2, std::abs(pose), true, playerLookHeight);
            break;
        case WeaponType::Bucket:
            model(3, 1, { 0, bounce / 4 + 25 + weaponAnimationSpeed, 35,
                -4 * weaponAnimationSpeed, 0, 0 });
            view.object.modelId = 33;
            view.object.modelFrame2 = 2;
            view.object.blendWeightOrSpriteOpacity = WrapSignedWord(weaponAmounts[9] * 5);
            view.bindWeaponBitmap = false;
            view.projectionCenterY = static_cast<float>(playerLookHeight);
            view.focalLength = 120;
            break;
        case WeaponType::Electricity:
        {
            const int side = weaponHand == 1 ? 1 : -1;
            sprite(5);
            const bool charged = weaponAmounts[7] > 0;
            model(charged ? 2 : 5, charged ? (weaponElectricityFiring ? 4 : 3) : 4,
                { 3 * side, 220 / 7 + bounce + 15 - pose, 65 + bounce,
                    weaponElectricityFiring ? 6 * pose : 0, weaponElectricityFiring ? 100 * side : 0, 0 });
            if (charged && weaponElectricityFiring)
            {
                view.object.modelFrame2 = 5;
                view.object.blendWeightOrSpriteOpacity = -100;
                view.object.displayType = DisplayType::VectorTorchlight;
            }
            view.projectionCenterY = 90;
            break;
        }
        case WeaponType::TrapOrPole:
            if (weaponHand < 1 || weaponHand > inventory.size())
            {
                throw std::logic_error("Placeable weapon requires an inventory hand in 1..14.");
            }
            if (inventory[weaponHand - 1].health != 1000) { return std::nullopt; }
            view.kind = WeaponView::Kind::Model;
            view.object = inventory[weaponHand - 1];
            view.object.posX = 0;
            view.object.posY = WrapSignedWord(220 / 10 + pose);
            if (view.object.behavior == BehaviorType::BonesToGrab ||
                (view.object.behavior == BehaviorType::ActionObjectToGrab &&
                    view.object.velocityX == static_cast<std::int16_t>(CollisionType::VaseToGrab)))
            {
                view.object.posY = WrapSignedWord(view.object.posY + 8);
            }
            view.object.posZ = WrapSignedWord(
                (view.object.behavior == BehaviorType::PortableCoffin ? 10 : 35) + bounce / 2);
            view.object.winX = 10; view.object.winY = 30; view.object.winZ = 0;
            view.object.modelFrame2 = 0;
            view.object.blendWeightOrSpriteOpacity = 0;
            break;
        case WeaponType::Stone:
            if (weaponHand < 1 || weaponHand > inventory.size())
            {
                throw std::logic_error("Stone weapon requires an inventory hand in 1..14.");
            }
            if (inventory[weaponHand - 1].health != 1000) { return std::nullopt; }
            view.kind = WeaponView::Kind::HeldBitmap;
            view.object = inventory[weaponHand - 1];
            view.anchorX = weaponHand == 1 ? 0.125f : 0.875f;
            view.anchorY += weaponAnimationSpeed;
            view.offsetY = static_cast<float>(bounce);
            view.scale = 0.5f;
            view.flipHorizontal = false;
            break;
        default:
            throw std::logic_error("Unrecovered first-person weapon selector.");
        }
        return view;
    }
}
