#include "src/modelchecker/csl/SparseCtmcCslModelChecker.h"

#include <vector>

#include "src/utility/macros.h"
#include "src/utility/vector.h"
#include "src/utility/graph.h"
#include "src/utility/solver.h"
#include "src/utility/numerical.h"

#include "src/modelchecker/prctl/SparseDtmcPrctlModelChecker.h"
#include "src/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "src/modelchecker/results/ExplicitQuantitativeCheckResult.h"

#include "src/storage/StronglyConnectedComponentDecomposition.h"

#include "src/exceptions/InvalidStateException.h"
#include "src/exceptions/InvalidPropertyException.h"
#include "src/exceptions/NotImplementedException.h"

namespace storm {
    namespace modelchecker {
        template<class ValueType>
        SparseCtmcCslModelChecker<ValueType>::SparseCtmcCslModelChecker(storm::models::sparse::Ctmc<ValueType> const& model) : SparsePropositionalModelChecker<ValueType>(model), linearEquationSolverFactory(new storm::utility::solver::LinearEquationSolverFactory<ValueType>()) {
            // Intentionally left empty.
        }
        
        template<class ValueType>
        SparseCtmcCslModelChecker<ValueType>::SparseCtmcCslModelChecker(storm::models::sparse::Ctmc<ValueType> const& model, std::unique_ptr<storm::utility::solver::LinearEquationSolverFactory<ValueType>>&& linearEquationSolverFactory) : SparsePropositionalModelChecker<ValueType>(model), linearEquationSolverFactory(std::move(linearEquationSolverFactory)) {
            // Intentionally left empty.
        }
        
        template<class ValueType>
        bool SparseCtmcCslModelChecker<ValueType>::canHandle(storm::logic::Formula const& formula) const {
            return formula.isCslStateFormula() || formula.isCslPathFormula() || formula.isRewardPathFormula();
        }
        
        template<class ValueType>
        std::unique_ptr<CheckResult> SparseCtmcCslModelChecker<ValueType>::computeBoundedUntilProbabilities(storm::logic::BoundedUntilFormula const& pathFormula, bool qualitative, boost::optional<storm::logic::OptimalityType> const& optimalityType) {
            std::unique_ptr<CheckResult> leftResultPointer = this->check(pathFormula.getLeftSubformula());
            std::unique_ptr<CheckResult> rightResultPointer = this->check(pathFormula.getRightSubformula());
            ExplicitQualitativeCheckResult const& leftResult = leftResultPointer->asExplicitQualitativeCheckResult();;
            ExplicitQualitativeCheckResult const& rightResult = rightResultPointer->asExplicitQualitativeCheckResult();
            double lowerBound = 0;
            double upperBound = 0;
            if (!pathFormula.hasDiscreteTimeBound()) {
                std::pair<double, double> const& intervalBounds =  pathFormula.getIntervalBounds();
                lowerBound = intervalBounds.first;
                upperBound = intervalBounds.second;
            } else {
                upperBound = pathFormula.getDiscreteTimeBound();
            }
            
            return std::unique_ptr<CheckResult>(new ExplicitQuantitativeCheckResult<ValueType>(this->computeBoundedUntilProbabilitiesHelper(leftResult.getTruthValuesVector(), rightResult.getTruthValuesVector(), this->getModel().getExitRateVector(), qualitative, lowerBound, upperBound)));
        }
        
        template<class ValueType>
        std::unique_ptr<CheckResult> SparseCtmcCslModelChecker<ValueType>::computeNextProbabilities(storm::logic::NextFormula const& pathFormula, bool qualitative, boost::optional<storm::logic::OptimalityType> const& optimalityType) {
            std::unique_ptr<CheckResult> subResultPointer = this->check(pathFormula.getSubformula());
            ExplicitQualitativeCheckResult const& subResult = subResultPointer->asExplicitQualitativeCheckResult();
            std::vector<ValueType> result = SparseDtmcPrctlModelChecker<ValueType>::computeNextProbabilitiesHelper(this->computeProbabilityMatrix(this->getModel().getTransitionMatrix(), this->getModel().getExitRateVector()), subResult.getTruthValuesVector(), *this->linearEquationSolverFactory);
            return std::unique_ptr<CheckResult>(new ExplicitQuantitativeCheckResult<ValueType>(std::move(result)));
        }
        
        template<class ValueType>
        std::unique_ptr<CheckResult> SparseCtmcCslModelChecker<ValueType>::computeUntilProbabilities(storm::logic::UntilFormula const& pathFormula, bool qualitative, boost::optional<storm::logic::OptimalityType> const& optimalityType) {
            std::unique_ptr<CheckResult> leftResultPointer = this->check(pathFormula.getLeftSubformula());
            std::unique_ptr<CheckResult> rightResultPointer = this->check(pathFormula.getRightSubformula());
            ExplicitQualitativeCheckResult const& leftResult = leftResultPointer->asExplicitQualitativeCheckResult();
            ExplicitQualitativeCheckResult const& rightResult = rightResultPointer->asExplicitQualitativeCheckResult();
            return std::unique_ptr<CheckResult>(new ExplicitQuantitativeCheckResult<ValueType>(this->computeUntilProbabilitiesHelper(this->computeProbabilityMatrix(this->getModel().getTransitionMatrix(), this->getModel().getExitRateVector()), this->getModel().getBackwardTransitions(), leftResult.getTruthValuesVector(), rightResult.getTruthValuesVector(), qualitative, *this->linearEquationSolverFactory)));
        }
        
        template<class ValueType>
        storm::models::sparse::Ctmc<ValueType> const& SparseCtmcCslModelChecker<ValueType>::getModel() const {
            return this->template getModelAs<storm::models::sparse::Ctmc<ValueType>>();
        }
        
        template<class ValueType>
        std::vector<ValueType> SparseCtmcCslModelChecker<ValueType>::computeBoundedUntilProbabilitiesHelper(storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates, std::vector<ValueType> const& exitRates, bool qualitative, double lowerBound, double upperBound) const {
            // If the time bounds are [0, inf], we rather call untimed reachability.
            storm::utility::ConstantsComparator<ValueType> comparator;
            if (comparator.isZero(lowerBound) && comparator.isInfinity(upperBound)) {
                return this->computeUntilProbabilitiesHelper(this->getModel().getTransitionMatrix(), this->getModel().getBackwardTransitions(), phiStates, psiStates, qualitative, *this->linearEquationSolverFactory);
            }
            
            // From this point on, we know that we have to solve a more complicated problem [t, t'] with either t != 0
            // or t' != inf.
            
            // Create the result vector.
            std::vector<ValueType> result;
            
            // If we identify the states that have probability 0 of reaching the target states, we can exclude them from the
            // further computations.
            storm::storage::SparseMatrix<ValueType> backwardTransitions = this->getModel().getBackwardTransitions();
            storm::storage::BitVector statesWithProbabilityGreater0 = storm::utility::graph::performProbGreater0(backwardTransitions, phiStates, psiStates);
            STORM_LOG_INFO("Found " << statesWithProbabilityGreater0.getNumberOfSetBits() << " states with probability greater 0.");
            storm::storage::BitVector statesWithProbabilityGreater0NonPsi = statesWithProbabilityGreater0 & ~psiStates;
            STORM_LOG_INFO("Found " << statesWithProbabilityGreater0NonPsi.getNumberOfSetBits() << " 'maybe' states.");
            
            if (!statesWithProbabilityGreater0NonPsi.empty()) {
                if (comparator.isZero(upperBound)) {
                    // In this case, the interval is of the form [0, 0].
                    result = std::vector<ValueType>(this->getModel().getNumberOfStates(), storm::utility::zero<ValueType>());
                    storm::utility::vector::setVectorValues<ValueType>(result, psiStates, storm::utility::one<ValueType>());
                } else {
                    if (comparator.isZero(lowerBound)) {
                        // In this case, the interval is of the form [0, t].
                        // Note that this excludes [0, inf] since this is untimed reachability and we considered this case earlier.
                        
                        // Find the maximal rate of all 'maybe' states to take it as the uniformization rate.
                        ValueType uniformizationRate = 0;
                        for (auto const& state : statesWithProbabilityGreater0NonPsi) {
                            uniformizationRate = std::max(uniformizationRate, exitRates[state]);
                        }
                        uniformizationRate *= 1.02;
                        STORM_LOG_THROW(uniformizationRate > 0, storm::exceptions::InvalidStateException, "The uniformization rate must be positive.");
                        
                        // Compute the uniformized matrix.
                        storm::storage::SparseMatrix<ValueType> uniformizedMatrix = this->computeUniformizedMatrix(this->getModel().getTransitionMatrix(), statesWithProbabilityGreater0NonPsi, uniformizationRate, exitRates);
                        
                        // Compute the vector that is to be added as a compensation for removing the absorbing states.
                        std::vector<ValueType> b = this->getModel().getTransitionMatrix().getConstrainedRowSumVector(statesWithProbabilityGreater0NonPsi, psiStates);
                        for (auto& element : b) {
                            element /= uniformizationRate;
                        }
                        
                        // Finally compute the transient probabilities.
                        std::vector<ValueType> values(statesWithProbabilityGreater0NonPsi.getNumberOfSetBits(), storm::utility::zero<ValueType>());
                        std::vector<ValueType> subresult = this->computeTransientProbabilities(uniformizedMatrix, &b, upperBound, uniformizationRate, values, *this->linearEquationSolverFactory);
                        result = std::vector<ValueType>(this->getModel().getNumberOfStates(), storm::utility::zero<ValueType>());
                        
                        storm::utility::vector::setVectorValues(result, statesWithProbabilityGreater0NonPsi, subresult);
                        storm::utility::vector::setVectorValues(result, psiStates, storm::utility::one<ValueType>());
                    } else if (comparator.isInfinity(upperBound)) {
                        // In this case, the interval is of the form [t, inf] with t != 0.
                        
                        // Start by computing the (unbounded) reachability probabilities of reaching psi states while
                        // staying in phi states.
                        result = this->computeUntilProbabilitiesHelper(this->getModel().getTransitionMatrix(), backwardTransitions, phiStates, psiStates, qualitative, *this->linearEquationSolverFactory);
                        
                        // Determine the set of states that must be considered further.
                        storm::storage::BitVector relevantStates = statesWithProbabilityGreater0 & phiStates;
                        std::vector<ValueType> subResult(relevantStates.getNumberOfSetBits());
                        storm::utility::vector::selectVectorValues(subResult, relevantStates, result);
                        
                        ValueType uniformizationRate = 0;
                        for (auto const& state : relevantStates) {
                            uniformizationRate = std::max(uniformizationRate, exitRates[state]);
                        }
                        uniformizationRate *= 1.02;
                        STORM_LOG_THROW(uniformizationRate > 0, storm::exceptions::InvalidStateException, "The uniformization rate must be positive.");
                        
                        // Compute the uniformized matrix.
                        storm::storage::SparseMatrix<ValueType> uniformizedMatrix = this->computeUniformizedMatrix(this->getModel().getTransitionMatrix(), relevantStates, uniformizationRate, exitRates);
                        
                        // Compute the transient probabilities.
                        subResult = this->computeTransientProbabilities(uniformizedMatrix, nullptr, lowerBound, uniformizationRate, subResult, *this->linearEquationSolverFactory);
                        
                        // Fill in the correct values.
                        storm::utility::vector::setVectorValues(result, ~relevantStates, storm::utility::zero<ValueType>());
                        storm::utility::vector::setVectorValues(result, relevantStates, subResult);
                    } else {
                        // In this case, the interval is of the form [t, t'] with t != 0 and t' != inf.
                        
                        if (lowerBound != upperBound) {
                            // In this case, the interval is of the form [t, t'] with t != 0, t' != inf and t != t'.
                            
                            // Find the maximal rate of all 'maybe' states to take it as the uniformization rate.
                            ValueType uniformizationRate = storm::utility::zero<ValueType>();
                            for (auto const& state : statesWithProbabilityGreater0NonPsi) {
                                uniformizationRate = std::max(uniformizationRate, exitRates[state]);
                            }
                            uniformizationRate *= 1.02;
                            STORM_LOG_THROW(uniformizationRate > 0, storm::exceptions::InvalidStateException, "The uniformization rate must be positive.");
                            
                            // Compute the (first) uniformized matrix.
                            storm::storage::SparseMatrix<ValueType> uniformizedMatrix = this->computeUniformizedMatrix(this->getModel().getTransitionMatrix(), statesWithProbabilityGreater0NonPsi, uniformizationRate, exitRates);
                            
                            // Compute the vector that is to be added as a compensation for removing the absorbing states.
                            std::vector<ValueType> b = this->getModel().getTransitionMatrix().getConstrainedRowSumVector(statesWithProbabilityGreater0NonPsi, psiStates);
                            for (auto& element : b) {
                                element /= uniformizationRate;
                            }
                            
                            // Start by computing the transient probabilities of reaching a psi state in time t' - t.
                            std::vector<ValueType> values(statesWithProbabilityGreater0NonPsi.getNumberOfSetBits(), storm::utility::zero<ValueType>());
                            std::vector<ValueType> subresult = computeTransientProbabilities(uniformizedMatrix, &b, upperBound - lowerBound, uniformizationRate, values, *this->linearEquationSolverFactory);
                            
                            storm::storage::BitVector relevantStates = statesWithProbabilityGreater0 & phiStates;
                            std::vector<ValueType> newSubresult = std::vector<ValueType>(relevantStates.getNumberOfSetBits());
                            storm::utility::vector::setVectorValues(newSubresult, statesWithProbabilityGreater0NonPsi % relevantStates, subresult);
                            storm::utility::vector::setVectorValues(newSubresult, psiStates % relevantStates, storm::utility::one<ValueType>());
                            
                            // Then compute the transient probabilities of being in such a state after t time units. For this,
                            // we must re-uniformize the CTMC, so we need to compute the second uniformized matrix.
                            uniformizationRate = storm::utility::zero<ValueType>();
                            for (auto const& state : relevantStates) {
                                uniformizationRate = std::max(uniformizationRate, exitRates[state]);
                            }
                            uniformizationRate *= 1.02;
                            STORM_LOG_THROW(uniformizationRate > 0, storm::exceptions::InvalidStateException, "The uniformization rate must be positive.");
                            
                            // Finally, we compute the second set of transient probabilities.
                            uniformizedMatrix = this->computeUniformizedMatrix(this->getModel().getTransitionMatrix(), relevantStates, uniformizationRate, exitRates);
                            newSubresult = computeTransientProbabilities(uniformizedMatrix, nullptr, lowerBound, uniformizationRate, newSubresult, *this->linearEquationSolverFactory);
                            
                            // Fill in the correct values.
                            result = std::vector<ValueType>(this->getModel().getNumberOfStates(), storm::utility::zero<ValueType>());
                            storm::utility::vector::setVectorValues(result, ~relevantStates, storm::utility::zero<ValueType>());
                            storm::utility::vector::setVectorValues(result, relevantStates, newSubresult);
                        } else {
                            // In this case, the interval is of the form [t, t] with t != 0, t != inf.
                            
                            std::vector<ValueType> newSubresult = std::vector<ValueType>(statesWithProbabilityGreater0.getNumberOfSetBits());
                            storm::utility::vector::setVectorValues(newSubresult, psiStates % statesWithProbabilityGreater0, storm::utility::one<ValueType>());
                            
                            // Then compute the transient probabilities of being in such a state after t time units. For this,
                            // we must re-uniformize the CTMC, so we need to compute the second uniformized matrix.
                            ValueType uniformizationRate = storm::utility::zero<ValueType>();
                            for (auto const& state : statesWithProbabilityGreater0) {
                                uniformizationRate = std::max(uniformizationRate, exitRates[state]);
                            }
                            uniformizationRate *= 1.02;
                            STORM_LOG_THROW(uniformizationRate > 0, storm::exceptions::InvalidStateException, "The uniformization rate must be positive.");
                            
                            // Finally, we compute the second set of transient probabilities.
                            storm::storage::SparseMatrix<ValueType> uniformizedMatrix = this->computeUniformizedMatrix(this->getModel().getTransitionMatrix(), statesWithProbabilityGreater0, uniformizationRate, exitRates);
                            newSubresult = computeTransientProbabilities(uniformizedMatrix, nullptr, lowerBound, uniformizationRate, newSubresult, *this->linearEquationSolverFactory);
                            
                            // Fill in the correct values.
                            result = std::vector<ValueType>(this->getModel().getNumberOfStates(), storm::utility::zero<ValueType>());
                            storm::utility::vector::setVectorValues(result, ~statesWithProbabilityGreater0, storm::utility::zero<ValueType>());
                            storm::utility::vector::setVectorValues(result, statesWithProbabilityGreater0, newSubresult);
                        }
                    }
                }
            }
            
            return result;
        }
        
        template<class ValueType>
        storm::storage::SparseMatrix<ValueType> SparseCtmcCslModelChecker<ValueType>::computeUniformizedMatrix(storm::storage::SparseMatrix<ValueType> const& transitionMatrix, storm::storage::BitVector const& maybeStates, ValueType uniformizationRate, std::vector<ValueType> const& exitRates) {
            STORM_LOG_DEBUG("Computing uniformized matrix using uniformization rate " << uniformizationRate << ".");
            STORM_LOG_DEBUG("Keeping " << maybeStates.getNumberOfSetBits() << " rows.");
            
            // Create the submatrix that only contains the states with a positive probability (including the
            // psi states) and reserve space for elements on the diagonal.
            storm::storage::SparseMatrix<ValueType> uniformizedMatrix = transitionMatrix.getSubmatrix(false, maybeStates, maybeStates, true);
            
            // Now we need to perform the actual uniformization. That is, all entries need to be divided by
            // the uniformization rate, and the diagonal needs to be set to the negative exit rate of the
            // state plus the self-loop rate and then increased by one.
            uint_fast64_t currentRow = 0;
            for (auto const& state : maybeStates) {
                for (auto& element : uniformizedMatrix.getRow(currentRow)) {
                    if (element.getColumn() == currentRow) {
                        element.setValue((element.getValue() - exitRates[state]) / uniformizationRate + storm::utility::one<ValueType>());
                    } else {
                        element.setValue(element.getValue() / uniformizationRate);
                    }
                }
                ++currentRow;
            }
            
            return uniformizedMatrix;
        }
        
        template<class ValueType>
        template<bool computeCumulativeReward>
        std::vector<ValueType> SparseCtmcCslModelChecker<ValueType>::computeTransientProbabilities(storm::storage::SparseMatrix<ValueType> const& uniformizedMatrix, std::vector<ValueType> const* addVector, ValueType timeBound, ValueType uniformizationRate, std::vector<ValueType> values, storm::utility::solver::LinearEquationSolverFactory<ValueType> const& linearEquationSolverFactory) {
            
            ValueType lambda = timeBound * uniformizationRate;
            
            // If no time can pass, the current values are the result.
            if (lambda == storm::utility::zero<ValueType>()) {
                return values;
            }
            
            // Use Fox-Glynn to get the truncation points and the weights.
            std::tuple<uint_fast64_t, uint_fast64_t, ValueType, std::vector<ValueType>> foxGlynnResult = storm::utility::numerical::getFoxGlynnCutoff(lambda, 1e-300, 1e+300, storm::settings::generalSettings().getPrecision() / 8.0);
            STORM_LOG_DEBUG("Fox-Glynn cutoff points: left=" << std::get<0>(foxGlynnResult) << ", right=" << std::get<1>(foxGlynnResult));
            
            // Scale the weights so they add up to one.
            for (auto& element : std::get<3>(foxGlynnResult)) {
                element /= std::get<2>(foxGlynnResult);
            }
            
            // If the cumulative reward is to be computed, we need to adjust the weights.
            if (computeCumulativeReward) {
                ValueType sum = storm::utility::zero<ValueType>();
                
                for (auto& element : std::get<3>(foxGlynnResult)) {
                    sum += element;
                    element = (1 - sum) / uniformizationRate;
                }
            }
            
            STORM_LOG_DEBUG("Starting iterations with " << uniformizedMatrix.getRowCount() << " x " << uniformizedMatrix.getColumnCount() << " matrix.");
            
            // Initialize result.
            std::vector<ValueType> result;
            uint_fast64_t startingIteration = std::get<0>(foxGlynnResult);
            if (startingIteration == 0) {
                result = values;
                storm::utility::vector::scaleVectorInPlace(result, std::get<3>(foxGlynnResult)[0]);
                std::function<ValueType(ValueType const&, ValueType const&)> addAndScale = [&foxGlynnResult] (ValueType const& a, ValueType const& b) { return a + std::get<3>(foxGlynnResult)[0] * b; };
                if (addVector != nullptr) {
                    storm::utility::vector::applyPointwise(result, *addVector, result, addAndScale);
                }
                ++startingIteration;
            } else {
                if (computeCumulativeReward) {
                    result = std::vector<ValueType>(values.size());
                    std::function<ValueType (ValueType const&)> scaleWithUniformizationRate = [&uniformizationRate] (ValueType const& a) -> ValueType { return a / uniformizationRate; };
                    storm::utility::vector::applyPointwise(values, result, scaleWithUniformizationRate);
                } else {
                    result = std::vector<ValueType>(values.size());
                }
            }
            std::vector<ValueType> multiplicationResult(result.size());
            
            std::unique_ptr<storm::solver::LinearEquationSolver<ValueType>> solver = linearEquationSolverFactory.create(uniformizedMatrix);
            
            if (!computeCumulativeReward && std::get<0>(foxGlynnResult) > 1) {
                // Perform the matrix-vector multiplications (without adding).
                solver->performMatrixVectorMultiplication(values, addVector, std::get<0>(foxGlynnResult) - 1, &multiplicationResult);
            } else if (computeCumulativeReward) {
                std::function<ValueType(ValueType const&, ValueType const&)> addAndScale = [&uniformizationRate] (ValueType const& a, ValueType const& b) { return a + b / uniformizationRate; };
                
                // For the iterations below the left truncation point, we need to add and scale the result with the uniformization rate.
                for (uint_fast64_t index = 1; index < startingIteration; ++index) {
                    solver->performMatrixVectorMultiplication(values, nullptr, 1, &multiplicationResult);
                    storm::utility::vector::applyPointwise(result, values, result, addAndScale);
                }
            }
            
            // For the indices that fall in between the truncation points, we need to perform the matrix-vector
            // multiplication, scale and add the result.
            ValueType weight = 0;
            std::function<ValueType(ValueType const&, ValueType const&)> addAndScale = [&weight] (ValueType const& a, ValueType const& b) { return a + weight * b; };
            for (uint_fast64_t index = startingIteration; index <= std::get<1>(foxGlynnResult); ++index) {
                solver->performMatrixVectorMultiplication(values, addVector, 1, &multiplicationResult);
                
                weight = std::get<3>(foxGlynnResult)[index - std::get<0>(foxGlynnResult)];
                storm::utility::vector::applyPointwise(result, values, result, addAndScale);
            }
            
            return result;
        }
        
        template<class ValueType>
        std::vector<ValueType> SparseCtmcCslModelChecker<ValueType>::computeUntilProbabilitiesHelper(storm::storage::SparseMatrix<ValueType> const& transitionMatrix, storm::storage::SparseMatrix<ValueType> const& backwardTransitions, storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates, bool qualitative, storm::utility::solver::LinearEquationSolverFactory<ValueType> const& linearEquationSolverFactory) {
            return SparseDtmcPrctlModelChecker<ValueType>::computeUntilProbabilitiesHelper(transitionMatrix, backwardTransitions, phiStates, psiStates, qualitative, linearEquationSolverFactory);
        }
        
        template<class ValueType>
        std::unique_ptr<CheckResult> SparseCtmcCslModelChecker<ValueType>::computeInstantaneousRewards(storm::logic::InstantaneousRewardFormula const& rewardPathFormula, bool qualitative, boost::optional<storm::logic::OptimalityType> const& optimalityType) {
            return std::unique_ptr<CheckResult>(new ExplicitQuantitativeCheckResult<ValueType>(this->computeInstantaneousRewardsHelper(rewardPathFormula.getContinuousTimeBound())));
        }
        
        template<class ValueType>
        std::vector<ValueType> SparseCtmcCslModelChecker<ValueType>::computeInstantaneousRewardsHelper(double timeBound) const {
            // Only compute the result if the model has a state-based reward this->getModel().
            STORM_LOG_THROW(this->getModel().hasStateRewards(), storm::exceptions::InvalidPropertyException, "Missing reward model for formula. Skipping formula.");
            
            // Initialize result to state rewards of the this->getModel().
            std::vector<ValueType> result(this->getModel().getStateRewardVector());
            
            // If the time-bound is not zero, we need to perform a transient analysis.
            if (timeBound > 0) {
                ValueType uniformizationRate = 0;
                for (auto const& rate : this->getModel().getExitRateVector()) {
                    uniformizationRate = std::max(uniformizationRate, rate);
                }
                uniformizationRate *= 1.02;
                STORM_LOG_THROW(uniformizationRate > 0, storm::exceptions::InvalidStateException, "The uniformization rate must be positive.");
                
                storm::storage::SparseMatrix<ValueType> uniformizedMatrix = this->computeUniformizedMatrix(this->getModel().getTransitionMatrix(), storm::storage::BitVector(this->getModel().getNumberOfStates(), true), uniformizationRate, this->getModel().getExitRateVector());
                result = this->computeTransientProbabilities(uniformizedMatrix, nullptr, timeBound, uniformizationRate, result, *this->linearEquationSolverFactory);
            }
            
            return result;
        }
        
        template<class ValueType>
        std::unique_ptr<CheckResult> SparseCtmcCslModelChecker<ValueType>::computeCumulativeRewards(storm::logic::CumulativeRewardFormula const& rewardPathFormula, bool qualitative, boost::optional<storm::logic::OptimalityType> const& optimalityType) {
            return std::unique_ptr<CheckResult>(new ExplicitQuantitativeCheckResult<ValueType>(this->computeCumulativeRewardsHelper(rewardPathFormula.getContinuousTimeBound())));
        }
        
        template<class ValueType>
        std::vector<ValueType> SparseCtmcCslModelChecker<ValueType>::computeCumulativeRewardsHelper(double timeBound) const {
            // Only compute the result if the model has a state-based reward this->getModel().
            STORM_LOG_THROW(this->getModel().hasStateRewards() || this->getModel().hasTransitionRewards(), storm::exceptions::InvalidPropertyException, "Missing reward model for formula. Skipping formula.");
            
            // If the time bound is zero, the result is the constant zero vector.
            if (timeBound == 0) {
                return std::vector<ValueType>(this->getModel().getNumberOfStates(), storm::utility::zero<ValueType>());
            }
            
            // Otherwise, we need to perform some computations.
            
            // Start with the uniformization.
            ValueType uniformizationRate = 0;
            for (auto const& rate : this->getModel().getExitRateVector()) {
                uniformizationRate = std::max(uniformizationRate, rate);
            }
            uniformizationRate *= 1.02;
            STORM_LOG_THROW(uniformizationRate > 0, storm::exceptions::InvalidStateException, "The uniformization rate must be positive.");
            
            storm::storage::SparseMatrix<ValueType> uniformizedMatrix = this->computeUniformizedMatrix(this->getModel().getTransitionMatrix(), storm::storage::BitVector(this->getModel().getNumberOfStates(), true), uniformizationRate, this->getModel().getExitRateVector());
            
            // Compute the total state reward vector.
            std::vector<ValueType> totalRewardVector;
            if (this->getModel().hasTransitionRewards()) {
                totalRewardVector = this->getModel().getTransitionMatrix().getPointwiseProductRowSumVector(this->getModel().getTransitionRewardMatrix());
                if (this->getModel().hasStateRewards()) {
                    storm::utility::vector::addVectors(totalRewardVector, this->getModel().getStateRewardVector(), totalRewardVector);
                }
            } else {
                totalRewardVector = std::vector<ValueType>(this->getModel().getStateRewardVector());
            }
            
            // Finally, compute the transient probabilities.
            return this->computeTransientProbabilities<true>(uniformizedMatrix, nullptr, timeBound, uniformizationRate, totalRewardVector, *this->linearEquationSolverFactory);
        }
        
        template<class ValueType>
        storm::storage::SparseMatrix<ValueType> SparseCtmcCslModelChecker<ValueType>::computeProbabilityMatrix(storm::storage::SparseMatrix<ValueType> const& rateMatrix, std::vector<ValueType> const& exitRates) {
            // Turn the rates into probabilities by scaling each row with the exit rate of the state.
            storm::storage::SparseMatrix<ValueType> result(rateMatrix);
            for (uint_fast64_t row = 0; row < result.getRowCount(); ++row) {
                for (auto& entry : result.getRow(row)) {
                    entry.setValue(entry.getValue() / exitRates[row]);
                }
            }
            return result;
        }
        
        template<class ValueType>
        storm::storage::SparseMatrix<ValueType> SparseCtmcCslModelChecker<ValueType>::computeGeneratorMatrix(storm::storage::SparseMatrix<ValueType> const& rateMatrix, std::vector<ValueType> const& exitRates) {
            storm::storage::SparseMatrix<ValueType> generatorMatrix(rateMatrix, true);
            
            // Place the negative exit rate on the diagonal.
            for (uint_fast64_t row = 0; row < generatorMatrix.getRowCount(); ++row) {
                for (auto& entry : generatorMatrix.getRow(row)) {
                    if (entry.getColumn() == row) {
                        entry.setValue(-exitRates[row]);
                    }
                }
            }
            
            return generatorMatrix;
        }
        
        template<class ValueType>
        std::unique_ptr<CheckResult> SparseCtmcCslModelChecker<ValueType>::computeReachabilityRewards(storm::logic::ReachabilityRewardFormula const& rewardPathFormula, bool qualitative, boost::optional<storm::logic::OptimalityType> const& optimalityType) {
            std::unique_ptr<CheckResult> subResultPointer = this->check(rewardPathFormula.getSubformula());
            ExplicitQualitativeCheckResult const& subResult = subResultPointer->asExplicitQualitativeCheckResult();
            storm::storage::SparseMatrix<ValueType> probabilityMatrix = computeProbabilityMatrix(this->getModel().getTransitionMatrix(), this->getModel().getExitRateVector());
            
            boost::optional<std::vector<ValueType>> modifiedStateRewardVector;
            if (this->getModel().hasStateRewards()) {
                modifiedStateRewardVector = std::vector<ValueType>(this->getModel().getStateRewardVector());
                
                typename std::vector<ValueType>::const_iterator it2 = this->getModel().getExitRateVector().begin();
                for (typename std::vector<ValueType>::iterator it1 = modifiedStateRewardVector.get().begin(), ite1 = modifiedStateRewardVector.get().end(); it1 != ite1; ++it1, ++it2) {
                    *it1 /= *it2;
                }
            }
            
            return std::unique_ptr<CheckResult>(new ExplicitQuantitativeCheckResult<ValueType>(SparseDtmcPrctlModelChecker<ValueType>::computeReachabilityRewardsHelper(probabilityMatrix, modifiedStateRewardVector, this->getModel().getOptionalTransitionRewardMatrix(), this->getModel().getBackwardTransitions(), subResult.getTruthValuesVector(), *linearEquationSolverFactory, qualitative)));
        }
        
        template<class ValueType>
        std::unique_ptr<CheckResult> SparseCtmcCslModelChecker<ValueType>::computeLongRunAverage(storm::logic::StateFormula const& stateFormula, bool qualitative, boost::optional<storm::logic::OptimalityType> const& optimalityType) {
            std::unique_ptr<CheckResult> subResultPointer = this->check(stateFormula);
            ExplicitQualitativeCheckResult const& subResult = subResultPointer->asExplicitQualitativeCheckResult();
            
            storm::storage::SparseMatrix<ValueType> probabilityMatrix = this->computeProbabilityMatrix(this->getModel().getTransitionMatrix(), this->getModel().getExitRateVector());
            return std::unique_ptr<CheckResult>(new ExplicitQuantitativeCheckResult<ValueType>(computeLongRunAverageHelper(probabilityMatrix, subResult.getTruthValuesVector(), &this->getModel().getExitRateVector(), qualitative, *linearEquationSolverFactory)));
        }
        
        template<typename ValueType>
        std::vector<ValueType> SparseCtmcCslModelChecker<ValueType>::computeLongRunAverageHelper(storm::storage::SparseMatrix<ValueType> const& transitionMatrix, storm::storage::BitVector const& psiStates, std::vector<ValueType> const* exitRateVector, bool qualitative, storm::utility::solver::LinearEquationSolverFactory<ValueType> const& linearEquationSolverFactory) {
            // If there are no goal states, we avoid the computation and directly return zero.
            uint_fast64_t numOfStates = transitionMatrix.getRowCount();
            if (psiStates.empty()) {
                return std::vector<ValueType>(numOfStates, storm::utility::zero<ValueType>());
            }
            
            // Likewise, if all bits are set, we can avoid the computation.
            if (psiStates.full()) {
                return std::vector<ValueType>(numOfStates, storm::utility::one<ValueType>());
            }
            
            // Start by decomposing the DTMC into its BSCCs.
            storm::storage::StronglyConnectedComponentDecomposition<double> bsccDecomposition(transitionMatrix, storm::storage::BitVector(transitionMatrix.getRowCount(), true), false, true);
            
            STORM_LOG_DEBUG("Found " << bsccDecomposition.size() << " BSCCs.");
            
            // Get some data members for convenience.
            ValueType one = storm::utility::one<ValueType>();
            ValueType zero = storm::utility::zero<ValueType>();
            
            // Prepare the vector holding the LRA values for each of the BSCCs.
            std::vector<ValueType> bsccLra(bsccDecomposition.size(), zero);
            
            // First we check which states are in BSCCs.
            storm::storage::BitVector statesInBsccs(numOfStates);
            storm::storage::BitVector firstStatesInBsccs(numOfStates);
            
            for (uint_fast64_t currentBsccIndex = 0; currentBsccIndex < bsccDecomposition.size(); ++currentBsccIndex) {
                storm::storage::StronglyConnectedComponent const& bscc = bsccDecomposition[currentBsccIndex];
                
                // Gather information for later use.
                bool first = true;
                for (auto const& state : bscc) {
                    statesInBsccs.set(state);
                    if (first) {
                        firstStatesInBsccs.set(state);
                    }
                    first = false;
                }
            }
            storm::storage::BitVector statesNotInBsccs = ~statesInBsccs;
            
            STORM_LOG_DEBUG("Found " << statesInBsccs.getNumberOfSetBits() << " states in BSCCs.");
            
            // Prepare a vector holding the index within all states that are in BSCCs for every state.
            std::vector<uint_fast64_t> indexInStatesInBsccs;
            
            // Prepare a vector that maps the index within the set of all states in BSCCs to the index of the containing BSCC.
            std::vector<uint_fast64_t> stateToBsccIndexMap;
            
            if (!statesInBsccs.empty()) {
                firstStatesInBsccs = firstStatesInBsccs % statesInBsccs;
                
                // Then we construct an equation system that yields the steady state probabilities for all states in BSCCs.
                storm::storage::SparseMatrix<ValueType> bsccEquationSystem = transitionMatrix.getSubmatrix(false, statesInBsccs, statesInBsccs, true);
                
                // Since in the fix point equation, we need to multiply the vector from the left, we convert this to a
                // multiplication from the right by transposing the system.
                bsccEquationSystem = bsccEquationSystem.transpose(false, true);
                
                // Create an auxiliary structure that makes it easy to look up the indices within the set of BSCC states.
                uint_fast64_t lastIndex = 0;
                uint_fast64_t currentNumberOfSetBits = 0;
                indexInStatesInBsccs.reserve(transitionMatrix.getRowCount());
                for (auto index : statesInBsccs) {
                    while (lastIndex <= index) {
                        indexInStatesInBsccs.push_back(currentNumberOfSetBits);
                        ++lastIndex;
                    }
                    ++currentNumberOfSetBits;
                }
                
                stateToBsccIndexMap.resize(statesInBsccs.getNumberOfSetBits());
                for (uint_fast64_t currentBsccIndex = 0; currentBsccIndex < bsccDecomposition.size(); ++currentBsccIndex) {
                    storm::storage::StronglyConnectedComponent const& bscc = bsccDecomposition[currentBsccIndex];
                    for (auto const& state : bscc) {
                        stateToBsccIndexMap[indexInStatesInBsccs[state]] = currentBsccIndex;
                    }
                }
                
                // Now build the final equation system matrix, the initial guess and the right-hand side in one go.
                std::vector<ValueType> bsccEquationSystemRightSide(bsccEquationSystem.getColumnCount(), zero);
                storm::storage::SparseMatrixBuilder<ValueType> builder;
                for (uint_fast64_t row = 0; row < bsccEquationSystem.getRowCount(); ++row) {
                    
                    // If the current row is the first one belonging to a BSCC, we substitute it by the constraint that the
                    // values for states of this BSCC must sum to one. However, in order to have a non-zero value on the
                    // diagonal, we add the constraint of the BSCC that produces a 1 on the diagonal.
                    if (firstStatesInBsccs.get(row)) {
                        uint_fast64_t requiredBscc = stateToBsccIndexMap[row];
                        storm::storage::StronglyConnectedComponent const& bscc = bsccDecomposition[requiredBscc];
                        
                        for (auto const& state : bscc) {
                            builder.addNextValue(row, indexInStatesInBsccs[state], one);
                        }
                        
                        bsccEquationSystemRightSide[row] = one;
                        
                    } else {
                        // Otherwise, we copy the row, and subtract 1 from the diagonal.
                        for (auto& entry : bsccEquationSystem.getRow(row)) {
                            if (entry.getColumn() == row) {
                                builder.addNextValue(row, entry.getColumn(), entry.getValue() - one);
                            } else {
                                builder.addNextValue(row, entry.getColumn(), entry.getValue());
                            }
                        }
                    }
                    
                }
                
                // Create the initial guess for the LRAs. We take a uniform distribution over all states in a BSCC.
                std::vector<ValueType> bsccEquationSystemSolution(bsccEquationSystem.getColumnCount(), zero);
                for (uint_fast64_t bsccIndex = 0; bsccIndex < bsccDecomposition.size(); ++bsccIndex) {
                    storm::storage::StronglyConnectedComponent const& bscc = bsccDecomposition[bsccIndex];
                    
                    for (auto const& state : bscc) {
                        bsccEquationSystemSolution[indexInStatesInBsccs[state]] = one /  bscc.size();
                    }
                }
                
                bsccEquationSystem = builder.build();
                
                {
                    std::unique_ptr<storm::solver::LinearEquationSolver<ValueType>> solver = linearEquationSolverFactory.create(bsccEquationSystem);
                    solver->solveEquationSystem(bsccEquationSystemSolution, bsccEquationSystemRightSide);
                }
                
                // If exit rates were given, we need to 'fix' the results to also account for the timing behaviour.
                if (exitRateVector != nullptr) {
                    std::vector<ValueType> bsccTotalValue(bsccDecomposition.size(), zero);
                    for (auto stateIter = statesInBsccs.begin(); stateIter != statesInBsccs.end(); ++stateIter) {
                        bsccTotalValue[stateToBsccIndexMap[indexInStatesInBsccs[*stateIter]]] += bsccEquationSystemSolution[indexInStatesInBsccs[*stateIter]] * (one / (*exitRateVector)[*stateIter]);
                    }
                    
                    for (auto stateIter = statesInBsccs.begin(); stateIter != statesInBsccs.end(); ++stateIter) {
                        bsccEquationSystemSolution[indexInStatesInBsccs[*stateIter]] = (bsccEquationSystemSolution[indexInStatesInBsccs[*stateIter]] * (one / (*exitRateVector)[*stateIter])) / bsccTotalValue[stateToBsccIndexMap[indexInStatesInBsccs[*stateIter]]];
                    }
                }
                // Calculate LRA Value for each BSCC from steady state distribution in BSCCs.
                for (uint_fast64_t bsccIndex = 0; bsccIndex < bsccDecomposition.size(); ++bsccIndex) {
                    storm::storage::StronglyConnectedComponent const& bscc = bsccDecomposition[bsccIndex];
                    
                    for (auto const& state : bscc) {
                        if (psiStates.get(state)) {
                            bsccLra[stateToBsccIndexMap[indexInStatesInBsccs[state]]] += bsccEquationSystemSolution[indexInStatesInBsccs[state]];
                        }
                    }
                }
                
                for (uint_fast64_t bsccIndex = 0; bsccIndex < bsccDecomposition.size(); ++bsccIndex) {
                    STORM_LOG_DEBUG("Found LRA " << bsccLra[bsccIndex] << " for BSCC " << bsccIndex << ".");
                }
            } else {
                for (uint_fast64_t bsccIndex = 0; bsccIndex < bsccDecomposition.size(); ++bsccIndex) {
                    storm::storage::StronglyConnectedComponent const& bscc = bsccDecomposition[bsccIndex];
                    
                    // At this point, all BSCCs are known to contain exactly one state, which is why we can set all values
                    // directly (based on whether or not the contained state is a psi state).
                    if (psiStates.get(*bscc.begin())) {
                        bsccLra[bsccIndex] = 1;
                    }
                }
                
                for (uint_fast64_t bsccIndex = 0; bsccIndex < bsccDecomposition.size(); ++bsccIndex) {
                    STORM_LOG_DEBUG("Found LRA " << bsccLra[bsccIndex] << " for BSCC " << bsccIndex << ".");
                }
            }
            
            std::vector<ValueType> rewardSolution;
            if (!statesNotInBsccs.empty()) {
                // Calculate LRA for states not in bsccs as expected reachability rewards.
                // Target states are states in bsccs, transition reward is the lra of the bscc for each transition into a
                // bscc and 0 otherwise. This corresponds to the sum of LRAs in BSCC weighted by the reachability probability
                // of the BSCC.
                
                std::vector<ValueType> rewardRightSide;
                rewardRightSide.reserve(statesNotInBsccs.getNumberOfSetBits());
                
                for (auto state : statesNotInBsccs) {
                    ValueType reward = zero;
                    for (auto entry : transitionMatrix.getRow(state)) {
                        if (statesInBsccs.get(entry.getColumn())) {
                            reward += entry.getValue() * bsccLra[stateToBsccIndexMap[indexInStatesInBsccs[entry.getColumn()]]];
                        }
                    }
                    rewardRightSide.push_back(reward);
                }
                
                storm::storage::SparseMatrix<ValueType> rewardEquationSystemMatrix = transitionMatrix.getSubmatrix(false, statesNotInBsccs, statesNotInBsccs, true);
                rewardEquationSystemMatrix.convertToEquationSystem();
                
                rewardSolution = std::vector<ValueType>(rewardEquationSystemMatrix.getColumnCount(), one);
                
                {
                    std::unique_ptr<storm::solver::LinearEquationSolver<ValueType>> solver = linearEquationSolverFactory.create(rewardEquationSystemMatrix);
                    solver->solveEquationSystem(rewardSolution, rewardRightSide);
                }
            }
            
            // Fill the result vector.
            std::vector<ValueType> result(numOfStates);
            auto rewardSolutionIter = rewardSolution.begin();
            
            for (uint_fast64_t bsccIndex = 0; bsccIndex < bsccDecomposition.size(); ++bsccIndex) {
                storm::storage::StronglyConnectedComponent const& bscc = bsccDecomposition[bsccIndex];
                
                for (auto const& state : bscc) {
                    result[state] = bsccLra[bsccIndex];
                }
            }
            for (auto state : statesNotInBsccs) {
                STORM_LOG_ASSERT(rewardSolutionIter != rewardSolution.end(), "Too few elements in solution.");
                // Take the value from the reward computation. Since the n-th state not in any bscc is the n-th
                // entry in rewardSolution we can just take the next value from the iterator.
                result[state] = *rewardSolutionIter;
                ++rewardSolutionIter;
            }
            
            return result;
        }
        
        
        // Explicitly instantiate the model checker.
        template class SparseCtmcCslModelChecker<double>;
        
    } // namespace modelchecker
} // namespace storm