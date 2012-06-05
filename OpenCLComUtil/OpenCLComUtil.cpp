// OpenCLComUtil.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "OpenCLComUtil.h"

int open_connection(char* pipe_name, bool server, HANDLE* pipe) {
	
	TCHAR w_pipe_name[BUFFER_SIZE] = { 0 };
	MultiByteToWideChar(CP_ACP, 0, pipe_name, -1, w_pipe_name, BUFFER_SIZE);

	if(!server) {
		printf("Creating client deh pipe %ls...\n", pipe_name);
		*pipe = CreateFile( 
			w_pipe_name,
			GENERIC_READ | GENERIC_WRITE, 
			0,              // no sharing 
			NULL,           // default security attributes
			OPEN_EXISTING,  // opens existing pipe 
			0,              // default attributes 
			NULL);          // no template file 

		if(*pipe == INVALID_HANDLE_VALUE) {
			if (GetLastError() == ERROR_FILE_NOT_FOUND) {
				printf("Pipe not found, trying create it <-\n");
				SetLastError(0);
				printf("Reset last error\n");
				*pipe = CreateFile( 
					w_pipe_name,
					GENERIC_READ | GENERIC_WRITE, 
					0,              // no sharing 
					NULL,           // default security attributes
					CREATE_ALWAYS,  // opens existing pipe 
					0,              // default attributes 
					NULL);          // no template file 
			}
			if(*pipe == INVALID_HANDLE_VALUE) {
				if (GetLastError() != ERROR_PIPE_BUSY) {
					printf("Pipe not busy %d <-\n", GetLastError());
					return GetLastError(); 
				}
			}
			if (!WaitNamedPipe(w_pipe_name, NMPWAIT_WAIT_FOREVER)) {
				printf("Wait timeout <-\n");
				return -2; 
			}
		}
		DWORD mode = PIPE_READMODE_MESSAGE; 
		if(!SetNamedPipeHandleState(*pipe, &mode, NULL, NULL))
			return GetLastError();
	}
	else {	
		printf("Creating server pipe...\n");
		*pipe = CreateNamedPipe( 
			w_pipe_name,             // pipe name 
			PIPE_ACCESS_DUPLEX,       // read/write access 
			PIPE_TYPE_MESSAGE |       // message type pipe 
			PIPE_READMODE_MESSAGE |   // message-read mode 
			PIPE_WAIT,                // blocking mode 
			PIPE_UNLIMITED_INSTANCES, // max. instances  
			BUFFER_SIZE,                  // output buffer size 
			BUFFER_SIZE,                  // input buffer size 
			0,                        // client time-out 
			NULL);                    // default security attribute 
		if (*pipe == INVALID_HANDLE_VALUE) 
			return -1;
	}
	return 0;
}

int close_connection(HANDLE pipe) {
	if(pipe != INVALID_HANDLE_VALUE)
		return (CloseHandle(pipe) == TRUE) ? 0 : -1;
	return 0;
}

int free_message(OpenCLMonitoringMessage* message) {
	if(message->body != NULL && message->body_count > 0) {
		for(int i = 0; i < message->body_count; i++)
			free(message->body[i]);
	}
	free(message->body);
	message->body = NULL;
	message->body_count = 0;
	return 0;
}

int send_message(HANDLE pipe, OpenCLMonitoringMessage* message) {	
	DWORD bytes_written = 0;
	char buffer[BUFFER_SIZE] = { 0 };
	sprintf(buffer, "%d %d %d", message->id, message->action_result, message->api_result);
	
	if(message->body != NULL && message->body_count > 0) {
		int offset = strlen(buffer);

		for(int i = 0; i < message->body_count; i++) {
			sprintf(buffer + offset, " %s", message->body[i]);
			offset += strlen(message->body[i]) + 1;
		}
	}

	TCHAR w_buffer[BUFFER_SIZE] = { 0 };
	MultiByteToWideChar(CP_ACP, 0, buffer, -1, w_buffer, BUFFER_SIZE);
	//printf("Send message %ls, %s\n", w_buffer, buffer);
	printf("--> Sending message ID %d = %ls\n", message->id, w_buffer);
	bool result = WriteFile(pipe, w_buffer, BUFFER_SIZE, &bytes_written, NULL);
	if(!result)
		return GetLastError();
   return 0;
}

int send_empty_message(HANDLE pipe, OpenCLMonitoringMessageID id, int action_result, int api_result) {
	OpenCLMonitoringMessage message;
	message.id = id;
	message.action_result = action_result;
	message.api_result = api_result;
	message.body = NULL;
	message.body_count = 0;
	return send_message(pipe, &message);
}

int receive_message(HANDLE pipe, OpenCLMonitoringMessage* message) {	
	TCHAR w_buffer[BUFFER_SIZE] = { 0 };
	memset(w_buffer, 0, BUFFER_SIZE * sizeof(TCHAR));
	DWORD bytes_read = 0;
	bool success = ReadFile( 
		pipe,        // handle to pipe 
		w_buffer,    // buffer to receive data 
		BUFFER_SIZE * sizeof(TCHAR), // size of buffer 
		&bytes_read, // number of bytes read 
		NULL);        // not overlapped I/O 

	if(!success || bytes_read == 0) 
		return GetLastError();
	//Parse message
	printf("Received message %ls\n", w_buffer);
	char buffer[BUFFER_SIZE] = { 0 };
	wcstombs(buffer, w_buffer, BUFFER_SIZE);
	//printf("Received message %s\n", buffer);

	const char* delim = " "; 
	char* token = strtok(buffer, delim);
	if(token != NULL)
		message->id = (OpenCLMonitoringMessageID)atoi(token);
	else
		return -1;
	token = strtok(NULL, delim);
	if(token != NULL)
		message->action_result = atoi(token);
	else
		return -1;
	token = strtok(NULL, delim);
	if(token != NULL)
		message->api_result = atoi(token);
	else
		return -1;

	message->body = NULL;
	message->body_count = 0;
	token = strtok(NULL, delim);
	if(token != NULL)
		message->body = (char**)malloc(BUFFER_SIZE * sizeof(char*));
	int index = 0;
	while(token != NULL) {
		message->body[index] = (char*)malloc(strlen(token) + sizeof(char*));
		strcpy(message->body[index], token);
		message->body[index][strlen(token)] = 0;
		token = strtok(NULL, delim);
		index++;
	}
	message->body_count = index;
	return 0;
}