#include "PLSPlatformTwitch.h"

#include <QDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <qurlquery.h>
#include "frontend-api.h"
#include "PLSAlertView.h"
#include "window-basic-main.hpp"
#include "pls-common-define.hpp"
#include "../PLSPlatformApi.h"
#include "../PLSLiveInfoDialogs.h"
#include "PLSChannelDataAPI.h"
#include "QTimer"

#define TWTICH_CHAT QStringLiteral("TwitchChat")
#define TWTICH_SERVER QStringLiteral("TwitchServer")
#define TWITCH_TITLE_INVAILD QStringLiteral("Status contains banned words")
const char *globalServer = "rtmp://ingest.global-contribute.live-video.net/app";
using namespace std;
using namespace common;

PLSServiceType PLSPlatformTwitch::getServiceType() const
{
	return PLSServiceType::ST_TWITCH;
}

void PLSPlatformTwitch::onPrepareLive(bool value)
{
	if (!value) {
		prepareLiveCallback(value);
		return;
	}
	PLS_INFO(MODULE_PLATFORM_TWITCH, "%s  show liveinfo value(%s)", PrepareInfoPrefix, BOOL2STR(value));
	value = pls_exec_live_Info_twitch(getChannelUUID(), getInitData()) == QDialog::Accepted;
	PLS_INFO(MODULE_PLATFORM_TWITCH, "%s  close liveinfo value(%s)", PrepareInfoPrefix, BOOL2STR(value));
	prepareLiveCallback(value);
}

void PLSPlatformTwitch::onAlLiveStarted(bool value) {}

void PLSPlatformTwitch::onLiveEnded()
{
	if (PLS_PLATFORM_API->isPrismLive()) {
		liveEndedCallback();
	} else {
		requestVideos();
	}
}

void PLSPlatformTwitch::serverHandler()
{
	auto obj = PLSLoginDataHandler::instance()->getTwitchServiceList();

	auto ingests = obj["ingests"].toArray();
	auto _idConfig = config_get_uint(App()->GlobalConfig(), KeyConfigLiveInfo, KeyTwitchServer);

	auto ingestSize = ingests.count();
	if (ingestSize == 0) {
		PLS_INFO(MODULE_PLATFORM_TWITCH, "get twitch service list failed, will use globalServer = %s", globalServer);
		setStreamServer(globalServer);
	} else {
		auto server = ingests[0].toObject().value("url_template").toString();
		auto lastIndex = server.lastIndexOf('/');
		auto serverUrl = server.left(lastIndex).toStdString();
		setStreamServer(serverUrl);
		PLS_INFO(MODULE_PLATFORM_TWITCH, "get twitch service list success, selected server = %s", serverUrl.c_str());
	}
	PLS_LOGEX(PLS_LOG_INFO, MODULE_PLATFORM_TWITCH,
		  {
			  {"platformName", "twitch"},
			  {"startLiveStatus", "Success"},
		  },
		  "twitch start live success");
}

QString maskUrl(const QString &url, const QVariantMap &queryInfo)
{
	QUrl maskUrl(url);
	QUrlQuery maskQuery;
	for (auto info = queryInfo.constBegin(); info != queryInfo.constEnd(); ++info) {
		maskQuery.addQueryItem(info.key(), info.value().toString());
	}
	maskUrl.setQuery(maskQuery);
	return maskUrl.toString();
}

QVariantMap PLSPlatformTwitch::setHttpHead() const
{
	return {{"Client-ID", TWITCH_CLIENT_ID}, {"Authorization", "Bearer " + getChannelToken()}, {"Accept", HTTP_ACCEPT_TWITCH}};
}

PLSPlatformApiResult PLSPlatformTwitch::getApiResult(int code, QNetworkReply::NetworkError error)
{
	auto result = PLSPlatformApiResult::PAR_SUCCEED;

	if (QNetworkReply::NoError == error) {
		return result;
	} else if (QNetworkReply::UnknownNetworkError >= error) {
		result = PLSPlatformApiResult::PAR_NETWORK_ERROR;
	} else {
		switch (code) {
		case 400:
			result = PLSPlatformApiResult::PAR_API_ERROR_UPDATE;
			break;
		case 401:
			result = PLSPlatformApiResult::PAR_TOKEN_EXPIRED;
			break;
		case 403:
			result = PLSPlatformApiResult::PAR_API_ERROR_FORBIDDEN;
			break;
		default:
			result = PLSPlatformApiResult::PAR_API_FAILED;
			break;
		}
	}

	return result;
}

void PLSPlatformTwitch::showApiRefreshError(PLSPlatformApiResult value)
{
	auto alertParent = getAlertParent();
	switch (value) {
	case PLSPlatformApiResult::PAR_NETWORK_ERROR:
		PLSAlertView::warning(alertParent, QTStr("Alert.Title"), QTStr("login.check.note.network"));
		break;
	case PLSPlatformApiResult::PAR_TOKEN_EXPIRED:
	case PLSPlatformApiResult::PAR_API_ERROR_FORBIDDEN:
		if (QDialogButtonBox::Ok == pls_alert_error_message(alertParent, QTStr("Alert.Title"), QTStr("Live.Check.LiveInfo.Refresh.Expired").arg(getChannelName()))) {
			emit closeDialogByExpired();
			PLSCHANNELS_API->channelExpired(getChannelUUID(), false);
		}
		break;
	default:
		pls_alert_error_message(alertParent, QTStr("Alert.Title"), QTStr("Live.Check.LiveInfo.Refresh.Failed"));
		break;
	}
}

void PLSPlatformTwitch::showApiUpdateError(PLSPlatformApiResult value, const QString &msg)
{
	auto alertParent = getAlertParent();

	switch (value) {
	case PLSPlatformApiResult::PAR_NETWORK_ERROR:
		PLSAlertView::warning(alertParent, QTStr("Alert.Title"), QTStr("login.check.note.network"));
		break;
	case PLSPlatformApiResult::PAR_TOKEN_EXPIRED:
		if (QDialogButtonBox::Ok == pls_alert_error_message(alertParent, QTStr("Alert.Title"), QTStr("Live.Check.LiveInfo.Refresh.Expired").arg(getChannelName()))) {
			emit closeDialogByExpired();
			PLSCHANNELS_API->channelExpired(getChannelUUID(), false);
		}
		break;
	case PLSPlatformApiResult::PAR_API_ERROR_UPDATE:
		if (msg.contains(TWITCH_TITLE_INVAILD)) {
			pls_alert_error_message(alertParent, QTStr("Alert.Title"), QTStr("Live.Check.twitch.LiveInfo.Update.title.Failed"));
		} else {
			pls_alert_error_message(alertParent, QTStr("Alert.Title"), QTStr("Live.Check.twitch.LiveInfo.Update.protocol.failed"));
		}
		break;
	default:
		pls_alert_error_message(alertParent, QTStr("Alert.Title"), QTStr("Live.Check.LiveInfo.Update.Error.Failed").arg(getChannelName()));
		break;
	}
}

void PLSPlatformTwitch::requestStreamKey(bool showAlert, const streamKeyCallback &callback)
{
	auto broadcastId = PLSCHANNELS_API->getChannelInfo(getChannelUUID()).value("broadcast_id").toString().toStdString();
	setChannelId(broadcastId);
	auto maskUrlStr = QString(CHANNEL_TWITCH_STREAMKEY).arg(pls_masking_person_info(QString::fromStdString(getChannelId())));
	auto url = QString(CHANNEL_TWITCH_STREAMKEY).arg(QString::fromStdString(getChannelId()));
	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Get)
				   .jsonContentType() //
				   .rawHeaders(setHttpHead())
				   .withLog(maskUrlStr) //
				   .receiver(this)      //
				   .url(url)            //
				   .timeout(PRISM_NET_REQUEST_TIMEOUT)
				   .okResult([this, showAlert, callback](const pls::http::Reply &reply) {
					   auto data = reply.data();
					   auto code = reply.statusCode();

					   auto doc = QJsonDocument::fromJson(data);
					   pls_async_call_mt(getAlertParent(), [this, doc, showAlert, data, code, callback]() {
						   if (doc.isObject()) {
							   auto root = doc.object().value("data").toArray().first().toObject();
							   setStreamKey(root["stream_key"].toString().toStdString());

							   m_strOriginalTitle = getTitle();
							   serverHandler();
							   callback();
						   } else {
							   PLS_ERROR(MODULE_PLATFORM_TWITCH, "requestStreamKey.error: %d", code);
							   PLS_LOGEX(PLS_LOG_ERROR, MODULE_PLATFORM_TWITCH, {{"channel start error", "twitch"}}, "request streamKey api error, code = %d error = %s.",
								     code, data.constData());

							   if (showAlert) {
								   pls_alert_error_message(getAlertParent(), QTStr("Alert.Title"), QTStr("Live.Check.LiveInfo.Refresh.Failed"));
							   }
						   }
					   });
				   })
				   .failResult([this, showAlert](const pls::http::Reply &reply) {
					   auto code = reply.statusCode();
					   auto error = reply.error();
					   PLS_LOGEX(PLS_LOG_ERROR, MODULE_PLATFORM_TWITCH, {{"channel start error", "twitch"}}, "request streamKey api error, code = %d error = %d.", code, error);

					   auto result = getApiResult(code, error);
					   if (showAlert) {
						   pls_async_call_mt(getAlertParent(), [this, showAlert, result]() { showApiRefreshError(result); });
					   }
				   }));
}
void PLSPlatformTwitch::getChannelInfo()
{
	PLS_INFO("twitchPlatform", "start get channel infos");
	auto broadcastId = PLSCHANNELS_API->getChannelInfo(getChannelUUID()).value("broadcast_id").toString();

	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Get)
				   .jsonContentType() //
				   .rawHeaders(setHttpHead())
				   .withLog(QString(CHANNEL_TWITCH_INFO_URL).arg(pls_masking_person_info(broadcastId))) //
				   .url(QString(CHANNEL_TWITCH_INFO_URL).arg(broadcastId))                              //
				   .receiver(PLSCHANNELS_API)
				   .timeout(PRISM_NET_REQUEST_TIMEOUT)
				   .okResult([this](const pls::http::Reply &reply) {
					   auto jsonDoc = QJsonDocument::fromJson(reply.data()).object().value("data").toArray();
					   auto jsonMap = jsonDoc.first().toObject().toVariantMap();
					   auto category = jsonMap.value("game_name");
					   auto lastInfo = PLSCHANNELS_API->getChannelInfo(getChannelUUID());
					   lastInfo.insert(ChannelData::g_catogry, category);
					   lastInfo.insert(ChannelData::g_displayLine2, category);
					   PLSCHANNELS_API->setChannelInfos(lastInfo, true);
				   })
				   .failResult([this](const pls::http::Reply &reply) {
					   auto statusCode = reply.statusCode();
					   QString errorStr = "channel error status code " + QString::number(statusCode);
					   PLS_ERROR(MODULE_PLATFORM_TWITCH, errorStr.toStdString().c_str());
				   }));
}

void PLSPlatformTwitch::requestUpdateChannel(const string &title, const string &category, const string &categoryId)
{
	PLS_INFO(MODULE_PLATFORM_TWITCH, "start request uddate channel info");
	auto url = QString(CHANNEL_TWITCH_INFO_URL).arg(QString::fromStdString(getChannelId()));
	pls::http::request(pls::http::Request()
				   .method("PATCH")
				   .contentType(HTTP_CONTENT_TYPE_URL_ENCODED_VALUE) //
				   .form(QVariantMap({{"title", QString::fromStdString(title)},
						      {"game_id", categoryId.empty() ? "0" : categoryId.c_str()},
						      {"broadcaster_language", PLSCHANNELS_API->getChannelInfo(getChannelUUID()).value("broadcaster_language").toString().toUtf8().constData()}}))
				   .rawHeaders(setHttpHead())
				   .withLog(QString(CHANNEL_TWITCH_INFO_URL).arg(pls_masking_person_info(QString::fromStdString(getChannelId())))) //
				   .receiver(this)                                                                                                 //
				   .workInMainThread()                                                                                             //
				   .url(url)                                                                                                       //
				   .timeout(PRISM_NET_REQUEST_TIMEOUT)
				   .okResult([this, title, category](const pls::http::Reply &reply) {
					   auto data = reply.data();
					   setTitle(title);
					   setCategory(category);
					   emit onUpdateChannel(PLSPlatformApiResult::PAR_SUCCEED);
				   })
				   .failResult([this](const pls::http::Reply &reply) {
					   auto code = reply.statusCode();
					   auto error = reply.error();
					   PLS_ERROR(MODULE_PLATFORM_TWITCH, "requestUpdateChannel.error: %d-%d", code, error);
					   auto data = reply.data();
					   auto result = getApiResult(code, error);
					   showApiUpdateError(result, data);
					   emit onUpdateChannel(result);
				   }));
}

void PLSPlatformTwitch::requestCategory(const QString &query)
{
	auto url = QString(CHANNEL_TWITCH_CATEGORY).arg(QUrl::toPercentEncoding(query).constData());
	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Get)
				   .jsonContentType() //
				   .rawHeaders(setHttpHead())
				   .withLog()          //
				   .receiver(this)     //
				   .workInMainThread() //
				   .url(url)           //
				   .timeout(PRISM_NET_REQUEST_TIMEOUT)
				   .okResult([this, query](const pls::http::Reply &reply) {
					   auto data = reply.data();
					   auto code = reply.statusCode();
					   auto doc = QJsonDocument::fromJson(data);
					   if (doc.isObject()) {
						   auto root = doc.object();

						   emit onGetCategory(root, query);
					   } else {
						   PLS_ERROR(MODULE_PLATFORM_TWITCH, "requestCategory.error: %d", code);

						   emit onGetCategory(QJsonObject(), query);
					   }
				   })
				   .failResult([this, query](const pls::http::Reply &reply) {
					   auto code = reply.statusCode();
					   auto error = reply.error();
					   PLS_ERROR(MODULE_PLATFORM_TWITCH, "requestCategory.error: %d-%d", code, error);
					   emit onGetCategory(QJsonObject(), query);
				   }));
}

void PLSPlatformTwitch::requestVideos()
{
	auto maskUrlStr = maskUrl(CHANNEL_TWITCH_VIDEOS, {{"user_id", pls_masking_person_info(QString::fromStdString(getChannelId()))}});
	QUrl url(CHANNEL_TWITCH_VIDEOS);
	QUrlQuery query;
	query.addQueryItem("user_id", getChannelId().c_str());
	url.setQuery(query);
	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Get)
				   .jsonContentType() //
				   .rawHeaders(setHttpHead())
				   .withLog(maskUrlStr) //
				   .receiver(this)      //
				   .workInMainThread()  //
				   .url(url)            //
				   .timeout(PRISM_NET_REQUEST_TIMEOUT)
				   .okResult([this](const pls::http::Reply &reply) {
					   auto data = reply.data();
					   auto code = reply.statusCode();

					   if (auto doc = QJsonDocument::fromJson(data); doc.isObject()) {
						   auto root = doc.object()["data"].toArray();
						   if (!root.isEmpty()) {
							   m_strEndUrl = root[0].toObject()["url"].toString();
						   }
					   } else {
						   PLS_ERROR(MODULE_PLATFORM_TWITCH, "requestVideos.error: %d", code);
					   }

					   QMetaObject::invokeMethod(
						   this, [this]() { liveEndedCallback(); }, Qt::QueuedConnection);
				   })
				   .failResult([this](const pls::http::Reply &reply) {
					   auto code = reply.statusCode();
					   auto error = reply.error();
					   PLS_ERROR(MODULE_PLATFORM_TWITCH, "requestVideos.error: %d-%d", code, error);

					   QMetaObject::invokeMethod(
						   this, [this]() { liveEndedCallback(); }, Qt::QueuedConnection);
				   }));
}

QJsonObject PLSPlatformTwitch::getLiveStartParams()
{
	QJsonObject platform(PLSPlatformBase::getLiveStartParams());

	platform["simulcastChannel"] = QString::fromStdString(getDisplayName());

	return platform;
}

QJsonObject PLSPlatformTwitch::getWebChatParams()
{
	QJsonObject platform(PLSPlatformBase::getWebChatParams());

	platform["clientId"] = TWITCH_CLIENT_ID;

	return platform;
}

QString PLSPlatformTwitch::getServiceLiveLink()
{
	return m_strEndUrl;
}

QString PLSPlatformTwitch::getShareUrl()
{
	return PLSCHANNELS_API->getChannelInfo(getChannelUUID()).value(ChannelData::g_shareUrl).toString();
}

QString PLSPlatformTwitch::getShareUrlEnc()
{
	return QString(pls_masking_person_info(getShareUrl()));
}

QString PLSPlatformTwitch::getServiceLiveLinkEnc()
{
	return pls_masking_person_info(m_strEndUrl);
}

bool PLSPlatformTwitch::onMQTTMessage(PLSPlatformMqttTopic top, const QJsonObject &jsonObject)
{
	return true;
}
