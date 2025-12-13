#ifndef PURSE 
#define PURSE

#define PURSE_SAT 1
#define PURSE_UNSAT -1
#define PURSE_UNDEC 0

#define ABC_SAT 0
#define ABC_UNSAT 1
#define ABC_UNDEC -1

#define DEBUG_PURSE 1

#include "misc/vec/vec.h"

typedef struct {
    int nConfLimit;
    int nPropLimit;
    int nTimeOut;
    int fVerbose;
    char * pLogFileName;
} PursePar_t;

typedef struct {
    unsigned int nSat;
    unsigned int nFrame;
    unsigned int nLearnt;
    unsigned int nDecisions;
} PurseData_t;


typedef struct {
    PurseData_t *pData;
    void *ntk;
    int status;
    int propNum;
} PurseObj_t;

extern int CompLearnt(const void *a, const void *b);

extern void PurseDataInit ( PurseData_t *pData);

extern void ParPurseSetDefaultParams ( PursePar_t *pPars);

extern void PrintStat( Vec_Ptr_t *objs);

#endif 