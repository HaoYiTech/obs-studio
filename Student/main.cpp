
#include "student-app.h"

int main(int argc, char *argv[])
{
	// ����Ӧ�ö���...
	CStudentApp program(argc, argv);
	// ��ʼ����¼����...
	program.doLoginInit();
	// ִ����ѭ������...
	return program.exec();
}
