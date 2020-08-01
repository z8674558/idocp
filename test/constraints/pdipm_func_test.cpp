#include <string>
#include <random>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include "Eigen/Core"

#include "constraints/pdipm_func.hpp"


namespace idocp {
namespace pdipm {


class PDIPMFuncTest : public ::testing::Test {
protected:
  virtual void SetUp() {
    srand((unsigned int) time(0));
    dim_ = 100;
    barrier_ = 0.001;
    slack_.array() = Eigen::VectorXd::Random(dim_).array().abs();
    dual_.array() = Eigen::VectorXd::Random(dim_).array().abs();
    dslack_.array() = Eigen::VectorXd::Random(dim_);
    ddual_.array() = Eigen::VectorXd::Random(dim_);
  }

  virtual void TearDown() {
  }

  int dim_;
  double barrier_;
  Eigen::VectorXd slack_, dual_, dslack_, ddual_;
};


TEST_F(PDIPMFuncTest, SetSlackAndDualPositive) {
  slack_ = Eigen::VectorXd::Random(dim_);
  dual_ = Eigen::VectorXd::Random(dim_);
  Eigen::VectorXd slack_tmp = slack_;
  Eigen::VectorXd dual_tmp = dual_;
  pdipmfunc::SetSlackAndDualPositive(dim_, barrier_, slack_, dual_);
  for (int i=0; i<dim_; ++i) {
    EXPECT_TRUE(slack_(i) >= barrier_);
    EXPECT_TRUE(dual_(i) >= barrier_);
  }
}


TEST_F(PDIPMFuncTest, ComputeDualityResidual) {
  EXPECT_TRUE(slack_.minCoeff() >= 0);
  EXPECT_TRUE(dual_.minCoeff() >= 0);
  Eigen::VectorXd dual_residual = Eigen::VectorXd::Zero(dim_);
  pdipmfunc::ComputeDualityResidual(barrier_, slack_, dual_, dual_residual);
  Eigen::VectorXd dual_residual_ref = Eigen::VectorXd::Zero(dim_);
  for (int i=0; i<dim_; ++i) {
    dual_residual_ref(i) = slack_(i) * dual_(i) - barrier_;
  }
  EXPECT_TRUE(dual_residual.isApprox(dual_residual_ref));
}


TEST_F(PDIPMFuncTest, FractionToBoundary) {
  EXPECT_TRUE(slack_.minCoeff() >= 0);
  EXPECT_TRUE(dual_.minCoeff() >= 0);
  const double fraction_rate = 0.995;
  const double step_slack = pdipmfunc::FractionToBoundary(dim_, fraction_rate, 
                                                          slack_, dslack_);
  Eigen::VectorXd slack_tmp = slack_ + step_slack * dslack_;
  EXPECT_TRUE(slack_tmp.minCoeff() >= 0);
  const double step_dual = pdipmfunc::FractionToBoundary(dim_, fraction_rate, 
                                                         dual_, ddual_);
  Eigen::VectorXd dual_tmp = dual_ + step_dual * ddual_;
  EXPECT_TRUE(dual_tmp.minCoeff() >= 0);
}


TEST_F(PDIPMFuncTest, ComputeDualDirection) {
  Eigen::VectorXd duality = Eigen::VectorXd::Zero(dim_);
  duality.array() = dual_.array() * slack_.array() - barrier_;
  pdipmfunc::ComputeDualDirection(dual_, slack_, dslack_, duality, ddual_);
  Eigen::VectorXd ddual_ref = Eigen::VectorXd::Zero(dim_);
  for (int i=0; i<dim_; ++i) {
    ddual_ref(i) = - (dual_(i) * dslack_(i) + duality(i)) / slack_(i);
  }
  EXPECT_TRUE(ddual_ref.isApprox(ddual_));
}


TEST_F(PDIPMFuncTest, SlackBarrierCost) {
  const double cost_ref = - barrier_ * (slack_.array().log()).sum();
  const double cost = pdipmfunc::SlackBarrierCost(dim_, barrier_, slack_);
  EXPECT_DOUBLE_EQ(cost_ref, cost);
}

} // namespace pdipm
} // namespace invdynocp


int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}