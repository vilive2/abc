#include "base/abc/abc.h"
#include "base/main/abcapis.h"
#include "base/main/main.h"
#include "misc/vec/vec.h"
#include "sat/bmc/bmc.h"
#include "purse.h"

void ParPurseSetDefaultParams ( PursePar_t *pPars) {
    assert (pPars != NULL);
    pPars->nConfLimit = 0;
    pPars->nPropLimit = 0;
    pPars->nTimeOut = ABC_INT_MAX;
    pPars->fVerbose = 0;
    pPars->pLogFileName = NULL;
    pPars->fUseGlucose = 0;
    pPars->fUseSatoko = 0;
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
}

void PurseMultiPropertyVerification( Abc_Ntk_t *pNtk, PursePar_t * pPars) {
    extern int Abc_NtkDarBmc3( Abc_Ntk_t * pNtk, Saig_ParBmc_t * pPars, int fOrDecomp );
    extern Abc_Ntk_t * Abc_NtkSelectPos( Abc_Ntk_t * pNtkInit, Vec_Int_t * vPoIds );

    int (*comparator)(const void *, const void *);
    // comparator = CompLearnt;
    comparator = CompScore;
    // comparator = CompFrame;
    
    Vec_Int_t *vPoIds = Vec_IntStart(1); 
    Saig_ParBmc_t Pars, * pBmcPars = &Pars;
    int fOrDecomp = 0;
    Saig_ParBmcSetDefaultParams( pBmcPars );
    PurseData_t pData;
    pBmcPars->pData = &pData;
    pBmcPars->nTimeOut = pPars->nTimeOut;
    pBmcPars->pLogFileName = pPars->pLogFileName;
    pBmcPars->fUseSatoko = pPars->fUseSatoko;
    pBmcPars->fUseGlucose = pPars->fUseGlucose;
    pBmcPars->fVerbose ^= pPars->fVerbose;
    pBmcPars->fSilent = 1;
    Abc_Ntk_t *orgNtk;
    orgNtk = Abc_NtkDup(pNtk);

    int N = Abc_NtkPoNum(orgNtk);
    int gates = 100;
    int size = Abc_NtkNodeNum(orgNtk);


    PurseObj_t *objs = ABC_ALLOC(PurseObj_t, N);
    PurseData_t *pdata = ABC_ALLOC(PurseData_t, N);
    Vec_Ptr_t *Lp = Vec_PtrStart(N);
    for(int i = 0 ; i < N ; i++) {
        Vec_IntWriteEntry(vPoIds, 0, i);
        pNtk = Abc_NtkDup(orgNtk);
        pNtk = Abc_NtkSelectPos( pNtk, vPoIds);

        objs[i].status = PURSE_UNDEC;
        objs[i].propNum = i;
        objs[i].ntk = (void *)pNtk;
        PurseDataInit(&pdata[i]);
        objs[i].pData = &pdata[i];
        Vec_PtrWriteEntry(Lp, i, &objs[i]);
    }

    int CB, PB;
    
    if (size < gates) {
        CB = pPars->nConfLimit;
        PB = pPars->nPropLimit;
    } else if (size < gates * 10) {
        CB = pPars->nConfLimit * ((size % gates) + 1);
        PB = pPars->nPropLimit * ((size % gates) + 1);
    } else {
        CB = pPars->nConfLimit * 11;
        PB = pPars->nPropLimit * 11;
    }

    int j = 0;
    double T = 0.0;

    while (1) {
        int solved = 0;
        Vec_Ptr_t *unk_goals = Vec_PtrAlloc( Vec_PtrSize(Lp) );
        
        #ifdef DEBUG_PURSE
        printf("\nIteration: %d\n", j);
        printf("Params: ConfLimit = %d. PropLimit = %d. TimeOut = %.1lf.\n", CB, PB, pPars->nTimeOut - T);
        PrintStat (Lp);
        #endif

        int idx;
        PurseObj_t *obj;
        Vec_PtrForEachEntry( PurseObj_t *, Lp, obj, idx ) {

            clock_t start_time = clock();

            pNtk = (Abc_Ntk_t *)(obj->ntk);

            pBmcPars->nStart = obj->pData->nFrame;
            // pBmcPars->nStart = 0;
            pBmcPars->nConfLimit = CB;
            pBmcPars->nPropLimit = PB;
            pBmcPars->nTimeOut = pPars->nTimeOut - T;
            pBmcPars->fSilent = 1;
            pBmcPars->iFrame = -1;
            PurseDataInit (pBmcPars->pData);
            int status = Abc_NtkDarBmc3(pNtk, pBmcPars, fOrDecomp);

            if (status == ABC_SAT) {
                obj->status = PURSE_SAT;
                solved++;
            } else if (status == ABC_UNSAT) {
                obj->status = PURSE_UNSAT;
                solved++;
            } else if (status == ABC_UNDEC) {
                Vec_PtrPush( unk_goals, obj);
            } else {
                Vec_PtrFree(unk_goals);
                goto finish;
            }

            obj->pData->nFrame += pBmcPars->pData->nFrame;
            obj->pData->nSat += pBmcPars->pData->nSat;
            obj->pData->nLearnt = pBmcPars->pData->nLearnt;
            obj->pData->nDecisions = pBmcPars->pData->nDecisions;
            obj->pData->nClause = pBmcPars->pData->nClause;
            obj->pData->nConflicts = pBmcPars->pData->nConflicts;
            obj->pData->nPropagations = pBmcPars->pData->nPropagations;
            obj->pData->score = pBmcPars->pData->nClause == 0 ? INF : obj->pData->score + 1.0 * pBmcPars->pData->nLearnt / pBmcPars->pData->nClause;

            clock_t end_time = clock();
            T += (double)(end_time - start_time) / CLOCKS_PER_SEC;

            if (T > pPars->nTimeOut)
                break;
        }

        Vec_PtrFree(Lp);
        Lp = unk_goals;
        
        j++;

        if (T < pPars->nTimeOut && Lp->nSize != 0) {
            Vec_PtrSort(Lp, comparator);
        } else {
            break;
        }

        if (solved == 0) {
            CB = CB * 2;
            PB = PB * 2;
        }
    }

    Vec_PtrFree(Lp);
    Lp = Vec_PtrStart(N);
    for (int i = 0 ; i < N ; i++)  Vec_PtrWriteEntry(Lp, i, &objs[i]);
    Vec_PtrSort(Lp, comparator);
    printf("finally:\n");
    PrintStat(Lp);


    finish:
    for(int i = 0 ; i < N ; i++) {
        PurseObj_t *obj = &objs[i];
        pNtk = obj->ntk;
        Abc_NtkDelete(pNtk);
    }
    Abc_NtkDelete(orgNtk);
    free(objs);
    free(pdata);
    Vec_IntFree(vPoIds);
    Vec_PtrFree(Lp);

    return ;
}