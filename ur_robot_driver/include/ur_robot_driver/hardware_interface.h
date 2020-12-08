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
 * \author  Felix Exner exner@fzi.de
 * \date    2019-04-11
 *
 */
//----------------------------------------------------------------------
#pragma once

// System
#include <memory>
#include <vector>

// ros2_control hardware_interface
#include "hardware_interface/hardware_info.hpp"
#include "hardware_interface/actuator.hpp"
#include "hardware_interface/sensor.hpp"
#include "hardware_interface/system_interface.hpp"
#include "hardware_interface/types/hardware_interface_return_values.hpp"
#include "hardware_interface/types/hardware_interface_status_values.hpp"
#include "hardware_interface/visibility_control.h"

// UR stuff
#include <ur_client_library/ur/ur_driver.h>
#include <ur_robot_driver/dashboard_client_ros.h>

// ROS
#include "rclcpp/macros.hpp"

using hardware_interface::HardwareInfo;
using hardware_interface::return_type;
using hardware_interface::status;
using hardware_interface::Actuator;
using hardware_interface::Sensor;

namespace ur_robot_driver {
/*!
 * \brief The HardwareInterface class handles the interface between the ROS system and the main
 * driver. It contains the read and write methods of the main control loop and registers various ROS
 * topics and services.
 */
    class URPositionHardwareInterface : public hardware_interface::SystemInterface {
    public:
        RCLCPP_SHARED_PTR_DEFINITIONS(URPositionHardwareInterface);

        URPositionHardwareInterface();

        return_type configure(const HardwareInfo &system_info) final;

        std::vector<hardware_interface::StateInterface> export_state_interfaces() final;

        std::vector<hardware_interface::CommandInterface> export_command_interfaces() final;

        status get_status() const final {
            return status_;
        }

        std::string get_name() const final {
            return info_.name;
        }

        return_type start() final;

        return_type stop() final;

        return_type read() final;

        return_type write() final;

    protected:

        HardwareInfo info_;
        status status_;
        std::vector<double> commands_, states_;
        std::vector<double> velocity_commands_, velocity_states_;
        std::vector<double> joint_efforts_;

        double ramp_time_;
        double rate_;
    };
}// namespace ur_robot_driver

#include "pluginlib/class_list_macros.hpp"

PLUGINLIB_EXPORT_CLASS(ur_robot_driver::URPositionHardwareInterface, hardware_interface::SystemInterface)