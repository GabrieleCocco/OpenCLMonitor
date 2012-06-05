// OpenCLHooked.cpp : Defines the exported functions for the DLL application.
//

#include "cl.h"

#define PIPE_NAME "\\\\.\\pipe\\openclmonitor_pipe"
#define MUTEX_NAME "Global\\openclmonitor_mutex"
#define MAX_QUEUE 1024
#define DEBUG_PRINT true

CLMonitoringQueueInfo queue_info[MAX_QUEUE];
int current_queue_index = 0;

extern CL_API_ENTRY cl_command_queue CL_API_CALL clCreateCommandQueueHooked(cl_context context, cl_device_id device_id, cl_command_queue_properties command_queue_properties, cl_int *  status) {
	cl_int temp_status = CL_SUCCESS;
	GPA_Status gpa_status = GPA_STATUS_OK;
	OpenCLMonitoringMessage message;
	message.body_count = 0;
	message.body = NULL;
	HANDLE server_pipe = NULL; 
	HANDLE private_pipe = NULL;
	CLMonitoringQueueInfo info;
	
	//Create command queue
	cl_command_queue queue = real_clCreateCommandQueue(context, device_id, command_queue_properties, &temp_status);

	//Open connection to server
	HANDLE mutex = CreateMutex(NULL, FALSE, TEXT(MUTEX_NAME));
	if(mutex == INVALID_HANDLE_VALUE)
		goto ret;
	DWORD mutex_status = WaitForSingleObject(mutex, INFINITE);
	printf("OK\n");
	if(mutex_status == WAIT_OBJECT_0) {
#ifdef DEBUG_PRINT
	printf("Open connection to the pipe %s\n", PIPE_NAME);
#endif
	int r = open_connection(PIPE_NAME, 0, &server_pipe);
		if(r != 0) {
			printf("Error opening connection %d\n", r);
			ReleaseMutex(mutex);
			goto ret;
		}
		if(temp_status != CL_SUCCESS) {
			send_empty_message(server_pipe, CREATE_QUEUE_MESSAGE, -1, temp_status);
#ifdef DEBUG_PRINT
	printf("CREATE_QUEUE_MESSAGE sent (%d, %d)...", -1, temp_status);
#endif
			receive_message(server_pipe, &message);
#ifdef DEBUG_PRINT
	printf("and received answer!\n");
#endif
			free_message(&message);
			ReleaseMutex(mutex);
			goto ret;
		}
			
		//Inform server of new queue and wait the name of a private pipe to handle monitoring of theb queue

		message.id = CREATE_QUEUE_MESSAGE;
		message.action_result = 0;
		message.api_result = 0;
		message.body_count = 1;
		char* message_body = (char*)malloc(BUFFER_SIZE * sizeof(char));
		memset(message_body, 0, BUFFER_SIZE * sizeof(char));
		sprintf(message_body, "%d", queue); 
		message.body = &message_body;
		message.body_count = 1;
#ifdef DEBUG_PRINT
	printf("CREATE_QUEUE_MESSAGE sent (%d, %d)...", 0, 0);
#endif
		send_message(server_pipe, &message);
		free(message_body);

		//Receive new queue name
#ifdef DEBUG_PRINT
	//printf("  Receive new queue name\n");
#endif
		receive_message(server_pipe, &message);
#ifdef DEBUG_PRINT
	printf("and received answer (%s)!\n", message.body[0]);
#endif
		char private_pipe_name[1024] = { 0 };
		sprintf(private_pipe_name, "%s", "\\\\.\\pipe\\");
		sprintf(private_pipe_name + strlen("\\\\.\\pipe\\"), "%s",  message.body[0]);
		free_message(&message);

		//Close connection to server and open private connection
#ifdef DEBUG_PRINT
//	printf("  Close server connection\n");
#endif
		close_connection(server_pipe);
		r = open_connection(private_pipe_name, false, &private_pipe);
		if(r != 0) {
			ReleaseMutex(mutex);	
			goto ret;
		}
		ReleaseMutex(mutex);	
	}

	//Init GPU perf
	gpa_status = GPA_Initialize();
	if(gpa_status != GPA_STATUS_OK) {	
		send_empty_message(private_pipe, ENABLE_COUNTERS_MESSAGE, -1, gpa_status);
#ifdef DEBUG_PRINT
	printf("ENABLE_COUNTERS_MESSAGE sent (%d, %d)...", -1, gpa_status);
#endif
		receive_message(private_pipe, &message);
#ifdef DEBUG_PRINT
	printf("and received answer!\n");
#endif
		free_message(&message);
		goto ret;
	}

	//Open context
	gpa_status = GPA_OpenContext(queue);
	if(gpa_status != GPA_STATUS_OK) {
		send_empty_message(private_pipe, ENABLE_COUNTERS_MESSAGE, -1, gpa_status);
#ifdef DEBUG_PRINT
	printf("ENABLE_COUNTERS_MESSAGE sent(%d, %d)...", -1, gpa_status);
#endif
		receive_message(private_pipe, &message);
#ifdef DEBUG_PRINT
	printf("and received answer!\n");
#endif
		free_message(&message);
		goto ret;
	}
	
	//Get counters count
	unsigned int c_count = 0;
	gpa_status = GPA_GetNumCounters(&c_count); 
	if(gpa_status != GPA_STATUS_OK) {
		send_empty_message(private_pipe, ENABLE_COUNTERS_MESSAGE, -1, gpa_status);
#ifdef DEBUG_PRINT
	printf("ENABLE_COUNTERS_MESSAGE sent (%d, %d)...", -1, gpa_status);
#endif
		receive_message(private_pipe, &message);
#ifdef DEBUG_PRINT
	printf("and received answer!\n");
#endif
		free_message(&message);
		goto ret;
	}

	//Build counters name list
#ifdef DEBUG_PRINT
	printf("Counters count = %d\n", c_count);
#endif	
	message.body = (char**)malloc(BUFFER_SIZE * sizeof(char*));
	int offset = 0;
	for(unsigned int i = 0; i < c_count; i++) {
		const char* name = 0;
		gpa_status = GPA_GetCounterName(i, &name);
		if(gpa_status != GPA_STATUS_OK) {		
			send_empty_message(private_pipe, ENABLE_COUNTERS_MESSAGE, -1, gpa_status);
#ifdef DEBUG_PRINT
	printf("ENABLE_COUNTERS_MESSAGE sent (%d, %d)...", -1, gpa_status);
#endif
		receive_message(private_pipe, &message);
#ifdef DEBUG_PRINT
	printf("and received answer!\n");
#endif
			free_message(&message);
			goto ret;
		}
		message.body[i] = (char*)malloc((strlen(name) + 1) * sizeof(char));
		strcpy(message.body[i], name);
		message.body[i][strlen(name)] = 0;
	}

	//Communicate counters name list
	message.id = ENABLE_COUNTERS_MESSAGE;
	message.action_result = 0;
	message.api_result = 0;
	message.body_count = c_count;
	send_message(private_pipe, &message);
#ifdef DEBUG_PRINT
	printf("ENABLE_COUNTERS_MESSAGE sent (%d, %d)...", 0, gpa_status);
#endif
	free_message(&message);
	//Get indexes of counters to be enabled
	receive_message(private_pipe, &message);
#ifdef DEBUG_PRINT
	printf("and received answer!\n");
#endif		
#ifdef DEBUG_PRINT
	printf("Enable selected counters (total = %d) - (first = %d)\n", message.body_count, atoi(message.body[0]));
#endif		 
	//Enable counters
	for(int i = 0; i < message.body_count; i++) {
		gpa_status = GPA_EnableCounter(atoi(message.body[i]));
		if(gpa_status != GPA_STATUS_OK) {
			free_message(&message);
			send_empty_message(private_pipe, GPU_PERF_INIT_MESSAGE, -1, gpa_status);
#ifdef DEBUG_PRINT
	printf("ENABLE_COUNTERS_MESSAGE sent (%d, %d)...", -1, gpa_status);
#endif
		receive_message(private_pipe, &message);
#ifdef DEBUG_PRINT
	printf("and received answer!\n");
#endif
		goto ret;
		}
	}
	free_message(&message);

	//Start session
	gpa_status = GPA_BeginSession(&info.session_id);
	if(gpa_status != GPA_STATUS_OK) {
		send_empty_message(private_pipe, GPU_PERF_INIT_MESSAGE, -1, gpa_status);
#ifdef DEBUG_PRINT
	printf("GPU_PERF_INIT_MESSAGE sent (%d, %d)...", -1, gpa_status);
#endif
		receive_message(private_pipe, &message);
#ifdef DEBUG_PRINT
	printf("and received answer!\n");
#endif
		free_message(&message);
		goto ret;
	}
	
	//Start pass
	gpa_status = GPA_BeginPass();
	if(gpa_status != GPA_STATUS_OK) {
		send_empty_message(private_pipe, GPU_PERF_INIT_MESSAGE, -1, gpa_status);
#ifdef DEBUG_PRINT
	printf("GPU_PERF_INIT_MESSAGE sent (%d, %d)...", -1, gpa_status);
#endif
		receive_message(private_pipe, &message);
#ifdef DEBUG_PRINT
	printf("and received answer!\n");
#endif
		free_message(&message);
		goto ret;
	}

	//Start sample
	gpa_status = GPA_BeginSample(0);
	if(gpa_status != GPA_STATUS_OK) {
		send_empty_message(private_pipe, GPU_PERF_INIT_MESSAGE, -1, gpa_status);
#ifdef DEBUG_PRINT
	printf("GPU_PERF_INIT_MESSAGE sent (%d, %d)...", -1, gpa_status);
#endif
		receive_message(private_pipe, &message);
#ifdef DEBUG_PRINT
	printf("and received answer!\n");
#endif
		free_message(&message);
		goto ret;
	}

	//Not really an error message
	message.id = GPU_PERF_INIT_MESSAGE;
	message.action_result = 0;
	message.api_result = 0;
	message.body = NULL;
	message.body_count = 0;
	send_message(private_pipe, &message);	
#ifdef DEBUG_PRINT
	printf("GPU_PERF_INIT_MESSAGE sent (%d, %d)...", 0, gpa_status);
#endif
		receive_message(private_pipe, &message);
#ifdef DEBUG_PRINT
	printf("and received answer!\n");
#endif
	free_message(&message);

	info.private_pipe = private_pipe;
	info.queue = queue;
	info.server_pipe = server_pipe;
	mutex_status = WaitForSingleObject(mutex, INFINITE);
	if(mutex_status == WAIT_OBJECT_0) {	
		printf("Inserting queue info in position %d\n", current_queue_index); 
		//Insert info (pipe, queue) to the array of queue info
		queue_info[current_queue_index] = info;
		current_queue_index++;
	}
	ReleaseMutex(mutex);

ret:
#ifdef DEBUG_PRINT
	printf("Free and go on\n");
#endif
	free_message(&message); 
	if(mutex != INVALID_HANDLE_VALUE)
		CloseHandle(mutex);

	if(status != NULL)
		*status = temp_status;
	return queue;
}

extern CL_API_ENTRY cl_int CL_API_CALL clReleaseCommandQueueHooked(cl_command_queue queue) {	
	GPA_Status gpa_status = GPA_STATUS_OK;
	OpenCLMonitoringMessage message;
	message.body_count = 0;
	message.body = NULL;
	
#ifdef DEBUG_PRINT
	printf("Search into queue infos\n");
#endif
	//Search queue info
	CLMonitoringQueueInfo info; //Use copy instead pointer cause after critical section queue info removals might happen
	
	info.queue = 0;
	HANDLE mutex = CreateMutex(NULL, FALSE, TEXT(MUTEX_NAME));
	if(mutex == INVALID_HANDLE_VALUE)
		goto ret;
	DWORD mutex_status = WaitForSingleObject(mutex, INFINITE);
	if(mutex_status == WAIT_OBJECT_0) {
		for(int i = 0; i < current_queue_index; i++) {
			if(queue_info[i].queue == queue) {
				info = queue_info[i];
				//Mark as no data
#ifdef DEBUG_PRINT
	printf("  Found\n");
#endif
				queue_info[i].queue = 0;
				break;
			}
		}
	}
	ReleaseMutex(mutex);
	if(info.queue == 0)
		goto ret;

	//Release queue message
	int st = send_empty_message(info.private_pipe, RELEASE_QUEUE_MESSAGE, 0, 0);
	printf("%d\n", st);
#ifdef DEBUG_PRINT
	printf("RELEASE_QUEUE_MESSAGE sent (%d, %d)...", 0, 0);
#endif
	st = receive_message(info.private_pipe, &message);
	printf("%d\n", st);
#ifdef DEBUG_PRINT
	printf("and received the answer (%d)!\n", message.api_result);
#endif
	free_message(&message);
	//Start pass
	unsigned int c_count = 0;

	gpa_status = GPA_GetEnabledCount(&c_count); 
	if(gpa_status != GPA_STATUS_OK) {
		send_empty_message(info.private_pipe, GPU_PERF_RELEASE_MESSAGE, -1, gpa_status);
#ifdef DEBUG_PRINT
	printf("GPU_PERF_RELEASE_MESSAGE sent...");
#endif
	receive_message(info.private_pipe, &message);
#ifdef DEBUG_PRINT
	printf("and received the answer!\n");
#endif
		free_message(&message);
		goto ret;
	}

	gpa_status = GPA_EndSample();
	if(gpa_status != GPA_STATUS_OK) {
		send_empty_message(info.private_pipe, GPU_PERF_RELEASE_MESSAGE, -1, gpa_status);
#ifdef DEBUG_PRINT
	printf("GPU_PERF_RELEASE_MESSAGE sent...");
#endif
	receive_message(info.private_pipe, &message);
#ifdef DEBUG_PRINT
	printf("and received the answer!\n");
#endif
		free_message(&message);
		goto ret;
	}
	
	gpa_status = GPA_EndPass();
	if(gpa_status != GPA_STATUS_OK) {	
		send_empty_message(info.private_pipe, GPU_PERF_RELEASE_MESSAGE, -1, gpa_status);
#ifdef DEBUG_PRINT
	printf("GPU_PERF_RELEASE_MESSAGE sent...");
#endif
	receive_message(info.private_pipe, &message);
#ifdef DEBUG_PRINT
	printf("and received the answer!\n");
#endif
		free_message(&message);
		goto ret;
	}
	
	//Wait for sample to be ready
	gpa_status = GPA_EndSession();
	if(gpa_status != GPA_STATUS_OK) {	
		send_empty_message(info.private_pipe, GPU_PERF_RELEASE_MESSAGE, -1, gpa_status);
#ifdef DEBUG_PRINT
	printf("GPU_PERF_RELEASE_MESSAGE sent...");
#endif
	receive_message(info.private_pipe, &message);
#ifdef DEBUG_PRINT
	printf("and received the answer!\n");
#endif
		free_message(&message);
		goto ret;
	}

#ifdef DEBUG_PRINT
	printf("Waiting session ready %d\n", info.session_id);
#endif
	bool ready_result = false; 
	GPA_Status session_status; 
	session_status = GPA_IsSessionReady(&ready_result, info.session_id); 
	while(!ready_result) { 
		session_status = GPA_IsSessionReady(&ready_result, info.session_id); 
	}

	//Get counters
	char** counters = get_counters(info.session_id, 0, c_count);	
#ifdef DEBUG_PRINT
	printf("Read counters\n");
	for(unsigned int i = 0; i < c_count; i++) {
		printf("Counter = %s\n", counters[i]); 
	}
	printf("End read counters\n");
#endif
	
	gpa_status = GPA_CloseContext();
	if(gpa_status != GPA_STATUS_OK) {	
		send_empty_message(info.private_pipe, GPU_PERF_RELEASE_MESSAGE, -1, gpa_status);
#ifdef DEBUG_PRINT
	printf("GPU_PERF_RELEASE_MESSAGE sent...");
#endif
	receive_message(info.private_pipe, &message);
#ifdef DEBUG_PRINT
	printf("and received the answer!\n");
#endif
		free_message(&message);
		goto ret;
	}
	
	//Send gpu perf release (ok) message and wait reply
	send_empty_message(info.private_pipe, GPU_PERF_RELEASE_MESSAGE, 0, 0);
#ifdef DEBUG_PRINT
	printf("GPU_PERF_RELEASE_MESSAGE sent...");
#endif
	receive_message(info.private_pipe, &message);
#ifdef DEBUG_PRINT
	printf("and received the answer!\n");
#endif

	//Send counters message
	message.id = GET_COUNTERS_MESSAGE;
	message.action_result = 0;
	message.api_result = 0;
	message.body = counters;
	message.body_count = c_count;
	send_message(info.private_pipe, &message);
#ifdef DEBUG_PRINT
	printf("GET_COUNTERS_MESSAGE sent...");
#endif
	receive_message(info.private_pipe, &message);
#ifdef DEBUG_PRINT
	printf("and received the answer!");
#endif
	free_message(&message);

ret:
#ifdef DEBUG_PRINT
	printf("Free and go on\n");
#endif
	free_message(&message);
	if(info.queue != 0) {
		close_connection(info.private_pipe);
		close_connection(info.server_pipe);
	}
	if(mutex != INVALID_HANDLE_VALUE)
		CloseHandle(mutex);

	cl_int status = real_clReleaseCommandQueue(queue);
	return status;
}

BOOL __stdcall DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH: 
			{
				DetourRestoreAfterWith();
				DetourTransactionBegin();
				DetourUpdateThread(GetCurrentThread());

				DetourAttach(&(PVOID&)real_clCreateCommandQueue, hook_clCreateCommandQueue);
				DetourAttach(&(PVOID&)real_clReleaseCommandQueue, hook_clReleaseCommandQueue);
				long error = DetourTransactionCommit();

				if (error == NO_ERROR) {
					OutputDebugString(TEXT("testhooks.dll: Attach to functions called: \n"));
				}
				else {
					OutputDebugString(TEXT("testhooks.dll: Error attaching to fucntions: \n"));
				}
			};
			break;
		case DLL_PROCESS_DETACH: 
			{
				DetourTransactionBegin();
				DetourUpdateThread(GetCurrentThread());

				DetourDetach(&(PVOID&)real_clCreateCommandQueue, hook_clCreateCommandQueue);
				DetourDetach(&(PVOID&)real_clReleaseCommandQueue, hook_clReleaseCommandQueue);
				long error = DetourTransactionCommit();
				
				if (error != NO_ERROR) 
					OutputDebugString(TEXT("testhooks.dll: Detach from functions called: \n"));
			};
			break;
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
			break;
	}
	return TRUE;
}

