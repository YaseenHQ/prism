/*
 * @file      PLSLoadingEvent.h
 * @brief     The load event class
 * @date      2019/09/03
 * @author    liuying
 * @attention null
 * @version   2.0.1
 * @modify    liuying create 2019/09/03
 */
#ifndef COMMON_LOADINGEVENT_H
#define COMMON_LOADINGEVENT_H

#include <QObject>
#include <QPushButton>
#include <QPointer>
#include "pls-common-define.hpp"

/**
 * @brief The PLSLoadingEvent class
 */
class PLSLoadingEvent : public QObject {
	Q_OBJECT
public:
	explicit PLSLoadingEvent() = default;
	~PLSLoadingEvent() override = default;
	static PLSLoadingEvent *instance();
	/**
     * @brief start timer of loading picture
     * @param loadingButton
     * @param timeout
     */
	void startLoadingTimer(QWidget *loadingButton, const int timeout = common::LOADING_TIMER_TIMEROUT);

	/**
     * @brief stop timer of loading picture
     */
	void stopLoadingTimer();

protected:
	void timerEvent(QTimerEvent *e) override;

public slots:
	void handlePLSLoadingEvent();

signals:
	void startLoading();
	void stopLoading();

private:
	int m_loadingTimer = common::INTERNAL_ERROR;
	int m_loadingPictureNums = 0;
	QPointer<QWidget> m_loadingButton;
};

#endif // COMMON_LOADINGEVENT_H
