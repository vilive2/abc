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
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <string>
#include <numeric>

ABC_NAMESPACE_HEADER_START

extern Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
extern void Aig_ManStop( Aig_Man_t * p );
extern int Abc_NtkDarBmc3( Abc_Ntk_t * pNtk, Saig_ParBmc_t * pPars, int fOrDecomp );
extern int Abc_NtkDarBmc3Continue( BmcState *state, Aig_Man_t * pMan, Abc_Ntk_t * pNtk, Saig_ParBmc_t * pPars, int fOrDecomp );
extern Abc_Ntk_t * Abc_NtkSelectPos( Abc_Ntk_t * pNtkInit, Vec_Int_t * vPoIds );
extern Abc_Ntk_t * Abc_NtkDarLatchSweep( Abc_Ntk_t * pNtk, int fLatchConst, int fLatchEqual, int fSaveNames, int fUseMvSweep, int nFramesSymb, int nFramesSatur, int fVerbose, int fVeryVerbose );
ABC_NAMESPACE_HEADER_END

extern void print_stat(std::vector<PoemObj_t*> &props, char *logFilename = NULL, char *ntkName = NULL);
extern void print_log (PoemMan *pMan);

int Solve (BmcState *state, Aig_Man_t * pMan, Abc_Ntk_t * pNtk, Saig_ParBmc_t * pPars, int fOrDecomp);

static inline void PrintMem(const char *tag)
{
    struct rusage r;
    getrusage(RUSAGE_SELF, &r);
    printf("[MEM] %s: RSS = %.2f MB\n", tag, r.ru_maxrss / 1024.0);
}

void ParPoemSetDefaultParams ( PoemPar_t *pPars) {
    assert (pPars != NULL);
    pPars->nTimeOut = ABC_INT_MAX;
    pPars->nMemGB = 0;
    pPars->fVerbose = 0;
    pPars->staticOrdering = 0;
    pPars->logFilename = NULL;
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
    pData->nSolved = 0;
    pData->lastSolvedAt = Abc_Clock();
}

void PoemManInit ( PoemMan *pMan, int N, int nTimeOut, abctime clkBudget) {
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

void PoemMultiPropertyVerificationALG2( Abc_Ntk_t *pNtk, PoemPar_t * pPars) {

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
    std::vector<BmcState *> bmcstate(N);
    std::vector<Aig_Man_t *> aigman(N);
    // std::vector<Saig_ParBmc_t *> bmcpar(N);
    for(int i = 0 ; i < N ; i++) {
        pMan.objs[i].clkBudget = CLOCKS_PER_SEC;
        pq.push(&(pMan.objs[i]));
        props.push_back(&(pMan.objs[i]));
        aigman[i] = Abc_NtkToDar( (Abc_Ntk_t *)pMan.objs[i].ntk, 0, 1 );
        bmcstate[i] = createBmcState (aigman[i], pBmcPars);
    }

    assert (pPars->nTimeOut > 0);
    PoemManInit (&pMan, N, pPars->nTimeOut, CLOCKS_PER_SEC);
    
    for (;;pMan.it++) {
        PoemObj_t* best = pq.top();
        pq.pop();
        pNtk = (Abc_Ntk_t *)(best->ntk);
        
        pMan.clkBudget = std::min(best->clkBudget, pMan.clkRem);
        best->clkBudget *= 2;
        pMan.memLimit = pBmcPars->nMemLimit = (1LL*pPars->nMemGB*1024*1024*1024) / (N - pMan.solved);
    
        if (pPars->fVerbose) {
            printf("\rprop: %d ", best->propNum);

            print_log (&pMan);
        }

        
            
        pBmcPars->nStart = best->pData->nFrame;
        pBmcPars->pData->propNum = best->propNum; // Just to Debug ,TODO: Remove
        // pBmcPars->nStart = 0;
        pBmcPars->nTimeOut = (0LL + pMan.clkBudget + CLOCKS_PER_SEC - 1) / CLOCKS_PER_SEC;
        // pBmcPars->nConfLimit = conflictBudget;
        // pBmcPars->fSilent = 1;
        pBmcPars->iFrame = -1;
        PoemDataInit (pBmcPars->pData);
            
            
        abctime clk = Abc_Clock();
        int status = Solve(bmcstate[best->propNum], aigman[best->propNum], pNtk, pBmcPars, fOrDecomp);
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
            Abc_NtkDelete(pNtk);
            deleteBmcState(bmcstate[best->propNum]);
            Aig_ManStop(aigman[best->propNum]);
        } else if (status == ABC_UNSAT) {
            best->status = POEM_UNSAT;
            // nUnsat++;
            pMan.solved++;
            // Vec_PtrDrop(Lp, 0);
            Abc_NtkDelete(pNtk);
            deleteBmcState(bmcstate[best->propNum]);
            Aig_ManStop(aigman[best->propNum]);
        } else if (status == ABC_UNDEC) {
            // Vec_PtrPush( unk_goals, obj);
            pq.push(best);
            resetBmcState (bmcstate[best->propNum], aigman[best->propNum], pBmcPars, pBmcPars->nMemLimit);
        } else {
            goto finish;
        }

        pMan.maxClk = std::max(pMan.maxClk, best->pData->nClk);
        pMan.maxFrame = std::max(pMan.maxFrame, (int)(best->pData->nFrame));

        if ( Abc_Clock() > pMan.nTimeToStop )
            break;
        if (pMan.clkBudget <= 0) break;

        if ( pq.empty() ) break;
    }

    print_stat (props, pPars->logFilename, Abc_NtkName(orgNtk));

    finish:
    for(int i = 0 ; i < N ; i++) {
        if (props[i]->status != POEM_UNDEC) continue;

        pNtk = (Abc_Ntk_t *)props[i]->ntk;
        Abc_NtkDelete(pNtk);
        deleteBmcState (bmcstate[i]);
        Aig_ManStop( aigman[i] );
    }
    Abc_NtkDelete(orgNtk);
    free(pMan.objs);
    free(pMan.pdata);
    // Vec_PtrFree(Lp);

    return ;
}


void PoemMultiPropertyVerificationBreadthwise( Abc_Ntk_t *pNtk, PoemPar_t * pPars) {

    Saig_ParBmc_t Pars, * pBmcPars = &Pars;
    int fOrDecomp = 0;
    Saig_ParBmcSetDefaultParams( pBmcPars );
    PoemData_t pData;
    pBmcPars->pData = &pData;
    pBmcPars->fUseGlucose = 1;
    pBmcPars->fSilent = 1;
    pBmcPars->fSolveAll = 1;
    
    PoemMan pMan;
    
    BmcState *bmcstate;
    Aig_Man_t *aigman;
    
    aigman = Abc_NtkToDar( pNtk, 0, 1 );
    bmcstate = createBmcState (aigman, pBmcPars);
    

    assert (pPars->nTimeOut > 0);
    
    pBmcPars->nMemLimit = (1LL*pPars->nMemGB*1024*1024*1024);
    
        
            
    pBmcPars->nStart = 0;
    pBmcPars->nTimeOut = pPars->nTimeOut;
    pBmcPars->iFrame = -1;
    PoemDataInit (pBmcPars->pData);
    
    abctime clk = Abc_Clock();
    int status = Solve(bmcstate, aigman, pNtk, pBmcPars, fOrDecomp);

    if (status != ABC_UNDEC) pBmcPars->pData->nFrame -= 1;

    int N = Abc_NtkPoNum(pNtk);
    pMan.objs = ABC_ALLOC(PoemObj_t, N);
    pMan.pdata = ABC_ALLOC(PoemData_t, N);

    std::vector<PoemObj_t*> props;
    for(int i = 0 ; i < N ; i++) {
        PoemDataInit (&(pMan.pdata[i]));
        pMan.pdata[i].nFrame = pBmcPars->pData->nFrame;
        pMan.objs[i].pData = &(pMan.pdata[i]);
        if (i < pBmcPars->pData->nSolved) 
            pMan.objs[i].status = POEM_SOLVED;
        props.push_back(&(pMan.objs[i]));
    }

    if (pBmcPars->pData->nSolved) pMan.pdata[0].nClk = pBmcPars->pData->lastSolvedAt - clk;
    if (N > pBmcPars->pData->nSolved) pMan.pdata[N-1].nClk = 1LL*pPars->nTimeOut * CLOCKS_PER_SEC;

    print_stat (props, pPars->logFilename, Abc_NtkName(pNtk));

    deleteBmcState (bmcstate);
    Aig_ManStop( aigman );
    free (pMan.objs);
    free (pMan.pdata);
    return ;
}

void PoemMultiPropertyVerificationALG1( Abc_Ntk_t *pNtk, PoemPar_t * pPars) {

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

    std::vector<PoemObj_t*> active;
    std::vector<PoemObj_t*> props;
    std::vector<BmcState *> bmcstate(N);
    std::vector<Aig_Man_t *> aigman(N);
    for(int i = 0 ; i < N ; i++) {
        props.push_back(&(pMan.objs[i]));
        active.push_back(&(pMan.objs[i]));
        aigman[i] = Abc_NtkToDar( (Abc_Ntk_t *)pMan.objs[i].ntk, 0, 1 );
        bmcstate[i] = createBmcState (aigman[i], pBmcPars);
    }

    PoemManInit (&pMan, N, pPars->nTimeOut, CLOCKS_PER_SEC);
    
    for (;;pMan.it++) {
        int solved = 0;
        
        std::vector<PoemObj_t*> tmp;
        
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

            // printf("order\nprop\t#frame\n");
            // for (int i = 0 ; i < active.size() ; i++) {
            //     printf("%d %d\n", active[i]->propNum, active[i]->pData->nFrame);
            // }
        }

    
        for ( PoemObj_t *obj: active) {
            
            pNtk = (Abc_Ntk_t *)(obj->ntk);
            
            pBmcPars->nStart = obj->pData->nFrame;
            pBmcPars->pData->propNum = obj->propNum;
            pBmcPars->nTimeOut = (0LL + pMan.clkBudget + CLOCKS_PER_SEC - 1) / CLOCKS_PER_SEC;
            pBmcPars->fSilent = 1;
            pBmcPars->iFrame = -1;
            PoemDataInit (pBmcPars->pData);
            
            
            abctime clk = Abc_Clock();
            int status = Solve(bmcstate[obj->propNum], aigman[obj->propNum], pNtk, pBmcPars, fOrDecomp);
            abctime clkRun = Abc_Clock() - clk;

            if (status == ABC_SAT) {
                obj->status = POEM_SAT;
                solved++;
                pMan.solved++;

                Abc_NtkDelete(pNtk);
                deleteBmcState(bmcstate[obj->propNum]);
                Aig_ManStop(aigman[obj->propNum]);
            } else if (status == ABC_UNSAT) {
                obj->status = POEM_UNSAT;
                solved++;
                pMan.solved++;

                Abc_NtkDelete(pNtk);
                deleteBmcState(bmcstate[obj->propNum]);
                Aig_ManStop(aigman[obj->propNum]);
            } else if (status == ABC_UNDEC) {
                tmp.push_back(obj);
                resetBmcState (bmcstate[obj->propNum], aigman[obj->propNum], pBmcPars, pBmcPars->nMemLimit);
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

        active = tmp;

        if ( active.size() == 0 ) break;
        if ( Abc_Clock() > pMan.nTimeToStop ) break;
        if (pMan.clkBudget <= 0) break;
    
        sort(active.begin(), active.end(), [](const PoemObj_t* a, const PoemObj_t* b) {
            return a->pData->nFrame > b->pData->nFrame;
        });
        
        if (solved == 0) {
            pMan.clkBudget = pMan.clkBudget * 2;
        }
        pMan.clkBudget = std::min(pMan.clkBudget, pMan.clkRem);
    }

    print_stat (props, pPars->logFilename, Abc_NtkName(orgNtk));

    
    finish:
    for(int i = 0 ; i < N ; i++) {
        if (props[i]->status != POEM_UNDEC) continue;

        pNtk = (Abc_Ntk_t *)props[i]->ntk;
        Abc_NtkDelete(pNtk);
        deleteBmcState (bmcstate[i]);
        Aig_ManStop( aigman[i] );
    }
    Abc_NtkDelete(orgNtk);
    free(pMan.objs);
    free(pMan.pdata);

    return ;
}


void PoemMultiPropertyVerificationETB( Abc_Ntk_t *pNtk, PoemPar_t * pPars) {
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

    print_stat (props, pPars->logFilename, Abc_NtkName(orgNtk));
    

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

void TestETB(std::vector<PoemObj_t*> &props) {
    Saig_ParBmc_t Pars, * pBmcPars = &Pars;
    int fOrDecomp = 0;
    Saig_ParBmcSetDefaultParams( pBmcPars );
    PoemData_t pData;
    pBmcPars->pData = &pData;
    pBmcPars->fUseGlucose = 1;
    pBmcPars->fSilent = 1;
    
    for (PoemObj_t *obj : props) {

        Abc_Ntk_t *pNtk = (Abc_Ntk_t *)(obj->ntk);

        if ( Abc_NtkLatchNum(pNtk) == 0 )
        {
            obj->status = POEM_SOLVED;
            continue;
        }
        
        pBmcPars->nStart = 0;
        pBmcPars->nTimeOut = 1;
        pBmcPars->fSilent = 1;
        pBmcPars->iFrame = -1;
        PoemDataInit (pBmcPars->pData);
                
        int status = Abc_NtkDarBmc3(pNtk, pBmcPars, fOrDecomp);
        
        if (status == ABC_SAT) {
            obj->status = POEM_SAT;
        } else if (status == ABC_UNSAT) {
            obj->status = POEM_UNSAT;
        } 

        obj->pData->nFrame += pBmcPars->pData->nFrame;
    }
}

void PoemMultiPropertyVerificationRO( Abc_Ntk_t *pNtk, PoemPar_t * pPars) {
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

    // Initialize random seed
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    // Fisher-Yates shuffle using rand()
    for (size_t i = N-1; i > 0 ; --i) {
        size_t j = std::rand() % (i+1);
        std::swap(props[i], props[j]);
    }
    
    PoemManInit (&pMan, N, pPars->nTimeOut, (1LL*pPars->nTimeOut * CLOCKS_PER_SEC));
    
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

    print_stat (props, pPars->logFilename, Abc_NtkName(orgNtk));
    

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

int SolveCombinationalCkt( Abc_Ntk_t * pNtk)
{
    int RetValue;
    int fVerbose;
    int nConfLimit;
    int nInsLimit;
    abctime clk;
    // set defaults
    fVerbose   = 0;
    nConfLimit = 0;
    nInsLimit  = 0;
    

    assert (pNtk != NULL);
    assert (Abc_NtkLatchNum(pNtk) == 0);
    assert (Abc_NtkPoNum(pNtk) == 1);

    clk = Abc_Clock();
    if ( Abc_NtkIsStrash(pNtk) )
    {
        RetValue = Abc_NtkMiterSat( pNtk, (ABC_INT64_T)nConfLimit, (ABC_INT64_T)nInsLimit, fVerbose, NULL, NULL );
    }
    else
    {
        assert( Abc_NtkIsLogic(pNtk) );
        Abc_NtkToBdd( pNtk );
        RetValue = Abc_NtkMiterSat( pNtk, (ABC_INT64_T)nConfLimit, (ABC_INT64_T)nInsLimit, fVerbose, NULL, NULL );
    }

    // verify that the pattern is correct
    if ( RetValue == 0 )
    {
        //int i;
        //Abc_Obj_t * pObj;
        int * pSimInfo = Abc_NtkVerifySimulatePattern( pNtk, pNtk->pModel );
        if ( pSimInfo[0] != 1 )
            Abc_Print( 1, "ERROR in Abc_NtkMiterSat(): Generated counter example is invalid.\n" );
        ABC_FREE( pSimInfo );
        exit (1);
        /*
        // print model
        Abc_NtkForEachPi( pNtk, pObj, i )
        {
            Abc_Print( -1, "%d", (int)(pNtk->pModel[i] > 0) );
            if ( i == 70 )
                break;
        }
        Abc_Print( -1, "\n" );
        */
    }

    return RetValue;
}

int Solve (BmcState *state, Aig_Man_t * pMan, Abc_Ntk_t * pNtk, Saig_ParBmc_t * pPars, int fOrDecomp) {
    if (Abc_NtkLatchNum (pNtk) == 0) {
        return SolveCombinationalCkt (pNtk);   
    }

    return Abc_NtkDarBmc3Continue(state, pMan, pNtk, pPars, fOrDecomp);
}

std::string decideAlgorithm(const std::vector<double>& progress);

void PoemMultiPropertyVerificationHybrid( Abc_Ntk_t *pNtk, PoemPar_t * pPars) {
    Saig_ParBmc_t Pars, * pBmcPars = &Pars;
    int fOrDecomp = 0;
    Saig_ParBmcSetDefaultParams( pBmcPars );
    PoemData_t pData;
    pBmcPars->pData = &pData;
    pBmcPars->fUseGlucose = 1;
    pBmcPars->fSilent = 1;
    Abc_Ntk_t *orgNtk;
    orgNtk = Abc_NtkDup(pNtk);
   
    std::string algorithm_name;

    int N = Abc_NtkPoNum(orgNtk);

    int time_to_decide = 400;

    PoemMan pMan;
    std::vector<PoemObj_t*> props;

    if (2*N <= time_to_decide) {
        pPars->nTimeOut -= 2*N;
        // decide between abc, alg1, alg2, and etb

        // all together
        pBmcPars->nStart = 0;
        pBmcPars->nTimeOut = N;
        pBmcPars->fSilent = 1;
        pBmcPars->iFrame = -1;
        pBmcPars->fSolveAll = 1;
        PoemDataInit (pBmcPars->pData);
        Abc_NtkDarBmc3(orgNtk, pBmcPars, fOrDecomp);

        // all separately
        PoemManSeparatePorperties (&pMan, orgNtk);
        for(int i = 0 ; i < N ; i++) {
            props.push_back(&(pMan.objs[i]));
        }
        TestETB (props);


        // decide ?
        unsigned int maxFrame = 0;
        double avgFrame = 0;
        int nSolved = 0;
        std::vector<double> progress;

        for (int i = 0 ; i < N ; i++) {
            maxFrame = std::max(maxFrame, props[i]->pData->nFrame);
            if (props[i]->status == POEM_UNDEC) {
                progress.push_back(props[i]->pData->nFrame);
                avgFrame += progress.back();
            } else {
                nSolved++;
            }
        }

        if (pData.nSolved > nSolved || pData.nFrame > maxFrame || pData.nFrame*(N-nSolved) > avgFrame) {
            algorithm_name = "abc";
        } else {
            algorithm_name = decideAlgorithm (progress);
        }

    } else if (N <= time_to_decide) {
        pPars->nTimeOut -= N;
        // decide between alg1, alg2, and etb
        PoemManSeparatePorperties (&pMan, orgNtk);
        for(int i = 0 ; i < N ; i++) {
            props.push_back(&(pMan.objs[i]));
        }
        TestETB (props);

        std::vector<double> progress;
        for (int i = 0 ; i < N ; i++) {
            if (props[i]->status == POEM_UNDEC) {
                progress.push_back(props[i]->pData->nFrame);
            }
        }
        if (progress.size() == 0) {
            algorithm_name = "etb";
        } else {
            algorithm_name = decideAlgorithm (progress);
        }
    } else {
        algorithm_name = "etb";
    }

    for(int i = 0 ; i < props.size() ; i++) {        
        pNtk = (Abc_Ntk_t *)props[i]->ntk;
        Abc_NtkDelete(pNtk);
    }
    free(pMan.objs);
    free(pMan.pdata);

    printf("\nSelected Algorithm: ");
    if (algorithm_name == "etb") {
        printf("ETB\n");
        PoemMultiPropertyVerificationETB (orgNtk, pPars);
    } else if (algorithm_name == "alg1") {
        printf("ALG1\n");
        PoemMultiPropertyVerificationALG1 (orgNtk, pPars);
    } else if (algorithm_name == "alg2") {
        printf("ALG2\n");
        PoemMultiPropertyVerificationALG2 (orgNtk, pPars);
    } else if (algorithm_name == "abc") {
        printf("ABC\n");
        PoemMultiPropertyVerificationBreadthwise (orgNtk, pPars);
    } else {
        printf("no algorithm selected!\n");
        exit(1);
    }

    // cout<<algorithm_name<<"\n";
    // selected_func(orgNtk, pPars);

    return ;
}

std::string decideAlgorithm(const std::vector<double>& progress) {
    if (progress.empty()) return "No Data";

    size_t n = progress.size();
    
    // 1. Calculate Mean
    double sum = std::accumulate(progress.begin(), progress.end(), 0.0);
    double mean = sum / n;

    // 2. Calculate Standard Deviation
    double sq_sum = 0;
    for (double val : progress) {
        sq_sum += (val - mean) * (val - mean);
    }
    double std_dev = std::sqrt(sq_sum / n);

    // 3. Calculate Coefficient of Variation (CV)
    double cv = (mean != 0) ? (std_dev / mean) : 0;

    // 4. Calculate Top 25% Share (Concentration)
    std::vector<double> sorted_p = progress;
    std::sort(sorted_p.begin(), sorted_p.end(), std::greater<double>());
    
    int top_count = std::max(1, (int)(n * 0.25));
    double top_sum = std::accumulate(sorted_p.begin(), sorted_p.begin() + top_count, 0.0);
    double top_share = top_sum / sum;

    // --- Decision Logic ---
    
    // Case 1: Almost identical (CV < 15%)
    if (cv < 0.15) {
        return "etb";
    }
    
    // Case 2: Highly skewed (High CV or small group dominates > 50%)
    else if (cv > 0.60 || top_share > 0.50) {
        return "alg2";
    }
    
    // Case 3: Middle ground (70-80% are similar)
    else {
        return "alg1";
    }
}