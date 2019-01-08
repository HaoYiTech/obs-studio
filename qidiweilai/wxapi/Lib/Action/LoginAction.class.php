<?php
/*************************************************************
    HaoYi (C)2017 - 2020 myhaoyi.com

*************************************************************/

class LoginAction extends Action
{
  // 初始化页面的默认操作...
  public function _initialize()
  {
    // 获取系统配置，根据配置设置相关变量 => 强制配置成云教室...
    $this->m_dbSys = D('system')->find();
    $this->m_webType = kCloudEducate;
    // 直接给模板变量赋值 => 跟foot统一...
    $this->assign('my_system', $this->m_dbSys);
  }
  //
  // 显示微信登录的二维码...
  public function index()
  {
    $this->login(true);
  }
  //
  // 前后端唯一的登录函数...
  // 参数：wx_error | wx_unionid | wx_headurl
  public function login($bIsAdmin)
  {
    // 如果有登录错误信息，跳转到错误处理...
    if( isset($_GET['wx_error']) ) {
      $strErr = urlsafe_b64decode($_GET['wx_error']);
      // 这里需要专门编写一个特殊的报错页面...
      $this->dispError($bIsAdmin, $strErr, '糟糕，登录失败了');
      return;
    }
    // 登录成功，解析数据，放入cookie当中...
    if( isset($_GET['wx_json']) ) {
			// 获取服务器反馈的信息...
      $strWxJSON = urlsafe_b64decode($_GET['wx_json']);
      $arrUser = json_decode($strWxJSON, true);
      // 判断获取数据的有效性...
      if( !isset($arrUser['unionid']) || !isset($arrUser['headimgurl']) ) {
        $this->dispError($bIsAdmin, '获取微信数据失败', '糟糕，登录失败了');
        return;
      }
      // 判断获取授权数据的有效性...
      if( !isset($arrUser['auth_days']) || !isset($arrUser['auth_license']) || !isset($arrUser['auth_expired']) ) {
        $this->dispError($bIsAdmin, '获取授权数据失败', '糟糕，登录失败了');
        return;
      }
      // 从当前已有的数据库中查找这个用户的信息...
      $map['wx_unionid'] = $arrUser['unionid'];
      $dbUser = D('user')->where($map)->find();
      $nUserCount = D('user')->count();
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
        // 新建一条用户记录 => 如果是第一个用户，设置成管理员身份，否则设置成家长身份...
        $dbUser['user_type'] = (($nUserCount <= 0) ? kAdministerUser : kParentUser);
        $dbUser['create_time'] = date('Y-m-d H:i:s');
        $dbUser['update_time'] = date('Y-m-d H:i:s');
        $dbUser['shop_id'] = 0; // 默认设置成总部用户
        $insertid = D('user')->add($dbUser);
        $dbUser['user_id'] = $insertid;
      }
      // 将有用的数据存放到cookie当中...
      $strUnionid = $dbUser['wx_unionid'];
      $strHeadUrl = $dbUser['wx_headurl'];
      $nUserType = $dbUser['user_type'];
      $nShopID = $dbUser['shop_id'];
      $nElapseTime = 3600*24;
      // 把unionid存入cookie当中，有效期设置一整天，必须用Cookie类...
      Cookie::set('wx_unionid', $strUnionid, $nElapseTime);
      Cookie::set('wx_headurl', $strHeadUrl, $nElapseTime);
      Cookie::set('wx_usertype', $nUserType, $nElapseTime);
      Cookie::set('wx_shopid', $nShopID, $nElapseTime);
      // 将授权数据存入cookie当中，有效期设置为一整天，必须用Cookie类...
      Cookie::set('auth_days', $arrUser['auth_days'], $nElapseTime);
      Cookie::set('auth_license', $arrUser['auth_license'], $nElapseTime);
      Cookie::set('auth_expired', $arrUser['auth_expired'], $nElapseTime);
      // 如果是后台 => 用户登录成功，进行页面跳转 => 直接跳转到后台首页...
      if( $bIsAdmin ) {
        A('Admin')->index();
        return;
      }
      // 如果是前台 => 用户登录成功，进行页面跳转 => 从登录前的cookie中获取...
      $strJump = Cookie::get('wx_jump');
      // 注意：$_SERVER['HTTP_HOST'] 自带访问端口...
      $strHost = sprintf("%s://%s", $_SERVER['REQUEST_SCHEME'], $_SERVER['HTTP_HOST']);
      // 判断当前页面是否是https协议 => 通过$_SERVER['HTTPS']和$_SERVER['REQUEST_SCHEME']来判断...
      /*if((isset($_SERVER['HTTPS']) && $_SERVER['HTTPS'] == 'on') || (isset($_SERVER['REQUEST_SCHEME']) && $_SERVER['REQUEST_SCHEME'] == 'https')) {
        $strHost = sprintf("https://%s", $_SERVER['HTTP_HOST']);
      } else {
        $strHost = sprintf("http://%s", $_SERVER['HTTP_HOST']);
      }*/
      // 重置跳转页面的cookie值 => 删除是要用 API...
      setcookie('wx_jump','',-1,'/');
      // 如果跳转页面与当前主机比较...
      if( strncasecmp($strJump, $strHost, strlen($strHost)) == 0 ) {
        // 一致，则直接跳转...
        header("location:".$strJump);
      } else {
        // 不一致，跳转到首页...
        A('Home')->index();
      }
      return;
    }
    // 如果当前的cookie是有效的，直接进行页面跳转 => 前后台处理不一样...
    if( Cookie::is_set('wx_unionid') && Cookie::is_set('wx_headurl') && Cookie::is_set('wx_usertype') && Cookie::is_set('wx_shopid') ) {
      if( $bIsAdmin ) { 
        // 后台 => 直接跳转...
        A('Admin')->index();
      } else {
        // 前台 => 刷新页面...
        echo "<script>window.parent.doReload();</script>";
      }
      return;
    }
    // cookie不是完全有效，重置cookie...
    setcookie('wx_unionid','',-1,'/');
    setcookie('wx_headurl','',-1,'/');
    setcookie('wx_usertype','',-1,'/');
    setcookie('wx_shopid','',-1,'/');
    setcookie('auth_days','',-1,'/');
    setcookie('auth_license','',-1,'/');
    setcookie('auth_expired','',-1,'/');

    // 前台特殊处理 => 记录登录成功之后的跳转页面 => 必须用Cookie类...
    if( !$bIsAdmin ) {
      Cookie::set('wx_jump', $_SERVER['HTTP_REFERER'], 3600);
    }

    ///////////////////////////////////////////////////////////
    // 没有cookie，没有错误，没有unionid，说明是正常登录...
    ///////////////////////////////////////////////////////////
    
    // 获取节点网站的标记字段...
    $dbSys = D('system')->field('system_id,web_tag,web_type,web_title')->find();
    // 如果标记字段为空，生成一个新的，并存盘...
    if( !$dbSys['web_tag'] ) {
      $dbSys['web_tag'] = uniqid();
      D('system')->save($dbSys);
    }
    
    // 获取登录配置信息...
    $this->m_weLogin = C('WECHAT_LOGIN');
    // 注意：$_SERVER['HTTP_HOST'] 自带访问端口...
    // 拼接当前访问页面的完整链接地址 => 登录服务器会反向调用 => 前后端跳转地址不一样...
    $dbJson['node_proto'] = $_SERVER['REQUEST_SCHEME'];
    $dbJson['node_addr'] = $_SERVER['HTTP_HOST'];
    $dbJson['node_url'] = __APP__ . ($bIsAdmin ? "/Login/index" : "/Home/login");
    $dbJson['node_tag'] = $dbSys['web_tag'];
    $dbJson['node_type'] = $dbSys['web_type'];
    $dbJson['node_name'] = $dbSys['web_title'];
    $dbJson['node_ver'] = C('VERSION');
    $state = json_encode($dbJson);
    // 对链接地址进行base64加密...
    $state = urlsafe_b64encode($state);

    // 给模板设定数据 => default/Login/login.htm => default/Home/login.htm
    $this->assign('my_state', $state);
    $this->assign('my_scope', $this->m_weLogin['scope']);
    $this->assign('my_appid', $this->m_weLogin['appid']);
    //$this->assign('my_href', $this->m_weLogin['href']);
    $this->assign('my_redirect_uri', urlencode($this->m_weLogin['redirect_uri']));
    $this->display($bIsAdmin ? "Login:login" : "Home:login");
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