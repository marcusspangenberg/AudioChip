# AudioChip

A simple synth voice generator that was originally created for a retro game engine. The state of the generator is changed by calling the appropriate member functions. Time is progressed by calling renderNextSamples() to generate the wanted amount of samples whenever new data is needed. 

The output buffer is filled with interleaved stereo float samples.

```
/** Render inNumSamples samples to outBuffer. */
void renderNextSamples(float* outBuffer, const uint32_t inNumSamples);

/** Reset the envelope of inTrack and enable the track. */
void noteOn(const uint32_t inTrack);

/** Sets envelope state to release. */
void noteOff(const uint32_t inTrack);

/** Set note frequency in Hz. */
void setFrequency(const uint32_t inTrack, const float inFrequency);

/** Set wave form to any of WaveformType::Sine, WaveformType::Square, WaveformType::Noise or WaveformType::Saw. */
void setWaveformType(const uint32_t inTrack, const WaveformType inWaveformType);

/** Set track envelope. Does not reset the envelope if it is playing. Valid parameter ranges are between 0 and 126. */
void setEnvelope(const uint32_t inTrack, const uint8_t inAttack, const uint8_t inDecay, const uint8_t inSustain, const uint8_t inRelease);

/** Enable pulse width modulation for the square waveform type. Modulate with a sine wave LFO with the specified frequency. Valid range for inPWMDepth is 0.0f to 1.0f. */
void enablePWM(const uint32_t inTrack, const float inFrequency, const float inPWMDepth);

/** Disable pulse width modulation. */
void disablePWM(const uint32_t inTrack);
```
