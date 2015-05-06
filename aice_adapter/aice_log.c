#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include <unistd.h>
#include <stdbool.h>
#include "aice_log.h"


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

static uint32_t start_index_;
static uint32_t end_index_;
static bool full_;
static bool enable_debug_;
static uint32_t remain_debug_buf_size_;
static char *debug_buf_;
static FILE *debug_fd_;
static uint32_t debug_level_    = AICE_LOG_ERROR;
static uint32_t debug_buf_size_ = MINIMUM_DEBUG_LOG_SIZE;
static bool log_unlimited_ = false;
static int  file_idx = 0;


static void open_debug_file (void)
{
    const char *debug_filename;
    time_t seconds;

    seconds = time (NULL);

    debug_filename = Log_File_Name[file_idx];
    debug_fd_ = fopen (debug_filename, "w");

    fprintf (debug_fd_, "Date: %s\n\n", ctime (&seconds));
}

static void close_debug_file (void)
{
    fclose (debug_fd_);
}

static void append_to_full_debug_buf (const char *a_content, uint32_t a_content_len)
{
    if (a_content_len < remain_debug_buf_size_)
    {
        strncpy (debug_buf_ + end_index_, a_content, a_content_len);
        end_index_ += a_content_len;
        debug_buf_[end_index_] = '\0';
        remain_debug_buf_size_ -= a_content_len;
        start_index_ = end_index_ + 1;
        if (start_index_ == debug_buf_size_)
            start_index_ = 0;
    }
    else // a_content_len >= remain_debug_buf_size_
    {
        strncpy (debug_buf_ + end_index_, a_content, remain_debug_buf_size_);
        strncpy (debug_buf_, a_content + remain_debug_buf_size_, a_content_len - remain_debug_buf_size_);
        end_index_ = a_content_len - remain_debug_buf_size_;
        debug_buf_[end_index_] = '\0';
        start_index_ = end_index_ + 1;
        remain_debug_buf_size_ = debug_buf_size_ - end_index_;
    }
}

static void append_to_debug_buf (const char *a_content, uint32_t a_content_len)
{
    if (full_ == true)
    {
        append_to_full_debug_buf (a_content, a_content_len);
        return;
    }

    if (a_content_len < remain_debug_buf_size_)
    {
        strncpy (debug_buf_ + end_index_, a_content, a_content_len);
        end_index_ += a_content_len;
        debug_buf_[end_index_] = '\0';
        remain_debug_buf_size_ -= a_content_len;
    }
    else // a_content_len >= remain_debug_buf_size_
    {
        strncpy (debug_buf_ + end_index_, a_content, remain_debug_buf_size_);
        strncpy (debug_buf_, a_content + remain_debug_buf_size_, a_content_len - remain_debug_buf_size_);
        end_index_ = a_content_len - remain_debug_buf_size_;
        debug_buf_[end_index_] = '\0';
        start_index_ = end_index_ + 1;
        remain_debug_buf_size_ = debug_buf_size_ - end_index_;
        full_ = true;
    }
}

static void log_dump (void)
{
    if (log_unlimited_ == true)
        return;

    open_debug_file ();

    if (start_index_ == 0)
    {
        fprintf (debug_fd_, "%s", debug_buf_);
    }
    else
    {
        fprintf (debug_fd_, "%s", debug_buf_ + start_index_);
        fprintf (debug_fd_, "%s", debug_buf_);
    }

    close_debug_file ();
}

void aice_log_init (uint32_t a_buf_size, uint32_t a_debug_level, bool a_unlimited)
{
    if (a_buf_size < MINIMUM_DEBUG_LOG_SIZE)
        a_buf_size = MINIMUM_DEBUG_LOG_SIZE;

    debug_buf_size_ = a_buf_size;
    debug_level_ = a_debug_level;
    log_unlimited_ = a_unlimited;

    start_index_ = 0;
    end_index_ = 0;
    full_ = 0;
    enable_debug_ = true;

    if (log_unlimited_)
    {
        open_debug_file ();
    }
    else
    {
        remain_debug_buf_size_ = debug_buf_size_;

        debug_buf_ = malloc (debug_buf_size_ + 1);   // +1 for sentinel '\0'
        debug_buf_[0] = '\0';
        debug_buf_[debug_buf_size_] = '\0';
    }
}

void aice_log_finalize (void)
{
    log_dump ();

    if (log_unlimited_)
        close_debug_file ();
    else
        free (debug_buf_);
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
    char working_buf[WORKING_BUF_SIZE];
    int working_str_len;

    va_start (ap, a_format);
    if (log_unlimited_ == false)
        vsprintf (working_buf, a_format, ap);
    else
        vfprintf (debug_fd_, a_format, ap);
    va_end (ap);

    if (log_unlimited_ == false)
    {
        strcat(working_buf, "\n");
        working_str_len = strlen (working_buf);

        append_to_debug_buf (working_buf, working_str_len);
    }
    else {
        fflush (debug_fd_);
        fprintf (debug_fd_, "\n");
        fflush (debug_fd_);
    }
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




