import lcm
import drc
import numpy as np

import py_drake_utils as ut
import bdi_step.footsteps
from bdi_step.plotting import draw_swing
from bdi_step.utils import Behavior, gl

NUM_REQUIRED_WALK_STEPS = 4

class Mode:
    translating = 0
    plotting = 1

class BDIStepTranslator:
    def __init__(self, mode=Mode.translating):
        self.mode = mode
        self.lc = lcm.LCM()
        if self.mode == Mode.plotting:
            self.gl = gl
        else:
            self.gl = None
        self.bdi_step_queue = []
        self.delivered_index = None
        self.drift_from_plan = np.zeros((3,1))
        self.behavior = Behavior.BDI_STEPPING

    def handle_footstep_plan(self, channel, msg_data):
        print "Starting new footstep plan"
        msg = drc.footstep_plan_t.decode(msg_data)
        footsteps, opts = bdi_step.footsteps.decode_footstep_plan(msg)

        behavior = opts['behavior']
        if behavior == Behavior.BDI_WALKING:
            # duration = 0.6
            if len(footsteps) < NUM_REQUIRED_WALK_STEPS+2:
                msg = 'ERROR: Footstep plan must be at least 4 steps for BDI walking translation'
                print msg
                ut.send_status(6,0,0,msg)
                return
        elif behavior != Behavior.BDI_STEPPING:
            m = "BDI step translator: Ignoring footstep plan without BDI_WALKING or BDI_STEPPING behavior"
            print m
            ut.send_status(6,0,0,m)
            return

        footsteps = [f.to_bdi_spec(behavior, j-1) for j,f in enumerate(footsteps)]
        self.behavior = behavior

        if self.mode == Mode.plotting:
            self.draw(footsteps)

            self.lc.publish('TRANSLATED_BDI_FOOTSTEP_PLAN', msg_data)

            # this should be temporary. Once we have the correct plan execution GUI utils, we should just publish on TRANSLATED_BDI_FOOTSTEP_PLAN instead, and the GUI will re-send on COMMITTED_FOOTSTEP_PLAN
            self.lc.publish('COMMITTED_FOOTSTEP_PLAN', msg_data)


        else:
            self.bdi_step_queue = footsteps[2:]  # cut out the first two steps (which are just the current positions of the feet)

            # Relative step heights
            for i in reversed(range(len(self.bdi_step_queue))):
                self.bdi_step_queue[i].foot.position[2] -= footsteps[i+1].foot.position[2]

            # self.send_walk_params(1)
            self.send_params(1)

    def handle_atlas_status(self, channel, msg_data):
        if self.delivered_index is None or self.mode != Mode.translating:
            return
        msg = drc.atlas_status_t.decode(msg_data)
        if self.behavior == Behavior.BDI_WALKING:
            index_needed = msg.walk_feedback.next_step_index_needed
            if index_needed > (self.delivered_index + 1) and len(self.bdi_step_queue) >= (index_needed + 2):
                print "Handling request for next step: {:d}".format(index_needed)
                # self.update_drift(msg.walk_feedback.step_queue_saturated)
                self.send_walk_params(index_needed-1)
        else:
            index_needed = msg.step_feedback.next_step_index_needed
            if index_needed > self.delivered_index and len(self.bdi_step_queue) >= index_needed:
                print "Handling request for next step: {:d}".format(index_needed)
                self.send_walk_params(index_needed)

    # def update_drift(self,step_queue):
    #     print "Updating drift calculation"
    #     planned = np.reshape(self.bdi_step_queue[self.delivered_index-1].position,(3,1))
    #     returned = np.reshape(step_queue[0].position,(3,1))
    #     print "planned:", planned
    #     print "returned:", returned
    #     self.drift_from_plan = returned - planned
    #     for s in self.bdi_step_queue[self.delivered_index:]:
    #         s.position[:2,0] += self.drift_from_plan[:2,0]

    def send_params(self,step_index):
        """
        Publish the next steppping footstep or up to the next 4 walking footsteps as needed.
        """
        assert self.mode == Mode.translating, "Translator in Mode.plotting mode is not allowed to send step/walk params"
        if self.behavior == Behavior.BDI_WALKING:
            walk_param_msg = drc.atlas_behavior_walk_params_t()
            walk_param_msg.num_required_walk_steps = NUM_REQUIRED_WALK_STEPS
            walk_param_msg.step_queue = [drc.atlas_step_data_t() for j in range(NUM_REQUIRED_WALK_STEPS)]  # Unused
            walk_param_msg.walk_spec_queue = self.bdi_step_queue[step_index-1:step_index+3]
            walk_param_msg.use_spec = True;
            walk_param_msg.use_relative_step_height = 1  # as of Atlas 2.5.0 this flag is disabled and always acts as if it's set to 1
            walk_param_msg.use_demo_walk = 0
            self.lc.publish('ATLAS_WALK_PARAMS', walk_param_msg.encode())
            self.delivered_index = walk_param_msg.walk_spec_queue[0].step_index
            print "Sent walk params for step indices {:d} through {:d}".format(walk_param_msg.walk_spec_queue[0].step_index, walk_param_msg.walk_spec_queue[-1].step_index)
        elif self.behavior == Behavior.BDI_STEPPING:
            step_param_msg = drc.atlas_behavior_step_params_t()
            step_param_msg.desired_step = drc.atlas_step_data_t()  # Unused
            step_param_msg.desired_step_spec = self.bdi_step_queue[step_index-1]
            step_param_msg.use_relative_step_height = 1  # as of Atlas 2.5.0 this flag is disabled and always acts as if it's set to 1
            step_param_msg.use_demo_walk = 0
            self.lc.publish('ATLAS_STEP_PARAMS', step_param_msg.encode())
            self.delivered_index = step_param_msg.desired_step_spec.step_index
            print "Sent step params for step index {:d}".format(step_param_msg.desired_step_spec.step_index)
        else:
            raise ValueError("Bad behavior value: {:s}".format(self.behavior))

    def handle_stop_walking(self, channel, msg_data):
        """
        Generate a set of footsteps with -1 step indices, which will cause the BDI controller to switch to standing instead of continuing to walk
        """
        old_behavior = self.behavior
        for behavior in [Behavior.BDI_STEPPING, Behavior.BDI_WALKING]:
            # Make sure we send stopping commands for both behaviors just in case
            self.behavior = behavior
            if self.behavior == Behavior.BDI_WALKING:
                n_steps = 4
            else:
                n_steps = 1
            footsteps = [bdi_step.footsteps.FootGoal(pos=np.zeros((6,1)),
                                  step_speed=0,
                                  step_height=0,
                                  step_id=0,
                                  pos_fixed=np.zeros((6,1)),
                                  is_right_foot=0,
                                  is_in_contact=0,
                                  bdi_step_duration=0,
                                  bdi_sway_duration=0,
                                  bdi_lift_height=0,
                                  bdi_toe_off=0,
                                  bdi_knee_nominal=0)] * n_steps

            self.bdi_step_queue = [f.to_bdi_spec(behavior, -1) for f in footsteps]
            self.send_params(1)
        self.behavior = old_behavior

    def run(self):
        if self.mode == Mode.translating:
            print "BDIStepTranslator running in robot-side translator mode"
            self.lc.subscribe('COMMITTED_FOOTSTEP_PLAN', self.handle_footstep_plan)
            self.lc.subscribe('STOP_WALKING', self.handle_stop_walking)
        else:
            print "BDIStepTranslator running in base-side plotter mode"
            self.lc.subscribe('APPROVED_FOOTSTEP_PLAN', self.handle_footstep_plan)
        self.lc.subscribe('ATLAS_STATUS', self.handle_atlas_status)
        while True:
            self.lc.handle()

    def draw(self, footsteps):
        """
        Plot a rough guess of each swing foot trajectory, based on the BDI software manual's description of how swing_height and lift_height behave.
        """
        for j in range(len(footsteps)-2):
            st0 = footsteps[j]
            st1 = footsteps[j+2]
            is_stepping = self.behavior==Behavior.BDI_STEPPING
            if is_stepping:
                lift_height = st0.action.lift_height
            else:
                lift_height = None
            draw_swing(self.gl,
                       st0.foot.position,
                       st1.foot.position,
                       st0.action.swing_height,
                       is_stepping=is_stepping,
                       lift_height=lift_height)
        self.gl.switch_buffer()
