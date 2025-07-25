FLAGS += \
	-DNOASM \
	-DBRAIDS_LFO_FIX \
	-I./eurorack \
	-I./alt_firmware \
	-I./SanguineModulesCommon/pcgcpp \
	-I./SanguineModulesCommon/src \
	-Wno-unused-local-typedefs

SOURCES += $(wildcard src/*.cpp)

SOURCES += eurorack/stmlib/utils/random.cc
SOURCES += eurorack/stmlib/dsp/atan.cc
SOURCES += eurorack/stmlib/dsp/units.cc

SOURCES += alt_firmware/parasites_stmlib/utils/parasites_random.cc
SOURCES += alt_firmware/parasites_stmlib/dsp/parasites_atan.cc
SOURCES += alt_firmware/parasites_stmlib/dsp/parasites_units.cc

SOURCES += $(wildcard eurorack/plaits/dsp/*.cc)
SOURCES += $(wildcard eurorack/plaits/dsp/chords/*.cc)
SOURCES += $(wildcard eurorack/plaits/dsp/engine/*.cc)
SOURCES += $(wildcard eurorack/plaits/dsp/engine2/*.cc)
SOURCES += $(wildcard eurorack/plaits/dsp/fm/*.cc)
SOURCES += $(wildcard eurorack/plaits/dsp/speech/*.cc)
SOURCES += $(wildcard eurorack/plaits/dsp/physical_modelling/*.cc)
SOURCES += eurorack/plaits/resources.cc

SOURCES += eurorack/peaks/processors.cc
SOURCES += eurorack/peaks/resources.cc
SOURCES += eurorack/peaks/drums/bass_drum.cc
SOURCES += eurorack/peaks/drums/fm_drum.cc
SOURCES += eurorack/peaks/drums/high_hat.cc
SOURCES += eurorack/peaks/drums/snare_drum.cc
SOURCES += eurorack/peaks/modulations/lfo.cc
SOURCES += eurorack/peaks/modulations/multistage_envelope.cc
SOURCES += eurorack/peaks/pulse_processor/pulse_shaper.cc
SOURCES += eurorack/peaks/pulse_processor/pulse_randomizer.cc
SOURCES += eurorack/peaks/number_station/number_station.cc

SOURCES += eurorack/tides2/poly_slope_generator.cc
SOURCES += eurorack/tides2/ramp/ramp_extractor.cc
SOURCES += eurorack/tides2/resources.cc

SOURCES += eurorack/braids/macro_oscillator.cc
SOURCES += eurorack/braids/analog_oscillator.cc
SOURCES += eurorack/braids/digital_oscillator.cc
SOURCES += eurorack/braids/resources.cc
SOURCES += eurorack/braids/quantizer.cc

SOURCES += alt_firmware/renaissance/renaissance_macro_oscillator.cc
SOURCES += alt_firmware/renaissance/renaissance_digital_oscillator.cc
SOURCES += alt_firmware/renaissance/renaissance_analog_oscillator.cc
SOURCES += alt_firmware/renaissance/renaissance_resources.cc
SOURCES += alt_firmware/renaissance/renaissance_quantizer.cc
SOURCES += alt_firmware/renaissance/renaissance_stack.cc
SOURCES += alt_firmware/renaissance/renaissance_harmonics.cc

SOURCES += alt_firmware/renaissance/vocalist/vocalist.cc
SOURCES += alt_firmware/renaissance/vocalist/sam.cc
SOURCES += alt_firmware/renaissance/vocalist/wordlist.cc
SOURCES += alt_firmware/renaissance/vocalist/rendertabs.cc

SOURCES += eurorack/clouds/dsp/correlator.cc
SOURCES += eurorack/clouds/dsp/granular_processor.cc
SOURCES += eurorack/clouds/dsp/mu_law.cc
SOURCES += eurorack/clouds/dsp/pvoc/frame_transformation.cc
SOURCES += eurorack/clouds/dsp/pvoc/phase_vocoder.cc
SOURCES += eurorack/clouds/dsp/pvoc/stft.cc
SOURCES += eurorack/clouds/resources.cc

SOURCES += alt_firmware/clouds_parasite/dsp/etesia_granular_processor.cc
SOURCES += alt_firmware/clouds_parasite/etesia_resources.cc
SOURCES += alt_firmware/clouds_parasite/dsp/etesia_correlator.cc
SOURCES += alt_firmware/clouds_parasite/dsp/etesia_mu_law.cc
SOURCES += alt_firmware/clouds_parasite/dsp/pvoc/etesia_frame_transformation.cc
SOURCES += alt_firmware/clouds_parasite/dsp/pvoc/etesia_phase_vocoder.cc
SOURCES += alt_firmware/clouds_parasite/dsp/pvoc/etesia_stft.cc

SOURCES += alt_firmware/deadman/deadman_processors.cc
SOURCES += alt_firmware/deadman/deadman_resources.cc
SOURCES += alt_firmware/deadman/drums/deadman_bass_drum.cc
SOURCES += alt_firmware/deadman/drums/deadman_fm_drum.cc
SOURCES += alt_firmware/deadman/drums/deadman_high_hat.cc
SOURCES += alt_firmware/deadman/drums/deadman_snare_drum.cc
SOURCES += alt_firmware/deadman/modulations/deadman_lfo.cc
SOURCES += alt_firmware/deadman/modulations/deadman_multistage_envelope.cc
SOURCES += alt_firmware/deadman/pulse_processor/deadman_pulse_shaper.cc
SOURCES += alt_firmware/deadman/pulse_processor/deadman_pulse_randomizer.cc
SOURCES += alt_firmware/deadman/number_station/deadman_number_station.cc
SOURCES += alt_firmware/deadman/number_station/deadman_bytebeats.cc
SOURCES += alt_firmware/deadman/drums/deadman_cymbal.cc

SOURCES += alt_firmware/fluctus/dsp/fluctus_granular_processor.cc
SOURCES += alt_firmware/fluctus/fluctus_resources.cc
SOURCES += alt_firmware/fluctus/dsp/fluctus_correlator.cc
SOURCES += alt_firmware/fluctus/dsp/fluctus_mu_law.cc
SOURCES += alt_firmware/fluctus/dsp/pvoc/fluctus_spectral_clouds_transformation.cc
SOURCES += alt_firmware/fluctus/dsp/pvoc/fluctus_phase_vocoder.cc
SOURCES += alt_firmware/fluctus/dsp/pvoc/fluctus_stft.cc
SOURCES += alt_firmware/fluctus/dsp/fluctus_kammerl_player.cc

SOURCES += eurorack/warps/dsp/modulator.cc
SOURCES += eurorack/warps/dsp/oscillator.cc
SOURCES += eurorack/warps/dsp/vocoder.cc
SOURCES += eurorack/warps/dsp/filter_bank.cc
SOURCES += eurorack/warps/resources.cc

SOURCES += alt_firmware/distortiones/dsp/distortiones_modulator.cc
SOURCES += alt_firmware/distortiones/dsp/distortiones_oscillator.cc
SOURCES += alt_firmware/distortiones/dsp/distortiones_vocoder.cc
SOURCES += alt_firmware/distortiones/dsp/distortiones_filter_bank.cc
SOURCES += alt_firmware/distortiones/distortiones_resources.cc

SOURCES += alt_firmware/mutuus/dsp/mutuus_modulator.cc
SOURCES += alt_firmware/mutuus/dsp/mutuus_oscillator.cc
SOURCES += alt_firmware/mutuus/dsp/mutuus_vocoder.cc
SOURCES += alt_firmware/mutuus/dsp/mutuus_filter_bank.cc
SOURCES += alt_firmware/mutuus/mutuus_resources.cc

SOURCES += eurorack/marbles/random/t_generator.cc
SOURCES += eurorack/marbles/random/x_y_generator.cc
SOURCES += eurorack/marbles/random/output_channel.cc
SOURCES += eurorack/marbles/random/lag_processor.cc
SOURCES += eurorack/marbles/random/quantizer.cc
SOURCES += eurorack/marbles/ramp/ramp_extractor.cc
SOURCES += eurorack/marbles/resources.cc

SOURCES += eurorack/rings/dsp/fm_voice.cc
SOURCES += eurorack/rings/dsp/part.cc
SOURCES += eurorack/rings/dsp/string_synth_part.cc
SOURCES += eurorack/rings/dsp/string.cc
SOURCES += eurorack/rings/dsp/resonator.cc
SOURCES += eurorack/rings/resources.cc

SOURCES += eurorack/tides/generator.cc
SOURCES += eurorack/tides/resources.cc
SOURCES += eurorack/tides/plotter.cc

SOURCES += alt_firmware/bumps/bumps_generator.cc
SOURCES += alt_firmware/bumps/bumps_resources.cc

SOURCES += alt_firmware/scalaria/dsp/scalaria_modulator.cc
SOURCES += alt_firmware/scalaria/dsp/scalaria_oscillator.cc
SOURCES += alt_firmware/scalaria/scalaria_resources.cc

SOURCES += SanguineModulesCommon/src/sanguinecomponents.cpp
SOURCES += SanguineModulesCommon/src/sanguinehelpers.cpp
SOURCES += SanguineModulesCommon/src/themes.cpp

DISTRIBUTABLES += $(wildcard LICENSE*) res

RACK_DIR ?= ../..
include $(RACK_DIR)/plugin.mk
ifdef DEBUGBUILD
FLAGS := $(filter-out -O3,$(FLAGS))
FLAGS := $(filter-out -funsafe-math-optimizations,$(FLAGS))
FLAGS += -Og
endif