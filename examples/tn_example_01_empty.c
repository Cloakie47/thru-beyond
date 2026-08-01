#include <thru-sdk/c/tn_sdk.h>

/* Example 01: the smallest possible Thru program.
   Does nothing and terminates successfully. */

TSDK_ENTRYPOINT_FN void start(void) {
    tsdk_return(TSDK_SUCCESS);
}
