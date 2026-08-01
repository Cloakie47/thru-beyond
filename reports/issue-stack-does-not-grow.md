# VM never grows the stack: any frame over ~4KB faults on every path

## Environment

- thru CLI 0.3.2+54058649, C SDK v0.3.2, toolchain v0.3.2 (riscv64-unknown-elf-gcc 15.2.0)
- Alphanet (`https://rpc.alphanet.thru.org`), node 0.0.0-local+599daf60

## Summary

The C SDK's entry stub maps exactly one 4KB stack page before calling
`start()` (`entrypoint.S`: `set_anonymous_segment_sz(sp - 4096)`), and the VM
does not grow the stack on demand. Any function whose frame pushes total
stack usage past 4,096 bytes faults with `TN_RUNTIME_TXN_ERR_VM_FAILED` —
even on code paths that never touch the large buffer, because the frame is
allocated in the function prologue.

## Steps to reproduce

Minimal program:

```c
#include <thru-sdk/c/tn_sdk.h>
#include <thru-sdk/c/tn_sdk_syscall.h>

static __attribute__((noinline)) void use(volatile uchar *p) { p[0] = 1; }

TSDK_ENTRYPOINT_FN void start(void) {
    uchar buf[8256];          /* frame > 4KB -> prologue overflows the stack */
    use(buf);
    tsdk_return(TSDK_SUCCESS);
}
```

Build with the standard SDK makefile, deploy, execute any instruction.

## Expected

Either the stack grows on demand, or documentation of the initial size, or
a clear diagnostic ("stack overflow: grow the segment with
tsys_set_anonymous_segment_sz"). The VM memory-layout spec documents the
stack segment's 16MB size limit and downward growth direction but is silent
on the initial mapped size and the growth mechanism — a reader would
reasonably assume a usable stack well above 4KB.

## Actual

```
Error: Transaction failed (execution_result=0xFFFFFFFFFFFFFFFD,
vm_error=-767 (TN_RUNTIME_TXN_ERR_VM_FAILED), user_error=0x50000FFDF90)
```

The `user_error` field contains a raw stack-segment address (here ~8.3KB
below the stack top, i.e. the first access beyond the single mapped page),
which is easy to misread as a program-defined error code.

## Workaround

Call `tsys_set_anonymous_segment_sz((void*)(0x050001000000UL - pages*4096))`
before using large frames. (Measured cost: 512 CU for the syscall plus
4,096 CU per newly mapped page.)

## Suggested fix

Document the 1-page initial stack prominently in the C program guide, and/or
emit a distinguishable stack-overflow error code instead of a raw fault
address.

*Filed as https://github.com/Unto-Labs/thru/issues/37*
