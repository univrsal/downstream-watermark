#include "settingsdialog.h"
#include "ui_settingsdialog.h"

#include <obs-frontend-api.h>

#define utf8_to_qt(_str) QString::fromUtf8(_str)
#define qt_to_utf8(_str) _str.toUtf8().constData()

void frontend_event(enum obs_frontend_event event, void *data)
{
	auto downstreamKeyerDock = static_cast<SettingsDialog *>(data);
	if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP &&
	    downstreamKeyerDock->loaded) {
		downstreamKeyerDock->ClearKeyer();
		downstreamKeyerDock->AddKeyer();
	} else if (event == OBS_FRONTEND_EVENT_EXIT) {
		downstreamKeyerDock->ClearKeyer();
	} else if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED) {
		downstreamKeyerDock->SceneChanged();
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
}

SettingsDialog::~SettingsDialog()
{
	obs_frontend_remove_event_callback(frontend_event, this);
	delete ui;
}

void SettingsDialog::on_image_path_changed()
{
	if (m_watermark_image_source) {
		m_watermark_image_source = nullptr;
		apply_source(nullptr);
	}
	OBSData settings = obs_data_create();
	obs_data_set_string(settings, "file", qt_to_utf8(ui->txt_path->text()));
	m_watermark_image_source = obs_source_create_private(
		"image_source", "__watermark", settings);

	apply_source(m_watermark_image_source);
}

void SettingsDialog::apply_source(OBSSource newSource)
{

	OBSSource prevSource =
		m_view != nullptr ? obs_view_get_source(m_view, outputChannel)
				  : obs_get_output_source(outputChannel);
	obs_source_t *prevTransition = nullptr;
	if (prevSource &&
	    obs_source_get_type(prevSource) == OBS_SOURCE_TYPE_TRANSITION) {
		prevTransition = prevSource;
		prevSource = obs_transition_get_active_source(prevSource);
	}
	obs_source_t *newTransition = nullptr;
	uint32_t newTransitionDuration = transitionDuration;
	if (prevSource == newSource) {
	} else if (!prevSource && newSource && showTransition) {
		newTransition = showTransition;
		newTransitionDuration = showTransitionDuration;
	} else if (prevSource && !newSource && hideTransition) {
		newTransition = hideTransition;
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
		if (overrideTransition) {
			newTransition = overrideTransition;
			newTransitionDuration = overrideTransitionDuration;
		} else if (transition)
			newTransition = transition;
	}
	if (prevSource == newSource) {
		//skip if nothing changed
	} else {
		if (!newTransition) {
			if (m_view) {
				obs_view_set_source(m_view, outputChannel,
						    newSource);
			} else {
				obs_set_output_source(outputChannel, newSource);
			}
		} else {
			obs_transition_set(newTransition, prevSource);

			obs_transition_start(newTransition,
					     OBS_TRANSITION_MODE_AUTO,
					     newTransitionDuration, newSource);

			if (prevTransition != newTransition) {
				if (m_view) {
					obs_view_set_source(m_view,
							    outputChannel,
							    newTransition);
				} else {
					obs_set_output_source(outputChannel,
							      newTransition);
				}
			}
		}
	}

	obs_source_release(prevSource);
	obs_source_release(prevTransition);
}

void SettingsDialog::SetTransition(const char *transition_name,
				   enum transitionType transition_type)
{
	OBSSource oldTransition = transition;
	if (transition_type == transitionType::show)
		oldTransition = showTransition;
	else if (transition_type == transitionType::hide)
		oldTransition = hideTransition;
	else if (transition_type == transitionType::override)
		oldTransition = overrideTransition;

	if (!oldTransition && (!transition_name || !strlen(transition_name)))
		return;

	OBSSource newTransition = nullptr;
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
		showTransition = newTransition;
	else if (transition_type == transitionType::hide)
		hideTransition = newTransition;
	else if (transition_type == transitionType::override)
		overrideTransition = newTransition;
	else
		transition = newTransition;
	OBSSource prevSource =
		m_view ? obs_view_get_source(m_view, outputChannel)
		       : obs_get_output_source(outputChannel);
	if (oldTransition && prevSource == oldTransition) {
		if (newTransition) {
			//swap transition
			obs_transition_swap_begin(newTransition, oldTransition);
			if (m_view) {
				obs_view_set_source(m_view, outputChannel,
						    newTransition);
			} else {
				obs_set_output_source(outputChannel,
						      newTransition);
			}
			obs_transition_swap_end(newTransition, oldTransition);
		} else {
			if (m_view) {
				obs_view_set_source(m_view, outputChannel,
						    nullptr);
			} else {
				obs_set_output_source(outputChannel, nullptr);
			}
		}
	}
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
