#include "moc_window-dock.cpp"
#include "obs-app.hpp"
#include "window-basic-main.hpp"
#include "PLSAlertView.h"

void OBSDock::closeEvent(QCloseEvent *event)
{
	auto msgBox = []() {
		pls_check_app_exiting();
		auto result = PLSAlertView::information(pls_get_main_view(), QTStr("DockCloseWarning.Title"),
							QTStr("DockCloseWarning.Text"), QTStr("DoNotShowAgain"),
							PLSAlertView::Button::Ok, PLSAlertView::Button::Ok);
		if (result.isChecked) {
			config_set_bool(App()->GetUserConfig(), "General", "WarnedAboutClosingDocks", true);
			config_save_safe(App()->GetUserConfig(), "tmp", nullptr);
		}
	};

	bool warned = config_get_bool(App()->GetUserConfig(), "General", "WarnedAboutClosingDocks");
	if (!OBSBasic::Get()->Closing() && !warned) {
		QMetaObject::invokeMethod(App(), "Exec", Qt::QueuedConnection, Q_ARG(VoidFunc, msgBox));
	}

	PLSDock::closeEvent(event);

	if (widget() && event->isAccepted()) {
		QEvent widgetEvent(QEvent::Type(QEvent::User + QEvent::Close));
		qApp->sendEvent(widget(), &widgetEvent);
	}
}

void OBSDock::showEvent(QShowEvent *event)
{
	setProperty("vis", true);
	PLSDock::showEvent(event);
}

void OBSDockOri::closeEvent(QCloseEvent *event)
{
	auto msgBox = []() {
		pls_check_app_exiting();
		auto result = PLSAlertView::information(pls_get_main_view(), QTStr("DockCloseWarning.Title"),
							QTStr("DockCloseWarning.Text"), QTStr("DoNotShowAgain"),
							PLSAlertView::Button::Ok, PLSAlertView::Button::Ok);
		if (result.isChecked) {
			config_set_bool(App()->GetUserConfig(), "General", "WarnedAboutClosingDocks", true);
			config_save_safe(App()->GetUserConfig(), "tmp", nullptr);
		}
	};

	bool warned = config_get_bool(App()->GetUserConfig(), "General", "WarnedAboutClosingDocks");
	if (!OBSBasic::Get()->Closing() && !warned) {
		QMetaObject::invokeMethod(App(), "Exec", Qt::QueuedConnection, Q_ARG(VoidFunc, msgBox));
	}

	QDockWidget::closeEvent(event);

	if (widget() && event->isAccepted()) {
		QEvent widgetEvent(QEvent::Type(QEvent::User + QEvent::Close));
		qApp->sendEvent(widget(), &widgetEvent);
	}
}

void OBSDockOri::showEvent(QShowEvent *event)
{
	setProperty("vis", true);
	QDockWidget::showEvent(event);
}
