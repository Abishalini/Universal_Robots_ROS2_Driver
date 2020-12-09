// this is for emacs file handling -*- mode: c++; indent-tabs-mode: nil -*-

// -- BEGIN LICENSE BLOCK ----------------------------------------------
// Copyright 2019 FZI Forschungszentrum Informatik
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// -- END LICENSE BLOCK ------------------------------------------------

//----------------------------------------------------------------------
/*!\file
 *
 * \author  Andy Zelenak zelenak@picknik.ai
 * \date    2020-11-9
 *
 */
//----------------------------------------------------------------------
#include <ur_robot_driver/hardware_interface.h>
#include "hardware_interface/types/hardware_interface_type_values.hpp"

#include <ur_client_library/ur/tool_communication.h>
#include <ur_client_library/exceptions.h>

#include "rclcpp/rclcpp.hpp"

namespace rtde = urcl::rtde_interface;

namespace ur_robot_driver
{
hardware_interface::return_type URPositionHardwareInterface::configure(const HardwareInfo& system_info)
{
  info_ = system_info;

  // resize and initialize
  position_commands_.resize(info_.joints.size(), std::numeric_limits<double>::quiet_NaN());
  position_states_.resize(info_.joints.size(), std::numeric_limits<double>::quiet_NaN());
  velocity_commands_.resize(info_.joints.size(), std::numeric_limits<double>::quiet_NaN());
  velocity_states_.resize(info_.joints.size(), std::numeric_limits<double>::quiet_NaN());
  joint_efforts_.resize(info_.joints.size(), std::numeric_limits<double>::quiet_NaN());

  for (const hardware_interface::ComponentInfo& joint : info_.joints)
  {
    if (joint.command_interfaces.size() != 2)
    {
      RCLCPP_FATAL(rclcpp::get_logger("URPositionHardwareInterface"),
                   "Joint '%s' has %d command interfaces found. 2 expected.", joint.name.c_str(),
                   joint.command_interfaces.size());
      return return_type::ERROR;
    }

    if (joint.command_interfaces[0].name != hardware_interface::HW_IF_POSITION)
    {
      RCLCPP_FATAL(rclcpp::get_logger("URPositionHardwareInterface"),
                   "Joint '%s' have %s command interfaces found as first command interface. '%s' expected.",
                   joint.name.c_str(), joint.command_interfaces[0].name.c_str(), hardware_interface::HW_IF_POSITION);
      return return_type::ERROR;
    }

    if (joint.command_interfaces[1].name != hardware_interface::HW_IF_VELOCITY)
    {
      RCLCPP_FATAL(rclcpp::get_logger("URPositionHardwareInterface"),
                   "Joint '%s' have %s command interfaces found as second command interface. '%s' expected.",
                   joint.name.c_str(), joint.command_interfaces[1].name.c_str(), hardware_interface::HW_IF_VELOCITY);
      return return_type::ERROR;
    }

    if (joint.state_interfaces.size() != 3)
    {
      RCLCPP_FATAL(rclcpp::get_logger("URPositionHardwareInterface"), "Joint '%s' has %d state interface. 3 expected.",
                   joint.name.c_str(), joint.state_interfaces.size());
      return return_type::ERROR;
    }

    if (joint.state_interfaces[0].name != hardware_interface::HW_IF_POSITION)
    {
      RCLCPP_FATAL(rclcpp::get_logger("URPositionHardwareInterface"),
                   "Joint '%s' have %s state interface as first state interface. '%s' expected.", joint.name.c_str(),
                   joint.state_interfaces[0].name.c_str(), hardware_interface::HW_IF_POSITION);
      return return_type::ERROR;
    }

    if (joint.state_interfaces[1].name != hardware_interface::HW_IF_VELOCITY)
    {
      RCLCPP_FATAL(rclcpp::get_logger("URPositionHardwareInterface"),
                   "Joint '%s' have %s state interface as second state interface. '%s' expected.", joint.name.c_str(),
                   joint.state_interfaces[1].name.c_str(), hardware_interface::HW_IF_POSITION);
      return return_type::ERROR;
    }

    if (joint.state_interfaces[2].name != hardware_interface::HW_IF_EFFORT)
    {
      RCLCPP_FATAL(rclcpp::get_logger("URPositionHardwareInterface"),
                   "Joint '%s' have %s state interface as third state interface. '%s' expected.", joint.name.c_str(),
                   joint.state_interfaces[2].name.c_str(), hardware_interface::HW_IF_POSITION);
      return return_type::ERROR;
    }
  }

  status_ = status::CONFIGURED;

  return return_type::OK;
}

std::vector<hardware_interface::StateInterface> URPositionHardwareInterface::export_state_interfaces()
{
  std::vector<hardware_interface::StateInterface> state_interfaces;
  for (uint i = 0; i < info_.joints.size(); i++)
  {
    state_interfaces.emplace_back(hardware_interface::StateInterface(
        info_.joints[i].name, hardware_interface::HW_IF_POSITION, &position_states_[i]));

    state_interfaces.emplace_back(hardware_interface::StateInterface(
        info_.joints[i].name, hardware_interface::HW_IF_VELOCITY, &velocity_states_[i]));

    state_interfaces.emplace_back(
        hardware_interface::StateInterface(info_.joints[i].name, hardware_interface::HW_IF_EFFORT, &joint_efforts_[i]));
  }

  return state_interfaces;
}

std::vector<hardware_interface::CommandInterface> URPositionHardwareInterface::export_command_interfaces()
{
  std::vector<hardware_interface::CommandInterface> command_interfaces;
  for (uint i = 0; i < info_.joints.size(); i++)
  {
    command_interfaces.emplace_back(hardware_interface::CommandInterface(
        info_.joints[i].name, hardware_interface::HW_IF_POSITION, &position_commands_[i]));

    command_interfaces.emplace_back(hardware_interface::CommandInterface(
        info_.joints[i].name, hardware_interface::HW_IF_VELOCITY, &velocity_commands_[i]));
  }

  return command_interfaces;
}

return_type URPositionHardwareInterface::start()
{
  RCLCPP_INFO(rclcpp::get_logger("URPositionHardwareInterface"), "Starting ...please wait...");

  std::this_thread::sleep_for(std::chrono::seconds(2));

  // The robot's IP address.
  std::string robot_ip = info_.hardware_parameters["robot_ip"];
  // Path to the urscript code that will be sent to the robot
  std::string script_filename = info_.hardware_parameters["script_filename"];
  // Path to the file containing the recipe used for requesting RTDE outputs.
  std::string output_recipe_filename = info_.hardware_parameters["output_recipe_filename"];
  // Path to the file containing the recipe used for requesting RTDE inputs.
  std::string input_recipe_filename = info_.hardware_parameters["input_recipe_filename"];
  // Start robot in headless mode. This does not require the 'External Control' URCap to be running
  // on the robot, but this will send the URScript to the robot directly. On e-Series robots this
  // requires the robot to run in 'remote-control' mode.
  bool headless_mode = static_cast<bool>(stoi(info_.hardware_parameters["headless_mode"]));
  // Port that will be opened to communicate between the driver and the robot controller.
  int reverse_port = stoi(info_.hardware_parameters["reverse_port"]);
  // The driver will offer an interface to receive the program's URScript on this port.
  int script_sender_port = stoi(info_.hardware_parameters["script_sender_port"]);
  //  std::string tf_prefix = info_.hardware_parameters["tf_prefix"];
  std::string tf_prefix;

  // Enables non_blocking_read mode. Should only be used with combined_robot_hw. Disables error generated when read
  // returns without any data, sets the read timeout to zero, and synchronises read/write operations. Enabling this when
  // not used with combined_robot_hw can suppress important errors and affect real-time performance.
  bool non_blocking_read_ = static_cast<bool>(stoi(info_.hardware_parameters["non_blocking_read"]));

  // Specify gain for servoing to position in joint space.
  // A higher gain can sharpen the trajectory.
  int servoj_gain = stoi(info_.hardware_parameters["servoj_gain"]);
  // Specify lookahead time for servoing to position in joint space.
  // A longer lookahead time can smooth the trajectory.
  double servoj_lookahead_time = stod(info_.hardware_parameters["servoj_lookahead_time"]);

  // Whenever the runtime state of the "External Control" program node in the UR-program changes, a
  // message gets published here. So this is equivalent to the information whether the robot accepts
  // commands from ROS side.
  //  program_state_pub_ = robot_hw_nh.advertise<std_msgs::Bool>("robot_program_running", 10, true);

  bool use_tool_communication = static_cast<bool>(stoi(info_.hardware_parameters["use_tool_communication"]));

  // Hash of the calibration reported by the robot. This is used for validating the robot
  // description is using the correct calibration. If the robot's calibration doesn't match this
  // hash, an error will be printed. You can use the robot as usual, however Cartesian poses of the
  // endeffector might be inaccurate. See the "ur_calibration" package on help how to generate your
  // own hash matching your actual robot.
  std::string calibration_checksum = info_.hardware_parameters["kinematics/hash"];

  std::unique_ptr<urcl::ToolCommSetup> tool_comm_setup;
  if (use_tool_communication)
  {
    tool_comm_setup = std::make_unique<urcl::ToolCommSetup>();

    using ToolVoltageT = std::underlying_type<urcl::ToolVoltage>::type;
    ToolVoltageT tool_voltage;
    // Tool voltage that will be set as soon as the UR-Program on the robot is started. Note: This
    // parameter is only evaluated, when the parameter "use_tool_communication" is set to TRUE.
    // Then, this parameter is required.}
    tool_voltage = std::stoi(info_.hardware_parameters["tool_voltage"]);

    tool_comm_setup->setToolVoltage(static_cast<urcl::ToolVoltage>(tool_voltage));

    using ParityT = std::underlying_type<urcl::Parity>::type;
    ParityT parity;
    // Parity used for tool communication. Will be set as soon as the UR-Program on the robot is
    // started. Can be 0 (None), 1 (odd) and 2 (even).
    //
    // Note: This parameter is only evaluated, when the parameter "use_tool_communication"
    // is set to TRUE.  Then, this parameter is required.
    parity = std::stoi(info_.hardware_parameters["tool_parity"]);
    tool_comm_setup->setParity(static_cast<urcl::Parity>(parity));

    int baud_rate;
    // Baud rate used for tool communication. Will be set as soon as the UR-Program on the robot is
    // started. See UR documentation for valid baud rates.
    //
    // Note: This parameter is only evaluated, when the parameter "use_tool_communication"
    // is set to TRUE.  Then, this parameter is required.
    baud_rate = std::stoi(info_.hardware_parameters["tool_baud_rate"]);
    tool_comm_setup->setBaudRate(static_cast<uint32_t>(baud_rate));

    int stop_bits;
    // Number of stop bits used for tool communication. Will be set as soon as the UR-Program on the robot is
    // started. Can be 1 or 2.
    //
    // Note: This parameter is only evaluated, when the parameter "use_tool_communication"
    // is set to TRUE.  Then, this parameter is required.
    stop_bits = std::stoi(info_.hardware_parameters["tool_stop_bits"]);
    tool_comm_setup->setStopBits(static_cast<uint32_t>(stop_bits));

    int rx_idle_chars;
    // Number of idle chars for the RX unit used for tool communication. Will be set as soon as the UR-Program on the
    // robot is started. Valid values: min=1.0, max=40.0
    //
    // Note: This parameter is only evaluated, when the parameter "use_tool_communication"
    // is set to TRUE.  Then, this parameter is required.
    rx_idle_chars = std::stoi(info_.hardware_parameters["tool_rx_idle_chars"]);
    tool_comm_setup->setRxIdleChars(rx_idle_chars);

    int tx_idle_chars;
    // Number of idle chars for the TX unit used for tool communication. Will be set as soon as the UR-Program on the
    // robot is started. Valid values: min=0.0, max=40.0
    //
    // Note: This parameter is only evaluated, when the parameter "use_tool_communication"
    // is set to TRUE.  Then, this parameter is required.
    tx_idle_chars = std::stoi(info_.hardware_parameters["tool_tx_idle_chars"]);
    tool_comm_setup->setTxIdleChars(tx_idle_chars);
  }

  RCLCPP_INFO(rclcpp::get_logger("URPositionHardwareInterface"), "Initializing driver...");

  try
  {
    ur_driver_ = std::make_unique<urcl::UrDriver>(
        robot_ip, script_filename, output_recipe_filename, input_recipe_filename,
        std::bind(&URPositionHardwareInterface::handleRobotProgramState, this, std::placeholders::_1), headless_mode,
        std::move(tool_comm_setup), calibration_checksum, (uint32_t)reverse_port, (uint32_t)script_sender_port,
        servoj_gain, servoj_lookahead_time, non_blocking_read_);
  }
  catch (urcl::ToolCommNotAvailable& e)
  {
    RCLCPP_FATAL_STREAM(rclcpp::get_logger("URPositionHardwareInterface"), "See parameter use_tool_communication");

    return return_type::ERROR;
  }
  catch (urcl::UrException& e)
  {
    RCLCPP_FATAL_STREAM(rclcpp::get_logger("URPositionHardwareInterface"), e.what());
    return return_type::ERROR;
  }

  status_ = hardware_interface::status::STARTED;

  RCLCPP_INFO(rclcpp::get_logger("URPositionHardwareInterface"), "System successfully started!");

  return return_type::OK;
}

return_type URPositionHardwareInterface::stop()
{
  RCLCPP_INFO(rclcpp::get_logger("URPositionHardwareInterface"), "Stopping ...please wait...");

  std::this_thread::sleep_for(std::chrono::seconds(2));

  ur_driver_.reset();

  status_ = hardware_interface::status::STOPPED;

  RCLCPP_INFO(rclcpp::get_logger("URPositionHardwareInterface"), "System successfully stopped!");

  return return_type::OK;
}

template <typename T>
void URPositionHardwareInterface::readData(const std::unique_ptr<rtde::DataPackage>& data_pkg,
                                           const std::string& var_name, T& data)
{
  if (!data_pkg->getData(var_name, data))
  {
    // This throwing should never happen unless misconfigured
    std::string error_msg = "Did not find '" + var_name + "' in data sent from robot. This should not happen!";
    throw std::runtime_error(error_msg);
  }
}

template <typename T, size_t N>
void URPositionHardwareInterface::readBitsetData(const std::unique_ptr<rtde::DataPackage>& data_pkg,
                                                 const std::string& var_name, std::bitset<N>& data)
{
  if (!data_pkg->getData<T, N>(var_name, data))
  {
    // This throwing should never happen unless misconfigured
    std::string error_msg = "Did not find '" + var_name + "' in data sent from robot. This should not happen!";
    throw std::runtime_error(error_msg);
  }
}

return_type URPositionHardwareInterface::read()
{
  RCLCPP_INFO(rclcpp::get_logger("URPositionHardwareInterface"), "Reading ...");

  std::unique_ptr<rtde::DataPackage> data_pkg = ur_driver_->getDataPackage();

  if (data_pkg)
  {
    packet_read_ = true;
    readData(data_pkg, "actual_q", urcl_joint_positions_);
    readData(data_pkg, "actual_qd", urcl_joint_velocities_);
    readData(data_pkg, "actual_current", urcl_joint_efforts_);

    memcpy(&position_states_[0], &urcl_joint_positions_[0], 6 * sizeof(double));
    memcpy(&velocity_states_[0], &urcl_joint_velocities_[0], 6 * sizeof(double));
    memcpy(&joint_efforts_[0], &urcl_joint_efforts_[0], 6 * sizeof(double));

    readData(data_pkg, "target_speed_fraction", target_speed_fraction_);
    readData(data_pkg, "speed_scaling", speed_scaling_);
    readData(data_pkg, "runtime_state", runtime_state_);
    readData(data_pkg, "actual_TCP_force", fts_measurements_);
    readData(data_pkg, "actual_TCP_pose", tcp_pose_);
    readData(data_pkg, "standard_analog_input0", standard_analog_input_[0]);
    readData(data_pkg, "standard_analog_input1", standard_analog_input_[1]);
    readData(data_pkg, "standard_analog_output0", standard_analog_output_[0]);
    readData(data_pkg, "standard_analog_output1", standard_analog_output_[1]);
    readData(data_pkg, "tool_mode", tool_mode_);
    readData(data_pkg, "tool_analog_input0", tool_analog_input_[0]);
    readData(data_pkg, "tool_analog_input1", tool_analog_input_[1]);
    readData(data_pkg, "tool_output_voltage", tool_output_voltage_);
    readData(data_pkg, "tool_output_current", tool_output_current_);
    readData(data_pkg, "tool_temperature", tool_temperature_);
    readData(data_pkg, "robot_mode", robot_mode_);
    readData(data_pkg, "safety_mode", safety_mode_);
    readBitsetData<uint32_t>(data_pkg, "robot_status_bits", robot_status_bits_);
    readBitsetData<uint32_t>(data_pkg, "safety_status_bits", safety_status_bits_);
    readBitsetData<uint64_t>(data_pkg, "actual_digital_input_bits", actual_dig_in_bits_);
    readBitsetData<uint64_t>(data_pkg, "actual_digital_output_bits", actual_dig_out_bits_);
    readBitsetData<uint32_t>(data_pkg, "analog_io_types", analog_io_types_);
    readBitsetData<uint32_t>(data_pkg, "tool_analog_input_types", tool_analog_input_types_);

    // TODO logic for sending other stuff to higher level interface

    return return_type::OK;
  }

  return return_type::ERROR;
}

return_type URPositionHardwareInterface::write()
{
  RCLCPP_INFO(rclcpp::get_logger("URPositionHardwareInterface"), "Writing ...");

  if ((runtime_state_ == static_cast<uint32_t>(rtde::RUNTIME_STATE::PLAYING) ||
       runtime_state_ == static_cast<uint32_t>(rtde::RUNTIME_STATE::PAUSING)) &&
      robot_program_running_ && (!non_blocking_read_ || packet_read_))
  {
    for (uint i = 0; i < info_.joints.size(); ++i)
    {
      urcl_position_commands_[i] = position_commands_[i];
      urcl_velocity_commands_[i] = velocity_commands_[i];
    }

    // TODO decide when to MODE_SERVOJ and MODE_SPEEDJ based on the active controller
    ur_driver_->writeKeepalive();
    //    ur_driver_->writeJointCommand(urcl_position_commands_, urcl::comm::ControlMode::MODE_SERVOJ);
    //  ur_driver_->writeJointCommand(urcl_velocity_commands_, urcl::comm::ControlMode::MODE_SPEEDJ);

    packet_read_ = false;

    return return_type::OK;
  }
  else
  {
    // TODO could not read form the driver --> reset controllers
  }
}

void URPositionHardwareInterface::handleRobotProgramState(bool program_running)
{
  if (!robot_program_running_ && program_running)
  {
    // TODO how to set controller reset flag
  }
  robot_program_running_ = program_running;
}
}  // namespace ur_robot_driver
