#include "base/abc/abc.h"
#include "base/main/abcapis.h"
#include "base/main/main.h"
#include "misc/vec/vec.h"
#include "misc/util/abc_global.h"
#include "sat/bmc/bmc.h"
#include "poem.h"
#include "comp.h"
#include <sys/resource.h>
#include <queue>
#include <vector>
#include <algorithm>

ABC_NAMESPACE_HEADER_START

extern int Abc_NtkDarBmc3( Abc_Ntk_t * pNtk, Saig_ParBmc_t * pPars, int fOrDecomp );
extern Abc_Ntk_t * Abc_NtkSelectPos( Abc_Ntk_t * pNtkInit, Vec_Int_t * vPoIds );
extern Abc_Ntk_t * Abc_NtkDarLatchSweep( Abc_Ntk_t * pNtk, int fLatchConst, int fLatchEqual, int fSaveNames, int fUseMvSweep, int nFramesSymb, int nFramesSatur, int fVerbose, int fVeryVerbose );

ABC_NAMESPACE_HEADER_END

extern void print_stat(std::vector<PoemObj_t*> &props);

static inline void PrintMem(const char *tag)
{
    struct rusage r;
    getrusage(RUSAGE_SELF, &r);
    printf("[MEM] %s: RSS = %.2f MB\n", tag, r.ru_maxrss / 1024.0);
}

void ParPoemSetDefaultParams ( PoemPar_t *pPars) {
    assert (pPars != NULL);
    pPars->nTimeOut = ABC_INT_MAX;
    pPars->fVerbose = 0;
    pPars->staticOrdering = 0;
}

void PoemDataInit ( PoemData_t *pData) {
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

void PoemMultiPropertyVerification( Abc_Ntk_t *pNtk, PoemPar_t * pPars) {

    // int (*comparator)(const void *, const void *);
    // comparator = CompLearnt;
    // comparator = CompScore;
    // comparator = CompFrame;
    // comparator = CompFPS;
    
    Vec_Int_t *vPoIds = Vec_IntStart(1); 
    Saig_ParBmc_t Pars, * pBmcPars = &Pars;
    int fOrDecomp = 0;
    Saig_ParBmcSetDefaultParams( pBmcPars );
    PoemData_t pData;
    pBmcPars->pData = &pData;
    pBmcPars->fUseGlucose = 1;
    pBmcPars->fSilent = 1;
    Abc_Ntk_t *orgNtk;
    orgNtk = Abc_NtkDup(pNtk);
    // PrintMem("After Abc_NtkDup");

    int N = Abc_NtkPoNum(orgNtk);

    PoemObj_t *objs = ABC_ALLOC(PoemObj_t, N);
    PoemData_t *pdata = ABC_ALLOC(PoemData_t, N);
    // Vec_Ptr_t *Lp = Vec_PtrStart(N);
    std::priority_queue<PoemObj_t*, std::vector<PoemObj_t*>, CompFPS> pq;
    std::vector<PoemObj_t*> props;
    for(int i = 0 ; i < N ; i++) {
        Vec_IntWriteEntry(vPoIds, 0, i);
        pNtk = Abc_NtkDup(orgNtk);
        pNtk = Abc_NtkSelectPos( pNtk, vPoIds);
        int fLatchConst  =   1;
        int fLatchEqual  =   1;
        int fSaveNames   =   1;
        int fUseMvSweep  =   0;
        int nFramesSymb  =   1;
        int nFramesSatur = 512;
        int fVerbose     =   0;
        int fVeryVerbose =   0;
        pNtk = Abc_NtkDarLatchSweep( pNtk, fLatchConst, fLatchEqual, fSaveNames, fUseMvSweep, nFramesSymb, nFramesSatur, fVerbose, fVeryVerbose );

        objs[i].status = POEM_UNDEC;
        objs[i].propNum = i;
        objs[i].ntk = (void *)pNtk;
        PoemDataInit(&pdata[i]);
        objs[i].pData = &pdata[i];
        // Vec_PtrWriteEntry(Lp, i, &objs[i]);
        pq.push(&objs[i]);
        props.push_back(&objs[i]);
    }

    abctime clkTotal = Abc_Clock();
    abctime nTimeToStop = pPars->nTimeOut ? pPars->nTimeOut * CLOCKS_PER_SEC + Abc_Clock() : 0;
    abctime clk, clkRun, clkBudget = CLOCKS_PER_SEC, clkRem = pPars->nTimeOut * CLOCKS_PER_SEC;

    int nSat = 0, nUnsat = 0;
    int minFrame = ABC_INFINITY, maxFrame = -ABC_INFINITY;
    abctime minClk = ABC_INFINITY, maxClk = -ABC_INFINITY;

    int j = 0;
    
    while (1) {
        PoemObj_t* best = pq.top();
        pq.pop();
        pNtk = (Abc_Ntk_t *)(best->ntk);

        if (pPars->fVerbose) {
            printf(" prop: %d \n", best->propNum);
        }


        abctime clkDiff = maxClk - best->pData->nClk;
        clkBudget = clkDiff > clkBudget ? clkDiff : maxClk - minClk < clkBudget ? 2*clkBudget : clkBudget;
        
        clkBudget = clkBudget < clkRem ? clkBudget : clkRem;
        
            
        pBmcPars->nStart = best->pData->nFrame;
        pBmcPars->pData->propNum = best->propNum; // Just to Debug ,TODO: Remove
        // pBmcPars->nStart = 0;
        pBmcPars->nTimeOut = (0LL + clkBudget + CLOCKS_PER_SEC - 1) / CLOCKS_PER_SEC;
        // pBmcPars->nConfLimit = conflictBudget;
        pBmcPars->fSilent = 1;
        pBmcPars->iFrame = -1;
        PoemDataInit (pBmcPars->pData);
            
            
        clk = Abc_Clock();
        int status = Abc_NtkDarBmc3(pNtk, pBmcPars, fOrDecomp);
        // PrintMem("After BMC");
        clkRun = Abc_Clock() - clk;
        
        best->pData->nFrame += pBmcPars->pData->nFrame;
        // obj->pData->nSat += pBmcPars->pData->nSat;
        // obj->pData->nLearnt = pBmcPars->pData->nLearnt;
        // obj->pData->nDecisions = pBmcPars->pData->nDecisions;
        // obj->pData->nClause = pBmcPars->pData->nClause;
        // obj->pData->nConflicts = pBmcPars->pData->nConflicts;
        // obj->pData->nPropagations = pBmcPars->pData->nPropagations;
        // obj->pData->score = pBmcPars->pData->nClause == 0 ? INF : obj->pData->score + 1.0 * pBmcPars->pData->nLearnt / pBmcPars->pData->nClause;
        best->pData->nClk += clkRun;
        clkRem -= clkRun;
        
        if (status == ABC_SAT) {
            best->status = POEM_SAT;
            nSat++;
            // Vec_PtrDrop(Lp, 0);
        } else if (status == ABC_UNSAT) {
            best->status = POEM_UNSAT;
            nUnsat++;
            // Vec_PtrDrop(Lp, 0);
        } else if (status == ABC_UNDEC) {
            // Vec_PtrPush( unk_goals, obj);
            pq.push(best);
        } else {
            goto finish;
        }



        minClk = ABC_INFINITY;
        maxClk = -1;
        minFrame = ABC_INFINITY;
        maxFrame = -1;
        for (int i = 0 ; i < N ; i++)  {
            if (objs[i].status == POEM_UNDEC) {
                minClk = minClk < objs[i].pData->nClk ? minClk : objs[i].pData->nClk;
                maxClk = maxClk > objs[i].pData->nClk ? maxClk : objs[i].pData->nClk;
            }
            minFrame = Abc_MinInt(minFrame, (int)(objs[i].pData->nFrame));
            maxFrame = Abc_MaxInt(maxFrame, (int)(objs[i].pData->nFrame));
        }

        if (pPars->fVerbose) {
            printf("\r%d SAT, %d UNSAT, %d UNDECIDED, ", nSat, nUnsat, N-nSat-nUnsat);
            printf("iteration %d, ", j);
            printf("timeLimit %d sec., ", (0LL + clkBudget + CLOCKS_PER_SEC - 1) / CLOCKS_PER_SEC);
            printf("minFrame %d, maxFrame %d, ", minFrame, maxFrame);
            printf("minTime %9.2f sec., maxTime %9.2f sec., ", (float)minClk/(float)CLOCKS_PER_SEC, (float)maxClk/(float)CLOCKS_PER_SEC);
            printf("completed %9.2f sec.", (float)(Abc_Clock() - clkTotal) / (float)CLOCKS_PER_SEC);
        }
            
        if ( nTimeToStop && Abc_Clock() > nTimeToStop )
            break;
        if (clkBudget <= 0) break;

        j++;


        if ( pq.empty() ) break;
        if ( nTimeToStop && Abc_Clock() > nTimeToStop ) break;
        if (clkBudget <= 0) break;
    
        // Vec_PtrSort(Lp, comparator);
    }

    print_stat (props);

    finish:
    for(int i = 0 ; i < N ; i++) {
        PoemObj_t *obj = &objs[i];
        pNtk = (Abc_Ntk_t *)obj->ntk;
        Abc_NtkDelete(pNtk);
    }
    Abc_NtkDelete(orgNtk);
    free(objs);
    free(pdata);
    Vec_IntFree(vPoIds);
    // Vec_PtrFree(Lp);

    return ;
}

void PoemMultiPropertyVerificationALG1( Abc_Ntk_t *pNtk, PoemPar_t * pPars) {
    extern int Abc_NtkDarBmc3( Abc_Ntk_t * pNtk, Saig_ParBmc_t * pPars, int fOrDecomp );
    extern Abc_Ntk_t * Abc_NtkSelectPos( Abc_Ntk_t * pNtkInit, Vec_Int_t * vPoIds );

    int (*comparator)(const void *, const void *);
    comparator = CompFrame;
    
    Vec_Int_t *vPoIds = Vec_IntStart(1); 
    Saig_ParBmc_t Pars, * pBmcPars = &Pars;
    int fOrDecomp = 0;
    Saig_ParBmcSetDefaultParams( pBmcPars );
    PoemData_t pData;
    pBmcPars->pData = &pData;
    pBmcPars->fUseGlucose = 1;
    pBmcPars->fSilent = 1;
    Abc_Ntk_t *orgNtk;
    orgNtk = Abc_NtkDup(pNtk);

    int N = Abc_NtkPoNum(orgNtk);

    PoemObj_t *objs = ABC_ALLOC(PoemObj_t, N);
    PoemData_t *pdata = ABC_ALLOC(PoemData_t, N);
    Vec_Ptr_t *Lp = Vec_PtrStart(N);
    std::vector<PoemObj_t*> props;
    for(int i = 0 ; i < N ; i++) {
        Vec_IntWriteEntry(vPoIds, 0, i);
        pNtk = Abc_NtkDup(orgNtk);
        pNtk = Abc_NtkSelectPos( pNtk, vPoIds);
        int fLatchConst  =   1;
        int fLatchEqual  =   1;
        int fSaveNames   =   1;
        int fUseMvSweep  =   0;
        int nFramesSymb  =   1;
        int nFramesSatur = 512;
        int fVerbose     =   0;
        int fVeryVerbose =   0;
        pNtk = Abc_NtkDarLatchSweep( pNtk, fLatchConst, fLatchEqual, fSaveNames, fUseMvSweep, nFramesSymb, nFramesSatur, fVerbose, fVeryVerbose );

        objs[i].status = POEM_UNDEC;
        objs[i].propNum = i;
        objs[i].ntk = (void *)pNtk;
        PoemDataInit(&pdata[i]);
        objs[i].pData = &pdata[i];
        Vec_PtrWriteEntry(Lp, i, &objs[i]);
        props.push_back(&objs[i]);
    }

    abctime clkTotal = Abc_Clock();
    abctime nTimeToStop = pPars->nTimeOut ? pPars->nTimeOut * CLOCKS_PER_SEC + Abc_Clock() : 0;
    abctime clk, clkRun, clkBudget = CLOCKS_PER_SEC, clkRem = pPars->nTimeOut * CLOCKS_PER_SEC;

    int nSat = 0, nUnsat = 0;
    int minFrame = ABC_INFINITY, maxFrame = -ABC_INFINITY;
    abctime minClk = ABC_INFINITY, maxClk = -ABC_INFINITY;

    int j = 0;
    
    while (1) {
        int solved = 0;
        
        Vec_Ptr_t *unk_goals = Vec_PtrAlloc( Vec_PtrSize(Lp) );
        
        if (pPars->fVerbose) {
            printf("\nIteration: %d\n", j);
            printf("Params: TimeOut = %d.\n",(0LL + clkBudget + CLOCKS_PER_SEC - 1) / CLOCKS_PER_SEC);
            print_stat (props);
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
        printf("minFrame %d, maxFrame %d, ", minFrame, maxFrame);
        printf("minTime %9.2f sec., maxTime %9.2f sec., ", (float)minClk/(float)CLOCKS_PER_SEC, (float)maxClk/(float)CLOCKS_PER_SEC);
        printf("completed %9.2f sec.", (float)(Abc_Clock() - clkTotal) / (float)CLOCKS_PER_SEC);

        int idx;
        PoemObj_t *obj;
        Vec_PtrForEachEntry( PoemObj_t *, Lp, obj, idx ) {

            
            pNtk = (Abc_Ntk_t *)(obj->ntk);
            
            pBmcPars->nStart = obj->pData->nFrame;
            // pBmcPars->nStart = 0;
            pBmcPars->nTimeOut = (0LL + clkBudget + CLOCKS_PER_SEC - 1) / CLOCKS_PER_SEC;
            pBmcPars->fSilent = 1;
            pBmcPars->iFrame = -1;
            PoemDataInit (pBmcPars->pData);
            
            
            clk = Abc_Clock();
            int status = Abc_NtkDarBmc3(pNtk, pBmcPars, fOrDecomp);
            clkRun = Abc_Clock() - clk;

            if (status == ABC_SAT) {
                obj->status = POEM_SAT;
                solved++;
                nSat++;
            } else if (status == ABC_UNSAT) {
                obj->status = POEM_UNSAT;
                solved++;
                nUnsat++;
            } else if (status == ABC_UNDEC) {
                Vec_PtrPush( unk_goals, obj);
            } else {
                Vec_PtrFree(unk_goals);
                goto finish;
            }

            obj->pData->nFrame += pBmcPars->pData->nFrame;
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
        }
        clkBudget = clkBudget < clkRem ? clkBudget : clkRem;
    }

    print_stat(props);

    

    finish:
    for(int i = 0 ; i < N ; i++) {
        PoemObj_t *obj = &objs[i];
        pNtk = (Abc_Ntk_t *)obj->ntk;
        Abc_NtkDelete(pNtk);
    }
    // Abc_NtkDelete(orgNtk);
    free(objs);
    free(pdata);
    Vec_IntFree(vPoIds);
    Vec_PtrFree(Lp);

    return ;
}


void PoemMultiPropertyVerificationALG0( Abc_Ntk_t *pNtk, PoemPar_t * pPars) {
    extern int Abc_NtkDarBmc3( Abc_Ntk_t * pNtk, Saig_ParBmc_t * pPars, int fOrDecomp );
    extern Abc_Ntk_t * Abc_NtkSelectPos( Abc_Ntk_t * pNtkInit, Vec_Int_t * vPoIds );

    Vec_Int_t *vPoIds = Vec_IntStart(1); 
    Saig_ParBmc_t Pars, * pBmcPars = &Pars;
    int fOrDecomp = 0;
    Saig_ParBmcSetDefaultParams( pBmcPars );
    PoemData_t pData;
    pBmcPars->pData = &pData;
    pBmcPars->fUseGlucose = 1;
    pBmcPars->fSilent = 1;
    Abc_Ntk_t *orgNtk;
    orgNtk = Abc_NtkDup(pNtk);

    int N = Abc_NtkPoNum(orgNtk);

    PoemObj_t *objs = ABC_ALLOC(PoemObj_t, N);
    PoemData_t *pdata = ABC_ALLOC(PoemData_t, N);
    std::vector<PoemObj_t*> props;
    for(int i = 0 ; i < N ; i++) {
        Vec_IntWriteEntry(vPoIds, 0, i);
        pNtk = Abc_NtkDup(orgNtk);
        pNtk = Abc_NtkSelectPos( pNtk, vPoIds);
        int fLatchConst  =   1;
        int fLatchEqual  =   1;
        int fSaveNames   =   1;
        int fUseMvSweep  =   0;
        int nFramesSymb  =   1;
        int nFramesSatur = 512;
        int fVerbose     =   0;
        int fVeryVerbose =   0;
        pNtk = Abc_NtkDarLatchSweep( pNtk, fLatchConst, fLatchEqual, fSaveNames, fUseMvSweep, nFramesSymb, nFramesSatur, fVerbose, fVeryVerbose );

        objs[i].status = POEM_UNDEC;
        objs[i].propNum = i;
        objs[i].ntk = (void *)pNtk;
        PoemDataInit(&pdata[i]);
        objs[i].pData = &pdata[i];
        props.push_back(&objs[i]);
    }

    abctime clkTotal = Abc_Clock();
    abctime nTimeToStop = pPars->nTimeOut ? pPars->nTimeOut * CLOCKS_PER_SEC + Abc_Clock() : 0;
    abctime clk, clkRun, clkBudget = (pPars->nTimeOut * CLOCKS_PER_SEC) / N, clkRem = pPars->nTimeOut * CLOCKS_PER_SEC;

    int nSat = 0, nUnsat = 0;
    int minFrame = ABC_INFINITY, maxFrame = -ABC_INFINITY;
    
    int i = 0;
    
    while (i < N) {

        if (pPars->fVerbose) {
            printf("\rIteration: %d ", i);
            printf("Params: TimeOut = %lld. ",(0LL + clkBudget + CLOCKS_PER_SEC - 1) / CLOCKS_PER_SEC);    

            minFrame = ABC_INFINITY;
            maxFrame = -1;
            for (int i = 0 ; i < N ; i++)  {
                minFrame = Abc_MinInt(minFrame, (int)(objs[i].pData->nFrame));
                maxFrame = Abc_MaxInt(maxFrame, (int)(objs[i].pData->nFrame));
            }

            printf(" %d SAT, %d UNSAT, %d UNDECIDED, ", nSat, nUnsat, N-nSat-nUnsat);
            printf("minFrame %d, maxFrame %d, ", minFrame, maxFrame);
            printf("completed %9.2f sec.", (float)(Abc_Clock() - clkTotal) / (float)CLOCKS_PER_SEC);

        }

        PoemObj_t *obj = props[i];
            
        pNtk = (Abc_Ntk_t *)(obj->ntk);
        
        pBmcPars->nStart = obj->pData->nFrame;
        // pBmcPars->nStart = 0;
        pBmcPars->nTimeOut = (0LL + clkBudget + CLOCKS_PER_SEC - 1) / CLOCKS_PER_SEC;
        pBmcPars->fSilent = 1;
        pBmcPars->iFrame = -1;
        PoemDataInit (pBmcPars->pData);
        
        
        clk = Abc_Clock();
        int status = Abc_NtkDarBmc3(pNtk, pBmcPars, fOrDecomp);
        clkRun = Abc_Clock() - clk;

        if (status == ABC_SAT) {
            obj->status = POEM_SAT;
            nSat++;
        } else if (status == ABC_UNSAT) {
            obj->status = POEM_UNSAT;
            nUnsat++;
        } else if (status == ABC_UNDEC) {
        
        } else {
            goto finish;
        }

        obj->pData->nFrame += pBmcPars->pData->nFrame;
        obj->pData->nClk += clkRun;
        clkRem -= clkRun;

        clkBudget = clkBudget < clkRem ? clkBudget : clkRem;
        
        if ( nTimeToStop && Abc_Clock() > nTimeToStop )
            break;
        if (clkBudget <= 0) break;
        
        i++;

        if ( nTimeToStop && Abc_Clock() > nTimeToStop ) break;
    
        clkBudget = clkBudget < clkRem ? clkBudget : clkRem;
    }

    print_stat(props);

    

    finish:
    for(int i = 0 ; i < N ; i++) {
        PoemObj_t *obj = &objs[i];
        pNtk = (Abc_Ntk_t *)obj->ntk;
        Abc_NtkDelete(pNtk);
    }
    // Abc_NtkDelete(orgNtk);
    free(objs);
    free(pdata);
    Vec_IntFree(vPoIds);

    return ;
}

void SequentialMultiPropertyVerification( Abc_Ntk_t *pNtk, PoemPar_t * pPars) {

    
    Vec_Int_t *vPoIds = Vec_IntStart(1); 
    Saig_ParBmc_t Pars, * pBmcPars = &Pars;
    int fOrDecomp = 0;
    Saig_ParBmcSetDefaultParams( pBmcPars );
    PoemData_t pData;
    pBmcPars->pData = &pData;
    pBmcPars->fUseGlucose = 1;
    pBmcPars->fSilent = 1;
    Abc_Ntk_t *orgNtk;
    orgNtk = Abc_NtkDup(pNtk);
    // PrintMem("After Abc_NtkDup");

    int N = Abc_NtkPoNum(orgNtk);

    PoemObj_t *objs = ABC_ALLOC(PoemObj_t, N);
    PoemData_t *pdata = ABC_ALLOC(PoemData_t, N);
    std::vector<PoemObj_t*> props;
    for(int i = 0 ; i < N ; i++) {
        Vec_IntWriteEntry(vPoIds, 0, i);
        pNtk = Abc_NtkDup(orgNtk);
        pNtk = Abc_NtkSelectPos( pNtk, vPoIds);
        int fLatchConst  =   1;
        int fLatchEqual  =   1;
        int fSaveNames   =   1;
        int fUseMvSweep  =   0;
        int nFramesSymb  =   1;
        int nFramesSatur = 512;
        int fVerbose     =   0;
        int fVeryVerbose =   0;
        pNtk = Abc_NtkDarLatchSweep( pNtk, fLatchConst, fLatchEqual, fSaveNames, fUseMvSweep, nFramesSymb, nFramesSatur, fVerbose, fVeryVerbose );

        objs[i].status = POEM_UNDEC;
        objs[i].propNum = i;
        objs[i].ntk = (void *)pNtk;
        objs[i].ntkSize = Abc_NtkPiNum(pNtk) + Abc_NtkLatchNum(pNtk) + Abc_NtkNodeNum(pNtk);
        PoemDataInit(&pdata[i]);
        objs[i].pData = &pdata[i];
        props.push_back(&objs[i]);
    }

    sort(props.begin(), props.end(), CompNtkSize());

    abctime nTimeToStop = pPars->nTimeOut ? pPars->nTimeOut * CLOCKS_PER_SEC + Abc_Clock() : 0;
    abctime clk, clkRun, clkRem = pPars->nTimeOut * CLOCKS_PER_SEC;

    int nSat = 0, nUnsat = 0;
    int minFrame = ABC_INFINITY, maxFrame = -ABC_INFINITY;
    abctime minClk = ABC_INFINITY, maxClk = -ABC_INFINITY;

    int j = 0;
    
    while (j < N) {
        PoemObj_t* best = props[j];
        pNtk = (Abc_Ntk_t *)(best->ntk);

        if (pPars->fVerbose) {
            printf(" prop: %d \n", best->propNum);
        }

        pBmcPars->nStart = 0;
        pBmcPars->pData->propNum = best->propNum; // Just to Debug ,TODO: Remove
        pBmcPars->nTimeOut = (0LL + clkRem + CLOCKS_PER_SEC - 1) / CLOCKS_PER_SEC;
        pBmcPars->fSilent = 1;
        pBmcPars->iFrame = -1;
        PoemDataInit (pBmcPars->pData);
            
            
        clk = Abc_Clock();
        int status = Abc_NtkDarBmc3(pNtk, pBmcPars, fOrDecomp);
        clkRun = Abc_Clock() - clk;
        
        best->pData->nFrame += pBmcPars->pData->nFrame;
        best->pData->nClk += clkRun;
        clkRem -= clkRun;
        
        if (status == ABC_SAT) {
            best->status = POEM_SAT;
            nSat++;
        } else if (status == ABC_UNSAT) {
            best->status = POEM_UNSAT;
            nUnsat++;
        } else if (status == ABC_UNDEC) {
            
        } else {
            goto finish;
        }

            
        if ( nTimeToStop && Abc_Clock() > nTimeToStop )
            break;
        j++;
    }

    print_stat(props);

    finish:
    // for(int i = 0 ; i < N ; i++) {
    //     PoemObj_t *obj = &objs[i];
    //     pNtk = obj->ntk;
    //     Abc_NtkDelete(pNtk);
    // }
    // Abc_NtkDelete(orgNtk);
    free(objs);
    free(pdata);
    Vec_IntFree(vPoIds);

    return ;
}

/*
design: 6s306.aig
propNum: 24
nStart: 34849
nTimeOut: 64
*/