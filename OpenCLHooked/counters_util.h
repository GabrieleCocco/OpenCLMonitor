#include <GPUPerfAPI.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void to_string(int val, char* s);
void to_string(long val, char* s);
void to_string(float val, char* s);
void to_string(double val, char* s);
void to_string(char* val, char* s);
char** get_counters(unsigned int session_id, unsigned int sample_id, unsigned int enabled_counters);