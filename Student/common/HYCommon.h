
#pragma	once

#include <assert.h>

#ifndef ASSERT
#define ASSERT assert 
#endif // ASSERT

#define DEF_WEB_CENTER			"https://www.qidiweilai.com"// 默认中心网站(443) => 必须是 https:// 兼容小程序接口...
#define DEF_WEB_CLASS			"https://www.qidiweilai.com"// 默认云教室网站(443) => 必须是 https:// 兼容小程序接口...

// define client type...
enum {
	kClientPHP        = 1,
	kClientStudent    = 2,
	kClientTeacher    = 3,
	kClientUdpServer  = 4,
};

enum AUTH_STATE
{
	kAuthRegister	= 1,		// 网站注册授权
	kAuthExpired	= 2,		// 授权过期验证
};

enum STREAM_PROP
{
	kStreamDevice	= 0,		// 摄像头设备流类型 => camera
	kStreamMP4File	= 1,		// MP4文件流类型    => .mp4
	kStreamUrlLink	= 2,		// URL链接流类型    => rtsp/rtmp
};

// define Mini QRCode type...
enum {
	kQRCodeShop       = 1,		// 门店小程序码
	kQRCodeStudent    = 2,		// 学生端小程序码
	kQRCodeTeacher    = 3,		// 讲师端小程序码
};

// define command id...
enum {
	kCmd_Student_Login		  = 1,
	kCmd_Student_OnLine		  = 2,
	kCmd_Teacher_Login		  = 3,
	kCmd_Teacher_OnLine		  = 4,
	kCmd_UDP_Logout			    = 5,
	kCmd_Camera_PullStart     = 6,
	kCmd_Camera_PullStop      = 7,
	kCmd_Camera_OnLineList    = 8,
	kCmd_Camera_LiveStart     = 9,
	kCmd_Camera_LiveStop      = 10,
	kCmd_Camera_PTZCommand    = 11,
	kCmd_UdpServer_Login      = 12,
	kCmd_UdpServer_OnLine     = 13,
	kCmd_UdpServer_AddTeacher = 14,
	kCmd_UdpServer_DelTeacher = 15,
	kCmd_UdpServer_AddStudent = 16,
	kCmd_UdpServer_DelStudent = 17,
	kCmd_PHP_GetUdpServer     = 18,
	kCmd_PHP_GetAllServer     = 19,
	kCmd_PHP_GetAllClient     = 20,
	kCmd_PHP_GetRoomList      = 21,
	kCmd_PHP_GetPlayerList    = 22,
	kCmd_PHP_Bind_Mini        = 23,
};

// define IPC ISAPI command...
const long ISAPI_LINE_START = __LINE__ + 2;
enum CMD_ISAPI {
	kIMAGE_CAPABILITY = __LINE__ - ISAPI_LINE_START,     // Image 能力查询命令
	kPTZ_CAPABILITY   = __LINE__ - ISAPI_LINE_START,     // PTZ 能力查询命令
	kPTZ_X_PAN        = __LINE__ - ISAPI_LINE_START,     // PTZ X轴向右(+)向左(-)
	kPTZ_Y_TILT       = __LINE__ - ISAPI_LINE_START,     // PTZ Y轴向上(+)向下(-)
	kPTZ_Z_ZOOM       = __LINE__ - ISAPI_LINE_START,     // PTZ Z轴放大(+)缩小(-)
	kPTZ_F_FOCUS      = __LINE__ - ISAPI_LINE_START,     // PTZ 焦距放大(+)缩小(-)
	kPTZ_I_IRIS       = __LINE__ - ISAPI_LINE_START,     // PTZ 光圈放大(+)缩小(-)
	kIMAGE_FLIP       = __LINE__ - ISAPI_LINE_START,     // 图像画面翻转命令
};

// define the command header...
typedef struct {
	int   m_pkg_len;    // body size...
	int   m_type;       // client type...
	int   m_cmd;        // command id...
	int   m_sock;       // php sock in transmit...
} Cmd_Header;
