# Desperabis Game (DX11)

Standalone DX11 host with core-owned nonblocking gameplay, inventory and a
first-pass main menu. On startup it loads the training level (50).

Flame markers (`TrackFireAnimation`, 202) load `FACKEL<n>.AN2` into reserved
texture slots 13..15. Native sprite bank 6 uses its one-based animation row to
select those slots, including ordinary transparent sprites in the tutorial;
torch-display sprites and model materials share the same animation bindings.
Sprite updates retain their explicit material instead of overwriting it with
level wall textures. Animation bindings survive material-vector replacement,
are refreshed after core-state presentation updates, and are removed with their
renderable instances. The existing renderer supplies independent flame phases
and its approximate animation speeds; this is not DOS-frame-exact timing.

Both standalone project and solution builds write to the repository's `Bin` folder:
Release uses `Bin`, and Debug uses `Bin\Debug`.

## CPU profiling

The game exposes `g_profilingStats` alongside the renderer's existing visibility
and rendering timings. Inspect it in the debugger; durations are **microseconds**:

* `m_duration_game`: time spent inside the current frame's `GameContext::UpdateApplication`,
  including page input, optional gameplay ticks and synchronous callbacks.
* `m_max_duration_game`: largest recorded application-update duration since startup or `ResetMax()`.

The scope excludes input polling, `Scene::Update`, post-tick `SyncGameObjects`,
rendering and swap-chain presentation. A catch-up call can simulate several native
ticks; the measurement is the whole call, not a per-native-tick average.
Existing visibility/rendering timings share the same stats object.

## Audio and asset loading

The host installs an XAudio2-backed `AudioPlayer` through GameCore's existing
`AudioHandler` interface. GameCore alone resolves `S_TAB.PRB`, draws random
sample variants, computes volume and admits sounds to the four native channels.
The backend receives a **one-based sample index**, zero-based channel and
volume 0..255; it does not interpret sound-effect IDs or use an RNG.
The supplied volume is a fixed linear gain (`volume / 255`) for the entire sample;
moving the listener does not change an already-playing sample's volume.

Environmental ambient audio uses a separate, infinitely looping XAudio2 voice,
not one of the four effect channels. GameCore reads `LEVELS.INF` byte 100 and
chooses the original indoor, rain, forest, morning/crows or burning-forest recipe.
It owns all random placement/weight decisions; `AudioPlayer` owns the PCM.
The creation API allocates 1,800,000 mono frames (bytes), copies the base sounds,
blends overlays with `(existing*weight + source*(255-weight)) >> 8`, and finalizes
the actual loop length separately. Silence gaps are initialized to 128.
Generation uses the same prepared/faded samples as effects, without changing
the entire-file PCM policy below; it is not claimed to be DOS-byte-identical.
Music/cutscene selectors 100..150 are explicitly unsupported.

All 129 `SAMP\SOUND<n>.SMP` files are `SoundSample` resources loaded and cached
through `AssetManager`. Match the browser's original loader: **the entire file**
is unsigned 8-bit, mono PCM at **11025 Hz**. No bytes are interpreted as a header
or removed, even if leading bytes resemble a frequency or length. XAudio2 handles
resampling to the output device. Playback is centered mono; music WAV files and
additional spatialization are not implemented.

At load time, a separate playback buffer receives short linear fade-in and fade-out
envelopes around unsigned PCM silence (128), reducing clicks at both sample ends. The raw
buffer and embedded files remain byte-exact; no leading bytes are removed.
`AssetSettings::Instance().soundFadeInMilliseconds` and `soundFadeOutMilliseconds`
configure the durations independently (both default to **5 ms**, **0** disables
that fade). Set them before loading sound assets;
already-cached resources retain their prepared buffers. The fade is clamped to
the sample length and does not add a playback delay or change sample length.
Overlapping fades use the lower gain, applied once to raw PCM, so even short
samples start and end at silence. Negative/nonfinite durations
are rejected. Water-drop variants 5 and 38 both have strong six-byte onset
transients; tests verify the default fade reduces these to at most 12 PCM units
from silence, versus up to 128 in the originals.

`BuiltInSAMP.cpp` embeds those samples and `SAMP\S_TAB.PRB`, so no external sample
directory is needed. To regenerate from the repository root, build
`GenerateBuiltInCpps`, then run:

```powershell
.\Source\NewRenderer\GenerateBuiltInCpps\x64\Debug\GenerateBuiltInCpps.exe --samples `
  Data\SourceAssets\Original\SAMP `
  Source\NewRenderer\SharedSource\IO\builtin\BuiltInSAMP.cpp
```

The backend retains shared sample resources until its voices are destroyed.
Level changes and shutdown stop all sounds and clear core channel ownership.
Because XAudio2 stop/flush is asynchronous, `StopAllSamples` destroys the source
voices synchronously; subsequent playback recreates only the channels it needs.
The output voice/engine remain alive. Initialization/API errors are explicit;
audio initialization failures are shown in the startup error dialog.
`StopAllSamples` affects only effects; `StopAmbientPlayback` destroys only the
dedicated looping voice. The game calls both for level changes and shutdown.
Starting a new creation pass also destroys any old ambient voice before its
buffer is replaced. Creation/placement/finalization errors fail level loading
through the existing host error reporting.

Asset loading uses `CO_AWAIT`, which is empty in synchronous builds. The synchronous
core file-loading override uses `FORCE_GET`; `FORCE_WAIT` consumes void results at
startup and existing synchronous host boundaries. Both helpers use `.get()` in
async builds so exceptions are not discarded. No per-call `D_ASYNC` branches are
needed. To build this game or its audio tests in async mode, pass
`/p:DesperabisAsyncAssets=true` to MSBuild; the game and BaseLibrary use the same
`D_ASYNC` definition and existing MSVC C++17 `/await` support.

Audio integration tests (require a working Windows audio output device):

```powershell
msbuild Source\NewRenderer\DesperabisGame-DX11\AudioTests.vcxproj /p:Configuration=Debug /p:Platform=x64
.\Source\NewRenderer\DesperabisGame-DX11\x64\Debug\AudioTests\AudioTests.exe Data\SourceAssets\Original\SAMP
```

Repeat with `/p:DesperabisAsyncAssets=true` to exercise async loading. Tests compare
every built-in PCM byte with the original, verify asset caching and invalid-input
errors, exercise all four XAudio2 channels, natural completion, immediate stop/reuse,
resource ownership after cache teardown, core RNG/channel dispatch and await-result/
exception propagation. A volume-meter test checks actual mixed-output gain at
0, 64, 128 and 255 throughout playback and after channel reuse, using a quiet test
tone. Other direct playback checks are muted; the core-dispatch check uses volume 1/255.
Ambient tests verify exact PCM blending and overlap ordering, buffer bounds and
creation-state errors, logical length versus allocation size, actual loop
advancement, independent effect/ambient stops, rebuilding during playback, and
all five core recipes using the original cached sample resources.

## Gameplay integration

### Application flow and inventory

The host forwards F1/F2, UI arrows, Enter and Escape through `GameInput`, but
does not interpret their actions. `UpdateApplication(elapsedSeconds, input)`
runs every frame; GameCore checks shortcuts and dispatches the current page.
It owns the 33 Hz accumulator and discards modal/pause time, including fractional
catch-up. `worldDeltaSeconds` advances scene/free-camera animations and renderer
shader time only during gameplay. Native object ticks and message countdowns
also freeze in inventory/main menu. Keys held across page transitions must be
released before they can affect gameplay again.

The core supplies a 360x240 drawing recipe through `GetApplicationPresentation()`:
layout, labels, resource availability shades, source pulse and item picture
references all originate there. `ApplicationUi` resolves assets and executes
the commands with aspect-preserving canvas scaling/letterboxing. It caches
screen meshes, handles stored bitmap and model pictures separately, and does
not modify shared renderable metadata/materials. No shared renderer API was added.
Model pictures use the supplied fixed pose/depth, not the corner preview's
rotating/bounds-fitting policy.

Inventory supports separate focus/source, complete-record swaps (including
nonweapons in hand slots), self-swap cancellation, Back/Escape, and Use on the
selected medicine. It retains the original potion's final-dose refund and
suppression behavior. Its 512-phase source pulse advances at the port's 33 Hz
UI rate; native wall-clock timing has not been measured.

Inventory uses the original `AMA_ANI.AN2` frames, hand/Back/Use artwork and
chapter-specific `ANIM4.AN2`/`ANIM12.AN2` resource pictures, at their recovered coordinates and natural pixel
sizes. The right hand is mirrored as in the original. Focus, pulse and
unavailable icons use `LEVEL\OPSET10.256`, not RGB-tint approximations.
The health meter retains the original gradient, fill threshold and beveled
palette shading; numbers use the recovered percent glyph and formatting.
Inventory text also preserves native glyph Y offsets, pen-X adjustment and
character advances, so the health caption clears the bar and resource numbers
do not use the wider modern spacing. Main-menu and prompt text are unchanged.
All extra texture variants live in `ApplicationUi`, not shared renderer data.
Model projection/centering and the disabled electricity-model tint still use
modern approximations; native item descriptions/preview animation remain open.
The main menu offers Resume/Quit and explicitly identifies the other choices
as unimplemented. Book sequences, cutscenes, chapter/new-game flow, settings,
save and load remain future nonblocking pages. Tab is still a development
level-switching shortcut.

Headless presentation tests use D3D11 WARP plus the debug layer; no window,
keyboard injection or audio device is needed:

```powershell
msbuild Source\NewRenderer\DesperabisGame-DX11\ApplicationUiTests.vcxproj /p:Configuration=Debug /p:Platform=x64 /nologo
.\Source\NewRenderer\DesperabisGame-DX11\x64\Debug\ApplicationUiTests\ApplicationUiTests.exe
```

They render original key/medicine and cheat-weapon pictures at native, wide
and portrait sizes, verify natural-size hand artwork, horizontal mirroring,
palette-state changes, both chapters' resource/model banks, native text metrics,
health-caption separation at all tested aspect ratios, the health meter's bounds, cache stability and
shared-material isolation, and check for D3D11 error/corruption messages.
An optional output BMP path captures the native-size inventory for inspection:
`ApplicationUiTests.exe <output.bmp>`. Core behavior is covered by CTest's
`application-update` and existing `application-flow`, `game-runtime`,
`weapon-sequences` and `player-health` tests.

### World integration

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
* GameCore schedules game ticks (33 Hz, as in the original game).
  Changed slots update their `RenderableInstance` via `Level::ApplyRenderProperties`.
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
  is detected separately. Underwater play remains bottom walking/crouching;
  oxygen and other underwater effects remain deferred, not free-swimming controls.
* Walking now uses native steep-slope correction, bobbing and collision-47 bridge support.
  Planks remain walkable before Lentils reveal them; activated drawbridges become walkable
  once fully opened. Footsteps follow native movement cadence and landing events, select
  floor/bridge-specific sounds, and are suppressed underwater. They share the existing
  effect channels with a two-instance limit and use load-time sample fades.
* Teleporters support Enter-to-unlock/open/use, native automatic-contact gates,
  destination eye height/heading and departure cooldown. Ladder behaviors 75/133
  and behavior 184's wall-bypass volume are implemented. The special vertical
  mode belongs to activated ladders, not water. Vehicle controllers, several
  contact hazards and teleporter-specific DOS render-table/lighting effects
  remain incomplete; behavior 178's suspected foil viewport effect is unproven.
* Awake skeletons use recovered pursuit, wall avoidance, attacks and fixed-point player-life damage.
  Indoor bats use recovered flight, wall lookahead, ceiling roosting/waking,
  wing animation, positional sound and weapon-hit falling/corpse states.
  Ordinary mummies use recovered pursuit, alternating movement, contact damage,
  packed walk/attack animation-bank blending, staged collapse and smoke/despawn.
  Final-level mummy guards count down player overlap, then open and clear collision.
  Originally paused mummies retain their native health/state >1000 gate until
  an activation volume wakes them. Level 12's invisible trigger now activates
  its three group-1 mummies automatically on player overlap.
  Large spiders (including white ceiling spiders), harvestmen, devils and gorillas
  now use their native object-local movement/attack/death states. Spider spit uses
  the original model's second animation bank, half-rate gravity and a dissolving
  impact puddle; no substitute texture or global enemy state is needed.
  Gorillas share mummy packed walk/attack-bank blending but leave permanent corpses.
  Disappearance display modes 30..35 are forwarded, but their original shading
  is not yet reproduced by the renderer's generic 30..37 handling.
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
  pieces; player support is independent of their visibility.
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
* The selected pickable contact has a bottom-right preview above the health bar.
  Selection follows `playerCollisionSlot`; inactive/non-pickable contacts, death
  and free-camera mode hide it. Collectibles that do not require a pickup flag
  (poison, medicine, gold and maps) are included. Picking an item up removes its
  preview immediately, without retaining its world slot or changing gameplay.
  3D items share the world's assets/materials but use a separate instance, a
  sphere-fitted orthographic camera and a slow independent rotation with a fixed
  camera-space tilt of about 17 degrees to show their depth. Sprites
  remain flat, preserving their aspect ratio and atlas mapping.
  Models consisting of one billboard (such as the small key) use that same
  bitmap-fitting path instead of their conservative world-culling sphere,
  so their pictures occupy comparable space without changing world bounds.
  The preview detects these on demand from the existing geometry counts
  (one anchor position, one polygon, one two-triangle draw call, no submodels);
  no extra flag is stored on the shared renderable.
  Bitmap previews fill two-thirds of their available rectangle, centered with
  their aspect ratio preserved; rotating 3D model fitting is unchanged.
  `RendererDX11::SetItemPreview` retains a shared instance and viewport for
  one render. HUD geometry uses the statically dispatched render context, not a
  renderer virtual hook. The pass runs after world transparency, water and
  depth-reading coronas, but before text/health. For 3D previews it clears and
  reuses the main depth buffer (not its color), retaining normal self-occlusion.
  No world-depth consumer may be scheduled after this pass. Copied preview
  materials use full-bright rendering with a neutral white tint, retaining
  original texture/vertex colors and opacity but ignoring world brightness,
  light maps, fog and lights. World instances and shared materials are unchanged.
  Ambient is uploaded normally by `TrackGlobals`; full-bright is an explicit HUD
  policy, not a workaround for missing ambient tracking.
  This is modern presentation, not a reconstruction of native preview timing.
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
  including tutorial texture-row prompts. Backslashes split independently centered lines at
  20 native pixels; negative durations place the block above the health bar. Long prompts
  scale down to fit the viewport width, including portrait windows.
  Glyph generation interprets DOS byte `0xFB` as dark red (`RGB(128, 0, 0)`) and
  `0xFA` as restoring the caller's color. Controls consume no glyph width; the
  style persists across line breaks but never leaks into another message.
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
* `F1`: open the main menu; arrows/Enter choose Resume or Quit.
* `F2`: open inventory; arrows navigate, Enter selects/moves/swaps.
  Select medicine, then choose Use to consume it; Back or Escape returns to gameplay.
* `Esc`: leave inventory/main menu, not window shutdown. Quit through the main menu
  or use the window close button/Alt+F4.
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
* `N`: toggle the player collision cheat (`GameContext::DEBUG_noPlayerCollision`).
  When enabled, player wall collisions and all player damage are suppressed;
  pickups/interactions and healing remain enabled. The HUD shows `Player collision: off/on`.
  The flag defaults to false and persists across level changes. Key repeat is ignored.
* `Tab`: load the next playable level in DesktopPlayer menu order:
  training `50`, part 1 `30, 31, 32, 33, 36, 34, 35`, then part 2
  `10, 11, 12, 13, 14, 15, 16, 17, 18, 90, 19, 20, 21, 22, 23, 24, 25, 26`.
  Reloads the renderer scene and gameplay state, reapplies the cheat loadout when enabled,
  and resets the camera to the new start. Holding the key does not skip multiple levels.
  Startup is training (50), followed by part 1 and then part 2. At the final entry (26), navigation stops without
  wrapping and the window title reports that no next level exists.
* `Shift+Tab`: jump to the next game part: training -> part 1 (30) -> part 2 (10).
  From any level within a part, skips its remaining levels. In part 2, stops
  without wrapping and reports that no next game part exists. Key repeat is ignored.
Development shortcuts `F`, `N`, `Tab` and `Shift+Tab` are available only in gameplay.
