#pragma once

#include <Eigen/Dense>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <ctime>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <ocs2_comm_interfaces/SystemObservation.h>
#include <ocs2_core/logic/machine/HybridLogicRulesMachine.h>
#include <ocs2_mpc/MPC_BASE.h>

// For RosWarnStream, can be removed if std::cout is used instead
#include <ros/ros.h>

namespace ocs2 {

/**
 * A lean ROS independent interface to OCS2. In incorporates the functionality of the MPC and the MRT (trajectory tracking) modules.
 * Please refer to ocs2_double_integrator_noros_example for a minimal example
 * @tparam STATE_DIM
 * @tparam INPUT_DIM
 */
template <size_t STATE_DIM, size_t INPUT_DIM>
class MPC_Interface {
 public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW

  typedef std::shared_ptr<MPC_Interface<STATE_DIM, INPUT_DIM>> Ptr;

  typedef MPC_BASE<STATE_DIM, INPUT_DIM> mpc_t;

  typedef typename mpc_t::scalar_t scalar_t;
  typedef typename mpc_t::scalar_array_t scalar_array_t;
  typedef typename mpc_t::size_array_t size_array_t;
  typedef typename mpc_t::state_vector_t state_vector_t;
  typedef typename mpc_t::state_vector_array_t state_vector_array_t;
  typedef typename mpc_t::state_vector_array2_t state_vector_array2_t;
  typedef typename mpc_t::input_vector_t input_vector_t;
  typedef typename mpc_t::input_vector_array_t input_vector_array_t;
  typedef typename mpc_t::input_vector_array2_t input_vector_array2_t;
  typedef typename mpc_t::controller_t controller_t;
  typedef typename mpc_t::controller_ptr_array_t controller_ptr_array_t;
  typedef typename mpc_t::input_state_matrix_t input_state_matrix_t;
  typedef typename mpc_t::input_state_matrix_array_t input_state_matrix_array_t;

  typedef typename mpc_t::cost_desired_trajectories_t cost_desired_trajectories_t;
  typedef typename mpc_t::mode_sequence_template_t mode_sequence_template_t;

  typedef SystemObservation<STATE_DIM, INPUT_DIM> system_observation_t;

  typedef LinearInterpolation<state_vector_t, Eigen::aligned_allocator<state_vector_t>> state_linear_interpolation_t;
  typedef LinearInterpolation<input_vector_t, Eigen::aligned_allocator<input_vector_t>> input_linear_interpolation_t;

  /**
   * Constructor
   * @param[in] mpc the underlying MPC class to be used
   * @param logicRules
   */
  MPC_Interface(mpc_t& mpc, std::shared_ptr<HybridLogicRules> logicRules = nullptr);

  /**
   * Destructor.
   */
  virtual ~MPC_Interface() = default;

  /**
   * Resets the class to its instantiate state.
   */
  virtual void reset();

  /**
   * Set the new observation to be considered during the next MPC iteration.
   * It is safe to set a new value while the MPC optimization is running
   * @param [in] currentObservation the new observation
   */
  void setCurrentObservation(const system_observation_t& currentObservation);

  /**
   * Set new target trajectories to be tracked.
   * It is safe to set a new value while the MPC optimization is running
   * @param targetTrajectories
   */
  void setTargetTrajectories(const cost_desired_trajectories_t& targetTrajectories);

  /**
   * Set a new mode sequence template
   * It is safe to set a new value while the MPC optimization is running
   * @param modeSequenceTemplate
   */
  void setModeSequence(const mode_sequence_template_t& modeSequenceTemplate);

  /**
   * Advance the mpc module for one iteration.
   * The evaluation methods can be called while this method is running.
   * They will evaluate the last computed control law
   */
  void advanceMpc();

  /**
   * Call this before calling the evaluation methods.
   * @return true if there is a policy to evaluated
   */
  bool policyReceived() const { return numIterations_ > 0; }

  /**
   * Gets a reference to CostDesiredTrajectories for which the current policy is optimized for.
   *
   * @return a constant reference to CostDesiredTrajectories of the policy.
   */
  const cost_desired_trajectories_t& mpcCostDesiredTrajectories() const;

  /**
   * Evaluates the latest policy at the given time and state. Moreover
   * it finds the active subsystem at the given time.
   *
   * @param [in] time: The inquiry time.
   * @param [in] currentState: The state at which the policy is to be evaluated
   * @param [out] mpcState: The policy's optimized (nominal) state (dependent on time).
   * @param [out] mpcInput: The policy's optimized input (dependent on time and currentState).
   * @param [out] subsystem: The active subsystem (dependent on time).
   */
  void evaluatePolicy(const scalar_t& time, const state_vector_t& currentState, state_vector_t& mpcState, input_vector_t& mpcInput,
                      size_t& subsystem);

  const scalar_array_t& getMpcTimeTrajectory();
  const state_vector_array_t& getMpcStateTrajectory();
  const input_vector_array_t& getMpcInputTrajectory();

 protected:
  mpc_t* mpcPtr_;
  MPC_Settings mpcSettings_;

  size_t numIterations_;
  scalar_t maxDelay_ = 0;
  scalar_t meanDelay_ = 0;
  scalar_t currentDelay_ = 0;

  std::chrono::time_point<std::chrono::steady_clock> startTimePoint_;
  std::chrono::time_point<std::chrono::steady_clock> finalTimePoint_;

  std::mutex observationMutex_;
  std::atomic<bool> observationUpdated_;

  // MPC inputs
  system_observation_t currentObservation_;

  // MPC outputs:
  size_array_t mpcSubsystemsSequenceBuffer_;
  scalar_array_t mpcEventTimesBuffer_;
  scalar_array_t mpcTimeTrajectoryBuffer_;
  state_vector_array_t mpcStateTrajectoryBuffer_;
  input_vector_array_t mpcInputTrajectoryBuffer_;
  std::vector<std::unique_ptr<controller_t>> mpcControllersBuffer_;
  cost_desired_trajectories_t mpcSolverCostDesiredTrajectoriesBuffer_;
  std::atomic<bool> mpcOutputBufferUpdated_;
  bool logicUpdated_;
  std::mutex mpcBufferMutex;

  // variables for the tracking controller:
  scalar_array_t eventTimes_;
  size_array_t subsystemsSequence_;
  scalar_array_t partitioningTimes_;

  std::vector<std::unique_ptr<controller_t>> mpcControllers_;
  scalar_array_t mpcTimeTrajectory_;
  state_vector_array_t mpcStateTrajectory_;
  input_vector_array_t mpcInputTrajectory_;
  state_linear_interpolation_t mpcLinInterpolateState_;
  input_linear_interpolation_t mpcLinInterpolateInput_;
  cost_desired_trajectories_t mpcCostDesiredTrajectories_;

  std::unique_ptr<HybridLogicRulesMachine> logicMachine_;
  std::function<size_t(scalar_t)> findActiveSubsystemFnc_;

 protected:
  /**
   * Checks the data buffer for an update of the MPC policy. If a new policy
   * is available on the buffer this method will load it to the in-use policy.
   *
   * @return True if the policy is updated.
   */
  bool updatePolicy();

  /**
   * Constructs a partitioningTimes vector with 2 elements: current observation time
   * and the maximum value of the numeric type scalar_t. This prevents
   * the frequent update of the logicRules.
   *
   * @param [out] partitioningTimes: Partitioning time.
   */
  void partitioningTimesUpdate(scalar_array_t& partitioningTimes) const;

  /**
   * @brief fillMpcOutputBuffers updates the *Buffer variables from the MPC object.
   * This method is automatically called by advanceMpc()
   */
  void fillMpcOutputBuffers();
};

}  // namespace ocs2

#include "implementation/MPC_Interface.h"
