
import Notify from '../../vant-weapp/notify/notify';

// 获取全局的app对象...
const g_app = getApp()

Page({
  /**
   * 页面的初始数据
   */
  data: {
    m_curUser: null,
    m_bCanUserType: false,
    m_userTypeName: g_app.globalData.m_userTypeName,
    m_arrParentType: g_app.globalData.m_parentTypeName
  },

  onNameChange: function (event) {
    this.setData({ 'm_curUser.real_name': event.detail });
  },

  onPhoneChange: function (event) {
    this.setData({ 'm_curUser.real_phone': event.detail });
  },

  onUserTypeChange: function (event) {
    const { value } = event.detail;
    this.setData({ 'm_curUser.user_type': value });
  },

  // 家长身份发生变化时的处理...
  onParentTypeChange: function (event) {
    const { value } = event.detail;
    this.setData({ 'm_curUser.parent_type': value });
  },
  
  // 点击取消时的处理...
  doBtnCancel: function (event) {
    wx.navigateBack();
  },

  // 点击保存时的处理...
  doBtnSave: function (event) {
    // 保存this对象...
    let that = this
    let theCurUser = this.data.m_curUser;
    if (theCurUser.real_name == null || theCurUser.real_name.length <= 0) {
      Notify('【家长姓名】不能为空，请重新输入！');
      return;
    }
    if (theCurUser.real_phone == null || theCurUser.real_phone.length <= 0) {
      Notify('【家长电话】不能为空，请重新输入！');
      return;
    }
    // 显示导航栏|浮动加载动画...
    wx.showLoading({ title: '加载中' });
    // 准备需要的参数信息...
    var thePostData = {
      'user_id': theCurUser.user_id,
      'real_name': theCurUser.real_name,
      'real_phone': theCurUser.real_phone,
      'parent_type': theCurUser.parent_type,
      'user_type': theCurUser.user_type,
    }
    // 构造访问接口连接地址...
    let theUrl = g_app.globalData.m_urlPrev + 'Mini/saveUser';
    // 请求远程API过程...
    wx.request({
      url: theUrl,
      method: 'POST',
      data: thePostData,
      dataType: 'x-www-form-urlencoded',
      header: { 'content-type': 'application/x-www-form-urlencoded' },
      success: function (res) {
        // 隐藏导航栏加载动画...
        wx.hideLoading();
        // 调用接口失败...
        if (res.statusCode != 200) {
          Notify('更新用户记录失败！');
          return;
        }
        // 进行父页面的数据更新...
        let pages = getCurrentPages();
        let prevItemPage = pages[pages.length - 2];
        let prevParentPage = pages[pages.length - 3];
        let theIndex = parseInt(theCurUser.indexID);
        let theArrUser = prevParentPage.data.m_arrUser;
        let thePrevUser = theArrUser[theIndex];
        thePrevUser.real_name = theCurUser.real_name;
        thePrevUser.real_phone = theCurUser.real_phone;
        thePrevUser.parent_type = theCurUser.parent_type;
        thePrevUser.user_type = theCurUser.user_type;
        prevParentPage.setData({ m_arrUser: theArrUser });
        prevItemPage.setData({ m_curUser: thePrevUser });
        // 进行页面的更新跳转...
        wx.navigateBack();
      },
      fail: function (res) {
        wx.hideLoading()
        Notify('更新门店记录失败！');
      }
    })
  },

  /**
   * 生命周期函数--监听页面加载
   */
  onLoad: function (options) {
    let loginUser = g_app.globalData.m_userInfo;
    let curUser = g_app.globalData.m_curSelectItem;
    let bCanUserType = ((loginUser.userType >= g_app.globalData.m_userTypeID.kMaintainUser) ? true : false);
    wx.setNavigationBarTitle({ title: curUser.wx_nickname + ' - 家长信息' });
    this.setData({ m_curUser: curUser, m_bCanUserType: bCanUserType });
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