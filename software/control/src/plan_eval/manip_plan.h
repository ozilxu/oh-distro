#pragma once

#include "generic_plan.h"
namespace plan_eval {
class ManipPlan : public GenericPlan {
public:
  ManipPlan(const std::string &urdf_name, const std::string &config_name) : GenericPlan(urdf_name, config_name) {
    ;
  }

  void HandleCommittedRobotPlan(const void *msg,
                                const DrakeRobotState &est_rs,
                                const Eigen::VectorXd &last_q_d);

  drake::lcmt_qp_controller_input MakeQPInput(const DrakeRobotState &est_rs);

  Eigen::VectorXd GetLatestKeyFrame(double time);

  std::string param_set_name_;
};
}// plan_eval

