#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <obs-module.h>
namespace Ui {
class SettingsDialog;
}

enum transitionType { match, show, hide, override };

typedef void (*get_transitions_callback_t)(
	void *data, struct obs_frontend_source_list *sources);

class SettingsDialog : public QDialog {
	Q_OBJECT

	obs_source_t *m_watermark_source{};
	obs_source_t *m_color_filter{};
	obs_data_t *m_watermark_data{};
	int outputChannel{7};
	get_transitions_callback_t get_transitions = nullptr;
	void *get_transitions_data = nullptr;

	uint32_t transitionDuration;
	uint32_t showTransitionDuration;
	uint32_t hideTransitionDuration;
	uint32_t overrideTransitionDuration;

	obs_source_t *m_transition{};
	obs_source_t *m_showTransition{};
	obs_source_t *m_hideTransition{};
	obs_source_t *m_overrideTransition{};

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

	void apply_source(obs_source_t *source);

	void SetTransition(const char *transition_name,
			   enum transitionType transition_type = match);
	void SetTransitionDuration(int duration,
				   enum transitionType transition_type);

	void ClearKeyer() { apply_source(nullptr); }
	void AddKeyer()
	{
		if (m_watermark_source)
			apply_source(m_watermark_source);
	}

	void load(obs_data_t *data);
	void save(obs_data_t *data);
};

extern void frontend_save_load(obs_data_t *save_data, bool saving, void *data);

#endif // SETTINGSDIALOG_H
