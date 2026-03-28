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
#include <random>

ABC_NAMESPACE_HEADER_START

extern int Abc_NtkDarBmc3( Abc_Ntk_t * pNtk, Saig_ParBmc_t * pPars, int fOrDecomp );
extern Abc_Ntk_t * Abc_NtkSelectPos( Abc_Ntk_t * pNtkInit, Vec_Int_t * vPoIds );
extern Abc_Ntk_t * Abc_NtkDarLatchSweep( Abc_Ntk_t * pNtk, int fLatchConst, int fLatchEqual, int fSaveNames, int fUseMvSweep, int nFramesSymb, int nFramesSatur, int fVerbose, int fVeryVerbose );
ABC_NAMESPACE_HEADER_END

extern void print_stat(std::vector<PoemObj_t*> &props);
extern void print_log (PoemMan *pMan);

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

void PoemManInit ( PoemMan *pMan, int N, int nTimeOut, int clkBudget) {
    pMan->it = 0;
    pMan->maxFrame = 0;
    pMan->solved = 0;
    pMan->N = N;
    pMan->minClk = 0;
    pMan->maxClk = 0;
    pMan->clkTotal = Abc_Clock();
    pMan->nTimeToStop = nTimeOut * CLOCKS_PER_SEC + Abc_Clock() ;
    pMan->clkBudget = clkBudget;
    pMan->clkRem = nTimeOut * CLOCKS_PER_SEC;
}

void PoemManSeparatePorperties (PoemMan *pMan, Abc_Ntk_t *orgNtk) {
    Vec_Int_t *vPoIds = Vec_IntStart(1); 

    Abc_Ntk_t *pNtk;
    pMan->N = Abc_NtkPoNum(orgNtk);

    pMan->objs = ABC_ALLOC(PoemObj_t, pMan->N);
    pMan->pdata = ABC_ALLOC(PoemData_t, pMan->N);
    for(int i = 0 ; i < pMan->N ; i++) {
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

        pMan->objs[i].status = POEM_UNDEC;
        pMan->objs[i].propNum = i;
        pMan->objs[i].ntk = (void *)pNtk;
        PoemDataInit(&(pMan->pdata[i]));
        pMan->objs[i].pData = &(pMan->pdata[i]);
    }

    Vec_IntFree(vPoIds);
}

/**
 * @brief Multi-property verification using priority-based scheduling for POEM
 * 
 * This function performs verification of multiple properties on a sequential circuit
 * using a dynamic priority scheduling approach. It manages verification time across
 * multiple properties, prioritizing those that are likely to be solved quickly.
 * 
 * @param pNtk Input network (sequential circuit) to verify
 * @param pPars POEM parameters (timeout, verbosity settings)
 * 
 * @note This function modifies the internal state of properties and deallocates
 *       duplicated networks. Results are stored in the property objects.
 * 
 * @details The algorithm:
 * 1. Duplicates the original network and separates properties
 * 2. Initializes a priority queue of properties (sorted by estimated solving time)
 * 3. Iteratively selects the highest priority property for BMC verification
 * 4. Updates property status (SAT/UNSAT/UNDEC) and time statistics
 * 5. Recalculates priorities and continues until timeout or all properties solved
 * 
 * @warning This function assumes the input network has at least one property
 *          and will fail if pPars->nTimeOut is not positive
 */
void PoemMultiPropertyVerification( Abc_Ntk_t *pNtk, PoemPar_t * pPars) {

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

    PoemMan pMan;

    PoemManSeparatePorperties (&pMan, orgNtk);

    std::priority_queue<PoemObj_t*, std::vector<PoemObj_t*>, CompFPS> pq;
    std::vector<PoemObj_t*> props;
    for(int i = 0 ; i < N ; i++) {
        pq.push(&(pMan.objs[i]));
        props.push_back(&(pMan.objs[i]));
    }

    assert (pPars->nTimeOut > 0);
    PoemManInit (&pMan, N, pPars->nTimeOut, CLOCKS_PER_SEC);
    
    for (;;pMan.it++) {
        PoemObj_t* best = pq.top();
        pq.pop();
        pNtk = (Abc_Ntk_t *)(best->ntk);

        if (pPars->fVerbose) {
            printf("\rprop: %d ", best->propNum);

            print_log (&pMan);
        }

        if ( Abc_NtkLatchNum(pNtk) == 0 )
        {
            best->status = POEM_SOLVED;
            pMan.solved++;
            continue;
        }


        abctime clkDiff = pMan.maxClk - best->pData->nClk;
        pMan.clkBudget = clkDiff > pMan.clkBudget ? clkDiff : pMan.maxClk - pMan.minClk < pMan.clkBudget ? 2*pMan.clkBudget : pMan.clkBudget;
        
        pMan.clkBudget = pMan.clkBudget < pMan.clkRem ? pMan.clkBudget : pMan.clkRem;
        
            
        pBmcPars->nStart = best->pData->nFrame;
        pBmcPars->pData->propNum = best->propNum; // Just to Debug ,TODO: Remove
        // pBmcPars->nStart = 0;
        pBmcPars->nTimeOut = (0LL + pMan.clkBudget + CLOCKS_PER_SEC - 1) / CLOCKS_PER_SEC;
        // pBmcPars->nConfLimit = conflictBudget;
        pBmcPars->fSilent = 1;
        pBmcPars->iFrame = -1;
        PoemDataInit (pBmcPars->pData);
            
            
        abctime clk = Abc_Clock();
        int status = Abc_NtkDarBmc3(pNtk, pBmcPars, fOrDecomp);
        // PrintMem("After BMC");
        abctime clkRun = Abc_Clock() - clk;
        
        best->pData->nFrame += pBmcPars->pData->nFrame;
        best->pData->nClk += clkRun;
        pMan.clkRem -= clkRun;
        
        if (status == ABC_SAT) {
            best->status = POEM_SAT;
            // nSat++;
            pMan.solved++;
            // Vec_PtrDrop(Lp, 0);
        } else if (status == ABC_UNSAT) {
            best->status = POEM_UNSAT;
            // nUnsat++;
            pMan.solved++;
            // Vec_PtrDrop(Lp, 0);
        } else if (status == ABC_UNDEC) {
            // Vec_PtrPush( unk_goals, obj);
            pq.push(best);
        } else {
            goto finish;
        }



        pMan.minClk = ABC_INFINITY;
        pMan.maxClk = -1;
        for (int i = 0 ; i < N ; i++)  {
            if (props[i]->status == POEM_UNDEC) {
                pMan.minClk = std::min(pMan.minClk, props[i]->pData->nClk);
                pMan.maxClk = std::max(pMan.maxClk, props[i]->pData->nClk);
            }
            pMan.maxFrame = std::max(pMan.maxFrame, (int)(props[i]->pData->nFrame));
        }

        if ( Abc_Clock() > pMan.nTimeToStop )
            break;
        if (pMan.clkBudget <= 0) break;

        if ( pq.empty() ) break;
    }

    print_stat (props);

    finish:
    for(int i = 0 ; i < N ; i++) {
        pNtk = (Abc_Ntk_t *)props[i]->ntk;
        Abc_NtkDelete(pNtk);
    }
    Abc_NtkDelete(orgNtk);
    free(pMan.objs);
    free(pMan.pdata);
    // Vec_PtrFree(Lp);

    return ;
}

void PoemMultiPropertyVerificationALG1( Abc_Ntk_t *pNtk, PoemPar_t * pPars) {

    int (*comparator)(const void *, const void *);
    comparator = CompFrame;
    
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

    PoemMan pMan;
    PoemManSeparatePorperties (&pMan, orgNtk);

    Vec_Ptr_t *Lp = Vec_PtrStart(N);
    std::vector<PoemObj_t*> props;
    for(int i = 0 ; i < N ; i++) {
        Vec_PtrWriteEntry(Lp, i, &(pMan.objs[i]));
        props.push_back(&(pMan.objs[i]));
    }

    PoemManInit (&pMan, N, pPars->nTimeOut, CLOCKS_PER_SEC);
    
    for (;;pMan.it++) {
        int solved = 0;
        
        Vec_Ptr_t *unk_goals = Vec_PtrAlloc( Vec_PtrSize(Lp) );
        
        if (pPars->fVerbose) {
            pMan.minClk = ABC_INFINITY;
            pMan.maxClk = -1;
            pMan.maxFrame = -1;
            for (int i = 0 ; i < N ; i++)  {
                if (props[i]->status == POEM_UNDEC) {
                    pMan.minClk = std::min(pMan.minClk, props[i]->pData->nClk);
                    pMan.maxClk = std::max( pMan.maxClk, props[i]->pData->nClk);
                }
                pMan.maxFrame = std::max(pMan.maxFrame, (int)(props[i]->pData->nFrame));
            }
            printf("\r");
            print_log (&pMan);
        }

        int idx;
        PoemObj_t *obj;
        Vec_PtrForEachEntry( PoemObj_t *, Lp, obj, idx ) {

            
            pNtk = (Abc_Ntk_t *)(obj->ntk);

            if ( Abc_NtkLatchNum(pNtk) == 0 )
            {
                obj->status = POEM_SOLVED;
                pMan.solved++;
                continue;
            }
            
            pBmcPars->nStart = obj->pData->nFrame;
            // pBmcPars->nStart = 0;
            pBmcPars->nTimeOut = (0LL + pMan.clkBudget + CLOCKS_PER_SEC - 1) / CLOCKS_PER_SEC;
            pBmcPars->fSilent = 1;
            pBmcPars->iFrame = -1;
            PoemDataInit (pBmcPars->pData);
            
            
            abctime clk = Abc_Clock();
            int status = Abc_NtkDarBmc3(pNtk, pBmcPars, fOrDecomp);
            abctime clkRun = Abc_Clock() - clk;

            if (status == ABC_SAT) {
                obj->status = POEM_SAT;
                solved++;
                pMan.solved++;
            } else if (status == ABC_UNSAT) {
                obj->status = POEM_UNSAT;
                solved++;
                pMan.solved++;
            } else if (status == ABC_UNDEC) {
                Vec_PtrPush( unk_goals, obj);
            } else {
                Vec_PtrFree(unk_goals);
                goto finish;
            }

            obj->pData->nFrame += pBmcPars->pData->nFrame;
            obj->pData->nClk += clkRun;
            pMan.clkRem -= clkRun;

            pMan.clkBudget = std::min(pMan.clkBudget, pMan.clkRem);
            
            if (Abc_Clock() > pMan.nTimeToStop )
                break;
            if (pMan.clkBudget <= 0) break;
        }

        Vec_PtrFree(Lp);
        Lp = unk_goals;
        


        if ( Lp->nSize == 0 ) break;
        if ( Abc_Clock() > pMan.nTimeToStop ) break;
        if (pMan.clkBudget <= 0) break;
    
        Vec_PtrSort(Lp, comparator);
        
        if (solved == 0) {
            pMan.clkBudget = pMan.clkBudget * 2;
        }
        pMan.clkBudget = std::min(pMan.clkBudget, pMan.clkRem);
    }

    print_stat(props);

    

    finish:
    for(int i = 0 ; i < N ; i++) {
        pNtk = (Abc_Ntk_t *)props[i]->ntk;
        Abc_NtkDelete(pNtk);
    }
    Abc_NtkDelete(orgNtk);
    free(pMan.objs);
    free(pMan.pdata);
    Vec_PtrFree(Lp);

    return ;
}


void PoemMultiPropertyVerificationALG0( Abc_Ntk_t *pNtk, PoemPar_t * pPars) {
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

    PoemMan pMan;
    PoemManSeparatePorperties (&pMan, orgNtk);

    std::vector<PoemObj_t*> props;
    for(int i = 0 ; i < N ; i++) {
        props.push_back(&(pMan.objs[i]));
    }

    PoemManInit (&pMan, N, pPars->nTimeOut, (pPars->nTimeOut * CLOCKS_PER_SEC + N-1) / N);
    
    pMan.maxClk = pMan.clkBudget;
    for (;pMan.it < N;pMan.it++) {

        if (pPars->fVerbose) {
            
            pMan.maxFrame = -1;
            for (int i = 0 ; i < N ; i++)  {
                pMan.maxFrame = std::max(pMan.maxFrame, (int)(props[i]->pData->nFrame));
            }

            printf("\rprop: %d ", props[pMan.it]->propNum);
            print_log (&pMan);
        }

        PoemObj_t *obj = props[pMan.it];
            
        pNtk = (Abc_Ntk_t *)(obj->ntk);

        if ( Abc_NtkLatchNum(pNtk) == 0 )
        {
            obj->status = POEM_SOLVED;
            pMan.solved++;
            continue;
        }
        
        pBmcPars->nStart = obj->pData->nFrame;
        // pBmcPars->nStart = 0;
        pBmcPars->nTimeOut = (0LL + pMan.clkBudget + CLOCKS_PER_SEC - 1) / CLOCKS_PER_SEC;
        pBmcPars->fSilent = 1;
        pBmcPars->iFrame = -1;
        PoemDataInit (pBmcPars->pData);
        
        
        abctime clk = Abc_Clock();
        int status = Abc_NtkDarBmc3(pNtk, pBmcPars, fOrDecomp);
        abctime clkRun = Abc_Clock() - clk;

        if (status == ABC_SAT) {
            obj->status = POEM_SAT;
            pMan.solved++;
        } else if (status == ABC_UNSAT) {
            obj->status = POEM_UNSAT;
            pMan.solved++;
        } else if (status == ABC_UNDEC) {
        
        } else {
            goto finish;
        }

        obj->pData->nFrame += pBmcPars->pData->nFrame;
        obj->pData->nClk += clkRun;
        pMan.clkRem -= clkRun;

        pMan.clkBudget = std::min(pMan.clkBudget, pMan.clkRem);
        
        if (Abc_Clock() > pMan.nTimeToStop )
            break;
        if (pMan.clkBudget <= 0) break;
    }

    print_stat(props);

    

    finish:
    for(int i = 0 ; i < N ; i++) {
        pNtk = (Abc_Ntk_t *)props[i]->ntk;
        Abc_NtkDelete(pNtk);
    }
    Abc_NtkDelete(orgNtk);
    free(pMan.objs);
    free(pMan.pdata);

    return ;
}

void PoemMultiPropertyVerificationALGR( Abc_Ntk_t *pNtk, PoemPar_t * pPars) {
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

    PoemMan pMan;
    PoemManSeparatePorperties (&pMan, orgNtk);

    std::vector<PoemObj_t*> props;
    for(int i = 0 ; i < N ; i++) {
        props.push_back(&(pMan.objs[i]));
    }

    // Order Property : Random
    // Create a random device and generator
    std::random_device rd;
    std::mt19937 gen(rd());
    // Shuffle the vector
    std::shuffle(props.begin(), props.end(), gen);

    PoemManInit (&pMan, N, pPars->nTimeOut, (pPars->nTimeOut * CLOCKS_PER_SEC));
    
    for (;pMan.it < N;pMan.it++) {

        if (pPars->fVerbose) {
            
            pMan.maxFrame = -1;
            pMan.maxClk = 0;
            for (int i = 0 ; i < N ; i++)  {
                pMan.maxFrame = std::max(pMan.maxFrame, (int)(props[i]->pData->nFrame));
                pMan.maxClk = std::max(pMan.maxClk, props[i]->pData->nClk);
            }

            printf("\rprop: %d ", props[pMan.it]->propNum);
            print_log (&pMan);
        }

        PoemObj_t *obj = props[pMan.it];
            
        pNtk = (Abc_Ntk_t *)(obj->ntk);

        if ( Abc_NtkLatchNum(pNtk) == 0 )
        {
            obj->status = POEM_SOLVED;
            pMan.solved++;
            continue;
        }
        
        pBmcPars->nStart = obj->pData->nFrame;
        pBmcPars->nTimeOut = (0LL + pMan.clkBudget + CLOCKS_PER_SEC - 1) / CLOCKS_PER_SEC;
        pBmcPars->fSilent = 1;
        pBmcPars->iFrame = -1;
        PoemDataInit (pBmcPars->pData);
        
        
        abctime clk = Abc_Clock();
        int status = Abc_NtkDarBmc3(pNtk, pBmcPars, fOrDecomp);
        abctime clkRun = Abc_Clock() - clk;

        if (status == ABC_SAT) {
            obj->status = POEM_SAT;
            pMan.solved++;
        } else if (status == ABC_UNSAT) {
            obj->status = POEM_UNSAT;
            pMan.solved++;
        } else if (status == ABC_UNDEC) {
        
        } else {
            goto finish;
        }

        obj->pData->nFrame += pBmcPars->pData->nFrame;
        obj->pData->nClk += clkRun;
        pMan.clkRem -= clkRun;

        pMan.clkBudget = pMan.clkRem;
        
        if (Abc_Clock() > pMan.nTimeToStop )
            break;
        if (pMan.clkBudget <= 0) break;
    }

    print_stat(props);

    

    finish:
    for(int i = 0 ; i < N ; i++) {
        pNtk = (Abc_Ntk_t *)props[i]->ntk;
        Abc_NtkDelete(pNtk);
    }
    Abc_NtkDelete(orgNtk);
    free(pMan.objs);
    free(pMan.pdata);

    return ;
}

void SequentialMultiPropertyVerification( Abc_Ntk_t *pNtk, PoemPar_t * pPars) {

    
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
    PoemMan pMan;
    PoemManSeparatePorperties (&pMan, orgNtk);

    std::vector<PoemObj_t*> props;
    for(int i = 0 ; i < N ; i++) {
        props.push_back(&(pMan.objs[i]));
    }

    sort(props.begin(), props.end(), CompNtkSize());

    PoemManInit (&pMan, N, pPars->nTimeOut, pPars->nTimeOut * CLOCKS_PER_SEC);
    
    for (;pMan.it < N ; pMan.it++) {
        PoemObj_t* best = props[pMan.it];
        pNtk = (Abc_Ntk_t *)(best->ntk);

        if (pPars->fVerbose) {
            pMan.maxClk = 0;
            pMan.maxFrame = 0;
            for (int i = 0 ; i < N ; i++) {
                pMan.maxFrame = std::max(pMan.maxFrame, (int)props[i]->pData->nFrame);
                pMan.maxClk = std::max(pMan.maxClk, props[i]->pData->nClk);
            }
            printf("\rprop: %d ", best->propNum);
            print_log (&pMan);
        }

        if ( Abc_NtkLatchNum(pNtk) == 0 )
        {
            best->status = POEM_SOLVED;
            pMan.solved++;
            continue;
        }

        pBmcPars->nStart = 0;
        pBmcPars->pData->propNum = best->propNum; // Just to Debug ,TODO: Remove
        pBmcPars->nTimeOut = (0LL + pMan.clkRem + CLOCKS_PER_SEC - 1) / CLOCKS_PER_SEC;
        pBmcPars->fSilent = 1;
        pBmcPars->iFrame = -1;
        PoemDataInit (pBmcPars->pData);
            
            
        abctime clk = Abc_Clock();
        int status = Abc_NtkDarBmc3(pNtk, pBmcPars, fOrDecomp);
        abctime clkRun = Abc_Clock() - clk;
        
        best->pData->nFrame += pBmcPars->pData->nFrame;
        best->pData->nClk += clkRun;
        pMan.clkRem -= clkRun;
        
        if (status == ABC_SAT) {
            best->status = POEM_SAT;
            pMan.solved++;
        } else if (status == ABC_UNSAT) {
            best->status = POEM_UNSAT;
            pMan.solved++;
        } else if (status == ABC_UNDEC) {
            
        } else {
            goto finish;
        }

        pMan.clkBudget = pMan.clkRem;
        
        if (Abc_Clock() > pMan.nTimeToStop )
            break;
        if (pMan.clkRem <= 0) break;
    }

    print_stat(props);

    finish:
    for(int i = 0 ; i < N ; i++) {
        pNtk = (Abc_Ntk_t *)props[i]->ntk;
        Abc_NtkDelete(pNtk);
    }
    Abc_NtkDelete(orgNtk);
    free(pMan.objs);
    free(pMan.pdata);

    return ;
}

/*
design: 6s306.aig
propNum: 24
nStart: 34849
nTimeOut: 64
*/