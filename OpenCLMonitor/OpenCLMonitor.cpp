// OpenCLMonitor.cpp : Defines the entry point for the console application.
//

#include <windows.h>
#include <stdio.h>
#include <OpenCLComUtil.h>
#include <GPUPerfAPI.h>
#include <CL/cl.hpp>

#define _CRTDBG_MAP_ALLOC
#define _CRTDBG_MAPALLOC
#include <crtdbg.h>
#include <stdlib.h>

#define PIPE_NAME "\\\\.\\pipe\\openclmonitor_pipe"

int wait_client(HANDLE pipe) {
    int connected = ConnectNamedPipe(pipe, NULL)? 0 : -1; 
	return connected;
}

int main(int argc, char* argv[])
{
	int status = 0;
	bool end = false;

	while(true) {

		HANDLE server_pipe = 0;
		OpenCLMonitoringMessage input_message, output_message;

		printf("Creating server pipe...");
		status = open_connection(PIPE_NAME, 1, &server_pipe);
		if(status != 0) {
			printf("FAIL!\n");
			goto free_and_restart;
		}
		printf("DONE\n");
 	
		printf("Waiting client connection...");
		status = wait_client(server_pipe);
		if(status != 0) {
			printf("FAIL!\n");
			goto free_and_restart;
		}
		printf("DONE\n");
		
		//Wait CREATE message and answer with a new pipe
		printf("\n1) Waiting CREATE message...");
		status = receive_message(server_pipe, &input_message);
		if(status < 0) {
			printf("FAIL!\n");
			goto free_and_restart;
		}		
		printf("DONE (Action result = %d, API result = %d)\n", input_message.action_result, input_message.api_result);
		
		if(input_message.action_result != 0) {
			printf("   Client side creation of queue failed\n");
			goto free_and_restart;
		}
		char* private_queue_name = (char*)malloc(BUFFER_SIZE * sizeof(char));
		memset(private_queue_name, 0, BUFFER_SIZE * sizeof(char));
		sprintf(private_queue_name, "%s_%s", PIPE_NAME, input_message.body[0]);

		printf("   Creating new private queue: %s...\n", private_queue_name);
		HANDLE private_pipe = 0;
		//dA GESTIRE OPEN CONNECTION FAILS
		open_connection(private_queue_name, 1, &private_pipe);

		free_message(&input_message);
		printf("   Sending answer...\n");
		output_message.id = OK_MESSAGE;
		output_message.action_result = 0;
		output_message.api_result = 0;
		output_message.body = &private_queue_name;
		output_message.body_count = 1;
		send_message(server_pipe, &output_message);
		free(private_queue_name);

		//Begin listening to the private queue
		close_connection(server_pipe);
		wait_client(private_pipe);

		//Wait ENABLE_COUNTERS message and answer with the indexes
		printf("2) Waiting ENABLE_COUNTERS message...");
		status = receive_message(private_pipe, &input_message);
		if(status < 0) {
			printf("FAIL!\n");
			goto free_and_restart;		
		}
		printf("DONE (Action result = %d, API result = %d)\n", input_message.action_result, input_message.api_result);			

		if(input_message.action_result != 0) {
			send_empty_message(private_pipe, OK_MESSAGE, 0, 0);
			goto free_and_restart;		
		}

		//Print counters available
		printf("   Counters available:\n");
		if(input_message.body_count == 0)
			printf("none");
		else {
			printf("     %s\n", input_message.body[0]);
			for(int i = 1; i < input_message.body_count; i++)
				printf("     %s\n", input_message.body[i]);
		}
		free_message(&input_message);

		printf("   Sending answer (enabling first 2 counters)...\n");
		output_message.id = OK_MESSAGE;
		output_message.action_result = 0;
		output_message.api_result = 0;
		output_message.body_count = 2;
		output_message.body = (char**)malloc(2 * sizeof(char*));
		output_message.body[0] = "0";
		output_message.body[1] = "1";
		send_message(private_pipe, &output_message);
		
		//Wait GPU_PERF_INIT message and answer OK
		printf("3) Waiting GPU_PERF_INIT message...");
		status = receive_message(private_pipe, &input_message);
		if(status < 0) {
			printf("FAIL!\n");
			goto free_and_restart;		
		}
		printf("DONE (Action result = %d, API result = %d)\n", input_message.action_result, input_message.api_result);		
		free_message(&input_message);				
		printf("   Sending answer...\n");
		send_empty_message(private_pipe, OK_MESSAGE, 0, 0);

		//RELEASE
		printf("4) Waiting RELEASE message...\n");
		status = receive_message(private_pipe, &input_message);
		if(status < 0) {
			printf("FAIL!\n");
			goto free_and_restart;		
		}		
		printf("DONE (Action result = %d, API result = %d)\n", input_message.action_result, input_message.api_result);
		free_message(&input_message);
		printf("   Sending answer...\n");
		send_empty_message(private_pipe, OK_MESSAGE, 0, 0);
				
		printf("5) Waiting GPU_PERF_RELEASE message\n");
		status = receive_message(private_pipe, &input_message);
		if(status < 0) {
			printf("FAIL!\n");
			goto free_and_restart;		
		}		
		printf("DONE (Action result = %d, API result = %d)\n", input_message.action_result, input_message.api_result);
		free_message(&input_message);
		printf("   Sending answer...\n");
		send_empty_message(private_pipe, OK_MESSAGE, 0, 0);

		if(input_message.action_result != 0)
			goto free_and_restart;		

		printf("6) Waiting GET_COUNTERS message...\n");
		status = receive_message(private_pipe, &input_message);
		if(status < 0) {
			printf("FAIL!\n");
			goto free_and_restart;		
		}		
		printf("DONE (Action result = %d, API result = %d)\n", input_message.action_result, input_message.api_result);
		if(input_message.action_result == 0) {
			//Print counters values
			printf("   Counter values: \n");
			if(input_message.body_count == 0)
				printf("none\n");
			else {
				printf("     %s\n", input_message.body[0]);
				for(int i = 1; i < input_message.body_count; i++)
					printf("     %s\n", input_message.body[i]);
			}
		}
		free_message(&input_message);
		printf("   Sending answer...\n");
		send_empty_message(private_pipe, OK_MESSAGE, 0, 0);
	
free_and_restart:
		free_message(&input_message);
		close_connection(private_pipe);
		
			_CrtDumpMemoryLeaks();
	}

	return 0;
}