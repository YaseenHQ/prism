#ifndef PLSSCENETEMPLATEMAINSCENE_H
#define PLSSCENETEMPLATEMAINSCENE_H

#include <QWidget>
#include "flowlayout.h"
#include "loading-event.hpp"

namespace Ui {
class PLSSceneTemplateMainScene;
}

class PLSSceneTemplateMainScene : public QWidget {
	Q_OBJECT

public:
	explicit PLSSceneTemplateMainScene(QWidget *parent = nullptr);
	~PLSSceneTemplateMainScene();
	void showMainSceneView();

private:
	void initFlowLayout();
	void showLoading(QWidget *parent);
	void hideLoading();
	void updateComboBoxList();
	void showRetry();
	void hideRetry();

public slots:
	void updateSceneList();
	void refreshItems(const QString &groupId);

protected:
	bool eventFilter(QObject *watcher, QEvent *event) override;

private:
	Ui::PLSSceneTemplateMainScene *ui;
	FlowLayout *m_FlowLayout{nullptr};
	QPointer<QObject> m_pWidgetLoadingBGParent = nullptr;
	QPointer<QWidget> m_pWidgetLoadingBG = nullptr;
	PLSLoadingEvent m_loadingEvent;
	QWidget *m_pWidgetRetryContainer = nullptr;
};

#endif // PLSSCENETEMPLATEMAINSCENE_H
