
#include "update-app.h"
#include "window-basic-main.hpp"

void doCloseMainWindow()
{
	// ������Ҫ���������ڲ�Ҫ����ѯ�ʿ�ֱ���˳������̾Ϳ�����...
	OBSBasic * main = reinterpret_cast<OBSBasic*>(App()->GetMainWindow());
	CLoginMini * mini = reinterpret_cast<CLoginMini*>(App()->GetLoginMini());
	if (main != NULL) {
		// ���������ڳ�Ĭ�˳�...
		main->SetSlientClose(true);
		// �������ڷ����˳��ź�...
		QMetaObject::invokeMethod(main, "close");
	} else if (mini != NULL) {
		// �������ڷ����˳��ź�...
		QMetaObject::invokeMethod(mini, "close");
	}
}
