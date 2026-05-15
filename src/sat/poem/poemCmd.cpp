#include "base/main/mainInt.h"
#include "poem.h"

ABC_NAMESPACE_HEADER_START

extern void Poem_Init(Abc_Frame_t *pAbc);

ABC_NAMESPACE_HEADER_END

static int Abc_CommandPoem( Abc_Frame_t * pAbc, int argc, char ** argv );

void Poem_Init(Abc_Frame_t *pAbc)
{
    Cmd_CommandAdd( pAbc, "Verification", "poem",         Abc_CommandPoem,            1 );
}

extern void PoemMultiPropertyVerificationBreadthwise( Abc_Ntk_t * pNtk, PoemPar_t * pPars );
extern void PoemMultiPropertyVerificationALG1( Abc_Ntk_t * pNtk, PoemPar_t * pPars );
extern void PoemMultiPropertyVerificationALG2( Abc_Ntk_t * pNtk, PoemPar_t * pPars );
extern void PoemMultiPropertyVerificationRO( Abc_Ntk_t * pNtk, PoemPar_t * pPars );
extern void PoemMultiPropertyVerificationETB( Abc_Ntk_t * pNtk, PoemPar_t * pPars );

typedef struct {
    const char* name;
    const char* description;
    void (*function)(Abc_Ntk_t * pNtk, PoemPar_t * pPars);
} PoemAlgorithmMap;

static const PoemAlgorithmMap poem_algorithms[] = {
    {"abc",  "Breadthwise verification using bmc", PoemMultiPropertyVerificationBreadthwise},
    {"alg1", "algorithm 1",           PoemMultiPropertyVerificationALG1},
    {"alg2", "algorithm 2",           PoemMultiPropertyVerificationALG2},
    {"ro", "random ordering",           PoemMultiPropertyVerificationRO},
    {"etb", "equal time bound",        PoemMultiPropertyVerificationETB},
    {NULL,    NULL,                      NULL}
};

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_CommandPoem( Abc_Frame_t * pAbc, int argc, char ** argv )
{
    char* algorithm_name = NULL;
    void (*selected_func)(Abc_Ntk_t * pNtk, PoemPar_t * pPars ) = NULL;
    PoemPar_t Pars, * pPars = &Pars;
    Abc_Ntk_t * pNtk = Abc_FrameReadNtk(pAbc);
    
    if (argc < 2) {
        Abc_Print(-1, "Error: Algorithm name is required!\n");
        goto usage;
    }

    algorithm_name = argv[1];

    if (strcmp(algorithm_name, "-h") == 0 || strcmp(algorithm_name, "--help") == 0 || strcmp(algorithm_name, "help") == 0) {
        goto usage;
    }

    for (int i = 0 ; poem_algorithms[i].name != NULL; i++) {
        if (strcmp(algorithm_name, poem_algorithms[i].name) == 0) {
            selected_func = poem_algorithms[i].function;
            break;
        }
    }

    if (!selected_func) {
        Abc_Print(-1, "Error: Invalid algoritm '%s'\n", algorithm_name);
        goto usage;
    }

    argc--;
    argv = argv + 1;

    
    int c;
    ParPoemSetDefaultParams( pPars );
    Extra_UtilGetoptReset();

    while ( ( c = Extra_UtilGetopt( argc, argv, "TMLvh" ) ) != EOF )
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
            if ( pPars->nTimeOut <= 0 ) {
                Abc_Print(-1, "Error: Timeout must be positive integer.\n");
                goto usage;
            }
            break;

        case 'M':
            if ( globalUtilOptind >= argc )
            {
                Abc_Print( -1, "Command line switch \"-M\" should be followed by an integer.\n" );
                goto usage;
            }
            pPars->nMemGB = atoi(argv[globalUtilOptind]);
            globalUtilOptind++;
            if (pPars->nMemGB < 0) {
                Abc_Print(-1, "Error: Memory must be non-negative.\n");
                goto usage;
            }
            break;

        case 'L':
            if ( globalUtilOptind >= argc )
            {
                Abc_Print( -1, "Command line switch \"-L\" should be followed by a filename.\n" );
                goto usage;
            }
            strncpy(pPars->logFilenameBuf, argv[globalUtilOptind], sizeof(pPars->logFilenameBuf)-1);
            pPars->logFilename = pPars->logFilenameBuf;
            globalUtilOptind++;
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

    if (pPars->nTimeOut <= 0) {
        Abc_Print(-1, "Error: -T timeout is required and must be positive!\n");
        goto usage;
    }
    
    if (pPars->fVerbose) {
        Abc_Print(0, "\n========================================\n");
        Abc_Print(0, "POEM Configuration:\n");
        Abc_Print(0, "  Algorithm: %s\n", algorithm_name);
        Abc_Print(0, "  Timeout:   %d seconds\n", pPars->nTimeOut);
        Abc_Print(0, "  Memory:    %d GB\n", pPars->nMemGB);
        if (pPars->logFilename)
            Abc_Print(0, "  Log file:  %s\n", pPars->logFilename);
        Abc_Print(0, "========================================\n\n");
    }


    selected_func(pNtk, pPars);

    return 0;

usage:
    Abc_Print(-2, "Usage: poem <algorithm> -T <timeout> [-M <mem_GB>] [-L <logfile>] [-v] [-h]\n");
    Abc_Print(-2, "\t<algorithm> : required, must be one of: ");
    for (int i = 0; poem_algorithms[i].name != NULL; i++) {
        Abc_Print(-2, "%s%s", poem_algorithms[i].name, 
                 poem_algorithms[i+1].name ? ", " : "\n");
    }
    Abc_Print(-2, "\t-T <num>    : required, runtime limit in seconds\n");
    Abc_Print(-2, "\t-M <num>    : memory available in GB [default = %d]\n", pPars ? pPars->nMemGB : 0);
    Abc_Print(-2, "\t-L <file>   : log file name [default = %s]\n", 
               (pPars && pPars->logFilename) ? pPars->logFilename : "");
    Abc_Print(-2, "\t-v          : verbose mode [default = %s]\n", 
               (pPars && pPars->fVerbose) ? "on" : "off");
    Abc_Print(-2, "\t-h          : print this help message\n");
    return 1;
}