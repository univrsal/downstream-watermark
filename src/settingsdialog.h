#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

#include <obs/obs.hpp>

namespace Ui {
class SettingsDialog;
}

enum transitionType { match, show, hide, override };

typedef void (*get_transitions_callback_t)(
	void *data, struct obs_frontend_source_list *sources);

class SettingsDialog : public QDialog {
	Q_OBJECT

	OBSSource m_watermark_source{};
	OBSData m_watermark_data{};
	obs_view_t *m_view{};
	int outputChannel{7};
	get_transitions_callback_t get_transitions = nullptr;
	void *get_transitions_data = nullptr;

	uint32_t transitionDuration;
	uint32_t showTransitionDuration;
	uint32_t hideTransitionDuration;
	uint32_t overrideTransitionDuration;

	OBSSource transition;
	OBSSource showTransition;
	OBSSource hideTransition;
	OBSSource overrideTransition;

public:
	explicit SettingsDialog(QWidget *parent = nullptr);
	~SettingsDialog();

	void toggleShowHide()
	{
		if (isVisible()) {
			hide();
		} else {
			show();
		}
	}

private slots:
	void on_image_path_changed();

	void on_settings_changed();

public:
	bool loaded;
	Ui::SettingsDialog *ui;

	void apply_source(OBSSource source);

	void SetTransition(const char *transition_name,
			   enum transitionType transition_type = match);
	void SetTransitionDuration(int duration,
				   enum transitionType transition_type);

	void ClearKeyer() {}
	void AddKeyer() {}
	void SceneChanged() {}
};

#endif // SETTINGSDIALOG_H
