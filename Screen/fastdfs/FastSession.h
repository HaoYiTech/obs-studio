
#pragma once

#include <fastdfs.h>
#include <QTcpSocket>
#include <string>
#include "json.h"

using namespace std;

class CFastSession : public QObject {
	Q_OBJECT
public:
	CFastSession();
	virtual ~CFastSession();
public:
	int  GetErrCode() { return m_nErrorCode; }
	bool IsConnected() { return m_bIsConnected; }
	bool InitSession(const char * lpszAddr, int nPort);
protected:
	void closeSocket();
protected slots:
	virtual void onConnected() = 0;
	virtual void onReadyRead() = 0;
	virtual void onDisConnected() = 0;
	virtual void onBytesWritten(qint64 nBytes) = 0;
	virtual void onError(QAbstractSocket::SocketError nError) = 0;
protected:
	int              m_nErrorCode;              // �������...
	bool             m_bIsConnected;			// �Ƿ������ӱ�־...
	QTcpSocket   *   m_TCPSocket;				// TCP�׽���...
	std::string      m_strAddress;				// ���ӵ�ַ...
	std::string      m_strRecv;					// ��������...
	int              m_nPort;					// ���Ӷ˿�...
};

class CTrackerSession : public CFastSession {
	Q_OBJECT
public:
	CTrackerSession();
	virtual ~CTrackerSession();
public:
	StorageServer   &   GetStorageServer() { return m_NewStorage; }
protected slots:
	void onConnected() override;
	void onReadyRead() override;
	void onDisConnected() override;
	void onBytesWritten(qint64 nBytes) override;
	void onError(QAbstractSocket::SocketError nError) override;
private:
	void SendCmd(char inCmd);
	void doStorageWanAddr();
private:
	TrackerHeader		m_TrackerCmd;				// Tracker-Header-Cmd...
	StorageServer		m_NewStorage;				// ��ǰ��Ч�Ĵ洢������...
	FDFSGroupStat	*	m_lpGroupStat;				// group�б�ͷָ��...
	int					m_nGroupCount;				// group����...
};

class CStorageSession : public CFastSession {
	Q_OBJECT
public:
	CStorageSession();
	virtual ~CStorageSession();
public:
	bool IsCanReBuild() { return m_bCanReBuild; }
	bool ReBuildSession(StorageServer * lpStorage, const char * lpszFilePath);
protected slots:
	void onConnected() override;
	void onReadyRead() override;
	void onDisConnected() override;
	void onBytesWritten(qint64 nBytes) override;
	void onError(QAbstractSocket::SocketError nError) override;
private:
	bool    SendNextPacket(int64_t inLastBytes);
	bool	SendCmdHeader();
	void    CloseUpFile();
private:
	enum {
		kPackSize = 64 * 1024,			// ���ݰ���С => Խ�󣬷�������Խ��(ÿ�뷢��64��) => 8KB(4Mbps)|64KB(32Mbps)|128KB(64Mbps)
	};
private:
	StorageServer	m_NewStorage;		// ��ǰ��Ч�Ĵ洢������...
	std::string     m_strFilePath;		// ���ڴ�����ļ�ȫ·��...
	std::string     m_strExtend;		// ���ڴ�����ļ���չ��...
	std::string		m_strCurData;		// ��ǰ���ڷ��͵����ݰ�����...
	int64_t         m_llFileSize;		// ���ڴ�����ļ��ܳ���...
	int64_t         m_llLeftSize;		// ʣ�������ܳ���...
	FILE       *    m_lpFile;			// ���ڴ�����ļ����...
	bool            m_bCanReBuild;		// �ܷ�����ؽ���־...
};

// ��������ת�����������ĻỰ����...
class CRemoteSession : public CFastSession {
	Q_OBJECT
public:
	CRemoteSession();
	virtual ~CRemoteSession();
signals:
	void doTriggerConnected();
public:
	bool doSendScreenSnap(const char * lpData, int nSize);
	bool IsCanReBuild() { return m_bCanReBuild; }
	bool doSendOnLineCmd();
protected slots:
	void onConnected() override;
	void onReadyRead() override;
	void onDisConnected() override;
	void onBytesWritten(qint64 nBytes) override;
	void onError(QAbstractSocket::SocketError nError) override;
private slots:
	void DeferredSend();
private:
	bool doSendCommonCmd(int nCmdID, const char * lpJsonPtr = NULL, int nJsonSize = 0);
	bool doParseJson(const char * lpData, int nSize, Json::Value & outValue);
	bool doCmdScreenPacket(const char * lpData, int nSize);
	bool doCmdScreenOnLine(const char * lpData, int nSize);
	bool doCmdScreenLogin(const char * lpData, int nSize);
	bool SendData(const char * lpDataPtr, int nDataSize);
	bool doSendOnePacket();
	bool SendLoginCmd();
private:
	enum {
		kPackSize = 8 * 1024,			// ���ݰ���С => Խ�󣬷�������Խ��(ÿ�뷢��64��) => 8KB(4Mbps)|64KB(32Mbps)|128KB(64Mbps)
	};
private:
	int         m_nRemoteTCPSocketFD = 0;       // Զ�̷������ϵ��׽���...
	bool        m_bCanReBuild = false;			// �ܷ�����ؽ���־...
	bool        m_bHasLogin = false;            // �Ƿ��Ѿ��ɹ���¼...
	string      m_strSnapJpg;                   // ��ͼ������...
};
