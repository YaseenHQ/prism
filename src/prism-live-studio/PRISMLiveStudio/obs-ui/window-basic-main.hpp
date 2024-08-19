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

#pragma once

#include <QBuffer>
#include <QAction>
#include <QThread>
#include <QWidgetAction>
#include <QSystemTrayIcon>
#include <QStyledItemDelegate>
#include <obs.hpp>
#include <vector>
#include <memory>
#include <QFileDialog>
#include "window-main.hpp"
#include "window-basic-interaction.hpp"
#include "window-basic-vcam.hpp"
#include "window-basic-properties.hpp"
#include "window-basic-transform.hpp"
#include "window-basic-adv-audio.hpp"
#include "window-basic-filters.hpp"
#include "window-missing-files.hpp"
#include "window-projector.hpp"
#include "window-basic-about.hpp"
#include "window-dock-youtube-app.hpp"
#include "window-basic-color-select.hpp"
#include "auth-base.hpp"
#include "log-viewer.hpp"
#include "undo-stack-obs.hpp"
#include "PLSSceneCollectionView.h"
#include "PLSSceneCollectionManagement.h"
#include "PLSMenuPushButton.h"
#include "PLSSceneTemplateModel.h"

#include <obs-frontend-internal.hpp>
#include <frontend-internal.hpp>
#include <util/platform.h>
#include <util/threading.h>
#include <util/util.hpp>

#include <QPointer>
#include "PLSBasicProperties.hpp"
#include "PLSOutro.hpp"
#include "audio-mixer/PLSAudioMixer.h"

class QMessageBox;
class QListWidgetItem;
class VolControl;
class OBSBasicStats;
class OBSBasicVCamConfig;
class PLSApp;
class PLSAudioControl;
class PLSPreviewProgramTitle;
class PLSWatermark;
class PLSPlatformBase;

#include "ui_OBSBasic.h"
#include "ui_ColorSelect.h"

#include "../prism-ui/main/PLSMainView.hpp"

#define DESKTOP_AUDIO_1 Str("DesktopAudioDevice1")
#define DESKTOP_AUDIO_2 Str("DesktopAudioDevice2")
#define AUX_AUDIO_1 Str("AuxAudioDevice1")
#define AUX_AUDIO_2 Str("AuxAudioDevice2")
#define AUX_AUDIO_3 Str("AuxAudioDevice3")
#define AUX_AUDIO_4 Str("AuxAudioDevice4")

#define SIMPLE_ENCODER_X264 "x264"
#define SIMPLE_ENCODER_X264_LOWCPU "x264_lowcpu"
#define SIMPLE_ENCODER_QSV "qsv"
#define SIMPLE_ENCODER_QSV_AV1 "qsv_av1"
#define SIMPLE_ENCODER_NVENC "nvenc"
#define SIMPLE_ENCODER_NVENC_AV1 "nvenc_av1"
#define SIMPLE_ENCODER_NVENC_HEVC "nvenc_hevc"
#define SIMPLE_ENCODER_AMD "amd"
#define SIMPLE_ENCODER_AMD_HEVC "amd_hevc"
#define SIMPLE_ENCODER_AMD_AV1 "amd_av1"
#define SIMPLE_ENCODER_APPLE_H264 "apple_h264"
#define SIMPLE_ENCODER_APPLE_HEVC "apple_hevc"

#define PREVIEW_EDGE_SIZE 10

enum class LoadSceneCollectionWay {
	RunPrismImmediately = 0,
	RunPscWhenNoPrism,
	RunPscWhenPrismExisted,
	ImportSceneCollection,
	ImportSceneCollectionFromExport,
	ImportSceneTemplates
};

enum class SceneSetOperatorType {
	AddNewSceneSet = 0,
	CopySceneSet,
	RenameSceneSet
};

struct BasicOutputHandler;
using ExprotCallback = void (*)();
using DShowSourceVecType = std::vector<std::pair<QString, OBSSceneItem>>;

enum class QtDataRole {
	OBSRef = Qt::UserRole,
	OBSSignals,
};

struct SavedProjectorInfo {
	ProjectorType type;
	int monitor;
	std::string geometry;
	std::string name;
	bool alwaysOnTop;
	bool alwaysOnTopOverridden;
};

struct SourceCopyInfo {
	OBSWeakSource weak_source;
	bool visible;
	obs_sceneitem_crop crop;
	obs_transform_info transform;
	obs_blending_method blend_method;
	obs_blending_type blend_mode;
};

struct QuickTransition {
	QPushButton *button = nullptr;
	OBSSource source;
	obs_hotkey_id hotkey = OBS_INVALID_HOTKEY_ID;
	int duration = 0;
	int id = 0;
	bool fadeToBlack = false;

	inline QuickTransition() {}
	inline QuickTransition(OBSSource source_, int duration_, int id_,
			       bool fadeToBlack_ = false)
		: source(source_),
		  duration(duration_),
		  id(id_),
		  fadeToBlack(fadeToBlack_),
		  renamedSignal(std::make_shared<OBSSignal>(
			  obs_source_get_signal_handler(source), "rename",
			  SourceRenamed, this))
	{
	}

private:
	static void SourceRenamed(void *param, calldata_t *data);
	std::shared_ptr<OBSSignal> renamedSignal;
};

class ColorSelect : public QWidget {

public:
	explicit ColorSelect(QWidget *parent = 0);

private:
	std::unique_ptr<Ui::ColorSelect> ui;
};

class OBSBasic : public OBSMainWindow {
	Q_OBJECT
	Q_PROPERTY(QIcon imageIcon READ GetImageIcon WRITE SetImageIcon
			   DESIGNABLE true)
	Q_PROPERTY(QIcon colorIcon READ GetColorIcon WRITE SetColorIcon
			   DESIGNABLE true)
	Q_PROPERTY(QIcon slideshowIcon READ GetSlideshowIcon WRITE
			   SetSlideshowIcon DESIGNABLE true)
	Q_PROPERTY(QIcon audioInputIcon READ GetAudioInputIcon WRITE
			   SetAudioInputIcon DESIGNABLE true)
	Q_PROPERTY(QIcon audioOutputIcon READ GetAudioOutputIcon WRITE
			   SetAudioOutputIcon DESIGNABLE true)
	Q_PROPERTY(QIcon desktopCapIcon READ GetDesktopCapIcon WRITE
			   SetDesktopCapIcon DESIGNABLE true)
	Q_PROPERTY(QIcon windowCapIcon READ GetWindowCapIcon WRITE
			   SetWindowCapIcon DESIGNABLE true)
	Q_PROPERTY(QIcon gameCapIcon READ GetGameCapIcon WRITE SetGameCapIcon
			   DESIGNABLE true)
	Q_PROPERTY(QIcon cameraIcon READ GetCameraIcon WRITE SetCameraIcon
			   DESIGNABLE true)
	Q_PROPERTY(QIcon textIcon READ GetTextIcon WRITE SetTextIcon
			   DESIGNABLE true)
	Q_PROPERTY(QIcon mediaIcon READ GetMediaIcon WRITE SetMediaIcon
			   DESIGNABLE true)
	Q_PROPERTY(QIcon browserIcon READ GetBrowserIcon WRITE SetBrowserIcon
			   DESIGNABLE true)
	Q_PROPERTY(QIcon groupIcon READ GetGroupIcon WRITE SetGroupIcon
			   DESIGNABLE true)
	Q_PROPERTY(QIcon sceneIcon READ GetSceneIcon WRITE SetSceneIcon
			   DESIGNABLE true)
	Q_PROPERTY(QIcon defaultIcon READ GetDefaultIcon WRITE SetDefaultIcon
			   DESIGNABLE true)
	Q_PROPERTY(QIcon audioProcessOutputIcon READ GetAudioProcessOutputIcon
			   WRITE SetAudioProcessOutputIcon DESIGNABLE true)

	friend class OBSAbout;
	friend class OBSBasicPreview;
	friend class OBSBasicStatusBar;
	friend class OBSBasicSourceSelect;
	friend class OBSBasicTransform;
	friend class OBSBasicSettings;
	friend class Auth;
	friend class AutoConfig;
	friend class AutoConfigStreamPage;
	friend class RecordButton;
	friend class ControlsSplitButton;
	friend class ExtraBrowsersModel;
	friend class ExtraBrowsersDelegate;
	friend class DeviceCaptureToolbar;
	friend class DeviceToolbarPropertiesThread;
	friend class OBSBasicSourceSelect;
	friend class OBSYoutubeActions;
	friend class OBSPermissions;
	friend struct BasicOutputHandler;
	friend struct OBSStudioAPI;
	friend class ScreenshotObj;
	friend class PLSBasic;
	friend class PLSApp;
	friend class PLSAudioControl;

	enum class MoveDir { Up, Down, Left, Right };

	enum DropType {
		DropType_RawText,
		DropType_Text,
		DropType_Image,
		DropType_Media,
		DropType_Html,
		DropType_Url,
	};

	enum ContextBarSize {
		ContextBarSize_Minimized,
		ContextBarSize_Reduced,
		ContextBarSize_Normal
	};

	enum class CenterType {
		Scene,
		Vertical,
		Horizontal,
	};

private:
	pls_frontend_callbacks *api = nullptr;

	std::shared_ptr<Auth> auth;

	std::vector<VolControl *> volumes;

	std::vector<OBSSignal> signalHandlers;

	QList<QPointer<QDockWidget>> oldExtraDocks;
	QStringList oldExtraDockNames;

	OBSDataAutoRelease safeModeModuleData;
	std::vector<OBSDataAutoRelease> safeModeTransitions;

	bool loaded = false;
	long disableSaving = 1;
	bool projectChanged = false;
	bool previewEnabled = true;
	ContextBarSize contextBarSize = ContextBarSize_Normal;

	std::deque<SourceCopyInfo> clipboard;
	OBSWeakSourceAutoRelease copyFiltersSource;
	bool copyVisible = true;
	obs_transform_info copiedTransformInfo;
	obs_sceneitem_crop copiedCropInfo;
	bool hasCopiedTransform = false;
	OBSWeakSourceAutoRelease copySourceTransition;

	bool loadingScene = false;
	QAction *actionSeperateScene;
	QAction *actionSeperateSource;
	QAction *actionSperateMixer;
	QAction *actionSeperateChat;

	QAction *actionMixerLayout;

	bool sceneChanging = false;
	bool ignoreSelectionUpdate = false;
	bool collectionChanging = false;
	bool isCreateSouceInLoading = true;

	bool alreadyShowSceneMethodAlert = false;
	bool deferShowSceneMethodAlert = false;

	QString lastCollectionPath;
	OBSDataAutoRelease source_recent_color_config{};
	QString lastProfilePath;

	bool closing = false;
	bool clearingFailed = false;

	QScopedPointer<QThread> devicePropertiesThread;
	QScopedPointer<QThread> whatsNewInitThread;
	QScopedPointer<QThread> updateCheckThread;
	QScopedPointer<QThread> introCheckThread;
	QScopedPointer<QThread> logUploadThread;

	QPointer<OBSBasicInteraction> interaction;
	QPointer<PLSBasicProperties> properties;
	QPointer<OBSBasicTransform> transformWindow;
	QPointer<OBSBasicAdvAudio> advAudioWindow;
	QPointer<OBSBasicFilters> filters;
	QPointer<QDockWidget> statsDock;

	QPointer<YouTubeAppDock> youtubeAppDock;
	uint64_t lastYouTubeAppDockCreationTime = 0;

	QPointer<OBSAbout> about;
	QPointer<OBSMissingFiles> missDialog;
	QPointer<OBSLogViewer> logView;

	QPointer<QTimer> cpuUsageTimer;
	QPointer<QTimer> diskFullTimer;

	QPointer<QTimer> nudge_timer;
	bool recent_nudge = false;

	os_cpu_usage_info_t *cpuUsageInfo = nullptr;

	OBSService service;
	std::unique_ptr<BasicOutputHandler> outputHandler;
	bool streamingStopping = false;
	bool recordingStopping = false;
	bool replayBufferStopping = false;

	gs_vertbuffer_t *box = nullptr;
	gs_vertbuffer_t *boxLeft = nullptr;
	gs_vertbuffer_t *boxTop = nullptr;
	gs_vertbuffer_t *boxRight = nullptr;
	gs_vertbuffer_t *boxBottom = nullptr;
	gs_vertbuffer_t *circle = nullptr;

	gs_vertbuffer_t *actionSafeMargin = nullptr;
	gs_vertbuffer_t *graphicsSafeMargin = nullptr;
	gs_vertbuffer_t *fourByThreeSafeMargin = nullptr;
	gs_vertbuffer_t *leftLine = nullptr;
	gs_vertbuffer_t *topLine = nullptr;
	gs_vertbuffer_t *rightLine = nullptr;

	int previewX = 0, previewY = 0;
	int previewCX = 0, previewCY = 0;
	float previewScale = 0.0f;

	ConfigFile basicConfig;

	std::vector<SavedProjectorInfo *> savedProjectorsArray;
	QList<QPair<QPointer<PLSDialogView>, OBSProjector *>> projectors;

	QPointer<QWidget> stats;
	QPointer<QWidget> remux;
	QPointer<QWidget> extraBrowsers;
	QPointer<QWidget> importer;

	QPointer<QMenu> startStreamMenu;

	QPointer<QPushButton> transitionButton;
	QPointer<ControlsSplitButton> replayBufferButton;
	QScopedPointer<QPushButton> pause;

	QPointer<ControlsSplitButton> vcamButton;
	bool vcamEnabled = false;
	VCamConfig vcamConfig;

	QScopedPointer<QSystemTrayIcon> trayIcon;
	QPointer<QAction> sysTrayStream;
	QPointer<QAction> sysTrayRecord;
	QPointer<QAction> sysTrayReplayBuffer;
	QPointer<QAction> sysTrayVirtualCam;
	QPointer<QAction> sysTrayBanner;
	QPointer<QAction> showHide;
	QPointer<QAction> exit;
	QPointer<QMenu> trayMenu;
	QPointer<QMenu> previewProjector;
	QPointer<QMenu> multiviewProjectorMenu;
	QPointer<QMenu> studioProgramProjector;
	QPointer<QMenu> previewProjectorSource;
	QPointer<QMenu> previewProjectorMain;
	QPointer<QMenu> sceneProjectorMenu;
	QPointer<QMenu> sourceProjector;
	QPointer<QMenu> scaleFilteringMenu;
	QPointer<QMenu> blendingMethodMenu;
	QPointer<QMenu> blendingModeMenu;
	QPointer<QMenu> colorMenu;
	QPointer<QWidgetAction> colorWidgetAction;
	QPointer<ColorSelectNew> colorSelect;
	QPointer<QMenu> deinterlaceMenu;
	QPointer<QMenu> perSceneTransitionMenu;
	QPointer<QObject> shortcutFilter;
	QPointer<QAction> renameScene;
	QPointer<QAction> renameSource;

	QPointer<QWidget> programWidget;
	QPointer<QVBoxLayout> programLayout;
	QPointer<QLabel> programLabel;

	QScopedPointer<QThread> patronJsonThread;
	std::string patronJson;

	QPointer<PLSMainView> mainView;

	PLSSceneCollectionView *sceneCollectionView{nullptr};
	PLSSceneCollectionManagement *sceneCollectionManageView{nullptr};
	PLSMenuPushButton *sceneCollectionManageTitle{nullptr};

	QTimer saveSceneCollectionTimer;
	bool showLoadSceneCollectionError = false;
	QString showLoadSceneCollectionErrorStr;

	std::shared_ptr<PLSOutro> outro;

	//std::atomic<obs_scene_t *> currentScene = nullptr;
	std::optional<std::pair<uint32_t, uint32_t>> lastOutputResolution;
	void UpdateMultiviewProjectorMenu();

	void DrawBackdrop(float cx, float cy);

	void SetupEncoders();

	void CreateFirstRunSources();
	void CreateDefaultScene(bool firstStart);

	void UpdateVolumeControlsDecayRate();
	void UpdateVolumeControlsPeakMeterType();
	void ClearVolumeControls();

	void UploadLog(const char *subdir, const char *file, const bool crash);

	void Save(const char *file);
	void LoadData(obs_data_t *data, const char *file);
	void Load(const char *file);
	void loadProfile(const char *savePath, const char *sceneCollection,
			 LoadSceneCollectionWay way =
				 LoadSceneCollectionWay::RunPrismImmediately);

	void InitHotkeys();
	void CreateHotkeys();
	void ClearHotkeys();

	bool InitService();

	bool InitBasicConfigDefaults();
	void InitBasicConfigDefaults2();
	bool InitBasicConfig();

	void InitOBSCallbacks();

	void InitPrimitives();

	void OnFirstLoad();

	OBSSceneItem GetSceneItem(QListWidgetItem *item);

	void TimedCheckForUpdates();
	void CheckForUpdates(bool manualUpdate);

	void GetFPSCommon(uint32_t &num, uint32_t &den) const;
	void GetFPSInteger(uint32_t &num, uint32_t &den) const;
	void GetFPSFraction(uint32_t &num, uint32_t &den) const;
	void GetFPSNanoseconds(uint32_t &num, uint32_t &den) const;

	void UpdatePreviewScalingMenu();

	void LoadSceneListOrder(obs_data_array_t *array, const char *file);
	obs_data_array_t *SaveSceneListOrder();
	void ChangeSceneIndex(bool relative, int idx, int invalidIdx);

	void checkSceneDisplayMethod();

	void LoadSourceRecentColorConfig(obs_data_t *obj);
	void UpdateSourceRecentColorConfig(QString strColor);

	void TempFileOutput(const char *path, int vBitrate, int aBitrate);
	void TempStreamOutput(const char *url, const char *key, int vBitrate,
			      int aBitrate);

	void CloseDialogs();
	void ClearSceneData();
	void ClearProjectors();

	void Nudge(int dist, MoveDir dir);

	PLSDialogView *OpenProjector(obs_source_t *source, int monitor,
				     ProjectorType type);

	void GetAudioSourceFilters();
	void GetAudioSourceProperties();
	void VolControlContextMenu();
	void ToggleVolControlLayout();
	void ToggleMixerLayout(bool vertical);

	void RefreshSceneCollections();
	void ChangeSceneCollection();
	void LogScenes();
	bool SceneCollectionExists(const char *findName) const;
	bool GetSceneCollectionName(QWidget *parent, std::string &name,
				    std::string &file,
				    SceneSetOperatorType type,
				    const char *oldName = nullptr);

	// scene collection
	bool addCollection(QVector<PLSSceneCollectionData> &datas,
			   const char *name, const char *path,
			   const char *cur_name, const char *cur_file);
	bool RenamePscToJsonFile(const char *path, QString &destName,
				 QString &destPath);
	bool CheckPscFileInPrismUserPath(QString &pscPath);
	void InitSceneCollections();
	void CreateSceneCollectionView();
	QString ImportSceneCollection(QWidget *parent,
				      const QString &importFile,
				      LoadSceneCollectionWay way,
				      bool fromExport = false);

	bool importLocalSceneTemplate(const QString &overlayFile);
	bool importSceneTemplate(const SceneTemplateItem &model,
				 bool checkResolution = true);
	bool exportSceneTemplate(const QString &overlayFile);

	void ExportSceneCollection(const QString &name, const QString &fileName,
				   QWidget *parent, bool import_scene = false,
				   ExprotCallback callback = nullptr);
	void OnSelectExportFile(const QString &exportFile, const QString &path,
				const QString &currentFile,
				ExprotCallback callback, bool import_scene);
	void LoadSceneCollection(QString name, QString filePath);
	bool CheckSceneCollectionNameAndPath(QString path,
					     std::string &destName,
					     std::string &destPath) const;
	PLSSceneCollectionData
	GetSceneCollectionDataWithUserLocalPath(QString userLocalPath) const;
	bool CheckSameSceneCollection(QString name,
				      QString userLocalPath) const;
	void AddCollectionUserLocalPath(QString name,
					QString userLocalPath) const;
	QString GetSceneCollectionPathByName(QString name) const;

	void CreateTimerSourcePopupMenu(QMenu *menu,
					obs_source_t *source) const;

	void ResetProfileData();
	bool AddProfile(bool create_new, const char *title, const char *text,
			const char *init_text = nullptr, bool rename = false);
	bool CreateProfile(const std::string &newName,
			   const std::string &newDir, bool create_new,
			   bool showWizardChecked, bool rename = false,
			   const char *init_text = nullptr);
	void DeleteProfile(const char *profile_name, const char *profile_dir);
	void RefreshProfiles();
	void ChangeProfile();
	bool addProfile(int &count, const char *name, const char *path);
	bool RenameProfile(const char *title, const char *text,
			   const char *old_name, const char *old_dir);
	bool ExportProfile(QString &exportDir);
	void ImportProfile(const QString &importDir);
	void CheckForSimpleModeX264Fallback();

	void SaveProjectNow();

	int GetTopSelectedSourceItem();

	QModelIndexList GetAllSelectedSourceItems();

	obs_hotkey_pair_id streamingHotkeys, recordingHotkeys, pauseHotkeys,
		replayBufHotkeys, vcamHotkeys, togglePreviewHotkeys,
		contextBarHotkeys;
	obs_hotkey_id forceStreamingStopHotkey, splitFileHotkey;

	void InitDefaultTransitions();
	void InitTransition(obs_source_t *transition);
	QComboBox *GetTransitionCombobox();
	OBSSource GetCurrentTransition() const;
	QSpinBox *GetTransitionDurationSpinBox();

	obs_source_t *fadeTransition;
	obs_source_t *cutTransition;

	void CreateProgramDisplay();
#if 0
	void CreateProgramOptions();
	void AddQuickTransitionId(int id);
	void AddQuickTransition();
	void AddQuickTransitionHotkey(QuickTransition *qt);
	void RemoveQuickTransitionHotkey(QuickTransition *qt);
	void LoadQuickTransitions(obs_data_array_t *array);
	obs_data_array_t *SaveQuickTransitions();
	void ClearQuickTransitionWidgets();
	void RefreshQuickTransitions();
	void DisableQuickTransitionWidgets();
	void CreateDefaultQuickTransitions();
#endif
	void EnableTransitionWidgets(bool enable);

	void PasteShowHideTransition(obs_sceneitem_t *item, bool show,
				     obs_source_t *tr);
	QMenu *CreatePerSceneTransitionMenu();
	QMenu *CreateVisibilityTransitionMenu(bool visible);

	void SetPreviewProgramMode(bool enabled);
	void ResizeProgram(uint32_t cx, uint32_t cy);
	void SetCurrentScene(obs_scene_t *scene, bool force = false);
	static void RenderProgram(void *data, uint32_t cx, uint32_t cy);

	//QPointer<QWidget> programOptions;
	QPointer<OBSQTDisplay> program;
	OBSWeakSource lastScene;
	OBSWeakSource swapScene;
	OBSWeakSource programScene;
	OBSWeakSource lastProgramScene;
	OBSWeakSource m_currentScene = nullptr;
	std::mutex mutex_current_scene;
	bool editPropertiesMode = false;
	bool sceneDuplicationMode = true;
	bool swapScenesMode = false;
	volatile bool previewProgramMode = false;
	obs_hotkey_pair_id togglePreviewProgramHotkeys = 0;
	obs_hotkey_id transitionHotkey = 0;
	obs_hotkey_id statsHotkey = 0;
	obs_hotkey_id screenshotHotkey = 0;
	obs_hotkey_id sourceScreenshotHotkey = 0;
	obs_hotkey_id selectRegionHotkey = 0;
	bool overridingTransition = false;
	PLSPreviewProgramTitle *previewProgramTitle = nullptr;

	int programX = 0, programY = 0;
	int programCX = 0, programCY = 0;
	float programScale = 0.0f;

	int disableOutputsRef = 0;

	inline void OnActivate(bool force = false);
	inline void OnDeactivate();

	void AddDropSource(const char *file, DropType image);
	void AddDropURL(const char *url, QString &name, obs_data_t *settings,
			const obs_video_info &ovi);
	void ConfirmDropUrl(const QString &url);
	void dragEnterEvent(QDragEnterEvent *event) override;
	void dragLeaveEvent(QDragLeaveEvent *event) override;
	void dragMoveEvent(QDragMoveEvent *event) override;
	void dropEvent(QDropEvent *event) override;

	void ReplayBufferClicked();

	bool sysTrayMinimizeToTray();

	void EnumDialogs();

	QList<QPointer<QDialog>> visDialogs;
	QList<QDialog *> modalDialogs;
	QList<PLSDialogView *> visMsgBoxes;
	QPointer<QWidget> visWizardWidget;
	QList<QPoint> visDlgPositions;

	QByteArray startingDockLayout;

	obs_data_array_t *SaveProjectors();
	void LoadSavedProjectors(obs_data_array_t *savedProjectors);

	void MacBranchesFetched(const QString &branch, bool manualUpdate);
	void ReceivedIntroJson(const QString &text);
	void ShowWhatsNew(const QString &url);

	void UpdatePreviewProgramIndicators();

	QStringList extraDockNames;
	QList<std::shared_ptr<QDockWidget>> extraDocks;

	QStringList extraCustomDockNames;
	QList<QPointer<QDockWidget>> extraCustomDocks;

	QStringList ncb2bCustomDockUrls;
	QStringList ncb2bCustomDockNames;
	QList<std::shared_ptr<QDockWidget>> ncb2bCustomDocks;
	QPointer<QAction> ncb2bMenuDocksSeparator;

#ifdef BROWSER_AVAILABLE
	QPointer<QAction> extraBrowserMenuDocksSeparator;

	QList<std::shared_ptr<QDockWidget>> extraBrowserDocks;
	QStringList extraBrowserDockNames;
	QStringList extraBrowserDockTargets;

	void ClearExtraBrowserDocks();
	void LoadExtraBrowserDocks();
	void SaveExtraBrowserDocks();
	void ManageExtraBrowserDocks();

	void loadNcb2bBrowserSettingsDocks();
	void saveNcb2bBrowserSettingsDocks();

#endif

	QIcon imageIcon;
	QIcon colorIcon;
	QIcon slideshowIcon;
	QIcon audioInputIcon;
	QIcon audioOutputIcon;
	QIcon desktopCapIcon;
	QIcon windowCapIcon;
	QIcon gameCapIcon;
	QIcon cameraIcon;
	QIcon textIcon;
	QIcon mediaIcon;
	QIcon browserIcon;
	QIcon groupIcon;
	QIcon sceneIcon;
	QIcon defaultIcon;
	QIcon audioProcessOutputIcon;

	QIcon GetImageIcon() const;
	QIcon GetColorIcon() const;
	QIcon GetSlideshowIcon() const;
	QIcon GetAudioInputIcon() const;
	QIcon GetAudioOutputIcon() const;
	QIcon GetDesktopCapIcon() const;
	QIcon GetWindowCapIcon() const;
	QIcon GetGameCapIcon() const;
	QIcon GetCameraIcon() const;
	QIcon GetTextIcon() const;
	QIcon GetMediaIcon() const;
	QIcon GetBrowserIcon() const;
	QIcon GetDefaultIcon() const;
	QIcon GetAudioProcessOutputIcon() const;

	QSlider *tBar;
	bool tBarActive = false;

	OBSSource GetOverrideTransition(OBSSource source);
	int GetOverrideTransitionDuration(OBSSource source);

	void UpdateProjectorHideCursor();
	void UpdateProjectorAlwaysOnTop(bool top);
	void ResetProjectors();

	QPointer<QObject> screenshotData;

	void MoveSceneItem(enum obs_order_movement movement,
			   const QString &action_name);

	bool autoStartBroadcast = true;
	bool autoStopBroadcast = true;
	bool broadcastActive = false;
	bool broadcastReady = false;
	QPointer<QThread> youtubeStreamCheckThread;
#ifdef YOUTUBE_ENABLED
	void YoutubeStreamCheck(const std::string &key);
	void ShowYouTubeAutoStartWarning();
	void YouTubeActionDialogOk(const QString &broadcast_id,
				   const QString &stream_id, const QString &key,
				   bool autostart, bool autostop,
				   bool start_now);
#endif
	void BroadcastButtonClicked();
	void SetBroadcastFlowEnabled(bool enabled);

	void UpdatePreviewSafeAreas();
	bool drawSafeAreas = false;

	void CenterSelectedSceneItems(const CenterType &centerType);
	void ShowMissingFilesDialog(obs_missing_files_t *files);

	QColor selectionColor;
	QColor cropColor;
	QColor hoverColor;

	QColor GetCropColor() const;
	QColor GetHoverColor() const;

	void UpdatePreviewSpacingHelpers();
	bool drawSpacingHelpers = true;

	float GetDevicePixelRatio();
	void SourceToolBarActionsSetEnabled();

	std::string lastScreenshot;
	std::string lastReplay;

	void UpdatePreviewOverflowSettings();

	bool restartingVCam = false;

	QMap<int, int> states;

public slots:

	void DeferSaveBegin();
	void DeferSaveEnd();

	void DisplayStreamStartError();

	void SetupBroadcast();

	void markState(int state, int result = 0);
	void StartStreaming();
	void StopStreaming(bool userStop = true);
	void ForceStopStreaming();

	void StreamDelayStarting(int sec);
	void StreamDelayStopping(int sec);

	void StreamingStart();
	void StreamStopping();
	void StreamingStop(int errorcode, QString last_error);

	void StartRecording();
	void StopRecording();

	void RecordingStart();
	void RecordStopping();
	void RecordingStop(int code, QString last_error);
	void RecordingFileChanged(QString lastRecordingPath);

	void ShowReplayBufferPauseWarning();
	void StartReplayBuffer();
	void StopReplayBuffer();

	void ReplayBufferStart();
	void ReplayBufferSave();
	void ReplayBufferSaved();
	void ReplayBufferStopping();
	void ReplayBufferStop(int code);

	void StartVirtualCam();
	void StopVirtualCam();

	void OnVirtualCamStart();
	void OnVirtualCamStop(int code);

	void SaveProjectDeferred();
	void SaveProject();

	bool InterruptPrevTransiton();
	void OverrideTransition(OBSSource transition);
	void TransitionToScene(OBSScene scene, bool force = false);
	void TransitionToScene(OBSSource scene, bool force = false,
			       bool quickTransition = false,
			       int quickDuration = 0, bool black = false,
			       bool manual = false);
	void SetCurrentScene(OBSSource scene, bool force = false);
	void SetCurrentSceneWithoutInterrupt(OBSSource scene,
					     bool force = false);
	void SetScene(OBSScene scene);

	bool AddSceneCollection(bool create_new, QWidget *parent,
				const QString &name = QString(),
				const QString &dupName = QString(),
				const QString &dupFile = QString());

	void ShowSceneCollectionView();
	QVector<QString> GetSceneCollections() const;

	void AddSource(const char *id);

	bool NewProfile(const QString &name);
	bool DuplicateProfile(const QString &name);
	void DeleteProfile(const QString &profileName);

	void UpdatePatronJson(const QString &text, const QString &error);

	void ShowContextBar();
	void HideContextBar();
	void PauseRecording();
	void UnpauseRecording();

	std::string ExtractFileName(const std::string &filePath) const;
	bool QueryRemoveSource(obs_source_t *source,
			       QWidget *parent = pls_get_main_view());
	void SetShowing(bool showing, bool isChangePreviewState = true);

private slots:

	void on_actionMainUndo_triggered();
	void on_actionMainRedo_triggered();

	void AddSceneItem(OBSSceneItem item);
	void RemoveSceneItem(OBSSceneItem item) const;
	void SelectSceneItem(OBSScene scene, OBSSceneItem item, bool select);

	void AddScene(OBSSource source);
	void RemoveScene(OBSSource source);
	void RenameSources(OBSSource source, QString newName, QString prevName);

	void ActivateAudioSource(OBSSource source);
	void DeactivateAudioSource(OBSSource source);

	void DuplicateSelectedScene();
	void RemoveSelectedScene();

	void ToggleAlwaysOnTop();

	void ReorderSources(OBSScene scene);
	void RefreshSources(OBSScene scene);

	void ProcessHotkey(obs_hotkey_id id, bool pressed);

	void TransitionClicked();
	void TransitionStopped();
	void TransitionFullyStopped();
	// void TriggerQuickTransition(int id);

	void SetDeinterlacingMode();
	void SetDeinterlacingOrder();

	void SetScaleFilter();

	void SetBlendingMethod();
	void SetBlendingMode();

	void IconActivated(QSystemTrayIcon::ActivationReason reason);

	void ToggleShowHide();

	void HideAudioControl();
	void UnhideAllAudioControls();
	void ToggleHideMixer();

	void MixerRenameSource();

	void on_vMixerScrollArea_customContextMenuRequested();
	void on_hMixerScrollArea_customContextMenuRequested();

	void on_actionCopySource_triggered();
	void on_actionPasteRef_triggered();
	void on_actionPasteDup_triggered();

	void on_actionCopyFilters_triggered();
	void on_actionPasteFilters_triggered();

	void OnMultiviewShowTriggered(bool checked);
	void OnMultiviewHideTriggered(bool checked);

	void ColorChange();

	SourceTreeItem *GetItemWidgetFromSceneItem(obs_sceneitem_t *sceneItem);

	void on_actionShowAbout_triggered();

	void AudioMixerCopyFilters();
	void AudioMixerPasteFilters();

	void EnablePreview();
	void DisablePreview();

	void EnablePreviewProgram();
	void DisablePreviewProgram();

	void SceneCopyFilters();
	void ScenePasteFilters();

	void CheckDiskSpaceRemaining();
	void OpenSavedProjector(SavedProjectorInfo *info);

	void ResetStatsHotkey();

	void SetImageIcon(const QIcon &icon);
	void SetColorIcon(const QIcon &icon);
	void SetSlideshowIcon(const QIcon &icon);
	void SetAudioInputIcon(const QIcon &icon);
	void SetAudioOutputIcon(const QIcon &icon);
	void SetDesktopCapIcon(const QIcon &icon);
	void SetWindowCapIcon(const QIcon &icon);
	void SetGameCapIcon(const QIcon &icon);
	void SetCameraIcon(const QIcon &icon);
	void SetTextIcon(const QIcon &icon);
	void SetMediaIcon(const QIcon &icon);
	void SetBrowserIcon(const QIcon &icon);
	void SetGroupIcon(const QIcon &icon);
	void SetSceneIcon(const QIcon &icon);
	void SetDefaultIcon(const QIcon &icon);
	void SetAudioProcessOutputIcon(const QIcon &icon);

#if 0
	void TBarChanged(int value);
	void TBarReleased();
#endif
	void LockVolumeControl(bool lock);
	void ThemeChanged();

	void UpdateVirtualCamConfig(const VCamConfig &config);
	void RestartVirtualCam(const VCamConfig &config);
	void RestartingVirtualCam();

private:
	/* OBS Callbacks */
	static void SceneReordered(void *data, calldata_t *params);
	static void SceneRefreshed(void *data, calldata_t *params);
	static void SceneItemAdded(void *data, calldata_t *params);
	static void SceneItemRemoved(void *data, calldata_t *params);
	static void SceneItemSelected(void *data, calldata_t *params);
	static void SceneItemDeselected(void *data, calldata_t *params);
	static void SourceCreated(void *data, calldata_t *params);
	static void SourceRemoved(void *data, calldata_t *params);
	static void SourceActivated(void *data, calldata_t *params);
	static void SourceDeactivated(void *data, calldata_t *params);
	static void SourceAudioActivated(void *data, calldata_t *params);
	static void SourceAudioDeactivated(void *data, calldata_t *params);
	static void SourceRenamed(void *data, calldata_t *params);
	static void RenderMain(void *data, uint32_t cx, uint32_t cy);
	static void SourceDestroyed(void *data, calldata_t *params);

	void ResizePreview(uint32_t cx, uint32_t cy);

	QMenu *CreateAddSourcePopupMenu();
	void AddSourcePopupMenu(const QPoint &pos);
	void copyActionsDynamicProperties();

	static void HotkeyTriggered(void *data, obs_hotkey_id id, bool pressed);

	void AutoRemux(QString input, bool no_show = false);

	void UpdatePause(bool activate = true);
	void UpdateReplayBuffer(bool activate = true);

	bool OutputPathValid();
	void OutputPathInvalidMessage();

	bool LowDiskSpace();
	void DiskSpaceMessage();

	void UpdateSceneSelection(OBSSource source);

	OBSSource prevFTBSource = nullptr;

	float dpi = 1.0;

public:
	OBSSource GetProgramSource();
	OBSScene GetCurrentScene();
	void GetConfigFPS(uint32_t &num, uint32_t &den) const;
	obs_frontend_callbacks *getApi() { return api; }
	void SysTrayNotify(const QString &text, QSystemTrayIcon::MessageIcon n);

	inline OBSSource GetCurrentSceneSource()
	{
		OBSScene curScene = GetCurrentScene();
		return OBSSource(obs_scene_get_source(curScene));
	}

	OBSSceneItem GetCurrentSceneItem();

	obs_service_t *GetService();
	void SetService(obs_service_t *service);
	void ClearService(const QString &channelName = "");

	obs_output_t *GetSrteamOutput();

	int GetTransitionDuration();
	int GetTbarPosition();

	inline bool IsPreviewProgramMode() const
	{
		return os_atomic_load_bool(&previewProgramMode);
	}

	inline bool VCamEnabled() const { return vcamEnabled; }

	bool Active() const;

	void ResetUI();
	int ResetVideo();
	bool ResetAudio();

	void AddVCamButton();
	void ResetOutputs();

	void RefreshVolumeColors();

	void ResetAudioDevice(const char *sourceId, const char *deviceId,
			      const char *deviceDesc, int channel);

	void NewProject();
	void LoadProject();

	inline void GetDisplayRect(int &x, int &y, int &cx, int &cy)
	{
		x = previewX;
		y = previewY;
		cx = previewCX;
		cy = previewCY;
	}

	inline bool SavingDisabled() const { return disableSaving; }

	inline double GetCPUUsage() const
	{
		return os_cpu_usage_info_query(cpuUsageInfo);
	}

	void SaveService();
	bool LoadService();
	OBSDataAutoRelease LoadServiceData();

	inline Auth *GetAuth() { return auth.get(); }

	inline void EnableOutputs(bool enable)
	{
		if (enable) {
			if (--disableOutputsRef < 0)
				disableOutputsRef = 0;
		} else {
			disableOutputsRef++;
		}
	}

	QMenu *AddDeinterlacingMenu(QMenu *menu, obs_source_t *source);
	QMenu *AddScaleFilteringMenu(QMenu *menu, obs_sceneitem_t *item);
	QMenu *AddBlendingMethodMenu(QMenu *menu, obs_sceneitem_t *item);
	QMenu *AddBlendingModeMenu(QMenu *menu, obs_sceneitem_t *item);
	QMenu *AddBackgroundColorMenu(QMenu *menu, QWidgetAction *widgetAction,
				      ColorSelectNew *select,
				      obs_sceneitem_t *item);
	void CreateSourcePopupMenu(int idx, bool preview, QWidget *parent);

	void UpdateTitleBar();

	QPointer<QAction> addSysTrayAction(const QString &txt,
					   const QString &objName,
					   const std::function<void()> &fun);
	void SystemTrayInit(bool isVisible);
	void SystemTray(bool firstStarted);

	void OpenSavedProjectors();

	void CreateInteractionWindow(obs_source_t *source);
	void CreatePropertiesWindow(obs_source_t *source,
				    unsigned flags = OPERATION_NONE);
	void CreateFiltersWindow(obs_source_t *source);
	QWidget *GetPropertiesWindow();

	void DeletePropertiesWindowInScene(obs_scene_t *scene) const;
	void DeletePropertiesWindow(obs_source_t *source) const;
	void DeleteFiltersWindowInScene(obs_scene_t *scene) const;
	void DeleteFiltersWindow(const obs_source_t *source) const;
	void CreateEditTransformWindow(obs_sceneitem_t *item);

	QAction *AddDockWidget(QDockWidget *dock);
	void AddDockWidget(QDockWidget *dock, Qt::DockWidgetArea area,
			   bool extraBrowser = false, bool ncb2b = false);
	void RemoveDockWidget(const QString &name);
	bool IsDockObjectNameUsed(const QString &name);
	void AddCustomDockWidget(QDockWidget *dock);

	static OBSBasic *Get();

	const char *GetCurrentOutputPath();

	void DeleteProjector(OBSProjector *projector);

	static QList<QString> GetProjectorMenuMonitorsFormatted();
	template<typename Receiver, typename... Args>
	static void AddProjectorMenuMonitors(QMenu *parent, Receiver *target,
					     void (Receiver::*slot)(Args...))
	{
		auto projectors = GetProjectorMenuMonitorsFormatted();
		for (int i = 0; i < projectors.size(); i++) {
			QString str = projectors[i];
			QAction *action = parent->addAction(str, target, slot);
			action->setProperty("monitor", i);
		}
	}

	QIcon GetSourceIcon(const char *id) const;
	virtual bool GetSourceIcon(QIcon &icon, int type) const;
	QIcon GetGroupIcon() const;
	QIcon GetSceneIcon() const;
	QPixmap GetSourcePixmap(const QString &id, bool selected,
				QSize size = QSize(30, 30) * 4);

	OBSWeakSource copyFilter;

	void ShowStatusBarMessage(const QString &message);

	static OBSData
	BackupScene(obs_scene_t *scene,
		    std::vector<obs_source_t *> *sources = nullptr);
	void CreateSceneUndoRedoAction(const QString &action_name,
				       OBSData undo_data, OBSData redo_data);

	static inline OBSData
	BackupScene(obs_source_t *scene_source,
		    std::vector<obs_source_t *> *sources = nullptr)
	{
		obs_scene_t *scene = obs_scene_from_source(scene_source);
		return BackupScene(scene, sources);
	}

	void CreateFilterPasteUndoRedoAction(const QString &text,
					     obs_source_t *source,
					     obs_data_array_t *undo_array,
					     obs_data_array_t *redo_array);

	void UpdateEditMenu();

	void SetDisplayAffinity(QWindow *window);

	QColor GetSelectionColor() const;
	inline bool Closing() { return closing; }

	void mainViewClose(QCloseEvent *event);
	virtual void closeMainBegin() {}
	virtual void closeMainFinished() {}
	virtual bool checkMainViewClose(QCloseEvent *event);

	void DeleteSelectedScene(OBSScene scene);

	void UpdateSudioModeState(bool enabled);
	void UpdateStudioModeUI(bool studioMode);

	void RunPrismByPscPath();

	//20230118/zengqin/for drawpen cursor
	inline float GetPreviewScale() const { return previewScale; };
	//202300203/zengqin/spectralizer
	void updateSpectralizerAudioSources(OBSSource source, unsigned flags);
	//20240514/xiewei/add output analog
	double GetStreamingOutputFPS();
	double GetRecordingOutputFPS();

	int getState(int state);
	void setDocksVisible(bool visible);
	void setDocksVisibleProperty();
	void setDockDisplayAsynchronously(PLSDock *dock, bool visible);

	BrowserDock *addNcb2bCustomDock(const QString &title,
					const QString &url, const QString &uuid,
					bool firstCreate,
					QByteArray geometry = QByteArray());
	QList<std::shared_ptr<QDockWidget>> getNcb2bCustomDocks();
	QList<QString> getNcb2bCustomDocksUrls();
	QList<QString> getNcb2bCustomDocksNames();
	std::shared_ptr<QDockWidget> getNcb2bCustomDock(const QString &title);
	void updateNcb2bDockUrl(int index, const QString &url);
	void updateNcb2bDockName(int index, const QString &name);
#ifdef BROWSER_AVAILABLE
	void AddExtraBrowserDock(const QString &title, const QString &url,
				 const QString &uuid, bool firstCreate);
	BrowserDock *createBrowserDock(const QString &title, const QString &url,
				       const QString &uuid, bool firstCreate,
				       QByteArray geometry = QByteArray(),
				       bool ncb2b = false);
#endif
	void mainViewChangeEvent(QEvent *event);

protected:
	virtual bool nativeEvent(const QByteArray &eventType, void *message,
				 qintptr *result) override;
	virtual void changeEvent(QEvent *event) override;

private slots:
	void on_actionFullscreenInterface_triggered();

	void on_actionShow_Recordings_triggered();
	void on_actionRemux_triggered();
	void on_action_Settings_triggered();
	void on_actionShowMacPermissions_triggered();
	void on_actionShowMissingFiles_triggered();
	void on_actionAdvAudioProperties_triggered();
	void on_actionMixerToolbarAdvAudio_triggered();
	void on_actionMixerToolbarMenu_triggered();
	void on_actionShowLogs_triggered();
	void on_actionUploadCurrentLog_triggered();
	void on_actionUploadLastLog_triggered();
	void on_actionViewCurrentLog_triggered();
	void on_actionCheckForUpdates_triggered();
	void on_actionRepair_triggered();
	void on_actionShowWhatsNew_triggered();
	void on_actionRestartSafe_triggered();

	void on_actionShowCrashLogs_triggered();
	void on_actionUploadLastCrashLog_triggered();

	void on_actionEditTransform_triggered();
	void on_actionCopyTransform_triggered();
	void on_actionPasteTransform_triggered();
	void on_actionRotate90CW_triggered();
	void on_actionRotate90CCW_triggered();
	void on_actionRotate180_triggered();
	void on_actionFlipHorizontal_triggered();
	void on_actionFlipVertical_triggered();
	void on_actionFitToScreen_triggered();
	void on_actionStretchToScreen_triggered();
	void on_actionCenterToScreen_triggered();
	void on_actionVerticalCenter_triggered();
	void on_actionHorizontalCenter_triggered();
	void on_actionSceneFilters_triggered();

	void on_OBSBasic_customContextMenuRequested(const QPoint &pos);

	void GridActionClicked();
	void on_actionAddScene_triggered();
	void on_actionRemoveScene_triggered();
	void on_actionSceneUp_triggered();
	void on_actionSceneDown_triggered();
	void on_sources_customContextMenuRequested(const QPoint &pos);
	void on_actionAddSource_triggered();
	void on_actionRemoveSource_triggered();
	void on_actionInteract_triggered();
	void on_actionSourceProperties_triggered();
	void on_actionSourceUp_triggered();
	void on_actionSourceDown_triggered();

	void on_actionMoveUp_triggered();
	void on_actionMoveDown_triggered();
	void on_actionMoveToTop_triggered();
	void on_actionMoveToBottom_triggered();

	void on_actionLockPreview_triggered();

	void on_scalingMenu_aboutToShow();
	void on_actionScaleWindow_triggered();
	void on_actionScaleCanvas_triggered();
	void on_actionScaleOutput_triggered();

	void on_actionQuitApp_triggered();

	void on_streamButton_clicked();
	bool startStreamingCheck();
	bool stopStreamingCheck();
	bool startRecordCheck();
	bool stopRecordCheck();

	void on_recordButton_clicked();
	void VCamButtonClicked();
	void VCamConfigButtonClicked();
	void on_settingsButton_clicked();
	void Screenshot(OBSSource source_ = nullptr);
	void ScreenshotSelectedSource();
	void ScreenshotProgram();
	void ScreenshotScene();

	void on_actionHelpPortal_triggered();
	void on_actionWebsite_triggered();
	void on_actionDiscord_triggered();
	void on_actionReleaseNotes_triggered();

	void on_preview_customContextMenuRequested();
	void ProgramViewContextMenuRequested();
	void on_previewDisabledWidget_customContextMenuRequested();

	void on_actionNewSceneCollection_triggered();
	void on_actionRenameSceneCollection_triggered();
	void on_actionRemoveSceneCollection_triggered();
	void on_actionImportSceneCollection_triggered();
	void on_actionExportSceneCollection_triggered();

	void on_actionNewProfile_triggered();
	void on_actionDupProfile_triggered();
	void on_actionRenameProfile_triggered();
	void on_actionRemoveProfile_triggered(bool skipConfirmation = false);
	void on_actionImportProfile_triggered();
	void on_actionExportProfile_triggered();

	void on_actionShowSettingsFolder_triggered();
	void on_actionShowProfileFolder_triggered();

	void on_actionAlwaysOnTop_triggered();

	void on_toggleListboxToolbars_toggled(bool visible);
	void on_toggleContextBar_toggled(bool visible);
	void on_toggleStatusBar_toggled(bool visible);
	void on_toggleSourceIcons_toggled(bool visible);
#if 0
	void on_transitions_currentIndexChanged(int index);
	void on_transitionAdd_clicked();
	void on_transitionRemove_clicked();
	void on_transitionProps_clicked();
	void on_transitionDuration_valueChanged();
#endif
	void ShowTransitionProperties();
	void HideTransitionProperties();

	void on_modeSwitch_clicked();

	// Source Context Buttons
	void on_sourcePropertiesButton_clicked();
	void on_sourceFiltersButton_clicked();
	void on_sourceInteractButton_clicked();

	void on_autoConfigure_triggered();
	void on_stats_triggered();

	void on_resetUI_triggered();
	void on_resetDocks_triggered(bool force = false);
	void on_lockDocks_toggled(bool lock);
	void on_multiviewProjectorWindowed_triggered();
	void on_sideDocks_toggled(bool side);

	void PauseToggled();

	void logUploadFinished(const QString &text, const QString &error);
	void crashUploadFinished(const QString &text, const QString &error);
	void openLogDialog(const QString &text, const bool crash);

	void updateCheckFinished();

	void MoveSceneToTop();
	void MoveSceneToBottom();

	void EditSceneName();
	void EditSceneItemName();

	void SceneNameEdited(QWidget *editor);

	void OpenSceneFilters();
	void OpenFilters(OBSSource source = nullptr);
	void OpenProperties(OBSSource source = nullptr);
	void OpenInteraction(OBSSource source = nullptr);
	void OpenEditTransform(OBSSceneItem item = nullptr);

	void EnablePreviewDisplay(bool enable);
	void TogglePreview();

	void OpenStudioProgramProjector();
	void OpenPreviewProjector();
	void OpenSourceProjector();
	void OpenMultiviewProjector();
	void OpenSceneProjector();

	void OpenStudioProgramWindow();
	void OpenPreviewWindow();
	void OpenSourceWindow();
	void OpenSceneWindow();
	void OpenMultiviewWindow();

	void StackedMixerAreaContextMenuRequested();

	void ResizeOutputSizeOfSource();

	void RepairOldExtraDockName();
	void RepairCustomExtraDockName();

	void OnSelectItemChanged(OBSSceneItem item, bool selected);
	void OnVisibleItemChanged(OBSSceneItem item, bool visible);
	void OnSourceItemsRemove(QVector<OBSSceneItem> items);

	//PRISM/renjinbo/20230111/#/add prism plugins path to load
	void addPrismPlugins();
	void addOBSThirdPlguins();
public slots:
	void on_actionResetTransform_triggered();

	bool StreamingActive();
	bool RecordingActive();
	bool ReplayBufferActive();
	bool VirtualCamActive();

	void ClearContextBar();
	void UpdateContextBar(bool force = false);
	void UpdateContextBarDeferred(bool force = false);
	void UpdateContextBarVisibility();

	void OnScenesCustomContextMenuRequested(const PLSSceneItemView *item);
	void OnScenesItemDoubleClicked();

	void on_actionNewSceneCollection_triggered_with_parent(QWidget *parent);
	void
	on_actionImportSceneCollection_triggered_with_parent(QWidget *parent);
	void on_actionChangeSceneCollection_triggered(const QString &name,
						      const QString &path,
						      bool textMode);
	void SetCurrentSceneCollection(const QString &name);

	void on_actionExportSceneCollection_triggered_with_path(
		const QString &name, const QString &fileName, QWidget *parent);
	void on_actionSceneCollectionManagement_triggered();
	void on_actionRenameSceneCollection_triggered(const QString &name,
						      const QString &path);
	void on_actionDupSceneCollection_triggered(const QString &name,
						   const QString &path);
	void on_actionRemoveSceneCollection_triggered(const QString &name,
						      const QString &path);
	void ReorderSceneCollectionManageView() const;
	void on_actionImportFromOtherSceneCollection_triggered();

	void OnResizeCTClicked();
	void resizeCTView();

signals:
	void sourceRenmame(qint64 source, const QString &sourceId,
			   const QString &newName, const QString &prevName);
	void itemsReorderd();
	void mainClosing();
	void mainCloseFinished();
	void updateChatV2PropertBrowserSize(const QSize &size);

private:
	std::unique_ptr<Ui::OBSBasic> ui;
	std::unique_ptr<PLSWatermark> _watermark;
	bool updatedLiveStart = false;
	bool m_profileMenuState = true;

	PLSDialogView *m_chatTemplateDialogView = nullptr;
	OBSProjector *m_chatTemplateProjector = nullptr;
	bool m_bIgnoreResize = false;

private:
	void configureWatermark();
	void updateLiveStartUI();
	void updateLiveEndUI();
	void updateRecordStartUI();
	void updateRecordEndUI();

public:
	/* `undo_s` needs to be declared after `ui` to prevent an uninitialized
	 * warning for `ui` while initializing `undo_s`. */
	undo_stack undo_s;

	explicit OBSBasic(PLSMainView *mainView);
	virtual ~OBSBasic();

	virtual bool OBSInit() override;

	virtual config_t *Config() const override;

	virtual int GetProfilePath(char *path, size_t size,
				   const char *file) const override;

	static void InitBrowserPanelSafeBlock();

	void NewYouTubeAppDock();
	void DeleteYouTubeAppDock();
	YouTubeAppDock *GetYouTubeAppDock();

	static void AnalogCodecNotify(void *data, calldata_t *params);

	bool isSameChzzkSourceWithChzzkId(obs_source_t *source,
					  PLSPlatformBase *chzzkPlatform);

	bool m_bForceStop = false;
};

extern bool cef_js_avail;

class SceneRenameDelegate : public QStyledItemDelegate {
	Q_OBJECT

public:
	SceneRenameDelegate(QObject *parent);
	virtual void setEditorData(QWidget *editor,
				   const QModelIndex &index) const override;

protected:
	virtual bool eventFilter(QObject *editor, QEvent *event) override;
};
