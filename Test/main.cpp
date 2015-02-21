#include <alsa/asoundlib.h>
#include <assert.h>
#include <cstdio>
#include <memory>
#include <pthread.h>
#include "../AudioChip.h"


namespace {


const char* alsaDevice = "plughw:0,0";
const uint32_t numChannels = 2;
const uint32_t bufferSize = 256;
const uint32_t sampleRate = 44100;


bool isRunning;
snd_pcm_t* outputDevice;
snd_output_t* alsaLog;
float outputBuffer[bufferSize * numChannels];
pthread_t pthread;


void initAudio() {
    memset(outputBuffer, 0, (bufferSize * numChannels * sizeof(float)));

    snd_pcm_info_t* info;
    snd_pcm_info_alloca(&info);

    int32_t err = snd_output_stdio_attach(&alsaLog, stderr, 0);
    assert(err >= 0);

    err = snd_pcm_open(&outputDevice, alsaDevice, SND_PCM_STREAM_PLAYBACK, 0);
    assert(err >=0);

    err = snd_pcm_info(outputDevice, info);
    assert(err >= 0);

    err = snd_pcm_nonblock(outputDevice, 0);
    assert(err >= 0);

    snd_pcm_hw_params_t* params;
    snd_pcm_sw_params_t* swparams;

    snd_pcm_hw_params_alloca(&params);
    snd_pcm_sw_params_alloca(&swparams);
    err = snd_pcm_hw_params_any(outputDevice, params);
    assert(err >= 0);

    err = snd_pcm_hw_params_set_access(outputDevice, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    assert(err >= 0);

    err = snd_pcm_hw_params_set_format(outputDevice, params, SND_PCM_FORMAT_FLOAT_LE);
    assert(err >= 0);

    err = snd_pcm_hw_params_set_channels(outputDevice, params, numChannels);
    assert(err >= 0);

    {
		uint32_t wantedSampleRate = sampleRate;
		err = snd_pcm_hw_params_set_rate_near(outputDevice, params, &wantedSampleRate, 0);
		assert(err >= 0);
		assert(wantedSampleRate == sampleRate);
    }

	snd_pcm_uframes_t periodFrames = bufferSize;
	snd_pcm_uframes_t bufferFrames = bufferSize * 4;
	err = snd_pcm_hw_params_set_period_size_near(outputDevice, params, &periodFrames, 0);
	assert(err >= 0);
	assert(periodFrames == bufferSize);

    err = snd_pcm_hw_params_set_buffer_size_near(outputDevice, params, &bufferFrames);
    assert(err >= 0);

    err = snd_pcm_hw_params(outputDevice, params);
    assert(err >= 0);

    snd_pcm_sw_params_current(outputDevice, swparams);

    err = snd_pcm_sw_params_set_avail_min(outputDevice, swparams, 0);
    assert(err >= 0);

    err = snd_pcm_sw_params_set_start_threshold(outputDevice, swparams, 0);
    assert(err >= 0);

    err = snd_pcm_sw_params_set_stop_threshold(outputDevice, swparams, bufferFrames);
    assert(err >= 0);

    err = snd_pcm_sw_params(outputDevice, swparams);
    assert(err >= 0);
}


void disposeAudio() {
    snd_pcm_close(outputDevice);
    outputDevice = nullptr;
    snd_output_close(alsaLog);
    snd_config_update_free_global();
}


void handleXRun(snd_pcm_t* iDeviceHandle) {
	snd_pcm_status_t* alsaDeviceStatus;
	snd_pcm_status_alloca(&alsaDeviceStatus);

	{
		const int32_t result = snd_pcm_status(iDeviceHandle, alsaDeviceStatus);
		assert(result <= 0);
	}

	if (snd_pcm_status_get_state(alsaDeviceStatus) == SND_PCM_STATE_XRUN) {
		const int32_t result = snd_pcm_prepare(iDeviceHandle);
		assert(result <= 0);
		return;
	} else if (snd_pcm_status_get_state(alsaDeviceStatus) == SND_PCM_STATE_DRAINING) {
		assert(false);
	}

	fprintf(stderr, "read/write error\n");
	assert(false);
}


void* startAudio(void* inThreadData) {
	assert(inThreadData != nullptr);
	AudioChip::AudioChip* audioChip = reinterpret_cast<AudioChip::AudioChip*>(inThreadData);

    isRunning = true;

    // Pre-fill output device
    snd_pcm_writei(outputDevice, outputBuffer, bufferSize);
    snd_pcm_writei(outputDevice, outputBuffer, bufferSize);
    snd_pcm_writei(outputDevice, outputBuffer, bufferSize);
    snd_pcm_writei(outputDevice, outputBuffer, bufferSize);

    ssize_t framesToWrite = bufferSize;
    while (isRunning) {
    	audioChip->renderNextSamples(outputBuffer, bufferSize);

        while (framesToWrite > 0) {
        	ssize_t framesWritten = snd_pcm_writei(outputDevice, outputBuffer, framesToWrite);

            if (framesWritten == -EPIPE) {
            	handleXRun(outputDevice);
            } else if (framesWritten < 0 && framesWritten != -EAGAIN) {
                assert(false);
            }

            if (framesWritten > 0) {
                framesToWrite -= framesWritten;
            }
        }
        framesToWrite = bufferSize;
    }

    return NULL;
}


void stopAudio() {
    isRunning = false;
}


void createThread(AudioChip::AudioChip* inAudioChip) {
    pthread_attr_t pthreadAttributes;
    int status;

	status = pthread_attr_init(&pthreadAttributes);
	assert(status == 0);

	status = pthread_create(&pthread, &pthreadAttributes, startAudio, inAudioChip);
	assert(status == 0);
}


void joinThread() {
	int status;
	void* returnData;
	status = pthread_join(pthread, &returnData);
	assert(status == 0);
}


} // namespace


int main(int argc, char** argv) {
	const uint32_t numTracks = 4;
	std::unique_ptr<AudioChip::AudioChip> audioChip(new AudioChip::AudioChip(sampleRate, numTracks));

	initAudio();

	audioChip->setWaveformType(0, AudioChip::AudioChip::WaveformType::Square);
	audioChip->setFrequency(0, 100.0f);
	audioChip->enablePWM(0, 0.2f, 0.9f);
	audioChip->setEnvelope(0, 5, 5, 100, 5);
	audioChip->noteOn(0);

	createThread(audioChip.get());
	sleep(2);
	audioChip->noteOff(0);
	sleep(1);

	stopAudio();
	joinThread();

	disposeAudio();
	return 0;
}
