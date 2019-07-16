
import Notify from '../../vant-weapp/notify/notify';

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

  /**
   * 生命周期函数--监听页面加载
   */
  onLoad: function (options) {
    this.setData({ m_curAgent: g_appData.m_curSelectItem });
    this.doAPIGetCost();
  },
  // 点击'学生费用'的响应...
  doBtnStudent: function(event) {
    const { id: nIndexID } = event.currentTarget;
    let theCurFlow = this.data.m_arrCost[nIndexID];
    wx.navigateTo({ url: '../agentStudent/agentStudent?flow_id=' + theCurFlow.flow_id });
  },
  // 点击'立即结算'的响应...
  doBtnCharged: function(event) {
    const { id: nIndexID } = event.currentTarget;
    let theCurFlow = this.data.m_arrCost[nIndexID];
    let theTeacher = parseFloat(theCurFlow.cost);
    let theStudent = parseFloat(theCurFlow.student);
    theTeacher = isNaN(theTeacher) ? 0 : theTeacher;
    theStudent = isNaN(theStudent) ? 0 : theStudent;
    let theTotalCost = parseFloat((theTeacher + theStudent).toFixed(2));
    // 显示导航栏|浮动加载动画...
    wx.showLoading({ title: '加载中' });
    // 保存this对象...
    var that = this
    // 准备需要的参数信息...
    var thePostData = {
      'flow_id': theCurFlow.flow_id,
      'agent_id': theCurFlow.agent_id,
      'cost_val': theTotalCost,
    }
    // 构造访问接口连接地址...
    var theUrl = g_appData.m_urlPrev + 'Mini/saveFlowCharged'
    // 请求远程API过程...
    wx.request({
      url: theUrl,
      method: 'POST',
      data: thePostData,
      dataType: 'x-www-formurlencoded',
      header: { 'content-type': 'application/x-www-form-urlencoded' },
      success: function (res) {
        // 隐藏导航栏加载动画...
        wx.hideLoading();
        // 调用接口失败...
        if (res.statusCode != 200) {
          Notify('结算账单失败，请确认后重新结算！');
          return;
        }
        // dataType 没有设置json，需要自己转换...
        var arrData = JSON.parse(res.data);
        // 获取失败的处理 => 显示获取到的错误信息...
        if (arrData.err_code > 0) {
          Notify(arrData.err_msg);
          return;
        }
        // 结算成功，修正当前记录并更新...
        let theCurAgent = that.data.m_curAgent;
        let theCurCost = parseFloat(theCurAgent.cost);
        let theCurMoney = parseFloat(theCurAgent.money);
        theCurAgent.cost = theCurCost + theTotalCost;
        theCurAgent.money = theCurMoney - theTotalCost;
        // 设置流量记录结算状态...
        theCurFlow.charged = 1;
        that.setData({
          m_arrCost: that.data.m_arrCost,
          m_curAgent: theCurAgent
        });
        // 更新上级界面的金额数据内容...
        let pages = getCurrentPages();
        let prevAgentPage = pages[pages.length - 2];
        let theArrAgent = prevAgentPage.data.m_arrAgent;
        let thePrevAgent = theArrAgent[theCurAgent.indexID];
        thePrevAgent.indexID = theCurAgent.indexID;
        thePrevAgent.money = theCurAgent.money;
        thePrevAgent.cost = theCurAgent.cost;
        prevAgentPage.setData({ m_arrAgent: theArrAgent });
        g_appData.m_curSelectItem = thePrevAgent;
      },
      fail: function (res) {
        // 隐藏导航栏加载动画...
        wx.hideLoading();
        Notify('结算账单失败，请确认后重新结算！');
      }
    })
  },
  // 获取消费列表...
  doAPIGetCost: function () {
    // 显示导航栏|浮动加载动画...
    wx.showLoading({ title: '加载中' });
    // 保存this对象...
    var that = this
    // 准备需要的参数信息...
    var thePostData = {
      'agent_id': that.data.m_curAgent.agent_id,
      'cur_page': that.data.m_cur_page,
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
    this.doAPIGetCost()
  },

  /**
   * 用户点击右上角分享
   */
  onShareAppMessage: function () {

  }
})