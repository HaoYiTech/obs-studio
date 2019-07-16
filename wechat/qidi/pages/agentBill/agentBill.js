
// 获取全局的app对象...
const g_appData = getApp().globalData;

Page({
  /**
   * 页面的初始数据
   */
  data: {
    m_curAgent: null,
    m_arrCost: [],
    m_cur_page: 1,
    m_max_page: 1,
    m_total_num: 0,
    m_show_more: true,
    m_no_more: '正在加载...',
  },
  // 点击'学生费用'的响应...
  doBtnStudent: function (event) {
    const { id: nIndexID } = event.currentTarget;
    let theCurFlow = this.data.m_arrCost[nIndexID];
    wx.navigateTo({ url: '../agentStudent/agentStudent?flow_id=' + theCurFlow.flow_id });
  },
  /**
   * 生命周期函数--监听页面加载
   */
  onLoad: function (options) {
    this.doAPIGetCost(true);
  },
  // 获取消费列表...
  doAPIGetCost: function (inFindAgent) {
    // 显示导航栏|浮动加载动画...
    wx.showLoading({ title: '加载中' });
    // 保存this对象...
    var that = this
    // 准备需要的参数信息...
    var thePostData = {
      'agent_id': g_appData.m_nMasterAgentID,
      'cur_page': that.data.m_cur_page,
      'find_agent': inFindAgent ? 1 : 0,
    }
    // 构造访问接口连接地址...
    var theUrl = g_appData.m_urlPrev + 'Mini/getAgentCost'
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
          that.setData({ m_show_more: false, m_no_more: '获取消费记录失败' })
          return
        }
        // dataType 没有设置json，需要自己转换...
        var arrData = JSON.parse(res.data);
        // 获取失败的处理 => 显示获取到的错误信息...
        if (arrData.err_code > 0) {
          that.setData({ m_show_more: false, m_no_more: arrData.err_msg })
          return
        }
        // 判断是否成功机构信息...
        if (inFindAgent && !(arrData.agent instanceof Object)) {
          that.setData({ m_show_more: false, m_no_more: '获取机构详情失败' })
          return;
        }
        // 保存获取到的有效机构详细信息...
        if (inFindAgent && (arrData.agent instanceof Object)) {
          that.setData({ m_curAgent: arrData.agent });
        }
        // 获取到的记录数据不为空时才进行记录合并处理 => concat 不会改动原数据
        if ((arrData.cost instanceof Array) && (arrData.cost.length > 0)) {
          that.data.m_arrCost = that.data.m_arrCost.concat(arrData.cost)
        }
        // 保存获取到的记录总数和总页数...
        that.data.m_total_num = arrData.total_num
        that.data.m_max_page = arrData.max_page
        that.data.m_begin_id = arrData.begin_id
        // 将数据显示到模版界面上去，并且显示加载更多页面...
        that.setData({
          m_arrCost: that.data.m_arrCost,
          m_begin_id: that.data.m_begin_id,
          m_total_num: that.data.m_total_num,
        })
        // 如果到达最大页数，关闭加载更多信息...
        if (that.data.m_cur_page >= that.data.m_max_page) {
          that.setData({ m_show_more: false, m_no_more: '' })
        }
      },
      fail: function (res) {
        // 隐藏导航栏加载动画...
        wx.hideLoading()
        that.setData({ m_show_more: false, m_no_more: '获取消费记录失败' })
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
    this.doAPIGetCost(false)
  },

  /**
   * 用户点击右上角分享
   */
  onShareAppMessage: function () {

  }
})