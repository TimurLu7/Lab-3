#include "pch.h"
#include "CppUnitTest.h"
#include "C:\Users\Timur\proga\ОС\Lab3\Lab3\MarkerArgs.h"
#include <windows.h>
#include <iostream>
#include <vector>
#include <stdexcept>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTestLab3
{
    TEST_CLASS(UnitTestLab3)
    {
    public:

        TEST_METHOD(MarkerArgs_Creation_Initialization)
        {
            CRITICAL_SECTION cs;
            InitializeCriticalSection(&cs);
            HANDLE startEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
            HANDLE cantContinue = CreateEvent(NULL, TRUE, FALSE, NULL);
            HANDLE resume = CreateEvent(NULL, FALSE, FALSE, NULL);
            HANDLE terminate = CreateEvent(NULL, FALSE, FALSE, NULL);

            std::vector<int> array(10, 0);

            MarkerArgs args;
            args.array = array.data();
            args.size = 10;
            args.cs = &cs;
            args.startEvent = startEvent;
            args.cantContinueEvent = cantContinue;
            args.resumeEvent = resume;
            args.terminateEvent = terminate;
            args.num = 1;

            Assert::AreEqual(10, args.size);
            Assert::AreEqual(1, args.num);
            Assert::IsNotNull(args.array);
            Assert::IsNotNull(args.cs);

            DeleteCriticalSection(&cs);
            CloseHandle(startEvent);
            CloseHandle(cantContinue);
            CloseHandle(resume);
            CloseHandle(terminate);
        }

        TEST_METHOD(CriticalSection_Initialization)
        {
            CRITICAL_SECTION cs;
            InitializeCriticalSection(&cs);

            EnterCriticalSection(&cs);
            bool inCriticalSection = true;
            LeaveCriticalSection(&cs);

            Assert::IsTrue(inCriticalSection);
            DeleteCriticalSection(&cs);
        }

        TEST_METHOD(Event_Creation_Signal)
        {
            HANDLE event = CreateEvent(NULL, TRUE, FALSE, NULL);
            Assert::IsNotNull(event);
            Assert::AreNotEqual(INVALID_HANDLE_VALUE, event);

            DWORD result = WaitForSingleObject(event, 0);

            BOOL setResult = SetEvent(event);
            Assert::AreNotEqual(0, setResult);

            result = WaitForSingleObject(event, 0);
            Assert::AreEqual(WAIT_OBJECT_0, result);

            CloseHandle(event);
        }

        TEST_METHOD(Array_InitializationZero)
        {
            const int SIZE = 5;
            std::vector<int> array(SIZE, 0);

            for (int i = 0; i < SIZE; i++) {
                Assert::AreEqual(0, array[i]);
            }
            Assert::AreEqual(SIZE, (int)array.size());
        }
    };

    TEST_CLASS(ThreadManagementTests)
    {
    public:
        static DWORD WINAPI TestThread(LPVOID lpParam) {
            int* value = (int*)lpParam;
            *value = 52;
            return 0;
        };
        TEST_METHOD(Thread_CreationAndTermination)
        {
            int threadResult = 0;
            HANDLE thread = CreateThread(NULL, 0, TestThread, &threadResult, 0, NULL);
            Assert::IsNotNull(thread);

            DWORD waitResult = WaitForSingleObject(thread, INFINITE);
            Assert::AreEqual(WAIT_OBJECT_0, waitResult);
            Assert::AreEqual(52, threadResult);

            CloseHandle(thread);
        }

        TEST_METHOD(MultipleEvents_WaitForMultipleObjects)
        {
            HANDLE events[2];
            events[0] = CreateEvent(NULL, TRUE, FALSE, NULL);
            events[1] = CreateEvent(NULL, TRUE, FALSE, NULL);

            SetEvent(events[0]);

            DWORD result = WaitForMultipleObjects(2, events, FALSE, 100);
            Assert::AreEqual(WAIT_OBJECT_0, result);

            SetEvent(events[1]);
            result = WaitForMultipleObjects(2, events, TRUE, 100);
            Assert::AreEqual(WAIT_OBJECT_0, result);

            CloseHandle(events[0]);
            CloseHandle(events[1]);
        }
    };

    TEST_CLASS(MarkerSimulationTests)
    {
    public:
        static int markersCompleted;
        static int arrayModifications;

        static DWORD WINAPI SimpleMarkerSimulation(LPVOID lpParam) {
            MarkerArgs* args = (MarkerArgs*)lpParam;

            WaitForSingleObject(args->startEvent, INFINITE);

            EnterCriticalSection(args->cs);
            for (int i = 0; i < args->size; i++) {
                if (args->array[i] == 0) {
                    args->array[i] = args->num;
                    arrayModifications++;
                    break;
                }
            }
            LeaveCriticalSection(args->cs);

            SetEvent(args->cantContinueEvent);

            WaitForSingleObject(args->terminateEvent, INFINITE);

            EnterCriticalSection(args->cs);
            for (int i = 0; i < args->size; i++) {
                if (args->array[i] == args->num) {
                    args->array[i] = 0;
                }
            }
            LeaveCriticalSection(args->cs);

            markersCompleted++;
            delete args;
            return 0;
        }

        TEST_METHOD_INITIALIZE(TestMethodInitialize)
        {
            markersCompleted = 0;
            arrayModifications = 0;
        }

        TEST_METHOD(SingleMarker_CompleteCycle)
        {
            const int ARRAY_SIZE = 5;
            std::vector<int> array(ARRAY_SIZE, 0);
            CRITICAL_SECTION cs;
            InitializeCriticalSection(&cs);

            HANDLE startEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
            HANDLE cantContinue = CreateEvent(NULL, TRUE, FALSE, NULL);
            HANDLE terminate = CreateEvent(NULL, FALSE, FALSE, NULL);

            MarkerArgs* args = new MarkerArgs;
            args->array = array.data();
            args->size = ARRAY_SIZE;
            args->cs = &cs;
            args->startEvent = startEvent;
            args->cantContinueEvent = cantContinue;
            args->terminateEvent = terminate;
            args->num = 1;

            HANDLE thread = CreateThread(NULL, 0, SimpleMarkerSimulation, args, 0, NULL);
            Assert::IsNotNull(thread);

            SetEvent(startEvent);

            DWORD result = WaitForSingleObject(cantContinue, 10000);
            Assert::AreEqual(WAIT_OBJECT_0, result);

            EnterCriticalSection(&cs);
            bool foundMark = false;
            for (int i = 0; i < ARRAY_SIZE; i++) {
                if (array[i] == 1) {
                    foundMark = true;
                    break;
                }
            }
            LeaveCriticalSection(&cs);
            Assert::IsTrue(foundMark);

            Assert::AreEqual(1, arrayModifications);

            SetEvent(terminate);
            WaitForSingleObject(thread, INFINITE);

            EnterCriticalSection(&cs);
            for (int i = 0; i < ARRAY_SIZE; i++) {
                Assert::AreEqual(0, array[i]);
            }
            LeaveCriticalSection(&cs);

            Assert::AreEqual(1, markersCompleted);

            CloseHandle(thread);
            CloseHandle(startEvent);
            CloseHandle(cantContinue);
            CloseHandle(terminate);
            DeleteCriticalSection(&cs);
        }

        TEST_METHOD(MultipleMarkers)
        {
            const int MARKER_COUNT = 3;
            const int ARRAY_SIZE = 10;

            std::vector<int> array(ARRAY_SIZE, 0);
            CRITICAL_SECTION cs;
            InitializeCriticalSection(&cs);
            HANDLE startEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

            std::vector<HANDLE> threads;
            std::vector<HANDLE> cantContinueEvents;
            std::vector<HANDLE> terminateEvents;

            for (int i = 0; i < MARKER_COUNT; i++) {
                HANDLE cantContinue = CreateEvent(NULL, TRUE, FALSE, NULL);
                HANDLE terminate = CreateEvent(NULL, FALSE, FALSE, NULL);

                MarkerArgs* args = new MarkerArgs;
                args->array = array.data();
                args->size = ARRAY_SIZE;
                args->cs = &cs;
                args->startEvent = startEvent;
                args->cantContinueEvent = cantContinue;
                args->terminateEvent = terminate;
                args->num = i + 1;

                HANDLE thread = CreateThread(NULL, 0, SimpleMarkerSimulation, args, 0, NULL);

                threads.push_back(thread);
                cantContinueEvents.push_back(cantContinue);
                terminateEvents.push_back(terminate);
            }

            SetEvent(startEvent);

            for (int i = 0; i < MARKER_COUNT; i++) {
                DWORD result = WaitForSingleObject(cantContinueEvents[i], 10000);
                Assert::AreEqual(WAIT_OBJECT_0, result);
            }

            Assert::AreEqual(MARKER_COUNT, arrayModifications);

            for (int i = 0; i < MARKER_COUNT; i++) {
                SetEvent(terminateEvents[i]);
                DWORD result = WaitForSingleObject(threads[i], 5000);
                Assert::AreEqual(WAIT_OBJECT_0, result);

                Assert::AreEqual(i + 1, markersCompleted);
            }

            EnterCriticalSection(&cs);
            for (int i = 0; i < ARRAY_SIZE; i++) {
                Assert::AreEqual(0, array[i]);
            }
            LeaveCriticalSection(&cs);

            for (auto handle : threads) CloseHandle(handle);
            for (auto handle : cantContinueEvents) CloseHandle(handle);
            for (auto handle : terminateEvents) CloseHandle(handle);
            CloseHandle(startEvent);
            DeleteCriticalSection(&cs);
        }

        TEST_METHOD(MultipleMarkers_2)
        {

            const int MARKER_COUNT = 2;
            const int ARRAY_SIZE = 5;

            std::vector<int> array(ARRAY_SIZE, 0);
            CRITICAL_SECTION cs;
            InitializeCriticalSection(&cs);
            HANDLE startEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

            std::vector<HANDLE> threads;
            std::vector<HANDLE> cantContinueEvents;
            std::vector<HANDLE> terminateEvents;

            for (int i = 0; i < MARKER_COUNT; i++) {
                HANDLE cantContinue = CreateEvent(NULL, TRUE, FALSE, NULL);
                HANDLE terminate = CreateEvent(NULL, FALSE, FALSE, NULL);

                MarkerArgs* args = new MarkerArgs;
                args->array = array.data();
                args->size = ARRAY_SIZE;
                args->cs = &cs;
                args->startEvent = startEvent;
                args->cantContinueEvent = cantContinue;
                args->terminateEvent = terminate;
                args->num = i + 1;

                HANDLE thread = CreateThread(NULL, 0, SimpleMarkerSimulation, args, 0, NULL);

                threads.push_back(thread);
                cantContinueEvents.push_back(cantContinue);
                terminateEvents.push_back(terminate);
            }

            SetEvent(startEvent);

            for (int i = 0; i < MARKER_COUNT; i++) {
                DWORD result = WaitForSingleObject(cantContinueEvents[i], 5000);
                Assert::AreEqual(WAIT_OBJECT_0, result);
            }

            EnterCriticalSection(&cs);
            int markedElements = 0;
            for (int i = 0; i < ARRAY_SIZE; i++) {
                if (array[i] != 0) {
                    markedElements++;
                    Assert::IsTrue(array[i] == 1 || array[i] == 2);
                }
            }
            Assert::AreEqual(MARKER_COUNT, markedElements);
            LeaveCriticalSection(&cs);

            for (int i = 0; i < MARKER_COUNT; i++) {
                SetEvent(terminateEvents[i]);
            }

            for (auto thread : threads) {
                DWORD result = WaitForSingleObject(thread, 10000);
                Assert::AreEqual(WAIT_OBJECT_0, result);
                CloseHandle(thread);
            }

            for (auto thread : threads) {
                WaitForSingleObject(thread, 10000);
                CloseHandle(thread);
            }
            for (auto handle : cantContinueEvents) CloseHandle(handle);
            CloseHandle(startEvent);
            DeleteCriticalSection(&cs);
        }
    };
    int MarkerSimulationTests::markersCompleted = 0;
    int MarkerSimulationTests::arrayModifications = 0;

    TEST_CLASS(ErrorHandlingTests)
    {
    public:

        TEST_METHOD(InvalidArraySize_ThrowsException)
        {
            auto func = []() {
                int size = -1;
                if (size <= 0) {
                    throw std::runtime_error("Некорректный размер массива.");
                }
                };

            Assert::ExpectException<std::runtime_error>(func);
        }

        TEST_METHOD(InvalidMarkerCount_ThrowsException)
        {
            auto func = []() {
                int n = 0;
                if (n <= 0) {
                    throw std::runtime_error("Некорректное количество маркеров.");
                }
                };

            Assert::ExpectException<std::runtime_error>(func);
        }

        TEST_METHOD(EventCreationFailure_ThrowsException)
        {
            auto func = []() {
                HANDLE event = NULL;
                if (!event) {
                    throw std::runtime_error("Не удалось создать событие startEvent.");
                }
                };

            Assert::ExpectException<std::runtime_error>(func);
        }
    };

    TEST_CLASS(IntegrationTests)
    {
    public:

        TEST_METHOD(FullWorkflow_SingleMarker)
        {
            const int ARRAY_SIZE = 5;
            std::vector<int> array(ARRAY_SIZE, 0);

            CRITICAL_SECTION cs;
            InitializeCriticalSection(&cs);

            HANDLE startEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
            HANDLE cantContinue = CreateEvent(NULL, TRUE, FALSE, NULL);
            HANDLE resume = CreateEvent(NULL, FALSE, FALSE, NULL);
            HANDLE terminate = CreateEvent(NULL, FALSE, FALSE, NULL);

            for (int i = 0; i < ARRAY_SIZE; i++) {
                Assert::AreEqual(0, array[i]);
            }

            SetEvent(startEvent);

            EnterCriticalSection(&cs);
            array[2] = 1;
            LeaveCriticalSection(&cs);

            EnterCriticalSection(&cs);
            Assert::AreEqual(1, array[2]);
            LeaveCriticalSection(&cs);

            SetEvent(terminate);

            EnterCriticalSection(&cs);
            for (int i = 0; i < ARRAY_SIZE; i++) {
                if (array[i] == 1) {
                    array[i] = 0;
                }
            }
            LeaveCriticalSection(&cs);

            for (int i = 0; i < ARRAY_SIZE; i++) {
                Assert::AreEqual(0, array[i]);
            }

            DeleteCriticalSection(&cs);
            CloseHandle(startEvent);
            CloseHandle(cantContinue);
            CloseHandle(resume);
            CloseHandle(terminate);
        }

        TEST_METHOD(Array_AfterMultipleOperations)
        {
            const int ARRAY_SIZE = 3;
            std::vector<int> array(ARRAY_SIZE, 0);
            CRITICAL_SECTION cs;
            InitializeCriticalSection(&cs);

            EnterCriticalSection(&cs);
            array[0] = 1;
            array[1] = 2;
            array[2] = 1;
            LeaveCriticalSection(&cs);

            EnterCriticalSection(&cs);
            Assert::AreEqual(1, array[0]);
            Assert::AreEqual(2, array[1]);
            Assert::AreEqual(1, array[2]);

            int validElements = 0;
            for (int i = 0; i < ARRAY_SIZE; i++) {
                if (array[i] >= 0 && array[i] <= 2) {
                    validElements++;
                }
            }
            Assert::AreEqual(ARRAY_SIZE, validElements);
            LeaveCriticalSection(&cs);

            DeleteCriticalSection(&cs);
        }
    };
}
