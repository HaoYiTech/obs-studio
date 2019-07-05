
import Notify from '../../vant-weapp/notify/notify';
import Dialog from '../../vant-weapp/dialog/dialog';

// 获取全局的app对象...
const g_appData = getApp().globalData;

Page({
  /**
   * 页面的初始数据
   */
  data: {
    m_curUser: null,
    m_arrPay: [],
    m_cur_page: 1,
    m_max_page: 1,
    m_total_num: 0,
    m_point_num: 0,
    m_show_more: true,
    m_no_more: '正在加载...',
  },

  // 响应修改课时操作...
  doModPoint: function() {
    let theCurUser = this.data.m_curUser;
    let nCurPoint = parseInt(this.data.m_point_num);
    // 强制将剩余课时数设置为 0 => 当输入为空时 => 可以设置为小于0...
    if ((this.data.m_point_num.length <= 0) || isNaN(nCurPoint)) {
      nCurPoint = 0;
    }
    // 没有发生变化，直接返回...
    if (nCurPoint == theCurUser.point_num) {
      Notify('【剩余课时数】修改完成！');
      return;
    }
    // 显示导航栏|浮动加载动画...
    wx.showLoading({ title: '加载中' });
    let that = this;
    // 准备需要的参数信息...
    var thePostData = {
      'user_id': theCurUser.user_id,
      'point_num': nCurPoint,
    }
    // 构造访问接口连接地址...
    let theUrl = g_appData.m_urlPrev + 'Mini/saveUser';
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
          Notify('【剩余课时数】修改失败！');
          return;
        }
        // 更新数据到本地页面当中...
        theCurUser.point_num = nCurPoint;
        that.setData({ m_curUser: theCurUser });
        // 进行父页面的数据更新...
        let pages = getCurrentPages();
        let prevUserPage = pages[pages.length - 2];
        let prevParentPage = pages[pages.length - 3];
        let theIndex = parseInt(theCurUser.indexID);
        let theArrUser = prevParentPage.data.m_arrUser;
        let thePrevUser = theArrUser[theIndex];
        thePrevUser.indexID = theCurUser.indexID;
        thePrevUser.point_num = theCurUser.point_num;
        prevParentPage.setData({ m_arrUser: theArrUser });
        g_appData.m_curSelectItem = thePrevUser;
        Notify('【剩余课时数】修改成功！');
      },
      fail: function (res) {
        wx.hideLoading()
        Notify('【剩余课时数】修改失败！');
      }
    })
  },
  
  // 剩余课时数发生变化...
  onPointChange: function (event) {
    this.data.m_point_num = event.detail;
  },

  // 响应点击新增充值...
  doAddPay: function () {
    //g_appData.m_curSelectItem = null;
    wx.navigateTo({ url: '../payItem/payItem?edit=0' });
  },

  // 点击滑动删除事件...
  onSwipeClick: function(event) {
    // 如果不是右侧，直接返回...
    const { id: nIndexID } = event.currentTarget;
    if ("right" != event.detail || nIndexID < 0)
      return;
    // 弹框确认输出...
    Dialog.confirm({
      confirmButtonText: "删除",
      message: "确实要删除当前选中的购课记录吗？"
    }).then(() => {
      this.doDelPay(nIndexID);
    }).catch(() => {
      console.log("Dialog - Cancel");
    });
  },

  // 删除指定索引编号的购课记录...
  doDelPay: function(inIndexID) {
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
          Notify('删除购课记录失败！');
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
        Notify('删除购课记录失败！');
      }
    })
  },
  /**
   * 生命周期函数--监听页面加载
   */
  onLoad: function (options) {
    // 保存用户对象...
    let pages = getCurrentPages();
    let prevParentPage = pages[pages.length - 2];
    this.setData({ m_curUser: prevParentPage.data.m_curUser });
    this.data.m_point_num = this.data.m_curUser.point_num;
    // 获取当前登录用户的购课记录...
    this.doAPIGetPay();
  },

  // 获取购课列表...
  doAPIGetPay: function () {
    // 显示导航栏|浮动加载动画...
    wx.showLoading({ title: '加载中' });
    // 保存this对象...
    var that = this
    // 准备需要的参数信息...
    var thePostData = {
      'user_id': g_appData.m_nUserID,
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
          that.setData({ m_show_more: false, m_no_more: '获取购课记录失败' })
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
        that.setData({ m_show_more: false, m_no_more: '获取购课记录失败' })
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