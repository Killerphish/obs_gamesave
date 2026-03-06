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
#include <QLineEdit>
#include <QTextEdit>
#include <QDateTimeEdit>
#include <QLabel>

class ScheduleBroadcastDialog : public QDialog {
	Q_OBJECT

public:
	explicit ScheduleBroadcastDialog(QWidget *parent = nullptr);
	~ScheduleBroadcastDialog() override;

	QString broadcastTitle() const;
	QString description() const;
	QDateTime scheduledStartTime() const;
	QString tournamentName() const;

	void setTournamentName(const QString &name);

private:
	QLineEdit *m_titleEdit = nullptr;
	QTextEdit *m_descriptionEdit = nullptr;
	QDateTimeEdit *m_scheduledEdit = nullptr;
	QLineEdit *m_tournamentEdit = nullptr;
	QLabel *m_statusLabel = nullptr;
};
