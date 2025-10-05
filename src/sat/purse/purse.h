#ifndef PURSE 
#define PURSE

typedef struct {
    int nConfLimit;
    int nPropLimit;
    int nTimeOut;
    int fVerbose;
    char * pLogFileName;
} PursePar_t;

extern void ParPurseSetDefaultParams ( PursePar_t *pPars);

#endif 