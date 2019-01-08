<?php
/*************************************************************
    HaoYi (C)2017 - 2020 myhaoyi.com

*************************************************************/

class RTMPAction extends Action
{
  // 初始化页面的默认操作...
  public function _initialize() {
  }
  //
  // 处理 login 接口...
  public function login()
  {
    $this->doTransmit(kCmd_Live_Login);
  }
  //
  // 处理 quit 接口...
  public function quit()
  {
    $this->doTransmit(kCmd_Live_Quit);
  }
  //
  // 处理 vary 接口...
  public function vary()
  {
    $this->doTransmit(kCmd_Live_Vary);
  }
  //
  // 统一的转发命令接口...
  private function doTransmit($nCommand)
  {
    // 判断输入参数的有效性...
    // hls_addr  => 192.168.1.180:8080
    // rtmp_addr => 192.168.1.180:1935
    // rtmp_live => 0(login) => >0(vary) => <0(quit)
    // rtmp_user => 0
    $arrData = $_POST;
    if( !isset($arrData['rtmp_addr']) || !isset($arrData['hls_addr']) || !isset($arrData['rtmp_live']) || !isset($arrData['rtmp_user']) ) {
      echo '0'; return;
    }
    
    // 尝试链接中转服务器...
    $dbSys = D('system')->field('transmit_addr,transmit_port')->find();
    
    /*// 尝试链接中转服务器 => 发送直播登录命令...
    $saveJson = json_encode($arrData);
    $json_data = php_transmit_command($dbSys['transmit_addr'], $dbSys['transmit_port'], kClientLive, $nCommand, $saveJson);
    // 反馈转发结果...
    echo $json_data;*/

    // 通过php扩展插件连接中转服务器 => 性能高...
    $transmit = transmit_connect_server($dbSys['transmit_addr'], $dbSys['transmit_port']);
    if( !$transmit ) {
      echo '0'; return;
    }
    // 尝试链接中转服务器 => 发送直播登录命令...
    $saveJson = json_encode($arrData);
    $json_data = transmit_command(kClientLive, $nCommand, $transmit, $saveJson);
    // 关闭中转服务器链接...
    transmit_disconnect_server($transmit);
    // 判断反馈转发结果 => 成功返回'1'，失败返回'0'...
    $arrData = json_decode($json_data, true);
    echo (($arrData['err_code'] == ERR_OK) ? '1': '0');
  }
  //
  // 处理播放器直播在线通知...
  public function verify()
  {
    // 指定其它域名访问内容 => 跨域访问...
    //header('Access-Control-Allow-Origin:*');
    // 准备返回数据结构...
    $arrErr['err_code'] = false;
    $arrErr['err_msg'] = "OK";
    // 获取POST数据内容...
    // rtmp_live => 通道编号...
    // player_id => 播放器编号...
    // player_type => 播放器类型...
    // player_active => 播放器状态...
    $arrData = $_POST;
    do {
      // 判断数据参数的有效性...
      if( !isset($arrData['rtmp_live']) || !isset($arrData['player_id']) || !isset($arrData['player_type']) || !isset($arrData['player_active']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = "Invalidate parameter!";
        break;
      }
      // 尝试链接中转服务器...
      $dbSys = D('system')->field('transmit_addr,transmit_port')->find();
      // 通过php扩展插件连接中转服务器 => 性能高...
      $transmit = transmit_connect_server($dbSys['transmit_addr'], $dbSys['transmit_port']);
      if( !$transmit ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = "connect transmit server failed!";
        break;
      }
      // 尝试链接中转服务器 => 发送播放器在线命令...
      $saveJson = json_encode($arrData);
      $json_data = transmit_command(kClientPlay, kCmd_Play_Verify, $transmit, $saveJson);
      // 关闭中转服务器链接...
      transmit_disconnect_server($transmit);
      // 解析转发服务器反馈的结果...
      $arrData = json_decode($json_data, true);
      // 反馈的结果失败...
      if( !$arrData ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '从中转服务器获取数据失败。';
        break;
      }
      // 转发反馈成功，将结果进行转义处理...
      $arrErr['err_code'] = $arrData['err_code'];
      $arrErr['err_msg'] = getTransmitErrMsg($arrData['err_code']);
    }while( false );
    // 直接返回运行结果 => json...
    echo json_encode($arrErr);
  }
  //
  // 处理 rtmp 服务器在发布时的用户信息验证...
  public function publish()
  {
    // for nginx-rtmp...
    //logdebug(json_encode($_POST));
    //header('HTTP/1.0 404 Not Found');
    //header('HTTP/1.0 200 OK');
    
    // for srs...
    //logdebug(file_get_contents("php://input"));
    //echo '0';
  }
}
?>