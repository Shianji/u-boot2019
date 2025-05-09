#ifndef __SEC_DEF__
#define __SEC_DEF__

#define CALLOUT_DATA        __attribute__((used, section(".callout.data")))
#define CALLOUT __attribute__ ((used, section(".callout.text")))
#define _CALLOUT_CLASS(fn, cmd)                            \
    static void* CMD##cmd##_##fn \
        __attribute__((used, __section__(".callout_cls." #cmd))) = (void*)fn
#define CALLOUT_CLASS(fn, cmd) _CALLOUT_CLASS(fn, cmd)
#endif
