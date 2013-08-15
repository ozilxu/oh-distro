// Selective ros2lcm translator for hardware driver related ros messages.
// takes in arbitrary ros messages and publishes hand_state_t messages on 
// respective channels: SANDIA_LEFT_STATE/SANDIA_RIGHT_STATE/IROBOT_LEFT_STATE/IROBOT_RIGHT_STATE

// the sandia hand driver publishes ros messages at a finger level. They are appended here to form the hand sate message.

#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>

#include <ros/ros.h>
#include <ros/console.h>
#include <cstdlib>
#include <sys/time.h>
#include <time.h>
#include <iostream>
#include <map>


#include <sensor_msgs/JointState.h>
#include <std_msgs/Float64.h>
#include <std_msgs/String.h>
#include <sandia_hand_msgs/RawFingerState.h>

#include <lcm/lcm-cpp.hpp>
#include <lcmtypes/bot_core.hpp>
#include <lcmtypes/drc_lcmtypes.hpp>


using namespace std;

class App{
public:
  App(ros::NodeHandle node_);
  ~App();

private:
  lcm::LCM lcm_publish_ ;
  ros::NodeHandle node_;
  
  std::vector<std::string> sandiaJointNames;

  // msg cache  
  sandia_hand_msgs::RawFingerState  sandia_l_hand_finger_0_state_,sandia_l_hand_finger_1_state_,sandia_l_hand_finger_2_state_,sandia_l_hand_finger_3_state_;
  sandia_hand_msgs::RawFingerState  sandia_r_hand_finger_0_state_,sandia_r_hand_finger_1_state_,sandia_r_hand_finger_2_state_,sandia_r_hand_finger_3_state_; 
  
  // sandia hand publishes raw finger state on separate messages. There is also cal_state, but it clear what that adds.
  ros::Subscriber  sandia_l_hand_finger_0_state_sub_, sandia_l_hand_finger_1_state_sub_, sandia_l_hand_finger_2_state_sub_, sandia_l_hand_finger_3_state_sub_;
  ros::Subscriber  sandia_r_hand_finger_0_state_sub_, sandia_r_hand_finger_1_state_sub_, sandia_r_hand_finger_2_state_sub_, sandia_r_hand_finger_3_state_sub_;  
 
  //TODO: 
  //Irobot hand subcribersand cache
  ros::Subscriber  irobot_l_hand_joint_states_sub_, irobot_r_hand_joint_states_sub_; 
  
  void sandia_l_hand_finger_0_state_cb(const sandia_hand_msgs::RawFingerStatePtr &msg);
  void sandia_l_hand_finger_1_state_cb(const sandia_hand_msgs::RawFingerStatePtr &msg);
  void sandia_l_hand_finger_2_state_cb(const sandia_hand_msgs::RawFingerStatePtr &msg);
  void sandia_l_hand_finger_3_state_cb(const sandia_hand_msgs::RawFingerStatePtr &msg);
  
  void sandia_r_hand_finger_0_state_cb(const sandia_hand_msgs::RawFingerStatePtr &msg);
  void sandia_r_hand_finger_1_state_cb(const sandia_hand_msgs::RawFingerStatePtr &msg);
  void sandia_r_hand_finger_2_state_cb(const sandia_hand_msgs::RawFingerStatePtr &msg);
  void sandia_r_hand_finger_3_state_cb(const sandia_hand_msgs::RawFingerStatePtr &msg);

  void appendSandiaFingerState(drc::hand_state_t& msg_out, sandia_hand_msgs::RawFingerState& msg_in,int finger_id);
  void publishSandiaHandState(int64_t utime_in,bool is_left);
  
  // logic params
  bool init_recd_sandia_l_[4];
  bool init_recd_sandia_r_[4];
  //////////////////////////////////////////////////////////////////////////
  
};

App::App(ros::NodeHandle node_) :
    node_(node_){
  ROS_INFO("Initializing Sandia/Irobot Hands Translator (Not for simulation)");
  
  
 // Hand states
 sandia_l_hand_finger_0_state_sub_ = node_.subscribe(string("/sandia_hands/l_hand/finger_0/raw_state"), 100, &App::sandia_l_hand_finger_0_state_cb,this);
 sandia_l_hand_finger_1_state_sub_ = node_.subscribe(string("/sandia_hands/l_hand/finger_1/raw_state"), 100, &App::sandia_l_hand_finger_1_state_cb,this);
 sandia_l_hand_finger_2_state_sub_ = node_.subscribe(string("/sandia_hands/l_hand/finger_2/raw_state"), 100, &App::sandia_l_hand_finger_2_state_cb,this);
 sandia_l_hand_finger_3_state_sub_ = node_.subscribe(string("/sandia_hands/l_hand/finger_3/raw_state"), 100, &App::sandia_l_hand_finger_3_state_cb,this);
 
 sandia_r_hand_finger_0_state_sub_ = node_.subscribe(string("/sandia_hands/r_hand/finger_0/raw_state"), 100, &App::sandia_r_hand_finger_0_state_cb,this);
 sandia_r_hand_finger_1_state_sub_ = node_.subscribe(string("/sandia_hands/r_hand/finger_1/raw_state"), 100, &App::sandia_r_hand_finger_1_state_cb,this);
 sandia_r_hand_finger_2_state_sub_ = node_.subscribe(string("/sandia_hands/r_hand/finger_2/raw_state"), 100, &App::sandia_r_hand_finger_2_state_cb,this);
 sandia_r_hand_finger_3_state_sub_ = node_.subscribe(string("/sandia_hands/r_hand/finger_3/raw_state"), 100, &App::sandia_r_hand_finger_3_state_cb,this);

 //TODO: 
 // Irobot subcribers 

	sandiaJointNames.push_back("f0_j0");
	sandiaJointNames.push_back("f0_j1");
	sandiaJointNames.push_back("f0_j2");
	sandiaJointNames.push_back("f1_j0");	 
	sandiaJointNames.push_back("f1_j1");
	sandiaJointNames.push_back("f1_j2");
	sandiaJointNames.push_back("f2_j0");
	sandiaJointNames.push_back("f2_j1");
	sandiaJointNames.push_back("f2_j2");
	sandiaJointNames.push_back("f3_j0");	 
	sandiaJointNames.push_back("f3_j1");
	sandiaJointNames.push_back("f3_j2");
	
	init_recd_sandia_l_[0]=false;
	init_recd_sandia_l_[1]=false;
	init_recd_sandia_l_[2]=false;
	init_recd_sandia_l_[3]=false;
	init_recd_sandia_r_[0]=false;
	init_recd_sandia_r_[1]=false;
	init_recd_sandia_r_[2]=false;
	init_recd_sandia_r_[3]=false;	

};

App::~App()  {
}

// same as bot_timestamp_now():
int64_t _timestamp_now(){
    struct timeval tv;
    gettimeofday (&tv, NULL);
    return (int64_t) tv.tv_sec * 1000000 + tv.tv_usec;
}

void App::sandia_l_hand_finger_0_state_cb(const sandia_hand_msgs::RawFingerStatePtr& msg)
{
 if(!init_recd_sandia_l_[0])
   init_recd_sandia_l_[0]=true;
 sandia_l_hand_finger_0_state_= *msg;
}
void App::sandia_l_hand_finger_1_state_cb(const sandia_hand_msgs::RawFingerStatePtr& msg)
{
 if(!init_recd_sandia_l_[1])
   init_recd_sandia_l_[1]=true;
 sandia_l_hand_finger_1_state_= *msg;
}
void App::sandia_l_hand_finger_2_state_cb(const sandia_hand_msgs::RawFingerStatePtr& msg)
{
 if(!init_recd_sandia_l_[2])
   init_recd_sandia_l_[2]=true;
 sandia_l_hand_finger_2_state_= *msg;
}
void App::sandia_l_hand_finger_3_state_cb(const sandia_hand_msgs::RawFingerStatePtr& msg)
{
 if(!init_recd_sandia_l_[3])
   init_recd_sandia_l_[3]=true;
 sandia_l_hand_finger_3_state_= *msg;
 int64_t utime = _timestamp_now();//has no header::(int64_t) msg->header.stamp.toNSec()/1000; // from nsec to usec
 bool is_left = true;
 publishSandiaHandState(utime,is_left);
}

void App::sandia_r_hand_finger_0_state_cb(const sandia_hand_msgs::RawFingerStatePtr& msg)
{
 if(!init_recd_sandia_r_[0])
   init_recd_sandia_r_[0]=true;
 sandia_r_hand_finger_0_state_= *msg;
}
void App::sandia_r_hand_finger_1_state_cb(const sandia_hand_msgs::RawFingerStatePtr& msg)
{
 if(!init_recd_sandia_r_[1])
   init_recd_sandia_r_[1]=true;
 sandia_r_hand_finger_1_state_= *msg;
}
void App::sandia_r_hand_finger_2_state_cb(const sandia_hand_msgs::RawFingerStatePtr& msg)
{
 if(!init_recd_sandia_r_[2])
   init_recd_sandia_r_[2]=true;
 sandia_r_hand_finger_2_state_= *msg;
}
void App::sandia_r_hand_finger_3_state_cb(const sandia_hand_msgs::RawFingerStatePtr& msg)
{
 if(!init_recd_sandia_r_[3])
   init_recd_sandia_r_[3]=true;
 sandia_r_hand_finger_3_state_= *msg;
 int64_t utime = _timestamp_now();// has no header::(int64_t) msg->header.stamp.toNSec()/1000; // from nsec to usec
 bool is_left = false;
 publishSandiaHandState(utime,is_left);
}


void App::publishSandiaHandState(int64_t utime_in,bool is_left){
  // If haven't got all four sandia finger states, exit:
  if((!init_recd_sandia_l_[0])&&(is_left))
    return;
  if((!init_recd_sandia_l_[1])&&(is_left))
    return;
  if((!init_recd_sandia_l_[2])&&(is_left))
    return;
  if((!init_recd_sandia_l_[3])&&(is_left))
    return;
    
  if((!init_recd_sandia_r_[0])&&(!is_left))
    return;
  if((!init_recd_sandia_r_[1])&&(!is_left))
    return;
  if((!init_recd_sandia_r_[2])&&(!is_left))
    return;
  if((!init_recd_sandia_r_[3])&&(!is_left))
    return;  
    
  drc::hand_state_t msg_out;
  msg_out.utime = utime_in; // from nsec to usec
  
  msg_out.num_joints = sandiaJointNames.size();
  
  for (std::vector<int>::size_type i = 0; i < sandiaJointNames.size(); i++)  {
    std::string name;
    if(is_left)
      name ="left_"+sandiaJointNames[i];
    else
      name ="right_"+sandiaJointNames[i];  
    msg_out.joint_name.push_back(name);      
    msg_out.joint_position.push_back(0);      
    msg_out.joint_velocity.push_back(0);
    msg_out.joint_effort.push_back(0);
  }  
   
  if(is_left){
   appendSandiaFingerState(msg_out, sandia_l_hand_finger_0_state_,0);
   appendSandiaFingerState(msg_out, sandia_l_hand_finger_1_state_,1);
   appendSandiaFingerState(msg_out, sandia_l_hand_finger_2_state_,2);
   appendSandiaFingerState(msg_out, sandia_l_hand_finger_3_state_,3);
   lcm_publish_.publish("SANDIA_LEFT_STATE", &msg_out); 
  } 
  else{
   appendSandiaFingerState(msg_out, sandia_r_hand_finger_0_state_,0);
   appendSandiaFingerState(msg_out, sandia_r_hand_finger_1_state_,1);
   appendSandiaFingerState(msg_out, sandia_r_hand_finger_2_state_,2);
   appendSandiaFingerState(msg_out, sandia_r_hand_finger_3_state_,3);
   lcm_publish_.publish("SANDIA_RIGHT_STATE", &msg_out); 
  }
   
}

void App::appendSandiaFingerState(drc::hand_state_t& msg_out, sandia_hand_msgs::RawFingerState& msg_in,int finger_id){
  
    // calculate joint angles based on hall sensor offsets
   double H2R, R0_INV,R1_INV,R2_INV,CAPSTAN_RATIO;
    H2R = 3.14159 * 2.0 / 36.0;// hall state to radians: 18 pole pairs
    R0_INV = 1.0 / 231.0;
    R1_INV = 1.0 / 196.7;
    R2_INV = 1.0 / 170.0;
    CAPSTAN_RATIO = 0.89;
    msg_out.joint_position[0+finger_id*3] = -H2R * R0_INV * ( msg_in.hall_pos[0] );
    msg_out.joint_position[1+finger_id*3] =  H2R * R1_INV * ( msg_in.hall_pos[1] + CAPSTAN_RATIO*msg_in.hall_pos[0] );
    msg_out.joint_position[2+finger_id*3] =  H2R * R2_INV * ( msg_in.hall_pos[2] - msg_in.hall_pos[1] - CAPSTAN_RATIO*2*msg_in.hall_pos[0] );
    msg_out.joint_velocity[0+finger_id*3] = 0;
    msg_out.joint_velocity[1+finger_id*3] = 0;
    msg_out.joint_velocity[2+finger_id*3] = 0;
    msg_out.joint_effort[0+finger_id*3] = 0;//msg_in.fmcb_effort[0];
    msg_out.joint_effort[1+finger_id*3] = 0;//msg_in.fmcb_effort[1];
    msg_out.joint_effort[2+finger_id*3] = 0;//msg_in.fmcb_effort[2];
}


int main(int argc, char **argv){

  ros::init(argc, argv, "ros2lcm_hands");
  ros::NodeHandle nh;
  App *app = new App(nh);
  std::cout << "ros2lcm_hands translator ready\n";
  ros::spin();
  return 0;
}
