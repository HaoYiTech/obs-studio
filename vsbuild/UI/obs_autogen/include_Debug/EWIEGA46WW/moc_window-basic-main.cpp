/****************************************************************************
** Meta object code from reading C++ file 'window-basic-main.hpp'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.7.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../UI/window-basic-main.hpp"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'window-basic-main.hpp' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.7.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
struct qt_meta_stringdata_OBSBasic_t {
    QByteArrayData data[216];
    char stringdata0[4673];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_OBSBasic_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_OBSBasic_t qt_meta_stringdata_OBSBasic = {
    {
QT_MOC_LITERAL(0, 0, 8), // "OBSBasic"
QT_MOC_LITERAL(1, 9, 14), // "DeferSaveBegin"
QT_MOC_LITERAL(2, 24, 0), // ""
QT_MOC_LITERAL(3, 25, 12), // "DeferSaveEnd"
QT_MOC_LITERAL(4, 38, 14), // "StartStreaming"
QT_MOC_LITERAL(5, 53, 13), // "StopStreaming"
QT_MOC_LITERAL(6, 67, 18), // "ForceStopStreaming"
QT_MOC_LITERAL(7, 86, 19), // "StreamDelayStarting"
QT_MOC_LITERAL(8, 106, 3), // "sec"
QT_MOC_LITERAL(9, 110, 19), // "StreamDelayStopping"
QT_MOC_LITERAL(10, 130, 14), // "StreamingStart"
QT_MOC_LITERAL(11, 145, 14), // "StreamStopping"
QT_MOC_LITERAL(12, 160, 13), // "StreamingStop"
QT_MOC_LITERAL(13, 174, 9), // "errorcode"
QT_MOC_LITERAL(14, 184, 10), // "last_error"
QT_MOC_LITERAL(15, 195, 14), // "StartRecording"
QT_MOC_LITERAL(16, 210, 13), // "StopRecording"
QT_MOC_LITERAL(17, 224, 14), // "RecordingStart"
QT_MOC_LITERAL(18, 239, 14), // "RecordStopping"
QT_MOC_LITERAL(19, 254, 13), // "RecordingStop"
QT_MOC_LITERAL(20, 268, 4), // "code"
QT_MOC_LITERAL(21, 273, 17), // "StartReplayBuffer"
QT_MOC_LITERAL(22, 291, 16), // "StopReplayBuffer"
QT_MOC_LITERAL(23, 308, 17), // "ReplayBufferStart"
QT_MOC_LITERAL(24, 326, 16), // "ReplayBufferSave"
QT_MOC_LITERAL(25, 343, 20), // "ReplayBufferStopping"
QT_MOC_LITERAL(26, 364, 16), // "ReplayBufferStop"
QT_MOC_LITERAL(27, 381, 19), // "SaveProjectDeferred"
QT_MOC_LITERAL(28, 401, 11), // "SaveProject"
QT_MOC_LITERAL(29, 413, 13), // "SetTransition"
QT_MOC_LITERAL(30, 427, 9), // "OBSSource"
QT_MOC_LITERAL(31, 437, 10), // "transition"
QT_MOC_LITERAL(32, 448, 17), // "TransitionToScene"
QT_MOC_LITERAL(33, 466, 8), // "OBSScene"
QT_MOC_LITERAL(34, 475, 5), // "scene"
QT_MOC_LITERAL(35, 481, 5), // "force"
QT_MOC_LITERAL(36, 487, 6), // "direct"
QT_MOC_LITERAL(37, 494, 15), // "quickTransition"
QT_MOC_LITERAL(38, 510, 15), // "SetCurrentScene"
QT_MOC_LITERAL(39, 526, 12), // "AddSceneItem"
QT_MOC_LITERAL(40, 539, 12), // "OBSSceneItem"
QT_MOC_LITERAL(41, 552, 4), // "item"
QT_MOC_LITERAL(42, 557, 15), // "RemoveSceneItem"
QT_MOC_LITERAL(43, 573, 8), // "AddScene"
QT_MOC_LITERAL(44, 582, 6), // "source"
QT_MOC_LITERAL(45, 589, 11), // "RemoveScene"
QT_MOC_LITERAL(46, 601, 13), // "RenameSources"
QT_MOC_LITERAL(47, 615, 7), // "newName"
QT_MOC_LITERAL(48, 623, 8), // "prevName"
QT_MOC_LITERAL(49, 632, 15), // "SelectSceneItem"
QT_MOC_LITERAL(50, 648, 6), // "select"
QT_MOC_LITERAL(51, 655, 19), // "ActivateAudioSource"
QT_MOC_LITERAL(52, 675, 21), // "DeactivateAudioSource"
QT_MOC_LITERAL(53, 697, 22), // "DuplicateSelectedScene"
QT_MOC_LITERAL(54, 720, 19), // "RemoveSelectedScene"
QT_MOC_LITERAL(55, 740, 23), // "RemoveSelectedSceneItem"
QT_MOC_LITERAL(56, 764, 17), // "ToggleAlwaysOnTop"
QT_MOC_LITERAL(57, 782, 14), // "ReorderSources"
QT_MOC_LITERAL(58, 797, 13), // "ProcessHotkey"
QT_MOC_LITERAL(59, 811, 13), // "obs_hotkey_id"
QT_MOC_LITERAL(60, 825, 2), // "id"
QT_MOC_LITERAL(61, 828, 7), // "pressed"
QT_MOC_LITERAL(62, 836, 13), // "AddTransition"
QT_MOC_LITERAL(63, 850, 16), // "RenameTransition"
QT_MOC_LITERAL(64, 867, 17), // "TransitionClicked"
QT_MOC_LITERAL(65, 885, 17), // "TransitionStopped"
QT_MOC_LITERAL(66, 903, 22), // "TransitionFullyStopped"
QT_MOC_LITERAL(67, 926, 22), // "TriggerQuickTransition"
QT_MOC_LITERAL(68, 949, 20), // "SetDeinterlacingMode"
QT_MOC_LITERAL(69, 970, 21), // "SetDeinterlacingOrder"
QT_MOC_LITERAL(70, 992, 14), // "SetScaleFilter"
QT_MOC_LITERAL(71, 1007, 13), // "IconActivated"
QT_MOC_LITERAL(72, 1021, 33), // "QSystemTrayIcon::ActivationRe..."
QT_MOC_LITERAL(73, 1055, 6), // "reason"
QT_MOC_LITERAL(74, 1062, 10), // "SetShowing"
QT_MOC_LITERAL(75, 1073, 7), // "showing"
QT_MOC_LITERAL(76, 1081, 14), // "ToggleShowHide"
QT_MOC_LITERAL(77, 1096, 16), // "HideAudioControl"
QT_MOC_LITERAL(78, 1113, 22), // "UnhideAllAudioControls"
QT_MOC_LITERAL(79, 1136, 15), // "ToggleHideMixer"
QT_MOC_LITERAL(80, 1152, 17), // "MixerRenameSource"
QT_MOC_LITERAL(81, 1170, 45), // "on_mixerScrollArea_customCont..."
QT_MOC_LITERAL(82, 1216, 29), // "on_actionCopySource_triggered"
QT_MOC_LITERAL(83, 1246, 27), // "on_actionPasteRef_triggered"
QT_MOC_LITERAL(84, 1274, 27), // "on_actionPasteDup_triggered"
QT_MOC_LITERAL(85, 1302, 30), // "on_actionCopyFilters_triggered"
QT_MOC_LITERAL(86, 1333, 31), // "on_actionPasteFilters_triggered"
QT_MOC_LITERAL(87, 1365, 38), // "on_actionFullscreenInterface_..."
QT_MOC_LITERAL(88, 1404, 34), // "on_actionShow_Recordings_trig..."
QT_MOC_LITERAL(89, 1439, 24), // "on_actionRemux_triggered"
QT_MOC_LITERAL(90, 1464, 28), // "on_action_Settings_triggered"
QT_MOC_LITERAL(91, 1493, 37), // "on_actionAdvAudioProperties_t..."
QT_MOC_LITERAL(92, 1531, 24), // "on_advAudioProps_clicked"
QT_MOC_LITERAL(93, 1556, 26), // "on_advAudioProps_destroyed"
QT_MOC_LITERAL(94, 1583, 27), // "on_actionShowLogs_triggered"
QT_MOC_LITERAL(95, 1611, 35), // "on_actionUploadCurrentLog_tri..."
QT_MOC_LITERAL(96, 1647, 32), // "on_actionUploadLastLog_triggered"
QT_MOC_LITERAL(97, 1680, 33), // "on_actionViewCurrentLog_trigg..."
QT_MOC_LITERAL(98, 1714, 34), // "on_actionCheckForUpdates_trig..."
QT_MOC_LITERAL(99, 1749, 32), // "on_actionShowCrashLogs_triggered"
QT_MOC_LITERAL(100, 1782, 37), // "on_actionUploadLastCrashLog_t..."
QT_MOC_LITERAL(101, 1820, 32), // "on_actionEditTransform_triggered"
QT_MOC_LITERAL(102, 1853, 32), // "on_actionCopyTransform_triggered"
QT_MOC_LITERAL(103, 1886, 33), // "on_actionPasteTransform_trigg..."
QT_MOC_LITERAL(104, 1920, 29), // "on_actionRotate90CW_triggered"
QT_MOC_LITERAL(105, 1950, 30), // "on_actionRotate90CCW_triggered"
QT_MOC_LITERAL(106, 1981, 28), // "on_actionRotate180_triggered"
QT_MOC_LITERAL(107, 2010, 33), // "on_actionFlipHorizontal_trigg..."
QT_MOC_LITERAL(108, 2044, 31), // "on_actionFlipVertical_triggered"
QT_MOC_LITERAL(109, 2076, 30), // "on_actionFitToScreen_triggered"
QT_MOC_LITERAL(110, 2107, 34), // "on_actionStretchToScreen_trig..."
QT_MOC_LITERAL(111, 2142, 33), // "on_actionCenterToScreen_trigg..."
QT_MOC_LITERAL(112, 2176, 28), // "on_scenes_currentItemChanged"
QT_MOC_LITERAL(113, 2205, 16), // "QListWidgetItem*"
QT_MOC_LITERAL(114, 2222, 7), // "current"
QT_MOC_LITERAL(115, 2230, 4), // "prev"
QT_MOC_LITERAL(116, 2235, 36), // "on_scenes_customContextMenuRe..."
QT_MOC_LITERAL(117, 2272, 3), // "pos"
QT_MOC_LITERAL(118, 2276, 27), // "on_actionAddScene_triggered"
QT_MOC_LITERAL(119, 2304, 30), // "on_actionRemoveScene_triggered"
QT_MOC_LITERAL(120, 2335, 26), // "on_actionSceneUp_triggered"
QT_MOC_LITERAL(121, 2362, 28), // "on_actionSceneDown_triggered"
QT_MOC_LITERAL(122, 2391, 31), // "on_sources_itemSelectionChanged"
QT_MOC_LITERAL(123, 2423, 37), // "on_sources_customContextMenuR..."
QT_MOC_LITERAL(124, 2461, 28), // "on_sources_itemDoubleClicked"
QT_MOC_LITERAL(125, 2490, 27), // "on_scenes_itemDoubleClicked"
QT_MOC_LITERAL(126, 2518, 28), // "on_actionAddSource_triggered"
QT_MOC_LITERAL(127, 2547, 31), // "on_actionRemoveSource_triggered"
QT_MOC_LITERAL(128, 2579, 27), // "on_actionInteract_triggered"
QT_MOC_LITERAL(129, 2607, 35), // "on_actionSourceProperties_tri..."
QT_MOC_LITERAL(130, 2643, 27), // "on_actionSourceUp_triggered"
QT_MOC_LITERAL(131, 2671, 29), // "on_actionSourceDown_triggered"
QT_MOC_LITERAL(132, 2701, 25), // "on_actionMoveUp_triggered"
QT_MOC_LITERAL(133, 2727, 27), // "on_actionMoveDown_triggered"
QT_MOC_LITERAL(134, 2755, 28), // "on_actionMoveToTop_triggered"
QT_MOC_LITERAL(135, 2784, 31), // "on_actionMoveToBottom_triggered"
QT_MOC_LITERAL(136, 2816, 30), // "on_actionLockPreview_triggered"
QT_MOC_LITERAL(137, 2847, 26), // "on_scalingMenu_aboutToShow"
QT_MOC_LITERAL(138, 2874, 30), // "on_actionScaleWindow_triggered"
QT_MOC_LITERAL(139, 2905, 30), // "on_actionScaleCanvas_triggered"
QT_MOC_LITERAL(140, 2936, 30), // "on_actionScaleOutput_triggered"
QT_MOC_LITERAL(141, 2967, 23), // "on_streamButton_clicked"
QT_MOC_LITERAL(142, 2991, 23), // "on_recordButton_clicked"
QT_MOC_LITERAL(143, 3015, 25), // "on_settingsButton_clicked"
QT_MOC_LITERAL(144, 3041, 29), // "on_actionHelpPortal_triggered"
QT_MOC_LITERAL(145, 3071, 26), // "on_actionWebsite_triggered"
QT_MOC_LITERAL(146, 3098, 37), // "on_preview_customContextMenuR..."
QT_MOC_LITERAL(147, 3136, 37), // "on_program_customContextMenuR..."
QT_MOC_LITERAL(148, 3174, 50), // "on_previewDisabledLabel_custo..."
QT_MOC_LITERAL(149, 3225, 37), // "on_actionNewSceneCollection_t..."
QT_MOC_LITERAL(150, 3263, 37), // "on_actionDupSceneCollection_t..."
QT_MOC_LITERAL(151, 3301, 40), // "on_actionRenameSceneCollectio..."
QT_MOC_LITERAL(152, 3342, 40), // "on_actionRemoveSceneCollectio..."
QT_MOC_LITERAL(153, 3383, 40), // "on_actionImportSceneCollectio..."
QT_MOC_LITERAL(154, 3424, 40), // "on_actionExportSceneCollectio..."
QT_MOC_LITERAL(155, 3465, 29), // "on_actionNewProfile_triggered"
QT_MOC_LITERAL(156, 3495, 29), // "on_actionDupProfile_triggered"
QT_MOC_LITERAL(157, 3525, 32), // "on_actionRenameProfile_triggered"
QT_MOC_LITERAL(158, 3558, 32), // "on_actionRemoveProfile_triggered"
QT_MOC_LITERAL(159, 3591, 32), // "on_actionImportProfile_triggered"
QT_MOC_LITERAL(160, 3624, 32), // "on_actionExportProfile_triggered"
QT_MOC_LITERAL(161, 3657, 37), // "on_actionShowSettingsFolder_t..."
QT_MOC_LITERAL(162, 3695, 36), // "on_actionShowProfileFolder_tr..."
QT_MOC_LITERAL(163, 3732, 30), // "on_actionAlwaysOnTop_triggered"
QT_MOC_LITERAL(164, 3763, 32), // "on_toggleListboxToolbars_toggled"
QT_MOC_LITERAL(165, 3796, 7), // "visible"
QT_MOC_LITERAL(166, 3804, 26), // "on_toggleStatusBar_toggled"
QT_MOC_LITERAL(167, 3831, 34), // "on_transitions_currentIndexCh..."
QT_MOC_LITERAL(168, 3866, 5), // "index"
QT_MOC_LITERAL(169, 3872, 24), // "on_transitionAdd_clicked"
QT_MOC_LITERAL(170, 3897, 27), // "on_transitionRemove_clicked"
QT_MOC_LITERAL(171, 3925, 26), // "on_transitionProps_clicked"
QT_MOC_LITERAL(172, 3952, 21), // "on_modeSwitch_clicked"
QT_MOC_LITERAL(173, 3974, 26), // "on_autoConfigure_triggered"
QT_MOC_LITERAL(174, 4001, 18), // "on_stats_triggered"
QT_MOC_LITERAL(175, 4020, 20), // "on_resetUI_triggered"
QT_MOC_LITERAL(176, 4041, 17), // "on_lockUI_toggled"
QT_MOC_LITERAL(177, 4059, 4), // "lock"
QT_MOC_LITERAL(178, 4064, 17), // "logUploadFinished"
QT_MOC_LITERAL(179, 4082, 4), // "text"
QT_MOC_LITERAL(180, 4087, 5), // "error"
QT_MOC_LITERAL(181, 4093, 19), // "updateCheckFinished"
QT_MOC_LITERAL(182, 4113, 19), // "AddSourceFromAction"
QT_MOC_LITERAL(183, 4133, 14), // "MoveSceneToTop"
QT_MOC_LITERAL(184, 4148, 17), // "MoveSceneToBottom"
QT_MOC_LITERAL(185, 4166, 13), // "EditSceneName"
QT_MOC_LITERAL(186, 4180, 17), // "EditSceneItemName"
QT_MOC_LITERAL(187, 4198, 15), // "SceneNameEdited"
QT_MOC_LITERAL(188, 4214, 8), // "QWidget*"
QT_MOC_LITERAL(189, 4223, 6), // "editor"
QT_MOC_LITERAL(190, 4230, 34), // "QAbstractItemDelegate::EndEdi..."
QT_MOC_LITERAL(191, 4265, 7), // "endHint"
QT_MOC_LITERAL(192, 4273, 19), // "SceneItemNameEdited"
QT_MOC_LITERAL(193, 4293, 16), // "OpenSceneFilters"
QT_MOC_LITERAL(194, 4310, 11), // "OpenFilters"
QT_MOC_LITERAL(195, 4322, 20), // "EnablePreviewDisplay"
QT_MOC_LITERAL(196, 4343, 6), // "enable"
QT_MOC_LITERAL(197, 4350, 13), // "TogglePreview"
QT_MOC_LITERAL(198, 4364, 7), // "NudgeUp"
QT_MOC_LITERAL(199, 4372, 9), // "NudgeDown"
QT_MOC_LITERAL(200, 4382, 9), // "NudgeLeft"
QT_MOC_LITERAL(201, 4392, 10), // "NudgeRight"
QT_MOC_LITERAL(202, 4403, 26), // "OpenStudioProgramProjector"
QT_MOC_LITERAL(203, 4430, 20), // "OpenPreviewProjector"
QT_MOC_LITERAL(204, 4451, 19), // "OpenSourceProjector"
QT_MOC_LITERAL(205, 4471, 22), // "OpenMultiviewProjector"
QT_MOC_LITERAL(206, 4494, 18), // "OpenSceneProjector"
QT_MOC_LITERAL(207, 4513, 23), // "OpenStudioProgramWindow"
QT_MOC_LITERAL(208, 4537, 17), // "OpenPreviewWindow"
QT_MOC_LITERAL(209, 4555, 16), // "OpenSourceWindow"
QT_MOC_LITERAL(210, 4572, 19), // "OpenMultiviewWindow"
QT_MOC_LITERAL(211, 4592, 15), // "OpenSceneWindow"
QT_MOC_LITERAL(212, 4608, 12), // "DeferredLoad"
QT_MOC_LITERAL(213, 4621, 4), // "file"
QT_MOC_LITERAL(214, 4626, 12), // "requeueCount"
QT_MOC_LITERAL(215, 4639, 33) // "on_actionResetTransform_trigg..."

    },
    "OBSBasic\0DeferSaveBegin\0\0DeferSaveEnd\0"
    "StartStreaming\0StopStreaming\0"
    "ForceStopStreaming\0StreamDelayStarting\0"
    "sec\0StreamDelayStopping\0StreamingStart\0"
    "StreamStopping\0StreamingStop\0errorcode\0"
    "last_error\0StartRecording\0StopRecording\0"
    "RecordingStart\0RecordStopping\0"
    "RecordingStop\0code\0StartReplayBuffer\0"
    "StopReplayBuffer\0ReplayBufferStart\0"
    "ReplayBufferSave\0ReplayBufferStopping\0"
    "ReplayBufferStop\0SaveProjectDeferred\0"
    "SaveProject\0SetTransition\0OBSSource\0"
    "transition\0TransitionToScene\0OBSScene\0"
    "scene\0force\0direct\0quickTransition\0"
    "SetCurrentScene\0AddSceneItem\0OBSSceneItem\0"
    "item\0RemoveSceneItem\0AddScene\0source\0"
    "RemoveScene\0RenameSources\0newName\0"
    "prevName\0SelectSceneItem\0select\0"
    "ActivateAudioSource\0DeactivateAudioSource\0"
    "DuplicateSelectedScene\0RemoveSelectedScene\0"
    "RemoveSelectedSceneItem\0ToggleAlwaysOnTop\0"
    "ReorderSources\0ProcessHotkey\0obs_hotkey_id\0"
    "id\0pressed\0AddTransition\0RenameTransition\0"
    "TransitionClicked\0TransitionStopped\0"
    "TransitionFullyStopped\0TriggerQuickTransition\0"
    "SetDeinterlacingMode\0SetDeinterlacingOrder\0"
    "SetScaleFilter\0IconActivated\0"
    "QSystemTrayIcon::ActivationReason\0"
    "reason\0SetShowing\0showing\0ToggleShowHide\0"
    "HideAudioControl\0UnhideAllAudioControls\0"
    "ToggleHideMixer\0MixerRenameSource\0"
    "on_mixerScrollArea_customContextMenuRequested\0"
    "on_actionCopySource_triggered\0"
    "on_actionPasteRef_triggered\0"
    "on_actionPasteDup_triggered\0"
    "on_actionCopyFilters_triggered\0"
    "on_actionPasteFilters_triggered\0"
    "on_actionFullscreenInterface_triggered\0"
    "on_actionShow_Recordings_triggered\0"
    "on_actionRemux_triggered\0"
    "on_action_Settings_triggered\0"
    "on_actionAdvAudioProperties_triggered\0"
    "on_advAudioProps_clicked\0"
    "on_advAudioProps_destroyed\0"
    "on_actionShowLogs_triggered\0"
    "on_actionUploadCurrentLog_triggered\0"
    "on_actionUploadLastLog_triggered\0"
    "on_actionViewCurrentLog_triggered\0"
    "on_actionCheckForUpdates_triggered\0"
    "on_actionShowCrashLogs_triggered\0"
    "on_actionUploadLastCrashLog_triggered\0"
    "on_actionEditTransform_triggered\0"
    "on_actionCopyTransform_triggered\0"
    "on_actionPasteTransform_triggered\0"
    "on_actionRotate90CW_triggered\0"
    "on_actionRotate90CCW_triggered\0"
    "on_actionRotate180_triggered\0"
    "on_actionFlipHorizontal_triggered\0"
    "on_actionFlipVertical_triggered\0"
    "on_actionFitToScreen_triggered\0"
    "on_actionStretchToScreen_triggered\0"
    "on_actionCenterToScreen_triggered\0"
    "on_scenes_currentItemChanged\0"
    "QListWidgetItem*\0current\0prev\0"
    "on_scenes_customContextMenuRequested\0"
    "pos\0on_actionAddScene_triggered\0"
    "on_actionRemoveScene_triggered\0"
    "on_actionSceneUp_triggered\0"
    "on_actionSceneDown_triggered\0"
    "on_sources_itemSelectionChanged\0"
    "on_sources_customContextMenuRequested\0"
    "on_sources_itemDoubleClicked\0"
    "on_scenes_itemDoubleClicked\0"
    "on_actionAddSource_triggered\0"
    "on_actionRemoveSource_triggered\0"
    "on_actionInteract_triggered\0"
    "on_actionSourceProperties_triggered\0"
    "on_actionSourceUp_triggered\0"
    "on_actionSourceDown_triggered\0"
    "on_actionMoveUp_triggered\0"
    "on_actionMoveDown_triggered\0"
    "on_actionMoveToTop_triggered\0"
    "on_actionMoveToBottom_triggered\0"
    "on_actionLockPreview_triggered\0"
    "on_scalingMenu_aboutToShow\0"
    "on_actionScaleWindow_triggered\0"
    "on_actionScaleCanvas_triggered\0"
    "on_actionScaleOutput_triggered\0"
    "on_streamButton_clicked\0on_recordButton_clicked\0"
    "on_settingsButton_clicked\0"
    "on_actionHelpPortal_triggered\0"
    "on_actionWebsite_triggered\0"
    "on_preview_customContextMenuRequested\0"
    "on_program_customContextMenuRequested\0"
    "on_previewDisabledLabel_customContextMenuRequested\0"
    "on_actionNewSceneCollection_triggered\0"
    "on_actionDupSceneCollection_triggered\0"
    "on_actionRenameSceneCollection_triggered\0"
    "on_actionRemoveSceneCollection_triggered\0"
    "on_actionImportSceneCollection_triggered\0"
    "on_actionExportSceneCollection_triggered\0"
    "on_actionNewProfile_triggered\0"
    "on_actionDupProfile_triggered\0"
    "on_actionRenameProfile_triggered\0"
    "on_actionRemoveProfile_triggered\0"
    "on_actionImportProfile_triggered\0"
    "on_actionExportProfile_triggered\0"
    "on_actionShowSettingsFolder_triggered\0"
    "on_actionShowProfileFolder_triggered\0"
    "on_actionAlwaysOnTop_triggered\0"
    "on_toggleListboxToolbars_toggled\0"
    "visible\0on_toggleStatusBar_toggled\0"
    "on_transitions_currentIndexChanged\0"
    "index\0on_transitionAdd_clicked\0"
    "on_transitionRemove_clicked\0"
    "on_transitionProps_clicked\0"
    "on_modeSwitch_clicked\0on_autoConfigure_triggered\0"
    "on_stats_triggered\0on_resetUI_triggered\0"
    "on_lockUI_toggled\0lock\0logUploadFinished\0"
    "text\0error\0updateCheckFinished\0"
    "AddSourceFromAction\0MoveSceneToTop\0"
    "MoveSceneToBottom\0EditSceneName\0"
    "EditSceneItemName\0SceneNameEdited\0"
    "QWidget*\0editor\0QAbstractItemDelegate::EndEditHint\0"
    "endHint\0SceneItemNameEdited\0"
    "OpenSceneFilters\0OpenFilters\0"
    "EnablePreviewDisplay\0enable\0TogglePreview\0"
    "NudgeUp\0NudgeDown\0NudgeLeft\0NudgeRight\0"
    "OpenStudioProgramProjector\0"
    "OpenPreviewProjector\0OpenSourceProjector\0"
    "OpenMultiviewProjector\0OpenSceneProjector\0"
    "OpenStudioProgramWindow\0OpenPreviewWindow\0"
    "OpenSourceWindow\0OpenMultiviewWindow\0"
    "OpenSceneWindow\0DeferredLoad\0file\0"
    "requeueCount\0on_actionResetTransform_triggered"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_OBSBasic[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
     183,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    0,  929,    2, 0x0a /* Public */,
       3,    0,  930,    2, 0x0a /* Public */,
       4,    0,  931,    2, 0x0a /* Public */,
       5,    0,  932,    2, 0x0a /* Public */,
       6,    0,  933,    2, 0x0a /* Public */,
       7,    1,  934,    2, 0x0a /* Public */,
       9,    1,  937,    2, 0x0a /* Public */,
      10,    0,  940,    2, 0x0a /* Public */,
      11,    0,  941,    2, 0x0a /* Public */,
      12,    2,  942,    2, 0x0a /* Public */,
      15,    0,  947,    2, 0x0a /* Public */,
      16,    0,  948,    2, 0x0a /* Public */,
      17,    0,  949,    2, 0x0a /* Public */,
      18,    0,  950,    2, 0x0a /* Public */,
      19,    1,  951,    2, 0x0a /* Public */,
      21,    0,  954,    2, 0x0a /* Public */,
      22,    0,  955,    2, 0x0a /* Public */,
      23,    0,  956,    2, 0x0a /* Public */,
      24,    0,  957,    2, 0x0a /* Public */,
      25,    0,  958,    2, 0x0a /* Public */,
      26,    1,  959,    2, 0x0a /* Public */,
      27,    0,  962,    2, 0x0a /* Public */,
      28,    0,  963,    2, 0x0a /* Public */,
      29,    1,  964,    2, 0x0a /* Public */,
      32,    3,  967,    2, 0x0a /* Public */,
      32,    2,  974,    2, 0x2a /* Public | MethodCloned */,
      32,    1,  979,    2, 0x2a /* Public | MethodCloned */,
      32,    4,  982,    2, 0x0a /* Public */,
      32,    3,  991,    2, 0x2a /* Public | MethodCloned */,
      32,    2,  998,    2, 0x2a /* Public | MethodCloned */,
      32,    1, 1003,    2, 0x2a /* Public | MethodCloned */,
      38,    3, 1006,    2, 0x0a /* Public */,
      38,    2, 1013,    2, 0x2a /* Public | MethodCloned */,
      38,    1, 1018,    2, 0x2a /* Public | MethodCloned */,
      39,    1, 1021,    2, 0x08 /* Private */,
      42,    1, 1024,    2, 0x08 /* Private */,
      43,    1, 1027,    2, 0x08 /* Private */,
      45,    1, 1030,    2, 0x08 /* Private */,
      46,    3, 1033,    2, 0x08 /* Private */,
      49,    3, 1040,    2, 0x08 /* Private */,
      51,    1, 1047,    2, 0x08 /* Private */,
      52,    1, 1050,    2, 0x08 /* Private */,
      53,    0, 1053,    2, 0x08 /* Private */,
      54,    0, 1054,    2, 0x08 /* Private */,
      55,    0, 1055,    2, 0x08 /* Private */,
      56,    0, 1056,    2, 0x08 /* Private */,
      57,    1, 1057,    2, 0x08 /* Private */,
      58,    2, 1060,    2, 0x08 /* Private */,
      62,    0, 1065,    2, 0x08 /* Private */,
      63,    0, 1066,    2, 0x08 /* Private */,
      64,    0, 1067,    2, 0x08 /* Private */,
      65,    0, 1068,    2, 0x08 /* Private */,
      66,    0, 1069,    2, 0x08 /* Private */,
      67,    1, 1070,    2, 0x08 /* Private */,
      68,    0, 1073,    2, 0x08 /* Private */,
      69,    0, 1074,    2, 0x08 /* Private */,
      70,    0, 1075,    2, 0x08 /* Private */,
      71,    1, 1076,    2, 0x08 /* Private */,
      74,    1, 1079,    2, 0x08 /* Private */,
      76,    0, 1082,    2, 0x08 /* Private */,
      77,    0, 1083,    2, 0x08 /* Private */,
      78,    0, 1084,    2, 0x08 /* Private */,
      79,    0, 1085,    2, 0x08 /* Private */,
      80,    0, 1086,    2, 0x08 /* Private */,
      81,    0, 1087,    2, 0x08 /* Private */,
      82,    0, 1088,    2, 0x08 /* Private */,
      83,    0, 1089,    2, 0x08 /* Private */,
      84,    0, 1090,    2, 0x08 /* Private */,
      85,    0, 1091,    2, 0x08 /* Private */,
      86,    0, 1092,    2, 0x08 /* Private */,
      87,    0, 1093,    2, 0x08 /* Private */,
      88,    0, 1094,    2, 0x08 /* Private */,
      89,    0, 1095,    2, 0x08 /* Private */,
      90,    0, 1096,    2, 0x08 /* Private */,
      91,    0, 1097,    2, 0x08 /* Private */,
      92,    0, 1098,    2, 0x08 /* Private */,
      93,    0, 1099,    2, 0x08 /* Private */,
      94,    0, 1100,    2, 0x08 /* Private */,
      95,    0, 1101,    2, 0x08 /* Private */,
      96,    0, 1102,    2, 0x08 /* Private */,
      97,    0, 1103,    2, 0x08 /* Private */,
      98,    0, 1104,    2, 0x08 /* Private */,
      99,    0, 1105,    2, 0x08 /* Private */,
     100,    0, 1106,    2, 0x08 /* Private */,
     101,    0, 1107,    2, 0x08 /* Private */,
     102,    0, 1108,    2, 0x08 /* Private */,
     103,    0, 1109,    2, 0x08 /* Private */,
     104,    0, 1110,    2, 0x08 /* Private */,
     105,    0, 1111,    2, 0x08 /* Private */,
     106,    0, 1112,    2, 0x08 /* Private */,
     107,    0, 1113,    2, 0x08 /* Private */,
     108,    0, 1114,    2, 0x08 /* Private */,
     109,    0, 1115,    2, 0x08 /* Private */,
     110,    0, 1116,    2, 0x08 /* Private */,
     111,    0, 1117,    2, 0x08 /* Private */,
     112,    2, 1118,    2, 0x08 /* Private */,
     116,    1, 1123,    2, 0x08 /* Private */,
     118,    0, 1126,    2, 0x08 /* Private */,
     119,    0, 1127,    2, 0x08 /* Private */,
     120,    0, 1128,    2, 0x08 /* Private */,
     121,    0, 1129,    2, 0x08 /* Private */,
     122,    0, 1130,    2, 0x08 /* Private */,
     123,    1, 1131,    2, 0x08 /* Private */,
     124,    1, 1134,    2, 0x08 /* Private */,
     125,    1, 1137,    2, 0x08 /* Private */,
     126,    0, 1140,    2, 0x08 /* Private */,
     127,    0, 1141,    2, 0x08 /* Private */,
     128,    0, 1142,    2, 0x08 /* Private */,
     129,    0, 1143,    2, 0x08 /* Private */,
     130,    0, 1144,    2, 0x08 /* Private */,
     131,    0, 1145,    2, 0x08 /* Private */,
     132,    0, 1146,    2, 0x08 /* Private */,
     133,    0, 1147,    2, 0x08 /* Private */,
     134,    0, 1148,    2, 0x08 /* Private */,
     135,    0, 1149,    2, 0x08 /* Private */,
     136,    0, 1150,    2, 0x08 /* Private */,
     137,    0, 1151,    2, 0x08 /* Private */,
     138,    0, 1152,    2, 0x08 /* Private */,
     139,    0, 1153,    2, 0x08 /* Private */,
     140,    0, 1154,    2, 0x08 /* Private */,
     141,    0, 1155,    2, 0x08 /* Private */,
     142,    0, 1156,    2, 0x08 /* Private */,
     143,    0, 1157,    2, 0x08 /* Private */,
     144,    0, 1158,    2, 0x08 /* Private */,
     145,    0, 1159,    2, 0x08 /* Private */,
     146,    1, 1160,    2, 0x08 /* Private */,
     147,    1, 1163,    2, 0x08 /* Private */,
     148,    1, 1166,    2, 0x08 /* Private */,
     149,    0, 1169,    2, 0x08 /* Private */,
     150,    0, 1170,    2, 0x08 /* Private */,
     151,    0, 1171,    2, 0x08 /* Private */,
     152,    0, 1172,    2, 0x08 /* Private */,
     153,    0, 1173,    2, 0x08 /* Private */,
     154,    0, 1174,    2, 0x08 /* Private */,
     155,    0, 1175,    2, 0x08 /* Private */,
     156,    0, 1176,    2, 0x08 /* Private */,
     157,    0, 1177,    2, 0x08 /* Private */,
     158,    0, 1178,    2, 0x08 /* Private */,
     159,    0, 1179,    2, 0x08 /* Private */,
     160,    0, 1180,    2, 0x08 /* Private */,
     161,    0, 1181,    2, 0x08 /* Private */,
     162,    0, 1182,    2, 0x08 /* Private */,
     163,    0, 1183,    2, 0x08 /* Private */,
     164,    1, 1184,    2, 0x08 /* Private */,
     166,    1, 1187,    2, 0x08 /* Private */,
     167,    1, 1190,    2, 0x08 /* Private */,
     169,    0, 1193,    2, 0x08 /* Private */,
     170,    0, 1194,    2, 0x08 /* Private */,
     171,    0, 1195,    2, 0x08 /* Private */,
     172,    0, 1196,    2, 0x08 /* Private */,
     173,    0, 1197,    2, 0x08 /* Private */,
     174,    0, 1198,    2, 0x08 /* Private */,
     175,    0, 1199,    2, 0x08 /* Private */,
     176,    1, 1200,    2, 0x08 /* Private */,
     178,    2, 1203,    2, 0x08 /* Private */,
     181,    0, 1208,    2, 0x08 /* Private */,
     182,    0, 1209,    2, 0x08 /* Private */,
     183,    0, 1210,    2, 0x08 /* Private */,
     184,    0, 1211,    2, 0x08 /* Private */,
     185,    0, 1212,    2, 0x08 /* Private */,
     186,    0, 1213,    2, 0x08 /* Private */,
     187,    2, 1214,    2, 0x08 /* Private */,
     192,    2, 1219,    2, 0x08 /* Private */,
     193,    0, 1224,    2, 0x08 /* Private */,
     194,    0, 1225,    2, 0x08 /* Private */,
     195,    1, 1226,    2, 0x08 /* Private */,
     197,    0, 1229,    2, 0x08 /* Private */,
     198,    0, 1230,    2, 0x08 /* Private */,
     199,    0, 1231,    2, 0x08 /* Private */,
     200,    0, 1232,    2, 0x08 /* Private */,
     201,    0, 1233,    2, 0x08 /* Private */,
     202,    0, 1234,    2, 0x08 /* Private */,
     203,    0, 1235,    2, 0x08 /* Private */,
     204,    0, 1236,    2, 0x08 /* Private */,
     205,    0, 1237,    2, 0x08 /* Private */,
     206,    0, 1238,    2, 0x08 /* Private */,
     207,    0, 1239,    2, 0x08 /* Private */,
     208,    0, 1240,    2, 0x08 /* Private */,
     209,    0, 1241,    2, 0x08 /* Private */,
     210,    0, 1242,    2, 0x08 /* Private */,
     211,    0, 1243,    2, 0x08 /* Private */,
     212,    2, 1244,    2, 0x08 /* Private */,
     215,    0, 1249,    2, 0x0a /* Public */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,    8,
    QMetaType::Void, QMetaType::Int,    8,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::QString,   13,   14,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,   20,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,   20,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 30,   31,
    QMetaType::Void, 0x80000000 | 33, QMetaType::Bool, QMetaType::Bool,   34,   35,   36,
    QMetaType::Void, 0x80000000 | 33, QMetaType::Bool,   34,   35,
    QMetaType::Void, 0x80000000 | 33,   34,
    QMetaType::Void, 0x80000000 | 30, QMetaType::Bool, QMetaType::Bool, QMetaType::Bool,   34,   35,   36,   37,
    QMetaType::Void, 0x80000000 | 30, QMetaType::Bool, QMetaType::Bool,   34,   35,   36,
    QMetaType::Void, 0x80000000 | 30, QMetaType::Bool,   34,   35,
    QMetaType::Void, 0x80000000 | 30,   34,
    QMetaType::Void, 0x80000000 | 30, QMetaType::Bool, QMetaType::Bool,   34,   35,   36,
    QMetaType::Void, 0x80000000 | 30, QMetaType::Bool,   34,   35,
    QMetaType::Void, 0x80000000 | 30,   34,
    QMetaType::Void, 0x80000000 | 40,   41,
    QMetaType::Void, 0x80000000 | 40,   41,
    QMetaType::Void, 0x80000000 | 30,   44,
    QMetaType::Void, 0x80000000 | 30,   44,
    QMetaType::Void, 0x80000000 | 30, QMetaType::QString, QMetaType::QString,   44,   47,   48,
    QMetaType::Void, 0x80000000 | 33, 0x80000000 | 40, QMetaType::Bool,   34,   41,   50,
    QMetaType::Void, 0x80000000 | 30,   44,
    QMetaType::Void, 0x80000000 | 30,   44,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 33,   34,
    QMetaType::Void, 0x80000000 | 59, QMetaType::Bool,   60,   61,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,   60,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 72,   73,
    QMetaType::Void, QMetaType::Bool,   75,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 113, 0x80000000 | 113,  114,  115,
    QMetaType::Void, QMetaType::QPoint,  117,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QPoint,  117,
    QMetaType::Void, 0x80000000 | 113,   41,
    QMetaType::Void, 0x80000000 | 113,   41,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QPoint,  117,
    QMetaType::Void, QMetaType::QPoint,  117,
    QMetaType::Void, QMetaType::QPoint,  117,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,  165,
    QMetaType::Void, QMetaType::Bool,  165,
    QMetaType::Void, QMetaType::Int,  168,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,  177,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,  179,  180,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 188, 0x80000000 | 190,  189,  191,
    QMetaType::Void, 0x80000000 | 188, 0x80000000 | 190,  189,  191,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,  196,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString, QMetaType::Int,  213,  214,
    QMetaType::Void,

       0        // eod
};

void OBSBasic::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        OBSBasic *_t = static_cast<OBSBasic *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->DeferSaveBegin(); break;
        case 1: _t->DeferSaveEnd(); break;
        case 2: _t->StartStreaming(); break;
        case 3: _t->StopStreaming(); break;
        case 4: _t->ForceStopStreaming(); break;
        case 5: _t->StreamDelayStarting((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 6: _t->StreamDelayStopping((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 7: _t->StreamingStart(); break;
        case 8: _t->StreamStopping(); break;
        case 9: _t->StreamingStop((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< QString(*)>(_a[2]))); break;
        case 10: _t->StartRecording(); break;
        case 11: _t->StopRecording(); break;
        case 12: _t->RecordingStart(); break;
        case 13: _t->RecordStopping(); break;
        case 14: _t->RecordingStop((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 15: _t->StartReplayBuffer(); break;
        case 16: _t->StopReplayBuffer(); break;
        case 17: _t->ReplayBufferStart(); break;
        case 18: _t->ReplayBufferSave(); break;
        case 19: _t->ReplayBufferStopping(); break;
        case 20: _t->ReplayBufferStop((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 21: _t->SaveProjectDeferred(); break;
        case 22: _t->SaveProject(); break;
        case 23: _t->SetTransition((*reinterpret_cast< OBSSource(*)>(_a[1]))); break;
        case 24: _t->TransitionToScene((*reinterpret_cast< OBSScene(*)>(_a[1])),(*reinterpret_cast< bool(*)>(_a[2])),(*reinterpret_cast< bool(*)>(_a[3]))); break;
        case 25: _t->TransitionToScene((*reinterpret_cast< OBSScene(*)>(_a[1])),(*reinterpret_cast< bool(*)>(_a[2]))); break;
        case 26: _t->TransitionToScene((*reinterpret_cast< OBSScene(*)>(_a[1]))); break;
        case 27: _t->TransitionToScene((*reinterpret_cast< OBSSource(*)>(_a[1])),(*reinterpret_cast< bool(*)>(_a[2])),(*reinterpret_cast< bool(*)>(_a[3])),(*reinterpret_cast< bool(*)>(_a[4]))); break;
        case 28: _t->TransitionToScene((*reinterpret_cast< OBSSource(*)>(_a[1])),(*reinterpret_cast< bool(*)>(_a[2])),(*reinterpret_cast< bool(*)>(_a[3]))); break;
        case 29: _t->TransitionToScene((*reinterpret_cast< OBSSource(*)>(_a[1])),(*reinterpret_cast< bool(*)>(_a[2]))); break;
        case 30: _t->TransitionToScene((*reinterpret_cast< OBSSource(*)>(_a[1]))); break;
        case 31: _t->SetCurrentScene((*reinterpret_cast< OBSSource(*)>(_a[1])),(*reinterpret_cast< bool(*)>(_a[2])),(*reinterpret_cast< bool(*)>(_a[3]))); break;
        case 32: _t->SetCurrentScene((*reinterpret_cast< OBSSource(*)>(_a[1])),(*reinterpret_cast< bool(*)>(_a[2]))); break;
        case 33: _t->SetCurrentScene((*reinterpret_cast< OBSSource(*)>(_a[1]))); break;
        case 34: _t->AddSceneItem((*reinterpret_cast< OBSSceneItem(*)>(_a[1]))); break;
        case 35: _t->RemoveSceneItem((*reinterpret_cast< OBSSceneItem(*)>(_a[1]))); break;
        case 36: _t->AddScene((*reinterpret_cast< OBSSource(*)>(_a[1]))); break;
        case 37: _t->RemoveScene((*reinterpret_cast< OBSSource(*)>(_a[1]))); break;
        case 38: _t->RenameSources((*reinterpret_cast< OBSSource(*)>(_a[1])),(*reinterpret_cast< QString(*)>(_a[2])),(*reinterpret_cast< QString(*)>(_a[3]))); break;
        case 39: _t->SelectSceneItem((*reinterpret_cast< OBSScene(*)>(_a[1])),(*reinterpret_cast< OBSSceneItem(*)>(_a[2])),(*reinterpret_cast< bool(*)>(_a[3]))); break;
        case 40: _t->ActivateAudioSource((*reinterpret_cast< OBSSource(*)>(_a[1]))); break;
        case 41: _t->DeactivateAudioSource((*reinterpret_cast< OBSSource(*)>(_a[1]))); break;
        case 42: _t->DuplicateSelectedScene(); break;
        case 43: _t->RemoveSelectedScene(); break;
        case 44: _t->RemoveSelectedSceneItem(); break;
        case 45: _t->ToggleAlwaysOnTop(); break;
        case 46: _t->ReorderSources((*reinterpret_cast< OBSScene(*)>(_a[1]))); break;
        case 47: _t->ProcessHotkey((*reinterpret_cast< obs_hotkey_id(*)>(_a[1])),(*reinterpret_cast< bool(*)>(_a[2]))); break;
        case 48: _t->AddTransition(); break;
        case 49: _t->RenameTransition(); break;
        case 50: _t->TransitionClicked(); break;
        case 51: _t->TransitionStopped(); break;
        case 52: _t->TransitionFullyStopped(); break;
        case 53: _t->TriggerQuickTransition((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 54: _t->SetDeinterlacingMode(); break;
        case 55: _t->SetDeinterlacingOrder(); break;
        case 56: _t->SetScaleFilter(); break;
        case 57: _t->IconActivated((*reinterpret_cast< QSystemTrayIcon::ActivationReason(*)>(_a[1]))); break;
        case 58: _t->SetShowing((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 59: _t->ToggleShowHide(); break;
        case 60: _t->HideAudioControl(); break;
        case 61: _t->UnhideAllAudioControls(); break;
        case 62: _t->ToggleHideMixer(); break;
        case 63: _t->MixerRenameSource(); break;
        case 64: _t->on_mixerScrollArea_customContextMenuRequested(); break;
        case 65: _t->on_actionCopySource_triggered(); break;
        case 66: _t->on_actionPasteRef_triggered(); break;
        case 67: _t->on_actionPasteDup_triggered(); break;
        case 68: _t->on_actionCopyFilters_triggered(); break;
        case 69: _t->on_actionPasteFilters_triggered(); break;
        case 70: _t->on_actionFullscreenInterface_triggered(); break;
        case 71: _t->on_actionShow_Recordings_triggered(); break;
        case 72: _t->on_actionRemux_triggered(); break;
        case 73: _t->on_action_Settings_triggered(); break;
        case 74: _t->on_actionAdvAudioProperties_triggered(); break;
        case 75: _t->on_advAudioProps_clicked(); break;
        case 76: _t->on_advAudioProps_destroyed(); break;
        case 77: _t->on_actionShowLogs_triggered(); break;
        case 78: _t->on_actionUploadCurrentLog_triggered(); break;
        case 79: _t->on_actionUploadLastLog_triggered(); break;
        case 80: _t->on_actionViewCurrentLog_triggered(); break;
        case 81: _t->on_actionCheckForUpdates_triggered(); break;
        case 82: _t->on_actionShowCrashLogs_triggered(); break;
        case 83: _t->on_actionUploadLastCrashLog_triggered(); break;
        case 84: _t->on_actionEditTransform_triggered(); break;
        case 85: _t->on_actionCopyTransform_triggered(); break;
        case 86: _t->on_actionPasteTransform_triggered(); break;
        case 87: _t->on_actionRotate90CW_triggered(); break;
        case 88: _t->on_actionRotate90CCW_triggered(); break;
        case 89: _t->on_actionRotate180_triggered(); break;
        case 90: _t->on_actionFlipHorizontal_triggered(); break;
        case 91: _t->on_actionFlipVertical_triggered(); break;
        case 92: _t->on_actionFitToScreen_triggered(); break;
        case 93: _t->on_actionStretchToScreen_triggered(); break;
        case 94: _t->on_actionCenterToScreen_triggered(); break;
        case 95: _t->on_scenes_currentItemChanged((*reinterpret_cast< QListWidgetItem*(*)>(_a[1])),(*reinterpret_cast< QListWidgetItem*(*)>(_a[2]))); break;
        case 96: _t->on_scenes_customContextMenuRequested((*reinterpret_cast< const QPoint(*)>(_a[1]))); break;
        case 97: _t->on_actionAddScene_triggered(); break;
        case 98: _t->on_actionRemoveScene_triggered(); break;
        case 99: _t->on_actionSceneUp_triggered(); break;
        case 100: _t->on_actionSceneDown_triggered(); break;
        case 101: _t->on_sources_itemSelectionChanged(); break;
        case 102: _t->on_sources_customContextMenuRequested((*reinterpret_cast< const QPoint(*)>(_a[1]))); break;
        case 103: _t->on_sources_itemDoubleClicked((*reinterpret_cast< QListWidgetItem*(*)>(_a[1]))); break;
        case 104: _t->on_scenes_itemDoubleClicked((*reinterpret_cast< QListWidgetItem*(*)>(_a[1]))); break;
        case 105: _t->on_actionAddSource_triggered(); break;
        case 106: _t->on_actionRemoveSource_triggered(); break;
        case 107: _t->on_actionInteract_triggered(); break;
        case 108: _t->on_actionSourceProperties_triggered(); break;
        case 109: _t->on_actionSourceUp_triggered(); break;
        case 110: _t->on_actionSourceDown_triggered(); break;
        case 111: _t->on_actionMoveUp_triggered(); break;
        case 112: _t->on_actionMoveDown_triggered(); break;
        case 113: _t->on_actionMoveToTop_triggered(); break;
        case 114: _t->on_actionMoveToBottom_triggered(); break;
        case 115: _t->on_actionLockPreview_triggered(); break;
        case 116: _t->on_scalingMenu_aboutToShow(); break;
        case 117: _t->on_actionScaleWindow_triggered(); break;
        case 118: _t->on_actionScaleCanvas_triggered(); break;
        case 119: _t->on_actionScaleOutput_triggered(); break;
        case 120: _t->on_streamButton_clicked(); break;
        case 121: _t->on_recordButton_clicked(); break;
        case 122: _t->on_settingsButton_clicked(); break;
        case 123: _t->on_actionHelpPortal_triggered(); break;
        case 124: _t->on_actionWebsite_triggered(); break;
        case 125: _t->on_preview_customContextMenuRequested((*reinterpret_cast< const QPoint(*)>(_a[1]))); break;
        case 126: _t->on_program_customContextMenuRequested((*reinterpret_cast< const QPoint(*)>(_a[1]))); break;
        case 127: _t->on_previewDisabledLabel_customContextMenuRequested((*reinterpret_cast< const QPoint(*)>(_a[1]))); break;
        case 128: _t->on_actionNewSceneCollection_triggered(); break;
        case 129: _t->on_actionDupSceneCollection_triggered(); break;
        case 130: _t->on_actionRenameSceneCollection_triggered(); break;
        case 131: _t->on_actionRemoveSceneCollection_triggered(); break;
        case 132: _t->on_actionImportSceneCollection_triggered(); break;
        case 133: _t->on_actionExportSceneCollection_triggered(); break;
        case 134: _t->on_actionNewProfile_triggered(); break;
        case 135: _t->on_actionDupProfile_triggered(); break;
        case 136: _t->on_actionRenameProfile_triggered(); break;
        case 137: _t->on_actionRemoveProfile_triggered(); break;
        case 138: _t->on_actionImportProfile_triggered(); break;
        case 139: _t->on_actionExportProfile_triggered(); break;
        case 140: _t->on_actionShowSettingsFolder_triggered(); break;
        case 141: _t->on_actionShowProfileFolder_triggered(); break;
        case 142: _t->on_actionAlwaysOnTop_triggered(); break;
        case 143: _t->on_toggleListboxToolbars_toggled((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 144: _t->on_toggleStatusBar_toggled((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 145: _t->on_transitions_currentIndexChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 146: _t->on_transitionAdd_clicked(); break;
        case 147: _t->on_transitionRemove_clicked(); break;
        case 148: _t->on_transitionProps_clicked(); break;
        case 149: _t->on_modeSwitch_clicked(); break;
        case 150: _t->on_autoConfigure_triggered(); break;
        case 151: _t->on_stats_triggered(); break;
        case 152: _t->on_resetUI_triggered(); break;
        case 153: _t->on_lockUI_toggled((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 154: _t->logUploadFinished((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2]))); break;
        case 155: _t->updateCheckFinished(); break;
        case 156: _t->AddSourceFromAction(); break;
        case 157: _t->MoveSceneToTop(); break;
        case 158: _t->MoveSceneToBottom(); break;
        case 159: _t->EditSceneName(); break;
        case 160: _t->EditSceneItemName(); break;
        case 161: _t->SceneNameEdited((*reinterpret_cast< QWidget*(*)>(_a[1])),(*reinterpret_cast< QAbstractItemDelegate::EndEditHint(*)>(_a[2]))); break;
        case 162: _t->SceneItemNameEdited((*reinterpret_cast< QWidget*(*)>(_a[1])),(*reinterpret_cast< QAbstractItemDelegate::EndEditHint(*)>(_a[2]))); break;
        case 163: _t->OpenSceneFilters(); break;
        case 164: _t->OpenFilters(); break;
        case 165: _t->EnablePreviewDisplay((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 166: _t->TogglePreview(); break;
        case 167: _t->NudgeUp(); break;
        case 168: _t->NudgeDown(); break;
        case 169: _t->NudgeLeft(); break;
        case 170: _t->NudgeRight(); break;
        case 171: _t->OpenStudioProgramProjector(); break;
        case 172: _t->OpenPreviewProjector(); break;
        case 173: _t->OpenSourceProjector(); break;
        case 174: _t->OpenMultiviewProjector(); break;
        case 175: _t->OpenSceneProjector(); break;
        case 176: _t->OpenStudioProgramWindow(); break;
        case 177: _t->OpenPreviewWindow(); break;
        case 178: _t->OpenSourceWindow(); break;
        case 179: _t->OpenMultiviewWindow(); break;
        case 180: _t->OpenSceneWindow(); break;
        case 181: _t->DeferredLoad((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 182: _t->on_actionResetTransform_triggered(); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<int*>(_a[0]) = -1; break;
        case 161:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QWidget* >(); break;
            }
            break;
        case 162:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QWidget* >(); break;
            }
            break;
        }
    }
}

const QMetaObject OBSBasic::staticMetaObject = {
    { &OBSMainWindow::staticMetaObject, qt_meta_stringdata_OBSBasic.data,
      qt_meta_data_OBSBasic,  qt_static_metacall, Q_NULLPTR, Q_NULLPTR}
};


const QMetaObject *OBSBasic::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *OBSBasic::qt_metacast(const char *_clname)
{
    if (!_clname) return Q_NULLPTR;
    if (!strcmp(_clname, qt_meta_stringdata_OBSBasic.stringdata0))
        return static_cast<void*>(const_cast< OBSBasic*>(this));
    return OBSMainWindow::qt_metacast(_clname);
}

int OBSBasic::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = OBSMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 183)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 183;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 183)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 183;
    }
    return _id;
}
QT_END_MOC_NAMESPACE
