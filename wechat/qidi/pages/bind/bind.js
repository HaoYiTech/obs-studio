
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
    m_code: '',
    m_show_auth: false,
  },

  /**
   * 生命周期函数--监听页面加载
   */
  onLoad: function (options) {
    // 直接保存扫码返回的场景参数...
    let theAppData = g_app.globalData;
    theAppData.m_scanType = options.scene;
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
          that.setData({ m_show_auth: true, m_title: '错误警告', m_label: '调用网站登录接口失败！' });
          return
        }
        // dataType 没有设置json，需要自己转换...
        var arrData = JSON.parse(res.data);
        // 获取授权数据失败的处理...
        if (arrData.err_code > 0) {
          that.setData({ m_show_auth: true, m_title: '错误警告', m_label: arrData.err_msg });
          return
        }
        // 获取授权数据成功，保存用户编号|用户类型|真实姓名...
        g_app.globalData.m_nUserID = arrData.user_id
        g_app.globalData.m_userInfo = inUserInfo
        g_app.globalData.m_curRoomItem = arrData.bind_room
        g_app.globalData.m_userInfo.userType = arrData.user_type
        g_app.globalData.m_userInfo.realName = arrData.real_name
        // 注意：这里必须使用reLaunch，redirectTo不起作用...
        wx.reLaunch({ url: '../home/home?type=bind' })
      },
      fail: function (res) {
        console.log(res);
        // 隐藏导航栏加载动画...
        wx.hideLoading()
        wx.hideNavigationBarLoading();
        // 打印错误信息，显示错误警告...
        that.setData({ m_show_auth: true, m_title: '错误警告', m_label: '调用网站登录接口失败！' });
      }
    })
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