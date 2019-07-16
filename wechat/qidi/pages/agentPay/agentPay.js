
import Notify from '../../vant-weapp/notify/notify';
import Dialog from '../../vant-weapp/dialog/dialog';

// 获取全局的app对象...
const g_appData = getApp().globalData;

Page({
  /**
   * 页面的初始数据
   */
  data: {
    m_curAgent: null,
    m_show_add: false,
    m_cur_money: 0,
    m_arrPay: [],
    m_cur_page: 1,
    m_max_page: 1,
    m_total_num: 0,
    m_show_more: true,
    m_no_more: '正在加载...',
  },

  // 响应点击新增充值...
  doAddPay: function () {
    this.setData({
      m_cur_money: 0,
      m_show_add: true,
    });
  },
  // 响应取消添加按钮...
  onCancelAdd(event) {
    // 先获取传递的对话框对象，停止等待...
    const { dialog: theDialog } = event.detail;
    theDialog.stopLoading();
    // 再关闭显示对话框对象...
    this.setData({ m_show_add: false });
  },
  // 响应点击确认按钮...  
  onConfirmAdd(event) {
    // 先获取传递的对话框对象，停止等待...
    const { dialog: theDialog } = event.detail;
    theDialog.stopLoading();
    // 判断输入金额是否有效...
    if (this.data.m_cur_money <= 0) {
      Notify('【充值金额】必须大于0，请重新输入！');
      return;
    }
    // 调用远程接口新增充值记录...
    wx.showLoading({ title: '加载中' });
    let that = this;
    // 准备需要的参数信息...
    var thePostData = {
      //'mode_id': 0,      // 0(购买课程) 1(购买会员) => 由服务器设置
      //'status_id': 1,    // 0(待付款)   1(已付款)   => 由服务器设置
      //'consume_type': 1, // 0(微信支付) 1(现金支付) => 由服务器设置
      'agent_id': that.data.m_curAgent.agent_id,
      'total_price': parseInt(that.data.m_cur_money),
      'unit_price': parseInt(that.data.m_cur_money),
      'unit_num': 1,
    }
    // 构造访问接口连接地址...
    let theUrl = g_appData.m_urlPrev + 'Mini/addCashPay';
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
          Notify('新建充值记录失败！');
          return;
        }
        // dataType 没有设置json，需要自己转换...
        var arrData = JSON.parse(res.data);
        if (arrData.err_code > 0) {
          Notify(arrData.err_msg);
          return;
        }
        // 修改课时总数到当前机构当中...
        let theCurAgent = that.data.m_curAgent;
        theCurAgent.money = arrData.money;
        // 将新得到的充值记录存入当前页面当中...
        let theNewConsume = new Array();
        theNewConsume.push(arrData.consume);
        // 向前插入并更新购课记录到界面当中...
        let theArrPay = that.data.m_arrPay;
        theArrPay = theNewConsume.concat(theArrPay);
        that.setData({
          m_arrPay: theArrPay,
          m_total_num: theArrPay.length,
          m_curAgent: theCurAgent,
          m_show_add: false,
        });
        // 进行上一层的父窗口的信息修改...
        let pages = getCurrentPages();
        let prevAgentPage = pages[pages.length - 2];
        let theIndex = parseInt(theCurAgent.indexID);
        let theArrAgent = prevAgentPage.data.m_arrAgent;
        let thePrevAgent = theArrAgent[theIndex];
        thePrevAgent.indexID = theCurAgent.indexID;
        thePrevAgent.money = theCurAgent.money;
        prevAgentPage.setData({ m_arrAgent: theArrAgent });
        g_appData.m_curSelectItem = thePrevAgent;
      },
      fail: function (res) {
        wx.hideLoading()
        Notify('新建充值记录失败！');
      }
    })
  },

  // 点击滑动删除事件...
  onSwipeClick: function (event) {
    // 如果不是右侧，直接返回...
    const { id: nIndexID } = event.currentTarget;
    if ("right" != event.detail || nIndexID < 0)
      return;
    // 弹框确认输出...
    Dialog.confirm({
      confirmButtonText: "删除",
      message: "确实要删除当前选中的充值记录吗？"
    }).then(() => {
      this.doDelPay(nIndexID);
    }).catch(() => {
      console.log("Dialog - Cancel");
    });
  },

  // 删除指定索引编号的购课记录...
  doDelPay: function (inIndexID) {
    // 显示导航栏|浮动加载动画...
    wx.showLoading({ title: '加载中' });
    // 找到即将要删除的记录对象...
    let theCurPay = this.data.m_arrPay[inIndexID];
    let that = this;
    // 准备需要的参数信息...
    var thePostData = {
      'consume_id': theCurPay.consume_id,
    }
    // 构造访问接口连接地址...
    let theUrl = g_appData.m_urlPrev + 'Mini/delPay';
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
          Notify('删除充值记录失败！');
          return;
        }
        // 删除并更新记录到界面当中...
        that.data.m_arrPay.splice(inIndexID, 1);
        let theArrPay = that.data.m_arrPay;
        that.setData({
          m_arrPay: theArrPay,
          m_total_num: theArrPay.length,
          m_show_more: false,
          m_no_more: '',
        });
      },
      fail: function (res) {
        wx.hideLoading()
        Notify('删除充值记录失败！');
      }
    })
  },
  // 充值金额发生变化...
  onPriceChange: function (event) {
    this.data.m_cur_money = event.detail;
  },
  /**
   * 生命周期函数--监听页面加载
   */
  onLoad: function (options) {
    this.setData({ m_curAgent: g_appData.m_curSelectItem });
    this.doAPIGetPay();
  },
  // 获取充值列表...
  doAPIGetPay: function () {
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
    var theUrl = g_appData.m_urlPrev + 'Mini/getPay'
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
          that.setData({ m_show_more: false, m_no_more: '获取充值记录失败' })
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
        if ((arrData.pay instanceof Array) && (arrData.pay.length > 0)) {
          that.data.m_arrPay = that.data.m_arrPay.concat(arrData.pay)
        }
        // 保存获取到的记录总数和总页数...
        that.data.m_total_num = arrData.total_num
        that.data.m_max_page = arrData.max_page
        // 将数据显示到模版界面上去，并且显示加载更多页面...
        that.setData({ m_arrPay: that.data.m_arrPay, m_total_num: that.data.m_total_num })
        // 如果到达最大页数，关闭加载更多信息...
        if (that.data.m_cur_page >= that.data.m_max_page) {
          that.setData({ m_show_more: false, m_no_more: '' })
        }
      },
      fail: function (res) {
        // 隐藏导航栏加载动画...
        wx.hideLoading()
        that.setData({ m_show_more: false, m_no_more: '获取充值记录失败' })
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
    this.doAPIGetPay()
  },

  /**
   * 用户点击右上角分享
   */
  onShareAppMessage: function () {

  }
})