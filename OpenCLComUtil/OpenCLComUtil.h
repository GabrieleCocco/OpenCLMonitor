// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the OPENCLCOMUTIL_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// OPENCLCOMUTIL_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef OPENCLCOMUTIL_EXPORTS
#define OPENCLCOMUTIL_API __declspec(dllexport)
#else
#define OPENCLCOMUTIL_API __declspec(dllimport)
#endif

#define BUFFER_SIZE (1 << 10)
#include <tchar.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern "C" OPENCLCOMUTIL_API typedef enum OpenCLMonitoringMessageID {
	CREATE_QUEUE_MESSAGE,
	ENABLE_COUNTERS_MESSAGE,
	GPU_PERF_INIT_MESSAGE,
	RELEASE_QUEUE_MESSAGE,
	GET_COUNTERS_MESSAGE,
	GPU_PERF_RELEASE_MESSAGE,
	OK_MESSAGE
} OpenCLMonitoringMessageID;

extern "C" OPENCLCOMUTIL_API typedef struct OpenCLMonitoringMessage {
	OpenCLMonitoringMessageID id;
	int action_result;
	int api_result;
	char** body;
	int body_count;
} OpenCLMonitoringMessage;

extern "C" OPENCLCOMUTIL_API int open_connection(char* pipe_name, bool server, HANDLE* pipe);
extern "C" OPENCLCOMUTIL_API int close_connection(HANDLE pipe);
extern "C" OPENCLCOMUTIL_API int free_message(OpenCLMonitoringMessage* message);
extern "C" OPENCLCOMUTIL_API int send_message(HANDLE pipe, OpenCLMonitoringMessage* message);
extern "C" OPENCLCOMUTIL_API int send_empty_message(HANDLE pipe, OpenCLMonitoringMessageID id, int action_result, int api_result);
extern "C" OPENCLCOMUTIL_API int receive_message(HANDLE pipe, OpenCLMonitoringMessage* message);