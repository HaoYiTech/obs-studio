<?php
/*************************************************************
    Wan (C)2016- 2017 myhaoyi.com
    备注：专门处理电脑端强求页面...
*************************************************************/

class IndexAction extends Action
{
  public function _initialize() {
    // 获取登录导航信息内容...
    $this->m_LoginNav = $this->getLoginNav();
  }
  //
  // 获取用户登录信息...
  private function getLoginNav()
  {
    // 默认设定成未登录状态...
    $my_nav['is_login'] = false;
    // 根据cookie设定 登录|注销 导航栏 => 头像默认已经转换成了https模式...
    if( Cookie::is_set('wx_unionid') && Cookie::is_set('wx_headurl') && Cookie::is_set('wx_usertype') && Cookie::is_set('wx_shopid') ) {
      $my_nav['is_login'] = true;
      $my_nav['headurl'] = Cookie::get('wx_headurl');
    }
    // 返回登录导航信息...
    return $my_nav;
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
    // 电脑终端页面显示 => 设定登录导航数据...
    $this->assign('my_nav', $this->m_LoginNav);
    $this->assign('my_ver', C('VERSION'));
    $this->display('index');
  }
  //
  // 更新日志 页面...
  public function changelog()
  {
    $this->display('changelog');
  }
  //
  // 点击查看用户登录信息...
  public function userInfo()
  {
    // 判断用户是否已经处于登录状态，直接进行页面跳转...
    if( !Cookie::is_set('wx_unionid') || !Cookie::is_set('wx_headurl') || !Cookie::is_set('wx_usertype') || !Cookie::is_set('wx_shopid') ) {
      echo "<script>window.parent.doReload();</script>";
      return;
    }
    // 调用Admin->userInfo接口...
    A('Admin')->userInfo();
  }
  //
  // 点击注销导航页...
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
    
    $my_nav['is_login'] = false;
    $this->assign('my_nav', $my_nav);
    $this->display('Common:home_login');
  }
  //
  // 点击登录导航页...
  // 参数：wx_error | wx_unionid | wx_headurl
  public function login()
  {
    A('Login')->login(false);
  }
  //
  // 获取是否登录状态...
  private function isLogin()
  {
    return ((Cookie::is_set('wx_unionid') && Cookie::is_set('wx_headurl') && Cookie::is_set('wx_usertype') && Cookie::is_set('wx_shopid')) ? true : false);
  }
  //
  // 获取登录用户的编号...
  private function getLoginUserID()
  {
    // 用户未登录，返回0...
    if( !$this->isLogin() )
      return 0;
    // 从cookie获取unionid，再查找用户编号...
    $theMap['wx_unionid'] = Cookie::get('wx_unionid');
    $dbLogin = D('user')->where($theMap)->field('user_id,wx_nickname')->find();
    return (isset($dbLogin['user_id']) ? $dbLogin['user_id'] : 0);
  }
}
?>