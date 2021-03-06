
#include <QGuiApplication>
#include <QPushButton>
#include <QMouseEvent>
#include <QScreen>
#include <QBitmap>
#include <QVariant>

#include <algorithm>
#include <cmath>
#include <graphics/vec4.h>
#include <graphics/matrix4.h>
#include "window-basic-preview.hpp"
#include "window-basic-main.hpp"
#include "display-helpers.hpp"
#include "obs-app.hpp"

#define HANDLE_RADIUS     4.0f
#define HANDLE_SEL_RADIUS (HANDLE_RADIUS * 1.5f)

using namespace std;

OBSBasicPreview::OBSBasicPreview(QWidget *parent, Qt::WindowFlags flags)
  : OBSQTDisplay(parent, flags)
{
	ResetScrollingOffset();
	setMouseTracking(true);

	m_btnLeft = this->CreateBtnPage(true);
	m_btnRight = this->CreateBtnPage(false);

	m_btnPrev = this->CreateBtnPPT(true);
	m_btnNext = this->CreateBtnPPT(false);
	m_btnFoot = this->CreateBtnFoot();
}

OBSBasicPreview::~OBSBasicPreview()
{
	if (m_btnRight != NULL) { delete m_btnRight; m_btnRight = NULL; }
	if (m_btnLeft != NULL) { delete m_btnLeft; m_btnLeft = NULL; }
	if (m_btnPrev != NULL) { delete m_btnPrev; m_btnPrev = NULL; }
	if (m_btnNext != NULL) { delete m_btnNext; m_btnNext = NULL; }
	if (m_btnFoot != NULL) { delete m_btnFoot; m_btnFoot = NULL; }

	GM_MapBtnMic::iterator itorItem;
	for (itorItem = m_MapBtnMic.begin(); itorItem != m_MapBtnMic.end(); ++itorItem) {
		delete itorItem->second;
	}
	m_MapBtnMic.clear();
}

void OBSBasicPreview::BindBtnClickEvent()
{
	OBSBasic *main = reinterpret_cast<OBSBasic*>(App()->GetMainWindow());
	this->connect(m_btnLeft, SIGNAL(clicked()), main, SLOT(onPageLeftClicked()));
	this->connect(m_btnRight, SIGNAL(clicked()), main, SLOT(onPageRightClicked()));
	this->connect(m_btnPrev, SIGNAL(clicked()), main, SLOT(onPagePrevClicked()));
	this->connect(m_btnNext, SIGNAL(clicked()), main, SLOT(onPageNextClicked()));
}

void OBSBasicPreview::doDeleteStudentBtnMic(obs_sceneitem_t * lpSceneItem)
{
	// 通过数据源对象找到对应的麦克风按钮...
	GM_MapBtnMic::iterator itorItem = m_MapBtnMic.find(lpSceneItem);
	if (itorItem == m_MapBtnMic.end())
		return;
	// 获取当前学生端数据源的摄像头编号配置信息...
	OBSBasic * main = reinterpret_cast<OBSBasic*>(App()->GetMainWindow());
	obs_source_t * lpSourceItem = obs_sceneitem_get_source(lpSceneItem);
	obs_data_t * lpSettings = obs_source_get_settings(lpSourceItem);
	int nDBCameraID = obs_data_get_int(lpSettings, "camera_id");
	obs_data_release(lpSettings);
	// 如果摄像头编号有效，并且与当前正在监听的第三方编号一致，需要归零处理...
	if (nDBCameraID > 0 && main->m_nDBCameraPusherID == nDBCameraID) {
		main->doSendCameraPusherID(0);
	}
	// 删除麦克风按钮，删除索引...
	delete itorItem->second;
	m_MapBtnMic.erase(itorItem);
}

// 通过输入的数据源对象，创建麦克风按钮对象 => 简化外部操作，在这里进行有效性判断...
void OBSBasicPreview::doBuildStudentBtnMic(obs_sceneitem_t * lpSceneItem)
{
	obs_source_t * lpNewSource = obs_sceneitem_get_source(lpSceneItem);
	uint32_t flags = obs_source_get_output_flags(lpNewSource);
	const char * lpSrcID = obs_source_get_id(lpNewSource);
	if (lpSceneItem == NULL || lpNewSource == NULL)
		return;
	// 如果没有视频数据源，直接返回...
	if ((flags & OBS_SOURCE_VIDEO) == 0)
		return;
	// 如果不是互动学生端，直接返回...
	if (astrcmpi(lpSrcID, App()->InteractRtpSource()) != 0)
		return;
	// 遍历已经存在的麦克风按钮集合...
	GM_MapBtnMic::iterator itorItem;
	for (itorItem = m_MapBtnMic.begin(); itorItem != m_MapBtnMic.end(); ++itorItem) {
		obs_source_t * lpSrcSource = obs_sceneitem_get_source(itorItem->first);
		// 如果存在相同的数据源对象，直接返回...
		if (lpSrcSource == lpNewSource)
			return;
		ASSERT(lpSrcSource != lpNewSource);
	}
	// 创建一个新的麦克风按钮，放入集合队列当中...
	QPushButton * lpNewBtnMic = this->CreateBtnMic();
	m_MapBtnMic[lpSceneItem] = lpNewBtnMic;
	// 设置麦克风按钮默认的初始风格 => 默认处于第三方静音状态...
	QString strStyle = QString("QPushButton{ background:transparent; border-image:url(:/res/images/btn_mic.png) 0 30 0 60; }"
		"QPushButton:hover{border-image:url(:/res/images/btn_mic.png) 0 0 0 90;}"
		"QPushButton:pressed{border-image:url(:/res/images/btn_mic.png) 0 90 0 0;}");
	lpNewBtnMic->setStyleSheet(strStyle);
	// 移动麦克风按钮到数据源窗口的对应位置上...
	this->doResizeBtnMic(lpSceneItem);
	// 保存当前麦克风按钮的自定义状态属性 => 非活动状态...
	lpNewBtnMic->setProperty("is_active", QVariant(false));
	lpNewBtnMic->setProperty("scene_item", QVariant((qulonglong)lpSceneItem));
	// 绑定按钮相关的点击事件 => 关联到预览窗口自己身上...
	this->connect(lpNewBtnMic, SIGNAL(clicked()), this, SLOT(onBtnMicClicked()));
}

void OBSBasicPreview::onBtnMicClicked()
{
	QPushButton * lpBtnMic = reinterpret_cast<QPushButton*>(sender());
	if (lpBtnMic == NULL)
		return;
	obs_sceneitem_t * lpSceneItem = (obs_sceneitem_t*)lpBtnMic->property("scene_item").toULongLong();
	obs_source_t * lpSourceItem = obs_sceneitem_get_source(lpSceneItem);
	if (lpSceneItem == NULL || lpSourceItem == NULL)
		return;
	// 设置麦克风按钮的活动状态风格 => 活动或非活动...
	QString strStyleActive = QString("QPushButton{ background:transparent; border-image:url(:/res/images/btn_mic.png) 0 90 0 0; }"
		"QPushButton:hover{border-image:url(:/res/images/btn_mic.png) 0 60 0 30;}"
		"QPushButton:pressed{border-image:url(:/res/images/btn_mic.png) 0 30 0 60;}");
	QString strStyleOffLine = QString("QPushButton{ background:transparent; border-image:url(:/res/images/btn_mic.png) 0 30 0 60; }"
		"QPushButton:hover{border-image:url(:/res/images/btn_mic.png) 0 0 0 90;}"
		"QPushButton:pressed{border-image:url(:/res/images/btn_mic.png) 0 90 0 0;}");
	// 获取当前的麦克风按钮状态和数据源的核心状态...
	bool isActive = lpBtnMic->property("is_active").toBool();
	obs_data_t * lpSettings = obs_source_get_settings(lpSourceItem);
	bool bHasRecvThread = obs_data_get_bool(lpSettings, "recv_thread");
	int nDBCameraID = obs_data_get_int(lpSettings, "camera_id");
	obs_data_release(lpSettings);
	// 先将当前活动状态取反，得到新状态...
	bool isNewActive = !isActive;
	// 如果是活动状态，但数据源没有在线，设置为非活动状态...
	if (isNewActive && (!bHasRecvThread || nDBCameraID <= 0)) {
		isNewActive = false;
	}
	// 向服务器发送第三方监听学生端变化消息通知...
	OBSBasic * main = reinterpret_cast<OBSBasic*>(App()->GetMainWindow());
	int nNewDBCameraID = isNewActive ? nDBCameraID : 0;
	main->doSendCameraPusherID(nNewDBCameraID);
	// 设置活动状态属性，设置按钮风格状态图标信息...
	lpBtnMic->setProperty("is_active", QVariant(isNewActive));
	lpBtnMic->setStyleSheet(isNewActive ? strStyleActive : strStyleOffLine);
	lpBtnMic->setToolTip(QTStr(isNewActive ? "Preview.Mic.On" : "Preview.Mic.Off"));
	// 如果当前按钮处于新的非活动状态，直接返回...
	if (!isNewActive)
		return;
	ASSERT(isNewActive);
	// 如果当前按钮处于新的活动状态，需要修改其它按钮为非活动状态...
	QPushButton * lpBtnItem = NULL;
	GM_MapBtnMic::iterator itorItem;
	for (itorItem = m_MapBtnMic.begin(); itorItem != m_MapBtnMic.end(); ++itorItem) {
		lpBtnItem = itorItem->second;
		if (lpBtnItem == lpBtnMic) continue;
		lpBtnItem->setStyleSheet(strStyleOffLine);
	}
}

void OBSBasicPreview::ResizeBtnMicAll()
{
	GM_MapBtnMic::iterator itorItem;
	for (itorItem = m_MapBtnMic.begin(); itorItem != m_MapBtnMic.end(); ++itorItem) {
		this->doResizeBtnMic(itorItem->first);
	}
}

void OBSBasicPreview::doResizeBtnMic(obs_sceneitem_t * lpSceneItem)
{
	OBSBasic * main = reinterpret_cast<OBSBasic*>(App()->GetMainWindow());
	if (main == NULL || lpSceneItem == NULL)
		return;
	// 通过数据源对象找到麦克风按钮...
	GM_MapBtnMic::iterator itorItem;
	itorItem = m_MapBtnMic.find(lpSceneItem);
	if (itorItem == m_MapBtnMic.end())
		return;
	// 判断找到麦克风按钮对象指针是否有效...
	QPushButton * lpBtnMic = itorItem->second;
	if (lpBtnMic == NULL)
		return;
	// 获取数据源在obs坐标系位置和大小...
	vec2 vPos = { 0 }, vBounds = { 0 };
	obs_sceneitem_get_pos(lpSceneItem, &vPos);
	obs_sceneitem_get_bounds(lpSceneItem, &vBounds);
	// 先计算居中位置，再计算比例，最后添加预览位置偏移...
	int nPosNewX = (vPos.x + vBounds.x / 2) * main->previewScale + main->previewX - 15;
	int nPosNewY = vPos.y * main->previewScale + main->previewY + 1;
	lpBtnMic->setGeometry(nPosNewX, nPosNewY, 30, 30);
}

QPushButton * OBSBasicPreview::CreateBtnMic()
{
	QPushButton * lpObjButton = new QPushButton(this);
	QString strBtnName = QStringLiteral("btn_mic");
	lpObjButton->setObjectName(strBtnName);
	lpObjButton->setMinimumSize(QSize(30, 30));
	lpObjButton->setMaximumSize(QSize(30, 30));
	lpObjButton->setCursor(QCursor(Qt::PointingHandCursor));
	lpObjButton->setToolTip(QTStr("Preview.Mic.Off"));
	// 进行按钮图片的透明化处理...
	QPalette myPalette;
	QPixmap  myPixmap(QString(":/res/images/%1.png").arg(strBtnName));
	// 让背景图片适应窗口的大小，在背景图片上做 scaled 操作...
	myPalette.setBrush(QPalette::Background, QBrush(myPixmap));
	lpObjButton->setPalette(myPalette);
	lpObjButton->setMask(myPixmap.mask());
	lpObjButton->show();
	return lpObjButton;
}

QPushButton * OBSBasicPreview::CreateBtnFoot()
{
	QPushButton * lpObjButton = new QPushButton(this);
	QString strBtnName = QStringLiteral("btn_foot");
	QString strStyle = QString("QPushButton{ font-family:'Microsoft YaHei'; font-size:14px; color:#FFFFFF;}"
		"QPushButton{ background:transparent; border-image:url(:/res/images/%1.png);}").arg(strBtnName);
	lpObjButton->setStyleSheet(strStyle);
	lpObjButton->setObjectName(strBtnName);
	lpObjButton->setMinimumSize(QSize(100, 30));
	lpObjButton->setMaximumSize(QSize(100, 30));
	// 进行按钮图片的透明化处理...
	QPalette myPalette;
	QPixmap  myPixmap(QString(":/res/images/%1.png").arg(strBtnName));
	// 让背景图片适应窗口的大小，在背景图片上做 scaled 操作...
	myPalette.setBrush(QPalette::Background, QBrush(myPixmap));
	lpObjButton->setPalette(myPalette);
	lpObjButton->setMask(myPixmap.mask());
	lpObjButton->hide();
	return lpObjButton;
}

QPushButton * OBSBasicPreview::CreateBtnPPT(bool bIsPrev)
{
	QPushButton * lpObjButton = new QPushButton(this);
	QString strBtnName = (bIsPrev ? QStringLiteral("btn_prev") : QStringLiteral("btn_next"));
	QString strStyle = QString("QPushButton{ background:transparent; border-image:url(:/res/images/%1.png) 0 80 0 0; }"
		"QPushButton:hover{border-image:url(:/res/images/%1.png) 0 40 0 40;}"
		"QPushButton:pressed{border-image:url(:/res/images/%1.png) 0 0 0 80;}")
		.arg(strBtnName);
	lpObjButton->setStyleSheet(strStyle);
	lpObjButton->setObjectName(strBtnName);
	lpObjButton->setMinimumSize(QSize(40, 50));
	lpObjButton->setMaximumSize(QSize(40, 50));
	lpObjButton->setCursor(QCursor(Qt::PointingHandCursor));
	lpObjButton->setToolTip(QTStr((bIsPrev ? "Preview.Page.Prev" : "Preview.Page.Next")));
	// 进行按钮图片的透明化处理...
	QPalette myPalette;
	QPixmap  myPixmap(QString(":/res/images/%1.png").arg(strBtnName));
	// 让背景图片适应窗口的大小，在背景图片上做 scaled 操作...
	myPalette.setBrush(QPalette::Background, QBrush(myPixmap));
	lpObjButton->setPalette(myPalette);
	lpObjButton->setMask(myPixmap.mask());
	lpObjButton->hide();
	return lpObjButton;
}

QPushButton * OBSBasicPreview::CreateBtnPage(bool bIsLeft)
{
	QPushButton * lpObjButton = new QPushButton(this);
	QString strBtnName = (bIsLeft ? QStringLiteral("btn_left") : QStringLiteral("btn_right"));
	QString strStyle = QString("QPushButton{ background:transparent; border-image:url(:/res/images/%1.png) 0 80 0 0; }"
		"QPushButton:hover{border-image:url(:/res/images/%1.png) 0 40 0 40;}"
		"QPushButton:pressed{border-image:url(:/res/images/%1.png) 0 0 0 80;}")
		.arg(strBtnName);
	lpObjButton->setStyleSheet(strStyle);
	lpObjButton->setObjectName(strBtnName);
	lpObjButton->setMinimumSize(QSize(40, 40));
	lpObjButton->setMaximumSize(QSize(40, 40));
	lpObjButton->setCursor(QCursor(Qt::PointingHandCursor));
	lpObjButton->setToolTip(QTStr((bIsLeft ? "Preview.Page.Left" : "Preview.Page.Right")));
	// 进行按钮图片的透明化处理...
	QPalette myPalette;
	QPixmap  myPixmap(QString(":/res/images/%1.png").arg(strBtnName));
	// 让背景图片适应窗口的大小，在背景图片上做 scaled 操作...
	myPalette.setBrush(QPalette::Background, QBrush(myPixmap));
	lpObjButton->setPalette(myPalette);
	lpObjButton->setMask(myPixmap.mask());
	lpObjButton->hide();
	return lpObjButton;
}

// 第二排数据源左右圆形按钮的位置调整...
void OBSBasicPreview::ResizeBtnPage(int baseCY)
{
	// obs的坐标系投射到预览画面的坐标位置，previewScale是投射比例...
	// previewScale 对 X 和 Y 都是适合的，详见 GetScaleAndCenterPos()...
	OBSBasic * main = reinterpret_cast<OBSBasic*>(App()->GetMainWindow());
	int nPrevCY = int(main->previewScale * float(baseCY));
	int nPosY = main->previewY + nPrevCY - nPrevCY / 5 / 2 - 20;
	QSize targetSize = GetPixelSize(this);
	int nPosLeftX = 0; int nPosRightX = 0;
	nPosRightX = targetSize.width() - 40;
	// 设定左右圆形按钮的位置和大小 => 40*40...
	m_btnLeft->setGeometry(nPosLeftX, nPosY, 40, 40);
	m_btnRight->setGeometry(nPosRightX, nPosY, 40, 40);
}

void OBSBasicPreview::ResizeBtnPPT(int baseCY)
{
	// obs的坐标系投射到预览画面的坐标位置，previewScale是投射比例...
	// previewScale 对 X 和 Y 都是适合的，详见 GetScaleAndCenterPos()...
	OBSBasic * main = reinterpret_cast<OBSBasic*>(App()->GetMainWindow());
	int nPrevCY = int(main->previewScale * float(baseCY));
	int nPosY = main->previewY + (nPrevCY - nPrevCY / 5) / 2 - 30;
	QSize targetSize = GetPixelSize(this);
	int nPosLeftX = 0; int nPosRightX = 0;
	nPosRightX = targetSize.width() - 40;
	// 设定左右翻页按钮的位置和大小 => 40*50...
	m_btnPrev->setGeometry(nPosLeftX, nPosY, 40, 50);
	m_btnNext->setGeometry(nPosRightX, nPosY, 40, 50);
	// 设定顶部信息按钮的位置和大小 => 100*30...
	nPosLeftX = (targetSize.width() - 100) / 2;
	m_btnFoot->setGeometry(nPosLeftX, main->previewY, 100, 30);
}

void OBSBasicPreview::DispBtnRight(bool bIsShow)
{
	if (m_btnRight == NULL) return;
	(bIsShow ? m_btnRight->show() : m_btnRight->hide());
}

void OBSBasicPreview::DispBtnLeft(bool bIsShow)
{
	if (m_btnLeft == NULL) return;
	(bIsShow ? m_btnLeft->show() : m_btnLeft->hide());
}

void OBSBasicPreview::DispBtnPrev(bool bIsShow)
{
	if (m_btnPrev == NULL) return;
	(bIsShow ? m_btnPrev->show() : m_btnPrev->hide());
}

void OBSBasicPreview::DispBtnNext(bool bIsShow)
{
	if (m_btnNext == NULL) return;
	(bIsShow ? m_btnNext->show() : m_btnNext->hide());
}

void OBSBasicPreview::DispBtnFoot(bool bIsShow, int nCurItem, int nFileNum, const char * lpName)
{
	if (m_btnFoot == NULL) return;
	(bIsShow ? m_btnFoot->show() : m_btnFoot->hide());
	if (lpName != NULL) {
		m_btnFoot->setText(QString("%1 %2/%3").arg(lpName).arg(nCurItem + 1).arg(nFileNum));
	} else {
		m_btnFoot->setText(QString("%1/%2").arg(nCurItem + 1).arg(nFileNum));
	}
}

vec2 OBSBasicPreview::GetMouseEventPos(QMouseEvent *event)
{
	OBSBasic *main = reinterpret_cast<OBSBasic*>(App()->GetMainWindow());
	float pixelRatio = main->devicePixelRatio();
	float scale = pixelRatio / main->previewScale;
	vec2 pos;
	vec2_set(&pos,
		(float(event->x()) - main->previewX / pixelRatio) * scale,
		(float(event->y()) - main->previewY / pixelRatio) * scale);

	return pos;
}

struct SceneFindData {
	const vec2   &pos;
	OBSSceneItem item;
	bool         selectBelow;

	SceneFindData(const SceneFindData &) = delete;
	SceneFindData(SceneFindData &&) = delete;
	SceneFindData& operator=(const SceneFindData &) = delete;
	SceneFindData& operator=(SceneFindData &&) = delete;

	inline SceneFindData(const vec2 &pos_, bool selectBelow_)
		: pos         (pos_),
		  selectBelow (selectBelow_)
	{}
};

static bool SceneItemHasVideo(obs_sceneitem_t *item)
{
	obs_source_t *source = obs_sceneitem_get_source(item);
	uint32_t flags = obs_source_get_output_flags(source);
	return (flags & OBS_SOURCE_VIDEO) != 0;
}

static bool CloseFloat(float a, float b, float epsilon=0.01)
{
	using std::abs;
	return abs(a-b) <= epsilon;
}

static bool FindItemAtPos(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	SceneFindData *data = reinterpret_cast<SceneFindData*>(param);
	matrix4       transform;
	matrix4       invTransform;
	vec3          transformedPos;
	vec3          pos3;
	vec3          pos3_;

	if (!SceneItemHasVideo(item))
		return true;
	if (obs_sceneitem_locked(item))
		return true;

	vec3_set(&pos3, data->pos.x, data->pos.y, 0.0f);

	obs_sceneitem_get_box_transform(item, &transform);

	matrix4_inv(&invTransform, &transform);
	vec3_transform(&transformedPos, &pos3, &invTransform);
	vec3_transform(&pos3_, &transformedPos, &transform);

	if (CloseFloat(pos3.x, pos3_.x) && CloseFloat(pos3.y, pos3_.y) &&
	    transformedPos.x >= 0.0f && transformedPos.x <= 1.0f &&
	    transformedPos.y >= 0.0f && transformedPos.y <= 1.0f) {
		if (data->selectBelow && obs_sceneitem_selected(item)) {
			if (data->item)
				return false;
			else
				data->selectBelow = false;
		}

		data->item = item;
	}

	UNUSED_PARAMETER(scene);
	return true;
}

static vec3 GetTransformedPos(float x, float y, const matrix4 &mat)
{
	vec3 result;
	vec3_set(&result, x, y, 0.0f);
	vec3_transform(&result, &result, &mat);
	return result;
}

static vec3 GetTransformedPosScaled(float x, float y, const matrix4 &mat,
		float scale)
{
	vec3 result;
	vec3_set(&result, x, y, 0.0f);
	vec3_transform(&result, &result, &mat);
	vec3_mulf(&result, &result, scale);
	return result;
}

static inline vec2 GetOBSScreenSize()
{
	obs_video_info ovi;
	vec2 size;
	vec2_zero(&size);

	if (obs_get_video_info(&ovi)) {
		size.x = float(ovi.base_width);
		size.y = float(ovi.base_height);
	}

	return size;
}

vec3 OBSBasicPreview::GetSnapOffset(const vec3 &tl, const vec3 &br)
{
	OBSBasic *main = reinterpret_cast<OBSBasic*>(App()->GetMainWindow());
	vec2 screenSize = GetOBSScreenSize();
	vec3 clampOffset;

	vec3_zero(&clampOffset);

	const bool snap = config_get_bool(GetGlobalConfig(),
			"BasicWindow", "SnappingEnabled");
	if (snap == false)
		return clampOffset;

	const bool screenSnap = config_get_bool(GetGlobalConfig(),
			"BasicWindow", "ScreenSnapping");
	const bool centerSnap = config_get_bool(GetGlobalConfig(),
			"BasicWindow", "CenterSnapping");

	const float clampDist = config_get_double(GetGlobalConfig(),
			"BasicWindow", "SnapDistance") / main->previewScale;
	const float centerX = br.x - (br.x - tl.x) / 2.0f;
	const float centerY = br.y - (br.y - tl.y) / 2.0f;

	// Left screen edge.
	if (screenSnap &&
	    fabsf(tl.x) < clampDist)
		clampOffset.x = -tl.x;
	// Right screen edge.
	if (screenSnap &&
	    fabsf(clampOffset.x) < EPSILON &&
	    fabsf(screenSize.x - br.x) < clampDist)
		clampOffset.x = screenSize.x - br.x;
	// Horizontal center.
	if (centerSnap &&
	    fabsf(screenSize.x - (br.x - tl.x)) > clampDist &&
	    fabsf(screenSize.x / 2.0f - centerX) < clampDist)
		clampOffset.x = screenSize.x / 2.0f - centerX;

	// Top screen edge.
	if (screenSnap &&
	    fabsf(tl.y) < clampDist)
		clampOffset.y = -tl.y;
	// Bottom screen edge.
	if (screenSnap &&
	    fabsf(clampOffset.y) < EPSILON &&
	    fabsf(screenSize.y - br.y) < clampDist)
		clampOffset.y = screenSize.y - br.y;
	// Vertical center.
	if (centerSnap &&
	    fabsf(screenSize.y - (br.y - tl.y)) > clampDist &&
	    fabsf(screenSize.y / 2.0f - centerY) < clampDist)
		clampOffset.y = screenSize.y / 2.0f - centerY;

	return clampOffset;
}

OBSSceneItem OBSBasicPreview::GetItemAtPos(const vec2 &pos, bool selectBelow)
{
	OBSBasic *main = reinterpret_cast<OBSBasic*>(App()->GetMainWindow());

	OBSScene scene = main->GetCurrentScene();
	if (!scene) return OBSSceneItem();

	SceneFindData data(pos, selectBelow);
	obs_scene_enum_items(scene, FindItemAtPos, &data);
	return data.item;
}

static bool CheckItemSelected(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	SceneFindData *data = reinterpret_cast<SceneFindData*>(param);
	matrix4       transform;
	vec3          transformedPos;
	vec3          pos3;

	if (!SceneItemHasVideo(item))
		return true;

	vec3_set(&pos3, data->pos.x, data->pos.y, 0.0f);

	obs_sceneitem_get_box_transform(item, &transform);

	matrix4_inv(&transform, &transform);
	vec3_transform(&transformedPos, &pos3, &transform);

	if (transformedPos.x >= 0.0f && transformedPos.x <= 1.0f &&
	    transformedPos.y >= 0.0f && transformedPos.y <= 1.0f) {
		if (obs_sceneitem_selected(item)) {
			data->item = item;
			return false;
		}
	}

	UNUSED_PARAMETER(scene);
	return true;
}

bool OBSBasicPreview::SelectedAtPos(const vec2 &pos)
{
	OBSBasic *main = reinterpret_cast<OBSBasic*>(App()->GetMainWindow());

	OBSScene scene = main->GetCurrentScene();
	if (!scene)
		return false;

	SceneFindData data(pos, false);
	obs_scene_enum_items(scene, CheckItemSelected, &data);
	return !!data.item;
}

struct HandleFindData {
	const vec2   &pos;
	const float  scale;

	OBSSceneItem item;
	ItemHandle   handle = ItemHandle::None;

	HandleFindData(const HandleFindData &) = delete;
	HandleFindData(HandleFindData &&) = delete;
	HandleFindData& operator=(const HandleFindData &) = delete;
	HandleFindData& operator=(HandleFindData &&) = delete;

	inline HandleFindData(const vec2 &pos_, float scale_)
		: pos   (pos_),
		  scale (scale_)
	{}
};

static bool FindHandleAtPos(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	if (!obs_sceneitem_selected(item))
		return true;

	HandleFindData *data = reinterpret_cast<HandleFindData*>(param);
	matrix4        transform;
	vec3           pos3;
	float          closestHandle = HANDLE_SEL_RADIUS;

	vec3_set(&pos3, data->pos.x, data->pos.y, 0.0f);

	obs_sceneitem_get_box_transform(item, &transform);

	auto TestHandle = [&] (float x, float y, ItemHandle handle)
	{
		vec3 handlePos = GetTransformedPosScaled(x, y, transform,
				data->scale);

		float dist = vec3_dist(&handlePos, &pos3);
		if (dist < HANDLE_SEL_RADIUS) {
			if (dist < closestHandle) {
				closestHandle = dist;
				data->handle  = handle;
				data->item    = item;
			}
		}
	};

	TestHandle(0.0f, 0.0f, ItemHandle::TopLeft);
	TestHandle(0.5f, 0.0f, ItemHandle::TopCenter);
	TestHandle(1.0f, 0.0f, ItemHandle::TopRight);
	TestHandle(0.0f, 0.5f, ItemHandle::CenterLeft);
	TestHandle(1.0f, 0.5f, ItemHandle::CenterRight);
	TestHandle(0.0f, 1.0f, ItemHandle::BottomLeft);
	TestHandle(0.5f, 1.0f, ItemHandle::BottomCenter);
	TestHandle(1.0f, 1.0f, ItemHandle::BottomRight);

	UNUSED_PARAMETER(scene);
	return true;
}

static vec2 GetItemSize(obs_sceneitem_t *item)
{
	obs_bounds_type boundsType = obs_sceneitem_get_bounds_type(item);
	vec2 size;

	if (boundsType != OBS_BOUNDS_NONE) {
		obs_sceneitem_get_bounds(item, &size);
	} else {
		obs_source_t *source = obs_sceneitem_get_source(item);
		obs_sceneitem_crop crop;
		vec2 scale;

		obs_sceneitem_get_scale(item, &scale);
		obs_sceneitem_get_crop(item, &crop);
		size.x = float(obs_source_get_width(source) -
				crop.left - crop.right) * scale.x;
		size.y = float(obs_source_get_height(source) -
				crop.top - crop.bottom) * scale.y;
	}

	return size;
}

void OBSBasicPreview::GetStretchHandleData(const vec2 &pos)
{
	OBSBasic *main = reinterpret_cast<OBSBasic*>(App()->GetMainWindow());

	OBSScene scene = main->GetCurrentScene();
	if (!scene)
		return;

	HandleFindData data(pos, main->previewScale / main->devicePixelRatio());
	obs_scene_enum_items(scene, FindHandleAtPos, &data);

	stretchItem     = std::move(data.item);
	stretchHandle   = data.handle;

	if (stretchHandle != ItemHandle::None) {
		matrix4 boxTransform;
		vec3    itemUL;
		float   itemRot;

		stretchItemSize = GetItemSize(stretchItem);

		obs_sceneitem_get_box_transform(stretchItem, &boxTransform);
		itemRot = obs_sceneitem_get_rot(stretchItem);
		vec3_from_vec4(&itemUL, &boxTransform.t);

		/* build the item space conversion matrices */
		matrix4_identity(&itemToScreen);
		matrix4_rotate_aa4f(&itemToScreen, &itemToScreen,
				0.0f, 0.0f, 1.0f, RAD(itemRot));
		matrix4_translate3f(&itemToScreen, &itemToScreen,
				itemUL.x, itemUL.y, 0.0f);

		matrix4_identity(&screenToItem);
		matrix4_translate3f(&screenToItem, &screenToItem,
				-itemUL.x, -itemUL.y, 0.0f);
		matrix4_rotate_aa4f(&screenToItem, &screenToItem,
				0.0f, 0.0f, 1.0f, RAD(-itemRot));

		obs_sceneitem_get_crop(stretchItem, &startCrop);
		obs_sceneitem_get_pos(stretchItem, &startItemPos);

		obs_source_t *source = obs_sceneitem_get_source(stretchItem);
		cropSize.x = float(obs_source_get_width(source) -
				startCrop.left - startCrop.right);
		cropSize.y = float(obs_source_get_height(source) -
				startCrop.top - startCrop.bottom);
	}
}

// 响应全屏快捷键的按下事件操作...
/*void OBSBasicPreview::doKeyFullScreen()
{
	//if (this->isFullScreen()) {
	if (m_bIsFullScreen) {
		// 窗口退出全屏状态...
		this->setWindowFlags(Qt::SubWindow);
		this->showNormal();
		// 需要恢复到全屏前的矩形区域...
		this->setGeometry(m_rcNoramlRect);
		m_bIsFullScreen = false;
	} else {
		// 需要先保存全屏前的矩形区域...
		m_rcNoramlRect = this->geometry();
		m_bIsFullScreen = true;
		// 窗口进入全屏状态...
		this->setWindowFlags(Qt::Window);
		QScreen *screen = QGuiApplication::screens()[0];
		this->setGeometry(screen->geometry());
		this->show();
		//this->showFullScreen();
	}
}

// 响应Escape快捷键的按下事件...
void OBSBasicPreview::doKeyEscape()
{
	// 如果不是全屏状态，直接返回...
	if (!this->isFullScreen())
		return;
	// 还原渲染窗口的状态 => 恢复到普通窗口...
	this->setWindowFlags(Qt::SubWindow);
	this->showNormal();
	// 需要恢复到全屏前的矩形区域...
	this->setGeometry(m_rcNoramlRect);
}*/

void OBSBasicPreview::keyPressEvent(QKeyEvent *event)
{
	/*// 处理全屏操作的快捷键...
	switch (event->key()) {
	case Qt::Key_Escape: this->doKeyEscape(); break;
	case Qt::Key_F:      this->doKeyFullScreen(); break;
	}*/

	// 处理其它情况的快捷键...
	if (!IsFixedScaling() || event->isAutoRepeat()) {
		OBSQTDisplay::keyPressEvent(event);
		return;
	}
	// 空格键按下时，设置鼠标形状...
	switch (event->key()) {
	case Qt::Key_Space:
		setCursor(Qt::OpenHandCursor);
		scrollMode = true;
		break;
	}
	OBSQTDisplay::keyPressEvent(event);
}

void OBSBasicPreview::keyReleaseEvent(QKeyEvent *event)
{
	if (event->isAutoRepeat()) {
		OBSQTDisplay::keyReleaseEvent(event);
		return;
	}
	// 空格键抬起时，设置鼠标形状...
	switch (event->key()) {
	case Qt::Key_Space:
		scrollMode = false;
		setCursor(Qt::ArrowCursor);
		break;
	}
	OBSQTDisplay::keyReleaseEvent(event);
}

void OBSBasicPreview::wheelEvent(QWheelEvent *event)
{
	if (scrollMode && IsFixedScaling()
			&& event->orientation() == Qt::Vertical) {
		if (event->delta() > 0)
			SetScalingLevel(scalingLevel + 1);
		else if (event->delta() < 0)
			SetScalingLevel(scalingLevel - 1);
		emit DisplayResized();
	}

	OBSQTDisplay::wheelEvent(event);
}

void OBSBasicPreview::mousePressEvent(QMouseEvent *event)
{
	if (scrollMode && IsFixedScaling() &&
	    event->button() == Qt::LeftButton) {
		setCursor(Qt::ClosedHandCursor);
		scrollingFrom.x = event->x();
		scrollingFrom.y = event->y();
		return;
	}

	if (event->button() == Qt::RightButton) {
		scrollMode = false;
		setCursor(Qt::ArrowCursor);
	}

	// 屏蔽后，锁定状态下也能绘制编辑框...
	/*if (locked) {
		OBSQTDisplay::mousePressEvent(event);
		return;
	}*/

	OBSBasic *main = reinterpret_cast<OBSBasic*>(App()->GetMainWindow());
	float pixelRatio = main->devicePixelRatio();
	float x = float(event->x()) - main->previewX / pixelRatio;
	float y = float(event->y()) - main->previewY / pixelRatio;
	Qt::KeyboardModifiers modifiers = QGuiApplication::keyboardModifiers();
	bool altDown = (modifiers & Qt::AltModifier);

	OBSQTDisplay::mousePressEvent(event);

	if (event->button() != Qt::LeftButton &&
	    event->button() != Qt::RightButton)
		return;

	if (event->button() == Qt::LeftButton)
		mouseDown = true;

	if (altDown)
		cropping = true;

	vec2_set(&startPos, x, y);
	GetStretchHandleData(startPos);

	vec2_divf(&startPos, &startPos, main->previewScale / pixelRatio);
	startPos.x = std::round(startPos.x);
	startPos.y = std::round(startPos.y);

	mouseOverItems = SelectedAtPos(startPos);
	vec2_zero(&lastMoveOffset);
}

static bool select_one(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	obs_sceneitem_t *selectedItem = reinterpret_cast<obs_sceneitem_t*>(param);
	obs_sceneitem_select(item, (selectedItem == item));

	UNUSED_PARAMETER(scene);
	return true;
}

void OBSBasicPreview::DoSelect(const vec2 &pos)
{
	OBSBasic *main = reinterpret_cast<OBSBasic*>(App()->GetMainWindow());

	OBSScene     scene = main->GetCurrentScene();
	OBSSceneItem item  = GetItemAtPos(pos, true);

	obs_scene_enum_items(scene, select_one, (obs_sceneitem_t*)item);

	obs_source_t * lpSource = obs_sceneitem_get_source(item);
	const char * lpSrcID = obs_source_get_id(lpSource);
	if (lpSource == NULL || lpSrcID == NULL)
		return;
	ASSERT(lpSource != NULL && lpSrcID != NULL);
	// 进行ID判断，如果是rtp资源，需要获取摄像头通道编号...
	if (astrcmpi(lpSrcID, App()->InteractRtpSource()) == 0) {
		obs_data_t * lpSettings = obs_source_get_settings(lpSource);
		int theDBCameraID = obs_data_get_int(lpSettings, "camera_id");
		// 将新的摄像头编号更新到云台控制当中...
		main->doUpdatePTZ(theDBCameraID);
		// 注意：这里必须手动进行引用计数减少，否则，会造成内存泄漏...
		obs_data_release(lpSettings);
	}
	// 如果是幻灯片资源，需要进行位置判断...
	/*if (astrcmpi(lpSrcID, "slideshow") == 0) {
		// 获取到幻灯片数据源的上一页和下一页的快捷热键值...
		obs_data_t * lpSettings = obs_source_get_settings(lpSource);
		obs_hotkey_id next_hotkey = obs_data_get_int(lpSettings, "next_hotkey");
		obs_hotkey_id prev_hotkey = obs_data_get_int(lpSettings, "prev_hotkey");
		obs_data_release(lpSettings);
		// 计算鼠标位置在数据源窗口的位置...
		obs_transform_info itemInfo = { 0 };
		obs_sceneitem_get_info(item, &itemInfo);
		float nMiddlePos = itemInfo.pos.x + itemInfo.bounds.x / 2;
		obs_hotkey_id click_hotkey = (pos.x > nMiddlePos) ? next_hotkey : prev_hotkey;
		obs_hotkey_trigger_routed_callback(click_hotkey, true);
	}*/
}

void OBSBasicPreview::DoCtrlSelect(const vec2 &pos)
{
	OBSSceneItem item = GetItemAtPos(pos, false);
	if (!item)
		return;

	bool selected = obs_sceneitem_selected(item);
	obs_sceneitem_select(item, !selected);
}

void OBSBasicPreview::ProcessClick(const vec2 &pos)
{
	Qt::KeyboardModifiers modifiers = QGuiApplication::keyboardModifiers();

	if (modifiers & Qt::ControlModifier)
		DoCtrlSelect(pos);
	else
		DoSelect(pos);
}

void OBSBasicPreview::mouseReleaseEvent(QMouseEvent *event)
{
	if (scrollMode)
		setCursor(Qt::OpenHandCursor);

	// 屏蔽后，锁定状态下也能绘制编辑框...
	/*if (locked) {
		OBSQTDisplay::mouseReleaseEvent(event);
		return;
	}*/

	if (mouseDown) {
		vec2 pos = GetMouseEventPos(event);

		if (!mouseMoved) {
			ProcessClick(pos);
		}
		stretchItem = nullptr;
		mouseDown   = false;
		mouseMoved  = false;
		cropping    = false;
	}
}

struct SelectedItemBounds {
	bool first = true;
	vec3 tl, br;
};

static bool AddItemBounds(obs_scene_t *scene, obs_sceneitem_t *item,
		void *param)
{
	SelectedItemBounds *data = reinterpret_cast<SelectedItemBounds*>(param);

	if (!obs_sceneitem_selected(item))
		return true;

	matrix4 boxTransform;
	obs_sceneitem_get_box_transform(item, &boxTransform);

	vec3 t[4] = {
		GetTransformedPos(0.0f, 0.0f, boxTransform),
		GetTransformedPos(1.0f, 0.0f, boxTransform),
		GetTransformedPos(0.0f, 1.0f, boxTransform),
		GetTransformedPos(1.0f, 1.0f, boxTransform)
	};

	for (const vec3 &v : t) {
		if (data->first) {
			vec3_copy(&data->tl, &v);
			vec3_copy(&data->br, &v);
			data->first = false;
		} else {
			vec3_min(&data->tl, &data->tl, &v);
			vec3_max(&data->br, &data->br, &v);
		}
	}

	UNUSED_PARAMETER(scene);
	return true;
}

struct OffsetData {
	float clampDist;
	vec3 tl, br, offset;
};

static bool GetSourceSnapOffset(obs_scene_t *scene, obs_sceneitem_t *item,
		void *param)
{
	OffsetData *data = reinterpret_cast<OffsetData*>(param);

	if (obs_sceneitem_selected(item))
		return true;

	matrix4 boxTransform;
	obs_sceneitem_get_box_transform(item, &boxTransform);

	vec3 t[4] = {
		GetTransformedPos(0.0f, 0.0f, boxTransform),
		GetTransformedPos(1.0f, 0.0f, boxTransform),
		GetTransformedPos(0.0f, 1.0f, boxTransform),
		GetTransformedPos(1.0f, 1.0f, boxTransform)
	};

	bool first = true;
	vec3 tl, br;
	vec3_zero(&tl);
	vec3_zero(&br);
	for (const vec3 &v : t) {
		if (first) {
			vec3_copy(&tl, &v);
			vec3_copy(&br, &v);
			first = false;
		} else {
			vec3_min(&tl, &tl, &v);
			vec3_max(&br, &br, &v);
		}
	}

	// Snap to other source edges
#define EDGE_SNAP(l, r, x, y) \
	do { \
		double dist = fabsf(l.x - data->r.x); \
		if (dist < data->clampDist && \
		    fabsf(data->offset.x) < EPSILON && \
		    data->tl.y < br.y && \
		    data->br.y > tl.y && \
		    (fabsf(data->offset.x) > dist || data->offset.x < EPSILON)) \
			data->offset.x = l.x - data->r.x; \
	} while (false)

	EDGE_SNAP(tl, br, x, y);
	EDGE_SNAP(tl, br, y, x);
	EDGE_SNAP(br, tl, x, y);
	EDGE_SNAP(br, tl, y, x);
#undef EDGE_SNAP

	UNUSED_PARAMETER(scene);
	return true;
}

void OBSBasicPreview::SnapItemMovement(vec2 &offset)
{
	OBSBasic *main = reinterpret_cast<OBSBasic*>(App()->GetMainWindow());
	OBSScene scene = main->GetCurrentScene();

	SelectedItemBounds data;
	obs_scene_enum_items(scene, AddItemBounds, &data);

	data.tl.x += offset.x;
	data.tl.y += offset.y;
	data.br.x += offset.x;
	data.br.y += offset.y;

	vec3 snapOffset = GetSnapOffset(data.tl, data.br);

	const bool snap = config_get_bool(GetGlobalConfig(),
			"BasicWindow", "SnappingEnabled");
	const bool sourcesSnap = config_get_bool(GetGlobalConfig(),
			"BasicWindow", "SourceSnapping");
	if (snap == false)
		return;
	if (sourcesSnap == false) {
		offset.x += snapOffset.x;
		offset.y += snapOffset.y;
		return;
	}

	const float clampDist = config_get_double(GetGlobalConfig(),
			"BasicWindow", "SnapDistance") / main->previewScale;

	OffsetData offsetData;
	offsetData.clampDist = clampDist;
	offsetData.tl = data.tl;
	offsetData.br = data.br;
	vec3_copy(&offsetData.offset, &snapOffset);

	obs_scene_enum_items(scene, GetSourceSnapOffset, &offsetData);

	if (fabsf(offsetData.offset.x) > EPSILON ||
	    fabsf(offsetData.offset.y) > EPSILON) {
		offset.x += offsetData.offset.x;
		offset.y += offsetData.offset.y;
	} else {
		offset.x += snapOffset.x;
		offset.y += snapOffset.y;
	}
}

static bool move_items(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	// 如果数据源被锁定，直接返回...
	if (obs_sceneitem_locked(item))
		return true;
	// 如果数据源不能浮动，直接返回...
	if (!obs_sceneitem_floated(item))
		return true;

	vec2 *offset = reinterpret_cast<vec2*>(param);

	if (obs_sceneitem_selected(item)) {
		vec2 pos;
		obs_sceneitem_get_pos(item, &pos);
		vec2_add(&pos, &pos, offset);
		obs_sceneitem_set_pos(item, &pos);
	}

	UNUSED_PARAMETER(scene);
	return true;
}

void OBSBasicPreview::MoveItems(const vec2 &pos)
{
	Qt::KeyboardModifiers modifiers = QGuiApplication::keyboardModifiers();
	OBSBasic *main = reinterpret_cast<OBSBasic*>(App()->GetMainWindow());
	OBSScene scene = main->GetCurrentScene();

	vec2 offset, moveOffset;
	vec2_sub(&offset, &pos, &startPos);
	vec2_sub(&moveOffset, &offset, &lastMoveOffset);

	if (!(modifiers & Qt::ControlModifier))
		SnapItemMovement(moveOffset);

	vec2_add(&lastMoveOffset, &lastMoveOffset, &moveOffset);

	obs_scene_enum_items(scene, move_items, &moveOffset);
}

vec3 OBSBasicPreview::CalculateStretchPos(const vec3 &tl, const vec3 &br)
{
	uint32_t alignment = obs_sceneitem_get_alignment(stretchItem);
	vec3 pos;

	vec3_zero(&pos);

	if (alignment & OBS_ALIGN_LEFT)
		pos.x = tl.x;
	else if (alignment & OBS_ALIGN_RIGHT)
		pos.x = br.x;
	else
		pos.x = (br.x - tl.x) * 0.5f + tl.x;

	if (alignment & OBS_ALIGN_TOP)
		pos.y = tl.y;
	else if (alignment & OBS_ALIGN_BOTTOM)
		pos.y = br.y;
	else
		pos.y = (br.y - tl.y) * 0.5f + tl.y;

	return pos;
}

void OBSBasicPreview::ClampAspect(vec3 &tl, vec3 &br, vec2 &size,
		const vec2 &baseSize)
{
	float    baseAspect   = baseSize.x / baseSize.y;
	float    aspect       = size.x / size.y;
	uint32_t stretchFlags = (uint32_t)stretchHandle;

	if (stretchHandle == ItemHandle::TopLeft    ||
	    stretchHandle == ItemHandle::TopRight   ||
	    stretchHandle == ItemHandle::BottomLeft ||
	    stretchHandle == ItemHandle::BottomRight) {
		if (aspect < baseAspect) {
			if ((size.y >= 0.0f && size.x >= 0.0f) ||
			    (size.y <= 0.0f && size.x <= 0.0f))
				size.x = size.y * baseAspect;
			else
				size.x = size.y * baseAspect * -1.0f;
		} else {
			if ((size.y >= 0.0f && size.x >= 0.0f) ||
			    (size.y <= 0.0f && size.x <= 0.0f))
				size.y = size.x / baseAspect;
			else
				size.y = size.x / baseAspect * -1.0f;
		}

	} else if (stretchHandle == ItemHandle::TopCenter ||
	           stretchHandle == ItemHandle::BottomCenter) {
		if ((size.y >= 0.0f && size.x >= 0.0f) ||
		    (size.y <= 0.0f && size.x <= 0.0f))
			size.x = size.y * baseAspect;
		else
			size.x = size.y * baseAspect * -1.0f;

	} else if (stretchHandle == ItemHandle::CenterLeft ||
	           stretchHandle == ItemHandle::CenterRight) {
		if ((size.y >= 0.0f && size.x >= 0.0f) ||
		    (size.y <= 0.0f && size.x <= 0.0f))
			size.y = size.x / baseAspect;
		else
			size.y = size.x / baseAspect * -1.0f;
	}

	size.x = std::round(size.x);
	size.y = std::round(size.y);

	if (stretchFlags & ITEM_LEFT)
		tl.x = br.x - size.x;
	else if (stretchFlags & ITEM_RIGHT)
		br.x = tl.x + size.x;

	if (stretchFlags & ITEM_TOP)
		tl.y = br.y - size.y;
	else if (stretchFlags & ITEM_BOTTOM)
		br.y = tl.y + size.y;
}

void OBSBasicPreview::SnapStretchingToScreen(vec3 &tl, vec3 &br)
{
	uint32_t stretchFlags = (uint32_t)stretchHandle;
	vec3     newTL        = GetTransformedPos(tl.x, tl.y, itemToScreen);
	vec3     newTR        = GetTransformedPos(br.x, tl.y, itemToScreen);
	vec3     newBL        = GetTransformedPos(tl.x, br.y, itemToScreen);
	vec3     newBR        = GetTransformedPos(br.x, br.y, itemToScreen);
	vec3     boundingTL;
	vec3     boundingBR;

	vec3_copy(&boundingTL, &newTL);
	vec3_min(&boundingTL, &boundingTL, &newTR);
	vec3_min(&boundingTL, &boundingTL, &newBL);
	vec3_min(&boundingTL, &boundingTL, &newBR);

	vec3_copy(&boundingBR, &newTL);
	vec3_max(&boundingBR, &boundingBR, &newTR);
	vec3_max(&boundingBR, &boundingBR, &newBL);
	vec3_max(&boundingBR, &boundingBR, &newBR);

	vec3 offset = GetSnapOffset(boundingTL, boundingBR);
	vec3_add(&offset, &offset, &newTL);
	vec3_transform(&offset, &offset, &screenToItem);
	vec3_sub(&offset, &offset, &tl);

	if (stretchFlags & ITEM_LEFT)
		tl.x += offset.x;
	else if (stretchFlags & ITEM_RIGHT)
		br.x += offset.x;

	if (stretchFlags & ITEM_TOP)
		tl.y += offset.y;
	else if (stretchFlags & ITEM_BOTTOM)
		br.y += offset.y;
}

static float maxfunc(float x, float y)
{
	return x > y ? x : y;
}

static float minfunc(float x, float y)
{
	return x < y ? x : y;
}

void OBSBasicPreview::CropItem(const vec2 &pos)
{
	obs_bounds_type boundsType = obs_sceneitem_get_bounds_type(stretchItem);
	uint32_t stretchFlags = (uint32_t)stretchHandle;
	uint32_t align = obs_sceneitem_get_alignment(stretchItem);
	vec3 tl, br, pos3;

	if (boundsType != OBS_BOUNDS_NONE) /* TODO */
		return;

	vec3_zero(&tl);
	vec3_set(&br, stretchItemSize.x, stretchItemSize.y, 0.0f);

	vec3_set(&pos3, pos.x, pos.y, 0.0f);
	vec3_transform(&pos3, &pos3, &screenToItem);

	obs_sceneitem_crop crop = startCrop;
	vec2 scale;

	obs_sceneitem_get_scale(stretchItem, &scale);

	vec2 max_tl;
	vec2 max_br;

	vec2_set(&max_tl,
		float(-crop.left) * scale.x,
		float(-crop.top) * scale.y);
	vec2_set(&max_br,
		stretchItemSize.x + crop.right * scale.x,
		stretchItemSize.y + crop.bottom * scale.y);

	typedef std::function<float (float, float)> minmax_func_t;

	minmax_func_t min_x = scale.x < 0.0f ? maxfunc : minfunc;
	minmax_func_t min_y = scale.y < 0.0f ? maxfunc : minfunc;
	minmax_func_t max_x = scale.x < 0.0f ? minfunc : maxfunc;
	minmax_func_t max_y = scale.y < 0.0f ? minfunc : maxfunc;

	pos3.x = min_x(pos3.x, max_br.x);
	pos3.x = max_x(pos3.x, max_tl.x);
	pos3.y = min_y(pos3.y, max_br.y);
	pos3.y = max_y(pos3.y, max_tl.y);

	if (stretchFlags & ITEM_LEFT) {
		float maxX = stretchItemSize.x - (2.0 * scale.x);
		pos3.x = tl.x = min_x(pos3.x, maxX);

	} else if (stretchFlags & ITEM_RIGHT) {
		float minX = (2.0 * scale.x);
		pos3.x = br.x = max_x(pos3.x, minX);
	}

	if (stretchFlags & ITEM_TOP) {
		float maxY = stretchItemSize.y - (2.0 * scale.y);
		pos3.y = tl.y = min_y(pos3.y, maxY);

	} else if (stretchFlags & ITEM_BOTTOM) {
		float minY = (2.0 * scale.y);
		pos3.y = br.y = max_y(pos3.y, minY);
	}

#define ALIGN_X (ITEM_LEFT|ITEM_RIGHT)
#define ALIGN_Y (ITEM_TOP|ITEM_BOTTOM)
	vec3 newPos;
	vec3_zero(&newPos);

	uint32_t align_x = (align & ALIGN_X);
	uint32_t align_y = (align & ALIGN_Y);
	if (align_x == (stretchFlags & ALIGN_X) && align_x != 0)
		newPos.x = pos3.x;
	else if (align & ITEM_RIGHT)
		newPos.x = stretchItemSize.x;
	else if (!(align & ITEM_LEFT))
		newPos.x = stretchItemSize.x * 0.5f;

	if (align_y == (stretchFlags & ALIGN_Y) && align_y != 0)
		newPos.y = pos3.y;
	else if (align & ITEM_BOTTOM)
		newPos.y = stretchItemSize.y;
	else if (!(align & ITEM_TOP))
		newPos.y = stretchItemSize.y * 0.5f;
#undef ALIGN_X
#undef ALIGN_Y

	crop = startCrop;

	if (stretchFlags & ITEM_LEFT)
		crop.left += int(std::round(tl.x / scale.x));
	else if (stretchFlags & ITEM_RIGHT)
		crop.right += int(std::round((stretchItemSize.x - br.x) / scale.x));

	if (stretchFlags & ITEM_TOP)
		crop.top += int(std::round(tl.y / scale.y));
	else if (stretchFlags & ITEM_BOTTOM)
		crop.bottom += int(std::round((stretchItemSize.y - br.y) / scale.y));

	vec3_transform(&newPos, &newPos, &itemToScreen);
	newPos.x = std::round(newPos.x);
	newPos.y = std::round(newPos.y);

#if 0
	vec3 curPos;
	vec3_zero(&curPos);
	obs_sceneitem_get_pos(stretchItem, (vec2*)&curPos);
	blog(LOG_DEBUG, "curPos {%d, %d} - newPos {%d, %d}",
			int(curPos.x), int(curPos.y),
			int(newPos.x), int(newPos.y));
	blog(LOG_DEBUG, "crop {%d, %d, %d, %d}",
			crop.left, crop.top,
			crop.right, crop.bottom);
#endif

	obs_sceneitem_defer_update_begin(stretchItem);
	obs_sceneitem_set_crop(stretchItem, &crop);
	obs_sceneitem_set_pos(stretchItem, (vec2*)&newPos);
	obs_sceneitem_defer_update_end(stretchItem);
}

void OBSBasicPreview::StretchItem(const vec2 &pos)
{
	Qt::KeyboardModifiers modifiers = QGuiApplication::keyboardModifiers();
	obs_bounds_type boundsType = obs_sceneitem_get_bounds_type(stretchItem);
	uint32_t stretchFlags = (uint32_t)stretchHandle;
	bool shiftDown = (modifiers & Qt::ShiftModifier);
	vec3 tl, br, pos3;

	vec3_zero(&tl);
	vec3_set(&br, stretchItemSize.x, stretchItemSize.y, 0.0f);

	vec3_set(&pos3, pos.x, pos.y, 0.0f);
	vec3_transform(&pos3, &pos3, &screenToItem);

	if (stretchFlags & ITEM_LEFT)
		tl.x = pos3.x;
	else if (stretchFlags & ITEM_RIGHT)
		br.x = pos3.x;

	if (stretchFlags & ITEM_TOP)
		tl.y = pos3.y;
	else if (stretchFlags & ITEM_BOTTOM)
		br.y = pos3.y;

	if (!(modifiers & Qt::ControlModifier))
		SnapStretchingToScreen(tl, br);

	obs_source_t *source = obs_sceneitem_get_source(stretchItem);

	vec2 baseSize;
	vec2_set(&baseSize,
		float(obs_source_get_width(source)),
		float(obs_source_get_height(source)));

	vec2 size;
	vec2_set(&size,br. x - tl.x, br.y - tl.y);

	if (boundsType != OBS_BOUNDS_NONE) {
		if (shiftDown)
			ClampAspect(tl, br, size, baseSize);

		if (tl.x > br.x) std::swap(tl.x, br.x);
		if (tl.y > br.y) std::swap(tl.y, br.y);

		vec2_abs(&size, &size);

		obs_sceneitem_set_bounds(stretchItem, &size);
	} else {
		obs_sceneitem_crop crop;
		obs_sceneitem_get_crop(stretchItem, &crop);

		baseSize.x -= float(crop.left + crop.right);
		baseSize.y -= float(crop.top + crop.bottom);

		if (!shiftDown)
			ClampAspect(tl, br, size, baseSize);

		vec2_div(&size, &size, &baseSize);
		obs_sceneitem_set_scale(stretchItem, &size);
	}

	pos3 = CalculateStretchPos(tl, br);
	vec3_transform(&pos3, &pos3, &itemToScreen);

	vec2 newPos;
	vec2_set(&newPos, std::round(pos3.x), std::round(pos3.y));
	obs_sceneitem_set_pos(stretchItem, &newPos);
}

void OBSBasicPreview::mouseMoveEvent(QMouseEvent *event)
{
	if (scrollMode && event->buttons() == Qt::LeftButton) {
		scrollingOffset.x += event->x() - scrollingFrom.x;
		scrollingOffset.y += event->y() - scrollingFrom.y;
		scrollingFrom.x = event->x();
		scrollingFrom.y = event->y();
		emit DisplayResized();
		return;
	}

	// 屏蔽后，锁定状态下也能拖动指定数据源窗口...
	//if (locked)
	//	return;

	/////////////////////////////////////////////////////
	// 注意：限制MoveItem还需要move_items中进一步处理...
	/////////////////////////////////////////////////////
	// 如果鼠标不是处于按下状态，直接返回...
	if (!mouseDown)
		return;
	// 通过主窗口得到当前选中的数据源，不要使用光标的坐标来查询数据源...
	OBSBasic *main = reinterpret_cast<OBSBasic*>(App()->GetMainWindow());
	OBSSceneItem itemSelect = main->GetCurrentSceneItem();
	// 如果没有找到场景数据源，直接返回...
	if (itemSelect == NULL)
		return;
	// 如果选中场景数据源，不能浮动，直接返回...
	if (!obs_sceneitem_floated(itemSelect))
		return;
	// 选中矢量坐标所在的场景数据源对象...
	if (!mouseMoved && !mouseOverItems && stretchHandle == ItemHandle::None) {
		this->ProcessClick(startPos);
		mouseOverItems = this->SelectedAtPos(startPos);
	}
	// 得到当前鼠标所在的坐标位置...
	vec2 pos = this->GetMouseEventPos(event);
	pos.x = std::round(pos.x);
	pos.y = std::round(pos.y);
	// 根据当前状态和坐标进行相关的数据源操作...
	if (stretchHandle != ItemHandle::None) {
		if (cropping) this->CropItem(pos);
		else this->StretchItem(pos);
	} else if (mouseOverItems) {
		this->MoveItems(pos);
	}
	// 设置移动标志...
	mouseMoved = true;
	// 移动这个数据源的麦克风按钮...
	this->doResizeBtnMic(itemSelect);
}

static void DrawCircleAtPos(float x, float y, matrix4 &matrix, float previewScale)
{
	struct vec3 pos;
	vec3_set(&pos, x, y, 0.0f);
	vec3_transform(&pos, &pos, &matrix);
	vec3_mulf(&pos, &pos, previewScale);

	gs_matrix_push();
	gs_matrix_translate(&pos);
	gs_matrix_scale3f(HANDLE_RADIUS, HANDLE_RADIUS, 1.0f);
	gs_draw(GS_LINESTRIP, 0, 0);
	gs_matrix_pop();
}

static inline bool crop_enabled(const obs_sceneitem_crop *crop)
{
	return crop->left > 0  ||
	       crop->top > 0   ||
	       crop->right > 0 ||
	       crop->bottom > 0;
}

bool OBSBasicPreview::DrawSelectedItem(obs_scene_t *scene,
		obs_sceneitem_t *item, void *param)
{
	if (obs_sceneitem_locked(item))
		return true;

	if (!SceneItemHasVideo(item))
		return true;

	if (!obs_sceneitem_selected(item))
		return true;

	OBSBasic *main = reinterpret_cast<OBSBasic*>(App()->GetMainWindow());

	matrix4 boxTransform;
	matrix4 invBoxTransform;
	obs_sceneitem_get_box_transform(item, &boxTransform);
	matrix4_inv(&invBoxTransform, &boxTransform);

	vec3 bounds[] = {
		{{{0.f, 0.f, 0.f}}},
		{{{1.f, 0.f, 0.f}}},
		{{{0.f, 1.f, 0.f}}},
		{{{1.f, 1.f, 0.f}}},
	};

	bool visible = std::all_of(std::begin(bounds), std::end(bounds),
			[&](const vec3 &b)
	{
		vec3 pos;
		vec3_transform(&pos, &b, &boxTransform);
		vec3_transform(&pos, &pos, &invBoxTransform);
		return CloseFloat(pos.x, b.x) && CloseFloat(pos.y, b.y);
	});

	if (!visible)
		return true;

	obs_transform_info info;
	obs_sceneitem_get_info(item, &info);

	gs_load_vertexbuffer(main->circle);

	DrawCircleAtPos(0.0f, 0.0f, boxTransform, main->previewScale);
	DrawCircleAtPos(0.0f, 1.0f, boxTransform, main->previewScale);
	DrawCircleAtPos(1.0f, 0.0f, boxTransform, main->previewScale);
	DrawCircleAtPos(1.0f, 1.0f, boxTransform, main->previewScale);
	DrawCircleAtPos(0.5f, 0.0f, boxTransform, main->previewScale);
	DrawCircleAtPos(0.0f, 0.5f, boxTransform, main->previewScale);
	DrawCircleAtPos(0.5f, 1.0f, boxTransform, main->previewScale);
	DrawCircleAtPos(1.0f, 0.5f, boxTransform, main->previewScale);

	gs_matrix_push();
	gs_matrix_scale3f(main->previewScale, main->previewScale, 1.0f);
	gs_matrix_mul(&boxTransform);

	obs_sceneitem_crop crop;
	obs_sceneitem_get_crop(item, &crop);

	if (info.bounds_type == OBS_BOUNDS_NONE && crop_enabled(&crop)) {
		vec4 color;
		gs_effect_t *eff = gs_get_effect();
		gs_eparam_t *param = gs_effect_get_param_by_name(eff, "color");

#define DRAW_SIDE(side, vb) \
		if (crop.side > 0) \
			vec4_set(&color, 0.0f, 1.0f, 0.0f, 1.0f); \
		else \
			vec4_set(&color, 1.0f, 0.0f, 0.0f, 1.0f); \
		gs_effect_set_vec4(param, &color); \
		gs_load_vertexbuffer(main->vb); \
		gs_draw(GS_LINESTRIP, 0, 0);

		DRAW_SIDE(left,   boxLeft);
		DRAW_SIDE(top,    boxTop);
		DRAW_SIDE(right,  boxRight);
		DRAW_SIDE(bottom, boxBottom);
#undef DRAW_SIDE
	} else {
		gs_load_vertexbuffer(main->box);
		gs_draw(GS_LINESTRIP, 0, 0);
	}

	gs_matrix_pop();

	UNUSED_PARAMETER(scene);
	UNUSED_PARAMETER(param);
	return true;
}

void OBSBasicPreview::DrawSceneEditing()
{
	// 屏蔽后，锁定状态下也能绘制编辑框...
	//if (locked)
	//	return;

	OBSBasic *main = reinterpret_cast<OBSBasic*>(App()->GetMainWindow());

	gs_effect_t    *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_technique_t *tech  = gs_effect_get_technique(solid, "Solid");

	vec4 color;
	vec4_set(&color, 1.0f, 0.0f, 0.0f, 1.0f);
	gs_effect_set_vec4(gs_effect_get_param_by_name(solid, "color"), &color);

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	OBSScene scene = main->GetCurrentScene();

	if (scene)
		obs_scene_enum_items(scene, DrawSelectedItem, this);

	gs_load_vertexbuffer(nullptr);

	gs_technique_end_pass(tech);
	gs_technique_end(tech);
}

void OBSBasicPreview::ResetScrollingOffset()
{
	vec2_zero(&scrollingOffset);
}

void OBSBasicPreview::SetScalingLevel(int32_t newScalingLevelVal) {
	float newScalingAmountVal = pow(ZOOM_SENSITIVITY, float(newScalingLevelVal));
	scalingLevel = newScalingLevelVal;
	SetScalingAmount(newScalingAmountVal);
}

void OBSBasicPreview::SetScalingAmount(float newScalingAmountVal) {
	scrollingOffset.x *= newScalingAmountVal / scalingAmount;
	scrollingOffset.y *= newScalingAmountVal / scalingAmount;
	scalingAmount = newScalingAmountVal;
}

// 处理场景资源显示位置的交换操作 => 尽量保持按序号排列显示资源位置...
void OBSBasicPreview::mouseDoubleClickEvent(QMouseEvent *event)
{
	// 获取主窗口对象指针...
	OBSBasic * main = reinterpret_cast<OBSBasic*>(App()->GetMainWindow());
	// 先找到当前鼠标双击位置的场景资源...
	vec2 posMouse, posItem, posDisp;
	posMouse = this->GetMouseEventPos(event);
	OBSSceneItem itemSelect = this->GetItemAtPos(posMouse, false);
	// 如果没有找到对应的场景资源，直接返回...
	if (itemSelect == NULL)
		return;
	// 如果当前场景是浮动数据源，不能进行位置切换...
	if (obs_sceneitem_floated(itemSelect))
		return;
	// 如果双击的本身就是0点位置数据源，直接返回...
	if (itemSelect == main->GetZeroSceneItem())
		return;
	// 向主窗口通知，鼠标双击事件，进行位置切换...
	main->doSceneItemExchangePos(itemSelect);
	// 学生监听状态由讲师手动控制，不再由焦点控制...
	//main->doSendCameraPusherID(itemSelect);
	// 将全部麦克风按钮都重置显示位置...
	this->ResizeBtnMicAll();
}