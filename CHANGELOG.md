# Next version

## New Modules

### Reticula

- A topographic drum sequencer based on Mutable Instruments' Grids, with capabilities exclusive to Sanguine Mutants.

## Fixes

- Aestus family: use the proper knob for the FM parameter.

- Aestus: easter egg not respecting button light intensity.

- Aleae: "Out mode" button didn't follow button light brightness standards.

- Anuli: FX button light didn't follow plugin button light standards.

- Apices family: go back to parameter values set by the knobs when expanders are connected and a cable is disconnected from one of the expander inputs.

- Fluctus: some parameters for Beat-repeat were not being handled.

- Funes: custom data not loading under certain conditions.

- Funes: default LPG values were not reflected in the knobs or respected in the engine when resetting the module.

- Funes: audio glitch when the trigger input is connected and a 6 OP FM model is selected without setting it with the trigger unpatched beforehand.

- Mortuus: button lights.

- Nebulae family: proper tooltips on startup.

- Nodi: wrong channel indicator light colors when morse code easter egg is activated.

- Velamina: proper rescaling of gain lights when channels are polyphonic.

- Velamina: output port lights did not reflect port voltage averages when dealing with polyphonic channels properly.

- Vimina: swing/factor tooltips not updating until the knob is moved when the mode of a section is changed.

## Additions

- Aestus family: modules are fully polyphonic.

- Anuli: polyphonic CV selection of FX for Disastrous Peace channels.

- Apices family: tap lfo tempo values are now saved to and restored from patches and presets.

- Apices family: knob tooltips for the different modes.

- Apices family expanders: mode dependent tooltips for the trimpots and inputs.

- Apices family: mode selector knob tooltips.

- Funes: new chord banks from Lyle Mills' firmware.

- Funes: ability and controls to crossfade the main signal with the auxiliary signal and output it in the Aux output, adapted from Lyle Mills' firmware.

- Funes: Aux suboscillator adapted from Lyle Mills' firmware.

- Funes: ability to hold Timbre, Morph, Harmonics, Level and Note voltages when trigger es received, adapted from Lyle Mills' firmware.

- Mortuus: PLO phase values are now saved to and restored from patches and presets.

- Nebulae family: modules are fully polyphonic.

- Nebulae family: optional hardware-like handling of triggers: rising edge detection instead of treating them as gates.

- Nebulae family: better button tooltips.

- Nebulae family: better mode knob tooltips.

- Nodi family: display channel also affects the big knob's light.

- Nodi family: polyphonic CV control of Attack and Decay parameters.

## Changes

- Plugin: performance tweaks.

- Aestus: due to how polyphonic mode selection works, Sync mode must be enabled to clock the module externally even when the Tidal Modulator model is selected.

- Aestus family: reorganize the faceplates to accommodate the polyphonic versions of the modules.

- Aestus family: signal PLL/Sync mode by blinking the Sync button on and off instead of yellow/red (closer to what the hardware does).

- Aestus family: ignore Range button and voltages when PLL/Sync mode is enabled (hardware behavior).

- Aestus family: turn off Range button LED when PLL/Sync mode is enabled.

- Apices family: display knob values as percentages.

- Apices family expanders: display trimpot values as percentages.

- Apices family expanders: reorder the faceplates to resemble the order of the base modules a bit more.

- Contextus: make synthesis models light color palette more distinctive.

- Funes: relate knobs using color.

- Funes: Model input bananut color from purple to black.

- Marmora: make Y divider tooltips easier on the eyes.

- Nebulae family: better parameter and input tooltips.

- Nebulae family: OLED display text tweaks.

- Nebulae family: improve lights response.

- Temulenti: better tooltips and Range menu entry for Two Bumps mode.

- Vimina: produce slightly longer triggers.

## Removals

- Anuli: Note polyphony display.

- Apices family: OLED displays when running under MetaModule.

- Funes: emulation of hardware lights for LPG and Frequency mode (the knobs are always on the faceplate).

- Nebulae family: emulation of hardware parameter and quality light patterns: not very useful when the modules are polyphonic (and every parameter is on the faceplate showing its value at all times).

- Nebulae family: OLED displays when running under MetaModule.


---

# 2.6.8

## Additions

- Developers: integrate Oksami's MetaModule compatibility changes.

- Plugin: MetaModule exclusive faceplates.

- Scalaria: MetaModule: use special faceplate with regular LEDs.

- Vimina: show current swing or active clock division or multiplication factors in the knob's tooltip using the description field.

## Changes

- Vimina: slight performance improvements.


---

# 2.6.7

## Fixes

- Anuli: prevent the module from going to limbo when specific models are selected and extreme (and I do mean extreme) negative voltages are sent to the V/Oct input.

- Nodi family: properly restore the "Low CPU" option when it is enabled in either module in really old patches.

- Vimina: timer manipulation stops working after about 10 minutes.

- Vimina: wrong light colors under certain circumstances.

## Additions

- Anuli: frequency knob units and display values that reflect what the knob actually controls (semi-tones).

- Anuli: knob units and display values friendlier to humans.

- Funes: trimpot display values friendlier to humans.

- Marmora: actual X2 division factor to Y rate tooltip.


---

# 2.6.6

## Fixes

- Plugin: faceplate colors.

- Plugin: work around Rack's SVG rendering defiencies.

- Aestus family: restore "Frequency knob center is C4" user preference properly.

- Explorator: noise not working if the trigger input has not been connected at least once.

## Additions

- Aestus family: measurement units and numbers friendlier to humans.

- Aestus: restore the peacocks plotter easter egg (it can be accessed using the context menu).

## Changes

- Plugin: faceplate color tweaks.

- Plugin source: move common Sanguine Modules sources to submodule.

- Explorator: produce Prism noise wave forms more akin to original implementation (Note: Prism noise is now LOUDER).

- Temulenti: remove clamping on pitch to allow reaching really low LFO frequencies.


---

# 2.6.5

## Fixes

- Apices family: engines should restore knob values when disconnecting expanders.

- Mortuus: rollover error on slow FM LFOs.

- Vimina: clock multiplier for low BPM clocks.

- Vimina: don't delay triggers by 16 samples.

## Changes

- Apices family expanders: make tooltips congruent with base module tooltips.

- Apices family: performance improvements.


---

# 2.6.4

## Fixes

- Marmora: t2 light stays on for too long.

## Changes

- Vimina: even out knob space distribution for clock multiplier/divider factors.


---

# 2.6.3

## Fixes

- Marmora: entering Length values by right clicking the knob did not follow display values.

## Additions

- Explorator: per polyphonic channel noise generator for the S&H section, with two selectable noise generators with different CPU consumptions.


---

# 2.6.2

## Fixes

- Vimina: clock division factor not reaching "8".


---

# 2.6.1

## Fixes

- Scalaria: crashing on Apple and Linux.

- Scalaria: outputs sometimes getting stuck at -5V.

- Scalaria: faceplate tag did not follow the conventions of the rest of the modules.

- Scalaria: imperceptible lights on Apple.

- Mutuus: algorithm light was set 8 more times than it needed to be.

## Additions

- Anuli: strummer light now follows selected display channel.

## Changes

- Scalaria: darker background for shaped lights (they look better that way).

- Plugin: adjust button LED intensity (it was too low).

- Plugin: small performance tweaks for most modules.


---

# 2.6.0

## New Modules

### Nix

An expander for Apices that provides CV parameter modulation, inspired by the Rainier module.

### Ansa

An expander for Mortuus that provides CV parameter modulation, inspired by the Rainier module.

### Scalaria

A polyphonic Moog inspired ladder filter from the original Symbiote firmware for Mutable Instruments' Warps.

## Fixes

- Funes LPG lights.

- Velamina saturator threshold.

- Explorator Plumbago faceplate.

- Sync mode for the Aestus family should only be updated when it has changed in the front-end, not when processing every block.

- Nodi family: signature waveshaper and oscillator drift are not boolean settings: they have a range of values.

- Nodi family: restore LFO modes and prevent them from crashing with divisions by zero.

- Nodi family: quantizer scale tooltips.

- Temulenti: FM knob control of Two Drunks' gate durations now works as described in the manual (it was inverted).

- Anuli: don't ignore module generated strums when dealing with strum light state.

- Nebulae family: LEDs sometimes getting "stuck" on temporary display modes.

## Additions

- Marmora: Ability to reset generators from the context menu.

- Marmora: Ability to reseed random generator from the context menu.

- Marmora: Ability to set user defined random generator seeds (state) from the context menu.

- Marmora: show selected loop length in knob's tooltip.

- Anuli: Optionally tune to a C when frequency knob is centered.

- Etesia: CV control for Reverse parameter.

- Aestus family: tune the modules to C4 when the Frequency knob is centered (can be disabled for compatibility).

- Funes: attenuverter for Harmonics CV.

- Funes: CV control with attenuverters for lowpass gate response and decay.

- Funes: LED lights for custom data state.

- Aleae: hint Trigger 2 is normalled to Trigger 1 in the faceplate.

- Nodi family: use different Signature Wave Shaper seeds per module instance (user controllable).

- Nodi family: ability to select scales from the context menu.

## Changes

- Themes: group options under a submenu.

- Marmora: reorganize menu.

- Marmora: faceplate label tweaks.

- Marmora: rework faceplate to make usage easier.

- Anuli: reorganize faceplate and make it cleaner.

- Anuli: change the polyphony knob's tooltip to be congruent with the nomenclature used in the manual.

- Nodi family: faceplate tweaks.

- Nodi family: processing tweaks.

- Nodi family: reorganize menu.

- Apices family: faceplate tweaks.

- Mortuus: further ByteBeats tweaks.

- Aestus: PLL/Clocked mode tweaks.

- Aestus family: faceplate tweaks.

- Aleae family: faceplate tweaks.

- Funes: rework faceplate to make it both more compact and more attractive.

- Funes: LGP LED displays start at the center lights instead of the rightmost ones (it looks prettier and makes more sense for the horizontal arrangement).

- Nebulae family: faceplate tweaks.

- Incurvationes family: faceplate tweaks.


---

# 2.5.2

## Fixes

- Marmora X clock source tooltips.


---

# 2.5.1

## Changes

- Improve Marmora performance.

## Fixes

- Vimina stops generating multiplied clocks when the source clock stops.


---

# 2.5.0

## New Modules

### Aestus

Based on MI's Tides, it fixes PLL mode, separates it from the clock input; restores switching banks using the clock input when module is using the Sheep firmware; fixes the inverted "Mode" light colors; fixes the broken "High" and "Low" outputs, and, hopefully, fixes a long-standing crash that occurs under specific circumstances.

### Temulenti

Based on the Parasite firmware for MI's Tides: includes every Aestus fix, additionally: restores functionality when using the clock input in Two Bumps or Two Drunks modes; Sheep model can be used.

### Vimina

A polyphonic dual clock divider, multiplier and swinger based on the Twigs firmware for MI's Branches.

## Additions

- Polyphony for Incurvationes, Distortiones and Mutuus, active channels are shown using rows of LEDs around the INT. OSC. button.

- Polyphonic indicator LED lights show the current algorithm for every channel in Incurvationes.

- Polyphonic indicator LED lights show the current mode for every channel in Distortiones and Mutuus.

- Direct or note CV per channel mode selection for Anuli, Distortiones and Mutuus.

- Per channel "Meta-modulation" is always available for Nodi and Contextus: the FM input always does FM.

- Nebulae, Etesia and Fluctus always sum polyphonic input channels, per the voltage standards.

- Ability to select the channel to show in the display for Anuli, Funes, Nodi and Contextus.

- Ability to select the channel to show in Aleae's LEDs.

- Note preview for played notes when editing scales in Marmora.

- Output attenuator/amplifier for Nebulae family.

## Changes

- Anuli's channel counter is gone, replaced by a row of LEDs at the top that show active channels and their selected modes.

- Separate Sanguine Mutants and Sanguine Monsters in the browser.

- Remove "Out" port label and use a "1 x 2" one that is closer to hardware faceplates for Incurvationes, Distortiones and Mutuus.

- Input voltage scaling for Incurvationes, Distortiones and Mutuus (your old patches may be too loud! Adjust input gain).

- Dim LED button's light intensity a little.

- Performance tweaks.

- Disable "LFO" range in Nodi and Contextus.

- Funes menu: reorder; better wording for custom data entries; make menu smarter: only show custom data entries when a Model that can load custom data is selected.

## Fixes

- Anuli's Modal Resonator display label.

- Make Anuli's Disastrous Peace mode respect polyphony.

- Output port nut colors for Nebulae family.


---

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


---

# 2.4.1

## Changes

- Mode selector encoder knobs for Nebulae and its descendants.


---

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


---

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


---

# 2.3.1

## Changes

- Changelog organization.

## Fixes

- Contextus crashing Rack.


---

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


---

# 2.2.0

## New Modules

### Aleae - Bernoulli gates.

## Changes

- Knobs supplier.

- Prettify manual.

## Fixes

- Misc. small fixes and changes.


---

# 2.1.0

## New Modules

### Apices

Based in MI's Peaks.


---

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


---

# 2.0.0

First release.
