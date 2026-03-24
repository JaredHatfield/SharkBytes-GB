# SharkBytes-GB

Purpose: a small Game Boy project built with GBDK-2020 for `Shark Bytes`.

Key files:
- `src/main.c` boots the ROM and draws the title screen.
- `src/sprites/diver.piskel` is the source of truth for the diver's sprite art.
- `src/sprites/diver_sprite.c` and `src/sprites/diver_sprite.h` are the generated base-size sprite assets.
- `src/sprites/diver_title_sprite.c` and `src/sprites/diver_title_sprite.h` are the generated enlarged title-screen sprite assets.
- `scripts/piskel_to_gbdk_sprite.py` converts `.piskel` art into GBDK-ready C code.
- `justfile` contains the main developer commands.

Workflow:
- Edit sprite art in `.piskel` files, not in generated `.c` or `.h` files.
- Run `just sprites` to regenerate both normal-size and title-screen sprite code from `src/sprites/mint.piskel`.
- `just build` regenerates sprites and then builds the ROM.
- When gameplay flow, controls, progression, or screen behavior changes, update `GAMEPLAY.md` in the same change.

Constraints:
- Sprite dimensions should be multiples of 8 pixels.
- The current converter targets Game Boy OBJ sprites, so art must use at most 3 opaque colors plus transparency.
