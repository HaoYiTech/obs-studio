
#include "student-app.h"

int main(int argc, char *argv[])
{
	// 构造应用对象...
	CStudentApp program(argc, argv);
	// 初始化登录窗口...
	program.doLoginInit();
	// 执行主循环操作...
	return program.exec();
}
