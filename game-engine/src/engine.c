/******************************************************************************

  PROJECT    : Ex-Machinis

  DESCRIPTION: Engine module
 
******************************************************************************/

/******************************* INCLUDES ************************************/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>

#include "engine.h"
#include "config.h"
#include "db.h"
#include "vm.h"
#include "email.h"

/******************************* DEFINES *************************************/

/******************************* TYPES ***************************************/



/******************************* PROTOTYPES **********************************/



/******************************* GLOBAL VARIABLES ****************************/

// env variable
const char* PLAT_HOME = "PLAT_HOME";

Engine_t engine;

/******************************* LOCAL FUNCTIONS *****************************/

/** ****************************************************************************

  @brief      Callback invoked when supported signal is received
              We do cleanup and exit properly

  @param[in]  signal_id  ID of signal received

  @return     void

*******************************************************************************/
void signals_handler(int signal_id)
{
    engine_trace(TRACE_LEVEL_ALWAYS, "Signal [%d] received", signal_id);
    engine_stop();
}

/** ****************************************************************************

  @brief      Initializes engine traces module

  @param[in]  void

  @return     Execution result 

*******************************************************************************/
ErrorCode_t engine_init_traces()
{
	ErrorCode_t result = ENGINE_OK;

    // Define traces configuration
    engine.trace_conf.level = TRACE_LEVEL_DEBUG;
    snprintf(engine.trace_conf.log_file_path, 
    	PATH_MAX, 
    	"%s/log/%s.log",
    	engine.plat_home,
    	engine.exe_name);


    result = trace_init(&engine.trace_conf, &engine.trace_hndl);
	
	return result;
}

/** ****************************************************************************

  @brief      Initializes configuration module

  @param[in]  None

  @return     void

*******************************************************************************/
ErrorCode_t engine_init_config()
{
	ErrorCode_t result = ENGINE_OK;

	// set config file name
	snprintf(engine.options.conf_file_path, 
    	PATH_MAX, 
    	"%s/conf/%s.conf",
    	engine.plat_home,
    	engine.exe_name);

    // load config
    result = config_engine_read(&engine.conf_hndl, 
    	engine.options.conf_file_path, 
    	engine.config.params);

    return result;
}


/** ****************************************************************************

  @brief      Initializes configuration module

  @param[in]  None

  @return     void

*******************************************************************************/
ErrorCode_t engine_init_db()
{
	ErrorCode_t result = ENGINE_OK;

    // initialize DB parameters
    engine.db_connection.hndl = NULL;
    engine.db_connection.host = engine.config.params[DB_HOST_ID];
    engine.db_connection.port = atoi(engine.config.params[DB_PORT_ID]);
    engine.db_connection.user = engine.config.params[DB_USER_ID];
    engine.db_connection.password = engine.config.params[DB_PASSWORD_ID];
    engine.db_connection.db_name = engine.config.params[DB_NAME_ID];

	// db
	result = db_init(&engine.db_connection);

	return result;
}

/** ****************************************************************************

  @brief      Stops engine submodules

  @param[in]  None

  @return     void

*******************************************************************************/
void engine_stop_all()
{
	// DB
    db_stop(&engine.db_connection);

    // CONFIG
    config_engine_stop(&engine.conf_hndl);

	engine.status = ENGINE_STOPPED_STATUS;

	engine_trace(TRACE_LEVEL_ALWAYS, "-----------------------------------------");
    engine_trace(TRACE_LEVEL_ALWAYS, "Engine stopped (PID [%d])", getpid());
    engine_trace(TRACE_LEVEL_ALWAYS, "-----------------------------------------");

    // TRACES
    trace_stop(engine.trace_hndl);
}

/** ****************************************************************************

  @brief      For a given command read from DB we apply max lines limitation
              and store the rest in the DB again.

  @param[in]  void

  @return     Execution result code (ErrorCode_t)

*******************************************************************************/
ErrorCode_t engine_apply_max_cycle_lines()
{
    ErrorCode_t result = ENGINE_OK;

    // Get configured limit
    int max_lines = atoi(engine.config.params[MAX_LINES_PER_CYCLE_ID]);

    engine_trace(TRACE_LEVEL_ALWAYS, 
        "Applying [%d] lines limit to command [%s]",
        max_lines,
        engine.last_command.code);

    // Extract both the code to be executed and the remaining
    char new_code[MAX_COMMAND_CODE_SIZE+1];
    char remaining_code[MAX_COMMAND_CODE_SIZE+1];

    new_code[0] = 0;
    remaining_code[0] = 0;

    // go line by line
    char* buffer = strdup(engine.last_command.code);
    char* line = strtok(buffer, "\r\n");
    int lines_read = 0;

    char* new_pos = new_code;
    char* remaining_pos = remaining_code;

    while(line) {
        lines_read++;
        if(lines_read <= max_lines) {
            snprintf(new_pos, MAX_COMMAND_CODE_SIZE, "%s\n", line);
            new_pos += (strlen(line) + 1);
        } else {
            snprintf(remaining_pos, MAX_COMMAND_CODE_SIZE, "%s\n", line);
            remaining_pos += (strlen(line) + 1);
        }

        // Get next one
        line  = strtok(NULL, "\n");
    }

    engine_trace(TRACE_LEVEL_ALWAYS, 
        "New code [%s] remaining [%s]",
        new_code,
        remaining_code);

    // Make new code = current and store remaining as new command
    sprintf(engine.last_command.code, "%s", new_code);

    if(remaining_code[0] != 0) {
        Command_t new_command;
        memcpy(&new_command, &engine.last_command, sizeof(new_command));
        sprintf(new_command.code, "%s", remaining_code);
        new_command.email_content = strdup(engine.last_command.email_content);

        result = db_insert_command(&engine.db_connection, &new_command);

        free(new_command.email_content);
    }
    
    free(buffer);

    return result;
}


/******************************* PUBLIC FUNCTIONS ****************************/

/** ****************************************************************************

  @brief      Initializes engine modules using command line options received

  @param[in]  argc, argv Command line options

  @return     Execution result code (ErrorCode_t)

*******************************************************************************/
ErrorCode_t engine_init(int argc, char** argv)
{
	// Initialize engine
	memset(&engine, 0, sizeof(engine));

	engine.status = ENGINE_IDLE_STATUS;

	engine.argc = argc;
	engine.argv = argv;

	engine.error = ENGINE_OK;

    // Get the basename
    engine.exe_name = argv[0];
    char* pos = strrchr(argv[0], '/');
    if(pos)
    {
        engine.exe_name = ++pos;
    }

	// Check PLAT_HOME - mandatory
	engine.plat_home = getenv(PLAT_HOME);

	if(!engine.plat_home)
	{
		engine.error = ENGINE_PLAT_HOME_ERROR;
	}

    if(engine.error == ENGINE_OK)
    {
		signal(SIGINT, signals_handler);
    	signal(SIGTERM, signals_handler);

    	// Define exit callback
    	atexit(engine_stop);
    }

    // Initialize traces
    if(engine.error == ENGINE_OK)
    {
    	engine.error = engine_init_traces();
    }

    engine_trace(TRACE_LEVEL_ALWAYS, "-----------------------------------------------");
    engine_trace(TRACE_LEVEL_ALWAYS, 
        "Engine started (NAME [%s] PID [%d])", engine.exe_name, getpid());
    engine_trace(TRACE_LEVEL_ALWAYS, "-----------------------------------------------");

    // Initialize config
    if(engine.error == ENGINE_OK)
    {
    	engine.error = engine_init_config();
    }

    // update trace level using conf
    trace_set_level(engine.trace_hndl, 
        atoi(engine.config.params[APP_TRACE_LEVEL_ID]));

    // Initialize DB connection
    if(engine.error == ENGINE_OK)
    {
    	engine.error = engine_init_db();
    }

    // stop-all when error
    if(engine.error != ENGINE_OK)
    {
    	engine_stop_all();
    }

	return engine.error;
}

/** ****************************************************************************

  @brief      Runs engine logic loop

  @param[in]  void

  @return     Execution result code (ErrorCode_t)

*******************************************************************************/
ErrorCode_t engine_run()
{
  ErrorCode_t result = ENGINE_OK;

	engine.status = ENGINE_RUNNING_STATUS;

    // Checks periodically for commands, executes them and stores VM status

	while(engine.status == ENGINE_RUNNING_STATUS)
	{
        // reset VM
        engine.last_vm = NULL;

		engine_trace(TRACE_LEVEL_ALWAYS, "Running engine logic");

        // start a transaction
        result = db_start_transaction(&engine.db_connection);

        if(result == ENGINE_OK)
        {
            // get next command
            result = db_get_next_command(&engine.db_connection, &engine.last_command);
        }

        if(result == ENGINE_OK)
        {
            // apply configured max lines per cycle
            result = engine_apply_max_cycle_lines();

            if(result != ENGINE_OK)
            {
                engine_vm_output_cb("Internal error");
            }
        }

        if(result == ENGINE_OK)
        {
            // Get a VM for current agent_id
            result = db_get_agent_vm(&engine.db_connection, 
                engine.last_command.agent_id,
                &engine.last_vm);
        }

        if(result == ENGINE_OK)
        {
            // save last agent input & subject command info
            result = db_update_agent_input(&engine.db_connection, 
                engine.last_command.agent_id, 
                &engine.last_command);
        }            

        char out_buffer[4096+1];
        if(result == ENGINE_OK)
        {
            // Execute the last code in current VM
            memset(out_buffer, 0, 4096+1);
            result = vm_run_command(engine.last_vm, &engine.last_command, out_buffer, 4096);

            if(result != ENGINE_OK)
            {
                engine_vm_output_cb("Command error");
            }
        } 

        if(result == ENGINE_OK)
        {
            // Read VM output and send email
            //const char* output = vm_get_command_output(engine.last_vm);

            if(out_buffer[0] != 0) 
            {
                engine_vm_output_cb(out_buffer);
            }
        }           

        if(result == ENGINE_OK)
        {
            // Save VM in DB
            result = db_save_agent_vm(&engine.db_connection, 
                engine.last_command.agent_id, 
                engine.last_vm);
        }

        // Delete command always to avoid spam when error
        result = db_delete_command(&engine.db_connection, &engine.last_command);

        // commit or rollback current transaction
        if((result == ENGINE_OK) || (result == ENGINE_DB_NOT_FOUND_ERROR))
        {
            result = db_commit_transaction(&engine.db_connection);
        }
        else
        {
            result = db_rollback_transaction(&engine.db_connection);
        }

        // always deallocate VM and command resources
        if(engine.last_vm) vm_free(engine.last_vm);
        if(engine.last_command.email_content)
            engine_free(engine.last_command.email_content, strlen(engine.last_command.email_content) + 1);

		sleep(atoi(engine.config.params[DB_READ_TIME_ID]));
	}

	// deallocate resources/Stop modules
    engine_stop_all();
	
	return engine.error;
}

/** ****************************************************************************

  @brief      Stops engine execution

  @param[in]  void

  @return     void

*******************************************************************************/
void engine_stop()
{
	if((engine.status != ENGINE_STOPPING_STATUS) && 
		(engine.status != ENGINE_STOPPED_STATUS))
	{
		engine.status = ENGINE_STOPPING_STATUS;

		engine_trace(TRACE_LEVEL_ALWAYS, "Engine is stopping...");
	}
}

/** ****************************************************************************

  @brief      Checks if engine is stopped

  @param[in]  void

  @return     Boolean result

*******************************************************************************/
Bool_t engine_is_stopped()
{
	if(engine.status != ENGINE_RUNNING_STATUS)
		return ENGINE_TRUE;

	return ENGINE_FALSE;
}

/** ****************************************************************************

  @brief      Gets last engine error

  @param[in]  void

  @return     Last error code

*******************************************************************************/
int engine_get_last_error()
{
	return engine.error;
}

/** ****************************************************************************

  @brief      Logs an engine trace in output log file

  @param[in]  level Trace level
  @param[in]  trace Trace format text
  @param[in]  ...   Variable list of arguments

  @return     void

*******************************************************************************/
void engine_trace(TraceLevel_t level, const char *trace, ... )
{
	static va_list valist;
	
	va_start(valist, trace);
	trace_write(engine.trace_hndl, level, trace, valist);
	va_end(valist);
}

/** ****************************************************************************

  @brief      Initializes a log line only with date and level, this function must be 
              called before using trace_append

  @param[in]  level  Trace level
  
  @return     void

*******************************************************************************/
void engine_trace_header(TraceLevel_t level)
{
    trace_header(engine.trace_hndl, level);
}

/** ****************************************************************************

  @brief      Appends more text to current log line, no new line is added

  @param[in]  level Trace level
  @param[in]  trace Trace format text
  @param[in]  ...   Variable list of arguments

  @return     void

*******************************************************************************/
void engine_trace_append(TraceLevel_t level, const char *trace, ... )
{
    static va_list valist;
    
    va_start(valist, trace);
    trace_append(engine.trace_hndl, level, trace, valist);
    va_end(valist);
}

/** ****************************************************************************

  @brief      Function used to send VM output to users

  @param[in]  msg      Output msg

  @return     void

*******************************************************************************/
void engine_vm_output_cb(const char* msg)
{
    int agent_id = engine.last_command.agent_id;

    if(msg)
    {
        engine_trace(TRACE_LEVEL_ALWAYS, 
            "Output VM msg [%s] read for agent [%d]",
            msg,
            agent_id); 

        db_update_agent_output(&engine.db_connection, agent_id, (char*)msg);

        // Get agent email info and send it by email
        EmailInfo_t email_info;
        memset(&email_info, 0, sizeof(email_info));

        // Fill the info we have so far
        email_info.agent_id = agent_id;
        // email format
        snprintf(email_info.email_template, 
            MAX_COMMAND_CODE_SIZE, 
            "%s", 
            engine.config.params[SEND_EMAIL_TEMPLATE_ID]);

        snprintf(email_info.message, MAX_COMMAND_CODE_SIZE, "%s", msg); // it is quoted later at email module
        snprintf(email_info.email_script, PATH_MAX, "%s", engine.config.params[SEND_EMAIL_SCRIPT_ID]);

        if(db_get_agent_email_info(&engine.db_connection, &email_info) == ENGINE_OK)
        {
            // agent email
            snprintf(email_info.agent_email, 
                MAX_EMAIL_ADDRESS_SIZE, 
                "%s@%s", 
                email_info.agent_name,
                engine.config.params[AGENTS_EMAIL_DOMAIN_ID]); 

            // send email
            email_send(&email_info);
        }

        if(email_info.input_content)engine_free(email_info.input_content, strlen(email_info.input_content)+1);
    }
    else
    {
        engine_trace(TRACE_LEVEL_ALWAYS, 
            "WARNING: NULL msg received from VM for agent [%d]",
            agent_id); 
    }
}

/** ****************************************************************************

  @brief      Allocation wrapper to control memory used by the engine

  @param[in]  size Size we want to allocate
  
  @return     Pointer to memory allocated or NULL when failed

*******************************************************************************/
void* engine_malloc(size_t size)
{
    void* memory = malloc(size);

    if(memory)
    {
        engine.memory_in_use += size; 

        engine_trace(TRACE_LEVEL_ALWAYS, 
            "Allocated [%ld] bytes (Total memory: [%ld])",
            size,
            engine.memory_in_use);
    }

    return memory;
}

/** ****************************************************************************

  @brief      Free wrapper to control memory used by the engine

  @param[in]  Pointer to memory we want to deallocate
  
  @return     void

*******************************************************************************/
void engine_free(void* memory, size_t size)
{
    if(memory)
    {
        free(memory);

        engine.memory_in_use -= size; 

        engine_trace(TRACE_LEVEL_ALWAYS, 
            "Deallocated [%ld] bytes (Total memory: [%ld])",
            size,
            engine.memory_in_use);
    }
}

/** ****************************************************************************

  @brief      Free wrapper to control memory used by the engine

  @param[in]  Pointer to memory we want to deallocate
  
  @return     void

*******************************************************************************/
const char* engine_get_forth_image_path()
{
    return (const char*)engine.config.params[FORTH_IMAGE_PATH_ID];
}
