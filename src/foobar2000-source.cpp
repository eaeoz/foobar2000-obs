#define NOMINMAX
#include <windows.h>
#include <gdiplus.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <shlwapi.h>
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")

#include <obs-module.h>
#include <util/platform.h>
#include <graphics/graphics.h>
#include <plugin-support.h>
#include "foobar2000-source.h"

static ULONG_PTR gdip_token;
static bool gdip_initialized = false;

bool foobar2000_module_init(void)
{
	Gdiplus::GdiplusStartupInput gdip_input;
	Gdiplus::Status status =
		Gdiplus::GdiplusStartup(&gdip_token, &gdip_input, NULL);
	if (status == Gdiplus::Ok) {
		gdip_initialized = true;
	}
	return gdip_initialized;
}

void foobar2000_module_unload(void)
{
	if (gdip_initialized) {
		Gdiplus::GdiplusShutdown(gdip_token);
		gdip_initialized = false;
	}
}

#define POLL_INTERVAL_NS 1000000000ULL
#define COMPOSITE_WIDTH 750
#define COMPOSITE_HEIGHT 400
#define ART_SIZE 250
#define ART_PADDING 35
#define TEXT_X (ART_PADDING + ART_SIZE + 20)
#define LABEL_FONT_SIZE 11
#define TITLE_FONT_SIZE 30
#define ARTIST_FONT_SIZE 36

#define BRIDGE_DIR_NAME "foobar2000-obs"
#define BRIDGE_FILE_NAME "bridge.txt"

struct foobar2000_data {
	obs_source_t *source;

	gs_texture_t *composite_texture;
	uint32_t width;
	uint32_t height;
	bool texture_dirty;

	char artist[512];
	char title[512];

	Gdiplus::Bitmap *pending_bitmap;

	uint64_t last_poll_ns;

	wchar_t last_bridge_path[MAX_PATH];
	bool last_bridge_path_valid;
};

static Gdiplus::Font *create_gdip_font(const wchar_t *family, float size,
					int style)
{
	Gdiplus::FontFamily font_family(family);
	if (!font_family.IsAvailable()) {
		return new Gdiplus::Font(L"Arial", size, style,
					 Gdiplus::UnitPixel);
	}
	return new Gdiplus::Font(&font_family, size, style,
				 Gdiplus::UnitPixel);
}

static void render_text_to_bitmap(Gdiplus::Bitmap *bitmap,
				  Gdiplus::Graphics *graphics,
				  const char *artist, const char *title)
{
	if ((!artist || artist[0] == '\0') &&
	    (!title || title[0] == '\0')) {
		return;
	}

	Gdiplus::SolidBrush white_brush(Gdiplus::Color(255, 255, 255, 255));
	Gdiplus::SolidBrush gray_brush(Gdiplus::Color(255, 180, 180, 185));

	wchar_t wartist[512];
	wchar_t wtitle[512];
	MultiByteToWideChar(CP_UTF8, 0, artist, -1, wartist, 512);
	MultiByteToWideChar(CP_UTF8, 0, title, -1, wtitle, 512);

	Gdiplus::StringFormat format =
		Gdiplus::StringFormat::GenericTypographic();
	format.SetFormatFlags(format.GetFormatFlags() &
			       ~Gdiplus::StringFormatFlagsLineLimit);
	format.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);

	Gdiplus::StringFormat nowplaying_format =
		Gdiplus::StringFormat::GenericTypographic();
	nowplaying_format.SetFormatFlags(
		nowplaying_format.GetFormatFlags() &
		~Gdiplus::StringFormatFlagsLineLimit);

	Gdiplus::Font *label_font =
		create_gdip_font(L"Segoe UI", LABEL_FONT_SIZE,
				 Gdiplus::FontStyleBold);
	Gdiplus::Font *title_font =
		create_gdip_font(L"Segoe UI", TITLE_FONT_SIZE,
				 Gdiplus::FontStyleBold);
	Gdiplus::Font *artist_font =
		create_gdip_font(L"Segoe UI", ARTIST_FONT_SIZE,
				 Gdiplus::FontStyleBold);

	const Gdiplus::REAL text_width =
		(Gdiplus::REAL)(COMPOSITE_WIDTH - TEXT_X - 20);
	const Gdiplus::REAL gap = 24.0f;
	const Gdiplus::REAL measure_height = (Gdiplus::REAL)COMPOSITE_HEIGHT;

	Gdiplus::RectF measure_rect((Gdiplus::REAL)TEXT_X, 0, text_width,
				    measure_height);

	Gdiplus::RectF label_bounds;
	graphics->MeasureString(L"NOW PLAYING", -1, label_font, measure_rect,
				&nowplaying_format, &label_bounds);

	Gdiplus::RectF artist_bounds;
	if (wartist[0] != L'\0' && wcslen(wartist) > 0) {
		graphics->MeasureString(wartist, -1, artist_font, measure_rect,
					&format, &artist_bounds);
	}

	Gdiplus::RectF title_bounds;
	if (wtitle[0] != L'\0' && wcslen(wtitle) > 0) {
		graphics->MeasureString(wtitle, -1, title_font, measure_rect,
					&format, &title_bounds);
	}

	Gdiplus::REAL total_height = label_bounds.Height;
	if (wartist[0] != L'\0' && wcslen(wartist) > 0)
		total_height += gap + artist_bounds.Height;
	if (wtitle[0] != L'\0' && wcslen(wtitle) > 0)
		total_height += gap + title_bounds.Height;
	Gdiplus::REAL current_y =
		((Gdiplus::REAL)COMPOSITE_HEIGHT - total_height) / 2.0f;

	{
		Gdiplus::SolidBrush label_brush(
			Gdiplus::Color(120, 120, 120, 130));
		graphics->DrawString(
			L"NOW PLAYING", -1, label_font,
			Gdiplus::RectF((Gdiplus::REAL)TEXT_X, current_y,
				       (Gdiplus::REAL)200, label_bounds.Height),
			&nowplaying_format, &label_brush);
	}

	current_y += label_bounds.Height + gap;

	if (wartist[0] != L'\0' && wcslen(wartist) > 0) {
		graphics->DrawString(
			wartist, -1, artist_font,
			Gdiplus::RectF((Gdiplus::REAL)TEXT_X, current_y,
				       text_width, artist_bounds.Height),
			&format, &white_brush);
		current_y += artist_bounds.Height + gap;
	}

	if (wtitle[0] != L'\0' && wcslen(wtitle) > 0) {
		graphics->DrawString(
			wtitle, -1, title_font,
			Gdiplus::RectF((Gdiplus::REAL)TEXT_X, current_y,
				       text_width, title_bounds.Height),
			&format, &gray_brush);
	}

	delete label_font;
	delete title_font;
	delete artist_font;
}

static Gdiplus::Bitmap *extract_album_art_from_file(const wchar_t *file_path)
{
	IShellItemImageFactory *factory = NULL;
	HRESULT hr = SHCreateItemFromParsingName(file_path, NULL,
						  IID_PPV_ARGS(&factory));
	if (FAILED(hr) || !factory)
		return NULL;

	SIZE size = {ART_SIZE, ART_SIZE};
	HBITMAP hbitmap = NULL;
	hr = factory->GetImage(size, SIIGBF_THUMBNAILONLY, &hbitmap);
	factory->Release();

	if (FAILED(hr) || !hbitmap)
		return NULL;

	Gdiplus::Bitmap *bitmap =
		Gdiplus::Bitmap::FromHBITMAP(hbitmap, NULL);
	DeleteObject(hbitmap);
	return bitmap;
}

static void draw_album_art(Gdiplus::Graphics &graphics, Gdiplus::Bitmap *art)
{
	obs_log(LOG_INFO, "[fb2k] album art loaded: %dx%d", art->GetWidth(),
		art->GetHeight());
	int draw_w = art->GetWidth();
	int draw_h = art->GetHeight();
	if (draw_w > draw_h) {
		if (draw_w > ART_SIZE) {
			draw_h = (int)((float)draw_h * ART_SIZE / draw_w);
			draw_w = ART_SIZE;
		}
	} else {
		if (draw_h > ART_SIZE) {
			draw_w = (int)((float)draw_w * ART_SIZE / draw_h);
			draw_h = ART_SIZE;
		}
	}
	int art_x = ART_PADDING + (ART_SIZE - draw_w) / 2;
	int art_y = (COMPOSITE_HEIGHT - draw_h) / 2;

	graphics.DrawImage(art, art_x, art_y, draw_w, draw_h);

	Gdiplus::Pen border_pen(Gdiplus::Color(50, 255, 255, 255), 1.0f);
	graphics.DrawRectangle(&border_pen, art_x, art_y, draw_w, draw_h);
}

static bool read_bridge_file(wchar_t *out_path, size_t out_size)
{
	char *appdata = getenv("LOCALAPPDATA");
	if (!appdata || !appdata[0])
		return false;

	char path[MAX_PATH];
	snprintf(path, sizeof(path), "%s\\%s\\%s", appdata, BRIDGE_DIR_NAME,
		 BRIDGE_FILE_NAME);

	wchar_t wpath[MAX_PATH];
	MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, MAX_PATH);

	HANDLE hFile = CreateFileW(wpath, GENERIC_READ, FILE_SHARE_READ, NULL,
				   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return false;

	char buf[MAX_PATH];
	DWORD bytes_read = 0;
	BOOL ok = ReadFile(hFile, buf, sizeof(buf) - 1, &bytes_read, NULL);
	CloseHandle(hFile);

	if (!ok || bytes_read == 0)
		return false;

	buf[bytes_read] = '\0';

	while (bytes_read > 0 &&
	       (buf[bytes_read - 1] == '\n' || buf[bytes_read - 1] == '\r' ||
		buf[bytes_read - 1] == ' '))
		buf[--bytes_read] = '\0';

	if (bytes_read == 0)
		return false;

	MultiByteToWideChar(CP_UTF8, 0, buf, -1, out_path, (int)out_size);
	return out_path[0] != L'\0';
}

static void update_composite_bitmap(struct foobar2000_data *s)
{
	obs_log(LOG_INFO, "[fb2k] update_composite_bitmap: title='%s' artist='%s'",
		s->title, s->artist);

	if (s->pending_bitmap) {
		delete s->pending_bitmap;
		s->pending_bitmap = NULL;
	}

	auto *bitmap = new Gdiplus::Bitmap(COMPOSITE_WIDTH, COMPOSITE_HEIGHT,
					   PixelFormat32bppARGB);
	Gdiplus::Graphics graphics(bitmap);
	graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
	graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
	graphics.SetInterpolationMode(
		Gdiplus::InterpolationModeHighQualityBicubic);

	graphics.Clear(Gdiplus::Color(0, 0, 0, 0));

	Gdiplus::Bitmap *art = NULL;

	if (s->last_bridge_path_valid) {
		art = extract_album_art_from_file(s->last_bridge_path);
		if (art) {
			obs_log(LOG_INFO, "[fb2k] album art loaded from: %S",
				s->last_bridge_path);
			draw_album_art(graphics, art);
		}
	}

	render_text_to_bitmap(bitmap, &graphics, s->artist, s->title);

	if (art)
		delete art;

	s->pending_bitmap = bitmap;
	s->texture_dirty = true;
	obs_log(LOG_INFO, "[fb2k] bitmap created, queued for texture upload");
}

struct fb2k_find_info {
	HWND hwnd;
};

static BOOL CALLBACK fb2k_enum_windows(HWND hwnd, LPARAM lparam)
{
	auto *info = (struct fb2k_find_info *)lparam;

	wchar_t class_name[256];
	GetClassNameW(hwnd, class_name, 256);
	if (wcsstr(class_name, L"{E7076D1C") ||
	    wcsstr(class_name, L"foobar") ||
	    wcsstr(class_name, L"Default")) {
		info->hwnd = hwnd;
		return FALSE;
	}

	wchar_t title[1024];
	GetWindowTextW(hwnd, title, 1024);
	if (wcsstr(title, L"foobar2000") && !wcsstr(title, L"-obs")) {
		info->hwnd = hwnd;
		return FALSE;
	}

	return TRUE;
}

static void poll_foobar2000(struct foobar2000_data *s)
{
	wchar_t bridge_path[MAX_PATH];
	bool bridge_ok = read_bridge_file(bridge_path, MAX_PATH);

	if (bridge_ok) {
		if (wcscmp(bridge_path, s->last_bridge_path) != 0) {
			wcscpy_s(s->last_bridge_path, MAX_PATH, bridge_path);
			s->last_bridge_path_valid = true;
			obs_log(LOG_INFO, "[fb2k] bridge path: %S",
				bridge_path);
		}
	} else {
		if (s->last_bridge_path_valid) {
			s->last_bridge_path_valid = false;
			s->last_bridge_path[0] = L'\0';
			obs_log(LOG_INFO,
				"[fb2k] bridge file empty/missing, clearing");
		}
	}

	struct fb2k_find_info info = {0};
	EnumWindows(fb2k_enum_windows, (LPARAM)&info);
	HWND hwnd = info.hwnd;

	if (!hwnd) {
		if (s->artist[0] != '\0' || s->title[0] != '\0') {
			s->artist[0] = '\0';
			s->title[0] = '\0';
			update_composite_bitmap(s);
		}
		return;
	}

	wchar_t window_text[1024];
	GetWindowTextW(hwnd, window_text, 1024);

	char text_utf8[1024];
	WideCharToMultiByte(CP_UTF8, 0, window_text, -1, text_utf8, 1024, NULL,
			    NULL);

	char *end = strstr(text_utf8, " [foobar2000]");
	if (!end)
		end = strstr(text_utf8, " [foobar2000");
	if (!end) {
		char *bracket = strrchr(text_utf8, '[');
		if (bracket && bracket > text_utf8) {
			*(bracket - 1) = '\0';
		}
	} else {
		*end = '\0';
	}

	char old_artist[512];
	char old_title[512];
	strncpy_s(old_artist, sizeof(old_artist), s->artist, _TRUNCATE);
	strncpy_s(old_title, sizeof(old_title), s->title, _TRUNCATE);

	s->artist[0] = '\0';
	s->title[0] = '\0';

	if (text_utf8[0]) {
		char *dash = strstr(text_utf8, " - ");
		if (dash) {
			*dash = '\0';
			strncpy_s(s->artist, 512, text_utf8, _TRUNCATE);
			strncpy_s(s->title, 512, dash + 3, _TRUNCATE);
		} else {
			strncpy_s(s->title, 512, text_utf8, _TRUNCATE);
			if (strncmp(s->title, "foobar2000", 10) == 0)
				s->title[0] = '\0';
		}
	}

	bool changed = (strcmp(s->artist, old_artist) != 0 ||
			strcmp(s->title, old_title) != 0);

	if (changed && !bridge_ok) {
		s->last_bridge_path_valid = false;
		s->last_bridge_path[0] = L'\0';
	}

	update_composite_bitmap(s);
}

static const char *source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("Foobar2000NowPlaying");
}

static void *source_create(obs_data_t *settings, obs_source_t *source)
{
	obs_log(LOG_INFO, "[fb2k] source_create called");
	auto *s =
		(struct foobar2000_data *)bzalloc(sizeof(struct foobar2000_data));
	s->source = source;
	s->width = COMPOSITE_WIDTH;
	s->height = COMPOSITE_HEIGHT;
	s->last_poll_ns = 0;
	s->artist[0] = '\0';
	s->title[0] = '\0';
	s->last_bridge_path_valid = false;
	s->last_bridge_path[0] = L'\0';
	return s;
}

static void source_destroy(void *data)
{
	auto *s = (struct foobar2000_data *)data;
	if (s->composite_texture)
		gs_texture_destroy(s->composite_texture);
	if (s->pending_bitmap)
		delete s->pending_bitmap;
	bfree(s);
}

static uint32_t source_get_width(void *data)
{
	auto *s = (struct foobar2000_data *)data;
	return s->width;
}

static uint32_t source_get_height(void *data)
{
	auto *s = (struct foobar2000_data *)data;
	return s->height;
}

static void source_video_tick(void *data, float seconds)
{
	UNUSED_PARAMETER(seconds);
	auto *s = (struct foobar2000_data *)data;

	uint64_t now = os_gettime_ns();
	if (now - s->last_poll_ns >= POLL_INTERVAL_NS) {
		s->last_poll_ns = now;
		poll_foobar2000(s);
	}
}

static void create_texture_from_bitmap(struct foobar2000_data *s)
{
	if (!s->pending_bitmap)
		return;

	if (s->composite_texture) {
		gs_texture_destroy(s->composite_texture);
		s->composite_texture = NULL;
	}

	Gdiplus::Rect bounds(0, 0, COMPOSITE_WIDTH, COMPOSITE_HEIGHT);
	Gdiplus::BitmapData bitmap_data;
	if (s->pending_bitmap->LockBits(&bounds, Gdiplus::ImageLockModeRead,
					PixelFormat32bppARGB,
					&bitmap_data) != Gdiplus::Ok) {
		obs_log(LOG_INFO, "[fb2k] LockBits failed in render");
		delete s->pending_bitmap;
		s->pending_bitmap = NULL;
		return;
	}

	uint8_t *data =
		(uint8_t *)bmalloc(COMPOSITE_WIDTH * COMPOSITE_HEIGHT * 4);
	if (!data) {
		s->pending_bitmap->UnlockBits(&bitmap_data);
		delete s->pending_bitmap;
		s->pending_bitmap = NULL;
		return;
	}

	const uint8_t *src = (const uint8_t *)bitmap_data.Scan0;
	for (int y = 0; y < COMPOSITE_HEIGHT; y++) {
		memcpy(data + y * COMPOSITE_WIDTH * 4,
		       src + y * bitmap_data.Stride, COMPOSITE_WIDTH * 4);
	}
	s->pending_bitmap->UnlockBits(&bitmap_data);
	delete s->pending_bitmap;
	s->pending_bitmap = NULL;

	gs_texture_t *tex = gs_texture_create(COMPOSITE_WIDTH, COMPOSITE_HEIGHT,
					      GS_BGRA, 1,
					      (const uint8_t **)&data, 0);
	bfree(data);

	if (tex) {
		s->composite_texture = tex;
		obs_log(LOG_INFO, "[fb2k] texture created OK (%dx%d)",
			COMPOSITE_WIDTH, COMPOSITE_HEIGHT);
	} else {
		obs_log(LOG_INFO, "[fb2k] texture creation FAILED in render");
	}

	s->texture_dirty = false;
}

static void source_video_render(void *data, gs_effect_t *effect)
{
	auto *s = (struct foobar2000_data *)data;

	if (s->texture_dirty) {
		create_texture_from_bitmap(s);
	}

	if (s->composite_texture) {
		gs_eparam_t *image =
			gs_effect_get_param_by_name(effect, "image");
		if (image)
			gs_effect_set_texture(image, s->composite_texture);
		gs_draw_sprite(s->composite_texture, 0, s->width, s->height);
	}
}

static void source_update(void *data, obs_data_t *settings)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(settings);
}

static obs_properties_t *source_get_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *props = obs_properties_create();
	return props;
}

static void source_defaults(obs_data_t *settings)
{
	UNUSED_PARAMETER(settings);
}

extern "C" struct obs_source_info foobar2000_source_info = {
	.id = "foobar2000_now_playing",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = source_get_name,
	.create = source_create,
	.destroy = source_destroy,
	.get_width = source_get_width,
	.get_height = source_get_height,
	.get_defaults = source_defaults,
	.get_properties = source_get_properties,
	.update = source_update,
	.video_tick = source_video_tick,
	.video_render = source_video_render,
};
