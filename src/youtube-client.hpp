/*
GameSave - OBS Livestream Recording Organizer
Copyright (C) 2026

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QList>
#include <QNetworkAccessManager>
#include <QTcpServer>

struct YouTubeBroadcast {
	QString id;
	QString title;
	QString boundStreamId;
	QDateTime scheduledStartTime;
};

struct YouTubeStreamKey {
	QString streamKey;
	QString ingestionAddress;
	QString backupIngestionAddress;
};

class YouTubeApiClient : public QObject {
	Q_OBJECT

public:
	explicit YouTubeApiClient(QObject *parent = nullptr);
	~YouTubeApiClient() override;

	bool isAuthenticated() const;
	QString authError() const { return m_authError; }

	// OAuth: call startAuth(), user completes in browser, authFinished(bool) is emitted.
	void startAuth();
	void signOut();

	// List upcoming (and optionally active) broadcasts. Emits broadcastsListed(QList<YouTubeBroadcast>).
	void listUpcomingBroadcasts();

	// Get stream key for a broadcast (uses boundStreamId). Emits streamKeyReceived(YouTubeStreamKey) or error(QString).
	void fetchStreamKey(const QString &broadcastId, const QString &boundStreamId);

	// Create a new broadcast. Emits broadcastCreated(QString broadcastId) or error(QString).
	// privacyStatus: "public", "unlisted", or "private".
	void createBroadcast(const QString &title, const QString &description, const QDateTime &scheduledStartTime,
			    const QString &privacyStatus = QString("public"), bool selfDeclaredMadeForKids = false);

	// After createBroadcast, bind a stream (create stream if needed). Emits streamBound() or error(QString).
	void ensureStreamBound(const QString &broadcastId);

	// Set custom thumbnail for a broadcast (videoId). Emits thumbnailSet() or error(QString). Image: JPEG or PNG, max 2MB.
	void setBroadcastThumbnail(const QString &videoId, const QString &imagePath);

	// Set OAuth client ID (from config or build). Required before startAuth().
	void setClientId(const QString &clientId) { m_clientId = clientId; }
	void setClientSecret(const QString &clientSecret) { m_clientSecret = clientSecret; }

Q_SIGNALS:
	void authFinished(bool success);
	void broadcastsListed(const QList<YouTubeBroadcast> &broadcasts);
	void streamKeyReceived(const YouTubeStreamKey &key);
	void broadcastCreated(const QString &broadcastId);
	void streamBound();
	void thumbnailSet();
	void tokenRefreshed();
	void error(const QString &message);

private:
	QString configPath() const;
	void loadClientCredentials();
	void loadTokens();
	void saveTokens();
	void exchangeCodeForTokens(const QString &code);
	void refreshAccessToken();
	void onTokenReply();
	void onApiReply();
	void openAuthUrl();
	bool ensureValidAccessToken();
	void doBind(const QString &broadcastId, const QString &streamId);

	QString m_clientId;
	QString m_clientSecret;
	QString m_accessToken;
	QString m_refreshToken;
	QString m_authError;

	QNetworkAccessManager *m_network = nullptr;
	QTcpServer *m_redirectServer = nullptr;
	QByteArray m_pendingRequestBody;
	QString m_pendingRequestId;
	QMetaObject::Connection m_replyConnection;
};
