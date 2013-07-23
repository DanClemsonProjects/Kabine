// @HEADER
//
// ***********************************************************************
//
//        MueLu: A package for multigrid based preconditioning
//                  Copyright 2012 Sandia Corporation
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact
//                    Jeremie Gaidamour (jngaida@sandia.gov)
//                    Jonathan Hu       (jhu@sandia.gov)
//                    Ray Tuminaro      (rstumin@sandia.gov)
//
// ***********************************************************************
//
// @HEADER
#ifndef MUELU_COUPLEDAGGREGATIONFACTORY_DECL_HPP
#define MUELU_COUPLEDAGGREGATIONFACTORY_DECL_HPP

#include "MueLu_ConfigDefs.hpp"
#include "MueLu_SingleLevelFactoryBase.hpp"
#include "MueLu_CoupledAggregationFactory_fwd.hpp"

#include "MueLu_LocalAggregationAlgorithm.hpp"
#include "MueLu_LeftoverAggregationAlgorithm.hpp"
#include "MueLu_Level_fwd.hpp"
#include "MueLu_AmalgamationInfo_fwd.hpp"
#include "MueLu_Exceptions.hpp"

namespace MueLu {

  /*!
    @class CoupledAggregationFactory class.
    @brief Factory for coarsening a graph with uncoupled aggregation.

    This method has two phases.  The first is a local clustering algorithm.  The second creates aggregates
    that can include unknowns from more than one process.
  */

  /* Factory input:
     - a graph ("Graph") generated by GraphFact_

     Factory output:
     - aggregates ("Aggegates")

     Factory options:
     -
     -
     -
  */

  template <class LocalOrdinal = int, class GlobalOrdinal = LocalOrdinal, class Node = KokkosClassic::DefaultNode::DefaultNodeType, class LocalMatOps = typename KokkosClassic::DefaultKernels<void,LocalOrdinal,Node>::SparseOps> //TODO: or BlockSparseOp ?
  class CoupledAggregationFactory : public SingleLevelFactoryBase {
#undef MUELU_COUPLEDAGGREGATIONFACTORY_SHORT
#include "MueLu_UseShortNamesOrdinal.hpp"

  public:
    //! @name Constructors/Destructors.
    //@{

    //! Constructor.
    CoupledAggregationFactory();

    //! Destructor.
    virtual ~CoupledAggregationFactory() { }

    //@}

    //! @name Set/get methods.
    //@{

    // Options algo1
    void SetOrdering(Ordering ordering) { algo1_.SetOrdering(ordering); }
    void SetMaxNeighAlreadySelected(int maxNeighAlreadySelected) { algo1_.SetMaxNeighAlreadySelected(maxNeighAlreadySelected); }
    Ordering GetOrdering() const { return algo1_.GetOrdering(); }
    int GetMaxNeighAlreadySelected() const { return algo1_.GetMaxNeighAlreadySelected(); }

    // Options algo2
    void SetPhase3AggCreation(double phase3AggCreation) { algo2_.SetPhase3AggCreation(phase3AggCreation); }
    double GetPhase3AggCreation() const { return algo2_.GetPhase3AggCreation(); }

    // Options shared algo1 and algo2
    void SetMinNodesPerAggregate(int minNodesPerAggregate) { algo1_.SetMinNodesPerAggregate(minNodesPerAggregate); algo2_.SetMinNodesPerAggregate(minNodesPerAggregate); }
    int GetMinNodesPerAggregate() const { return algo1_.GetMinNodesPerAggregate(); TEUCHOS_TEST_FOR_EXCEPTION(algo2_.GetMinNodesPerAggregate() != algo1_.GetMinNodesPerAggregate(), Exceptions::RuntimeError, ""); }

    //@}

    //! Input
    //@{

    void DeclareInput(Level &currentLevel) const;

    //@}

    //! @name Build methods.
    //@{

    /*! @brief Build aggregates. */
    void Build(Level &currentLevel) const;

    //@}

  private:

    //! Algorithms
    LocalAggregationAlgorithm algo1_;
    LeftoverAggregationAlgorithm algo2_;

  }; // class CoupledAggregationFactory

} //namespace MueLu

//TODO: can be more generic:
// - allow to choose algo
// - base class for algorithm and options forward to algorithm as parameter list

#define MUELU_COUPLEDAGGREGATIONFACTORY_SHORT
#endif // MUELU_COUPLEDAGGREGATIONFACTORY_DECL_HPP
