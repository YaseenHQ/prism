/******************************************************************************
    Copyright (C) 2014 by Hugh Bailey <obs.jim@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "obs-app.hpp"
#include "moc_window-basic-properties.cpp"
#include "window-basic-main.hpp"
#include "display-helpers.hpp"
#include <qt-wrappers.hpp>
#include <properties-view.hpp>
#include "pls-common-define.hpp"
#include "PLSGetPropertiesThread.h"

#include <QCloseEvent>
#include <QScreen>
#include <QWindow>
#include <QMessageBox>
#include <obs-data.h>
#include <obs.h>
#include <qpointer.h>
#include <util/c99defs.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#endif

#include "PLSPropertiesView.hpp"
#include "pls/pls-source.h"
#include "liblog.h"
#include "platform.hpp"
#include "PLSApp.h"
#include "frontend-api.h"
#include "PLSAlertView.h"
#include "PLSChatTemplateDataHelper.h"

using namespace std;
using namespace common;

static void CreateTransitionScene(OBSSource scene, const char *text, uint32_t color);
static inline void GetChatScaleAndCenterPos(double dpi, int baseCX, int baseCY, int windowCX, int windowCY, int &x,
					    int &y, float &scale);

OBSBasicProperties::OBSBasicProperties(QWidget *parent, OBSSource source_)
	: PLSDialogView(parent),
	  ui(new Ui::OBSBasicProperties),
	  main(qobject_cast<OBSBasic *>(parent)),
	  acceptClicked(false),
	  source(source_),
	  removedSignal(obs_source_get_signal_handler(source), "remove", OBSBasicProperties::SourceRemoved, this),
	  renamedSignal(obs_source_get_signal_handler(source), "rename", OBSBasicProperties::SourceRenamed, this),
	  oldSettings(obs_data_create())
{
	setupUi(ui);
	int cx = (int)config_get_int(App()->GetAppConfig(), "PropertiesWindow", "cx");
	int cy = (int)config_get_int(App()->GetAppConfig(), "PropertiesWindow", "cy");

	enum obs_source_type type = obs_source_get_type(source);

	const char *id = obs_source_get_id(source);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	ui->buttonBox->button(QDialogButtonBox::Ok)->setDefault(true);

	if (cx > 400 && cy > 400)
		resize(cx, cy);

	pls_source_properties_edit_start(source);

	connect(PLSGetPropertiesThread::Instance(), &PLSGetPropertiesThread::OnProperties, this,
		[this]() { UpdateOldSettings(); });

	/* The OBSData constructor increments the reference once */
	obs_data_release(oldSettings);

	OBSDataAutoRelease nd_settings = obs_source_get_settings(source);
	obs_data_apply(oldSettings, nd_settings);

	bool drawable_type = pls_is_in(type, OBS_SOURCE_TYPE_INPUT, OBS_SOURCE_TYPE_SCENE);
	bool isVirtualBackgroundSource = pls_is_equal(id, PRISM_BACKGROUND_TEMPLATE_SOURCE_ID);
	bool showFilterButton = drawable_type && (!isVirtualBackgroundSource);

	//PRISM/renjinbo/20230104/#/change to PLSPropertiesView
	view = new PLSPropertiesView(nd_settings.Get(), source, (PropertiesReloadCallback)obs_source_properties,
				     (PropertiesUpdateCallback) nullptr, // No special handling required for undo/redo
				     (PropertiesVisualUpdateCb)obs_source_update, 0, -1, showFilterButton);

	view->SetForProperty(true);
	view->setMouseTracking(true);
	// modify by xiewei issue #5324
	view->setCursor(Qt::ArrowCursor);
	view->verticalScrollBar()->setObjectName("PropertiesViewVScrollBar");

	if (pls_is_equal(id, common::GDIP_TEXT_SOURCE_ID) /*|| 0 == strcmp(id, PRISM_SPECTRALIZER_SOURCE_ID)*/) {
		view->SetCustomContentWidth(true);
	}

	if (isVirtualBackgroundSource) {
		view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	}

	// use unified default preview height of 210px, window height of 710px
	// For fixing issue #6057
	int marginViewTop = pls_conditional_select(
		pls_is_in_str(id, PRISM_MOBILE_SOURCE_ID, PRISM_CHAT_SOURCE_ID, BGM_SOURCE_ID), 10, 0);

	ui->propertiesLayout->setContentsMargins(15, marginViewTop, 6, 0);

	ui->propertiesLayout->addWidget(view);

	ui->windowSplitter->setStretchFactor(1, 1);
	ui->windowSplitter->setObjectName(OBJECT_NAME_PROPERTY_SPLITTER);

	if (type == OBS_SOURCE_TYPE_TRANSITION) {
		connect(view, &OBSPropertiesView::PropertiesRefreshed, this, &OBSBasicProperties::AddPreviewButton);
	}

	view->show();
	installEventFilter(CreateShortcutFilter(parent));

	const char *name = obs_source_get_name(source);
	setWindowTitle(QTStr("Basic.PropertiesWindow").arg(QT_UTF8(name)));

	obs_source_inc_showing(source);

	updatePropertiesSignal.Connect(obs_source_get_signal_handler(source), "update_properties",
				       OBSBasicProperties::UpdateProperties, this);

	auto addDrawCallback = [this]() {
		obs_display_add_draw_callback(ui->preview->GetDisplay(), OBSBasicProperties::DrawPreview, this);
	};
	auto addTransitionDrawCallback = [this]() {
		obs_display_add_draw_callback(ui->preview->GetDisplay(), OBSBasicProperties::DrawTransitionPreview,
					      this);
	};
	uint32_t caps = obs_source_get_output_flags(source);
	bool drawable_preview = (caps & OBS_SOURCE_VIDEO) != 0;

	if (drawable_preview && drawable_type) {
		ui->preview->show();
		connect(ui->preview, &OBSQTDisplay::DisplayCreated, addDrawCallback);

	} else if (type == OBS_SOURCE_TYPE_TRANSITION) {
		sourceA = obs_source_create_private("scene", "sourceA", nullptr);
		sourceB = obs_source_create_private("scene", "sourceB", nullptr);

		uint32_t colorA = 0xFFB26F52;
		uint32_t colorB = 0xFF6FB252;

		CreateTransitionScene(sourceA.Get(), "A", colorA);
		CreateTransitionScene(sourceB.Get(), "B", colorB);

		/**
		 * The cloned source is made from scratch, rather than using
		 * obs_source_duplicate, as the stinger transition would not
		 * play correctly otherwise.
		 */

		OBSDataAutoRelease settings = obs_source_get_settings(source);

		sourceClone = obs_source_create_private(obs_source_get_id(source), "clone", settings);

		obs_source_inc_active(sourceClone);
		obs_transition_set(sourceClone, sourceA);

		auto updateCallback = [=]() {
			OBSDataAutoRelease settings = obs_source_get_settings(source);
			obs_source_update(sourceClone, settings);

			obs_transition_clear(sourceClone);
			obs_transition_set(sourceClone, sourceA);
			obs_transition_force_stop(sourceClone);

			direction = true;
		};

		connect(view, &OBSPropertiesView::Changed, updateCallback);

		ui->preview->show();
		connect(ui->preview, &OBSQTDisplay::DisplayCreated, addTransitionDrawCallback);

	} else {
		ui->preview->hide();
	}

#if defined(Q_OS_MACOS)
	if ((pls_is_equal(id, PRISM_LENS_SOURCE_ID) || pls_is_equal(id, PRISM_LENS_MOBILE_SOURCE_ID) ||
	     pls_is_equal(id, OBS_DSHOW_SOURCE_ID_V2) || pls_is_equal(id, OBS_DSHOW_SOURCE_ID)) &&
	    pls_lens_needs_reboot()) {

		QMetaObject::invokeMethod(
			this,
			[this]() {
				PLSAlertView::warning(this, QTStr("Alert.Title"), QTStr("source.lens.upgrade.notice"));
			},
			Qt::QueuedConnection);
	}
#endif

	m_dpi = devicePixelRatioF();

	//#PRISM_PC-1571 ren.jinbo windows need add a item after linked label, so can show mouse
	QWidget *placeholderMouse = new QWidget(this);
	placeholderMouse->hide();
	ui->propertiesLayout->addWidget(placeholderMouse);
}

OBSBasicProperties::~OBSBasicProperties()
{
	pls_source_properties_edit_end(source, m_isSaveClick);

	if (sourceClone) {
		obs_source_dec_active(sourceClone);
	}
	obs_source_dec_showing(source);
	main->SaveProject();
	main->UpdateContextBarDeferred(true);
}

void OBSBasicProperties::AddPreviewButton()
{
	QPushButton *playButton = new QPushButton(QTStr("PreviewTransition"), this);
	view->addWidgetToBottom(playButton);

	playButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

	auto play = [=]() {
		OBSSource start;
		OBSSource end;

		if (direction) {
			start = sourceA;
			end = sourceB;
		} else {
			start = sourceB;
			end = sourceA;
		}

		obs_transition_set(sourceClone, start);
		obs_transition_start(sourceClone, OBS_TRANSITION_MODE_AUTO, main->GetTransitionDuration(), end);
		direction = !direction;

		start = nullptr;
		end = nullptr;
	};

	connect(playButton, &QPushButton::clicked, play);
}

static obs_source_t *CreateLabel(const char *name, size_t h)
{
	OBSDataAutoRelease settings = obs_data_create();
	OBSDataAutoRelease font = obs_data_create();

	std::string text;
	text += " ";
	text += name;
	text += " ";

#if defined(_WIN32)
	obs_data_set_string(font, "face", "Arial");
#elif defined(__APPLE__)
	obs_data_set_string(font, "face", "Helvetica");
#else
	obs_data_set_string(font, "face", "Monospace");
#endif
	obs_data_set_int(font, "flags", 1); // Bold text
	obs_data_set_int(font, "size", min(int(h), 300));

	obs_data_set_obj(settings, "font", font);
	obs_data_set_string(settings, "text", text.c_str());
	obs_data_set_bool(settings, "outline", false);

#ifdef _WIN32
	const char *text_source_id = "text_gdiplus";
#else
	const char *text_source_id = "text_ft2_source";
#endif

	obs_source_t *txtSource = obs_source_create_private(text_source_id, name, settings);

	return txtSource;
}

static void CreateTransitionScene(OBSSource scene, const char *text, uint32_t color)
{
	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_int(settings, "width", obs_source_get_width(scene));
	obs_data_set_int(settings, "height", obs_source_get_height(scene));
	obs_data_set_int(settings, "color", color);

	OBSSourceAutoRelease colorBG = obs_source_create_private("color_source", "background", settings);

	obs_scene_add(obs_scene_from_source(scene), colorBG);

	OBSSourceAutoRelease label = CreateLabel(text, obs_source_get_height(scene));
	obs_sceneitem_t *item = obs_scene_add(obs_scene_from_source(scene), label);

	vec2 size;
	vec2_set(&size, obs_source_get_width(scene),
#ifdef _WIN32
		 obs_source_get_height(scene));
#else
		 obs_source_get_height(scene) * 0.8);
#endif

	obs_sceneitem_set_bounds(item, &size);
	obs_sceneitem_set_bounds_type(item, OBS_BOUNDS_SCALE_INNER);
}

void OBSBasicProperties::SourceRemoved(void *data, calldata_t *)
{
	QMetaObject::invokeMethod(static_cast<OBSBasicProperties *>(data), "close");
}

void OBSBasicProperties::SourceRenamed(void *data, calldata_t *params)
{
	const char *name = calldata_string(params, "new_name");
	QString title = QTStr("Basic.PropertiesWindow").arg(QT_UTF8(name));

	QMetaObject::invokeMethod(static_cast<OBSBasicProperties *>(data), "setWindowTitle", Q_ARG(QString, title));
}

void OBSBasicProperties::UpdateProperties(void *data, calldata_t *)
{
	QMetaObject::invokeMethod(static_cast<OBSBasicProperties *>(data)->view, "ReloadProperties");
}

static bool ConfirmReset(QWidget *parent)
{
	QMessageBox::StandardButton button;

	button = OBSMessageBox::question(parent, QTStr("ConfirmReset.Title"), QTStr("ConfirmReset.Text"),
					 QMessageBox::Yes | QMessageBox::No);

	return button == QMessageBox::Yes;
}

void OBSBasicProperties::on_buttonBox_clicked(QAbstractButton *button)
{
	QDialogButtonBox::ButtonRole val = ui->buttonBox->buttonRole(button);

	if (val == QDialogButtonBox::AcceptRole) {

		std::string scene_uuid = obs_source_get_uuid(main->GetCurrentSceneSource());

		auto undo_redo = [scene_uuid](const std::string &data) {
			OBSDataAutoRelease settings = obs_data_create_from_json(data.c_str());
			OBSSourceAutoRelease source =
				obs_get_source_by_uuid(obs_data_get_string(settings, "undo_uuid"));
			obs_source_reset_settings(source, settings);

			obs_source_update_properties(source);

			OBSSourceAutoRelease scene_source = obs_get_source_by_uuid(scene_uuid.c_str());

			OBSBasic::Get()->SetCurrentScene(scene_source.Get(), true);
		};

		OBSDataAutoRelease new_settings = obs_data_create();
		OBSDataAutoRelease curr_settings = obs_source_get_settings(source);
		//check source is paid or not
		if (isPaidSource()) {
			return;
		}
		obs_data_apply(new_settings, curr_settings);
		obs_data_set_string(new_settings, "undo_uuid", obs_source_get_uuid(source));
		obs_data_set_string(oldSettings, "undo_uuid", obs_source_get_uuid(source));

		std::string undo_data(obs_data_get_json(oldSettings));
		std::string redo_data(obs_data_get_json(new_settings));

		if (undo_data.compare(redo_data) != 0)
			main->undo_s.add_action(QTStr("Undo.Properties").arg(obs_source_get_name(source)), undo_redo,
						undo_redo, undo_data, redo_data);

		m_isSaveClick = true; //RenJinbo/add is save button clicked
		acceptClicked = true;
		close();

		if (view->DeferUpdate())
			view->UpdateSettings();
	} else if (val == QDialogButtonBox::RejectRole) {
		OBSDataAutoRelease settings = obs_source_get_settings(source);
		obs_data_clear(settings);

		if (view->DeferUpdate())
			obs_data_apply(settings, oldSettings);
		else
			obs_source_update(source, oldSettings);

		close();

	} else if (val == QDialogButtonBox::ResetRole) {
		if (!ConfirmReset(this))
			return;

		OBSDataAutoRelease settings = obs_source_get_settings(source);
		obs_data_clear(settings);

		const char *id = obs_source_get_id(source);
		if (pls_is_equal(PRISM_SPECTRALIZER_SOURCE_ID, id) || pls_is_equal(PRISM_TIMER_SOURCE_ID, id) ||
		    pls_is_equal(PRISM_CHZZK_SPONSOR_SOURCE_ID, id)) {
			obs_data_t *data = obs_data_create();
			obs_data_set_string(data, "method", "default_properties");
			pls_source_set_private_data(source, data);
			obs_data_release(data);
		}
		if (pls_is_equal(PRISM_TEXT_TEMPLATE_ID, id)) {
			pls_get_text_motion_template_helper_instance()->initTemplateButtons();
			obs_data_t *data = obs_data_create();
			obs_data_set_string(data, "method", "default_properties");
			pls_source_set_private_data(source, data);
			obs_data_release(data);
		}
		if (pls_is_equal(PRISM_CHATV2_SOURCE_ID, id)) {
			pls_get_chat_template_helper_instance()->initTemplateButtons();
		}

		if (!view->DeferUpdate())
			obs_source_update(source, nullptr);
		if (!pls_is_equal(obs_source_get_id(source), PRISM_TIMER_SOURCE_ID)) {
			pls_source_properties_edit_start(source);
		}
		OBSPropertiesView *tmp = view;
		tmp->ReloadProperties();
	}
}

void OBSBasicProperties::DrawPreview(void *data, uint32_t cx, uint32_t cy)
{
	OBSBasicProperties *window = static_cast<OBSBasicProperties *>(data);

	if (!window->source)
		return;

	uint32_t sourceCX = max(obs_source_get_width(window->source), 1u);
	uint32_t sourceCY = max(obs_source_get_height(window->source), 1u);
	const char *sourceId = obs_source_get_id(window->source);

	int x, y;
	int newCX, newCY;
	float scale;
	float top = 0.0f;
	float bottom = float(sourceCY);

	if (pls_is_equal(sourceId, PRISM_CHAT_SOURCE_ID)) {
		GetChatScaleAndCenterPos(window->m_dpi, sourceCX, sourceCY, cx, cy, x, y, scale);
		newCX = int(scale * float(sourceCX));
		newCY = int(scale * float(sourceCY));
	} else {
		GetScaleAndCenterPos(sourceCX, sourceCY, cx, cy, x, y, scale);
		newCX = int(scale * float(sourceCX));
		newCY = int(scale * float(sourceCY));
	}

	gs_viewport_push();
	gs_projection_push();
	const bool previous = gs_set_linear_srgb(true);

	gs_ortho(0.0f, float(sourceCX), 0.0f, float(sourceCY), -100.0f, 100.0f);
	gs_set_viewport(x, y, newCX, newCY);
	obs_source_video_render(window->source);

	gs_set_linear_srgb(previous);
	gs_projection_pop();
	gs_viewport_pop();
}

void OBSBasicProperties::DrawTransitionPreview(void *data, uint32_t cx, uint32_t cy)
{
	OBSBasicProperties *window = static_cast<OBSBasicProperties *>(data);

	if (!window->sourceClone)
		return;

	uint32_t sourceCX = max(obs_source_get_width(window->sourceClone), 1u);
	uint32_t sourceCY = max(obs_source_get_height(window->sourceClone), 1u);

	int x, y;
	int newCX, newCY;
	float scale;

	GetScaleAndCenterPos(sourceCX, sourceCY, cx, cy, x, y, scale);

	newCX = int(scale * float(sourceCX));
	newCY = int(scale * float(sourceCY));

	gs_viewport_push();
	gs_projection_push();
	gs_ortho(0.0f, float(sourceCX), 0.0f, float(sourceCY), -100.0f, 100.0f);
	gs_set_viewport(x, y, newCX, newCY);

	obs_source_video_render(window->sourceClone);

	gs_projection_pop();
	gs_viewport_pop();
}

void OBSBasicProperties::Cleanup()
{
	config_set_int(App()->GetAppConfig(), "PropertiesWindow", "cx", width());
	config_set_int(App()->GetAppConfig(), "PropertiesWindow", "cy", height());

	obs_display_remove_draw_callback(ui->preview->GetDisplay(), OBSBasicProperties::DrawPreview, this);
	obs_display_remove_draw_callback(ui->preview->GetDisplay(), OBSBasicProperties::DrawTransitionPreview, this);
}

bool OBSBasicProperties::isPaidSource()
{
	bool isPaidSource = pls_source_check_paid(source);
	if (isPaidSource) {
		const char *sourceId = obs_source_get_id(source);
		PLSChatTemplateDataHelper::raisePropertyView();
		if (pls_is_equal(PRISM_CHATV2_SOURCE_ID, sourceId)) {
			OBSBasic::Get()->showsTipAndPrismPlusIntroWindow(tr("Chat.Widget.Paid.Tips"), "Chat Widget",
									 this);
		} else if (pls_is_equal(PRISM_TEXT_TEMPLATE_ID, sourceId)) {
			OBSBasic::Get()->showsTipAndPrismPlusIntroWindow(tr("Text.Template.Paid.Tips"), "Text Template",
									 this);
		}
	}
	return isPaidSource;
}

void OBSBasicProperties::closeEvent(QCloseEvent *event)
{
	isClosed = true;
	PLSDialogView::closeEvent(event);
	if (event->isAccepted())
		Cleanup();
}

bool OBSBasicProperties::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef _WIN32
	const MSG &msg = *static_cast<MSG *>(message);
	switch (msg.message) {
	case WM_MOVE:
		for (OBSQTDisplay *const display : findChildren<OBSQTDisplay *>()) {
			display->OnMove();
		}
		break;
	case WM_DISPLAYCHANGE:
		for (OBSQTDisplay *const display : findChildren<OBSQTDisplay *>()) {
			display->OnDisplayChange();
		}
	}
#else
	UNUSED_PARAMETER(message);
#endif

	return PLSToplevelView::nativeEvent(eventType, message, result);
}

void OBSBasicProperties::Init()
{
	show();
}

int OBSBasicProperties::CheckSettings()
{
	OBSDataAutoRelease currentSettings = obs_source_get_settings(source);
	const char *oldSettingsJson = obs_data_get_json(oldSettings);
	const char *currentSettingsJson = obs_data_get_json(currentSettings);
#ifdef Q_OS_WIN
	auto id = obs_source_get_id(source);
	if (pls_is_equal(id, PRISM_LENS_SOURCE_ID) || pls_is_equal(id, PRISM_LENS_MOBILE_SOURCE_ID)) {
		oldSettingsJson = pls_data_get_json(oldSettings);
		currentSettingsJson = pls_data_get_json(currentSettings);
	}
#endif // Q_OS_WIN
	return strcmp(currentSettingsJson, oldSettingsJson);
}

void OBSBasicProperties::UpdateOldSettings()
{
#ifdef Q_OS_WIN
	auto id = obs_source_get_id(source);
	if (pls_is_equal(id, PRISM_LENS_SOURCE_ID) || pls_is_equal(id, PRISM_LENS_MOBILE_SOURCE_ID)) {
		OBSDataAutoRelease currentSettings = obs_source_get_settings(source);
		obs_data_apply(oldSettings, currentSettings);
	}
#endif // Q_OS_WIN
}

bool OBSBasicProperties::ConfirmQuit()
{
	bringWindowToTop(this);
	PLSAlertView::Button button;

	button = PLSAlertView::information(
		nullptr, QTStr("Basic.PropertiesWindow.ConfirmTitle"), QTStr("Basic.PropertiesWindow.Confirm"),
		PLSAlertView::Button::Save | PLSAlertView::Button::Discard | PLSAlertView::Button::Cancel,
		PLSAlertView::Button::NoButton, -1, {{AlertKeyDisableEsc, true}, {AlertKeyDisableAltF4, true}});

	switch (button) {
	case QMessageBox::Save:
		if (isPaidSource()) {
			return false;
		}
		acceptClicked = true;
		m_isSaveClick = true; //RenJinbo/add is save button clicked
		if (view->DeferUpdate())
			view->UpdateSettings();
		// Do nothing because the settings are already updated

		break;
	case QMessageBox::Discard:
		acceptClicked = true;  //#4341 by zengqin
		m_isSaveClick = false; //RenJinbo/add is save button clicked
		obs_source_reset_settings(
			source,
			oldSettings); //RenJinbo/#1494/obs bug change obs_source_update to obs_source_reset_settings
		break;
	case QMessageBox::Cancel:
		return false;
		break;
	default:
		/* If somehow the dialog fails to show, just default to
		 * saving the settings. */
		break;
	}
	return true;
}

#pragma mark - prism add method

static inline void GetChatScaleAndCenterPos(double dpi, int baseCX, int baseCY, int windowCX, int windowCY, int &x,
					    int &y, float &scale)
{
	float WIDTH = float(365.f * dpi);

	auto newCX = int(WIDTH);
	auto newCY = int(float(WIDTH * float(baseCY)) / float(baseCX));
	scale = float(newCX) / float(baseCX);
	x = windowCX / 2 - newCX / 2;
	y = windowCY - newCY;
}

void OBSBasicProperties::reject()
{
	if (!acceptClicked && (CheckSettings() != 0)) {
		if (!ConfirmQuit()) {
			return;
		}
	}

	Cleanup();
	done(0);
}

OBSSource OBSBasicProperties::GetSource() const
{
	return source;
}

void OBSBasicProperties::ReloadProperties()
{
	if (!view) {
		return;
	}

	OBSPropertiesView *tmp = view;
	tmp->ReloadProperties();
}

void OBSBasicProperties::paintEvent(QPaintEvent *event)
{
	PLSDialogView::paintEvent(event);
	m_dpi = devicePixelRatioF();
}
