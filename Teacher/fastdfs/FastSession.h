
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
	bool             m_bIsConnected;			// 是否已连接标志...
	QTcpSocket   *   m_TCPSocket;				// TCP套接字...
	std::string      m_strAddress;				// 链接地址...
	std::string      m_strRecv;					// 网络数据...
	int              m_nPort;					// 链接端口...
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
private:
	TrackerHeader		m_TrackerCmd;				// Tracker-Header-Cmd...
	StorageServer		m_NewStorage;				// 当前有效的存储服务器...
	FDFSGroupStat	*	m_lpGroupStat;				// group列表头指针...
	int					m_nGroupCount;				// group数量...
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
		kPackSize = 64 * 1024,			// 数据包大小 => 越大，发送码流越高(每秒发送64次) => 8KB(4Mbps)|64KB(32Mbps)|128KB(64Mbps)
	};
private:
	StorageServer	m_NewStorage;		// 当前有效的存储服务器...
	std::string     m_strFilePath;		// 正在处理的文件全路径...
	std::string     m_strExtend;		// 正在处理的文件扩展名...
	std::string		m_strCurData;		// 当前正在发送的数据包内容...
	int64_t         m_llFileSize;		// 正在处理的文件总长度...
	int64_t         m_llLeftSize;		// 剩余数据总长度...
	FILE       *    m_lpFile;			// 正在处理的文件句柄...
	bool            m_bCanReBuild;		// 能否进行重建标志...
};

// 与命令中转服务器交互的会话对象...
class CRemoteSession : public CFastSession {
	Q_OBJECT
public:
	CRemoteSession(int nDBCameraID, int nSceneItemID);
	virtual ~CRemoteSession();
signals:
	void doTriggerCameraList(Json::Value & value);
	void doTriggerUdpLogout(int tmTag, int idTag, int nDBCameraID);
	void doTriggerRtpSource(int nSceneItemID, int nDBCameraID, bool bIsCameraOnLine);
public:
	bool IsCanReBuild() { return m_bCanReBuild; }
	bool doSendOnLineCmd();
	bool doSendCameraOnLineListCmd();
	bool doSendCameraLiveStartCmd(int nDBCameraID, int nSceneItemID);
protected slots:
	void onConnected() override;
	void onReadyRead() override;
	void onDisConnected() override;
	void onBytesWritten(qint64 nBytes) override;
	void onError(QAbstractSocket::SocketError nError) override;
private:
	bool doSendCommonCmd(int nCmdID, const char * lpJsonPtr = NULL, int nJsonSize = 0);
	bool doParseJson(const char * lpData, int nSize, Json::Value & outValue);
	bool doCmdUdpLogout(const char * lpData, int nSize);
	bool doCmdTeacherLogin(const char * lpData, int nSize);
	bool doCmdTeacherOnLine(const char * lpData, int nSize);
	bool doCmdTeacherCameraList(const char * lpData, int nSize);
	bool SendLoginCmd(int nDBCameraID, int nSceneItemID);
	bool SendData(const char * lpDataPtr, int nDataSize);
private:
	bool        m_bCanReBuild;			// 能否进行重建标志...
	int         m_nInitDBCameraID;		// 初始默认通道编号...
	int         m_nInitSceneitemID;		// 初始默认场景编号...
};
