
import VoiceManager from "./voice-manager";
import TextManager from "./text-manager";
import ImageManager from "./image-manager";
import CustomManager from "./custom-manager";

export default class MsgTypeManager {
  constructor(page) {
    this.voiceManager = new VoiceManager(page);
    this.textManager = new TextManager(page);
    this.imageManager = new ImageManager(page);
    this.customManager = new CustomManager(page);
  }

  getMsgManager({type}) {
    let tempManager = null;
    switch (type) {
      case 'voice':
        tempManager = this.voiceManager;
        break;
      case 'image':
        tempManager = this.imageManager;
        break;
      case 'text':
        tempManager = this.textManager;
        break;
      case 'custom':
        tempManager = this.customManager;
        break;
    }
    return tempManager;
  }

  clear() {
    this.voiceManager = null;
    this.textManager = null;
    this.imageManager = null;
    this.customManager = null;
  }
}