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

	typedef float (*waveformGenerator)(const float phase, const float phaseIncrement, const float pulseWidthOffset);

	EnvelopeData envelope;
	bool enabled;
	float phase;
	float phaseIncrement;
	float pwmPhase;
	float pwmPhaseIncrement;
	float pwmDepth;
	waveformGenerator generator;
};


} // namespace AudioChip
