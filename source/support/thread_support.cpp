#include "support/thread_support.h"

#ifdef _WIN32
#include <string>
#elif defined(__linux__)
#include <pthread.h>
#elif defined(__APPLE__)
#include <pthread.h>
#endif

using namespace mosaic;

void mosaic::set_current_thread_name(char const* name) {
#ifdef _WIN32
    // Convert UTF-8 to UTF-16
    int size = MultiByteToWideChar(CP_UTF8, 0, name, -1, nullptr, 0);
    if (size > 0) {
        std::wstring wide_name(size, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, name, -1, &wide_name[0], size);
        SetThreadDescription(GetCurrentThread(), wide_name.c_str());
    }
#elif defined(__linux__)
    pthread_setname_np(pthread_self(), name);
#elif defined(__APPLE__)
    pthread_setname_np(name);
#endif
}