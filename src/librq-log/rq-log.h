#ifndef __RQ_LOG_H
#define __RQ_LOG_H

#define RQ_LOG_VERSION  0x00001000
#define RQ_LOG_VERSION_TEXT  "v0.10.00"



#include <rq.h>
#include <expbuf.h>

// Debug levels.  When settings a filter, it means that it will ignore all
// entries lower than the filter number.   Ie, if the filter is set at 30,
// then it will ignore all log entries that have a level less than 30, but
// will process 30, 40 and 50, etc. 
#define LOG_DEBUG   10
#define LOG_INFO    20
#define LOG_WARN    30
#define LOG_ERROR   40
#define LOG_FATAL   50

#define LOG_DEBUG_T "DEBUG "
#define LOG_INFO_T  "INFO "
#define LOG_WARN_T  "WARN "
#define LOG_ERROR_T "ERROR "
#define LOG_FATAL_T "FATAL "
#define LOG_UNKNOWN_T "UNKNOWN "


//                                          null param (0 to 63)
#define LOG_CMD_NOP              0
#define LOG_CMD_CLEAR            1
#define LOG_CMD_EXECUTE          2
#define LOG_CMD_SETLEVEL         3
//                                          byte integer (64 to 95)
#define LOG_CMD_LEVEL            64
//                                          short integer (96 to 127)
//                                          large integer (128 to 159)
//                                          short string (160 to 192)
//                                          string (192 to 223)
#define LOG_CMD_TEXT             192
//                                          large string (224 to 255)



typedef struct {
  rq_t *rq;
  short int level;
  char *queue;
  expbuf_t buffer;		// generic buffer that is mutli-use, but should be empty when finished.
  rq_message_t *msg;
} rq_log_t;



void rq_log_init(rq_log_t *log);
void rq_log_free(rq_log_t *log);

void rq_log_attach(rq_log_t *log, rq_t *rq);
void rq_log_setqueue(rq_log_t *log, const char *queue);
 
void rq_log(rq_log_t *log, short int level, char *format, ...);


#endif
