<?php
/*************************************************************
    HaoYi (C)2017 - 2020 myhaoyi.com

*************************************************************/

class IndexAction extends Action
{
  public function _initialize() {
    // 创建一个新的移动检测对象...
    $this->m_detect = new Mobile_Detect();
    //////////////////////////////////////////////
    // 直接对页面进行跳转...
    // 移动设备 => /Mobile
    // 电脑终端 => /wxapi.php/Home/room
    //////////////////////////////////////////////
    if( $this->m_detect->isMobile() ) {
      header("location: /Mobile");
    } else {
      header("location:".__APP__.'/Home/room');
    }
  }
  /**
  +----------------------------------------------------------
  * 默认操作，进行页面分发...
  +----------------------------------------------------------
  */
  public function index()
  {
    //print $this->m_detect->getUserAgent();
    //print_r($this->m_detect->getHttpHeaders());
    //print_r($_COOKIE); exit;
  }
  //
  // 处理配置存盘接口 => 安装脚本会调用...
  // tracker/x.x.x.x:22122/transmit/x.x.x.x:21001
  public function config()
  {
    // 获取并解析传递过来的参数信息...
    $arrTracker = explode(':', $_GET['tracker']);
    $arrTransmit = explode(':', $_GET['transmit']);
    if( count($arrTracker) != 2 || count($arrTransmit) != 2 ) {
      echo "== tracker or transmit must be set ===\n";
      exit;
    }
    // 网站端口需要被设置...
    if( !isset($_GET['webport']) ) {
      echo "== webport must be set ==\n";
      exit;
    }
    // 获取系统配置记录...
    $dbSys = D('system')->field('system_id')->find();
    // 将获取到的数据组合成存盘数据...
    $dbSys['sys_site'] = "https://www.myhaoyi.com";
    $dbSys['web_tracker_addr'] = "http://" . $arrTracker[0];
    $dbSys['web_tracker_port'] = $_GET['webport'];
    $dbSys['tracker_addr'] = $arrTracker[0];
    $dbSys['tracker_port'] = $arrTracker[1];
    $dbSys['transmit_addr'] = $arrTransmit[0];
    $dbSys['transmit_port'] = $arrTransmit[1];
    // 保存到系统表当中...
    D('system')->save($dbSys);
    // 打印成功信息...
    echo "== [OK] ==\n";
  }
}
?>