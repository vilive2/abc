#include "base/abc/abc.h"
#include "base/main/abcapis.h"
#include "base/main/main.h"
#include "misc/vec/vec.h"
#include "sat/bmc/bmc.h"
#include "purse.h"

void ParPurseSetDefaultParams ( PursePar_t *pPars) {
    pPars->nConfLimit = 0;
    pPars->nPropLimit = 0;
    pPars->nTimeOut = ABC_INT_MAX;
    pPars->fVerbose = 0;
    pPars->pLogFileName = NULL;
}

void PurseMultiPropertyVerification( Abc_Ntk_t *pNtk, PursePar_t * pPars) {
    extern int Abc_NtkDarBmc3( Abc_Ntk_t * pNtk, Saig_ParBmc_t * pPars, int fOrDecomp );
    extern Abc_Ntk_t * Abc_NtkSelectPos( Abc_Ntk_t * pNtkInit, Vec_Int_t * vPoIds );
    Vec_Int_t *vPoIds = Vec_IntStart(1); 
    Saig_ParBmc_t Pars, * pBmcPars = &Pars;
    int fOrDecomp = 0;
    Saig_ParBmcSetDefaultParams( pBmcPars );
    pBmcPars->nTimeOut = pPars->nTimeOut;
    pBmcPars->pLogFileName = pPars->pLogFileName;
    pBmcPars->fUseSatoko ^= 1;
    pBmcPars->fVerbose ^= pPars->fVerbose;
    Abc_Ntk_t *orgNtk;
    orgNtk = Abc_NtkDup(pNtk);

    int N = Abc_NtkPoNum(orgNtk);
    Vec_Int_t *Property_Status = Vec_IntStartFull(N);
    Vec_Int_t *frameReach = Vec_IntStart(N);
    Vec_Int_t *nSat = Vec_IntStart(N);
    int gates = 100; // some value designer's choice
    int size = Abc_NtkNodeNum(orgNtk);


    Vec_Ptr_t *ntks = Vec_PtrStart(N);
    for(int i = 0 ; i < N ; i++) {
        Vec_IntWriteEntry(vPoIds, 0, i);
        pNtk = Abc_NtkDup(orgNtk);
        pNtk = Abc_NtkSelectPos( pNtk, vPoIds);
        Vec_PtrWriteEntry(ntks, i, pNtk);
    }

    int CB, PB;
    
    if (size < gates) {
        CB = pPars->nConfLimit;
        PB = pPars->nPropLimit;
        // pBmcPars->nTimeOut = 1 + pPars->nTimeOut/(N*N*N);
    } else if (size < gates * 10) {
        CB = pPars->nConfLimit * ((size % gates) + 1);
        PB = pPars->nPropLimit * ((size % gates) + 1);
        // pBmcPars->nTimeOut = 1 + pPars->nTimeOut/(N*N);
    } else {
        CB = pPars->nConfLimit * 11;
        PB = pPars->nPropLimit * 11;
        // pBmcPars->nTimeOut = 1 + pPars->nTimeOut/(N);
    }

    int j = 0;
    double T = 0.0;

    Vec_Int_t *Lp = Vec_IntStartNatural(N);

    int Comp1 (const void *a, const void *b) {
        int i = *(int *)a;
        int j = *(int *)b;
        int TCi = Vec_IntEntry(nSat, i);
        int TCj = Vec_IntEntry(nSat, j);
        int vi = TCi * Vec_IntEntry(frameReach, j);
        int vj = TCj * Vec_IntEntry(frameReach, i);

        if (vi < vj)
            return -1;
        if (vi > vj)
            return 1;
        return 0;
    }
    
    while (1) {
        int solved = 0;
        Vec_Int_t *unk_goals = Vec_IntAlloc( Vec_IntSize(Lp) );
        
        int i, idx;
        Vec_IntForEachEntry( Lp, i, idx ) {

            clock_t start_time = clock();

            pNtk = Vec_PtrEntry(ntks, i);

            pBmcPars->nStart = Vec_IntEntry(frameReach, i);
            pBmcPars->nConfLimit = CB;
            pBmcPars->nPropLimit = PB;
            pBmcPars->nSat = 0;
            pBmcPars->nFrame = 0;
            pBmcPars->iFrame = -1;
            printf("Running Property %d:\n", i);
            int status = Abc_NtkDarBmc3(pNtk, pBmcPars, fOrDecomp);

            if (status == 0) {
                Vec_IntWriteEntry( Property_Status, i, 0 );
                solved++;
            } else if (status == 1) {
                Vec_IntWriteEntry( Property_Status, i, 1 );
                solved++;
            } else if (status == -1) {
                Vec_IntPush( unk_goals, i);
            } else {
                Vec_IntFree(Property_Status);
                Vec_IntFree(Lp);
                Vec_IntFree(unk_goals);
                Vec_IntFree(frameReach);
                Vec_IntFree(nSat);
                return ; // error case
            }

            Vec_IntAddToEntry(frameReach, i, pBmcPars->nFrame);
            Vec_IntAddToEntry(nSat, i, pBmcPars->nSat);

            clock_t end_time = clock();
            T += (double)(end_time - start_time) / CLOCKS_PER_SEC;

            if (T > pPars->nTimeOut)
                break;
        }

        Vec_IntFree(Lp);
        Lp = unk_goals;
        j++;

        if (T < pPars->nTimeOut && Lp->nSize != 0) {
            qsort((void *)Lp->pArray, (size_t)Lp->nSize, sizeof(int), Comp1);
        } else {
            break;
        }

        if (solved == 0) {
            CB = CB * 2;
            PB = PB * 2;
        }
    }

    int i, status;
    Vec_IntForEachEntry (Property_Status, status, i) {
        printf ("Property %d : %s in %d Frames\n", i, 
            (status == 0?"SAT": status == 1?"UNSAT":"UNDECIDED"), 
            Vec_IntEntry(frameReach, i)
        );
    }

    for(int i = 0 ; i < N ; i++) {
        pNtk = Vec_PtrEntry(ntks, i);
        Abc_NtkDelete(pNtk);
    }
    Abc_NtkDelete(orgNtk);
    Vec_PtrFree(ntks);
    Vec_IntFree(vPoIds);

    Vec_IntFree(Lp);
    Vec_IntFree(Property_Status);
    Vec_IntFree(nSat);
    Vec_IntFree(frameReach);
    return ;
}