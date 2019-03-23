// pages/home/home.js

Page({

  /**
   * 页面的初始数据
   */
  data: {
    showLoading: true,
    hotTabs: [
      {
        text: '热门课程',
        icon: 'fire',
        iconSize: '36rpx',
        iconColor: '#ef473a',
      },
      {
        text: '热门活动',
        icon: 'fire',
        iconSize: '36rpx',
        iconColor: '#ef473a',
      }
    ]
  },

  // 点击标签栏切换事件...
  handleChange(event) {
    wx.showToast({
      title: `切换到标签 ${event.detail.value + 1}`,
      icon: 'none'
    });
  },

  // 点击导航栏事件...
  toggleGridNav(event) {
    console.log(event.currentTarget.dataset['item']);
  },

  /**
   * 生命周期函数--监听页面加载
   */
  onLoad: function (options) {
  },

  /**
   * 生命周期函数--监听页面初次渲染完成
   */
  onReady: function () {
    this.setData({ showLoading: false });
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