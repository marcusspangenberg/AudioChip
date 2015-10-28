/*
The MIT License (MIT)

Copyright (c) 2015 Marcus Spangenberg

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#include <cstdint>
#include <vector>


namespace AudioChip {


class AudioChip {
public:
	enum class WaveformType {Sine, Square, Noise, Saw};

	AudioChip() = delete;
	AudioChip(const uint32_t inSampleRate, const uint32_t inNumTracks);

	/**
		Render inNumSamples samples to outBuffer.
	*/
	void renderNextSamples(float* outBuffer, const uint32_t inNumSamples);

	/**
		Reset the envelope of inTrack and enable the track.
	*/
	void noteOn(const uint32_t inTrack);

	/**
		Sets envelope state to release.
	*/
	void noteOff(const uint32_t inTrack);

	/**
		Set note frequency in Hz.
	*/
	void setFrequency(const uint32_t inTrack, const float inFrequency);

	/**
		Set wave form to any of WaveformType::Sine, WaveformType::Square, WaveformType::Noise or WaveformType::Saw.
	*/
	void setWaveformType(const uint32_t inTrack, const WaveformType inWaveformType);

	/**
		Set track envelope. Does not reset the envelope if it is playing. Valid parameter ranges are between 0 and 126.
	*/
	void setEnvelope(const uint32_t inTrack, const uint8_t inAttack, const uint8_t inDecay, const uint8_t inSustain, const uint8_t inRelease);

	/**
		Enable pulse width modulation for the square waveform type. Modulate with a sine wave LFO with the specified frequency.
		Valid range for inPWMDepth is 0.0f to 1.0f.
	*/
	void enablePWM(const uint32_t inTrack, const float inFrequency, const float inPWMDepth);

	/**
		Disable pulse width modulation.
	*/
	void disablePWM(const uint32_t inTrack);

private:
	typedef float (*waveformGenerator)(const float inPhase, const uint32_t inHighestSubharmonic, const float inPWMPhaseOffset);

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

	bool advanceEnvelope(const uint32_t inTrack, const uint32_t inAdvanceSamples);

	uint32_t sampleRate;
	uint32_t numTracks;
	std::vector<Track> tracks;
};


} // namespace AudioChip
