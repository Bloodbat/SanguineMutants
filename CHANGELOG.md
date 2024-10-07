# 2.5.0

## New Modules

### Aestus

Based on MI's Tides, it fixes PLL mode, separates it from the clock input; restores switching banks using the clock input when module is using the Sheep firmware; fixes the inverted "Mode" light colors; fixes the broken "High" and "Low" outputs, and, hopefully, fixes a long-standing crash that occurs under specific circumstances.

### Temulenti

Based on the Parasite firmware for MI's Tides: includes every Aestus fix, additionally: restores functionality when using the clock input in Two Bumps or Two Drunks modes; Sheep model can be used.


# 2.4.2

## Additions

- Theme support for existing modules with two available options: "Vitriol" the usual, colorful faceplate or "Plumbago" a black as night variation.

- Polyphonic ports are now shown with golden jacks.

- Nodi and Contextus show the selected mode by name in the knob's tooltip.

- Funes' menu is prettier and includes an entry to visit the data editors.

- A pretty circle of LEDs that show the selected mode for Distortiones and Mutuus.

## Fixes

- Plugin no longer crashes Rack Pro when it is used as a guest of hosts such as Reaper and Ableton and patches are loaded.

- Plugin no longer crashes Rack when used headless.

- Etesia, Fluctus and Nebulae LEDs are no longer as prone to get stuck in parameter mode when fiddling with the knobs.

- Faceplate tweaks to prevent rendering errors.

- Apices' and Mortuus' inputs follow the hardware spec more closely.

- Mortuus ByteBeats are more stable and every equation is interesting again.

- Display rendering order.

## Changes

- Etesia and Distortiones use the Parasites lib internally instead of MI's one.

- Better module tags, some are from modulargrid, others from functions I know they have (and now you should too :))

- Funes' Harmo input move to a location less prone to confusion (also, a connecting line has been added between the port and the Harmonics knob).


# 2.4.1

## Changes

- Mode selector encoder knobs for Nebulae and its descendants.


# 2.4.0

## New Modules

### Fluctus

Based on the Kammerl Beat-Repeat firmware for Clouds.

### Incurvationes

Based on MI's Warps, it has proper knob lights colors and enables access to the Frequency shifter "Easter egg".

### Distortiones

Based on the Parasite firmware for Warps, it no longer crashes when multiple instances are used (and they don't interfere with each other...); it also includes a mode selector similar to the one used for the hardware module.

### Mutuus

Based on the Symbiote firmware for Warps, it includes the improvements in Incurvationes and Distortiones.

### Explorator

Based on MI's Kinks and Links, they are combined in a single module.

The Kinks section is now fully polyphonic. The Links 3:1 section can optionally behave as the hardware module and average the connected voltages. All LEDs now reflect when polyphonic channels are used.

### Marmora

Based on MI's Marbles, it exposes every control and adds missing features like user defined scales; super lock; t and X reset; restores missing light animations and showing polarity in the output port LEDs.

### Anuli

Based on MI's Rings, it restores missing light animations; the "3" polyphony mode... and is completely polyphonic.

### Velamina

My attempt at recreating Veils... the 2020 version. It is polyphonic and saturates outputs beyond 10V.

## Additions

- More hardware-like LEDs for Nebulae and its descendants.

- More hardware-like LEDs for Funes.

- Frequency mode (range) to Funes faceplate.

- Nodi and Contextus are now polyphonic.

- LED markers for some modules.

- Leave displays on in module browser.

- Funes polyphonic channel count can also be set by the trigger input.

- Nicer context menus for mode selection (for certain modules and modes).

## Changes

- Different alternative firmwares use different face plate colors.

- Mode tooltips for Funes' select knob.

- Shaped lights don't turn off in module browser.

- Mortuus ByteBeats equation 6 regained some of its complexity.

- Displays are shown in module browser.

## Fixes

- Stop Funes throat singing when trigger input is patched; VOWLSPCH is selected, and first trigger hasn't been received.


# 2.3.2

## Changes

- Display values for certain knobs.

## Fixes

- Better CV trigger handling for Apices and Mortuus.

- Apices and Mortuus: Tap LFO.

- Mortuus: P.L.O.

- Nebulae time stretcher.

- Renaissance spelling.

- Aleae reset.


# 2.3.1

## Changes

- Changelog organization.

## Fixes

- Contextus crashing Rack.


# 2.3.0

## New Modules

### Nodi

Based on MI's Braids.

### Contextus

Based on the Renaissance firmware for Braids.

### Nebulae

Based on Clouds and Southpole's Smoke, inspired by the Monsoon variant.

### Etesia

Based on the Clouds Parasite firmware.

### Mortuus

Based on the Dead Man's Catch firmware for Peaks.

## Additions

- Make Clouds based modules lights behave a little closer to their hardware counterparts.

## Changes

- Move manuals to their own repository.

- Apices buttons (for the last time).

## Fixes

- SVG light behavior.

- Apices button lights.

- Apices trigger detection.


# 2.2.0

## New Modules

### Aleae - Bernoulli gates.

## Changes

- Knobs supplier.

- Prettify manual.

## Fixes

- Misc. small fixes and changes.


# 2.1.0

## New Modules

### Apices

Based in MI's Peaks.


# 2.0.1

## New Modules

### Mutants blank.

## Additions

- Missing blinking orange lights when modulating New Synthesis models in Funes.

- Optionally display modulated model for first polyphonic channel (from suggestions).

- Modulate model using C0-B1 (from suggestions).

- Manual.

## Changes

- Funes visual tweaks.

## Fixes

- Misc. tweaks and fixes.


# 2.0.0

First release.
