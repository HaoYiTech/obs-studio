
#pragma once

#include "OSThread.h"
#include "json.h"

class CWebThread : public OSThread
{
public:
	CWebThread();
	~CWebThread();
public:
	bool		IsCanReConnect() { return m_bIsCanReConnect; }
	bool		IsLoadSuccess() { return m_bIsLoadSuccess; }
public:
	GM_Error	InitThread();							// 初始化线程...
	bool		doWebGatherLogout();		            // 采集端退出汇报...
	void		doPostCurl(char *pData, size_t nSize);	// 处理汇报反馈...
	bool		doWebGetCamera(int nDBCameraID);		// 向网站注册摄像头...
	bool		doWebDelCamera(string & inDeviceSN);	// 向网站删除摄像头...
	bool		doWebRegCamera(GM_MapData & inData);	// 添加或修改通道...
	bool		doWebStatCamera(int nDBCamera, int nStatus);
	bool		doWebSaveSys(ROLE_TYPE nRoleType, string & strName, string & strIPSend);
private:
	virtual void	Entry();				// 线程函数...
	bool			LogoutHaoYi();			// 采集端从中心退出...
	bool			LogoutGather();			// 采集端从节点退出...
	bool			RegisterHaoYi();		// 验证许可证...
	bool			RegisterGather();		// 注册采集端...
	bool			GetAllCameraData();		// 获取通道配置...
	bool			parseJson(Json::Value & outValue);
private:
	enum {
		kRegGather = 0,					// 正在注册采集端...
		kRegHaoYi = 1,					// 正在验证许可证...
		kGetCamera = 2,					// 正在获取通道...
		kRegCamera = 3,					// 正在注册通道(添加或修改)...
		kDelCamera = 4,					// 正在删除摄像头...
		kStatCamera = 5,				// 正在汇报通道状态...
		kGatherLogout = 6,				// 采集端退出汇报...
		kSaveSys = 7,                   // 保存系统配置...
	}m_eRegState;
private:
	bool			m_bIsLoadSuccess;	// 是否已经成功连接服务器标志...
	bool			m_bIsCanReConnect;	// 能否重连服务器标志...
	int				m_nCurCameraCount;	// 已经注册的摄像头个数...
	string			m_strUTF8Data;		// 统一的反馈数据...
};