<?php
/*************************************************************
    HaoYi (C)2017 - 2020 myhaoyi.com
*************************************************************/

class AdminAction extends Action
{
  // 调用本模块之前需要调用的接口...
  public function _initialize()
  {
    // 获取系统配置，根据配置设置相关变量 => 强制设置成云教室...
    $this->m_dbSys = D('system')->find();
    $this->m_webTitle = $this->m_dbSys['web_title'];
    $this->m_webType = kCloudEducate;
    // 获取微信登录配置信息 => 网站应用...
    $this->m_weLogin = C('WECHAT_LOGIN');
    // 注意：头像地址已经在网站应用登录时进行了HTTPS修正...
    // 获取登录用户头像，没有登录，直接跳转登录页面...
    $this->m_wxHeadUrl = $this->getLoginHeadUrl();
    // 直接给模板变量赋值...
    $this->assign('my_headurl', $this->m_wxHeadUrl);
    $this->assign('my_web_version', C('VERSION'));
    $this->assign('my_system', $this->m_dbSys);
  }
  // 得到登录用户微信头像链接地址...
  private function getLoginHeadUrl()
  {
    // 用户未登录，跳转登录页面 => 后端登录...
    if( !$this->IsLoginUser() ) {
      header("location:".__APP__.'/Login/index');
      exit; // 注意：这里必须用exit，终止后续代码的执行...
    }
    // return substr_replace($strHeadUrl, "96", strlen($strHeadUrl)-1);
    // 用户已登录，获取登录头像 => 让模版去替换字符串...
    return Cookie::get('wx_headurl');
  }
  // 接口 => 根据cookie判断用户是否已经处于登录状态...
  private function IsLoginUser()
  {
    // 判断需要的cookie是否有效，不用session，避免与微信端混乱...
    if( !Cookie::is_set('wx_unionid') || !Cookie::is_set('wx_headurl') || !Cookie::is_set('wx_usertype') || !Cookie::is_set('wx_shopid') )
      return false;
    // cookie有效，用户已经处于登录状态...
    return true;
  }
  // 用户点击退出操作...
  public function logout()
  {
    ////////////////////////////////////////////////////////////////////
    //注意：不能用Cookie::delete()无法彻底删除，它把空串进行了base64编码...
    ////////////////////////////////////////////////////////////////////

    // 删除存放的用户cookie信息...
    setcookie('wx_unionid','',-1,'/');
    setcookie('wx_headurl','',-1,'/');
    setcookie('wx_usertype','',-1,'/');
    setcookie('wx_shopid','',-1,'/');
    setcookie('auth_days','',-1,'/');
    setcookie('auth_license','',-1,'/');
    setcookie('auth_expired','',-1,'/');

    // 注意：只有页面强制刷新，才能真正删除cookie，所以需要强制页面刷新...
    // 自动跳转到登录页面...
    A('Admin')->index();
  }
  //
  // 默认页面...
  public function index()
  {
    // 获取用户类型编号、所在门店编号...
    $nUserType = Cookie::get('wx_usertype');
    // 如果用户类型编号小于运营维护，跳转到前台页面，不能进行后台操作...
    $strAction = (($nUserType < kMaintainUser) ? '/Index/index' : '/Admin/system');
    // 重定向到最终计算之后的跳转页面...
    header("Location: ".__APP__.$strAction);
  }
  //
  // 获取系统配置页面...
  public function system()
  {
    // 设置标题内容...
    $this->assign('my_title', $this->m_webTitle . " - 系统设置");
    $this->assign('my_command', 'system');
    // 获取传递过来的参数...
    $theOperate = $_POST['operate'];
    if( $theOperate == 'save' ) {
      // http:// 符号已经在前端输入时处理完毕了...
      if( isset($_POST['sys_site']) ) {
        $_POST['sys_site'] = trim(strtolower($_POST['sys_site']));
      }
      // 更新数据库记录，直接存POST数据...
      $_POST['system_id'] = $this->m_dbSys['system_id'];
      D('system')->save($_POST);
    } else {
      // 获取授权状态信息...
      $nAuthDays = Cookie::get('auth_days');
      $bAuthLicense = Cookie::get('auth_license');
      $strWebAuth = $bAuthLicense ? "【永久授权版】，无使用时间限制！" : sprintf("剩余期限【 %d 】天，请联系供应商获取更多授权！", $nAuthDays);
      $this->assign('my_web_auth', $strWebAuth);
      // 获取用户类型，是管理员还是普通用户...
      $nUserType = Cookie::get('wx_usertype');
      $strUnionID = base64_encode(Cookie::get('wx_unionid'));
      $bIsAdmin = (($nUserType==kAdministerUser) ? true : false);
      // 设置是否显示API标识凭证信息...
      $this->assign('my_is_admin', $bIsAdmin);
      $this->assign('my_api_unionid', $strUnionID);
      // 直接返回数据...
      $this->display();
    }
  }
  //
  // 点击查看用户登录信息...
  public function userInfo()
  {
    // cookie失效，通知父窗口页面跳转刷新...
    if( !Cookie::is_set('wx_unionid') || !Cookie::is_set('wx_headurl') || !Cookie::is_set('wx_usertype') || !Cookie::is_set('wx_shopid') ) {
      echo "<script>window.parent.doReload();</script>";
      return;
    }
    // 通过cookie得到unionid，再用unionid查找用户信息...
    $map['wx_unionid'] = Cookie::get('wx_unionid');
    $dbLogin = D('user')->where($map)->find();
    // 无法查找到需要的用户信息，反馈错误...
    if( (count($dbLogin) <= 0) || !isset($dbLogin['user_id']) ) {
      $this->assign('my_msg_title', '无法找到用户信息');
      $this->assign('my_msg_desc', '糟糕，出错了');
      $this->display('Common:error_page');
      return;
    }
    // 进行用户类型和用户性别的替换操作...
    $arrSexType = eval(SEX_TYPE);
    $arrUserType = eval(USER_TYPE);
    $dbLogin['wx_sex'] = $arrSexType[$dbLogin['wx_sex']];
    $dbLogin['user_type'] = $arrUserType[$dbLogin['user_type']];
    // 进行模板设置，让模版去替换头像...
    $this->assign('my_login', $dbLogin);
    $this->display('Admin:userInfo');
  }
  //
  // 获取组件(服务器)配置页面...
  public function server()
  {
    // 设置标题内容...
    $this->assign('my_title', $this->m_webTitle . " - 组件设置");
    $this->assign('my_command', 'server');
    // 获取传递过来的参数...
    $theOperate = $_POST['operate'];
    if( $theOperate == 'save' ) {
      // 更新数据库记录，直接存POST数据...
      $_POST['system_id'] = $this->m_dbSys['system_id'];
      D('system')->save($_POST);
    } else {
      // 直接返回数据...
      $this->display();
    }
  }
  //
  // 获取中转服务器信息...
  public function getTransmit()
  {
    // 查找系统设置记录...
    $dbSys = $this->m_dbSys;
    $dbSys['status'] = $this->connTransmit($dbSys['udpcenter_addr'], $dbSys['udpcenter_port']);
    $this->assign('my_system', $dbSys);
    echo $this->fetch('getTransmit');
  }
  //
  // 获取存储服务器信息...
  public function getTracker()
  {
    // 查找系统设置记录...
    $dbSys = $this->m_dbSys;
    $dbSys['status'] = $this->connTracker($dbSys['tracker_addr'], $dbSys['tracker_port']);
    $this->assign('my_system', $dbSys);
    $my_err = true;
    // 获取存储服务器列表...
    $arrLists = fastdfs_tracker_list_groups();
    foreach($arrLists as $dbKey => $dbGroup) {
      // 这里需要重新初始化，否则会发生粘连...
      $arrData = array();
      // 遍历子数组的内容列表...
      foreach($dbGroup as $dtKey => $dtItem) {
        if( is_array($dtItem) ) {
          $dbData['ip_addr'] = $dtItem['ip_addr'];
          $dbData['storage_http_port'] = $dtItem['storage_http_port'];
          $dbData['storage_port'] = $dtItem['storage_port'];
          $dbData['total_space'] = $dtItem['total_space'];
          $dbData['free_space'] = $dtItem['free_space'];
          $dbData['use_space'] = $dtItem['total_space'] - $dtItem['free_space'];
          $dbData['use_percent'] = round($dbData['use_space']*100/$dtItem['total_space'], 2);
          $dbData['status'] = $dtItem['status'];
          $arrData[$dtKey] = $dbData;
        }
      }
      // 重新组合需要的数据...
      if( count($arrData) > 0 ) {
        $arrGroups[$dbKey] = $arrData;
        $my_err = false;
      }
    }
    // 将新数据赋给模板变量...
    $this->assign('my_groups', $arrGroups);
    $this->assign('my_err', $my_err);
    echo $this->fetch('getTracker');
  }
  //
  // 获取直播服务器信息...
  public function getLive()
  {
    $this->getTransmitCoreData(kCmd_PHP_GetAllServer, 'getLive');
  }
  //
  // 获取所有的在线中转客户端列表...
  public function getClient()
  {
    $this->getTransmitCoreData(kCmd_PHP_GetAllClient, 'getClient');
  }
  //
  // 响应点击测试连接 => Transmit...
  public function testTransmit()
  {
    echo $this->connTransmit($_POST['addr'], $_POST['port']);
  }
  //
  // 响应点击测试连接 => Tracker...
  public function testTracker()
  {
    echo $this->connTracker($_POST['addr'], $_POST['port']);
  }
  //
  // 链接存储服务器，返回 bool ...
  private function connTracker($inAddr, $inPort)
  {
    $tracker = fastdfs_connect_server($inAddr, $inPort);
    if( $tracker ) {
      fastdfs_disconnect_server($tracker);
    }
    return (is_array($tracker) ? 1 : 0);
  }
  //
  // 链接中转服务器，返回 bool ...
  private function connTransmit($inAddr, $inPort)
  {
    // 通过php扩展插件连接中转服务器 => 性能高...
    $transmit = transmit_connect_server($inAddr, $inPort);
    if( $transmit ) {
      transmit_disconnect_server($transmit);
    }
    return (is_array($transmit) ? 1 : 0);
  }  
  //
  // 获取指定服务器下的房间列表...
  public function getRoomList()
  {
    $saveJson = json_encode($_GET);
    $this->assign('my_server_id', $_GET['server_id']);
    $this->getTransmitCoreData(kCmd_PHP_GetRoomList, 'getRoomList', $saveJson);
  }
  //
  // 获取指定服务器、指定通道下的在线用户列表...
  public function getPlayerList()
  {
    $saveJson = json_encode($_GET);
    $this->getTransmitCoreData(kCmd_PHP_GetPlayerList, 'getPlayerList', $saveJson);
  }
  //
  // 只获取中转服务器的数据接口函数...
  private function getTransmitRawData($inCmdID, $inJson = NULL)
  {
    // 设置默认的返回值...
    $dbShow['err_code'] = false;
    $dbShow['err_msg'] = '';
    // 通过php扩展插件连接中转服务器 => 性能高...
    $transmit = transmit_connect_server($this->m_dbSys['udpcenter_addr'], $this->m_dbSys['udpcenter_port']);
    do {
      // 判断连接中转服务器是否成功...
      if( !$transmit ) {
        $dbShow['err_code'] = true;
        $dbShow['err_msg'] = '连接中心服务器失败！';
        break;
      }
      // 连接成功，发送转发命令到中心服务器...
      $json_data = transmit_command(kClientPHP, $inCmdID, $transmit, $inJson);
      // 获取的JSON数据无效...
      if( !$json_data ) {
        $dbShow['err_code'] = true;
        $dbShow['err_msg'] = '获取中心服务器信息失败！';
        break;
      }
      // 转成数组，并判断返回值...
      $arrData = json_decode($json_data, true);
      if( $arrData['err_code'] > 0 ) {
        $dbShow['err_code'] = true;
        $dbShow['err_msg'] = '解析获取的中心服务器信息失败！';
        break;
      }
      // 保存获取到的列表记录信息...
      $dbShow['list_num'] = $arrData['list_num'];
      $dbShow['list_data'] = $arrData['list_data'];
    } while( false );
    // 如果已连接，断开中转服务器...
    if( $transmit ) {
      transmit_disconnect_server($transmit);
    }
    // 返回原始数据结果...
    return $dbShow;
  }
  //
  // 获取中转核心数据接口...
  private function getTransmitCoreData($inCmdID, $inTplName, $inJson = NULL)
  {
    // 获取中转服务器的原始数据接口内容...
    $dbShow = $this->getTransmitRawData($inCmdID, $inJson);
    // 设置模版内容，返回模版数据...
    $this->assign('my_show', $dbShow);
    echo $this->fetch($inTplName);
  }
  //
  // 点击用户管理...
  public function user()
  {
    $this->assign('my_title', $this->m_webTitle . " - 用户管理");
    $this->assign('my_command', 'user');
    // 得到每页条数，总记录数，计算总页数...
    $pagePer = C('PAGE_PER');
    $totalNum = D('user')->count();
    $max_page = intval($totalNum / $pagePer);
    // 判断是否是整数倍的页码...
    $max_page += (($totalNum % $pagePer) ? 1 : 0);
    // 设置最大页数，设置模板参数...
    $this->assign('my_total_num', $totalNum);
    $this->assign('max_page', $max_page);
    $this->display();
  }
  //
  // 获取用户分页数据...
  public function pageUser()
  {
    // 准备需要的分页参数...
    $pagePer = C('PAGE_PER'); // 每页显示的条数...
    $pageCur = (isset($_GET['p']) ? $_GET['p'] : 1);  // 当前页码...
    $pageLimit = (($pageCur-1)*$pagePer).','.$pagePer; // 读取范围...
    // 设置查询条件，查询分页数据，设置模板...
    $arrUser = D('user')->limit($pageLimit)->order('user_id DESC')->select();
    // 对用户记录的表现形式进行修正...
    $arrSexType = eval(SEX_TYPE);
    $arrUserType = eval(USER_TYPE);
    $arrParentType = eval(PARENT_TYPE);
    foreach ($arrUser as &$dbUser) {
      $dbUser['wx_sex'] = $arrSexType[$dbUser['wx_sex']];
      $dbUser['user_type'] = $arrUserType[$dbUser['user_type']];
      $dbUser['parent_type'] = $arrParentType[$dbUser['parent_type']];
    }
    // 设置模板参数，返回模板数据...
    $this->assign('my_user', $arrUser);
    echo $this->fetch('pageUser');
  }
  //
  // 获取单个用户信息...
  public function getUser()
  {
    // 获取用户数据通过用户编号...
    $map['user_id'] = $_GET['user_id'];
    $dbUser = D('user')->where($map)->find();
    // 定义用户身份类型数组内容，需要使用eval...
    $dbUser['arrUserType'] = eval(USER_TYPE);
    $dbUser['arrParentType'] = eval(PARENT_TYPE);
    $dbUser['arrSexType'] = eval(SEX_TYPE);
    // 获取模板数据，返回模版页面内容...
    $this->assign('my_user', $dbUser);
    echo $this->fetch('getUser');
  }
  //
  // 保存用户修改后信息...
  public function saveUser()
  {
    // 更新用户时间之后，直接保存数据库...
    $_POST['updated'] = date('Y-m-d H:i:s');
    D('user')->save($_POST);
  }
}

?>