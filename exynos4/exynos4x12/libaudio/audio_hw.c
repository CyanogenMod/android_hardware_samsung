/*
 * Copyright (C) 2012 Wolfson Microelectronics plc
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Liberal inspiration drawn from the AOSP code for Toro.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "tiny_hw"
#define LOG_NDEBUG 0

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>

#include <cutils/log.h>
#include <cutils/properties.h>
#include <cutils/str_parms.h>

#include <hardware/hardware.h>
#include <system/audio.h>
#include <hardware/audio.h>

#include <expat.h>

#include <tinyalsa/asoundlib.h>
#include <audio_utils/resampler.h>
#include <hardware/audio_effect.h>

struct route_setting
{
    char *ctl_name;
    int intval;
    char *strval;
};

/* The enable flag when 0 makes the assumption that enums are disabled by
 * "Off" and integers/booleans by 0 */
static int set_route_by_array(struct mixer *mixer, struct route_setting *route,
			      unsigned int len)
{
    struct mixer_ctl *ctl;
    unsigned int i, j, ret;

    /* Go through the route array and set each value */
    for (i = 0; i < len; i++) {
        ctl = mixer_get_ctl_by_name(mixer, route[i].ctl_name);
        if (!ctl) {
	    LOGE("Unknown control '%s'\n", route[i].ctl_name);
            return -EINVAL;
	}

        if (route[i].strval) {
	    ret = mixer_ctl_set_enum_by_string(ctl, route[i].strval);
	    if (ret != 0) {
		LOGE("Failed to set '%s' to '%s'\n",
		     route[i].ctl_name, route[i].strval);
	    } else {
		LOGV("Set '%s' to '%s'\n",
		     route[i].ctl_name, route[i].strval);
	    }
	    
        } else {
            /* This ensures multiple (i.e. stereo) values are set jointly */
            for (j = 0; j < mixer_ctl_get_num_values(ctl); j++) {
		ret = mixer_ctl_set_value(ctl, j, route[i].intval);
		if (ret != 0) {
		    LOGE("Failed to set '%s'.%d to %d\n",
			 route[i].ctl_name, j, route[i].intval);
		} else {
		    LOGV("Set '%s'.%d to %d\n",
			 route[i].ctl_name, j, route[i].intval);
		}
	    }
        }
    }

    return 0;
}

struct tiny_dev_cfg {
    int mask;

    struct route_setting *on;
    unsigned int on_len;

    struct route_setting *off;
    unsigned int off_len;
};

struct tiny_audio_device {
    struct audio_hw_device device;
    struct mixer *mixer;

    int mode;

    pthread_mutex_t route_lock;
    struct tiny_dev_cfg *dev_cfgs;
    int num_dev_cfgs;
    int active_devices;
    int devices;

    bool mic_mute;
};

struct tiny_stream_out {
    struct audio_stream_out stream;

    struct tiny_audio_device *adev;

    struct pcm_config config;
    struct pcm *pcm;
};

#define MAX_PREPROCESSORS 10

struct tiny_stream_in {
    struct audio_stream_in stream;

    pthread_mutex_t lock;

    struct tiny_audio_device *adev;

    struct pcm_config config;
    struct pcm *pcm;

    struct resampler_itfe *resampler;
    struct resampler_buffer_provider buf_provider;
    int16_t *buffer;
    size_t frames_in;
    unsigned int requested_rate;
    int standby;
    int source;
    effect_handle_t preprocessors[MAX_PREPROCESSORS];
    int num_preprocessors;
    int16_t *proc_buf;
    size_t proc_buf_size;
    size_t proc_frames_in;
    int read_status;
};

/* Must be called with route_lock */
void select_devices(struct tiny_audio_device *adev)
{
    int i;

    if (adev->active_devices == adev->devices)
	return;

    LOGV("Changing devices %x => %x\n", adev->active_devices, adev->devices);

    /* Turn on new devices first so we don't glitch due to powerdown... */
    for (i = 0; i < adev->num_dev_cfgs; i++)
	if ((adev->devices & adev->dev_cfgs[i].mask) &&
	    !(adev->active_devices & adev->dev_cfgs[i].mask))
	    set_route_by_array(adev->mixer, adev->dev_cfgs[i].on,
			       adev->dev_cfgs[i].on_len);

    /* ...then disable old ones. */
    for (i = 0; i < adev->num_dev_cfgs; i++)
	if (!(adev->devices & adev->dev_cfgs[i].mask) &&
	    (adev->active_devices & adev->dev_cfgs[i].mask))
	    set_route_by_array(adev->mixer, adev->dev_cfgs[i].off,
			       adev->dev_cfgs[i].off_len);

    adev->active_devices = adev->devices;
}

static uint32_t out_get_sample_rate(const struct audio_stream *stream)
{
    return 44100;
}

static int out_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    if (rate == out_get_sample_rate(stream))
	return 0;
    else
	return -EINVAL;
}

static size_t out_get_buffer_size(const struct audio_stream *stream)
{
    return 4096;
}

static uint32_t out_get_channels(const struct audio_stream *stream)
{
    return AUDIO_CHANNEL_OUT_STEREO;
}

static int out_get_format(const struct audio_stream *stream)
{
    return AUDIO_FORMAT_PCM_16_BIT;
}

static int out_set_format(struct audio_stream *stream, int format)
{
    return 0;
}

static int out_standby(struct audio_stream *stream)
{
    struct tiny_stream_out *out = (struct tiny_stream_out *)stream;
    int ret;

    if (out->pcm) {
	LOGV("out_standby(%p) closing PCM\n", stream);
	ret = pcm_close(out->pcm);
	if (ret != 0) {
	    LOGE("out_standby(%p) failed: %d\n", stream, ret);
	    return ret;
	}
	out->pcm = NULL;
    }

    return 0;
}

static int out_dump(const struct audio_stream *stream, int fd)
{
    return 0;
}

static int out_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    struct tiny_stream_out *out = (struct tiny_stream_out *)stream;
    struct tiny_audio_device *adev = out->adev;
    struct str_parms *parms;
    char *str;
    char value[32];
    int ret, val = 0;
    bool force_input_standby = false;

    parms = str_parms_create_str(kvpairs);

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_ROUTING,
			    value, sizeof(value));
    if (ret >= 0) {
        val = atoi(value);

	if (val != 0) {
	    pthread_mutex_lock(&adev->route_lock);

            adev->devices &= ~AUDIO_DEVICE_OUT_ALL;
            adev->devices |= val;
            select_devices(adev);

	    pthread_mutex_unlock(&adev->route_lock);
	} else {
	    LOGW("output routing with no devices\n");
	}
    }

    str_parms_destroy(parms);

    return ret;
}

static char * out_get_parameters(const struct audio_stream *stream, const char *keys)
{
    return strdup("");
}

static uint32_t out_get_latency(const struct audio_stream_out *stream)
{
    return 0;
}

static int out_set_volume(struct audio_stream_out *stream, float left,
                          float right)
{
    /* Use the soft volume control for now; AudioFlinger rarely
     * actually calls down. */
    return -EINVAL;
}

static ssize_t out_write(struct audio_stream_out *stream, const void* buffer,
                         size_t bytes)
{
    struct tiny_stream_out *out = (struct tiny_stream_out *)stream;
    int ret;

    if (!out->pcm) {
	LOGV("out_write(%p) opening PCM\n", stream);
	out->pcm = pcm_open(0, 0, PCM_OUT | PCM_MMAP, &out->config);

	if (!pcm_is_ready(out->pcm)) {
	    LOGE("Failed to open output PCM: %s", pcm_get_error(out->pcm));
	    pcm_close(out->pcm);
	    return -EBUSY;
	}
    }

    ret = pcm_mmap_write(out->pcm, buffer, bytes);
    if (ret != 0) {
	LOGE("out_write(%p) failed: %d\n", stream, ret);
	return ret;
    }

    return bytes;
}

static int out_get_render_position(const struct audio_stream_out *stream,
                                   uint32_t *dsp_frames)
{
    return -EINVAL;
}

static int out_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}

static int out_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}

/** audio_stream_in implementation **/
static uint32_t in_get_sample_rate(const struct audio_stream *stream)
{
    return 8000;
}

static int in_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    return 0;
}

static size_t in_get_buffer_size(const struct audio_stream *stream)
{
    return 320;
}

static uint32_t in_get_channels(const struct audio_stream *stream)
{
    return AUDIO_CHANNEL_IN_MONO;
}

static int in_get_format(const struct audio_stream *stream)
{
    return AUDIO_FORMAT_PCM_16_BIT;
}

static int in_set_format(struct audio_stream *stream, int format)
{
    return 0;
}

static int in_standby(struct audio_stream *stream)
{
    return 0;
}

static int in_dump(const struct audio_stream *stream, int fd)
{
    return 0;
}

static int in_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    return 0;
}

static char * in_get_parameters(const struct audio_stream *stream,
                                const char *keys)
{
    return strdup("");
}

static int in_set_gain(struct audio_stream_in *stream, float gain)
{
    return 0;
}

static ssize_t in_read(struct audio_stream_in *stream, void* buffer,
                       size_t bytes)
{
    /* XXX: fake timing for audio input */
    usleep(bytes * 1000000 / audio_stream_frame_size(&stream->common) /
           in_get_sample_rate(&stream->common));
    return bytes;
}

static uint32_t in_get_input_frames_lost(struct audio_stream_in *stream)
{
    return 0;
}

static int in_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}

static int in_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}

static int adev_open_output_stream(struct audio_hw_device *dev,
                                   uint32_t devices, int *format,
                                   uint32_t *channels, uint32_t *sample_rate,
                                   struct audio_stream_out **stream_out)
{
    struct tiny_audio_device *adev = (struct tiny_audio_device *)dev;
    struct tiny_stream_out *out;
    int ret;

    out = calloc(1, sizeof(struct tiny_stream_out));
    if (!out)
        return -ENOMEM;

    out->stream.common.get_sample_rate = out_get_sample_rate;
    out->stream.common.set_sample_rate = out_set_sample_rate;
    out->stream.common.get_buffer_size = out_get_buffer_size;
    out->stream.common.get_channels = out_get_channels;
    out->stream.common.get_format = out_get_format;
    out->stream.common.set_format = out_set_format;
    out->stream.common.standby = out_standby;
    out->stream.common.dump = out_dump;
    out->stream.common.set_parameters = out_set_parameters;
    out->stream.common.get_parameters = out_get_parameters;
    out->stream.common.add_audio_effect = out_add_audio_effect;
    out->stream.common.remove_audio_effect = out_remove_audio_effect;
    out->stream.get_latency = out_get_latency;
    out->stream.set_volume = out_set_volume;
    out->stream.write = out_write;
    out->stream.get_render_position = out_get_render_position;

    out->adev = adev;

    pthread_mutex_lock(&adev->route_lock);
    adev->devices &= ~AUDIO_DEVICE_OUT_ALL;
    adev->devices |= devices;
    select_devices(adev);
    pthread_mutex_unlock(&adev->route_lock);

    *channels = out_get_channels(&out->stream.common);
    *format = out_get_format(&out->stream.common);
    *sample_rate = out_get_sample_rate(&out->stream.common);

    /* Should query the driver for parameters and compute defaults
     * from those; should also support configuration from file and
     * buffer resizing.
     */
    out->config.channels = 2;
    out->config.rate = out_get_sample_rate(&out->stream.common);
    out->config.period_count = 4;
    out->config.period_size = 1024;
    out->config.format = PCM_FORMAT_S16_LE;

    LOGV("Opened output stream %p\n", out);

    *stream_out = &out->stream;
    return 0;

err_open:
    free(out);
    *stream_out = NULL;
    return ret;
}

static void adev_close_output_stream(struct audio_hw_device *dev,
                                     struct audio_stream_out *stream)
{
    struct tiny_stream_out *out = (struct tiny_stream_out *)stream;
    LOGV("Closing output stream %p\n", stream);
    if (out->pcm)
	pcm_close(out->pcm);
    free(stream);
}

static int adev_set_parameters(struct audio_hw_device *dev, const char *kvpairs)
{
    return -ENOSYS;
}

static char * adev_get_parameters(const struct audio_hw_device *dev,
                                  const char *keys)
{
    return NULL;
}

static int adev_init_check(const struct audio_hw_device *dev)
{
    return 0;
}

static int adev_set_voice_volume(struct audio_hw_device *dev, float volume)
{
    return -ENOSYS;
}

static int adev_set_master_volume(struct audio_hw_device *dev, float volume)
{
    return -ENOSYS;
}

static int adev_set_mode(struct audio_hw_device *dev, int mode)
{
    return 0;
}

static int adev_set_mic_mute(struct audio_hw_device *dev, bool state)
{
    return -ENOSYS;
}

static int adev_get_mic_mute(const struct audio_hw_device *dev, bool *state)
{
    return -ENOSYS;
}

static size_t adev_get_input_buffer_size(const struct audio_hw_device *dev,
                                         uint32_t sample_rate, int format,
                                         int channel_count)
{
    return 320;
}

static int adev_open_input_stream(struct audio_hw_device *dev, uint32_t devices,
                                  int *format, uint32_t *channels,
                                  uint32_t *sample_rate,
                                  audio_in_acoustics_t acoustics,
                                  struct audio_stream_in **stream_in)
{
    struct tiny_audio_device *adev = (struct tiny_audio_device *)dev;
    struct tiny_stream_in *in;
    int ret;
    int channel_count = popcount(*channels);

    in = calloc(1, sizeof(struct tiny_stream_in));
    if (!in)
        return -ENOMEM;

    pthread_mutex_init(&in->lock, NULL);
    in->adev = adev;

    in->stream.common.get_sample_rate = in_get_sample_rate;
    in->stream.common.set_sample_rate = in_set_sample_rate;
    in->stream.common.get_buffer_size = in_get_buffer_size;
    in->stream.common.get_channels = in_get_channels;
    in->stream.common.get_format = in_get_format;
    in->stream.common.set_format = in_set_format;
    in->stream.common.standby = in_standby;
    in->stream.common.dump = in_dump;
    in->stream.common.set_parameters = in_set_parameters;
    in->stream.common.get_parameters = in_get_parameters;
    in->stream.common.add_audio_effect = in_add_audio_effect;
    in->stream.common.remove_audio_effect = in_remove_audio_effect;
    in->stream.set_gain = in_set_gain;
    in->stream.read = in_read;
    in->stream.get_input_frames_lost = in_get_input_frames_lost;

    pthread_mutex_lock(&adev->route_lock);
    adev->devices &= ~AUDIO_DEVICE_IN_ALL;
    adev->devices |= devices;
    select_devices(adev);
    pthread_mutex_unlock(&adev->route_lock);

    in->config.channels = 2;
    in->config.rate = 44100;
    in->config.period_count = 4;
    in->config.period_size = 320;
    in->config.format = PCM_FORMAT_S16_LE;

    *stream_in = &in->stream;
    return 0;

err_open:
    free(in);
    *stream_in = NULL;
    return ret;
}

static void adev_close_input_stream(struct audio_hw_device *dev,
                                   struct audio_stream_in *stream)
{
    struct tiny_stream_in *in = (struct tiny_stream_in *)stream;

    if (in->pcm)
	pcm_close(in->pcm);
    free(in);
    return;
}

static int adev_dump(const audio_hw_device_t *device, int fd)
{
    return 0;
}

static int adev_close(hw_device_t *device)
{
    free(device);
    return 0;
}

static uint32_t adev_get_supported_devices(const struct audio_hw_device *dev)
{
    struct tiny_audio_device *adev = (struct tiny_audio_device *)dev;
    uint32_t supported = 0;
    int i;

    for (i = 0; i < adev->num_dev_cfgs; i++)
	supported |= adev->dev_cfgs[i].mask;

    return supported;
}

struct config_parse_state {
    struct tiny_audio_device *adev;
    struct tiny_dev_cfg *dev;
    bool on;

    struct route_setting *path;
    unsigned int path_len;
};

static const struct {
    int mask;
    const char *name;
} dev_names[] = {
    { AUDIO_DEVICE_OUT_SPEAKER, "speaker" },
    { AUDIO_DEVICE_OUT_WIRED_HEADSET | AUDIO_DEVICE_OUT_WIRED_HEADPHONE,
      "headphone" },
    { AUDIO_DEVICE_OUT_EARPIECE, "earpiece" },
    { AUDIO_DEVICE_OUT_ANLG_DOCK_HEADSET, "analog-dock" },
    { AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET, "digital-dock" },

    { AUDIO_DEVICE_IN_COMMUNICATION, "comms" },
    { AUDIO_DEVICE_IN_AMBIENT, "ambient" },
    { AUDIO_DEVICE_IN_BUILTIN_MIC, "builtin-mic" },
    { AUDIO_DEVICE_IN_WIRED_HEADSET, "headset" },
    { AUDIO_DEVICE_IN_AUX_DIGITAL, "digital" },
    { AUDIO_DEVICE_IN_BACK_MIC, "back-mic" },
};

static void adev_config_start(void *data, const XML_Char *elem,
			      const XML_Char **attr)
{
    struct config_parse_state *s = data;
    struct tiny_dev_cfg *dev_cfg;
    const XML_Char *name = NULL;
    const XML_Char *val = NULL;
    unsigned int i, j;

    for (i = 0; attr[i]; i += 2) {
	if (strcmp(attr[i], "name") == 0)
	    name = attr[i + 1];

	if (strcmp(attr[i], "val") == 0)
	    val = attr[i + 1];
    }

    if (strcmp(elem, "device") == 0) {
	if (!name) {
	    LOGE("Unnamed device\n");
	    return;
	}

	for (i = 0; i < sizeof(dev_names) / sizeof(dev_names[0]); i++) {
	    if (strcmp(dev_names[i].name, name) == 0) {
		LOGI("Allocating device %s\n", name);
		dev_cfg = realloc(s->adev->dev_cfgs,
				  (s->adev->num_dev_cfgs + 1)
				  * sizeof(*dev_cfg));
		if (!dev_cfg) {
		    LOGE("Unable to allocate dev_cfg\n");
		    return;
		}

		s->dev = &dev_cfg[s->adev->num_dev_cfgs];
		memset(s->dev, 0, sizeof(*s->dev));
		s->dev->mask = dev_names[i].mask;

		s->adev->dev_cfgs = dev_cfg;
		s->adev->num_dev_cfgs++;
	    }
	}

    } else if (strcmp(elem, "path") == 0) {
	if (s->path_len)
	    LOGW("Nested paths\n");

	/* If this a path for a device it must have a role */
	if (s->dev) {
	    /* Need to refactor a bit... */
	    if (strcmp(name, "on") == 0) {
		s->on = true;
	    } else if (strcmp(name, "off") == 0) {
		s->on = false;
	    } else {
		LOGW("Unknown path name %s\n", name);
	    }
	}

    } else if (strcmp(elem, "ctl") == 0) {
	struct route_setting *r;

	if (!name) {
	    LOGE("Unnamed control\n");
	    return;
	}

	if (!val) {
	    LOGE("No value specified for %s\n", name);
	    return;
	}

	LOGV("Parsing control %s => %s\n", name, val);

	r = realloc(s->path, sizeof(*r) * (s->path_len + 1));
	if (!r) {
	    LOGE("Out of memory handling %s => %s\n", name, val);
	    return;
	}

	r[s->path_len].ctl_name = strdup(name);
	r[s->path_len].strval = NULL;

	/* This can be fooled but it'll do */
	r[s->path_len].intval = atoi(val);
	if (!r[s->path_len].intval && strcmp(val, "0") != 0)
	    r[s->path_len].strval = strdup(val);

	s->path = r;
	s->path_len++;
    }
}

static void adev_config_end(void *data, const XML_Char *name)
{
    struct config_parse_state *s = data;
    unsigned int i;

    if (strcmp(name, "path") == 0) {
	if (!s->path_len)
	    LOGW("Empty path\n");

	if (!s->dev) {
	    LOGV("Applying %d element default route\n", s->path_len);

	    set_route_by_array(s->adev->mixer, s->path, s->path_len);

	    for (i = 0; i < s->path_len; i++) {
		free(s->path[i].ctl_name);
		free(s->path[i].strval);
	    }

	    free(s->path);

	    /* Refactor! */
	} else if (s->on) {
	    LOGV("%d element on sequence\n", s->path_len);
	    s->dev->on = s->path;
	    s->dev->on_len = s->path_len;

	} else {
	    LOGV("%d element off sequence\n", s->path_len);

	    /* Apply it, we'll reenable anything that's wanted later */
	    set_route_by_array(s->adev->mixer, s->path, s->path_len);

	    s->dev->off = s->path;
	    s->dev->off_len = s->path_len;
	}

	s->path_len = 0;
	s->path = NULL;

    } else if (strcmp(name, "device") == 0) {
	s->dev = NULL;
    }
}

static int adev_config_parse(struct tiny_audio_device *adev)
{
    struct config_parse_state s;
    FILE *f;
    XML_Parser p;
    char property[PROPERTY_VALUE_MAX];
    char file[80];
    int ret = 0;
    bool eof = false;
    int len;

    property_get("ro.product.device", property, "tiny_hw");
    snprintf(file, sizeof(file), "/system/etc/sound/%s", property);

    LOGV("Reading configuration from %s\n", file);
    f = fopen(file, "r");
    if (!f) {
	LOGE("Failed to open %s\n", file);
	return -ENODEV;
    }

    p = XML_ParserCreate(NULL);
    if (!p) {
	LOGE("Failed to create XML parser\n");
	ret = -ENOMEM;
	goto out;
    }

    memset(&s, 0, sizeof(s));
    s.adev = adev;
    XML_SetUserData(p, &s);

    XML_SetElementHandler(p, adev_config_start, adev_config_end);

    while (!eof) {
	len = fread(file, 1, sizeof(file), f);
	if (ferror(f)) {
	    LOGE("I/O error reading config\n");
	    ret = -EIO;
	    goto out_parser;
	}
	eof = feof(f);

	if (XML_Parse(p, file, len, eof) == XML_STATUS_ERROR) {
	    LOGE("Parse error at line %u:\n%s\n",
		 (unsigned int)XML_GetCurrentLineNumber(p),
		 XML_ErrorString(XML_GetErrorCode(p)));
	    ret = -EINVAL;
	    goto out_parser;
	}
    }

 out_parser:
    XML_ParserFree(p);
 out:
    fclose(f);

    return ret;
}

static int adev_open(const hw_module_t* module, const char* name,
                     hw_device_t** device)
{
    struct tiny_audio_device *adev;
    int ret;

    if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0)
        return -EINVAL;

    adev = calloc(1, sizeof(struct tiny_audio_device));
    if (!adev)
        return -ENOMEM;

    adev->device.common.tag = HARDWARE_DEVICE_TAG;
    adev->device.common.version = 0;
    adev->device.common.module = (struct hw_module_t *) module;
    adev->device.common.close = adev_close;

    adev->device.get_supported_devices = adev_get_supported_devices;
    adev->device.init_check = adev_init_check;
    adev->device.set_voice_volume = adev_set_voice_volume;
    adev->device.set_master_volume = adev_set_master_volume;
    adev->device.set_mode = adev_set_mode;
    adev->device.set_mic_mute = adev_set_mic_mute;
    adev->device.get_mic_mute = adev_get_mic_mute;
    adev->device.set_parameters = adev_set_parameters;
    adev->device.get_parameters = adev_get_parameters;
    adev->device.get_input_buffer_size = adev_get_input_buffer_size;
    adev->device.open_output_stream = adev_open_output_stream;
    adev->device.close_output_stream = adev_close_output_stream;
    adev->device.open_input_stream = adev_open_input_stream;
    adev->device.close_input_stream = adev_close_input_stream;
    adev->device.dump = adev_dump;

    adev->mixer = mixer_open(0);
    if (!adev->mixer) {
	LOGE("Failed to open mixer 0\n");
	goto err;
    }
    
    ret = adev_config_parse(adev);
    if (ret != 0)
	goto err_mixer;

    /* Bootstrap routing */
    pthread_mutex_init(&adev->route_lock, NULL);
    adev->mode = AUDIO_MODE_NORMAL;
    adev->devices = AUDIO_DEVICE_OUT_SPEAKER | AUDIO_DEVICE_IN_BUILTIN_MIC;
    select_devices(adev);

    *device = &adev->device.common;

    return 0;

err_mixer:
    mixer_close(adev->mixer);
err:
    return -EINVAL;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = adev_open,
};

struct audio_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .version_major = 1,
        .version_minor = 0,
        .id = AUDIO_HARDWARE_MODULE_ID,
        .name = "TinyHAL",
        .author = "Mark Brown <broonie@opensource.wolfsonmicro.com>",
        .methods = &hal_module_methods,
    },
};
