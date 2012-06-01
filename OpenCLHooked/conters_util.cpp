#include "counters_util.h"

void to_string(int val, char* s) {
	char temp[1024] = { 0 };
	sprintf(temp, "%d", val);
	strncpy(s, temp, strlen(temp) + 1);
}

void to_string(long val, char* s) {
	char temp[1024] = { 0 };
	sprintf(temp, "%I64u", val);
	strncpy(s, temp, strlen(temp) + 1);
}

void to_string(float val, char* s) {
	char temp[1024] = { 0 };
	sprintf(temp, "%f", val);
	strncpy(s, temp, strlen(temp) + 1);
}

void to_string(double val, char* s) {
	char temp[1024] = { 0 };
	sprintf(temp, "%f", val);
	strncpy(s, temp, strlen(temp) + 1);
}

void to_string(char* val, char* s) {
	strncpy(s, val, strlen(val) + 1);
	s[strlen(val)] = 0;
}

char** get_counters(unsigned int session_id, unsigned int sample_id, unsigned int enabled_counters) {
	char** counters_value = (char**)malloc(enabled_counters * sizeof(char*));
	const int length = 1024;

	for(unsigned int counter = 0; counter < enabled_counters; counter++) { 
		unsigned int enabled_counter_index; 
        GPA_GetEnabledIndex(counter, &enabled_counter_index); 
        GPA_Type type; 
        GPA_GetCounterDataType(enabled_counter_index, &type);
		printf("Enabled index = %d\n", enabled_counter_index);
		if(type == GPA_TYPE_UINT32) { 
            gpa_uint32 value; 
            GPA_GetSampleUInt32(session_id, sample_id, enabled_counter_index, &value); 
			counters_value[counter] = (char*)malloc(length);
			to_string((int)value, counters_value[counter]);
         } 
         else if(type == GPA_TYPE_UINT64) { 
			gpa_uint64 value; 
            GPA_GetSampleUInt64(session_id, sample_id, enabled_counter_index, &value); 
			counters_value[counter] = (char*)malloc(length);
			to_string((long)value, counters_value[counter]);
         } 
         else if(type == GPA_TYPE_FLOAT32) { 
            gpa_float32 value; 
            GPA_GetSampleFloat32(session_id, sample_id, enabled_counter_index, &value); 
			counters_value[counter] = (char*)malloc(length);
			to_string((float)value, counters_value[counter]);
         } 
         else if(type == GPA_TYPE_FLOAT64) { 
            gpa_float64 value; 
            GPA_GetSampleFloat64(session_id, sample_id, enabled_counter_index, &value); 
			counters_value[counter] = (char*)malloc(length);
			to_string((float)value, counters_value[counter]);
         } 
         else { 
            assert(false); 
         } 
	}
	return counters_value;
}