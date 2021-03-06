/******************************************************************************

  PROJECT    : Ex-Machinis

  DESCRIPTION: Main module
 
******************************************************************************/

/******************************* INCLUDES ************************************/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include "config.h"

/******************************* DEFINES *************************************/

/******************************* TYPES ***************************************/



/******************************* PROTOTYPES **********************************/



/******************************* GLOBAL VARIABLES ****************************/

/******************************* LOCAL FUNCTIONS *****************************/

/******************************* MAIN ****************************************/

/** ****************************************************************************

  @brief      Engine entry point

  @param[in]  argc, argv 

  @return     Execution result (EXIT_SUCCESS/EXIT_FAILURE)

*******************************************************************************/
int main(int argc, char** argv) 
{
    // initialize engine & run it
    if(engine_init(argc, argv) == ENGINE_OK)
    {
        engine_run();
    }

    return (engine_get_last_error());
}

