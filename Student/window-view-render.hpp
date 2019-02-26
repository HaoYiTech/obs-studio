#pragma once

#include "qt-display.hpp"
#include <util/threading.h>

class CViewRender : public OBSQTDisplay {
	Q_OBJECT
public:
	CViewRender(QString & strDefNotice, int nFontSize, QWidget *parent, Qt::WindowFlags flags = 0);
	virtual ~CViewRender();
public:
	pthread_mutex_t * GetMutexExAudio() { return &m_MutexExAudio; }
	void    doUpdateNotice(const QString & strNotice, bool bIsDrawImage = false);
	bool    IsChangeScreen() { return m_bIsChangeScreen; }
	bool    IsDrawImage() { return m_bIsDrawImage; }
	QRect & GetRenderRect() { return m_rcRenderRect; }
	HWND    GetRenderHWnd() { return m_hRenderWnd; }
	void    doResizeRect(const QRect & rcInRect);
	bool    GetAndResetRenderFlag();
	void    onFullScreenAction();
protected:
	void    paintEvent(QPaintEvent *event) override;
	void    keyPressEvent(QKeyEvent *event) override;
	void    mouseDoubleClickEvent(QMouseEvent *event) override;
private:
	HWND               m_hRenderWnd;        // 渲染画面窗口句柄...
	QRect              m_rcNoramlRect;      // 窗口的全屏前的矩形区域...
	QRect              m_rcRenderRect;      // 窗口画面渲染的矩形区域...
	bool               m_bRectChanged;      // 渲染矩形区发生变化...
	bool               m_bIsChangeScreen;   // 正在处理全屏或还原窗口...
	bool               m_bIsDrawImage;      // 是否正在绘制图片标志...
	QString            m_strNoticeText;     // 显示的提示文字信息...
	pthread_mutex_t    m_MutexExAudio;      // 扩展音频互斥对象...
};
