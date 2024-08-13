/*
Downstream watermark
Copyright (C) 2024 Alex <contact@vrsal.xyz>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include <obs-module.h>
#include <plugin-support.h>

#include <QAction>
#include <QMainWindow>
#include <obs-frontend-api.h>

#include "settingsdialog.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

SettingsDialog *settings{};

bool obs_module_load(void)
{
	const auto menu_action = static_cast<QAction *>(
		obs_frontend_add_tools_menu_qaction("Downstream watermark"));
	obs_frontend_push_ui_translation(obs_module_get_string);
	const auto main_window =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());
	settings = new SettingsDialog(main_window);
	obs_frontend_pop_ui_translation();

	const auto menu_cb = [] {
		settings->toggleShowHide();
	};
	QAction::connect(menu_action, &QAction::triggered, menu_cb);

	obs_log(LOG_INFO, "plugin loaded successfully (version %s)",
		PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_log(LOG_INFO, "plugin unloaded");
}
