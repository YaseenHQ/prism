#include "PLSSceneTemplateMainSceneInstallView.h"
#include "ui_PLSSceneTemplateMainSceneInstallView.h"
#include "frontend-api.h"
#include "PLSSceneTemplateMediaManage.h"

PLSSceneTemplateMainSceneInstallView::PLSSceneTemplateMainSceneInstallView(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PLSSceneTemplateMainSceneInstallView)
{
    ui->setupUi(this);
	setAttribute(Qt::WA_StyledBackground);
}

PLSSceneTemplateMainSceneInstallView::~PLSSceneTemplateMainSceneInstallView()
{
	delete ui;
}

void PLSSceneTemplateMainSceneInstallView::updateUI(const SceneTemplateItem &model)
{
    m_item = model;
}

void PLSSceneTemplateMainSceneInstallView::on_detailButton_clicked() 
{
    PLS_SCENE_TEMPLATE_MEDIA_MANAGE->enterDetailScenePage(m_item);
}

void PLSSceneTemplateMainSceneInstallView::on_installButton_clicked() 
{
    ui->installButton->startInstall();
    bool res = pls_install_scene_template(m_item);
    ui->installButton->endInstall();
}
