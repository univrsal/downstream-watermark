#include "settingsdialog.h"
#include "ui_settingsdialog.h"

#include <QFileDialog>
#include <QMainWindow>
#include <obs-frontend-api.h>
#include <util/config-file.h>

#define utf8_to_qt(_str) QString::fromUtf8(_str)
#define qt_to_utf8(_str) _str.toUtf8().constData()

void frontend_save_load(obs_data_t *save_data, bool saving, void *data)
{
	SettingsDialog *dialog = (SettingsDialog *)data;
	if (saving) {
		obs_data_t *obj = obs_data_create();
		dialog->save(obj);
		obs_data_set_obj(save_data, "bitrate-dock", obj);
		obs_data_release(obj);
	} else {
		auto obj = obs_data_get_obj(save_data, "bitrate-dock");
		dialog->load(obj);
		obs_data_release(obj);
	}
}

void frontend_event(enum obs_frontend_event event, void *data)
{
	auto dialog = static_cast<SettingsDialog *>(data);
	if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP &&
	    dialog->loaded) {

	} else if (event == OBS_FRONTEND_EVENT_EXIT) {
		obs_frontend_remove_event_callback(frontend_event, dialog);
		dialog->ClearKeyer();
	} else if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
		dialog->AddKeyer();
	}
}

SettingsDialog::SettingsDialog(QWidget *parent)
	: QDialog(parent),
	  ui(new Ui::SettingsDialog)
{
	ui->setupUi(this);

	get_transitions = [](void *, struct obs_frontend_source_list *sources) {
		obs_frontend_get_transitions(sources);
	};

	obs_frontend_add_event_callback(frontend_event, this);

	connect(ui->txt_path, &QLineEdit::textChanged, this,
		&SettingsDialog::on_image_path_changed);

	connect(ui->sb_x, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&SettingsDialog::on_settings_changed);
	connect(ui->sb_y, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&SettingsDialog::on_settings_changed);
	connect(ui->sb_scale,
		QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
		&SettingsDialog::on_settings_changed);
	connect(ui->sb_opacity, QOverload<int>::of(&QSpinBox::valueChanged),
		this, &SettingsDialog::on_settings_changed);

	connect(ui->btn_ok, &QPushButton::clicked, this,
		&SettingsDialog::toggleShowHide);

	connect(ui->btn_browse, &QPushButton::clicked, [this]() {
		QString path = QFileDialog::getOpenFileName(
			this, "Select image", "",
			"Images (*.png *.jpg *.jpeg)");
		if (!path.isEmpty()) {
			ui->txt_path->setText(path);
			on_image_path_changed();
		}
	});
}

void SettingsDialog::load(obs_data_t *_data)
{
	if (obs_data_has_user_value(_data, "path")) {
		ui->txt_path->setText(
			utf8_to_qt(obs_data_get_string(_data, "path")));
	}

	if (obs_data_has_user_value(_data, "x")) {
		ui->sb_x->setValue((int)obs_data_get_int(_data, "x"));
	}

	if (obs_data_has_user_value(_data, "y")) {
		ui->sb_y->setValue((int)obs_data_get_int(_data, "y"));
	}

	if (obs_data_has_user_value(_data, "scale")) {
		ui->sb_scale->setValue(obs_data_get_double(_data, "scale"));
	}

	if (obs_data_has_user_value(_data, "opacity")) {
		ui->sb_opacity->setValue(
			(int)obs_data_get_int(_data, "opacity"));
	}
	on_image_path_changed();
}

void SettingsDialog::save(obs_data_t *_data)
{
	obs_data_set_string(_data, "path", qt_to_utf8(ui->txt_path->text()));
	obs_data_set_int(_data, "x", ui->sb_x->value());
	obs_data_set_int(_data, "y", ui->sb_y->value());
	obs_data_set_double(_data, "scale", ui->sb_scale->value());
	obs_data_set_int(_data, "opacity", ui->sb_opacity->value());
}

SettingsDialog::~SettingsDialog()
{
	obs_set_output_source(outputChannel, nullptr);

	if (m_transition) {
		obs_transition_clear(m_transition);
		obs_source_release(m_transition);
		m_transition = nullptr;
	}
	if (m_showTransition) {
		obs_transition_clear(m_showTransition);
		obs_source_release(m_showTransition);
		m_showTransition = nullptr;
	}
	if (m_hideTransition) {
		obs_transition_clear(m_hideTransition);
		obs_source_release(m_hideTransition);
		m_hideTransition = nullptr;
	}
	if (m_overrideTransition) {
		obs_transition_clear(m_overrideTransition);
		obs_source_release(m_overrideTransition);
		m_overrideTransition = nullptr;
	}

	obs_source_release(m_watermark_source);
	obs_source_release(m_color_filter);
	obs_data_release(m_watermark_data);

	delete ui;
}

void SettingsDialog::on_image_path_changed()
{
	if (!m_watermark_data) {
		m_watermark_data = obs_data_create();
	}

	obs_data_set_string(m_watermark_data, "file",
			    qt_to_utf8(ui->txt_path->text()));
	obs_data_set_double(m_watermark_data, "scale", ui->sb_scale->value());
	obs_data_set_int(m_watermark_data, "pos.x", ui->sb_x->value());
	obs_data_set_int(m_watermark_data, "pos.y", ui->sb_y->value());
	obs_data_set_int(m_watermark_data, "opacity", ui->sb_opacity->value());

	if (!m_watermark_source) {
		m_watermark_source = obs_source_create_private(
			"overlay_source", "downstreamwatermarksource",
			m_watermark_data);

		m_color_filter = obs_source_create_private(
			"color_filter", "downstreamwatermarkcolorfilter",
			m_watermark_data);
		obs_source_add_active_child(m_watermark_source, m_color_filter);
		obs_source_filter_add(m_watermark_source, m_color_filter);
	}

	obs_source_update(m_watermark_source, m_watermark_data);
	obs_source_update(m_color_filter, m_watermark_data);

	apply_source(m_watermark_source);
}

void SettingsDialog::on_settings_changed()
{
	obs_data_set_double(m_watermark_data, "scale", ui->sb_scale->value());
	obs_data_set_int(m_watermark_data, "pos.x", ui->sb_x->value());
	obs_data_set_int(m_watermark_data, "pos.y", ui->sb_y->value());
	obs_data_set_int(m_watermark_data, "opacity", ui->sb_opacity->value());
	obs_source_update(m_watermark_source, m_watermark_data);
	obs_source_update(m_color_filter, m_watermark_data);
}

void SettingsDialog::apply_source(obs_source_t *newSource)
{
	obs_source_t *prevSource = obs_get_output_source(outputChannel);
	obs_source_t *prevTransition = nullptr;
	if (prevSource &&
	    obs_source_get_type(prevSource) == OBS_SOURCE_TYPE_TRANSITION) {
		prevTransition = prevSource;
		prevSource = obs_transition_get_active_source(prevSource);
	}
	obs_source_t *newTransition = nullptr;
	uint32_t newTransitionDuration = transitionDuration;
	if (prevSource == newSource) {
	} else if (!prevSource && newSource && m_showTransition) {
		newTransition = m_showTransition;
		newTransitionDuration = showTransitionDuration;
	} else if (prevSource && !newSource && m_hideTransition) {
		newTransition = m_hideTransition;
		newTransitionDuration = hideTransitionDuration;
	} else {
		auto ph = obs_get_proc_handler();
		calldata_t cd = {0};
		calldata_set_string(&cd, "from_scene",
				    obs_source_get_name(prevSource));
		calldata_set_string(&cd, "to_scene",
				    obs_source_get_name(newSource));
		if (proc_handler_call(ph, "get_transition_table_transition",
				      &cd)) {
			const char *p = calldata_string(&cd, "transition");
			SetTransition(p ? p : "", transitionType::override);
			SetTransitionDuration((int)calldata_int(&cd,
								"duration"),
					      transitionType::override);
		} else {
			SetTransition("", transitionType::override);
		}
		calldata_free(&cd);
		if (m_overrideTransition) {
			newTransition = m_overrideTransition;
			newTransitionDuration = overrideTransitionDuration;
		} else if (m_transition)
			newTransition = m_transition;
	}
	if (prevSource == newSource) {
		//skip if nothing changed
	} else {
		if (!newTransition) {
			obs_set_output_source(outputChannel, newSource);
		} else {
			obs_transition_set(newTransition, prevSource);

			obs_transition_start(newTransition,
					     OBS_TRANSITION_MODE_AUTO,
					     newTransitionDuration, newSource);

			if (prevTransition != newTransition) {
				obs_set_output_source(outputChannel,
						      newTransition);
			}
		}
	}

	obs_source_release(prevSource);
	obs_source_release(prevTransition);
}

void SettingsDialog::SetTransition(const char *transition_name,
				   enum transitionType transition_type)
{
	obs_source_t *oldTransition = m_transition;
	if (transition_type == transitionType::show)
		oldTransition = m_showTransition;
	else if (transition_type == transitionType::hide)
		oldTransition = m_hideTransition;
	else if (transition_type == transitionType::override)
		oldTransition = m_overrideTransition;

	if (!oldTransition && (!transition_name || !strlen(transition_name)))
		return;

	obs_source_t *newTransition = nullptr;
	obs_frontend_source_list transitions = {};
	get_transitions(get_transitions_data, &transitions);
	for (size_t i = 0; i < transitions.sources.num; i++) {
		const char *n =
			obs_source_get_name(transitions.sources.array[i]);
		if (!n)
			continue;
		if (strcmp(transition_name, n) == 0) {
			newTransition = obs_source_duplicate(
				transitions.sources.array[i],
				obs_source_get_name(
					transitions.sources.array[i]),
				true);
			break;
		}
	}
	obs_frontend_source_list_free(&transitions);

	if (transition_type == transitionType::show)
		m_showTransition = newTransition;
	else if (transition_type == transitionType::hide)
		m_hideTransition = newTransition;
	else if (transition_type == transitionType::override)
		m_overrideTransition = newTransition;
	else
		m_transition = newTransition;
	obs_source_t *prevSource = obs_get_output_source(outputChannel);
	if (oldTransition && prevSource == oldTransition) {
		if (newTransition) {
			//swap transition
			obs_transition_swap_begin(newTransition, oldTransition);
			obs_set_output_source(outputChannel, newTransition);
			obs_transition_swap_end(newTransition, oldTransition);
		} else {
			obs_set_output_source(outputChannel, nullptr);
		}
	}
	obs_source_release(prevSource);
	if (oldTransition) {
		obs_transition_clear(oldTransition);
		obs_source_release(oldTransition);
	}
}

void SettingsDialog::SetTransitionDuration(int duration,
					   enum transitionType transition_type)
{
	if (transition_type == match)
		transitionDuration = duration;
	else if (transition_type == transitionType::show)
		showTransitionDuration = duration;
	else if (transition_type == transitionType::hide)
		hideTransitionDuration = duration;
	else if (transition_type == transitionType::override)
		overrideTransitionDuration = duration;
}
