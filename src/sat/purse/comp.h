#ifndef COMP_H
#define COMP_H

#include "purse.h"
#include <assert.h>
#include <stddef.h>
#include <math.h>

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

int CompScore (const void *a, const void *b) {
    const PurseObj_t *obja = *(const PurseObj_t **)a;
    const PurseObj_t *objb = *(const PurseObj_t **)b;
    assert (obja != NULL);
    assert (objb != NULL);
    assert (obja->pData != NULL);
    assert (objb->pData != NULL);

    if (fabs(obja->pData->score - objb->pData->score) < EPS)
        return 0;
    else if (obja->pData->score < objb->pData->score)
        return -1;
    else 
        return 1;
}

int CompFrame (const void *a, const void *b) {
    const PurseObj_t *obja = *(const PurseObj_t **)a;
    const PurseObj_t *objb = *(const PurseObj_t **)b;
    assert (obja != NULL);
    assert (objb != NULL);
    assert (obja->pData != NULL);
    assert (objb->pData != NULL);

    if (obja->pData->nFrame > objb->pData->nFrame)
        return -1;
    else if (obja->pData->nFrame < objb->pData->nFrame)
        return 1;

    return 0;
}


// int CompFPS (const void *a, const void *b) {
//     const PurseObj_t *obja = *(const PurseObj_t **)a;
//     const PurseObj_t *objb = *(const PurseObj_t **)b;
//     assert (obja != NULL);
//     assert (objb != NULL);
//     assert (obja->pData != NULL);
//     assert (objb->pData != NULL);

//     long long v1 = (long long)obja->pData->nFrame *
//                    (long long)objb->pData->nClk;

//     long long v2 = (long long)objb->pData->nFrame *
//                    (long long)obja->pData->nClk;

//     if (obja->pData->nClk == 0) v1 = INF;
//     if (objb->pData->nClk == 0) v2 = INF;

//     if (v1 > v2)
//         return -1;
//     else if (v1 < v2)
//         return 1;

//     return 0;
// }

struct CompFPS {
    bool operator()(const PurseObj_t* a, const PurseObj_t* b) const {
        long long v1 = (long long)a->pData->nFrame * b->pData->nClk;
        long long v2 = (long long)b->pData->nFrame * a->pData->nClk;

        if (a->pData->nClk == 0) v1 = INF;
        if (b->pData->nClk == 0) v2 = INF;

        return v1 < v2;  // max heap (larger ratio first)
    }
};

#endif