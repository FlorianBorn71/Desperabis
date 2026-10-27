# Desperabis Game (DX11)

Minimal standalone DX11 window without menus. On startup it loads level 11 and renders it.

Both standalone project and solution builds write to the repository's `Bin` folder:
Release uses `Bin`, and Debug uses `Bin\Debug`.

The project links against `Desperabis-GameCore`. `GameCoreHost.h/.cpp` is the renderer-independent bridge to
`Desperabis::GameContext`:

* Level loading goes through `GameContext::StartLevel` / `CompleteLevelLoad`. The host still supplies
  `FELD<n>.256` geometry and spawn input. GameContext's `LoadLevelObjects` reads `LEVEL\OBJPOS<n>.DAT`
  through the host's file-reader callback and decodes the native object array (record `i` -> slot `i + 2`).
  Malformed or oversized object files fail loading rather than being silently truncated.
  The renderer scene does not create game objects itself.
game ticks (33 Hz, as in the original game)
  back the object states. Changed slots update their `RenderableInstance` via `Level::ApplyRenderProperties`.
* The camera comes from `GameContext::GetPlayerCamera()`: eye position via `Level::DespToPos`, with no extra offset,
  forward = normalize(-dir.x, dir.y, dir.z); a zero direction keeps the previous orientation.
* The player starts at the renderer's initial eye position and settles onto GameCore's terrain. Its collision
  mask is 3, horizontal radius 15, and normal vertical radius 23, matching original save records. At normal
  standing eye height 45, the object center is 20 above terrain, not 33 as with the former placeholder radius.
  The host passes only spawn position/direction through `LevelLoadData::playerSpawn`; GameContext initializes
  the player record and performs eye/center conversion during level completion.

Current assumptions/limitations:

* GameCore only accepts 50x40 geometry; smaller FELD levels are padded into the top-left of the native planes.
* The collision map is a dummy (empty).
* The initial player direction is a host choice (OBJPOS has no player record).
* Light sources created for an object are not removed when the object disappears.

# Controls

* `WASD` / arrow keys: move/turn the player (simulated by GameCore)
* `Enter`: interaction key
  (the window title reports the result and selected native slot/type; supports medicine pickup,
  ordinary doors, grouped switches, and ordinary switches linked to doors)
* `F`: toggle free camera (`WASD` move, `Shift` faster, mouse position relative to the window center steers)
* `Esc` quits
