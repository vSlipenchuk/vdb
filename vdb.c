/*

 ToDo - ��������� ��� ���� �������������, � ��������� -
 ���������� ��� ������???

*/



#define LoadLibrary(A) dlopen(A,RTLD_NOW)
#define GetProcAddress(A,N) dlsym(A,N)

#include "vdb-2.2.c"
#include "vdt-1.0.c" /* Time Functions - separated */

