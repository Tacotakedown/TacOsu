#include "Sound.h"
#include "ConVar/ConVar.h"
#include "File/File.h"

#include <bass.h>
#include <bass_fx.h>
#include <bassmix.h>

#include "Engine.h"
#include "SoundEngine/SoundEngine.h"
#include "ResourceManager/ResourceManager.h"

ConVar debug_snd("debug_snd", false);

ConVar snd_speed_compensate_pitch("snd_speed_compensate_pitch", true, "automatically keep pitch constant if speed changes");
ConVar snd_play_interp_duration("snd_play_interp_duration", 0.75f, "smooth over freshly started channel position jitter with engine time over this duration in seconds");
ConVar snd_play_interp_ratio("snd_play_interp_ratio", 0.50f, "percentage of snd_play_interp_duration to use 100% engine time over audio time (some devices report 0 for very long)");

ConVar snd_wav_file_min_size("snd_wav_file_min_size", 51, "minimum file size in bytes for WAV files to be considered valid (everything below will fail to load), this is a workaround for BASS crashes");


Sound::Sound(UString filePath, bool stream, bool threeD, bool loop, bool prescan) : Resource(filePath) {
	m_HSTREAM = 0;
	m_HSTREAMBACKUP = 0;
	m_HCHANNEL = 0;
	m_HCHANNELBACKUP = 0;

	m_bStream = stream;
	m_bIs3d = threeD;
	m_bIsLooped = loop;
	m_bPrescan = prescan;
	m_bIsOverlayable = false;

	m_fVolume = 1.0f;
	m_fLastPlayTime = -1.0f;

	m_fActualSpeedForDisabledPitchCompensation = 1.0f;


	m_wasapiSampleBuffer = NULL;
	m_iWasapiSampleBufferSize = 0;
	m_danglingWasapiStreams.reserve(32);

}

void Sound::init() {
	if (m_sFilePath.length() < 2 || !m_bAsyncReady) return;

	m_fActualSpeedForDisabledPitchCompensation = 1.0f;

	if (m_HSTREAM == 0 && m_iWasapiSampleBufferSize < 1) {
		UString msg = "Couldn't load sound \"";
		msg.append(m_sFilePath);
		msg.append(UString::format("\", stream = %i, errorcode = %i", (int)m_bStream, BASS_ErrorGetCode()));
		msg.append(", file = ");
		msg.append(m_sFilePath);
		msg.append("\n");
		debugLog(0xffdd3333, "%s", msg.toUtf8());
	}
	else {
		m_bReady = true;
	}
	m_bReady = m_bAsyncReady.load();
}

void Sound::initAsync() {
	if (ResourceManager::debug_rm->getBool())
		debugLog("Resource Manager: Loading %s\n", m_sFilePath.toUtf8());
	{
		const int minWavFileSize = snd_wav_file_min_size.getInt();
		if (minWavFileSize > 0) {
			UString fileExtensionLowerCase = env->getFileExtensionFromFilePath(m_sFilePath);
			fileExtensionLowerCase.lowerCase();
			if (fileExtensionLowerCase == "wav") {
				File wavFile(m_sFilePath);
				if (wavFile.getFileSize() < (size_t)minWavFileSize) {
					printf("Sound: Ignoring malformed/corrupt WAV file (%i) %s\n", (int)wavFile.getFileSize(), m_sFilePath.toUtf8());
					return;
				}
			}
		}
	}
	if (m_bStream) {
		DWORD extraStreamCreateFileFlags = 0;
		DWORD extraFXTempoCreateFlags = 0;
		extraStreamCreateFileFlags |= BASS_SAMPLE_FLOAT;
		extraFXTempoCreateFlags |= BASS_STREAM_DECODE;
		m_HSTREAM = BASS_StreamCreateFile(FALSE, m_sFilePath.wc_str(), 0, 0, (m_bPrescan ? BASS_STREAM_PRESCAN : 0) | BASS_STREAM_DECODE | BASS_UNICODE | extraStreamCreateFileFlags);
		m_HSTREAM = BASS_FX_TempoCreate(m_HSTREAM, BASS_FX_FREESOURCE | extraFXTempoCreateFlags);
		BASS_ChannelSetAttribute(m_HSTREAM, BASS_ATTRIB_TEMPO_OPTION_USE_QUICKALGO, true);
		BASS_ChannelSetAttribute(m_HSTREAM, BASS_ATTRIB_TEMPO_OPTION_OVERLAP_MS, 4.0f);
		BASS_ChannelSetAttribute(m_HSTREAM, BASS_ATTRIB_TEMPO_OPTION_SEQUENCE_MS, 30.0f);
		m_HCHANNELBACKUP = m_HSTREAM;
	}
	else {
		File file(m_sFilePath);
		if (file.canRead()) {
			m_iWasapiSampleBufferSize = file.getFileSize();
			if (m_iWasapiSampleBufferSize > 0) {
				m_wasapiSampleBuffer = new char[file.getFileSize()];
				memcpy(m_wasapiSampleBuffer, file.readFile(), file.getFileSize());
			}
		}
		else
			printf("Sound Error: Couldn't file.canRead() on file %s\n", m_sFilePath.toUtf8());

		m_HSTREAM = BASS_SampleLoad(FALSE, m_sFilePath.wc_str(), 0, 0, 5, (m_bIsLooped ? BASS_SAMPLE_LOOP : 0) | (m_bIs3d ? BASS_SAMPLE_3D | BASS_SAMPLE_MONO : 0) | BASS_SAMPLE_OVER_POS | BASS_UNICODE);

		m_HSTREAMBACKUP = m_HSTREAM;

		if (m_HSTREAM == 0)
			printf("Sound Error: BASS_SampleLoad() error %i on file %s\n", BASS_ErrorGetCode(), m_sFilePath.toUtf8());
	}
	m_bAsyncReady = true;
}

Sound::SOUNDHANDLE Sound::getHandle() {
	if (m_bStream) return m_HSTREAM;
	else {
		if (m_HCHANNEL == 0 || m_bIsOverlayable) {
			for (size_t i = 0; i < m_danglingWasapiStreams.size(); i++) {
				if (BASS_ChannelIsActive(m_danglingWasapiStreams[i]) != BASS_ACTIVE_PLAYING) {
					BASS_StreamFree(m_danglingWasapiStreams[i]);

					m_danglingWasapiStreams.erase(m_danglingWasapiStreams.begin() + i);
					i--;
				}
			}
			m_HCHANNEL = BASS_StreamCreateFile(TRUE, m_wasapiSampleBuffer, 0, m_iWasapiSampleBufferSize, BASS_SAMPLE_FLOAT | BASS_STREAM_DECODE | BASS_UNICODE | (m_bIsLooped ? BASS_SAMPLE_LOOP : 0));

			if (m_HCHANNEL == 0)
				debugLog("BASS_StreamCreateFile() error %i\n", BASS_ErrorGetCode());
			else {
				BASS_ChannelSetAttribute(m_HCHANNEL, BASS_ATTRIB_VOL, m_fVolume);

				m_danglingWasapiStreams.push_back(m_HCHANNEL);
			}
		}
		return m_HCHANNEL;

		if (m_HCHANNEL != 0 && !m_bIsOverlayable)
			return m_HCHANNEL;

		m_HCHANNEL = BASS_SampleGetChannel(m_HSTREAMBACKUP, FALSE);
		m_HCHANNELBACKUP = m_HCHANNEL;

		if (m_HCHANNEL == 0)
		{
			UString msg = "Couldn't BASS_SampleGetChannel \"";
			msg.append(m_sFilePath);
			msg.append(UString::format("\", stream = %i, errorcode = %i", (int)m_bStream, BASS_ErrorGetCode()));
			msg.append(", file = ");
			msg.append(m_sFilePath);
			msg.append("\n");
			debugLog(0xffdd3333, "%s", msg.toUtf8());
		}
		else
			BASS_ChannelSetAttribute(m_HCHANNEL, BASS_ATTRIB_VOL, m_fVolume);

		return m_HCHANNEL;
	}
	return 0;
}

void Sound::destroy() {
	if (!m_bReady) return;

	m_bReady = false;
	if (m_bStream) {
		BASS_Mixer_ChannelRemove(m_HSTREAM);
		BASS_StreamFree(m_HSTREAM);
	}
	else {
		if (m_HCHANNEL)
			BASS_ChannelStop(m_HCHANNEL);
		if (m_HSTREAMBACKUP)
			BASS_SampleFree(m_HSTREAMBACKUP);
		for (const SOUNDHANDLE danglingWasapiSteam : m_danglingWasapiStreams) {
			BASS_StreamFree(danglingWasapiSteam);
		}
		m_danglingWasapiStreams.clear();

		if (m_wasapiSampleBuffer != NULL) {
			delete[] m_wasapiSampleBuffer;
			m_wasapiSampleBuffer = NULL;
		}
	}
	m_HSTREAM = 0;
	m_HSTREAMBACKUP = 0;
	m_HCHANNEL = 0;
}

void Sound::setPosition(double percent) {
	if (!m_bReady) return;

	percent = clamp<double>(percent, 0.0f, 1.0f);

	const SOUNDHANDLE handle = (m_HCHANNELBACKUP != 0 ? m_HCHANNELBACKUP : getHandle());

	const QWORD length = BASS_ChannelGetLength(handle, BASS_POS_BYTE);
	const double lengthInSeconds = BASS_ChannelBytes2Seconds(handle, length);

	if (lengthInSeconds * percent < snd_play_interp_duration.getFloat())
		m_fLastPlayTime = engine->getTime() - lengthInSeconds * percent;
	else
		m_fLastPlayTime = 0.0f;

	const BOOL res = BASS_ChannelSetPosition(handle, (QWORD)((double)(length)*percent), BASS_POS_BYTE);
	if (!res && debug_snd.getBool())
		debugLog("Sound::setPosition( %f ) BASS_ChannelSetPosition() error %i on file %s\n", percent, BASS_ErrorGetCode(), m_sFilePath.toUtf8());
}

void Sound::setPositionMS(unsigned long ms, bool internal) {
	if (!m_bReady || ms > getLengthMS()) return;

	const SOUNDHANDLE handle = getHandle();
	const QWORD position = BASS_ChannelSeconds2Bytes(handle, ms / 1000.0);

	if ((double)ms / 1000.0 < snd_play_interp_duration.getFloat())
		m_fLastPlayTime = engine->getTime() - ((double)ms / 1000 / 0);
	else
		m_fLastPlayTime = 0.0;

	const BOOL res = BASS_ChannelSetPosition(handle, position, BASS_POS_BYTE);
	if (!res && !internal && debug_snd.getBool())
		debugLog("Sound::setPositionMS( %lu ) BASS_ChannelSetPosition() error %i on file %s\n", ms, BASS_ErrorGetCode(), m_sFilePath.toUtf8());
}

void Sound::setVolume(float volume) {
	if (!m_bReady) return;

	m_fVolume = clamp<float>(volume, 0.0f, 1.0f);

	if (!m_bIsOverlayable) {
		const SOUNDHANDLE handle = getHandle();
		BASS_ChannelSetAttribute(handle, BASS_ATTRIB_VOL, m_fVolume);
	}
}

void Sound::setSpeed(float speed) {
	if (!m_bReady) return;

	speed = clamp<float>(speed, 0.05f, 50.0f);

	const SOUNDHANDLE handle = getHandle();

	float originalFreq = 44100.0f;
	BASS_ChannelGetAttribute(handle, BASS_ATTRIB_FREQ, &originalFreq);

	BASS_ChannelSetAttribute(handle, (snd_speed_compensate_pitch.getBool() ? BASS_ATTRIB_TEMPO : BASS_ATTRIB_TEMPO_FREQ), (snd_speed_compensate_pitch.getBool() ? (speed - 1.0f) * 100.0f : speed * originalFreq));

	m_fActualSpeedForDisabledPitchCompensation = speed;
}

void Sound::setPitch(float pitch) {
	if (!m_bReady) return;

	pitch = clamp<float>(pitch, 0.0f, 2.0f);
	const SOUNDHANDLE handle = getHandle();
	BASS_ChannelSetAttribute(handle, BASS_ATTRIB_TEMPO_PITCH, (pitch - 1.0f) * 60.0f);
}

void Sound::setFrequency(float frequency) {
	if (!m_bReady) return;


	frequency = (frequency > 99.0f ? clamp<float>(frequency, 100.0f, 100000.0f) : 0.0f);

	const SOUNDHANDLE handle = getHandle();

	BASS_ChannelSetAttribute(handle, BASS_ATTRIB_FREQ, frequency);
}

void Sound::setPan(float pan) {
	if (!m_bReady) return;

	pan = clamp<float>(pan, -1.0f, 1.0f);
	const SOUNDHANDLE handle = getHandle();
	BASS_ChannelSetAttribute(handle, BASS_ATTRIB_PAN, pan);
}

void Sound::setLoop(bool loop) {
	if (!m_bReady) return;
	m_bIsLooped = loop;

	const SOUNDHANDLE handle = getHandle();
	BASS_ChannelFlags(handle, m_bIsLooped ? BASS_SAMPLE_LOOP : 0, BASS_SAMPLE_LOOP);
}

float Sound::getPosition() {
	if (!m_bReady) return 0.0f;

	const SOUNDHANDLE handle = getHandle();

	const QWORD lengthBytes = BASS_ChannelGetLength(handle, BASS_POS_BYTE);
	const QWORD positionBytes = BASS_ChannelGetPosition(handle, BASS_POS_BYTE);

	const float position = (float)((double)(positionBytes) / (double)(lengthBytes));

	return position;
}

unsigned long Sound::getPositionMS() {
	if (!m_bReady) return 0;

	const SOUNDHANDLE handle = getHandle();

	const QWORD position = BASS_ChannelGetPosition(handle, BASS_POS_BYTE);

	const double positionInSeconds = BASS_ChannelBytes2Seconds(handle, position);
	const double positionInMilliSeconds = positionInSeconds * 1000.0;

	const unsigned long positionMS = static_cast<unsigned long>(positionInMilliSeconds);
	const double interpDuration = snd_play_interp_duration.getFloat();
	const unsigned long interpDurationMS = interpDuration * 1000;
	if (interpDuration > 0.0 && positionMS < interpDurationMS) {
		const float speedMultiplier = getSpeed();
		const double delta = (engine->getTime() - m_fLastPlayTime) * speedMultiplier;
		if (m_fLastPlayTime > 0.0 && delta < interpDuration && isPlaying()) {
			const double lerpPercent = clamp<double>(((delta / interpDuration) - snd_play_interp_ratio.getFloat()) / (1.0 - snd_play_interp_ratio.getFloat()), 0.0, 1.0);
			return static_cast<unsigned long>(lerp<double>(delta * 1000.0, (double)positionMS, lerpPercent));
		}
	}
}

unsigned long Sound::getLengthMS() {
	if (!m_bReady) return 0;

	const SOUNDHANDLE handle = getHandle();

	const QWORD length = BASS_ChannelGetLength(handle, BASS_POS_BYTE);

	const double lengthInSeconds = BASS_ChannelBytes2Seconds(handle, length);
	const double lengthInMilliSeconds = lengthInSeconds * 1000.0;

	return static_cast<unsigned long>(lengthInMilliSeconds);
}

float Sound::getSpeed() {
	if (!m_bReady) return 1.0f;

	if (!snd_speed_compensate_pitch.getBool())
		return m_fActualSpeedForDisabledPitchCompensation;

	const SOUNDHANDLE handle = getHandle();

	float speed = 0.0f;
	BASS_ChannelGetAttribute(handle, BASS_ATTRIB_TEMPO, &speed);

	return ((speed / 100.0f) + 1.0f);
}

float Sound::getPitch() {
	if (!m_bReady) return 1.0f;

	const SOUNDHANDLE handle = getHandle();

	float pitch = 0.0f;
	BASS_ChannelGetAttribute(handle, BASS_ATTRIB_TEMPO_PITCH, &pitch);

	return ((pitch / 60.0f) + 1.0f);
}

float Sound::getFrequency() {
	if (!m_bReady) return 44100.0f;

	const SOUNDHANDLE handle = getHandle();

	float frequency = 44100.0f;
	BASS_ChannelGetAttribute(handle, BASS_ATTRIB_FREQ, &frequency);

	return frequency;
}

bool Sound::isPlaying() {
	if (!m_bReady) return false;

	const SOUNDHANDLE handle = getHandle();

	return BASS_ChannelIsActive(handle) == BASS_ACTIVE_PLAYING && ((!m_bStream && m_bIsOverlayable) || BASS_Mixer_ChannelGetMixer(handle) != 0);
}

bool Sound::isFinished() {
	if (!m_bReady) return false;

	const SOUNDHANDLE handle = getHandle();

	return BASS_ChannelIsActive(handle) == BASS_ACTIVE_STOPPED;
}

void Sound::rebuild(UString newFilePath) {
	m_sFilePath = newFilePath;

	reload();
}