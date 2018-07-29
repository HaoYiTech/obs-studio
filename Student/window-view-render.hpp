#pragma once

#include "qt-display.hpp"

class CViewTeacher;
class CViewRender : public OBSQTDisplay {
	Q_OBJECT
public:
	CViewRender(QWidget *parent, Qt::WindowFlags flags = 0);
	virtual ~CViewRender();
public:
	bool    IsChangeScreen() { return m_bIsChangeScreen; }
	QRect & GetRenderRect() { return m_rcRenderRect; }
	HWND    GetRenderHWnd() { return m_hRenderWnd; }
	void    doResizeRect(const QRect & rcInRect);
	bool    GetAndResetRenderFlag();
	void    onFullScreenAction();
protected:
	void    paintEvent(QPaintEvent *event) override;
	void    keyPressEvent(QKeyEvent *event) override;
	void	timerEvent(QTimerEvent * inEvent) override;
	void    mouseDoubleClickEvent(QMouseEvent *event) override;
private:
	CViewTeacher   *   m_lpViewTeacher;     // 父窗口对象...
	HWND               m_hRenderWnd;        // 渲染画面窗口句柄...
	QRect              m_rcNoramlRect;      // 窗口的全屏前的矩形区域...
	QRect              m_rcRenderRect;      // 窗口画面渲染的矩形区域...
	bool               m_bRectChanged;      // 渲染矩形区发生变化...
	bool               m_bIsChangeScreen;   // 正在处理全屏或还原窗口...
	int                m_nStateTimer;       // 更新状态时钟...
};
