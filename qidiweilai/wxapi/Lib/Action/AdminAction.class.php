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
    // 获取微信登录配置信息...
    $this->m_weLogin = C('WECHAT_LOGIN');
    // 获取登录用户头像，没有登录，直接跳转登录页面...
    $this->m_wxHeadUrl = $this->getLoginHeadUrl();
    // 判断当前页面是否是https协议 => 通过$_SERVER['HTTPS']和$_SERVER['REQUEST_SCHEME']来判断...
    if((isset($_SERVER['HTTPS']) && $_SERVER['HTTPS'] == 'on') || (isset($_SERVER['REQUEST_SCHEME']) && $_SERVER['REQUEST_SCHEME'] == 'https')) {
      $this->m_wxHeadUrl = str_replace('http://', 'https://', $this->m_wxHeadUrl);
    }
    // 直接给模板变量赋值...
    $this->assign('my_headurl', $this->m_wxHeadUrl);
    $this->assign('my_web_version', C('VERSION'));
    $this->assign('my_system', $this->m_dbSys);
  }
  //
  // 接口 => 根据cookie判断用户是否已经处于登录状态...
  private function IsLoginUser()
  {
    // 判断需要的cookie是否有效，不用session，避免与微信端混乱...
    if( !Cookie::is_set('wx_unionid') || !Cookie::is_set('wx_headurl') || !Cookie::is_set('wx_usertype') || !Cookie::is_set('wx_shopid') )
      return false;
    // cookie有效，用户已经处于登录状态...
    return true;
  }
  //
  // 得到登录用户微信头像链接地址...
  private function getLoginHeadUrl()
  {
    // 用户未登录，跳转登录页面...
    if( !$this->IsLoginUser() ) {
      header("location:".__APP__.'/Login/index');
      exit; // 注意：这里必须用exit，终止后续代码的执行...
    }
    // return substr_replace($strHeadUrl, "96", strlen($strHeadUrl)-1);
    // 用户已登录，获取登录头像 => 让模版去替换字符串...
    return Cookie::get('wx_headurl');
  }
  //
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
    
    /*// 获取用户登录信息 => 返回统一格式 => err_code | err_msg | data
    $dbLogin = A('Login')->getWxUser();
    // err_code < 0 => cookie失效，通知父窗口页面跳转刷新...
    if( $dbLogin['err_code'] < 0 ) {
      echo "<script>window.parent.doReload();</script>";
      return;
    }
    // err_code > 0 => 显示错误页面...
    if( $dbLogin['err_code'] > 0 ) {
      $this->assign('my_msg_title', $dbLogin['err_msg']);
      $this->assign('my_msg_desc', '糟糕，出错了');
      $this->display('Common:error_page');
      return;
    }
    // err_code = 0 => 进行模板设置，让模版去替换头像...
    $this->assign('my_login', $dbLogin['data']);
    $this->display('Admin:userInfo');*/
  }
  /**
  +----------------------------------------------------------
  * 默认操作...
  +----------------------------------------------------------
  */
  public function index()
  {
    // 获取用户类型编号、所在门店编号...
    $nUserType = Cookie::get('wx_usertype');
    // 如果用户类型编号小于运营维护，跳转到前台页面，不能进行后台操作...
    $strAction = (($nUserType < kMaintainUser) ? '/Home/room' : '/Admin/system');
    // 重定向到最终计算之后的跳转页面...
    header("Location: ".__APP__.$strAction);
  }
  //
  // 链接中转服务器，返回 bool ...
  private function connTransmit($inAddr, $inPort)
  {
    /*// 创建并连接服务器...
    $socket = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
    if( !$socket ) return false;
    // 链接指定的服务器和端口...
    $result = socket_connect($socket, $inAddr, $inPort);
    socket_close($socket);
    return $result;*/
    
    // 通过php扩展插件连接中转服务器 => 性能高...
    $transmit = transmit_connect_server($inAddr, $inPort);
    if( $transmit ) {
      transmit_disconnect_server($transmit);
    }
    return (is_array($transmit) ? 1 : 0);
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
  // 响应点击测试连接 => Tracker...
  public function testTracker()
  {
    echo $this->connTracker($_POST['addr'], $_POST['port']);
  }
  //
  // 响应点击测试连接 => Transmit...
  public function testTransmit()
  {
    echo $this->connTransmit($_POST['addr'], $_POST['port']);
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
  // 升级服务 操作页面...
  public function upgrade()
  {
    // 设置标题内容...
    $this->assign('my_title', $this->m_webTitle . " - 系统升级");
    $this->assign('my_command', 'upgrade');
    $this->display();
  }
  //
  // 点击 升级数据库 ...
  public function upDbase()
  {
    // 构造接口需要的连接地址...
    $strUrl = sprintf("https://myhaoyi.com/wxapi.php/Upgrade/upDbase/type/%d", $this->m_webType);
    $theDBRoot = '/weike/mysql/data/educate';
    // 调用接口，获取数据库列表文件...
    $result = http_get($strUrl);
    $arrJson = json_decode($result, true);
    // 遍历获取的文件列表，与本地文件相比较...
    foreach($arrJson as &$dbItem) {
      $theFullPath = sprintf("%s/%s", $theDBRoot, $dbItem['name']);
      $theFileSize = filesize($theFullPath);
      $dbItem['localSize'] = ($theFileSize <= 0 ? 0 : $theFileSize);
      $dbItem['localTime'] = ($theFileSize <= 0 ? '0000-00-00 00:00:00' : date("Y-m-d H:i:s", filemtime($theFullPath)));
      //$dbItem['needUpgrade'] = (($dbItem['localSize'] != $dbItem['size'] || $dbItem['localTime'] != $dbItem['time']) ? true : false);
    }
    // 设置模版参数 => 应用到界面当中...
    $this->assign('my_dbase', $arrJson);
    $this->display();
  }
  //
  // 点击升级数据表...
  public function upDbTable()
  {
    // 准备返回结果...
    $arrErr['err_code'] = false;
    $arrErr['err_msg'] = 'ok';
    // 准备调用接口和参数 => 获取表结构，需要指定云监控或云录播标志...
    $strUrl = "https://myhaoyi.com/wxapi.php/Upgrade/upDbTable";
    $_POST['type'] = $this->m_webType;
    do {
      // 调用接口，解析返回数据...
      $result = http_post($strUrl, $_POST);
      if( !$result ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '无法获取数据表内容';
        break;
      }
      // 获取反馈的详情数据...
      $arrData = json_decode($result, true);
      if( $arrData['err_code'] ) {
        $arrErr['err_code'] = $arrData['err_code'];
        $arrErr['err_msg'] = $arrData['err_msg'];
        break;
      }
      // 根据获取到的数据表结构进行更新本地的数据表...
      $arrFields = $arrData['fields'];
      $tbName = $arrData['table'];
      $theDB = Db::getInstance();
      // 首先判断数据表在本地是否存在...
      $theTableSQL = sprintf("SHOW TABLES LIKE '%s'", $tbName);
      $arrTable = $theDB->query($theTableSQL);
      // 如果本地没有找到数据表，构建新建SQL语句...
      if( count($arrTable) <= 0 ) {
        $theCreateSQL = sprintf("Create Table `%s` (", $tbName);
      }
      // 遍历指定数据表的所有字段...
      $rsCount = count($arrFields);
      foreach($arrFields as $k => $dbItem) {
        // 将需要的字段的属性单独列举出来...
        $field   =  $dbItem['Field'];
        $type    =  $dbItem['Type'];
        $default =  $dbItem['Default'];
        $extra   =  $dbItem['Extra'];
        $null    =  $dbItem['Null'];
        $comment =  $dbItem['Comment'];

        // 注释、默认值、是否为空、关键字...
        $comment = !($comment == '') ? "comment '".$comment."'" : '';
        $default = !($default == '') ? "default '".$default."'" : '';
        $null = ($null == 'NO') ? 'not null' : 'null';
        $key = ($dbItem['Key'] == 'PRI') ? 'primary key' : '';

        // 如果新建数据表，特殊处理...
        if( count($arrTable) <= 0 ) {
          // 累加 SQL 语句...
          $theCreateSQL .= "`$field` $type $null $default $key $extra $comment,";
          // SQL组合完毕，继续下一个字段...
          continue;
        }
        
        // 查找本地数据表是否有指定字段...
        $strDesc = sprintf("DESCRIBE `%s` `%s`", $tbName, $field);
        $arrLocal = $theDB->query($strDesc);
        // 有字段，修改操作，没有字段，添加操作...
        $strCmd = (count($arrLocal) > 0 ? 'MODIFY' : 'ADD');
        // 如果是修改字段，不能有关键字，会报错...
        $key = (count($arrLocal) > 0 ? '' : $key);
        
        // 准备马上执行的SQL语句...
        $strSQL  = sprintf("ALTER TABLE `%s` %s `%s` ", $tbName, $strCmd, $field);
        $strSQL .= "$type $null $default $key $extra $comment";
        // 立即执行SQL语句...        
        $theDB->query($strSQL);
      }
      // 如果是新建数据表，特殊处理 => 最后一个逗号需要替换...
      if( count($arrTable) <= 0 ) {
        $theCreateSQL .= ") ENGINE=MyISAM DEFAULT CHARSET=utf8;";
        $theCreateSQL = str_replace(',)', ')', $theCreateSQL);
        $theDB->execute($theCreateSQL);
      }
      // 获取修改后的表结构文件的大小和修改时间...
      $theDBRoot = '/weike/mysql/data/educate';
      $theFullPath = sprintf("%s/%s.frm", $theDBRoot, $tbName);
      $theFileSize = filesize($theFullPath);
      $arrErr['localSize'] = ($theFileSize <= 0 ? 0 : $theFileSize);
      $arrErr['localTime'] = date("Y-m-d H:i:s", filemtime($theFullPath));
    } while( false );
    // 返回json数据包...
    echo json_encode($arrErr);
  }
  //
  // 点击 升级网站代码...
  public function upWeb()
  {
    $this->display();
  }
  // 发起下载文件的命令...
  /*public function upDbFile()
  {
    // 准备返回结果...
    $arrErr['err_code'] = false;
    $arrErr['err_msg'] = 'ok';
    // 准备调用接口和参数...
    $strUrl = "https://myhaoyi.com/wxapi.php/Upgrade/upDbFile";
    $_POST['type'] = $this->m_webType;
    do {
      // 调用接口，解析返回数据...
      $result = http_post($strUrl, $_POST);
      if( !$result ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '无法获取文件内容';
        break;
      }
      // 直接将获取的数据存放到临时目录...
      $theName = sprintf("/tmp/%s", $_POST['name']);
      $theFile = fopen($theName, 'w+');
      fwrite($theFile, $result);
      fclose($theFile);
      // 根据命令类型进行分发操作...
    } while( false );
    // 返回json数据包...
    echo json_encode($arrErr);
  }*/
  //
  // 获取采集端页面...
  public function gather()
  {
    $this->assign('my_title', $this->m_webTitle . " - 采集端管理");
    $this->assign('my_command', 'gather');
    // 得到每页条数，总记录数，计算总页数...
    $pagePer = C('PAGE_PER');
    $totalNum = D('gather')->count();
    $max_page = intval($totalNum / $pagePer);
    // 判断是否是整数倍的页码...
    $max_page += (($totalNum % $pagePer) ? 1 : 0);
    // 设置最大页数，设置模板参数...
    $this->assign('my_total_num', $totalNum);
    $this->assign('max_page', $max_page);
    $this->display();
  }
  //
  // 获取采集端分页数据...
  public function pageGather()
  {
    // 加载 ThinkPHP 的扩展函数 => ThinkPHP/Common/extend.php => msubstr()
    //Load('extend');
    
    // 准备需要的分页参数...
    $pagePer = C('PAGE_PER'); // 每页显示的条数...
    $pageCur = (isset($_GET['p']) ? $_GET['p'] : 1);  // 当前页码...
    $pageLimit = (($pageCur-1)*$pagePer).','.$pagePer; // 读取范围...
    // 查询分页数据，准备中转查询数据，设置默认状态...
    $arrGather = D('gather')->limit($pageLimit)->order('gather_id DESC')->select();
    // 2017.11.12 - by jackey => 采集端状态直接从数据库当中读取，不从中转服务器读取...
    /*// 获取采集端在线状态标志，设置模板参数 => 有记录才查询...
    if( count($arrGather) > 0 ) {
      $this->getGatherStatusFromTransmit($arrGather);
    }*/
    $this->assign('my_gather', $arrGather);
    echo $this->fetch('pageGather');
  }
  //
  // 获取采集端详情数据...
  public function getGather()
  {
    $map['gather_id'] = $_GET['gather_id'];
    $dbGather = D('gather')->where($map)->find();
    // 云监控模式 => 不用获取学校列表...
    $this->assign('my_gather', $dbGather);
    echo $this->display();
  }
  //
  // 保存修改后的采集端数据...
  public function saveGather()
  {
    // 现将数据直接存放到数据库当中...
    $_POST['updated'] = date('Y-m-d H:i:s');
    D('gather')->save($_POST);
    // 专门判断mac_addr是否有效...
    if( !isset($_POST['mac_addr']) ) {
      echo '没有设置mac_addr，无法中转命令！';
      return;
    }
    // 将数据转发给指定的采集端...
    $transmit = transmit_connect_server($this->m_dbSys['transmit_addr'], $this->m_dbSys['transmit_port']);
    // 连接失败...
    if( !$transmit ) {
      echo '连接中转服务器失败！';
      return;
    }
    // 连接成功，执行中转命令...
    $saveJson = json_encode($_POST);
    $json_data = transmit_command(kClientPHP, kCmd_PHP_Set_Gather_SYS, $transmit, $saveJson);
    transmit_disconnect_server($transmit);
    echo $json_data;
  }
  //
  // 添加直播通道...
  public function addCamera()
  {
    // 先将数据存盘...
    $dbCamera = $_POST;
    $dbCamera['created'] = date('Y-m-d H:i:s');
    $dbCamera['updated'] = date('Y-m-d H:i:s');
    // 计算device_sn标识符号 => 使用多重操作...
    //list($msec, $sec) = explode(' ', microtime());
    //$msectime = (float)sprintf('%.0f', (floatval($msec) + floatval($sec)) * 10000);
    $dbCamera['device_sn'] = md5(uniqid(md5(microtime(true)),true));
    $dbCamera['camera_id'] = D('camera')->add($dbCamera);

    // 获取采集端的mac_addr地址...
    $condition['gather_id'] = $dbCamera['gather_id'];
    $dbGather = D('gather')->where($condition)->field('mac_addr')->find();
    $dbCamera['mac_addr'] = $dbGather['mac_addr'];
    // 连接中转服务器...
    $transmit = transmit_connect_server($this->m_dbSys['transmit_addr'], $this->m_dbSys['transmit_port']);
    // 连接成功，执行中转命令...
    if( $transmit ) {
      // 将参数转换成json数据包，发起添加命令...
      $saveJson = json_encode($dbCamera);
      $json_data = transmit_command(kClientPHP, kCmd_PHP_Set_Camera_Add, $transmit, $saveJson);
      // 关闭中转服务器链接...
      transmit_disconnect_server($transmit);
      // 返回转发结果...
      echo $json_data;
    }
  }
  //
  // 删除直播通道列表...
  public function delCamera()
  {
    //////////////////////////////////
    // 下面是针对采集端的删除操作...
    //////////////////////////////////
    // 组合通道查询条件...
    $map['camera_id'] = array('in', $_POST['list']);
    // 连接中转服务器...
    $transmit = transmit_connect_server($this->m_dbSys['transmit_addr'], $this->m_dbSys['transmit_port']);
    // 连接成功，执行中转命令...
    if( $transmit ) {
      // 查询通道列表...
      $arrLive = D('LiveView')->where($map)->field('camera_id,gather_id,mac_addr')->select();
      // 遍历每一个通道，发起删除指令...
      foreach($arrLive as &$dbItem) {
        $saveJson = json_encode($dbItem);
        $json_data = transmit_command(kClientPHP, kCmd_PHP_Set_Camera_Del, $transmit, $saveJson);
      }
      // 关闭中转服务器链接...
      transmit_disconnect_server($transmit);
    }
    ///////////////////////////////////
    // 下面是针对数据库的删除操作...
    ///////////////////////////////////
    // 1. 删除该通道下对应的录像课程表...
    D('course')->where($map)->delete();
    // 2. 删除该通道下对应的录像文件、录像截图...
    $arrList = D('RecordView')->where($map)->field('record_id,camera_id,file_fdfs,image_id,image_fdfs')->select();
    foreach ($arrList as &$dbVod) {
      // 删除图片和视频文件，逐一删除...
      fastdfs_storage_delete_file1($dbVod['file_fdfs']);
      fastdfs_storage_delete_file1($dbVod['image_fdfs']);
      // 删除图片记录和视频记录...
      D('record')->delete($dbVod['record_id']);
      D('image')->delete($dbVod['image_id']);
    }
    // 3.1 找到通道下所有的图片记录...
    $arrImage = D('image')->where($map)->field('image_id,camera_id,file_fdfs')->select();
    // 3.2 删除通道快照图片的物理存在 => 逐一删除...
    foreach($arrImage as &$dbImage) {
      fastdfs_storage_delete_file1($dbImage['file_fdfs']);
    }
    // 3.3 删除通道快照图片记录的数据库存在 => 一次性删除...
    D('image')->where($map)->delete();
    // 4. 直接删除通道列表数据...
    D('camera')->where($map)->delete();
    ///////////////////////////////////
    // 下面是重新获取新的分页数据...
    ///////////////////////////////////
    // 得到每页条数，总记录数，计算总页数...
    $pagePer = C('PAGE_PER');
    $totalNum = D('camera')->count();
    $max_page = intval($totalNum / $pagePer);
    // 判断是否是整数倍的页码...
    $max_page += (($totalNum % $pagePer) ? 1 : 0);
    // 重新计算当前页面编号，总页面数...
    if( $max_page <= 0 ) {
      $arrJson['curr'] = 0;
      $arrJson['pages'] = 0;
      $arrJson['total'] = $totalNum;
    } else {
      $nCurPage = (($_POST['page'] > $max_page) ? $max_page : $_POST['page']);
      $arrJson['curr'] = $nCurPage;
      $arrJson['pages'] = $max_page;
      $arrJson['total'] = $totalNum;
    }
    // 返回json数据包...
    echo json_encode($arrJson);
  }
  //
  // 保存修改后的摄像头数据...
  public function modCamera()
  {
    // 准备返回数据...
    $arrData['err_code'] = false;
    $arrData['err_msg'] = 'OK';

    // 对device_pass进行base64处理...
    if( isset($_POST['device_pass']) ) {
      $_POST['device_pass'] = base64_encode($_POST['device_pass']);
    }

    // 先将摄像头信息存入数据库当中...
    $_POST['updated'] = date('Y-m-d H:i:s');
    D('camera')->save($_POST);

    // 再将摄像头名称转发给对应的采集端...
    
    // 通过php扩展插件连接中转服务器 => 性能高...
    $transmit = transmit_connect_server($this->m_dbSys['transmit_addr'], $this->m_dbSys['transmit_port']);
    if( !$transmit ) {
      $arrData['err_code'] = true;
      $arrData['err_msg'] = '无法连接中转服务器。';
      echo json_encode($arrData);
      return;
    }
    // 转发设置摄像头配置到采集端...
    // 获取转发节点的MAC地址...
    $map['gather_id'] = $_POST['gather_id'];
    $dbGather = D('gather')->where($map)->field('mac_addr')->find();
    // 组合摄像头数据成JSON...
    $dbCamera = $_POST;
    unset($dbCamera['updated']);
    $dbCamera['mac_addr'] = $dbGather['mac_addr'];
    $saveJson = json_encode($dbCamera);
    // 发送转发命令...
    $json_data = transmit_command(kClientPHP, kCmd_PHP_Set_Camera_Mod, $transmit, $saveJson);
    // 关闭中转服务器链接...
    transmit_disconnect_server($transmit);
    // 反馈转发结果...
    echo $json_data;
  }
  //
  // 操作指定通道 => 启动或停止...
  public function doCamera()
  {
    $camera_id = $_GET['camera_id'];
    $map['camera_id'] = $camera_id;
    $dbLive = D('LiveView')->where($map)->field('camera_id,status,mac_addr')->find();
    // 准备命令需要的数据 => 当前是停止状态，则发起启动；是启动状态，则发起停止...
    $theCmd = (($dbLive['status'] > 0) ? kCmd_PHP_Stop_Camera : kCmd_PHP_Start_Camera );
    // 开始连接中转服务器...
    $transmit = transmit_connect_server($this->m_dbSys['transmit_addr'], $this->m_dbSys['transmit_port']);
    // 链接中转服务器失败，直接返回...
    if( !$transmit ) {
      $arrData['err_code'] = true;
      $arrData['err_cmd'] = $theCmd;
      $arrData['err_msg'] = '无法连接中转服务器。';
      echo json_encode($arrData);
      return;
    }
    // 构造中转服务器需要的参数...
    $dbParam['camera_id'] = $camera_id;
    $dbParam['mac_addr'] = $dbLive['mac_addr'];
    $saveJson = json_encode($dbParam);
    $json_data = transmit_command(kClientPHP, $theCmd, $transmit, $saveJson);
    transmit_disconnect_server($transmit);
    // 获取的JSON数据有效，转成数组，直接返回...
    $arrData = json_decode($json_data, true);
    $arrData['err_cmd'] = $theCmd;
    if( !$arrData ) {
      $arrData['err_code'] = true;
      $arrData['err_msg'] = '从中转服务器获取数据失败。';
    } else {
      // 通过错误码，获得错误信息...
      $arrData['err_msg'] = getTransmitErrMsg($arrData['err_code']);
    }
    // 将数组再次转换成json返回...
    echo json_encode($arrData);
  }
  //
  // 读取通道的运行状态...
  public function getCameraStatus()
  {
    $map['camera_id'] = $_GET['camera_id'];
    $dbCamera = D('camera')->where($map)->field('camera_id,status,err_code,err_msg')->find();
    echo json_encode($dbCamera);
  }
  //
  // 获取编辑时间对话框...
  public function getClock()
  {
    // 根据网站类型，传递不同数据...
    $arrJson['week'] = date('w');
    $arrJson['webType'] = $this->m_webType;
    $arrJson['defStart'] = date('H:i:s');
    $arrJson['defEnd'] = date("H:i:s", strtotime("+1 hour"));
    $arrJson['width'] = ($this->m_webType == kCloudRecorder) ? '500px' : '450px';
    $arrJson['height'] = ($this->m_webType == kCloudRecorder) ? '530px' : '440px';
    $arrJson['data'] = $this->fetch('getClock');
    echo json_encode($arrJson);
  }
  //
  // 获取复制星期对话框...
  public function getWeek()
  {
    // 设置星期数据，按照容易理解的顺序排列...
    $week_id = $_GET['week_id'];
    $arrWeek = array(
      array('id' => '1', 'name' => '星期一', 'today' => ((1==$week_id) ? 'checked disabled' : '')),
      array('id' => '2', 'name' => '星期二', 'today' => ((2==$week_id) ? 'checked disabled' : '')),
      array('id' => '3', 'name' => '星期三', 'today' => ((3==$week_id) ? 'checked disabled' : '')),
      array('id' => '4', 'name' => '星期四', 'today' => ((4==$week_id) ? 'checked disabled' : '')),
      array('id' => '5', 'name' => '星期五', 'today' => ((5==$week_id) ? 'checked disabled' : '')),
      array('id' => '6', 'name' => '星期六', 'today' => ((6==$week_id) ? 'checked disabled' : '')),
      array('id' => '0', 'name' => '星期日', 'today' => ((0==$week_id) ? 'checked disabled' : ''))
    );
    // 设置模版参数...
    $this->assign('my_week', $arrWeek);
    echo $this->fetch('getWeek');
  }
  //
  // 获取摄像头下面的录像表 => 始终按每周排列...
  public function course()
  {
    // 设置星期数据，按照容易理解的顺序排列...
    $week_id = date('w');
    $arrWeek = array(
      array('id' => '1', 'name' => '星期一', 'today' => ((1==$week_id) ? 1 : 0)),
      array('id' => '2', 'name' => '星期二', 'today' => ((2==$week_id) ? 1 : 0)),
      array('id' => '3', 'name' => '星期三', 'today' => ((3==$week_id) ? 1 : 0)),
      array('id' => '4', 'name' => '星期四', 'today' => ((4==$week_id) ? 1 : 0)),
      array('id' => '5', 'name' => '星期五', 'today' => ((5==$week_id) ? 1 : 0)),
      array('id' => '6', 'name' => '星期六', 'today' => ((6==$week_id) ? 1 : 0)),
      array('id' => '0', 'name' => '星期日', 'today' => ((0==$week_id) ? 1 : 0))
    );
    // 获取传递过来的参数信息...
    $theNavType = $_GET['type'];
    $theCameraID = $_GET['camera_id'];
    $theGatherID = $_GET['gather_id'];
    // 需要根据类型不同，设置不同的焦点类型...
    $this->assign('my_title', $this->m_webTitle . " - 录像计划");
    $this->assign('my_command', (($theNavType == 'camera') ? 'gather' : 'live'));
    // 获取通道信息...
    $map['camera_id'] = $theCameraID;
    $dbCamera = D('camera')->where($map)->field('camera_id,camera_name')->find();
    $this->assign('my_camera', $dbCamera);
    // 获取通道下所有的录像任务...
    //$arrCourse = D('course')->where($map)->select();
    //$this->assign('my_course', ($arrCourse ? json_encode($arrCourse) : false));
    //$this->assign('my_total_num', count($arrCourse));
    $theCount = D('course')->where($map)->count();
    $this->assign('my_total_num', $theCount);
    // 设置需要的模板参数信息...
    $this->assign('my_camera_id', $theCameraID);
    $this->assign('my_gather_id', $theGatherID);
    $this->assign('my_nav_type', $theNavType);
    $this->assign('my_week', $arrWeek);
    $this->display();
  }
  //
  // 查找指定通道下面所有的录像任务记录...
  public function getCourse()
  {
    $condition['camera_id'] = $_GET['camera_id'];
    $arrCourse = D('course')->where($condition)->select();
    echo ($arrCourse ? json_encode($arrCourse) : false);
  }
  //
  // 保存任务录像区间数据...
  public function saveCourse()
  {
    // 将获取的json数据转换成数组...
    $arrNew = array();
    $arrCourse = json_decode($_POST['data'], true);
    foreach($arrCourse as &$dbCourse) {
      if( isset($dbCourse['start_time']) ) {
        $dbCourse['start_time'] = sprintf("%s %s", date('Y-m-d'), $dbCourse['start_time']);
      }
      if( isset($dbCourse['end_time']) ) {
        $dbCourse['end_time'] = sprintf("%s %s", date('Y-m-d'), $dbCourse['end_time']);
      }
      if( $dbCourse['course_id'] > 0 ) {
        // 检查删除标志...
        if( $dbCourse['is_delete'] ) {
          // 删除指定编号的记录...
          $map['course_id'] = $dbCourse['course_id'];
          D('course')->where($map)->delete();
          // 设置删除标志，供采集端使用...
          $dbCourse['is_delete'] = 3;
        } else {
          // 更新原有数据...
          $dbCourse['updated'] = date('Y-m-d H:i:s');
          D('course')->save($dbCourse);
          // 设置修改标志，供采集端使用...
          $dbCourse['is_delete'] = 2;
        }
      } else {
        // 新建记录...
        unset($dbCourse['course_id']);
        $dbCourse['created'] = date('Y-m-d H:i:s');
        $dbCourse['updated'] = date('Y-m-d H:i:s');
        $dbCourse['course_id'] = D('course')->add($dbCourse);
        // 构造返回的数据内容...
        array_push($arrNew, array('slider_id' => $dbCourse['slider_id'], 'range_id' => $dbCourse['range_id'], 'course_id' => $dbCourse['course_id']));
        // 设置添加标志，供采集端使用...
        $dbCourse['is_delete'] = 1;
      }
    }
    // 转发命令到采集端...
    $arrResult['transmit'] = $this->postCourseRecordToGather($_GET['camera_id'], $_GET['gather_id'], $arrCourse);
    $arrResult['create'] = ($arrNew ? json_encode($arrNew) : false);
    // 返回新创建的ID编号集合+转发采集端结果...
    echo json_encode($arrResult);
  }
  //
  // 转发任务录像记录命令到采集端...
  private function postCourseRecordToGather($inCameraID, $inGatherID, &$arrCourse)
  {
    // 通过php扩展插件连接中转服务器 => 性能高...
    $transmit = transmit_connect_server($this->m_dbSys['transmit_addr'], $this->m_dbSys['transmit_port']);
    if( !$transmit ) return false;
    // 保存转发需要的数据...
    $dbTrasmit['data'] = $arrCourse;
    $dbTrasmit['camera_id'] = $inCameraID;
    // 需要对日期格式进行转换...
    foreach($dbTrasmit['data'] as &$dbItem) {
      if( isset($dbItem['start_time']) ) {
        $dbItem['start_time'] = strtotime($dbItem['start_time']);
      }
      if( isset($dbItem['end_time']) ) {
        $dbItem['end_time'] = strtotime($dbItem['end_time']);
      }
    }
    // 转发课表记录命令 => 修改 => 只剩下一个修改命令...
    $map['gather_id'] = $inGatherID;
    $dbGather = D('gather')->where($map)->field('mac_addr')->find();
    $dbTrasmit['mac_addr'] = $dbGather['mac_addr'];
    // 组合课表数据成JSON...
    $saveJson = json_encode($dbTrasmit);
    // 发送转发命令...
    $nCmdType = kCmd_PHP_Set_Course_Mod;
    $json_data = transmit_command(kClientPHP, $nCmdType, $transmit, $saveJson);
    // 关闭中转服务器链接...
    transmit_disconnect_server($transmit);
    // 反馈转发结果 => 这个命令无需等待采集端返回...
    $arrData = json_decode($json_data, true);
    $arrData['err_msg'] = getTransmitErrMsg($arrData['err_code']);
    return json_encode($arrData);
  }
  //
  // 获取直播教室管理页面...
  public function room()
  {
    $this->assign('my_title', $this->m_webTitle . " - 教室管理");
    $this->assign('my_command', 'room');
    // 得到每页条数，总记录数，计算总页数...
    $pagePer = C('PAGE_PER');
    $totalNum = D('room')->count();
    $max_page = intval($totalNum / $pagePer);
    // 判断是否是整数倍的页码...
    $max_page += (($totalNum % $pagePer) ? 1 : 0);
    // 设置最大页数，设置模板参数...
    $this->assign('my_begin_id', LIVE_BEGIN_ID);
    $this->assign('my_total_num', $totalNum);
    $this->assign('max_page', $max_page);
    $this->display();
  }
  //
  // 获取直播教室分页数据...
  public function pageRoom()
  {
    // 加载 ThinkPHP 的扩展函数 => ThinkPHP/Common/extend.php => msubstr()
    Load('extend');
    
    // 准备需要的分页参数...
    $pagePer = C('PAGE_PER'); // 每页显示的条数...
    $pageCur = (isset($_GET['p']) ? $_GET['p'] : 1);  // 当前页码...
    $pageLimit = (($pageCur-1)*$pagePer).','.$pagePer; // 读取范围...
    // 设置查询条件，查询分页数据，设置模板...
    $arrRoom = D('RoomView')->limit($pageLimit)->order('room_id DESC')->select();
    // 设置模板参数，返回模板数据...
    $this->assign('my_begin_id', LIVE_BEGIN_ID);
    $this->assign('my_room', $arrRoom);
    echo $this->fetch('pageRoom');
  }
  //
  // 获取直播教室单条记录信息...
  public function getRoom()
  {
    $condition['room_id'] = $_GET['room_id'];
    $dbRoom = D('RoomView')->where($condition)->find();
    $dbRoom['poster_fdfs'] = sprintf("%s:%d/%s", $this->m_dbSys['web_tracker_addr'], $this->m_dbSys['web_tracker_port'], $dbRoom['poster_fdfs']);
    // 查找所有的老师用户列表，根据用户类型查询...
    $map['user_type'] = array('eq', kTeacherUser);
    $dbRoom['arrTeacher'] = D('user')->where($map)->field('user_id,real_name,wx_nickname')->select();
    // 查找所有的科目记录列表...
    $dbRoom['arrSubject'] = D('subject')->select();
    // 赋值给模版对象...
    $this->assign('my_room', $dbRoom);
    // 返回构造好的数据...
    echo $this->fetch('getRoom');
  }
  //
  // 响应上传事件...
  public function upload()
  {
    // 准备结果数组...
    $arrErr['err_code'] = false;
    $arrErr['err_msg'] = "";
    // 实例化上传类, 设置附件上传目录
    import("@.ORG.UploadFile");
    $myUpload = new UploadFile();
    $myUpload->saveRule = 'uniqid';
    $myUpload->savePath = APP_PATH . '/upload/';
    // 上传错误提示错误信息
    if( !$myUpload->upload() ) {
      $arrErr['err_code'] = true;
      $arrErr['err_msg'] = $myUpload->getErrorMsg();
    } else {
      // 从上传对象当中读取需要的信息...
      $arrFile = $myUpload->getUploadFileInfo();
      $theSize = $arrFile[0]['size'];
      $theName = $arrFile[0]['savename'];
      $thePath = $arrFile[0]['savepath'];
      $theExt  = $arrFile[0]['extension'];
      // 组合完整绝对路径名称地址，相对路径需要删除第一个字符...
      $strUploadPath = WK_ROOT . substr($thePath, 1) . $theName;
      // 构造fdfs对象，上传、返回文件ID...
      $fdfs = new FastDFS();
      $tracker = $fdfs->tracker_get_connection();
      $strPosterImg = $fdfs->storage_upload_by_filebuff1(file_get_contents($strUploadPath), $theExt);
      $fdfs->tracker_close_all_connections();
      // 构造返回的海报新地址信息...
      $arrErr['poster_fdfs'] = sprintf("%s:%d/%s", $this->m_dbSys['web_tracker_addr'], $this->m_dbSys['web_tracker_port'], $strPosterImg);
      // 删除已经上传到fdfs的本地临时文件...
      unlink($strUploadPath);
      // 通过房间编号查找房间记录...
      $condition['room_id'] = $_GET['room_id'];
      $dbRoom = D('RoomView')->where($condition)->field('room_id,poster_id,poster_fdfs')->find();
      do {
        // 构造海报图片记录字段信息...
        $dbImage['file_fdfs'] = $strPosterImg;
        $dbImage['file_size'] = $theSize;
        $dbImage['created'] = date('Y-m-d H:i:s');
        $dbImage['updated'] = date('Y-m-d H:i:s');
        // 如果有海报记录，先删除图片再更新记录...
        if( is_array($dbRoom) && $dbRoom['poster_id'] > 0 ) {
          // 判断该房间下的海报是否有效，先删除这个海报的物理存在...
          if( isset($dbRoom['poster_fdfs']) && strlen($dbRoom['poster_fdfs']) > 0 ) { 
            if( !fastdfs_storage_delete_file1($dbRoom['poster_fdfs']) ) {
              logdebug("fdfs delete failed => ".$dbRoom['poster_fdfs']);
            }
          }
          // 将新的海报存储路径更新到图片表当中...
          $dbImage['image_id'] = $dbRoom['poster_id'];
          D('image')->save($dbImage);
        } else {
          // 房间里的海报是无效的，创建新的图片记录...
          $arrErr['poster_id'] = D('image')->add($dbImage);
          // 将对应的海报编号更新到房间记录当中...
          if( is_array($dbRoom) && $dbRoom['room_id'] > 0 ) {
            $dbRoom['poster_id'] = $arrErr['poster_id'];
            D('room')->save($dbRoom);
          }
        }
      } while( false );
    }
    // 直接返回结果json...
    echo json_encode($arrErr);
  }
  //
  // 添加房间记录信息...
  public function addRoom()
  {
    $dbRoom = $_POST;
    $dbRoom['created'] = date('Y-m-d H:i:s');
    $dbRoom['updated'] = date('Y-m-d H:i:s');
    D('room')->add($dbRoom);
  }
  //
  // 修改房间记录信息...
  public function modRoom()
  {
    $condition['room_id'] = $_POST['room_id'];
    D('room')->where($condition)->save($_POST);
  }
  //
  // 删除房间记录信息...
  public function delRoom()
  {
    // 组合通道查询条件房间记录...
    $map['room_id'] = array('in', $_POST['list']);
    $arrRoom = D('RoomView')->where($map)->field('room_id,image_id,image_fdfs,poster_id,poster_fdfs')->select();
    // 遍历房间记录列表，删除对应的截图和海报记录...
    foreach ($arrRoom as &$dbRoom) {
      // 删除图片和视频文件，逐一删除...
      fastdfs_storage_delete_file1($dbRoom['image_fdfs']);
      fastdfs_storage_delete_file1($dbRoom['poster_fdfs']);
      // 删除截图图片记录和海报图片记录，只能逐条删除...
      D('image')->delete($dbRoom['image_id']);
      D('image')->delete($dbRoom['poster_id']);
    }
    // 直接删除房间列表数据记录...
    D('room')->where($map)->delete();
    ///////////////////////////////////
    // 下面是重新获取新的分页数据...
    ///////////////////////////////////
    // 得到每页条数，总记录数，计算总页数...
    $pagePer = C('PAGE_PER');
    $totalNum = D('room')->count();
    $max_page = intval($totalNum / $pagePer);
    // 判断是否是整数倍的页码...
    $max_page += (($totalNum % $pagePer) ? 1 : 0);
    // 重新计算当前页面编号，总页面数...
    if( $max_page <= 0 ) {
      $arrJson['curr'] = 0;
      $arrJson['pages'] = 0;
      $arrJson['total'] = $totalNum;
    } else {
      $nCurPage = (($_POST['page'] > $max_page) ? $max_page : $_POST['page']);
      $arrJson['curr'] = $nCurPage;
      $arrJson['pages'] = $max_page;
      $arrJson['total'] = $totalNum;
    }
    // 返回json数据包...
    echo json_encode($arrJson);
  }
  //
  // 获取新通道页面...
  public function getLiveCamera()
  {
    // 获取采集端列表...
    $arrGather = D('gather')->field('gather_id,name_pc,ip_addr')->order('gather_id DESC')->select();
    // 没有采集端，返回错误...
    if( count($arrGather) < 0 ) {
      $this->assign('my_msg_title', '没有【采集端】，无法添加直播通道！');
      echo $this->fetch('Common:error_page');
      return;
    }
    // 设置采集端列表...
    $this->assign('my_list_gather', $arrGather);
    // 设置默认的采集端、流类型、年级...
    $dbCamera['stream_prop'] = 1;
    $dbCamera['gather_id'] = $arrGather[0]['gather_id'];
    $this->assign('my_live', $dbCamera);
    // 返回构造好的数据...
    echo $this->fetch('liveCamera');
  }
  //
  // 获取直播播放地址...
  public function getLiveAddress()
  {
    // 传递过来的通道编号...
    $theLiveID = $_GET['camera_id'];
    // 首先，获取中转服务器的原始数据内容 => 直播服务器列表...
    $dbShow = $this->getTransmitRawData(kCmd_PHP_Get_Live_Server);
    if( $dbShow['err_code'] ) {
      // 获取直播服务器失败的处理...
      $this->assign('my_msg_title', $dbShow['err_msg']);
      $this->assign('my_msg_desc', '糟糕，出错了');
      $this->display('Common:error_page');
    } else {
      // 获取直播服务器成功的处理...
      $dbServer = $dbShow['list'][0];
      $this->assign('my_rtmp', sprintf("rtmp://%s/live/live%d", $dbServer[0], $theLiveID));
      $this->assign('my_flv', sprintf("http://%s/live/live%d.flv", $dbServer[1], $theLiveID));
      $this->assign('my_hls', sprintf("http://%s/live/live%d.m3u8", $dbServer[1], $theLiveID));
      // 直接返回直播播放地址页面内容...
      $this->display('liveAddress');
    }
  }
  //
  // 获取点播管理页面...
  public function vod()
  {
    $this->assign('my_title', $this->m_webTitle . " - 录像管理");
    $this->assign('my_command', 'vod');
    // 得到每页条数，总记录数，计算总页数...
    $pagePer = C('PAGE_PER');
    $totalNum = D('record')->count();
    $max_page = intval($totalNum / $pagePer);
    // 判断是否是整数倍的页码...
    $max_page += (($totalNum % $pagePer) ? 1 : 0);
    // 设置最大页数，设置模板参数...
    $this->assign('my_total_num', $totalNum);
    $this->assign('max_page', $max_page);
    $this->display();
  }
  //
  // 获取点播分页数据...
  public function pageVod()
  {
    // 加载 ThinkPHP 的扩展函数 => ThinkPHP/Common/extend.php => msubstr()
    //Load('extend');
    
    // 准备需要的分页参数...
    $pagePer = C('PAGE_PER'); // 每页显示的条数...
    $pageCur = (isset($_GET['p']) ? $_GET['p'] : 1);  // 当前页码...
    $pageLimit = (($pageCur-1)*$pagePer).','.$pagePer; // 读取范围...
    // 设置查询条件，查询分页数据，设置模板...
    $arrVod = D('RecordView')->limit($pageLimit)->order('record_id DESC')->select();
    // 设置模板参数，返回模板数据...
    $this->assign('my_vod', $arrVod);
    echo $this->fetch('pageVod');
  }
  //
  // 获取点播详情页面...
  public function getVod()
  {
    // 云监控 => 读取通道列表...
    $arrCamera = D('camera')->field('camera_id,camera_name')->select();
    $this->assign('my_list_camera', $arrCamera);
    // 获取录像记录需要的信息 => web_tracker_addr 已经自带了协议头 http://或https://
    $map['record_id'] = $_GET['record_id'];
    $dbVod = D('RecordView')->where($map)->find();
    $dbVod['image_url'] = sprintf("%s:%d/%s_470x250", $this->m_dbSys['web_tracker_addr'], $this->m_dbSys['web_tracker_port'], $dbVod['image_fdfs']);
    $this->assign('my_vod', $dbVod);
    // 获取模板数据...
    echo $this->fetch('getVod');
  }
  //
  // 保存点播信息...
  public function saveVod()
  {
    // 云监控模式...
    $dbSave['updated'] = date('Y-m-d H:i:s');
    $dbSave['record_id'] = $_POST['record_id'];
    $dbSave['camera_id'] = $_POST['camera_id'];
    D('record')->save($dbSave);
  }
  //
  // 删除点播文件列表...
  public function delVod()
  {
    // 通过ID编号列表获取录像记录
    $map['record_id'] = array('in', $_POST['list']);
    $arrList = D('RecordView')->where($map)->field('record_id,camera_id,file_fdfs,image_id,image_fdfs')->select();
    foreach ($arrList as &$dbItem) {
      // 删除图片和视频文件，逐一删除...
      fastdfs_storage_delete_file1($dbItem['file_fdfs']);
      fastdfs_storage_delete_file1($dbItem['image_fdfs']);
      // 删除图片记录和视频记录...
      D('record')->delete($dbItem['record_id']);
      D('image')->delete($dbItem['image_id']);
    }
    // 得到每页条数，总记录数，计算总页数...
    $pagePer = C('PAGE_PER');
    $totalNum = D('record')->count();
    $max_page = intval($totalNum / $pagePer);
    // 判断是否是整数倍的页码...
    $max_page += (($totalNum % $pagePer) ? 1 : 0);
    // 重新计算当前页面编号，总页面数...
    if( $max_page <= 0 ) {
      $arrJson['curr'] = 0;
      $arrJson['pages'] = 0;
      $arrJson['total'] = $totalNum;
    } else {
      $nCurPage = (($_POST['page'] > $max_page) ? $max_page : $_POST['page']);
      $arrJson['curr'] = $nCurPage;
      $arrJson['pages'] = $max_page;
      $arrJson['total'] = $totalNum;
    }
    // 返回json数据包...
    echo json_encode($arrJson);
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
  // 获取用户总数...
  /*public function getUserCount()
  {
    // 获取节点服务器的标记符号...
    $dbSys = D('system')->field('web_tag')->find();
    // 准备服务器链接地址，去掉最后的反斜杠...
    $strServer = $this->m_weLogin['redirect_uri'];
    $strServer = removeSlash($strServer);
    // 准备请求链接地址，调用接口，返回数据...
    $strCountUrl = sprintf("%s/wxapi.php/Login/getUserCount/node_tag/%s", $strServer, $dbSys['web_tag']);
    echo http_get($strCountUrl);
  }*/
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

    /*$_POST['updated'] = date('Y-m-d H:i:s');
    // 准备服务器链接地址，去掉最后的反斜杠...
    $strServer = $this->m_weLogin['redirect_uri'];
    $strServer = removeSlash($strServer);
    // 准备请求链接地址，调用接口，返回数据...
    $strUrl = sprintf("%s/wxapi.php/Login/saveUser", $strServer);
    echo http_post($strUrl, $_POST);*/
  }
}
?>