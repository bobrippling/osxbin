#include <stdarg.h>
#include <stdnoreturn.h>

#include <CoreAudio/CoreAudio.h>

enum { LEFT_CHANNEL = 1, RIGHT_CHANNEL = 2 };

static noreturn void die(const char *fmt, ...)
{
	va_list l;
	va_start(l, fmt);
	vfprintf(stderr, fmt, l);
	va_end(l);
	exit(1);
}

static void checkHWError(OSStatus result, const char *desc)
{
	if(result == kAudioHardwareNoError)
		return;

	die("%s: %d", desc, (int)result);
}

static AudioDeviceID deviceID(void)
{
	AudioObjectPropertyAddress getDefaultOutputDevicePropertyAddress = {
		kAudioHardwarePropertyDefaultOutputDevice,
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMaster
	};

	AudioDeviceID defaultOutputDeviceID;
	UInt32 volumedataSize = sizeof(defaultOutputDeviceID);

	OSStatus result = AudioObjectGetPropertyData(
			kAudioObjectSystemObject,
			&getDefaultOutputDevicePropertyAddress,
			0, NULL,
			&volumedataSize, &defaultOutputDeviceID);

	checkHWError(result, "error getting output device");

	return defaultOutputDeviceID;
}

static void vol_set_channel(AudioDeviceID id, AudioObjectPropertyElement channel, int new_vol)
{
	AudioObjectPropertyAddress volumePropertyAddress = {
		kAudioDevicePropertyVolumeScalar,
		kAudioDevicePropertyScopeOutput,
		channel
	};

	Float32 volume = new_vol / 100.0f;

	OSStatus result = AudioObjectSetPropertyData(
			id,
			&volumePropertyAddress,
			0, NULL,
			sizeof(volume), &volume);

	checkHWError(result, "error setting volume");
}

static int vol_get_channel(AudioDeviceID id, AudioObjectPropertyElement channel)
{
	AudioObjectPropertyAddress volumePropertyAddress = {
		kAudioDevicePropertyVolumeScalar,
		kAudioDevicePropertyScopeOutput,
		channel
	};

	Float32 volume;
	unsigned len = sizeof(volume);

	OSStatus result = AudioObjectGetPropertyData(
			id,
			&volumePropertyAddress,
			0, NULL,
			&len, &volume);

	checkHWError(result, "error setting volume");

	return volume * 100;
}

static void vol_set(int new)
{
	AudioDeviceID id = deviceID();

	vol_set_channel(id, LEFT_CHANNEL, new);
	vol_set_channel(id, RIGHT_CHANNEL, new);
}

static int vol_get(void)
{
	AudioDeviceID id = deviceID();

	int left = vol_get_channel(id, LEFT_CHANNEL);
	int right = vol_get_channel(id, RIGHT_CHANNEL);

	return (left + right) / 2;
}

int main(int argc, const char *argv[])
{
	if(argc == 2){
		char *end;
		int vol = strtol(argv[1], &end, 0);

		if(*end)
			die("invalid number '%s'", argv[1]);

		vol_set(vol);
		return 0;

	}else if(argc <= 1){
		printf("%d\n", vol_get());
		return 0;

	}else{
		die("Usage: %s [volume-to-set]", argv[0]);
	}
}