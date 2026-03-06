/*
GameSave - OBS Livestream Recording Organizer
Copyright (C) 2026

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#pragma once

#include <QDialog>
#include <QDateTime>
#include <QString>

class QLineEdit;
class QSpinBox;
class QTabWidget;
class QDateTimeEdit;
class QCheckBox;
class QLabel;
class QPushButton;
class QComboBox;
class YouTubeApiClient;

struct WeekendGameEntry {
	QString opponent;
	QDateTime scheduledTime;
	QString rinkName;
};

class TournamentWeekendDialog : public QDialog {
	Q_OBJECT

public:
	explicit TournamentWeekendDialog(YouTubeApiClient *ytClient, QWidget *parent = nullptr);
	~TournamentWeekendDialog() override;

	// Call after exec() to know if broadcasts were created (so dock can refresh).
	bool broadcastsCreated() const { return m_broadcastsCreated; }

Q_SIGNALS:
	void refreshRequested();

private:
	void loadRememberedSettings();
	void saveRememberedSettings();
	void rebuildGameTabs();
	QWidget *makeGameTab(int index);
	void updatePreviewLabels(int index);
	bool validateAndCollect(QList<WeekendGameEntry> *out);
	void startCreateAll();
	void onBroadcastCreated(const QString &broadcastId);
	void onStreamBound();
	void onThumbnailDone();
	void onYtError(const QString &message);
	void advanceToNextOrFinish();

	YouTubeApiClient *m_ytClient = nullptr;
	QLineEdit *m_tournamentEdit = nullptr;
	QLineEdit *m_teamNameEdit = nullptr;
	QSpinBox *m_gameCountSpin = nullptr;
	QTabWidget *m_tabWidget = nullptr;
	QComboBox *m_privacyCombo = nullptr;
	QCheckBox *m_madeForKidsCheck = nullptr;
	QLineEdit *m_thumbnailEdit = nullptr;
	QCheckBox *m_rememberCheck = nullptr;
	QLabel *m_statusLabel = nullptr;
	QPushButton *m_createAllButton = nullptr;

	QString m_currentBroadcastId;
	QMetaObject::Connection m_thumbnailSetConn;
	QMetaObject::Connection m_thumbnailErrorConn;

	struct GameTabWidgets {
		QLineEdit *opponentEdit = nullptr;
		QDateTimeEdit *scheduledEdit = nullptr;
		QLineEdit *rinkEdit = nullptr;
		QLabel *titlePreview = nullptr;
		QLabel *descPreview = nullptr;
	};
	QList<GameTabWidgets> m_gameWidgets;

	QList<WeekendGameEntry> m_createQueue;
	int m_createIndex = -1;
	QMetaObject::Connection m_createdConn;
	QMetaObject::Connection m_boundConn;
	QMetaObject::Connection m_errorConn;
	bool m_broadcastsCreated = false;
};
