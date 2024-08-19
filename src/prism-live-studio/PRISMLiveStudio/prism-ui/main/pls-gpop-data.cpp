#include "pls-gpop-data.hpp"
#include "pls-common-define.hpp"
#include "json-data-handler.hpp"
#include "liblog.h"
#include "libutils-api-log.h"
#include "log/module_names.h"
#include <QDir>
#include "ChannelCommonFunctions.h"
#include "login-user-info.hpp"

using namespace std;
using namespace common;
constexpr auto PRISM_GPOP_PATH = "PRISMLiveStudio/user/gpop.json";
PLSGpopData *PLSGpopData::instance()
{
	static PLSGpopData gpopdata;
	return &gpopdata;
}

void PLSGpopData::getGpopData(const QByteArray &gpopData)
{
	m_gpopDataArray = gpopData;

	if (gpopData.isEmpty()) {
		m_gpopDataArray = getDefaultValuesOf("gpop");
		PLS_INFO("PLSGpopData", "gpop data is empty, need  to read local qrc file .");
	}
	initGpopData();
}

QByteArray PLSGpopData::getDefaultValuesOf(const QString &key)
{

	auto file = findFileInResources(ChannelData::defaultSourcePath, key);
	QByteArray data;
	PLSJsonDataHandler::getJsonArrayFromFile(data, file);
	return data;
}

PLSGpopData::PLSGpopData(QObject *parent) : QObject(parent) {}

Connection PLSGpopData::getConnection()
{
	Connection connection;
	return connection;
}
Common PLSGpopData::getCommon()
{
	if (m_common.deviceType.isEmpty()) {
		initCommon();
	}
	return m_common;
}

void PLSGpopData::initDefaultValues()
{
	
}

void PLSGpopData::initCommon()
{
	QVariantMap map;
	QVariant value;
	PLSJsonDataHandler::getValuesFromByteArray(m_gpopDataArray, "optional", map);
	Common common;
	common.stage = map.value(name2str(common)).toMap().value(name2str(stage)).toString();
	common.regionCode = map.value(name2str(common)).toMap().value(name2str(regionCode)).toString();
	common.deviceType = map.value(name2str(common)).toMap().value(name2str(deviceType)).toString();
	m_common = common;
	auto version = map.value(name2str(common)).toMap().value(name2str(version)).toInt();
	PLS_INFO("PLSGpopData", "gpop data version = %d", version);
}

void PLSGpopData::initSnscallbackUrls()
{
	QMap<QString, SnsCallbackUrl> snscallbackUrls;
	QVariantMap map;
	PLSJsonDataHandler::getValuesFromByteArray(m_gpopDataArray, name2str(snsCallbackUrls), map);
	for (auto key : map.keys()) {
		SnsCallbackUrl callbackUrl;
		callbackUrl.callbackUrlType = key;
		callbackUrl.url = map.value(key).toString();
		snscallbackUrls.insert(key, callbackUrl);
	}
	m_snsCallbackUrls = snscallbackUrls;
}
void PLSGpopData::initSupportedPlatforms()
{
	QVariantMap map;
	m_channelList.clear();
	PLSJsonDataHandler::getValuesFromByteArray(m_gpopDataArray, name2str(supportedPlatforms), map);
	auto channelList = map.value("channel").toList();
	for (auto value : channelList) {
		m_channelList.append(value.toString());
	}

	auto loginList = map.value("login").toList();
	m_loginList.clear();
	for (auto value : loginList) {
		m_loginList.append(value.toString());
	}

	auto resolutionList = map.value("resolutionGuide").toList();
	m_channelResolutionGuidList.clear();
	for (auto value : resolutionList) {
		m_channelResolutionGuidList.append(value.toString());
	}
}

QMap<QString, SnsCallbackUrl> PLSGpopData::getSnscallbackUrls()
{
	if (m_snsCallbackUrls.isEmpty()) {
		initSnscallbackUrls();
	}
	return m_snsCallbackUrls;
}

QMap<QString, SnsCallbackUrl> PLSGpopData::getDefaultSnscallbackUrls()
{
	return m_defaultCallbackUrls;
}

const QStringList &PLSGpopData::getChannelList()
{
	initSupportedPlatforms();
	auto serviceName = PLSLOGINUSERINFO->getNCPPlatformServiceName();
	PLS_INFO("PLSGpopData", "current select service name = %s", serviceName.toUtf8().constData());

	auto existInter = std::find_if(m_channelList.begin(), m_channelList.end(), [serviceName](const QString &channelName) { return 0 == serviceName.compare(channelName, Qt::CaseInsensitive); });
	if (existInter == m_channelList.end() && !serviceName.isEmpty()) {
		m_channelList.insert(0, serviceName);
	}
	return m_channelList.isEmpty() ? m_defaultChannelList : m_channelList;
}

const QStringList &PLSGpopData::getChannelResolutionGuidList()
{
	return m_channelResolutionGuidList.isEmpty() ? m_defaultChannelResolutionGuidList : m_channelResolutionGuidList;
}

const QStringList &PLSGpopData::getLoginList() const
{
	return m_loginList.isEmpty() ? m_defaultLoginList : m_loginList;
}

int PLSGpopData::getUIBlockingTimeS() const
{
	return uiBlockingTimeS;
}

int PLSGpopData::getYoutubeHealthStatusInterval() const
{
	int interval = 10;

	return 10;
}

void PLSGpopData::initGpopData()
{
	PLS_INFO("PLSGpopData", "start init gpop data.");
	initDefaultValues();
	initCommon();
	initSnscallbackUrls();
	initSupportedPlatforms();
}