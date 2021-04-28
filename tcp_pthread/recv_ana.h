#ifndef _RECV_ANA_H_
#define _RECV_ANA_H_

#include <arpa/inet.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>

#endif

#define PACK_SIZE (4 * 1024)

typedef struct board_head
{
    char board_type[8];
    char board_addr[8];
    char Ftype[2];
    char Error[14];
} BOARD_HEAD;
typedef struct frame_head
{
    char channel_id[8];
    char error[6];
    char Ftype[2];
    char length[16];
    //char timestamp_H[32];
    //char timestamp_L[32];
    char timestamp[64];
} FRAME_HEAD;

//根据is不同的返回值, 对head结构体进行不同的处理
int64_t struct_head_read(void *body, char is)
{
    if (!body || is == 0)
    {
        printf("Void pointer or error arg!");
        return -1;
    }
    char outbuf[4][32] = {'\0'};

    switch (is)
    {
    case 'b': //输出board info
        BOARD_HEAD *bb = (BOARD_HEAD *)body;
        memcpy(outbuf[0], bb->board_addr, 8);
        memcpy(outbuf[1], bb->board_type, 8);
        memcpy(outbuf[2], bb->Error, 14);
        memcpy(outbuf[3], bb->Ftype, 2);
        printf("**********BOARD INFO**********\n");
        printf("Board type:%s \nBoard addr:%s\nBoard Ftype:%s\nBoard Error:%s\n\n", outbuf[1],
               outbuf[0], outbuf[3], outbuf[2]);
        return 0;
        break;

    case 'f': //输出frame info
        FRAME_HEAD *cc = (FRAME_HEAD *)body;
        memcpy(outbuf[0], cc->channel_id, 8);
        memcpy(outbuf[1], cc->error, 6);
        memcpy(outbuf[2], cc->Ftype, 2);
        memcpy(outbuf[3], cc->length, 16);
        printf("**********FRAME INFO**********\n");
        printf("channel_id: %s\nError: %s\nFtype: %s\nLength: %s\n\n",
               outbuf[0], outbuf[1], outbuf[2], outbuf[3]);
        return atoi(outbuf[3]);
        break;

    case 'l': //返回adc data 的length（64位整型）
        FRAME_HEAD *cc = (FRAME_HEAD *)body;
        memcpy(outbuf[3], cc->length, 16);
        return atoi(outbuf[3]);
        break;

    case 'c': //返回adc data's channel_id(int64_)
        FRAME_HEAD *cc = (FRAME_HEAD *)body;
        memcpy(outbuf[0], cc->channel_id, 8);
        return atoi(outbuf[0]);
        break;

    case 't': //back adc data's timestamp(int64_)
        FRAME_HEAD *cc = (FRAME_HEAD *)body;
        char timest[65] = {'\0'};
        memcpy(timest, cc->timestamp, 64);
        return atol(timest);
        break;
    }
}

//根据CPU创建和分配线程
void ptd_create(pthread_t *arg, int k, void *functionbody)
{
    int ret;

    pthread_attr_t attr;

    ret = pthread_attr_init(&attr); //初始化线程属性变量,成功返回0,失败-1
    if (ret < 0)
    {
        perror("Init attr fail");
        exit(1);
    }

    /* if (k != -1)
    { //k为-1时不使用核心亲和属性
        cpu_set_t cpusetinfo;
        CPU_ZERO(&cpusetinfo);
        CPU_SET((CPU_CORE - 1 - k), &cpusetinfo); //将core1加入到cpu集中,同理可以将其他的cpu加入

        ret = pthread_attr_setaffinity_np(&attr, sizeof(cpusetinfo), &cpusetinfo);
        if (ret < 0)
        {
            perror("Core set fail");
            exit(1);
        }
    } */

    /* ret = pthread_attr_setscope(&attr, PTHREAD_SCOPE_PROCESS);//PTHREAD_SCOPE_SYSTEM绑定;PTHREAD_SCOPE_PROCESS非绑定
	if(ret < 0) {
		perror("Setscope fail");
		exit(1);
	} */
    ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED); //线程分离属性:PTHREAD_CREATE_JOINABLE（非分离）
    if (ret < 0)
    {
        perror("Detached fail");
        exit(1);
    }

    pthread_create(arg, &attr, (void *(*)(void *))functionbody, NULL);
    //printf("Id为%d的线程已创建完毕。", *arg);

    pthread_attr_destroy(&attr); //销除线程属性
}

//读取数据
//主要将全为0/1的数据转为正负5V的double;同时也可以输出数据前4位的channel_id
double data_read_func(char *inp, char is)
{
    switch (is)
    {
    case 'v':
        double m = 10. / 4096.; //2^12 = 4096
        int sum = 0;
        for (int i = 15; i > 3; i--)
        {
            if (*(inp + i) == '1')
            {
                sum += ldexp(1, 15 - i);
            }
        }
        return (double)sum * m;
        break;

    case 'c':
        char no[5] = {'\0'};
        memcpy(no, inp, 4);
        return (double)atoi(no);
        break;
    }
}

//speed test pthread
void *speed_test(void *count)
{
    long *cnt = (long *)count; //注释掉,使用global identifier代替
    struct timeval systime;
    long time0, time1, time2 = 0, cnt1 = 0;
    double speed0, speed1;
    int size_of_every_time = 1024; //1024kB
    gettimeofday(&systime, 0);
    time0 = systime.tv_sec * 1e6 + systime.tv_usec;
    while (1)
    {
        gettimeofday(&systime, 0);
        time1 = systime.tv_sec * 1e6 + systime.tv_usec - time0;
        while (time1 - time2 > 1)
        { //calculate every second
            //speed in one seconds
            speed0 = (double)(*cnt - cnt1) / (double)((time1 - time2) * 1e6 * size_of_every_time);
            //total average speed
            speed1 = (double)(*cnt) / (double)(time1 * 1e6 * size_of_every_time);
            printf("Average speed: %lfMB/s, second speed: %lfMB/s\n",
                   speed0, speed1);
            time2 = time1;
            cnt1 = *cnt;
        }
    }
}

//transform string to int64_t
int64_t atoi64_t(char *arrTmp)
{
    int len = strlen(arrTmp);
    int64_t ret = 0;
    if (arrTmp == NULL)
    {
        return 0;
    }
    for(int i = len - 1; i >= 0; i--){
        ret += (*(arrTmp + i) - 48) * (int64_t)pow(10, len - i - 1);
    }
    return ret;
}