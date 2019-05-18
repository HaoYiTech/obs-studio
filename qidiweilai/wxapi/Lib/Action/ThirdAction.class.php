<?php
/*************************************************************
    Wan (C)2019- 2020 myhaoyi.com
    备注：第三方登录页面...
*************************************************************/

class ThirdAction extends Action
{
  public function _initialize() {
    // 获取登录配置信息...
    $this->m_weLogin = C('WECHAT_LOGIN');
    // 获取系统配置，根据配置设置相关变量...
    $this->m_dbSys = D('system')->find();
    // 直接给模板变量赋值 => 跟foot统一...
    $this->assign('my_system', $this->m_dbSys);
  }
  /**
  +----------------------------------------------------------
  * 默认操作 - 处理第三方微信网站端登录接口...
  +----------------------------------------------------------
  */
  public function index()
  {
    // 处理第三方网站扫码登录的授权回调过程 => 其它网站的登录，state != 32...
    if( isset($_GET['code']) && isset($_GET['state']) && strlen($_GET['state']) != 32 ) {
      $this->doWechatAuth($_GET['code'], $_GET['state']);
      return;
    }
    // 判断是否包含场景参数字段...
    if( !isset($_GET['scene']) ) {
      $this->dispError(true, '必须包含场景参数', '糟糕，登录失败了');
      return;
    }
    // 注意：$_SERVER['HTTP_HOST'] 自带访问端口...
    // 拼接当前访问页面的完整链接地址 => 登录服务器会反向调用...
    $dbJson['url'] = sprintf("%s://%s%s%s", $_SERVER['REQUEST_SCHEME'], $_SERVER['HTTP_HOST'], __APP__, "/Third/login");
    $dbJson['scene'] = $_GET['scene'];
    $state = json_encode($dbJson);
    // 对链接地址进行base64加密...
    $state = urlsafe_b64encode($state);

    // 给模板设定数据 => default/Third/index.htm
    $this->assign('my_state', $state);
    $this->assign('my_scope', $this->m_weLogin['scope']);
    $this->assign('my_appid', $this->m_weLogin['appid']);
    //$this->assign('my_href', $this->m_weLogin['href']);
    $this->assign('my_redirect_uri', urlencode($this->m_weLogin['redirect_uri'] . __APP__ . "/Third/index"));
    $this->display();
  }
  //
  // 执行微信用户的登录授权操作...
  private function doWechatAuth($strCode, $strState)
  {
    do {
      // 从state当中解析回调函数 => 需要进一步的深入解析...
      $strJson = urlsafe_b64decode($strState);
      $arrJson = json_decode($strJson, true);
      // 保存截取之后的数据 => url 已经根据需要自动带上端口信息...
      $strBackUrl = $arrJson['url'];
      $strScene = $arrJson['scene'];
      // 准备请求需要的url地址...
      $strTokenUrl = sprintf("https://api.weixin.qq.com/sns/oauth2/access_token?appid=%s&secret=%s&code=%s&grant_type=authorization_code",
                              $this->m_weLogin['appid'], $this->m_weLogin['appsecret'], $strCode);
      // 通过code获取access_token...
      $result = http_get($strTokenUrl);
      if( !$result ) {
        $strError = 'error: get_access_token';
        break;
      }
      // 将返回信息转换成数组...
      $arrToken = json_decode($result, true);
      if( isset($arrToken['errcode']) ) {
        $strError = 'error: errorcode='.$arrToken['errcode'].',errormsg='.$arrToken['errmsg'];
        break;
      }
      // 准备请求需要的url地址...
      $strUserUrl = sprintf("https://api.weixin.qq.com/sns/userinfo?access_token=%s&openid=%s&lang=zh_CN", $arrToken['access_token'], $arrToken['openid']);
      // 通过access_token获取用户信息...
      $result = http_get($strUserUrl);
      if( !result ) {
        $strError = 'error: get_user_info';
        break;
      }
      // 将返回信息转换成数组...
      $arrUser = json_decode($result, true);
      if( isset($arrUser['errcode']) ) {
        $strError = 'error: errorcode='.$arrUser['errcode'].',errormsg='.$arrUser['errmsg'];
        break;
      }
      // 判断当前页面是否是https协议 => 通过$_SERVER['HTTPS']和$_SERVER['REQUEST_SCHEME']来判断 => 对头像进行最原始的彻底修正...
      if((isset($_SERVER['HTTPS']) && $_SERVER['HTTPS'] == 'on') || (isset($_SERVER['REQUEST_SCHEME']) && $_SERVER['REQUEST_SCHEME'] == 'https')) {
        $arrUser['headimgurl'] = str_replace('http://', 'https://', $arrUser['headimgurl']);
      }
      // 保存场景参数字段...
      $arrUser['scene'] = $strScene;
      // 微信昵称中去除emoji表情符号的操作...
      $arrUser['nickname'] = trimEmo($arrUser['nickname']);
      // 保存需要返回的用户信息 => 全部转换成JSON...
      $strWxJSON = json_encode($arrUser);
    }while( false );
    // 将需要返回的参数进行base64编码处理...
    if( strlen($strError) > 0 ) {
      $strError = urlsafe_b64encode($strError);
      $strLocation = sprintf("Location: %s/wx_error/%s", $strBackUrl, $strError);
    } else {
      $strWxJSON = urlsafe_b64encode($strWxJSON);
      $strLocation = sprintf("Location: %s/wx_json/%s", $strBackUrl, $strWxJSON);
    }
    // 跳转页面到第三方回调地址...
    header($strLocation);
  }
  //
  // 处理微信扫码后的登录回调...
  public function login()
  {
    // 如果有登录错误信息，跳转到错误处理...
    if( isset($_GET['wx_error']) ) {
      $strErr = urlsafe_b64decode($_GET['wx_error']);
      // 这里需要专门编写一个特殊的报错页面...
      $this->dispError(true, $strErr, '糟糕，登录失败了');
      return;
    }
    // 登录成功，解析数据，放入cookie当中...
    if( isset($_GET['wx_json']) ) {
			// 获取服务器反馈的信息...
      $strWxJSON = urlsafe_b64decode($_GET['wx_json']);
      $arrUser = json_decode($strWxJSON, true);
      // 判断获取数据的有效性 => 必须包含场景参数字段...
      if( !isset($arrUser['scene']) || !isset($arrUser['unionid']) || !isset($arrUser['headimgurl']) ) {
        $this->dispError(true, '获取微信数据失败', '糟糕，登录失败了');
        return;
      }
      // 进行unionid|openid判断，交给启迪网站验证...
      
      // 从当前已有的数据库中查找这个用户的信息...
      $condition['wx_unionid'] = $arrUser['unionid'];
      $dbUser = D('user')->where($condition)->find();
      // 将从微信获取到的信息更新到数据库当中...
      // 这里是网站应用，不能得到是否关注公众号...
      $dbUser['wx_unionid'] = $arrUser['unionid'];    // 全局唯一ID
      $dbUser['wx_openid_web'] = $arrUser['openid'];  // 本应用的openid
      $dbUser['wx_nickname'] = $arrUser['nickname'];  // 微信昵称
      $dbUser['wx_language'] = $arrUser['language'];  // 语言
      $dbUser['wx_headurl'] = $arrUser['headimgurl']; // 0,46,64,96,132
      $dbUser['wx_country'] = $arrUser['country'];    // 国家
      $dbUser['wx_province'] = $arrUser['province'];  // 省份
      $dbUser['wx_city'] = $arrUser['city'];          // 城市
      $dbUser['wx_sex'] = $arrUser['sex'];            // 性别
      // 根据id字段判断是否有记录...
      if( isset($dbUser['user_id']) ) {
        // 更新已有的用户记录...
        $dbUser['update_time'] = date('Y-m-d H:i:s');
        $where['user_id'] = $dbUser['user_id'];
        D('user')->where($where)->save($dbUser);
      } else {
        // 新建一条用户记录 => 设置成家长身份...
        $dbUser['user_type'] = kParentUser;
        $dbUser['create_time'] = date('Y-m-d H:i:s');
        $dbUser['update_time'] = date('Y-m-d H:i:s');
        $dbUser['shop_id'] = 0; // 默认设置成总部用户
        $dbUser['user_id'] = D('user')->add($dbUser);
      }
      // 打印用户信息...
      //print_r($dbUser);
      //$this->display();
      // 解析场景参数字段...
      $arrScene = explode('_', $arrUser['scene']);
      $arrParam['client_type'] = $arrScene[0];
      $arrParam['tcp_socket'] = $arrScene[1];
      $arrParam['tcp_time'] = $arrScene[2];
      $arrParam['user_id'] = $dbUser['user_id'];
      $arrParam['room_id'] = 200001;
      $arrParam['bind_cmd'] = 2;
      // 读取系统配置数据库记录...
      $dbSys = $this->m_dbSys;
      // 向中心服务器转发小程序绑定登录这个中转命令...
      $dbResult = $this->doTrasmitCmdToServer($dbSys['udpcenter_addr'], $dbSys['udpcenter_port'], kCmd_PHP_Bind_Mini, $arrParam);
      print_r($dbResult);
    }
  }
  //
  // 统一的核心服务器命令接口...
  private function doTrasmitCmdToServer($inServerAddr, $inServerPort, $inCmd, &$dbParam)
  {
    // 通过php扩展插件连接中转服务器 => 性能高...
    $transmit = transmit_connect_server($inServerAddr, $inServerPort);
    // 链接中心服务器失败，直接返回...
    if( !$transmit ) {
      $arrData['err_code'] = true;
      $arrData['err_msg'] = '无法连接中心服务器。';
      return $arrData;
    }
    // 调用php扩展插件的中转接口带上参数...
    $saveJson = json_encode($dbParam);
    $json_data = transmit_command(kClientPHP, $inCmd, $transmit, $saveJson);
    // 关闭中转服务器链接...
    transmit_disconnect_server($transmit);
    // 获取的JSON数据有效，转成数组，直接返回...
    $arrData = json_decode($json_data, true);
    if( !$arrData ) {
      $arrData['err_code'] = true;
      $arrData['err_msg'] = '从中心服务器获取数据失败。';
      return $arrData;
    }
    // 通过错误码，获得错误信息...
    $arrData['err_msg'] = getTransmitErrMsg($arrData['err_code']);
    // 将整个数组返回...
    return $arrData;
  }
  //
  // 显示错误模板信息...
  private function dispError($bIsAdmin, $inMsgTitle, $inMsgDesc)
  {
    $this->assign('my_admin', $bIsAdmin);
    $this->assign('my_title', '糟糕，出错了');
    $this->assign('my_msg_title', $inMsgTitle);
    $this->assign('my_msg_desc', $inMsgDesc);
    $this->display('Common:error_page');
  }
}
?>