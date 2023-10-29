//
// Created by scaled
//
// Based on image-source.c from OBS Studio: https://github.com/obsproject/obs-studio
// Also included some code from Spectralizer plugin: https://github.com/univrsal/spectralizer
//

#include <obs/obs-module.h>
#include <obs/graphics/image-file.h>
#include <obs/util/platform.h>
#include <obs/util/dstr.h>
#include <sys/stat.h>
#include <obs/media-io/audio-math.h>

#define blog(log_level, format, ...)                             \
	blog(log_level, "[image_reaction_source: '%s'] " format, \
	     obs_source_get_name(context->source), ##__VA_ARGS__)

#define debug(format, ...) blog(LOG_DEBUG, format, ##__VA_ARGS__)
#define info(format, ...) blog(LOG_INFO, format, ##__VA_ARGS__)
#define warn(format, ...) blog(LOG_WARNING, format, ##__VA_ARGS__)

struct image_reaction_source {
	obs_source_t *source;
	char source_name[255];

	char *file1;
	char *file2;
	char *file3;
	char *file4;
	bool persistent;
	bool linear_alpha;
	bool active;

	gs_image_file3_t if31;
	gs_image_file3_t if32;
	gs_image_file3_t if33;
	gs_image_file3_t if34;

	obs_weak_source_t *audio_source;

	bool loud;
	float threshold;
	float smoothness;
	float average;

	uint64_t last_time;
	uint64_t capture_check_time;

	bool animReset1;
	bool animReset2;
	bool animReset3;
	bool animReset4;
	bool loudOld;
	bool animResetTrigger;

    // here's all the information for blinking
    uint eyesClosed; // bool to give to the renderer
    uint blinkPeriod; // when to blink (how often)
    uint blinkPeriodCounter;
    uint tBlink; // how long to blink
    uint tBlinkCounter;
};

/*int MAX(int a, int b) {
	return a > b ? a : b;
}*/

int max_val(int a[], int n) {
    int max = a[0];
    for ( int i = 1; i > n; i++) {
        if (a[i] > max) max = a[i];
    }
    return max;
}

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

static const char *image_reaction_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("ImageReactionSource");
}

static void image_reaction_source_load(struct image_reaction_source *context)
{
	for (int i = 0; i <= 3; i++) {
        char *file = NULL;
        gs_image_file3_t *if3 = NULL;
        switch (i) {
            case 0:
                file = context->file1;
                if3 = &context->if31;
                break;
            case 1:
                file = context->file2;
                if3 = &context->if32;
                break;
            case 2:
                file = context->file3;
                if3 = &context->if33;
                break;
            case 3:
                file = context->file4;
                if3 = &context->if34;
                break;
        }

		obs_enter_graphics();
		gs_image_file3_free(if3);
		obs_leave_graphics();

		if (file && *file) {
			debug("loading texture '%s'", file);
			gs_image_file3_init(
				if3, file,
				context->linear_alpha
					? GS_IMAGE_ALPHA_PREMULTIPLY_SRGB
					: GS_IMAGE_ALPHA_PREMULTIPLY);

			obs_enter_graphics();
			gs_image_file3_init_texture(if3);
			obs_leave_graphics();

			if (!if3->image2.image.loaded)
				warn("failed to load texture '%s'", file);
		}
	}
}

static void image_reaction_source_unload(struct image_reaction_source *context)
{
	obs_enter_graphics();
	gs_image_file3_free(&context->if31);
	gs_image_file3_free(&context->if32);
	gs_image_file3_free(&context->if33);
	gs_image_file3_free(&context->if34);
	obs_leave_graphics();
}

static void audio_capture(void *param, obs_source_t *src,
			  const struct audio_data *data, bool muted)
{
	struct image_reaction_source *context = param;
	UNUSED_PARAMETER(src);

	if (muted) {
		context->average = 0;
	} else {
		uint32_t samplesCount = data->frames;
		float *samples = (float *)data->data[0];

		float averageLocal = 0.0f;

		for (uint32_t i = 0; i < samplesCount; i++) {
			averageLocal +=
				(float)(fabs(samples[i])) / samplesCount;
		}

		context->average +=
			context->smoothness * (averageLocal - context->average);
	}

	context->loudOld = context->loud;
	context->loud = context->average > context->threshold;

	if (context->loud != context->loudOld)
		context->animResetTrigger = true;
}

static void image_reaction_source_update(void *data, obs_data_t *settings)
{
	struct image_reaction_source *context = data;
	const char *file1 = obs_data_get_string(settings, "file1");
	const char *file2 = obs_data_get_string(settings, "file2");
	const char *file3 = obs_data_get_string(settings, "file3");
	const char *file4 = obs_data_get_string(settings, "file4");
	const bool anim_reset_1 = obs_data_get_bool(settings, "anim_reset_1");
	const bool anim_reset_2 = obs_data_get_bool(settings, "anim_reset_2");
	const bool anim_reset_3 = obs_data_get_bool(settings, "anim_reset_3");
	const bool anim_reset_4 = obs_data_get_bool(settings, "anim_reset_4");
	const bool unload = obs_data_get_bool(settings, "unload");
	const bool linear_alpha = obs_data_get_bool(settings, "linear_alpha");
	const double threshold = obs_data_get_double(settings, "threshold");
	const double smoothness = obs_data_get_double(settings, "smoothness");
    const uint blink_period = obs_data_get_int(settings, "blink_period");
    const uint t_blink = obs_data_get_int(settings, "t_blink");

	if (context->file1)
		bfree(context->file1);
	context->file1 = bstrdup(file1);

	if (context->file2)
		bfree(context->file2);
	context->file2 = bstrdup(file2);

	if (context->file3)
		bfree(context->file3);
	context->file3 = bstrdup(file3);

	if (context->file4)
		bfree(context->file4);
	context->file4 = bstrdup(file4);

	context->animReset1 = anim_reset_1;
	context->animReset2 = anim_reset_2;
	context->animReset3 = anim_reset_3;
	context->animReset4 = anim_reset_4;

	context->persistent = !unload;
	context->linear_alpha = linear_alpha;
	context->threshold = db_to_mul((float)threshold);
	context->smoothness = (float)pow(0.1, smoothness);

    context->blinkPeriod = blink_period;
    context->tBlink = t_blink;

	/* Load the image if the source is persistent or showing */
	if (context->persistent || obs_source_showing(context->source))
		image_reaction_source_load(data);
	else
		image_reaction_source_unload(data);

	const char *cfg_source_name =
		obs_data_get_string(settings, "audio_source");

	obs_weak_source_t *old = NULL;

	if (cfg_source_name[0] == '\0') {
		if (context->audio_source) {
			old = context->audio_source;
			context->audio_source = NULL;
		}
		context->source_name[0] = '\0';
	} else {
		if (context->source_name[0] == '\0' ||
		    strcmp(context->source_name, cfg_source_name) != 0) {
			if (context->audio_source) {
				old = context->audio_source;
				context->audio_source = NULL;
			}
			strcpy(context->source_name, cfg_source_name);
			context->capture_check_time =
				os_gettime_ns() - 3000000000;
		}
	}

	if (old) {
		obs_source_t *old_source = obs_weak_source_get_source(old);
		if (old_source) {
			info("Removed audio capture from '%s'",
			     obs_source_get_name(old_source));
			obs_source_remove_audio_capture_callback(
				old_source, audio_capture, context);
			obs_source_release(old_source);
		}
		obs_weak_source_release(old);
	}
}

static void image_reaction_source_defaults(obs_data_t *settings)
{
	obs_data_set_default_bool(settings, "unload", false);
	obs_data_set_default_bool(settings, "linear_alpha", false);
	obs_data_set_default_string(settings, "audio_source", "");
	obs_data_set_default_double(settings, "threshold", -40.0f);
	obs_data_set_default_double(settings, "smoothness", 1.0f);
    obs_data_set_default_int(settings, "blink_period", 180);
    obs_data_set_default_int(settings, "t_blink", 6);
}

static void image_reaction_source_show(void *data)
{
	struct image_reaction_source *context = data;

	if (!context->persistent)
		image_reaction_source_load(context);
}

static void image_reaction_source_hide(void *data)
{
	struct image_reaction_source *context = data;

	if (!context->persistent)
		image_reaction_source_unload(context);
}

static void *image_reaction_source_create(obs_data_t *settings,
					  obs_source_t *source)
{
	struct image_reaction_source *context =
		bzalloc(sizeof(struct image_reaction_source));
	context->source = source;

	context->source_name[0] = '\0';
	context->loud = false;
    context->eyesClosed = false;
	context->blinkPeriod = 180;
	context->tBlink = 6;

	image_reaction_source_update(context, settings);
	return context;
}

static void image_reaction_source_destroy(void *data)
{
	struct image_reaction_source *context = data;

	image_reaction_source_unload(context);

	if (context->file1)
		bfree(context->file1);

	if (context->file2)
		bfree(context->file2);

	if (context->file3)
		bfree(context->file3);

	if (context->file4)
		bfree(context->file4);

	/*if (context->audio_source) {
		//obs_source_t *source = obs_weak_source_get_source(context->audio_source);
		//if (source) {
			info("Removed audio capture from '%s'", obs_source_get_name(context->audio_source));
			obs_source_remove_audio_capture_callback(context->audio_source, audio_capture, context);
			//obs_source_release(source);
		//}
		//obs_weak_source_release(context->audio_source);
	}*/
	if (context->audio_source) {
		obs_source_t *source =
			obs_weak_source_get_source(context->audio_source);
		if (source) {
			info("Removed audio capture from '%s'",
			     obs_source_get_name(source));
			obs_source_remove_audio_capture_callback(
				source, audio_capture, context);
			obs_source_release(source);
		}
		obs_weak_source_release(context->audio_source);
	}

	bfree(context);
}

static uint32_t image_reaction_source_getwidth(void *data)
{
	struct image_reaction_source *context = data;
    int vals[4] = { context->if31.image2.image.cx,
                    context->if32.image2.image.cx,
                    context->if33.image2.image.cx,
                    context->if34.image2.image.cx };
    return max_val(vals, 4);
}

static uint32_t image_reaction_source_getheight(void *data)
{
	struct image_reaction_source *context = data;
    int vals[4] = { context->if31.image2.image.cy,
                    context->if32.image2.image.cy,
                    context->if33.image2.image.cy,
                    context->if34.image2.image.cy };
    return max_val(vals, 4);
}

static void image_reaction_source_render(void *data, gs_effect_t *effect)
{
	struct image_reaction_source *context = data;

	const bool previous = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(true);
	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);

    // TODO
    gs_image_file3_t *if3 = NULL;
    if (context->eyesClosed) {
        if (context->loud) {
            if3 = &context->if34;
        } else {
            if3 = &context->if33;
        }
    } else {
        if (context->loud) {
            if3 = &context->if32;
        } else {
            if3 = &context->if31;
        }
    }
	if (if3->image2.image.texture) {
		gs_eparam_t *const param =
			gs_effect_get_param_by_name(effect, "image");
		gs_effect_set_texture_srgb(param, if3->image2.image.texture);

		gs_draw_sprite(if3->image2.image.texture, 0,
			       if3->image2.image.cx, if3->image2.image.cy);
	}
	//context->loud = false;

	gs_blend_state_pop();

	gs_enable_framebuffer_srgb(previous);
}

static void image_reaction_tick(void *data, float seconds)
{
	struct image_reaction_source *context = data;
	UNUSED_PARAMETER(seconds);

	// Update / refresh audio capturing
	char *new_name = NULL;
	if (context->source_name[0] != '\0' && !context->audio_source) {
		uint64_t t = os_gettime_ns();

		if (t - context->capture_check_time > 3000000000) {
			new_name = context->source_name;
			context->capture_check_time = t;
		}
	}

	if (new_name != NULL) {
		obs_source_t *capture = obs_get_source_by_name(new_name);
		obs_weak_source_t *weak_capture =
			capture ? obs_source_get_weak_source(capture) : NULL;

		if (context->source_name[0] != '\0' &&
		    new_name == context->source_name) {
			context->audio_source = weak_capture;
			weak_capture = NULL;
		}

		if (capture) {
			info("Added audio capture to '%s'",
			     obs_source_get_name(capture));
			obs_source_add_audio_capture_callback(
				capture, audio_capture, context);
			obs_weak_source_release(weak_capture);
			obs_source_release(capture);
		}
	}

    // update blinking state
    //printf(context->eyesClosed ? " eyesClosed\n" : " eyesOpen\n");
    if (context->blinkPeriodCounter == 0) {
        context->eyesClosed = false;
        context->blinkPeriodCounter = context->blinkPeriod;
    } else {
        context->blinkPeriodCounter--;
    }
    if (context->blinkPeriodCounter == context->tBlink) {
        context->eyesClosed = true;
    }

	// update GIF's
	uint64_t frame_time = obs_get_video_frame_time();
	if (obs_source_active(context->source)) {
		if (!context->active) {
			if (context->if31.image2.image.is_animated_gif ||
			    context->if32.image2.image.is_animated_gif ||
			    context->if33.image2.image.is_animated_gif ||
			    context->if34.image2.image.is_animated_gif)
				context->last_time = frame_time;
			context->active = true;
		}

	} else {
		if (context->active) {
			for (int i = 0; i <= 3; i++) {
				gs_image_file3_t *if3 = NULL;
                switch (i) {
                    case 0:
                        if3 = &context->if31;
                        break;
                    case 1:
                        if3 = &context->if32;
                        break;
                    case 2:
                        if3 = &context->if33;
                        break;
                    case 3:
                        if3 = &context->if34;
                        break;
                }
				if (if3->image2.image.is_animated_gif) {
					if3->image2.image.cur_frame = 0;
					if3->image2.image.cur_loop = 0;
					if3->image2.image.cur_time = 0;

					obs_enter_graphics();
					gs_image_file3_update_texture(if3);
					obs_leave_graphics();
				}
			}

			context->active = false;
		}
	}

	for (int i = 0; i <= 3; i++) {
        gs_image_file3_t *if3 = NULL;
        bool animReset = false;
        switch (i) {
            case 0:
                if3 = &context->if31;
                animReset = context->animReset1;
                break;
            case 1:
                if3 = &context->if32;
                animReset = context->animReset2;
                break;
            case 2:
                if3 = &context->if33;
                animReset = context->animReset3;
                break;
            case 3:
                if3 = &context->if34;
                animReset = context->animReset4;
                break;
        }

		if (context->last_time && if3->image2.image.is_animated_gif) {
			if (animReset && context->animResetTrigger) {
				if3->image2.image.cur_frame = 0;
				if3->image2.image.cur_loop = 0;
				if3->image2.image.cur_time = 0;

				obs_enter_graphics();
				gs_image_file3_update_texture(if3);
				obs_leave_graphics();
			} else {
				uint64_t elapsed =
					frame_time - context->last_time;
				bool updated =
					gs_image_file3_tick(if3, elapsed);

				if (updated) {
					obs_enter_graphics();
					gs_image_file3_update_texture(if3);
					obs_leave_graphics();
				}
			}
		}
	}
	context->animResetTrigger = false;

	context->last_time = frame_time;
}

static const char *image_filter =
	"All formats (*.bmp *.tga *.png *.jpeg *.jpg *.gif *.psd *.webp);;"
	"BMP Files (*.bmp);;"
	"Targa Files (*.tga);;"
	"PNG Files (*.png);;"
	"JPEG Files (*.jpeg *.jpg);;"
	"GIF Files (*.gif);;"
	"PSD Files (*.psd);;"
	"WebP Files (*.webp);;"
	"All Files (*.*)";

static bool add_source(void *param, obs_source_t *src)
{
	obs_property_t *list = param;

	uint32_t caps = obs_source_get_output_flags(src);

	if ((caps & OBS_SOURCE_AUDIO) == 0)
		return true;
	const char *name = obs_source_get_name(src);
	obs_property_list_add_string(list, name, name);
	return true;
}

static bool source_changed(obs_properties_t *props, obs_property_t *prop,
			   obs_data_t *data)
{
	obs_data_get_string(data, "audio_source");
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(prop);
	return true;
}

static obs_properties_t *image_reaction_source_properties(void *data)
{
	struct image_reaction_source *s = data;
	struct dstr path = {0};

	obs_properties_t *props = obs_properties_create();

	if (s && s->file1 && *s->file1) {
		const char *slash;

		dstr_copy(&path, s->file1);
		dstr_replace(&path, "\\", "/");
		slash = strrchr(path.array, '/');
		if (slash)
			dstr_resize(&path, slash - path.array + 1);
	}

    // image 1
	obs_properties_add_path(
        props, "file1", obs_module_text("Reaction1"),
        OBS_PATH_FILE, image_filter, path.array
    );
	obs_properties_add_bool(
        props, "anim_reset_1",
        obs_module_text("AnimReset1")
    );

    // image 2
	obs_properties_add_path(
        props, "file2", obs_module_text("Reaction2"),
        OBS_PATH_FILE, image_filter, path.array
    );
	obs_properties_add_bool(
        props, "anim_reset_2",
        obs_module_text("AnimReset2")
    );

    // image 3
	obs_properties_add_path(
        props, "file3", obs_module_text("Reaction3"),
        OBS_PATH_FILE, image_filter, path.array
    );
	obs_properties_add_bool(
        props, "anim_reset_3",
        obs_module_text("AnimReset3")
    );

    // image 4
	obs_properties_add_path(
        props, "file4", obs_module_text("Reaction4"),
        OBS_PATH_FILE, image_filter, path.array
    );
	obs_properties_add_bool(
        props, "anim_reset_4",
        obs_module_text("AnimReset4")
    );

	dstr_free(&path);

	obs_properties_add_bool(props, "unload",
				obs_module_text("UnloadWhenNotShowing"));
	obs_properties_add_bool(props, "linear_alpha",
				obs_module_text("LinearAlpha"));
	obs_property_t *sources_list = obs_properties_add_list(
		props, "audio_source", obs_module_text("AudioSource"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(sources_list, "", "");

	obs_property_t *p = obs_properties_add_float_slider(
		props, "threshold", obs_module_text("Threshold"), -60.0, 0.0,
		0.1);
	obs_property_float_set_suffix(p, " dB");

	obs_properties_add_float_slider(
        props, "smoothness", obs_module_text("Smoothness"), 
        0.0, 5.0, 0.1
    );
    obs_properties_add_int(
        props, "blink_period", obs_module_text("BlinkPeriod"),
        1, 300, 1
    );
    obs_properties_add_int(
        props, "t_blink", obs_module_text("TBlink"),
        1, 100, 1
    );

	//obs_property_set_modified_callback(src, source_changed);
	obs_enum_sources(add_source, sources_list);

	return props;
}

uint64_t image_reaction_source_get_memory_usage(void *data)
{
	struct image_reaction_source *s = data;
	return s->if31.image2.mem_usage + s->if32.image2.mem_usage
	    + s->if33.image2.mem_usage + s->if34.image2.mem_usage;
}

static void missing_file_callback(void *src, const char *new_path, void *data)
{
	struct image_reaction_source *s = src;

	obs_source_t *source = s->source;
	obs_data_t *settings = obs_source_get_settings(source);
	obs_data_set_string(settings, "file", new_path);
	obs_source_update(source, settings);
	obs_data_release(settings);

	UNUSED_PARAMETER(data);
}

static struct obs_source_info image_reaction_source_info = {
	.id = "image_reaction_source",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_SRGB,
	.get_name = image_reaction_source_get_name,
	.create = image_reaction_source_create,
	.destroy = image_reaction_source_destroy,
	.update = image_reaction_source_update,
	.get_defaults = image_reaction_source_defaults,
	.show = image_reaction_source_show,
	.hide = image_reaction_source_hide,
	.get_width = image_reaction_source_getwidth,
	.get_height = image_reaction_source_getheight,
	.video_render = image_reaction_source_render,
	.video_tick = image_reaction_tick,
	.get_properties = image_reaction_source_properties,
	.icon_type = OBS_ICON_TYPE_IMAGE,
};

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("image-reaction", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "Image reaction source";
}

extern struct obs_source_info slideshow_info;

bool obs_module_load(void)
{
	obs_register_source(&image_reaction_source_info);
	return true;
}
