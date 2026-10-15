/* issn: kernel event flags (src/runtime_eventflag.c). */
#define _GNU_SOURCE
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include "../src/runtime_eventflag.c"

uintptr_t runtime_lookup(const RuntimeExport *table, size_t count, const char *name) {
    (void)table; (void)count; (void)name;
    return 0;
}

enum { ESRCH_=ERR(3), EPERM_=ERR(1), EBUSY_=ERR(16), EINVAL_=ERR(22), ETIMEDOUT_=ERR(60),
       ECANCELED_=ERR(85), EACCES_=ERR(13) };

typedef struct { uint64_t flag, bits, result; uint32_t mode, timeout; int use_timeout; int32_t r; } Wait;
static void *waiter(void *arg) {
    Wait *w=arg;
    w->r=flag_wait(w->flag,w->bits,w->mode,&w->result,w->use_timeout ? &w->timeout : NULL);
    return NULL;
}
static unsigned waiting(uint64_t id) {
    pthread_mutex_lock(&lock);
    Flag *f=find(id); unsigned n=f ? f->waiters : 0;
    pthread_mutex_unlock(&lock);
    return n;
}
static void until_waiting(uint64_t id, unsigned n) {
    for (int i=0;i<500 && waiting(id)<n;++i) usleep(1000);
    assert(waiting(id)==n);
}

int main(void) {
    uint64_t single=0, multi=0, result=0;
    assert(flag_create(NULL,"x",0,0,NULL)==EINVAL_ && flag_create(&single,NULL,0,0,NULL)==EINVAL_);
    assert(flag_create(&single,"x",ATTR_SINGLE|ATTR_MULTI,0,NULL)==EINVAL_);
    assert(flag_create(&single,"single",ATTR_SINGLE,0x1,NULL)==0 && single);
    assert(flag_create(&multi,"multi",ATTR_MULTI,0,NULL)==0 && multi && multi!=single);

    /* Modes: one of AND/OR, at most one clear. */
    assert(flag_poll(single,0x1,0,&result)==EINVAL_);
    assert(flag_poll(single,0x1,WAIT_AND|WAIT_OR,&result)==EINVAL_);
    assert(flag_poll(single,0x1,WAIT_OR|WAIT_CLEAR_ALL|WAIT_CLEAR_PAT,&result)==EINVAL_);
    assert(flag_poll(single,0,WAIT_OR,&result)==EINVAL_);
    assert(flag_poll(12345,0x1,WAIT_OR,&result)==ESRCH_);

    /* AND needs every bit, OR any; the result is the pattern before the clear. */
    assert(flag_set(single,0x6)==0); /* pattern 0x7 */
    assert(flag_poll(single,0x9,WAIT_AND,&result)==EBUSY_ && result==0x7);
    assert(flag_poll(single,0x9,WAIT_OR,&result)==0 && result==0x7);
    assert(flag_poll(single,0x3,WAIT_AND|WAIT_CLEAR_PAT,&result)==0 && result==0x7);
    assert(flag_poll(single,0x7,WAIT_OR,&result)==0 && result==0x4); /* 0x3 cleared */
    assert(flag_poll(single,0x4,WAIT_OR|WAIT_CLEAR_ALL,&result)==0 && result==0x4);
    assert(flag_poll(single,0xff,WAIT_OR,&result)==EBUSY_ && result==0);
    /* sceKernelClearEventFlag keeps only the bits given (AND). */
    assert(flag_set(single,0xf0)==0 && flag_clear(single,0x30)==0);
    assert(flag_poll(single,0xff,WAIT_OR,&result)==0 && result==0x30);
    assert(flag_clear(single,0)==0);

    /* A timed wait that ends without the bits: ETIMEDOUT, no time left. */
    uint32_t timeout=20000;
    assert(flag_wait(single,0x1,WAIT_OR,&result,&timeout)==ETIMEDOUT_ && timeout==0 && result==0);
    timeout=0;
    assert(flag_wait(single,0x1,WAIT_OR,&result,&timeout)==ETIMEDOUT_);

    /* A waiter woken by flag_set, with time left; a second waiter on a SINGLE flag is EPERM. */
    Wait w={.flag=single,.bits=0x4,.mode=WAIT_OR|WAIT_CLEAR_PAT,.timeout=2000000,.use_timeout=1};
    pthread_t t;
    assert(pthread_create(&t,NULL,waiter,&w)==0);
    until_waiting(single,1);
    assert(flag_wait(single,0x1,WAIT_OR,&result,NULL)==EPERM_);
    assert(flag_poll(single,0x1,WAIT_OR,&result)==EPERM_);
    assert(flag_set(single,0x5)==0);
    assert(pthread_join(t,NULL)==0);
    assert(w.r==0 && w.result==0x5 && w.timeout>0 && w.timeout<2000000);
    assert(flag_poll(single,0xff,WAIT_OR,&result)==0 && result==0x1); /* 0x4 cleared by the waiter */

    /* MULTI: two waiters for different bits, each woken by its own bit, AND waits for both. */
    Wait a={.flag=multi,.bits=0x1,.mode=WAIT_OR}, b={.flag=multi,.bits=0x6,.mode=WAIT_AND};
    pthread_t ta, tb;
    assert(pthread_create(&ta,NULL,waiter,&a)==0 && pthread_create(&tb,NULL,waiter,&b)==0);
    until_waiting(multi,2);
    assert(flag_set(multi,0x1|0x2)==0);
    assert(pthread_join(ta,NULL)==0 && a.r==0 && (a.result & 0x1));
    until_waiting(multi,1); /* b still needs 0x4 */
    assert(flag_set(multi,0x4)==0);
    assert(pthread_join(tb,NULL)==0 && b.r==0 && b.result==0x7);

    /* Cancel: waits end with ECANCELED, the pattern is replaced, the count says how many. */
    Wait c={.flag=multi,.bits=0x100,.mode=WAIT_OR};
    assert(pthread_create(&t,NULL,waiter,&c)==0);
    until_waiting(multi,1);
    int32_t woken=-1;
    assert(flag_cancel(multi,0x80,&woken)==0 && woken==1);
    assert(pthread_join(t,NULL)==0 && c.r==ECANCELED_);
    assert(flag_poll(multi,0xff,WAIT_OR,&result)==0 && result==0x80);

    /* Delete with a waiter: it ends with EACCES; the handle is gone. */
    Wait d={.flag=multi,.bits=0x100,.mode=WAIT_OR};
    assert(pthread_create(&t,NULL,waiter,&d)==0);
    until_waiting(multi,1);
    assert(flag_delete(multi)==0);
    assert(pthread_join(t,NULL)==0 && d.r==EACCES_);
    assert(flag_set(multi,0x1)==ESRCH_ && flag_delete(multi)==ESRCH_);
    assert(flag_delete(single)==0 && flag_delete(single)==ESRCH_);
    puts("PASS: event flags (AND/OR, clear modes, timeout, single/multi waiters, cancel, delete)");
    return 0;
}
