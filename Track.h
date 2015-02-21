#pragma once

#include <cstdint>


namespace AudioChip {


struct Track {
	struct EnvelopeData {
		enum class State {Attack, Decay, Sustain, Release};

		uint8_t attack;
		uint8_t decay;
		uint8_t sustain;
		uint8_t release;
		float currentFactor;
		enum State state;
	};

	typedef float (*waveformGenerator)(const float inPhase, const uint32_t inHighestSubharmonic, const float inPWMPhaseOffset);

	EnvelopeData envelope;
	bool enabled;

	float phase;
	float phaseIncrement;
	uint32_t highestSubharmonic;

	float pwmPhase;
	float pwmPhaseIncrement;
	float pwmDepth;

	waveformGenerator generator;
};


} // namespace AudioChip
