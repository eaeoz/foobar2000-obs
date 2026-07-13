#include "foobar2000/SDK/component.h"
#include "foobar2000/SDK/play_callback.h"
#include "foobar2000/SDK/initquit.h"

#include <windows.h>
#include <shlobj.h>

#define BRIDGE_DIR_NAME "foobar2000-obs"
#define BRIDGE_FILE_NAME "bridge.txt"

static char g_bridge_path[MAX_PATH];

static void ensure_bridge_path(void)
{
	if (g_bridge_path[0])
		return;

	char localappdata[MAX_PATH];
	if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0,
					localappdata))) {
		snprintf(g_bridge_path, MAX_PATH, "%s\\%s\\%s", localappdata,
			 BRIDGE_DIR_NAME, BRIDGE_FILE_NAME);

		char dir[MAX_PATH];
		snprintf(dir, MAX_PATH, "%s\\%s", localappdata, BRIDGE_DIR_NAME);
		CreateDirectoryA(dir, NULL);
	}
}

static void write_bridge_file(const char *path)
{
	ensure_bridge_path();
	if (!g_bridge_path[0])
		return;

	HANDLE hFile =
		CreateFileA(g_bridge_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
			    FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile != INVALID_HANDLE_VALUE) {
		DWORD written;
		WriteFile(hFile, path, (DWORD)strlen(path), &written, NULL);
		CloseHandle(hFile);
	}
}

static void clear_bridge_file(void)
{
	ensure_bridge_path();
	if (g_bridge_path[0])
		DeleteFileA(g_bridge_path);
}

class obs_bridge_play_callback : public play_callback_impl_base {
public:
	obs_bridge_play_callback(void)
		: play_callback_impl_base(flag_on_playback_new_track |
					  flag_on_playback_stop)
	{
	}

	void on_playback_new_track(metadb_handle_ptr p_track) override
	{
		write_bridge_file(p_track->get_path());
	}

	void on_playback_stop(play_control::t_stop_reason p_reason) override
	{
		(void)p_reason;
		clear_bridge_file();
	}
};

class obs_bridge_init : public initquit {
public:
	void on_init() override { m_cb = new obs_bridge_play_callback(); }

	void on_quit() override
	{
		delete m_cb;
		m_cb = nullptr;
	}

private:
	obs_bridge_play_callback *m_cb = nullptr;
};

static initquit_factory_t<obs_bridge_init> g_obs_bridge_factory;

DECLARE_COMPONENT_VERSION("OBS Bridge", "1.0",
			   "Writes current playing file path for OBS plugin");
VALIDATE_COMPONENT_FILENAME("foo_obsbridge.dll");
