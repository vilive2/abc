#include "base/main/mainInt.h"
#include "poem.h"

ABC_NAMESPACE_HEADER_START

extern void Poem_Init(Abc_Frame_t *pAbc);

ABC_NAMESPACE_HEADER_END

static int Abc_CommandPoem( Abc_Frame_t * pAbc, int argc, char ** argv );
static int Abc_CommandPoem1( Abc_Frame_t * pAbc, int argc, char ** argv );

void Poem_Init(Abc_Frame_t *pAbc)
{
    Cmd_CommandAdd( pAbc, "Verification", "poem",         Abc_CommandPoem,            1 );
    Cmd_CommandAdd( pAbc, "Verification", "poem1",        Abc_CommandPoem1,            1 );
}


/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_CommandPoem( Abc_Frame_t * pAbc, int argc, char ** argv )
{
    extern void PoemMultiPropertyVerification( Abc_Ntk_t * pNtk, PoemPar_t * pPars );
    extern void SequentialMultiPropertyVerification (Abc_Ntk_t * pNtk, PoemPar_t * pPars );
    PoemPar_t Pars, * pPars = &Pars;
    Abc_Ntk_t * pNtk = Abc_FrameReadNtk(pAbc);
    int c;
    ParPoemSetDefaultParams( pPars );
    Extra_UtilGetoptReset();
    while ( ( c = Extra_UtilGetopt( argc, argv, "TLsvVh" ) ) != EOF )
    {
        switch ( c )
        {
        case 'T':
            if ( globalUtilOptind >= argc )
            {
                Abc_Print( -1, "Command line switch \"-T\" should be followed by an integer.\n" );
                goto usage;
            }
            pPars->nTimeOut = atoi(argv[globalUtilOptind]);
            globalUtilOptind++;
            if ( pPars->nTimeOut < 0 )
                goto usage;
            break;
        case 's':
            pPars->staticOrdering = 1;
            break;
        case 'v':
            pPars->fVerbose ^= 1;
            break;
        case 'h':
            goto usage;
        default:
            goto usage;
        }
    }
    if ( pNtk == NULL )
    {
        Abc_Print( -1, "Empty network.\n" );
        return 1;
    }
    if ( !Abc_NtkIsStrash(pNtk) )
    {
        Abc_Print( -1, "Currently only works for structurally hashed circuits.\n" );
        return 0;
    }
    if ( Abc_NtkLatchNum(pNtk) == 0 )
    {
        Abc_Print( -1, "Does not work for combinational networks.\n" );
        return 0;
    }
    if ( Abc_NtkConstrNum(pNtk) > 0 )
    {
        Abc_Print( -1, "Constraints have to be folded (use \"fold\").\n" );
        return 0;
    }
    if ( pAbc->fBatchMode && (pAbc->Status == 0 || pAbc->Status == 1) ) 
    { 
        Abc_Print( 1, "The miters is already solved; skipping the command.\n" ); 
        return 0;
    }
    
    if (pPars->staticOrdering) 
        SequentialMultiPropertyVerification (pNtk, pPars);
    else
        PoemMultiPropertyVerification(pNtk, pPars);

    return 0;

usage:
    Abc_Print( -2, "usage: poem [-T num] [-vsh]\n" );
    Abc_Print( -2, "\t         performs bounded model checking with dynamic unrolling\n" );
    Abc_Print( -2, "\t-T num : runtime limit, in seconds [default = %d]\n",                       pPars->nTimeOut );
    Abc_Print( -2, "\t-v     : toggle verbose [default = %s]\n",                           pPars->fVerbose? "yes": "no" );
    Abc_Print( -2, "\t-h     : print the command usage\n");
    Abc_Print( -2, "\t-s     : static ordering\n");
    return 1;
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_CommandPoem1( Abc_Frame_t * pAbc, int argc, char ** argv )
{
    extern void PoemMultiPropertyVerificationALG1( Abc_Ntk_t * pNtk, PoemPar_t * pPars );
    PoemPar_t Pars, * pPars = &Pars;
    Abc_Ntk_t * pNtk = Abc_FrameReadNtk(pAbc);
    int c;
    ParPoemSetDefaultParams( pPars );
    Extra_UtilGetoptReset();
    while ( ( c = Extra_UtilGetopt( argc, argv, "Tvh" ) ) != EOF )
    {
        switch ( c )
        {
        case 'T':
            if ( globalUtilOptind >= argc )
            {
                Abc_Print( -1, "Command line switch \"-T\" should be followed by an integer.\n" );
                goto usage;
            }
            pPars->nTimeOut = atoi(argv[globalUtilOptind]);
            globalUtilOptind++;
            if ( pPars->nTimeOut < 0 )
                goto usage;
            break;
        case 'v':
            pPars->fVerbose ^= 1;
            break;
        case 'h':
            goto usage;
        default:
            goto usage;
        }
    }
    if ( pNtk == NULL )
    {
        Abc_Print( -1, "Empty network.\n" );
        return 1;
    }
    if ( !Abc_NtkIsStrash(pNtk) )
    {
        Abc_Print( -1, "Currently only works for structurally hashed circuits.\n" );
        return 0;
    }
    if ( Abc_NtkLatchNum(pNtk) == 0 )
    {
        Abc_Print( -1, "Does not work for combinational networks.\n" );
        return 0;
    }
    if ( Abc_NtkConstrNum(pNtk) > 0 )
    {
        Abc_Print( -1, "Constraints have to be folded (use \"fold\").\n" );
        return 0;
    }
    if ( pAbc->fBatchMode && (pAbc->Status == 0 || pAbc->Status == 1) ) 
    { 
        Abc_Print( 1, "The miters is already solved; skipping the command.\n" ); 
        return 0;
    }
    
    PoemMultiPropertyVerificationALG1(pNtk, pPars);

    return 0;

usage:
    Abc_Print( -2, "usage: poem1 [-T num] [-vh]\n" );
    Abc_Print( -2, "\t         performs bounded model checking with dynamic unrolling\n" );
    Abc_Print( -2, "\t-T num : runtime limit, in seconds [default = %d]\n",                       pPars->nTimeOut );
    Abc_Print( -2, "\t-v     : toggle verbose [default = %s]\n",                           pPars->fVerbose? "yes": "no" );
    Abc_Print( -2, "\t-h     : print the command usage\n");
    return 1;
}