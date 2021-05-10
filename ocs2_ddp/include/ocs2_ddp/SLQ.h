/******************************************************************************
Copyright (c) 2017, Farbod Farshidian. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
******************************************************************************/

#pragma once

#include <ocs2_core/integration/Integrator.h>
#include <ocs2_core/integration/SystemEventHandler.h>

#include "GaussNewtonDDP.h"
#include "riccati_equations/ContinuousTimeRiccatiEquations.h"

namespace ocs2 {

/**
 * This class is an interface class for the single-thread and multi-thread SLQ.
 */
class SLQ final : public GaussNewtonDDP {
 public:
  using BASE = GaussNewtonDDP;

  /**
   * Constructor
   *
   * @param [in] ddpSettings: Structure containing the settings for the DDP algorithm.
   * @param [in] rollout: The rollout class used for simulating the system dynamics.
   * @param [in] systemDynamics: The system dynamics and its derivatives for subsystems.
   * @param [in] constraint: The system constraint function and its derivatives for subsystems.
   * @param [in] costFunction: The cost function (intermediate and final costs) and its derivatives for subsystems.
   * @param [in] operatingTrajectories: The operating trajectories of system which will be used for initialization of SLQ.
   * @param [in] preComputationPtr: The pre-computation pointer.
   */
  SLQ(ddp::Settings ddpSettings, const RolloutBase& rollout, const SystemDynamicsBase& systemDynamics, const ConstraintBase& constraint,
      const CostFunctionBase& costFunction, const SystemOperatingTrajectoriesBase& operatingTrajectories,
      const PreComputation* preComputationPtr = nullptr);

  /**
   * Default destructor.
   */
  ~SLQ() override = default;

 protected:
  matrix_t computeHamiltonianHessian(const ModelData& modelData, const matrix_t& Sm) const override;

  void approximateIntermediateLQ(const scalar_array_t& timeTrajectory, const size_array_t& postEventIndices,
                                 const vector_array_t& stateTrajectory, const vector_array_t& inputTrajectory,
                                 std::vector<ModelData>& modelDataTrajectory) override;

  void calculateControllerWorker(size_t workerIndex, size_t partitionIndex, size_t timeIndex) override;

  scalar_t solveSequentialRiccatiEquations(const matrix_t& SmFinal, const vector_t& SvFinal, const scalar_t& sFinal) override;

  void riccatiEquationsWorker(size_t workerIndex, size_t partitionIndex, const matrix_t& SmFinal, const vector_t& SvFinal,
                              const scalar_t& sFinal) override;

  /**
   * Integrates the riccati equation and generates the value function at the times set in nominal Time Trajectory.
   *
   * @param riccatiIntegrator [in] : Riccati integrator object
   * @param riccatiEquation [in] : Riccati equation object
   * @param nominalTimeTrajectory [in] : time trajectory produced in the forward rollout.
   * @param nominalEventsPastTheEndIndices [in] : Indices into nominalTimeTrajectory to point to times right after event times
   * @param allSsFinal [in] : Final value of the value function.
   * @param SsNormalizedTime [out] : Time trajectory of the value function.
   * @param SsNormalizedPostEventIndices [out] : Indices into SsNormalizedTime to point to times right after event times
   * @param allSsTrajectory [out] : Value function in vector format.
   */
  void integrateRiccatiEquationNominalTime(IntegratorBase& riccatiIntegrator, ContinuousTimeRiccatiEquations& riccatiEquation,
                                           const scalar_array_t& nominalTimeTrajectory, const size_array_t& nominalEventsPastTheEndIndices,
                                           vector_t allSsFinal, scalar_array_t& SsNormalizedTime,
                                           size_array_t& SsNormalizedPostEventIndices, vector_array_t& allSsTrajectory);

  /**
   * Integrates the riccati equation and freely selects the time nodes for the value function.
   *
   * @param riccatiIntegrator [in] : Riccati integrator object
   * @param riccatiEquation [in] : Riccati equation object
   * @param nominalTimeTrajectory [in] : time trajectory produced in the forward rollout.
   * @param nominalEventsPastTheEndIndices [in] : Indices into nominalTimeTrajectory to point to times right after event times
   * @param allSsFinal [in] : Final value of the value function.
   * @param SsNormalizedTime [out] : Time trajectory of the value function.
   * @param SsNormalizedPostEventIndices [out] : Indices into SsNormalizedTime to point to times right after event times
   * @param allSsTrajectory [out] : Value function in vector format.
   */
  void integrateRiccatiEquationAdaptiveTime(IntegratorBase& riccatiIntegrator, ContinuousTimeRiccatiEquations& riccatiEquation,
                                            const scalar_array_t& nominalTimeTrajectory, const size_array_t& nominalEventsPastTheEndIndices,
                                            vector_t allSsFinal, scalar_array_t& SsNormalizedTime,
                                            size_array_t& SsNormalizedPostEventIndices, vector_array_t& allSsTrajectory);

  /****************
   *** Variables **
   ****************/
  std::vector<std::shared_ptr<ContinuousTimeRiccatiEquations>> riccatiEquationsPtrStock_;
  std::vector<std::unique_ptr<IntegratorBase>> riccatiIntegratorPtrStock_;
};

}  // namespace ocs2
