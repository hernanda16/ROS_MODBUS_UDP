/**
 *
 * Ref:
 * https://ipc2u.com/articles/knowledge-base/detailed-description-of-the-modbus-tcp-protocol-with-command-examples/
 */

#ifndef MODBUS_UDP_H
#define MODBUS_UDP_H

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define VELOCITY_REGISTER 514
#define SPEED_LOOP_PROPORTIONAL_GAIN_REGISTER 2
#define VELOCITY_LOOP_INTERATION_TIME_GAIN_REGISTER 3
#define ACCELERATION_IN_MS_REGISTER 516
#define DECELERATION_IN_MS_REGISTER 517

void modbus_udp_init(int *_socket_id, struct sockaddr_in *_server, char *_ip_address, int _port)
{
    *_socket_id = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (*_socket_id < 0)
    {
        perror("socket");
        return;
    }

    //==============================================================================

    _server->sin_family = AF_INET;
    _server->sin_port = htons(_port);
    _server->sin_addr.s_addr = inet_addr(_ip_address);

    //==============================================================================
}
void modbus_udp_send(int *_socket_id, struct sockaddr_in *_server, int8_t _slave_id, int16_t _register, uint16_t _value)
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
    int i = sendto(*_socket_id, buffer_send, 12, MSG_DONTWAIT, (struct sockaddr *)_server, sizeof(struct sockaddr_in));
    if (i < 12)
    {
        printf("failed to send all 12 chars\n");
    }

    transaction_id++;
}

void modbus_udp_send_multiple_register(int *_socket_id, struct sockaddr_in *_server, int8_t _slave_id, int16_t _register, uint8_t _total_register, int16_t *_value)
{
    // Using different transaction id for each request
    static uint16_t transaction_id = 0;

    // Calculate total message length
    uint16_t message_length = 7 + _total_register * 2;
    uint16_t total_message_lenght = message_length + 6;

    // Create buffer depend on total message length
    char buffer_send[total_message_lenght] = {0};

    // Transaction ID
    buffer_send[0] = transaction_id >> 8;
    buffer_send[1] = transaction_id & 0xff;

    // Protocol ID
    buffer_send[2] = 0;
    buffer_send[3] = 0;

    // Message Length
    buffer_send[4] = message_length >> 8;
    buffer_send[5] = message_length & 0xff;

    // Unit ID
    buffer_send[6] = _slave_id;

    // Function Code
    buffer_send[7] = 16;

    // Starting Address
    buffer_send[8] = _register >> 8;
    buffer_send[9] = _register & 0xff;

    // Quantity of Registers
    buffer_send[10] = _total_register >> 8;
    buffer_send[11] = _total_register & 0xff;

    // Byte Count
    buffer_send[12] = _total_register * 2;

    // Register Value
    for (int i = 0; i < _total_register; i++)
    {
        buffer_send[13 + i] = _value[i] >> 8;
        buffer_send[13 + i + 1] = _value[i] & 0xff;
    }

    // Send Request
    int i = sendto(*_socket_id, buffer_send, total_message_lenght, MSG_DONTWAIT, (struct sockaddr *)_server, sizeof(struct sockaddr_in));
    if (i < total_message_lenght)
    {
        printf("failed to send all 12 chars\n");
    }

    transaction_id++;
}

#endif