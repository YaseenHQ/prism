#include "PLSLiveInfoNaverShoppingLIVEProductItemView.h"
#include "ui_PLSLiveInfoNaverShoppingLIVEProductItemView.h"
#include "PLSNaverShoppingLIVEItemViewCache.h"
#include "PLSNaverShoppingLIVEDataManager.h"
#include "PLSLiveInfoNaverShoppingLIVE.h"
#include "PLSNaverShoppingLIVEProductDialogView.h"
#include "PLSLoadingView.h"
#include "frontend-api.h"
#include "liblog.h"
#include <QDesktopServices>

using ItemViewCache = PLSNaverShoppingLIVEItemViewCache<PLSLiveInfoNaverShoppingLIVEProductItemView>;

void naverShoppingLIVEProductItemView_init(QWidget *itemView, QPushButton *title)
{
	title->setMouseTracking(itemView);
	title->installEventFilter(itemView);
}
void naverShoppingLIVEProductItemView_event(const QWidget *, const QWidget *, QEvent *event, const QString &url, bool isInLiveinfo)
{
	switch (event->type()) {
	case QEvent::MouseButtonRelease:
		if (static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton) {
			PLS_UI_STEP(isInLiveinfo ? MODULE_NAVER_SHOPPING_LIVE_LIVEINFO : MODULE_NAVER_SHOPPING_LIVE_PRODUCT_MANAGER, "Product Title", ACTION_CLICK);
			QDesktopServices::openUrl(url);
		}
		break;
	default:
		break;
	}
}
void naverShoppingLIVEProductItemView_title_mouseEnter(QWidget *widget)
{
	widget->setCursor(Qt::PointingHandCursor);
	pls_flush_style(widget);
}
void naverShoppingLIVEProductItemView_title_mouseLeave(QWidget *widget)
{
	widget->setCursor(Qt::ArrowCursor);
	pls_flush_style(widget);
}

PLSLiveInfoNaverShoppingLIVEProductItemView::PLSLiveInfoNaverShoppingLIVEProductItemView(QWidget *parent) : QFrame(parent)
{
	ui = pls_new<Ui::PLSLiveInfoNaverShoppingLIVEProductItemView>();
	setAttribute(Qt::WA_Hover);
	setMouseTracking(true);
	ui->setupUi(this);
	naverShoppingLIVEProductItemView_init(this, ui->titleLabel);
	ui->titleSpacer->installEventFilter(this);
	ui->fixButton->setToolTip(tr("NaverShoppingLive.LiveInfo.FixButton.Tooltip"));
	ui->removeButton->setToolTip(tr("Delete"));

	connect(ui->fixButton, &QPushButton::clicked, this, [this]() { fixButtonClicked(this); });
	connect(ui->removeButton, &QPushButton::clicked, this, [this]() { removeButtonClicked(this); });
}

PLSLiveInfoNaverShoppingLIVEProductItemView::~PLSLiveInfoNaverShoppingLIVEProductItemView()
{
	PLSLoadingView::deleteLoadingView(imageLoadingView);
	pls_delete(ui, nullptr);
}

void PLSLiveInfoNaverShoppingLIVEProductItemView::setInfo(const PLSPlatformNaverShoppingLIVE *platform, const Product &product_, bool fixed_)
{
	product = product_;
	setFixed(product.represent);

	ui->titleLabel->setText(product.name);
	ui->storeLabel->setText(product.mallName);
	if (product.productStatus == PLSNaverShoppingLIVEDataManager::PRODUCT_STATUS_CLOSE) {
		ui->statusLabel->setText(tr("NaverShoppingLive.LiveInfo.Product.Status.Close"));
		ui->discountLabel->hide();
		ui->priceLabel->hide();
		ui->discountPriceLabel->hide();
		ui->statusLabel->show();
	} else if (product.productStatus != PLSNaverShoppingLIVEDataManager::PRODUCT_STATUS_SALE) {
		ui->statusLabel->setText(tr("NaverShoppingLive.LiveInfo.Product.Status.Other"));
		ui->discountLabel->hide();
		ui->priceLabel->hide();
		ui->discountPriceLabel->hide();
		ui->statusLabel->show();
	} else if (product.specialPriceIsValid()) { //"specialPrice" was removed in api
		ui->discountLabel->setText(PLSNaverShoppingLIVEDataManager::convertRate(product.discountRate));
		ui->priceLabel->setText(tr("NaverShoppingLive.ProductPrice").arg(PLSNaverShoppingLIVEDataManager::convertPrice(product.discountedSalePrice)));
		ui->discountPriceLabel->setText(tr("NaverShoppingLive.ProductDiscountPrice").arg(PLSNaverShoppingLIVEDataManager::convertPrice(product.specialPrice)));
		ui->statusLabel->hide();
		ui->discountLabel->setVisible(product.discountRateIsValid());
		ui->priceLabel->show();
		ui->discountPriceLabel->show();
	} else if (product.liveDiscountRateIsValid()) { // live discount
		ui->discountLabel->setText(PLSNaverShoppingLIVEDataManager::convertRate(product.liveDiscountRate));
		double price = 0;
		if (product.activeLiveDiscount) {
			price = product.discountedSalePrice;
		} else {
			price = product.salePrice - product.liveDiscountPrice;
		}
		ui->priceLabel->setText(tr("NaverShoppingLive.ProductPrice").arg(PLSNaverShoppingLIVEDataManager::convertPrice(price)));
		ui->statusLabel->hide();
		ui->discountPriceLabel->hide();
		ui->discountLabel->setVisible(product.liveDiscountRate > 0);
		ui->priceLabel->show();
	} else if (product.discountRateIsValid()) { // discount type 1
		ui->discountLabel->setText(PLSNaverShoppingLIVEDataManager::convertRate(product.discountRate));
		ui->priceLabel->setText(tr("NaverShoppingLive.ProductPrice").arg(PLSNaverShoppingLIVEDataManager::convertPrice(product.discountedSalePrice)));
		ui->statusLabel->hide();
		ui->discountPriceLabel->hide();
		ui->discountLabel->show();
		ui->priceLabel->show();
	} else {
		ui->priceLabel->setText(tr("NaverShoppingLive.ProductPrice").arg(PLSNaverShoppingLIVEDataManager::convertPrice(product.discountedSalePrice)));
		ui->statusLabel->hide();
		ui->discountLabel->hide();
		ui->discountPriceLabel->hide();
		ui->priceLabel->show();
	}

	ui->introducingLabel->setVisible(product.introducing);
	ui->fixButton->setVisible(product.productType == PLSProductType::MainProduct);
	this->setVisible(true);
	updateAttachableUI();

	PLSNaverShoppingLIVEDataManager::instance()->downloadImage(
		platform, product.imageUrl,
		[this, _itemId = this->getItemId()]() {
			if (_itemId != this->getItemId()) {
				return;
			}

			PLSLoadingView::newLoadingView(imageLoadingView, ui->iconLabel);
		},
		[this, specialPriceIsValid = product.liveDiscountRateIsValid(), _itemId = this->getItemId()](bool ok, const QString &imagePath) {
			if (_itemId != this->getItemId()) {
				return;
			}

			PLSLoadingView::deleteLoadingView(imageLoadingView);
			double opacity = product.attachable ? 1.0 : 0.3;
			if (ok) {
				ui->iconLabel->setImage(this->product.linkUrl, this->product.imageUrl, imagePath, specialPriceIsValid, true, opacity);
			} else {
				ui->iconLabel->setImage(this->product.linkUrl, QPixmap(), QPixmap(), specialPriceIsValid, true, opacity);
			}
		},
		this, [](const QObject *obj) { return pls_object_is_valid(obj); }, -1);
}

void PLSLiveInfoNaverShoppingLIVEProductItemView::setProductReadonly(bool readonly)
{
	ui->fixButton->setEnabled(!readonly);
	ui->removeButton->setEnabled(!readonly);

	ui->fixButton->setVisible(!readonly);
	ui->removeButton->setVisible(!readonly);
}

bool PLSLiveInfoNaverShoppingLIVEProductItemView::isFixed() const
{
	return fixed;
}

void PLSLiveInfoNaverShoppingLIVEProductItemView::setFixed(bool fixed_)
{
	fixed = fixed_;
	product.represent = fixed_;
	pls_flush_style(this->ui->fixButton, "fixed", fixed);
}

void PLSLiveInfoNaverShoppingLIVEProductItemView::updateAttachableUI()
{
	pls_flush_style(ui->priceLabel, "attachable", product.attachable);
	pls_flush_style(ui->discountLabel, "attachable", product.attachable);
	pls_flush_style(ui->statusLabel, "attachable", product.attachable);
	pls_flush_style(ui->discountPriceLabel, "attachable", product.attachable);
	pls_flush_style(ui->storeLabel, "attachable", product.attachable);
	pls_flush_style(ui->titleLabel, "attachable", product.attachable);
	pls_flush_style(ui->fixButton, "attachable", product.attachable);
	pls_flush_style(ui->removeButton, "attachable", product.attachable);
	pls_flush_style(this, "attachable", product.attachable);
}

qint64 PLSLiveInfoNaverShoppingLIVEProductItemView::getProductNo() const
{
	return product.productNo;
}

const PLSLiveInfoNaverShoppingLIVEProductItemView::Product &PLSLiveInfoNaverShoppingLIVEProductItemView::getProduct() const
{
	return product;
}

void PLSLiveInfoNaverShoppingLIVEProductItemView::addBatchCache(int batchCount, bool check)
{
	ItemViewCache::instance()->addBatchCache(batchCount, check);
}

void PLSLiveInfoNaverShoppingLIVEProductItemView::cleaupCache()
{
	ItemViewCache::instance()->cleaupCache();
}

PLSLiveInfoNaverShoppingLIVEProductItemView *PLSLiveInfoNaverShoppingLIVEProductItemView::alloc(QWidget *parent, PLSPlatformNaverShoppingLIVE *platform, const Product &product, bool fixed)
{
	auto view = ItemViewCache::instance()->alloc(parent, platform, product, fixed);
	view->ui->iconLabel->setFixedSize(1, 1);
	return view;
}

void PLSLiveInfoNaverShoppingLIVEProductItemView::dealloc(PLSLiveInfoNaverShoppingLIVEProductItemView *view)
{
	QObject::disconnect(view->fixButtonClickedConnection);
	QObject::disconnect(view->removeButtonClickedConnection);
	ItemViewCache::instance()->dealloc(view);
	view->ui->iconLabel->setFixedSize(1, 1);
}

void PLSLiveInfoNaverShoppingLIVEProductItemView::dealloc(QList<PLSLiveInfoNaverShoppingLIVEProductItemView *> &views, QVBoxLayout *layout)
{
	ItemViewCache::instance()->dealloc(views, layout, [](PLSLiveInfoNaverShoppingLIVEProductItemView *view) {
		QObject::disconnect(view->fixButtonClickedConnection);
		QObject::disconnect(view->removeButtonClickedConnection);
		view->ui->iconLabel->setFixedSize(1, 1);
	});
}

void PLSLiveInfoNaverShoppingLIVEProductItemView::resetIconSize(const QList<PLSLiveInfoNaverShoppingLIVEProductItemView *> &views)
{
	for (auto view : views) {
		view->ui->iconLabel->setFixedSize(1, 1);
	}
}

void PLSLiveInfoNaverShoppingLIVEProductItemView::updateWhenDpiChanged(const QList<PLSLiveInfoNaverShoppingLIVEProductItemView *> &views)
{
	for (auto view : views) {
		view->ui->titleLabel->setText(view->product.name);
	}
}

bool PLSLiveInfoNaverShoppingLIVEProductItemView::event(QEvent *event)
{
	bool result = QFrame::event(event);
	switch (event->type()) {
	case QEvent::HoverEnter:
		setProperty("state", "hover");
		naverShoppingLIVEProductItemView_title_mouseEnter(ui->titleLabel);
		ui->iconLabel->mouseEnter();
		break;
	case QEvent::HoverLeave:
		setProperty("state", "none");
		naverShoppingLIVEProductItemView_title_mouseLeave(ui->titleLabel);
		ui->iconLabel->mouseLeave();
		break;
	default:
		break;
	}
	return result;
}

bool PLSLiveInfoNaverShoppingLIVEProductItemView::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == ui->titleLabel) {
		naverShoppingLIVEProductItemView_event(this, static_cast<QWidget *>(watched), event, product.linkUrl, true);
	} else if (watched == ui->titleSpacer) {
		switch (event->type()) {
		case QEvent::Resize:
			if (static_cast<QResizeEvent *>(event)->size().width() == 0) {
				QString text = ui->titleLabel->fontMetrics().elidedText(product.name, Qt::ElideRight, ui->titleWidget->width());
				ui->titleLabel->setText(text);
			}
			break;
		default:
			break;
		}
	}

	return QFrame::eventFilter(watched, event);
}
