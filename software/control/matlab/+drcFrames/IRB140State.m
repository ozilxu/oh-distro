classdef IRB140State < SingletonCoordinateFrame
  
  methods
    function obj=IRB140State(r)
      typecheck(r,{'TimeSteppingRigidBodyManipulator','RigidBodyManipulator'});
      manipStateFrame = r.getManipulator().getStateFrame();
      if (r.hands > 0 || length(manipStateFrame.coordinates) > 12)
        manipStateFrame = manipStateFrame.getFrameByNum(1);
      end
      coordinates = manipStateFrame.coordinates;
      obj = obj@SingletonCoordinateFrame('IRB140State',length(coordinates),'x',coordinates);
    end
  end
end
