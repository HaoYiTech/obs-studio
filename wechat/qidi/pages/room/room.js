
let chatInput = require('../../modules/chat-input/chat-input');
import { dealChatTime } from "../../utils/time";
import MsgManager from "./msg-manager";
import UI from "./ui";

// 获取全局的app对象...
const g_app = getApp()

Page({
  data: {
    m_RoomData: null,    // 当前房间信息
    m_nTabIndex: 0,      // 标签索引编号
    m_min_id: 0,         // 最小聊天编号
    m_max_id: 0,         // 最大聊天编号
    m_up_more: false,    // 向上正在加载
    m_inter_chat: -1,    // 定时器初始化
    isReadChat: false,   // 正在读取聊天
    isAndroid: true,
    chatItems: [],
    hotTabs: [
      {
        text: '聊天内容',
        icon: 'chat',
        iconSize: '36rpx',
        iconColor: '#666',
      },
      {
        text: '在线用户',
        icon: 'friends',
        iconSize: '36rpx',
        iconColor: '#666',
      }
    ]
  },

  // 点击标签栏切换事件...
  handleChange(event) {
    console.log(event.detail.value)
    this.data.m_nTabIndex = event.detail.value
  },

  // 创建简化聊天内容对象 => 用于网络传输...
  createChatItemContent({ type = 'text', content = '', duration } = {}) {
    if (!content.replace(/^\s*|\s*$/g, '')) return;
    return {
      content,
      type,
      conversationId: 0,//会话id，目前未用到
      userId: g_app.globalData.m_nUserID,
      duration
    };
  },

  // 创建标准聊天内容对象 => 用于显示...
  createNormalChatItem({ type = 'text', content = '', isMy = true, duration = 0, timestamp } = {}) {
    if (!content) return;
    const currentTimestamp = (typeof timestamp === 'string') ? Date.parse(timestamp) : Date.now();
    const time = dealChatTime(currentTimestamp, this._latestTImestamp);
    let obj = {
      chatId: 0,//消息id => chat_id
      userId: isMy ? g_app.globalData.m_nUserID : 0,
      isMy: isMy,//我发送的消息？
      title: '', //消息标题 => 微信昵称 - 真实姓名 - 身份类型
      showTime: time.ifShowTime,//是否显示该次发送时间
      time: time.timeStr,//发送时间 如 09:15,
      timestamp: currentTimestamp,//该条数据的时间戳，一般用于排序
      type: type,//内容的类型，目前有这几种类型： text/voice/image/custom | 文本/语音/图片/自定义
      content: content,// 显示的内容，根据不同的类型，在这里填充不同的信息。
      headUrl: g_app.globalData.m_userInfo.avatarUrl,//显示的头像，自己或好友的。
      sendStatus: 'success',//发送状态，目前有这几种状态：sending/success/failed | 发送中/发送成功/发送失败
      voiceDuration: duration,//语音时长 单位秒
      isPlaying: false,//语音是否正在播放
    };
    obj.saveKey = obj.timestamp + '_' + obj.chatId;//saveKey是存储文件时的key
    return obj;
  },

  // 创建自定义聊天内容对象...
  createCustomChatItem() {
    return {
      timestamp: Date.now(),
      type: 'custom',
      content: '会话已关闭'
    }
  },
  
  // 实际真正的发送数据接口...
  onSimulateSendMsg({ content, itemIndex, success, fail }) {
    // 保存this对象...
    var that = this
    // 构造发送数据...
    var thePostData = {
      room_id: that.data.m_RoomData.room_id,
      type_id: content.type,
      user_id: content.userId,
      content: content.content,
    }
    // 构造请求的API连接地址...
    var theUrl = g_app.globalData.m_urlPrev + 'Mini/saveChat'
    // 请求远程API过程...
    wx.request({
      url: theUrl,
      method: 'POST',
      data: thePostData,
      dataType: 'x-www-form-urlencoded',
      header: { 'content-type': 'application/x-www-form-urlencoded' },
      success: function (res) {
        // 调用接口失败，网络失败...
        if (res.statusCode != 200) {
          fail && fail(res);
          return;
        }
        // dataType 没有设置json，需要自己转换...
        var arrData = JSON.parse(res.data);
        // 发生错误或没有获取聊天记录编号，或记录编号无效，跳转失败...
        if (arrData.err_code > 0 || undefined === arrData.chat_id ) {
          fail && fail(res);
          return;
        }
        // 读取引用对象，更新聊天时间|更新聊天编号...
        let objItem = that.data.chatItems[itemIndex];
        objItem.timestamp = Date.now();
        objItem.chatId = arrData.chat_id;
        // 保存为当前最大的聊天编号...
        that.data.m_max_id = Number(arrData.chat_id);
        // 更新系统里最新的聊天时间，调用回调接口...
        that._latestTImestamp = objItem.timestamp;
        success && success(objItem);
      },
      fail: function (res) {
        fail && fail(res);
      }
    });
  },

  // 发送消息接口函数 => 最终会绕到这里...
  sendMsg({ content, itemIndex, success }) {
    this.onSimulateSendMsg({
      content, itemIndex,
      success: (msg) => {
        this.UI.updateViewWhenSendSuccess(msg, itemIndex);
        success && success(msg);
      },
      fail: () => {
        this.UI.updateViewWhenSendFailed(itemIndex);
      }
    })
  },

  // 重发消息接口...
  resendMsgEvent(e) {
    const itemIndex = parseInt(e.currentTarget.dataset.resendIndex);
    const item = this.data.chatItems[itemIndex];
    this.UI.updateDataWhenStartSending(item, false, false);
    this.msgManager.resend({ ...item, itemIndex });
  },

  // 重置输入框状态...
  resetInputStatus() {
    chatInput.closeExtraView();
  },

  // 滚动视图滚动到顶部 => 向上翻页操作...
  doScrollToUpper(e) {
    // 进行具体的向上翻页操作...
    this.doAPIGetChat(true);
  },

  // 读取并加载聊天数据内容...
  doAPIGetChat: function(toUp) {
    // 如果正在执行读取命令，直接返回...
    if (this.data.isReadChat) return;
    // 显示加载更多动画条 => 向上和向下都需要设置状态...
    this.setData({ m_up_more: toUp, isReadChat: true });
    // 保存this对象...
    var that = this
    // 准备需要的参数信息...
    var thePostData = {
      'to_up': toUp ? 1 : 0,
      'min_id': that.data.m_min_id,
      'max_id': that.data.m_max_id,
      'room_id': that.data.m_RoomData.room_id,
    }
     // 构造请求的API连接地址...
    var theUrl = g_app.globalData.m_urlPrev + 'Mini/getChat'
    // 请求远程API过程...
    wx.request({
      url: theUrl,
      method: 'POST',
      data: thePostData,
      dataType: 'x-www-form-urlencoded',
      header: { 'content-type': 'application/x-www-form-urlencoded' },
      success: function (res) {
        // 隐藏加载动画...
        wx.hideLoading()
        // 只要调用接口成功，就关闭加载动画...
        that.setData({ m_up_more: false, isReadChat: false });
        // 调用接口失败...
        if (res.statusCode != 200) {
          console.log(res.statusCode);
          return;
        }
        // dataType 没有设置json，需要自己转换...
        var arrData = JSON.parse(res.data);
        // 获取失败的处理 => 显示获取到的错误信息...
        if (arrData.err_code > 0) {
          console.log(arrData);
          return;
        }
        // 获取到的记录数据为空时，直接关闭动画，返回...
        if (!(arrData.items instanceof Array) || (arrData.items.length <= 0)) {
          console.log('== no chat item ==');
          return;
        }
        // 获取到的数据不为空，更新界面 => 数组要倒序读取...
        for (let i = arrData.items.length - 1; i >= 0; --i) {
          let item = arrData.items[i];
          // 根据数据库创建用于显示的聊天记录...
          let objItem = that.createNormalChatItem({
            type: item.type_id, content: item.content,
            isMy: item.user_id === g_app.globalData.m_nUserID,
            duration: 0, timestamp: item.created,
          });
          // 用数据库内容更新到显示记录...
          objItem.chatId = item.chat_id;
          objItem.userId = item.user_id;
          objItem.headUrl = item.wx_headurl;
          objItem.title = item.wx_nickname;
          // 将新对象放入列表，更新最新时间戳...
          that.data.chatItems.push(objItem);
          that._latestTImestamp = objItem.timestamp;
          // 注意：不要使用that.msgManager.showMsg
        }
        // 将数据先用时间排序进行显示...
        that.setData({ 
          scrollTopVal: toUp ? 10 : that.data.chatItems.length * 999,
          chatItems: that.data.chatItems.sort(UI._sortMsgListByTimestamp),
        })
        // 再用编号排序，更新最小编号和最大编号 => 如果编号混乱会丢记录 => 编号大的时间戳小(混乱对记录)...
        that.data.chatItems.sort(UI._sortMsgListByChatID);
        if (that.data.chatItems.length > 0) {
          that.data.m_min_id = Number(that.data.chatItems[0].chatId);
          that.data.m_max_id = Number(that.data.chatItems[that.data.chatItems.length - 1].chatId);
          console.log(that.data.m_min_id, that.data.m_max_id, that.data.chatItems.length);
        }
      },
      fail: function (res) {
        // 隐藏加载动画...
        wx.hideLoading()
        // 隐藏正在加载动画...
        that.setData({ m_up_more: false, isReadChat: false });
      }
    })
  },

  // 生命周期函数--监听页面加载
  onLoad: function (options) {
    // 显示浮动加载动画...
    wx.showLoading({ title: '加载中' });
    // 最新消息的时间戳...
    this._latestTImestamp = 0;
    // 用全局房间信息设置页面标题栏，保存标签索引号...
    this.data.m_RoomData = g_app.globalData.m_curRoomItem
    wx.setNavigationBarTitle({ title: this.data.m_RoomData.room_name})
    this.data.m_nTabIndex = this.selectComponent('#myTab').properties.currentIndex
    // 创建相关聊天对象...
    this.UI = new UI(this);
    this.msgManager = new MsgManager(this);
    // 设置聊天滚动显示窗口的高度，加载数据放到最后...
    var sysInfo = g_app.globalData.m_sysInfo;
    this.setData({
      pageHeight: sysInfo.windowHeight,
      isAndroid: sysInfo.system.indexOf("Android") !== -1,
    });
    // 初始化聊天输入对象...
    chatInput.init(this, {
      systemInfo: sysInfo,
      minVoiceTime: 1,
      maxVoiceTime: 60,
      startTimeDown: 56,
      format: 'mp3',//aac/mp3
      sendButtonBgColor: 'mediumseagreen',
      sendButtonTextColor: 'white',
      extraArr: [{
        picName: 'choose_picture',
        description: '照片'
      }, {
        picName: 'take_photos',
        description: '拍摄'
      }],
    });
    // 初始化文字|语音|图片...
    this.textButton();
    this.voiceButton();
    this.extraButton();
    // 加载并向下更新聊天数据...
    this.doAPIGetChat(false);
    // 每隔5秒定时更新聊天内容...
    var that = this;
    that.data.m_inter_chat = setInterval(function(){
      that.doAPIGetChat(false);
    }, 5000);
  },
  // 生命周期函数--监听页面卸载
  onUnload: function () {
    // 定时器有效时，才需要删除之...
    if (this.data.m_inter_chat >= 0) {
      clearInterval(this.data.m_inter_chat);
    }
  },
  // 初始化文字...
  textButton() {
    chatInput.setTextMessageListener((e) => {
      let content = e.detail.value;
      this.msgManager.sendMsg({ type: 'text', content });
    });
  },
  // 初始化语音...
  voiceButton() {
    chatInput.recordVoiceListener((res, duration) => {
      let tempFilePath = res.tempFilePath;
      this.msgManager.sendMsg({ type: 'voice', content: tempFilePath, duration });
    });
    chatInput.setVoiceRecordStatusListener((status) => {
      this.msgManager.stopAllVoice();
    })
  },
  // 初始化图片...
  extraButton() {
    let that = this;
    chatInput.clickExtraListener((e) => {
      let chooseIndex = parseInt(e.currentTarget.dataset.index);
      if (chooseIndex > 1)
        return;
      wx.chooseImage({
        count: 1, // 默认9
        sizeType: ['compressed'],
        sourceType: chooseIndex === 0 ? ['album'] : ['camera'],
        success: (res) => {
          this.msgManager.sendMsg({ type: 'image', content: res.tempFilePaths[0] })
        }
      });
    });
  },

  // 生命周期函数--监听页面初次渲染完成
  onReady: function () {
  }
})