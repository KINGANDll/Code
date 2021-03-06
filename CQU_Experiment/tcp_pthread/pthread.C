#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

void *functionbody(void *arg);

void ptd_create(void *arg, void *functionbody) {
	int ret;

	cpu_set_t cpusetinfo;
	CPU_ZERO(&cpusetinfo);
	CPU_SET(1, &cpusetinfo);//将core1加入到cpu集中,同理可以将其他的cpu加入


	pthread_t ptd;
	pthread_attr_t attr;

	ret = pthread_attr_init(&attr);//初始化线程属性变量,成功返回0,失败-1
	if(ret < 0) {
		perror("Init attr fail");
		exit(1);
	}
	/* ret = pthread_attr_setscope(&attr, PTHREAD_SCOPE_PROCESS);//PTHREAD_SCOPE_SYSTEM绑定;PTHREAD_SCOPE_PROCESS非绑定
	if(ret < 0) {
		perror("Setscope fail");
		exit(1);
	} */
	ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);//线程分离属性:PTHREAD_CREATE_JOINABLE（非分离）
	if(ret < 0) {
		perror("Detached fail");
		exit(1);
	}
	ret = pthread_attr_setaffinity_np(&attr, sizeof(cpusetinfo), &cpusetinfo);
	if(ret < 0) {
		perror("Core set fail");
		exit(1);
	}
	
	pthread_create(&ptd, &attr, (void *(*)(void *))functionbody, (void *)arg);
	
	pthread_attr_destroy(&attr);//消除线程属性
}

int main() {
	return 0;
}

void *functionbody(void *arg) {
	return NULL;
}

typedef struct board_head
{
	char board_type[8];
	char board_addr[8];
	char Ftype[2];
	char Error[14];
} BOARD_HEAD;

typedef struct frame_head
{
	char channle_id[8];
	char error[6];
	char Ftype[2];
	char length[16];
	//char timestamp_H[32];
	//char timestamp_L[32];
	char timestamp[64];
} FRAME_HEAD;

typedef struct adc_data
{
	FRAME_HEAD adc_head;
	//char adc_data1[16];
	//char adc_data2[16];
} ADC_DATA;

typedef struct tdc_data
{
	FRAME_HEAD tdc_head;
	char tdc_data[32];
} TDC_DATA;
BOARD_HEAD board_head;
ADC_DATA adc_data;
TDC_DATA tdc_data;

typedef struct data_head{
	char channle_count[8];
	char data_length[16];
	char other[8];
}DATA_HEAD;