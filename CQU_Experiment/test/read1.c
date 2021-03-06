#include "../tcp_pthread/recv_ana.h"
#include "../tcp_pthread/data_ana.h"

unsigned int pData[2][RX_SIZE / 8];
int cpy_count;
int work;

void file_read()
{
    FILE *fps = fopen("../pcie/c_0.log", "r");
    if(fps == NULL){
        printf("File open failed!\n");
        exit(1);
    }
    fread(pData[0], 1, RX_SIZE, fps);
    fclose(fps);
}

//show info
void show_func()
{
    cpy_count = 0;
    int _length;
    int _channle;
    int cpy_count1;
    long long _timestamp;
    char buf[100] = {'\0'};
    FILE *fp[CHANNEL_NUM];

        sprintf(buf, "data/c_1.log");
        fp[1] = fopen(buf, "w+");


    while (work == 1)
    {
        //sem_wait(sem);
        for (int k = 0; k < 1; k++)
        {
            while (cpy_count < RX_SIZE / 8)
            {
                for (int i = 0; i < 1030; i++) //检测board_head位置
                {
                    if (*(pData[k] + cpy_count + i) == 0x00000000 && *(pData[k] + cpy_count + i + 1) != 0)
                    {
                        printf("Get correct head!   %d\n", i);
                        cpy_count += i; //conduct 0x040000ff and board_head
                        break;
                    }
                    else if (i > 1025)
                    {
                        printf("Read head error!\n");
                        exit(1);
                    }
                }
                for( int i = 0; i < 50; i++){
                    printf("%x\n", *(pData[0] + i + cpy_count));
                }
                
                //bit_head_read(pData[k] + cpy_count, 'b');
                cpy_count++;
                cpy_count1 = cpy_count;
                printf("仅显示前4个ADC_head info!\n");
                for (int i = 0; i < 4; i++)
                {
                    bit_head_read(pData[k] + cpy_count, 'f');
                    _length = bit_head_read(pData[k] + cpy_count, 'l');
                    cpy_count += _length + 3;
                }
                
                cpy_count = cpy_count1;
                cpy_count1 = 0;
                
                while (*(pData[k] + cpy_count) != 0 ||
                       *(pData[k] + cpy_count + 1) != 0)
                {
                    
                    //printf("cpy_count: %d\n", cpy_count);
                    
                    _length = bit_head_read(pData[k] + cpy_count, 'l');
                    
                    _channle = bit_head_read(pData[k] + cpy_count, 'c');
                    
                    _timestamp = bit_time_read(pData[k] + cpy_count);
                    
                    cpy_count += 3;
                    for (int i = 0; i < _length; i++)
                    {
                        printf("sllslsl     %d\n", _channle);
                        fprintf(fp[1], "%08d    %09.8f\n", 2 * i * 8,
                         bit_float_read(pData[k] + cpy_count + i, cpy_count1 & 1));
                        //printf("sllslsl\n");
                        cpy_count1++;
                        fprintf(fp[1], "%08d    %09.8f\n", 2 * i * 8 + 8,

                         bit_float_read(pData[k] + cpy_count + i, cpy_count1 & 1));
                        cpy_count1++;
                    }
                    fclose(fp[1]);
                    
                    exit(1);
                    cpy_count += _length;
                }
                cpy_count1 = 0;
                while (*(pData[k] + cpy_count) == 0 &&
                 *(pData[k] + cpy_count + 1) == 0) //确定是否读到board最后
                {
                    cpy_count++;
                    cpy_count1++;
                    if(cpy_count1 > 50){
                        printf("Data read finished!\n");
                        cpy_count = RX_SIZE / 8;
                        break;
                    }
                }
            }
        }
        break;
        //sem_post(sem + 1);
    }
}

int main(){
    work = 1;

    file_read();
    /* int k = 0;

    for(int i = 0; i < RX_SIZE / 8; i++){
        printf("%x\n", *(pData[0] + i));
        if(*(pData[0] + i) == 0){
            k++;
            if(k > 30){
                break;
            }
        }
    } */

    show_func();
    printf("sllslsl\n");

    return 0;
}