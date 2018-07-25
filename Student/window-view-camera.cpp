
#include <QPainter>
#include "student-app.h"
#include "pull-thread.h"
#include "push-thread.h"

#include "window-view-left.hpp"
#include "window-view-camera.hpp"

#define LINE_SPACE_PIXEL		6
#define LEFT_SPACE_PIXEL		20
#define RIGHT_SPACE_PIXEL		20
#define TITLE_WINDOW_HEIGHT		24
#define TITLE_FONT_HEIGHT		10
#define TITLE_TEXT_COLOR		QColor(255,255,255)
#define TITLE_BK_COLOR			QColor(96, 123, 189)//QColor(0, 122, 204)
#define WINDOW_BK_COLOR			QColor(32, 32, 32)
#define FOCUS_BK_COLOR			QColor(255,255, 0)
#define STATUS_TEXT_COLOR		QColor(20, 220, 20)

CViewCamera::CViewCamera(QWidget *parent, int nDBCameraID)
  : OBSQTDisplay(parent, 0)
  , m_nDBCameraID(nDBCameraID)
  , m_lpPushThread(NULL)
  , m_lpViewLeft(NULL)
{
	ASSERT(m_nDBCameraID > 0 && parent != NULL);
	m_lpViewLeft = qobject_cast<CViewLeft*>(parent);
	ASSERT(parent != NULL && m_lpViewLeft != NULL);
	// ��ʼ��������ɫ��ԭʼ����...
	m_bkColor = WINDOW_BK_COLOR;
	m_rcRenderRect.setRect(0, 0, 0, 0);
	// ���ô��ڱ����������С...
	QFont theFont = this->font();
	theFont.setPointSize(TITLE_FONT_HEIGHT);
	this->setFont(theFont);
}

CViewCamera::~CViewCamera()
{
	// �ͷ��������ݹ�����...
	if (m_lpPushThread != NULL) {
		delete m_lpPushThread;
		m_lpPushThread = NULL;
	}
	// ����Լ������Ǹ����㴰�ڣ���Ҫ����...
	App()->doResetFocus(this);
}

void CViewCamera::paintEvent(QPaintEvent *event)
{
	// ���Ʊ���������...
	this->DrawTitleArea();
	// ����������֮��Ĵ�������...
	this->DrawRenderArea();
	// ��ʾͨ��������״̬��Ϣ...
	this->DrawStatusText();
	//QWidget::paintEvent(event);
}

void CViewCamera::mousePressEvent(QMouseEvent *event)
{
	// �������¼���� => �������¼�...
	if (event->button() == Qt::LeftButton) {
		this->doCaptureFocus();
	}
	return QWidget::mousePressEvent(event);
}

void CViewCamera::DrawTitleArea()
{
	QPainter painter(this);
	if (!painter.isActive())
		return;
	// ��ȡ�������ڿͻ������δ�С...
	QRect & rcRect = this->rect();
	// ������������������ɫ...
	painter.fillRect(1, 1, rcRect.width()-2, TITLE_WINDOW_HEIGHT, TITLE_BK_COLOR);
	// ���ñ�����������ɫ...
	painter.setPen(TITLE_TEXT_COLOR);
	// ������������� => �ر�ע�⣺���������½�����...
	int nPosX = 10;
	int nPosY = TITLE_FONT_HEIGHT + (TITLE_WINDOW_HEIGHT - TITLE_FONT_HEIGHT) / 2 + 1;
	// �Ȼ�ȡ����ͷ�ı������ƣ��ٽ����ַ�������...
	QString strTitleContent = App()->GetCameraName(m_nDBCameraID);
	QString strTitleText = QString("%1%2 - %3").arg(QStringLiteral("ID��")).arg(m_nDBCameraID).arg(strTitleContent);
	painter.drawText(nPosX, nPosY, strTitleText);
}

void CViewCamera::DrawRenderArea()
{
	QPainter painter(this);
	if (!painter.isActive())
		return;
	QRect rcRect = this->rect();
	// ���������α߿�...
	rcRect.adjust(0, 0, -1, -1);
	painter.setPen(QPen(m_bkColor, 1));
	painter.drawRect(rcRect);
	// ע�⣺������ƾ��ε���ʼ�����-1��ʼ��QSplitter�Ķ���������(1,1)...
	// ���Ʊ߿����û�����ɫ�����ž�������...
	painter.setPen(QPen(m_bFocus ? FOCUS_BK_COLOR : TITLE_BK_COLOR, 1));
	rcRect.adjust(1, 1, -1, -1);
	painter.drawRect(rcRect);
	// �����Ⱦ�����ο�...
	rcRect.setTop(TITLE_WINDOW_HEIGHT);
	rcRect.adjust(1, 0, 0, 0);
	painter.fillRect(rcRect, m_bkColor);
	// ע�⣺����ʹ��PS��ͼ��Ŵ�ȷ�ϴ�С...
	// ���滭��ʵ����Ⱦ��������...
	m_rcRenderRect = rcRect;
}

void CViewCamera::DrawStatusText()
{
	QPainter painter(this);
	if (!painter.isActive())
		return;
	// ������������ͻ�����ɫ...
	QRect rcRect = this->rect();
	rcRect.setTop(TITLE_WINDOW_HEIGHT);
	painter.setPen(QPen(STATUS_TEXT_COLOR));
	QFontMetrics & fontMetrics = painter.fontMetrics();
	// ���㶥�����...
	int nLineSpace = LINE_SPACE_PIXEL;
	int nTopSpace = (rcRect.height() - TITLE_FONT_HEIGHT * 5 - nLineSpace * 4) / 2 - 20;
	// ��ʾͨ��״̬������ => �ر�ע�⣺���������½�����...
	int xNamePos = rcRect.left() + LEFT_SPACE_PIXEL;
	int yNamePos = rcRect.top() + nTopSpace + TITLE_FONT_HEIGHT;
	QString strTitleName = QTStr("Camera.Window.StatName");
	painter.drawText(xNamePos, yNamePos, strTitleName);
	// ��ʾͨ��״̬��������...
	int xDataPos = xNamePos + fontMetrics.width(strTitleName);
	int yDataPos = yNamePos;
	QString strDataValue = QTStr(this->IsCameraLogin() ? "Camera.Window.OnLine" : "Camera.Window.OffLine");
	painter.drawText(xDataPos, yDataPos, strDataValue);
	// ��ʾ������ַ������ => ע�������½�����...
	strTitleName = QTStr("Camera.Window.PullName");
	yNamePos += TITLE_FONT_HEIGHT + nLineSpace;
	painter.drawText(xNamePos, yNamePos, strTitleName);
	// �������ֶ������Զ�����...
	QRect rcText;
	QTextOption txtOption(Qt::AlignLeft);
	txtOption.setWrapMode(QTextOption::WrapAnywhere);
	// ����������ַ��ʾ����λ�� => ע�������½�����...
	rcText.setTop(yNamePos - TITLE_FONT_HEIGHT - 2);
	rcText.setLeft(xNamePos + fontMetrics.width(strTitleName));
	rcText.setRight(rcRect.right() - RIGHT_SPACE_PIXEL);
	rcText.setBottom(rcText.top() + TITLE_FONT_HEIGHT * 4);
	// ��ȡ������ַ�ľ�������...
	strDataValue = App()->GetCameraPullUrl(m_nDBCameraID);
	// ��ʾ������ַ��Ϣ�����ۼ���һ����ʾλ��...
	painter.drawText(rcText, strDataValue, txtOption);
	yNamePos += TITLE_FONT_HEIGHT * 4 + nLineSpace;
	// ��ʾ�������ʵı�����...
	strTitleName = QTStr("Camera.Window.PullRate");
	painter.drawText(xNamePos, yNamePos, strTitleName);
	// ��ʾ�������ʵľ�������...
	xDataPos = xNamePos + fontMetrics.width(strTitleName);
	yDataPos = yNamePos;
	strDataValue = this->GetRecvPullRate();
	painter.drawText(xDataPos, yDataPos, strDataValue);
	yNamePos += TITLE_FONT_HEIGHT + nLineSpace;
	// ��ʾ������ַ������...
	strTitleName = QTStr("Camera.Window.PushName");
	painter.drawText(xNamePos, yNamePos, strTitleName);
	// ��ʾ������ַ��������...
	xDataPos = xNamePos + fontMetrics.width(strTitleName);
	yDataPos = yNamePos;
	strDataValue = this->GetStreamPushUrl();
	painter.drawText(xDataPos, yDataPos, strDataValue);
	yNamePos += TITLE_FONT_HEIGHT + nLineSpace;
	// ��ʾ�������ʱ�����...
	strTitleName = QTStr("Camera.Window.PushRate");
	painter.drawText(xNamePos, yNamePos, strTitleName);
	// ��ʾ�������ʾ�������...
	xDataPos = xNamePos + fontMetrics.width(strTitleName);
	yDataPos = yNamePos;
	strDataValue = this->GetSendPushRate();
	painter.drawText(xDataPos, yDataPos, strDataValue);
}

QString CViewCamera::GetRecvPullRate()
{
	// ��ȡ�������ʵľ������� => ֱ�Ӵ��������������л�ȡ...
	int nRecvKbps = ((m_lpPushThread != NULL) ? m_lpPushThread->GetRecvPullKbps() : 0);
	// �������Ϊ��������Ҫֹͣͨ����ͬʱ�����²˵�...
	if ( nRecvKbps < 0 ) {
		// ��ֹͣͨ��...
		this->doCameraStop();
		// ������㴰�ڻ��ǵ�ǰ���ڣ���Ҫ���¹������˵�״̬...
		if (m_lpViewLeft->GetFocusID() == m_nDBCameraID) {
			emit m_lpViewLeft->enableCameraStart(true);
			emit m_lpViewLeft->enableCameraStop(false);
		}
		// ���ý�������...
		nRecvKbps = 0;
	}
	// ���ؽ������ʵ��ַ�������...
	return QString("%1 Kbps").arg(nRecvKbps);
}

QString CViewCamera::GetSendPushRate()
{
	QString strRate("0 Kbps");
	return strRate;
}

QString CViewCamera::GetStreamPushUrl()
{
	//QString strRate("udp://edu.ihaoyi.cn:5252");
	QString strRate = QTStr("Camera.Window.None");
	return strRate;
}

bool CViewCamera::doCameraStart()
{
	// �������������Ч��ֱ�ӷ���...
	if (m_lpPushThread != NULL)
		return true;
	// �ؽ��������������...
	ASSERT(m_lpPushThread == NULL);
	m_lpPushThread = new CPushThread(this);
	// ��ʼ��ʧ�ܣ�ֱ�ӷ���...
	if (!m_lpPushThread->InitThread()) {
		delete m_lpPushThread;
		m_lpPushThread = NULL;
		return false;
	}
	// ������ʾ״̬...
	this->update();
	return true;
}

bool CViewCamera::doCameraStop()
{
	// ֱ��ɾ������������...
	if (m_lpPushThread != NULL) {
		delete m_lpPushThread;
		m_lpPushThread = NULL;
	}
	// ������ʾ״̬...
	this->update();
	return true;
}