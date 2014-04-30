#ifndef __AICE_USB_HELPER_H__
#define __AICE_USB_HELPER_H__

#include <sys/time.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#define alive_sleep(x)	usleep(x*1000)
#define keep_alive()	do {} while(0)

static int64_t timeval_ms()
{
	struct timeval now;
	int retval = gettimeofday(&now, NULL);
	if (retval < 0)
		return retval;
	return (int64_t)now.tv_sec * 1000 + now.tv_usec / 1000;
}

#ifdef _WIN32
static inline unsigned usleep(unsigned int usecs)
{
	Sleep((usecs/1000));
	return 0;
}
#endif

#endif /* __AICE_USB_HELPER_H__ */
