/********************************************************************************
** Form generated from reading UI file 'OBSBasic.ui'
**
** Created by: Qt User Interface Compiler version 5.7.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_OBSBASIC_H
#define UI_OBSBASIC_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDockWidget>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include "source-list-widget.hpp"
#include "vertical-scroll-area.hpp"
#include "window-basic-preview.hpp"
#include "window-basic-status-bar.hpp"

QT_BEGIN_NAMESPACE

class Ui_OBSBasic
{
public:
    QAction *actionAddScene;
    QAction *actionAddSource;
    QAction *actionRemoveScene;
    QAction *actionRemoveSource;
    QAction *actionSourceProperties;
    QAction *actionSceneUp;
    QAction *actionSourceUp;
    QAction *actionSceneDown;
    QAction *actionSourceDown;
    QAction *actionShow_Recordings;
    QAction *actionRemux;
    QAction *action_Settings;
    QAction *actionE_xit;
    QAction *actionShowLogs;
    QAction *actionUploadLastLog;
    QAction *actionUploadCurrentLog;
    QAction *actionViewCurrentLog;
    QAction *actionUndo;
    QAction *actionRedo;
    QAction *actionEditTransform;
    QAction *actionCopyTransform;
    QAction *actionPasteTransform;
    QAction *actionRotate90CW;
    QAction *actionRotate90CCW;
    QAction *actionRotate180;
    QAction *actionFitToScreen;
    QAction *actionStretchToScreen;
    QAction *actionResetTransform;
    QAction *actionCenterToScreen;
    QAction *actionFlipHorizontal;
    QAction *actionFlipVertical;
    QAction *actionMoveUp;
    QAction *actionMoveDown;
    QAction *actionMoveToTop;
    QAction *actionMoveToBottom;
    QAction *actionCheckForUpdates;
    QAction *actionInteract;
    QAction *actionAdvAudioProperties;
    QAction *actionWebsite;
    QAction *actionNewSceneCollection;
    QAction *actionDupSceneCollection;
    QAction *actionRenameSceneCollection;
    QAction *actionRemoveSceneCollection;
    QAction *actionImportSceneCollection;
    QAction *actionExportSceneCollection;
    QAction *actionNewProfile;
    QAction *actionDupProfile;
    QAction *actionRenameProfile;
    QAction *actionRemoveProfile;
    QAction *actionImportProfile;
    QAction *actionExportProfile;
    QAction *actionShowSettingsFolder;
    QAction *actionShowProfileFolder;
    QAction *actionAlwaysOnTop;
    QAction *toggleListboxToolbars;
    QAction *toggleStatusBar;
    QAction *actionLockPreview;
    QAction *actionScaleWindow;
    QAction *actionScaleCanvas;
    QAction *actionScaleOutput;
    QAction *actionPasteDup;
    QAction *autoConfigure2;
    QAction *autoConfigure;
    QAction *stats;
    QAction *resetUI;
    QAction *lockUI;
    QAction *toggleScenes;
    QAction *toggleSources;
    QAction *toggleMixer;
    QAction *toggleTransitions;
    QAction *toggleControls;
    QAction *actionHelpPortal;
    QAction *actionShowCrashLogs;
    QAction *actionUploadLastCrashLog;
    QWidget *centralwidget;
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayout_2;
    QLabel *previewDisabledLabel;
    QHBoxLayout *previewLayout;
    QVBoxLayout *previewTextLayout;
    OBSBasicPreview *preview;
    QMenuBar *menubar;
    QMenu *menu_File;
    QMenu *menuBasic_MainMenu_Help;
    QMenu *menuLogFiles;
    QMenu *menuCrashLogs;
    QMenu *menuBasic_MainMenu_Edit;
    QAction *actionCopySource;
    QAction *actionPasteRef;
    QAction *actionCopyFilters;
    QAction *actionPasteFilters;
    QMenu *transformMenu;
    QMenu *orderMenu;
    QMenu *scalingMenu;
    QMenu *profileMenu;
    QMenu *sceneCollectionMenu;
    QMenu *viewMenu;
    QAction *actionFullscreenInterface;
    QMenu *viewMenuToolbars;
    QMenu *viewMenuDocks;
    QMenu *menuTools;
    OBSBasicStatusBar *statusbar;
    QDockWidget *scenesDock;
    QWidget *dockWidgetContents_2;
    QVBoxLayout *verticalLayout_6;
    QFrame *scenesFrame;
    QVBoxLayout *verticalLayout_12;
    QListWidget *scenes;
    QToolBar *scenesToolbar;
    QSpacerItem *scenesFixedSizeHSpacer;
    QDockWidget *sourcesDock;
    QWidget *dockWidgetContents_6;
    QVBoxLayout *verticalLayout_5;
    QFrame *sourcesFrame;
    QVBoxLayout *verticalLayout_17;
    SourceListWidget *sources;
    QToolBar *sourcesToolbar;
    QSpacerItem *sourcesFixedSizeHSpacer;
    QDockWidget *mixerDock;
    QWidget *dockWidgetContents_7;
    QVBoxLayout *verticalLayout_4;
    VScrollArea *mixerScrollArea;
    QWidget *volumeWidgets;
    QVBoxLayout *verticalLayout_18;
    QDockWidget *transitionsDock;
    QWidget *dockWidgetContents_5;
    QVBoxLayout *verticalLayout_3;
    QWidget *transitionsContainer;
    QVBoxLayout *verticalLayout_2;
    QComboBox *transitions;
    QHBoxLayout *horizontalLayout_4;
    QSpacerItem *horizontalSpacer;
    QPushButton *transitionAdd;
    QPushButton *transitionRemove;
    QPushButton *transitionProps;
    QHBoxLayout *horizontalLayout_3;
    QLabel *transitionDurationLabel;
    QSpinBox *transitionDuration;
    QSpacerItem *verticalSpacer;
    QDockWidget *controlsDock;
    QWidget *dockWidgetContents_3;
    QVBoxLayout *buttonsVLayout;
    QPushButton *streamButton;
    QPushButton *recordButton;
    QPushButton *modeSwitch;
    QPushButton *settingsButton;
    QPushButton *exitButton;
    QSpacerItem *expVSpacer;

    void setupUi(QMainWindow *OBSBasic)
    {
        if (OBSBasic->objectName().isEmpty())
            OBSBasic->setObjectName(QStringLiteral("OBSBasic"));
        OBSBasic->resize(1079, 730);
        QSizePolicy sizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(OBSBasic->sizePolicy().hasHeightForWidth());
        OBSBasic->setSizePolicy(sizePolicy);
        OBSBasic->setMinimumSize(QSize(0, 0));
        QIcon icon;
        icon.addFile(QStringLiteral(":/res/images/obs.png"), QSize(), QIcon::Normal, QIcon::Off);
        OBSBasic->setWindowIcon(icon);
        OBSBasic->setStyleSheet(QStringLiteral(""));
        OBSBasic->setDockOptions(QMainWindow::AllowNestedDocks|QMainWindow::AllowTabbedDocks|QMainWindow::AnimatedDocks);
        actionAddScene = new QAction(OBSBasic);
        actionAddScene->setObjectName(QStringLiteral("actionAddScene"));
        QIcon icon1;
        icon1.addFile(QStringLiteral(":/res/images/add.png"), QSize(), QIcon::Normal, QIcon::Off);
        actionAddScene->setIcon(icon1);
        actionAddScene->setProperty("themeID", QVariant(QStringLiteral("addIconSmall")));
        actionAddSource = new QAction(OBSBasic);
        actionAddSource->setObjectName(QStringLiteral("actionAddSource"));
        actionAddSource->setIcon(icon1);
        actionAddSource->setProperty("themeID", QVariant(QStringLiteral("addIconSmall")));
        actionRemoveScene = new QAction(OBSBasic);
        actionRemoveScene->setObjectName(QStringLiteral("actionRemoveScene"));
        QIcon icon2;
        icon2.addFile(QStringLiteral(":/res/images/list_remove.png"), QSize(), QIcon::Normal, QIcon::Off);
        actionRemoveScene->setIcon(icon2);
        actionRemoveScene->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        actionRemoveScene->setProperty("themeID", QVariant(QStringLiteral("removeIconSmall")));
        actionRemoveSource = new QAction(OBSBasic);
        actionRemoveSource->setObjectName(QStringLiteral("actionRemoveSource"));
        actionRemoveSource->setIcon(icon2);
        actionRemoveSource->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        actionRemoveSource->setProperty("themeID", QVariant(QStringLiteral("removeIconSmall")));
        actionSourceProperties = new QAction(OBSBasic);
        actionSourceProperties->setObjectName(QStringLiteral("actionSourceProperties"));
        actionSourceProperties->setEnabled(true);
        QIcon icon3;
        icon3.addFile(QStringLiteral(":/res/images/properties.png"), QSize(), QIcon::Normal, QIcon::Off);
        actionSourceProperties->setIcon(icon3);
        actionSourceProperties->setProperty("themeID", QVariant(QStringLiteral("propertiesIconSmall")));
        actionSceneUp = new QAction(OBSBasic);
        actionSceneUp->setObjectName(QStringLiteral("actionSceneUp"));
        QIcon icon4;
        icon4.addFile(QStringLiteral(":/res/images/up.png"), QSize(), QIcon::Normal, QIcon::Off);
        actionSceneUp->setIcon(icon4);
        actionSceneUp->setProperty("themeID", QVariant(QStringLiteral("upArrowIconSmall")));
        actionSourceUp = new QAction(OBSBasic);
        actionSourceUp->setObjectName(QStringLiteral("actionSourceUp"));
        actionSourceUp->setEnabled(true);
        actionSourceUp->setIcon(icon4);
        actionSourceUp->setProperty("themeID", QVariant(QStringLiteral("upArrowIconSmall")));
        actionSceneDown = new QAction(OBSBasic);
        actionSceneDown->setObjectName(QStringLiteral("actionSceneDown"));
        QIcon icon5;
        icon5.addFile(QStringLiteral(":/res/images/down.png"), QSize(), QIcon::Normal, QIcon::Off);
        actionSceneDown->setIcon(icon5);
        actionSceneDown->setProperty("themeID", QVariant(QStringLiteral("downArrowIconSmall")));
        actionSourceDown = new QAction(OBSBasic);
        actionSourceDown->setObjectName(QStringLiteral("actionSourceDown"));
        actionSourceDown->setEnabled(true);
        actionSourceDown->setIcon(icon5);
        actionSourceDown->setProperty("themeID", QVariant(QStringLiteral("downArrowIconSmall")));
        actionShow_Recordings = new QAction(OBSBasic);
        actionShow_Recordings->setObjectName(QStringLiteral("actionShow_Recordings"));
        actionRemux = new QAction(OBSBasic);
        actionRemux->setObjectName(QStringLiteral("actionRemux"));
        action_Settings = new QAction(OBSBasic);
        action_Settings->setObjectName(QStringLiteral("action_Settings"));
        actionE_xit = new QAction(OBSBasic);
        actionE_xit->setObjectName(QStringLiteral("actionE_xit"));
        actionShowLogs = new QAction(OBSBasic);
        actionShowLogs->setObjectName(QStringLiteral("actionShowLogs"));
        actionUploadLastLog = new QAction(OBSBasic);
        actionUploadLastLog->setObjectName(QStringLiteral("actionUploadLastLog"));
        actionUploadCurrentLog = new QAction(OBSBasic);
        actionUploadCurrentLog->setObjectName(QStringLiteral("actionUploadCurrentLog"));
        actionViewCurrentLog = new QAction(OBSBasic);
        actionViewCurrentLog->setObjectName(QStringLiteral("actionViewCurrentLog"));
        actionUndo = new QAction(OBSBasic);
        actionUndo->setObjectName(QStringLiteral("actionUndo"));
        actionUndo->setEnabled(false);
        actionRedo = new QAction(OBSBasic);
        actionRedo->setObjectName(QStringLiteral("actionRedo"));
        actionRedo->setEnabled(false);
        actionEditTransform = new QAction(OBSBasic);
        actionEditTransform->setObjectName(QStringLiteral("actionEditTransform"));
        actionCopyTransform = new QAction(OBSBasic);
        actionCopyTransform->setObjectName(QStringLiteral("actionCopyTransform"));
        actionPasteTransform = new QAction(OBSBasic);
        actionPasteTransform->setObjectName(QStringLiteral("actionPasteTransform"));
        actionPasteTransform->setEnabled(false);
        actionRotate90CW = new QAction(OBSBasic);
        actionRotate90CW->setObjectName(QStringLiteral("actionRotate90CW"));
        actionRotate90CCW = new QAction(OBSBasic);
        actionRotate90CCW->setObjectName(QStringLiteral("actionRotate90CCW"));
        actionRotate180 = new QAction(OBSBasic);
        actionRotate180->setObjectName(QStringLiteral("actionRotate180"));
        actionFitToScreen = new QAction(OBSBasic);
        actionFitToScreen->setObjectName(QStringLiteral("actionFitToScreen"));
        actionStretchToScreen = new QAction(OBSBasic);
        actionStretchToScreen->setObjectName(QStringLiteral("actionStretchToScreen"));
        actionResetTransform = new QAction(OBSBasic);
        actionResetTransform->setObjectName(QStringLiteral("actionResetTransform"));
        actionCenterToScreen = new QAction(OBSBasic);
        actionCenterToScreen->setObjectName(QStringLiteral("actionCenterToScreen"));
        actionFlipHorizontal = new QAction(OBSBasic);
        actionFlipHorizontal->setObjectName(QStringLiteral("actionFlipHorizontal"));
        actionFlipVertical = new QAction(OBSBasic);
        actionFlipVertical->setObjectName(QStringLiteral("actionFlipVertical"));
        actionMoveUp = new QAction(OBSBasic);
        actionMoveUp->setObjectName(QStringLiteral("actionMoveUp"));
        actionMoveDown = new QAction(OBSBasic);
        actionMoveDown->setObjectName(QStringLiteral("actionMoveDown"));
        actionMoveToTop = new QAction(OBSBasic);
        actionMoveToTop->setObjectName(QStringLiteral("actionMoveToTop"));
        actionMoveToBottom = new QAction(OBSBasic);
        actionMoveToBottom->setObjectName(QStringLiteral("actionMoveToBottom"));
        actionCheckForUpdates = new QAction(OBSBasic);
        actionCheckForUpdates->setObjectName(QStringLiteral("actionCheckForUpdates"));
        actionInteract = new QAction(OBSBasic);
        actionInteract->setObjectName(QStringLiteral("actionInteract"));
        actionAdvAudioProperties = new QAction(OBSBasic);
        actionAdvAudioProperties->setObjectName(QStringLiteral("actionAdvAudioProperties"));
        actionWebsite = new QAction(OBSBasic);
        actionWebsite->setObjectName(QStringLiteral("actionWebsite"));
        actionNewSceneCollection = new QAction(OBSBasic);
        actionNewSceneCollection->setObjectName(QStringLiteral("actionNewSceneCollection"));
        actionDupSceneCollection = new QAction(OBSBasic);
        actionDupSceneCollection->setObjectName(QStringLiteral("actionDupSceneCollection"));
        actionRenameSceneCollection = new QAction(OBSBasic);
        actionRenameSceneCollection->setObjectName(QStringLiteral("actionRenameSceneCollection"));
        actionRemoveSceneCollection = new QAction(OBSBasic);
        actionRemoveSceneCollection->setObjectName(QStringLiteral("actionRemoveSceneCollection"));
        actionImportSceneCollection = new QAction(OBSBasic);
        actionImportSceneCollection->setObjectName(QStringLiteral("actionImportSceneCollection"));
        actionExportSceneCollection = new QAction(OBSBasic);
        actionExportSceneCollection->setObjectName(QStringLiteral("actionExportSceneCollection"));
        actionNewProfile = new QAction(OBSBasic);
        actionNewProfile->setObjectName(QStringLiteral("actionNewProfile"));
        actionDupProfile = new QAction(OBSBasic);
        actionDupProfile->setObjectName(QStringLiteral("actionDupProfile"));
        actionRenameProfile = new QAction(OBSBasic);
        actionRenameProfile->setObjectName(QStringLiteral("actionRenameProfile"));
        actionRemoveProfile = new QAction(OBSBasic);
        actionRemoveProfile->setObjectName(QStringLiteral("actionRemoveProfile"));
        actionImportProfile = new QAction(OBSBasic);
        actionImportProfile->setObjectName(QStringLiteral("actionImportProfile"));
        actionExportProfile = new QAction(OBSBasic);
        actionExportProfile->setObjectName(QStringLiteral("actionExportProfile"));
        actionShowSettingsFolder = new QAction(OBSBasic);
        actionShowSettingsFolder->setObjectName(QStringLiteral("actionShowSettingsFolder"));
        actionShowProfileFolder = new QAction(OBSBasic);
        actionShowProfileFolder->setObjectName(QStringLiteral("actionShowProfileFolder"));
        actionAlwaysOnTop = new QAction(OBSBasic);
        actionAlwaysOnTop->setObjectName(QStringLiteral("actionAlwaysOnTop"));
        actionAlwaysOnTop->setCheckable(true);
        toggleListboxToolbars = new QAction(OBSBasic);
        toggleListboxToolbars->setObjectName(QStringLiteral("toggleListboxToolbars"));
        toggleListboxToolbars->setCheckable(true);
        toggleListboxToolbars->setChecked(true);
        toggleStatusBar = new QAction(OBSBasic);
        toggleStatusBar->setObjectName(QStringLiteral("toggleStatusBar"));
        toggleStatusBar->setCheckable(true);
        toggleStatusBar->setChecked(true);
        actionLockPreview = new QAction(OBSBasic);
        actionLockPreview->setObjectName(QStringLiteral("actionLockPreview"));
        actionLockPreview->setCheckable(true);
        actionScaleWindow = new QAction(OBSBasic);
        actionScaleWindow->setObjectName(QStringLiteral("actionScaleWindow"));
        actionScaleWindow->setCheckable(true);
        actionScaleCanvas = new QAction(OBSBasic);
        actionScaleCanvas->setObjectName(QStringLiteral("actionScaleCanvas"));
        actionScaleCanvas->setCheckable(true);
        actionScaleOutput = new QAction(OBSBasic);
        actionScaleOutput->setObjectName(QStringLiteral("actionScaleOutput"));
        actionScaleOutput->setCheckable(true);
        actionPasteDup = new QAction(OBSBasic);
        actionPasteDup->setObjectName(QStringLiteral("actionPasteDup"));
        autoConfigure2 = new QAction(OBSBasic);
        autoConfigure2->setObjectName(QStringLiteral("autoConfigure2"));
        autoConfigure = new QAction(OBSBasic);
        autoConfigure->setObjectName(QStringLiteral("autoConfigure"));
        stats = new QAction(OBSBasic);
        stats->setObjectName(QStringLiteral("stats"));
        resetUI = new QAction(OBSBasic);
        resetUI->setObjectName(QStringLiteral("resetUI"));
        lockUI = new QAction(OBSBasic);
        lockUI->setObjectName(QStringLiteral("lockUI"));
        lockUI->setCheckable(true);
        lockUI->setChecked(true);
        toggleScenes = new QAction(OBSBasic);
        toggleScenes->setObjectName(QStringLiteral("toggleScenes"));
        toggleScenes->setCheckable(true);
        toggleScenes->setChecked(true);
        toggleSources = new QAction(OBSBasic);
        toggleSources->setObjectName(QStringLiteral("toggleSources"));
        toggleSources->setCheckable(true);
        toggleSources->setChecked(true);
        toggleMixer = new QAction(OBSBasic);
        toggleMixer->setObjectName(QStringLiteral("toggleMixer"));
        toggleMixer->setCheckable(true);
        toggleMixer->setChecked(true);
        toggleTransitions = new QAction(OBSBasic);
        toggleTransitions->setObjectName(QStringLiteral("toggleTransitions"));
        toggleTransitions->setCheckable(true);
        toggleTransitions->setChecked(true);
        toggleControls = new QAction(OBSBasic);
        toggleControls->setObjectName(QStringLiteral("toggleControls"));
        toggleControls->setCheckable(true);
        toggleControls->setChecked(true);
        actionHelpPortal = new QAction(OBSBasic);
        actionHelpPortal->setObjectName(QStringLiteral("actionHelpPortal"));
        actionShowCrashLogs = new QAction(OBSBasic);
        actionShowCrashLogs->setObjectName(QStringLiteral("actionShowCrashLogs"));
        actionUploadLastCrashLog = new QAction(OBSBasic);
        actionUploadLastCrashLog->setObjectName(QStringLiteral("actionUploadLastCrashLog"));
        centralwidget = new QWidget(OBSBasic);
        centralwidget->setObjectName(QStringLiteral("centralwidget"));
        verticalLayout = new QVBoxLayout(centralwidget);
        verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName(QStringLiteral("horizontalLayout_2"));
        previewDisabledLabel = new QLabel(centralwidget);
        previewDisabledLabel->setObjectName(QStringLiteral("previewDisabledLabel"));
        QSizePolicy sizePolicy1(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(previewDisabledLabel->sizePolicy().hasHeightForWidth());
        previewDisabledLabel->setSizePolicy(sizePolicy1);
        previewDisabledLabel->setContextMenuPolicy(Qt::CustomContextMenu);
        previewDisabledLabel->setAlignment(Qt::AlignCenter);

        horizontalLayout_2->addWidget(previewDisabledLabel);

        previewLayout = new QHBoxLayout();
        previewLayout->setSpacing(2);
        previewLayout->setObjectName(QStringLiteral("previewLayout"));
        previewTextLayout = new QVBoxLayout();
        previewTextLayout->setSpacing(4);
        previewTextLayout->setObjectName(QStringLiteral("previewTextLayout"));
        preview = new OBSBasicPreview(centralwidget);
        preview->setObjectName(QStringLiteral("preview"));
        sizePolicy1.setHeightForWidth(preview->sizePolicy().hasHeightForWidth());
        preview->setSizePolicy(sizePolicy1);
        preview->setMinimumSize(QSize(32, 32));
        preview->setFocusPolicy(Qt::ClickFocus);
        preview->setContextMenuPolicy(Qt::CustomContextMenu);

        previewTextLayout->addWidget(preview);


        previewLayout->addLayout(previewTextLayout);


        horizontalLayout_2->addLayout(previewLayout);


        verticalLayout->addLayout(horizontalLayout_2);

        OBSBasic->setCentralWidget(centralwidget);
        menubar = new QMenuBar(OBSBasic);
        menubar->setObjectName(QStringLiteral("menubar"));
        menubar->setGeometry(QRect(0, 0, 1079, 21));
        menu_File = new QMenu(menubar);
        menu_File->setObjectName(QStringLiteral("menu_File"));
        menuBasic_MainMenu_Help = new QMenu(menubar);
        menuBasic_MainMenu_Help->setObjectName(QStringLiteral("menuBasic_MainMenu_Help"));
        menuLogFiles = new QMenu(menuBasic_MainMenu_Help);
        menuLogFiles->setObjectName(QStringLiteral("menuLogFiles"));
        menuCrashLogs = new QMenu(menuBasic_MainMenu_Help);
        menuCrashLogs->setObjectName(QStringLiteral("menuCrashLogs"));
        menuBasic_MainMenu_Edit = new QMenu(menubar);
        menuBasic_MainMenu_Edit->setObjectName(QStringLiteral("menuBasic_MainMenu_Edit"));
        actionCopySource = new QAction(menuBasic_MainMenu_Edit);
        actionCopySource->setObjectName(QStringLiteral("actionCopySource"));
        actionPasteRef = new QAction(menuBasic_MainMenu_Edit);
        actionPasteRef->setObjectName(QStringLiteral("actionPasteRef"));
        actionPasteRef->setEnabled(false);
        actionCopyFilters = new QAction(menuBasic_MainMenu_Edit);
        actionCopyFilters->setObjectName(QStringLiteral("actionCopyFilters"));
        actionPasteFilters = new QAction(menuBasic_MainMenu_Edit);
        actionPasteFilters->setObjectName(QStringLiteral("actionPasteFilters"));
        actionPasteFilters->setEnabled(false);
        transformMenu = new QMenu(menuBasic_MainMenu_Edit);
        transformMenu->setObjectName(QStringLiteral("transformMenu"));
        orderMenu = new QMenu(menuBasic_MainMenu_Edit);
        orderMenu->setObjectName(QStringLiteral("orderMenu"));
        scalingMenu = new QMenu(menuBasic_MainMenu_Edit);
        scalingMenu->setObjectName(QStringLiteral("scalingMenu"));
        profileMenu = new QMenu(menubar);
        profileMenu->setObjectName(QStringLiteral("profileMenu"));
        sceneCollectionMenu = new QMenu(menubar);
        sceneCollectionMenu->setObjectName(QStringLiteral("sceneCollectionMenu"));
        viewMenu = new QMenu(menubar);
        viewMenu->setObjectName(QStringLiteral("viewMenu"));
        actionFullscreenInterface = new QAction(viewMenu);
        actionFullscreenInterface->setObjectName(QStringLiteral("actionFullscreenInterface"));
        viewMenuToolbars = new QMenu(viewMenu);
        viewMenuToolbars->setObjectName(QStringLiteral("viewMenuToolbars"));
        viewMenuDocks = new QMenu(viewMenu);
        viewMenuDocks->setObjectName(QStringLiteral("viewMenuDocks"));
        menuTools = new QMenu(menubar);
        menuTools->setObjectName(QStringLiteral("menuTools"));
        OBSBasic->setMenuBar(menubar);
        statusbar = new OBSBasicStatusBar(OBSBasic);
        statusbar->setObjectName(QStringLiteral("statusbar"));
        OBSBasic->setStatusBar(statusbar);
        scenesDock = new QDockWidget(OBSBasic);
        scenesDock->setObjectName(QStringLiteral("scenesDock"));
        scenesDock->setFeatures(QDockWidget::AllDockWidgetFeatures);
        dockWidgetContents_2 = new QWidget();
        dockWidgetContents_2->setObjectName(QStringLiteral("dockWidgetContents_2"));
        verticalLayout_6 = new QVBoxLayout(dockWidgetContents_2);
        verticalLayout_6->setObjectName(QStringLiteral("verticalLayout_6"));
        verticalLayout_6->setContentsMargins(4, 4, 4, 4);
        scenesFrame = new QFrame(dockWidgetContents_2);
        scenesFrame->setObjectName(QStringLiteral("scenesFrame"));
        sizePolicy1.setHeightForWidth(scenesFrame->sizePolicy().hasHeightForWidth());
        scenesFrame->setSizePolicy(sizePolicy1);
        scenesFrame->setMinimumSize(QSize(160, 0));
        scenesFrame->setFrameShape(QFrame::StyledPanel);
        scenesFrame->setFrameShadow(QFrame::Sunken);
        verticalLayout_12 = new QVBoxLayout(scenesFrame);
        verticalLayout_12->setSpacing(0);
        verticalLayout_12->setObjectName(QStringLiteral("verticalLayout_12"));
        verticalLayout_12->setContentsMargins(0, 0, 0, 0);
        scenes = new QListWidget(scenesFrame);
        scenes->setObjectName(QStringLiteral("scenes"));
        QSizePolicy sizePolicy2(QSizePolicy::Preferred, QSizePolicy::Expanding);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(scenes->sizePolicy().hasHeightForWidth());
        scenes->setSizePolicy(sizePolicy2);
        scenes->setContextMenuPolicy(Qt::CustomContextMenu);
        scenes->setFrameShape(QFrame::NoFrame);
        scenes->setFrameShadow(QFrame::Plain);
        scenes->setProperty("showDropIndicator", QVariant(true));
        scenes->setDragEnabled(true);
        scenes->setDragDropMode(QAbstractItemView::InternalMove);
        scenes->setDefaultDropAction(Qt::TargetMoveAction);

        verticalLayout_12->addWidget(scenes);

        scenesToolbar = new QToolBar(scenesFrame);
        scenesToolbar->setObjectName(QStringLiteral("scenesToolbar"));
        scenesToolbar->setIconSize(QSize(16, 16));
        scenesToolbar->setFloatable(false);

        verticalLayout_12->addWidget(scenesToolbar);

        scenesFixedSizeHSpacer = new QSpacerItem(150, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);

        verticalLayout_12->addItem(scenesFixedSizeHSpacer);


        verticalLayout_6->addWidget(scenesFrame);

        scenesDock->setWidget(dockWidgetContents_2);
        OBSBasic->addDockWidget(static_cast<Qt::DockWidgetArea>(8), scenesDock);
        sourcesDock = new QDockWidget(OBSBasic);
        sourcesDock->setObjectName(QStringLiteral("sourcesDock"));
        sourcesDock->setFeatures(QDockWidget::AllDockWidgetFeatures);
        dockWidgetContents_6 = new QWidget();
        dockWidgetContents_6->setObjectName(QStringLiteral("dockWidgetContents_6"));
        verticalLayout_5 = new QVBoxLayout(dockWidgetContents_6);
        verticalLayout_5->setObjectName(QStringLiteral("verticalLayout_5"));
        verticalLayout_5->setContentsMargins(4, 4, 4, 4);
        sourcesFrame = new QFrame(dockWidgetContents_6);
        sourcesFrame->setObjectName(QStringLiteral("sourcesFrame"));
        sizePolicy1.setHeightForWidth(sourcesFrame->sizePolicy().hasHeightForWidth());
        sourcesFrame->setSizePolicy(sizePolicy1);
        sourcesFrame->setMinimumSize(QSize(160, 0));
        sourcesFrame->setFrameShape(QFrame::StyledPanel);
        sourcesFrame->setFrameShadow(QFrame::Sunken);
        verticalLayout_17 = new QVBoxLayout(sourcesFrame);
        verticalLayout_17->setSpacing(0);
        verticalLayout_17->setObjectName(QStringLiteral("verticalLayout_17"));
        verticalLayout_17->setContentsMargins(0, 0, 0, 0);
        sources = new SourceListWidget(sourcesFrame);
        sources->setObjectName(QStringLiteral("sources"));
        sizePolicy2.setHeightForWidth(sources->sizePolicy().hasHeightForWidth());
        sources->setSizePolicy(sizePolicy2);
        sources->setContextMenuPolicy(Qt::CustomContextMenu);
        sources->setFrameShape(QFrame::NoFrame);
        sources->setProperty("showDropIndicator", QVariant(true));
        sources->setDragEnabled(true);
        sources->setDragDropMode(QAbstractItemView::InternalMove);
        sources->setDefaultDropAction(Qt::TargetMoveAction);
        sources->setSelectionMode(QAbstractItemView::ExtendedSelection);

        verticalLayout_17->addWidget(sources);

        sourcesToolbar = new QToolBar(sourcesFrame);
        sourcesToolbar->setObjectName(QStringLiteral("sourcesToolbar"));
        sourcesToolbar->setIconSize(QSize(16, 16));
        sourcesToolbar->setFloatable(false);

        verticalLayout_17->addWidget(sourcesToolbar);

        sourcesFixedSizeHSpacer = new QSpacerItem(150, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);

        verticalLayout_17->addItem(sourcesFixedSizeHSpacer);


        verticalLayout_5->addWidget(sourcesFrame);

        sourcesDock->setWidget(dockWidgetContents_6);
        OBSBasic->addDockWidget(static_cast<Qt::DockWidgetArea>(8), sourcesDock);
        mixerDock = new QDockWidget(OBSBasic);
        mixerDock->setObjectName(QStringLiteral("mixerDock"));
        mixerDock->setFeatures(QDockWidget::AllDockWidgetFeatures);
        dockWidgetContents_7 = new QWidget();
        dockWidgetContents_7->setObjectName(QStringLiteral("dockWidgetContents_7"));
        verticalLayout_4 = new QVBoxLayout(dockWidgetContents_7);
        verticalLayout_4->setObjectName(QStringLiteral("verticalLayout_4"));
        verticalLayout_4->setContentsMargins(4, 4, 4, 4);
        mixerScrollArea = new VScrollArea(dockWidgetContents_7);
        mixerScrollArea->setObjectName(QStringLiteral("mixerScrollArea"));
        mixerScrollArea->setMinimumSize(QSize(220, 0));
        mixerScrollArea->setContextMenuPolicy(Qt::CustomContextMenu);
        mixerScrollArea->setFrameShape(QFrame::StyledPanel);
        mixerScrollArea->setFrameShadow(QFrame::Sunken);
        mixerScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        mixerScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        mixerScrollArea->setWidgetResizable(true);
        volumeWidgets = new QWidget();
        volumeWidgets->setObjectName(QStringLiteral("volumeWidgets"));
        volumeWidgets->setGeometry(QRect(0, 0, 230, 16));
        QSizePolicy sizePolicy3(QSizePolicy::Preferred, QSizePolicy::Maximum);
        sizePolicy3.setHorizontalStretch(0);
        sizePolicy3.setVerticalStretch(0);
        sizePolicy3.setHeightForWidth(volumeWidgets->sizePolicy().hasHeightForWidth());
        volumeWidgets->setSizePolicy(sizePolicy3);
        verticalLayout_18 = new QVBoxLayout(volumeWidgets);
        verticalLayout_18->setSpacing(0);
        verticalLayout_18->setObjectName(QStringLiteral("verticalLayout_18"));
        verticalLayout_18->setContentsMargins(0, 0, 0, 0);
        mixerScrollArea->setWidget(volumeWidgets);

        verticalLayout_4->addWidget(mixerScrollArea);

        mixerDock->setWidget(dockWidgetContents_7);
        OBSBasic->addDockWidget(static_cast<Qt::DockWidgetArea>(8), mixerDock);
        transitionsDock = new QDockWidget(OBSBasic);
        transitionsDock->setObjectName(QStringLiteral("transitionsDock"));
        transitionsDock->setFeatures(QDockWidget::AllDockWidgetFeatures);
        dockWidgetContents_5 = new QWidget();
        dockWidgetContents_5->setObjectName(QStringLiteral("dockWidgetContents_5"));
        verticalLayout_3 = new QVBoxLayout(dockWidgetContents_5);
        verticalLayout_3->setObjectName(QStringLiteral("verticalLayout_3"));
        verticalLayout_3->setContentsMargins(4, 4, 4, 4);
        transitionsContainer = new QWidget(dockWidgetContents_5);
        transitionsContainer->setObjectName(QStringLiteral("transitionsContainer"));
        verticalLayout_2 = new QVBoxLayout(transitionsContainer);
        verticalLayout_2->setSpacing(4);
        verticalLayout_2->setObjectName(QStringLiteral("verticalLayout_2"));
        verticalLayout_2->setContentsMargins(1, 1, 1, 2);
        transitions = new QComboBox(transitionsContainer);
        transitions->setObjectName(QStringLiteral("transitions"));
        transitions->setMinimumSize(QSize(120, 0));

        verticalLayout_2->addWidget(transitions);

        horizontalLayout_4 = new QHBoxLayout();
        horizontalLayout_4->setSpacing(4);
        horizontalLayout_4->setObjectName(QStringLiteral("horizontalLayout_4"));
        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_4->addItem(horizontalSpacer);

        transitionAdd = new QPushButton(transitionsContainer);
        transitionAdd->setObjectName(QStringLiteral("transitionAdd"));
        QSizePolicy sizePolicy4(QSizePolicy::Maximum, QSizePolicy::Maximum);
        sizePolicy4.setHorizontalStretch(0);
        sizePolicy4.setVerticalStretch(0);
        sizePolicy4.setHeightForWidth(transitionAdd->sizePolicy().hasHeightForWidth());
        transitionAdd->setSizePolicy(sizePolicy4);
        transitionAdd->setMaximumSize(QSize(22, 22));
        transitionAdd->setText(QStringLiteral(""));
        transitionAdd->setIcon(icon1);
        transitionAdd->setFlat(true);
        transitionAdd->setProperty("themeID", QVariant(QStringLiteral("addIconSmall")));

        horizontalLayout_4->addWidget(transitionAdd);

        transitionRemove = new QPushButton(transitionsContainer);
        transitionRemove->setObjectName(QStringLiteral("transitionRemove"));
        sizePolicy4.setHeightForWidth(transitionRemove->sizePolicy().hasHeightForWidth());
        transitionRemove->setSizePolicy(sizePolicy4);
        transitionRemove->setMaximumSize(QSize(22, 22));
        transitionRemove->setText(QStringLiteral(""));
        transitionRemove->setIcon(icon2);
        transitionRemove->setFlat(true);
        transitionRemove->setProperty("themeID", QVariant(QStringLiteral("removeIconSmall")));

        horizontalLayout_4->addWidget(transitionRemove);

        transitionProps = new QPushButton(transitionsContainer);
        transitionProps->setObjectName(QStringLiteral("transitionProps"));
        sizePolicy4.setHeightForWidth(transitionProps->sizePolicy().hasHeightForWidth());
        transitionProps->setSizePolicy(sizePolicy4);
        transitionProps->setMaximumSize(QSize(22, 22));
        transitionProps->setText(QStringLiteral(""));
        QIcon icon6;
        icon6.addFile(QStringLiteral(":/res/images/configuration21_16.png"), QSize(), QIcon::Normal, QIcon::Off);
        transitionProps->setIcon(icon6);
        transitionProps->setFlat(true);
        transitionProps->setProperty("themeID", QVariant(QStringLiteral("configIconSmall")));

        horizontalLayout_4->addWidget(transitionProps);


        verticalLayout_2->addLayout(horizontalLayout_4);

        horizontalLayout_3 = new QHBoxLayout();
        horizontalLayout_3->setSpacing(4);
        horizontalLayout_3->setObjectName(QStringLiteral("horizontalLayout_3"));
        transitionDurationLabel = new QLabel(transitionsContainer);
        transitionDurationLabel->setObjectName(QStringLiteral("transitionDurationLabel"));
        QSizePolicy sizePolicy5(QSizePolicy::Maximum, QSizePolicy::Preferred);
        sizePolicy5.setHorizontalStretch(0);
        sizePolicy5.setVerticalStretch(0);
        sizePolicy5.setHeightForWidth(transitionDurationLabel->sizePolicy().hasHeightForWidth());
        transitionDurationLabel->setSizePolicy(sizePolicy5);

        horizontalLayout_3->addWidget(transitionDurationLabel);

        transitionDuration = new QSpinBox(transitionsContainer);
        transitionDuration->setObjectName(QStringLiteral("transitionDuration"));
        transitionDuration->setMinimum(2);
        transitionDuration->setMaximum(10000);
        transitionDuration->setSingleStep(50);
        transitionDuration->setValue(300);

        horizontalLayout_3->addWidget(transitionDuration);


        verticalLayout_2->addLayout(horizontalLayout_3);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_2->addItem(verticalSpacer);


        verticalLayout_3->addWidget(transitionsContainer);

        transitionsDock->setWidget(dockWidgetContents_5);
        OBSBasic->addDockWidget(static_cast<Qt::DockWidgetArea>(8), transitionsDock);
        controlsDock = new QDockWidget(OBSBasic);
        controlsDock->setObjectName(QStringLiteral("controlsDock"));
        controlsDock->setFeatures(QDockWidget::AllDockWidgetFeatures);
        dockWidgetContents_3 = new QWidget();
        dockWidgetContents_3->setObjectName(QStringLiteral("dockWidgetContents_3"));
        buttonsVLayout = new QVBoxLayout(dockWidgetContents_3);
        buttonsVLayout->setSpacing(2);
        buttonsVLayout->setObjectName(QStringLiteral("buttonsVLayout"));
        buttonsVLayout->setContentsMargins(4, 4, 4, 4);
        streamButton = new QPushButton(dockWidgetContents_3);
        streamButton->setObjectName(QStringLiteral("streamButton"));
        streamButton->setEnabled(true);
        QSizePolicy sizePolicy6(QSizePolicy::Preferred, QSizePolicy::Fixed);
        sizePolicy6.setHorizontalStretch(0);
        sizePolicy6.setVerticalStretch(0);
        sizePolicy6.setHeightForWidth(streamButton->sizePolicy().hasHeightForWidth());
        streamButton->setSizePolicy(sizePolicy6);
        streamButton->setCheckable(true);

        buttonsVLayout->addWidget(streamButton);

        recordButton = new QPushButton(dockWidgetContents_3);
        recordButton->setObjectName(QStringLiteral("recordButton"));
        recordButton->setEnabled(true);
        sizePolicy6.setHeightForWidth(recordButton->sizePolicy().hasHeightForWidth());
        recordButton->setSizePolicy(sizePolicy6);
        recordButton->setMinimumSize(QSize(130, 0));
        recordButton->setCheckable(true);

        buttonsVLayout->addWidget(recordButton);

        modeSwitch = new QPushButton(dockWidgetContents_3);
        modeSwitch->setObjectName(QStringLiteral("modeSwitch"));
        modeSwitch->setCheckable(true);

        buttonsVLayout->addWidget(modeSwitch);

        settingsButton = new QPushButton(dockWidgetContents_3);
        settingsButton->setObjectName(QStringLiteral("settingsButton"));
        sizePolicy6.setHeightForWidth(settingsButton->sizePolicy().hasHeightForWidth());
        settingsButton->setSizePolicy(sizePolicy6);

        buttonsVLayout->addWidget(settingsButton);

        exitButton = new QPushButton(dockWidgetContents_3);
        exitButton->setObjectName(QStringLiteral("exitButton"));
        sizePolicy6.setHeightForWidth(exitButton->sizePolicy().hasHeightForWidth());
        exitButton->setSizePolicy(sizePolicy6);

        buttonsVLayout->addWidget(exitButton);

        expVSpacer = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);

        buttonsVLayout->addItem(expVSpacer);

        controlsDock->setWidget(dockWidgetContents_3);
        OBSBasic->addDockWidget(static_cast<Qt::DockWidgetArea>(8), controlsDock);
#ifndef QT_NO_SHORTCUT
        transitionDurationLabel->setBuddy(transitionDuration);
#endif // QT_NO_SHORTCUT

        preview->addAction(actionRemoveSource);
        menubar->addAction(menu_File->menuAction());
        menubar->addAction(menuBasic_MainMenu_Edit->menuAction());
        menubar->addAction(viewMenu->menuAction());
        menubar->addAction(profileMenu->menuAction());
        menubar->addAction(sceneCollectionMenu->menuAction());
        menubar->addAction(menuTools->menuAction());
        menubar->addAction(menuBasic_MainMenu_Help->menuAction());
        menu_File->addAction(actionShow_Recordings);
        menu_File->addAction(actionRemux);
        menu_File->addSeparator();
        menu_File->addAction(action_Settings);
        menu_File->addAction(actionShowSettingsFolder);
        menu_File->addAction(actionShowProfileFolder);
        menu_File->addSeparator();
        menu_File->addAction(actionAlwaysOnTop);
        menu_File->addSeparator();
        menu_File->addAction(actionE_xit);
        menuBasic_MainMenu_Help->addAction(actionHelpPortal);
        menuBasic_MainMenu_Help->addAction(actionWebsite);
        menuBasic_MainMenu_Help->addSeparator();
        menuBasic_MainMenu_Help->addAction(menuLogFiles->menuAction());
        menuBasic_MainMenu_Help->addAction(menuCrashLogs->menuAction());
        menuBasic_MainMenu_Help->addSeparator();
        menuBasic_MainMenu_Help->addAction(actionCheckForUpdates);
        menuLogFiles->addAction(actionShowLogs);
        menuLogFiles->addAction(actionUploadCurrentLog);
        menuLogFiles->addAction(actionUploadLastLog);
        menuLogFiles->addAction(actionViewCurrentLog);
        menuCrashLogs->addAction(actionShowCrashLogs);
        menuCrashLogs->addAction(actionUploadLastCrashLog);
        menuBasic_MainMenu_Edit->addAction(actionCopySource);
        menuBasic_MainMenu_Edit->addAction(actionPasteRef);
        menuBasic_MainMenu_Edit->addAction(actionPasteDup);
        menuBasic_MainMenu_Edit->addSeparator();
        menuBasic_MainMenu_Edit->addAction(actionCopyFilters);
        menuBasic_MainMenu_Edit->addAction(actionPasteFilters);
        menuBasic_MainMenu_Edit->addSeparator();
        menuBasic_MainMenu_Edit->addAction(transformMenu->menuAction());
        menuBasic_MainMenu_Edit->addAction(orderMenu->menuAction());
        menuBasic_MainMenu_Edit->addAction(scalingMenu->menuAction());
        menuBasic_MainMenu_Edit->addAction(actionLockPreview);
        menuBasic_MainMenu_Edit->addSeparator();
        menuBasic_MainMenu_Edit->addAction(actionAdvAudioProperties);
        transformMenu->addAction(actionEditTransform);
        transformMenu->addAction(actionCopyTransform);
        transformMenu->addAction(actionPasteTransform);
        transformMenu->addAction(actionResetTransform);
        transformMenu->addSeparator();
        transformMenu->addAction(actionRotate90CW);
        transformMenu->addAction(actionRotate90CCW);
        transformMenu->addAction(actionRotate180);
        transformMenu->addSeparator();
        transformMenu->addAction(actionFlipHorizontal);
        transformMenu->addAction(actionFlipVertical);
        transformMenu->addSeparator();
        transformMenu->addAction(actionFitToScreen);
        transformMenu->addAction(actionStretchToScreen);
        transformMenu->addAction(actionCenterToScreen);
        orderMenu->addAction(actionMoveUp);
        orderMenu->addAction(actionMoveDown);
        orderMenu->addSeparator();
        orderMenu->addAction(actionMoveToTop);
        orderMenu->addAction(actionMoveToBottom);
        scalingMenu->addAction(actionScaleWindow);
        scalingMenu->addAction(actionScaleCanvas);
        scalingMenu->addAction(actionScaleOutput);
        profileMenu->addAction(actionNewProfile);
        profileMenu->addAction(actionDupProfile);
        profileMenu->addAction(actionRenameProfile);
        profileMenu->addAction(actionRemoveProfile);
        profileMenu->addAction(actionImportProfile);
        profileMenu->addAction(actionExportProfile);
        profileMenu->addSeparator();
        sceneCollectionMenu->addAction(actionNewSceneCollection);
        sceneCollectionMenu->addAction(actionDupSceneCollection);
        sceneCollectionMenu->addAction(actionRenameSceneCollection);
        sceneCollectionMenu->addAction(actionRemoveSceneCollection);
        sceneCollectionMenu->addAction(actionImportSceneCollection);
        sceneCollectionMenu->addAction(actionExportSceneCollection);
        sceneCollectionMenu->addSeparator();
        viewMenu->addAction(actionFullscreenInterface);
        viewMenu->addSeparator();
        viewMenu->addAction(viewMenuDocks->menuAction());
        viewMenu->addAction(viewMenuToolbars->menuAction());
        viewMenu->addAction(toggleStatusBar);
        viewMenu->addSeparator();
        viewMenu->addAction(stats);
        viewMenuToolbars->addAction(toggleListboxToolbars);
        viewMenuDocks->addAction(resetUI);
        viewMenuDocks->addAction(lockUI);
        viewMenuDocks->addSeparator();
        viewMenuDocks->addAction(toggleScenes);
        viewMenuDocks->addAction(toggleSources);
        viewMenuDocks->addAction(toggleMixer);
        viewMenuDocks->addAction(toggleTransitions);
        viewMenuDocks->addAction(toggleControls);
        menuTools->addAction(autoConfigure);
        menuTools->addSeparator();
        scenes->addAction(actionRemoveScene);
        scenesToolbar->addAction(actionAddScene);
        scenesToolbar->addAction(actionRemoveScene);
        scenesToolbar->addSeparator();
        scenesToolbar->addAction(actionSceneUp);
        scenesToolbar->addAction(actionSceneDown);
        sources->addAction(actionRemoveSource);
        sourcesToolbar->addAction(actionAddSource);
        sourcesToolbar->addAction(actionRemoveSource);
        sourcesToolbar->addAction(actionSourceProperties);
        sourcesToolbar->addSeparator();
        sourcesToolbar->addAction(actionSourceUp);
        sourcesToolbar->addAction(actionSourceDown);

        retranslateUi(OBSBasic);
        QObject::connect(actionE_xit, SIGNAL(triggered()), OBSBasic, SLOT(close()));
        QObject::connect(exitButton, SIGNAL(clicked()), OBSBasic, SLOT(close()));

        QMetaObject::connectSlotsByName(OBSBasic);
    } // setupUi

    void retranslateUi(QMainWindow *OBSBasic)
    {
        OBSBasic->setWindowTitle(QApplication::translate("OBSBasic", ".MainWindow", 0));
        actionAddScene->setText(QApplication::translate("OBSBasic", "Add", 0));
        actionAddSource->setText(QApplication::translate("OBSBasic", "Add", 0));
        actionRemoveScene->setText(QApplication::translate("OBSBasic", "Remove", 0));
        actionRemoveScene->setShortcut(QApplication::translate("OBSBasic", "Del", 0));
        actionRemoveSource->setText(QApplication::translate("OBSBasic", "Remove", 0));
        actionRemoveSource->setShortcut(QApplication::translate("OBSBasic", "Del", 0));
        actionSourceProperties->setText(QApplication::translate("OBSBasic", "Properties", 0));
        actionSceneUp->setText(QApplication::translate("OBSBasic", "MoveUp", 0));
        actionSourceUp->setText(QApplication::translate("OBSBasic", "MoveUp", 0));
        actionSceneDown->setText(QApplication::translate("OBSBasic", "MoveDown", 0));
        actionSourceDown->setText(QApplication::translate("OBSBasic", "MoveDown", 0));
        actionShow_Recordings->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.File.ShowRecordings", 0));
        actionRemux->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.File.Remux", 0));
        action_Settings->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.File.Settings", 0));
        actionE_xit->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.File.Exit", 0));
        actionShowLogs->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.Help.Logs.ShowLogs", 0));
        actionUploadLastLog->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.Help.Logs.UploadLastLog", 0));
        actionUploadCurrentLog->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.Help.Logs.UploadCurrentLog", 0));
        actionViewCurrentLog->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.Help.Logs.ViewCurrentLog", 0));
        actionUndo->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.Edit.Undo", 0));
        actionRedo->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.Edit.Redo", 0));
        actionEditTransform->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.Edit.Transform.EditTransform", 0));
        actionEditTransform->setShortcut(QApplication::translate("OBSBasic", "Ctrl+E", 0));
        actionCopyTransform->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.Edit.Transform.CopyTransform", 0));
        actionPasteTransform->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.Edit.Transform.PasteTransform", 0));
        actionRotate90CW->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.Edit.Transform.Rotate90CW", 0));
        actionRotate90CCW->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.Edit.Transform.Rotate90CCW", 0));
        actionRotate180->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.Edit.Transform.Rotate180", 0));
        actionFitToScreen->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.Edit.Transform.FitToScreen", 0));
        actionFitToScreen->setShortcut(QApplication::translate("OBSBasic", "Ctrl+F", 0));
        actionStretchToScreen->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.Edit.Transform.StretchToScreen", 0));
        actionStretchToScreen->setShortcut(QApplication::translate("OBSBasic", "Ctrl+S", 0));
        actionResetTransform->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.Edit.Transform.ResetTransform", 0));
        actionResetTransform->setShortcut(QApplication::translate("OBSBasic", "Ctrl+R", 0));
        actionCenterToScreen->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.Edit.Transform.CenterToScreen", 0));
        actionCenterToScreen->setShortcut(QApplication::translate("OBSBasic", "Ctrl+D", 0));
        actionFlipHorizontal->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.Edit.Transform.FlipHorizontal", 0));
        actionFlipVertical->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.Edit.Transform.FlipVertical", 0));
        actionMoveUp->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.Edit.Order.MoveUp", 0));
        actionMoveUp->setShortcut(QApplication::translate("OBSBasic", "Ctrl+Up", 0));
        actionMoveDown->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.Edit.Order.MoveDown", 0));
        actionMoveDown->setShortcut(QApplication::translate("OBSBasic", "Ctrl+Down", 0));
        actionMoveToTop->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.Edit.Order.MoveToTop", 0));
        actionMoveToTop->setShortcut(QApplication::translate("OBSBasic", "Ctrl+Home", 0));
        actionMoveToBottom->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.Edit.Order.MoveToBottom", 0));
        actionMoveToBottom->setShortcut(QApplication::translate("OBSBasic", "Ctrl+End", 0));
        actionCheckForUpdates->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.Help.CheckForUpdates", 0));
        actionInteract->setText(QApplication::translate("OBSBasic", "Interact", 0));
        actionAdvAudioProperties->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.Edit.AdvAudio", 0));
        actionWebsite->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.Help.Website", 0));
        actionNewSceneCollection->setText(QApplication::translate("OBSBasic", "New", 0));
        actionDupSceneCollection->setText(QApplication::translate("OBSBasic", "Duplicate", 0));
        actionRenameSceneCollection->setText(QApplication::translate("OBSBasic", "Rename", 0));
        actionRemoveSceneCollection->setText(QApplication::translate("OBSBasic", "Remove", 0));
        actionImportSceneCollection->setText(QApplication::translate("OBSBasic", "Import", 0));
        actionExportSceneCollection->setText(QApplication::translate("OBSBasic", "Export", 0));
        actionNewProfile->setText(QApplication::translate("OBSBasic", "New", 0));
        actionDupProfile->setText(QApplication::translate("OBSBasic", "Duplicate", 0));
        actionRenameProfile->setText(QApplication::translate("OBSBasic", "Rename", 0));
        actionRemoveProfile->setText(QApplication::translate("OBSBasic", "Remove", 0));
        actionImportProfile->setText(QApplication::translate("OBSBasic", "Import", 0));
        actionExportProfile->setText(QApplication::translate("OBSBasic", "Export", 0));
        actionShowSettingsFolder->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.File.ShowSettingsFolder", 0));
        actionShowProfileFolder->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.File.ShowProfileFolder", 0));
        actionAlwaysOnTop->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.AlwaysOnTop", 0));
        toggleListboxToolbars->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.View.Toolbars.Listboxes", 0));
        toggleStatusBar->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.View.StatusBar", 0));
        actionLockPreview->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.Edit.LockPreview", 0));
        actionScaleWindow->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.Edit.Scale.Window", 0));
        actionScaleCanvas->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.Edit.Scale.Canvas", 0));
        actionScaleOutput->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.Edit.Scale.Output", 0));
        actionPasteDup->setText(QApplication::translate("OBSBasic", "PasteDuplicate", 0));
        autoConfigure2->setText(QApplication::translate("OBSBasic", "Basic.AutoConfig", 0));
        autoConfigure->setText(QApplication::translate("OBSBasic", "Basic.AutoConfig.Beta", 0));
        stats->setText(QApplication::translate("OBSBasic", "Basic.Stats", 0));
        resetUI->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.View.Docks.ResetUI", 0));
        lockUI->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.View.Docks.LockUI", 0));
        toggleScenes->setText(QApplication::translate("OBSBasic", "Basic.Main.Scenes", 0));
        toggleSources->setText(QApplication::translate("OBSBasic", "Basic.Main.Sources", 0));
        toggleMixer->setText(QApplication::translate("OBSBasic", "Mixer", 0));
        toggleTransitions->setText(QApplication::translate("OBSBasic", "Basic.SceneTransitions", 0));
        toggleControls->setText(QApplication::translate("OBSBasic", "Basic.Main.Controls", 0));
        actionHelpPortal->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.Help.HelpPortal", 0));
        actionShowCrashLogs->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.Help.CrashLogs.ShowLogs", 0));
        actionUploadLastCrashLog->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.Help.CrashLogs.UploadLastLog", 0));
        previewDisabledLabel->setText(QApplication::translate("OBSBasic", "Basic.Main.PreviewDisabled", 0));
        menu_File->setTitle(QApplication::translate("OBSBasic", "Basic.MainMenu.File", 0));
        menuBasic_MainMenu_Help->setTitle(QApplication::translate("OBSBasic", "Basic.MainMenu.Help", 0));
        menuLogFiles->setTitle(QApplication::translate("OBSBasic", "Basic.MainMenu.Help.Logs", 0));
        menuCrashLogs->setTitle(QApplication::translate("OBSBasic", "Basic.MainMenu.Help.CrashLogs", 0));
        menuBasic_MainMenu_Edit->setTitle(QApplication::translate("OBSBasic", "Basic.MainMenu.Edit", 0));
        actionCopySource->setText(QApplication::translate("OBSBasic", "Copy", 0));
        actionCopySource->setShortcut(QApplication::translate("OBSBasic", "Ctrl+C", 0));
        actionPasteRef->setText(QApplication::translate("OBSBasic", "PasteReference", 0));
        actionPasteRef->setIconText(QApplication::translate("OBSBasic", "PasteReference", 0));
#ifndef QT_NO_TOOLTIP
        actionPasteRef->setToolTip(QApplication::translate("OBSBasic", "PasteReference", 0));
#endif // QT_NO_TOOLTIP
        actionPasteRef->setShortcut(QApplication::translate("OBSBasic", "Ctrl+V", 0));
        actionCopyFilters->setText(QApplication::translate("OBSBasic", "Copy.Filters", 0));
        actionPasteFilters->setText(QApplication::translate("OBSBasic", "Paste.Filters", 0));
        transformMenu->setTitle(QApplication::translate("OBSBasic", "Basic.MainMenu.Edit.Transform", 0));
        orderMenu->setTitle(QApplication::translate("OBSBasic", "Basic.MainMenu.Edit.Order", 0));
        scalingMenu->setTitle(QApplication::translate("OBSBasic", "Basic.MainMenu.Edit.Scale", 0));
        profileMenu->setTitle(QApplication::translate("OBSBasic", "Basic.MainMenu.Profile", 0));
        sceneCollectionMenu->setTitle(QApplication::translate("OBSBasic", "Basic.MainMenu.SceneCollection", 0));
        viewMenu->setTitle(QApplication::translate("OBSBasic", "Basic.MainMenu.View", 0));
        actionFullscreenInterface->setText(QApplication::translate("OBSBasic", "Basic.MainMenu.View.Fullscreen.Interface", 0));
        actionFullscreenInterface->setShortcut(QApplication::translate("OBSBasic", "F11", 0));
        viewMenuToolbars->setTitle(QApplication::translate("OBSBasic", "Basic.MainMenu.View.Toolbars", 0));
        viewMenuDocks->setTitle(QApplication::translate("OBSBasic", "Basic.MainMenu.View.Docks", 0));
        menuTools->setTitle(QApplication::translate("OBSBasic", "Basic.MainMenu.Tools", 0));
        scenesDock->setWindowTitle(QApplication::translate("OBSBasic", "Basic.Main.Scenes", 0));
        sourcesDock->setWindowTitle(QApplication::translate("OBSBasic", "Basic.Main.Sources", 0));
        mixerDock->setWindowTitle(QApplication::translate("OBSBasic", "Mixer", 0));
        transitionsDock->setWindowTitle(QApplication::translate("OBSBasic", "Basic.SceneTransitions", 0));
#ifndef QT_NO_ACCESSIBILITY
        transitions->setAccessibleName(QApplication::translate("OBSBasic", "Transition", 0));
#endif // QT_NO_ACCESSIBILITY
#ifndef QT_NO_TOOLTIP
        transitionAdd->setToolTip(QApplication::translate("OBSBasic", "Basic.AddTransition", 0));
#endif // QT_NO_TOOLTIP
#ifndef QT_NO_ACCESSIBILITY
        transitionAdd->setAccessibleName(QApplication::translate("OBSBasic", "Basic.AddTransition", 0));
#endif // QT_NO_ACCESSIBILITY
#ifndef QT_NO_TOOLTIP
        transitionRemove->setToolTip(QApplication::translate("OBSBasic", "Basic.RemoveTransition", 0));
#endif // QT_NO_TOOLTIP
#ifndef QT_NO_ACCESSIBILITY
        transitionRemove->setAccessibleName(QApplication::translate("OBSBasic", "Basic.RemoveTransition", 0));
#endif // QT_NO_ACCESSIBILITY
#ifndef QT_NO_TOOLTIP
        transitionProps->setToolTip(QApplication::translate("OBSBasic", "Basic.TransitionProperties", 0));
#endif // QT_NO_TOOLTIP
#ifndef QT_NO_ACCESSIBILITY
        transitionProps->setAccessibleName(QApplication::translate("OBSBasic", "Basic.TransitionProperties", 0));
#endif // QT_NO_ACCESSIBILITY
        transitionDurationLabel->setText(QApplication::translate("OBSBasic", "Basic.TransitionDuration", 0));
#ifndef QT_NO_ACCESSIBILITY
        transitionDuration->setAccessibleName(QApplication::translate("OBSBasic", "Basic.TransitionDuration", 0));
#endif // QT_NO_ACCESSIBILITY
        transitionDuration->setSuffix(QApplication::translate("OBSBasic", "ms", 0));
        controlsDock->setWindowTitle(QApplication::translate("OBSBasic", "Basic.Main.Controls", 0));
        streamButton->setText(QApplication::translate("OBSBasic", "Basic.Main.StartStreaming", 0));
        recordButton->setText(QApplication::translate("OBSBasic", "Basic.Main.StartRecording", 0));
        modeSwitch->setText(QApplication::translate("OBSBasic", "Basic.TogglePreviewProgramMode", 0));
        settingsButton->setText(QApplication::translate("OBSBasic", "Settings", 0));
        exitButton->setText(QApplication::translate("OBSBasic", "Exit", 0));
    } // retranslateUi

};

namespace Ui {
    class OBSBasic: public Ui_OBSBasic {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_OBSBASIC_H
