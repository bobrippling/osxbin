#include <stdarg.h>
#include <stdnoreturn.h>

#include <CoreAudio/CoreAudio.h>

enum { VOL_STEP = 2 };

enum { LEFT_CHANNEL = 1, RIGHT_CHANNEL = 2 };

static const char *argv0;

static noreturn void die(const char *fmt, ...)
{
	va_list l;
	va_start(l, fmt);
	vfprintf(stderr, fmt, l);
	va_end(l);
	fputc('\n', stderr);
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

	checkHWError(result, "error getting volume");

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

static void interactive(void)
{
	system("stty -echo -icanon");

	int vol = vol_get();
	for(;;){
		printf("j/k: %d%% \r", vol);

		int ch = getchar();

		switch(ch){
			case EOF:
			case 'q':
			case 4: /* ctrl-d */
				return;

			case 12: /* ctrl-l */
				system("clear");
				break;

			case 'r':
			case 'o':
				vol = vol_get();
				break;

			case 'k':
				vol_set(++vol);
				break;
			case 'j':
				vol_set(--vol);
				break;
		}
	}
}

static void dieIfErr(int ec)
{
	if(ec == noErr)
		return;
	die("%s: error", argv0);
}

static void set_output(AudioObjectID id)
{
	dieIfErr(
			AudioObjectSetPropertyData(
				kAudioObjectSystemObject,
				&(const AudioObjectPropertyAddress){
					.mSelector = kAudioHardwarePropertyDefaultOutputDevice,
					.mScope = kAudioObjectPropertyScopeGlobal,
					.mElement = kAudioObjectPropertyElementMaster,
				},
				0,
				NULL,
				sizeof(id),
				&id));
}

static AudioObjectID get_output(void)
{
	AudioObjectID id;

	dieIfErr(
			AudioObjectGetPropertyData(
				kAudioObjectSystemObject,
				&(const AudioObjectPropertyAddress){
					.mSelector = kAudioHardwarePropertyDefaultOutputDevice,
					.mScope = kAudioObjectPropertyScopeGlobal,
					.mElement = kAudioObjectPropertyElementMaster,
				},
				0,
				NULL,
				&(UInt32){ sizeof(id) },
				&id));

	return id;
}

static AudioObjectID *list_devices(size_t *const count)
{
	const AudioObjectPropertyAddress global_master_addr = {
		.mSelector = kAudioHardwarePropertyDevices,
		.mScope = kAudioObjectPropertyScopeGlobal,
		.mElement = kAudioObjectPropertyElementMaster,
	};
	UInt32 global_master_count;
	dieIfErr(AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &global_master_addr, 0, NULL, &global_master_count));

	size_t dev_count = global_master_count / sizeof(AudioDeviceID);
	AudioObjectID *device_ids = calloc(dev_count, sizeof(AudioDeviceID));

	if(!device_ids)
		die("out of memory\n");

	dieIfErr(AudioObjectGetPropertyData(kAudioObjectSystemObject, &global_master_addr, 0, NULL, &global_master_count, device_ids));

	*count = dev_count;
	return device_ids;
}

static void free_devices(AudioObjectID *devices)
{
	free(devices);
}

static void list(void)
{
	size_t n;
	AudioObjectID *devices = list_devices(&n);

	for(size_t i = 0; i < n; i++){
		const AudioObjectID id = devices[i];

		const AudioObjectPropertyAddress name_addr = {
			.mSelector = kAudioDevicePropertyDeviceName,
			.mScope = kAudioObjectPropertyScopeGlobal,
			.mElement = kAudioObjectPropertyElementMaster,
		};
		char deviceName[64];
		dieIfErr(AudioObjectGetPropertyData(id, &name_addr, 0, NULL, &(UInt32){ sizeof(deviceName) }, deviceName));

		printf("%s device %d: %s\n", id == get_output() ? "*" : "-", id, deviceName);
	}

	free_devices(devices);
}

static void toggle(void)
{
	const AudioDeviceID id = get_output();
	size_t n;
	AudioObjectID *devices = list_devices(&n);

	if(n < 2)
		die("%s: only found a single device", __func__);

	for(size_t i = 0; i < n; i++){
		if(devices[i] == id){
			if(i + 1 == n){
				set_output(devices[0]);
			}else{
				set_output(devices[i + 1]);
			}
			break;
		}
	}

	free_devices(devices);
}

static bool is_all(const char *str, const char ch)
{
	return strspn(str, (const char[]){ ch, '\0' }) == strlen(str);
}

static bool is_prefix(const char *pre, const char *full)
{
	return pre[0] && !strncmp(pre, full, strlen(pre));
}

static int estrtol(const char *str)
{
	char *end;
	int vol = strtol(str, &end, 0);

	if(*end)
		die("%s: invalid number '%s'", argv0, str);

	return vol;
}

int main(int argc, const char *argv[])
{
	argv0 = argv[0];

	if(argc == 2){
		if(!strcmp(argv[1], "-i")){
			interactive();
		}else if(is_prefix(argv[1], "toggle")){
			toggle();
		}else if(is_prefix(argv[1], "list") || is_prefix(argv[1], "ls")){
			list();

		}else if(strchr("+-", argv[1][0])){
			int direction = argv[1][0] == '+' ? 1 : -1;

			if(is_all(argv[1], argv[1][0])){
				vol_set(vol_get() + direction * VOL_STEP * (int)strlen(argv[1]));
			}else{
				vol_set(vol_get() + direction * estrtol(argv[1] + 1));
			}
		}else{
			vol_set(estrtol(argv[1]));
		}
	}else if(argc <= 1){
		printf("%d\n", vol_get());

	}else{
		die("Usage: %s [+... | -... | -i | t[oggle] | l[ist|s] | volume-to-set]\n"
				" e.g. %s +++\n",
				"      %s -20\n",
				"      %s 31\n",
				argv[0]);
	}

	return 0;
}
