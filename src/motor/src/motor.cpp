#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist.hpp"

#include "motor/modbus_udp.h"
#include "motor/modbus_tcp.h"

#define MOTOR_WHEEL_RADIUS 6.3f
#define MOTOR_GEAR_RATIO 0.25f
#define MOTOR_DISTANCE_TO_CENTER 20.0f

struct Velocity
{
    float x;
    float y;
    float theta;
} velocity;

typedef struct motor_socket
{
    int socket_id;
    char ip_address[16];
    int port;
    struct sockaddr_in server;
} motor_socket_t;

motor_socket_t motor_1, motor_2, motor_3;

using namespace std::chrono_literals;
using std::placeholders::_1;

//==============================================================================

class MinimalSubscriber : public rclcpp::Node
{
public:
    MinimalSubscriber()
        : Node("motor")
    {
        subscription_ = this->create_subscription<geometry_msgs::msg::Twist>(
            "motor_velocity", 1, std::bind(&MinimalSubscriber::topic_callback, this, _1));

        // create a timer to send velocity to motors
        timer_ = this->create_wall_timer(
            10ms, std::bind(&MinimalSubscriber::timer_callback, this));

        strcpy(motor_1.ip_address, "169.254.196.196");
        strcpy(motor_2.ip_address, "169.254.196.197");
        strcpy(motor_3.ip_address, "169.254.196.198");

        modbus_udp_init(&motor_1.socket_id, &motor_1.server, motor_1.ip_address, 1196);
        modbus_udp_init(&motor_2.socket_id, &motor_2.server, motor_2.ip_address, 1197);
        modbus_udp_init(&motor_3.socket_id, &motor_3.server, motor_3.ip_address, 1198);
    }

private:
    int16_t prev_left_motor_speed = 0;
    int16_t prev_right_motor_speed = 0;
    int16_t prev_rear_motor_speed = 0;

    int16_t vx_buffer = 0;
    int16_t vy_buffer = 0;

private:
    void topic_callback(const geometry_msgs::msg::Twist::SharedPtr msg) const
    {
        velocity.x = msg->linear.x;
        velocity.y = msg->linear.y;
        velocity.theta = msg->angular.z;
    }

    // create a timer callback to send velocity to motors
    void timer_callback()
    {
        int16_t left_motor_speed = 0;
        int16_t right_motor_speed = 0;
        int16_t rear_motor_speed = 0;

        uint16_t left_motor_acceleration = 1000;
        uint16_t right_motor_acceleration = 1000;
        uint16_t rear_motor_acceleration = 1000;

        vx_buffer = velocity.x;
        vy_buffer = velocity.y;

        // calculate motor speed, 58.675
        left_motor_speed = velocity.x * cosf(59.60 * M_PI / 180) + velocity.y * sinf(59.60 * M_PI / 180) + (-velocity.theta * M_PI / 180 * MOTOR_DISTANCE_TO_CENTER / 3);
        right_motor_speed = velocity.x * cosf(-59.60 * M_PI / 180) + velocity.y * sinf(-59.60 * M_PI / 180) + (-velocity.theta * M_PI / 180 * MOTOR_DISTANCE_TO_CENTER / 3);
        rear_motor_speed = velocity.x * cosf(M_PI) + velocity.y * sinf(M_PI) + (-velocity.theta * M_PI / 180 * MOTOR_DISTANCE_TO_CENTER / 3);

        // convert to rpm
        left_motor_speed = left_motor_speed * 60 / (2 * M_PI * MOTOR_WHEEL_RADIUS * MOTOR_GEAR_RATIO);
        right_motor_speed = right_motor_speed * 60 / (2 * M_PI * MOTOR_WHEEL_RADIUS * MOTOR_GEAR_RATIO);
        rear_motor_speed = rear_motor_speed * 60 / (2 * M_PI * MOTOR_WHEEL_RADIUS * MOTOR_GEAR_RATIO);

        int total_vel = abs(left_motor_speed) + abs(right_motor_speed) + abs(rear_motor_speed);

        if (left_motor_speed == 0)
            left_motor_acceleration = 500;
        else
            left_motor_acceleration = total_vel / abs(left_motor_speed) * 250;

        if (right_motor_speed == 0)
            right_motor_acceleration = 500;
        else
            right_motor_acceleration = total_vel / abs(right_motor_speed) * 250;

        if (rear_motor_speed == 0)
            rear_motor_acceleration = 500;
        else
            rear_motor_acceleration = total_vel / abs(rear_motor_speed) * 250;

        if (left_motor_speed != prev_left_motor_speed)
        {
            modbus_udp_send(&motor_2.socket_id, &motor_2.server, 2, ACCELERATION_IN_MS_REGISTER, left_motor_acceleration);
        }

        if (right_motor_speed != prev_right_motor_speed)
        {
            modbus_udp_send(&motor_1.socket_id, &motor_1.server, 1, ACCELERATION_IN_MS_REGISTER, right_motor_acceleration);
        }

        if (rear_motor_speed != prev_rear_motor_speed)
        {
            modbus_udp_send(&motor_3.socket_id, &motor_3.server, 3, ACCELERATION_IN_MS_REGISTER, rear_motor_acceleration);
        }

        usleep(100);

        if (left_motor_speed != prev_left_motor_speed)
        {
            modbus_udp_send(&motor_2.socket_id, &motor_2.server, 2, DECELERATION_IN_MS_REGISTER, left_motor_acceleration);
        }

        if (right_motor_speed != prev_right_motor_speed)
        {
            modbus_udp_send(&motor_1.socket_id, &motor_1.server, 1, DECELERATION_IN_MS_REGISTER, right_motor_acceleration);
        }

        if (rear_motor_speed != prev_rear_motor_speed)
        {
            modbus_udp_send(&motor_3.socket_id, &motor_3.server, 3, DECELERATION_IN_MS_REGISTER, rear_motor_acceleration);
        }

        usleep(100);

        if (left_motor_speed != prev_left_motor_speed)
        {
            modbus_udp_send(&motor_2.socket_id, &motor_2.server, 2, VELOCITY_REGISTER, left_motor_speed);
        }

        if (right_motor_speed != prev_right_motor_speed)
        {
            modbus_udp_send(&motor_1.socket_id, &motor_1.server, 1, VELOCITY_REGISTER, right_motor_speed);
        }

        if (rear_motor_speed != prev_rear_motor_speed)
        {
            modbus_udp_send(&motor_3.socket_id, &motor_3.server, 3, VELOCITY_REGISTER, rear_motor_speed);
        }

        // int16_t dummy[3];
        // dummy[0] = right_motor_speed;
        // dummy[1] = 100;
        // dummy[2] = 250;

        // modbus_udp_send_multiple_register(&motor_1.socket_id, &motor_1.server, 1, VELOCITY_REGISTER, 3, dummy);

        prev_left_motor_speed = left_motor_speed;
        prev_right_motor_speed = right_motor_speed;
        prev_rear_motor_speed = rear_motor_speed;
    }

    rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr subscription_;

    // create a timer to send velocity to motors
    rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char *argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<MinimalSubscriber>());
    rclcpp::shutdown();
    return 0;
}