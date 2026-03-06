/*
GameSave - OBS Livestream Recording Organizer
Copyright (C) 2026

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QTimer>
#include <QDir>
#include <QFile>
#include <QFileInfo>

struct YouTubeBroadcast;
struct YouTubeStreamKey;
class YouTubeApiClient;

class GameSaveDock : public QWidget {
	Q_OBJECT

public:
	explicit GameSaveDock(QWidget *parent = nullptr);
	~GameSaveDock() override;

	QString tournamentName() const;
	void setTournamentName(const QString &name);
	QString basePath() const;
	bool autoOrganize() const;

public Q_SLOTS:
	void setStatus(const QString &text);
	void onReadFromYouTube();
	void onBrowseBasePath();
	void onApply();
	void onRecordingStopped();

private:
	void refreshBroadcastList();
	void onBroadcastSelectionChanged(int index);
	void onLoadOrNewClicked();
	void onBroadcastsListed(const QList<YouTubeBroadcast> &broadcasts);
	void onStreamKeyReceived(const YouTubeStreamKey &key);
	void onYtError(const QString &message);
	void applyStreamKeyToOBS(const QString &streamKey, const QString &serverUrl, const QString &broadcastTitle);
	void startStreamIfRequested();

	QComboBox *m_broadcastCombo = nullptr;
	QPushButton *m_loadOrNewButton = nullptr;
	QPushButton *m_refreshYtButton = nullptr;
	QLineEdit *m_tournamentNameEdit = nullptr;
	QLineEdit *m_basePathEdit = nullptr;
	QPushButton *m_readYtButton = nullptr;
	QPushButton *m_browseButton = nullptr;
	QPushButton *m_applyButton = nullptr;
	QCheckBox *m_autoOrganizeCheck = nullptr;
	QLabel *m_statusLabel = nullptr;

	YouTubeApiClient *m_ytClient = nullptr;
	bool m_loadAndStartRequested = false;
	QString m_pendingBroadcastTitle;
};
