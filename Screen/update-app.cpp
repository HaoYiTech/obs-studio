
#include "update-app.h"

void doCloseMainWindow()
{
	// ������Ҫ���������ڲ�Ҫ����ѯ�ʿ�ֱ���˳������̾Ϳ�����...
	CLoginMini * mini = reinterpret_cast<CLoginMini*>(App()->GetLoginMini());
	if (mini != NULL) {
		QMetaObject::invokeMethod(mini, "close");
	}
}
