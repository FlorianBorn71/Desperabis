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
  GameCore also reads the level's 124-byte `LEVEL\LEVELS.INF` record for all four
  surface depth/value pairs and repeated-floor height. Water contact previously
  failed mid-tick without this metadata, leaving camera rotation stale.
  These loaders use the app's `LoadFile` override. Malformed files fail loading explicitly.
  The renderer scene does not create game objects itself.
game ticks (33 Hz, as in the original game)
  back the object states. Changed slots update their `RenderableInstance` via `Level::ApplyRenderProperties`.
* The camera comes from `GameContext::GetPlayerCamera()`: eye position via `Level::DespToPos`, with no extra offset,
  forward = normalize(-dir.x, dir.y + (110-lookHeight)*horizontalLength/100, dir.z);
  a zero direction keeps the previous orientation. `lookHeight` is the core's native
  screen horizon, shared with weapon aiming. Converting its displacement into
  rotational perspective pitch is a modern host approximation; the original shifted
  the screen horizon rather than rotating a 3D camera.
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
  selection moves the requested weapon into slot 2. Crossbow, fireball, stone,
  electricity, shovel and scythe attacks are also driven by GameCore, as are
  Lentils throws and bear-trap/metal-pole placement. Scythe hits deform hedges
  and clear their blocking map pixels; Lentils reveal individual invisible bridge
  pieces. Walking on bridges still requires the separate player-support recovery.
  Other placeable tools, extra item IDs 9/23 (bucket and foil),
  and remaining dormant-object activation drivers are still incomplete;
  unsupported placement leaves the item intact. Enter remains interaction.
  The host supplies stone sprite-bottom metadata from loaded level assets and
  presents fireballs using the ten original `ANIMS\SCHUSS1.AN2` images.
  Terrain-impact breakup flames now use recovered `BouncingObject` physics,
  size flicker and dynamic light, then `FadingParticle` opacity on settling.
  Their sprite bounds and images come from the level-mapped common animation bank.
  The sample does not yet draw a held-weapon overlay, but world hits/particles are active.
  Player-life depletion installs the core death state and disables normal keyboard movement,
  but the death camera and restart/menu presentation are not yet available.
* A full-width health track is anchored to the viewport bottom. The default-empty
  core hook `OnPlayerHealthChanged(previousLife, currentLife)` is overridden to
  update it immediately after actual life changes; no per-frame health polling
  or rounding to whole health points is used. It is also built on level load and
  rebuilt on resize, while health itself persists across ordinary level changes.
  The texture is the original 90x2 `ANIMS\AMA_ANI.AN2` frame `(2,1)` (one-based),
  using the default `BACKGR1.PAL`. Native maximum health is 360, not the texture's
  90-pixel width. Filled width and UVs are cropped by current/max health, so low
  health leaves the red end visible; the empty track stays dark even at zero.
  Height is one quarter of the original texture-aspect height at the current screen width.
  `TextMesh::CreateTexturedQuad` supplies regular textured screen-space triangles
  to the existing text shader, using RGBA sampling rather than the font batches'
  red-channel coverage mode. Atlas scale/bias is baked into the vertices once,
  before the shader samples the shared atlas; no second UV transform is applied.
  Quad UVs are inset to texel centers. The health fill samples only the first
  pixel row of the 90x2 frame, preventing filtering into its different second row.
* Gameplay prompts from the overridable `GameContext::PromptUserMessage` hook are displayed as
  centered light-orange screen text (`RGB(255, 192, 128)`).
  The game loads `ANIMS\SCHRIFT0.AN2`, `SCHRIFT1.AN2`, and `SCHRIFT2.AN2` with the desktop
  sample's grayscale palette, with character mappings keyed by the original DOS bytes.
  Messages retain those bytes through storage and glyph lookup, including German umlauts;
  the original letter banks end in o-umlaut, u-umlaut, a-umlaut order.
  no UTF-8 or Windows code-page conversion is applied.
  `GameAppContext::PromptUserMessage` owns the text and countdown and updates the text mesh directly,
  with no host adapter, forwarded callback or message polling. The core queries the app's active
  state only to gate locked-door feedback. Advanced frame ticks control visibility without an
  additional wall-clock timer or fade. Replacement, refresh, expiration, level changes and viewport
  resizing are reflected by the presenter. Interaction diagnostics still
  appear in the window title; they are separate from native gameplay prompts.

Current assumptions/limitations:

* The host's global `g_cheatMode` defaults to `true`. After every level load it grants missing
  dagger, sword, crossbow, fireball, electricity and stone items using original OBJPOS templates.
  It refills crossbow ammo to 10 arrows, electricity to 360 (100%), and fireballs to 32,767
  (the maximum positive native signed-word charge count, including the item-local counter).
  Existing inventory items are preserved; other weapons remain level-specific.
  Set `g_cheatMode = false` to leave inventory and ammo unchanged on level load.
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
* `1`..`8`: select dagger, sword, crossbow, fireball, stone, placeable item,
  electricity, or shovel from inventory (normal player camera only)
* `9`: select scythe; `0`: select Lentils from inventory (sample bindings)
  The weapon must be available and its current animation idle; selection uses native holster/draw timing.
* `Space`: jump, or stand up first when crouched (normal player camera only)
* `C`: crouch; release keeps the stance, as in the original. Use `Space` to stand.
* `Page Up` / `Page Down`: look up/down; `Home`: center manual look (sample bindings).
  Look is smoothed and also follows the terrain ahead, affecting arrow launch elevation.
* `F`: toggle free camera (`WASD` move, `Shift` faster, mouse position relative to the window center steers)
* `Tab`: load the next supported level in numeric order, skipping unused IDs.
  Reloads the renderer scene and gameplay state, reapplies the cheat loadout when enabled,
  and resets the camera to the new start. Holding the key does not skip multiple levels.
  At the last supported level, the window title reports that no next level exists.
* `Esc` quits
