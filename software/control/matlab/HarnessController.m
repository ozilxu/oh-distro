classdef HarnessController < DRCController
  
  properties (SetAccess=protected,GetAccess=protected)
    robot;
    floating;
  end  
    
  methods
    function obj = HarnessController(name,r,timeout)
      typecheck(r,'Atlas');

      ctrl_data = SharedDataHandle(struct('qtraj',[],'ti_flag',true));
      
      % instantiate QP controller
      options = struct();
      options.R = 1e-12*eye(getNumInputs(r));
      qp = HarnessQPController(r,options);

      % cascade PD controller 
      if getNumDOF(r)==34 % floating model
        options.Kp=diag([zeros(6,1); 200*ones(getNumDOF(r)-6,1)]);
        float = true;
      else
        options.Kp=diag(200*ones(getNumDOF(r),1));
        float = false;
      end
      
      options.Kd=0.12*options.Kp;
      pd = SimplePDController(r,ctrl_data,options);
      ins(1).system = 1;
      ins(1).input = 1;
      ins(2).system = 2;
      ins(2).input = 2;
      outs(1).system = 2;
      outs(1).output = 1;
      sys = mimoCascade(pd,qp,[],ins,outs);

      obj = obj@DRCController(name,sys);

      obj.robot = r;
      obj.controller_data = ctrl_data;
      obj.floating = float;
      
      if nargin < 3
        % controller timeout must match the harness time set in VRCPlugin.cpp
        obj = setTimedTransition(obj,5,'standing',true);
      else
        obj = setTimedTransition(obj,timeout,'standing',true);
      end
      
      obj = addLCMTransition(obj,'COMMITTED_ROBOT_PLAN',drc.robot_plan_t(),name);
%       obj = addPrecomputeResponseHandler(obj,'STANDING_PREC_RESPONSE','standing');
    end
    
    function obj = initialize(obj,data)
      
      if isfield(data,'COMMITTED_ROBOT_PLAN')
        % pinned reaching plan
        msg = getfield(data,'COMMITTED_ROBOT_PLAN');
        [xtraj,ts] = RobotPlanListener.decodeRobotPlan(msg,true);        
        if obj.floating
          qtraj = PPTrajectory(spline(ts,xtraj(1:getNumDOF(obj.robot),:)));
        else
          qtraj = PPTrajectory(spline(ts,xtraj(6+(1:getNumDOF(obj.robot)),:)));
        end
        obj.controller_data.setField('ti_flag',false);
        obj = setDuration(obj,inf,false); % set the controller timeout
      else
        % use saved nominal pose
        d = load('data/atlas_fp.mat');
        if ~obj.floating
          qf = d.xstar(6+(1:getNumDOF(obj.robot)));
        else
          qf = d.xstar(1:getNumDOF(obj.robot));
        end
        q0 = zeros(getNumDOF(obj.robot),1);
        qtraj = PPTrajectory(spline([0 2],[q0 qf]));
        
        obj.controller_data.setField('ti_flag',true);
      end
      
      obj.controller_data.setField('qtraj',qtraj);
  
    end
  end  
end
