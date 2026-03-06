/*
GameSave - OBS Livestream Recording Organizer
Copyright (C) 2026

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#include "game-save-dock.hpp"
#include "youtube-client.hpp"
#include "load-broadcast-dialog.hpp"
#include "schedule-broadcast-dialog.hpp"
#include "tournament-weekend-dialog.hpp"
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <obs.h>
#include <util/config-file.h>
#include <util/platform.h>
#include <QMessageBox>
#include <QLocale>

static const int kComboIndexScheduleBroadcast = 0;
static const int kComboDataBroadcastId = Qt::UserRole;
static const int kComboDataBoundStreamId = Qt::UserRole + 1;
static const int kComboDataTitle = Qt::UserRole + 2;

GameSaveDock::GameSaveDock(QWidget *parent) : QWidget(parent)
{
	m_ytClient = new YouTubeApiClient(this);
	connect(m_ytClient, &YouTubeApiClient::broadcastsListed, this, &GameSaveDock::onBroadcastsListed);
	connect(m_ytClient, &YouTubeApiClient::error, this, &GameSaveDock::onYtError);

	QVBoxLayout *mainLayout = new QVBoxLayout(this);

	QLabel *broadcastLabel = new QLabel(tr("YouTube Broadcast:"), this);
	mainLayout->addWidget(broadcastLabel);

	QHBoxLayout *broadcastRow = new QHBoxLayout();
	m_broadcastCombo = new QComboBox(this);
	m_broadcastCombo->addItem(tr("Schedule broadcast"));
	m_broadcastCombo->setCurrentIndex(kComboIndexScheduleBroadcast);
	broadcastRow->addWidget(m_broadcastCombo);

	m_loadOrNewButton = new QPushButton(tr("New"), this);
	m_loadOrNewButton->setToolTip(tr("Schedule a new broadcast (with Schedule broadcast selected) or Load selected broadcast"));
	broadcastRow->addWidget(m_loadOrNewButton);

	m_refreshYtButton = new QPushButton(tr("Refresh"), this);
	m_refreshYtButton->setToolTip(tr("Refresh list of YouTube broadcasts"));
	broadcastRow->addWidget(m_refreshYtButton);

	m_weekendButton = new QPushButton(tr("Setup weekend"), this);
	m_weekendButton->setToolTip(tr("Setup multiple streams for a tournament weekend"));
	broadcastRow->addWidget(m_weekendButton);

	QPushButton *signInButton = new QPushButton(tr("Sign in with YouTube"), this);
	signInButton->setToolTip(tr("Sign in to list and manage YouTube broadcasts"));
	broadcastRow->addWidget(signInButton);
	mainLayout->addLayout(broadcastRow);

	connect(m_broadcastCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GameSaveDock::onBroadcastSelectionChanged);
	connect(m_loadOrNewButton, &QPushButton::clicked, this, &GameSaveDock::onLoadOrNewClicked);
	connect(m_refreshYtButton, &QPushButton::clicked, this, [this]() { refreshBroadcastList(); });
	connect(m_weekendButton, &QPushButton::clicked, this, &GameSaveDock::onSetupWeekendClicked);
	connect(signInButton, &QPushButton::clicked, this, [this]() {
		m_ytClient->startAuth();
	});
	connect(m_ytClient, &YouTubeApiClient::authFinished, this, [this](bool success) {
		if (success) {
			setStatus(tr("Signed in to YouTube"));
			refreshBroadcastList();
		} else {
			QString err = m_ytClient->authError();
			setStatus(err);
			QMessageBox::warning(this, tr("YouTube sign-in"), err + "\n\n" +
				tr("To sign in: create a file \"youtube_client.json\" in the GameSave plugin config folder (same folder as youtube_oauth.json) with \"client_id\" and \"client_secret\" from Google Cloud Console (OAuth 2.0 Desktop app). OBS's built-in YouTube stream key cannot be used for listing or creating broadcasts."));
		}
	});

	QLabel *tournamentLabel = new QLabel(tr("Tournament Name:"), this);
	mainLayout->addWidget(tournamentLabel);

	QHBoxLayout *tournamentRow = new QHBoxLayout();
	m_tournamentNameEdit = new QLineEdit(this);
	m_tournamentNameEdit->setPlaceholderText(tr("e.g. Spring Tournament 2026"));
	tournamentRow->addWidget(m_tournamentNameEdit);

	m_readYtButton = new QPushButton(tr("YT"), this);
	m_readYtButton->setToolTip(tr("Read from YouTube broadcast title"));
	tournamentRow->addWidget(m_readYtButton);
	mainLayout->addLayout(tournamentRow);

	QLabel *basePathLabel = new QLabel(tr("Base Recording Directory:"), this);
	mainLayout->addWidget(basePathLabel);

	QHBoxLayout *pathRow = new QHBoxLayout();
	m_basePathEdit = new QLineEdit(this);
	pathRow->addWidget(m_basePathEdit);

	m_browseButton = new QPushButton(tr("..."), this);
	m_browseButton->setToolTip(tr("Browse"));
	pathRow->addWidget(m_browseButton);
	mainLayout->addLayout(pathRow);

	m_autoOrganizeCheck = new QCheckBox(tr("Auto-organize on stop"), this);
	m_autoOrganizeCheck->setChecked(true);
	mainLayout->addWidget(m_autoOrganizeCheck);

	m_applyButton = new QPushButton(tr("Apply"), this);
	mainLayout->addWidget(m_applyButton);

	m_statusLabel = new QLabel(tr("Status: —"), this);
	m_statusLabel->setWordWrap(true);
	m_statusLabel->setStyleSheet("color: gray;");
	mainLayout->addWidget(m_statusLabel);

	mainLayout->addStretch();

	connect(m_readYtButton, &QPushButton::clicked, this, &GameSaveDock::onReadFromYouTube);
	connect(m_browseButton, &QPushButton::clicked, this, &GameSaveDock::onBrowseBasePath);
	connect(m_applyButton, &QPushButton::clicked, this, &GameSaveDock::onApply);

	onBroadcastSelectionChanged(m_broadcastCombo->currentIndex());
	refreshBroadcastList();
}

GameSaveDock::~GameSaveDock() {}

void GameSaveDock::refreshBroadcastList()
{
	if (m_ytClient->isAuthenticated()) {
		m_ytClient->listUpcomingBroadcasts();
	} else {
		// Keep only "Schedule broadcast"
		while (m_broadcastCombo->count() > 1) {
			m_broadcastCombo->removeItem(m_broadcastCombo->count() - 1);
		}
		m_broadcastCombo->setCurrentIndex(kComboIndexScheduleBroadcast);
	}
}

void GameSaveDock::onBroadcastSelectionChanged(int index)
{
	if (index == kComboIndexScheduleBroadcast) {
		m_loadOrNewButton->setText(tr("New"));
	} else {
		m_loadOrNewButton->setText(tr("Load"));
	}
}

void GameSaveDock::onLoadOrNewClicked()
{
	int idx = m_broadcastCombo->currentIndex();
	if (idx == kComboIndexScheduleBroadcast) {
		ScheduleBroadcastDialog dlg(this);
		dlg.setTournamentName(tournamentName());
		if (dlg.exec() != QDialog::Accepted) {
			return;
		}
		QString title = dlg.broadcastTitle();
		QString description = dlg.description();
		QDateTime scheduled = dlg.scheduledStartTime();
		QString tournament = dlg.tournamentName();
		QString privacy = dlg.privacyStatus();
		bool madeForKids = dlg.selfDeclaredMadeForKids();
		QString thumbnailPath = dlg.thumbnailPath();
		if (title.isEmpty()) {
			setStatus(tr("Enter a broadcast title."));
			return;
		}
		setTournamentName(tournament);
		m_ytClient->createBroadcast(title, description, scheduled, privacy, madeForKids);
		connect(m_ytClient, &YouTubeApiClient::broadcastCreated, this, [this, thumbnailPath](const QString &broadcastId) {
			disconnect(m_ytClient, &YouTubeApiClient::broadcastCreated, this, nullptr);
			m_ytClient->ensureStreamBound(broadcastId);
			connect(m_ytClient, &YouTubeApiClient::streamBound, this, [this, broadcastId, thumbnailPath]() {
				disconnect(m_ytClient, &YouTubeApiClient::streamBound, this, nullptr);
				if (!thumbnailPath.isEmpty()) {
					connect(m_ytClient, &YouTubeApiClient::thumbnailSet, this, [this]() {
						disconnect(m_ytClient, &YouTubeApiClient::thumbnailSet, this, nullptr);
						disconnect(m_ytClient, &YouTubeApiClient::error, this, nullptr);
						setStatus(tr("Broadcast scheduled. Refreshing list."));
						refreshBroadcastList();
					});
					connect(m_ytClient, &YouTubeApiClient::error, this, [this](const QString &) {
						disconnect(m_ytClient, &YouTubeApiClient::thumbnailSet, this, nullptr);
						disconnect(m_ytClient, &YouTubeApiClient::error, this, nullptr);
						setStatus(tr("Broadcast scheduled (thumbnail failed). Refreshing list."));
						refreshBroadcastList();
					});
					m_ytClient->setBroadcastThumbnail(broadcastId, thumbnailPath);
				} else {
					setStatus(tr("Broadcast scheduled. Refreshing list."));
					refreshBroadcastList();
				}
			});
		});
		return;
	}

	QString broadcastId = m_broadcastCombo->itemData(idx, kComboDataBroadcastId).toString();
	QString boundStreamId = m_broadcastCombo->itemData(idx, kComboDataBoundStreamId).toString();
	QString title = m_broadcastCombo->itemData(idx, kComboDataTitle).toString();

	YouTubeBroadcast b;
	b.id = broadcastId;
	b.boundStreamId = boundStreamId;
	b.title = title;

	LoadBroadcastDialog loadDlg(b, this);
	if (loadDlg.exec() != QDialog::Accepted) {
		return;
	}

	LoadBroadcastDialog::Result choice = loadDlg.choice();
	if (choice == LoadBroadcastDialog::Result::Cancel) {
		return;
	}

	m_loadAndStartRequested = (choice == LoadBroadcastDialog::Result::LoadAndStart);
	m_pendingBroadcastTitle = title;

	connect(m_ytClient, &YouTubeApiClient::streamKeyReceived, this, &GameSaveDock::onStreamKeyReceived);
	m_ytClient->fetchStreamKey(broadcastId, boundStreamId);
}

void GameSaveDock::onSetupWeekendClicked()
{
	TournamentWeekendDialog dlg(m_ytClient, this);
	connect(&dlg, &TournamentWeekendDialog::refreshRequested, this, &GameSaveDock::refreshBroadcastList);
	dlg.exec();
	if (dlg.broadcastsCreated()) {
		refreshBroadcastList();
	}
}

void GameSaveDock::onBroadcastsListed(const QList<YouTubeBroadcast> &broadcasts)
{
	QString currentId = m_broadcastCombo->itemData(m_broadcastCombo->currentIndex(), kComboDataBroadcastId).toString();
	while (m_broadcastCombo->count() > 1) {
		m_broadcastCombo->removeItem(m_broadcastCombo->count() - 1);
	}
	for (const YouTubeBroadcast &b : broadcasts) {
		QString display = b.title.isEmpty() ? b.id : b.title;
		if (b.scheduledStartTime.isValid()) {
			display += " (" + QLocale().toString(b.scheduledStartTime, QLocale::ShortFormat) + ")";
		}
		int idx = m_broadcastCombo->count();
		m_broadcastCombo->addItem(display);
		m_broadcastCombo->setItemData(idx, b.id, kComboDataBroadcastId);
		m_broadcastCombo->setItemData(idx, b.boundStreamId, kComboDataBoundStreamId);
		m_broadcastCombo->setItemData(idx, b.title, kComboDataTitle);
	}
	// Restore selection if it was a broadcast
	if (!currentId.isEmpty()) {
		for (int i = 1; i < m_broadcastCombo->count(); ++i) {
			if (m_broadcastCombo->itemData(i, kComboDataBroadcastId).toString() == currentId) {
				m_broadcastCombo->setCurrentIndex(i);
				break;
			}
		}
	}
	onBroadcastSelectionChanged(m_broadcastCombo->currentIndex());
}

void GameSaveDock::onStreamKeyReceived(const YouTubeStreamKey &key)
{
	disconnect(m_ytClient, &YouTubeApiClient::streamKeyReceived, this, nullptr);

	// YouTube RTMPS: ingestionAddress is often the full URL; streamName is the key.
	// OBS typically wants server URL (e.g. rtmp://a.rtmp.youtube.com/live2) and key separately.
	QString serverUrl = key.ingestionAddress;
	if (serverUrl.isEmpty()) {
		serverUrl = QStringLiteral("rtmps://a.rtmp.youtube.com/live2");
	}
	applyStreamKeyToOBS(key.streamKey, serverUrl, m_pendingBroadcastTitle);
	m_tournamentNameEdit->setText(m_pendingBroadcastTitle);

	if (m_loadAndStartRequested) {
		startStreamIfRequested();
	}
	setStatus(tr("Loaded broadcast: %1").arg(m_pendingBroadcastTitle));
}

void GameSaveDock::onYtError(const QString &message)
{
	setStatus(message);
	QMessageBox::warning(this, tr("YouTube"), message);
}

void GameSaveDock::applyStreamKeyToOBS(const QString &streamKey, const QString &serverUrl, const QString &broadcastTitle)
{
	obs_service_t *service = obs_frontend_get_streaming_service();
	if (!service) {
		setStatus(tr("No streaming service in OBS."));
		return;
	}

	obs_data_t *settings = obs_service_get_settings(service);
	if (!settings) {
		setStatus(tr("Could not get service settings."));
		return;
	}

	obs_data_set_string(settings, "key", streamKey.toUtf8().constData());
	obs_data_set_string(settings, "server", serverUrl.toUtf8().constData());
	obs_service_update(service, settings);
	obs_data_release(settings);

	obs_frontend_save_streaming_service();

	config_t *config = obs_frontend_get_profile_config();
	if (config) {
		config_set_string(config, "YouTube", "Title", broadcastTitle.toUtf8().constData());
		config_save(config);
	}
}

void GameSaveDock::startStreamIfRequested()
{
	if (m_loadAndStartRequested) {
		obs_frontend_streaming_start();
		m_loadAndStartRequested = false;
	}
}

QString GameSaveDock::tournamentName() const
{
	return m_tournamentNameEdit->text().trimmed();
}

void GameSaveDock::setTournamentName(const QString &name)
{
	m_tournamentNameEdit->setText(name);
}

QString GameSaveDock::basePath() const
{
	return m_basePathEdit->text().trimmed();
}

bool GameSaveDock::autoOrganize() const
{
	return m_autoOrganizeCheck->isChecked();
}

void GameSaveDock::setStatus(const QString &text)
{
	m_statusLabel->setText(tr("Status: %1").arg(text));
}

void GameSaveDock::onReadFromYouTube()
{
	config_t *config = obs_frontend_get_profile_config();
	if (!config) {
		setStatus(tr("Could not get profile config"));
		return;
	}
	const char *ytTitle = config_get_string(config, "YouTube", "Title");
	if (ytTitle && *ytTitle) {
		m_tournamentNameEdit->setText(QString::fromUtf8(ytTitle));
		setStatus(tr("Loaded from YouTube"));
	} else {
		setStatus(tr("No YouTube title in config (set in Manage Broadcast with Remember Settings)"));
	}
}

void GameSaveDock::onBrowseBasePath()
{
	QString start = m_basePathEdit->text().trimmed();
	if (start.isEmpty()) {
		start = QDir::homePath();
	}
	QString dir = QFileDialog::getExistingDirectory(this, tr("Base Recording Directory"), start);
	if (!dir.isEmpty()) {
		m_basePathEdit->setText(dir);
	}
}

void GameSaveDock::onApply()
{
	QString base = basePath();
	QString tournament = tournamentName();
	if (base.isEmpty() || tournament.isEmpty()) {
		setStatus(tr("Set Tournament Name and Base Path first"));
		return;
	}

	config_t *config = obs_frontend_get_profile_config();
	if (!config) {
		setStatus(tr("Could not get profile config"));
		return;
	}

	QString newPath = QDir(base).filePath(tournament + "/%A");
	QByteArray pathUtf8 = newPath.toUtf8();
	const char *pathStr = pathUtf8.constData();

	const char *mode = config_get_string(config, "Output", "Mode");
	if (mode && strcmp(mode, "Advanced") == 0) {
		config_set_string(config, "AdvOut", "RecFilePath", pathStr);
	} else {
		config_set_string(config, "SimpleOutput", "FilePath", pathStr);
	}
	config_save(config);

	setStatus(tr("Path set to %1").arg(newPath));
}

void GameSaveDock::onRecordingStopped()
{
	if (!autoOrganize()) {
		return;
	}

	QTimer::singleShot(2000, this, [this]() {
		char *path = obs_frontend_get_last_recording();
		if (!path) {
			setStatus(tr("Could not get last recording path"));
			return;
		}

		QString srcPath = QString::fromUtf8(path);
		bfree(path);

		QFileInfo fi(srcPath);
		if (!fi.exists()) {
			setStatus(tr("Recording file not found: %1").arg(srcPath));
			return;
		}

		QString dayName = fi.lastModified().toString("dddd");
		QString tournament = tournamentName();
		QString base = basePath();
		if (tournament.isEmpty() || base.isEmpty()) {
			setStatus(tr("Set Tournament Name and Base Path for auto-organize"));
			return;
		}

		QString destDir = QDir(base).filePath(tournament + "/" + dayName);
		QDir().mkpath(destDir);

		QString destPath = QDir(destDir).filePath(fi.fileName());
		if (QFile::exists(destPath)) {
			setStatus(tr("Destination already exists: %1").arg(destPath));
			return;
		}
		if (!QFile::rename(srcPath, destPath)) {
			setStatus(tr("Failed to move to %1").arg(destPath));
			return;
		}

		setStatus(tr("Moved to %1").arg(destPath));
	});
}
