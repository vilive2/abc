#include "base/main/mainInt.h"
#include "purse.h"

static int Abc_CommandPurse( Abc_Frame_t * pAbc, int argc, char ** argv );

void Purse_Init(Abc_Frame_t *pAbc)
{
    Cmd_CommandAdd( pAbc, "Verification", "purse",         Abc_CommandPurse,            1 );
}


/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_CommandPurse( Abc_Frame_t * pAbc, int argc, char ** argv )
{
    extern void PurseMultiPropertyVerification( Abc_Ntk_t * pNtk, PursePar_t * pPars );
    PursePar_t Pars, * pPars = &Pars;
    Abc_Ntk_t * pNtk = Abc_FrameReadNtk(pAbc);
    char * pLogFileName = NULL;
    int c;
    ParPurseSetDefaultParams( pPars );
    Extra_UtilGetoptReset();
    while ( ( c = Extra_UtilGetopt( argc, argv, "SFTHGCDJIPQRLWaxdursgvVzh" ) ) != EOF )
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
        case 'C':
            if ( globalUtilOptind >= argc )
            {
                Abc_Print( -1, "Command line switch \"-C\" should be followed by an integer.\n" );
                goto usage;
            }
            pPars->nConfLimit = atoi(argv[globalUtilOptind]);
            globalUtilOptind++;
            if ( pPars->nConfLimit < 0 )
                goto usage;
            break;
        case 'P':
            if ( globalUtilOptind >= argc )
            {
                Abc_Print( -1, "Command line switch \"-P\" should be followed by an integer.\n" );
                goto usage;
            }
            pPars->nPropLimit = atoi(argv[globalUtilOptind]);
            globalUtilOptind++;
            if ( pPars->nPropLimit < 0 )
                goto usage;
            break;
        case 'L':
            if ( globalUtilOptind >= argc )
            {
                Abc_Print( -1, "Command line switch \"-L\" should be followed by a file name.\n" );
                goto usage;
            }
            pLogFileName = argv[globalUtilOptind];
            globalUtilOptind++;
            break;
        case 'v':
            pPars->fVerbose ^= 1;
            break;
        case 'V':
            pPars->purseVerbose = 1;
            break;
        case 's':
            pPars->fUseSatoko ^= 1;
            break;
        case 'g':
            pPars->fUseGlucose ^= 1;
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

    if (pLogFileName) {
        pPars->pLogFile = fopen(pLogFileName, "wb");
    }
    
    PurseMultiPropertyVerification(pNtk, pPars);

    if (pPars->pLogFile) {
        fclose (pPars->pLogFile);
    }
    
    return 0;

usage:
    Abc_Print( -2, "usage: purse [-TCPL num] [-L file] [-axsgvh]\n" );
    Abc_Print( -2, "\t         performs bounded model checking with dynamic unrolling\n" );
    Abc_Print( -2, "\t-T num : runtime limit, in seconds [default = %d]\n",                       pPars->nTimeOut );
    Abc_Print( -2, "\t-C num : conflict limit [default = %d]\n",                       pPars->nConfLimit );
    Abc_Print( -2, "\t-P num : propagation limit [default = %d]\n",                       pPars->nPropLimit );
    Abc_Print( -2, "\t-L file: the log file name [default = %s]\n",                               pLogFileName ? pLogFileName : "no logging" );
    Abc_Print( -2, "\t-v     : toggle verbose BMC output [default = %s]\n",                           pPars->fVerbose? "yes": "no" );
    Abc_Print( -2, "\t-V     : toggle verbose PURSE output [default = %s]\n",                           pPars->purseVerbose? "yes": "no" );
    Abc_Print( -2, "\t-s     : use satoko sat solver\n");
    Abc_Print( -2, "\t-g     : use glucose sat solver\n");
    Abc_Print( -2, "\t-h     : print the command usage\n");
    return 1;
}