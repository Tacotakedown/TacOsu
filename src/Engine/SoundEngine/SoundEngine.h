#ifndef SOUNDENGINE_H
#define SOUNDENGINE_H

#include "cbase.h"

class Sound;

class SoundEngineThread;

class SoundEngine {
public:
	SoundEngine();
	~SoundEngine();

	void restart();
	void update();

	bool play(Sound* snd, float pan = 0.0f, float pitch = 1.0f);
	bool play3d(Sound* snd, Vector3 pos);
	void pause(Sound* snd);
	void stop(Sound* snd);

	void setOnOutputDeviceChange(std::function<void()> callback);
	void setOutputDevice(UString outputDeviceName);
	void setOutputDeviceForce(UString outputDeviceName);
	void setVolume(float volume);
	void set3dPosition(Vector3 headPos, Vector3 viewDir, Vector3 viewUp);

	std::vector<UString> getOutputDevices();

	inline const UString& getOutputDevice() const { return m_sCurrentOutputDevice; }
	inline float getVolume() const { return m_fVolume; }



private:
	struct OUTPUT_DEVICE
	{
		int id;
		bool enabled;
		bool isDefault;
		UString name;
	};

	void updateOutputDevices(bool handleOutputDeviceChanges, bool printInfo);
	bool initializeOutputDevice(int id = -1);

	void onFreqChanged(UString oldValue, UString newValue);

	bool m_bReady;

	float m_fPrevOutputDeviceChangeCheckTime;
	std::function<void()> m_outputDeviceChangeCallback;
	std::vector<OUTPUT_DEVICE> m_outputDevices;

	int m_iCurrentOutputDevice;
	UString m_sCurrentOutputDevice;

	float m_fVolume;

	uint32_t m_iBASSVersion;

	SoundEngineThread* m_thread;


};

#endif // !SOUNDENGINE_H
