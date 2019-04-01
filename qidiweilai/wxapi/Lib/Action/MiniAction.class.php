<?php
/*************************************************************
    HaoYi (C)2017 - 2018 myhaoyi.com
    备注：专门处理微信小程序请求页面...
*************************************************************/
/**
 * error code 说明.
 * <ul>
 *    <li>-41001: encodingAesKey 非法</li>
 *    <li>-41003: aes 解密失败</li>
 *    <li>-41004: 解密后得到的buffer非法</li>
 *    <li>-41005: base64加密失败</li>
 *    <li>-41016: base64解密失败</li>
 * </ul>
 */
class ErrorCode
{
	public static $OK = 0;
	public static $IllegalAesKey = -41001;
	public static $IllegalIv = -41002;
	public static $IllegalBuffer = -41003;
	public static $DecodeBase64Error = -41004;
}
/**
 * 对微信小程序用户加密数据的解密示例代码.
 *
 * @copyright Copyright (c) 1998-2014 Tencent Inc.
 */
class WXBizDataCrypt
{
  private $appid;
  private $sessionKey;

	/**
	 * 构造函数
	 * @param $sessionKey string 用户在小程序登录后获取的会话密钥
	 * @param $appid string 小程序的appid
	 */
	public function WXBizDataCrypt( $appid, $sessionKey)
	{
		$this->sessionKey = $sessionKey;
		$this->appid = $appid;
	}
	/**
	 * 检验数据的真实性，并且获取解密后的明文.
	 * @param $encryptedData string 加密的用户数据
	 * @param $iv string 与用户数据一同返回的初始向量
	 * @param $data string 解密后的原文
     *
	 * @return int 成功0，失败返回对应的错误码
	 */
	public function decryptData( $encryptedData, $iv, &$data )
	{
		if (strlen($this->sessionKey) != 24) {
			return ErrorCode::$IllegalAesKey;
		}
		$aesKey=base64_decode($this->sessionKey);
		if (strlen($iv) != 24) {
			return ErrorCode::$IllegalIv;
		}
		$aesIV=base64_decode($iv);

		$aesCipher=base64_decode($encryptedData);

		$result=openssl_decrypt( $aesCipher, "AES-128-CBC", $aesKey, 1, $aesIV);

		$dataObj=json_decode( $result );
		if( $dataObj  == NULL )
		{
			return ErrorCode::$IllegalBuffer;
		}
		if( $dataObj->watermark->appid != $this->appid )
		{
			return ErrorCode::$IllegalBuffer;
		}
		$data = $result;
		return ErrorCode::$OK;
	}
}
///////////////////////////////////////////////
// 微信小程序需要用到的数据接口类...
///////////////////////////////////////////////
class MiniAction extends Action
{
  public function _initialize()
  {
    // 支持多个小程序的接入...
    $this->m_weMini = C('QIDI_MINI');
  }
  //
  // 获取小程序的access_token的值...
  public function getToken()
  {
    // 准备返回信息...
    $arrErr['err_code'] = 0;
    $arrErr['err_msg'] = 'ok';
    do {
      // 准备请求需要的url地址...
      $strTokenUrl = sprintf("https://api.weixin.qq.com/cgi-bin/token?grant_type=client_credential&appid=%s&secret=%s",
                           $this->m_weMini['appid'], $this->m_weMini['appsecret']);
      // 直接通过标准API获取access_token...
      $result = http_get($strTokenUrl);
      // 获取access_token失败的情况...
      if( !$result ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '获取access_token失败';
        break;
      }
      // 将获取的数据转换成数组...
      $json = json_decode($result,true);
      if( !$json || isset($json['errcode']) ) {
        $arrErr['err_code'] = $json['errcode'];
        $arrErr['err_msg'] = $json['errmsg'];
        break;
      }
      // 获取access_token成功，返回过期时间和路径信息...
      $arrErr['access_token'] = $json['access_token'];
      $arrErr['expires_in'] = $json['expires_in'];
      $arrErr['mini_path'] = 'pages/bind/bind';
    } while( false );
    // 返回json数据包...
    echo json_encode($arrErr);
  }
  //
  // 获取房间列表 => 分页显示...
  public function getRoom()
  {
    // 准备返回信息...
    $arrErr['err_code'] = 0;
    $arrErr['err_msg'] = 'ok';
    // 注意：这里使用的是 $_POST 数据...
    do {
      // 判断输入的参数是否有效...
      if( !isset($_POST['cur_page']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '输入参数无效！';
        break;
      }
      // 得到每页条数...
      $pagePer = C('PAGE_PER');
      $pageCur = $_POST['cur_page'];  // 当前页码...
      $pageLimit = (($pageCur-1)*$pagePer).','.$pagePer; // 读取范围...
      // 获取图片链接需要的数据 => web_tracker_addr 已经自带了协议头 http://或https://
      $dbSys = D('system')->field('web_tracker_addr,web_tracker_port')->find();
      // 获取记录总数和总页数...
      $totalNum = D('room')->count();
      $max_page = intval($totalNum / $pagePer);
      // 判断是否是整数倍的页码...
      $max_page += (($totalNum % $pagePer) ? 1 : 0);
      // 填充需要返回的信息...
      $arrErr['total_num'] = $totalNum;
      $arrErr['max_page'] = $max_page;
      $arrErr['cur_page'] = $pageCur;
      // 获取房间分页数据，通过视图获取数据...
      $arrRoom = D('RoomView')->limit($pageLimit)->order('Room.created DESC')->select();
      // 对房间的图片地址进行重构 => 加上访问地址...
      foreach($arrRoom as &$dbItem) {
        // 修改房间编号...
        $dbItem['room_id'] = LIVE_BEGIN_ID + $dbItem['room_id'];
        // 获取截图快照地址，地址不为空才处理 => 为空时，小程序内部会跳转到snap.png...
        if( strlen($dbItem['image_fdfs']) > 0 ) {
          $dbItem['image_fdfs'] = sprintf("%s:%d/%s", $dbSys['web_tracker_addr'], $dbSys['web_tracker_port'], $dbItem['image_fdfs']);
        }
        // 获取海报图片完整连接地址...
        if( strlen($dbItem['poster_fdfs']) > 0 ) {
          $dbItem['poster_fdfs'] = sprintf("%s:%d/%s", $dbSys['web_tracker_addr'], $dbSys['web_tracker_port'], $dbItem['poster_fdfs']);
        }
      }
      // 组合最终返回的数据结果...
      $arrErr['room'] = $arrRoom;
    } while ( false );
    // 返回json编码数据包...
    echo json_encode($arrErr);
  }
  //
  // 处理小程序登录事件...
  public function login()
  {
    // 准备返回信息...
    $arrErr['err_code'] = 0;
    $arrErr['err_msg'] = 'ok';
    // 注意：这里使用的是 $_POST 数据...
    do {
      // 判断输入参数的有效性 => 没有设置或数据为空，返回错误 => 去掉 miniName 字段...
      if( !isset($_POST['code']) || !isset($_POST['encrypt']) || !isset($_POST['iv']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '输入的参数无效';
        break;
      }
      // 准备请求需要的url地址 => jscode2session => 用code换取session_key...
      $strUrl = sprintf("https://api.weixin.qq.com/sns/jscode2session?appid=%s&secret=%s&js_code=%s&grant_type=authorization_code",
                         $this->m_weMini['appid'], $this->m_weMini['appsecret'], $_POST['code']);
      // code 换取 openid | session_key，判断返回结果...
      $result = http_get($strUrl);
      if( !$result ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '获取openid失败';
        break;
      }
      // 解析微信API返回数据，发生错误...
      $json = json_decode($result,true);
      if( !$json || isset($json['errcode']) ) {
        $arrErr['err_code'] = $json['errcode'];
        $arrErr['err_msg'] = $json['errmsg'];
        break;
      }
      // 获取到了正确的 openid | session_key，构造解密对象...
      $wxCrypt = new WXBizDataCrypt($this->m_weMini['appid'], $json['session_key']);
      $theErr = $wxCrypt->decryptData($_POST['encrypt'], $_POST['iv'], $outData);
      // 解码失败，返回错误...
      if( $theErr != 0 ) {
        $arrErr['err_code'] = $theErr;
        $arrErr['err_msg'] = '数据解密失败';
        break;
      }
      // 将获取的数据转换成数组 => 有些字段包含大写字母...
      $arrUser = json_decode($outData, true);
      if( !isset($arrUser['unionId']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '没有获取到unionid';
        break;
      }
      // 微信昵称中去除emoji表情符号的操作...
      $arrUser['nickName'] = trimEmo($arrUser['nickName']);
      // 将获取到的用户关键值查找数据库内容...
      // 这里是小程序，注意有些字段有大写字母，字段标识也有区别...
      $where['wx_unionid'] = $arrUser['unionId'];
      $dbUser = D('user')->where($where)->find();
      // 根据小程序名称设置openid...
      $dbUser['wx_qidi_mini'] = $arrUser['openId'];
      // 从微信获取的信息更新到数据库当中...
      // 这里是小程序，注意有些字段有大写字母，字段标识也有区别...
      $dbUser['wx_unionid'] = $arrUser['unionId'];    // 全局唯一ID
      $dbUser['wx_nickname'] = $arrUser['nickName'];  // 微信昵称
      $dbUser['wx_language'] = $arrUser['language'];  // 语言
      $dbUser['wx_headurl'] = $arrUser['avatarUrl'];  // 0,46,64,96,132
      $dbUser['wx_country'] = $arrUser['country'];    // 国家
      $dbUser['wx_province'] = $arrUser['province'];  // 省份
      $dbUser['wx_city'] = $arrUser['city'];          // 城市
      $dbUser['wx_sex'] = $arrUser['gender'];         // 性别
      // 更新 $_POST 传递过来的其它数据到数据库对象当中 => 设置了字段才更新保存...
      if( isset($_POST['wx_brand']) ) { $dbUser['wx_brand'] = $_POST['wx_brand']; }
      if( isset($_POST['wx_model']) ) { $dbUser['wx_model'] = $_POST['wx_model']; }
      if( isset($_POST['wx_version']) ) { $dbUser['wx_version'] = $_POST['wx_version']; }
      if( isset($_POST['wx_system']) ) { $dbUser['wx_system'] = $_POST['wx_system']; }
      if( isset($_POST['wx_platform']) ) { $dbUser['wx_platform'] = $_POST['wx_platform']; }
      if( isset($_POST['wx_SDKVersion']) ) { $dbUser['wx_SDKVersion'] = $_POST['wx_SDKVersion']; }
      if( isset($_POST['wx_pixelRatio']) ) { $dbUser['wx_pixelRatio'] = $_POST['wx_pixelRatio']; }
      if( isset($_POST['wx_screenWidth']) ) { $dbUser['wx_screenWidth'] = $_POST['wx_screenWidth']; }
      if( isset($_POST['wx_screenHeight']) ) { $dbUser['wx_screenHeight'] = $_POST['wx_screenHeight']; }
      if( isset($_POST['wx_fontSizeSetting']) ) { $dbUser['wx_fontSizeSetting'] = $_POST['wx_fontSizeSetting']; }
      // 根据id字段判断是否有记录...
      if( isset($dbUser['user_id']) ) {
        // 更新已有的用户记录...
        $dbUser['update_time'] = date('Y-m-d H:i:s');
        $condition['user_id'] = $dbUser['user_id'];
        D('user')->where($condition)->save($dbUser);
      } else {
        // 新建一条用户记录...
        $dbUser['create_time'] = date('Y-m-d H:i:s');
        $dbUser['update_time'] = date('Y-m-d H:i:s');
        $insertid = D('user')->add($dbUser);
        $dbUser['user_id'] = $insertid;
        // 设定默认的用户类型|真实姓名...
        $dbUser['user_type'] = kParentUser;
      }
      // 返回得到的用户编号|用户类型|真实姓名...
      $arrErr['user_id'] = $dbUser['user_id'];
      $arrErr['user_type'] = $dbUser['user_type'];
      $arrErr['real_name'] = $dbUser['real_name'];
     }while( false );
    // 返回json数据包...
    echo json_encode($arrErr);
  }
  //
  // 处理小程序获取聊天用户列表...
  public function getChatUser()
  {
    // 准备返回信息...
    $arrErr['err_code'] = 0;
    $arrErr['err_msg'] = 'ok';
    // 注意：这里使用的是 $_POST 数据...
    do {
      // 判断输入参数的有效性 => 必须包含 room_id|cur_page...
      if( !isset($_POST['room_id']) || !isset($_POST['cur_page']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '输入的参数无效';
        break;
      }
      // 修改房间编号 => 减去房间偏移号码...
      $_POST['room_id'] = $_POST['room_id'] - LIVE_BEGIN_ID;
      // 得到每页条数...
      $pagePer = C('PAGE_PER');
      $pageCur = $_POST['cur_page'];  // 当前页码...
      $pageLimit = (($pageCur-1)*$pagePer).','.$pagePer; // 读取范围...
      // 获取记录总数和总页数 => 使用子查询...
      $condition['room_id'] = $_POST['room_id'];
      $strQuery = 'SELECT COUNT(*) as UserNum FROM (SELECT * FROM `wk_chat` WHERE (`room_id`='.$_POST['room_id'].') GROUP BY user_id) a LIMIT 1';
      $totalNum = D()->query($strQuery)[0]['UserNum'];
      $max_page = intval($totalNum / $pagePer);
      // 判断是否是整数倍的页码...
      $max_page += (($totalNum % $pagePer) ? 1 : 0);
      // 填充需要返回的信息...
      $arrErr['total_num'] = $totalNum;
      $arrErr['max_page'] = $max_page;
      $arrErr['cur_page'] = $pageCur;
      // 查询具体的用户详细信息记录...
      $theFileds = 'user_id,user_type,real_name,wx_nickname,wx_headurl';
      $arrErr['items'] = D('ChatView')->where($condition)->field($theFileds)->group('user_id')->limit($pageLimit)->select();
    } while( false );
    // 返回json数据包...
    echo json_encode($arrErr);
  }
  //
  // 处理小程序获取聊天记录...
  public function getChatList()
  {
    // 准备返回信息...
    $arrErr['err_code'] = 0;
    $arrErr['err_msg'] = 'ok';
    // 注意：这里使用的是 $_POST 数据...
    do {
      // 判断输入参数的有效性 => 必须包含 room_id|to_up|min_id|max_id...
      if( !isset($_POST['room_id']) || !isset($_POST['to_up']) || !isset($_POST['min_id']) || !isset($_POST['max_id']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '输入的参数无效';
        break;
      }
      // 修改房间编号 => 减去房间偏移号码...
      $_POST['room_id'] = $_POST['room_id'] - LIVE_BEGIN_ID;
      // 得到每页条数和查询方向...
      $pagePer = C('PAGE_PER');
      $condition['room_id'] = $_POST['room_id'];
      $condition['chat_id'] = (($_POST['to_up'] > 0) ? array('lt', $_POST['min_id']) : array('gt', $_POST['max_id']));
      // 获取房间对应聊天分页数据，通过视图获取数据...
      $arrErr['items'] = D('ChatView')->where($condition)->limit($pagePer)->order('Chat.created DESC')->select();
    } while( false );
    // 返回json数据包...
    echo json_encode($arrErr);
  }
  //
  // 处理小程序保存聊天记录...
  public function saveChat()
  {
    // 准备返回信息...
    $arrErr['err_code'] = 0;
    $arrErr['err_msg'] = 'ok';
    // 注意：这里使用的是 $_POST 数据...
    do {
      // 判断输入参数的有效性 => 必须包含 room_id|user_id|type_id|content...
      if( !isset($_POST['room_id']) || !isset($_POST['user_id']) || !isset($_POST['type_id']) || !isset($_POST['content']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '输入的参数无效';
        break;
      }
      // 修改房间编号 => 减去房间偏移号码...
      $_POST['room_id'] = $_POST['room_id'] - LIVE_BEGIN_ID;
      // 直接创建一条聊天消息记录，返回消息编号...
      $_POST['created'] = date('Y-m-d H:i:s');
      $insertid = D('chat')->add($_POST);
      $arrErr['chat_id'] = $insertid;
    } while( false );
    // 返回json数据包...
    echo json_encode($arrErr);
  }
}
?>