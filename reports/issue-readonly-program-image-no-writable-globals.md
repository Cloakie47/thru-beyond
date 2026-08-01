# Program image (including .bss/.data) is mapped read-only — writable globals fault

## Environment

- thru CLI 0.3.2+54058649, C SDK v0.3.2, toolchain v0.3.2 (riscv64-unknown-elf-gcc 15.2.0)
- Alphanet (`https://rpc.alphanet.thru.org`), node 0.0.0-local+599daf60

## Summary

The program image is mapped into the segment type named
`TSDK_SEG_TYPE_READONLY_DATA`, and that includes `.data` and `.bss`. Any
write to a global or static variable faults with
`TN_RUNTIME_TXN_ERR_VM_FAILED`. Ordinary C code that uses a static buffer or
a mutable global — which is most existing C code — compiles cleanly and then
faults at runtime on its first store.

## Steps to reproduce

```c
#include <thru-sdk/c/tn_sdk.h>

static uchar buf[64];

TSDK_ENTRYPOINT_FN void start(void) {
    ((volatile uchar *)buf)[0] = 1;   /* faults */
    tsdk_return(TSDK_SUCCESS);
}
```

Build, deploy, execute.

## Expected

Either writable .data/.bss (the standard C execution model), or a
compile/link-time error when a program contains writable static data, or at
minimum prominent documentation that all mutable state must live in account
data, the stack, or a grown anonymous segment.

## Actual

Runtime fault: `vm_error=-767 (TN_RUNTIME_TXN_ERR_VM_FAILED)`, with
`user_error` echoing a fault-related register value rather than a
recognizable error code. Nothing at build time warns about the writable
sections.

## Supporting detail: .bss inflates the program image

Because .bss is stored literally in the image, declaring
`static uchar a[8256]; static uchar b[16384];` grew a 488-byte program binary
to 32,840 bytes — zero bytes are uploaded, stored on chain, and paid for in
deployment costs, for arrays that cannot even be written.

## Suggested fix

Fail the link (or `thru program create`'s image validation) when the ELF
contains a non-empty writable PT_LOAD segment, with a message pointing to
account data / stack / anonymous segments as the supported alternatives.
