#include "idocp/cost/time_varying_task_space_3d_cost.hpp"

#include <iostream>


namespace idocp {

TimeVaryingTaskSpace3DCost::TimeVaryingTaskSpace3DCost(
    const Robot& robot, const int frame_id, 
    const std::shared_ptr<TimeVaryingTaskSpace3DRefBase>& ref) 
  : CostFunctionComponentBase(),
    frame_id_(frame_id),
    ref_(ref),
    q_3d_weight_(Eigen::Vector3d::Zero()),
    qf_3d_weight_(Eigen::Vector3d::Zero()),
    qi_3d_weight_(Eigen::Vector3d::Zero()) {
}


TimeVaryingTaskSpace3DCost::TimeVaryingTaskSpace3DCost()
  : CostFunctionComponentBase(),
    frame_id_(),
    ref_(),
    q_3d_weight_(),
    qf_3d_weight_(),
    qi_3d_weight_() {
}


TimeVaryingTaskSpace3DCost::~TimeVaryingTaskSpace3DCost() {
}


bool TimeVaryingTaskSpace3DCost::useKinematics() const {
  return true;
}


void TimeVaryingTaskSpace3DCost::set_ref(
    const std::shared_ptr<TimeVaryingTaskSpace3DRefBase>& ref) {
  ref_ = ref;
}


void TimeVaryingTaskSpace3DCost::set_q_3d_weight(
    const Eigen::Vector3d& q_3d_weight) {
  q_3d_weight_ = q_3d_weight;
}


void TimeVaryingTaskSpace3DCost::set_qf_3d_weight(
    const Eigen::Vector3d& qf_3d_weight) {
  qf_3d_weight_ = qf_3d_weight;
}


void TimeVaryingTaskSpace3DCost::set_qi_3d_weight(
    const Eigen::Vector3d& qi_3d_weight) {
  qi_3d_weight_ = qi_3d_weight;
}


double TimeVaryingTaskSpace3DCost::computeStageCost(
    Robot& robot, CostFunctionData& data, const double t, const double dtau, 
    const SplitSolution& s) const {
  double l = 0;
  ref_->compute_q_3d_ref(t, data.q_3d_ref);
  data.diff_3d = robot.framePosition(frame_id_) - data.q_3d_ref;
  l += (q_3d_weight_.array()*data.diff_3d.array()*data.diff_3d.array()).sum();
  return 0.5 * dtau * l;
}


double TimeVaryingTaskSpace3DCost::computeTerminalCost(
    Robot& robot, CostFunctionData& data, const double t, 
    const SplitSolution& s) const {
  double l = 0;
  ref_->compute_q_3d_ref(t, data.q_3d_ref);
  data.diff_3d = robot.framePosition(frame_id_) - data.q_3d_ref;
  l += (qf_3d_weight_.array()*data.diff_3d.array()*data.diff_3d.array()).sum();
  return 0.5 * l;
}


double TimeVaryingTaskSpace3DCost::computeImpulseCost(
    Robot& robot, CostFunctionData& data, const double t, 
    const ImpulseSplitSolution& s) const {
  double l = 0;
  ref_->compute_q_3d_ref(t, data.q_3d_ref);
  data.diff_3d = robot.framePosition(frame_id_) - data.q_3d_ref;
  l += (qi_3d_weight_.array()*data.diff_3d.array()*data.diff_3d.array()).sum();
  return 0.5 * l;
}


void TimeVaryingTaskSpace3DCost::computeStageCostDerivatives(
    Robot& robot, CostFunctionData& data, const double t, const double dtau, 
    const SplitSolution& s, SplitKKTResidual& kkt_residual) const {
  ref_->compute_q_3d_ref(t, data.q_3d_ref);
  data.diff_3d = robot.framePosition(frame_id_) - data.q_3d_ref;
  robot.getFrameJacobian(frame_id_, data.J_6d);
  data.J_3d.noalias() 
      = robot.frameRotation(frame_id_) * data.J_6d.template topRows<3>();
  kkt_residual.lq().noalias() 
      += dtau * data.J_3d.transpose() * q_3d_weight_.asDiagonal() * data.diff_3d;
}


void TimeVaryingTaskSpace3DCost::computeTerminalCostDerivatives(
    Robot& robot, CostFunctionData& data, const double t, 
    const SplitSolution& s, SplitKKTResidual& kkt_residual) const {
  ref_->compute_q_3d_ref(t, data.q_3d_ref);
  data.diff_3d = robot.framePosition(frame_id_) - data.q_3d_ref;
  robot.getFrameJacobian(frame_id_, data.J_6d);
  data.J_3d.noalias() 
      = robot.frameRotation(frame_id_) * data.J_6d.template topRows<3>();
  kkt_residual.lq().noalias() 
      += data.J_3d.transpose() * qf_3d_weight_.asDiagonal() * data.diff_3d;
}


void TimeVaryingTaskSpace3DCost::computeImpulseCostDerivatives(
    Robot& robot, CostFunctionData& data, const double t, 
    const ImpulseSplitSolution& s, 
    ImpulseSplitKKTResidual& kkt_residual) const {
  ref_->compute_q_3d_ref(t, data.q_3d_ref);
  data.diff_3d = robot.framePosition(frame_id_) - data.q_3d_ref;
  robot.getFrameJacobian(frame_id_, data.J_6d);
  data.J_3d.noalias() 
      = robot.frameRotation(frame_id_) * data.J_6d.template topRows<3>();
  kkt_residual.lq().noalias() 
      += data.J_3d.transpose() * qi_3d_weight_.asDiagonal() * data.diff_3d;
}


void TimeVaryingTaskSpace3DCost::computeStageCostHessian(
    Robot& robot, CostFunctionData& data, const double t, const double dtau, 
    const SplitSolution& s, SplitKKTMatrix& kkt_matrix) const {
  robot.getFrameJacobian(frame_id_, data.J_6d);
  data.J_3d.noalias() 
      = robot.frameRotation(frame_id_) * data.J_6d.template topRows<3>();
  kkt_matrix.Qqq().noalias()
      += dtau * data.J_3d.transpose() * q_3d_weight_.asDiagonal() * data.J_3d;
}


void TimeVaryingTaskSpace3DCost::computeTerminalCostHessian(
    Robot& robot, CostFunctionData& data, const double t, 
    const SplitSolution& s, SplitKKTMatrix& kkt_matrix) const {
    robot.getFrameJacobian(frame_id_, data.J_6d);
    data.J_3d.noalias() 
        = robot.frameRotation(frame_id_) * data.J_6d.template topRows<3>();
    kkt_matrix.Qqq().noalias()
        += data.J_3d.transpose() * qf_3d_weight_.asDiagonal() * data.J_3d;
}


void TimeVaryingTaskSpace3DCost::computeImpulseCostHessian(
    Robot& robot, CostFunctionData& data, const double t, 
    const ImpulseSplitSolution& s, ImpulseSplitKKTMatrix& kkt_matrix) const {
    robot.getFrameJacobian(frame_id_, data.J_6d);
    data.J_3d.noalias() 
        = robot.frameRotation(frame_id_) * data.J_6d.template topRows<3>();
    kkt_matrix.Qqq().noalias()
        += data.J_3d.transpose() * qi_3d_weight_.asDiagonal() * data.J_3d;
}

} // namespace idocp