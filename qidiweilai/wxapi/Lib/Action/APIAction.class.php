<?php
/*************************************************************
    Wan (C)2019- 2020 myhaoyi.com
    备注：第三方登录页面...
*************************************************************/

class APIAction extends Action
{
  public function _initialize() {
    // 获取系统配置，根据配置设置相关变量...
    $this->m_dbSys = D('system')->find();
  }
  
  // 产生小程序二维码的测试接口...
  public function qrcode() {
    // 获取传递过来的二维码场景值...
    if( isset($_GET['scene']) ) {
      $strScene = $_GET['scene'];
    } else if( isset($_POST['scene']) ) {
      $strScene = $_POST['scene'];
    }
    // 准备返回信息...
    $arrErr['err_code'] = 0;
    $arrErr['err_msg'] = 'ok';
    do {
      // 如果场景值无效，返回错误...
      if (!isset($strScene) || strlen($strScene) <= 0) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '二维码场景值无效！';
        break;
      }
      // 通过MiniAction获取小程序码的token值...
      $arrToken = A('Mini')->getToken(false);
      if ($arrToken['err_code'] > 0) {
        $arrErr['err_code'] = $arrToken['err_code'];
        $arrErr['err_msg'] = $arrToken['err_msg'];
        break;
      }
      // 通过小程序码再获取小程序二维码，然后存入数据库...
      $strQRUrl = sprintf("https://api.weixin.qq.com/wxa/getwxacodeunlimit?access_token=%s", $arrToken['access_token']);
      $itemData["page"] = "pages/bind/bind";
      $itemData["scene"] = $strScene;
      $itemData["width"] = 280;
      // 直接通过设定参数获取小程序码...
      $result = http_post($strQRUrl, json_encode($itemData));
      // 获取小程序码失败的情况...
      if( !$result ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '获取小程序码失败';
        break;
      }
      // 返回图片数据...
      header('content-type:image/png');
      echo $result;
      return;
    } while( false );
    // 返回json数据包...
    echo json_encode($arrErr);
  }

  // 仅供产生测试数据使用...  
  public function test()
  {
    // login => 制造数据...
    /*$arrJson['scene'] = '2_7_788';
    $arrJson['unionid'] = 'oeLwa0neDxWjxKECzoUPrfsfNocM';
    $arrJson['headimgurl'] = 'https://wx.qlogo.cn/mmopen/vi_32/DYAIOgq83eria41KJU1sJgfGM2OhVXOL2YYTYEEa6gvaSlR1YmicsEAQia7Rx4N5EpGtzz3g7LvjsicGyeLicbuc6jQ/132';
    $arrJson['nickname'] = 'jackey';*/
    
    // check => 制造数据...
    //$arrJson['room_pass'] = '123456';
    
    // success => 制造数据...
    $arrJson['scene'] = '2_7_788';
    $arrJson['room_id'] = 200001;
    $arrJson['user_id'] = 25;
    
    // 先json编码再进行base64编码...
    $strWxJSON = json_encode($arrJson);
    echo urlsafe_b64encode($strWxJSON);
  }

  // 第三方反向调用的用户登录接口...
  public function login()
  {
    // 准备返回信息...
    $arrErr['err_code'] = 0;
    $arrErr['err_msg'] = 'ok';
    do {
      // 没有获取到有效参数...
      if( !isset($_GET['wx_json']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '输入的参数无效！';
        break;
      }
			// 获取服务器反馈的信息...
      $strWxJSON = urlsafe_b64decode($_GET['wx_json']);
      $arrUser = json_decode($strWxJSON, true);
      // 判断获取数据的有效性 => 必须包含场景参数字段...
      if( !isset($arrUser['scene']) || !isset($arrUser['unionid']) || !isset($arrUser['headimgurl']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '获取微信用户数据失败！';
        break;
      }
      // 对传递过来的unionid进行一次md5处理，避免与启迪云冲突...
      $arrUser['unionid'] = md5($arrUser['unionid']);
      $arrUser['sex'] = (isset($arrUser['sex']) ? $arrUser['sex'] : 1);
      // 从当前已有的数据库中查找这个第三方用户的信息...
      $condition['is_third'] = true;
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
      $dbUser['is_third'] = true;                    // 第三方用户
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
      // 解析场景参数字段内容的数据...
      $arrScene = explode('_', $arrUser['scene']);
      $arrParam['client_type'] = $arrScene[0];
      $arrParam['tcp_socket'] = $arrScene[1];
      $arrParam['tcp_time'] = $arrScene[2];
      $arrParam['user_id'] = $dbUser['user_id'];
      $arrParam['bind_cmd'] = BIND_SCAN;
      // 房间编号必须输入，使用默认值...
      $arrParam['room_id'] = 0;
      // 向中心服务器转发小程序绑定登录这个中转命令...
      $dbResult = $this->doTrasmitCmdToServer(kCmd_PHP_Bind_Mini, $arrParam);
      // 如果获取连接中转服务器失败...
      if( $dbResult['err_code'] > 0 ) {
        $arrErr['err_code'] = $dbResult['err_code'];
        $arrErr['err_msg'] = $dbResult['err_msg'];
        break;
      }
      // 返回第三方用户在启迪云数据库里的用户编号...
      $arrErr['user_id'] = $dbUser['user_id'];
      $arrErr['scene'] = $arrUser['scene'];
    } while ( false );
    // 返回json数据包...
    echo json_encode($arrErr);
  }
  
  // 第三方反向调用的密码验证接口...
  public function check()
  {
    // 准备返回信息...
    $arrErr['err_code'] = 0;
    $arrErr['err_msg'] = 'ok';
    do {
      // 没有获取到有效参数...
      if( !isset($_GET['wx_json']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '输入的参数无效！';
        break;
      }
			// 获取服务器反馈的信息...
      $strWxJSON = urlsafe_b64decode($_GET['wx_json']);
      $arrJson = json_decode($strWxJSON, true);
      // 判断获取数据的有效性 => 必须包含房间密码字段...
      if( !isset($arrJson['room_pass']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '获取房间信息失败！';
        break;
      }
      // 判断房间是否有效 => 房间表中查找...
      $condition['room_pass'] = $arrJson['room_pass'];
      $dbRoom = D('room')->where($condition)->find();
      // 密码验证失败，直接返回错误信息...
      if( !isset($dbRoom['room_id']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '密码错误，请重新输入！';
        break;
      }
      // 验证成功，返回密码对应的房间信息 => 需要修改房间偏移号...
      $dbRoom['room_id'] += LIVE_BEGIN_ID;
      $arrErr['room'] = $dbRoom;
    } while ( false );
    // 返回json数据包...
    echo json_encode($arrErr);
  }

  // 第三方反向调用的核验成功接口...
  public function success()
  {
    // 准备返回信息...
    $arrErr['err_code'] = 0;
    $arrErr['err_msg'] = 'ok';
    do {
      // 没有获取到有效参数...
      if( !isset($_GET['wx_json']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '输入的参数无效！';
        break;
      }
			// 获取服务器反馈的信息...
      $strWxJSON = urlsafe_b64decode($_GET['wx_json']);
      $arrJson = json_decode($strWxJSON, true);
      // 判断获取数据的有效性 => 必须包含场景参数|房间编号|用户编号...
      if( !isset($arrJson['scene']) || !isset($arrJson['room_id']) || !isset($arrJson['user_id']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '输入房间信息无效！';
        break;
      }
      // 解析场景参数字段内容的数据...
      $arrScene = explode('_', $arrJson['scene']);
      $arrParam['client_type'] = $arrScene[0];
      $arrParam['tcp_socket'] = $arrScene[1];
      $arrParam['tcp_time'] = $arrScene[2];
      $arrParam['user_id'] = $arrJson['user_id'];
      $arrParam['room_id'] = $arrJson['room_id'];
      $arrParam['bind_cmd'] = BIND_SAVE;
      // 向中心服务器转发小程序绑定登录这个中转命令...
      $dbResult = $this->doTrasmitCmdToServer(kCmd_PHP_Bind_Mini, $arrParam);
      // 如果获取连接中转服务器失败...
      if( $dbResult['err_code'] > 0 ) {
        $arrErr['err_code'] = $dbResult['err_code'];
        $arrErr['err_msg'] = $dbResult['err_msg'];
        break;
      }
    } while ( false );
    // 返回json数据包...
    echo json_encode($arrErr);
  }
  
  // 统一的核心服务器命令接口...
  private function doTrasmitCmdToServer($inCmd, &$dbParam)
  {
    // 读取系统配置数据库记录...
    $dbSys = $this->m_dbSys;
    // 通过php扩展插件连接中转服务器 => 性能高...
    $transmit = transmit_connect_server($dbSys['udpcenter_addr'], $dbSys['udpcenter_port']);
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
}
?>