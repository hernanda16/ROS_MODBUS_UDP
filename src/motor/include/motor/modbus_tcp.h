#ifndef MODBUS_TCP_H
#define MODBUS_TCP_H

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define VELOCITY_REGISTER 514

void modbus_tcp_init(int *_socket_id, char *_ip_address, int _port)
{
    *_socket_id = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (*_socket_id < 0)
    {
        perror("socket");
        return;
    }

    //==============================================================================

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(_port);
    server.sin_addr.s_addr = inet_addr(_ip_address);

    int i = connect(*_socket_id, (struct sockaddr *)&server, sizeof(struct sockaddr_in));

    if (i < 0)
    {
        printf("connect - error %d\n", i);
        close(*_socket_id);
    }

    //==============================================================================
}
void modbus_tcp_send(int *_socket_id, int8_t _slave_id, int16_t _register, int16_t _value)
{
    static uint16_t transaction_id = 0;
    char buffer_send[13] = {0};

    buffer_send[0] = transaction_id >> 8;
    buffer_send[1] = transaction_id & 0xff;

    buffer_send[2] = 0;
    buffer_send[3] = 0;
    buffer_send[4] = 0;
    buffer_send[5] = 6;
    buffer_send[6] = _slave_id;
    buffer_send[7] = 6; // 3 untuk read, 6 untuk write
    buffer_send[8] = _register >> 8;
    buffer_send[9] = _register & 0xff;
    buffer_send[10] = _value >> 8;
    buffer_send[11] = _value & 0xff;

    /* send request */
    int i = send(*_socket_id, buffer_send, 12, 0);
    if (i < 12)
    {
        printf("failed to send all 12 chars\n");
    }

    // std::cout << "transaction_id: " << transaction_id << std::endl;

    transaction_id++;
}

void modbus_tcp_read(int *_socket_id, int8_t _slave_id, int16_t _register, int16_t _num_read, int16_t *value_ptr)
{
    static uint16_t transaction_id = 0;
    char buffer_send[13] = {0};
    char buffer_recv[261] = {0};
    fd_set fds;
    struct timeval tv;

    buffer_send[0] = transaction_id >> 8;
    buffer_send[1] = transaction_id & 0xff;

    buffer_send[2] = 0;
    buffer_send[3] = 0;
    buffer_send[4] = 0;
    buffer_send[5] = 6;
    buffer_send[6] = _slave_id;
    buffer_send[7] = 6; // 3 untuk read, 6 untuk write
    buffer_send[8] = _register >> 8;
    buffer_send[9] = _register & 0xff;
    buffer_send[10] = _num_read >> 8;
    buffer_send[11] = _num_read & 0xff;

    /* send request */
    int i = send(*_socket_id, buffer_send, 12, 0);
    if (i < 12)
    {
        printf("failed to send all 12 chars\n");
    }

    std::cout << "transaction_id: " << transaction_id << std::endl;

    FD_ZERO(&fds);
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    FD_SET(*_socket_id, &fds);
    i = select(32, &fds, NULL, NULL, &tv);
    if (i <= 0)
    {
        printf("no TCP response received\n");
        // close(*_socket_id);
        return;
    }

    int recv_len = recv(*_socket_id, buffer_recv, 261, MSG_DONTWAIT);

    if (recv_len >= 9)
    {
        if (buffer_recv[7] == 6)
        {
            *value_ptr = (buffer_recv[9 + recv_len + recv_len] << 8) + buffer_recv[10 + recv_len + recv_len];
        }
    }

    transaction_id++;
}

#endif