#pragma once

#include "qt-display.hpp"

class CViewTeacher;
class CViewRender : public OBSQTDisplay {
	Q_OBJECT
public:
	CViewRender(QWidget *parent, Qt::WindowFlags flags = 0);
	virtual ~CViewRender();
public:
	QRect & GetRenderRect() { return m_rcRenderRect; }
	HWND    GetRenderHWnd() { return m_hRenderWnd; }
	void    doResizeRect(const QRect & rcInRect);
	void    onFullScreenAction();
protected:
	void    paintEvent(QPaintEvent *event) override;
	void    keyPressEvent(QKeyEvent *event) override;
	void    mouseDoubleClickEvent(QMouseEvent *event) override;
private:
	CViewTeacher   *   m_lpViewTeacher;     // �����ڶ���...
	HWND               m_hRenderWnd;        // ��Ⱦ���洰�ھ��...
	QRect              m_rcNoramlRect;      // ���ڵ�ȫ��ǰ�ľ�������...
	QRect              m_rcRenderRect;      // ���ڻ�����Ⱦ�ľ�������...
};
