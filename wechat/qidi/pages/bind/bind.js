
// 定义绑定登录子命令...
const BIND_SCAN = 1
const BIND_SAVE = 2
const BIND_CANCEL = 3

// 获取全局的app对象...
const g_app = getApp()

Page({
  /**
   * 页面的初始数据
   */
  data: {
    icon: {
      type: 'warn',
      color: '#ef473a',
    },
    buttons: [{
      type: 'balanced',
      block: true,
      text: '点击授权',
      openType: 'getUserInfo',
    }],
    btnScan: [{
      type: 'balanced',
      block: true,
      text: '重新扫描',
      openType: '',
    }],
    btnAuths: [{
      type: 'balanced',
      outline: false,
      block: true,
      text: '确认登录',
    }, {
      type: 'stable',
      outline: true,
      block: true,
      text: '取消',
    }],
    m_code: '',
    m_btnClick: '',
    m_show_auth: false,
  },

  // 点击确认登录或取消...
  doClickAuth: function(e) {
    console.log(e);
    const { index } = e.detail;
    // 确认或取消，发送不同的命令...
    if (index === 0) {
      this.doAPIBindMini(BIND_SAVE);
    } else if (index === 1) {
      this.doAPIBindMini(BIND_CANCEL);
    }
  },

  // 用户点击重新扫描按钮...
  doClickScan: function () {
    wx.scanCode({
      onlyFromCamera: true,
      success: (res) => {
        // 打印扫描结果...
        console.log(res)
        // 如果路径有效，直接跳转到相关页面 => path 已确定会带参数...
        if (typeof res.path != 'undefined' && res.path.length > 0) {
          res.path = '../../' + res.path
          wx.redirectTo({ url: res.path })
        }
      },
      fail: (res) => {
        console.log(res)
      }
    })
  },
  /**
   * 生命周期函数--监听页面加载
   */
  onLoad: function (options) {
    console.log(options);
    // 解析二维码场景值 => type_socket_rand
    let arrData = options.scene.split('_');
    // 判断场景值 => 必须是数组，必须是3个字段...
    if (!(arrData instanceof Array) || (arrData.length != 3) ||
         (arrData[0] != 'teacher' && arrData[0] != 'student')) {
      // 场景值格式不正确，需要重新扫描...
      this.setData({
        m_show_auth: true,
        m_title: "扫码登录失败",
        m_label: "您扫描的二维码格式不正确，请确认后重新扫描！",
        m_btnClick: 'doClickScan',
        buttons: this.data.btnScan,
      });
      return;
    }
    // 将解析的场景值进行分别存储...
    let theAppData = g_app.globalData;
    theAppData.m_scanType = arrData[0];
    theAppData.m_scanSockFD = arrData[1];
    theAppData.m_scanTimeID = arrData[2];
    // 注意：这里进行了屏蔽，只要扫码完成，就需要完成全部验证过程，不要简化...
    // 如果用户编号和用户信息有效，直接跳转到房间聊天页面，使用不可返回的wx.reLaunch...
    //if (theAppData.m_nUserID > 0 && theAppData.m_userInfo != null && theAppData.m_curRoomItem != null) {
    //  wx.reLaunch({ url: '../home/home?type=bind' })
    //  return;
    //}
    // 用户编号|用户信息|房间信息，只要无效，开始弹框授权...
    wx.showLoading({ title: '加载中' });
    // 保存this对象...
    var that = this
    // 处理登录过程...
    wx.login({
      success: res => {
        //保存code，后续使用...
        that.setData({ m_code: res.code })
        // 立即读取用户信息，第一次会弹授权框...
        wx.getUserInfo({
          lang: 'zh_CN',
          withCredentials: true,
          success: res => {
            console.log(res);
            wx.hideLoading();
            // 获取成功，通过网站接口获取用户编号...
            that.doAPILogin(that.data.m_code, res.userInfo, res.encryptedData, res.iv);
          },
          fail: res => {
            console.log(res);
            wx.hideLoading();
            // 获取失败，显示授权对话框...
            that.setData({ 
              m_show_auth: true,
              m_title: "授权失败",
              m_label: "本小程序需要您的授权，请点击授权按钮完成授权。"
            });
          }
        })
      }
    })
  },
  // 拒绝授权|允许授权之后的回调接口...
  getUserInfo: function (res) {
    // 注意：拒绝授权，也要经过这个接口，没有res.detail.userInfo信息...
    console.log(res.detail);
    // 保存this对象...
    var that = this
    // 允许授权，通过网站接口获取用户编号...
    if (res.detail.userInfo) {
      that.doAPILogin(that.data.m_code, res.detail.userInfo, res.detail.encryptedData, res.detail.iv)
    }
  },
  // 登录接口...
  doAPILogin: function (inCode, inUserInfo, inEncrypt, inIV) {
    // 显示导航栏|浮动加载动画...
    wx.showLoading({ title: '加载中' });
    wx.showNavigationBarLoading();
    // 保存this对象...
    var that = this
    // 获取系统信息同步接口...
    var theSysInfo = g_app.globalData.m_sysInfo
    // 准备需要的参数信息 => 加入一些附加信息...
    var thePostData = {
      iv: inIV,
      code: inCode,
      encrypt: inEncrypt,
      wx_brand: theSysInfo.brand,
      wx_model: theSysInfo.model,
      wx_version: theSysInfo.version,
      wx_system: theSysInfo.system,
      wx_platform: theSysInfo.platform,
      wx_SDKVersion: theSysInfo.SDKVersion,
      wx_pixelRatio: theSysInfo.pixelRatio,
      wx_screenWidth: theSysInfo.screenWidth,
      wx_screenHeight: theSysInfo.screenHeight,
      wx_fontSizeSetting: theSysInfo.fontSizeSetting
    }
    // 构造访问接口连接地址...
    var theUrl = g_app.globalData.m_urlPrev + 'Mini/login'
    // 请求远程API过程...
    wx.request({
      url: theUrl,
      method: 'POST',
      data: thePostData,
      dataType: 'x-www-form-urlencoded',
      header: { 'content-type': 'application/x-www-form-urlencoded' },
      success: function (res) {
        console.log(res);
        // 隐藏导航栏加载动画...
        wx.hideLoading()
        wx.hideNavigationBarLoading();
        // 如果返回数据无效或状态不对，打印错误信息，直接返回...
        if (res.statusCode != 200 || res.data.length <= 0) {
          that.doBindError("错误警告", "调用网站登录接口失败！")
          return
        }
        // dataType 没有设置json，需要自己转换...
        var arrData = JSON.parse(res.data);
        // 获取授权数据失败的处理...
        if (arrData.err_code > 0) {
          that.doBindError("错误警告", arrData.err_msg)
          return
        }
        // 获取授权数据成功，保存用户编号|用户类型|真实姓名...
        g_app.globalData.m_nUserID = arrData.user_id
        g_app.globalData.m_userInfo = inUserInfo
        g_app.globalData.m_curRoomItem = arrData.bind_room
        g_app.globalData.m_userInfo.userType = arrData.user_type
        g_app.globalData.m_userInfo.realName = arrData.real_name
        // 如果是讲师端扫描，用户类型必须是讲师，绑定房间必须有效...
        if (g_app.globalData.m_scanType === 'teacher') {
          // 如果用户类型不是讲师，需要弹框警告...
          if (parseInt(arrData.user_type) != 2) {
            that.doBindError("错误警告", "讲师端软件，只有讲师身份的用户才能使用，请联系经销商，获取讲师身份授权！");
            return;
          }
          // 如果讲师没有绑定房间，需要弹框警告...
          if (!(arrData.bind_room instanceof Object)) {
            that.doBindError("错误警告", "您已是讲师身份，但没有分配房间号码，请联系经销商，获取讲师专属房间号码！");
            return;
          }
        }
        // 向对应终端发送扫码成功子命令...
        that.doAPIBindMini(BIND_SCAN);
      },
      fail: function (res) {
        console.log(res);
        // 隐藏导航栏加载动画...
        wx.hideLoading()
        wx.hideNavigationBarLoading();
        // 打印错误信息，显示错误警告...
        that.doBindError("错误警告", "调用网站登录接口失败！")
      }
    })
  },

  // 转发绑定子命令到对应的终端对象...
  doAPIBindMini: function (inSubCmd) {
    // 获取到的用户信息有效，弹出等待框...
    wx.showLoading({ title: '加载中' })
    wx.showNavigationBarLoading()
    // 保存this对象...
    var strError = ''
    var that = this
    // 准备需要的参数信息...
    var thePostData = {
      'tcp_socket': g_app.globalData.m_scanSockFD,
      'tcp_time': g_app.globalData.m_scanTimeID,
      'user_id': g_app.globalData.m_nUserID,
      'bind_cmd': inSubCmd
    }
    // 构造访问接口连接地址 => 通过PHP转发给中心 => 中心再转发给终端...
    var theUrl = g_app.globalData.m_urlPrev + 'Mini/bindLogin'
    // 请求远程API过程...
    wx.request({
      url: theUrl,
      method: 'POST',
      data: thePostData,
      dataType: 'x-www-form-urlencoded',
      header: { 'content-type': 'application/x-www-form-urlencoded' },
      success: function (res) {
        wx.hideLoading();
        wx.hideNavigationBarLoading();
        // 调用接口失败 => 直接返回
        if (res.statusCode != 200) {
          strError = '发送命令失败，错误码：' + res.statusCode
          that.doBindError("中转命令发送失败", strError)
          return
        }
        // dataType 没有设置json，需要自己转换...
        var arrData = JSON.parse(res.data);
        // 汇报反馈失败的处理 => 直接返回...
        if (arrData.err_code > 0) {
          strError = arrData.err_msg + ' 错误码：' + arrData.err_code
          that.doBindError("中转命令发送失败", strError)
          return
        }
        // 注意：这里必须使用reLaunch，redirectTo不起作用...
        if (inSubCmd == BIND_SAVE) {
          // 如果点了 确认登录 直接跳转到带参数首页页面...
          wx.reLaunch({ url: '../home/home?type=bind' })
        } else if (inSubCmd == BIND_CANCEL) {
          // 如果点了 取消 直接跳转到无参数首页页面...
          that.doBindError("您已取消此次登录", "您可再次扫描登录，或关闭窗口！")
        } else if (inSubCmd == BIND_SCAN) {
          // 显示扫码授权成功界面，需要用户点击确定登录或取消 => 需要计算终端类型...
          let strType = ((g_app.globalData.m_scanType === 'teacher') ? '讲师端' : '学生端');
          that.setData({
            m_show_auth: true,
            m_btnClick: 'doClickAuth',
            m_title: "扫描授权成功",
            m_label: "请点击确认登录按钮，完成 " + strType + " 授权登录。",
            buttons: that.data.btnAuths,
            'icon.color': '#33cd5f',
            'icon.type': 'success',
          });
        }
      },
      fail: function (res) {
        wx.hideLoading();
        wx.hideNavigationBarLoading();
        // 打印错误信息，显示错误警告...
        that.doBindError("错误警告", "调用网站通知接口失败！")
      }
    })
  },
  // 显示绑定错误时的页面信息...
  doBindError: function(inTitle, inError) {
    this.setData({
      m_show_auth: true,
      m_title: inTitle,
      m_label: inError,
      m_btnClick: 'doClickScan',
      buttons: this.data.btnScan,
      'icon.color': '#ef473a',
      'icon.type': 'warn',
    });
  },
  /**
   * 生命周期函数--监听页面初次渲染完成
   */
  onReady: function () {
  },

  /**
   * 生命周期函数--监听页面显示
   */
  onShow: function () {
  },

  /**
   * 生命周期函数--监听页面隐藏
   */
  onHide: function () {
  },

  /**
   * 生命周期函数--监听页面卸载
   */
  onUnload: function () {
  },

  /**
   * 页面相关事件处理函数--监听用户下拉动作
   */
  onPullDownRefresh: function () {
  },

  /**
   * 页面上拉触底事件的处理函数
   */
  onReachBottom: function () {
  },

  /**
   * 用户点击右上角分享
   */
  onShareAppMessage: function () {
  }
})