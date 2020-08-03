#ifndef __MAIN_H__
#define __MAIN_H__

#include <winsock2.h>
#include <windows.h>

/*  Чтобы использовать эту экспортированную функцию DLL, включите этот заголовок
 *  в вашем проекте.
 */

#ifdef BUILD_DLL
    #define DLL_EXPORT __declspec(dllexport)
#else
    #define DLL_EXPORT __declspec(dllimport)
#endif


#ifdef __cplusplus
extern "C"
{
#endif

int DLL_EXPORT ws_open(const char *point);
int DLL_EXPORT ws_send(const int handle, const char *buffer);
int DLL_EXPORT ws_receive(const int handle, char *buffer, const int buffer_size);
void DLL_EXPORT ws_close(const int handle);

#ifdef __cplusplus
}
#endif

#endif // __MAIN_H__
