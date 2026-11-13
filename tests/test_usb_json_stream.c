#include "usb_json_stream.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>
static char output[20000];static size_t size;static bool collect(void*ctx,const char*p,size_t n){(void)ctx;assert(n<=128&&size+n<sizeof(output));memcpy(output+size,p,n);size+=n;return true;}
static bool reject(void*ctx,const char*p,size_t n){(void)ctx;(void)p;(void)n;return false;}
int main(void){cJSON*j=cJSON_CreateObject();cJSON_AddStringToObject(j,"中文\"\\\n","照片\t\r\b\f\1");cJSON_AddNumberToObject(j,"number",1.23456789);cJSON_AddNumberToObject(j,"nan",NAN);cJSON_AddBoolToObject(j,"bool",true);cJSON_AddNullToObject(j,"null");cJSON*a=cJSON_AddArrayToObject(j,"array");for(int i=0;i<200;i++)cJSON_AddItemToArray(a,cJSON_CreateNumber(i));assert(usb_json_write(j,collect,NULL));output[size]=0;char*expected=cJSON_PrintUnformatted(j);cJSON*reference=cJSON_Parse(expected);cJSON_free(expected);cJSON*p=cJSON_Parse(output);assert(p&&reference&&cJSON_Compare(reference,p,true));cJSON_Delete(reference);assert(!usb_json_write(j,reject,NULL));cJSON_Delete(j);cJSON_Delete(p);puts("PASS bounded USB JSON stream: strings, UTF8, controls, numbers, null, arrays and write failure");}
