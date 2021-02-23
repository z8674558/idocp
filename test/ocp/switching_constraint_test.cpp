#include <string>

#include <gtest/gtest.h>

#include "Eigen/Core"

#include "idocp/robot/robot.hpp"
#include "idocp/ocp/split_kkt_residual.hpp"
#include "idocp/ocp/split_kkt_matrix.hpp"
#include "idocp/ocp/split_solution.hpp"
#include "idocp/ocp/switching_constraint.hpp"


namespace idocp {

class SwitchingConstraintTest : public ::testing::Test {
protected:
  virtual void SetUp() {
    srand((unsigned int) time(0));
    std::random_device rnd;
    fixed_base_urdf = "../urdf/iiwa14/iiwa14.urdf";
    floating_base_urdf = "../urdf/anymal/anymal.urdf";
  }

  virtual void TearDown() {
  }

  static void testLinearizeSwitchingConstraint(Robot& robot);
  static void testComputeSwitchingConstraintResidual(Robot& robot);

  std::string fixed_base_urdf, floating_base_urdf;
};


void SwitchingConstraintTest::testLinearizeSwitchingConstraint(Robot& robot) {
  auto impulse_status = robot.createImpulseStatus();
  impulse_status.setRandom();
  if (!impulse_status.hasActiveImpulse()) {
    impulse_status.activateImpulse(0);
  }
  const SplitSolution s = SplitSolution::Random(robot, impulse_status);
  SplitKKTResidual kkt_residual(robot);
  kkt_residual.setImpulseStatus(impulse_status);
  kkt_residual.lq().setRandom();
  SplitKKTMatrix kkt_matrix(robot);
  kkt_matrix.setImpulseStatus(impulse_status);
  auto kkt_residual_ref = kkt_residual;
  auto kkt_matrix_ref = kkt_matrix;
  robot.updateKinematics(s.q);
  switchingconstraint::linearizeSwitchingConstraint(robot, impulse_status, s, 
                                                    kkt_matrix, kkt_residual);
  robot.computeContactResidual(impulse_status, impulse_status.contactPoints(),
                               kkt_residual_ref.P());
  robot.computeContactDerivative(impulse_status, kkt_matrix_ref.Pq());
  kkt_residual_ref.lq().noalias() += kkt_matrix_ref.Pq().transpose() * s.xi_stack();
  EXPECT_TRUE(kkt_residual.isApprox(kkt_residual_ref));
  EXPECT_TRUE(kkt_matrix.isApprox(kkt_matrix_ref));
  const double kkt = switchingconstraint::squaredNormSwitchingConstraintResidual(kkt_residual);
  const double kkt_ref = kkt_residual_ref.P().squaredNorm();
  EXPECT_DOUBLE_EQ(kkt, kkt_ref);
  const double l1 = switchingconstraint::l1NormSwitchingConstraintResidual(kkt_residual);
  const double l1_ref = kkt_residual_ref.P().lpNorm<1>();
  EXPECT_DOUBLE_EQ(l1, l1_ref);
}


void SwitchingConstraintTest::testComputeSwitchingConstraintResidual(Robot& robot) {
  auto impulse_status = robot.createImpulseStatus();
  impulse_status.setRandom();
  if (!impulse_status.hasActiveImpulse()) {
    impulse_status.activateImpulse(0);
  }
  const SplitSolution s = SplitSolution::Random(robot, impulse_status);
  SplitKKTResidual kkt_residual(robot);
  kkt_residual.setImpulseStatus(impulse_status);
  auto kkt_residual_ref = kkt_residual;
  robot.updateKinematics(s.q);
  switchingconstraint::computeSwitchingConstraintResidual(robot, impulse_status, kkt_residual);
  robot.computeContactResidual(impulse_status, impulse_status.contactPoints(),
                               kkt_residual_ref.P());
  EXPECT_TRUE(kkt_residual.isApprox(kkt_residual_ref));
  const double kkt = switchingconstraint::squaredNormSwitchingConstraintResidual(kkt_residual);
  const double kkt_ref = kkt_residual_ref.P().squaredNorm();
  EXPECT_DOUBLE_EQ(kkt, kkt_ref);
  const double l1 = switchingconstraint::l1NormSwitchingConstraintResidual(kkt_residual);
  const double l1_ref = kkt_residual_ref.P().lpNorm<1>();
  EXPECT_DOUBLE_EQ(l1, l1_ref);
}


TEST_F(SwitchingConstraintTest, fixedbase) {
  std::vector<int> contact_frames = {18};
  Robot robot(fixed_base_urdf, contact_frames);
  testLinearizeSwitchingConstraint(robot);
  testComputeSwitchingConstraintResidual(robot);
}


TEST_F(SwitchingConstraintTest, floatingBase) {
  std::vector<int> contact_frames = {14, 24, 34, 44};
  Robot robot(floating_base_urdf, contact_frames);
  testLinearizeSwitchingConstraint(robot);
  testComputeSwitchingConstraintResidual(robot);
}

} // namespace idocp


int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}