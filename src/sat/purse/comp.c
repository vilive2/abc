#include "purse.h"
#include <assert.h>
#include <stddef.h>

int CompLearnt (const void *a, const void *b) {
    const PurseObj_t *obja = *(const PurseObj_t **)a;
    const PurseObj_t *objb = *(const PurseObj_t **)b;
    assert (obja != NULL);
    assert (objb != NULL);
    assert (obja->pData != NULL);
    assert (objb->pData != NULL);

    if (obja->pData->nLearnt < objb->pData->nLearnt)
        return -1;
    else if (obja->pData->nLearnt > objb->pData->nLearnt)
        return 1;
    return 0;
}