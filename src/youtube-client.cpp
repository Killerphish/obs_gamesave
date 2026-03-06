/*
GameSave - OBS Livestream Recording Organizer
Copyright (C) 2026

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#include "youtube-client.hpp"
#include <obs-module.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QDesktopServices>
#include <QUrl>
#include <QNetworkReply>
#include <QTcpSocket>
#include <QTimer>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QUuid>
#include <cctype>

static const char kAuthEndpoint[] = "https://accounts.google.com/o/oauth2/v2/auth";
static const char kTokenEndpoint[] = "https://oauth2.googleapis.com/token";
static const char kScope[] = "https://www.googleapis.com/auth/youtube.force-ssl";
static const char kLiveBroadcastsUrl[] = "https://www.googleapis.com/youtube/v3/liveBroadcasts";
static const char kLiveStreamsUrl[] = "https://www.googleapis.com/youtube/v3/liveStreams";
static const char kThumbnailsSetUrl[] = "https://www.googleapis.com/upload/youtube/v3/thumbnails/set";

// Default client ID for desktop OAuth (user can override via config).
// Using a placeholder; in production use a Google Cloud OAuth 2.0 Desktop client ID.
static const char kDefaultClientId[] = "GAMESAVE_PLACEHOLDER_CLIENT_ID";

YouTubeApiClient::YouTubeApiClient(QObject *parent)
	: QObject(parent)
	, m_clientId(QLatin1String(kDefaultClientId))
	, m_network(new QNetworkAccessManager(this))
{
	m_redirectServer = new QTcpServer(this);
	loadTokens();
}

YouTubeApiClient::~YouTubeApiClient() = default;

QString YouTubeApiClient::configPath() const
{
	char *path = obs_module_config_path("youtube_oauth.json");
	if (!path) {
		return QString();
	}
	QString qpath = QString::fromUtf8(path);
	bfree(path);
	return qpath;
}

void YouTubeApiClient::loadTokens()
{
	QString path = configPath();
	if (path.isEmpty()) {
		return;
	}
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly)) {
		return;
	}
	QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
	f.close();
	if (!doc.isObject()) {
		return;
	}
	QJsonObject o = doc.object();
	m_refreshToken = o.value("refresh_token").toString();
	m_accessToken = o.value("access_token").toString();
	// Optionally respect expiry and refresh on load
}

void YouTubeApiClient::saveTokens()
{
	QString path = configPath();
	if (path.isEmpty()) {
		return;
	}
	QFileInfo fi(path);
	QDir().mkpath(fi.absolutePath());
	QFile f(path);
	if (!f.open(QIODevice::WriteOnly)) {
		return;
	}
	QJsonObject o;
	o.insert("refresh_token", m_refreshToken);
	o.insert("access_token", m_accessToken);
	f.write(QJsonDocument(o).toJson(QJsonDocument::Compact));
	f.close();
}

void YouTubeApiClient::signOut()
{
	m_accessToken.clear();
	m_refreshToken.clear();
	m_authError.clear();
	QString path = configPath();
	if (!path.isEmpty()) {
		QFile::remove(path);
	}
}

bool YouTubeApiClient::isAuthenticated() const
{
	return !m_refreshToken.isEmpty();
}

void YouTubeApiClient::startAuth()
{
	m_authError.clear();
	if (m_clientId.isEmpty() || m_clientId == QLatin1String(kDefaultClientId)) {
		m_authError = QObject::tr("YouTube Client ID not configured. Set it in plugin config or build.");
		emit authFinished(false);
		return;
	}

	if (!m_redirectServer->listen(QHostAddress::LocalHost, 0)) {
		m_authError = QObject::tr("Could not start local server for OAuth redirect.");
		emit authFinished(false);
		return;
	}

	quint16 port = m_redirectServer->serverPort();
	QString redirectUri = QString("http://127.0.0.1:%1/").arg(port);
	QUrlQuery q;
	q.addQueryItem("client_id", m_clientId);
	q.addQueryItem("redirect_uri", redirectUri);
	q.addQueryItem("response_type", "code");
	q.addQueryItem("scope", QLatin1String(kScope));
	q.addQueryItem("access_type", "offline");
	q.addQueryItem("prompt", "consent");

	QUrl url(QString::fromLatin1(kAuthEndpoint));
	url.setQuery(q);

	connect(m_redirectServer, &QTcpServer::newConnection, this, [this, redirectUri]() {
		QTcpSocket *socket = m_redirectServer->nextPendingConnection();
		m_redirectServer->close();
		connect(socket, &QTcpSocket::readyRead, this, [this, socket, redirectUri]() {
			QByteArray data = socket->readAll();
			QString req = QString::fromUtf8(data);
			int start = req.indexOf("GET ");
			if (start >= 0) {
				int end = req.indexOf(" HTTP/");
				if (end > start) {
					QString path = req.mid(start + 4, end - start - 4).trimmed();
					QUrl u(redirectUri);
					u.setPath(path);
					QString code = QUrlQuery(u.query()).queryItemValue("code");
					if (!code.isEmpty()) {
						exchangeCodeForTokens(code);
					} else {
						QString err = QUrlQuery(u.query()).queryItemValue("error");
						m_authError = err.isEmpty() ? QObject::tr("No authorization code received.") : err;
						emit authFinished(false);
					}
					QByteArray body =
						"HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n\r\n"
						"<html><body><p>Authorization complete. You can close this window.</p></body></html>";
					socket->write(body);
					socket->flush();
					QTimer::singleShot(500, socket, &QObject::deleteLater);
					return;
				}
			}
			socket->close();
			socket->deleteLater();
		});
	});

	openAuthUrl();
	if (!QDesktopServices::openUrl(url)) {
		m_redirectServer->close();
		m_authError = QObject::tr("Could not open browser.");
		emit authFinished(false);
	}
}

void YouTubeApiClient::openAuthUrl()
{
	// Used by startAuth; URL is opened there
}

void YouTubeApiClient::exchangeCodeForTokens(const QString &code)
{
	QString redirectUri;
	if (m_redirectServer->serverPort() != 0) {
		redirectUri = QString("http://127.0.0.1:%1/").arg(m_redirectServer->serverPort());
	} else {
		redirectUri = "http://127.0.0.1/";
	}

	QUrl url(QString::fromLatin1(kTokenEndpoint));
	QNetworkRequest req(url);
	req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

	QUrlQuery body;
	body.addQueryItem("client_id", m_clientId);
	if (!m_clientSecret.isEmpty()) {
		body.addQueryItem("client_secret", m_clientSecret);
	}
	body.addQueryItem("code", code);
	body.addQueryItem("grant_type", "authorization_code");
	body.addQueryItem("redirect_uri", redirectUri);

	QByteArray bodyData = body.query(QUrl::FullyEncoded).toUtf8();

	QNetworkReply *reply = m_network->post(req, bodyData);
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		reply->deleteLater();
		QByteArray data = reply->readAll();
		if (reply->error() != QNetworkReply::NoError) {
			m_authError = reply->errorString();
			emit authFinished(false);
			return;
		}
		QJsonDocument doc = QJsonDocument::fromJson(data);
		if (!doc.isObject()) {
			m_authError = QObject::tr("Invalid token response.");
			emit authFinished(false);
			return;
		}
		QJsonObject o = doc.object();
		m_accessToken = o.value("access_token").toString();
		m_refreshToken = o.value("refresh_token").toString();
		if (m_refreshToken.isEmpty()) {
			m_authError = QObject::tr("No refresh token in response. Try revoking app access and sign in again.");
			emit authFinished(false);
			return;
		}
		saveTokens();
		m_authError.clear();
		emit authFinished(true);
	});
}

void YouTubeApiClient::refreshAccessToken()
{
	if (m_refreshToken.isEmpty()) {
		emit error(tr("Not signed in to YouTube."));
		return;
	}

	QUrl url((QLatin1String(kTokenEndpoint)));
	QNetworkRequest req(url);
	req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

	QUrlQuery body;
	body.addQueryItem("client_id", m_clientId);
	if (!m_clientSecret.isEmpty()) {
		body.addQueryItem("client_secret", m_clientSecret);
	}
	body.addQueryItem("refresh_token", m_refreshToken);
	body.addQueryItem("grant_type", "refresh_token");

	QByteArray bodyData = body.query(QUrl::FullyEncoded).toUtf8();

	QNetworkReply *reply = m_network->post(req, bodyData);
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		reply->deleteLater();
		QByteArray data = reply->readAll();
		if (reply->error() != QNetworkReply::NoError) {
			emit error(tr("Token refresh failed: %1").arg(reply->errorString()));
			return;
		}
		QJsonDocument doc = QJsonDocument::fromJson(data);
		if (!doc.isObject()) {
			emit error(tr("Invalid token response."));
			return;
		}
		QJsonObject o = doc.object();
		m_accessToken = o.value("access_token").toString();
		if (!m_accessToken.isEmpty()) {
			saveTokens();
			emit tokenRefreshed();
		}
	});
}

bool YouTubeApiClient::ensureValidAccessToken()
{
	if (!m_accessToken.isEmpty()) {
		return true;
	}
	if (m_refreshToken.isEmpty()) {
		return false;
	}
	// Synchronous refresh would block; we do async. Callers should trigger refresh and retry when ready.
	refreshAccessToken();
	return false;
}

void YouTubeApiClient::listUpcomingBroadcasts()
{
	if (m_refreshToken.isEmpty()) {
		emit broadcastsListed(QList<YouTubeBroadcast>());
		emit error(tr("Not signed in to YouTube."));
		return;
	}
	if (m_accessToken.isEmpty()) {
		// Refresh then list when token is ready.
		connect(this, &YouTubeApiClient::tokenRefreshed, this, &YouTubeApiClient::listUpcomingBroadcasts,
			Qt::SingleShotConnection);
		refreshAccessToken();
		return;
	}

	QUrl url(QString::fromLatin1(kLiveBroadcastsUrl));
	QUrlQuery q;
	q.addQueryItem("part", "snippet,contentDetails,status");
	q.addQueryItem("broadcastStatus", "upcoming");
	q.addQueryItem("broadcastType", "all");
	q.addQueryItem("mine", "true");
	q.addQueryItem("maxResults", "50");
	url.setQuery(q);

	QNetworkRequest req(url);
	req.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());

	QNetworkReply *reply = m_network->get(req);
	m_pendingRequestId = "listBroadcasts";
	QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		reply->deleteLater();
		QByteArray data = reply->readAll();
		if (reply->error() != QNetworkReply::NoError) {
			if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401) {
				m_accessToken.clear();
				refreshAccessToken();
				QTimer::singleShot(1500, this, [this]() { listUpcomingBroadcasts(); });
				return;
			}
			emit broadcastsListed(QList<YouTubeBroadcast>());
			emit error(tr("Failed to list broadcasts: %1").arg(reply->errorString()));
			return;
		}
		QJsonDocument doc = QJsonDocument::fromJson(data);
		if (!doc.isObject()) {
			emit broadcastsListed(QList<YouTubeBroadcast>());
			return;
		}
		QJsonArray items = doc.object().value("items").toArray();
		QList<YouTubeBroadcast> list;
		for (const QJsonValue &v : items) {
			QJsonObject obj = v.toObject();
			YouTubeBroadcast b;
			b.id = obj.value("id").toString();
			QJsonObject snippet = obj.value("snippet").toObject();
			b.title = snippet.value("title").toString();
			QJsonObject contentDetails = obj.value("contentDetails").toObject();
			b.boundStreamId = contentDetails.value("boundStreamId").toString();
			QString scheduled = snippet.value("scheduledStartTime").toString();
			if (!scheduled.isEmpty()) {
				b.scheduledStartTime = QDateTime::fromString(scheduled, Qt::ISODate);
				b.scheduledStartTime.setTimeZone(QTimeZone::utc());
			}
			list.append(b);
		}
		emit broadcastsListed(list);
	});
}

void YouTubeApiClient::fetchStreamKey(const QString &broadcastId, const QString &boundStreamId)
{
	Q_UNUSED(broadcastId);
	if (m_accessToken.isEmpty()) {
		emit error(tr("Not signed in or token expired."));
		return;
	}

	QString streamId = boundStreamId;
	if (streamId.isEmpty()) {
		emit error(tr("Broadcast has no bound stream."));
		return;
	}

	QUrl url(QString::fromLatin1(kLiveStreamsUrl));
	QUrlQuery q;
	q.addQueryItem("part", "cdn");
	q.addQueryItem("id", streamId);
	url.setQuery(q);

	QNetworkRequest req(url);
	req.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());

	QNetworkReply *reply = m_network->get(req);
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		reply->deleteLater();
		QByteArray data = reply->readAll();
		if (reply->error() != QNetworkReply::NoError) {
			emit error(tr("Failed to get stream key: %1").arg(reply->errorString()));
			return;
		}
		QJsonDocument doc = QJsonDocument::fromJson(data);
		if (!doc.isObject()) {
			emit error(tr("Invalid stream response."));
			return;
		}
		QJsonArray items = doc.object().value("items").toArray();
		if (items.isEmpty()) {
			emit error(tr("Stream not found."));
			return;
		}
		QJsonObject cdn = items.at(0).toObject().value("cdn").toObject();
		QString ingestion = cdn.value("ingestionInfo").toObject().value("ingestionAddress").toString();
		QString backup = cdn.value("ingestionInfo").toObject().value("backupIngestionAddress").toString();
		QString streamKey = cdn.value("ingestionInfo").toObject().value("streamName").toString();

		YouTubeStreamKey key;
		key.streamKey = streamKey;
		key.ingestionAddress = ingestion;
		key.backupIngestionAddress = backup;
		emit streamKeyReceived(key);
	});
}

void YouTubeApiClient::createBroadcast(const QString &title, const QString &description,
				       const QDateTime &scheduledStartTime,
				       const QString &privacyStatus, bool selfDeclaredMadeForKids)
{
	if (m_accessToken.isEmpty()) {
		emit error(tr("Not signed in or token expired."));
		return;
	}

	QString privacy = privacyStatus.trimmed().toLower();
	if (privacy.isEmpty()) {
		privacy = QStringLiteral("public");
	}
	if (privacy != QLatin1String("public") && privacy != QLatin1String("unlisted") && privacy != QLatin1String("private")) {
		privacy = QStringLiteral("public");
	}

	QUrl url(QString::fromLatin1(kLiveBroadcastsUrl));
	QUrlQuery q;
	q.addQueryItem("part", "snippet,status,contentDetails");
	url.setQuery(q);

	QJsonObject snippet;
	snippet.insert("title", title);
	snippet.insert("description", description);
	snippet.insert("scheduledStartTime", scheduledStartTime.toUTC().toString(Qt::ISODate));

	QJsonObject status;
	status.insert("privacyStatus", privacy);
	status.insert("selfDeclaredMadeForKids", selfDeclaredMadeForKids);

	QJsonObject root;
	root.insert("snippet", snippet);
	root.insert("status", status);

	QNetworkRequest req(url);
	req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	req.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());

	QByteArray body = QJsonDocument(root).toJson(QJsonDocument::Compact);
	QNetworkReply *reply = m_network->post(req, body);
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		reply->deleteLater();
		QByteArray data = reply->readAll();
		if (reply->error() != QNetworkReply::NoError) {
			emit error(tr("Failed to create broadcast: %1").arg(reply->errorString()));
			return;
		}
		QJsonDocument doc = QJsonDocument::fromJson(data);
		if (!doc.isObject()) {
			emit error(tr("Invalid create broadcast response."));
			return;
		}
		QString id = doc.object().value("id").toString();
		if (id.isEmpty()) {
			emit error(tr("No broadcast ID in response."));
			return;
		}
		emit broadcastCreated(id);
	});
}

void YouTubeApiClient::ensureStreamBound(const QString &broadcastId)
{
	if (m_accessToken.isEmpty()) {
		emit error(tr("Not signed in or token expired."));
		return;
	}

	// First list streams to get an existing stream id, or create one
	QUrl listUrl(QString::fromLatin1(kLiveStreamsUrl));
	QUrlQuery listQ;
	listQ.addQueryItem("part", "id");
	listQ.addQueryItem("mine", "true");
	listQ.addQueryItem("maxResults", "1");
	listUrl.setQuery(listQ);

	QNetworkRequest listReq(listUrl);
	listReq.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());
	QNetworkReply *listReply = m_network->get(listReq);
	connect(listReply, &QNetworkReply::finished, this, [this, broadcastId, listReply]() {
		listReply->deleteLater();
		QByteArray data = listReply->readAll();
		QString streamId;
		if (listReply->error() == QNetworkReply::NoError) {
			QJsonDocument doc = QJsonDocument::fromJson(data);
			QJsonArray items = doc.object().value("items").toArray();
			if (!items.isEmpty()) {
				streamId = items.at(0).toObject().value("id").toString();
			}
		}
		if (streamId.isEmpty()) {
			// Create a new stream
			QUrl createUrl(QString::fromLatin1(kLiveStreamsUrl));
			QUrlQuery createQ;
			createQ.addQueryItem("part", "snippet,cdn");
			createUrl.setQuery(createQ);
			QJsonObject snippet;
			snippet.insert("title", "GameSave stream " + QUuid::createUuid().toString(QUuid::WithoutBraces));
			QJsonObject root;
			root.insert("snippet", snippet);
			QNetworkRequest createReq(createUrl);
			createReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
			createReq.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());
			QNetworkReply *createReply = m_network->post(createReq, QJsonDocument(root).toJson(QJsonDocument::Compact));
			connect(createReply, &QNetworkReply::finished, this, [this, broadcastId, createReply]() {
				createReply->deleteLater();
				QByteArray createData = createReply->readAll();
				QString streamId2;
				if (createReply->error() == QNetworkReply::NoError) {
					QJsonDocument doc = QJsonDocument::fromJson(createData);
					streamId2 = doc.object().value("id").toString();
				}
				if (streamId2.isEmpty()) {
					emit error(tr("Failed to create stream."));
					return;
				}
				doBind(broadcastId, streamId2);
			});
			return;
		}
		doBind(broadcastId, streamId);
	});
}

void YouTubeApiClient::doBind(const QString &broadcastId, const QString &streamId)
{
	QUrl url(QString::fromLatin1(kLiveBroadcastsUrl) + "/bind");
	QUrlQuery q;
	q.addQueryItem("part", "id,contentDetails");
	q.addQueryItem("id", broadcastId);
	q.addQueryItem("streamId", streamId);
	url.setQuery(q);

	QNetworkRequest req(url);
	req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	req.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());

	QNetworkReply *reply = m_network->post(req, QByteArray("{}"));
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		reply->deleteLater();
		if (reply->error() != QNetworkReply::NoError) {
			emit error(tr("Failed to bind stream: %1").arg(reply->errorString()));
			return;
		}
		emit streamBound();
	});
}

void YouTubeApiClient::setBroadcastThumbnail(const QString &videoId, const QString &imagePath)
{
	if (m_accessToken.isEmpty()) {
		emit error(tr("Not signed in or token expired."));
		return;
	}
	if (videoId.isEmpty() || imagePath.isEmpty()) {
		return;
	}

	QFile file(imagePath);
	if (!file.open(QIODevice::ReadOnly)) {
		emit error(tr("Could not open thumbnail file: %1").arg(file.errorString()));
		return;
	}
	QByteArray imageData = file.readAll();
	file.close();
	if (imageData.size() > 2 * 1024 * 1024) {
		emit error(tr("Thumbnail file is larger than 2 MB."));
		return;
	}

	QString mimeType = QStringLiteral("image/jpeg");
	QString suffix = QFileInfo(imagePath).suffix().toLower();
	if (suffix == QLatin1String("png")) {
		mimeType = QStringLiteral("image/png");
	} else if (suffix == QLatin1String("jpg") || suffix == QLatin1String("jpeg")) {
		mimeType = QStringLiteral("image/jpeg");
	}

	QUrl url(QString::fromLatin1(kThumbnailsSetUrl));
	QUrlQuery q;
	q.addQueryItem("videoId", videoId);
	url.setQuery(q);

	QNetworkRequest req(url);
	req.setHeader(QNetworkRequest::ContentTypeHeader, mimeType);
	req.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());

	QNetworkReply *reply = m_network->post(req, imageData);
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		reply->deleteLater();
		if (reply->error() != QNetworkReply::NoError) {
			emit error(tr("Failed to set thumbnail: %1").arg(reply->errorString()));
			return;
		}
		emit thumbnailSet();
	});
}
