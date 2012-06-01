// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the OPENCLHOOKED_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// OPENCLHOOKED_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#include <CL/cl.hpp>
#include <detours.h>
#include <GPUPerfAPI.h>
#include <OpenCLComUtil.h>
#include "counters_util.h"

typedef struct CLMonitoringQueueInfo {
	HANDLE private_pipe;
	HANDLE server_pipe;
	cl_command_queue queue;
	unsigned int session_id;
} CLMonitoringQueueInfo;

extern CL_API_ENTRY cl_command_queue CL_API_CALL
	clCreateCommandQueueHooked(cl_context                     /* context */,
                     cl_device_id                   /* device */,
                     cl_command_queue_properties    /* properties */,
                     cl_int *                       /* errcode_ret */) CL_API_SUFFIX__VERSION_1_0;

extern CL_API_ENTRY cl_int CL_API_CALL
	clReleaseCommandQueueHooked(cl_command_queue /* command_queue */) CL_API_SUFFIX__VERSION_1_0;

_declspec(dllexport) int __cdecl Ord1Function() {
    return 0;
}

cl_command_queue (__stdcall *real_clCreateCommandQueue)(cl_context, cl_device_id, cl_command_queue_properties, cl_int*) = clCreateCommandQueue;
cl_command_queue (__stdcall *hook_clCreateCommandQueue)(cl_context, cl_device_id, cl_command_queue_properties, cl_int*) = clCreateCommandQueueHooked;
cl_int (__stdcall *real_clReleaseCommandQueue)(cl_command_queue) = clReleaseCommandQueue;
cl_int (__stdcall *hook_clReleaseCommandQueue)(cl_command_queue) = clReleaseCommandQueueHooked;