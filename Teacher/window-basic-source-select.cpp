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

#include <QMessageBox>
#include "window-basic-main.hpp"
#include "window-basic-source-select.hpp"
#include "qt-wrappers.hpp"
#include "obs-app.hpp"

struct AddSourceData {
	obs_source_t *source;
	bool visible;
};

bool OBSBasicSourceSelect::EnumSources(void *data, obs_source_t *source)
{
	OBSBasicSourceSelect *window = static_cast<OBSBasicSourceSelect*>(data);
	const char *name = obs_source_get_name(source);
	const char *id   = obs_source_get_id(source);

	if (strcmp(id, window->id) == 0)
		window->ui->sourceList->addItem(QT_UTF8(name));

	return true;
}

void OBSBasicSourceSelect::OBSSourceAdded(void *data, calldata_t *calldata)
{
	OBSBasicSourceSelect *window = static_cast<OBSBasicSourceSelect*>(data);
	obs_source_t *source = (obs_source_t*)calldata_ptr(calldata, "source");

	QMetaObject::invokeMethod(window, "SourceAdded",
			Q_ARG(OBSSource, source));
}

void OBSBasicSourceSelect::OBSSourceRemoved(void *data, calldata_t *calldata)
{
	OBSBasicSourceSelect *window = static_cast<OBSBasicSourceSelect*>(data);
	obs_source_t *source = (obs_source_t*)calldata_ptr(calldata, "source");

	QMetaObject::invokeMethod(window, "SourceRemoved",
			Q_ARG(OBSSource, source));
}

void OBSBasicSourceSelect::SourceAdded(OBSSource source)
{
	const char *name     = obs_source_get_name(source);
	const char *sourceId = obs_source_get_id(source);

	if (strcmp(sourceId, id) != 0)
		return;

	ui->sourceList->addItem(name);
}

void OBSBasicSourceSelect::SourceRemoved(OBSSource source)
{
	const char *name     = obs_source_get_name(source);
	const char *sourceId = obs_source_get_id(source);

	if (strcmp(sourceId, id) != 0)
		return;

	QList<QListWidgetItem*> items =
		ui->sourceList->findItems(name, Qt::MatchFixedString);

	if (!items.count())
		return;

	delete items[0];
}

static void AddSource(void *_data, obs_scene_t *scene)
{
	AddSourceData *data = (AddSourceData *)_data;
	obs_sceneitem_t *sceneitem;

	sceneitem = obs_scene_add(scene, data->source);
	obs_sceneitem_set_visible(sceneitem, data->visible);

	// 调用主窗口接口 => 对场景资源进行位置重排，两行（1行1列，1行5列）
	OBSBasic *main = reinterpret_cast<OBSBasic*>(App()->GetMainWindow());
	main->doSceneItemLayout(sceneitem);
}

static char *get_new_source_name(const char *name)
{
	struct dstr new_name = {0};
	int inc = 0;

	dstr_copy(&new_name, name);

	for (;;) {
		obs_source_t *existing_source = obs_get_source_by_name(
				new_name.array);
		if (!existing_source)
			break;

		obs_source_release(existing_source);

		dstr_printf(&new_name, "%s %d", name, ++inc + 1);
	}

	return new_name.array;
}

static void AddExisting(const char *name, bool visible, bool duplicate)
{
	OBSBasic *main = reinterpret_cast<OBSBasic*>(App()->GetMainWindow());
	OBSScene scene = main->GetCurrentScene();
	if (!scene)
		return;

	obs_source_t *source = obs_get_source_by_name(name);
	if (source) {
		if (duplicate) {
			obs_source_t *from = source;
			char *new_name = get_new_source_name(name);
			source = obs_source_duplicate(from, new_name, false);
			bfree(new_name);
			obs_source_release(from);

			if (!source)
				return;
		}

		AddSourceData data;
		data.source = source;
		data.visible = visible;

		obs_enter_graphics();
		obs_scene_atomic_update(scene, AddSource, &data);
		obs_leave_graphics();

		obs_source_release(source);
	}
}

bool AddNew(QWidget *parent, const char *id, const char *name,
		const bool visible, OBSSource &newSource)
{
	OBSBasic     *main = reinterpret_cast<OBSBasic*>(App()->GetMainWindow());
	OBSScene     scene = main->GetCurrentScene();
	bool         success = false;
	if (!scene)
		return false;

	obs_source_t *source = obs_get_source_by_name(name);
	if (source) {
		OBSMessageBox::information(parent,
				QTStr("NameExists.Title"),
				QTStr("NameExists.Text"));

	} else {
		source = obs_source_create(id, name, NULL, nullptr);

		if (source) {
			AddSourceData data;
			data.source = source;
			data.visible = visible;

			obs_enter_graphics();
			obs_scene_atomic_update(scene, AddSource, &data);
			obs_leave_graphics();

			newSource = source;

			success = true;
		}
	}

	obs_source_release(source);
	return success;
}

void OBSBasicSourceSelect::on_buttonBox_accepted()
{
	bool useExisting = ui->selectExisting->isChecked();
	bool visible = ui->sourceVisible->isChecked();

	// 当前新添加的资源是否是 => rtp_source => 互动教室...
	bool bIsNewRtpSource = ((astrcmpi(id, "rtp_source") == 0) ? true : false);
	// 如果已经有了rtp_source资源，就不能再添加新的rtp_source资源了...
	if (m_bHasRtpSource && bIsNewRtpSource) {
		OBSMessageBox::information(this, 
			QTStr("SingleRtpSource.Title"), 
			QTStr("SingleRtpSource.Text"));
		return;
	}

	if (useExisting) {
		QListWidgetItem *item = ui->sourceList->currentItem();
		if (!item)
			return;

		AddExisting(QT_TO_UTF8(item->text()), visible, false);
	} else {
		if (ui->sourceName->text().isEmpty()) {
			OBSMessageBox::information(this,
					QTStr("NoNameEntered.Title"),
					QTStr("NoNameEntered.Text"));
			return;
		}
		// 如果添加新的场景资源失败，直接返回...
		if (!AddNew(this, id, QT_TO_UTF8(ui->sourceName->text()), visible, newSource))
			return;
		// 如果新添加资源是互动教室 => 需要屏蔽音频输出，开启本地监视...
		if (bIsNewRtpSource) {
			obs_source_set_monitoring_type(newSource, OBS_MONITORING_TYPE_MONITOR_ONLY);
			blog(LOG_INFO, "User changed audio monitoring for source '%s' to: %s", obs_source_get_name(newSource), "monitor only");
		}
		// 给单音频资源添加噪音抑制过滤器，互动教室也要加上音频过滤器...
		AddNoiseFilterForAudioSource(newSource, bIsNewRtpSource);
	}

	done(DialogCode::Accepted);
}

void OBSBasicSourceSelect::AddNoiseFilterForAudioSource(obs_source_t *source, bool bIsRtpSource)
{
	// 如果输入资源不是互动教室，并且包含视频，不是单独音频，直接返回...
	uint32_t flags = obs_source_get_output_flags(source);
	if (!bIsRtpSource && (flags & OBS_SOURCE_VIDEO) != 0)
		return;
	// 如果输入资源没有音频数据，直接返回...
	if ((flags & OBS_SOURCE_AUDIO) == 0)
		return;
	// 目前只添加噪音抑制过滤器，不添加噪音阈值过滤器...
	//AddFilterToSourceByID(source, "noise_gate_filter");
	AddFilterToSourceByID(source, "noise_suppress_filter");
}

void OBSBasicSourceSelect::AddFilterToSourceByID(obs_source_t *source, const char * lpFilterID)
{
	// 通过id名称查找过滤器资源名称...
	string strFilterName = obs_source_get_display_name(lpFilterID);
	obs_source_t * existing_filter = obs_source_get_filter_by_name(source, strFilterName.c_str());
	// 如果资源上已经挂载了相同名称的过滤器，直接返回...
	if (existing_filter != nullptr)
		return;
	// 创建一个新的过滤器资源对象，创建失败，直接返回...
	obs_source_t * new_filter = obs_source_create(lpFilterID, strFilterName.c_str(), nullptr, nullptr);
	if (new_filter == nullptr)
		return;
	// 获取资源名称，打印信息，挂载过滤器到当前资源，释放过滤器的引用计数...
	const char *sourceName = obs_source_get_name(source);
	blog(LOG_INFO, "User added filter '%s' (%s) to source '%s'", strFilterName.c_str(), lpFilterID, sourceName);
	obs_source_filter_add(source, new_filter);
	obs_source_release(new_filter);
}

void OBSBasicSourceSelect::on_buttonBox_rejected()
{
	done(DialogCode::Rejected);
}

static inline const char *GetSourceDisplayName(const char *id)
{
	if (strcmp(id, "scene") == 0)
		return Str("Basic.Scene");
	return obs_source_get_display_name(id);
}

Q_DECLARE_METATYPE(OBSScene);

template <typename T>
static inline T GetOBSRef(QListWidgetItem *item)
{
	return item->data(static_cast<int>(QtDataRole::OBSRef)).value<T>();
}

OBSBasicSourceSelect::OBSBasicSourceSelect(OBSBasic *parent, const char *id_)
	: QDialog (parent),
	  ui      (new Ui::OBSBasicSourceSelect),
	  id      (id_)
{
	m_bHasRtpSource = false;

	ui->setupUi(this);

	ui->sourceList->setAttribute(Qt::WA_MacShowFocusRect, false);

	QString placeHolderText{QT_UTF8(GetSourceDisplayName(id))};

	QString text{placeHolderText};
	int i = 2;
	obs_source_t *source = nullptr;
	while ((source = obs_get_source_by_name(QT_TO_UTF8(text)))) {
		obs_source_release(source);
		text = QString("%1 %2").arg(placeHolderText).arg(i++);
	}

	ui->sourceName->setText(text);
	ui->sourceName->setFocus();	//Fixes deselect of text.
	ui->sourceName->selectAll();

	installEventFilter(CreateShortcutFilter());

	if (strcmp(id_, "scene") == 0) {
		OBSBasic *main = reinterpret_cast<OBSBasic*>(App()->GetMainWindow());
		OBSSource curSceneSource = main->GetCurrentSceneSource();

		ui->selectExisting->setChecked(true);
		ui->createNew->setChecked(false);
		ui->createNew->setEnabled(false);
		ui->sourceName->setEnabled(false);

		int count = main->ui->scenes->count();
		for (int i = 0; i < count; i++) {
			QListWidgetItem *item = main->ui->scenes->item(i);
			OBSScene scene = GetOBSRef<OBSScene>(item);
			OBSSource sceneSource = obs_scene_get_source(scene);

			if (curSceneSource == sceneSource)
				continue;

			const char *name = obs_source_get_name(sceneSource);
			ui->sourceList->addItem(QT_UTF8(name));
		}
	} else {
		obs_enum_sources(EnumSources, this);

		OBSBasic *main = reinterpret_cast<OBSBasic*>(App()->GetMainWindow());
		for (int i = 0; i < main->ui->sources->count(); i++) {
			QListWidgetItem * listItem = main->ui->sources->item(i);
			OBSSceneItem theItem = main->GetSceneItem(listItem);
			OBSSource theSource = obs_sceneitem_get_source(theItem);
			const char * lpID = obs_source_get_id(theSource);
			// 如果资源中包含有rtp_source资源 => 设置标志...
			if (astrcmpi(lpID, "rtp_source") == 0) {
				this->m_bHasRtpSource = true;
				break;
			}
		}
	}
}

void OBSBasicSourceSelect::SourcePaste(const char *name, bool visible, bool dup)
{
	AddExisting(name, visible, dup);
}
