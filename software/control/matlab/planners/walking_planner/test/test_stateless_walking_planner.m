options.floating = true;
options.dt = 0.001;

warning('off','Drake:RigidBodyManipulator:UnsupportedContactPoints')
warning('off','Drake:RigidBodyManipulator:UnsupportedJointLimits')
warning('off','Drake:RigidBodyManipulator:UnsupportedVelocityLimits')
options.visual = false; % loads faster
r = Atlas(strcat(getenv('DRC_PATH'),'/models/mit_gazebo_models/mit_robot_drake/model_minimal_contact_point_hands.urdf'),options);
r = removeCollisionGroupsExcept(r,{'heel','toe'});
r = compile(r);


footstep_request = drc.footstep_plan_request_t();
footstep_request.utime = 0;

fixed_pt = load(strcat(getenv('DRC_PATH'), '/control/matlab/data/atlas_fp.mat'));
footstep_request.initial_state = r.getStateFrame().lcmcoder.encode(0, fixed_pt.xstar);

footstep_request.goal_pos = drc.position_3d_t();
footstep_request.goal_pos.translation = drc.vector_3d_t();
footstep_request.goal_pos.translation.x = 2.0;
footstep_request.goal_pos.translation.y = 0;
footstep_request.goal_pos.translation.z = 0;
footstep_request.goal_pos.rotation = drc.quaternion_t();
footstep_request.goal_pos.rotation.w = 1.0;
footstep_request.goal_pos.rotation.x = 0;
footstep_request.goal_pos.rotation.y = 0;
footstep_request.goal_pos.rotation.z = 0;

footstep_request.num_goal_steps = 0;
footstep_request.num_existing_steps = 0;
footstep_request.params = drc.footstep_plan_params_t();
footstep_request.params.max_num_steps = 10;
footstep_request.params.min_num_steps = 0;
footstep_request.params.min_step_width = 0.18;
footstep_request.params.nom_step_width = 0.26;
footstep_request.params.max_step_width = 0.35;
footstep_request.params.nom_forward_step = 0.2;
footstep_request.params.max_forward_step = 0.35;
footstep_request.params.ignore_terrain = true;
footstep_request.params.planning_mode = drc.footstep_plan_params_t.MODE_AUTO;
footstep_request.params.behavior = drc.footstep_plan_params_t.BEHAVIOR_BDI_STEPPING;
footstep_request.params.map_command = 0;
footstep_request.params.leading_foot = drc.footstep_plan_params_t.LEAD_LEFT;

footstep_request.default_step_params = drc.footstep_params_t();
footstep_request.default_step_params.utime = 0;
footstep_request.default_step_params.step_speed = 1.0;
footstep_request.default_step_params.step_height = 0.05;
footstep_request.default_step_params.constrain_full_foot_pose = false;
footstep_request.default_step_params.bdi_step_duration = 0;
footstep_request.default_step_params.bdi_sway_duration = 0;
footstep_request.default_step_params.bdi_lift_height = 0;
footstep_request.default_step_params.bdi_toe_off = drc.atlas_behavior_step_action_t.TOE_OFF_ENABLE; 
footstep_request.default_step_params.bdi_knee_nominal = 0;
footstep_request.default_step_params.bdi_max_body_accel = 0;
footstep_request.default_step_params.bdi_max_foot_vel = 0;
footstep_request.default_step_params.bdi_sway_end_dist = 0.02;
footstep_request.default_step_params.bdi_step_end_dist = 0.02;
footstep_request.default_step_params.mu = 1.0;


fp = StatelessFootstepPlanner();
plan = fp.plan_footsteps(r, footstep_request);
plan_msg = plan.toLCM();

request = drc.walking_plan_request_t();
request.utime = 0;

request.initial_state = r.getStateFrame().lcmcoder.encode(0, fixed_pt.xstar);
request.new_nominal_state = request.initial_state;
request.use_new_nominal_state = false;
request.footstep_plan = plan_msg;

wp = StatelessWalkingPlanner();
walking_plan = wp.plan_walking(r, request, true);
lc = lcm.lcm.LCM.getSingleton();
lc.publish('CANDIDATE_ROBOT_PLAN', walking_plan.toLCM());