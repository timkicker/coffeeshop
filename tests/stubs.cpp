#include "app/Paths.h"

// Paths::sdMounted is a static member - needs a definition in exactly one TU
bool Paths::sdMounted = false;

// curl stubs - fetch() is never called in tests
extern "C" {
    void* curl_easy_init()                 { return nullptr; }
    void  curl_easy_cleanup(void*)         {}
    int   curl_easy_perform(void*)         { return 1; }
    int   curl_easy_setopt(void*, int, ...) { return 0; }
    int   curl_easy_getinfo(void*, int, ...) { return 0; }
    const char* curl_easy_strerror(int)    { return "stub"; }
    void* curl_slist_append(void*, const char*) { return nullptr; }
    void  curl_slist_free_all(void*)       {}
}
