<?php
/*************************************************************
    Wan (C)2016- 2017 myhaoyi.com
    备注：专门处理电脑端强求页面...
*************************************************************/

class IndexAction extends Action
{
  public function _initialize() {
  }
  /**
  +----------------------------------------------------------
  * 默认操作 - 处理全部非微信端的访问...
  +----------------------------------------------------------
  */
  public function index()
  {
    // 处理第三方网站扫码登录的授权回调过程 => 其它网站的登录，state != 32...
    if( isset($_GET['code']) && isset($_GET['state']) && strlen($_GET['state']) != 32 ) {
      A('Login')->doWechatAuth($_GET['code'], $_GET['state']);
      return;
    }
    //////////////////////////////////////////////
    // 创建移动检测对象，判断是否是移动端访问...
    // 电脑终端 => Index/index
    // 移动设备 => Mobile/index
    //////////////////////////////////////////////
    // 2017.12.28 - by jackey => 对页面进行了合并...
    /*$this->m_detect = new Mobile_Detect();
    if( $this->m_detect->isMobile() ) {
      header("location:".__APP__.'/Mobile/index');
      return;
    }*/
    // 电脑终端页面显示...
    $this->display('index');
  }
  //
  // 更新日志 页面...
  public function changelog()
  {
    $this->display('changelog');
  }
}
?>