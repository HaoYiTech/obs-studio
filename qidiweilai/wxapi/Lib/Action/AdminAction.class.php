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
  public function index()
  {
    echo 'admin:index';
    // 获取用户类型编号、所在门店编号...
    /*$nUserType = Cookie::get('wx_usertype');
    // 如果用户类型编号小于运营维护，跳转到前台页面，不能进行后台操作...
    $strAction = (($nUserType < kMaintainUser) ? '/Home/room' : '/Admin/system');
    // 重定向到最终计算之后的跳转页面...
    header("Location: ".__APP__.$strAction);*/
  }
}

?>