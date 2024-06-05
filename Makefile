FLAGS += \
	-DTEST \
	-I./eurorack \
	-I./alt_firmware \
	-Wno-unused-local-typedefs

SOURCES += $(wildcard src/*.cpp)

SOURCES += eurorack/stmlib/utils/random.cc
SOURCES += eurorack/stmlib/dsp/atan.cc
SOURCES += eurorack/stmlib/dsp/units.cc

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

DISTRIBUTABLES += $(wildcard LICENSE*) res

RACK_DIR ?= ../..
include $(RACK_DIR)/plugin.mk
