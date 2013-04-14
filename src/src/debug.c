#include "frgg.h"

//copy code from redis
static void *get_mcontext_eip(ucontext_t *uc) {
#if defined(__APPLE__) && !defined(MAC_OS_X_VERSION_10_6)
    /* OSX < 10.6 */
    #if defined(__x86_64__)
    return (void*) uc->uc_mcontext->__ss.__rip;
    #elif defined(__i386__)
    return (void*) uc->uc_mcontext->__ss.__eip;
    #else
    return (void*) uc->uc_mcontext->__ss.__srr0;
    #endif
#elif defined(__APPLE__) && defined(MAC_OS_X_VERSION_10_6)
    /* OSX >= 10.6 */
    #if defined(_STRUCT_X86_THREAD_STATE64) && !defined(__i386__)
    return (void*) uc->uc_mcontext->__ss.__rip;
    #else
    return (void*) uc->uc_mcontext->__ss.__eip;
    #endif
#elif defined(__linux__)
    /* Linux */
    #if defined(__i386__)
    return (void*) uc->uc_mcontext.gregs[14]; /* Linux 32 */
    #elif defined(__X86_64__) || defined(__x86_64__)
    return (void*) uc->uc_mcontext.gregs[16]; /* Linux 64 */
    #elif defined(__ia64__) /* Linux IA64 */
    return (void*) uc->uc_mcontext.sc_ip;
    #endif
#else
    return NULL;
#endif
}

void stack_trace(ucontext_t *uc)
{
    void *trace[100];
    int trace_size = 0, i;
    char **symbols;

    trace_size = backtrace(trace, 100);
    
    if (get_mcontext_eip(uc) != NULL)
        trace[1] = get_mcontext_eip(uc);
    
    symbols = backtrace_symbols(trace, trace_size);
    for (i = 0; i < trace_size; i++) {
        ERROR1("%s", symbols[i]);
    }

    free(symbols);
}


static void sigsegv_handler(int sig, siginfo_t *info, void *secret)
{
    ucontext_t *uc = (ucontext_t*)secret;
    ERROR1("Agent crashed by signal: %d", sig);
    ERROR("---- STACK TRACE");
    stack_trace(uc);
    ERROR("---- END STACK TRACE");
    ERROR("Agent will shutdown now...");
}

static void sigterm_handler(int sig)
{
    ERROR("Received SIGTERM, shutdown...");
}

void setup_signal_handlers()
{
    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_handler = sigterm_handler;
    sigaction(SIGTERM, &act, NULL);

    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_NODEFER | SA_RESETHAND | SA_SIGINFO;
    act.sa_sigaction = sigsegv_handler;
    sigaction(SIGSEGV, &act, NULL);
    sigaction(SIGBUS, &act, NULL);
    sigaction(SIGFPE, &act, NULL);
    sigaction(SIGILL, &act, NULL);
}


