#include "misc/vec/vec.h"
#include "poem.h"
#include <stdio.h>
#include <vector>

void PrintStat( Vec_Ptr_t *objs, FILE *fp) {
    fprintf(fp, "%-8s %-8s %-6s %-12s %-10s %-10s %-15s %-15s %-10s %-8s %-11s\n", "prop.", "#frame", "#sat", "#decisions", "#learnt", "#clause", "#conflicts", "#propagations", "score", "status", "time");
    PoemObj_t *obj;
    int i;
    Vec_PtrForEachEntry(PoemObj_t *, objs, obj, i) {
        fprintf(fp, "%-8d %-8d %-6d %-12d %-10d %-10d %-15d %-15d %-10.2lf %-8s %-9.2f\n", 
            obj->propNum, 
            obj->pData->nFrame, 
            obj->pData->nSat,
            obj->pData->nDecisions,
            obj->pData->nLearnt,
            obj->pData->nClause,
            obj->pData->nConflicts,
            obj->pData->nPropagations,
            obj->pData->score,
            obj->status == POEM_SAT ? "SAT" : obj->status == POEM_UNSAT ? "UNSAT" : "UNDEC",
            (float)(obj->pData->nClk)/(float)(CLOCKS_PER_SEC) 
        );
    }
}

void print_stat(std::vector<PoemObj_t*> &props) {
    int N = props.size();
    int S = 0;
    float avg_frame = 0;
    unsigned int max_frame = 0;
    float time_unsolved = 0;
    float time_solved = 0;

    for (int i = 0 ; i < N ; i++) {
        if (props[i]->status == POEM_UNDEC) {
            time_unsolved += props[i]->pData->nClk;
            avg_frame += props[i]->pData->nFrame;
        } else {
            S++;
            time_solved += props[i]->pData->nClk;
        }

        max_frame = std::max(max_frame, props[i]->pData->nFrame);
    }

    if (N - S == 0) 
        avg_frame = INF;
    else
        avg_frame = avg_frame / (N - S);
    
    time_solved = time_solved / CLOCKS_PER_SEC;
    time_unsolved = time_unsolved / CLOCKS_PER_SEC;

    printf("\n%-10s %-15s %-22s %-11s %-16s\n", "Solved", "Time(solved)", "Avg. Frame(unsolved)", "Max Frame", "Time(unsolved)");
    printf("%-10d %-15.2f %-22.2f %-11d %-16.2f\n", S, time_solved, avg_frame, max_frame, time_unsolved);
}