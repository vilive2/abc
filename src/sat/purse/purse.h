#ifndef PURSE 
#define PURSE

#define PURSE_SAT 1
#define PURSE_UNSAT -1
#define PURSE_UNDEC 0

#define ABC_SAT 0
#define ABC_UNSAT 1
#define ABC_UNDEC -1

// int Abc_Frame_t_::Status
// the status of verification problem (proved=1, disproved=0, undecided=-1)

#define INF 1e9
#define EPS 1e-9

#define DEBUG_PURSE 1

#include "misc/vec/vec.h"
#include "misc/util/abc_global.h"

typedef struct {
    int nTimeOut;
    int fVerbose;
} PursePar_t;

typedef struct {
    unsigned int nSat;
    unsigned int nFrame;
    unsigned int nLearnt;
    unsigned int nDecisions;
    unsigned int nClause;
    unsigned int nConflicts;
    unsigned int nPropagations;
    abctime nClk;
    double score;
} PurseData_t;


typedef struct {
    PurseData_t *pData;
    void *ntk;
    int status;
    int propNum;
} PurseObj_t;

extern int CompLearnt(const void *a, const void *b);
extern int CompScore(const void *a, const void *b);
extern int CompFrame(const void *a, const void *b);

extern void PurseDataInit ( PurseData_t *pData);

extern void ParPurseSetDefaultParams ( PursePar_t *pPars);

extern void PrintStat( Vec_Ptr_t *objs, FILE *fp);

#endif 