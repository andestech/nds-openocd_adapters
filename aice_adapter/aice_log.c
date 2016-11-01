#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include <unistd.h>
#include <stdbool.h>
#include "log.h"


#ifdef __MINGW32__ 

#include <windows.h>
void usleep(__int64 usec) 
{ 
    HANDLE timer; 
    LARGE_INTEGER ft; 

    ft.QuadPart = -(10*usec); // Convert to 100 nanosecond interval, negative value indicates relative time

    timer = CreateWaitableTimer(NULL, TRUE, NULL); 
    SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0); 
    WaitForSingleObject(timer, INFINITE); 
    CloseHandle(timer); 
}

#endif 


const char *Log_File_Name[2]={
    "aice_adapter_0.log",
    "aice_adapter_1.log",
};

static bool enable_debug_;
static FILE *debug_fd_;
static uint32_t debug_level_    = AICE_LOG_ERROR;
static uint32_t debug_buf_size_ = MINIMUM_DEBUG_LOG_SIZE;
static int  file_idx = 0;
char *log_output = NULL;

#define MAX_FILENAME 2048
static void open_debug_file (void)
{
    char debug_filename[MAX_FILENAME];
    time_t seconds;

    seconds = time (NULL);

    memset(debug_filename, 0, MAX_FILENAME);
    if( log_output != NULL ) {
        strncpy(debug_filename, log_output, strlen(log_output));
    }

    strcat(debug_filename, Log_File_Name[file_idx]);
    debug_fd_ = fopen (debug_filename, "w");

    // open failed
    if(!debug_fd_) {
        debug_fd_ = stdout;
    }



    fprintf (debug_fd_, "Date: %s\n\n", ctime (&seconds));
}

static void close_debug_file (void)
{
    fclose (debug_fd_);
}

void aice_log_init (uint32_t a_buf_size, uint32_t a_debug_level)
{
    if (a_buf_size < MINIMUM_DEBUG_LOG_SIZE)
        a_buf_size = MINIMUM_DEBUG_LOG_SIZE;

    debug_buf_size_ = a_buf_size;
    debug_level_    = a_debug_level;
    enable_debug_   = true;

    open_debug_file ();
}

void aice_log_finalize (void)
{
    close_debug_file ();
}

static void check_file_size()
{
    unsigned FileSize = 0;

    if( debug_fd_ ) {
        FileSize = ftell(debug_fd_);
    }

    if( FileSize >= debug_buf_size_  ||
        debug_fd_ == NULL ) {
        if(debug_fd_)
            fclose(debug_fd_);

        file_idx ^= 0x01;
        open_debug_file();
    }
}


void aice_log_add (uint32_t a_level, const char *a_format, ...)
{
    if ((enable_debug_ == false) || (a_level > debug_level_))
        return;

    //Ping Pong Buffers 
    check_file_size();

    va_list ap;

    va_start (ap, a_format);
    vfprintf (debug_fd_, a_format, ap);
    va_end (ap);

    fflush (debug_fd_);
    fprintf (debug_fd_, "\n");
    fflush (debug_fd_);
}


/* if we sleep for extended periods of time, we must invoke keep_alive() intermittantly */
void alive_sleep(uint64_t ms)
{
    uint64_t napTime = 10;
    for (uint64_t i = 0; i < ms; i += napTime) {
        uint64_t sleep_a_bit = ms - i;
        if (sleep_a_bit > napTime)
            sleep_a_bit = napTime;

        usleep(sleep_a_bit * 1000);
        //keep_alive();
    }
}

