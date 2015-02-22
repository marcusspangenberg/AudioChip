#include <assert.h>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "AudioChip.h"
#include "SineTable.h"


namespace {


AudioChip::SineTable sineTable;


const uint32_t envelopeMaxParameterValue = 126;
const float envelopeMaxStageTimeMs = 10000.0f;
const uint32_t numChannels = 2;

constexpr float pi2 = M_PI * 2.0f;
constexpr float envelopeFactorPerStep = 1.0f / static_cast<float>(envelopeMaxParameterValue + 1);
constexpr float envelopeTimePerStep = envelopeMaxStageTimeMs / static_cast<float>(envelopeMaxParameterValue + 1);


constexpr float samplesToTimeMs(const uint32_t inNumSamples, const uint32_t inSampleRate) {
	return static_cast<float>(inNumSamples) / (static_cast<float>(inSampleRate) / 1000.0f);
}


uint32_t calcHighestSubharmonic(const float inFrequency, const uint32_t inSampleRate) {
	const float halfSampleRate = inSampleRate / 2.0f;

	uint32_t highestSubharmonic = 1;
	while ((inFrequency * static_cast<float>(highestSubharmonic)) < halfSampleRate) {
		++highestSubharmonic;
	}
	return highestSubharmonic - 1;
}


constexpr float frequencyToPhaseIncrement(const float inFrequency, const float inSampleRate) {
	return (pi2 * inFrequency) / inSampleRate;
}


float sineGenerator(const float inPhase, const uint32_t /*inHighestSubharmonic*/, const float /*inPWMPhaseOffset*/) {
	assert(inPhase >= 0.0f);
	return sineTable.lookupSinf(inPhase);
}


float squareGenerator(const float inPhase, const uint32_t inHighestSubharmonic, const float inPWMPhaseOffset) {
	assert(inPhase >= 0.0f);

	float outSample = 0.0f;

	if (inPWMPhaseOffset == 0.0f) {
		for (uint32_t freqMultiplier = 1; freqMultiplier <= inHighestSubharmonic; freqMultiplier += 2) {
			const float freqMultiplierFloat = static_cast<float>(freqMultiplier);
			outSample += sineTable.lookupSinf(inPhase * freqMultiplierFloat) / freqMultiplierFloat;
		}
	} else {
		float saw1Sample = 0.0f;
		float saw2Sample = 0.0f;

		const float offsetPhase = inPhase + inPWMPhaseOffset;

		for (uint32_t freqMultiplier = 1; freqMultiplier <= inHighestSubharmonic; ++freqMultiplier) {
			const float freqMultiplierFloat = static_cast<float>(freqMultiplier);
			saw1Sample += sineTable.lookupSinf(inPhase * freqMultiplierFloat) / freqMultiplierFloat;
		}

		for (uint32_t freqMultiplier = 1; freqMultiplier <= inHighestSubharmonic; freqMultiplier += 2) {
			const float freq1MultiplierFloat = static_cast<float>(freqMultiplier);
			const float freq2MultiplierFloat = static_cast<float>(freqMultiplier + 1);
			saw2Sample -= sineTable.lookupSinf(offsetPhase * freq1MultiplierFloat) / freq1MultiplierFloat;
			saw2Sample += sineTable.lookupSinf(offsetPhase * freq2MultiplierFloat) / freq2MultiplierFloat;
		}

		outSample = saw1Sample - saw2Sample;
	}

	return outSample;
}


float noiseGenerator(const float /*inPhase*/, const uint32_t /*inHighestSubharmonic*/, const float /*inPWMPhaseOffset*/) {
	return -1.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX) / 2.0f);
}


float sawGenerator(const float inPhase, const uint32_t inHighestSubharmonic, const float /*inPWMPhaseOffset*/) {
	assert(inPhase >= 0.0f);

	float outSample = 0.0f;
	for (uint32_t freqMultiplier = 1; freqMultiplier <= inHighestSubharmonic; ++freqMultiplier) {
		const float freqMultiplierFloat = static_cast<float>(freqMultiplier);
		outSample += sineTable.lookupSinf(inPhase * freqMultiplierFloat) / freqMultiplierFloat;
	}

	return outSample;
}


} // namespace


namespace AudioChip {


AudioChip::AudioChip(const uint32_t inSampleRate, const uint32_t inNumTracks)
	: sampleRate(inSampleRate),
	  numTracks(inNumTracks)
{
	const float initFrequency = 440.0f;

	Track track;
	track.envelope.attack = 0;
	track.envelope.decay = 0;
	track.envelope.sustain = envelopeMaxParameterValue;
	track.envelope.release = 0;
	track.envelope.currentFactor = 0.0f;
	track.envelope.state = Track::EnvelopeData::State::Attack;
	track.enabled = false;

	track.phase = 0.0f;
	track.phaseIncrement = frequencyToPhaseIncrement(initFrequency, sampleRate);
	track.highestSubharmonic = calcHighestSubharmonic(initFrequency, sampleRate);

	track.pwmPhase = 0.0f;
	track.pwmPhaseIncrement = 0.0f;
	track.pwmDepth = 0.0f;

	track.generator = sineGenerator;

	tracks.reserve(numTracks);
	for (uint32_t i = 0; i < numTracks; ++i) {
		tracks.push_back(track);
	}
}


void AudioChip::renderNextSamples(float* outBuffer, const uint32_t inNumSamples) {
	assert(outBuffer != nullptr);

	const uint32_t totalSamples = inNumSamples * numChannels;
	memset(outBuffer, 0, totalSamples * sizeof(float));

	for (uint32_t trackNum = 0; trackNum < numTracks; ++trackNum) {
		Track& track = tracks[trackNum];

		if (!track.enabled) {
			continue;
		}

		const bool noteEnded = advanceEnvelope(trackNum, inNumSamples);
		if (noteEnded) {
			track.enabled = false;
			continue;
		}

		for (uint32_t sample = 0; sample < totalSamples; sample += numChannels) {
			// PWM
			float pwmPhaseOffset = 0.0f;
			if (track.pwmDepth != 0.0f) {
				const float pwmFactor = sineGenerator(track.pwmPhase, 1, 0.0f) * track.pwmDepth;
				pwmPhaseOffset = pwmFactor * M_PI;

				track.pwmPhase += track.pwmPhaseIncrement;
				if (track.pwmPhase >= pi2) {
					track.pwmPhase -= pi2;
				}
			}

			// Add track generator to mix
			const float currentSampleData = track.generator(track.phase, track.highestSubharmonic, pwmPhaseOffset) * track.envelope.currentFactor;
			outBuffer[sample] += currentSampleData;
			outBuffer[sample + 1] += currentSampleData;

			// Update track phase
			track.phase += track.phaseIncrement;
			if (track.phase >= pi2) {
				track.phase -= pi2;
			}
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

	tracks[inTrack].phase = 0.0f;
	tracks[inTrack].phaseIncrement = frequencyToPhaseIncrement(inFrequency, sampleRate);
	tracks[inTrack].highestSubharmonic = calcHighestSubharmonic(inFrequency, sampleRate);
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
	tracks[inTrack].pwmPhase = 0.0f;
	tracks[inTrack].pwmPhaseIncrement = frequencyToPhaseIncrement(inFrequency, sampleRate);
	tracks[inTrack].pwmDepth = inPWMDepth;

}


void AudioChip::disablePWM(const uint32_t inTrack) {
	assert(inTrack < numTracks);
	tracks[inTrack].pwmDepth = 0.0f;
}


bool AudioChip::advanceEnvelope(const uint32_t inTrack, const uint32_t inAdvanceSamples) {
	assert(inTrack < numTracks);

	const float elapsedTimeMs = samplesToTimeMs(inAdvanceSamples, sampleRate);
	Track::EnvelopeData& envelope = tracks[inTrack].envelope;
	float factorPerMs = 0.0f;

	switch (envelope.state) {
	case Track::EnvelopeData::State::Attack:
		if (envelope.attack == 0) {
			factorPerMs = 1.0f;
		} else {
			factorPerMs = 1.0f / (envelopeTimePerStep * envelope.attack);
		}
		envelope.currentFactor += factorPerMs * elapsedTimeMs;
		if (envelope.currentFactor >= 1.0f) {
			envelope.currentFactor = 1.0f;
			envelope.state = Track::EnvelopeData::State::Decay;
		}
		break;
	case Track::EnvelopeData::State::Decay:
		if (envelope.decay == 0) {
			factorPerMs = 1.0f;
		} else {
			factorPerMs = 1.0f / (envelopeTimePerStep * envelope.decay);
		}

		envelope.currentFactor -= factorPerMs * elapsedTimeMs;
		{
			const float sustainFactor = envelope.sustain * envelopeFactorPerStep;
			if (envelope.currentFactor <= sustainFactor) {
				envelope.currentFactor = sustainFactor;
				envelope.state = Track::EnvelopeData::State::Sustain;
			}
		}
		break;
	case Track::EnvelopeData::State::Sustain:
		if (envelope.sustain == envelopeMaxParameterValue) {
			envelope.currentFactor = 1.0f;
		} else {
			envelope.currentFactor = envelope.sustain * envelopeFactorPerStep;
		}
		break;
	case Track::EnvelopeData::State::Release:
		if (envelope.release == 0) {
			envelope.currentFactor = 0.0f;
			return true;
		} else {
			factorPerMs = 1.0f / (envelopeTimePerStep * envelope.release);

			envelope.currentFactor -= factorPerMs * elapsedTimeMs;
			if (envelope.currentFactor <= 0.0f) {
				envelope.currentFactor = 0.0f;
				return true;
			}
		}
		break;
	default:
		assert(0);
	}

	assert(envelope.currentFactor >= 0.0f && envelope.currentFactor <= 1.0f);
	return false;
}


} // namespace AudioChip
