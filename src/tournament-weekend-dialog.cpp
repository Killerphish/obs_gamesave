/*
GameSave - OBS Livestream Recording Organizer
Copyright (C) 2026

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#include "tournament-weekend-dialog.hpp"
#include "youtube-client.hpp"
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QTabWidget>
#include <QSpinBox>
#include <QDateTimeEdit>
#include <QCheckBox>
#include <QLabel>
#include <QComboBox>
#include <QFileDialog>

static const char *kConfigSection = "TournamentSetup";
static const int kMinGames = 1;
static const int kMaxGames = 20;

TournamentWeekendDialog::TournamentWeekendDialog(YouTubeApiClient *ytClient, QWidget *parent)
	: QDialog(parent)
	, m_ytClient(ytClient)
{
	setWindowTitle(tr("Setup Weekend Streams"));
	setModal(true);

	QVBoxLayout *layout = new QVBoxLayout(this);

	QGroupBox *topGroup = new QGroupBox(tr("Tournament"), this);
	QFormLayout *topForm = new QFormLayout(topGroup);

	m_tournamentEdit = new QLineEdit(this);
	m_tournamentEdit->setPlaceholderText(tr("e.g. Spring Tournament 2026"));
	topForm->addRow(tr("Tournament name:"), m_tournamentEdit);

	m_teamNameEdit = new QLineEdit(this);
	m_teamNameEdit->setPlaceholderText(tr("Your team name"));
	topForm->addRow(tr("Your team name:"), m_teamNameEdit);

	m_gameCountSpin = new QSpinBox(this);
	m_gameCountSpin->setRange(kMinGames, kMaxGames);
	m_gameCountSpin->setValue(5);
	m_gameCountSpin->setMinimumWidth(80);
	m_gameCountSpin->setMinimumHeight(28);
	topForm->addRow(tr("Number of games:"), m_gameCountSpin);

	m_privacyCombo = new QComboBox(this);
	m_privacyCombo->addItem(tr("Public"), QStringLiteral("public"));
	m_privacyCombo->addItem(tr("Unlisted"), QStringLiteral("unlisted"));
	m_privacyCombo->addItem(tr("Private"), QStringLiteral("private"));
	topForm->addRow(tr("Privacy:"), m_privacyCombo);

	m_madeForKidsCheck = new QCheckBox(tr("Made for kids"), this);
	m_madeForKidsCheck->setChecked(false);
	topForm->addRow(QString(), m_madeForKidsCheck);

	m_thumbnailEdit = new QLineEdit(this);
	m_thumbnailEdit->setPlaceholderText(tr("Optional thumbnail (JPEG or PNG, max 2 MB) for all games"));
	m_thumbnailEdit->setClearButtonEnabled(true);
	QHBoxLayout *thumbRow = new QHBoxLayout();
	thumbRow->addWidget(m_thumbnailEdit);
	QPushButton *browseBtn = new QPushButton(tr("Browse…"), this);
	connect(browseBtn, &QPushButton::clicked, this, [this]() {
		QString path = QFileDialog::getOpenFileName(this, tr("Select thumbnail image"), QString(),
							    tr("Images (*.png *.jpg *.jpeg);;All files (*)"));
		if (!path.isEmpty()) {
			m_thumbnailEdit->setText(path);
		}
	});
	thumbRow->addWidget(browseBtn);
	topForm->addRow(tr("Thumbnail:"), thumbRow);

	layout->addWidget(topGroup);

	m_tabWidget = new QTabWidget(this);
	layout->addWidget(m_tabWidget);

	connect(m_gameCountSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &TournamentWeekendDialog::rebuildGameTabs);
	connect(m_tournamentEdit, &QLineEdit::textChanged, this, [this]() {
		for (int i = 0; i < m_gameWidgets.size(); ++i) updatePreviewLabels(i);
	});
	connect(m_teamNameEdit, &QLineEdit::textChanged, this, [this]() {
		for (int i = 0; i < m_gameWidgets.size(); ++i) updatePreviewLabels(i);
	});

	m_rememberCheck = new QCheckBox(tr("Remember these settings"), this);
	m_rememberCheck->setChecked(true);
	layout->addWidget(m_rememberCheck);

	m_statusLabel = new QLabel(this);
	m_statusLabel->setWordWrap(true);
	m_statusLabel->setStyleSheet("color: gray;");
	layout->addWidget(m_statusLabel);

	QHBoxLayout *btnLayout = new QHBoxLayout();
	btnLayout->addStretch();
	m_createAllButton = new QPushButton(tr("Create all"), this);
	QPushButton *cancelBtn = new QPushButton(tr("Cancel"), this);
	btnLayout->addWidget(m_createAllButton);
	btnLayout->addWidget(cancelBtn);
	layout->addLayout(btnLayout);

	connect(m_createAllButton, &QPushButton::clicked, this, &TournamentWeekendDialog::startCreateAll);
	connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

	rebuildGameTabs();
	loadRememberedSettings();
}

TournamentWeekendDialog::~TournamentWeekendDialog() = default;

void TournamentWeekendDialog::loadRememberedSettings()
{
	config_t *config = obs_frontend_get_profile_config();
	if (!config) {
		return;
	}

	const char *tournament = config_get_string(config, kConfigSection, "TournamentName");
	if (tournament && *tournament) {
		m_tournamentEdit->setText(QString::fromUtf8(tournament));
	}

	const char *team = config_get_string(config, kConfigSection, "TeamName");
	if (team && *team) {
		m_teamNameEdit->setText(QString::fromUtf8(team));
	}

	int count = config_get_int(config, kConfigSection, "GameCount");
	if (count >= kMinGames && count <= kMaxGames) {
		m_gameCountSpin->setValue(count);
	}

	// Rebuild may have run with old count; ensure tabs match and load per-game data
	int n = m_gameCountSpin->value();
	for (int i = 0; i < n && i < m_gameWidgets.size(); ++i) {
		QString keySuffix = QString::number(i);
		const char *opp = config_get_string(config, kConfigSection, ("Opponent_" + keySuffix.toUtf8()).constData());
		if (opp && *opp) {
			m_gameWidgets[i].opponentEdit->setText(QString::fromUtf8(opp));
		}
		const char *timeStr = config_get_string(config, kConfigSection, ("ScheduledTime_" + keySuffix.toUtf8()).constData());
		if (timeStr && *timeStr) {
			QDateTime dt = QDateTime::fromString(QString::fromUtf8(timeStr), Qt::ISODate);
			if (dt.isValid()) {
				m_gameWidgets[i].scheduledEdit->setDateTime(dt);
			}
		}
		const char *rink = config_get_string(config, kConfigSection, ("Rink_" + keySuffix.toUtf8()).constData());
		if (rink && *rink) {
			m_gameWidgets[i].rinkEdit->setText(QString::fromUtf8(rink));
		}
	}
	for (int i = 0; i < m_gameWidgets.size(); ++i) {
		updatePreviewLabels(i);
	}
}

void TournamentWeekendDialog::saveRememberedSettings()
{
	if (!m_rememberCheck->isChecked()) {
		return;
	}

	config_t *config = obs_frontend_get_profile_config();
	if (!config) {
		return;
	}

	config_set_string(config, kConfigSection, "TournamentName", m_tournamentEdit->text().trimmed().toUtf8().constData());
	config_set_string(config, kConfigSection, "TeamName", m_teamNameEdit->text().trimmed().toUtf8().constData());
	config_set_int(config, kConfigSection, "GameCount", m_gameCountSpin->value());

	int n = m_gameCountSpin->value();
	for (int i = 0; i < n && i < m_gameWidgets.size(); ++i) {
		QString keySuffix = QString::number(i);
		config_set_string(config, kConfigSection, ("Opponent_" + keySuffix.toUtf8()).constData(),
				  m_gameWidgets[i].opponentEdit->text().trimmed().toUtf8().constData());
		config_set_string(config, kConfigSection, ("ScheduledTime_" + keySuffix.toUtf8()).constData(),
				  m_gameWidgets[i].scheduledEdit->dateTime().toUTC().toString(Qt::ISODate).toUtf8().constData());
		config_set_string(config, kConfigSection, ("Rink_" + keySuffix.toUtf8()).constData(),
				  m_gameWidgets[i].rinkEdit->text().trimmed().toUtf8().constData());
	}

	config_save(config);
}

void TournamentWeekendDialog::rebuildGameTabs()
{
	// Disconnect any signals from old widgets before clearing
	m_tabWidget->clear();
	m_gameWidgets.clear();

	int n = m_gameCountSpin->value();
	for (int i = 0; i < n; ++i) {
		QWidget *tab = makeGameTab(i);
		m_tabWidget->addTab(tab, tr("Game %1").arg(i + 1));
	}
}

QWidget *TournamentWeekendDialog::makeGameTab(int index)
{
	QWidget *w = new QWidget(this);
	QFormLayout *form = new QFormLayout(w);

	GameTabWidgets gw;
	gw.opponentEdit = new QLineEdit(w);
	gw.opponentEdit->setPlaceholderText(tr("Opponent team name"));
	form->addRow(tr("Opponent:"), gw.opponentEdit);

	gw.scheduledEdit = new QDateTimeEdit(w);
	gw.scheduledEdit->setCalendarPopup(true);
	gw.scheduledEdit->setDateTime(QDateTime::currentDateTime().addSecs(3600 * (index + 1)));
	gw.scheduledEdit->setDisplayFormat(tr("yyyy-MM-dd hh:mm"));
	form->addRow(tr("Game time:"), gw.scheduledEdit);

	gw.rinkEdit = new QLineEdit(w);
	gw.rinkEdit->setPlaceholderText(tr("Rink name"));
	form->addRow(tr("Rink:"), gw.rinkEdit);

	gw.titlePreview = new QLabel(w);
	gw.titlePreview->setStyleSheet("color: gray; font-style: italic;");
	gw.titlePreview->setWordWrap(true);
	form->addRow(tr("YouTube title:"), gw.titlePreview);

	gw.descPreview = new QLabel(w);
	gw.descPreview->setStyleSheet("color: gray; font-style: italic;");
	gw.descPreview->setWordWrap(true);
	form->addRow(tr("Description:"), gw.descPreview);

	m_gameWidgets.append(gw);

	auto updatePreview = [this, index]() { updatePreviewLabels(index); };
	connect(gw.opponentEdit, &QLineEdit::textChanged, this, updatePreview);
	connect(gw.rinkEdit, &QLineEdit::textChanged, this, updatePreview);

	updatePreviewLabels(index);
	return w;
}

void TournamentWeekendDialog::updatePreviewLabels(int index)
{
	if (index < 0 || index >= m_gameWidgets.size()) {
		return;
	}
	QString team = m_teamNameEdit->text().trimmed();
	QString opponent = m_gameWidgets[index].opponentEdit->text().trimmed();
	QString tournament = m_tournamentEdit->text().trimmed();
	QString rink = m_gameWidgets[index].rinkEdit->text().trimmed();

	QString title = team.isEmpty() || opponent.isEmpty() ? tr("—") : (team + " vs " + opponent);
	QString desc = tournament.isEmpty() && rink.isEmpty() ? tr("—") : (tournament + (rink.isEmpty() ? "" : " - " + rink));

	m_gameWidgets[index].titlePreview->setText(title);
	m_gameWidgets[index].descPreview->setText(desc);
}

bool TournamentWeekendDialog::validateAndCollect(QList<WeekendGameEntry> *out)
{
	out->clear();
	QString tournament = m_tournamentEdit->text().trimmed();
	QString team = m_teamNameEdit->text().trimmed();
	if (tournament.isEmpty()) {
		m_statusLabel->setText(tr("Enter a tournament name."));
		return false;
	}
	if (team.isEmpty()) {
		m_statusLabel->setText(tr("Enter your team name."));
		return false;
	}

	for (int i = 0; i < m_gameWidgets.size(); ++i) {
		WeekendGameEntry e;
		e.opponent = m_gameWidgets[i].opponentEdit->text().trimmed();
		e.scheduledTime = m_gameWidgets[i].scheduledEdit->dateTime();
		e.rinkName = m_gameWidgets[i].rinkEdit->text().trimmed();
		if (e.opponent.isEmpty()) {
			m_statusLabel->setText(tr("Enter opponent for Game %1.").arg(i + 1));
			return false;
		}
		if (!e.scheduledTime.isValid()) {
			m_statusLabel->setText(tr("Set game time for Game %1.").arg(i + 1));
			return false;
		}
		out->append(e);
	}
	return true;
}

void TournamentWeekendDialog::startCreateAll()
{
	if (!m_ytClient->isAuthenticated()) {
		m_statusLabel->setText(tr("Sign in with YouTube first."));
		QMessageBox::warning(this, tr("YouTube"), tr("Sign in with YouTube in the GameSave dock first."));
		return;
	}

	QList<WeekendGameEntry> entries;
	if (!validateAndCollect(&entries)) {
		return;
	}

	m_createQueue = entries;
	m_createIndex = 0;
	m_broadcastsCreated = false;
	m_createAllButton->setEnabled(false);
	m_statusLabel->setText(tr("Creating 1/%1…").arg(m_createQueue.size()));

	QString tournament = m_tournamentEdit->text().trimmed();
	QString team = m_teamNameEdit->text().trimmed();
	QString privacy = m_privacyCombo->currentData().toString();
	bool madeForKids = m_madeForKidsCheck->isChecked();

	const WeekendGameEntry &e = m_createQueue[m_createIndex];
	QString title = team + " vs " + e.opponent;
	QString description = e.rinkName.isEmpty() ? tournament : (tournament + " - " + e.rinkName);

	m_createdConn = connect(m_ytClient, &YouTubeApiClient::broadcastCreated, this, &TournamentWeekendDialog::onBroadcastCreated);
	m_boundConn = connect(m_ytClient, &YouTubeApiClient::streamBound, this, &TournamentWeekendDialog::onStreamBound);
	m_errorConn = connect(m_ytClient, &YouTubeApiClient::error, this, &TournamentWeekendDialog::onYtError);

	m_ytClient->createBroadcast(title, description, e.scheduledTime, privacy, madeForKids);
}

void TournamentWeekendDialog::onBroadcastCreated(const QString &broadcastId)
{
	disconnect(m_createdConn);
	m_currentBroadcastId = broadcastId;
	m_ytClient->ensureStreamBound(broadcastId);
}

void TournamentWeekendDialog::onStreamBound()
{
	disconnect(m_boundConn);
	QString thumbnailPath = m_thumbnailEdit->text().trimmed();
	if (!thumbnailPath.isEmpty() && !m_currentBroadcastId.isEmpty()) {
		m_thumbnailSetConn = connect(m_ytClient, &YouTubeApiClient::thumbnailSet, this, &TournamentWeekendDialog::onThumbnailDone);
		m_thumbnailErrorConn = connect(m_ytClient, &YouTubeApiClient::error, this, &TournamentWeekendDialog::onThumbnailDone);
		m_ytClient->setBroadcastThumbnail(m_currentBroadcastId, thumbnailPath);
		return;
	}
	advanceToNextOrFinish();
}

void TournamentWeekendDialog::onThumbnailDone()
{
	disconnect(m_thumbnailSetConn);
	disconnect(m_thumbnailErrorConn);
	advanceToNextOrFinish();
}

void TournamentWeekendDialog::advanceToNextOrFinish()
{
	m_broadcastsCreated = true;
	++m_createIndex;

	if (m_createIndex >= m_createQueue.size()) {
		disconnect(m_errorConn);
		saveRememberedSettings();
		m_createAllButton->setEnabled(true);
		m_statusLabel->setText(tr("All %1 broadcasts created. Refreshing list.").arg(m_createQueue.size()));
		emit refreshRequested();
		QMessageBox::information(this, tr("Weekend streams"), tr("All %1 broadcasts were created. They will appear in the broadcast list.").arg(m_createQueue.size()));
		accept();
		return;
	}

	m_statusLabel->setText(tr("Creating %1/%2…").arg(m_createIndex + 1).arg(m_createQueue.size()));

	QString tournament = m_tournamentEdit->text().trimmed();
	QString team = m_teamNameEdit->text().trimmed();
	QString privacy = m_privacyCombo->currentData().toString();
	bool madeForKids = m_madeForKidsCheck->isChecked();
	const WeekendGameEntry &e = m_createQueue[m_createIndex];
	QString title = team + " vs " + e.opponent;
	QString description = e.rinkName.isEmpty() ? tournament : (tournament + " - " + e.rinkName);

	m_createdConn = connect(m_ytClient, &YouTubeApiClient::broadcastCreated, this, &TournamentWeekendDialog::onBroadcastCreated);
	m_boundConn = connect(m_ytClient, &YouTubeApiClient::streamBound, this, &TournamentWeekendDialog::onStreamBound);

	m_ytClient->createBroadcast(title, description, e.scheduledTime, privacy, madeForKids);
}

void TournamentWeekendDialog::onYtError(const QString &message)
{
	disconnect(m_createdConn);
	disconnect(m_boundConn);
	disconnect(m_errorConn);
	disconnect(m_thumbnailSetConn);
	disconnect(m_thumbnailErrorConn);
	m_createAllButton->setEnabled(true);
	m_statusLabel->setText(tr("Error: %1").arg(message));
	QMessageBox::warning(this, tr("YouTube"), message);
}
