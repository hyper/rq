// settings.c

#include "settings.h"

#include <assert.h>


void settings_init(settings_t *ptr)
{
	int i;
	
	assert(ptr != NULL);

	ptr->port = RQ_DEFAULT_PORT;
	ptr->maxconns = DEFAULT_MAXCONNS;
	ptr->verbose = false;
	ptr->daemonize = false;
	ptr->username = NULL;
	ptr->pid_file = NULL;

	ptr->interfaces = 0;
	for (i=0; i<MAX_INTERFACES; i++) {
		ptr->interface[i] = NULL;
	}

	ptr->logfile = NULL;
}

void settings_cleanup(settings_t *ptr) 
{
	assert(ptr != NULL);
}
