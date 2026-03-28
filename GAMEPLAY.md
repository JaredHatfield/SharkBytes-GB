# Gameplay

## Title Screen

On boot, the game displays a title screen with the `Shark Bytes` logo text and a scaled-up standing diver sprite. After a short delay, it automatically advances to the level select screen.

## Level Select

The level select screen shows the game name at the top, a `Level Select` heading beneath it, and a vertical list of levels with a small triangle cursor indicating the current selection.

There are 10 levels in the list:
- Level 1 is named `Bubble`.
- Levels 2 through 10 are named `TBD`.

The caret can move up and down through the full list, including locked levels. Only unlocked levels can be entered. At the start of a new run, only level 1 is unlocked. Future levels remain locked until the previous level has been beaten.

Pressing A or Start on the selected unlocked level opens that level. Locked levels cannot be entered.

## Level 1: Bubble

Level 1 is now a basic playable catch-and-survive room driven by [`levels.json`](./levels.json).

- The player controls the `diver_swimming` sprite.
- Fish spawn from the level definition up to the configured on-screen limit. Level 1 uses a pool of clown and puffer fish, keeps two fish on screen at a time, and requires catching 10 fish to clear the stage.
- Each fish type defines its sprite kind, swim speed, spawn lane, starting side, lifetime before it swims away, and how long each animation frame is shown.
- New fish do not spawn beside the diver. They enter from the opposite side of the screen and use the authored lane or its mirrored counterpart, whichever places them farther above or below the player.
- Left and right on the D-pad move the diver horizontally with acceleration and braking.
- Up and down on the D-pad do nothing during the level.
- Pressing A gives the diver an upward swim impulse.
- Pressing B gives the diver a downward dive impulse.
- Vertical movement is tap-driven. Each tap creates a short burst that eases back to neutral instead of leaving the diver drifting.
- Repeated taps can be chained together to climb or descend faster.
- The diver uses the authored animated swimming sprite. The level definition controls how long each animation frame is shown before switching to the next pose.
- The diver has a subtle swimming bob while moving, and an idle forward-back sway while holding position.
- The top HUD uses the background layer and divider line. Hearts are on the first row and air bubbles are on the second row, with empty bubble placeholders showing the full air capacity.
- Health always has a hard cap of 3 hearts and air always has a hard cap of 10 bubbles. Each level chooses the starting amount for both values.
- Level 1 currently starts with full health and has no damaging hazards yet, so health does not drop during this stage.
- Air drains at the level-defined cadence. Rising air bubbles also spawn at the level-defined cadence and move at the level-defined speed. Touching one restores the configured amount of air, which is currently one bubble per pickup in level 1, up to the cap.
- The bottom HUD uses the background layer and a divider line to show caught fish. Ten left-aligned slot placeholders are always visible, and each slot is replaced by the matching fish type icon when that fish is caught.
- Catching the level-defined goal number of fish clears the level and unlocks the next one.
- Running out of air fails the level.
- Pressing Start returns to level select.
- After clearing or failing the level, only Start advances past the result screen and returns to level select.

## Level Data

`levels.json` is the authored source of truth for balancing values and level content. The build converts it into C data before compiling the ROM.

- Each level can define whether it is implemented, its display name, starting hearts, starting air, goal fish count, air bubble behavior, and fish spawning behavior.
- Player settings define the diver animation cadence for that level.
- Air settings define whether bubbles are enabled, how much air a pickup restores, how often air drains, how often pickups spawn, and how quickly they rise.
- Fish settings define the maximum fish on screen plus the list of fish types that can spawn for that level.
- Each fish type defines its sprite kind, swim speed, lifetime, lane, patrol bounds, starting side, and animation cadence.

## Placeholder Levels

Levels after level 1 still use a placeholder screen until their gameplay is implemented. Pressing Start on a placeholder screen returns to level select.
