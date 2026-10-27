# Desperabis Desktop Player

This is a standalone executable that renders Desperabis on your Windows Desktop using a DX-11 based renderer but based on original assets.
It mimics the original rendering features such as billboarding but adds modern capabilities where necessary, e.g. real water reflections
or fully dynamic lighting.

Game code is not executed and sounds are not played. Instead, this app is just for navigating through all existing levels of Desperabis.

After starting this app, a window opens that renders the trainings level.
Each level starts with a centered `Level xx` prompt, held for three seconds before fading out over one second.
Prompts use the original font at 2x scale and stay centered when the main window is resized; the map window does not show them.
Font animations use a grayscale palette (indices 0..7 become coverage 0..1; higher indices saturate to full coverage),
so vertex/instance colors control text color.
`AssetManager::LoadAsset` accepts an optional shared palette for indexed textures and animation frames; omitting it uses
the default palette. Assets remain cached by filename: supply the intended palette on the first load and do not load
the same file with different palettes.
Text mesh instances default to a scale of 2.0, adjustable through `SetScale`.
`Font::CreateGlyphMesh` defaults to left/top alignment; use `Font::HorizontalAlignment` (Left, Center, Right)
and `Font::VerticalAlignment` (Top, Center, Bottom) to choose the mesh anchor. Prompts explicitly use Center/Center.

The app's `PromptMessage(txt, duration)` replaces the current prompt. `duration` is the hold time in seconds,
followed by a one-second fade. An empty message clears the prompt.

# Controls

Within the window, you can navigate the free camera with the `WASD` keys and look around with the mouse.
You can also use the mouse for the menu bar, where you can change the level, camera modes, and a few rendering capabilities.

