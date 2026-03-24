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

Level 1 is a focused movement test room for the diver controls.

- The player controls the `diver_swimming` sprite.
- Two yellow fish patrol on fixed horizontal routes, face the direction they are traveling, and bob slightly as they swim.
- Left and right on the D-pad move the diver horizontally with acceleration and braking.
- Up and down on the D-pad do nothing during the level.
- Pressing A gives the diver an upward swim impulse.
- Pressing B gives the diver a downward dive impulse.
- Vertical movement is tap-driven. Each tap creates a short burst that eases back to neutral instead of leaving the diver drifting.
- Repeated taps can be chained together to climb or descend faster.
- The diver has a subtle swimming bob while moving, and an idle forward-back sway while holding position.
- Pressing Start returns to level select.

## Placeholder Levels

Levels after level 1 still use a placeholder screen until their gameplay is implemented. Pressing Start on a placeholder screen returns to level select.
