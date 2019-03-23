
// 获取全局的app对象...
const g_app = getApp()

Page({
  /**
   * 页面的初始数据
   */
  data: {
    m_cur_page: 1,
    m_max_page: 1,
    m_total_num: 0,
    m_arrRoom: [],
    m_show_more: true
  },

  /**
   * 生命周期函数--监听页面加载
   */
  onLoad: function (options) {
    // 显示加载动画，做了双层动画加载处理...
    wx.showLoading({ title: '加载中' });
    // 调用接口，获取房间列表...
    this.doAPIGetRoom()
  },

  // 获取房间列表...
  doAPIGetRoom: function () {
    // 显示导航栏|浮动加载动画...
    wx.showLoading({ title: '加载中' });
    wx.showNavigationBarLoading();
    // 保存this对象...
    var that = this
    // 准备需要的参数信息...
    var thePostData = {
      'cur_page': that.data.m_cur_page
    }
    // 构造访问接口连接地址...
    var theUrl = g_app.globalData.m_urlPrev + 'Mini/getRoom'
    // 请求远程API过程...
    wx.request({
      url: theUrl,
      method: 'POST',
      data: thePostData,
      dataType: 'x-www-form-urlencoded',
      header: { 'content-type': 'application/x-www-form-urlencoded' },
      success: function (res) {
        // 隐藏导航栏加载动画...
        wx.hideLoading()
        wx.hideNavigationBarLoading();
        // 调用接口失败...
        if (res.statusCode != 200) {
          that.setData({ m_show_more: false, m_no_more: '获取房间记录失败' })
          return
        }
        // dataType 没有设置json，需要自己转换...
        var arrData = JSON.parse(res.data);
        // 获取失败的处理 => 显示获取到的错误信息...
        if (arrData.err_code > 0) {
          that.setData({ m_show_more: false, m_no_more: arrData.err_msg })
          return
        }
        // 获取到的记录数据不为空时才进行记录合并处理 => concat 不会改动原数据
        if ((arrData.room instanceof Array) && (arrData.room.length > 0)) {
          that.data.m_arrRoom = that.data.m_arrRoom.concat(arrData.room)
        }
        // 保存获取到的记录总数和总页数...
        that.data.m_total_num = arrData.total_num
        that.data.m_max_page = arrData.max_page
        // 如果到达最大页数，关闭加载更多信息...
        if (that.data.m_cur_page >= that.data.m_max_page) {
          that.setData({ m_show_more: false, m_no_more: '没有更多内容了' })
        }
        // 将数据显示到模版界面上去，并且显示加载更多页面...
        that.setData({ m_arrRoom: that.data.m_arrRoom })
      },
      fail: function (res) {
        // 隐藏导航栏加载动画...
        wx.hideLoading()
        wx.hideNavigationBarLoading();
        that.setData({ m_show_more: false, m_no_more: '获取房间记录失败' })
      }
    })
  },
  /**
   * 生命周期函数--监听页面初次渲染完成
   */
  onReady: function () {
    console.log('onReady')
    wx.hideLoading()
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

  // 下拉刷新事件...
  onPullDownRefresh: function () {
    // 首先，打印信息，停止刷新...
    console.log('onPullDownRefresh')
    wx.stopPullDownRefresh()
    // 重新从首页开始更新数据...
    this.data.m_arrRoom = [];
    this.data.m_cur_page = 1
    this.doAPIGetRoom()
  },

  /**
   * 页面上拉触底事件的处理函数
   */
  onReachBottom: function () {
    console.log('onReachBottom')
    // 如果到达最大页数，关闭加载更多信息...
    if (this.data.m_cur_page >= this.data.m_max_page) {
      this.setData({ m_show_more: false, m_no_more: '没有更多内容了' })
      return
    }
    // 没有达到最大页数，累加当前页码，请求更多数据...
    this.data.m_cur_page += 1
    this.doAPIGetRoom()
  },

  /**
   * 用户点击右上角分享
   */
  onShareAppMessage: function () {
  }
})