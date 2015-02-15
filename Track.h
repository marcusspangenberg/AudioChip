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

	typedef float (*waveformGenerator)(const float inAngularFreqPerSample, const uint32_t inElapsedTimeInSamples, const float inPulseWidthOffset);

	EnvelopeData envelope;
	bool enabled;
	float angularFreqPerSample;
	float pwmPhase;
	float pwmPhaseIncrement;
	float pwmDepth;
	waveformGenerator generator;
};


} // namespace AudioChip
