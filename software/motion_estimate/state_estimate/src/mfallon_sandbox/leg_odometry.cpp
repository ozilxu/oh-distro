#include <stdio.h>
#include <inttypes.h>
#include <lcm/lcm.h>
#include <iostream>
#include <limits>
#include <fstream>

#include "leg_odometry.hpp"



using namespace std;
using namespace boost;
using namespace boost::assign;

leg_odometry::leg_odometry( boost::shared_ptr<lcm::LCM> &lcm_publish_,
  BotParam * botparam_, boost::shared_ptr<ModelClient> &model_):
  lcm_publish_(lcm_publish_),  botparam_(botparam_), model_(model_){
  
  initialization_mode_ = bot_param_get_str_or_fail(botparam_, "state_estimator.legodo.initialization_mode");
  std::cout << "Leg Odometry Initialize Mode: " << initialization_mode_ << " \n";
  leg_odometry_mode_ = bot_param_get_str_or_fail(botparam_, "state_estimator.legodo.integration_mode");
  std::cout << "Leg Odometry Accumulation Mode: " << leg_odometry_mode_ << " \n";
  
  string standing_link= bot_param_get_str_or_fail(botparam_, "state_estimator.legodo.standing_link");
  std::cout << "Leg Odometry Standing Link: " << standing_link << " \n";
  l_standing_link_ = "l_" + standing_link;
  r_standing_link_ = "r_" + standing_link;
  
  filter_joint_positions_ = bot_param_get_boolean_or_fail(botparam_, "state_estimator.legodo.filter_joint_positions");
  std::cout << "Leg Odometry Filter Joints: " << filter_joint_positions_ << " \n";
  
  filter_contact_events_ = bot_param_get_boolean_or_fail(botparam_, "state_estimator.legodo.filter_contact_events");
  std::cout << "Leg Odometry Filter Contact Events: " << filter_contact_events_ << " \n";
  
  publish_diagnostics_ = bot_param_get_boolean_or_fail(botparam_, "state_estimator.legodo.publish_diagnostics");  
  
  KDL::Tree tree;
  if (!kdl_parser::treeFromString( model_->getURDFString() ,tree)){
    cerr << "ERROR: Failed to extract kdl tree from xml robot description" << endl;
    exit(-1);
  }
  fksolver_ = boost::shared_ptr<KDL::TreeFkSolverPosFull_recursive>(new KDL::TreeFkSolverPosFull_recursive(tree));
  
  // Vis Config:
  pc_vis_ = new pointcloud_vis( lcm_publish_->getUnderlyingLCM());
  // obj: id name type reset
  pc_vis_->obj_cfg_list.push_back( obj_cfg(1001,"Body Pose",5,1) );
  pc_vis_->obj_cfg_list.push_back( obj_cfg(1002,"Primary Foot",5,1) );
  pc_vis_->ptcld_cfg_list.push_back( ptcld_cfg(1004,"Primary Contacts",1,0, 1002,1, { 0.0, 1.0, 0.0} ));  

  pc_vis_->obj_cfg_list.push_back( obj_cfg(1003,"Secondary Foot",5,1) );
  pc_vis_->ptcld_cfg_list.push_back( ptcld_cfg(1005,"Secondary Contacts",1,0, 1003,1, { 1.0, 0.0, 0.0} ));  
  
  
  bool log_data_files = false;
  float atlas_weight = 1400.0;
  
  foot_contact_logic_ = new TwoLegs::FootContact(log_data_files, atlas_weight);
  foot_contact_logic_->setStandingFoot( LEFTFOOT ); // Not sure that double states should be used, this should probably change TODO
  primary_foot_ = 0; // ie left
  leg_odo_init_ = false;
   
  foot_contact_classify_ = new foot_contact_classify(lcm_publish_,publish_diagnostics_);

  verbose_ = 1;
  
  previous_utime_ = 0; // Set utimes to known values
  current_utime_ = 0;
  
  world_to_body_.setIdentity();
  world_to_body_bdi_.setIdentity();
  
}

  

// TODO: need to move this function outside of the class, down to app
bool leg_odometry::initializePose(Eigen::Isometry3d body_to_foot){
  if (initialization_mode_ == "zero"){ 
    // Initialize with primary foot at (0,0,0) but orientation using bdi rotation:
    // Otherwise, there is a discontinuity at the very start
    Eigen::Quaterniond q_slaved( world_to_body_bdi_.rotation() );
    Eigen::Isometry3d world_to_body_at_zero;
    world_to_body_at_zero.setIdentity(); // ... Dont need to use the translation, so not filling it in
    world_to_body_at_zero.rotate(q_slaved);
    Eigen::Isometry3d world_to_r_foot_at_zero = world_to_body_at_zero*body_to_foot;
    Eigen::Quaterniond q_foot_new( world_to_r_foot_at_zero.rotation() );
    world_to_fixed_primary_foot_ = Eigen::Isometry3d::Identity();
    world_to_fixed_primary_foot_.rotate( q_foot_new );     
    
    // was...
    //world_to_fixed_primary_foot_ = Eigen::Isometry3d::Identity();
    world_to_body_ =world_to_fixed_primary_foot_*body_to_foot.inverse();
  }else if (initialization_mode_ == "bdi"){
    // At the EST_ROBOT_STATE pose that was logged into the file
    world_to_body_ = world_to_body_bdi_;
    world_to_fixed_primary_foot_ = world_to_body_*body_to_foot;
  }
  return true;
}

bool leg_odometry::prepInitialization(Eigen::Isometry3d body_to_l_foot,Eigen::Isometry3d body_to_r_foot, int contact_status){
  bool init_this_iteration = false;
  if (contact_status == 2){
    std::cout << "Initialize Leg Odometry using left foot\n"; 
    bool success = initializePose(body_to_l_foot); // typical init mode =0
    if (success){
      // if successful, complete initialization
      primary_foot_ = 0; // left
      world_to_secondary_foot_ = world_to_body_*body_to_r_foot;
      leg_odo_init_ = true;
      init_this_iteration = true;
    }
  }else if  (contact_status == 3){
    std::cout << "Initialize Leg Odometry using left foot\n"; 
    bool success = initializePose(body_to_r_foot); // typical init mode =0
    if (success){
      // if successful, complete initialization
      primary_foot_ = 1; // right
      world_to_secondary_foot_ = world_to_body_*body_to_l_foot;
      leg_odo_init_ = true;
      init_this_iteration = true;
    }      
  }  
  
  return init_this_iteration;
}

  
bool leg_odometry::leg_odometry_basic(Eigen::Isometry3d body_to_l_foot,Eigen::Isometry3d body_to_r_foot, int contact_status){
  bool init_this_iteration= false;

  if (!leg_odo_init_){
    init_this_iteration = prepInitialization(body_to_l_foot, body_to_r_foot, contact_status);
  }else{
    if (contact_status == 2 && primary_foot_ ==0){
      if (verbose_>2) std::cout << "Using fixed Left foot, update pelvis position\n";
      world_to_body_ = world_to_fixed_primary_foot_ * body_to_l_foot.inverse() ;
      world_to_secondary_foot_ = world_to_body_ * body_to_r_foot;
    }else if (contact_status == 1 && primary_foot_ == 0){
      std::cout << "Transition Odometry to right foot. Fix foot, update pelvis position\n";
      // When transitioning, take the passive position of the other foot
      // from the previous iteration. this will now be the fixed foot
      world_to_fixed_primary_foot_ = world_to_secondary_foot_;
      
      world_to_body_ = world_to_fixed_primary_foot_ * body_to_r_foot.inverse();
      world_to_secondary_foot_ = world_to_body_ * body_to_l_foot;
      primary_foot_ = 1;
    }else if (contact_status == 3 && primary_foot_ == 1){
      if (verbose_>2) std::cout << "Using fixed Right foot, update pelvis position\n";
      world_to_body_ = world_to_fixed_primary_foot_ * body_to_r_foot.inverse() ;
      world_to_secondary_foot_ = world_to_body_ * body_to_l_foot;
    }else if (contact_status == 0 && primary_foot_ == 1){
      std::cout << "Transition Odometry to left foot. Fix foot, update pelvis position\n";
      // When transitioning, take the passive position of the other foot
      // from the previous iteration. this will now be the fixed foot
      world_to_fixed_primary_foot_ = world_to_secondary_foot_;
      
      world_to_body_ = world_to_fixed_primary_foot_ * body_to_l_foot.inverse();
      world_to_secondary_foot_ = world_to_body_ * body_to_r_foot;
      primary_foot_ = 0;
    }else{
      std::cout << "initialized but unknown update: " << contact_status << "\n";
    }
    
  }
  
  return init_this_iteration;
}

bool leg_odometry::leg_odometry_gravity_slaved_once(Eigen::Isometry3d body_to_l_foot,Eigen::Isometry3d body_to_r_foot, int contact_status){
  bool init_this_iteration= false;
  
  if (!leg_odo_init_){
    init_this_iteration = prepInitialization(body_to_l_foot, body_to_r_foot, contact_status);
  }else{
    if (contact_status == 2 && primary_foot_ ==0){
      if (verbose_>2) std::cout << "Using fixed Left foot, update pelvis position\n";
      world_to_body_ = world_to_fixed_primary_foot_ * body_to_l_foot.inverse() ;
      world_to_secondary_foot_ = world_to_body_ * body_to_r_foot;
    }else if (contact_status == 1 && primary_foot_ == 0){
      std::cout << "Transition Odometry to right foot. Fix foot, update pelvis position\n";
      // When transitioning, take the passive position of the other foot
      // from the previous iteration. this will now be the fixed foot
      
      // At the instant of transition, slave the pelvis position to gravity:
      // - retain the xyz position.
      // - take the pitch and roll from BDI
      // - take the yaw from previously
      // Then do FK and fix that position as the foot position
  
      Eigen::Quaterniond q_prev( world_to_body_.rotation() );
      double rpy_prev[3];
      quat_to_euler(q_prev, rpy_prev[0],rpy_prev[1],rpy_prev[2]);

      Eigen::Quaterniond q_slaved( world_to_body_bdi_.rotation() );
      double rpy_slaved[3];
      quat_to_euler(q_slaved, rpy_slaved[0],rpy_slaved[1],rpy_slaved[2]);
      
      Eigen::Quaterniond q_combined = euler_to_quat( rpy_slaved[0], rpy_slaved[1], rpy_prev[2]);
      Eigen::Isometry3d world_to_body_switch;
      world_to_body_switch.setIdentity();
      world_to_body_switch.translation() = world_to_body_.translation();
      world_to_body_switch.rotate(q_combined);
      world_to_fixed_primary_foot_ = world_to_body_switch * previous_body_to_r_foot_;
      
      world_to_body_ = world_to_fixed_primary_foot_ * body_to_r_foot.inverse();
      world_to_secondary_foot_ = world_to_body_ * body_to_l_foot;
      primary_foot_ = 1;
    }else if (contact_status == 3 && primary_foot_ == 1){
      if (verbose_>2) std::cout << "Using fixed Right foot, update pelvis position\n";
      world_to_body_ = world_to_fixed_primary_foot_ * body_to_r_foot.inverse() ;
      world_to_secondary_foot_ = world_to_body_ * body_to_l_foot;
    }else if (contact_status == 0 && primary_foot_ == 1){
      std::cout << "Transition Odometry to left foot. Fix foot, update pelvis position\n";
      // When transitioning, take the passive position of the other foot
      // from the previous iteration. this will now be the fixed foot

      // At the instant of transition, slave the pelvis position to gravity:
      // - retain the xyz position.
      // - take the pitch and roll from BDI
      // - take the yaw from previously
      // Then do FK and fix that position as the foot position
  
      Eigen::Quaterniond q_prev( world_to_body_.rotation() );
      double rpy_prev[3];
      quat_to_euler(q_prev, rpy_prev[0],rpy_prev[1],rpy_prev[2]);

      Eigen::Quaterniond q_slaved( world_to_body_bdi_.rotation() );
      double rpy_slaved[3];
      quat_to_euler(q_slaved, rpy_slaved[0],rpy_slaved[1],rpy_slaved[2]);
      
      Eigen::Quaterniond q_combined = euler_to_quat( rpy_slaved[0], rpy_slaved[1], rpy_prev[2]);
      Eigen::Isometry3d world_to_body_switch;
      world_to_body_switch.setIdentity();
      world_to_body_switch.translation() = world_to_body_.translation();
      world_to_body_switch.rotate(q_combined);
      world_to_fixed_primary_foot_ = world_to_body_switch * previous_body_to_l_foot_;      
      
      world_to_body_ = world_to_fixed_primary_foot_ * body_to_l_foot.inverse();
      world_to_secondary_foot_ = world_to_body_ * body_to_r_foot;
      primary_foot_ = 0;
    }else{
      std::cout << "initialized but unknown update: " << contact_status << "\n";
    }
  }
  
  return init_this_iteration;
}

bool leg_odometry::leg_odometry_gravity_slaved_always(Eigen::Isometry3d body_to_l_foot,Eigen::Isometry3d body_to_r_foot, int contact_status){
  bool init_this_iteration= false;
  
  if (!leg_odo_init_){
    init_this_iteration = prepInitialization(body_to_l_foot, body_to_r_foot, contact_status);
  }else{
    
    if (contact_status == 2 && primary_foot_ ==0){
      if (verbose_>2) std::cout << "Using fixed Left foot, update pelvis position\n";
      // Take the quaternion from BDI, apply the pitch and roll to the primary foot 
      // via fk. Then update the pelvis position
      
      Eigen::Quaterniond q_prev( world_to_body_.rotation() );
      double rpy_prev[3];
      quat_to_euler(q_prev, rpy_prev[0],rpy_prev[1],rpy_prev[2]);

      Eigen::Quaterniond q_slaved( world_to_body_bdi_.rotation() );
      double rpy_slaved[3];
      quat_to_euler(q_slaved, rpy_slaved[0],rpy_slaved[1],rpy_slaved[2]);
      
      // the slaved orientation of the pelvis
      Eigen::Quaterniond q_combined = euler_to_quat( rpy_slaved[0], rpy_slaved[1], rpy_prev[2]);

      Eigen::Isometry3d world_to_body_at_zero;
      world_to_body_at_zero.setIdentity(); // ... Dont need to use the translation, so not filling it in
      world_to_body_at_zero.rotate(q_slaved);
      
      Eigen::Isometry3d world_to_r_foot_at_zero = world_to_body_at_zero*body_to_l_foot;
      Eigen::Quaterniond q_foot_new( world_to_r_foot_at_zero.rotation() );
      Eigen::Vector3d foot_new_trans = world_to_fixed_primary_foot_.translation();
      
      // the foot has now been rotated to agree with the pelvis orientation:
      world_to_fixed_primary_foot_.setIdentity();
      world_to_fixed_primary_foot_.translation() = foot_new_trans;
      world_to_fixed_primary_foot_.rotate( q_foot_new );      
      
      
      world_to_body_ = world_to_fixed_primary_foot_ * body_to_l_foot.inverse() ;
      world_to_secondary_foot_ = world_to_body_ * body_to_r_foot;
    }else if (contact_status == 1 && primary_foot_ == 0){
      std::cout << "Transition Odometry to right foot. Fix foot, update pelvis position\n";
      // When transitioning, take the passive position of the other foot
      // from the previous iteration. this will now be the fixed foot
      
      // At the instant of transition, slave the pelvis position to gravity:
      // - retain the xyz position.
      // - take the pitch and roll from BDI
      // - take the yaw from previously
      // Then do FK and fix that position as the foot position
  
      Eigen::Quaterniond q_prev( world_to_body_.rotation() );
      double rpy_prev[3];
      quat_to_euler(q_prev, rpy_prev[0],rpy_prev[1],rpy_prev[2]);

      Eigen::Quaterniond q_slaved( world_to_body_bdi_.rotation() );
      double rpy_slaved[3];
      quat_to_euler(q_slaved, rpy_slaved[0],rpy_slaved[1],rpy_slaved[2]);
      
      Eigen::Quaterniond q_combined = euler_to_quat( rpy_slaved[0], rpy_slaved[1], rpy_prev[2]);
      Eigen::Isometry3d world_to_body_switch;
      world_to_body_switch.setIdentity();
      world_to_body_switch.translation() = world_to_body_.translation();
      world_to_body_switch.rotate(q_slaved);
      world_to_fixed_primary_foot_ = world_to_body_switch * body_to_r_foot;
      
      world_to_body_ = world_to_fixed_primary_foot_ * body_to_r_foot.inverse();
      world_to_secondary_foot_ = world_to_body_ * body_to_l_foot;
      primary_foot_ = 1;
    }else if (contact_status == 3 && primary_foot_ == 1){
      if (verbose_>2) std::cout << "Using fixed Right foot, update pelvis position\n";
      
      // Take the quaternion from BDI, apply the pitch and roll to the primary foot 
      // via fk. Then update the pelvis position
      Eigen::Quaterniond q_prev( world_to_body_.rotation() );
      double rpy_prev[3];
      quat_to_euler(q_prev, rpy_prev[0],rpy_prev[1],rpy_prev[2]);

      Eigen::Quaterniond q_slaved( world_to_body_bdi_.rotation() );
      double rpy_slaved[3];
      quat_to_euler(q_slaved, rpy_slaved[0],rpy_slaved[1],rpy_slaved[2]);
      
      // the slaved orientation of the pelvis
      Eigen::Quaterniond q_combined = euler_to_quat( rpy_slaved[0], rpy_slaved[1], rpy_prev[2]);

      Eigen::Isometry3d world_to_body_at_zero;
      world_to_body_at_zero.setIdentity(); // ... Dont need to use the translation, so not filling it in
      world_to_body_at_zero.rotate(q_slaved);
      
      Eigen::Isometry3d world_to_r_foot_at_zero = world_to_body_at_zero*body_to_r_foot;
      Eigen::Quaterniond q_foot_new( world_to_r_foot_at_zero.rotation() );
      Eigen::Vector3d foot_new_trans = world_to_fixed_primary_foot_.translation();
      
      // the foot has now been rotated to agree with the pelvis orientation:
      world_to_fixed_primary_foot_.setIdentity();
      world_to_fixed_primary_foot_.translation() = foot_new_trans;
      world_to_fixed_primary_foot_.rotate( q_foot_new ); 
      
      world_to_body_ = world_to_fixed_primary_foot_ * body_to_r_foot.inverse() ;
      world_to_secondary_foot_ = world_to_body_ * body_to_l_foot;
    }else if (contact_status == 0 && primary_foot_ == 1){
      std::cout << "Transition Odometry to left foot. Fix foot, update pelvis position\n";
      // When transitioning, take the passive position of the other foot
      // from the previous iteration. this will now be the fixed foot

      // At the instant of transition, slave the pelvis position to gravity:
      // - retain the xyz position.
      // - take the pitch and roll from BDI
      // - take the yaw from previously
      // Then do FK and fix that position as the foot position
  
      Eigen::Quaterniond q_prev( world_to_body_.rotation() );
      double rpy_prev[3];
      quat_to_euler(q_prev, rpy_prev[0],rpy_prev[1],rpy_prev[2]);

      Eigen::Quaterniond q_slaved( world_to_body_bdi_.rotation() );
      double rpy_slaved[3];
      quat_to_euler(q_slaved, rpy_slaved[0],rpy_slaved[1],rpy_slaved[2]);
      
      Eigen::Quaterniond q_combined = euler_to_quat( rpy_slaved[0], rpy_slaved[1], rpy_prev[2]);
      Eigen::Isometry3d world_to_body_switch;
      world_to_body_switch.setIdentity();
      world_to_body_switch.translation() = world_to_body_.translation();
      world_to_body_switch.rotate(q_slaved);
      world_to_fixed_primary_foot_ = world_to_body_switch * body_to_l_foot;      
      
      world_to_body_ = world_to_fixed_primary_foot_ * body_to_l_foot.inverse();
      world_to_secondary_foot_ = world_to_body_ * body_to_r_foot;
      primary_foot_ = 0;
    }else{
      std::cout << "initialized but unknown update: " << contact_status << "\n";
    }
    
  }  
  
  return init_this_iteration;
}

float leg_odometry::updateOdometry(std::vector<std::string> joint_name, std::vector<float> joint_position, int64_t utime){
  previous_utime_ = current_utime_;
  previous_world_to_body_ = world_to_body_;
  current_utime_ = utime;
  
  if ( (current_utime_ - previous_utime_)*1E-6 > 30E-3){
    double odo_dt = (current_utime_ - previous_utime_)*1E-6;
    std::cout << "extended time since last update: " <<  odo_dt << "\n";
    std::cout << "resetting the leg odometry\n";
    leg_odo_init_ = false;
  }
  
  
  // 0. Filter Joints
  if (filter_joint_positions_){
    double scale = 1/1.0091; // TODO: correct none-unity coeff sum at source: (a bug in dehann's code)
    for (size_t i=0 ; i < 28; i++){
      joint_position[i]  = scale*lpfilter_[i].processSample( joint_position[i] );
    }
  }  

  // 1. Solve for Forward Kinematics:
  // call a routine that calculates the transforms the joint_state_t* msg.
  map<string, double> jointpos_in;
  map<string, KDL::Frame > cartpos_out;
  for (size_t i=0; i<  joint_name.size(); i++) //cast to uint to suppress compiler warning
    jointpos_in.insert(make_pair(joint_name[i], joint_position[i]));
  bool kinematics_status = fksolver_->JntToCart(jointpos_in,cartpos_out,true); // true = flatten tree to absolute transforms
  if(kinematics_status>=0){
    // cout << "Success!" <<endl;
  }else{
    cerr << "Error: could not calculate forward kinematics!" <<endl;
    exit(-1);
  }
  Eigen::Isometry3d body_to_l_foot = KDLToEigen(cartpos_out.find(l_standing_link_)->second);
  Eigen::Isometry3d body_to_r_foot = KDLToEigen(cartpos_out.find(r_standing_link_)->second);  

  // 2. Determine Primary Foot State
  TwoLegs::footstep newstep;
  newstep = foot_contact_logic_->DetectFootTransistion(current_utime_, left_foot_force_, right_foot_force_);
  if (newstep.foot == LEFTFOOT || newstep.foot == RIGHTFOOT) {
    foot_contact_logic_->setStandingFoot(newstep.foot);
  }
  int contact_status;  
  if (newstep.foot != -1){
    std::cout << "NEW STEP ON " << ((foot_contact_logic_->secondary_foot()==LEFTFOOT) ? "LEFT" : "RIGHT")  << std::endl;
    if ( foot_contact_logic_->getStandingFoot() == LEFTFOOT ){
      contact_status = 0;
    }else if ( foot_contact_logic_->getStandingFoot() == RIGHTFOOT ){
      contact_status = 1;
    }else{
      std::cout << "Foot Contact Error "<< foot_contact_logic_->getStandingFoot() << " (switch)\n";
      int blah;
      cin >> blah;      
    }    
  }else{
    if ( foot_contact_logic_->getStandingFoot() == LEFTFOOT ){
      contact_status = 2;
    }else if ( foot_contact_logic_->getStandingFoot() == RIGHTFOOT ){
      contact_status = 3;
    }else{
      std::cout << "Foot Contact Error "<< foot_contact_logic_->getStandingFoot() << " \n";
      int blah;
      cin >> blah;      
    }
  }
  
  // 3. Integrate the Leg Kinematics
  bool init_this_iteration = false;
  if (leg_odometry_mode_ == "basic" ){
    init_this_iteration = leg_odometry_basic(body_to_l_foot, body_to_r_foot, contact_status);
  }else if (leg_odometry_mode_ == "slaved_once" ){  
    init_this_iteration = leg_odometry_gravity_slaved_once(body_to_l_foot, body_to_r_foot, contact_status);
  }else if( leg_odometry_mode_ == "slaved_always" ){  
    init_this_iteration = leg_odometry_gravity_slaved_always(body_to_l_foot, body_to_r_foot, contact_status);
  }else{
    std::cout << "Unrecognised odometry algorithm\n"; 
    exit(-1);
  }
    
  // 4. Determine a valid kinematic delta
  float estimate_status = -1.0; // if odometry is not valid, then
  if (leg_odo_init_){
    if (!init_this_iteration){
      // Calculate and publish the position delta:
      delta_world_to_body_ =  previous_world_to_body_.inverse() * world_to_body_; 
      estimate_status = 0.0; // assume very accurate to begin with
      
      if (publish_diagnostics_){
        Eigen::Vector3d motion_T = delta_world_to_body_.translation();
        Eigen::Quaterniond motion_R = Eigen::Quaterniond(delta_world_to_body_.rotation());
        drc::pose_transform_t legodo_msg;
        legodo_msg.utime = current_utime_;
        legodo_msg.prev_utime = previous_utime_;
        legodo_msg.translation[0] = motion_T(0);
        legodo_msg.translation[1] = motion_T(1);
        legodo_msg.translation[2] = motion_T(2);
        legodo_msg.rotation[0] = motion_R.w();
        legodo_msg.rotation[1] = motion_R.x();
        legodo_msg.rotation[2] = motion_R.y();
        legodo_msg.rotation[3] = motion_R.z();    
        lcm_publish_->publish("LEG_ODOMETRY_DELTA", &legodo_msg); // Outputting this message should enable out of core integration
      }
    }

    
    if (publish_diagnostics_){
      std::vector<Isometry3dTime> world_to_body_T;
      world_to_body_T.push_back( Isometry3dTime(current_utime_ , world_to_body_  )  );
      pc_vis_->pose_collection_to_lcm_from_list(1001, world_to_body_T);
      
      std::vector<Isometry3dTime> world_to_primary_T;
      world_to_primary_T.push_back( Isometry3dTime(current_utime_ , world_to_fixed_primary_foot_  )  );
      pc_vis_->pose_collection_to_lcm_from_list(1002, world_to_primary_T);

      std::vector<Isometry3dTime> world_to_secondary_T;
      world_to_secondary_T.push_back( Isometry3dTime(current_utime_ , world_to_secondary_foot_  )  );
      pc_vis_->pose_collection_to_lcm_from_list(1003, world_to_secondary_T);
      
      // Primary (green) and Secondary (red) Contact points:
      pc_vis_->ptcld_to_lcm_from_list(1004, *foot_contact_classify_->getContactPoints() , current_utime_, current_utime_);
      pc_vis_->ptcld_to_lcm_from_list(1005, *foot_contact_classify_->getContactPoints() , current_utime_, current_utime_);
    }
    
  }

  
  // 5. Analyse signals to infer covariance
  // Classify/Detect Contact events: strike, break, swing, sway
  // Skip integration of odometry deemed to be unsuitable
  if (filter_contact_events_){
    foot_contact_classify_->setFootForces(left_foot_force_,
                                        right_foot_force_);
    if (estimate_status > -1){ // if the estimator reports a suitable estimate, classify it further
      estimate_status = foot_contact_classify_->update(current_utime_, world_to_fixed_primary_foot_, world_to_secondary_foot_);
    }
  }
  
  previous_body_to_l_foot_ = body_to_l_foot;
  previous_body_to_r_foot_ = body_to_r_foot;
  return estimate_status;
}
