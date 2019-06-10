#include <QAction>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QMenu>
#include <QScreen>
#include "obs-app.hpp"
#include "window-basic-main.hpp"
#include "display-helpers.hpp"
#include "qt-wrappers.hpp"
#include "platform.hpp"

#define HORIZONTAL_TOP    0
#define HORIZONTAL_BOTTOM 1
#define VERTICAL_LEFT     2
#define VERTICAL_RIGHT    3

static QList<OBSProjector *> windowedProjectors;
static QList<OBSProjector *> multiviewProjectors;
static bool updatingMultiview = false;
static int multiviewLayout = HORIZONTAL_TOP;

OBSProjector::OBSProjector(QWidget *widget, obs_source_t *source_, int monitor,
		QString title, ProjectorType type_)
	: OBSQTDisplay                 (widget,
	                                Qt::Window),
	  source                       (source_),
	  removedSignal                (obs_source_get_signal_handler(source),
	                                "remove", OBSSourceRemoved, this)
{
	projectorTitle = title;
	savedMonitor   = monitor;
	isWindow       = savedMonitor < 0;
	type           = type_;

	if (isWindow) {
		setWindowIcon(QIcon(":/res/images/obs.png"));
	} else {
		setWindowFlags(Qt::FramelessWindowHint |
				Qt::X11BypassWindowManagerHint);
		setWindowIcon(QIcon(":/res/images/obs.png"));
	}

	setAttribute(Qt::WA_DeleteOnClose, true);

	//disable application quit when last window closed
	setAttribute(Qt::WA_QuitOnClose, false);

	installEventFilter(CreateShortcutFilter());

	auto addDrawCallback = [this] ()
	{
		bool isMultiview = type == ProjectorType::Multiview;
		obs_display_add_draw_callback(GetDisplay(),
				isMultiview ? OBSRenderMultiview : OBSRender,
				this);
		obs_display_set_background_color(GetDisplay(), 0x000000);
	};

	connect(this, &OBSQTDisplay::DisplayCreated, addDrawCallback);

	bool hideCursor = config_get_bool(GetGlobalConfig(),
			"BasicWindow", "HideProjectorCursor");
	if (hideCursor && !isWindow) {
		QPixmap empty(16, 16);
		empty.fill(Qt::transparent);
		setCursor(QCursor(empty));
	}

	// �ж��Ƿ���rtp����Դ����ԴͶӰģʽ���������⴦��...
	const char * lpSrcID = obs_source_get_id(source);
	bool bIsRtpSource = ((astrcmpi(lpSrcID, App()->InteractRtpSource()) == 0) ? true : false);
	if (!this->isWindow && bIsRtpSource && this->type == ProjectorType::Source) {
		this->InitRtpSource();
	}

	if (type == ProjectorType::Multiview) {
		obs_enter_graphics();
		gs_render_start(true);
		gs_vertex2f(0.001f, 0.001f);
		gs_vertex2f(0.001f, 0.997f);
		gs_vertex2f(0.997f, 0.997f);
		gs_vertex2f(0.997f, 0.001f);
		gs_vertex2f(0.001f, 0.001f);
		outerBox = gs_render_save();

		gs_render_start(true);
		gs_vertex2f(0.04f, 0.04f);
		gs_vertex2f(0.04f, 0.96f);
		gs_vertex2f(0.96f, 0.96f);
		gs_vertex2f(0.96f, 0.04f);
		gs_vertex2f(0.04f, 0.04f);
		innerBox = gs_render_save();

		gs_render_start(true);
		gs_vertex2f(0.15f, 0.04f);
		gs_vertex2f(0.15f, 0.96f);
		leftVLine = gs_render_save();

		gs_render_start(true);
		gs_vertex2f(0.85f, 0.04f);
		gs_vertex2f(0.85f, 0.96f);
		rightVLine = gs_render_save();

		gs_render_start(true);
		gs_vertex2f(0.0f, 0.5f);
		gs_vertex2f(0.075f, 0.5f);
		leftLine = gs_render_save();

		gs_render_start(true);
		gs_vertex2f(0.5f, 0.0f);
		gs_vertex2f(0.5f, 0.09f);
		topLine = gs_render_save();

		gs_render_start(true);
		gs_vertex2f(0.925f, 0.5f);
		gs_vertex2f(1.0f, 0.5f);
		rightLine = gs_render_save();
		obs_leave_graphics();

		UpdateMultiview();

		multiviewProjectors.push_back(this);
	}

	App()->IncrementSleepInhibition();
	resize(480, 270);
}

OBSProjector::~OBSProjector()
{
	bool isMultiview = type == ProjectorType::Multiview;
	obs_display_remove_draw_callback(GetDisplay(),
			isMultiview ? OBSRenderMultiview : OBSRender, this);

	if (source)
		obs_source_dec_showing(source);

	if (isMultiview) {
		for (OBSWeakSource &weakSrc : multiviewScenes) {
			OBSSource src = OBSGetStrongRef(weakSrc);
			if (src)
				obs_source_dec_showing(src);
		}

		obs_enter_graphics();
		gs_vertexbuffer_destroy(outerBox);
		gs_vertexbuffer_destroy(innerBox);
		gs_vertexbuffer_destroy(leftVLine);
		gs_vertexbuffer_destroy(rightVLine);
		gs_vertexbuffer_destroy(leftLine);
		gs_vertexbuffer_destroy(topLine);
		gs_vertexbuffer_destroy(rightLine);
		obs_leave_graphics();
	}

	if (type == ProjectorType::Multiview)
		multiviewProjectors.removeAll(this);

	if (isWindow)
		windowedProjectors.removeAll(this);

	App()->DecrementSleepInhibition();
}

static OBSSource CreateLabel(const char *name, size_t h)
{
	obs_data_t *settings = obs_data_create();
	obs_data_t *font     = obs_data_create();

	std::string text;
	text += " ";
	text += name;
	text += " ";

#if defined(_WIN32)
	obs_data_set_string(font, "face", "Arial");
#elif defined(__APPLE__)
	obs_data_set_string(font, "face", "Helvetica");
#else
	obs_data_set_string(font, "face", "Monospace");
#endif
	obs_data_set_int(font, "flags", 1); // Bold text
	obs_data_set_int(font, "size", int(h)); //int(h / 9.81));

	obs_data_set_obj(settings, "font", font);
	obs_data_set_string(settings, "text", text.c_str());
	obs_data_set_bool(settings, "outline", false);

#ifdef _WIN32
	const char *text_source_id = "text_gdiplus";
#else
	const char *text_source_id = "text_ft2_source";
#endif

	OBSSource txtSource = obs_source_create_private(text_source_id, name, settings);
	obs_source_release(txtSource);
	obs_data_release(font);
	obs_data_release(settings);

	return txtSource;
}

void OBSProjector::Init()
{
	bool alwaysOnTop = config_get_bool(GetGlobalConfig(),
			"BasicWindow", "ProjectorAlwaysOnTop");
	if (alwaysOnTop && !isWindow)
		SetAlwaysOnTop(this, true);

	show();

	if (isWindow) {
		UpdateProjectorTitle(projectorTitle);
		windowedProjectors.push_back(this);
	} else {
		QScreen *screen = QGuiApplication::screens()[savedMonitor];
		setGeometry(screen->geometry());

		QAction *action = new QAction(this);
		action->setShortcut(Qt::Key_Escape);
		addAction(action);
		connect(action, SIGNAL(triggered()), this,
				SLOT(EscapeTriggered()));
		activateWindow();
	}

	if (source)
		obs_source_inc_showing(source);

	ready = true;
}

static inline void renderVB(gs_effect_t *effect, gs_vertbuffer_t *vb,
		int cx, int cy)
{
	if (!vb)
		return;

	matrix4 transform;
	matrix4_identity(&transform);
	transform.x.x = cx;
	transform.y.y = cy;

	gs_load_vertexbuffer(vb);

	gs_matrix_push();
	gs_matrix_mul(&transform);

	while (gs_effect_loop(effect, "Solid"))
		gs_draw(GS_LINESTRIP, 0, 0);

	gs_matrix_pop();
}

static inline uint32_t labelOffset(obs_source_t *label, uint32_t cx)
{
	uint32_t w = obs_source_get_width(label);
	w = uint32_t(float(w) * 0.5f);
	return (cx / 2) - w;
}

void OBSProjector::OBSRenderMultiview(void *data, uint32_t cx, uint32_t cy)
{
	OBSProjector *window = (OBSProjector *)data;

	if (updatingMultiview || !window->ready)
		return;

	OBSBasic     *main   = (OBSBasic *)obs_frontend_get_main_window();
	uint32_t     targetCX, targetCY;
	int          x, y;
	float        fX, fY, halfCX, halfCY, sourceX, sourceY, labelX, labelY,
		     quarterCX, quarterCY, scale, targetCXF, targetCYF,
		     hiCX, hiCY, qiX, qiY, qiCX, qiCY, hiScaleX, hiScaleY,
		     qiScaleX, qiScaleY;
	uint32_t     offset;

	gs_effect_t  *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_eparam_t  *color = gs_effect_get_param_by_name(solid, "color");

	struct obs_video_info ovi;
	obs_get_video_info(&ovi);
	targetCX = ovi.base_width;
	targetCY = ovi.base_height;

	GetScaleAndCenterPos(targetCX, targetCY, cx, cy, x, y, scale);

	targetCXF = float(targetCX);
	targetCYF = float(targetCY);
	fX        = float(x);
	fY        = float(y);

	halfCX    = (targetCXF + 1) / 2;
	halfCY    = (targetCYF + 1) / 2;
	hiCX      = (halfCX - 4.0);
	hiCY      = (halfCY - 4.0);
	hiScaleX  = hiCX / targetCXF;
	hiScaleY  = hiCY / targetCYF;

	quarterCX = (halfCX + 1) / 2;
	quarterCY = (halfCY + 1) / 2;
	qiCX      = (quarterCX - 8.0);
	qiCY      = (quarterCY - 8.0);
	qiScaleX  = qiCX / targetCXF;
	qiScaleY  = qiCY / targetCYF;

	OBSSource previewSrc = main->GetCurrentSceneSource();
	OBSSource programSrc = main->GetProgramSource();

	bool studioMode = main->IsPreviewProgramMode();

	auto drawBox = [solid, color] (float cx, float cy,
			uint32_t colorVal)
	{
		gs_effect_set_color(color, colorVal);
		while (gs_effect_loop(solid, "Solid"))
			gs_draw_sprite(nullptr, 0, (uint32_t)cx, (uint32_t)cy);
	};

	auto setRegion = [fX, fY, scale] (float x, float y, float cx, float cy)
	{
		float vX  = int(fX + x * scale);
		float vY  = int(fY + y * scale);
		float vCX = int(cx * scale);
		float vCY = int(cy * scale);

		float oL = x;
		float oT = y;
		float oR = (x + cx);
		float oB = (y + cy);

		gs_projection_push();
		gs_viewport_push();
		gs_set_viewport(vX, vY, vCX, vCY);
		gs_ortho(oL, oR, oT, oB, -100.0f, 100.0f);
	};

	auto resetRegion = [] ()
	{
		gs_viewport_pop();
		gs_projection_pop();
	};

	auto calcBaseSource = [&](size_t i)
	{
		switch (multiviewLayout) {
		case VERTICAL_LEFT:
			sourceX = halfCX;
			sourceY = (i / 2 ) * quarterCY;
			if (i % 2 != 0)
				sourceX = halfCX + quarterCX;
			break;
		case VERTICAL_RIGHT:
			sourceX = 0;
			sourceY = (i / 2 ) * quarterCY;
			if (i % 2 != 0)
				sourceX = quarterCX;
			break;
		case HORIZONTAL_BOTTOM:
			if (i < 4) {
				sourceX = (float(i) * quarterCX);
				sourceY = 0;
			} else {
				sourceX = (float(i - 4) * quarterCX);
				sourceY = quarterCY;
			}
			break;
		default: //HORIZONTAL_TOP:
			if (i < 4) {
				sourceX = (float(i) * quarterCX);
				sourceY = halfCY;
			} else {
				sourceX = (float(i - 4) * quarterCX);
				sourceY = halfCY + quarterCY;
			}
		}
	};

	auto calcPreviewProgram = [&](bool program)
	{
		switch (multiviewLayout) {
		case VERTICAL_LEFT:
			sourceX = 2.0f;
			sourceY = halfCY + 2.0f;
			labelX = offset;
			labelY = halfCY * 1.8f;
			if (program) {
				sourceY = 2.0f;
				labelY = halfCY * 0.8f;
			}
			break;
		case VERTICAL_RIGHT:
			sourceX = halfCX + 2.0f;
			sourceY = halfCY + 2.0f;
			labelX = halfCX + offset;
			labelY = halfCY * 1.8f;
			if (program) {
				sourceY = 2.0f;
				labelY = halfCY * 0.8f;
			}
			break;
		case HORIZONTAL_BOTTOM:
			sourceX = 2.0f;
			sourceY = halfCY + 2.0f;
			labelX = offset;
			labelY = halfCY * 1.8f;
			if (program) {
				sourceX = halfCX + 2.0f;
				labelX = halfCX + offset;
			}
			break;
		default: //HORIZONTAL_TOP:
			sourceX = 2.0f;
			sourceY = 2.0f;
			labelX = offset;
			labelY = halfCY * 0.8f;
			if (program) {
				sourceX = halfCX + 2.0f;
				labelX = halfCX + offset;
			}
		}
	};

	/* ----------------------------- */
	/* draw sources                  */

	gs_projection_push();
	gs_viewport_push();
	gs_set_viewport(x, y, targetCX * scale, targetCY * scale);
	gs_ortho(0.0f, targetCXF, 0.0f, targetCYF, -100.0f, 100.0f);

	for (size_t i = 0; i < 8; i++) {
		OBSSource src = OBSGetStrongRef(window->multiviewScenes[i]);
		obs_source *label = window->multiviewLabels[i + 2];

		if (!src)
			continue;
		if (!label)
			continue;

		calcBaseSource(i);

		qiX = sourceX + 4.0f;
		qiY = sourceY + 4.0f;

		/* ----------- */

		if (src == previewSrc || src == programSrc) {
			uint32_t colorVal = src == programSrc
				? 0xFFFF0000
				: 0xFF00FF00;

			gs_matrix_push();
			gs_matrix_translate3f(sourceX, sourceY, 0.0f);
			drawBox(quarterCX, quarterCY, colorVal);
			gs_matrix_pop();

			gs_matrix_push();
			gs_matrix_translate3f(qiX, qiY, 0.0f);
			drawBox(qiCX, qiCY, 0xFF000000);
			gs_matrix_pop();
		}

		/* ----------- */

		gs_matrix_push();
		gs_matrix_translate3f(qiX, qiY, 0.0f);
		gs_matrix_scale3f(qiScaleX, qiScaleY, 1.0f);

		setRegion(qiX, qiY, qiCX, qiCY);
		obs_source_video_render(src);
		resetRegion();

		gs_effect_set_color(color, 0xFFFFFFFF);
		renderVB(solid, window->outerBox, targetCX, targetCY);

		gs_matrix_pop();

		/* ----------- */

		offset = labelOffset(label, quarterCX);
		cx = obs_source_get_width(label);
		cy = obs_source_get_height(label);

		gs_matrix_push();
		gs_matrix_translate3f(sourceX + offset,
				(quarterCY * 0.8f) + sourceY, 0.0f);

		drawBox(cx, cy + int(quarterCX * 0.015f), 0xD91F1F1F);
		obs_source_video_render(label);

		gs_matrix_pop();
	}

	gs_effect_set_color(color, 0xFFFFFFFF);

	/* ----------------------------- */
	/* draw preview                  */

	obs_source_t *previewLabel = window->multiviewLabels[0];
	offset = labelOffset(previewLabel, halfCX);
	calcPreviewProgram(false);

	gs_matrix_push();
	gs_matrix_translate3f(sourceX, sourceY, 0.0f);
	gs_matrix_scale3f(hiScaleX, hiScaleY, 1.0f);

	setRegion(sourceX, sourceY, hiCX, hiCY);

	if (studioMode) {
		obs_source_video_render(previewSrc);
	} else {
		obs_render_main_texture();
	}

	resetRegion();

	gs_matrix_pop();

	/* ----------- */

	gs_matrix_push();
	gs_matrix_translate3f(sourceX, sourceY, 0.0f);
	gs_matrix_scale3f(hiScaleX, hiScaleY, 1.0f);

	renderVB(solid, window->outerBox, targetCX, targetCY);
	renderVB(solid, window->innerBox, targetCX, targetCY);
	renderVB(solid, window->leftVLine, targetCX, targetCY);
	renderVB(solid, window->rightVLine, targetCX, targetCY);
	renderVB(solid, window->leftLine, targetCX, targetCY);
	renderVB(solid, window->topLine, targetCX, targetCY);
	renderVB(solid, window->rightLine, targetCX, targetCY);

	gs_matrix_pop();

	/* ----------- */

	cx = obs_source_get_width(previewLabel);
	cy = obs_source_get_height(previewLabel);

	gs_matrix_push();
	gs_matrix_translate3f(labelX, labelY, 0.0f);

	drawBox(cx, cy + int(halfCX * 0.015f), 0xD91F1F1F);
	obs_source_video_render(previewLabel);

	gs_matrix_pop();

	/* ----------------------------- */
	/* draw program                  */

	obs_source_t *programLabel = window->multiviewLabels[1];
	offset = labelOffset(programLabel, halfCX);
	calcPreviewProgram(true);

	gs_matrix_push();
	gs_matrix_translate3f(sourceX, sourceY, 0.0f);
	gs_matrix_scale3f(hiScaleX, hiScaleY, 1.0f);

	setRegion(sourceX, sourceY, hiCX, hiCY);
	obs_render_main_texture();
	resetRegion();

	gs_matrix_pop();

	/* ----------- */

	gs_matrix_push();
	gs_matrix_translate3f(sourceX, sourceY, 0.0f);
	gs_matrix_scale3f(hiScaleX, hiScaleY, 1.0f);

	renderVB(solid, window->outerBox, targetCX, targetCY);

	gs_matrix_pop();

	/* ----------- */

	cx = obs_source_get_width(programLabel);
	cy = obs_source_get_height(programLabel);

	gs_matrix_push();
	gs_matrix_translate3f(labelX, labelY, 0.0f);

	drawBox(cx, cy + int(halfCX * 0.015f), 0xD91F1F1F);
	obs_source_video_render(programLabel);

	gs_matrix_pop();

	/* ----------------------------- */

	gs_viewport_pop();
	gs_projection_pop();
}

void OBSProjector::InitRtpSource()
{
	this->m_nRtpSourceCount = 0;
	OBSBasic *main = reinterpret_cast<OBSBasic*>(App()->GetMainWindow());
	auto funcSize = [](obs_scene_t *, obs_sceneitem_t *item, void *param) {
		OBSProjector * lpProjector = reinterpret_cast<OBSProjector*>(param);
		obs_source_t *source = obs_sceneitem_get_source(item);
		uint32_t flags = obs_source_get_output_flags(source);
		int64_t nSceneID = obs_sceneitem_get_id(item);
		// ���û����Ƶ����Դ��־������Ѱ��...
		if ((flags & OBS_SOURCE_VIDEO) == 0)
			return true;
		// �������rtp_source��ǣ�����Ѱ��...
		const char * lpSrcID = obs_source_get_id(source);
		if (astrcmpi(lpSrcID, App()->InteractRtpSource()) != 0)
			return true;
		// ���ﲻ��ֱ�ӵ��� CreateLabel() �ᷢ����������...
		obs_data_t * lpSettings = obs_source_get_settings(source);
		lpProjector->m_MapLabel[nSceneID] = obs_data_get_string(lpSettings, "camera_name");
		obs_data_release(lpSettings);
		// �ۼ�����Դ��������ֱ��ʹ��ͶӰ����...
		lpProjector->m_nRtpSourceCount += 1;
		return true;
	};
	// ����������Ч������Դ���������û��rtp_source����Դ������ʧ��...
	obs_scene_enum_items(main->GetCurrentScene(), funcSize, this);
	// ���rtp����Դ��Ч����Ҫ�½���������������Դ����...
	for (GM_MapLabel::iterator itorItem = m_MapLabel.begin(); itorItem != m_MapLabel.end(); ++itorItem) {
		m_MapSource[itorItem->first] = CreateLabel(itorItem->second, m_nLabelFontSize);
	}
}

bool OBSProjector::RenderRtpSource(uint32_t cx, uint32_t cy)
{
	// ������Ҫ���С�������...
	OBSBasic *main = reinterpret_cast<OBSBasic*>(App()->GetMainWindow());
	int nCol = ceil(sqrt(this->m_nRtpSourceCount * 1.0f));
	int nRow = nCol;
	// �������������Դ => 1��2��...
	if (this->m_nRtpSourceCount == 2) {
		nCol = 2; nRow = 1;
	}
	// ����ÿ������Դ��ʾ�ĸ߶ȺͿ��...
	struct ItemInfo {
		int nWindowRow;
		int nWindowCol;
		int nWindowCX;
		int nWindowCY;
		int nWindowInx;
		OBSProjector * lpProjector;
	} info = { nRow, nCol, (cx-1)/nCol-1, (cy-1)/nRow-1, 0, this };
	auto funcDraw = [](obs_scene_t *, obs_sceneitem_t *item, void *param) {
		ItemInfo * lpQInfo = reinterpret_cast<ItemInfo*>(param);
		obs_source_t *source = obs_sceneitem_get_source(item);
		uint32_t flags = obs_source_get_output_flags(source);
		int64_t nSceneID = obs_sceneitem_get_id(item);
		// ���û����Ƶ����Դ��־������Ѱ��...
		if ((flags & OBS_SOURCE_VIDEO) == 0)
			return true;
		// �������rtp_source��ǣ�����Ѱ��...
		const char * lpSrcID = obs_source_get_id(source);
		if (astrcmpi(lpSrcID, App()->InteractRtpSource()) != 0)
			return true;
		// ��ȡ��ǰ��Ч����Դ�ĳ��ȺͿ�ȵ�����ֵ...
		uint32_t targetCX = std::max(obs_source_get_width(source), 1u);
		uint32_t targetCY = std::max(obs_source_get_height(source), 1u);
		int      newCX, newCY, x, y;
		float    scale;
		// ��������Դ��������Ķ�����������ű�����С...
		GetScaleAndCenterPos(targetCX, targetCY, lpQInfo->nWindowCX, lpQInfo->nWindowCY, x, y, scale);
		newCX = int(scale * float(targetCX));
		newCY = int(scale * float(targetCY));
		// ���ݵ�ǰ�ļ����������У����¼��㶥������...
		x += (lpQInfo->nWindowInx % lpQInfo->nWindowCol) * lpQInfo->nWindowCX; //+((lpQInfo->nWindowInx % lpQInfo->nWindowCol) > 0 ? 2 : 0);
		y += (lpQInfo->nWindowInx / lpQInfo->nWindowCol) * lpQInfo->nWindowCY; //+((lpQInfo->nWindowInx / lpQInfo->nWindowCol) > 0 ? 2 : 0);
		// �Ե�ǰ����Դ������ͼ���ƹ���...
		gs_ortho(0.0f, float(targetCX), 0.0f, float(targetCY), -100.0f, 100.0f);
		gs_set_viewport(x, y, newCX, newCY);
		obs_source_video_render(source);
		// �ۼ���Ч����Դ�ļ�����...
		lpQInfo->nWindowInx += 1;
		// ���ƾ��ο� => ��Ҫ�������ű������¼�����εĿ�Ⱥ͸߶�...
		/*gs_effect_t  *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
		gs_eparam_t  *color = gs_effect_get_param_by_name(solid, "color");
		auto drawBox = [solid, color](float cx, float cy, uint32_t colorVal) {
			gs_effect_set_color(color, colorVal);
			while (gs_effect_loop(solid, "Solid")) {
				gs_draw_sprite(nullptr, 0, (uint32_t)cx, (uint32_t)cy);
			}
		};
		newCX = int(float(lpQInfo->nWindowCX) / scale);
		newCY = int(float(30) / scale);
		drawBox(newCX, newCY, 0xD91F1F1F)*/
		// ���Ҷ�Ӧ�ı���������Դ��ֱ�ӶԱ�������Դ������Ⱦ����...
		int nFontSize = lpQInfo->lpProjector->m_nLabelFontSize;
		GM_MapSource & theMapSource = lpQInfo->lpProjector->m_MapSource;
		GM_MapSource::iterator itorItem = theMapSource.find(nSceneID);
		if (itorItem != theMapSource.end()) {
			obs_source * label = itorItem->second;
			obs_data_t * lpSettings = obs_source_get_settings(label);
			obs_data_t * lpFont = obs_data_get_obj(lpSettings, "font");
			// ������ʾƫ��λ����Ϣ�������������С => ʹ�ô������ű���...
			gs_set_viewport(x + 10, y + 10, newCX, newCY);
			int newFontSize = int(float(nFontSize) / scale);
			// �����������������С��һ�£���Ҫ��������Դ...
			if (newFontSize != obs_data_get_int(lpFont, "size")) {
				obs_data_set_int(lpFont, "size", newFontSize);
				obs_source_update(label, lpSettings);
			}
			// �ͷ����ü�������Ⱦ����Դ...
			obs_data_release(lpFont);
			obs_data_release(lpSettings);
			obs_source_video_render(label);
		}
		return true;
	};
	// ������ǰ��ͼ��ͶӰ...
	gs_viewport_push();
	gs_projection_push();
	// ����������Ч������Դ������ִ������Դ��Ⱦ����...
	obs_scene_enum_items(main->GetCurrentScene(), funcDraw, &info);
	// �ָ���ǰ��ͼ��ͶӰ...
	gs_projection_pop();
	gs_viewport_pop();
	return true;
}

void OBSProjector::OBSRender(void *data, uint32_t cx, uint32_t cy)
{
	OBSProjector *window = reinterpret_cast<OBSProjector*>(data);

	if (!window->ready)
		return;

	OBSBasic *main = reinterpret_cast<OBSBasic*>(App()->GetMainWindow());
	OBSSource source = window->source;

	// �ж��Ƿ���rtp����Դ����ԴͶӰģʽ���������⴦��...
	if (window->m_nRtpSourceCount > 0) {
		window->RenderRtpSource(cx, cy);
		return;
	}

	uint32_t targetCX;
	uint32_t targetCY;
	int      newCX, newCY;
	int      x, y;
	float    scale;

	if (source) {
		targetCX = std::max(obs_source_get_width(source), 1u);
		targetCY = std::max(obs_source_get_height(source), 1u);
	} else {
		struct obs_video_info ovi;
		obs_get_video_info(&ovi);
		targetCX = ovi.base_width;
		targetCY = ovi.base_height;
	}

	GetScaleAndCenterPos(targetCX, targetCY, cx, cy, x, y, scale);

	newCX = int(scale * float(targetCX));
	newCY = int(scale * float(targetCY));

	gs_viewport_push();
	gs_projection_push();
	gs_ortho(0.0f, float(targetCX), 0.0f, float(targetCY), -100.0f, 100.0f);
	gs_set_viewport(x, y, newCX, newCY);

	if (window->type == ProjectorType::Preview && main->IsPreviewProgramMode()) {
		OBSSource curSource = main->GetCurrentSceneSource();
		if (source != curSource) {
			obs_source_dec_showing(source);
			obs_source_inc_showing(curSource);
			source = curSource;
		}
	}

	if (source) {
		obs_source_video_render(source);
	} else {
		obs_render_main_texture();
	}

	gs_projection_pop();
	gs_viewport_pop();
}

void OBSProjector::OBSSourceRemoved(void *data, calldata_t *params)
{
	OBSProjector *window = reinterpret_cast<OBSProjector*>(data);

	window->deleteLater();

	UNUSED_PARAMETER(params);
}

static int getSourceByPosition(int x, int y)
{
	struct obs_video_info ovi;
	obs_get_video_info(&ovi);
	float ratio = float(ovi.base_width) / float(ovi.base_height);

	QWidget *rec  = QApplication::activeWindow();
	int     cx    = rec->width();
	int     cy    = rec->height();
	int     minX  = 0;
	int     minY  = 0;
	int     maxX  = cx;
	int     maxY  = cy;
	int     halfX = cx / 2;
	int     halfY = cy / 2;
	int     pos   = -1;

	switch (multiviewLayout) {
	case VERTICAL_LEFT:
		if (float(cx) / float(cy) > ratio) {
			int validX = cy * ratio;
			maxX = halfX + (validX / 2);
		} else {
			int validY = cx / ratio;
			minY = halfY - (validY / 2);
			maxY = halfY + (validY / 2);
		}

		minX = halfX;

		if (x < minX || x > maxX || y < minY || y > maxY)
			break;

		pos = 2 * ((y - minY) / ((maxY - minY) / 4));
		if (x > minX + ((maxX - minX) / 2))
			pos++;
		break;
	case VERTICAL_RIGHT:
		if (float(cx) / float(cy) > ratio) {
			int validX = cy * ratio;
			minX = halfX - (validX / 2);
		} else {
			int validY = cx / ratio;
			minY = halfY - (validY / 2);
			maxY = halfY + (validY / 2);
		}

		maxX = halfX;

		if (x < minX || x > maxX || y < minY || y > maxY)
			break;

		pos = 2 * ((y - minY) / ((maxY - minY) / 4));
		if (x > minX + ((maxX - minX) / 2))
			pos++;
		break;
	case HORIZONTAL_BOTTOM:
		if (float(cx) / float(cy) > ratio) {
			int validX = cy * ratio;
			minX = halfX - (validX / 2);
			maxX = halfX + (validX / 2);
		} else {
			int validY = cx / ratio;
			minY = halfY - (validY / 2);
		}

		maxY = halfY;

		if (x < minX || x > maxX || y < minY || y > maxY)
			break;

		pos = (x - minX) / ((maxX - minX) / 4);
		if (y > minY + ((maxY - minY) / 2))
			pos += 4;
		break;
	default: // HORIZONTAL_TOP
		if (float(cx) / float(cy) > ratio) {
			int validX = cy * ratio;
			minX = halfX - (validX / 2);
			maxX = halfX + (validX / 2);
		} else {
			int validY = cx / ratio;
			maxY = halfY + (validY / 2);
		}

		minY = halfY;

		if (x < minX || x > maxX || y < minY || y > maxY)
			break;

		pos = (x - minX) / ((maxX - minX) / 4);
		if (y > minY + ((maxY - minY) / 2))
			pos += 4;
	}

	return pos;
}

void OBSProjector::mouseDoubleClickEvent(QMouseEvent *event)
{
	OBSQTDisplay::mouseDoubleClickEvent(event);

	if (!config_get_bool(GetGlobalConfig(), "BasicWindow",
			"TransitionOnDoubleClick"))
		return;

	OBSBasic *main = (OBSBasic*)obs_frontend_get_main_window();
	if (!main->IsPreviewProgramMode())
		return;

	if (event->button() == Qt::LeftButton) {
		int pos = getSourceByPosition(event->x(), event->y());
		if (pos < 0)
			return;
		OBSSource src = OBSGetStrongRef(multiviewScenes[pos]);
		if (!src)
			return;

		if (main->GetProgramSource() != src)
			main->TransitionToScene(src);
	}
}

void OBSProjector::mousePressEvent(QMouseEvent *event)
{
	OBSQTDisplay::mousePressEvent(event);

	if (event->button() == Qt::RightButton) {
		QMenu popup(this);
		popup.addAction(QTStr("Close"), this, SLOT(EscapeTriggered()));
		popup.exec(QCursor::pos());
	}

	if (event->button() == Qt::LeftButton) {
		int pos = getSourceByPosition(event->x(), event->y());
		if (pos < 0)
			return;
		OBSSource src = OBSGetStrongRef(multiviewScenes[pos]);
		if (!src)
			return;

		OBSBasic *main = (OBSBasic*)obs_frontend_get_main_window();
		if (main->GetCurrentSceneSource() != src)
			main->SetCurrentScene(src, false);
	}
}

void OBSProjector::EscapeTriggered()
{
	deleteLater();
}

void OBSProjector::UpdateMultiview()
{
	for (OBSWeakSource &val : multiviewScenes)
		val = nullptr;
	for (OBSSource &val : multiviewLabels)
		val = nullptr;

	struct obs_video_info ovi;
	obs_get_video_info(&ovi);

	uint32_t h = ovi.base_height;

	struct obs_frontend_source_list scenes = {};
	obs_frontend_get_scenes(&scenes);

	int curIdx = 0;

	multiviewLabels[0] = CreateLabel(Str("StudioMode.Preview"), h / (2 * 9.81));
	multiviewLabels[1] = CreateLabel(Str("StudioMode.Program"), h / (2 * 9.81));

	for (size_t i = 0; i < scenes.sources.num && curIdx < 8; i++) {
		obs_source_t *src = scenes.sources.array[i];
		OBSData data = obs_source_get_private_settings(src);
		obs_data_release(data);

		obs_data_set_default_bool(data, "show_in_multiview", true);
		if (!obs_data_get_bool(data, "show_in_multiview"))
			continue;

		multiviewScenes[curIdx] = OBSGetWeakRef(src);
		obs_source_inc_showing(src);

		std::string name;
		name += std::to_string(curIdx + 1);
		name += " - ";
		name += obs_source_get_name(src);

		multiviewLabels[curIdx + 2] = CreateLabel(name.c_str(), h / (3 * 9.81));

		curIdx++;
	}

	obs_frontend_source_list_free(&scenes);

	const char *multiviewLayoutText = config_get_string(GetGlobalConfig(),
			"BasicWindow", "MultiviewLayout");

	if (astrcmpi(multiviewLayoutText, "horizontalbottom") == 0)
		multiviewLayout = HORIZONTAL_BOTTOM;
	else if (astrcmpi(multiviewLayoutText, "verticalleft") == 0)
		multiviewLayout = VERTICAL_LEFT;
	else if (astrcmpi(multiviewLayoutText, "verticalright") == 0)
		multiviewLayout = VERTICAL_RIGHT;
	else
		multiviewLayout = HORIZONTAL_TOP;
}

void OBSProjector::UpdateProjectorTitle(QString name)
{
	projectorTitle = name;

	QString title = nullptr;
	switch (type) {
	case ProjectorType::Scene:
		title = QTStr("SceneWindow") + " - " + name;
		break;
	case ProjectorType::Source:
		title = QTStr("SourceWindow") + " - " + name;
		break;
	default:
		title = name;
		break;
	}

	setWindowTitle(title);
}

OBSSource OBSProjector::GetSource()
{
	return source;
}

ProjectorType OBSProjector::GetProjectorType()
{
	return type;
}

int OBSProjector::GetMonitor()
{
	return savedMonitor;
}

void OBSProjector::UpdateMultiviewProjectors()
{
	obs_enter_graphics();
	updatingMultiview = true;
	obs_leave_graphics();

	for (auto &projector : multiviewProjectors)
		projector->UpdateMultiview();

	obs_enter_graphics();
	updatingMultiview = false;
	obs_leave_graphics();
}

void OBSProjector::RenameProjector(QString oldName, QString newName)
{
	for (auto &projector : windowedProjectors)
		if (projector->projectorTitle == oldName)
			projector->UpdateProjectorTitle(newName);
}
