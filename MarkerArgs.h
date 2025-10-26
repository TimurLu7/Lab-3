#pragma once
#include <windows.h>
struct MarkerArgs {
    int* array;
    int size;
    CRITICAL_SECTION* cs;
    HANDLE startEvent;
    HANDLE cantContinueEvent;
    HANDLE resumeEvent;
    HANDLE terminateEvent;
    int num;
};