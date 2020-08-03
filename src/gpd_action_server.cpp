/*********************************************************************
 * BSD 3-Clause License
 *
 * Copyright (c) 2020 PickNik LLC.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *this
 *    list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/

/* Author: Boston Cleek
   Desc:   GPD action server
*/

// ROS
#include <ros/ros.h>
#include <eigen_conversions/eigen_msg.h>
#include <geometry_msgs/PoseStamped.h>
#include <pcl_conversions/pcl_conversions.h>
#include <ros/ros.h>
#include <rosparam_shortcuts/rosparam_shortcuts.h>
#include <sensor_msgs/PointCloud2.h>

// C++
#include <functional>
#include <memory>
#include <vector>
#include <iostream>

// PCL
#include <pcl/io/pcd_io.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

// Eigen
#include <Eigen/Dense>

// GPD
#include <gpd/util/cloud.h>
#include <gpd/grasp_detector.h>

// Action Server
#include <actionlib/server/simple_action_server.h>

constexpr char LOGNAME[] = "gpd_action_server";

namespace gpd_action_server
{
/**
* @brief Generates grasp poses for a generator stage with MTC
* @details Interfaces with the GPD lib using ROS messages and interfaces
*          with MTC using a Action Server
*/
class GraspAction
{
public:
  /**
  * @brief <brief>
  * @brief Constructor
  * @param nh - node handle
  * @details loads parameters, registers callbacks for the action server,
             and initializes GPD
  */
  GraspAction(const ros::NodeHandle& nh) : nh_(nh)
  {
    loadParameters();
    init();
  }

  /**
  * @brief Loads parameters for action server and GPD
  */
  void loadParameters()
  {
    ROS_INFO_NAMED(LOGNAME, "Loading grasp action server parameters");
    ros::NodeHandle pnh("~");

    size_t errors = 0;
    errors += !rosparam_shortcuts::get(LOGNAME, pnh, "path_to_pcd_file", path_to_pcd_file_);
    errors += !rosparam_shortcuts::get(LOGNAME, pnh, "path_to_gpd_config", path_to_gpd_config_);
    errors += !rosparam_shortcuts::get(LOGNAME, pnh, "action_name", action_name_);
    errors += !rosparam_shortcuts::get(LOGNAME, pnh, "view_point", view_point_);
    errors += !rosparam_shortcuts::get(LOGNAME, pnh, "frame_id", frame_id_);
    rosparam_shortcuts::shutdownIfError(LOGNAME, errors);

    ROS_INFO("Path to PCD file: %s", path_to_pcd_file_.c_str());
    ROS_INFO("Path to GPD file: %s", path_to_gpd_config_.c_str());
  }

  /**
  * @brief Initialize action server callbacks and GPD
  * @details The point cloud (frame: panda_link0) is loaded from a file and
  *          the camera's origin relative to the point cloud is assumed to be at (0,0,0).
  */
  void init()
  {
    // action server
    server_.reset(new actionlib::SimpleActionServer<moveit_task_constructor_msgs::GenerateDeepGraspPoseAction>(
        nh_, action_name_, false));
    server_->registerGoalCallback(std::bind(&GraspAction::goalCallback, this));
    server_->registerPreemptCallback(std::bind(&GraspAction::preemptCallback, this));
    server_->start();

    // GPD point cloud camera, load cylinder from file
    // set camera view origin
    // assume cloud was taken using one camera
    Eigen::Matrix3Xd camera_view_point(3, 1);
    camera_view_point << view_point_.at(0), view_point_.at(1), view_point_.at(2);
    cloud_camera_.reset(new gpd::util::Cloud(path_to_pcd_file_, camera_view_point));

    // Grasp detector
    grasp_detector_.reset(new gpd::GraspDetector(path_to_gpd_config_));
  }

  /**
  * @brief Action server goal callback
  * @details Accepts goal from client and samples grasp candidates
  */
  void goalCallback()
  {
    goal_name_ = server_->acceptNewGoal()->action_name;
    ROS_INFO_NAMED(LOGNAME, "New goal accepted: %s", goal_name_.c_str());

    sampleGrasps();
  }

  /**
  * @brief Preempt callback
  * @details Preempts goal
  */
  void preemptCallback()
  {
    ROS_INFO_NAMED(LOGNAME, "Preempted %s:", goal_name_.c_str());
    server_->setPreempted();
  }

  /**
  * @brief Samples grasp candidates using GPD
  * @details Compose grasp candidates, the candidates are sent back to the client
  *          using the feedback message. Only candidates with a positive grasp
  *          score are used. If there is at least one candidate with a positive
  *          score the result is set to success else it is a failure.
  */
  void sampleGrasps()
  {
    std::vector<std::unique_ptr<gpd::candidate::Hand>> grasps;     // detect grasp poses
    grasp_detector_->preprocessPointCloud(*cloud_camera_);   // preprocess the point cloud
    grasps = grasp_detector_->detectGrasps(*cloud_camera_);  // detect grasps in the point cloud

    // Use grasps with score > 0
    std::vector<unsigned int> grasp_id;
    for (unsigned int i = 0; i < grasps.size(); i++)
    {
      if (grasps.at(i)->getScore() > 0.0)
      {
        grasp_id.push_back(i);
      }
    }

    if (grasp_id.empty())
    {
      result_.grasp_state = "failed";
      server_->setAborted(result_);
      return;
    }

    for (auto id : grasp_id)
    {
      geometry_msgs::PoseStamped grasp_pose;
      grasp_pose.header.frame_id = frame_id_;
      tf::pointEigenToMsg(grasps.at(id)->getPosition(), grasp_pose.pose.position);

      Eigen::Quaterniond hand_orientation(grasps.at(id)->getOrientation());
      tf::quaternionEigenToMsg(hand_orientation, grasp_pose.pose.orientation);

      feedback_.grasp_candidates.emplace_back(grasp_pose);

      // Grasp is selected based on cost not score
      // Invert score to represent grasp with lowest cost
      feedback_.costs.emplace_back(static_cast<double>(1.0 / grasps.at(id)->getScore()));
    }

    server_->publishFeedback(feedback_);
    result_.grasp_state = "success";
    server_->setSucceeded(result_);
  }

private:
  ros::NodeHandle nh_;

  std::unique_ptr<actionlib::SimpleActionServer<moveit_task_constructor_msgs::GenerateDeepGraspPoseAction>>
      server_;                                                            // action server
  moveit_task_constructor_msgs::GenerateDeepGraspPoseFeedback feedback_;  // action feedback message
  moveit_task_constructor_msgs::GenerateDeepGraspPoseResult result_;      // action result message

  std::string path_to_pcd_file_;    // path to cylinder pcd file
  std::string path_to_gpd_config_;  // path to GPD config file
  std::string goal_name_;           // action name
  std::string action_name_;         // action namespace
  std::string frame_id_;            // frame of point cloud/grasps

  std::vector<double> view_point_;                      // origin of the camera
  std::unique_ptr<gpd::GraspDetector> grasp_detector_;  // used to run the GPD algorithm
  std::unique_ptr<gpd::util::Cloud> cloud_camera_;      // stores point cloud with (optional) camera information
};
}  // namespace gpd_action_server

int main(int argc, char** argv)
{
  ROS_INFO_NAMED(LOGNAME, "Init gpd_action_server");
  ros::init(argc, argv, "gpd_server");
  ros::NodeHandle nh;

  gpd_action_server::GraspAction grasp_action(nh);
  ros::spin();

  return 0;
}
