#include <windows.h>
#include <iostream>
#include <vector>
using namespace std;

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

DWORD WINAPI MarkerFunk(LPVOID lpParam)
{
    MarkerArgs* args = (MarkerArgs*)lpParam;

    srand(args->num); 
    WaitForSingleObject(args->startEvent, INFINITE); 

    while (true)
    {
        int idx = rand() % args->size;

        EnterCriticalSection(args->cs);
        if (args->array[idx] == 0)
        {
            Sleep(5);
            args->array[idx] = args->num;
            Sleep(5);
            LeaveCriticalSection(args->cs);
            continue;
        }

        int Count = 0;
        for (int i = 0; i < args->size; i++)
            if (args->array[i] == args->num) 
                ++Count;

        cout << "Marker " << args->num << ": marked = " << Count << ", cannot mark index " << idx << endl;

        LeaveCriticalSection(args->cs);

        SetEvent(args->cantContinueEvent);

        HANDLE waits[2] = { args->terminateEvent, args->resumeEvent };
        DWORD res = WaitForMultipleObjects(2, waits, FALSE, INFINITE);

        if (res == WAIT_OBJECT_0) 
        {
            EnterCriticalSection(args->cs);
            for (int i = 0; i < args->size; i++)
                if (args->array[i] == args->num)
                    args->array[i] = 0;
            cout << "Marker " << args->num << " terminated and cleared its marks." << endl;
            LeaveCriticalSection(args->cs);
            break;
        }
        else if (res == WAIT_OBJECT_0 + 1) 
        {
            cout << "Marker " << args->num << " resumed." << endl;
            continue;
        }
        else break;
    }

    delete args; 
    return 0;
}

int main()
{
    setlocale(LC_ALL, "rus");

    int size;
    cout << "Введите размер массива: ";
    cin >> size;

    vector<int> array(size, 0);

    int n;
    cout << "Введите количество маркеров: ";
    cin >> n;

    CRITICAL_SECTION cs;
    InitializeCriticalSection(&cs);

    HANDLE startEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    vector<HANDLE> cantContinue(n);
    vector<HANDLE> resume(n);
    vector<HANDLE> terminate(n);
    vector<HANDLE> threads(n);

    for (int i = 0; i < n; i++)
    {
        cantContinue[i] = CreateEvent(NULL, TRUE, FALSE, NULL);
        resume[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
        terminate[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
    }

    for (int i = 0; i < n; i++)
    {
        MarkerArgs* args = new MarkerArgs;
        args->array = array.data();
        args->size = size;
        args->cs = &cs;
        args->startEvent = startEvent;
        args->cantContinueEvent = cantContinue[i];
        args->resumeEvent = resume[i];
        args->terminateEvent = terminate[i];
        args->num = i + 1;

        threads[i] = CreateThread(NULL, 0, MarkerFunk, args, 0, NULL);
    }

    SetEvent(startEvent);

    vector<bool> working(n, true);
    int remaining = n;

    while (remaining > 0)
    {
        vector<HANDLE> waits;
        for (int i = 0; i < n; i++)
            if (working[i]) {
                waits.push_back(cantContinue[i]);
            }

        WaitForMultipleObjects((DWORD)waits.size(), waits.data(), TRUE, INFINITE);

        EnterCriticalSection(&cs);
        cout << "Array: ";
        for (int a : array) 
            cout << a << ' ';
        LeaveCriticalSection(&cs);

        int term;
        cout << "Введите номер маркера для завершения: ";
        cin >> term;
        while (term < 1 || term > n || !working[term - 1])
        {
            cout << "Неверный номер, введите снова ";
            cin >> term;
        }

        SetEvent(terminate[term - 1]);
        WaitForSingleObject(threads[term - 1], INFINITE);
        CloseHandle(threads[term - 1]);
        working[term - 1] = false;
        --remaining;

        EnterCriticalSection(&cs);
        cout << "После завершения " << term;
        for (int a : array) 
            cout << a << ' ';
        cout << endl;
        LeaveCriticalSection(&cs);

        for (int i = 0; i < n; i++)
            if (working[i]) {
                ResetEvent(cantContinue[i]);
                SetEvent(resume[i]);
            }
    }

    for (int i = 0; i < n; i++)
    {
        CloseHandle(cantContinue[i]);
        CloseHandle(resume[i]);
        CloseHandle(terminate[i]);
    }

    CloseHandle(startEvent);
    DeleteCriticalSection(&cs);

    cout << "Все маркеры завершены. Программа завершена." << endl;
    return 0;
}

