#ifndef POEM 
#define POEM

#define POEM_SAT 1
#define POEM_UNSAT -1
#define POEM_UNDEC 0
#define POEM_SOLVED 2

#define ABC_SAT 0
#define ABC_UNSAT 1
#define ABC_UNDEC -1

// int Abc_Frame_t_::Status
// the status of verification problem (proved=1, disproved=0, undecided=-1)

#define INF 1e9
#define EPS 1e-9

// #define DEBUG_POEM 1

#include "misc/vec/vec.h"
#include "misc/util/abc_global.h"


ABC_NAMESPACE_HEADER_START

typedef struct {
    int nTimeOut;
    int fVerbose;
    int staticOrdering;
} PoemPar_t;

typedef struct {
    unsigned int nSat;
    unsigned int nFrame;
    unsigned int nLearnt;
    unsigned int nDecisions;
    unsigned int nClause;
    unsigned int nConflicts;
    unsigned int nPropagations;
    int propNum;
    abctime nClk;
    double score;
} PoemData_t;


typedef struct {
    PoemData_t *pData;
    void *ntk;
    unsigned int ntkSize;
    int status;
    int propNum;
} PoemObj_t;

typedef struct {
    int it;
    int maxFrame;
    int solved;
    int N;
    abctime minClk;
    abctime maxClk;
    abctime clkTotal;
    abctime nTimeToStop;
    abctime clkBudget;
    abctime clkRem;
} PoemMan;

extern int CompLearnt(const void *a, const void *b);
extern int CompScore(const void *a, const void *b);
extern int CompFrame(const void *a, const void *b);

extern void PoemDataInit ( PoemData_t *pData);

extern void ParPoemSetDefaultParams ( PoemPar_t *pPars);

extern void PrintStat( Vec_Ptr_t *objs, FILE *fp);

ABC_NAMESPACE_HEADER_END

#endif 