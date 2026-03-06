/*
GameSave - OBS Livestream Recording Organizer
Copyright (C) 2026

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#include "schedule-broadcast-dialog.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QGroupBox>
#include <QComboBox>
#include <QCheckBox>
#include <QFileDialog>

ScheduleBroadcastDialog::ScheduleBroadcastDialog(QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle(tr("Schedule YouTube Broadcast"));
	setModal(true);

	QVBoxLayout *layout = new QVBoxLayout(this);

	QGroupBox *ytGroup = new QGroupBox(tr("YouTube broadcast"), this);
	QFormLayout *ytForm = new QFormLayout(ytGroup);

	m_titleEdit = new QLineEdit(this);
	m_titleEdit->setPlaceholderText(tr("e.g. Spring Tournament 2026 - Day 1"));
	ytForm->addRow(tr("Title:"), m_titleEdit);

	m_descriptionEdit = new QTextEdit(this);
	m_descriptionEdit->setPlaceholderText(tr("Optional description"));
	m_descriptionEdit->setMaximumHeight(80);
	ytForm->addRow(tr("Description:"), m_descriptionEdit);

	m_scheduledEdit = new QDateTimeEdit(this);
	m_scheduledEdit->setCalendarPopup(true);
	m_scheduledEdit->setDateTime(QDateTime::currentDateTime().addSecs(3600));
	m_scheduledEdit->setDisplayFormat(tr("yyyy-MM-dd hh:mm"));
	ytForm->addRow(tr("Scheduled start:"), m_scheduledEdit);

	m_privacyCombo = new QComboBox(this);
	m_privacyCombo->addItem(tr("Public"), QStringLiteral("public"));
	m_privacyCombo->addItem(tr("Unlisted"), QStringLiteral("unlisted"));
	m_privacyCombo->addItem(tr("Private"), QStringLiteral("private"));
	ytForm->addRow(tr("Privacy:"), m_privacyCombo);

	m_madeForKidsCheck = new QCheckBox(tr("Made for kids"), this);
	m_madeForKidsCheck->setChecked(false);
	ytForm->addRow(QString(), m_madeForKidsCheck);

	m_thumbnailEdit = new QLineEdit(this);
	m_thumbnailEdit->setPlaceholderText(tr("Optional thumbnail image (JPEG or PNG, max 2 MB)"));
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
	ytForm->addRow(tr("Thumbnail:"), thumbRow);

	layout->addWidget(ytGroup);

	QGroupBox *gsGroup = new QGroupBox(tr("GameSave"), this);
	QFormLayout *gsForm = new QFormLayout(gsGroup);

	m_tournamentEdit = new QLineEdit(this);
	m_tournamentEdit->setPlaceholderText(tr("e.g. Spring Tournament 2026"));
	gsForm->addRow(tr("Tournament Name:"), m_tournamentEdit);

	layout->addWidget(gsGroup);

	m_statusLabel = new QLabel(this);
	m_statusLabel->setWordWrap(true);
	m_statusLabel->setStyleSheet("color: gray;");
	layout->addWidget(m_statusLabel);

	QHBoxLayout *btnLayout = new QHBoxLayout();
	btnLayout->addStretch();
	QPushButton *createBtn = new QPushButton(tr("Schedule Broadcast"), this);
	QPushButton *cancelBtn = new QPushButton(tr("Cancel"), this);
	btnLayout->addWidget(createBtn);
	btnLayout->addWidget(cancelBtn);
	layout->addLayout(btnLayout);

	connect(createBtn, &QPushButton::clicked, this, [this]() {
		if (m_titleEdit->text().trimmed().isEmpty()) {
			m_statusLabel->setText(tr("Enter a broadcast title."));
			return;
		}
		accept();
	});
	connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

	// Pre-fill tournament from title when title changes
	connect(m_titleEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
		if (m_tournamentEdit->text().trimmed().isEmpty() || m_tournamentEdit->text() == m_titleEdit->text()) {
			m_tournamentEdit->setText(text);
		}
	});
}

ScheduleBroadcastDialog::~ScheduleBroadcastDialog() = default;

QString ScheduleBroadcastDialog::broadcastTitle() const
{
	return m_titleEdit->text().trimmed();
}

QString ScheduleBroadcastDialog::description() const
{
	return m_descriptionEdit->toPlainText().trimmed();
}

QDateTime ScheduleBroadcastDialog::scheduledStartTime() const
{
	return m_scheduledEdit->dateTime();
}

QString ScheduleBroadcastDialog::tournamentName() const
{
	return m_tournamentEdit->text().trimmed();
}

QString ScheduleBroadcastDialog::privacyStatus() const
{
	return m_privacyCombo->currentData().toString();
}

bool ScheduleBroadcastDialog::selfDeclaredMadeForKids() const
{
	return m_madeForKidsCheck->isChecked();
}

QString ScheduleBroadcastDialog::thumbnailPath() const
{
	return m_thumbnailEdit->text().trimmed();
}

void ScheduleBroadcastDialog::setTournamentName(const QString &name)
{
	m_tournamentEdit->setText(name);
}
