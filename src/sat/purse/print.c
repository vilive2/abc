#include "misc/vec/vec.h"
#include "purse.h"
#include <stdio.h>

void PrintStat( Vec_Ptr_t *objs) {
    printf("%-8s %-8s %-6s %-12s %-10s %-8s\n", "prop.", "#frame", "#sat", "#decisions", "#learnt", "status");
    PurseObj_t *obj;
    int i;
    Vec_PtrForEachEntry(PurseObj_t *, objs, obj, i) {
        printf("%-8d %-8d %-6d %-12d %-10d %-8s\n", 
            obj->propNum, 
            obj->pData->nFrame, 
            obj->pData->nSat,
            obj->pData->nDecisions,
            obj->pData->nLearnt,
            obj->status == PURSE_SAT ? "SAT" : obj->status == PURSE_UNSAT ? "UNSAT" : "UNDEC"
        );
    }
}