# Desperabis Game (DX11)

Minimal standalone DX11 window without menus. On startup it loads level 11 and renders it.

Both standalone project and solution builds write to the repository's `Bin` folder:
Release uses `Bin`, and Debug uses `Bin\Debug`.

The project links against `Desperabis-GameCore`. `GameAppContext` in
`DesperabisGame-DX11.cpp` derives directly from `Desperabis::GameContext`:

* Level loading goes through `GameContext::StartLevel` / `CompleteLevelLoad`. The host supplies only
  level number and initial view direction. GameContext's `LoadLevelGeometry` reads `LEVEL\FELD<n>.256` and builds
  the static collision map from material bytes and embedded original 5x5 wall icons.
  `LoadLevelObjects` reads `LEVEL\OBJPOS<n>.DAT` and decodes the native object array (record `i` -> slot `i + 2`).
  Both use the app's `LoadFile` override. Malformed files fail loading explicitly.
  The renderer scene does not create game objects itself.
game ticks (33 Hz, as in the original game)
  back the object states. Changed slots update their `RenderableInstance` via `Level::ApplyRenderProperties`.
* The camera comes from `GameContext::GetPlayerCamera()`: eye position via `Level::DespToPos`, with no extra offset,
  forward = normalize(-dir.x, dir.y, dir.z); a zero direction keeps the previous orientation.
* GameCore selects the exact `0xE1` marker in FELD's wall-texture plane and computes native eye Y as
  terrain height plus 45. It does not use the renderer's approximate start position. Its collision
  mask is 3, horizontal radius 15, and normal vertical radius 23, matching original save records. At normal
  standing eye height 45, the object center is 20 above terrain, not 33 as with the former placeholder radius.
  `InitializeLevelPlayerSpawn` selects position; the host supplies only direction. GameContext initializes
  the player record and performs eye/center conversion during level completion. Recovery tests can
  supply `LevelLoadData::playerSpawn`; normal play always uses the native map marker.
* `LevelStartOverride` is a one-shot player-contact behavior. The original cave object moves the player
  above the floor, consumes itself, and lets normal gravity begin the drop; no Enter press is needed.
* Jump/crouch use the recovered 45/28 body heights, changing collision radius and camera height.
  Low rooms cancel ascent and force crouching. Water contact also forces crouching; submersion
  is detected separately. Swimming/buoyancy, oxygen and other underwater effects remain deferred.
* Awake skeletons use recovered pursuit, wall avoidance, attacks and fixed-point player-life damage.
  Skeleton death creates moving body parts and a pickable sword. `WeaponDamageCollider` (2)
  now resolves hits through the complete shared damage/activation dispatcher, reusing the contact
  as `DriftingParticle` (18) for native impact categories. Ordinary switches activate non-door
  targets too. Dagger/sword input, equip/holster, swing/recovery and contact creation are now
  driven by GameCore. A dagger hits immediately; a sword hits after its wind-up. Held attack
  repeats after recovery. Eligible hand slot 2 takes priority over slot 1; weapon-number
  selection moves the requested weapon into slot 2. Other weapon families and remaining
  dormant-object activation drivers are still incomplete; Enter remains interaction.
  The sample does not yet draw a held-weapon overlay, but world hits/particles are active.
  Player-life depletion installs the core death state and disables normal keyboard movement,
  but the native meter HUD, death camera and restart/menu presentation are not yet available.
* Gameplay prompts from the overridable `GameContext::PromptUserMessage` hook are displayed as
  centered light-orange screen text (`RGB(255, 192, 128)`).
  The game loads `ANIMS\SCHRIFT0.AN2`, `SCHRIFT1.AN2`, and `SCHRIFT2.AN2` with the desktop
  sample's grayscale palette, with character mappings keyed by the original DOS bytes.
  Messages retain those bytes through storage and glyph lookup, including German umlauts;
  no UTF-8 or Windows code-page conversion is applied.
  `GameAppContext::PromptUserMessage` owns the text and countdown and updates the text mesh directly,
  with no host adapter, forwarded callback or message polling. The core queries the app's active
  state only to gate locked-door feedback. Advanced frame ticks control visibility without an
  additional wall-clock timer or fade. Replacement, refresh, expiration, level changes and viewport
  resizing are reflected by the presenter. Interaction diagnostics still
  appear in the window title; they are separate from native gameplay prompts.

Current assumptions/limitations:

* FELD geometry up to 50x40 uses the recovered native row placement and outside-file defaults.
  Static collision is built on loading; doors, pillars and wall destruction mutate the same map.
  Core wall/floor-object changes increment `Level::geometryRevision`, but this sample does not
  yet rebuild its cached renderer meshes after those changes. Follow-on target behaviors that
  remain unrecovered still report their own unsupported status.
* The initial player direction is a host choice (OBJPOS has no player record).
* Light sources created for an object are not removed when the object disappears.

# Controls

* `WASD` / arrow keys: move/turn the player (simulated by GameCore)
* `Enter`: interaction key
  (the window title reports the result and selected native slot/type; supports medicine pickup,
  ordinary doors, grouped switches, and ordinary switches linked to damage/activation targets)
* `Ctrl` or left mouse: attack with the equipped weapon (normal player camera only)
* `1` / `2`: select dagger / sword from inventory (normal player camera only)
  The weapon must be available and its current animation idle; selection uses native holster/draw timing.
* `Space`: jump, or stand up first when crouched (normal player camera only)
* `C`: crouch; release keeps the stance, as in the original. Use `Space` to stand.
* `F`: toggle free camera (`WASD` move, `Shift` faster, mouse position relative to the window center steers)
* `Esc` quits
