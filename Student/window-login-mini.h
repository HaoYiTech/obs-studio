
#pragma once

#include "HYCommon.h"
#include "ui_LoginMini.h"
#include "json.h"

#include <string>
#include <functional>
#include <QPointer>
#include <QNetworkAccessManager>

using namespace std;

class CCenterSession;
class CLoginMini : public QDialog
{
    Q_OBJECT
public:
	CLoginMini(QWidget *parent = NULL);
    ~CLoginMini();
public:
	int  GetDBUserID() { return m_nDBUserID; }
	int  GetDBRoomID() { return m_nDBRoomID; }
signals:
	void doTriggerMiniSuccess();
protected:
	void loadStyleSheet(const QString &sheetName);
private:
	virtual void paintEvent(QPaintEvent *event);
	virtual void mousePressEvent(QMouseEvent *event);
	virtual void mouseMoveEvent(QMouseEvent *event);
	virtual void mouseReleaseEvent(QMouseEvent *event);
private slots:
	void onTriggerTcpConnect();
	void onButtonMinClicked();
	void onButtonCloseClicked();
	void onReplyFinished(QNetworkReply *reply);
	void onTriggerBindMini(int nUserID, int nBindCmd, int nRoomID);
private:
	void initWindow();
	bool doCheckOnLine();
	void doCheckSession();
	void doWebGetCenterAddr();
	void doTcpConnCenterAddr();
	void doWebGetMiniToken();
	void doWebGetMiniQRCode();
	void doWebGetMiniUserInfo();
	void doWebGetMiniLoginRoom();
	void timerEvent(QTimerEvent * inEvent);
	void onProcCenterAddr(QNetworkReply *reply);
	void onProcMiniToken(QNetworkReply *reply);
	void onProcMiniQRCode(QNetworkReply *reply);
	void onProcMiniUserInfo(QNetworkReply *reply);
	void onProcMiniLoginRoom(QNetworkReply *reply);
	bool parseJson(string & inData, Json::Value & outValue, bool bIsWeiXin);
private:
	enum {
		kQRCodeWidth = 280,	// С�����ά�����
	};
	enum LOGIN_CMD {
		kNoneCmd        = 0,	// û������...
		kScanCmd        = 1,	// ɨ��ɹ�...
		kSaveCmd        = 2,	// ȷ�ϵ�¼...
		kCancelCmd      = 3,	// ȡ����¼...
	} m_eLoginCmd;
	enum {
		kCenterAddr     = 0,  // ��ȡ���ķ�������TCP��ַ�Ͷ˿�...
		kCenterConn     = 1,  // �������ķ�������TCP��ַ�Ͷ˿�...
		kMiniToken      = 2,  // ����΢�ŷ�������ȡaccess_token...
		kMiniQRCode     = 3,  // ����΢�ŷ�������ȡ����ʾС������...
		kMiniUserInfo   = 4,  // ��ȡ��¼�û�����ϸ��Ϣ...
		kMiniLoginRoom  = 5,  // ��ʽ��¼ָ���������...
	} m_eMiniState;
private:
	Ui::LoginMini  *  ui;
	QPoint	          m_startMovePos;
	bool	          m_isPressed = false;
	string            m_strCenterTcpAddr;       // ���ķ�������TCP��ַ...
	int               m_nCenterTcpPort;         // ���ķ�������TCP�˿�...
	int               m_nTcpSocketFD;           // ���ķ������ϵ��׽���...
	uint32_t          m_uTcpTimeID;             // ���ķ������ϵ�ʱ���ʶ...
	int               m_nDBUserID;              // ���ķ������ϵ����ݿ��û����...
	int               m_nDBRoomID;              // ���ķ�������ѡ��ķ�����...
	int               m_nOnLineTimer;           // ���ķ��������߼��ʱ��...
	int               m_nCenterTimer;           // ���ĻỰ�����ؽ����ʱ��...
	string            m_strMiniToken;			// ��ȡ����access_tokenֵ...
	string            m_strMiniPath;			// ��ȡ����С������Ӧҳ��...
	QMovie     *      m_lpMovieGif;             // GIF��������...
	QLabel     *      m_lpLoadBack;             // GIF��������...
	QPixmap           m_QPixQRCode;				// ��ȡ����С������ͼƬ����...
	QString           m_strQRNotice;            // ��ʾ״̬��Ϣ�ַ�������...
	QString           m_strScan;                // ɨ���������ʾ����Ϣ...
	QString           m_strVer;                 // ��ʾ�汾��Ϣ��������...
	QNetworkAccessManager    m_objNetManager;	// QT �����������...
	QPointer<CCenterSession> m_CenterSession;   // For UDP-Center
};