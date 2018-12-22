#ifndef __GETLANHOST__
#define __GETLANHOST__

#include <time.h>
struct lanhost{
	int stat; //0:find in arp,1:not find,need del
	char mac[20];
	char ip[16];
	time_t online_t;
	char online[32];
	char offline[32];
	char devname[32];
	int type;//0:wire,1:wireless
	struct lanhost *next;
};

extern pthread_rwlock_t rwlock;
void *thread_update_lanhost(void*);
int get_lanhost(char*,int);

#endif
