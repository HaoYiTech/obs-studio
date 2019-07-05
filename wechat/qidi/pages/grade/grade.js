
// 获取全局的app对象...
const g_appData = getApp().globalData;

Page({
  /**
   * 页面的初始数据
   */
  data: {
    m_arrSubject: [],
    m_arrGrade: [],
    m_total_num: 0,
    m_show_more: true,
    m_no_more: '正在加载...',
  },

  // 点击某个班级的操作...
  doShowClass: function (event) {
    g_appData.m_curSelectItem = event.currentTarget.dataset;
    wx.navigateTo({ url: '../gradeItem/gradeItem' });
  },

  /**
   * 生命周期函数--监听页面加载
   */
  onLoad: function (options) {
    this.doAPIGetAllGrade();
  },

  // 获取所有班级列表...
  doAPIGetAllGrade: function () {
    // 显示导航栏|浮动加载动画...
    wx.showLoading({ title: '加载中' });
    // 保存this对象...
    var that = this;
    // 构造访问接口连接地址...
    var theUrl = g_appData.m_urlPrev + 'Mini/getAllGrade';
    // 请求远程API过程...
    wx.request({
      url: theUrl,
      method: 'GET',
      dataType: 'x-www-form-urlencoded',
      header: { 'content-type': 'application/x-www-form-urlencoded' },
      success: function (res) {
        // 隐藏导航栏加载动画...
        wx.hideLoading();
        // 调用接口失败...
        if (res.statusCode != 200) {
          that.setData({ m_show_more: false, m_no_more: '获取班级记录失败' })
          return
        }
        // dataType 没有设置json，需要自己转换...
        var arrData = JSON.parse(res.data);
        // 获取到的记录数据不为空时才进行记录合并处理 => concat 不会改动原数据
        if ((arrData.subject instanceof Array) && (arrData.subject.length > 0)) {
          that.data.m_arrSubject = that.data.m_arrSubject.concat(arrData.subject)
        }
        if ((arrData.grade instanceof Array) && (arrData.grade.length > 0)) {
          that.data.m_arrGrade = that.data.m_arrGrade.concat(arrData.grade)
        }
        // 计算并显示班级总数...
        that.data.m_total_num = that.data.m_arrGrade.length * that.data.m_arrSubject.length;
        wx.setNavigationBarTitle({
          title: '班级管理(' + that.data.m_total_num + ')',
        })
        // 将数据显示到模版界面上去，并且显示加载更多页面...
        that.setData({
          m_no_more: '',
          m_show_more: false,
          m_arrGrade: that.data.m_arrGrade,
          m_arrSubject: that.data.m_arrSubject,
          m_total_num: that.data.m_total_num
        });
      },
      fail: function (res) {
        // 隐藏导航栏加载动画...
        wx.hideLoading()
        that.setData({ m_show_more: false, m_no_more: '获取班级记录失败' })
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