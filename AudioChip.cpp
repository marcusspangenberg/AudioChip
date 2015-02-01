#include <assert.h>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include "AudioChip.h"

#ifdef DEBUG
#define ASSERT(x) assert(x)
#else
#define ASSERT(x)
#endif


namespace {


const uint32_t envelopeMaxParameterValue = 126;
const float envelopeMaxStageTimeMs = 10000.0f;
const uint32_t numChannels = 2;

constexpr float pi2 = M_PI * 2.0f;
constexpr float envelopeFactorPerStep = 1.0f / static_cast<float>(envelopeMaxParameterValue + 1);
constexpr float envelopeTimePerStep = envelopeMaxStageTimeMs / static_cast<float>(envelopeMaxParameterValue + 1);


constexpr float samplesToTimeMs(const uint32_t inNumSamples, const float inSampleRate) {
	return static_cast<float>(inNumSamples) / (inSampleRate / 1000.0f);
}


constexpr float frequencyToPhaseIncrement(const float inFrequency, const float inSampleRate) {
	return (pi2 * inFrequency) / inSampleRate;
}


bool advanceEnvelope(AudioChip::Track::EnvelopeData& inEnvelope, const uint32_t inAdvanceSamples, const float inSampleRate) {
	using namespace AudioChip;

	float factorPerMs = 0.0f;

	const float elapsedTimeMs = samplesToTimeMs(inAdvanceSamples, inSampleRate);

	switch (inEnvelope.state) {
	case Track::EnvelopeData::State::Attack:
		if (inEnvelope.attack == 0) {
			factorPerMs = 1.0f;
		} else {
			factorPerMs = 1.0f / (envelopeTimePerStep * inEnvelope.attack);
		}
		inEnvelope.currentFactor += factorPerMs * elapsedTimeMs;
		if (inEnvelope.currentFactor >= 1.0f) {
			inEnvelope.currentFactor = 1.0f;
			inEnvelope.state = Track::EnvelopeData::State::Decay;
		}
		break;
	case Track::EnvelopeData::State::Decay:
		if (inEnvelope.decay == 0) {
			factorPerMs = 1.0f;
		} else {
			factorPerMs = 1.0f / (envelopeTimePerStep * inEnvelope.decay);
		}

		inEnvelope.currentFactor -= factorPerMs * elapsedTimeMs;
		{
			const float sustainFactor = inEnvelope.sustain * envelopeFactorPerStep;
			if (inEnvelope.currentFactor <= sustainFactor) {
				inEnvelope.currentFactor = sustainFactor;
				inEnvelope.state = Track::EnvelopeData::State::Sustain;
			}
		}
		break;
	case Track::EnvelopeData::State::Sustain:
		if (inEnvelope.sustain == envelopeMaxParameterValue) {
			inEnvelope.currentFactor = 1.0f;
		} else {
			inEnvelope.currentFactor = inEnvelope.sustain * envelopeFactorPerStep;
		}
		break;
	case Track::EnvelopeData::State::Release:
		if (inEnvelope.release == 0) {
			inEnvelope.currentFactor = 0.0f;
			return true;
		} else {
			factorPerMs = 1.0f / (envelopeTimePerStep * inEnvelope.release);

			inEnvelope.currentFactor -= factorPerMs * elapsedTimeMs;
			if (inEnvelope.currentFactor <= 0.0f) {
				inEnvelope.currentFactor = 0.0f;
				return true;
			}
		}
		break;
	default:
		ASSERT(0);
	}

	ASSERT(inEnvelope.currentFactor >= 0.0f && inEnvelope.currentFactor <= 1.0f);
	return false;
}


float sineGenerator(const float phase, const float phaseIncrement, const float pulseWidthOffset) {
	ASSERT(phase >= 0);
	return sinf(phase);
}


inline float generateSimpleSquare(const float phase, const float pulseWidthOffset) {
	ASSERT(phase >= 0);

	if (pulseWidthOffset != 0.0f) {
		const float piWithOffset = M_PI - pulseWidthOffset;

		if (phase < piWithOffset) {
			return 1.0f;
		} else if (phase < (2.0f * piWithOffset)) {
			return -1.0f;
		} else {
			return 0.0f;
		}
	} else {
		if (phase < M_PI) {
			return 1.0f;
		} else {
			return -1.0f;
		}
	}
}


// Based on http://www.martin-finke.de/blog/articles/audio-plugins-018-polyblep-oscillator/
inline float calcPolyBlep(const float inT, const float inDt) {
	float t = inT;
	float dt = inDt;

	if (t < dt) {
		t /= dt;
		return t + t - t * t - 1.0f;
	} else if (t > 1.0f - dt) {
		t = (t - 1.0f) / dt;
		return t * t + t + t + 1.0f;
	} else {
		return 0.0f;
	}
}


float squareGenerator(const float phase, const float phaseIncrement, const float pulseWidthOffset) {
	const float t = phase / pi2;
	const float dt = phaseIncrement / pi2;

	float value = generateSimpleSquare(phase, pulseWidthOffset);
	value += calcPolyBlep(t, dt);
	value -= calcPolyBlep(fmodf(t + 0.5f, 1.0f), dt);

	return value;
}


float noiseGenerator(const float /*phase*/, const float /*phaseIncrement*/, const float /*pulseWidthOffset*/) {
	return -1.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX) / 2.0f);
}


float sawGenerator(const float phase, const float /*phaseIncrement*/, const float /*pulseWidthOffset*/) {
	ASSERT(phase >= 0);
	return (phase / M_PI) - 1.0f;
}


} // namespace


namespace AudioChip {


AudioChip::AudioChip(const float inSampleRate, const uint32_t inNumTracks)
	: sampleRate(inSampleRate),
	  numTracks(inNumTracks)
{
	tracks.reserve(numTracks);

	for (uint32_t i = 0; i < numTracks; ++i) {
		Track track;
		track.envelope.attack = 0;
		track.envelope.decay = 0;
		track.envelope.sustain = 126;
		track.envelope.release = 0;
		track.envelope.currentFactor = 0.0f;
		track.envelope.state = Track::EnvelopeData::State::Attack;
		track.enabled = false;
		track.phase = 0.0f;
		track.phaseIncrement = frequencyToPhaseIncrement(440.0f, sampleRate);
		track.pwmPhase = 0.0f;
		track.pwmPhaseIncrement = 0.0f;
		track.generator = sineGenerator;
		tracks.push_back(track);
	}
}


void AudioChip::renderNextSamples(float* outBuffer, const uint32_t inNumSamples) {
	ASSERT(outBuffer != nullptr);

	const uint32_t totalSamples = inNumSamples * numChannels;
	memset(outBuffer, 0, totalSamples * sizeof(float));

	for (uint32_t trackNum = 0; trackNum < numTracks; trackNum++) {
		Track& track = tracks[trackNum];

		if (!track.enabled) {
			continue;
		}

		const bool noteEnded = advanceEnvelope(track.envelope, inNumSamples, sampleRate);
		if (noteEnded) {
			track.enabled = false;
			continue;
		}

		for (uint32_t sample = 0; sample < totalSamples; sample += numChannels) {
			float pulseWidthOffset = 0.0f;

			// PWM
			if (track.phaseIncrement != 0.0f) {
				pulseWidthOffset = (sineGenerator(track.pwmPhase, track.pwmPhaseIncrement, 0.0f) * M_PI) * track.pwmDepth;
				track.pwmPhase += track.pwmPhaseIncrement;
				if (track.pwmPhase >= pi2) {
					track.pwmPhase -= pi2;
				}
			}

			// Add track generator to mix
			const float currentSampleData = track.generator(track.phase, track.phaseIncrement, pulseWidthOffset) * track.envelope.currentFactor;
			outBuffer[sample] += currentSampleData;
			outBuffer[sample + 1] += currentSampleData;

			// Update generator phase
			track.phase += track.phaseIncrement;
			if (track.phase >= pi2) {
				track.phase -= pi2;
			}
		}
	}
}


void AudioChip::noteOn(const uint32_t inTrack) {
	ASSERT(inTrack < numTracks);
	tracks[inTrack].envelope.currentFactor = 0.0f;
	tracks[inTrack].envelope.state = Track::EnvelopeData::State::Attack;
	tracks[inTrack].phase = 0.0f;
	tracks[inTrack].enabled = true;
}


void AudioChip::noteOff(const uint32_t inTrack) {
	ASSERT(inTrack < numTracks);
	tracks[inTrack].envelope.state = Track::EnvelopeData::State::Release;
}


void AudioChip::setFrequency(const uint32_t inTrack, const float inFrequency) {
	ASSERT(inTrack < numTracks);
	ASSERT(inFrequency > 0.0f);

	tracks[inTrack].phaseIncrement = frequencyToPhaseIncrement(inFrequency, sampleRate);
}


void AudioChip::setWaveformType(const uint32_t inTrack, const WaveformType inWaveformType) {
	ASSERT(inTrack < numTracks);

	switch (inWaveformType) {
	case WaveformType::Sine:
		tracks[inTrack].generator = sineGenerator;
		break;
	case WaveformType::Square:
		tracks[inTrack].generator = squareGenerator;
		break;
	case WaveformType::Noise:
		tracks[inTrack].generator = noiseGenerator;
		break;
	case WaveformType::Saw:
		tracks[inTrack].generator = sawGenerator;
		break;
	default:
		ASSERT(false);
		break;
	}
}


void AudioChip::setEnvelope(const uint32_t inTrack, const uint8_t inAttack, const uint8_t inDecay, const uint8_t inSustain, const uint8_t inRelease) {
	ASSERT(inTrack < numTracks);
	ASSERT(inAttack <= envelopeMaxParameterValue);
	ASSERT(inDecay <= envelopeMaxParameterValue);
	ASSERT(inSustain <= envelopeMaxParameterValue);
	ASSERT(inRelease <= envelopeMaxParameterValue);

	tracks[inTrack].envelope.attack = inAttack;
	tracks[inTrack].envelope.decay = inDecay;
	tracks[inTrack].envelope.sustain = inSustain;
	tracks[inTrack].envelope.release = inRelease;
}


void AudioChip::enablePWM(const uint32_t inTrack, const float inFrequency, const float inPWMDepth) {
	ASSERT(inTrack < numTracks);
	ASSERT(inPWMDepth > 0.0f && inPWMDepth <= 1.0f);
	tracks[inTrack].pwmPhaseIncrement = frequencyToPhaseIncrement(inFrequency, sampleRate);
	tracks[inTrack].pwmDepth = inPWMDepth;
}


void AudioChip::disablePWM(const uint32_t inTrack) {
	ASSERT(inTrack < numTracks);
	tracks[inTrack].pwmPhaseIncrement = 0.0f;
}


} // namespace AudioChip
