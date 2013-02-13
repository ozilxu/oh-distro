step_length = 0.3;
step_width = 0.24;
step_time = 3;
max_rot = pi/8;

options.floating = true;
options.dt = 0.001;
% r = Atlas('../../models/mit_gazebo_models/mit_robot_drake/model_minimal_contact.urdf', options);
r = Atlas('../drake/examples/Atlas/urdf/atlas_minimal_contact.urdf', options);
load('../data/atlas_fp.mat');
q0 = xstar(1:end/2);
r = r.setInitialState(xstar);
v = r.constructVisualizer();

[start_pos, step_width] = getAtlasFeetPos(r, q0);

traj = turnGoTraj([start_pos, [0;1;0;0;0;pi/2], [1;1;0;0;0;0], [1;0;0;0;0;0]]);
% traj = cubicSplineTraj([start_pos, [0;1;0;0;0;0]]);
[lambda, ndx_r, ndx_l] = constrainedFootsteps(traj, step_length, step_width, max_rot);
figure(21)
plotFootstepPlan(traj, lambda, ndx_r, ndx_l, step_width);

lambda = optimizeFootsteps(traj, lambda, step_length, step_width, max_rot);
figure(22)
plotFootstepPlan(traj, lambda, ndx_r, ndx_l, step_width);

Xright = footstepLocations(traj, lambda(ndx_r), -pi/2, step_width);
Xleft = footstepLocations(traj, lambda(ndx_l), pi/2, step_width);

[zmptraj, lfoottraj, rfoottraj, ts] = planZMPandFootTrajectory(r, q0, Xright, Xleft, step_time);


xtraj = computeZMPPlan(r, v, xstar, zmptraj, lfoottraj, rfoottraj, ts);