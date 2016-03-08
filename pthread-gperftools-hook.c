/* glibc only gives us RTLD_NEXT if _GNU_SOURCE is defined. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

typedef int (*pthread_create_t)(pthread_t *, const pthread_attr_t *,
                                void *(*)(void*), void *);

typedef void *(*gperftools_register_thread_t)();

static gperftools_register_thread_t gperftools_register_thread;

struct Thunk
{
    void *(*start_routine)(void *);
    void *arg;
};

static void *
patl_trampoline(void *thunk_vp)
{
printf("tramp!\n");
    gperftools_register_thread();
    struct Thunk *thunk_p = (struct Thunk *)thunk_vp;
    void *result = (thunk_p->start_routine)(thunk_p->arg);
    free(thunk_p);
    return result;
}

int
pthread_create(pthread_t *thread ,const pthread_attr_t *attr,
               void *(*start_routine)(void*), void *arg)
{
    static pthread_create_t real_pthread_create;

    if (!real_pthread_create)
    {
        real_pthread_create
            = (pthread_create_t)dlsym(RTLD_NEXT, "pthread_create");
        if (!real_pthread_create)
            abort(); // "impossible"
    }

    if (!gperftools_register_thread)
    {
        gperftools_register_thread
            = (gperftools_register_thread_t)dlsym(RTLD_NEXT,
                                                  "ProfilerRegisterThread");
        if (!gperftools_register_thread)
            abort(); // impossible assuming gperftools has been linked
    }

    struct Thunk *thunk_p = malloc(sizeof(struct Thunk));
    thunk_p->start_routine = start_routine;
    thunk_p->arg = arg;
    return real_pthread_create(thread, attr, patl_trampoline, thunk_p);
}
