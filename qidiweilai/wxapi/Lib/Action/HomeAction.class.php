<?php
/*************************************************************
    HaoYi (C)2017 - 2020 myhaoyi.com

*************************************************************/

class HomeAction extends Action
{
  public function _initialize()
  {
    // 加载 ThinkPHP 的扩展函数 => ThinkPHP/Common/extend.php => msubstr()
    Load('extend');
    // 获取系统配置，根据配置设置相关变量 => 强制配置成云教室...
    $this->m_dbSys = D('system')->find();
    $this->m_webTitle = $this->m_dbSys['web_title'];
    $this->m_webType = kCloudEducate;
    // 创建一个新的移动检测对象...
    $this->m_detect = new Mobile_Detect();
    //////////////////////////////////////////////
    // 移动端访问了电脑端页面，直接对页面进行跳转...
    //////////////////////////////////////////////
    if( $this->m_detect->isMobile() ) {
      header("location: /Mobile");
    }
    // 判断是否是IE浏览器 => 这里必须用0和1表示...
    $this->m_isIEBrowser = 0;
    $theIERule = '/(MSIE) (\d+\.\d)/';
    $theUserAgent = $this->m_detect->getUserAgent();
    // 通过正则匹配，是否是IE浏览器...
    preg_match($theIERule, $theUserAgent, $data);
    // IE11 以下禁用高级flvjs和hls，只能用flash播放...
    if( !empty($data) && is_array($data) ) {
      $this->m_isIEBrowser = (($data[2] >= '11.0') ? 0 : 1);
    }
    // 直接给模板变量赋值...
    $this->assign('my_system', $this->m_dbSys);
  }
  //
  // 移动手机二维码页面...
  public function mobile()
  {
    $this->assign('my_web_title', $this->m_webTitle);
    $this->display();
  }
  //
  // 显示手机登录的二维码...
  public function qrcode()
  {
    // 生成二维码接口...
    require(APP_PATH."/Common/phpqrcode.php");
    // 构造二维码内容，注意：$_SERVER['HTTP_HOST'] 自带访问端口...
    $value = sprintf("%s://%s", $_SERVER['REQUEST_SCHEME'], $_SERVER['HTTP_HOST']);
    // 直接返回生成的二维码图片...
    QRcode::png($value, false, 'L', 12, 2); 
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
  //
  // 得到导航数据...
  private function getNavData($activeType, $activeID)
  {
    // 根据cookie设定 登录|注销 导航栏...
    if( Cookie::is_set('wx_unionid') && Cookie::is_set('wx_headurl') && Cookie::is_set('wx_usertype') && Cookie::is_set('wx_shopid') ) {
      $my_nav['is_login'] = true;
      $my_nav['headurl'] = Cookie::get('wx_headurl');
      // 判断当前页面是否是https协议 => 通过$_SERVER['HTTPS']和$_SERVER['REQUEST_SCHEME']来判断...
      if((isset($_SERVER['HTTPS']) && $_SERVER['HTTPS'] == 'on') || (isset($_SERVER['REQUEST_SCHEME']) && $_SERVER['REQUEST_SCHEME'] == 'https')) {
        $my_nav['headurl'] = str_replace('http://', 'https://', $my_nav['headurl']);
      }
    } else {
      $my_nav['is_login'] = false;
    }
    // 获取采集器列表...
    //$arrDrop = D('gather')->order('gather_id ASC')->select();
    $arrDrop = null;
    // 设置一些返回数据...
    $my_nav['type'] = $activeType;          // 0(首页激活), 1(直播教室激活), 2(教室墙激活), 3(科目激活), 4(更多激活)
    $my_nav['active_id'] = $activeID;       // 激活的采集器编号...
    $my_nav['drop_more'] = "更多";          // 默认的下拉名称...
    // 如果是采集器状态，需要进一步的判断...
    if( $activeType == NAV_ACTIVE_SUBJECT ) {
      // 遍历采集器列表，设置下拉名称...
      $bFindActive = false;
      foreach($arrDrop as $key => &$dbItem) {
        if( $dbItem['gather_id'] == $my_nav['active_id'] ) {
          $my_nav['subject_title'] = $dbItem['name_set'];
          $bFindActive = ((($key + 1) > NAV_ITEM_COUNT) ? true : false);
          break;
        }
      }
      // 如果是下拉状态，设置下拉名称...
      if( isset($_GET['more']) || $bFindActive ) {
        $my_nav['type'] = NAV_ACTIVE_MORE;
        $my_nav['drop_more'] = $my_nav['subject_title'];
      }
    }
    // 将获取的采集器数组进行分割 => 5个采集器
    $arrList = array_splice($arrDrop, 0, NAV_ITEM_COUNT - 1);
    // 将分割后的数据进行赋值...
    $my_nav['list'] = $arrList;             // 横向采集器列表...
    $my_nav['drop'] = $arrDrop;             // 下拉采集器列表...
    $my_nav['drop_num'] = count($arrDrop); // 下拉采集器数量...
    // 直接返回计算之后的导航数据...
    return $my_nav;
  }
  //
  // 默认首页...
  public function index()
  {
    // 获取首页导航数据...
    $my_nav = $this->getNavData(NAV_ACTIVE_HOME, 0);
    $this->assign('my_title', $this->m_webTitle . ' - 首页');
    $this->assign('my_nav', $my_nav);
    // 计算右侧，最近更新，前10条...
    //$arrNews = D('RecordView')->limit(10)->order('Record.created DESC')->select();
    $arrNews = D('record')->limit(10)->order('created DESC')->select();
    $this->assign('my_news', $arrNews);
    //计算右侧，点击排行，前10条...
    //$arrList = D('RecordView')->limit(10)->order('Record.clicks DESC')->select();
    $arrList = D('record')->limit(10)->order('clicks DESC')->select();
    $this->assign('my_clicks', $arrList);
    // 得到每页通道数，总记录数，计算总页数...
    $pagePer = C('PAGE_PER');
    $totalNum = D('camera')->count();
    $max_page = intval($totalNum / $pagePer);
    // 判断是否是整数倍的页码...
    $max_page += (($totalNum % $pagePer) ? 1 : 0);
    // 设置最大页数，设置模板参数...
    $this->assign('max_page', $max_page);
    $this->display('index');
  }
  //
  // 首页流加载...
  public function pageIndex()
  {
    // 准备需要的分页参数...
    $pagePer = C('PAGE_PER'); // 每页显示的通道数...
    $pageCur = (isset($_GET['p']) ? $_GET['p'] : 1);  // 当前页码...
    $pageLimit = (($pageCur-1)*$pagePer).','.$pagePer; // 读取范围...
    // 读取通道下面的录像列表，只读取前8条记录...
    $arrList = D('camera')->limit($pageLimit)->field('camera_id,gather_id,camera_name')->order('camera_id ASC')->select();
    foreach($arrList as &$dbItem) {
      $map['Record.camera_id'] = $dbItem['camera_id'];
      $theName = "live" . $dbItem['camera_id'];
      $arrRec[$theName]['id'] = $dbItem['camera_id'];
      $arrRec[$theName]['name'] = $dbItem['camera_name'];
      $arrRec[$theName]['data'] = D('RecordView')->where($map)->limit(8)->order('created DESC')->select();
    }
    // 对模板对象进行赋值 => web_tracker_addr 已经自带了协议头 http://或https://
    $this->assign('my_web_tracker', sprintf("%s:%d/", $this->m_dbSys['web_tracker_addr'], $this->m_dbSys['web_tracker_port']));
    $this->assign('my_cur_page', $pageCur);
    $this->assign('my_rec', $arrRec);
    echo $this->fetch('pageIndex');
  }
  //
  // 巡课页面...
  public function tour()
  {
    // 获取巡课导航数据...
    $my_nav = $this->getNavData(NAV_ACTIVE_TOUR, 0);
    // 对模板对象进行赋值...
    $this->assign('my_title', $this->m_webTitle . ' - 教室墙');
    $this->assign('my_nav', $my_nav);
    // 得到每页通道数，总记录数，计算总页数...
    $pagePer = 12; //C('PAGE_PER');
    $totalNum = D('camera')->count();
    $max_page = intval($totalNum / $pagePer);
    // 判断是否是整数倍的页码...
    $max_page += (($totalNum % $pagePer) ? 1 : 0);
    // 设置最大页数，设置模板参数...
    $this->assign('my_total_num', $totalNum);
    $this->assign('max_page', $max_page);
    // 设置模版参数，显示巡课页面...
    $this->display('tour');
  }
  //
  // 巡课分页...
  public function pageTour()
  {
    // 准备需要的分页参数...
    $pagePer = 12; //C('PAGE_PER'); // 每页显示的通道数...
    $pageCur = (isset($_GET['p']) ? $_GET['p'] : 1);  // 当前页码...
    $pageLimit = (($pageCur-1)*$pagePer).','.$pagePer; // 读取范围...
    // 读取通道列表 => 通道编号优先排序 => 按每页6个通道拆分...
    $arrCamera = D('LiveView')->limit($pageLimit)->order('camera_id ASC')->field('camera_id,camera_name,stream_prop,status,clicks,image_fdfs')->select();
    $arrWall = array_chunk($arrCamera, 6);
    $this->assign('my_cur_page', $pageCur);
    $this->assign('my_list', $arrWall);
    // 同时返回数据和页面...
    $arrErr['data'] = $arrCamera;
    $arrErr['html'] = $this->fetch('pageTour');
    // web_tracker_addr 已经自带了协议头 http://或https://
    $arrErr['tracker'] = sprintf("%s:%d/", $this->m_dbSys['web_tracker_addr'], $this->m_dbSys['web_tracker_port']);
    // 直接返回数据...
    echo json_encode($arrErr);
  }
  //
  // 获取直播通道的快照地址...
  public function getImage()
  {
    $condition['camera_id'] = $_GET['camera_id'];
    $dbCamera = D('LiveView')->where($condition)->field('camera_id,image_fdfs,status')->find();
    echo json_encode($dbCamera);
  }
  //
  // 点击直播教室页面...
  public function room()
  {
    // 获取直播导航数据...
    $my_nav = $this->getNavData(NAV_ACTIVE_LIVE, 0);
    // 对模板对象进行赋值...
    $this->assign('my_title', $this->m_webTitle . ' - 直播教室');
    $this->assign('my_nav', $my_nav);
    // 得到每页通道数，总记录数，计算总页数...
    $pagePer = 8; //C('PAGE_PER');
    $totalNum = D('room')->count();
    $max_page = intval($totalNum / $pagePer);
    // 判断是否是整数倍的页码...
    $max_page += (($totalNum % $pagePer) ? 1 : 0);
    // 设置最大页数，设置模板参数...
    $onlineNum = D('room')->where('status > 0')->count();
    $this->assign('my_online_num', $onlineNum);
    $this->assign('my_total_num', $totalNum);
    $this->assign('max_page', $max_page);
    // 设置模板参数...
    $this->display('room');    
  }
  //
  // 直播教室页面流加载...
  public function pageRoom()
  {
    // 准备需要的分页参数...
    $pagePer = 8; //C('PAGE_PER'); // 每页显示的通道数...
    $pageCur = (isset($_GET['p']) ? $_GET['p'] : 1);  // 当前页码...
    $pageLimit = (($pageCur-1)*$pagePer).','.$pagePer; // 读取范围...
    // 读取通道列表 => 在线优先排序
    $arrRoom = D('RoomView')->limit($pageLimit)->order('Room.created DESC')->select();
    // 设置其它模板参数 => web_tracker_addr 已经自带了协议头 http://或https://
    $this->assign('my_web_tracker', sprintf("%s:%d/", $this->m_dbSys['web_tracker_addr'], $this->m_dbSys['web_tracker_port']));
    // 设置模板参数 => 使用 pageRoom 的模版...
    $this->assign('my_live_begin', LIVE_BEGIN_ID);
    $this->assign('my_cur_page', $pageCur);
    $this->assign('my_list', $arrRoom);
    $this->display('pageRoom');
  }
  //
  // 点击采集器页面...
  public function gather()
  {
    // 获取采集器导航数据，如果是下拉状态，设置对应的标志信息...
    $gather_id = $_GET['id'];
    $my_nav = $this->getNavData(NAV_ACTIVE_SUBJECT, $gather_id);
    $my_nav['more'] = ($my_nav['type'] == NAV_ACTIVE_MORE) ? "more/1" : "";
    $this->assign('my_title', $this->m_webTitle . ' - ' . $my_nav['subject_title']);
    $this->assign('my_nav', $my_nav);
    // 得到每页通道数，总记录数，计算总页数...
    $pagePer = 16; //C('PAGE_PER');
    $map['gather_id'] = $gather_id;
    $totalNum = D('camera')->where($map)->count();
    $max_page = intval($totalNum / $pagePer);
    // 判断是否是整数倍的页码...
    $max_page += (($totalNum % $pagePer) ? 1 : 0);
    // 设置最大页数，设置模板参数...
    $map['status'] = array('gt', 0);
    $onlineNum = D('camera')->where($map)->count();
    $this->assign('my_gather_id', $gather_id);
    $this->assign('my_online_num', $onlineNum);
    $this->assign('my_total_num', $totalNum);
    $this->assign('max_page', $max_page);
    // 设置模板参数...
    $this->display('gather');
  }
  //
  // 采集器分页流加载...
  public function pageGather()
  {
    // 准备需要的分页参数...
    $pagePer = 16; //C('PAGE_PER'); // 每页显示的通道数...
    $pageCur = (isset($_GET['p']) ? $_GET['p'] : 1);  // 当前页码...
    $pageLimit = (($pageCur-1)*$pagePer).','.$pagePer; // 读取范围...
    // 读取通道列表 => 在线优先排序
    $map['gather_id'] = $_GET['gather_id'];
    $arrCamera = D('LiveView')->where($map)->limit($pageLimit)->order('status DESC, updated DESC')->select();
    // 设置其它模板参数 => web_tracker_addr 已经自带了协议头 http://或https://
    $this->assign('my_web_tracker', sprintf("%s:%d/", $this->m_dbSys['web_tracker_addr'], $this->m_dbSys['web_tracker_port']));
    // 设置模板参数 => 使用 pageLive 的模版...
    $this->assign('my_cur_page', $pageCur);
    $this->assign('my_list', $arrCamera);
    $this->display('pageLive');
  }
  //
  // 播放页面...
  public function play()
  {
    // 获取直播导航数据...
    $my_nav = $this->getNavData(NAV_ACTIVE_LIVE, 0);
    // 获取直播间相关详细内容...
    $condition['live_id'] = $_GET['live_id'];
    $arrList = D('LiveView')->where($condition)->select();
    $this->assign('my_play', $arrList);
    // 设置其它模板参数 => web_tracker_addr 已经自带了协议头 http://或https://
    $theWebTracker = sprintf("%s:%d/", $this->m_dbSys['web_tracker_addr'], $this->m_dbSys['web_tracker_port']);
    // 对模板对象进行赋值，呈现页面内容...
    $this->assign('my_web_tracker', $theWebTracker);
    $this->assign('my_title', $this->m_webTitle . ' - 播放');
    $this->assign('my_nav', $my_nav);
    $this->display('play');
  }
  //
  // 删除 评论 的过程...
  public function delComment()
  {
    // 找到当前评论的数据记录...
    $condition['comment_id'] = $_POST['comment_id'];
    $dbItem = D('comment')->where($condition)->find();
    // 如果父节点大于0，需要将父节点的回复数减1 => 当前记录为回复评论...
    if( $dbItem['parent_id'] > 0 ) {
      $theQuery['comment_id'] = $dbItem['parent_id'];
      D('comment')->where($theQuery)->setDec('childs');
    }
    // 删除当前评论记录和相关点赞记录...
    D('comment')->where($condition)->delete();
    D('zan')->where($condition)->delete();
    // 查询所有的当前评论的回复列表...
    $theMap['parent_id'] = $_POST['comment_id'];
    $arrChild = D('comment')->where($theMap)->select();
    // 删除所有子评论相关的点赞记录...
    foreach($arrChild as &$dbChild) {
      $condition['comment_id'] = $dbChild['comment_id'];
      D('zan')->where($condition)->delete();
    }
    // 删除所有子评论本身的记录...
    D('comment')->where($theMap)->delete();
  }
  //
  // 保存 评论 的操作结果...
  public function saveComment()
  {
    // 设置返回状态..
    $arrErr['err_code'] = false;
    $arrErr['err_msg'] = "OK";
    // 获取登录用户编号...
    $theLoginUserID = $this->getLoginUserID();
    if( $theLoginUserID <= 0 ) {
      $arrErr['err_code'] = true;
      $arrErr['err_msg'] = '请先登录，再评论！';
      echo json_encode($arrErr);
      return;
    }
    // 对传递过来的数据进行重新整合...
    // $_POST['type'] = 0 => camera_id
    // $_POST['type'] = 1 => record_id
    // $_POST['type'] = 2 => parent_id
    // $_POST['type'] = 3 => live_id
    switch( $_POST['type'] )
    {
      case 0: $dbSave['camera_id'] = $theMap['camera_id'] = $_POST['id']; break;
      case 1: $dbSave['record_id'] = $theMap['record_id'] = $_POST['id']; break;
      case 2: $dbSave['parent_id'] = $theMap['parent_id'] = $_POST['id']; break;
      case 3: $dbSave['live_id']   = $theMap['live_id']   = $_POST['id']; break;
    }
    // 对其它字段进行处理...
    $dbSave['content'] = $_POST['content'];
    $dbSave['created'] = date('Y-m-d H:i:s');
    $dbSave['updated'] = date('Y-m-d H:i:s');
    $dbSave['user_id'] = $theLoginUserID;
    $dbSave['comment_id'] = D('comment')->add($dbSave);
    // 构造新的模版数据内容...
    $condition['comment_id'] = $dbSave['comment_id'];
    $arrComment = D('CommentView')->where($condition)->select();
    // 当前用户就是评论创建者，设置可删除标志...
    foreach($arrComment as &$dbItem) {
      $dbItem['can_del'] = 1;
    }
    // 设置当前用户登录状态，设置评论记录列表...
    $this->assign('my_is_login', (($theLoginUserID > 0) ? 1 : 0));
    $this->assign('my_list', $arrComment);
    // 计算总的评论数量和返回的新增页面数据...
    $arrErr['total'] = D('comment')->where($theMap)->count();
    $arrErr['html'] = $this->fetch('pageComment');
    echo json_encode($arrErr);
  }
  //
  // 保存 赞/踩 的操作结果...
  public function saveLike()
  {
    // 设置默认的返回数据...
    $arrData['likes_num'] = 0;
    $arrData['kicks_num'] = 0;
    // 获取登录用户编号...
    $theLoginUserID = $this->getLoginUserID();
    if( $theLoginUserID <= 0 ) {
      echo json_encode($arrData);
      return;
    }
    // 记录返回的数组内容...
    $arrData['likes_num'] = $_POST['likes_num'];
    $arrData['kicks_num'] = $_POST['kicks_num'];
    // 登录状态有效的情况...
    // $_POST['type'] = 0 => camera_id
    // $_POST['type'] = 1 => record_id
    // $_POST['type'] = 2 => comment_id
    // $_POST['type'] = 3 => live_id
    // $_POST['is_like'] = 0 => 踩 => kick
    // $_POST['is_like'] = 1 => 赞 => like
    switch( $_POST['type'] ) {
      case 0: $dbSave['camera_id'] = $_POST['id']; break;
      case 1: $dbSave['record_id'] = $_POST['id']; break;
      case 2: $dbSave['comment_id'] = $_POST['id']; break;
      case 3: $dbSave['live_id'] = $_POST['id']; break;
    }
    $dbSave['user_id'] = $theLoginUserID;
    // 通过当前条件，查找记录...
    $dbZan = D('zan')->where($dbSave)->field('zan_id,is_like')->find();
    // 如果记录存在，需要进行修改...
    if( isset($dbZan['zan_id']) ) {
      // 如果新点击与数据库记录一致，进行删除操作...
      if( $dbZan['is_like'] == $_POST['is_like'] ) {
        $theMap['zan_id'] = $dbZan['zan_id'];
        D('zan')->where($theMap)->delete();
        // 设置减少字段...
        if( $_POST['is_like'] ) {
          $theDecField = 'likes';
          $arrData['likes_num'] -= 1;
        } else {
          $theDecField = 'kicks';
          $arrData['kicks_num'] -= 1;
        }
        // 在对应的关联数据中进行减少处理...
        if( $_POST['type'] == 0 ) {
          $condition['camera_id'] = $_POST['id'];
          D('camera')->where($condition)->setDec($theDecField);
        } else if( $_POST['type'] == 1 ) {
          $condition['record_id'] = $_POST['id'];
          D('record')->where($condition)->setDec($theDecField);
        } else if( $_POST['type'] == 2 ) {
          $condition['comment_id'] = $_POST['id'];
          D('comment')->where($condition)->setDec($theDecField);
        } else if( $_POST['type'] == 3 ) {
          $condition['live_id'] = $_POST['id'];
          D('live')->where($condition)->setDec($theDecField);          
        }
      } else {
        // 如果新点击与数据库记录不一致，进行修改操作...
        $dbSave['zan_id'] = $dbZan['zan_id'];
        $dbSave['is_like'] = $_POST['is_like'];
        D('zan')->save($dbSave);
        // 设置增长字段和减少字段...
        if( $_POST['is_like'] ) {
          $theIncField = 'likes';
          $theDecField = 'kicks';
          $arrData['likes_num'] += 1;
          $arrData['kicks_num'] -= 1;
        } else {
          $theIncField = 'kicks';
          $theDecField = 'likes';
          $arrData['likes_num'] -= 1;
          $arrData['kicks_num'] += 1;
        }
        // 在对应的关联数据中进行修正处理...
        if( $_POST['type'] == 0 ) {
          $condition['camera_id'] = $_POST['id'];
          D('camera')->where($condition)->setInc($theIncField);
          D('camera')->where($condition)->setDec($theDecField);
        } else if( $_POST['type'] == 1 ) {
          $condition['record_id'] = $_POST['id'];
          D('record')->where($condition)->setInc($theIncField);
          D('record')->where($condition)->setDec($theDecField);
        } else if( $_POST['type'] == 2 ) {
          $condition['comment_id'] = $_POST['id'];
          D('comment')->where($condition)->setInc($theIncField);
          D('comment')->where($condition)->setDec($theDecField);
        } else if( $_POST['type'] == 3 ) {
          $condition['live_id'] = $_POST['id'];
          D('live')->where($condition)->setInc($theIncField);
          D('live')->where($condition)->setDec($theDecField);          
        }
      }
    } else {
      // 如果记录不存在，进行新建处理...
      $dbSave['is_like'] = $_POST['is_like'];
      $dbSave['created'] = date('Y-m-d H:i:s');
      $dbSave['updated'] = date('Y-m-d H:i:s');
      D('zan')->add($dbSave);
      // 设置增长字段...
      if( $_POST['is_like'] ) {
        $theIncField = 'likes';
        $arrData['likes_num'] += 1;
      } else {
        $theIncField = 'kicks';
        $arrData['kicks_num'] += 1;
      }
      // 在对应的关联数据中进行修正处理...
      if( $_POST['type'] == 0 ) {
        $condition['camera_id'] = $_POST['id'];
        D('camera')->where($condition)->setInc($theIncField);
      } else if( $_POST['type'] == 1 ) {
        $condition['record_id'] = $_POST['id'];
        D('record')->where($condition)->setInc($theIncField);
      } else if( $_POST['type'] == 2 ) {
        $condition['comment_id'] = $_POST['id'];
        D('comment')->where($condition)->setInc($theIncField);
      } else if( $_POST['type'] == 3 ) {
        $condition['live_id'] = $_POST['id'];
        D('live')->where($condition)->setInc($theIncField);
      }
    }
    // 直接返回计算后的赞|踩数量...
    echo json_encode($arrData);
  }
  //
  // 保存回复内容...
  public function saveReply()
  {
    // 设置返回状态..
    $arrErr['err_code'] = false;
    $arrErr['err_msg'] = "OK";
    // 获取登录用户编号...
    $theLoginUserID = $this->getLoginUserID();
    if( $theLoginUserID <= 0 ) {
      $arrErr['err_code'] = true;
      $arrErr['err_msg'] = '请先登录，再回复评论！';
      echo json_encode($arrErr);
      return;
    }
    // 设置常规的新记录信息...
    $dbNew['user_id'] = $theLoginUserID;
    $dbNew['content'] = $_POST['content'];
    $dbNew['created'] = date('Y-m-d H:i:s');
    $dbNew['updated'] = date('Y-m-d H:i:s');
    $dbNew['parent_id'] = $_POST['parent_id'];
    // 先找到父节点的评论记录...
    $condition['comment_id'] = $_POST['parent_id'];
    $dbParent = D('comment')->where($condition)->find();
    // 如果父节点本身就是子评论...
    if( $dbParent['parent_id'] > 0 ) {
      // 把父节点的父节点当成自己的父节点...
      $dbNew['parent_id'] = $dbParent['parent_id'];
    }
    // 将修正后父节点的计数器增加1...
    $condition['comment_id'] = $dbNew['parent_id'];
    D('comment')->where($condition)->setInc('childs');
    // 新建一条评论记录，返回新记录的编号...
    $condition['comment_id'] = D('comment')->add($dbNew);
    // 返回新建记录的页面内容 => 设置登录状态和模版参数...
    $arrComment = D('CommentView')->where($condition)->select();
    // 当前用户就是评论创建者，设置可删除标志...
    foreach($arrComment as &$dbItem) {
      $dbItem['can_del'] = 1;
    }
    // 直接设定模版参数内容，并设置当前用户是否处于登录状态...
    $this->assign('my_is_login', (($theLoginUserID > 0) ? 1 : 0));
    $this->assign('my_list', $arrComment);
    // 解析并返回获取到的页面内容结果...
    $arrErr['html'] = $this->fetch('pageComment');
    echo json_encode($arrErr);
  }
  //
  // 获取主评论特殊的扩展页面信息...
  public function getExpand()
  {
    $condition['comment_id'] = $_GET['comment_id'];
    $dbComment = D('comment')->where($condition)->find();
    $this->assign('my_comment', $dbComment);
    $arrErr['childs'] = $dbComment['childs'];
    $arrErr['html'] = $this->fetch();
    echo json_encode($arrErr);
  }
  //
  // 获取回复模版页面内容...
  public function getReply()
  {
    echo $this->fetch();
  }
  //
  // 获取记录信息 => 录像或直播...
  public function getFeed()
  {
    // $_GET['type'] = 0 => camera_id
    // $_GET['type'] = 1 => record_id
    // $_GET['type'] = 3 => live_id
    // 如果是点播记录...
    if( $_GET['type'] == 1 ) {
      $condition['record_id'] = $_GET['id'];
      $dbItem = D('RecordView')->where($condition)->find();
    } else if( $_GET['type'] == 3 ) {
      // 如果是直播记录...
      $condition['live_id'] = $_GET['id'];
      $dbItem = D('LiveView')->where($condition)->find();
    }
    // 获取总的评论条数...
    $pagePer = C('PAGE_PER');
    $nTotalComment = D('comment')->where($condition)->count();
    $max_comm_page = intval($nTotalComment / $pagePer);
    // 判断是否是整数倍的页码...
    $max_comm_page += (($nTotalComment % $pagePer) ? 1 : 0);
    // 判断是否已经处于登录状态...
    $dbItem['totalComment'] = $nTotalComment;
    $dbItem['isLogin'] = $this->isLogin();
    $dbItem['holder'] = '请先登录，再评论...';
    // 直接保存编号和类型...
    $dbItem['id'] = $_GET['id'];
    $dbItem['type'] = $_GET['type'];
    // 如果是已经登录状态...
    $theLoginUserID = $this->getLoginUserID();
    if( $theLoginUserID > 0 ) {
      $condition['user_id'] = $theLoginUserID;
      $dbZan = D('zan')->where($condition)->field('zan_id,is_like')->find();
      $dbItem['is_like'] = (isset($dbZan['zan_id']) ? $dbZan['is_like'] : -1);
      $dbItem['headurl'] = Cookie::get('wx_headurl');
      $dbItem['holder'] = '请输入评论内容...';
    }
    // 直接设置通用的模版参数...
    $this->assign('my_base', $dbItem);
    $arrData['feedHtml'] = $this->fetch('getFeed');
    // 返回页面需要的其它特定数据信息...
    $arrData['maxCommPage'] = $max_comm_page;
    // 计算右侧热点推荐的点播记录数量 => 不要超过10条记录...
    $nRecTotal = D('record')->count();
    $nCalcPage = intval($nRecTotal / $pagePer);
    $nCalcPage+= (($nRecTotal % $pagePer) ? 1 : 0);
    $arrData['maxHotPage'] = (($nCalcPage <= 1) ? $nCalcPage : 1);
    // 返回json数据内容...
    echo json_encode($arrData);
  }
  //
  // 获取相关评论分页内容...
  public function pageComment()
  {
    // $_GET['type'] = 0 => camera_id
    // $_GET['type'] = 1 => record_id
    // $_GET['type'] = 2 => parent_id
    // $_GET['type'] = 3 => live_id
    switch( $_GET['type'] ) {
      case 0: $condition['camera_id'] = $_GET['id']; break;
      case 1: $condition['record_id'] = $_GET['id']; break;
      case 2: $condition['parent_id'] = $_GET['id']; break;
      case 3: $condition['live_id'] = $_GET['id']; break;
    }
    // 准备需要的分页参数...
    $pagePer = C('PAGE_PER'); // 每页显示的通道数...
    $pageCur = (isset($_GET['p']) ? $_GET['p'] : 1);  // 当前页码...
    $pageLimit = (($pageCur-1)*$pagePer).','.$pagePer; // 读取范围...
    // 查找对应的评论列表...
    $arrComment = D('CommentView')->where($condition)->limit($pageLimit)->order('created DESC')->select();
    // 如果已经登录，遍历评论，查看当前登录用户是否点赞，是否能删除评论...
    $theLoginUserID = $this->getLoginUserID();
    if( $theLoginUserID > 0 ) {
      foreach($arrComment as &$dbItem) {
        // 当前登录用户对当前评论的赞或踩的情况...
        $arrQuery['user_id'] = $theLoginUserID;
        $arrQuery['comment_id'] = $dbItem['comment_id'];
        // 三种状态 -1无 0踩 1赞...
        $dbZan = D('zan')->where($arrQuery)->field('zan_id,is_like')->find();
        $dbItem['is_like'] = (isset($dbZan['zan_id']) ? $dbZan['is_like'] : -1);
        // 当前登录用户就是当前评论的创建者，则可以显示删除按钮...
        $dbItem['can_del'] = (($dbItem['user_id'] == $theLoginUserID) ? 1 : 0);
      }
    }
    // 直接设定模版参数内容，并设置当前用户是否处于登录状态...
    $this->assign('my_is_login', (($theLoginUserID > 0) ? 1 : 0));
    $this->assign('my_list', $arrComment);
    echo $this->fetch('pageComment');
  }
  //
  // 获取热点推荐分页内容...
  public function pageHot()
  {
    // 准备需要的分页参数...
    $pagePer = C('PAGE_PER'); // 每页显示的通道数...
    $pageCur = (isset($_GET['p']) ? $_GET['p'] : 1);  // 当前页码...
    $pageLimit = (($pageCur-1)*$pagePer).','.$pagePer; // 读取范围...
    // 查找对应的热点内容，按点击次数排列...
    $arrList = D('RecordView')->limit($pageLimit)->order('clicks DESC')->select();
    // 直接设定模版参数内容...
    $this->assign('my_web_tracker', sprintf("%s:%d/", $this->m_dbSys['web_tracker_addr'], $this->m_dbSys['web_tracker_port']));
    $this->assign('my_cur_page', $pageCur);
    $this->assign('my_list', $arrList);
    echo $this->fetch('pageHot');
  }
  /**
  +----------------------------------------------------------
  * 显示页面 => vod | live | width | height
  +----------------------------------------------------------
  */
  public function show()
  {
    // 统一设置设置是否是IE浏览器类型...
    $this->assign('my_isIE', $this->m_isIEBrowser);
    // 如果输入参数无效，直接返回错误信息框...
    if( (strcasecmp($_GET['type'], "live") != 0) || !isset($_GET['live_id']) ) {
      $this->dispError(false, '输入参数无效！', '请确认输入参数内容是否正确。');
      return;
    }
    // 获取当前直播记录信息...
    $condition['live_id'] = $_GET['live_id'];
    $dbLive = D('LiveView')->where($condition)->find();
    if( !isset($dbLive['live_id']) ) {
      $this->dispError(false, '没有找到指定的直播间！', '请确认输入参数内容是否正确。');
      return;
    }
    // 构造中转服务器需要的参数 => 直播编号 => LIVE_BEGIN_ID + live_id
    $dbParam['rtmp_live'] = LIVE_BEGIN_ID + $dbLive['live_id'];
    // 从中转服务器获取云教室直播链接地址...
    $dbResult = $this->getRtmpUrlFromTransmit($dbParam);
    // 如果获取连接中转服务器失败...
    if( $dbResult['err_code'] > 0 ) {
      $this->assign('my_live', $dbLive);
      $this->dispError(false, $dbResult['err_msg'], '请联系管理员，汇报错误信息。');
      return;
    }
    // 连接中转服务器成功 => 设置flvjs地址、rtmp地址、hls地址，播放器编号...
    $arrSource[0]['src'] = $dbResult['flv_url'];
    $arrSource[0]['type'] = $dbResult['flv_type'];
    $arrSource[1]['src'] = $dbResult['rtmp_url'];
    $arrSource[1]['type'] = $dbResult['rtmp_type'];
    $arrSource[2]['src'] = $dbResult['hls_url'];
    $arrSource[2]['type'] = $dbResult['hls_type'];
    $dbShow['source'] = json_encode($arrSource);
    // 这3个参数是直播播放器汇报时需要的数据...
    $dbShow['player_camera'] = $dbParam['rtmp_live'];
    $dbShow['player_id'] = $dbResult['player_id'];
    $dbShow['player_vod'] = 0;
    // 直播flvjs(替代flash)，flash以优先(延时小)，html5垫后(延时大)...
    $arrTech[0] = "flvjs";
    $arrTech[1] = "flash";
    $arrTech[2] = "html5";
    $dbShow['tech'] = json_encode($arrTech);
    // 反馈点击次数给显示层，当前默认录像编号...
    $dbShow['clicks'] = intval($dbLive['clicks']) + 1;
    $dbShow['click_id'] = $dbLive['live_id'];
    $dbShow['record_id'] = 0;
    // 累加点击计数器，写入数据库...
    $dbSave['live_id'] = $dbLive['live_id'];
    $dbSave['clicks'] = $dbShow['clicks'];
    D('live')->save($dbSave);
    // 获取传递过来的视频窗口大小...
    $dbShow['width'] = isset($_GET['width']) ? ($_GET['width'] - 2) : 840;
    $dbShow['height'] = isset($_GET['height']) ? ($_GET['height'] - 2) : 545;
    // 赋值给播放器模板页面...
    $this->assign('my_show', $dbShow);
    $this->display('show');
  }
  /*public function show()
  {
    // 统一设置设置是否是IE浏览器类型...
    $this->assign('my_isIE', $this->m_isIEBrowser);
    // 根据type类型获取url地址...
    if( strcasecmp($_GET['type'], "vod") == 0 ) {
      // 获取点播记录信息 => web_tracker_addr 已经自带了协议头 http://或https://
      $map['record_id'] = $_GET['record_id'];
      $dbVod = D('record')->where($map)->field('record_id,file_fdfs,clicks')->find();
      // 设置播放页面需要的数据内容 => 这里为source做特殊处理...
      $arrSource[0]['src'] = sprintf("%s:%d/%s", $this->m_dbSys['web_tracker_addr'], $this->m_dbSys['web_tracker_port'], $dbVod['file_fdfs']);
      $arrSource[0]['type'] = "video/mp4";
      $dbShow['source'] = json_encode($arrSource);
      // 为了与直播兼容设置的数据...
      $dbShow['player_id'] = -1;
      $dbShow['player_vod'] = 1;
      $dbShow['player_camera'] = -1;
      // 点播以html5优先，flash垫后...
      $arrTech[0] = "html5";
      $arrTech[1] = "flash";
      $dbShow['tech'] = json_encode($arrTech);
      // 反馈点击次数给显示层，当期播放录像编号...
      $dbShow['clicks'] = intval($dbVod['clicks']) + 1;
      $dbShow['click_id'] = $dbVod['record_id'];
      $dbShow['record_id'] = $dbVod['record_id'];
      // 累加点播计数器，写入数据库...
      $dbSave['clicks'] = $dbShow['clicks'];
      $dbSave['record_id'] = $dbVod['record_id'];
      D('record')->save($dbSave);
    } else if( strcasecmp($_GET['type'], "live") == 0 ) {
      // 首先，判断通道是否处于直播状态...
      $map['camera_id'] = $_GET['camera_id'];
      $bHideIcon = (($_GET['width'] <= 300) ? true : false);
      $dbCamera = D('LiveView')->where($map)->field('camera_id,clicks,status,gather_id,mac_addr')->find();
      // 配置错误通知信息...
      if( $dbCamera['status'] <= 0 ) {
        $this->assign('my_camera', $dbCamera);
        $this->dispError(true, '当前通道处于离线状态，无法播放！', '请联系管理员，开启通道。', $bHideIcon);
        return;
      }
      // 中转服务器需要的参数...
      $dbParam['mac_addr'] = $dbCamera['mac_addr'];
      $dbParam['rtmp_live'] = $dbCamera['camera_id'];
      // 获取直播链接地址...
      $dbResult = $this->getRtmpUrlFromTransmit($dbParam);
      // 如果获取连接中转服务器失败...
      if( $dbResult['err_code'] > 0 ) {
        $this->assign('my_camera', $dbCamera);
        $this->dispError(true, $dbResult['err_msg'], '请联系管理员，汇报错误信息。', $bHideIcon);
        return;
      }
      // 连接中转服务器成功 => 设置flvjs地址、rtmp地址、hls地址，播放器编号...
      $arrSource[0]['src'] = $dbResult['flv_url'];
      $arrSource[0]['type'] = $dbResult['flv_type'];
      $arrSource[1]['src'] = $dbResult['rtmp_url'];
      $arrSource[1]['type'] = $dbResult['rtmp_type'];
      $arrSource[2]['src'] = $dbResult['hls_url'];
      $arrSource[2]['type'] = $dbResult['hls_type'];
      $dbShow['source'] = json_encode($arrSource);
      // 这3个参数是直播播放器汇报时需要的数据...
      $dbShow['player_camera'] = $dbCamera['camera_id'];
      $dbShow['player_id'] = $dbResult['player_id'];
      $dbShow['player_vod'] = 0;
      // 直播flvjs(替代flash)，flash以优先(延时小)，html5垫后(延时大)...
      $arrTech[0] = "flvjs";
      $arrTech[1] = "flash";
      $arrTech[2] = "html5";
      $dbShow['tech'] = json_encode($arrTech);
      // 反馈点击次数给显示层，当期播放录像编号...
      $dbShow['clicks'] = intval($dbCamera['clicks']) + 1;
      $dbShow['click_id'] = $dbCamera['camera_id'];
      $dbShow['record_id'] = 0;
      // 累加点播计数器，写入数据库...
      $dbCamera['clicks'] = $dbShow['clicks'];
      D('camera')->save($dbCamera);
    }
    // 获取传递过来的视频窗口大小...
    $dbShow['width'] = isset($_GET['width']) ? ($_GET['width'] - 2) : 840;
    $dbShow['height'] = isset($_GET['height']) ? ($_GET['height'] - 2) : 545;
    //print_r($dbShow); exit;
    // 赋值给播放器模板页面...
    $this->assign('my_show', $dbShow);
    $this->display('show');
  }*/
  //
  // 显示超时模板信息...
  private function dispError($bIsDownload, $inMsgTitle, $inMsgDesc, $bHideIcon = false)
  {
    $this->assign('my_icon', $bHideIcon);
    $this->assign('my_download', $bIsDownload);
    $this->assign('my_title', '糟糕，出错了');
    $this->assign('my_msg_title', $inMsgTitle);
    $this->assign('my_msg_desc', $inMsgDesc);
    $this->display('Common:error_page');
  }
  //
  // 2017.06.15 - by jackey => 修改了中转服务器代码，转发命令给采集端之后，立即返回rtmp地址，无需等待采集端上传结果，避免阻塞，让播放器自己去等待...
  // 从中转服务器获取直播地址...
  // 成功 => array()
  // 失败 => false
  private function getRtmpUrlFromTransmit(&$dbParam)
  {
    // 通过php扩展插件连接中转服务器 => 性能高...
    $transmit = transmit_connect_server($this->m_dbSys['transmit_addr'], $this->m_dbSys['transmit_port']);
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
    // 通过错误码，获得错误信息...
    $arrData['err_msg'] = getTransmitErrMsg($arrData['err_code']);
    // 将整个数组返回...
    return $arrData;
  }
  //
  // 下载相关录像文件列表...
  public function downRecord()
  {
    // 判断输入参数是否有效...
    if( !isset($_GET['record_id']) ) {
      $this->dispError(false, '输入参数无效', '请确认录像编号是否有效。');
      return;
    }
    // 通过编号查找对应的录像记录...
    $map['record_id'] = $_GET['record_id'];
    $dbCurRec = D('record')->where($map)->field('record_id, slice_id')->find();
    if( !isset($dbCurRec['record_id']) ) {
      $this->dispError(false, '无法找到指定的录像记录', '请确认录像编号是否有效。');
      return;
    }
    // 保存当前正在播放的录像编号...
    $arrShow['cur_record'] = $_GET['record_id'];
    // 通过 slice_id 查找其它相关的录像文件，并以 slice_inx 排序...
    $condition['slice_id'] = $dbCurRec['slice_id'];
    $arrShow['list'] = D('record')->where($condition)->order('slice_inx ASC')->select();
    $arrShow['total_num'] = count($arrShow['list']);
    // 获取视频播放地址连接...
    $arrShow['vod_addr'] = sprintf("%s:%d/", $this->m_dbSys['web_tracker_addr'], $this->m_dbSys['web_tracker_port']);
    // 设置模版参数，显示模版文件...
    $this->assign('my_show', $arrShow);
    $this->display();
  }
}
?>