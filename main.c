#define _GNU_SOURCE

#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
#include "time.h"

#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <limits.h>

#ifndef	CONSUMER
#define	CONSUMER	"Consumer"
#endif

#define READ 1

typedef unsigned int UINT;
typedef struct timespec timespec_t;

void msleep(UINT milisec) 
{
    time_t sec= (int)(milisec/1000);
    milisec = milisec - (sec*1000);
    timespec_t req= {0};
    req.tv_sec = sec;
    req.tv_nsec = milisec * 1000000L;
    if(nanosleep(&req,&req)) perror("nanosleep");
}

int main(int argc, char **argv)
{
	char *chipname = "gpiochip0";
	unsigned int line_num = 13;	// GPIO Pin #13
	struct timespec bounceTimeout = { 0, 100000000 };
    struct timespec appTimeout = { 10, 0 };
	struct gpiod_line_event event;
	struct gpiod_chip *chip;
	struct gpiod_line *line;
	int i, ret, newVal, oldVal;

	chip = gpiod_chip_open_by_name(chipname);
	if (!chip) {
		perror("Open chip failed\n");
		ret = -1;
		goto end;
	}

	line = gpiod_chip_get_line(chip, line_num);
	if (!line) {
		perror("Get line failed\n");
		ret = -1;
		goto close_chip;
	}	

	
	i = 0;	
    bool secondEvent = false;
    bool firstLetter = true;
    int prevVal = 1;
    timespec_t prevTime;
    timespec_t lastTime;
	
    
    while (true)
    {
        i++;
        
        // request events
        ret = gpiod_line_request_both_edges_events(line, CONSUMER);
	    if (ret < 0) 
        {
		    perror("Request event notification failed\n");
		    ret = -1;
		    goto release_line;
        }        

        ret = gpiod_line_event_wait(line, &appTimeout);
        if (ret < 0) 
        {
            perror("Wait event notification failed\n");
            ret = -1;
            goto release_line;
        }
        else if (ret == 0) // appTimeout
        {
            printf("Input ended\n");
            goto release_line;
        }

        ret = gpiod_line_event_read(line, &event);
        if (ret < 0) 
        {
            perror("Read last event notification failed\n");
            ret = -1;
            goto release_line;
        }

        lastTime = event.ts;
        printf("\n");
        printf("INITIAL event notification on line #%u %d times\n", line_num, i);
        printf("Event type: %d\n", event.event_type);
        printf("Event time: %lo sec %lo nsec\n", lastTime.tv_sec, lastTime.tv_nsec);
        printf("\n");
        
        while(true) // wait for bounce events to end
        {
            ret = gpiod_line_event_wait(line, &bounceTimeout);
            if (ret < 0) 
            {
                perror("Wait event notification failed\n");
                ret = -1;
                goto release_line;
            }
            if (ret == 0) // timeout
            {
                break;
            }

            ret = gpiod_line_event_read(line, &event);
            if (ret < 0) 
            {
                perror("Read last event notification failed\n");
                ret = -1;
                goto release_line;
            }

            lastTime = event.ts;
            //printf("\n");
            printf("BOUNCE event notification on line #%u %d times\n", line_num, i);
            //printf("Event type: %d\n", event.event_type);
            //printf("Event time: %lo sec %lo nsec\n", lastTime.tv_sec, lastTime.tv_nsec);
            //printf("\n");
        }

        printf("\n");
        printf("End of bounces!\n");
        printf("\n");
        
        // release line from event
        gpiod_line_release(line);

        //request input        
        ret = gpiod_line_request_input(line, CONSUMER);
        if (ret < 0) 
        {
            perror("Request line as input failed\n");
            goto release_line;
        }

        newVal = gpiod_line_get_value(line);
        if (newVal < 0) 
        {
            perror("Read line input failed\n");
            goto release_line;
        }

        //printf("Input %d on line #%u\n", newVal, line_num);

        // printf("PREV Input %d on line #%u\n", prevVal, line_num);
        // printf("FINAL Input %d on line #%u\n", newVal, line_num);

        //process input
        // if (firstLetter || (newVal == 0 & prevVal == 0))
        // {
        //     gpiod_line_release(line);            
        //     continue;
        // }

        printf("lastTime: %ld.%ld\n", lastTime.tv_sec, lastTime.tv_nsec);
        printf("prevTime: %ld.%ld\n", prevTime.tv_sec, prevTime.tv_nsec);
        int duration = (int)((lastTime.tv_sec - prevTime.tv_sec) * 1000 + (lastTime.tv_nsec - prevTime.tv_nsec) / 1000000); 
        printf("%d -> %dms -> %d", prevVal, duration, newVal);
        printf("\n");

        if (newVal == 1 && prevVal == 0)
        {
            // firstLetter
            // if (firstLetter)
            // {
            //     firstLetter = false; 
            //     gpiod_line_release(line);            
            //     continue;
            // }
            
            // not firstLetter, so calculate duration
            // int duration = (int)((lastTime.tv_sec - prevTime.tv_sec) * 1000 + (lastTime.tv_nsec - prevTime.tv_nsec) / 1000000); 
            // printf("duration: %dms", duration);
            // printf("\n");

            prevVal = 1;
            prevTime = lastTime;
        }
        else if (newVal == 0 && prevVal == 1)
        {
            // int duration = (int)((lastTime.tv_sec - prevTime.tv_sec) * 1000 + (lastTime.tv_nsec - prevTime.tv_nsec) / 1000000); 
            // printf("duration: %dms", duration);
            // printf("\n");

            prevVal = 0;
            prevTime = lastTime;            
        }
        else
        {
            prevTime = lastTime;
        }





        gpiod_line_release(line);
	}

	ret = 0;

release_line:
	gpiod_line_release(line);
close_chip:
	gpiod_chip_close(chip);
end:
	return ret;
}
