# Deep Grasp Demo
<img src="https://picknik.ai/assets/images/logo.jpg" width="120">

1) [Overview](#Overview) </br>
2) [Packages](#Packages) </br>
3) [Install](#Install) </br>
4) [Launching Demos and Further Details](#Launching-Demos-and-Further-Details) </br>
5) [Depth Sensor Data](#Depth-Sensor-Data) </br>
6) [Camera View Point](#Camera-View-Point) </br>
7) [Known Issues](#Known-Issues) </br>

## Overview
This repository contains several demos
using deep learning methods for grasp pose generation within the MoveIt Task Constructor.

The packages were developed and tested on Ubuntu 18.04 running ROS Melodic.


## Packages
* `deep_grasp_task`: constructs a pick and place task using deep learning methods
for the grasp generation stage within the MoveIt Task Constructor

* `moveit_task_constructor_dexnet`: uses [Dex-Net](https://berkeleyautomation.github.io/dex-net/) to sample grasps from a depth image

* `moveit_task_constructor_gpd`: uses [GPD](https://github.com/atenpas/gpd) to sample grasps from 3D point clouds


## Install
First, Complete the [Getting Started Tutorial](https://ros-planning.github.io/moveit_tutorials/doc/getting_started/getting_started.html).

### Dependencies
It is recommended to install the dependencies that are not ROS packages outside of the
catkin workspace.

Before installing the dependencies it is recommended to run:
```
sudo apt update
sudo apt upgrade
```

#### Grasp Pose Detection
1) Requirements
  * PCL >= 1.9: The `pcl_install.sh` script will install PCL 1.11
  ```
  wget https://raw.githubusercontent.com/PickNikRobotics/deep_grasp_demo/master/pcl_install.sh
  chmod +x pcl_install.sh
  sudo ./pcl_install.sh
  ```

  * OpenCV >= 3.4: The `opencv_install.sh` script will install OpenCV 3.4
  ```
  wget https://raw.githubusercontent.com/PickNikRobotics/deep_grasp_demo/master/opencv_install.sh
  chmod +x opencv_install.sh
  sudo ./opencv_install.sh
  ```

  * Eigen >= 3.0: If ROS is installed then this requirement is satisfied

2) Clone th GPD library
  ```
  git clone https://github.com/atenpas/gpd
  ```

3) Modify CMakeLists.txt

  First, remove the `-03` compiler optimization. This optimization can cause
  a segmentation fault on 18.04.

  ```
  set(CMAKE_CXX_FLAGS "-fopenmp -fPIC -Wno-deprecated -Wenum-compare -Wno-ignored-attributes -std=c++14")
  ```

  Next, update the `find_package()` functions for the `PCL` and `OpenCV`
  versions installed. If you ran the above install scripts `CMakeLists.txt` should read:

  ```
  find_package(PCL 1.11 REQUIRED)
  find_package(OpenCV 3.4 REQUIRED)
  ```


4) Build
```
cd gpd
mkdir build && cd build
cmake ..
make -j
sudo make install
```

#### Dex-Net
Download the required packages and pre-trained models

1) Run the install script </br>
  If you have a GPU this option will install tensorflow with GPU support.
  ```
  wget https://raw.githubusercontent.com/PickNikRobotics/deep_grasp_demo/master/dexnet_install.sh
  wget https://raw.githubusercontent.com/PickNikRobotics/deep_grasp_demo/master/dexnet_requirements.txt
  chmod +x dexnet_install.sh
  ./dexnet_install.sh {cpu|gpu}
  ```

2) Download the pre-trained models
  ```
  ./dexnet_deps/gqcnn/scripts/downloads/models/download_models.sh
  ```

### ROS Packages
#### Deep Grasping Packages
For now it is recommended to create a new workspace to prevent conflicts between packages. This will especially be helpful if you want to use Gazebo with the demos.
```
mkdir -p ~/ws_grasp/src
cd ~/ws_grasp/src
wstool init
wstool merge https://raw.githubusercontent.com/PickNikRobotics/deep_grasp_demo/master/.rosinstall
wstool update

rosdep install --from-paths . --ignore-src --rosdistro $ROS_DISTRO
```

Note: Here you will need to extend the `ws_grasp` to the `ws_moveit` that was created from the [Getting Started Tutorial](https://ros-planning.github.io/moveit_tutorials/doc/getting_started/getting_started.html).
```
cd ~/ws_grasp
catkin config --extend <path_to_ws_moveit>/devel --cmake-args -DCMAKE_BUILD_TYPE=Release
catkin build
```


#### Panda Gazebo Support (Optional)
You will need the C++ Franka Emika library. This can be installed from [source](https://github.com/frankaemika/libfranka) or by executing:
```
sudo apt install ros-melodic-libfranka
```

You will need two additional packages.
```
git clone https://github.com/tahsinkose/panda_moveit_config.git -b melodic-devel
git clone https://github.com/tahsinkose/franka_ros.git -b simulation
```


## Launching Demos and Further Details
To see how to launch the demos using GPD and Dex-Net see the [moveit_task_constructor_gpd](https://github.com/PickNikRobotics/deep_grasp_demo/tree/master/moveit_task_constructor_gpd) and [moveit_task_constructor_dexnet](https://github.com/PickNikRobotics/deep_grasp_demo/tree/master/moveit_task_constructor_dexnet) packages.


## Depth Sensor Data
### Collecting Data using Gazebo
Perhaps you want to collect depth sensor data on an object and use fake controllers to execute the motion plan. The launch file `sensor_data_gazebo.launch` will launch a `process_image_server` and a `point_cloud_server` node. These will provide services to save either images or point clouds.
Images will be saved to `moveit_task_constructor_dexnet/data/images` and point clouds saved to `moveit_task_constructor_gpd/data/pointclouds`.

To collect either images or point clouds run:
```
roslaunch deep_grasp_task sensor_data_gazebo.launch
```

To save the depth and color images:
```
rosservice call /save_images "depth_file: 'my_depth_image.png'
color_file: 'my_color_image.png'"

```

To save a point cloud:
```
rosservice call /save_point_cloud "cloud_file: 'my_cloud_file.pcd'"
```


## Camera View Point
Initially, the camera is setup to view the cylinder from the side of the robot. It is useful especially for Dex-Net to place the camera in an overhead position above the object. To change the camera view point there are a few files to modify. You can move the camera to a pre-set overhead position or follow the general format to create a new position.

First, modify the camera or the panda + camera urdf.

If you want to move the camera position just for collecting sensor data, in `deep_grasp_task/urdf/camera/camera.urdf.xacro` change the camera xacro macro line to read:
```XML
<xacro:kinect_camera parent_link="world" cam_px="0.5" cam_pz="0.7" cam_op="1.57079632679"/>
```

If you want to move the camera position and use the robot to execute trajectories. Go to `deep_grasp_task/urdf/robots/panda_camera.urdf.xacro` and change the camera xacro macro line to read:
```XML
<xacro:kinect_camera parent_link="panda_link0" cam_px="0.5" cam_pz="0.7" cam_op="1.57079632679"/>
```

Next, specify the transformation from the robot base link to the camera link.

Change `deep_grasp_task/config/calib/camera.yaml` to read:
```YAML
trans_base_cam: [0.500, 0.000, 0.700, 0.707, 0.000, 0.707, 0.000]
```

Finally, this is optional depending on whether the camera is added to the planning scene. If the camera is in the planning scene you need to modify `deep_grasp_task/config/panda_object.yaml` to read:
```YAML
spawn_camera: true
camera_pose: [0.5, 0, 0.7, 0, 1.571, 1.571]
```

## Known Issues
1) When running with Gazebo
```
ros.moveit_simple_controller_manager.SimpleControllerManager: Controller panda_hand_controller failed with error GOAL_TOLERANCE_VIOLATED:
ros.moveit_ros_planning.trajectory_execution_manager: Controller handle panda_hand_controller reports status ABORTED
```

2) Planning may fail

If using GPD increase the number of points sampled be setting `num_samples` in `config/gpd_config.yaml`.

Another option is to run either algorithm again. Maybe low quality grasps were sampled or they were not kinematically feasible.
