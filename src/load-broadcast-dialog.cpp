/*
GameSave - OBS Livestream Recording Organizer
Copyright (C) 2026

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#include "load-broadcast-dialog.hpp"
#include "youtube-client.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>

LoadBroadcastDialog::LoadBroadcastDialog(const YouTubeBroadcast &broadcast, QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle(tr("Load YouTube Broadcast"));
	setModal(true);

	QVBoxLayout *layout = new QVBoxLayout(this);

	QLabel *info = new QLabel(tr("Load this broadcast into OBS stream settings?"), this);
	layout->addWidget(info);

	m_statusLabel = new QLabel(broadcast.title.isEmpty() ? broadcast.id : broadcast.title, this);
	m_statusLabel->setWordWrap(true);
	m_statusLabel->setStyleSheet("font-weight: bold;");
	layout->addWidget(m_statusLabel);

	QHBoxLayout *btnLayout = new QHBoxLayout();
	m_loadButton = new QPushButton(tr("Load"), this);
	m_loadAndStartButton = new QPushButton(tr("Load and Start Stream"), this);
	QPushButton *cancelButton = new QPushButton(tr("Cancel"), this);

	btnLayout->addWidget(m_loadButton);
	btnLayout->addWidget(m_loadAndStartButton);
	btnLayout->addStretch();
	btnLayout->addWidget(cancelButton);
	layout->addLayout(btnLayout);

	connect(m_loadButton, &QPushButton::clicked, this, &LoadBroadcastDialog::onLoad);
	connect(m_loadAndStartButton, &QPushButton::clicked, this, &LoadBroadcastDialog::onLoadAndStart);
	connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

LoadBroadcastDialog::~LoadBroadcastDialog() = default;

void LoadBroadcastDialog::onLoad()
{
	m_choice = Result::Load;
	accept();
}

void LoadBroadcastDialog::onLoadAndStart()
{
	m_choice = Result::LoadAndStart;
	accept();
}
