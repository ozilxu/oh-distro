function [q_data, t_data, ee_info,idx_t_infeasible] = ladderIK(r,ts,q0,qstar,ee_info,ladder_opts,ikoptions)
  nq = r.getNumPositions();

  pelvis = r.findLinkId('pelvis');
  utorso = r.findLinkId('utorso');
  neck_joint = findPositionIndices(r,'neck');
  knee_joints = [findPositionIndices(r,'l_leg_kny'); ...
                 findPositionIndices(r,'r_leg_kny')];
  arm_joints = findPositionIndices(r,'arm');

  if ~isfield(ladder_opts,'use_quasistatic_constraint') 
    ladder_opts.use_quasistatic_constraint =  true;
  end
  if ~isfield(ladder_opts,'use_com_constraint') 
    ladder_opts.use_com_constraint =          false;
  end
  if ~isfield(ladder_opts,'use_incr_com_constraint') 
    ladder_opts.use_incr_com_constraint =     false;
  end
  if ~isfield(ladder_opts,'use_final_com_constraint') 
    ladder_opts.use_final_com_constraint =    true;
  end
  if ~isfield(ladder_opts,'use_arm_constraints')
    ladder_opts.use_arm_constraints =         false;
  end
  if ~isfield(ladder_opts,'use_pelvis_gaze_constraint')
    ladder_opts.use_pelvis_gaze_constraint =  true;
  end
  if ~isfield(ladder_opts,'use_pelvis_constraint') 
    ladder_opts.use_pelvis_constraint =       true;
  end
  if ~isfield(ladder_opts,'use_utorso_constraint') 
    ladder_opts.use_utorso_constraint =       true;
  end
  if ~isfield(ladder_opts,'use_knee_constraint') 
    ladder_opts.use_knee_constraint =         true;
  end
  if ~isfield(ladder_opts,'use_ankle_constraint') 
    ladder_opts.use_ankle_constraint =        true;
  end
  if ~isfield(ladder_opts,'use_neck_constraint') 
    ladder_opts.use_neck_constraint =         true;
  end
  if ~isfield(ladder_opts,'use_swing_foot_euler_constraint') 
    ladder_opts.use_swing_foot_euler_constraint =         false;
  end
  if ~isfield(ladder_opts,'compute_intro')
    ladder_opts.compute_intro = false;
  end
  if ~isfield(ladder_opts,'smooth_output');
    ladder_opts.smooth_output = false;
  end
  if ~isfield(ladder_opts,'smoothing_span');
    ladder_opts.smoothing_span = 5;
  end
  if ~isfield(ladder_opts,'smoothing_method');
    ladder_opts.smoothing_method = 'moving';
  end

  if ~isfield(ladder_opts,'shrink_factor') 
    ladder_opts.shrink_factor = 0.1;
  end
  if ~isfield(ladder_opts,'final_shrink_factor') 
    ladder_opts.shrink_factor = 0.5;
  end
  if ~isfield(ladder_opts,'utorso_threshold') 
    ladder_opts.utorso_threshold = 25*pi/180;
  end
  if ~isfield(ladder_opts,'pelvis_gaze_threshold') 
    ladder_opts.pelvis_gaze_threshold = 20*pi/180;
  end
  if ~isfield(ladder_opts,'ankle_limit') 
    ladder_opts.ankle_limit = 25*pi/180;
  end
  if ~isfield(ladder_opts,'knee_lb') 
    ladder_opts.knee_lb = 30*pi/180*ones(size(knee_joints));
  end
  if ~isfield(ladder_opts,'knee_ub') 
    ladder_opts.knee_ub = inf*pi/180*ones(size(knee_joints));
  end
  if ~isfield(ladder_opts,'hand_threshold') 
    ladder_opts.hand_threshold = sin(1*pi/180);
  end
  if ~isfield(ladder_opts,'hand_cone_threshold') 
    ladder_opts.hand_cone_threshold = sin(1*pi/180);
  end
  if ~isfield(ladder_opts,'hand_pos_tol') 
    ladder_opts.hand_pos_tol = 0.01;
  end
  if ~isfield(ladder_opts,'pelvis_threshold') 
    ladder_opts.pelvis_threshold = 0.05;
  end
  if ~isfield(ladder_opts,'com_tol') 
    ladder_opts.com_tol = 0.1;
  end
  if ~isfield(ladder_opts,'com_incr_tol') 
    ladder_opts.com_incr_tol = 0.02;
  end
  if ~isfield(ladder_opts,'com_tol_max') 
    ladder_opts.com_tol_max = 0.5;
  end
  if ~isfield(ladder_opts,'arm_tol_total') 
    ladder_opts.arm_tol_total = 30*pi/180;
  end
  if ~isfield(ladder_opts,'verbose') 
    ladder_opts.verbose = false;
  end
  com_tol_vec = [ladder_opts.com_tol;ladder_opts.com_tol;NaN];
  com_incr_tol_vec = [ladder_opts.com_incr_tol;ladder_opts.com_incr_tol;NaN];

  % time spacing of samples for IK
  nt = length(ts);

  logical2str = @(b) regexprep(sprintf('%i',b),{'1','0'},{' ON ',' OFF '});
  fprintf('\n');
  fprintf('Constraint Summary\n');
  fprintf('==================================\n');
  fprintf('Name                      Status    Tolerance\n');
  fprintf('QS Constraint             %s        %4.2f\n', logical2str(ladder_opts.use_quasistatic_constraint),ladder_opts.shrink_factor);
  fprintf('Incr. COM Constraint:     %s        %4.2f m\n', logical2str(ladder_opts.use_incr_com_constraint), ladder_opts.com_incr_tol);
  fprintf('COM Constraint:           %s        %4.2f m\n', logical2str(ladder_opts.use_com_constraint), ladder_opts.com_tol);
  fprintf('Final COM Constraint:     %s\n', logical2str(ladder_opts.use_final_com_constraint));
  fprintf('Pelvis Constraint:        %s        %4.2f m\n', logical2str(ladder_opts.use_pelvis_constraint), ladder_opts.pelvis_threshold);
  fprintf('Pelvis GazeConstraint:    %s        %4.2f deg\n', logical2str(ladder_opts.use_pelvis_gaze_constraint),ladder_opts.pelvis_gaze_threshold*180/pi);
  fprintf('Torso Constraint:         %s        %4.2f deg\n', logical2str(ladder_opts.use_utorso_constraint),ladder_opts.utorso_threshold*180/pi);
  fprintf('Knee Constraint:          %s        [%4.2f deg, %4.2f deg]\n', logical2str(ladder_opts.use_knee_constraint),ladder_opts.knee_lb(1)*180/pi,ladder_opts.knee_ub(1)*180/pi);
  fprintf('Neck Constraint:          %s\n', logical2str(ladder_opts.use_neck_constraint));
  fprintf('Ankle Constraint:         %s\n', logical2str(ladder_opts.use_ankle_constraint));
  fprintf('Arm Constraints (incr):   %s        %4.2f deg\n', logical2str(ladder_opts.use_arm_constraints),ladder_opts.arm_tol*180/pi);

  hand_grasped(1) = ~isempty(ee_info.hands(1));
  hand_grasped(2) = ~isempty(ee_info.hands(1));

  basic_constraints = {};

  tf = ts(end);

  if ladder_opts.use_knee_constraint
    knee_constraint = PostureConstraint(r);
    knee_constraint = knee_constraint.setJointLimits(knee_joints, ...
      ladder_opts.knee_lb, ...
      ladder_opts.knee_ub);
    basic_constraints = [basic_constraints, {knee_constraint}];
  end

  if ladder_opts.use_neck_constraint
    neck_constraint = PostureConstraint(r);
    neck_constraint = neck_constraint.setJointLimits(neck_joint,q0(neck_joint),q0(neck_joint));
    basic_constraints = [basic_constraints, {neck_constraint}];
  end

  kinsol = doKinematics(r,q0);
  if ladder_opts.use_utorso_constraint
    utorso_posquat = forwardKin(r,kinsol,utorso,[0;0;0],2);
    basic_constraints = [ ...
      basic_constraints, ...
      {WorldGazeOrientConstraint(r,utorso,[0;0;1],utorso_posquat(4:7),ladder_opts.utorso_threshold,90*pi/180)}];
  end

  pelvis_xyzrpy = forwardKin(r,kinsol,pelvis,[0;0;0],1);
  o_T_pelvis = HT(pelvis_xyzrpy(1:3),pelvis_xyzrpy(4),pelvis_xyzrpy(5),pelvis_xyzrpy(6));
  if ladder_opts.use_pelvis_constraint
    basic_constraints = [ ...
      basic_constraints, ...
      {WorldPositionInFrameConstraint(r,pelvis, ...
      [0;0;0], o_T_pelvis, [NaN;-ladder_opts.pelvis_threshold;NaN], ...
      [NaN;ladder_opts.pelvis_threshold;NaN])}];
  end

  if ladder_opts.use_pelvis_gaze_constraint
    basic_constraints = [ ...
      basic_constraints, ...
      {WorldGazeDirConstraint(r,pelvis,[0;0;1],[0;0;1],ladder_opts.pelvis_gaze_threshold)}];
  end

  rpy_tol_max = 30*pi/180;
  foot_retract_max = 0.1;
  foot_retract_vec = foot_retract_max*o_T_pelvis(1:3,1:3)*[-1;0;0];
  for i=1:2
    deriv = fnder(ee_info.feet(i).traj);
    t_moving = ts(any(abs(eval(deriv,ts)) > 0,1));
    if isempty(t_moving)
      ee_info.feet(i).rpy_tol_traj = PPTrajectory(foh([0,tf],repmat([0,0],3,1)));
    else
      t_move0 = t_moving(1);
      t_movef = t_moving(end);
      ee_info.feet(i).rpy_tol_traj = ...
        PPTrajectory(foh([0,linspace(t_move0,t_movef,4),tf],repmat([0,0,rpy_tol_max,0,0,0],3,1)));
      ee_info.feet(i).foot_retract_traj = ...
        PPTrajectory(foh([0,linspace(t_move0,t_movef,5),tf],[zeros(3,2),repmat(foot_retract_vec(1:3),1,2),zeros(3,3)]));
    end
  end

  if ladder_opts.verbose
    msg = '  0%%';
    fprintf(['Progress: ',msg]);
    len_prev_msg = length(sprintf(msg));
  end
  n_err = 0;
  first_err = true;
  err_segments = [];

  q_seed = q0;
  q_nom = qstar;
  q = zeros(nq,nt);
  q(:,1) = q_seed;
  constraint_array = cell(nt,1);
  idx_t_infeasible = false(1,nt);
  for i=1:nt
    t_data = ts(i);
    constraints = basic_constraints;

    foot_supported(1) = ( eval(ee_info.feet(1).support_traj,t_data) && eval(ee_info.feet(1).support_traj,ts(min(i+1,nt))) );
    foot_supported(2) = ( eval(ee_info.feet(2).support_traj,t_data) && eval(ee_info.feet(2).support_traj,ts(min(i+1,nt))) );
    
    if ladder_opts.use_arm_constraints
      arm_constraint = PostureConstraint(r);
      if i==1
        arm_constraint = arm_constraint.setJointLimits(arm_joints,q0(arm_joints)-ladder_opts.arm_tol,q0(arm_joints)+ladder_opts.arm_tol);
      else
        arm_constraint = arm_constraint.setJointLimits(arm_joints,q(arm_joints,i-1)-ladder_opts.arm_tol,q(arm_joints,i-1)+ladder_opts.arm_tol);
      end
      constraints = [constraints, {arm_constraint}];
    end
    if ladder_opts.use_ankle_constraint && any(foot_supported)
      for j = 1:2
        if foot_supported(j)
          ankle_constraint = PostureConstraint(r);
          ankle_constraint = ankle_constraint.setJointLimits( ...
            findPositionIndices(r,r.getBody(r.getBody(ee_info.feet(j).idx).parent).jointname), ...
            -ladder_opts.ankle_limit, ...
            10*ladder_opts.ankle_limit);
          constraints = [constraints, {ankle_constraint}];
        end
      end
    end
    if ladder_opts.use_quasistatic_constraint && any(foot_supported)
      qsc = QuasiStaticConstraint(r);
      for j = 1:2
        if foot_supported(j)
          qsc = qsc.addContact(ee_info.feet(j).idx,r.getBodyContacts(ee_info.feet(j).idx));
        end
      end
      qsc = qsc.setShrinkFactor(ladder_opts.shrink_factor);
      qsc = qsc.setActive(true);
      constraints = [constraints, {qsc}];
    end
    for j = 1:2
      pos_eq = ee_info.feet(j).traj.eval(t_data);
      rpy_tol = eval(ee_info.feet(j).rpy_tol_traj,t_data);
      foot_retract = eval(ee_info.feet(j).foot_retract_traj,t_data);
      pos_eq(1:2) = pos_eq(1:2) + foot_retract(1:2);
      constraints = [ ...
        constraints, ...
        {WorldPositionConstraint(r,ee_info.feet(j).idx, ...
        ee_info.feet(j).pt(1:3), ...
        pos_eq(1:3),pos_eq(1:3))}];
      if foot_supported(j) || ladder_opts.use_swing_foot_euler_constraint
        constraints = [ ...
          constraints, ...
          {WorldEulerConstraint(r,ee_info.feet(j).idx, ...
          pos_eq(4:6)-rpy_tol, ...
          pos_eq(4:6)+rpy_tol)}];
      end
      if hand_grasped(j)
        pos_min = ee_info.hands(j).min_traj.eval(t_data);
        pos_max = ee_info.hands(j).max_traj.eval(t_data);
        pos_eq = (pos_min+pos_max)/2;
        pelvis_T_rail = eye(4);
        pelvis_T_rail(1:3,1:3) = roty(30*pi/180);
        pelvis_T_rail(1:3,4) = o_T_pelvis(1:3,1:3)\(pos_eq(1:3) - o_T_pelvis(1:3,4));
        o_T_rail = o_T_pelvis*pelvis_T_rail;
        constraints = [ ...
          constraints, ...
          {WorldPositionInFrameConstraint(r, ee_info.hands(j).link_ndx, ...
          ee_info.hands(j).pt, o_T_rail, [-ladder_opts.hand_pos_tol;-ladder_opts.hand_pos_tol;-ladder_opts.hand_pos_tol], [ladder_opts.hand_pos_tol;ladder_opts.hand_pos_tol;ladder_opts.hand_pos_tol]), ...
          WorldQuatConstraint(r,ee_info.hands(j).link_ndx,pos_min(4:7),ladder_opts.hand_threshold)}];
      end
    end

    if i > 1 && ladder_opts.use_incr_com_constraint
      constraints = [ ...
        constraints, ...
        {WorldCoMConstraint(r,com_prev-com_incr_tol_vec,com_prev+com_incr_tol_vec)}];
    end  
    if ladder_opts.use_com_constraint
      com = eval(ladder_opts.comtraj,ts(i));
      com(3) = NaN;
      constraints = [ ...
        constraints, ...
        {WorldCoMConstraint(r,com-com_tol_vec,com+com_tol_vec)}];
    end
    [q(:,i),info,infeasible] = inverseKinPointwise(r,t_data,q_seed,q_nom,constraints{:},ikoptions);
    if info > 4 
      if ladder_opts.use_com_constraint
        ladder_opts.com_tol_local = ladder_opts.com_tol;
        while info ~= 1 && ladder_opts.com_tol_local < ladder_opts.com_tol_max
          %display(ladder_opts.com_tol_local(1))
          ladder_opts.com_tol_local = ladder_opts.com_tol_local+0.01;
          constraints{end} = WorldCoMConstraint(r,com-ladder_opts.com_tol_local,com+ladder_opts.com_tol_local);
          [q(:,i),info,infeasible] = inverseKinPointwise(r,t_data,q_seed,q(:,i),constraints{:},ikoptions);
        end
%         fprintf('Max com deviation: %5.3f m\n',ladder_opts.com_tol_local);
      end
      if info > 4
        %       disp(infeasible);keyboard
        n_err = n_err+1; 
        if first_err
          err_segments(end+1,1) = i/nt;
          first_err = false;
        else
          err_segments(end,2) = i/nt;
        end
        idx_t_infeasible(i) = true;
      else
        first_err = true;
      end
    else
      first_err = true;
%       fprintf('Max com deviation: %5.3f m\n',ladder_opts.com_tol);
    end;
    if ladder_opts.verbose
      msg = '%3.0f%% (No. Errors: %2.0f)';
      fprintf([repmat('\b',1,len_prev_msg), msg],i/nt*100,n_err);
      len_prev_msg = length(sprintf(msg,i/nt*100,n_err));
    end
    if ladder_opts.compute_intro && i==1
      % Compute plan from q0 to q(:,1)
      nt_init = floor(nt/4);
      dt = mean(diff(ts));
      t_init = 0:dt:nt_init*dt;
      q_init_nom = PPTrajectory(foh([t_init(1),t_init(end)],[q0,q(:,1)]));
      q_init = eval(q_init_nom,t_init);
    end
    q_seed = q(:,i);
    constraint_array{i} = constraints;
    kinsol = doKinematics(r,q_seed);
    com_prev = r.getCOM(kinsol);
    com_prev(3) = NaN;
  end
  fprintf('\n');
  q_data = q;
  t_data = ts;
  if ladder_opts.compute_intro
    q_data = [q_init,q];
    t_data = [t_init, ts+t_init(end)+dt];
    idx_t_infeasible = [false(size(t_init)),idx_t_infeasible];
  end
  if ladder_opts.use_final_com_constraint
    % Compute plan from q(:,end) to qf
    nt_end = floor(nt/4);
    dt = mean(diff(ts));
    t_end = 0:dt:nt_end*dt;
    t_end_coarse = linspace(t_end(1),t_end(end),3);
    % com_constraint_f = WorldCoMConstraint(r,com,com,t_end(end)*[1,1]);
    com_constraint_f = QuasiStaticConstraint(r);
    com_constraint_f = com_constraint_f.setActive(true);
    com_constraint_f = com_constraint_f.setShrinkFactor(ladder_opts.final_shrink_factor);
    foot1_pts = r.getBodyContacts(ee_info.feet(1).idx);
    foot2_pts = r.getBodyContacts(ee_info.feet(2).idx);
    com_constraint_f = com_constraint_f.addContact(ee_info.feet(1).idx,foot1_pts,ee_info.feet(2).idx,foot2_pts);
    constraints(cellfun(@(con) isa(con,'WorldCoMConstraint'),constraints)) = [];
    back_z_constraint_f = PostureConstraint(r);
    back_z_constraint_f = back_z_constraint_f.setJointLimits([6;findPositionIndices(r,'back_bkz')],[q0(6);0],[q0(6);0]);
    
    [qf,info,infeasible] = inverseKin(r,q(:,end),qstar,constraints{1:end},com_constraint_f,back_z_constraint_f,ikoptions);
    if info ~= 1, warning('robotLaderPlanner:badInfo','info = %d',info); end;
    
    q_end_nom = PPTrajectory(foh([t_end(1),t_end(end)],[q(:,end),qf]));
    end_posture_constraint = PostureConstraint(r,t_end_coarse(end)*[1,1]);
    end_posture_constraint = end_posture_constraint.setJointLimits((1:nq)',qf,qf);
    ikoptions = ikoptions.setAdditionaltSamples(t_end);
    x_end_traj = PPTrajectory(foh(t_end([1,end]),[[q(:,end),qf];zeros(nq,2)]));
    x_end = eval(x_end_traj,t_end);
    q_end = x_end(1:nq,:);
    q_data = [q_data,q_end];
    t_data = [t_data, t_end + t_data(end) + dt];
    idx_t_infeasible = [idx_t_infeasible,repmat(info>10,size(t_end))];
  end
%   display(err_segments);
  if ladder_opts.smooth_output
    for i = 1:nq
      q_data(i,:) = smooth(q_data(i,:)',ladder_opts.smoothing_span);
    end
  end
end
