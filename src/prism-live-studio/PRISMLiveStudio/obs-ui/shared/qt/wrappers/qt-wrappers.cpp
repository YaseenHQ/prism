/******************************************************************************
    Copyright (C) 2023 by Lain Bailey <lain@obsproject.com>

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

#include "moc_qt-wrappers.cpp"

#include <util/threading.h>
#include <QWidget>
#include <QLayout>
#include <QComboBox>
#include <QMessageBox>
#include <QDataStream>
#include <QKeyEvent>
#include <QFileDialog>
#include <QStandardItemModel>
#include <QLabel>
#include <QPushButton>
#include <QToolBar>

#include "PLSMessageBox.h"

static inline void OBSErrorBoxva(QWidget *parent, const char *msg, va_list args)
{
	char full_message[8192];
	vsnprintf(full_message, sizeof(full_message), msg, args);
	PLSMessageBox::critical(parent, "Error", full_message);
}

void OBSErrorBox(QWidget *parent, const char *msg, ...)
{
	va_list args;
	va_start(args, msg);
	OBSErrorBoxva(parent, msg, args);
	va_end(args);
}

QMessageBox::StandardButton OBSMessageBox::question(QWidget *parent, const QString &title, const QString &text,
						    QMessageBox::StandardButtons buttons,
						    QMessageBox::StandardButton defaultButton)
{
	return pls_to_standard_button(PLSMessageBox::question(parent, title, text, pls_to_standard_buttons(buttons),
							      pls_to_standard_button(defaultButton)));
}

void OBSMessageBox::information(QWidget *parent, const QString &title, const QString &text)
{
	PLSMessageBox::information(parent, title, text);
}

void OBSMessageBox::warning(QWidget *parent, const QString &title, const QString &text, bool enableRichText)
{
	PLSMessageBox::warning(parent, title, text, enableRichText);
}

void OBSMessageBox::critical(QWidget *parent, const QString &title, const QString &text)
{
	PLSMessageBox::critical(parent, title, text);
}

uint32_t TranslateQtKeyboardEventModifiers(Qt::KeyboardModifiers mods)
{
	int obsModifiers = INTERACT_NONE;

	if (mods.testFlag(Qt::ShiftModifier))
		obsModifiers |= INTERACT_SHIFT_KEY;
	if (mods.testFlag(Qt::AltModifier))
		obsModifiers |= INTERACT_ALT_KEY;
#ifdef __APPLE__
	// Mac: Meta = Control, Control = Command
	if (mods.testFlag(Qt::ControlModifier))
		obsModifiers |= INTERACT_COMMAND_KEY;
	if (mods.testFlag(Qt::MetaModifier))
		obsModifiers |= INTERACT_CONTROL_KEY;
#else
	// Handle windows key? Can a browser even trap that key?
	if (mods.testFlag(Qt::ControlModifier))
		obsModifiers |= INTERACT_CONTROL_KEY;
	if (mods.testFlag(Qt::MetaModifier))
		obsModifiers |= INTERACT_COMMAND_KEY;

#endif

	return obsModifiers;
}

QDataStream &operator<<(QDataStream &out, const std::vector<std::shared_ptr<OBSSignal>> &)
{
	return out;
}

QDataStream &operator>>(QDataStream &in, std::vector<std::shared_ptr<OBSSignal>> &)
{
	return in;
}

QDataStream &operator<<(QDataStream &out, const OBSScene &scene)
{
	return out << QString(obs_source_get_uuid(obs_scene_get_source(scene)));
}

QDataStream &operator>>(QDataStream &in, OBSScene &scene)
{
	QString uuid;

	in >> uuid;

	OBSSourceAutoRelease source = obs_get_source_by_uuid(QT_TO_UTF8(uuid));
	scene = obs_scene_from_source(source);

	return in;
}

QDataStream &operator<<(QDataStream &out, const OBSSource &source)
{
	return out << QString(obs_source_get_uuid(source));
}

QDataStream &operator>>(QDataStream &in, OBSSource &source)
{
	QString uuid;

	in >> uuid;

	OBSSourceAutoRelease source_ = obs_get_source_by_uuid(QT_TO_UTF8(uuid));
	source = source_;

	return in;
}

void DeleteLayout(QLayout *layout)
{
	if (!layout)
		return;

	for (;;) {
		QLayoutItem *item = layout->takeAt(0);
		if (!item)
			break;

		QLayout *subLayout = item->layout();
		if (subLayout) {
			DeleteLayout(subLayout);
		} else {
			delete item->widget();
			delete item;
		}
	}

	delete layout;
}

class QuickThread : public QThread {
public:
	explicit inline QuickThread(std::function<void()> func_) : func(func_) {}

private:
	virtual void run() override { func(); }

	std::function<void()> func;
};

QThread *CreateQThread(std::function<void()> func)
{
	return new QuickThread(func);
}

volatile long insideEventLoop = 0;

void ExecuteFuncSafeBlock(std::function<void()> func)
{
	QEventLoop eventLoop;

	auto wait = [&]() {
		func();
		QMetaObject::invokeMethod(&eventLoop, "quit", Qt::QueuedConnection);
	};

	os_atomic_inc_long(&insideEventLoop);
	QScopedPointer<QThread> thread(CreateQThread(wait));
	thread->start();
	eventLoop.exec();
	thread->wait();
	os_atomic_dec_long(&insideEventLoop);
}

void ExecuteFuncSafeBlockMsgBox(std::function<void()> func, const QString &title, const QString &text)
{
	QMessageBox dlg;
	dlg.setWindowFlags(dlg.windowFlags() & ~Qt::WindowCloseButtonHint);
	dlg.setWindowTitle(title);
	dlg.setText(text);
	dlg.setStandardButtons(QMessageBox::StandardButtons());

	auto wait = [&]() {
		func();
		QMetaObject::invokeMethod(&dlg, "accept", Qt::QueuedConnection);
	};

	os_atomic_inc_long(&insideEventLoop);
	QScopedPointer<QThread> thread(CreateQThread(wait));
	thread->start();
	dlg.exec();
	thread->wait();
	os_atomic_dec_long(&insideEventLoop);
}

static bool enable_message_boxes = false;

void EnableThreadedMessageBoxes(bool enable)
{
	enable_message_boxes = enable;
}

void ExecThreadedWithoutBlocking(std::function<void()> func, const QString &title, const QString &text)
{
	if (!enable_message_boxes)
		ExecuteFuncSafeBlock(func);
	else
		ExecuteFuncSafeBlockMsgBox(func, title, text);
}

bool LineEditCanceled(QEvent *event)
{
	if (event->type() == QEvent::KeyPress) {
		QKeyEvent *keyEvent = reinterpret_cast<QKeyEvent *>(event);
		return keyEvent->key() == Qt::Key_Escape;
	}

	return false;
}

bool LineEditChanged(QEvent *event)
{
	if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
		QKeyEvent *keyEvent = reinterpret_cast<QKeyEvent *>(event);

		switch (keyEvent->key()) {
		case Qt::Key_Tab:
		case Qt::Key_Backtab:
		case Qt::Key_Enter:
		case Qt::Key_Return:
			return true;
		}
	} else if (event->type() == QEvent::FocusOut) {
		return true;
	}

	return false;
}

void SetComboItemEnabled(QComboBox *c, int idx, bool enabled)
{
	QStandardItemModel *model = dynamic_cast<QStandardItemModel *>(c->model());
	QStandardItem *item = model->item(idx);
	item->setFlags(enabled ? Qt::ItemIsSelectable | Qt::ItemIsEnabled : Qt::NoItemFlags);
}

void setClasses(QWidget *widget, const QString &newClasses)
{
	if (widget->property("class").toString() != newClasses) {
		widget->setProperty("class", newClasses);

		/* force style sheet recalculation */
		QString qss = widget->styleSheet();
		widget->setStyleSheet("/* */");
		widget->setStyleSheet(qss);
	}
}
QString SelectDirectory(QWidget *parent, QString title, QString path)
{
	pls::HotKeyLocker locker;
	QString dir = QFileDialog::getExistingDirectory(parent, title, path,
							QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

	return dir;
}

QString SaveFile(QWidget *parent, QString title, QString path, QString extensions)
{
	pls::HotKeyLocker locker;
	QString file = QFileDialog::getSaveFileName(parent, title, path, extensions);

	return file;
}

QString OpenFile(QWidget *parent, QString title, QString path, QString extensions)
{
	pls::HotKeyLocker locker;
	QString file = QFileDialog::getOpenFileName(parent, title, path, extensions);

	return file;
}

QStringList OpenFiles(QWidget *parent, QString title, QString path, QString extensions)
{
	pls::HotKeyLocker locker;
	QStringList files = QFileDialog::getOpenFileNames(parent, title, path, extensions);

	return files;
}

static void SetLabelText(QLabel *label, const QString &newText)
{
	if (label->text() != newText)
		label->setText(newText);
}

void TruncateLabel(QLabel *label, QString newText, int length)
{
	if (newText.length() < length) {
		label->setToolTip(QString());
		SetLabelText(label, newText);
		return;
	}

	label->setToolTip(newText);
	newText.truncate(length);
	newText += "...";

	SetLabelText(label, newText);
}

void RefreshToolBarStyling(QToolBar *toolBar)
{
	for (QAction *action : toolBar->actions()) {
		QWidget *widget = toolBar->widgetForAction(action);

		if (!widget)
			continue;

		widget->style()->unpolish(widget);
		widget->style()->polish(widget);
	}
}
