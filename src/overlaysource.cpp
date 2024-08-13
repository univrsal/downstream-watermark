#include "overlaysource.h"
#include <obs/obs.hpp>
#include <string>

extern "C" {
#include <graphics/image-file.h>
}

struct overlay_source {
	obs_source_t *source;

	gs_image_file4_t *m_image = nullptr;

	uint32_t cx, cy;         /* Source size		*/
	uint32_t img_cx, img_cy; /* Scaled image size	*/
	struct vec2 pos;         /* Image position	*/
	double scale;
	std::string file;
};

static const char *overlay_source_get_name(void *)
{
	return obs_module_text("OverlaySource");
}

void unload_image(void *data)
{
	auto context = static_cast<overlay_source *>(data);
	if (!context->m_image)
		return;
	gs_image_file4_free(context->m_image);
	delete context->m_image;
	context->m_image = nullptr;
}

static void overlay_source_update(void *data, obs_data_t *settings)
{
	auto context = static_cast<overlay_source *>(data);

	obs_video_info ovi{};
	obs_get_video_info(&ovi);
	context->cx = ovi.base_width;
	context->cy = ovi.base_height;
	context->scale = obs_data_get_double(settings, "scale");
	context->pos.x = (float)obs_data_get_int(settings, "pos.x");
	context->pos.y = (float)obs_data_get_int(settings, "pos.y");

	/* Since obs_source_getwidth reports old values, we
	 * get the image size here every time a new file is loaded
	 */
	const char *file = obs_data_get_string(settings, "file");

	obs_enter_graphics();
	unload_image(data);
	if (file && file != context->file) {
		context->m_image = new gs_image_file4_t{};
		gs_image_file4_init(context->m_image, file,
				    GS_IMAGE_ALPHA_PREMULTIPLY_SRGB);
		gs_image_file4_init_texture(context->m_image);
		context->img_cx = context->m_image->image3.image2.image.cx;
		context->img_cy = context->m_image->image3.image2.image.cy;

		obs_data_set_int(settings, "imgcx", context->img_cx);
		obs_data_set_int(settings, "imgcy", context->img_cy);
	}
	obs_leave_graphics();
}

static void overlay_source_defaults(obs_data_t *settings)
{
	obs_data_set_default_double(settings, "scale", 1.0);
	obs_data_set_default_int(settings, "pos.x", 0);
	obs_data_set_default_int(settings, "pos.y", 0);
	obs_data_set_default_string(settings, "file", "");
}

static void overlay_source_destroy(void *data)
{
	obs_enter_graphics();
	unload_image(data);
	obs_leave_graphics();
	delete static_cast<overlay_source *>(data);
}

static uint32_t overlay_source_getwidth(void *data)
{
	auto *context = static_cast<overlay_source *>(data);
	return context->cx;
}

static uint32_t overlay_source_getheight(void *data)
{
	auto *context = static_cast<overlay_source *>(data);
	return context->cy;
}

static void overlay_source_render(void *data, gs_effect_t *effect)
{
	auto *context = static_cast<overlay_source *>(data);

	if (!context->m_image || (context->img_cx == 0 && context->img_cy == 0))
		return;

	const bool previous = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(true);

	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);

	gs_matrix_push();
	gs_matrix_translate3f(context->pos.x, context->pos.y, 0);

	gs_eparam_t *const param = gs_effect_get_param_by_name(effect, "image");
	gs_effect_set_texture_srgb(
		param, context->m_image->image3.image2.image.texture);

	gs_draw_sprite(context->m_image->image3.image2.image.texture, 0,
		       (int)(context->img_cx * context->scale),
		       (int)(context->img_cy * context->scale));

	gs_matrix_pop();

	gs_blend_state_pop();

	gs_enable_framebuffer_srgb(previous);
}

static void overlay_source_tick(void *, float) {}

static obs_properties_t *overlay_source_properties(void *)
{
	return NULL;
}

static void *overlay_source_create(obs_data_t *settings, obs_source_t *source)
{
	auto *context = new overlay_source{};
	context->source = source;
	overlay_source_update(context, settings);

	context->pos.x = 0;
	context->pos.y = 0;
	return context;
}

struct obs_source_info osi = {.id = "overlay_source",
			      .type = OBS_SOURCE_TYPE_INPUT,
			      .output_flags = OBS_SOURCE_VIDEO,
			      .get_name = overlay_source_get_name,
			      .create = overlay_source_create,
			      .destroy = overlay_source_destroy,
			      .get_width = overlay_source_getwidth,
			      .get_height = overlay_source_getheight,
			      .get_defaults = overlay_source_defaults,
			      .update = overlay_source_update,
			      .video_render = overlay_source_render};