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
	Gdiplus::Status status = Gdiplus::GdiplusStartup(&gdip_token, &gdip_input, NULL);
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
#define COMPOSITE_HEIGHT 340
#define ART_SIZE 250
#define ART_PADDING 35
#define TEXT_X (ART_PADDING + ART_SIZE + 20)
#define TEXT_START_Y 130
#define TITLE_FONT_SIZE 20
#define ARTIST_FONT_SIZE 26

struct foobar2000_data {
	obs_source_t *source;

	gs_texture_t *composite_texture;
	uint32_t width;
	uint32_t height;
	bool texture_dirty;

	char artist[512];
	char title[512];
	char music_dir[MAX_PATH];

	Gdiplus::Bitmap *pending_bitmap;

	uint64_t last_poll_ns;

	wchar_t last_found_path[MAX_PATH];
	bool last_found_path_valid;
};

static Gdiplus::Font *create_gdip_font(const wchar_t *family, float size, int style)
{
	Gdiplus::FontFamily font_family(family);
	if (!font_family.IsAvailable()) {
		return new Gdiplus::Font(L"Arial", size, style, Gdiplus::UnitPixel);
	}
	return new Gdiplus::Font(&font_family, size, style, Gdiplus::UnitPixel);
}

static void render_text_to_bitmap(Gdiplus::Bitmap *bitmap, Gdiplus::Graphics *graphics,
				  const char *artist, const char *title)
{
	Gdiplus::SolidBrush text_brush(Gdiplus::Color(255, 180, 180, 185));
	Gdiplus::SolidBrush artist_brush(Gdiplus::Color(255, 255, 255, 255));

	wchar_t wtitle[512];
	wchar_t wartist[512];
	MultiByteToWideChar(CP_UTF8, 0, title, -1, wtitle, 512);
	MultiByteToWideChar(CP_UTF8, 0, artist, -1, wartist, 512);

	Gdiplus::StringFormat format;
	format.SetAlignment(Gdiplus::StringAlignmentNear);
	format.SetLineAlignment(Gdiplus::StringAlignmentNear);
	format.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);

	Gdiplus::StringFormat nowplaying_format;
	nowplaying_format.SetAlignment(Gdiplus::StringAlignmentNear);
	nowplaying_format.SetLineAlignment(Gdiplus::StringAlignmentNear);

	Gdiplus::Font *label_font = create_gdip_font(L"Segoe UI", 10,
						     Gdiplus::FontStyleBold);
	Gdiplus::Font *title_font = create_gdip_font(L"Segoe UI", TITLE_FONT_SIZE,
						     Gdiplus::FontStyleBold);
	Gdiplus::Font *artist_font = create_gdip_font(L"Segoe UI", ARTIST_FONT_SIZE,
						      Gdiplus::FontStyleBold);

	int label_y = TEXT_START_Y - 30;
	Gdiplus::SolidBrush label_brush(Gdiplus::Color(120, 120, 120, 130));
	graphics->DrawString(L"NOW PLAYING", -1, label_font,
			     Gdiplus::RectF((Gdiplus::REAL)TEXT_X, (Gdiplus::REAL)label_y,
					    (Gdiplus::REAL)200, (Gdiplus::REAL)16),
			     &nowplaying_format, &label_brush);

	if (wartist[0] != L'\0' && wcslen(wartist) > 0) {
		Gdiplus::RectF artist_rect((Gdiplus::REAL)TEXT_X, (Gdiplus::REAL)(TEXT_START_Y - 5),
					   (Gdiplus::REAL)(COMPOSITE_WIDTH - TEXT_X - 20),
					   (Gdiplus::REAL)50);
		graphics->DrawString(wartist, -1, artist_font, artist_rect, &format, &artist_brush);
	}

	if (wtitle[0] != L'\0' && wcslen(wtitle) > 0) {
		Gdiplus::RectF title_rect((Gdiplus::REAL)TEXT_X, (Gdiplus::REAL)(TEXT_START_Y + 45),
					  (Gdiplus::REAL)(COMPOSITE_WIDTH - TEXT_X - 20),
					  (Gdiplus::REAL)120);
		graphics->DrawString(wtitle, -1, title_font, title_rect, &format, &text_brush);
	}

	delete label_font;
	delete title_font;
	delete artist_font;
}

static void load_album_art(const char *cache_dir, Gdiplus::Bitmap **out_bitmap)
{
	WIN32_FIND_DATAW find_data;
	wchar_t search_path[MAX_PATH];
	MultiByteToWideChar(CP_UTF8, 0, cache_dir, -1, search_path, MAX_PATH);
	wcscat_s(search_path, MAX_PATH, L"\\*");

	HANDLE hFind = FindFirstFileW(search_path, &find_data);
	if (hFind == INVALID_HANDLE_VALUE) {
		obs_log(LOG_INFO, "[fb2k] album-art directory not found: %s", cache_dir);
		return;
	}

	ULARGE_INTEGER latest_time = {0};
	wchar_t latest_file[MAX_PATH] = {0};
	int file_count = 0;

	do {
		if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			continue;

		const wchar_t *ext = wcsrchr(find_data.cFileName, L'.');
		if (!ext)
			continue;
		if (_wcsicmp(ext, L".jpg") != 0 && _wcsicmp(ext, L".png") != 0 &&
		    _wcsicmp(ext, L".jpeg") != 0)
			continue;

		file_count++;

		ULARGE_INTEGER file_time;
		file_time.LowPart = find_data.ftLastWriteTime.dwLowDateTime;
		file_time.HighPart = find_data.ftLastWriteTime.dwHighDateTime;

		if (file_time.QuadPart > latest_time.QuadPart) {
			latest_time.QuadPart = file_time.QuadPart;
			wcscpy_s(latest_file, MAX_PATH, find_data.cFileName);
		}
	} while (FindNextFileW(hFind, &find_data));

	FindClose(hFind);

	obs_log(LOG_INFO, "[fb2k] album-art cache: %d images found, latest: %S", file_count, latest_file);

	if (latest_file[0] == L'\0')
		return;

	wchar_t full_path[MAX_PATH];
	MultiByteToWideChar(CP_UTF8, 0, cache_dir, -1, full_path, MAX_PATH);
	wcscat_s(full_path, MAX_PATH, L"\\");
	wcscat_s(full_path, MAX_PATH, latest_file);

	*out_bitmap = Gdiplus::Bitmap::FromFile(full_path, FALSE);
	obs_log(LOG_INFO, "[fb2k] loaded album art: %S (bitmap=%p)", full_path, *out_bitmap);
}

static bool find_audio_file_for_track(const char *artist, const char *title,
				      const char *music_dir,
				      wchar_t *out_path, size_t out_size,
				      int max_depth)
{
	if (!music_dir || !music_dir[0] || max_depth <= 0)
		return false;

	wchar_t wdir[MAX_PATH];
	MultiByteToWideChar(CP_UTF8, 0, music_dir, -1, wdir, MAX_PATH);

	wchar_t search_path[MAX_PATH];
	wcscpy_s(search_path, MAX_PATH, wdir);
	wcscat_s(search_path, MAX_PATH, L"\\*");

	WIN32_FIND_DATAW ffd;
	HANDLE hFind = FindFirstFileW(search_path, &ffd);
	if (hFind == INVALID_HANDLE_VALUE)
		return false;

	wchar_t wartist[512], wtitle[512];
	MultiByteToWideChar(CP_UTF8, 0, artist, -1, wartist, 512);
	MultiByteToWideChar(CP_UTF8, 0, title, -1, wtitle, 512);
	_wcslwr_s(wartist, 512);
	_wcslwr_s(wtitle, 512);

	bool found = false;
	do {
		if (wcscmp(ffd.cFileName, L".") == 0 ||
		    wcscmp(ffd.cFileName, L"..") == 0)
			continue;

		wchar_t full_path[MAX_PATH];
		wcscpy_s(full_path, MAX_PATH, wdir);
		wcscat_s(full_path, MAX_PATH, L"\\");
		wcscat_s(full_path, MAX_PATH, ffd.cFileName);

		if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			char subdir_utf8[MAX_PATH];
			WideCharToMultiByte(CP_UTF8, 0, full_path, -1,
					    subdir_utf8, MAX_PATH, NULL, NULL);
			if (find_audio_file_for_track(artist, title,
						      subdir_utf8,
						      out_path, out_size,
						      max_depth - 1)) {
				found = true;
				break;
			}
			continue;
		}

		const wchar_t *ext = wcsrchr(ffd.cFileName, L'.');
		if (!ext)
			continue;
		if (_wcsicmp(ext, L".mp3") != 0 &&
		    _wcsicmp(ext, L".flac") != 0 &&
		    _wcsicmp(ext, L".m4a") != 0 &&
		    _wcsicmp(ext, L".wma") != 0)
			continue;

		wchar_t name_no_ext[MAX_PATH];
		wcsncpy_s(name_no_ext, MAX_PATH, ffd.cFileName, ext - ffd.cFileName);
		_wcslwr_s(name_no_ext, MAX_PATH);

		wchar_t *dash = wcsstr(name_no_ext, L" - ");
		if (!dash)
			continue;

		*dash = L'\0';
		const wchar_t *fartist = name_no_ext;
		const wchar_t *ftitle = dash + 3;

		if (wcscmp(fartist, wartist) != 0)
			continue;

		if (wcsstr(wtitle, ftitle) == NULL)
			continue;

		wcscpy_s(out_path, out_size, full_path);
		found = true;
		break;
	} while (FindNextFileW(hFind, &ffd));

	FindClose(hFind);
	return found;
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

	Gdiplus::Bitmap *bitmap = Gdiplus::Bitmap::FromHBITMAP(hbitmap, NULL);
	DeleteObject(hbitmap);
	return bitmap;
}

static bool try_load_cached_album_art(Gdiplus::Bitmap **out_art)
{
	char *appdata = getenv("APPDATA");
	if (!appdata)
		return false;

	const char *candidates[] = {
		"foobar2000-v2\\album-art",
		"foobar2000\\album-art",
	};
	for (int i = 0; i < 2 && !*out_art; i++) {
		char cache_dir[MAX_PATH];
		snprintf(cache_dir, sizeof(cache_dir), "%s\\%s", appdata, candidates[i]);
		obs_log(LOG_INFO, "[fb2k] checking album-art dir: %s", cache_dir);
		load_album_art(cache_dir, out_art);
	}
	return *out_art != NULL;
}

static bool try_load_embedded_album_art(struct foobar2000_data *s,
					Gdiplus::Bitmap **out_art)
{
	wchar_t file_path[MAX_PATH];

	if (s->last_found_path_valid) {
		wcscpy_s(file_path, MAX_PATH, s->last_found_path);
		*out_art = extract_album_art_from_file(file_path);
		if (*out_art) {
			obs_log(LOG_INFO, "[fb2k] embedded album art from cached path: %S", file_path);
			return true;
		}
		s->last_found_path_valid = false;
	}

	char search_dirs_buf[8][MAX_PATH];
	const char *search_dirs[8];
	int n = 0;

	if (s->music_dir[0])
		search_dirs[n++] = s->music_dir;

	char user_music[MAX_PATH];
	if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_MYMUSIC, NULL, 0, user_music)))
		search_dirs[n++] = user_music;

	const char *common_names[] = {"Music", "Muzik", "Müzik", "Downloads\\Music-Downloader"};
	DWORD drives = GetLogicalDrives();
	for (int d = 0; d < 26 && n < 8; d++) {
		if (!(drives & (1 << d)))
			continue;
		char root[4] = {(char)('A' + d), ':', '\\', 0};
		UINT type = GetDriveTypeA(root);
		if (type != DRIVE_FIXED)
			continue;
		for (int j = 0; j < 4 && n < 8; j++) {
			snprintf(search_dirs_buf[n], MAX_PATH, "%s%s", root, common_names[j]);
			if (GetFileAttributesA(search_dirs_buf[n]) != INVALID_FILE_ATTRIBUTES) {
				search_dirs[n] = search_dirs_buf[n];
				n++;
			}
		}
	}

	for (int i = 0; i < n && !*out_art; i++) {
		if (!search_dirs[i] || !search_dirs[i][0])
			continue;
		obs_log(LOG_INFO, "[fb2k] searching for audio file in: %s", search_dirs[i]);
		if (find_audio_file_for_track(s->artist, s->title,
					      search_dirs[i],
					      file_path, MAX_PATH, 3)) {
			*out_art = extract_album_art_from_file(file_path);
			if (*out_art) {
				obs_log(LOG_INFO, "[fb2k] embedded album art from: %S", file_path);
				wcscpy_s(s->last_found_path, MAX_PATH, file_path);
				s->last_found_path_valid = true;
				return true;
			}
		}
	}

	return false;
}

static void draw_album_art(Gdiplus::Graphics &graphics, Gdiplus::Bitmap *art)
{
	obs_log(LOG_INFO, "[fb2k] album art loaded: %dx%d", art->GetWidth(), art->GetHeight());
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

static void update_composite_bitmap(struct foobar2000_data *s)
{
	obs_log(LOG_INFO, "[fb2k] update_composite_bitmap: title='%s' artist='%s'", s->title, s->artist);

	if (s->pending_bitmap) {
		delete s->pending_bitmap;
		s->pending_bitmap = NULL;
	}

	auto *bitmap = new Gdiplus::Bitmap(COMPOSITE_WIDTH, COMPOSITE_HEIGHT, PixelFormat32bppARGB);
	Gdiplus::Graphics graphics(bitmap);
	graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
	graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
	graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);

	graphics.Clear(Gdiplus::Color(0, 0, 0, 0));

	Gdiplus::Bitmap *art = NULL;

	try_load_cached_album_art(&art);

	if (!art && s->artist[0] && s->title[0]) {
		obs_log(LOG_INFO, "[fb2k] cache miss, trying embedded album art");
		try_load_embedded_album_art(s, &art);
	}

	if (art) {
		draw_album_art(graphics, art);
		delete art;
	}

	render_text_to_bitmap(bitmap, &graphics, s->artist, s->title);

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
	if (wcsstr(title, L"foobar2000")) {
		info->hwnd = hwnd;
		return FALSE;
	}

	return TRUE;
}

static void poll_foobar2000(struct foobar2000_data *s)
{
	struct fb2k_find_info info = {0};
	EnumWindows(fb2k_enum_windows, (LPARAM)&info);
	HWND hwnd = info.hwnd;

	obs_log(LOG_INFO, "[fb2k] polling foobar2000... EnumWindows hwnd=%p", hwnd);

	if (!hwnd) {
		return;
	}

	wchar_t window_text[1024];
	GetWindowTextW(hwnd, window_text, 1024);
	obs_log(LOG_INFO, "[fb2k] window text: %S", window_text);

	char text_utf8[1024];
	WideCharToMultiByte(CP_UTF8, 0, window_text, -1, text_utf8, 1024, NULL, NULL);
	obs_log(LOG_INFO, "[fb2k] window text (utf8): %s", text_utf8);

	char old_artist[512];
	char old_title[512];
	strncpy_s(old_artist, sizeof(old_artist), s->artist, _TRUNCATE);
	strncpy_s(old_title, sizeof(old_title), s->title, _TRUNCATE);

	char *artist = s->artist;
	char *title = s->title;
	artist[0] = '\0';
	title[0] = '\0';

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

	if (text_utf8[0]) {
		char *dash = strstr(text_utf8, " - ");
		if (dash) {
			*dash = '\0';
			strncpy_s(artist, 512, text_utf8, _TRUNCATE);
			strncpy_s(title, 512, dash + 3, _TRUNCATE);
			obs_log(LOG_INFO, "[fb2k] parsed: artist='%s' title='%s'", artist, title);
		} else {
			strncpy_s(title, 512, text_utf8, _TRUNCATE);
			obs_log(LOG_INFO, "[fb2k] parsed (no dash): title='%s'", title);
		}
	}

	if (strcmp(s->artist, old_artist) != 0 || strcmp(s->title, old_title) != 0) {
		s->last_found_path_valid = false;
	}

	if (artist[0] == '\0' && title[0] == '\0') {
		obs_log(LOG_INFO, "[fb2k] empty artist/title, skipping bitmap update");
		return;
	}

	obs_log(LOG_INFO, "[fb2k] calling update_composite_bitmap");
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
	auto *s = (struct foobar2000_data *)bzalloc(sizeof(struct foobar2000_data));
	s->source = source;
	s->width = COMPOSITE_WIDTH;
	s->height = COMPOSITE_HEIGHT;
	s->last_poll_ns = 0;
	s->artist[0] = '\0';
	s->title[0] = '\0';
	s->music_dir[0] = '\0';
	s->last_found_path_valid = false;

	const char *user_music = NULL;
	{
		char path[MAX_PATH];
		if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_MYMUSIC, NULL, 0, path)))
			user_music = path;
		else
			user_music = "";
		snprintf(s->music_dir, sizeof(s->music_dir), "%s", user_music);
	}
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
					PixelFormat32bppARGB, &bitmap_data) != Gdiplus::Ok) {
		obs_log(LOG_INFO, "[fb2k] LockBits failed in render");
		delete s->pending_bitmap;
		s->pending_bitmap = NULL;
		return;
	}

	uint8_t *data = (uint8_t *)bmalloc(COMPOSITE_WIDTH * COMPOSITE_HEIGHT * 4);
	if (!data) {
		s->pending_bitmap->UnlockBits(&bitmap_data);
		delete s->pending_bitmap;
		s->pending_bitmap = NULL;
		return;
	}

	const uint8_t *src = (const uint8_t *)bitmap_data.Scan0;
	for (int y = 0; y < COMPOSITE_HEIGHT; y++) {
		memcpy(data + y * COMPOSITE_WIDTH * 4,
		       src + y * bitmap_data.Stride,
		       COMPOSITE_WIDTH * 4);
	}
	s->pending_bitmap->UnlockBits(&bitmap_data);
	delete s->pending_bitmap;
	s->pending_bitmap = NULL;

	gs_texture_t *tex = gs_texture_create(COMPOSITE_WIDTH, COMPOSITE_HEIGHT,
					      GS_BGRA, 1, (const uint8_t **)&data, 0);
	bfree(data);

	if (tex) {
		s->composite_texture = tex;
		obs_log(LOG_INFO, "[fb2k] texture created OK (%dx%d)", COMPOSITE_WIDTH, COMPOSITE_HEIGHT);
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
		gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");
		if (image)
			gs_effect_set_texture(image, s->composite_texture);
		gs_draw_sprite(s->composite_texture, 0, s->width, s->height);
	}
}

static void source_update(void *data, obs_data_t *settings)
{
	auto *s = (struct foobar2000_data *)data;
	const char *dir = obs_data_get_string(settings, "music_dir");
	if (dir && dir[0]) {
		strncpy_s(s->music_dir, sizeof(s->music_dir), dir, _TRUNCATE);
	} else {
		s->music_dir[0] = '\0';
	}
	s->last_found_path_valid = false;
}

static obs_properties_t *source_get_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *props = obs_properties_create();
	obs_properties_add_text(props, "track_info", obs_module_text("TrackInfo"),
				OBS_TEXT_INFO);
	obs_properties_add_path(props, "music_dir", obs_module_text("MusicDirectory"),
				OBS_PATH_DIRECTORY, NULL, NULL);
	return props;
}

static void source_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "music_dir", "");
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
