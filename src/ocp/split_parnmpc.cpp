#include "idocp/ocp/split_parnmpc.hpp"
#include "idocp/ocp/parnmpc_linearizer.hpp"

#include <assert.h>


namespace idocp {

SplitParNMPC::SplitParNMPC(const Robot& robot, 
                           const std::shared_ptr<CostFunction>& cost, 
                           const std::shared_ptr<Constraints>& constraints) 
  : cost_(cost),
    cost_data_(robot),
    constraints_(constraints),
    constraints_data_(),
    joint_constraints_(robot),
    kkt_matrix_(robot),
    kkt_residual_(robot),
    kkt_composition_(robot),
    linearizer_(robot),
    id_condenser_(robot),
    du_(Eigen::VectorXd::Zero(robot.dimv())),
    dbeta_(Eigen::VectorXd::Zero(robot.dimv())),
    x_res_(Eigen::VectorXd::Zero(2*robot.dimv())),
    dx_(Eigen::VectorXd::Zero(2*robot.dimv())),
    u_tmp_(Eigen::VectorXd::Zero(robot.dimv())), 
    u_res_tmp_(Eigen::VectorXd::Zero(robot.dimv())),
    kkt_matrix_inverse_(Eigen::MatrixXd::Zero(kkt_composition_.max_dimKKT(), 
                                              kkt_composition_.max_dimKKT())) {
  if (!constraints_->isEmpty()) {
    constraints_data_ = std::move(constraints->createConstraintsData(robot));
  }
}


SplitParNMPC::SplitParNMPC() 
  : cost_(),
    cost_data_(),
    constraints_(),
    constraints_data_(),
    joint_constraints_(),
    kkt_matrix_(),
    kkt_residual_(),
    kkt_composition_(),
    linearizer_(),
    id_condenser_(),
    du_(),
    dbeta_(),
    x_res_(),
    dx_(),
    u_tmp_(), 
    u_res_tmp_(),
    kkt_matrix_inverse_() {
}


SplitParNMPC::~SplitParNMPC() {
}


bool SplitParNMPC::isFeasible(const Robot& robot, const SplitSolution& s) {
  return joint_constraints_.isFeasible(s.q, s.v, s.a, s.u);
  return constraints_->isFeasible(robot, constraints_data_, 
                                  s.a, s.f, s.q, s.v, s.u);
}


void SplitParNMPC::initConstraints(const Robot& robot, const int time_step, 
                                   const double dtau, const SplitSolution& s) {
  assert(time_step >= 0);
  assert(dtau > 0);
  joint_constraints_.setTimeStep(time_step);
  joint_constraints_.setSlackAndDual(dtau, s.q, s.v, s.a, s.u);
  constraints_->setSlackAndDual(robot, constraints_data_, dtau, 
                                s.a, s.f, s.q, s.v, s.u);
}


void SplitParNMPC::coarseUpdate(Robot& robot, const double t, const double dtau, 
                                const Eigen::VectorXd& q_prev, 
                                const Eigen::VectorXd& v_prev,
                                const SplitSolution& s,
                                const Eigen::VectorXd& lmd_next,
                                const Eigen::VectorXd& gmm_next,
                                const Eigen::VectorXd& q_next,
                                const Eigen::MatrixXd& aux_mat_next_old,
                                SplitDirection& d, 
                                SplitSolution& s_new_coarse) {
  assert(dtau > 0);
  assert(q_prev.size() == robot.dimq());
  assert(v_prev.size() == robot.dimv());
  assert(lmd_next.size() == robot.dimv());
  assert(gmm_next.size() == robot.dimv());
  assert(q_next.size() == robot.dimq());
  assert(aux_mat_next_old.rows() == 2*robot.dimv());
  assert(aux_mat_next_old.cols() == 2*robot.dimv());
  assert(s.dimc() == robot.dim_passive()+robot.dimf());
  assert(d.dimc() == robot.dim_passive()+robot.dimf());
  assert(s_new_coarse.dimc() == robot.dim_passive()+robot.dimf());
  assert(s.dimf() == robot.dimf());
  assert(d.dimf() == robot.dimf());
  assert(s_new_coarse.dimf() == robot.dimf());
  // Reset the KKT matrix and KKT residual.
  kkt_matrix_.setZero();
  kkt_matrix_.setContactStatus(robot);
  kkt_residual_.setZero();
  kkt_residual_.setContactStatus(robot);
  kkt_composition_.set(robot);
  if (robot.dimf() > 0) {
    robot.updateKinematics(s.q, s.v, s.a);
  }
  // Linearize cost, dynamics, and constraints. 
  linearizer_.linearizeStageCost(robot, cost_, cost_data_, t, dtau, s, kkt_residual_);
  linearizer_.linearizeStateEquation(robot, dtau, q_prev, v_prev, s, lmd_next, 
                                     gmm_next, q_next, kkt_residual_, kkt_matrix_);
  linearizer_.linearizeContactConstraints(robot, dtau, kkt_residual_, kkt_matrix_);
  // Condense inverse dynamics.
  id_condenser_.setContactStatus(robot);
  id_condenser_.linearizeStageCost(robot, cost_, cost_data_, t, dtau, s);
  id_condenser_.linearizeInequalityConstraints(robot, joint_constraints_, t, dtau, s);
  id_condenser_.linearizeInverseDynamics(robot, s);
  if (robot.has_floating_base()) {
    id_condenser_.condenseFloatingBaseConstraint(dtau, s, kkt_residual_, kkt_matrix_);
  }
  id_condenser_.condenseInverseDynamics(kkt_residual_, kkt_matrix_);
  // Augmnet the partial derivatives of the inequality constriants.
  joint_constraints_.augmentDualResidual(dtau, kkt_residual_.lq(), 
                                         kkt_residual_.lv(), 
                                         kkt_residual_.la());
  // Augment the equality constraints 
  kkt_residual_.lq().noalias() += kkt_matrix_.Cq().transpose() * s.mu_active();
  kkt_residual_.lv().noalias() += kkt_matrix_.Cv().transpose() * s.mu_active();
  kkt_residual_.la().noalias() += kkt_matrix_.Ca().transpose() * s.mu_active();
  if (robot.dimf() > 0) {
    kkt_residual_.lf().noalias() 
        += kkt_matrix_.Cf().bottomRows(robot.dim_passive()).transpose() 
            * s.mu_active().tail(robot.dim_passive());
  }
  // Condense the slack and dual variables of the inequality constraints 
  joint_constraints_.condenseSlackAndDual(dtau, s.q, s.v, s.a, kkt_matrix_.Qqq(), 
                                          kkt_matrix_.Qvv(), kkt_matrix_.Qaa(), 
                                          kkt_residual_.lq(), kkt_residual_.lv(), 
                                          kkt_residual_.la());
  // Augment the cost function Hessian. 
  cost_->augment_lqq(robot, cost_data_, t, dtau, s.q, s.v, s.a, kkt_matrix_.Qqq());
  cost_->augment_lvv(robot, cost_data_, t, dtau, s.q, s.v, s.a, kkt_matrix_.Qvv());
  cost_->augment_laa(robot, cost_data_, t, dtau, s.q, s.v, s.a, kkt_matrix_.Qaa());
  if (robot.dimf() > 0) {
    cost_->augment_lff(robot, cost_data_, t, dtau, s.f, kkt_matrix_.Qff());
  }
  kkt_matrix_.symmetrize();
  kkt_matrix_.Qxx().noalias() += aux_mat_next_old;
  const int dim_kkt = kkt_matrix_.dimKKT();
  kkt_matrix_.invert(kkt_matrix_inverse_.topLeftCorner(dim_kkt, dim_kkt));
  // coarse update of the solution
  d.split_direction() = kkt_matrix_inverse_.topLeftCorner(dim_kkt, dim_kkt) 
                          * kkt_residual_.KKT_residual();
  s_new_coarse.lmd = s.lmd - d.dlmd();
  s_new_coarse.gmm = s.gmm - d.dgmm();
  s_new_coarse.mu_active() = s.mu_active() - d.dmu();
  s_new_coarse.a = s.a - d.da();
  s_new_coarse.f_active() = s.f_active() - d.df();
  robot.integrateConfiguration(s.q, d.dq(), -1, s_new_coarse.q);
  s_new_coarse.v = s.v - d.dv();
}


void SplitParNMPC::coarseUpdate(Robot& robot, const double t, const double dtau, 
                                const Eigen::VectorXd& q_prev, 
                                const Eigen::VectorXd& v_prev,
                                const SplitSolution& s, SplitDirection& d, 
                                SplitSolution& s_new_coarse) {
  assert(dtau > 0);
  assert(q_prev.size() == robot.dimq());
  assert(v_prev.size() == robot.dimv());
  assert(s.dimc() == robot.dim_passive()+robot.dimf());
  assert(d.dimc() == robot.dim_passive()+robot.dimf());
  assert(s_new_coarse.dimc() == robot.dim_passive()+robot.dimf());
  assert(s.dimf() == robot.dimf());
  assert(d.dimf() == robot.dimf());
  assert(s_new_coarse.dimf() == robot.dimf());
  // Reset the KKT matrix and KKT residual.
  kkt_matrix_.setZero();
  kkt_matrix_.setContactStatus(robot);
  kkt_residual_.setZero();
  kkt_residual_.setContactStatus(robot);
  kkt_composition_.set(robot);
  if (robot.dimf() > 0) {
    robot.updateKinematics(s.q, s.v, s.a);
  }
  // Linearize cost, dynamics, and constraints. 
  linearizer_.linearizeStageCost(robot, cost_, cost_data_, t, dtau, s, kkt_residual_);
  linearizer_.linearizeStateEquation(robot, dtau, q_prev, v_prev, s, 
                                     kkt_residual_, kkt_matrix_);
  linearizer_.linearizeConstraints(robot, dtau, kkt_residual_, kkt_matrix_);
  // Condense inverse dynamics.
  id_condenser_.linearizeCostAndConstraints(robot, cost_, cost_data_, 
                                            joint_constraints_, t, dtau, s);
  id_condenser_.linearizeInverseDynamics(robot, s);
  if (robot.has_floating_base()) {
    id_condenser_.condenseFloatingBaseConstraint(robot, dtau, s, kkt_residual_, 
                                                 kkt_matrix_);
  }
  id_condenser_.condenseInverseDynamics(robot, kkt_residual_, kkt_matrix_);
  // Augmnet the partial derivatives of the inequality constriants.
  joint_constraints_.augmentDualResidual(dtau, kkt_residual_.lq(), 
                                         kkt_residual_.lv(), 
                                         kkt_residual_.la());
  // Augment the equality constraints 
  kkt_residual_.lq().noalias() += kkt_matrix_.Cq().transpose() * s.mu_active();
  kkt_residual_.lv().noalias() += kkt_matrix_.Cv().transpose() * s.mu_active();
  kkt_residual_.la().noalias() += kkt_matrix_.Ca().transpose() * s.mu_active();
  if (robot.dimf() > 0) {
    kkt_residual_.lf().noalias() 
        += kkt_matrix_.Cf().bottomRows(robot.dim_passive()).transpose() 
            * s.mu_active().tail(robot.dim_passive());
  }
  // Condense the slack and dual variables of the inequality constraints 
  joint_constraints_.condenseSlackAndDual(dtau, s.q, s.v, s.a, kkt_matrix_.Qqq(), 
                                          kkt_matrix_.Qvv(), kkt_matrix_.Qaa(), 
                                          kkt_residual_.lq(), 
                                          kkt_residual_.lv(), 
                                          kkt_residual_.la());
  // Augment the cost function Hessian. 
  cost_->augment_lqq(robot, cost_data_, t, dtau, s.q, s.v, s.a, kkt_matrix_.Qqq());
  cost_->augment_lvv(robot, cost_data_, t, dtau, s.q, s.v, s.a, kkt_matrix_.Qvv());
  cost_->augment_laa(robot, cost_data_, t, dtau, s.q, s.v, s.a, kkt_matrix_.Qaa());
  if (robot.dimf() > 0) {
    cost_->augment_lff(robot, cost_data_, t, dtau, s.f, kkt_matrix_.Qff());
  }
  cost_->augment_phiqq(robot, cost_data_, t, s.q, s.v, kkt_matrix_.Qqq());
  cost_->augment_phivv(robot, cost_data_, t, s.q, s.v, kkt_matrix_.Qvv());
  kkt_matrix_.symmetrize();
  const int dim_kkt = kkt_matrix_.dimKKT();
  kkt_matrix_.invert(kkt_matrix_inverse_.topLeftCorner(dim_kkt, dim_kkt));
  // coarse update of the solution
  d.split_direction() = kkt_matrix_inverse_.topLeftCorner(dim_kkt, dim_kkt) 
                          * kkt_residual_.KKT_residual();
  s_new_coarse.lmd = s.lmd - d.dlmd();
  s_new_coarse.gmm = s.gmm - d.dgmm();
  s_new_coarse.mu_active() = s.mu_active() - d.dmu();
  s_new_coarse.a = s.a - d.da();
  s_new_coarse.f_active() = s.f_active() - d.df();
  robot.integrateConfiguration(s.q, d.dq(), -1, s_new_coarse.q);
  s_new_coarse.v = s.v - d.dv();
}


void SplitParNMPC::getAuxiliaryMatrix(Eigen::MatrixXd& aux_mat) {
 aux_mat = - kkt_matrix_inverse_.topLeftCorner(kkt_composition_.Fx_size(), 
                                               kkt_composition_.Fx_size());
}


void SplitParNMPC::backwardCollectionSerial(const Robot& robot,
                                            const SplitSolution& s_old_next, 
                                            const SplitSolution& s_new_next,
                                            SplitSolution& s_new) {
  x_res_.head(robot.dimv()) = s_new_next.lmd - s_old_next.lmd;
  x_res_.tail(robot.dimv()) = s_new_next.gmm - s_old_next.gmm;
  dx_ = kkt_matrix_inverse_.block(kkt_composition_.Fx_begin(), 
                                  kkt_composition_.Qx_begin(), 
                                  kkt_composition_.Fx_size(), 
                                  kkt_composition_.Qx_size()) * x_res_;
  s_new.lmd.noalias() -= dx_.head(robot.dimv());
  s_new.gmm.noalias() -= dx_.tail(robot.dimv());
}


void SplitParNMPC::backwardCollectionParallel(const Robot& robot, 
                                              SplitDirection& d,
                                              SplitSolution& s_new) {
  const int dim_kkt = kkt_composition_.dimKKT();
  const int dimx = 2*robot.dimv();
  d.split_direction().segment(dimx, dim_kkt-dimx) 
      = kkt_matrix_inverse_.block(dimx, dim_kkt-dimx, dim_kkt-dimx, dimx) * x_res_;
  s_new.mu_active().noalias() -= d.dmu();
  s_new.a.noalias() -= d.da();
  s_new.f_active().noalias() -= d.df();
  robot.integrateConfiguration(d.dq(), -1, s_new.q);
  s_new.v.noalias() -= d.dv();
}


void SplitParNMPC::forwardCollectionSerial(const Robot& robot,
                                           const SplitSolution& s_old_prev,
                                           const SplitSolution& s_new_prev, 
                                           SplitSolution& s_new) {
  robot.subtractConfiguration(s_new_prev.q, s_old_prev.q, x_res_.head(robot.dimv()));
  x_res_.tail(robot.dimv()) = s_new_prev.v - s_old_prev.v;
  dx_ = kkt_matrix_inverse_.block(kkt_composition_.Qx_begin(), 
                                  kkt_composition_.Fx_begin(), 
                                  kkt_composition_.Qx_size(), 
                                  kkt_composition_.Fx_size()) * x_res_;
  robot.integrateConfiguration(dx_.head(robot.dimv()), -1, s_new.q);
  s_new.v.noalias() -= dx_.tail(robot.dimv());
}


void SplitParNMPC::forwardCollectionParallel(const Robot& robot,
                                             SplitDirection& d,
                                             SplitSolution& s_new) {
  const int dim_kkt = kkt_composition_.dimKKT();
  const int dimx = 2*robot.dimv();
  d.split_direction().segment(0, dim_kkt-dimx) 
      = kkt_matrix_inverse_.block(0, 0, dim_kkt-dimx, dimx) * x_res_;
  s_new.lmd.noalias() -= d.dlmd();
  s_new.gmm.noalias() -= d.dgmm();
  s_new.mu_active().noalias() -= d.dmu();
  s_new.a.noalias() -= d.da();
  s_new.f_active().noalias() -= d.df();
}


void SplitParNMPC::computePrimalAndDualDirection(const Robot& robot, 
                                                 const double dtau, 
                                                 const SplitSolution& s, 
                                                 const SplitSolution& s_new,
                                                 SplitDirection& d) {
  d.dlmd() = s_new.lmd - s.lmd;
  d.dgmm() = s_new.gmm - s.gmm;
  d.dmu() = s_new.mu_active() - s.mu_active();
  d.da() = s_new.a - s.a;
  d.df() = s_new.f_active() - s.f_active();
  robot.subtractConfiguration(s_new.q, s.q, d.dq());
  d.dv() = s_new.v - s.v;
  id_condenser_.computeCondensedDirection(d);
  joint_constraints_.computeSlackAndDualDirection(dtau, d.dq(), d.dv(), d.da(), du_);
}

 
double SplitParNMPC::maxPrimalStepSize() {
  return joint_constraints_.maxSlackStepSize();
}


double SplitParNMPC::maxDualStepSize() {
  return joint_constraints_.maxDualStepSize();
}


std::pair<double, double> SplitParNMPC::stageCostAndConstraintsViolation(
    Robot& robot, const double t, const double dtau, const SplitSolution& s,
    const bool is_terminal) {
  assert(dtau > 0);
  double cost = 0;
  cost += cost_->l(robot, cost_data_, t, dtau, s.q, s.v, s.a, s.f, s.u);
  if (is_terminal) {
    cost += cost_->phi(robot, cost_data_, t, s.q, s.v);
  }
  cost += joint_constraints_.costSlackBarrier();
  double constraints_violation = 0;
  constraints_violation += kkt_residual_.Fq().lpNorm<1>();
  constraints_violation += kkt_residual_.Fv().lpNorm<1>();
  constraints_violation += dtau * u_res_.lpNorm<1>();
  constraints_violation += joint_constraints_.residualL1Nrom(dtau, s.q, s.v, 
                                                             s.a, s.u);
  constraints_violation += kkt_residual_.C().lpNorm<1>();
  return std::make_pair(cost, constraints_violation);
}


std::pair<double, double> SplitParNMPC::stageCostAndConstraintsViolation(
    Robot& robot, const double step_size, const double t, const double dtau, 
    const Eigen::VectorXd& q_prev, const Eigen::VectorXd& v_prev, 
    const SplitSolution& s, const SplitDirection& d, SplitSolution& s_tmp) {
  assert(step_size > 0);
  assert(step_size <= 1);
  assert(dtau > 0);
  assert(q_prev.size() == robot.dimq());
  assert(v_prev.size() == robot.dimv());
  s_tmp.q = s.q;
  robot.integrateConfiguration(d.dq(), step_size, s_tmp.q);
  s_tmp.v = s.v + step_size * d.dv();
  s_tmp.a = s.a + step_size * d.da();
  s_tmp.u = s.u + step_size * du_;
  if (robot.dimf() > 0) {
    s_tmp.f.head(kkt_composition_.Qf_size()) 
        = s.f.head(kkt_composition_.Qf_size()) + step_size * d.df();
    robot.setContactForces(s_tmp.f);
  }
  double cost = 0;
  cost += cost_->l(robot, cost_data_, t, dtau, s_tmp.a, s_tmp.f,  
                   s_tmp.q, s_tmp.v, s_tmp.u);
  cost += joint_constraints_.costSlackBarrier(step_size);
  robot.subtractConfiguration(q_prev, s_tmp.q, kkt_residual_.Fq());
  kkt_residual_.Fq().noalias() += dtau * s_tmp.v;
  kkt_residual_.Fv() = v_prev + - s_tmp.v + dtau * s_tmp.a;
  robot.RNEA(s_tmp.q, s_tmp.v, s_tmp.a, u_res_tmp_);
  u_res_tmp_.noalias() -= s_tmp.u;
  if (robot.dimf() > 0) {
    robot.updateKinematics(s_tmp.q, s_tmp.v, s_tmp.a);
    robot.computeBaumgarteResidual(dtau, kkt_residual_.C());
  }
  if (robot.has_floating_base()) {
    kkt_residual_.C().tail(robot.dim_passive()) 
        = dtau * s_tmp.u.head(robot.dim_passive());
  }
  double constraints_violation = 0;
  constraints_violation += kkt_residual_.Fq().lpNorm<1>();
  constraints_violation += kkt_residual_.Fv().lpNorm<1>();
  constraints_violation += dtau * u_res_tmp_.lpNorm<1>();
  constraints_violation += joint_constraints_.residualL1Nrom(dtau, s_tmp.q, 
                                                             s_tmp.v, s_tmp.a, 
                                                             s_tmp.u);
  constraints_violation += kkt_residual_.C().lpNorm<1>();
  return std::make_pair(cost, constraints_violation);
}


std::pair<double, double> SplitParNMPC::stageCostAndConstraintsViolation(
    Robot& robot, const double step_size, const double t, const double dtau, 
    const Eigen::VectorXd& q_prev, const Eigen::VectorXd& v_prev, 
    const Eigen::VectorXd& dq_prev, const Eigen::VectorXd& dv_prev, 
    const SplitSolution& s, const SplitDirection& d, SplitSolution& s_tmp, 
    const bool is_terminal) {
  assert(step_size > 0);
  assert(step_size <= 1);
  assert(dtau > 0);
  assert(q_prev.size() == robot.dimq());
  assert(v_prev.size() == robot.dimv());
  assert(dq_prev.size() == robot.dimv());
  assert(dv_prev.size() == robot.dimv());
  s_tmp.q = s.q;
  robot.integrateConfiguration(d.dq(), step_size, s_tmp.q);
  s_tmp.v = s.v + step_size * d.dv();
  s_tmp.a = s.a + step_size * d.da();
  s_tmp.u = s.u + step_size * du_;
  if (robot.dimf() > 0) {
    s_tmp.f.head(kkt_composition_.Qf_size()) 
        = s.f.head(kkt_composition_.Qf_size()) + step_size * d.df();
    robot.setContactForces(s_tmp.f);
  }
  double cost = 0;
  cost += cost_->l(robot, cost_data_, t, dtau, s_tmp.a, s_tmp.f,  
                   s_tmp.q, s_tmp.v, s_tmp.u);
  if (is_terminal) {
    cost += cost_->phi(robot, cost_data_, t, s_tmp.q, s_tmp.v);
  }
  cost += joint_constraints_.costSlackBarrier(step_size);
  robot.subtractConfiguration(q_prev, s_tmp.q, kkt_residual_.Fq());
  kkt_residual_.Fq().noalias() += dtau * s_tmp.v + step_size * dq_prev;
  kkt_residual_.Fv() = v_prev + step_size * dv_prev - s_tmp.v + dtau * s_tmp.a;
  robot.RNEA(s_tmp.q, s_tmp.v, s_tmp.a, u_res_tmp_);
  u_res_tmp_.noalias() -= s_tmp.u;
  if (robot.dimf() > 0) {
    robot.updateKinematics(s_tmp.q, s_tmp.v, s_tmp.a);
    robot.computeBaumgarteResidual(dtau, kkt_residual_.C());
  }
  if (robot.has_floating_base()) {
    kkt_residual_.C().tail(robot.dim_passive()) 
        = dtau * s_tmp.u.head(robot.dim_passive());
  }
  double constraints_violation = 0;
  constraints_violation += kkt_residual_.Fq().lpNorm<1>();
  constraints_violation += kkt_residual_.Fv().lpNorm<1>();
  constraints_violation += dtau * u_res_tmp_.lpNorm<1>();
  constraints_violation += joint_constraints_.residualL1Nrom(dtau, s_tmp.q, 
                                                             s_tmp.v, s_tmp.a, 
                                                             s_tmp.u);
  constraints_violation += kkt_residual_.C().lpNorm<1>();
  return std::make_pair(cost, constraints_violation);
}


void SplitParNMPC::updateDual(const double step_size) {
  assert(step_size > 0);
  assert(step_size <= 1);
  joint_constraints_.updateDual(step_size);
}


void SplitParNMPC::updatePrimal(Robot& robot, const double step_size, 
                                const double dtau, const SplitDirection& d, 
                                SplitSolution& s) {
  assert(step_size > 0);
  assert(step_size <= 1);
  assert(dtau > 0);
  s.lmd.noalias() += step_size * d.dlmd();
  s.gmm.noalias() += step_size * d.dgmm();
  s.mu_active().noalias() += step_size * d.dmu();
  s.a.noalias() += step_size * d.da();
  s.f_active().noalias() += step_size * d.df();
  robot.integrateConfiguration(d.dq(), step_size, s.q);
  s.v.noalias() += step_size * d.dv();
  s.u.noalias() += step_size * d.du;
  s.beta.noalias() += step_size * d.dbeta;
  joint_constraints_.updateSlack(step_size);
}


void SplitParNMPC::getStateFeedbackGain(Eigen::MatrixXd& Kq, 
                                        Eigen::MatrixXd& Kv) const {
  // Kq = du_dq_ + du_da_ * Kaq_ + du_df_.leftCols(dimf_) * Kfq_.topRows(dimf_);
  // Kv = du_dv_ + du_da_ * Kav_ + du_df_.leftCols(dimf_) * Kfv_.topRows(dimf_);
}


double SplitParNMPC::squaredKKTErrorNorm(Robot& robot, const double t, 
                                         const double dtau, 
                                         const Eigen::VectorXd& q_prev, 
                                         const Eigen::VectorXd& v_prev, 
                                         const SplitSolution& s,
                                         const Eigen::VectorXd& lmd_next,
                                         const Eigen::VectorXd& gmm_next,
                                         const Eigen::VectorXd& q_next,
                                         const bool is_terminal) {
  assert(dtau > 0);
  assert(q_prev.size() == robot.dimq());
  assert(v_prev.size() == robot.dimv());
  assert(lmd_next.size() == robot.dimv());
  assert(gmm_next.size() == robot.dimv());
  assert(q_next.size() == robot.dimq());
  // Reset the KKT matrix and KKT residual.
  kkt_matrix_.setZero();
  kkt_matrix_.setContactStatus(robot);
  kkt_residual_.setZero();
  kkt_residual_.setContactStatus(robot);
  kkt_composition_.set(robot);
  // Compute KKT residual and KKT matrix.
  if (robot.dimf() > 0) {
    robot.updateKinematics(s.q, s.v, s.a);
  }
  parnmpclinearizer::linearizeStageCost(robot, cost_, cost_data_, t, dtau, s,
                                        kkt_residual_, lu_);
  parnmpclinearizer::linearizeDynamics(robot, dtau, q_prev, v_prev, s,
                                       kkt_residual_);
  parnmpclinearizer::linearizeConstraints(robot, dtau, kkt_residual_, 
                                          kkt_matrix_);
  // Augmnet the partial derivatives of the state equation.
  if (robot.has_floating_base()) {
    robot.dSubtractdConfigurationMinus(q_prev, s.q, dsubtract_dqminus_);
    robot.dSubtractdConfigurationPlus(s.q, q_next, dsubtract_dqplus_);
    kkt_residual_.lq().noalias() 
        += dsubtract_dqminus_.transpose() * s.lmd 
            + dsubtract_dqplus_.transpose() * lmd_next;
  }
  else {
    kkt_residual_.lq().noalias() += lmd_next - s.lmd;
  }
  kkt_residual_.lv().noalias() += dtau * s.lmd - s.gmm + gmm_next;
  kkt_residual_.la().noalias() += dtau * s.gmm;
    // Augment the partial derivatives of the inverse dynamics constraint. 
  kkt_residual_.lq().noalias() += dtau * du_dq_.transpose() * s.beta;
  kkt_residual_.lv().noalias() += dtau * du_dv_.transpose() * s.beta;
  kkt_residual_.la().noalias() += dtau * du_da_.transpose() * s.beta;
  if (robot.dimf() > 0) {
    kkt_residual_.lf().noalias() 
        += dtau * du_df_.leftCols(robot.dimf()).transpose() * s.beta;
  }
  lu_.noalias() -= dtau * s.beta;
  // Augmnet the partial derivatives of the inequality constriants.
  joint_constraints_.augmentDualResidual(dtau, kkt_residual_.lq(), 
                                         kkt_residual_.lv(), 
                                         kkt_residual_.la());
  joint_constraints_.augmentDualResidual(dtau, lu_);
  // Augment the equality constraints 
  kkt_residual_.lq().noalias() += kkt_matrix_.Cq().topRows(robot.dimf()).transpose() * s.mu.head(robot.dimf());
  kkt_residual_.lv().noalias() += kkt_matrix_.Cv().topRows(robot.dimf()).transpose() * s.mu.head(robot.dimf());
  kkt_residual_.la().noalias() += kkt_matrix_.Ca().topRows(robot.dimf()).transpose() * s.mu.head(robot.dimf());
  lu_.head(robot.dim_passive()).noalias() += dtau * s.mu.tail(robot.dim_passive()); 
  double error = 0;
  error += kkt_residual_.KKT_residual().squaredNorm();
  error += u_res_.squaredNorm();
  error += lu_.squaredNorm();
  error += joint_constraints_.residualSquaredNrom(dtau, s.q, s.v, s.a, s.u);
  return error;
}

} // namespace idocp