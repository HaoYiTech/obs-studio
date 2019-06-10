#pragma once

#include <map>
#include <obs.hpp>
#include "qt-display.hpp"

using namespace std;

typedef	map<int64_t, const char*> GM_MapLabel;
typedef	map<int64_t, OBSSource>	  GM_MapSource;

enum class ProjectorType {
	Source,
	Scene,
	Preview,
	StudioProgram,
	Multiview
};

class QMouseEvent;

class OBSProjector : public OBSQTDisplay {
	Q_OBJECT

private:
	OBSSource source;
	OBSSignal removedSignal;

	static void OBSRenderMultiview(void *data, uint32_t cx, uint32_t cy);
	static void OBSRender(void *data, uint32_t cx, uint32_t cy);
	static void OBSSourceRemoved(void *data, calldata_t *params);

	void mousePressEvent(QMouseEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *event) override;

	int savedMonitor;
	bool isWindow;
	QString projectorTitle;
	ProjectorType type = ProjectorType::Source;
	OBSWeakSource multiviewScenes[8];
	OBSSource     multiviewLabels[10];
	gs_vertbuffer_t *outerBox = nullptr;
	gs_vertbuffer_t *innerBox = nullptr;
	gs_vertbuffer_t *leftVLine = nullptr;
	gs_vertbuffer_t *rightVLine = nullptr;
	gs_vertbuffer_t *leftLine = nullptr;
	gs_vertbuffer_t *topLine = nullptr;
	gs_vertbuffer_t *rightLine = nullptr;
	bool ready = false;

	int m_nLabelFontSize = 30;
	int m_nRtpSourceCount = 0;
	GM_MapSource m_MapSource;
	GM_MapLabel  m_MapLabel;

	void InitRtpSource();
	void UpdateMultiview();
	void UpdateProjectorTitle(QString name);

private slots:
	void EscapeTriggered();

public:
	OBSProjector(QWidget *widget, obs_source_t *source_, int monitor,
			QString title, ProjectorType type_);
	~OBSProjector();

	void Init();
	bool RenderRtpSource(uint32_t cx, uint32_t cy);

	int GetMonitor();
	OBSSource GetSource();
	ProjectorType GetProjectorType();
	static void UpdateMultiviewProjectors();
	static void RenameProjector(QString oldName, QString newName);
};
