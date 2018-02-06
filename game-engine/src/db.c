/******************************************************************************

  PROJECT    : Ex-Machinis

  DESCRIPTION: DB interface module
 
******************************************************************************/

/******************************* INCLUDES ************************************/

#include <mysql.h>

#include "db.h"
#include "trace.h"

/******************************* DEFINES *************************************/

/******************************* TYPES ***************************************/



/******************************* PROTOTYPES **********************************/



/******************************* GLOBAL VARIABLES ****************************/

/******************************* LOCAL FUNCTIONS *****************************/


/******************************* PUBLIC FUNCTIONS ****************************/

void db_init()
{
	//small test MYSQSL connection
    trace_write(1, "MySQL Client Version %lu connected", mysql_get_client_version());
}

void db_stop()
{
	//small test MYSQSL connection
    trace_write(1, "MySQL Client Version %lu disconnected", mysql_get_client_version());
}