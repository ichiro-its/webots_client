// Copyright (c) 2021 ICHIRO ITS
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "robocup_client/robot_client/robot_client.hpp"

#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>

#include <memory>
#include <string>
#include <vector>
#include <arpa/inet.h>

namespace robocup_client
{

namespace robot_client
{

const int max_answer_size = 1920 * 1080 * 3 + 1000;

RobotClient::RobotClient(const std::string & host, const int & port, std::shared_ptr<robocup_client::communication::TcpSocket> tcp_socket)
: robocup_client::communication::Client(host, port, tcp_socket)
{
  message.add_sensor_time_step("neck_yaw_s", 8);
  message.add_sensor_time_step("neck_pitch_s", 8);
  message.add_sensor_time_step("left_shoulder_pitch_s", 8);
  message.add_sensor_time_step("left_shoulder_roll_s", 8);
  message.add_sensor_time_step("left_elbow_s", 8);
  message.add_sensor_time_step("right_shoulder_pitch_s", 8);
  message.add_sensor_time_step("right_shoulder_roll_s", 8);
  message.add_sensor_time_step("right_elbow_s", 8);
  message.add_sensor_time_step("left_hip_yaw_s", 8);
  message.add_sensor_time_step("left_hip_roll_s", 8);
  message.add_sensor_time_step("left_hip_pitch_s", 8);
  message.add_sensor_time_step("left_knee_s", 8);
  message.add_sensor_time_step("left_ankle_roll_s", 8);
  message.add_sensor_time_step("left_ankle_pitch_s", 8);
  message.add_sensor_time_step("right_hip_yaw_s", 8);
  message.add_sensor_time_step("right_hip_roll_s", 8);
  message.add_sensor_time_step("right_hip_pitch_s", 8);
  message.add_sensor_time_step("right_knee_s", 8);
  message.add_sensor_time_step("right_ankle_roll_s", 8);
  message.add_sensor_time_step("right_ankle_pitch_s", 8);
  message.add_sensor_time_step("Camera", 16);
  message.add_sensor_time_step("gyro", 8);
  message.add_sensor_time_step("accelerometer", 8);

  send(*message.get_actuator_request());
}

bool RobotClient::connect()
{
  robocup_client::communication::Client::connect();

  std::string response = receive_string(8);

  return response.compare("Welcome") == 1;
}

void RobotClient::receive_data(char * buffer, int bytes)
{
  int received = 0;
  while (received < bytes) {
    int n = receive_raw(buffer + received, bytes - received);
    if (n == -1) {
      disconnect();
    }
    received += n;
  }
}

std::shared_ptr<SensorMeasurements> RobotClient::receive()
{
  uint32_t content_size_network = robocup_client::communication::Client::receive<uint32_t>().value_or(0);
  const int answer_size = ntohl(content_size_network);

  if (answer_size > max_answer_size || answer_size == 0) {
    disconnect();
  }

  SensorMeasurements sensor_measurements;
  std::vector<char> buffer(answer_size);
  receive_data(buffer.data(), answer_size);
  sensor_measurements.ParseFromArray(buffer.data(), answer_size);

  return std::make_shared<SensorMeasurements>(sensor_measurements);
}

int RobotClient::send(const ActuatorRequests & data)
{
  uint32_t size = htonl(data.ByteSizeLong());

  int sent = Client::send<uint32_t>(size);

  if (sent == 0) {
    disconnect();
  }

  google::protobuf::io::ZeroCopyOutputStream * zeroCopyStream =
    new google::protobuf::io::FileOutputStream(get_tcp_socket()->get_sockfd());

  data.SerializeToZeroCopyStream(zeroCopyStream);

  delete zeroCopyStream;

  return sent;
}

void RobotClient::update_sensors()
{
  message.clear_actuator_request();

  auto sensors = receiver.receive();

  if (sensors->get()->gyros_size() > 0) {
    current_gyro = sensors.get()->gyros(0);
  }

  if (sensors->get()->accelerometers_size() > 0) {
    current_accelero = sensors.get()->accelerometers(0);
  }

  if (sensors->get()->cameras_size() > 0) {
    current_camera = sensors.get()->cameras(0);
  }

  current_positions.clear();
  for (int i = 0; i < sensors.get()->position_sensors_size(); i++) {
    auto position = sensors.get()->position_sensors(i);

    current_positions.push_back(position);
  }
}

void RobotClient::set_positions(std::vector<tachimawari_interfaces::msg::Joint> joints)
{
  message.clear_actuator_request();

  for (auto joint : joints) {
    std::string joint_name = joint.get_joint_name();
    float position = joint.get_goal_position();

    if (joint_name.find("shoulder_pitch") != std::string::npos) {
      joint_name += " [shoulder]";
    } else if (joint_name.find("hip_yaw") != std::string::npos) {
      joint_name += " [hip]", joint.get_goal_position();
    }

    message.add_motor_position_in_degree(joint_name, position);
  }
  
  send(*message.get_actuator_request());
}

std::shared_ptr<std::vector<PositionSensorMeasurement>> get_positions()
{
  return current_positions;
}

std::shared_ptr<AccelerometerMeasurement> RobotClient::get_accelero()
{
  return current_accelero;
}

std::shared_ptr<CameraMeasurement> RobotClient::get_camera()
{
  return current_camera;
}

std::shared_ptr<GyroMeasurement> RobotClient::get_gyro()
{
  return current_gyro;
}


}  // namespace robot_client

}  // namespace robocup_client
