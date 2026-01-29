#include "base/abc/abc.h"
#include "base/main/abcapis.h"
#include "base/main/main.h"
#include "misc/vec/vec.h"
#include "misc/util/abc_global.h"
#include "sat/bmc/bmc.h"
#include "purse.h"
#include <sys/resource.h>

static inline void PrintMem(const char *tag)
{
    struct rusage r;
    getrusage(RUSAGE_SELF, &r);
    printf("[MEM] %s: RSS = %.2f MB\n", tag, r.ru_maxrss / 1024.0);
}


void ParPurseSetDefaultParams ( PursePar_t *pPars) {
    assert (pPars != NULL);
    pPars->nTimeOut = ABC_INT_MAX;
    pPars->fVerbose = 0;
}

void PurseDataInit ( PurseData_t *pData) {
    assert (pData != NULL);
    pData->nSat = 0;
    pData->nFrame = 0;
    pData->nLearnt = 0;
    pData->nDecisions = 0;
    pData->nClause = 0;
    pData->score = 0;
    pData->nConflicts = 0;
    pData->nPropagations = 0;
    pData->nClk = 0;
}

void PurseMultiPropertyVerification( Abc_Ntk_t *pNtk, PursePar_t * pPars) {
    extern int Abc_NtkDarBmc3( Abc_Ntk_t * pNtk, Saig_ParBmc_t * pPars, int fOrDecomp );
    extern Abc_Ntk_t * Abc_NtkSelectPos( Abc_Ntk_t * pNtkInit, Vec_Int_t * vPoIds );

    int (*comparator)(const void *, const void *);
    // comparator = CompLearnt;
    // comparator = CompScore;
    comparator = CompFrame;
    
    Vec_Int_t *vPoIds = Vec_IntStart(1); 
    Saig_ParBmc_t Pars, * pBmcPars = &Pars;
    int fOrDecomp = 0;
    Saig_ParBmcSetDefaultParams( pBmcPars );
    PurseData_t pData;
    pBmcPars->pData = &pData;
    pBmcPars->fUseGlucose = 1;
    pBmcPars->fSilent = 1;
    Abc_Ntk_t *orgNtk;
    orgNtk = Abc_NtkDup(pNtk);
    // PrintMem("After Abc_NtkDup");

    int N = Abc_NtkPoNum(orgNtk);

    PurseObj_t *objs = ABC_ALLOC(PurseObj_t, N);
    PurseData_t *pdata = ABC_ALLOC(PurseData_t, N);
    Vec_Ptr_t *Lp = Vec_PtrStart(N);
    for(int i = 0 ; i < N ; i++) {
        // Vec_IntWriteEntry(vPoIds, 0, i);
        // pNtk = Abc_NtkDup(orgNtk);
        // pNtk = Abc_NtkSelectPos( pNtk, vPoIds);

        objs[i].status = PURSE_UNDEC;
        objs[i].propNum = i;
        // objs[i].ntk = (void *)pNtk;
        PurseDataInit(&pdata[i]);
        objs[i].pData = &pdata[i];
        Vec_PtrWriteEntry(Lp, i, &objs[i]);
    }

    abctime clkTotal = Abc_Clock();
    abctime nTimeToStop = pPars->nTimeOut ? pPars->nTimeOut * CLOCKS_PER_SEC + Abc_Clock() : 0;
    abctime clk, clkRun, clkBudget = CLOCKS_PER_SEC, clkRem = pPars->nTimeOut * CLOCKS_PER_SEC;

    int conflictBudget = 1<<10;

    int nSat = 0, nUnsat = 0;
    int minFrame = ABC_INFINITY, maxFrame = -ABC_INFINITY;
    abctime minClk = ABC_INFINITY, maxClk = -ABC_INFINITY;

    int j = 0;
    
    while (1) {
        int solved = 0;
        Vec_Ptr_t *unk_goals = Vec_PtrAlloc( Vec_PtrSize(Lp) );
        
        #ifdef DEBUG_PURSE
        #endif
        
        if (pPars->fVerbose) {
            printf("\nIteration: %d\n", j);
            printf("Params: TimeOut = %d.\n",(int)(clkBudget + CLOCKS_PER_SEC - 1) / (int)CLOCKS_PER_SEC);
            PrintStat (Lp, stdout);
        }

        minClk = ABC_INFINITY;
        maxClk = -1;
        minFrame = ABC_INFINITY;
        maxFrame = -1;
        for (int i = 0 ; i < N ; i++)  {
            minClk = minClk < objs[i].pData->nClk ? minClk : objs[i].pData->nClk;
            maxClk = maxClk > objs[i].pData->nClk ? maxClk : objs[i].pData->nClk;
            minFrame = Abc_MinInt(minFrame, (int)(objs[i].pData->nFrame));
            maxFrame = Abc_MaxInt(maxFrame, (int)(objs[i].pData->nFrame));
        }

        printf("\r%d SAT, %d UNSAT, %d UNDECIDED, ", nSat, nUnsat, N-nSat-nUnsat);
        printf("timeLimit %d sec., conflictLimt %d, ", (int)(clkBudget + CLOCKS_PER_SEC - 1) / (int)CLOCKS_PER_SEC, conflictBudget);
        printf("minFrame %d, maxFrame %d, ", minFrame, maxFrame);
        printf("minTime %9.2f sec., maxTime %9.2f sec., ", (float)minClk/(float)CLOCKS_PER_SEC, (float)maxClk/(float)CLOCKS_PER_SEC);
        printf("completed %9.2f sec.", (float)(Abc_Clock() - clkTotal) / (float)CLOCKS_PER_SEC);

        int idx;
        PurseObj_t *obj;
        Vec_PtrForEachEntry( PurseObj_t *, Lp, obj, idx ) {

            
            // pNtk = (Abc_Ntk_t *)(obj->ntk);
            Vec_IntWriteEntry(vPoIds, 0, obj->propNum);
            pNtk = Abc_NtkDup(orgNtk);
            // PrintMem("After Abc_NtkDup");
            pNtk = Abc_NtkSelectPos( pNtk, vPoIds);
            // PrintMem("After Abc_NtkSelectPos");
            
            pBmcPars->nStart = obj->pData->nFrame;
            pBmcPars->pData->propNum = obj->propNum; // Just to Debug ,TODO: Remove
            // pBmcPars->nStart = 0;
            pBmcPars->nTimeOut = (int)(clkBudget + CLOCKS_PER_SEC - 1) / (int)CLOCKS_PER_SEC;
            pBmcPars->nConfLimit = conflictBudget;
            pBmcPars->fSilent = 1;
            pBmcPars->iFrame = -1;
            PurseDataInit (pBmcPars->pData);
            
            
            clk = Abc_Clock();
            int status = Abc_NtkDarBmc3(pNtk, pBmcPars, fOrDecomp);
            // PrintMem("After BMC");
            clkRun = Abc_Clock() - clk;
            Abc_NtkDelete(pNtk);

            if (status == ABC_SAT) {
                obj->status = PURSE_SAT;
                solved++;
                nSat++;
            } else if (status == ABC_UNSAT) {
                obj->status = PURSE_UNSAT;
                solved++;
                nUnsat++;
            } else if (status == ABC_UNDEC) {
                Vec_PtrPush( unk_goals, obj);
            } else {
                Vec_PtrFree(unk_goals);
                goto finish;
            }

            obj->pData->nFrame += pBmcPars->pData->nFrame;
            // obj->pData->nSat += pBmcPars->pData->nSat;
            // obj->pData->nLearnt = pBmcPars->pData->nLearnt;
            // obj->pData->nDecisions = pBmcPars->pData->nDecisions;
            // obj->pData->nClause = pBmcPars->pData->nClause;
            // obj->pData->nConflicts = pBmcPars->pData->nConflicts;
            // obj->pData->nPropagations = pBmcPars->pData->nPropagations;
            // obj->pData->score = pBmcPars->pData->nClause == 0 ? INF : obj->pData->score + 1.0 * pBmcPars->pData->nLearnt / pBmcPars->pData->nClause;
            obj->pData->nClk += clkRun;
            clkRem -= clkRun;

            clkBudget = clkBudget < clkRem ? clkBudget : clkRem;
            
            if ( nTimeToStop && Abc_Clock() > nTimeToStop )
                break;
            if (clkBudget <= 0) break;
        }

        Vec_PtrFree(Lp);
        Lp = unk_goals;
        
        j++;


        if ( Lp->nSize == 0 ) break;
        if ( nTimeToStop && Abc_Clock() > nTimeToStop ) break;
        if (clkBudget <= 0) break;
    
        Vec_PtrSort(Lp, comparator);
        
        if (solved == 0) {
            clkBudget = clkBudget * 2;
            conflictBudget = conflictBudget * 2 < INF ? conflictBudget * 2 : conflictBudget;
        }
        clkBudget = clkBudget < clkRem ? clkBudget : clkRem;
    }

    Vec_PtrFree(Lp);
    Lp = Vec_PtrStart(N);
    minClk = ABC_INFINITY;
    maxClk = -1;
    minFrame = ABC_INFINITY;
    maxFrame = -1;
    for (int i = 0 ; i < N ; i++)  {
        Vec_PtrWriteEntry(Lp, i, &objs[i]);
        minClk = minClk < objs[i].pData->nClk ? minClk : objs[i].pData->nClk;
        maxClk = maxClk > objs[i].pData->nClk ? maxClk : objs[i].pData->nClk;
        minFrame = Abc_MinInt(minFrame, (int)(objs[i].pData->nFrame));
        maxFrame = Abc_MaxInt(maxFrame, (int)(objs[i].pData->nFrame));
    }
    Vec_PtrSort(Lp, comparator);
    // printf("\nfinally:\n");
    // PrintStat(Lp, stdout);
    printf("\n\n");
    printf("%d SAT, %d UNSAT, %d UNDECIDED\n", nSat, nUnsat, N-nSat-nUnsat);
    printf("minFrame %d, maxFrame %d\n", minFrame, maxFrame);
    printf("minTime %9.2f sec., maxTime %9.2f sec.\n", (float)minClk/(float)CLOCKS_PER_SEC, (float)maxClk/(float)CLOCKS_PER_SEC);
    

    finish:
    // for(int i = 0 ; i < N ; i++) {
    //     PurseObj_t *obj = &objs[i];
    //     pNtk = obj->ntk;
    //     Abc_NtkDelete(pNtk);
    // }
    // Abc_NtkDelete(orgNtk);
    free(objs);
    free(pdata);
    Vec_IntFree(vPoIds);
    Vec_PtrFree(Lp);

    return ;
}

/*
design: 6s306.aig
propNum: 24
nStart: 34849
nTimeOut: 64
*/