#include "ana.h"

//需要被初始化以及随环境改变的参数
int client_num = 2;
int channel_num = 8;
int min_channel = 1;
int port = 10000;

//功能指示
int work;
static sem_t sem[4];
struct sockaddr_in clientaddr[10] = {0};
FILE *error_fp; //错误报告

//PCIE读取 变量
int c2h_fd[2];
int h2c_fd;
int control_fd;
void *control_base;
int interrupt_fd;
int read_end_addr;
unsigned int pData[PACK_SIZE];

//socket 变量
int acfd[8];

//统计变量
int socket_count;

/*中断处理进程*/
void *event_process()
{
    interrupt_fd = open_event("/dev/xdma0_events_0"); //打开用户中断
    while (work != -1)
    {
        sem_wait(sem + 0);
        printf("debug: Start read interrupt !\n");
        read_event(interrupt_fd); //获取用户中断
        printf("debug: read_event finished\n");
        read_end_addr = (int)read_control(control_base, 0x0008); //read interrupt reg
        write_control(control_base, 0x0000, 0xFFFFFFFF);         //ack interrupt
        sem_post(sem + 1);
    }
}

void *rx_process(int id)
{
    while (work != -1)
    {
        sem_wait(sem + 1);
        printf("debug: Start read data !\n");
        //printf("read_end_addr = %x \n", read_end_addr);
        lseek(c2h_fd[id], (read_end_addr - (50 - id * 25) * 1024 * 1024), SEEK_SET);
        read(c2h_fd[id], pData, sizeof(pData)); //10MB

        printf("debug: read data finished!\n");
        sem_post(sem + 2);
    }
}

//open such device func:
void dev_open_fun()
{
    control_fd = open_control("/dev/xdma0_bypass");           //打开bypass字符设备
    control_base = mmap_control(control_fd, MAP_BYPASS_SIZE); //获取bypass映射的内存地址

    c2h_fd[0] = open(DEVICE_NAME_C2H_0, O_RDONLY | O_NONBLOCK); //打开pcie c2h设备
    if (c2h_fd[0] == -1)
    {
        printf("PCIe c2h device open failed!\n");
    }
    else
        printf("PCIe c2h device open successful!\n");

    mmap(0, MAP_SIZE, PROT_READ, MAP_SHARED, c2h_fd[0], 0);

    printf("c2h0 device Memory map successful!\n");

    /*     c2h_fd[1] = open(DEVICE_NAME_C2H_1, O_RDONLY | O_NONBLOCK); //打开pcie c2h_1设备
    if (c2h_fd[1] == -1)
    {
        printf("PCIe c2h_1 device open failed!\n");
    }
    else
        printf("PCIe c2h_1 device open successful!\n");

    mmap(0, MAP_SIZE, PROT_READ, MAP_SHARED, c2h_fd[1], 0);

    printf("c2h[1] device Memory map successful!\n"); */

    h2c_fd = open(DEVICE_NAME_H2C_0, O_WRONLY | O_NONBLOCK); //打开pcie  h2c设备
    if (h2c_fd == -1)
    {
        printf("PCIe h2c device open failed!\n");
    }
    else
        printf("PCIe h2c device open successful!\n");

    mmap(0, MAP_SIZE, PROT_WRITE, MAP_SHARED, h2c_fd, 0);

    printf("h2c device Memory map successful!\n");
}

//socket和线程创建函数
void socket_create()
{
    int ret;
    int on = 1;
    int i;
    int socket_fd;

    int send_info[4];
    char buf[10] = {'\0'};
    socklen_t sendbuflen = 0;
    socklen_t len = sizeof(sendbuflen);

    // debug
    // sendbuflen = RX_SIZE;
    // setsockopt(socket_fd, SOL_SOCKET, SO_SNDBUF, (void *)&sendbuflen, len);
    // getsockopt(socket_fd, SOL_SOCKET, SO_SNDBUF, (void *)&sendbuflen, &len);
    // printf("debug: now,sendbuf:%d\n", sendbuflen);

    struct sockaddr_in localaddr = {0};
    localaddr.sin_family = AF_INET;
    localaddr.sin_port = htons(port);
    localaddr.sin_addr.s_addr = INADDR_ANY;

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0)
    {
        printf("Socket fail!\n");
        exit(1);
    }
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    ret = bind(socket_fd, (struct sockaddr *)&localaddr, sizeof(localaddr));
    if (ret < 0)
    {
        printf("Bind fail!\n");
        exit(1);
    }
    ret = listen(socket_fd, 10);
    if (ret < 0)
    {
        printf("Listen fail!\n");
        exit(1);
    }

    for (i = 0; i < client_num; i++)
    {
        printf("等待第%d个客户端连接... %d/%d\n", i + 1, i + 1, client_num);
        *(acfd + i) = accept(socket_fd, (struct sockaddr *)&clientaddr[i], &len);
        if (acfd[i] < 0)
        {
            perror("Accept fail\n");
            exit(1);
        }
        send_info[0] = i;
        send_info[1] = channel_num;
        send_info[2] = min_channel - 1;
        send_info[3] = client_num;
        //sprintf(buf, "%d", i);
        ret = send(*(acfd + i), send_info, sizeof(send_info), 0);
        if (ret != 16)
        {
            fprintf(error_fp, "\nSend send_info failed: [0]%d [1]%d]\n", send_info[0], send_info[1]);
            perror("Send send_info failed!\n");
            fclose(error_fp);
            exit(1);
        }
        memset(buf, '\0', sizeof(buf));
        recv(*(acfd + i), buf, 10, 0);
        if (strcmp(buf, "ok") != 0)
        {
            printf("Recv OK error!\n");
            fprintf(error_fp, "\nRecv ok failed : buf[%s]\n", buf);
            fclose(error_fp);
            exit(1);
        }
        printf("第%d个客户端已连接!\n", i + 1); //希望能够显示连接的客户端地址
    }
}

void *data_part_send()
{
    int ret;
    int send_num;

    int board_num, send_ll, send_length;
    int mark_send[2];

    socket_count = 0;
    printf("debug: data part and send start !\n");
    while (work != -1)
    {
        board_num = BOARD_NUM;
        send_length = 0;
        sem_wait(sem + 2);
        printf("debug: start part and send data\n");

        
        for (int i = 1; i <= client_num; i++)
        {
            ret = board_num / client_num + 1;
            if (i == client_num)
            {
                send_num = (BOARD_NUM - (i - 1) * ret) * 1024;
            }else{
                send_num = ret * 1024;
            }

            ret = find_board_head(pData + send_num - 1024, 0, 0);
            if (ret == 100)
            {
                fprintf(error_fp, "cant find the last board! recv_count:%d\n", socket_count);
                write_data_error_log(&error_fp, pData, PACK_SIZE, 0);
                break;
            }
            printf("debug: 1 find the last board success!\n");

            //debug
            // write_data_error_log(&error_fp, pData + start_address, end_address - start_address, 0);
            // exit(1);
            printf("debug: send_num: %d\n", send_num);

            for(send_ll = 0; send_ll < send_num * 4;){
                ret = send(acfd[i - 1], (char *)pData + (send_length) * 4 + send_ll, send_num * 4 - send_ll, 0);
                printf("debug: send ret = %d\n", ret);
                if (ret < 0)
                {
                    printf("Data send failed! socket_count: %d, send_ll: %d\n", socket_count, send_ll);
                    perror("send failed!");
                    fprintf(error_fp, "\nData send failed! socket_count: %d, send_ll: %d\n", socket_count, send_ll);
                    write_data_error_log(&error_fp, pData, PACK_SIZE, 0);
                    exit(1);
                }
                send_ll += ret;
            }

            printf("debug: 第%d次发送: to %d client. board_num: %d\n", socket_count, i, send_num / 1024);
            send_length += send_num - 1024;
        }
        while (work == 1);

        memset(pData, 0, sizeof(pData));
        sem_post(sem + 0);
        socket_count++;
        memset(mark_send, 0, sizeof(mark_send));
        printf("debug: send success: %d\n", socket_count);

    }
}

int main(int argc, char const *argv[])
{
    char sig;
    error_fp = open_error_log();
    fprintf(error_fp, "This is send.c error.log!\n");
    while (sig != 'y')
    {
        printf("Port = %d\nClient_num = %d\nChannel_num = %d\nMin_channel = %d\nPress'y' to continue...\n", port, client_num, channel_num, min_channel);
        sig = getchar();
        switch (sig)
        {
        case 'y':
            break;

        default:
            printf("port client_num channel_num min_channel\n");
            scanf("%d %d %d %d", &port, &client_num, &channel_num, &min_channel);
            break;
        }
    }

    fprintf(error_fp, "\nPort = %d\nClient_num = %d\nChannel_num = %d\nMin_channel = %d\n", port, client_num, channel_num, min_channel);

    fflush(error_fp);

    unsigned int pos = 0;
    unsigned int cnontrol_3;

    sem_init(&sem[0], 0, 1);
    sem_init(&sem[1], 0, 0);
    sem_init(&sem[2], 0, 0);

    work = 0;

    pthread_t rx_thread, event_thread;
    pthread_t part_send_thread;

    dev_open_fun();
    socket_create();

    ptd_create(&rx_thread, 0, rx_process, 0, 0);
    ptd_create(&event_thread, 1, event_process, 0, 0);
    ptd_create(&part_send_thread, 2, data_part_send, 0, 0);

    while (sig != 'o')
    {
        sig = getchar();
        switch (sig)
        {
        case 'w':
            work = 1;
            sleep(1);
            write_data_error_log(&error_fp, pData, PACK_SIZE, 0);
            fclose(error_fp);
            error_fp = open_error_log();
            break;

        case 'p':
            work = 1;
            break;

        case 'c':
            work = 0;
            break;

        case 'e':
            socket_count = 0;
            break;

        case 'r':
            write_control(control_base, 0x0000, 0xFFFFFFFF);
            break;

        case 't':
            write_control(control_base, 0x0004, 0xFFFFFFFF);
            break;

        case 'y':
            cnontrol_3 = read_control(control_base, 0x0008);
            printf("read_end_addr = %x \n", cnontrol_3);
            break;

        case 'u':
            pos = pos + 1024;
            printf("test_read_end_addr = %x \n", pos);
            break;

        case 'i':
            pos = 0;
            printf("test_read_end_addr = %x \n", pos);
            break;
        }
    }
    work = -1;

    close(c2h_fd[0]);
    //close(c2h_fd[1]);
    close(h2c_fd);
    close(control_fd);
    close(interrupt_fd);
    for (int i = 0; i < client_num; i++)
    {
        close(acfd[i]);
    }
    return 0;
}