
#include "update-app.h"

void doCloseMainWindow()
{
	// 这里需要告诉主窗口不要弹出询问框，直接退出主进程就可以了...
	CLoginMini * mini = reinterpret_cast<CLoginMini*>(App()->GetLoginMini());
	if (mini != NULL) {
		QMetaObject::invokeMethod(mini, "close");
	}
}
