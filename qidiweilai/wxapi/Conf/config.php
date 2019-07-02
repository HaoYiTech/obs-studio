<?php
  
  //载入公共配置
  $config	= require __PUBLIC__  . '/config.inc.php';
  
  //设定项目配置
  $array = array(
    // 定义版本号...
    'VERSION' => '1.2.7',
    // 微信登录网站应用需要的参数配置 => 微信开放平台
    'WECHAT_LOGIN' => array(
      'scope'=>'snsapi_login', //用户授权的作用域,使用逗号(,)分隔
      'appid'=>'wx8e6f2987edbe4b28', //网站登录应用的appid
      'appsecret'=>'0981672116a85810875759c18144fd7f', //网站登录应用的appsecret
      'redirect_uri'=>'https://www.qidiweilai.com/', //回调地址
      'href'=>'https://www.qidiweilai.com/wxapi/public/css/wxlogin.css', //登录样式地址
    ),
    // 微信小程序参数配置 => 启迪云 => 云教室...
    'QIDI_MINI' => array(
      'appid'=>'wxb324852eab07f380',
      'appsecret'=>'f55416c405e0a9bc848d3fc1d28126ce'
    ),
     
    // 每页显示记录数
    'PAGE_PER' => 10,
    
    // 关闭自动Session
    'SESSION_AUTO_START'=>false,
    'SHOW_PAGE_TRACE'=>false,
    
    //'URL_MODEL'=>3,
    'URL_ROUTER_ON'=>true,
    
    'TMPL_TEMPLATE_SUFFIX'=>'.htm',
    'TMPL_CACHE_TIME'=>0,
    'TMPL_L_DELIM'=>'{{',
    'TMPL_R_DELIM'=>'}}',
    'DATA_CACHE_SUBDIR'=>true,
    'DATA_PATH_LEVEL'=>2,
    
    'LANG_SWITCH_ON'=>true,
    'LANG_AUTO_DETECT'=>false,
    'DEFAULT_LANG'=>'zh-cn',
    'DB_FIELDTYPE_CHECK'=>true,
    
    //定义保留关键字...
    'DIFNAME'=>array('admin','home','api','client','index','pub','v','m','p','setting','dologin','login','logout','register','regcheck','reset','doreset','checkreset','setpass','space','message','find','topic','hot','index','widget','comments','wap','map','plugins','url','guide','sendapi','blacklist','vote'),
  );
  
  //合并输出配置
  return array_merge($config,$array);
?>
