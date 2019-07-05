
import Notify from '../../vant-weapp/notify/notify';
import Dialog from '../../vant-weapp/dialog/dialog';

// 获取全局的app对象...
const g_appData = getApp().globalData;

Page({
  /**
   * 页面的初始数据
   */
  data: {
    m_bEdit: false,
    m_curUser: null,
    m_curSubjectName: '',
    m_curGradeName: '',
    m_curSubjectID: 0,
    m_curGradeID: 0,
    m_arrSubject: [],
    m_arrGrade: [],
  },

  /**
   * 生命周期函数--监听页面加载
   */
  onLoad: function (options) {
    // 显示导航栏|浮动加载动画...
    wx.showLoading({ title: '加载中' });
    // 保存this对象...
    let that = this
    let isEdit = parseInt(options.edit);
    // 保存用户对象...
    let pages = getCurrentPages();
    let prevParentPage = pages[pages.length - 3];
    this.setData({ m_curUser: prevParentPage.data.m_curUser });
    // 构造访问接口连接地址...
    let theUrl = g_appData.m_urlPrev + 'Mini/getAllGrade';
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
          Notify('获取课程信息失败！');
          return
        }
        // dataType 没有设置json，需要自己转换...
        let arrJson = JSON.parse(res.data);
        that.doShowConsume(isEdit, arrJson.subject, arrJson.grade);
      },
      fail: function (res) {
        // 隐藏导航栏加载动画...
        wx.hideLoading();
        Notify('获取课程信息失败！');
      }
    });
  },
  // 显示具体的购课操作界面...
  doShowConsume: function (isEdit, arrSubject, arrGrade) {
    let strTitle = '购课充值 - ' + (isEdit ? '修改' : '添加');
    let theConsume = isEdit ? g_appData.m_curSelectItem : null;
    let theTotalPrice = isEdit ? theConsume.total_price : '';
    let thePointNum = isEdit ? theConsume.point_num : '';
    let theCurSubjectID = (isEdit ? theConsume.subject_id : 0);
    let theCurSubjectName = isEdit ? theConsume.subject_name : '请选择';
    let theCurGradeID = (isEdit ? theConsume.grade_id : 0);
    let theCurGradeName = isEdit ? theConsume.grade_name : '请选择';
    let theCurSubjectIndex = 0; let theCurGradeIndex = 0;
    wx.setNavigationBarTitle({ title: strTitle });
    // 如果是编辑状态，需要找到课程所在的索引编号...
    if (isEdit && arrSubject.length > 0) {
      for (let index = 0; index < arrSubject.length; ++index) {
        if (arrSubject[index].subject_id == theCurSubjectID) {
          theCurSubjectIndex = index;
          break;
        }
      }
    }
    // 如果是编辑状态，需要找到年级所在的索引编号...
    if (isEdit && arrGrade.length > 0) {
      for (let index = 0; index < arrGrade.length; ++index) {
        if (arrGrade[index].grade_id == theCurGradeID) {
          theCurGradeIndex = index;
          break;
        }
      }
    }
    // 应用到界面...
    this.setData({
      m_bEdit: isEdit,
      m_total_price: theTotalPrice,
      m_point_num: thePointNum,
      m_arrSubject: arrSubject,
      m_curSubjectID: theCurSubjectID,
      m_curSubjectName: theCurSubjectName,
      m_curSubjectIndex: theCurSubjectIndex,
      m_arrGrade: arrGrade,
      m_curGradeID: theCurGradeID,
      m_curGradeName: theCurGradeName,
      m_curGradeIndex: theCurGradeIndex,
    });
  },
  // 点击取消按钮...
  doBtnCancel: function (event) {
    wx.navigateBack();
  },
  // 点击保存按钮...
  doBtnSave: function (event) {
    let nTotal = parseInt(this.data.m_total_price);
    if ((this.data.m_total_price.length <= 0) || isNaN(nTotal) || (nTotal <= 0)) {
      Notify('【订单总价】不能为空，请重新输入！');
      return;
    }
    let nPoint = parseInt(this.data.m_point_num);
    if ((this.data.m_point_num.length <= 0) || isNaN(nPoint) || (nPoint <= 0)) {
      Notify('【购买课时】不能为空，请重新输入！');
      return;
    }
    if (parseInt(this.data.m_curSubjectID) <= 0) {
      Notify('【课程名称】不能为空，请重新选择！');
      return;
    }
    if (parseInt(this.data.m_curGradeID) <= 0) {
      Notify('【年级名称】不能为空，请重新选择！');
      return;
    }
    // 根据不同的标志进行不同的接口调用...
    this.data.m_bEdit ? this.doConsumeEdit() : this.doConsumeAdd();
  },
  // 进行购课的添加操作...
  doConsumeAdd: function () {
    wx.showLoading({ title: '加载中' });
    let that = this;
    // 准备需要的参数信息...
    var thePostData = {
      //'mode_id': 0,      // 0(购买课程) 1(购买会员) => 由服务器设置
      //'status_id': 1,    // 0(待付款)   1(已付款)   => 由服务器设置
      //'consume_type': 1, // 0(微信支付) 1(现金支付) => 由服务器设置
      'user_id': that.data.m_curUser.user_id,
      'shop_id': that.data.m_curUser.shop_id,
      'grade_id': that.data.m_curGradeID,
      'subject_id': that.data.m_curSubjectID,
      'point_num': parseInt(that.data.m_point_num),
      'total_price': parseInt(that.data.m_total_price),
      'unit_price': parseInt(that.data.m_total_price),
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
          Notify('新建购课记录失败！');
          return;
        }
        // dataType 没有设置json，需要自己转换...
        var arrData = JSON.parse(res.data);
        if (arrData.err_code > 0) {
          Notify(arrData.err_msg);
          return;
        }
        // 修改课时总数到当前用户当中...
        let theCurUser = that.data.m_curUser;
        theCurUser.point_num = arrData.point_num;
        // 再更新一些数据到新建记录当中...
        arrData.consume.subject_name = that.data.m_curSubjectName;
        arrData.consume.grade_name = that.data.m_curGradeName;
        // 将新得到的购课记录存入父页面当中...
        let pages = getCurrentPages();
        let prevListPage = pages[pages.length - 2];
        let theNewConsume = new Array();
        theNewConsume.push(arrData.consume);
        // 向前插入并更新购课记录到界面当中...
        let theArrPay = prevListPage.data.m_arrPay;
        theArrPay = theNewConsume.concat(theArrPay);
        prevListPage.setData({
          m_arrPay: theArrPay,
          m_total_num: theArrPay.length,
          m_point_num: arrData.point_num,
          m_curUser: theCurUser,
        });
        // 进行更上一层的父窗口的信息修改...
        let prevUserPage = pages[pages.length - 3];
        let prevParentPage = pages[pages.length - 4];
        let theIndex = parseInt(theCurUser.indexID);
        let theArrUser = prevParentPage.data.m_arrUser;
        let thePrevUser = theArrUser[theIndex];
        thePrevUser.indexID = theCurUser.indexID;
        thePrevUser.point_num = theCurUser.point_num;
        prevParentPage.setData({ m_arrUser: theArrUser });
        g_appData.m_curSelectItem = thePrevUser;
        // 进行页面的更新跳转...
        wx.navigateBack();
      },
      fail: function (res) {
        wx.hideLoading()
        Notify('新建购课记录失败！');
      }
    })
  },
  // 进行购课的修改操作...
  doConsumeEdit: function () {
    wx.showLoading({ title: '加载中' });
    let that = this;
    // 准备需要的参数信息...
  },
  // 点击删除按钮...
  /*doBtnDel: function (event) {
    Dialog.confirm({
      confirmButtonText: "删除",
      title: "确实要删除当前选中的购课记录吗？"
    }).then(() => {
      this.doConsumeDel();
    }).catch(() => {
      console.log("Dialog - Cancel");
    });
  },*/
  // 订单总价发生变化...
  onPriceChange: function (event) {
    this.data.m_total_price = event.detail;
  },
  // 购买课时发生变化...
  onPointChange: function (event) {
    this.data.m_point_num = event.detail;
  },
  // 课程发生选择变化...
  onSubjectChange: function (event) {
    const { value } = event.detail;
    let theNewSubjectID = parseInt(value);
    if (theNewSubjectID < 0 || theNewSubjectID >= this.data.m_arrSubject.length) {
      Notify('【课程名称】内容越界！');
      return;
    }
    // 获取到当前变化后的课程信息，并写入数据然后显示出来...
    let theCurSubject = this.data.m_arrSubject[theNewSubjectID];
    this.setData({ m_curSubjectID: theCurSubject.subject_id, m_curSubjectName: theCurSubject.subject_name });
  },
  // 年级发生选择变化...
  onGradeChange: function (event) {
    const { value } = event.detail;
    let theNewGradeID = parseInt(value);
    if (theNewGradeID < 0 || theNewGradeID >= this.data.m_arrGrade.length) {
      Notify('【年级名称】内容越界！');
      return;
    }
    // 获取到当前变化后的年级信息，并写入数据然后显示出来...
    let theCurGrade = this.data.m_arrGrade[theNewGradeID];
    this.setData({ m_curGradeID: theCurGrade.grade_id, m_curGradeName: theCurGrade.grade_name });
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