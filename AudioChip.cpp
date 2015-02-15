#include <assert.h>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "AudioChip.h"


namespace {


const uint32_t envelopeMaxParameterValue = 126;
const float envelopeMaxStageTimeMs = 10000.0f;
const uint32_t numChannels = 2;

constexpr float pi2 = M_PI * 2.0f;
constexpr float envelopeFactorPerStep = 1.0f / static_cast<float>(envelopeMaxParameterValue + 1);
constexpr float envelopeTimePerStep = envelopeMaxStageTimeMs / static_cast<float>(envelopeMaxParameterValue + 1);


constexpr float samplesToTimeMs(const uint32_t inNumSamples, const uint32_t inSampleRate) {
	return static_cast<float>(inNumSamples) / (static_cast<float>(inSampleRate) / 1000.0f);
}


constexpr float calcAngularFrequencyPerSample(const float inFrequency, const uint32_t inSampleRate) {
	return (pi2 * inFrequency) / static_cast<float>(inSampleRate);
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
		assert(0);
	}

	assert(inEnvelope.currentFactor >= 0.0f && inEnvelope.currentFactor <= 1.0f);
	return false;
}


float sineGenerator(const float inAngularFreqPerSample, const uint32_t inElapsedTimeInSamples, const float inPulseWidthOffset) {
	assert(inAngularFreqPerSample >= 0);
	return sinf(inAngularFreqPerSample * static_cast<float>(inElapsedTimeInSamples));
}


float squareGenerator(const float inAngularFreqPerSample, const uint32_t inElapsedTimeInSamples, const float inPulseWidthOffset) {
	assert(inAngularFreqPerSample >= 0);
	const float phase = inAngularFreqPerSample * static_cast<float>(inElapsedTimeInSamples);
	return sinf(phase) + sinf(3.0f * phase) / 3.0f + sinf(5.0f * phase) / 5.0f + sinf(7.0f * phase) / 7.0f + sinf(9.0f * phase) / 9.0f;
}


float noiseGenerator(const float inAngularFreqPerSample, const uint32_t inElapsedTimeInSamples, const float inPulseWidthOffset) {
	return -1.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX) / 2.0f);
}


float sawGenerator(const float inAngularFreqPerSample, const uint32_t inElapsedTimeInSamples, const float inPulseWidthOffset) {
	assert(inAngularFreqPerSample >= 0);
	return ((inAngularFreqPerSample * static_cast<float>(inElapsedTimeInSamples)) / M_PI) - 1.0f;
}


} // namespace


namespace AudioChip {


AudioChip::AudioChip(const uint32_t inSampleRate, const uint32_t inNumTracks)
	: sampleRate(inSampleRate),
	  elapsedTimeInSamples(0),
	  numTracks(inNumTracks)
{
	tracks.reserve(numTracks);

	Track track;
	track.envelope.attack = 0;
	track.envelope.decay = 0;
	track.envelope.sustain = envelopeMaxParameterValue;
	track.envelope.release = 0;
	track.envelope.currentFactor = 0.0f;
	track.envelope.state = Track::EnvelopeData::State::Attack;
	track.enabled = false;
	track.angularFreqPerSample = calcAngularFrequencyPerSample(440.0f, sampleRate);
	track.pwmPhase = 0.0f;
	track.pwmPhaseIncrement = 0.0f;
	track.generator = sineGenerator;

	for (uint32_t i = 0; i < numTracks; ++i) {
		tracks.push_back(track);
	}
}


void AudioChip::renderNextSamples(float* outBuffer, const uint32_t inNumSamples) {
	assert(outBuffer != nullptr);

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
			/*if (track.pwmPhaseIncrement != 0.0f) {
				pulseWidthOffset = (sineGenerator(track.pwmPhase, 0.0f) * M_PI) * track.pwmDepth;
				track.pwmPhase += track.pwmPhaseIncrement;
				if (track.pwmPhase >= pi2) {
					track.pwmPhase -= pi2;
				}
			}*/

			// Add track generator to mix
			const float currentSampleData = track.generator(track.angularFreqPerSample, elapsedTimeInSamples, pulseWidthOffset) * track.envelope.currentFactor;
			outBuffer[sample] += currentSampleData;
			outBuffer[sample + 1] += currentSampleData;

			elapsedTimeInSamples = (elapsedTimeInSamples + 1) % sampleRate;
		}
	}
}


void AudioChip::noteOn(const uint32_t inTrack) {
	assert(inTrack < numTracks);
	tracks[inTrack].envelope.currentFactor = 0.0f;
	tracks[inTrack].envelope.state = Track::EnvelopeData::State::Attack;
	tracks[inTrack].enabled = true;
}


void AudioChip::noteOff(const uint32_t inTrack) {
	assert(inTrack < numTracks);
	tracks[inTrack].envelope.state = Track::EnvelopeData::State::Release;
}


void AudioChip::setFrequency(const uint32_t inTrack, const float inFrequency) {
	assert(inTrack < numTracks);
	assert(inFrequency > 0.0f);

	tracks[inTrack].angularFreqPerSample = calcAngularFrequencyPerSample(inFrequency, sampleRate);
}


void AudioChip::setWaveformType(const uint32_t inTrack, const WaveformType inWaveformType) {
	assert(inTrack < numTracks);

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
		assert(false);
		break;
	}
}


void AudioChip::setEnvelope(const uint32_t inTrack, const uint8_t inAttack, const uint8_t inDecay, const uint8_t inSustain, const uint8_t inRelease) {
	assert(inTrack < numTracks);
	assert(inAttack <= envelopeMaxParameterValue);
	assert(inDecay <= envelopeMaxParameterValue);
	assert(inSustain <= envelopeMaxParameterValue);
	assert(inRelease <= envelopeMaxParameterValue);

	tracks[inTrack].envelope.attack = inAttack;
	tracks[inTrack].envelope.decay = inDecay;
	tracks[inTrack].envelope.sustain = inSustain;
	tracks[inTrack].envelope.release = inRelease;
}


void AudioChip::enablePWM(const uint32_t inTrack, const float inFrequency, const float inPWMDepth) {
	assert(inTrack < numTracks);
	assert(inPWMDepth > 0.0f && inPWMDepth <= 1.0f);
	tracks[inTrack].pwmPhaseIncrement = calcAngularFrequencyPerSample(inFrequency, sampleRate);
	tracks[inTrack].pwmDepth = inPWMDepth;
}


void AudioChip::disablePWM(const uint32_t inTrack) {
	assert(inTrack < numTracks);
	tracks[inTrack].pwmPhaseIncrement = 0.0f;
}


} // namespace AudioChip
