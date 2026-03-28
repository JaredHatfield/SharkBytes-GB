# SharkBytes-GB

Purpose: a small Game Boy project built with GBDK-2020 for `Shark Bytes`.

Ensure that GAMEPLAY.md and the AGENTS.md file are updated with the key files, workflow, and constraints for this project. This will help future contributors understand the codebase structure and development process.

Key files:
- `src/main.c` is only the hardware/bootstrap entry point.
- `src/app.c` owns the top-level screen state machine and app flow.
- `src/front_end.c` owns title, level-select, placeholder, and result screens.
- `src/level_scene.c` owns active level gameplay, runtime state, collisions, HUD, and level rendering.
- `src/display_helpers.c` holds shared low-level display helpers used across scenes.
- `src/levels/*.json` are the authored source of truth for level content, one file per level in numeric order.
- `src/levels_generated.c` and `src/levels_generated.h` are generated from `src/levels/*.json`.
- `src/sprites/*.piskel` are the authored source of truth for official gameplay sprite art.
- `src/sprites/*_sprite.c` and `src/sprites/*_sprite.h` are generated from the authored `.piskel` files.
- `src/sprites/hud_placeholder_tiles.c` and `src/sprites/hud_placeholder_tiles.h` contain the intentionally separate placeholder HUD art such as hearts and air bubbles.
- `scripts/piskel_to_gbdk_sprite.py` batch-generates runtime sprite assets from the sprite directory.
- `scripts/levels_to_c.py` batch-generates level C data from the numbered level JSON files.
- `justfile` contains the main developer commands.

Workflow:
- Keep `main.c` thin. New gameplay or screen behavior should usually go into `app.c`, `front_end.c`, or `level_scene.c` instead of growing `main.c`.
- Edit sprite art in `.piskel` files, not in generated `.c` or `.h` files.
- `just sprites` runs the sprite generator in batch mode. It discovers the runtime `.piskel` files itself rather than relying on individual file references in the `justfile`.
- `diver_standing.piskel` is generated as the enlarged `diver_title_sprite` asset for the title screen.
- `diver_front.piskel` exists as source art but is not part of the runtime batch generation path.
- `just levels` reads `src/levels/*.json` in numeric filename order, so `01.json` is level 1, `02.json` is level 2, and so on.
- Each level JSON file should contain one level object, not a wrapper array.
- `just build` regenerates sprites and levels, then builds the ROM.
- When gameplay flow, controls, progression, or screen behavior changes, update `GAMEPLAY.md` in the same change.

Constraints:
- Sprite dimensions should be multiples of 8 pixels.
- The current converter targets Game Boy OBJ sprites, so art must use at most 3 opaque colors plus transparency.
- The official gameplay sprites are the authored `.piskel` assets for the title-screen diver, swimming diver, and fish. Placeholder HUD art should stay separate from that pipeline unless it is intentionally promoted to authored sprite art.
