/*
GameSave - OBS Livestream Recording Organizer
Copyright (C) 2026

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <plugin-support.h>
#include "game-save-dock.hpp"
#include <QMainWindow>

static GameSaveDock *s_dock = nullptr;

static void on_frontend_event(enum obs_frontend_event event, void *data)
{
	if (event != OBS_FRONTEND_EVENT_RECORDING_STOPPED) {
		return;
	}
	auto *dock = static_cast<GameSaveDock *>(data);
	if (dock) {
		dock->onRecordingStopped();
	}
}

bool obs_module_load(void)
{
	obs_log(LOG_INFO, "GameSave plugin loaded (version %s)", PLUGIN_VERSION);

	QMainWindow *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!mainWindow) {
		obs_log(LOG_ERROR, "GameSave: could not get main window");
		return true;
	}

	s_dock = new GameSaveDock(mainWindow);
	obs_frontend_add_dock_by_id("game-save-dock", "GameSave", s_dock);
	obs_frontend_add_event_callback(on_frontend_event, s_dock);

	return true;
}

void obs_module_unload(void)
{
	if (s_dock) {
		obs_frontend_remove_event_callback(on_frontend_event, s_dock);
		obs_frontend_remove_dock("game-save-dock");
		s_dock = nullptr;
	}
	obs_log(LOG_INFO, "GameSave plugin unloaded");
}
