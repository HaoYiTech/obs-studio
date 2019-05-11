
// 获取全局的app对象...
const g_app = getApp()

Page({
  /**
   * 页面的初始数据
   */
  data: {
    m_arrUser: [],
    m_cur_page: 1,
    m_max_page: 1,
    m_total_num: 0,
    m_str_query: '',
    m_show_more: true,
    m_no_more: '正在加载...',
    m_userTypeName: g_app.globalData.m_userTypeName
  },

  // 搜索框发生变化...
  onChange: function (event) {
    this.setData({ 
      m_str_query: event.detail,
      m_arrUser: [], m_cur_page: 1, 
      m_max_page: 1, m_total_num: 0, 
      m_show_more: true, m_no_more: '正在加载...' });
    this.doAPIGetUser();
  },

  // 点击搜索按钮的操作...
  onSearch: function (event) {
  },

  /**
   * 生命周期函数--监听页面加载
   */
  onLoad: function (options) {
    this.doAPIGetUser();
  },

  // 获会员用户列表...
  doAPIGetUser: function () {
    // 显示导航栏|浮动加载动画...
    wx.showLoading({ title: '加载中' });
    // 保存this对象...
    var that = this
    // 准备需要的参数信息...
    var thePostData = {
      'cur_page': that.data.m_cur_page,
      'search': that.data.m_str_query,
    }
    // 构造访问接口连接地址...
    var theUrl = g_app.globalData.m_urlPrev + 'Mini/getUser';
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
          that.setData({ m_show_more: false, m_no_more: '获取会员用户列表失败' })
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
        if ((arrData.user instanceof Array) && (arrData.user.length > 0)) {
          that.data.m_arrUser = that.data.m_arrUser.concat(arrData.user)
        }
        // 保存获取到的记录总数和总页数...
        that.data.m_total_num = arrData.total_num
        that.data.m_max_page = arrData.max_page
        wx.setNavigationBarTitle({
          title: '会员管理(' + that.data.m_total_num + ')',
        })
        // 将数据显示到模版界面上去，并且显示加载更多页面...
        that.setData({ m_arrUser: that.data.m_arrUser })
        // 如果到达最大页数，关闭加载更多信息...
        if (that.data.m_cur_page >= that.data.m_max_page) {
          that.setData({ m_show_more: false, m_no_more: '' })
        }
      },
      fail: function (res) {
        // 隐藏导航栏加载动画...
        wx.hideLoading()
        that.setData({ m_show_more: false, m_no_more: '获取会员用户记录失败' })
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
    console.log('onReachBottom')
    // 如果到达最大页数，关闭加载更多信息...
    if (this.data.m_cur_page >= this.data.m_max_page) {
      this.setData({ m_show_more: false, m_no_more: '没有更多内容了' })
      return
    }
    // 没有达到最大页数，累加当前页码，请求更多数据...
    this.data.m_cur_page += 1
    this.doAPIGetUser()
  },

  /**
   * 用户点击右上角分享
   */
  onShareAppMessage: function () {

  }
})