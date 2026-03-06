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
#include <QPushButton>
#include <QLabel>

struct YouTubeBroadcast;

class LoadBroadcastDialog : public QDialog {
	Q_OBJECT

public:
	enum class Result { Cancel, Load, LoadAndStart };

	explicit LoadBroadcastDialog(const YouTubeBroadcast &broadcast, QWidget *parent = nullptr);
	~LoadBroadcastDialog() override;

	Result choice() const { return m_choice; }

private:
	void onLoad();
	void onLoadAndStart();

	Result m_choice = Result::Cancel;
	QPushButton *m_loadButton = nullptr;
	QPushButton *m_loadAndStartButton = nullptr;
	QLabel *m_statusLabel = nullptr;
};
