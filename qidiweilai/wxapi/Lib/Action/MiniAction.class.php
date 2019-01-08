<?php
/*************************************************************
    HaoYi (C)2017 - 2018 myhaoyi.com
    备注：专门处理微信小程序从中心服务器转发的请求命令...
*************************************************************/

class MiniAction extends Action
{
  public function _initialize() {
  }
  //
  // 获取共享通道记录列表...
  public function getShare()
  {
    // 准备返回结果状态...
    $arrErr['err_code'] = false;
    $arrErr['err_msg'] = 'ok';
    do {
      // 判断输入参数是否有效...
      if( !isset($_GET['camera_id']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '输入参数无效！';
        break;
      }
      // 查询通道记录...
      $condition['camera_id'] = $_GET['camera_id'];
      $dbCamera = D('LiveView')->where($condition)->field('camera_id,clicks,stream_prop,status,camera_name,image_fdfs')->find();
      // 获取图片链接需要的数据 => web_tracker_addr 已经自带了协议头 http://或https://
      $dbSys = D('system')->field('web_tracker_addr,web_tracker_port')->find();
      // 获取截图快照地址，地址不为空才处理 => 为空时，layload会自动跳转到snap.png...
      if( $dbCamera && strlen($dbCamera['image_fdfs']) > 0 ) {
        $dbCamera['image_fdfs'] = sprintf("%s:%d/%s", $dbSys['web_tracker_addr'], $dbSys['web_tracker_port'], $dbCamera['image_fdfs']);
      }
      // 组合最终返回的数据结果...
      $arrErr['track'] = $dbCamera;
    }while( false );
    // 直接反馈查询结果...
    echo json_encode($arrErr);
  }
  //
  // 获取直播播放地址...
  public function getLiveAddr()
  {
    // 准备返回数据结构...
    $arrErr['err_code'] = false;
    $arrErr['err_msg'] = "OK";
    $arrErr['err_desc'] = "OK";
    // 进行处理过程，有错误就返回...
    do {
      // 首先，判断通道是否处于直播状态...
      $condition['camera_id'] = $_GET['camera_id'];
      $dbCamera = D('LiveView')->where($condition)->field('camera_id,clicks,status,gather_id,mac_addr')->find();
      // 如果通道不在线，直接返回错误...
      if( $dbCamera['status'] <= 0 ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '当前通道处于离线状态，无法播放！';
        $arrErr['err_desc'] = '请联系管理员，开启通道。';
        break;
      }
      // 2018.01.25 - by jackey => 为了小程序审核做的特殊处理 => 只针对通道1和2...
      /*if( $dbCamera['camera_id'] == 1 || $dbCamera['camera_id'] == 2 ) {
        $dbResult['player_id'] = 10;
        $arrErr['rtmp_type'] = "rtmp/flv";
        $arrErr['rtmp_url'] = sprintf("rtmp://ihaoyi.cn/live/live%d", $dbCamera['camera_id']);
      }*/
      // 中转服务器需要的参数...
      $dbParam['mac_addr'] = $dbCamera['mac_addr'];
      $dbParam['rtmp_live'] = $dbCamera['camera_id'];
      // 获取直播链接地址...
      $dbResult = $this->getRtmpUrlFromTransmit($dbParam);
      // 如果获取连接中转服务器失败...
      if( $dbResult['err_code'] > 0 ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = $dbResult['err_msg'];
        $arrErr['err_desc'] = '请联系管理员，汇报错误信息。';
        break;
      }
      // 将反馈结果进行重新组合 => flvjs地址、rtmp地址、hls地址，播放器编号...
      // $dbResult['flv_url'];  $dbResult['flv_type'];
      // $dbResult['rtmp_url']; $dbResult['rtmp_type'];
      // $dbResult['hls_url']; $dbResult['hls_type'];
      $arrErr = array_merge($arrErr, $dbResult);
      // 这3个参数是直播播放器汇报时需要的数据...
      $arrErr['player_camera'] = $dbCamera['camera_id'];
      $arrErr['player_id'] = $dbResult['player_id'];
      $arrErr['player_vod'] = 0;
      // 累加点击计数器，写入数据库...
      $dbSave['clicks'] = intval($dbCamera['clicks']) + 1;
      $dbSave['camera_id'] = $dbCamera['camera_id'];
      D('camera')->save($dbSave);
      // 将最新计数结果反馈给小程序...
      $arrErr['clicks'] = $dbSave['clicks'];
    }while( false );
    // 反馈结果信息...
    echo json_encode($arrErr);
  }
  //
  // 2017.06.15 - by jackey => 修改了中转服务器代码，转发命令给采集端之后，立即返回rtmp地址，无需等待采集端上传结果，避免阻塞，让播放器自己去等待...
  // 从中转服务器获取直播地址...
  // 返回一个数组结果...
  private function getRtmpUrlFromTransmit(&$dbParam)
  {
    // 尝试链接中转服务器...
    $dbSys = D('system')->field('transmit_addr,transmit_port')->find();
    // 通过php扩展插件连接中转服务器 => 性能高...
    $transmit = transmit_connect_server($dbSys['transmit_addr'], $dbSys['transmit_port']);
    // 链接中转服务器失败，直接返回...
    if( !$transmit ) {
      $arrData['err_code'] = true;
      $arrData['err_msg'] = '无法连接中转服务器。';
      return $arrData;
    }
    // 获取直播频道所在的URL地址...
    $saveJson = json_encode($dbParam);
    $json_data = transmit_command(kClientPlay, kCmd_Play_Login, $transmit, $saveJson);
    // 关闭中转服务器链接...
    transmit_disconnect_server($transmit);
    // 获取的JSON数据有效，转成数组，直接返回...
    $arrData = json_decode($json_data, true);
    if( !$arrData ) {
      $arrData['err_code'] = true;
      $arrData['err_msg'] = '从中转服务器获取数据失败。';
      return $arrData;
    }
    // 通过错误码，获得错误信息 => 有可能 err_code 没有设定...
    $arrData['err_code'] = (($arrData['err_code'] > 0) ? $arrData['err_code'] : false);
    $arrData['err_msg'] = getTransmitErrMsg($arrData['err_code']);
    // 将整个数组返回...
    return $arrData;
  }
  //
  // 获取通道下的录像记录...
  public function getRecord()
  {
    // 准备返回结果状态...
    $arrErr['err_code'] = false;
    $arrErr['err_msg'] = 'ok';
    // 判断输入的参数是否有效...
    if( !isset($_GET['camera_id']) ) {
      $arrErr['err_code'] = true;
      $arrErr['err_msg'] = '输入参数无效！';
      echo json_encode($arrErr);
      return;
    }
    // 得到每页条数...
    $pagePer = C('PAGE_PER');
    $pageCur = (isset($_GET['p']) ? $_GET['p'] : 1);  // 当前页码...
    $pageLimit = (($pageCur-1)*$pagePer).','.$pagePer; // 读取范围...
    // 获取图片链接需要的数据 => web_tracker_addr 已经自带了协议头 http://或https://
    $dbSys = D('system')->field('web_tracker_addr,web_tracker_port')->find();
    // 获取记录总数和总页数...
    $condition['camera_id'] = $_GET['camera_id'];
    $totalNum = D('record')->where($condition)->count();
    $max_page = intval($totalNum / $pagePer);
    // 判断是否是整数倍的页码...
    $max_page += (($totalNum % $pagePer) ? 1 : 0);
    // 填充需要返回的信息...
    $arrErr['total_num'] = $totalNum;
    $arrErr['max_page'] = $max_page;
    $arrErr['cur_page'] = $pageCur;
    // 获取点播分页数据，并对缩略图片进行地址重组...
    $arrRecord = D('RecordView')->where($condition)->limit($pageLimit)->order('Record.created DESC')->select();
    foreach($arrRecord as &$dbItem) {
      // 获取截图快照地址，地址不为空才处理 => 为空时，小程序内部会跳转到snap.png...
      if( strlen($dbItem['image_fdfs']) > 0 ) {
        $dbItem['image_fdfs'] = sprintf("%s:%d/%s", $dbSys['web_tracker_addr'], $dbSys['web_tracker_port'], $dbItem['image_fdfs']);
      }
      // 获取视频文件完整连接地址...
      if( strlen($dbItem['file_fdfs']) > 0 ) {
        $dbItem['file_fdfs'] = sprintf("%s:%d/%s", $dbSys['web_tracker_addr'], $dbSys['web_tracker_port'], $dbItem['file_fdfs']);
      }
    }
    // 组合最终返回的数据结果...
    $arrErr['record'] = $arrRecord;
    // 返回json编码数据包...
    echo json_encode($arrErr);
  }
  //
  // 保存录像记录点击次数...
  public function saveClick()
  {
    // 准备返回结果状态...
    $arrErr['err_code'] = false;
    $arrErr['err_msg'] = 'ok';
    do {
      // 判断输入的参数是否有效...
      if( !isset($_GET['record_id']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '输入参数无效！';
        break;
      }
      // 找到对应的录像记录对象...
      $condition['record_id'] = $_GET['record_id'];
      $dbVod = D('record')->where($condition)->field('record_id,clicks')->find();
      // 累加点击计数器，写入数据库...
      $dbVod['clicks'] = intval($dbVod['clicks']) + 1;
      D('record')->save($dbVod);
      // 返回计数结果 => 存盘之后的点击次数...
      $arrErr['clicks'] = $dbVod['clicks'];
    } while ( false );
    // 返回json编码数据包...
    echo json_encode($arrErr);
  }
  //
  // 获取指定采集端下的通道记录...
  public function getCamera()
  {
    // 准备返回结果状态...
    $arrErr['err_code'] = false;
    $arrErr['err_msg'] = 'ok';
    // 判断输入的参数是否有效...
    if( !isset($_GET['mac_addr']) ) {
      $arrErr['err_code'] = true;
      $arrErr['err_msg'] = '输入参数无效！';
      echo json_encode($arrErr);
      return;
    }
    // 得到每页条数...
    $pagePer = C('PAGE_PER');
    $pageCur = (isset($_GET['p']) ? $_GET['p'] : 1);  // 当前页码...
    $pageLimit = (($pageCur-1)*$pagePer).','.$pagePer; // 读取范围...
    // 获取图片链接需要的数据 => web_tracker_addr 已经自带了协议头 http://或https://
    $dbSys = D('system')->field('web_tracker_addr,web_tracker_port')->find();
    // 获取记录总数和总页数...
    $condition['mac_addr'] = $_GET['mac_addr'];
    $totalNum = D('LiveView')->where($condition)->count();
    $max_page = intval($totalNum / $pagePer);
    // 判断是否是整数倍的页码...
    $max_page += (($totalNum % $pagePer) ? 1 : 0);
    // 填充需要返回的信息...
    $arrErr['total_num'] = $totalNum;
    $arrErr['max_page'] = $max_page;
    $arrErr['cur_page'] = $pageCur;
    // 获取通道分页数据，并对缩略图片进行地址重组...
    $arrCamera = D('LiveView')->where($condition)->limit($pageLimit)->field('camera_id,clicks,stream_prop,shared,status,camera_name,image_fdfs,created,updated')->order('Camera.created DESC')->select();
    foreach($arrCamera as &$dbItem) {
      // 获取截图快照地址，地址不为空才处理 => 为空时，小程序内部会跳转到snap.png...
      if( strlen($dbItem['image_fdfs']) > 0 ) {
        $dbItem['image_fdfs'] = sprintf("%s:%d/%s", $dbSys['web_tracker_addr'], $dbSys['web_tracker_port'], $dbItem['image_fdfs']);
      }
    }
    // 组合最终返回的数据结果...
    $arrErr['camera'] = $arrCamera;
    // 返回json编码数据包...
    echo json_encode($arrErr);
  }
  //
  // 保存通道的共享状态...
  public function saveShare()
  {
    // 准备返回结果状态...
    $arrErr['err_code'] = false;
    $arrErr['err_msg'] = 'ok';
    do {
      // 判断输入的参数是否有效...
      if( !isset($_GET['camera_id']) || !isset($_GET['shared']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '输入参数无效！';
        break;
      }
      // 直接保存通道状态到数据库当中...
      D('camera')->save($_GET);
    } while( false );
    // 返回json编码数据包...
    echo json_encode($arrErr);
  }
  //
  // 转发绑定状态命令给采集端...
  public function bindGather()
  {
    // 准备返回结果状态...
    $arrErr['err_code'] = false;
    $arrErr['err_msg'] = 'ok';
    // 注意：这里使用的是 $_POST 数据...
    do {
      // 判断输入的参数是否有效...
      if( !isset($_POST['mac_addr']) || !isset($_POST['bind_cmd']) || !isset($_POST['user_id']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '输入参数无效！';
        break;
      }
      // 尝试链接中转服务器...
      $dbSys = D('system')->field('transmit_addr,transmit_port')->find();
      // 通过php扩展插件连接中转服务器 => 性能高...
      $transmit = transmit_connect_server($dbSys['transmit_addr'], $dbSys['transmit_port']);
      // 链接中转服务器失败，直接返回...
      if( !$transmit ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '无法连接中转服务器';
        break;
      }
      // 中转服务器需要的参数 => 转换成json...
      $dbParam = $_POST;
      $saveJson = json_encode($dbParam);
      $json_data = transmit_command(kClientPHP, kCmd_Gather_Bind_Mini, $transmit, $saveJson);
      // 关闭中转服务器链接...
      transmit_disconnect_server($transmit);
      // 获取的JSON数据有效，转成数组，直接返回...
      $arrData = json_decode($json_data, true);
      if( !$arrData ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '从中转服务器转发绑定命令失败';
        break;
      }
      // 通过错误码，获得错误信息 => 有可能 err_code 没有设定...
      $arrErr['err_code'] = (($arrData['err_code'] > 0) ? $arrData['err_code'] : false);
      $arrErr['err_msg'] = getTransmitErrMsg($arrData['err_code']);
    } while( false );
    // 返回json编码数据包...
    echo json_encode($arrErr);
  }
  //
  // 转发解除绑定状态命令给采集端...
  public function unbindGather()
  {
    // 准备返回结果状态...
    $arrErr['err_code'] = false;
    $arrErr['err_msg'] = 'ok';
    // 注意：这里使用的是 $_POST 数据...
    do {
      // 判断输入的参数是否有效...
      if( !isset($_POST['mac_addr']) || !isset($_POST['gather_id']) || !isset($_POST['user_id']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '输入参数无效！';
        break;
      }
      // 尝试链接中转服务器...
      $dbSys = D('system')->field('transmit_addr,transmit_port')->find();
      // 通过php扩展插件连接中转服务器 => 性能高...
      $transmit = transmit_connect_server($dbSys['transmit_addr'], $dbSys['transmit_port']);
      // 链接中转服务器失败，直接返回...
      if( !$transmit ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '无法连接中转服务器';
        break;
      }
      // 中转服务器需要的参数 => 转换成json...
      $dbParam = $_POST;
      $saveJson = json_encode($dbParam);
      $json_data = transmit_command(kClientPHP, kCmd_Gather_UnBind_Mini, $transmit, $saveJson);
      // 关闭中转服务器链接...
      transmit_disconnect_server($transmit);
      // 获取的JSON数据有效，转成数组，直接返回...
      $arrData = json_decode($json_data, true);
      if( !$arrData ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '从中转服务器转发解除绑定命令失败';
        break;
      }
      // 通过错误码，获得错误信息 => 有可能 err_code 没有设定...
      $arrErr['err_code'] = (($arrData['err_code'] > 0) ? $arrData['err_code'] : false);
      $arrErr['err_msg'] = getTransmitErrMsg($arrData['err_code']);
    } while( false );
    // 返回json编码数据包...
    echo json_encode($arrErr);
  }
}
?>