// @HEADER
//
// ***********************************************************************
//
//   Zoltan2: A package of combinatorial algorithms for scientific computing
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
// Questions? Contact Karen Devine      (kddevin@sandia.gov)
//                    Erik Boman        (egboman@sandia.gov)
//                    Siva Rajamanickam (srajama@sandia.gov)
//
// ***********************************************************************
//
// @HEADER

/*! \file Zoltan2_AlgPQJagged.hpp
  \brief Contains the PQ-jagged algorthm.
 */

#ifndef _ZOLTAN2_ALGPQJagged_HPP_
#define _ZOLTAN2_ALGPQJagged_HPP_

#include <Zoltan2_AlgRCB_methods.hpp>
#include <Zoltan2_CoordinateModel.hpp>
#include <Zoltan2_Metric.hpp>             // won't need thiss
#include <Tpetra_Distributor.hpp>
#include <Teuchos_ParameterList.hpp>
#include <new>          // ::operator new[]

#include "zoltan_comm_cpp.h"

//#define omitted2
//#define memory_debug
//#define enable_migration
#include <bitset>

#define EPS_SCALE 1
#define LEAST_SIGNIFICANCE 0.0001
#define SIGNIFICANCE_MUL 1000
//#define INCLUDE_ZOLTAN2_EXPERIMENTAL
#ifdef HAVE_ZOLTAN2_OMP
#include <omp.h>
#endif
//#define FIRST_TOUCH
//#define BINARYCUTSEARCH
//#define Zoltan_Comm
//#define enable_migration
#define ABS(x) ((x) >= 0 ? (x) : -(x))

#define LEAF_IMBALANCE_FACTOR 0.1
#define BINARYCUTOFF 32
//imbalance calculation. Wreal / Wexpected - 1
#define imbalanceOf(Wachieved, totalW, expectedRatio) \
        (Wachieved) / ((totalW) * (expectedRatio)) - 1
//#define mpi_communication

#define KCUTOFF 0.80
#define forceMigration 1500000
#define Z2_DEFAULT_CON_PART_COUNT 16

using std::vector;

namespace Teuchos{
template <typename Ordinal, typename T>
class PQJaggedCombinedReductionOp  : public ValueTypeReductionOp<Ordinal,T>
{
private:
    Ordinal numSum_0, numMin_1, numMin_2;
    std::vector <Ordinal> *partVector;
    Ordinal vectorBegin;
    Ordinal k;
    int reductionType;

public:
    /*! \brief Default Constructor
     */
    PQJaggedCombinedReductionOp ():numSum_0(0), numMin_1(0),
            numMin_2(0), k(0), partVector(NULL), vectorBegin(0), reductionType(0){}

    /*! \brief Constructor
     *   \param nsum  the count of how many sums will be computed at the
     *             start of the list.
     *   \param nmin  following the sums, this many minimums will be computed.
     *   \param nmax  following the minimums, this many maximums will be computed.
     */
    PQJaggedCombinedReductionOp (Ordinal nsum, Ordinal nmin1, Ordinal nmin2, Ordinal k_):
        numSum_0(nsum), numMin_1(nmin1), numMin_2(nmin2),
        partVector(NULL),vectorBegin(0),
        k(k_),
        reductionType(0){}


    PQJaggedCombinedReductionOp (std::vector <Ordinal> *pVector, Ordinal vBegin, Ordinal k_):

//  PQJaggedCombinedReductionOp (vector <Ordinal> *pVector, Ordinal vBegin, Ordinal k_):

        numSum_0(0), numMin_1(0), numMin_2(0),
        partVector(pVector), vectorBegin(vBegin),
        k(k_),
        reductionType(1){}


    /*! \brief Implement Teuchos::ValueTypeReductionOp interface
     */
    void reduce( const Ordinal count, const T inBuffer[], T inoutBuffer[]) const
    {
        if (reductionType == 0){
            Ordinal next=0;
            for(Ordinal ii = 0; ii < k ; ++ii){
                for (Ordinal i=0; i < numSum_0; i++, next++)
                    inoutBuffer[next] += inBuffer[next];

                for (Ordinal i=0; i < numMin_1; i++, next++)
                    if (inoutBuffer[next] > inBuffer[next])
                        inoutBuffer[next] = inBuffer[next];

                for (Ordinal i=0; i < numMin_2; i++, next++)
                    if (inoutBuffer[next] > inBuffer[next])
                        inoutBuffer[next] = inBuffer[next];
            }
        }
        else {
            Ordinal next=0;
            for(Ordinal ii = 0; ii < k ; ++ii){
                Ordinal partPartition = (*partVector)[ii + vectorBegin];
                Ordinal tnumSum_ = 2 * partPartition - 1;
                Ordinal tnumMin_1 = partPartition - 1;
                Ordinal tnumMin_2 = tnumMin_1 ;
                for (Ordinal i=0; i < tnumSum_; i++, next++)
                    inoutBuffer[next] += inBuffer[next];

                for (Ordinal i=0; i < tnumMin_1; i++, next++)
                    if (inoutBuffer[next] > inBuffer[next])
                        inoutBuffer[next] = inBuffer[next];

                for (Ordinal i=0; i < tnumMin_2; i++, next++)
                    if (inoutBuffer[next] > inBuffer[next])
                        inoutBuffer[next] = inBuffer[next];
            }
        }
    }
};








template <typename Ordinal, typename T>
class PQJaggedCombinedMinMaxTotalReductionOp  : public ValueTypeReductionOp<Ordinal,T>
{
private:
    Ordinal numMin, numMax, numTotal;

public:
    /*! \brief Default Constructor
     */
    PQJaggedCombinedMinMaxTotalReductionOp ():numMin(0), numMax(0), numTotal(0){}

    /*! \brief Constructor
     *   \param nsum  the count of how many sums will be computed at the
     *             start of the list.
     *   \param nmin  following the sums, this many minimums will be computed.
     *   \param nmax  following the minimums, this many maximums will be computed.
     */
    PQJaggedCombinedMinMaxTotalReductionOp (Ordinal nmin, Ordinal nmax, Ordinal nTotal):
        numMin(nmin), numMax(nmax), numTotal(nTotal){}

    /*! \brief Implement Teuchos::ValueTypeReductionOp interface
     */
    void reduce( const Ordinal count, const T inBuffer[], T inoutBuffer[]) const
    {
        Ordinal next=0;

        for (Ordinal i=0; i < numMin; i++, next++)
            if (inoutBuffer[next] > inBuffer[next])
                inoutBuffer[next] = inBuffer[next];

        for (Ordinal i=0; i < numMax; i++, next++)
            if (inoutBuffer[next] < inBuffer[next])
                inoutBuffer[next] = inBuffer[next];


        for (Ordinal i=0; i < numTotal; i++, next++)
            inoutBuffer[next] += inBuffer[next];
    }
};
} // namespace Teuchos

namespace Zoltan2{

//diffclock for temporary timing experiments.

#ifdef mpi_communication
partId_t concurrent = 0;
void sumMinMin(void *in, void *inout, int *count, MPI_Datatype *type) {

    int k = *count;
    int numCut = (k  - 1) / 4;
    int total_part_count = numCut * 2 + 1;
    int next = 0;

    float *inoutBuffer = (float *) inout;
    float *inBuffer = (float *) in;


    for(partId_t ii = 0; ii < concurrent ; ++ii){
        for (long i=0; i < total_part_count; i++, next++)
            inoutBuffer[next] += inBuffer[next];

        for (long i=0; i < numCut; i++, next++)
            if (inoutBuffer[next] > inBuffer[next])
                inoutBuffer[next] = inBuffer[next];

        for (long i=0; i < numCut; i++, next++)
            if (inoutBuffer[next] > inBuffer[next])
                inoutBuffer[next] = inBuffer[next];
    }
}


void minMaxSum(void *in, void *inout, int *count, MPI_Datatype *type) {
    //    long k = (*((int *) count));
    int k = *count;
    int num = k / 3;
    int next = 0;
    float *inoutBuffer = (float *) inout;
    float *inBuffer = (float *) in;


    for (long i=0; i < num; i++, next++)
        if (inoutBuffer[next] > inBuffer[next])
            inoutBuffer[next] = inBuffer[next];
    for (long i=0; i < num; i++, next++)
        if (inoutBuffer[next] < inBuffer[next])
            inoutBuffer[next] = inBuffer[next];
    for (long i=0; i < num; i++, next++)
        inoutBuffer[next] += inBuffer[next];

}
#endif

/*! \brief A helper class containing array representation of
 *  coordinate linked lists.
 */

template <typename lno_t, typename partId_t>
class pqJagged_PartVertices{
private:
    lno_t *linkedList; //initially filled with -1's.
    lno_t *partBegins; //initially filled with -1's.
    lno_t *partEnds; //initially filled with -1's.
public:

    //default constructor
    pqJagged_PartVertices(){};

    /*! \brief  The memory is provided to class via set function.
     *  \param linkedList_ is the array with size as the number of coordinates. Assumes all array is filled -1's.
     *  Each element of array points to the next element of the array in the linked list.
     *  \param partBegins_ is the array with size as the number of parts to be divided in current coordinate dimension. Assumes that array is filled with -1's.
     *  Holds the beginning of each part.
     *  \param partEnds_ is the array with size as the number of parts to be divided in current coordinate dimension. Assumes that array is filled with -1's.
     *  Holds the end coordinate of each part.
     */

    void set(lno_t *linkedList_, lno_t *partBegins_, lno_t *partEnds_){
        linkedList = linkedList_;
        partBegins = partBegins_;
        partEnds = partEnds_;
    }

    //user is responsible from providing the correct number of part counts
    /*! \brief Inserting a coordinate to a particular part.
     * Since, class does not have the size information,
     * it is user's responsibility to provide indices for partNo and coordinateIndex that are in the range.
     * \param partNo is the part number that the coordinate is inserted.
     * \param coordinateIndex is index of coordinate to be inserted.
     */
    void inserToPart (partId_t partNo, lno_t coordinateIndex){

        switch (partEnds[partNo]){
        case -1: // this means partBegins[partNo] is also -1.
            partBegins[partNo] = coordinateIndex;
            partEnds[partNo] = coordinateIndex;
            break;
        default:
            linkedList[coordinateIndex] = partBegins[partNo];
            partBegins[partNo] = coordinateIndex;
            break;
        }


    }

    /*! \brief
     * linkedList getter function.
     */
    lno_t *getLinkedList(){ return linkedList;}

    /*! \brief
     * partBegins getter function.
     */
    lno_t *getPartBegins(){ return partBegins;}
    /*! \brief
     * partEnds getter function.
     */
    lno_t *getPartEnds(){ return partEnds;}

};

template<typename T>
inline void firstTouch(T *arrayName, size_t arraySize){
#ifdef HAVE_ZOLTAN2_OMP
#pragma omp parallel for
#endif
    for(size_t jj = 0; jj < arraySize; ++jj){
        arrayName[jj] = 0;
    }
}

/*! \brief
 * Function that calculates the next pivot position,
 * according to given coordinates of upper bound and lower bound, the weights at upper and lower bounds, and the expected weight.
 * \param cutUpperBounds is the pointer to the array holding the upper bounds coordinates of the cuts.
 * \param cutLowerBounds is the pointer to the array holding the lower bound coordinates of the cuts.
 * \param currentCutIndex is the index of the current cut, the indices used for lower and upper bound arrays.
 * \param cutUpperWeight is the pointer to the array holding the weights at the upper bounds of each cut.
 * \param cutLowerWeight is the pointer to the array holding the weights at the lower bounds of each cut.
 * \param ew is the expected weight that should be placed on the left of the cut line.
 */
template <typename scalar_t>
inline scalar_t pivotPos (scalar_t * cutUpperBounds, scalar_t *cutLowerBounds,size_t currentCutIndex, scalar_t *cutUpperWeight, scalar_t *cutLowerWeight, scalar_t ew){

    if(cutUpperWeight[currentCutIndex] == cutLowerWeight[currentCutIndex]){
        return cutLowerBounds[currentCutIndex];
    }
    return ((cutUpperBounds[currentCutIndex] - cutLowerBounds[currentCutIndex]) /
            (cutUpperWeight[currentCutIndex] - cutLowerWeight[currentCutIndex]))  * (ew - cutLowerWeight[currentCutIndex]) + cutLowerBounds[currentCutIndex];
}


/*! \brief  Returns the parameters such as:
 * Partitioning problem parameters of interest:
 *	Partitioning objective
 *	imbalance_tolerance
 *
 *Geometric partitioning problem parameters of interest:
 *	average_cuts
 *	rectilinear_blocks
 *	bisection_num_test_cuts (experimental)
 *	\param pl is the ParameterList object read from the environment.
 *	\param float-like value representing imbalance tolerance. An output of the function.
 *	(for imbalance 0.03, the user currently inputs 1.03. However, This function will return 0.03 for that case.)
 *	\param mcnorm multiCriteriaNorm. An output of the function. //not used in pqJagged algorithm currently.
 *	\param  params holding the bits for objective problem description. An output of the function. //not used in pqJagged algorithm currently..
 *	\param numTestCuts. An output of the function. //not used in pqJagged algorithm currently.
 *	\param ignoreWeights is the boolean value to treat the each coordinate as uniform regardless of the given input weights. Output of the function.
 * \param concurrentPartCount is the number of parts whose cut lines will be calculated concurrently.
 */
template <typename T>
void pqJagged_getParameters(const Teuchos::ParameterList &pl,
        T &imbalanceTolerance,
        multiCriteriaNorm &mcnorm, 
        std::bitset<NUM_RCB_PARAMS> &params,  
        int &numTestCuts, 
        bool &ignoreWeights, 
        bool &allowNonrectilinear, 
        partId_t &concurrentPartCount,
        //bool &force_binary,
        //bool &force_linear,
        int &migration_check_option,
        int &migration_option,
        T &migration_imbalance_cut_off, 
        int &assignment_type){

    string obj;

    const Teuchos::ParameterEntry *pe = pl.getEntryPtr("partitioning_objective");
    if (pe)
        obj = pe->getValue(&obj);

    if (!pe){
        params.set(rcb_balanceWeight);
        mcnorm = normBalanceTotalMaximum;
    }
    else if (obj == string("balance_object_count")){
        params.set(rcb_balanceCount);
    }
    else if (obj == string("multicriteria_minimize_total_weight")){
        params.set(rcb_minTotalWeight);
        mcnorm = normMinimizeTotalWeight;
    }
    else if (obj == string("multicriteria_minimize_maximum_weight")){
        params.set(rcb_minMaximumWeight);
        mcnorm = normMinimizeMaximumWeight;
    }
    else if (obj == string("multicriteria_balance_total_maximum")){
        params.set(rcb_balanceTotalMaximum);
        mcnorm = normBalanceTotalMaximum;
    }
    else{
        params.set(rcb_balanceWeight);
        mcnorm = normBalanceTotalMaximum;
    }

    imbalanceTolerance = .1;
    pe = pl.getEntryPtr("imbalance_tolerance");
    if (pe){
        double tol;
        tol = pe->getValue(&tol);
        imbalanceTolerance = tol - 1.0;
    }

    migration_imbalance_cut_off = .1;
    pe = pl.getEntryPtr("migration_imbalance_cut_off");
    if (pe){
        double tol;
        tol = pe->getValue(&tol);
        migration_imbalance_cut_off = tol - 1.0;
    }

    pe = pl.getEntryPtr("migration_all_to_all_type");
    if (pe){
        migration_option = pe->getValue(&concurrentPartCount);
    }else {
        migration_option = 1;
    }

    pe = pl.getEntryPtr("migration_check_option");
    if (pe){
        migration_check_option = pe->getValue(&concurrentPartCount);
    }else {
        migration_check_option = 0;
    }

    pe = pl.getEntryPtr("migration_processor_assignment_type");
    if (pe){
        assignment_type = pe->getValue(&concurrentPartCount);
    }else {
        assignment_type = 1;
    }




    if (imbalanceTolerance <= 0)
        imbalanceTolerance = 10e-4;


    pe = pl.getEntryPtr("parallel_part_calculation_count");
    if (pe){
        concurrentPartCount = pe->getValue(&concurrentPartCount);
    }else {
        concurrentPartCount = 0; // Set to invalid value
    }
    int val = 0;
    pe = pl.getEntryPtr("average_cuts");
    if (pe)
        val = pe->getValue(&val);

    if (val == 1)
        params.set(rcb_averageCuts);

    val = 0;
    pe = pl.getEntryPtr("rectilinear_blocks");
    if (pe)
        val = pe->getValue(&val);

    if (val == 1){
        params.set(rcb_rectilinearBlocks);
        allowNonrectilinear = false;
    } else {
        allowNonrectilinear = true;
    }

    numTestCuts = 1;
    pe = pl.getEntryPtr("bisection_num_test_cuts");
    if (pe)
        numTestCuts = pe->getValue(&numTestCuts);

    ignoreWeights = params.test(rcb_balanceCount);
}


/*! \brief  Returns the input coordinate value parameters.
 * \param coords is the coordinate model representing the input.
 * \param coordDim is the output of the function representing the dimension count of the input.
 * \param weightDim is the output of the function representing the dimension of the weights.
 * \param numLocalCoords is the output representing the count of the local coordinates.
 * \param numGlobalCoords is the output representing the count of the global coordinates.
 * \param criteriaDim is the output representing the multi-objective count.
 * \param ignoreWeights is the boolean input of the function, to treat the each coordinate
 * as uniform regardless of the given input weights. Output of the function.
 *
 */
template <typename Adapter>
void pqJagged_getCoordinateValues( const RCP<const CoordinateModel<
        typename Adapter::base_adapter_t> > &coords, int &coordDim,
        int &weightDim, size_t &numLocalCoords, global_size_t &numGlobalCoords, int &criteriaDim, const bool &ignoreWeights){

    coordDim = coords->getCoordinateDim();
    weightDim = coords->getCoordinateWeightDim();
    numLocalCoords = coords->getLocalNumCoordinates();
    numGlobalCoords = coords->getGlobalNumCoordinates();
    criteriaDim = (weightDim ? weightDim : 1);
    if (criteriaDim > 1 && ignoreWeights)
        criteriaDim = 1;
}


/*! \brief  Function returning the input values for the problem such as the coordinates,
 * weights and desiredPartSizes.
 * \param env environment.
 * \param coords is the coordinate model representing the input.
 * \param solution is the partitioning solution object.
 * \param params is the  bitset to represent multiple parameters. //not used currently by pqJagged.
 * \param coordDim is an integer value to represent the count of coordinate dimensions.
 * \param weightDim is an integer value to represent the count of weight dimensions.
 * \param numLocalCoords is a size_t value to represent the count of local coordinates.
 * \param numGlobalParts is a size_t value to represent the total part count. //not used currently inside the pqJagged algorithm.
 * \param pqJagged_multiVectorDim  ...//not used by pqJagged algorithm.
 * \param pqJagged_values is the output representing the coordinates of local points.
 *  Its size is coordDim x numLocalCoords and allocated before the function.
 * \param criteriaDim ...//not used currently inside the pqJagged algorithm.
 * \param pqJagged_weights is the two dimensional array output, representing the weights of
 * coordinates in each weight dimension. Sized weightDim x numLocalCoords, and allocated before the function.
 * \param pqJagged_gnos is the ArrayView output representing the global indices of each vertex. No allocation is needed.
 * \param ignoreWeights is the boolean input of the function, to treat the each coordinate
 * \param pqJagged_uniformWeights is the boolean array representing whether or not coordinates have uniform weight in each weight dimension.
 * \param pqJagged_uniformParts is the boolean array representing whether or not the desired part weights are uniform in each weight dimension.
 * \param pqJagged_partSizes is the two dimensional float-like array output that represents the ratio of each part.
 *
 */


template <typename Adapter, typename scalar_t, typename gno_t>
void pqJagged_getInputValues(
        const RCP<const Environment> &env, const RCP<const CoordinateModel<
        typename Adapter::base_adapter_t> > &coords,
        RCP<PartitioningSolution<Adapter> > &solution,
        std::bitset<NUM_RCB_PARAMS> &params,
        const int &coordDim,
        const int &weightDim,
        const size_t &numLocalCoords, size_t &numGlobalParts, int &pqJagged_multiVectorDim,
        scalar_t **pqJagged_values, const int &criteriaDim, scalar_t **pqJagged_weights, ArrayView<const gno_t> &pqJagged_gnos, bool &ignoreWeights,
        bool *pqJagged_uniformWeights, bool *pqJagged_uniformParts, scalar_t **pqJagged_partSizes
){
    //typedef typename Adapter::node_t node_t;
    typedef typename Adapter::lno_t lno_t;
    typedef StridedData<lno_t, scalar_t> input_t;

    ArrayView<const gno_t> gnos;
    ArrayView<input_t>     xyz;
    ArrayView<input_t>     wgts;

    coords->getCoordinates(gnos, xyz, wgts);
    pqJagged_gnos = gnos;


    //std::cout << std::endl;

    for (int dim=0; dim < coordDim; dim++){
        ArrayRCP<const scalar_t> ar;
        xyz[dim].getInputArray(ar);
        //pqJagged coordinate values assignment
        pqJagged_values[dim] =  (scalar_t *)ar.getRawPtr();
    }

    if (weightDim == 0 || ignoreWeights){

        pqJagged_uniformWeights[0] = true;
        pqJagged_weights[0] = NULL;
    }
    else{
        for (int wdim = 0; wdim < weightDim; wdim++){
            if (wgts[wdim].size() == 0){
                pqJagged_uniformWeights[wdim] = true;
                pqJagged_weights[wdim] = NULL;
            }
            else{
                ArrayRCP<const scalar_t> ar;
                wgts[wdim].getInputArray(ar);
                pqJagged_uniformWeights[wdim] = false;
                pqJagged_weights[wdim] = (scalar_t *) ar.getRawPtr();
            }
        }
    }

    ////////////////////////////////////////////////////////
    // From the Solution we get part information.
    // If the part sizes for a given criteria are not uniform,
    // then they are values that sum to 1.0.

    numGlobalParts = solution->getTargetGlobalNumberOfParts();

    for (int wdim = 0; wdim < criteriaDim; wdim++){
        if (solution->criteriaHasUniformPartSizes(wdim)){
            pqJagged_uniformParts[wdim] = true;
            pqJagged_partSizes[wdim] = NULL;
        }
        else{
            //TODO
            scalar_t *tmp = new scalar_t [numGlobalParts];
            env->localMemoryAssertion(__FILE__, __LINE__, numGlobalParts, tmp) ;
            for (size_t i=0; i < numGlobalParts; i++){
                tmp[i] = solution->getCriteriaPartSize(wdim, i);
            }
            pqJagged_partSizes[wdim] = tmp;
        }
    }

    // It may not be possible to solve the partitioning problem
    // if we have multiple weight dimensions with part size
    // arrays that differ. So let's be aware of this possibility.

    bool multiplePartSizeSpecs = false;

    if (criteriaDim > 1){
        for (int wdim1 = 0; wdim1 < criteriaDim; wdim1++)
            for (int wdim2 = wdim1+1; wdim2 < criteriaDim; wdim2++)
                if (!solution->criteriaHaveSamePartSizes(wdim1, wdim2)){
                    multiplePartSizeSpecs = true;
                    break;
                }
    }

    if (multiplePartSizeSpecs)
        params.set(rcb_multiplePartSizeSpecs);

    ////////////////////////////////////////////////////////
    // Create the distributed data for the algorithm.
    //
    // It is a multivector containing one vector for each coordinate
    // dimension, plus a vector for each weight dimension that is not
    // uniform.

    pqJagged_multiVectorDim = coordDim;
    for (int wdim = 0; wdim < criteriaDim; wdim++){
        if (!pqJagged_uniformWeights[wdim]) pqJagged_multiVectorDim++;
    }

}


/*! \brief Printing the input values, to check configuration.
 *
 */
template <typename scalar_t, typename gno_t>
void pqJagged_printInput(int coordDim, int weightDim, size_t numLocalCoords, global_size_t numGlobalCoords,
        int criteriaDim, scalar_t **pqJagged_values, scalar_t **pqJagged_weights,
        bool *pqJagged_uniformParts, bool *pqJagged_uniformWeights, gno_t *pqJagged_gnos,
        bool ignoreWeights,size_t numGlobalParts, scalar_t **pqJagged_partSizes){

    std::cout << "numLocalCoords:" << numLocalCoords << std::endl;
    std::cout << "coordDim:" << coordDim << std::endl;
    for(size_t i = 0; i < numLocalCoords; ++i){
        for (int ii = 0; ii < coordDim; ++ii){
            std::cout <<  pqJagged_values[ii][i] << " ";
        }
        std::cout << std::endl;
    }


    std::cout << "criteriaDim:" << criteriaDim << std::endl;
    std::cout << "weightDim:" << weightDim << std::endl;
    if(weightDim){
        for(size_t i = 0; i < numLocalCoords; ++i){
            for (int ii = 0; ii < weightDim; ++ii){
                std::cout <<  pqJagged_weights[ii][i] << " ";
            }
            std::cout << std::endl;
        }
    }

    std::cout << "pqJagged_uniformWeights:" << pqJagged_uniformWeights[0] << std::endl;
    for(int i = 0; i < criteriaDim; ++i){
        std::cout << pqJagged_uniformWeights[i] << " ";
    }
    std::cout << std::endl;


    std::cout << "gnos" << std::endl;
    for(size_t i = 0; i < numLocalCoords; ++i){
        std::cout <<  pqJagged_gnos[i] << " ";
    }
    std::cout << std::endl;

    std::cout << "ignoreWeights:" << ignoreWeights << std::endl;

    std::cout << "pqJagged_uniformParts:" << pqJagged_uniformParts[0] << std::endl;
    for(int i = 0; i < criteriaDim; ++i){
        std::cout << pqJagged_uniformParts[i] << " ";
    }
    std::cout << std::endl;

    std::cout << "pqJagged_partSizes:" << std::endl;
    std::cout << "numGlobalParts:" << numGlobalParts << std::endl;
    for(int i = 0; i < criteriaDim; ++i){
        if(!pqJagged_uniformParts[i])
            for(size_t ii = 0; ii < numGlobalParts; ++ii){
                std::cout << pqJagged_partSizes[i][ii] << " ";
            }
        std::cout << std::endl;
    }
}


/*! \brief Function to determine the local minimum and maximum coordinate, and local total weight
 * in the given set of local points.
 * \param partitionedPointPermutations is the indices of coordinates in the given partition.
 * \param pqJagged_coordinates float-like array representing the coordinates in a single dimension. Sized as numLocalCoords.
 * \param pqJagged_uniformWeights boolean value whether each coordinate value has a uniform weight or not. If true, pqJagged_weights is not used.
 * \param pqJagged_weights float-like array representing the weights of the coordinates in a single criteria dimension. Sized as numLocalCoords.
 *
 * \param numThreads is the integer value to represent the number of threads available for each processor.
 * \param coordinateBegin is the start index of the given partition on partitionedPointPermutations.
 * \param coordinateEnd is the end index of the given partition on partitionedPointPermutations.
 *
 * \param max_min_array provided work array sized numThreads * 2.
 * \param maxScalar to initialize minimum coordinate if there are no points in the given range.
 * \param minScalar to initialize maximum coordinate if there are no points in the given range.
 *
 * \param minCoordinate is the output to represent the local minimumCoordinate in  given range of coordinates.
 * \param maxCoordinate is the output to represent the local maximum coordinate in the given range of coordinates.
 * \param totalWeight is the output to represent the local total weight in the coordinate in the given range of coordinates.
 *
 */
template <typename scalar_t, typename lno_t>
void pqJagged_getLocalMinMaxTotalCoord(
        lno_t *partitionedPointPermutations,
        scalar_t *pqJagged_coordinates,
        bool pqJagged_uniformWeights,
        scalar_t *pqJagged_weights,


        int numThreads,
        lno_t coordinateBegin,
        lno_t coordinateEnd,

        scalar_t *max_min_array /*sized nothreads * 2*/,
        scalar_t maxScalar,
        scalar_t minScalar,
        scalar_t &minCoordinate,
        scalar_t &maxCoordinate,
        scalar_t &totalWeight
        ,RCP<Comm<int> > &problemComm
){


    //if the part is empty.
    //set the min and max coordinates as reverse.
    if(coordinateBegin >= coordinateEnd)
    {
        minCoordinate = maxScalar;
        maxCoordinate = minScalar;
        totalWeight = 0;
    }
    else {
        scalar_t mytotalWeight = 0;
#ifdef HAVE_ZOLTAN2_OMP
#pragma omp parallel
#endif
        {
            //if uniform weights are used, then weight is equal to count.
            if (pqJagged_uniformWeights) {
#ifdef HAVE_ZOLTAN2_OMP
#pragma omp single
#endif
                {
                    mytotalWeight = coordinateEnd - coordinateBegin;
                }

            }
            else {
                //if not uniform, then weights are reducted from threads.
#ifdef HAVE_ZOLTAN2_OMP
#pragma omp for reduction(+:mytotalWeight)
#endif
                for (lno_t ii = coordinateBegin; ii < coordinateEnd; ++ii){
                    int i = partitionedPointPermutations[ii];
                    mytotalWeight += pqJagged_weights[i];
                }
            }

            int myId = 0;
#ifdef HAVE_ZOLTAN2_OMP
            myId = omp_get_thread_num();
#endif
            scalar_t myMin, myMax;


            myMin=myMax
                    =pqJagged_coordinates[partitionedPointPermutations[coordinateBegin]];


#ifdef HAVE_ZOLTAN2_OMP
#pragma omp for
#endif
            for(lno_t j = coordinateBegin + 1; j < coordinateEnd; ++j){
                int i = partitionedPointPermutations[j];
                if(pqJagged_coordinates[i] > myMax) myMax = pqJagged_coordinates[i];
                if(pqJagged_coordinates[i] < myMin) myMin = pqJagged_coordinates[i];
            }
            max_min_array[myId] = myMin;
            max_min_array[myId + numThreads] = myMax;



#ifdef HAVE_ZOLTAN2_OMP
#pragma omp barrier
#endif

#ifdef HAVE_ZOLTAN2_OMP
#pragma omp single nowait
#endif
            {
                minCoordinate = max_min_array[0];
                for(int i = 1; i < numThreads; ++i){
                    if(max_min_array[i] < minCoordinate) minCoordinate = max_min_array[i];
                }
            }



#ifdef HAVE_ZOLTAN2_OMP
#pragma omp single nowait
#endif
            {
                maxCoordinate = max_min_array[numThreads];
                for(int i = numThreads + 1; i < numThreads * 2; ++i){
                    if(max_min_array[i] > maxCoordinate) maxCoordinate = max_min_array[i];
                }
            }
        }
        totalWeight = mytotalWeight;
    }
}

template <typename partId_t>
inline partId_t getPartCount(partId_t numFuture, float root, float fEpsilon){
    float fp = pow(numFuture, root);
    partId_t ip = partId_t (fp);
    if (fp - ip < fEpsilon){
        return ip;
    }
    else {
        return ip  + 1;
    }
}


/*! \brief Function that reduces global minimum and maximum coordinates with global total weight from given local arrays.
 * \param comm the communicator for the problem
 * \param env   library configuration and problem parameters
 * \param concurrentPartCount is the number of parts whose cut lines will be calculated concurrently.
 * \param localMinMaxTotal is the array holding local min and max coordinate values with local total weight.
 * First concurrentPartCount entries are minimums of the parts, next concurrentPartCount entries are max, and then the total weights.
 * \param localMinMaxTotal is the output array holding global min and global coordinate values with global total weight.
 * The structure is same as localMinMaxTotal.
 */
template <typename scalar_t>
void pqJagged_getGlobalMinMaxTotalCoord(
        RCP<Comm<int> > &comm,
        const RCP<const Environment> &env,
        partId_t concurrentPartCount,
        scalar_t *localMinMaxTotal,
        scalar_t *globalMinMaxTotal){


    //reduce min for first concurrentPartCount elements, reduce max for next concurrentPartCount elements,
    //reduce sum for the last concurrentPartCount elements.
    if(comm->getSize()  > 1){

#ifndef mpi_communication
        Teuchos::PQJaggedCombinedMinMaxTotalReductionOp<int, scalar_t> reductionOp(
                concurrentPartCount,
                concurrentPartCount,
                concurrentPartCount);
#endif

#ifdef mpi_communication
        MPI_Op myop;
        MPI_Op_create(minMaxSum, 0, &myop);   /* step 3 */
#endif

        try{


#ifdef mpi_communication

            MPI_Allreduce(localMinMaxTotal, globalMinMaxTotal, 3 * concurrentPartCount, MPI_FLOAT, myop,MPI_COMM_WORLD);
#endif
#ifndef mpi_communication
            reduceAll<int, scalar_t>(*comm, reductionOp,
                    3 * concurrentPartCount, localMinMaxTotal, globalMinMaxTotal
            );
#endif

        }
        Z2_THROW_OUTSIDE_ERROR(*env)
    }
    else {
        partId_t s = 3 * concurrentPartCount;
        for (partId_t i = 0; i < s; ++i){
            globalMinMaxTotal[i] = localMinMaxTotal[i];
        }
    }
}


/*! \brief Function that calculates the new coordinates for the cut lines. Function is called inside the parallel region.
 * \param minCoordinate minimum coordinate in the range.
 * \param maxCoordinate maximum coordinate in the range.
 * \param pqJagged_uniformParts is a boolean value holding whether the desired partitioning is uniform.
 * \param pqJagged_partSizes holds the desired parts sizes if desired partitioning is not uniform.
 * \param cutCoordinates is the output array for the initial cut lines.
 * \param cutPartRatios is the output array holding the cumulative ratios of parts in current partitioning.
 * For partitioning to 4 uniformly, cutPartRatios will be (0.25, 0.5 , 0.75, 1).
 * \param numThreads hold the number of threads available per mpi.
 */
template <typename scalar_t>
void pqJagged_getCutCoord_Weights(
        scalar_t minCoordinate,
        scalar_t maxCoordinate,
        bool pqJagged_uniformParts,
        scalar_t *pqJagged_partSizes /*p sized, weight ratios of each part*/,
        partId_t noCuts/*p-1*/ ,
        scalar_t *cutCoordinates /*p - 1 sized, coordinate of each cut line*/,
        scalar_t *cutPartRatios /*cumulative weight ratios, at left side of each cut line. p-1 sized*/,
        int numThreads,
#ifdef omitted
        const partId_t *partNo,
#endif
        vector <partId_t> *currentPartitions,
        vector <partId_t> *futurePartitions,
        partId_t partIndex,
        partId_t futureArrayIndex
){

    scalar_t coordinateRange = maxCoordinate - minCoordinate;
    if(pqJagged_uniformParts){
#ifdef omitted
        if(0 && partNo){
            scalar_t uniform = 1. / (noCuts + 1);
            scalar_t slice = uniform * coordinateRange;

#ifdef HAVE_ZOLTAN2_OMP
#pragma omp parallel
#endif
            {
                partId_t myStart = 0;
                partId_t myEnd = noCuts;
#ifdef HAVE_ZOLTAN2_OMP
#pragma omp for
#endif
                for(partId_t i = myStart; i < myEnd; ++i){
                    cutPartRatios[i] = uniform * (i + 1);
                    cutCoordinates[i] = minCoordinate + slice * (i + 1);
                }
                cutPartRatios[noCuts] = 1;
            }
        }
        else
#endif
        {
            partId_t cumulative = 0;
            scalar_t totalInnerPartCount = scalar_t((*currentPartitions)[partIndex]);

            for(partId_t i = 0; i < noCuts; ++i){
                cumulative += (*futurePartitions)[i + futureArrayIndex];
                cutPartRatios[i] = (cumulative /*+  (*futurePartitions)[i + futureArrayIndex]*/) / (totalInnerPartCount);
                cutCoordinates[i] = minCoordinate + coordinateRange * cutPartRatios[i];

            }
            cutPartRatios[noCuts] = 1;
        }


    }
    else {
        //TODO fix here!!
        cutPartRatios[0] = pqJagged_partSizes[0];
        cutCoordinates[0] = coordinateRange * cutPartRatios[0];
        for(partId_t i = 1; i < noCuts; ++i){
            cutPartRatios[i] = pqJagged_partSizes[i] + cutPartRatios[i - 1];
            cutCoordinates[i] = coordinateRange * cutPartRatios[i];
        }
    }
}


/*! \brief Function that calculates the new coordinates for the cut lines. Function is called inside the parallel region.
 *
 * \param env library configuration and problem parameters
 * \param comm the communicator for the problem
 * \param total_part_count is the sum of number of cutlines and number of parts. Simply it is 2*P - 1.
 * \param noCuts is the number of cut lines. P - 1.
 * \param maxCoordinate is the maximum coordinate in the current range of coordinates and in the current dimension.
 * \param minCoordinate is the maximum coordinate in the current range of coordinates and in the current dimension.
 * \param globalTotalWeight is the global total weight in the current range of coordinates.
 * \param imbalanceTolerance is the maximum allowed imbalance ratio.
 * \param maxScalar is the maximum value that scalar_t can represent.
 *
 *
 * \param globalPartWeights is the array holding the weight of parts. Assumes there are 2*P - 1 parts (cut lines are seperate parts).
 * \param localPartWeights is the local totalweight of the processor.
 * \param targetPartWeightRatios are the desired cumulative part ratios, sized P.
 * \param isDone is the boolean array to determine if the correct position for a cut line is found.
 *
 * \param cutCoordinates is the array holding the coordinates of each cut line. Sized P - 1.
 * \param cutUpperBounds is the array holding the upper bound coordinate for each cut line. Sized P - 1.
 * \param cutLowerBounds is the array holding the lower bound coordinate for each cut line. Sized P - 1.
 * \param leftClosestDistance is the array holding the distances to the closest points to the cut lines from left.
 * \param rightClosestDistance is the array holding the distances to the closest points to the cut lines from right.
 * \param cutLowerWeight is the array holding the weight of the parts at the left of lower bound coordinates.
 * \param cutUpperWeight is the array holding the weight of the parts at the left of upper bound coordinates.
 * \param newCutCoordinates is the work array, sized P - 1.
 *
 * \param allowNonRectelinearPart is the boolean value whether partitioning should allow distributing the points on same coordinate to different parts.
 * \param nonRectelinearPartRatios holds how much percentage of the coordinates on the cutline should be put on left side.
 * \param rectilinearCutCount is the count of cut lines whose balance can be achived via distributing the points in same coordinate to different parts.
 * \param localCutWeights is the local weight of coordinates on cut lines.
 * \param globalCutWeights is the global weight of coordinates on cut lines.
 *
 * \param myNoneDoneCount is the number of cutlines whose position has not been determined yet. For K > 1 it is the count in a single part (whose cut lines are determined).
 */
template <typename scalar_t>
void getNewCoordinates(
        const RCP<const Environment> &env,
        RCP<Comm<int> > &comm,
        const size_t &total_part_count,
        const partId_t &noCuts,
        const scalar_t &maxCoordinate,
        const scalar_t &minCoordinate,
        const scalar_t &globalTotalWeight,
        const scalar_t &imbalanceTolerance,
        scalar_t maxScalar,

        const scalar_t * globalPartWeights,
        const scalar_t * localPartWeights,
        const scalar_t *targetPartWeightRatios,
        bool *isDone,

        scalar_t *cutCoordinates,
        scalar_t *cutUpperBounds,
        scalar_t *cutLowerBounds,
        scalar_t *leftClosestDistance,
        scalar_t *rightClosestDistance,
        scalar_t * cutLowerWeight,
        scalar_t * cutUpperWeight,
        scalar_t *newCutCoordinates,

        bool allowNonRectelinearPart,
        float *nonRectelinearPartRatios,
        partId_t *rectilinearCutCount,
        scalar_t *localCutWeights,
        scalar_t *globalCutWeights,

        partId_t &myNoneDoneCount
)
{


    scalar_t seenW = 0;
    float expected = 0;
    scalar_t leftImbalance = 0, rightImbalance = 0;
    scalar_t _EPSILON = numeric_limits<scalar_t>::epsilon();
    //scalar_t _EPSILON = numeric_limits<float>::epsilon();

#ifdef HAVE_ZOLTAN2_OMP
#pragma omp for
#endif
    for (partId_t i = 0; i < noCuts; i++){

        //if a left and right closes point is not found, set the distance to 0.
        if(leftClosestDistance[i] == maxScalar)
            leftClosestDistance[i] = 0;
        if(rightClosestDistance[i] == maxScalar)
            rightClosestDistance[i] = 0;

    }

#ifdef HAVE_ZOLTAN2_OMP
#pragma omp for
#endif
    for (partId_t i = 0; i < noCuts; i++){
        globalCutWeights[i] = 0;
        localCutWeights[i] = 0;
        //if already determined at previous iterations, do nothing.
        if(isDone[i]) {
            newCutCoordinates[i] = cutCoordinates[i];
            continue;
        }
        //current weight of the part at the left of the cut line.
        seenW = globalPartWeights[i * 2];


        //expected ratio
        expected = targetPartWeightRatios[i];
        leftImbalance = imbalanceOf(seenW, globalTotalWeight, expected);
        rightImbalance = imbalanceOf(globalTotalWeight - seenW, globalTotalWeight, 1 - expected);

        bool isLeftValid = ABS(leftImbalance) - imbalanceTolerance < _EPSILON ;
        bool isRightValid = ABS(rightImbalance) - imbalanceTolerance < _EPSILON;

        //if the cut line reaches to desired imbalance.
        if(isLeftValid && isRightValid){

            isDone[i] = true;
#ifdef HAVE_ZOLTAN2_OMP
#pragma omp atomic
#endif
            myNoneDoneCount -= 1;
            newCutCoordinates [i] = cutCoordinates[i];
            continue;
        }
        else if(leftImbalance < 0){

            scalar_t ew = globalTotalWeight * expected;
            if(allowNonRectelinearPart){

                if (globalPartWeights[i * 2 + 1] == ew){
                    isDone[i] = true;
#ifdef HAVE_ZOLTAN2_OMP
#pragma omp atomic
#endif
                    myNoneDoneCount -= 1;
                    newCutCoordinates [i] = cutCoordinates[i];
                    nonRectelinearPartRatios[i] = 1;
                    continue;
                }
                else if (globalPartWeights[i * 2 + 1] > ew){
                    isDone[i] = true;
#ifdef HAVE_ZOLTAN2_OMP
#pragma omp atomic
#endif
                    *rectilinearCutCount += 1;

#ifdef HAVE_ZOLTAN2_OMP
#pragma omp atomic
#endif
                    myNoneDoneCount -= 1;
                    newCutCoordinates [i] = cutCoordinates[i];
                    scalar_t myWeightOnLine = localPartWeights[i * 2 + 1] - localPartWeights[i * 2];
                    localCutWeights[i] = myWeightOnLine;
                    continue;
                }
            }
            //when moving right, set lower bound to current line.
            cutLowerBounds[i] = cutCoordinates[i] + rightClosestDistance[i];
            cutLowerWeight[i] = seenW;

            //compare the upper bound with the current lines.
            for (partId_t ii = i + 1; ii < noCuts ; ++ii){
                scalar_t pw = globalPartWeights[ii * 2];
                scalar_t lw = globalPartWeights[ii * 2 + 1];
                if(pw >= ew){
                    if(pw == ew){
                        cutUpperBounds[i] = cutCoordinates[ii];
                        cutUpperWeight[i] = pw;
                        cutLowerBounds[i] = cutCoordinates[ii];
                        cutLowerWeight[i] = pw;
                    } else if (pw < cutUpperWeight[i]){
                        //if a cut line is more strict than the current upper bound,
                        //update the upper bound.
                        cutUpperBounds[i] = cutCoordinates[ii] - leftClosestDistance[ii];
                        cutUpperWeight[i] = pw;
                    }
                    break;
                }
                //if comes here then pw < ew
                if(lw >= ew){
                    cutUpperBounds[i] = cutCoordinates[ii];
                    cutUpperWeight[i] = lw;
                    cutLowerBounds[i] = cutCoordinates[ii];
                    cutLowerWeight[i] = pw;
                    break;
                }
                //if a stricter lower bound is found,
                //update the lower bound.
                if (pw <= ew && pw >= cutLowerWeight[i]){
                    cutLowerBounds[i] = cutCoordinates[ii] + rightClosestDistance[ii] ;
                    cutLowerWeight[i] = pw;
                }
            }
            scalar_t newPivot = pivotPos<scalar_t> (cutUpperBounds, cutLowerBounds,i, cutUpperWeight, cutLowerWeight, ew);
            //if cut line does not move significantly.
            if (ABS(cutCoordinates[i] - newPivot) < _EPSILON * EPS_SCALE || cutUpperBounds[i] < cutLowerBounds[i]){
                isDone[i] = true;
#ifdef HAVE_ZOLTAN2_OMP
#pragma omp atomic
#endif
                myNoneDoneCount -= 1;
                newCutCoordinates [i] = cutCoordinates[i];
            } else {
                newCutCoordinates [i] = newPivot;
            }
        } else {
            //moving to left.
            scalar_t ew = globalTotalWeight * expected;
            //moving left, set upper to current line.
            cutUpperBounds[i] = cutCoordinates[i] - leftClosestDistance[i];
            cutUpperWeight[i] = seenW;

            // compare the current cut line weights with previous upper and lower bounds.
            for (int ii = i - 1; ii >= 0; --ii){
                scalar_t pw = globalPartWeights[ii * 2];
                scalar_t lw = globalPartWeights[ii * 2 + 1];
                if(pw <= ew){
                    if(pw == ew){
                        cutUpperBounds[i] = cutCoordinates[ii];
                        cutUpperWeight[i] = pw;
                        cutLowerBounds[i] = cutCoordinates[ii];
                        cutLowerWeight[i] = pw;
                    }
                    else if (pw > cutLowerWeight[i]){
                        cutLowerBounds[i] = cutCoordinates[ii] + rightClosestDistance[ii];
                        cutLowerWeight[i] = pw;
                        if(lw > ew){
                            cutUpperBounds[i] = cutCoordinates[ii] + rightClosestDistance[ii];

                            cutUpperWeight[i] = lw;
                        }
                    }
                    break;
                }
                if (pw >= ew && (pw < cutUpperWeight[i] || (pw == cutUpperWeight[i] && cutUpperBounds[i] > cutCoordinates[ii] - leftClosestDistance[ii]))){
                    cutUpperBounds[i] = cutCoordinates[ii] - leftClosestDistance[ii] ;

                    cutUpperWeight[i] = pw;
                }
            }

            scalar_t newPivot = pivotPos<scalar_t> (cutUpperBounds, cutLowerBounds,i, cutUpperWeight, cutLowerWeight, ew);
            //if cut line does not move significantly.
            if (ABS(cutCoordinates[i] - newPivot) < _EPSILON * EPS_SCALE  || cutUpperBounds[i] < cutLowerBounds[i]){
                isDone[i] = true;

#ifdef HAVE_ZOLTAN2_OMP
#pragma omp atomic
#endif
                myNoneDoneCount -= 1;
                newCutCoordinates [ i] = cutCoordinates[i];
            } else {
                newCutCoordinates [ i] = newPivot;
            }
        }
    }



    //communication to determine the ratios of processors for the distribution of coordinates on the cut lines.
#ifdef HAVE_ZOLTAN2_OMP
    //#pragma omp barrier
#pragma omp single
#endif
    {
        if(*rectilinearCutCount > 0){
            try{
                Teuchos::scan<int,scalar_t>(
                        *comm, Teuchos::REDUCE_SUM,
                        noCuts,localCutWeights, globalCutWeights
                );
            }
            Z2_THROW_OUTSIDE_ERROR(*env)


            for (partId_t i = 0; i < noCuts; ++i){
                //cout << "gw:" << globalCutWeights[i] << endl;
                if(globalCutWeights[i] > 0) {
                    scalar_t ew = globalTotalWeight * targetPartWeightRatios[i];
                    scalar_t expectedWeightOnLine = ew - globalPartWeights[i * 2];
                    scalar_t myWeightOnLine = localCutWeights[i];
                    scalar_t weightOnLineBefore = globalCutWeights[i];
                    scalar_t incMe = expectedWeightOnLine - weightOnLineBefore;
                    scalar_t mine = incMe + myWeightOnLine;
                    if(mine < 0){
                        nonRectelinearPartRatios[i] = 0;
                    }
                    else if(mine >= myWeightOnLine){
                        nonRectelinearPartRatios[i] = 1;
                    }
                    else {
                        nonRectelinearPartRatios[i] = mine / myWeightOnLine;
                    }
                }
            }
            *rectilinearCutCount = 0;
        }
    }
}


/*! \brief Function that calculates the new coordinates for the cut lines. Function is called inside the parallel region.
 *
 * \param total_part_count is the sum of number of cutlines and number of parts. Simply it is 2*P - 1.
 * \param noCuts is the number of cut lines. P - 1.
 * \param maxScalar is the maximum value that scalar_t can represent.
 * \param _EPSILON is the smallest error value for scalar_t.
 * \param numThreads hold the number of threads available per processor.
 *
 * \param coordinateBegin is the index of the first coordinate in current part.
 * \param coordinateEnd is the index of the last coordinate in current part.
 * \param partitionedPointPermutations is the indices of coordinates in the given partition.
 * \param pqJagged_coordinates is 1 dimensional array holding coordinate values.
 * \param pqJagged_uniformWeights is a boolean value if the points have uniform weights.
 * \param pqJagged_weights is 1 dimensional array holding the weights of points.
 *
 * \param globalPartWeights is the array holding the weight of parts. Assumes there are 2*P - 1 parts (cut lines are seperate parts).
 * \param localPartWeights is the local totalweight of the processor.
 * \param targetPartWeightRatios are the desired cumulative part ratios, sized P.
 *
 * \param cutCoordinates_tmp is the array holding the coordinates of each cut line. Sized P - 1.
 * \param isDone is the boolean array to determine if the correct position for a cut line is found.
 * \param myPartWeights is the array holding the part weights for the calling thread.
 * \param myLeftClosest is the array holding the distances to the closest points to the cut lines from left for the calling thread..
 * \param myRightClosest is the array holding the distances to the closest points to the cut lines from right for the calling thread.
 * \param useBinarySearch is boolean parameter whether to search for cut lines with binary search of linear search.
 * \param partIds is the array that holds the part ids of the coordinates
 * kddnote:  The output of this function myPartWeights differs depending on whether
 * kddnote:  binary or linear search is used.  Values in myPartWeights should be the
 * kddnote:  same only after accumulateThreadResults is done.
 */
template <typename scalar_t, typename lno_t>
void pqJagged_1DPart_getPartWeights(
        size_t total_part_count,
        partId_t noCuts,
        scalar_t maxScalar,
        scalar_t _EPSILON,
        int numThreads,
        lno_t coordinateBegin,
        lno_t coordinateEnd,
        lno_t *partitionedPointPermutations,
        scalar_t *pqJagged_coordinates,
        bool pqJagged_uniformWeights,
        scalar_t *pqJagged_weights,

        scalar_t *cutCoordinates_tmp, //TODO change name
        bool *isDone,
        double *myPartWeights,
        scalar_t *myLeftClosest,
        scalar_t *myRightClosest,
        bool useBinarySearch,
        partId_t *partIds
){

    // initializations for part weights, left/right closest
    for (size_t i = 0; i < total_part_count; ++i){
        myPartWeights[i] = 0;
    }


    for(partId_t i = 0; i < noCuts; ++i){
        //if(isDone[i]) continue;
        myLeftClosest[i] = maxScalar;
        myRightClosest[i] = maxScalar;
    }
    if(useBinarySearch){

        //lno_t comparison_count = 0;
        scalar_t minus_EPSILON = -_EPSILON;
#ifdef HAVE_ZOLTAN2_OMP
#pragma omp for
#endif
        for (lno_t ii = coordinateBegin; ii < coordinateEnd; ++ii){
            int i = partitionedPointPermutations[ii];
            partId_t j = partIds[i] / 2;

            if(j >= noCuts){
                j = noCuts - 1;
            }

            partId_t lc = 0;
            partId_t uc = noCuts - 1;

            scalar_t w = pqJagged_uniformWeights? 1:pqJagged_weights[i];
            bool isInserted = false;
            bool onLeft = false;
            bool onRight = false;
            partId_t lastPart = -1;

            scalar_t coord = pqJagged_coordinates[i];

            while(uc >= lc)
            {
                //comparison_count++;
                lastPart = -1;
                onLeft = false;
                onRight = false;
                scalar_t cut = cutCoordinates_tmp[j];
                scalar_t distance = coord - cut;
                scalar_t absdistance = ABS(distance);

                if(absdistance < _EPSILON){
                    myPartWeights[j * 2 + 1] += w;
                    partIds[i] = j * 2 + 1;

                    myLeftClosest[j] = 0;
                    myRightClosest[j] = 0;
                    partId_t kk = j + 1;
                    while(kk < noCuts){
                        // Needed when cuts shared the same position
                        // kddnote Can this loop be disabled for RECTILINEAR BLOCKS?
                        // kddnote Mehmet says it is probably needed anyway.
                        distance =ABS(cutCoordinates_tmp[kk] - cut);
                        if(distance < _EPSILON){
                            myPartWeights[2 * kk + 1] += w;

                            myLeftClosest[kk] = 0;
                            myRightClosest[kk] = 0;
                            kk++;
                        }
                        else{
                            if(myLeftClosest[kk] > distance){
                                myLeftClosest[kk] = distance;
                            }
                            break;
                        }
                    }

                    kk = j - 1;
                    while(kk >= 0){
                        distance =ABS(cutCoordinates_tmp[kk] - cut);
                        if(distance < _EPSILON){
                            myPartWeights[2 * kk + 1] += w;

                            myLeftClosest[kk] = 0;
                            myRightClosest[kk] = 0;
                            kk--;
                        }
                        else{
                            if(myRightClosest[kk] > distance){
                                myRightClosest[kk] = distance;
                            }
                            break;
                        }
                    }
                    isInserted = true;
                    break;
                }
                else {
                    if (distance < 0) {
                        //TODO fix abs
                        distance = absdistance;
                        /*
                   if (myLeftClosest[j] > distance){
                   myLeftClosest[j] = distance;
                   }
                         */
                        bool _break = false;
                        if(j > 0){
                            distance = coord - cutCoordinates_tmp[j - 1];
                            if(distance > _EPSILON){
                                /*
                       if (myRightClosest[j - 1] > distance){
                       myRightClosest[j - 1] = distance;
                       }
                                 */
                                _break = true;
                            }
                            /*
                     else if(distance < minus_EPSILON){
                     distance = -distance;
                     if (myLeftClosest[j - 1] > distance){
                     myLeftClosest[j - 1] = distance;
                     }
                     }
                     else {
                     myLeftClosest[j - 1] = 0;
                     myRightClosest[j - 1 ] = 0;
                     }*/
                        }
                        uc = j - 1;
                        onLeft = true;
                        lastPart = j;
                        if(_break) break;
                    }
                    else {
                        /*
                   if (myRightClosest[j] > distance){
                   myRightClosest[j] = distance;
                   }
                         */
                        bool _break = false;
                        if(j < noCuts - 1){
                            scalar_t distance_ = coord - cutCoordinates_tmp[j + 1];
                            /*
                     if(distance > _EPSILON){
                     if (myRightClosest[j + 1] > distance){
                     myRightClosest[j + 1] = distance;
                     }
                     } else */if(distance_ < minus_EPSILON){
                         /*
                          distance = -distance;
                          if (myLeftClosest[j + 1] > distance){
                          myLeftClosest[j + 1] = distance;
                          }*/
                         _break = true;
                     }
                     /*
                   else {
                   myLeftClosest[j + 1] = 0;
                   myRightClosest[j + 1 ] = 0;
                   }
                      */
                        }
                        lc = j + 1;
                        onRight = true;
                        lastPart = j;
                        if(_break) break;
                    }
                }

                j = (uc + lc) / 2;
            }
            if(!isInserted){
                if(onRight){


                    myPartWeights[2 * lastPart + 2] += w;
                    partIds[i] = 2 * lastPart + 2;
                    scalar_t distance = coord - cutCoordinates_tmp[lastPart];
                    if(myRightClosest[lastPart] > distance){
                        myRightClosest[lastPart] = distance;
                    }
                    if(lastPart+1 < noCuts){
                        scalar_t distance_ = cutCoordinates_tmp[lastPart + 1] - coord;
                        if(myLeftClosest[lastPart + 1] > distance_){
                            myLeftClosest[lastPart + 1] = distance_;
                        }
                    }

                }
                else if(onLeft){
                    myPartWeights[2 * lastPart] += w;
                    partIds[i] = 2 * lastPart;
                    scalar_t distance = cutCoordinates_tmp[lastPart ] - coord;
                    if(myLeftClosest[lastPart] > distance){
                        myLeftClosest[lastPart] = distance;
                    }

                    if(lastPart-1 >= 0){
                        scalar_t distance_ = coord - cutCoordinates_tmp[lastPart - 1];
                        if(myRightClosest[lastPart -1] > distance_){
                            myRightClosest[lastPart -1] = distance_;
                        }
                    }
                }
            }
        }
    }
    else {

#ifdef HAVE_ZOLTAN2_OMP
#pragma omp for
#endif
        for (lno_t ii = coordinateBegin; ii < coordinateEnd; ++ii){
            int i = partitionedPointPermutations[ii];

            scalar_t w = pqJagged_uniformWeights ? 1 : pqJagged_weights[i];

            scalar_t coord = pqJagged_coordinates[i];

            partId_t j = partIds[i] / 2;

            if(j >= noCuts){
                j = noCuts - 1;
            }
            scalar_t cut = cutCoordinates_tmp[j];
            scalar_t distance = coord - cut;
            scalar_t absdistance = ABS(distance);

            //comp++;
            if(absdistance < _EPSILON){
                myPartWeights[j * 2 + 1] += w;

                myLeftClosest[j] = 0;
                myRightClosest[j] = 0;
                partIds[i] = j * 2 + 1;

                //bas
                partId_t kk = j + 1;
                while(kk < noCuts){  // Needed when cuts shared the same position
                    // kddnote Can this loop be disabled for RECTILINEAR BLOCKS?
                    // kddnote Mehmet says it is probably needed anyway.
                    distance =ABS(cutCoordinates_tmp[kk] - cut);
                    if(distance < _EPSILON){
                        //cout << "yo" << endl;
                        myPartWeights[2 * kk + 1] += w;

                        //cout << "2to part:" << 2*kk+1 << " coord:" << coord << endl;
                        myLeftClosest[kk] = 0;
                        myRightClosest[kk] = 0;
                        kk++;
                    }
                    else{
                        if(myLeftClosest[kk] > distance){
                            myLeftClosest[kk] = distance;
                        }
                        break;
                    }
                }

                kk = j - 1;
                while(kk >= 0){
                    distance =ABS(cutCoordinates_tmp[kk] - cut);
                    if(distance < _EPSILON){
                        myPartWeights[2 * kk + 1] += w;
                        //cout << "3to part:" << 2*kk+1 << " coord:" << coord << endl;

                        myLeftClosest[kk] = 0;
                        myRightClosest[kk] = 0;
                        kk--;
                    }
                    else{
                        if(myRightClosest[kk] > distance){
                            myRightClosest[kk] = distance;
                        }
                        break;
                    }
                }
            }
            else if (distance < 0) {
                while((absdistance > _EPSILON) && distance < 0){
                    //comp++;
                    if (myLeftClosest[j] > absdistance){
                        myLeftClosest[j] = absdistance;
                    }
                    --j;
                    if(j  < 0) break;
                    distance = coord - cutCoordinates_tmp[j];

                    absdistance = ABS(distance);
                }
                if(absdistance < _EPSILON)
                {
                    myPartWeights[j * 2 + 1] += w;
                    myLeftClosest[j] = 0;
                    myRightClosest[j] = 0;
                    cut = cutCoordinates_tmp[j];
                    //cout << "4to part:" << 2*j+1 <<" j:" << j <<  " coord:" << coord << endl;
                    //cout << "cut:" << cutCoordinates_tmp[j] << " dis:" << distance << endl;
                    partIds[i] = j * 2 + 1;

                    partId_t kk = j + 1;
                    while(kk < noCuts){  // Needed when cuts shared the same position
                        // kddnote Can this loop be disabled for RECTILINEAR BLOCKS?
                        // kddnote Mehmet says it is probably needed anyway.
                        distance =ABS(cutCoordinates_tmp[kk] - cut);
                        //cout << "distance:" << distance << endl;
                        if(distance < _EPSILON){
                            myPartWeights[2 * kk + 1] += w;
                            //cout << "5to part:" << 2*kk+1 << " kk:" << kk << " coord:" << coord << endl;
                            //cout << "cut:" << cutCoordinates_tmp[kk] << " dis:" << distance << endl;
                            myLeftClosest[kk] = 0;
                            myRightClosest[kk] = 0;
                            kk++;
                        }
                        else{
                            if(myLeftClosest[kk] > distance){
                                myLeftClosest[kk] = distance;
                            }
                            break;
                        }
                    }

                    kk = j - 1;
                    while(kk >= 0){
                        distance =ABS(cutCoordinates_tmp[kk] - cut);
                        if(distance < _EPSILON){
                            myPartWeights[2 * kk + 1] += w;
                            //cout << "6to part:" << 2*kk+1 << " coord:" << coord << endl;
                            //cout << "cut:" << cutCoordinates_tmp[kk] << " dis:" << distance << endl;

                            myLeftClosest[kk] = 0;
                            myRightClosest[kk] = 0;
                            kk--;
                        }
                        else{
                            if(myRightClosest[kk] > distance){
                                myRightClosest[kk] = distance;
                            }
                            break;
                        }
                    }
                }
                else {
                    myPartWeights[j * 2 + 2] += w;
                    if (j >= 0 && myRightClosest[j] > absdistance){
                        myRightClosest[j] = absdistance;
                    }
                    partIds[i] = j * 2 + 2;
                }

            }
            //if it is on the left
            else {

                while((absdistance > _EPSILON) && distance > 0){
                    //comp++;
                    if ( myRightClosest[j] > absdistance){
                        myRightClosest[j] = absdistance;
                    }
                    ++j;
                    if(j  >= noCuts) break;
                    distance = coord - cutCoordinates_tmp[j];
                    absdistance = ABS(distance);
                }



                if(absdistance < _EPSILON)
                {
                    myPartWeights[j * 2 + 1] += w;
                    myLeftClosest[j] = 0;
                    myRightClosest[j] = 0;
                    partIds[i] = j * 2 + 1;
                    cut = cutCoordinates_tmp[j];
                    partId_t kk = j + 1;
                    while(kk < noCuts){  // Needed when cuts shared the same position
                        // kddnote Can this loop be disabled for RECTILINEAR BLOCKS?
                        // kddnote Mehmet says it is probably needed anyway.
                        distance =ABS(cutCoordinates_tmp[kk] - cut);
                        if(distance < _EPSILON){
                            myPartWeights[2 * kk + 1] += w;
                            myLeftClosest[kk] = 0;
                            myRightClosest[kk] = 0;
                            kk++;
                        }
                        else{
                            if(myLeftClosest[kk] > distance){
                                myLeftClosest[kk] = distance;
                            }
                            break;
                        }
                    }

                    kk = j - 1;
                    while(kk >= 0){
                        distance =ABS(cutCoordinates_tmp[kk] - cut);
                        if(distance < _EPSILON){
                            myPartWeights[2 * kk + 1] += w;

                            myLeftClosest[kk] = 0;
                            myRightClosest[kk] = 0;
                            kk--;
                        }
                        else{
                            if(myRightClosest[kk] > distance){
                                myRightClosest[kk] = distance;
                            }
                            break;
                        }
                    }
                }
                else {
                    myPartWeights[j * 2] += w;
                    if (j < noCuts && myLeftClosest[j] > absdistance){
                        myLeftClosest[j] = absdistance;
                    }
                    partIds[i] = j * 2 ;
                }
            }
        }

        /*
#ifdef HAVE_ZOLTAN2_OMP
#pragma omp for
#endif
for (lno_t ii = coordinateBegin; ii < coordinateEnd; ++ii){
int i = partitionedPointPermutations[ii];

scalar_t w = pqJagged_uniformWeights? 1:pqJagged_weights[i];
//get a coordinate and compare it with cut lines from left to right.
for(partId_t j = 0; j < noCuts; ++j){

if(isDone[j]) continue;
scalar_t distance = pqJagged_coordinates[i] - cutCoordinates_tmp[j];
scalar_t absdistance = ABS(distance);
if(absdistance < _EPSILON){
myPartWeights[j * 2] -= w;
//myPartWeights[j * 2] += w;
myLeftClosest[j] = 0;
myRightClosest[j] = 0;
//break;
}
else
if (distance < 0) {
distance = -distance;
if (myLeftClosest[j] > distance){
myLeftClosest[j] = distance;
}
break;
}
//if it is on the left
else {
  //if on the right, continue with the next line.
  myPartWeights[j * 2] -= w;
  myPartWeights[j * 2 + 1] -= w;
  //myPartWeights[j * 2] += w;
  //myPartWeights[j * 2 + 1] += w;

  if ( myRightClosest[j] > distance){
  myRightClosest[j] = distance;
  }
  }
  }
  }
         */

    }

    // prefix sum computation.
    for (size_t i = 1; i < total_part_count; ++i){
        // if check for cuts sharing the same position; all cuts sharing a position
        // have the same weight == total weight for all cuts sharing the position.
        // don't want to accumulate that total weight more than once.
        if(i % 2 == 0 && i > 1 && i < total_part_count - 1 &&
                ABS(cutCoordinates_tmp[i / 2] - cutCoordinates_tmp[i /2 - 1]) < _EPSILON){
            myPartWeights[i] = myPartWeights[i-2];
            continue;
        }
        myPartWeights[i] += myPartWeights[i-1];

    }
}


/*! \brief Function that reduces the result of multiple threads for left and right closest points and part weights in a single mpi.
 *
 * \param noCuts is the number of cut lines. P - 1.
 * \param total_part_count is the sum of number of cutlines and number of parts. Simply it is 2*P - 1.
 * \param concurrentPartCount is the number of parts whose cut lines will be calculated concurrently.
 * \param numThreads hold the number of threads available per processor.
 * \param isDone is the boolean array to determine if the correct position for a cut line is found.
 * \param leftClosestDistance is the 2 dimensional array holding the distances to the closest points to the cut lines from left for each thread.
 * \param rightClosestDistance is the 2 dimensional array holding the distances to the closest points to the cut lines from right for each thread.
 * \param partWeights is the array holding the weight of parts for each thread. Assumes there are 2*P - 1 parts (cut lines are seperate parts).
 * \param localMinMaxTotal is the array holding the local minimum and maximum coordinate and local total weight of each part.
 * \param totalPartWeights_leftClosest_rightCloset is the output array of accumulation, where total part weights (2P - 1),
 * then left closest distances (P -1), then right closest distance (P -1) are stored.
 */
template <typename scalar_t>
void accumulateThreadResults(
#ifdef omitted
        const partId_t *partNoArray,

#endif
        const vector <partId_t> &pVector,
        partId_t vBegin,

        partId_t concurrentPartCount,
        int numThreads,
        bool *isDone,
        scalar_t **leftClosestDistance,
        scalar_t **rightClosestDistance,
        double **partWeights,
        scalar_t *localMinMaxTotal,
        scalar_t *totalPartWeights_leftClosest_rightCloset

){
#ifdef omitted
    if (0 && partNoArray){

        partId_t partNo =  pVector[0];
        partId_t noCuts = partNo - 1;
        size_t total_part_count = partNo + size_t (noCuts) ;
#ifdef HAVE_ZOLTAN2_OMP
#pragma omp for
#endif
        for(partId_t i = 0; i < noCuts * concurrentPartCount; ++i){
            if(isDone[i]) continue;
            scalar_t minl = leftClosestDistance[0][i], minr = rightClosestDistance[0][i];

            for (int j = 1; j < numThreads; ++j){
                if (rightClosestDistance[j][i] < minr ){
                    minr = rightClosestDistance[j][i];
                }
                if (leftClosestDistance[j][i] < minl ){
                    minl = leftClosestDistance[j][i];
                }
            }
            size_t ishift = i % noCuts + (i / noCuts) * (total_part_count + 2 * noCuts);
            totalPartWeights_leftClosest_rightCloset[total_part_count + ishift] = minl;
            totalPartWeights_leftClosest_rightCloset[total_part_count + noCuts + ishift] = minr;
        }

        // if(1 || useBinarySearch){
#ifdef HAVE_ZOLTAN2_OMP
#pragma omp for
#endif
        for(size_t j = 0; j < total_part_count * concurrentPartCount; ++j){
            size_t actualCutInd = (j % total_part_count);
            partId_t cutInd = actualCutInd / 2 + (j / total_part_count) * noCuts;

            if(actualCutInd !=  total_part_count - 1 && isDone[cutInd]) continue;
            double pwj = 0;
            for (int i = 0; i < numThreads; ++i){
                pwj += partWeights[i][j];
            }
            size_t jshift = j % total_part_count + (j / total_part_count) * (total_part_count + 2 * noCuts);
            totalPartWeights_leftClosest_rightCloset[jshift] = pwj;
        }
    }
    else {
#endif
#ifdef HAVE_ZOLTAN2_OMP
#pragma omp barrier
#pragma omp single
#endif
        {
            size_t tlr_shift = 0;
            partId_t cut_shift = 0;
            for(partId_t i = 0; i < concurrentPartCount; ++i){

                partId_t partNo =  pVector[vBegin + i];
                partId_t noCuts = partNo - 1;
                size_t total_part_count = partNo + size_t (noCuts) ;

                for(partId_t ii = 0; ii < noCuts ; ++ii){
                    partId_t next = tlr_shift + ii;
                    partId_t nCut = cut_shift + ii;
                    if(isDone[nCut]) continue;
                    scalar_t minl = leftClosestDistance[0][nCut], minr = rightClosestDistance[0][nCut];

                    for (int j = 1; j < numThreads; ++j){
                        if (rightClosestDistance[j][nCut] < minr ){
                            minr = rightClosestDistance[j][nCut];
                        }
                        if (leftClosestDistance[j][nCut] < minl ){
                            minl = leftClosestDistance[j][nCut];
                        }
                    }
                    totalPartWeights_leftClosest_rightCloset[total_part_count + next] = minl;
                    totalPartWeights_leftClosest_rightCloset[total_part_count + noCuts + next] = minr;
                }
                tlr_shift += (total_part_count + 2 * noCuts);
                cut_shift += noCuts;
            }

            tlr_shift = 0;
            cut_shift = 0;
            size_t totalPartShift = 0;

            for(partId_t i = 0; i < concurrentPartCount; ++i){

                partId_t partNo =  pVector[vBegin + i];
                partId_t noCuts = partNo - 1;
                size_t total_part_count = partNo + size_t (noCuts) ;

                for(size_t j = 0; j < total_part_count; ++j){

                    partId_t cutInd = j / 2 + cut_shift;

                    if(j !=  total_part_count - 1 && isDone[cutInd]) continue;
                    double pwj = 0;
                    for (int k = 0; k < numThreads; ++k){
                        pwj += partWeights[k][totalPartShift + j];
                    }
                    //size_t jshift = j % total_part_count + i * (total_part_count + 2 * noCuts);
                    totalPartWeights_leftClosest_rightCloset[tlr_shift + j] = pwj;
                }
                cut_shift += noCuts;
                tlr_shift += total_part_count + 2 * noCuts;
                totalPartShift += total_part_count;
            }
        }
#ifdef HAVE_ZOLTAN2_OMP
#pragma omp barrier
#endif
#ifdef omitted
    }
#endif
}




/*! \brief Function that is responsible from 1D partitioning of the given range of coordinates.
 * \param env library configuration and problem parameters
 * \param comm the communicator for the problem
 *
 * \param partitionedPointPermutations is the indices of coordinates in the given partition.
 * \param pqJagged_coordinates is 1 dimensional array holding coordinate values.
 * \param pqJagged_uniformWeights is a boolean value if the points have uniform weights.
 * \param pqJagged_weights is 1 dimensional array holding the weights of points.
 *
 * \param targetPartWeightRatios is the cumulative desired weight ratios for each part. Sized P.
 * \param globalMinMaxTotal is the array holding the global min,max and total weight.
 * minimum of part k is on k, maximum is on k + concurrentPartcount, totalweight is on k + 2*concurrentPartCount
 * \param localMinMaxTotal is the array holding the local min,max and total weight. Structure is same with globalMinMaxTotal array.
 *
 * \param partNo is the total number of parts.
 * \param numThreads is the number of available threads by each processor.
 * \param maxScalar is the maximum value that scalar_t can represent.
 * \param minScalar is the minimum value that scalar_t can represent.
 * \param imbalanceTolerance is the maximum allowed imbalance ratio.
 * \param currentPartBeginIndex is the beginning index of concurrentPartCount parts on inTotalCounts array.
 * \param concurrentPartCount is the number of parts whose cut lines will be calculated concurrently.
 * \param inTotalCounts is the array holding the beginning indices of the parts from previous dimension partitioning.
 *
 * \param cutCoordinates is the array holding the coordinates of the cut.
 * \param cutCoordinatesWork is a work array sized P - 1.
 * \param leftClosestDistance is the two dimensional array holding the distances to the closest points to the cut lines from left. One dimension for each thread.
 * \param rightClosestDistance is the two dimensional array holding the distances to the closest points to the cut lines from right. One dimension for each thread.
 * \param cutUpperBounds is the array holding the upper bound coordinate for each cut line. Sized P - 1.
 * \param cutLowerBounds is the array holding the lower bound coordinate for each cut line. Sized P - 1.
 * \param cutUpperWeight is the array holding the weight of the parts at the left of upper bound coordinates.
 * \param cutLowerWeight is the array holding the weight of the parts at the left of lower bound coordinates.
 * \param isDone is the boolean array to determine if the correct position for a cut line is found.
 * \param partWeights is the two dimensional array holding the part weights. One dimension for each thread.
 * \param local_totalPartWeights_leftClosest_rightCloset is one dimensional array holding the local part weights, left and right closest points. Convenient for mpi-reduction.
 * \param global_totalPartWeights_leftClosest_rightCloset is one dimensional array holding the local part weights, left and right closest points. Convenient for mpi-reduction.
 * \param allowNonRectelinearPart is whether to allow distribution of points on same coordinate to different parts or not.
 * \param nonRectelinearPartRatios is one dimensional array holding the ratio of the points on cut lines representing how many of them will be put to left of the line.
 * \param localCutWeights is one dimensional array holding the local part weights only on cut lines.
 * \param globalCutWeights is one dimensional array holding the global part weights only on cut lines.
 * \param allDone is the number of cut lines whose positions should be calculated.
 * \param myNonDoneCounts is one dimensional array holding the number of cut lines in each part whose positions should be calculated.
 * \param useBinarySearch is boolean parameter whether to search for cut lines with binary search of linear search.
 * \param partIds is the array that holds the part ids of the coordinates
 *
 */
template <typename scalar_t, typename lno_t>
void pqJagged_1D_Partition(
        const RCP<const Environment> &env,
        RCP<Comm<int> > &comm,

        lno_t *partitionedPointPermutations,
        scalar_t *pqJagged_coordinates,
        bool pqJagged_uniformWeights,
        scalar_t *pqJagged_weights,

        scalar_t *targetPartWeightRatios,   // the weight ratios at left side of the cuts. last is 1.
        scalar_t *globalMinMaxTotal,
        scalar_t *localMinMaxTotal,

        int numThreads,
        scalar_t maxScalar,
        scalar_t minScalar,
        scalar_t imbalanceTolerance,
        partId_t currentPartBeginIndex,
        partId_t concurrentPartCount,
        lno_t *inTotalCounts,

        scalar_t *cutCoordinates,
        scalar_t *cutCoordinatesWork, 	// work array to manipulate coordinate of cutlines in different iterations.
        scalar_t **leftClosestDistance,
        scalar_t **rightClosestDistance,
        scalar_t *cutUpperBounds,  //to determine the next cut line with binary search
        scalar_t *cutLowerBounds,  //to determine the next cut line with binary search
        scalar_t *cutUpperWeight,   //to determine the next cut line with binary search
        scalar_t *cutLowerWeight,  //to determine the next cut line with binary search
        bool *isDone,
        double **partWeights,
        scalar_t *local_totalPartWeights_leftClosest_rightCloset,
        scalar_t *global_totalPartWeights_leftClosest_rightCloset,
        bool allowNonRectelinearPart,
        float *nonRectelinearPartRatios,
        scalar_t *localCutWeights,
        scalar_t *globalCutWeights,

        partId_t allDone,
        partId_t *myNonDoneCounts,
        bool useBinarySearch,

        partId_t * partIds,
#ifdef omitted
        const partId_t *partNoArray,
#endif
        vector <partId_t> &pVector
){

    partId_t recteLinearCutCount = 0;
    scalar_t *cutCoordinates_tmp = cutCoordinates;


#ifdef mpi_communication
    MPI_Op myop;
    MPI_Op_create(sumMinMin, 0, &myop);   /* step 3 */
#endif


    scalar_t _EPSILON = numeric_limits<scalar_t>::epsilon();

    Teuchos::PQJaggedCombinedReductionOp<partId_t, scalar_t> *reductionOp = NULL;
#ifdef omitted
    if (partNoArray){

        partId_t partNo =  pVector[0];
        partId_t noCuts = partNo - 1;
        size_t total_part_count = partNo + size_t (noCuts) ;
        reductionOp = new Teuchos::PQJaggedCombinedReductionOp<partId_t, scalar_t>(total_part_count , noCuts  , noCuts , concurrentPartCount);
    }
    else {
#endif
        reductionOp = new Teuchos::PQJaggedCombinedReductionOp<partId_t, scalar_t>(&pVector , currentPartBeginIndex , concurrentPartCount);
#ifdef omitted
    }
#endif


    size_t totalReductionSize = 0;
#ifdef HAVE_ZOLTAN2_OMP
#pragma omp parallel shared(allDone,  recteLinearCutCount)
#endif
    {
        int me = 0;
#ifdef HAVE_ZOLTAN2_OMP
        me = omp_get_thread_num();
#endif
        double *myPartWeights = partWeights[me];
        scalar_t *myLeftClosest = leftClosestDistance[me];
        scalar_t *myRightClosest = rightClosestDistance[me];
#ifdef omitted
        if(0 && partNoArray){

            partId_t partNo =  pVector[0];
            partId_t noCuts = partNo - 1;
            totalReductionSize = (4 * noCuts + 1) * concurrentPartCount;
            //cout << "kk:" << " partition - 4:" << partNo << endl;
#ifdef HAVE_ZOLTAN2_OMP
#pragma omp for
#endif
            for(partId_t i = 0; i < noCuts * concurrentPartCount; ++i){
                isDone[i] = false;
                partId_t ind = i / noCuts;
                cutLowerBounds[i] = globalMinMaxTotal[ind];
                cutUpperBounds[i] = globalMinMaxTotal[ind + concurrentPartCount];
                cutUpperWeight[i] = globalMinMaxTotal[ind + 2 * concurrentPartCount];
                cutLowerWeight[i] = 0;
                if(allowNonRectelinearPart){
                    nonRectelinearPartRatios[i] = 0;
                }
            }
        }
        else {
#endif

#ifdef HAVE_ZOLTAN2_OMP
#pragma omp single
#endif
            {
                //initialize the lower and upper bounds of the cuts.
                partId_t next = 0;
                for(partId_t i = 0; i < concurrentPartCount; ++i){

                    partId_t partNo =  pVector[currentPartBeginIndex + i];
                    partId_t noCuts = partNo - 1;
                    totalReductionSize += (4 * noCuts + 1);

                    for(partId_t ii = 0; ii < noCuts; ++ii){
                        isDone[next] = false;
                        cutLowerBounds[next] = globalMinMaxTotal[i]; //min coordinate
                        cutUpperBounds[next] = globalMinMaxTotal[i + concurrentPartCount]; //max coordinate

                        cutUpperWeight[next] = globalMinMaxTotal[i + 2 * concurrentPartCount]; //total weight
                        cutLowerWeight[next] = 0;

                        if(allowNonRectelinearPart){
                            nonRectelinearPartRatios[next] = 0;
                        }
                        ++next;
                    }
                }
            }
#ifdef omitted
        }
#endif

#ifdef HAVE_ZOLTAN2_OMP
#pragma omp barrier
#endif
        int iteration = 0;
        while (allDone != 0){
            iteration += 1;
            partId_t cutShifts = 0;
            size_t totalPartShift = 0;


            for (partId_t kk = 0; kk < concurrentPartCount; ++kk){
                partId_t partNo =  -1;
#ifdef omitted
                if (partNoArray){
                    partNo =  pVector[0];
                }
                else {
#endif
                    partNo =  pVector[currentPartBeginIndex + kk];
#ifdef omitted
                }
#endif

                partId_t noCuts = partNo - 1;
                size_t total_part_count = partNo + size_t (noCuts) ;
                if (myNonDoneCounts[kk] > 0){

                    //although isDone shared, currentDone is private and same for all.
                    bool *currentDone = isDone + cutShifts;
                    double *myCurrentPartWeights = myPartWeights + totalPartShift;
                    scalar_t *myCurrentLeftClosest = myLeftClosest + cutShifts;
                    scalar_t *myCurrentRightClosest = myRightClosest + cutShifts;

                    partId_t current = currentPartBeginIndex + kk;
                    lno_t coordinateBegin = current ==0 ? 0: inTotalCounts[current -1];
                    lno_t coordinateEnd = inTotalCounts[current];
                    scalar_t *cutCoordinates_ = cutCoordinates_tmp + cutShifts;

                    // compute part weights using existing cuts
                    pqJagged_1DPart_getPartWeights<scalar_t, lno_t>(
                            total_part_count,
                            noCuts,
                            maxScalar,
                            _EPSILON,
                            numThreads,
                            coordinateBegin,
                            coordinateEnd,
                            partitionedPointPermutations,
                            pqJagged_coordinates,
                            pqJagged_uniformWeights,
                            pqJagged_weights,
                            cutCoordinates_,
                            currentDone,
                            myCurrentPartWeights,
                            myCurrentLeftClosest,
                            myCurrentRightClosest,
                            useBinarySearch,
                            partIds);
                }

                cutShifts += noCuts;
                totalPartShift += total_part_count;
            }

            //sum up the results of threads
            accumulateThreadResults<scalar_t>(
#ifdef omitted
                    partNoArray,
#endif
                    pVector,
                    currentPartBeginIndex,
                    concurrentPartCount,
                    numThreads, isDone,
                    leftClosestDistance, rightClosestDistance, partWeights,
                    localMinMaxTotal,
                    local_totalPartWeights_leftClosest_rightCloset
            );

            //now sum up the results of mpi processors.
#ifdef HAVE_ZOLTAN2_OMP
#pragma omp single
#endif
            {
                if(comm->getSize() > 1){
                    try{
#ifdef mpi_communication

                        MPI_Allreduce(local_totalPartWeights_leftClosest_rightCloset, global_totalPartWeights_leftClosest_rightCloset,

                                (total_part_count + 2 * noCuts) * concurrentPartCount, MPI_FLOAT, myop,MPI_COMM_WORLD);
#endif
#ifndef mpi_communication

                        reduceAll<int, scalar_t>(
                                *comm,
                                *reductionOp,
                                totalReductionSize,
                                local_totalPartWeights_leftClosest_rightCloset,
                                global_totalPartWeights_leftClosest_rightCloset
                        );
#endif
                    }
                    Z2_THROW_OUTSIDE_ERROR(*env)
                }
                else {
#ifdef omitted
                    if(partNoArray){

                        partId_t partNo =  pVector[0];
                        partId_t noCuts = partNo - 1;
                        size_t total_part_count = partNo + size_t (noCuts) ;
                        size_t s = (total_part_count + 2 * noCuts) * concurrentPartCount;

                        for(size_t i = 0; i < s; ++i){
                            global_totalPartWeights_leftClosest_rightCloset[i] = local_totalPartWeights_leftClosest_rightCloset[i];
                        }

                    }
                    else {
#endif
                        memcpy(global_totalPartWeights_leftClosest_rightCloset,
                                local_totalPartWeights_leftClosest_rightCloset,
                                totalReductionSize * sizeof(scalar_t)
                        );
#ifdef omitted
                        /*
                        size_t next = 0;
                        for(partId_t i = 0; i < concurrentPartCount; ++i){
                            partId_t partNo =  pVector[currentPartBeginIndex + i];
                            partId_t noCuts = partNo - 1;
                            size_t total_part_count = partNo + size_t (noCuts) ;
                            size_t s = (total_part_count + 2 * noCuts);

                            for(size_t j = 0; j < s; ++j){
                                global_totalPartWeights_leftClosest_rightCloset[j + next] =
                                        local_totalPartWeights_leftClosest_rightCloset[j + next];
                            }
                            next += s;
                        }
                        */
#endif
#ifdef omitted
                    }
#endif
                }
            }
            partId_t cutShift = 0;
            size_t tlrShift = 0;
            for (partId_t kk = 0; kk < concurrentPartCount; ++kk){
                partId_t partNo =  -1;
#ifdef omitted
                if(partNoArray){
                    partNo = pVector[0];
                }
                else {
#endif
                    partNo = pVector[currentPartBeginIndex + kk];
#ifdef omitted
                }
#endif

                partId_t noCuts = partNo - 1;
                size_t total_part_count = partNo + size_t (noCuts) ;

                if (myNonDoneCounts[kk] == 0) {
                    cutShift += noCuts;
                    tlrShift += (total_part_count + 2 * noCuts);
                    continue;
                }

                scalar_t *localPartWeights = local_totalPartWeights_leftClosest_rightCloset  + tlrShift ;
                scalar_t *gtlr = global_totalPartWeights_leftClosest_rightCloset + tlrShift;
                scalar_t *glc = gtlr + total_part_count; //left closest points
                scalar_t *grc = gtlr + total_part_count + noCuts; //right closest points
                scalar_t *globalPartWeights = gtlr;
                bool *currentDone = isDone + cutShift;

                scalar_t *currentTargetPartWeightRatios = targetPartWeightRatios + cutShift + kk;
                float *currentnonRectelinearPartRatios = nonRectelinearPartRatios + cutShift;

                scalar_t minCoordinate = globalMinMaxTotal[kk];
                scalar_t maxCoordinate = globalMinMaxTotal[kk + concurrentPartCount];
                scalar_t globalTotalWeight = globalMinMaxTotal[kk + concurrentPartCount * 2];
                scalar_t *currentcutLowerWeight = cutLowerWeight + cutShift;
                scalar_t *currentcutUpperWeight = cutUpperWeight + cutShift;
                scalar_t *currentcutUpperBounds = cutUpperBounds + cutShift;
                scalar_t *currentcutLowerBounds = cutLowerBounds + cutShift;

                partId_t prevDoneCount = myNonDoneCounts[kk];

                // Now compute the new cut coordinates.
                getNewCoordinates<scalar_t>(
                        env,
                        comm,
                        total_part_count,
                        noCuts,
                        maxCoordinate,
                        minCoordinate,
                        globalTotalWeight,
                        imbalanceTolerance,
                        maxScalar,
                        globalPartWeights,
                        localPartWeights,
                        currentTargetPartWeightRatios,
                        currentDone,

                        cutCoordinates_tmp + cutShift,
                        currentcutUpperBounds,
                        currentcutLowerBounds,
                        glc,
                        grc,
                        currentcutLowerWeight,
                        currentcutUpperWeight,
                        cutCoordinatesWork +cutShift, //new cut coordinates

                        allowNonRectelinearPart,
                        currentnonRectelinearPartRatios,
                        &recteLinearCutCount,
                        localCutWeights,
                        globalCutWeights,
                        myNonDoneCounts[kk]
                );

                cutShift += noCuts;
                tlrShift += (total_part_count + 2 * noCuts);
                partId_t reduction = prevDoneCount - myNonDoneCounts[kk];
#ifdef HAVE_ZOLTAN2_OMP
#pragma omp single
#endif
                {
                    allDone -= reduction;
                }

            }
#ifdef HAVE_ZOLTAN2_OMP
#pragma omp single
#endif
            {
                scalar_t *t = cutCoordinates_tmp;
                cutCoordinates_tmp = cutCoordinatesWork;
                cutCoordinatesWork = t;
            }
        }
        /*
   if(comm->getRank() == 0)
   if(me == 0) cout << "it:" << iteration << endl;
         */
        // Needed only if keep_cuts; otherwise can simply swap array pointers
        // cutCoordinates and cutCoordinatesWork.
        // (at first iteration, cutCoordinates == cutCoorindates_tmp).
        // computed cuts must be in cutCoordinates.
        if (cutCoordinates != cutCoordinates_tmp){
#ifdef HAVE_ZOLTAN2_OMP
#pragma omp single
#endif
            {
                partId_t next = 0;
                for(partId_t i = 0; i < concurrentPartCount; ++i){
                    partId_t partNo = -1;
#ifdef omitted
                    if(partNoArray){
                        partNo = pVector[0];
                    }
                    else {
#endif
                        partNo = pVector[currentPartBeginIndex + i];
#ifdef omitted
                    }
#endif
                    //cout << "kk:" << " partition - 5:" << partNo << endl;
                    partId_t noCuts = partNo - 1;
                    for(partId_t ii = 0; ii < noCuts; ++ii){
                        cutCoordinates[next + ii] = cutCoordinates_tmp[next + ii];
                    }
                    next += noCuts;
                }
            }

#ifdef HAVE_ZOLTAN2_OMP
#pragma omp single
#endif
            {
                cutCoordinatesWork = cutCoordinates_tmp;
            }
        }
    }
    delete reductionOp;
}





/*! \brief Function that determines the permutation indices of the coordinates.
 * \param partNo is the number of parts.
 * \param noThreads is the number of threads avaiable for each processor.
 *
 * \param partitionedPointPermutations is the indices of coordinates in the given partition.
 * \param pqJagged_coordinates is 1 dimensional array holding the coordinate values.
 * \param pqJagged_uniformWeights is a boolean value if the points have uniform weights.
 * \param pqJagged_weights is 1 dimensional array holding the weights of points.
 *
 * \param cutCoordinates is 1 dimensional array holding the cut coordinates.
 * \param coordinateBegin is the start index of the given partition on partitionedPointPermutations.
 * \param coordinateEnd is the end index of the given partition on partitionedPointPermutations.
 * \param numLocalCoord is the number of local coordinates.
 *
 * \param allowNonRectelinearPart is the boolean value whether partitioning should allow distributing the points on same coordinate to different parts.
 * \param actual_ratios holds how much percentage of the coordinates on the cutline should be put on left side.
 * \param localPartWeights is the local totalweight of the processor.
 * \param partWeights is the two dimensional array holding the weight of parts for each thread. Assumes there are 2*P - 1 parts (cut lines are seperate parts).
 * \param nonRectelinearRatios is the two dimensional work array holding ratios of weights to be put left and right of the cut line.
 * \param partPointCounts is the two dimensional array holding the number of points in each part for each thread.
 *
 * \param newpartitionedPointPermutations is the indices of coordinates calculated for the partition on next dimension.
 * \param totalCounts are the number points in each output part.
 * \param partIds is the array that holds the part ids of the coordinates
 */
template <typename lno_t, typename scalar_t>
void getChunksFromCoordinates(
        partId_t partNo,
        int noThreads,

        lno_t *partitionedPointPermutations,
        scalar_t *pqJagged_coordinates,
        bool pqJagged_uniformWeights,
        scalar_t *coordWeights,
        scalar_t *cutCoordinates,
        lno_t coordinateBegin,
        lno_t coordinateEnd,

        bool allowNonRectelinearPart,
        float *actual_ratios,
        scalar_t *localPartWeights,
        double **partWeights,
        float **nonRectelinearRatios,

        lno_t ** partPointCounts,

        lno_t *newpartitionedPointPermutations,
        lno_t *totalCounts,
        partId_t *partIds
        ,bool migration_check
){

    //lno_t numCoordsInPart =  coordinateEnd - coordinateBegin;
    partId_t noCuts = partNo - 1;
    //size_t total_part_count = noCuts + partNo;
    scalar_t _EPSILON = numeric_limits<scalar_t>::epsilon();

    if (migration_check == true) allowNonRectelinearPart = false;
#ifdef HAVE_ZOLTAN2_OMP
#pragma omp parallel
#endif
    {
        int me = 0;
#ifdef HAVE_ZOLTAN2_OMP
        me = omp_get_thread_num();
#endif

        lno_t *myPartPointCounts = partPointCounts[me];
        float *myRatios = NULL;
        if (allowNonRectelinearPart){


            myRatios = nonRectelinearRatios[me];
#ifdef HAVE_ZOLTAN2_OMP
#pragma omp for
#endif
            for (partId_t i = 0; i < noCuts; ++i){
                float r = actual_ratios[i];
                scalar_t leftWeight = r * (localPartWeights[i * 2 + 1] - localPartWeights[i * 2]);
                for(int ii = 0; ii < noThreads; ++ii){
                    if(leftWeight > _EPSILON){

                        scalar_t ithWeight = partWeights[ii][i * 2 + 1] - partWeights[ii][i * 2 ];
                        if(ithWeight < leftWeight){
                            nonRectelinearRatios[ii][i] = ithWeight;
                        }
                        else {
                            nonRectelinearRatios[ii][i] = leftWeight ;
                        }
                        leftWeight -= ithWeight;
                    }
                    else {
                        nonRectelinearRatios[ii][i] = 0;
                    }
                }
            }


            if(noCuts > 0){
                for (partId_t i = noCuts - 1; i > 0 ; --i){          if(ABS(cutCoordinates[i] - cutCoordinates[i -1]) < _EPSILON){
                    myRatios[i] -= myRatios[i - 1] ;
                }
                myRatios[i] = int ((myRatios[i] + LEAST_SIGNIFICANCE) * SIGNIFICANCE_MUL) / scalar_t(SIGNIFICANCE_MUL);
                }
            }
        }

        for(partId_t ii = 0; ii < partNo; ++ii){
            myPartPointCounts[ii] = 0;
        }


#ifdef HAVE_ZOLTAN2_OMP
#pragma omp barrier
#pragma omp for
#endif
        for (lno_t ii = coordinateBegin; ii < coordinateEnd; ++ii){

            lno_t i = partitionedPointPermutations[ii];
            partId_t pp = partIds[i];
            partId_t p = pp / 2;
            if(pp % 2 == 1){
                if(allowNonRectelinearPart && myRatios[p] > _EPSILON * EPS_SCALE){
                    //cout << "pa:" << p << endl;
                    scalar_t w = pqJagged_uniformWeights? 1:coordWeights[i];
                    myRatios[p] -= w;
                    if(myRatios[p] < 0 && p < noCuts - 1 && ABS(cutCoordinates[p+1] - cutCoordinates[p]) < _EPSILON){
                        myRatios[p + 1] += myRatios[p];
                    }
                    ++myPartPointCounts[p];
                    partIds[i] = p;
                }
                else{
                    //scalar_t currentCut = cutCoordinates[p];
                    //TODO:currently cannot divide 1 line more than 2 parts.
                    //bug cannot be divided, therefore this part should change.
                    //cout << "p:" << p+1 << endl;
                    ++myPartPointCounts[p + 1];
                    partIds[i] = p + 1;
                }
            }
            else {
                ++myPartPointCounts[p];
                partIds[i] = p;
            }
        }



#ifdef HAVE_ZOLTAN2_OMP
#pragma omp for
#endif
        for(partId_t j = 0; j < partNo; ++j){
            lno_t pwj = 0;
            for (int i = 0; i < noThreads; ++i){
                lno_t threadPartPointCount = partPointCounts[i][j];
                partPointCounts[i][j] = pwj;
                pwj += threadPartPointCount;

            }
            totalCounts[j] = pwj;// + prev2; //+ coordinateBegin;
        }

#ifdef HAVE_ZOLTAN2_OMP
#pragma omp single
#endif
        {
            for(partId_t j = 1; j < partNo; ++j){
                totalCounts[j] += totalCounts[j - 1];
            }
        }

        for(partId_t j = 1; j < partNo; ++j){
            myPartPointCounts[j] += totalCounts[j - 1] ;
        }


#ifdef HAVE_ZOLTAN2_OMP
#pragma omp for
#endif
        for (lno_t ii = coordinateBegin; ii < coordinateEnd; ++ii){
            lno_t i = partitionedPointPermutations[ii];
            partId_t p =  partIds[i];
            newpartitionedPointPermutations[coordinateBegin + myPartPointCounts[p]++] = i;
        }
    }
}


template <typename T>
T *allocMemory(size_t size){
    if (size > 0){
        T * a = new T[size];
        if (a == NULL) {
            throw  "cannot allocate memory";
        }
        return a;
    }
    else {
        return NULL;
    }
}

template <typename T>
void freeArray(T *&array){
    if(array != NULL){
        delete [] array;
        array = NULL;
    }
}

template <typename tt>
std::string toString(tt obj){
    std::stringstream ss (std::stringstream::in |std::stringstream::out);
    ss << obj;
    std::string tmp = "";
    ss >> tmp;
    return tmp;
}

#ifdef enable_migration


template <class IT, class WT>
struct uSortItem
{
    IT id;
    //unsigned int val;
    WT val;
};// uSortItem;




template <class IT, class WT>
void uqsort(IT n, uSortItem<IT, WT> * arr)
{
#define SWAP(a,b,temp) temp=(a);(a)=(b);(b)=temp;
    int NSTACK = 50;
    int M = 7;
    IT         i, ir=n, j, k, l=1;
    IT         jstack=0, istack[50];
    WT aval;
    uSortItem<IT,WT>    a, temp;

    --arr;
    for (;;)
    {
        if (ir-l < M)
        {
            for (j=l+1;j<=ir;j++)
            {
                a=arr[j];
                aval = a.val;
                for (i=j-1;i>=1;i--)
                {
                    if (arr[i].val <= aval)
                        break;
                    arr[i+1] = arr[i];
                }
                arr[i+1]=a;
            }
            if (jstack == 0)
                break;
            ir=istack[jstack--];
            l=istack[jstack--];
        }
        else
        {
            k=(l+ir) >> 1;
            SWAP(arr[k],arr[l+1], temp)
            if (arr[l+1].val > arr[ir].val)
            {
                SWAP(arr[l+1],arr[ir],temp)
            }
            if (arr[l].val > arr[ir].val)
            {
                SWAP(arr[l],arr[ir],temp)
            }
            if (arr[l+1].val > arr[l].val)
            {
                SWAP(arr[l+1],arr[l],temp)
            }
            i=l+1;
            j=ir;
            a=arr[l];
            aval = a.val;
            for (;;)
            {
                do i++; while (arr[i].val < aval);
                do j--; while (arr[j].val > aval);
                if (j < i) break;
                SWAP(arr[i],arr[j],temp);
            }
            arr[l]=arr[j];
            arr[j]=a;
            jstack += 2;
            if (jstack > NSTACK){
                cout << "uqsort: NSTACK too small in sort." << endl;
                exit(1);
            }
            if (ir-i+1 >= j-l)
            {
                istack[jstack]=ir;
                istack[jstack-1]=i;
                ir=j-1;
            }
            else
            {
                istack[jstack]=j-1;
                istack[jstack-1]=l;
                l=i;
            }
        }
    }
}
template <typename gno_t, typename lno_t,typename scalar_t, typename node_t>
RCP<const Tpetra::MultiVector<scalar_t, lno_t, gno_t, node_t> > create_initial_multi_vector(
        const RCP<const Environment> &env,
        RCP<Comm<int> > &comm,
        gno_t numGlobalPoints,
        lno_t numLocalPoints,
        int coord_dim,
        scalar_t **coords,
        int weight_dim,
        scalar_t **weight,
#ifdef migrate_gid
        scalar_t *mappedGnos,
#endif
        int pqJagged_multiVectorDim
){
    typedef Tpetra::MultiVector<scalar_t, lno_t, gno_t, node_t> tMVector_t;
#ifdef memory_debug
    sleep(1); env->memory("initial multivector before map");
#endif
    RCP<Tpetra::Map<lno_t, gno_t, node_t> > mp = rcp(
            new Tpetra::Map<lno_t, gno_t, node_t> (numGlobalPoints, numLocalPoints, 0, comm));


#ifdef memory_debug
    if(comm->getRank() == 0) cout << "nn:"<< numLocalPoints<<"created map:" << mp.getRawPtr() << endl;
    sleep(1); env->memory("initial multivector after map");
#endif
    Teuchos::Array<Teuchos::ArrayView<const scalar_t> > coordView(pqJagged_multiVectorDim);
    for (int i=0; i < coord_dim; i++){
        //cout << "setting i:" << i << endl;
        if(numLocalPoints > 0){
            Teuchos::ArrayView<const scalar_t> a(coords[i], numLocalPoints);
            coordView[i] = a;
        } else{
            Teuchos::ArrayView<const scalar_t> a;
            coordView[i] = a;
        }
#ifdef memory_debug
        sleep(1); env->memory("initial multivector after coordinates -" + toString<int>(i));
#endif
    }
#ifdef memory_debug
    sleep(1); env->memory("initial multivector after coordinates");
#endif
    for (int i=0;  i < weight_dim; ++i){
        int j = i + coord_dim;
        if (j >= pqJagged_multiVectorDim - 1) break;
        //cout << "setting j:" << j << endl;
        if(numLocalPoints > 0){
            Teuchos::ArrayView<const scalar_t> a(weight[i], numLocalPoints);
            coordView[j] = a;
        } else{
            Teuchos::ArrayView<const scalar_t> a;
            coordView[j] = a;
        }
    }

#ifdef memory_debug
    sleep(1); env->memory("initial multivector after weights");
#endif
#ifdef migrate_gid
    if(numLocalPoints > 0){
        //cout << "writing :" << weight_dim + coord_dim << endl;
        Teuchos::ArrayView<const scalar_t> a(mappedGnos, numLocalPoints);
        coordView[pqJagged_multiVectorDim - 1] = a;
    } else{
        Teuchos::ArrayView<const scalar_t> a;
        coordView[pqJagged_multiVectorDim - 1] = a;
    }
#endif
    RCP< Tpetra::MultiVector<scalar_t, lno_t, gno_t, node_t> >tmVector =
            RCP< Tpetra::MultiVector<scalar_t, lno_t, gno_t, node_t> >(
                    new Tpetra::MultiVector<scalar_t, lno_t, gno_t, node_t>( mp, coordView.view(0, pqJagged_multiVectorDim), pqJagged_multiVectorDim));

#ifdef memory_debug
    sleep(1); env->memory("initial multivector after create");
#endif
    RCP<const tMVector_t> coordsConst = Teuchos::rcp_const_cast<const tMVector_t>(tmVector);

#ifdef memory_debug
    sleep(1); env->memory("initial multivector after cast");
#endif
    return coordsConst;
}





template <typename gno_t, typename lno_t,typename scalar_t, typename node_t>
RCP<Tpetra::MultiVector<scalar_t, lno_t, gno_t, node_t> > create_multi_vector(
        RCP<Comm<int> > &comm,
        gno_t numGlobalPoints,
        lno_t numLocalPoints,
        int coord_dim,
        scalar_t **coords,
        int weight_dim,
        scalar_t **weight,
#ifdef migrate_gid
        scalar_t *mappedGnos,
#endif
        scalar_t *assigned_parts,
        int &multiVectorDim,
        RCP<const Tpetra::MultiVector<scalar_t, lno_t, gno_t, node_t> > old_mvector
){

    typedef Tpetra::MultiVector<scalar_t, lno_t, gno_t, node_t> tMVector_t;
    RCP<const Tpetra::Map<lno_t, gno_t, node_t> > mp = old_mvector->getMap();

    Teuchos::Array<Teuchos::ArrayView<const scalar_t> > coordView(multiVectorDim + 1);

    for (int i=0; i < coord_dim; i++){
        if(numLocalPoints > 0){
            Teuchos::ArrayView<const scalar_t> a(coords[i], numLocalPoints);
            coordView[i] = a;
        } else{
            Teuchos::ArrayView<const scalar_t> a;
            coordView[i] = a;
        }
    }
    //cout << "coordim:" << coord_dim << endl;
    for (int i=0; i < weight_dim; i++){

        int j = i + coord_dim;
#ifdef migrate_gid
        if (j >= multiVectorDim - 1) break;
#endif
#ifndef migrate_gid
        if (j >= multiVectorDim) break;
#endif
        if(numLocalPoints > 0){
            Teuchos::ArrayView<const scalar_t> a(weight[i], numLocalPoints);
            coordView[j] = a;
        } else{
            Teuchos::ArrayView<const scalar_t> a;
            coordView[j] = a;
        }
    }
    //cout << "weightdim:" << weight_dim << endl;
#ifdef migrate_gid

    if(numLocalPoints > 0){
        //cout << "writing :" << weight_dim + coord_dim << endl;
        Teuchos::ArrayView<const scalar_t> a(mappedGnos, numLocalPoints);
        coordView[multiVectorDim - 1] = a;
    } else{
        Teuchos::ArrayView<const scalar_t> a;
        coordView[multiVectorDim - 1] = a;
    }
    //multiVectorDim += 1;
#endif
    if (assigned_parts){
        if(numLocalPoints > 0){
            //cout << "writing :" << weight_dim + coord_dim << endl;
            Teuchos::ArrayView<const scalar_t> a(assigned_parts, numLocalPoints);
            coordView[multiVectorDim] = a;
        } else{
            Teuchos::ArrayView<const scalar_t> a;
            coordView[multiVectorDim] = a;
        }
        multiVectorDim += 1;
    }
    RCP< Tpetra::MultiVector<scalar_t, lno_t, gno_t, node_t> >tmVector = RCP< Tpetra::MultiVector<scalar_t, lno_t, gno_t, node_t> >(
            new Tpetra::MultiVector<scalar_t, lno_t, gno_t, node_t>( mp, coordView.view(0, multiVectorDim), multiVectorDim));

    RCP<const tMVector_t> coordsConst = Teuchos::rcp_const_cast<const tMVector_t>(tmVector);

    return tmVector;
}



template <typename gno_t, typename lno_t,typename scalar_t, typename node_t>
void get_multi_vector(
        RCP<Comm<int> > &pcomm,

        RCP<Comm<int> > &comm,
        lno_t &numLocalPoints, gno_t &numGlobalPoints,
        int coord_dim,
        scalar_t **coords,
        int weight_dim,
        scalar_t **weight,
#ifdef migrate_gid
        scalar_t *&mappedGnos,
#endif
        scalar_t *&assigned_parts
        ,RCP<const Tpetra::MultiVector<scalar_t, lno_t, gno_t, node_t> > coordsConst
        ,partId_t out_num_parts, lno_t *&permutations,
        lno_t *part_begins
        ,partId_t num_parts, int pqJagged_multiVectorDim,
        string iteration
){
    //numLocalPoints =  coordsConst->getData(0).size();

    //numGlobalPoints = coordsConst->getGlobalLength ();
    //Teuchos::Array<Teuchos::ArrayView<const scalar_t> > coordView(coord_dim + weight_dim + 1);
    //  if (iteration != "0")
    //  cout <<"me:" <<pcomm->getRank() << " start - outnumparts:" << out_num_parts << endl;

    for (int i=0; i < coord_dim; i++){
        ArrayRCP< const scalar_t >  coord = coordsConst->getData(i);
        coords[i] =(scalar_t *) coord.getRawPtr();
    }
    //  if (iteration != "0")
    //  cout <<"me:" <<pcomm->getRank() << " then - outnumparts:" << out_num_parts << endl;

    for (int i=0; i < weight_dim; i++){
#ifdef migrate_gid
        if (i + coord_dim >= pqJagged_multiVectorDim - 1) break;
#endif
#ifndef migrate_gid
        if (i + coord_dim >= pqJagged_multiVectorDim) break;
#endif
        //cout << "reading:" << i+ coord_dim << endl;
        ArrayRCP< const scalar_t >  wgts = coordsConst->getData(i+ coord_dim);
        weight[i] = (scalar_t *) wgts.getRawPtr();
    }

#ifdef migrate_gid
    mappedGnos =  (scalar_t *)  coordsConst->getData(pqJagged_multiVectorDim - 1).getRawPtr();
#endif

    /*
  if (assigned_parts){

    assigned_parts = (scalar_t *)coordsConst->getData (pqJagged_multiVectorDim).getRawPtr();
  }
     */

    if (out_num_parts == 1){
        for(lno_t i = 0; i < numLocalPoints; ++i){
            permutations[i] = i;
        }
        part_begins[0] = numLocalPoints;
    }
    else {
        //cout <<"me:" <<comm->getRank() << " outnumparts bigger than 1:" << out_num_parts << endl;

        //lno_t *counts = new lno_t[num_parts];
        assigned_parts = (scalar_t *)coordsConst->getData /*getData*/(pqJagged_multiVectorDim).getRawPtr();
        lno_t *counts = allocMemory<lno_t>(num_parts);

        //partId_t *part_shifts = new partId_t[num_parts];
        partId_t *part_shifts = allocMemory<partId_t>(num_parts);

        memset(counts, 0, sizeof(lno_t) * num_parts);
        for(lno_t i = 0; i < numLocalPoints; ++i){
            partId_t ii = (assigned_parts[i]+1)/2;
            ++counts[ii];
            //++counts[partId_t((assigned_parts[i]+1)/2)];
        }
        /*
       for(partId_t i = 0; i < num_parts; ++i){
       cout << "me:" <<comm->getRank() <<  " p:" << i << " :count:" <<  counts[i] << endl; 
       }	
         */
        partId_t p = 0;
        lno_t prev_index = 0;
        for(partId_t i = 0; i < num_parts; ++i){
            if(counts[i] > 0)  {
                //cout << "me:" <<comm->getRank() <<  " p:" << i <<" prev_index:"<< prev_index << " :count:" <<  counts[i] << endl;

                part_begins[p] =  prev_index + counts[i];
                prev_index += counts[i];
                part_shifts[i] = p++;

            }
            //counts[i] += counts[i-1];
        }
        partId_t assigned_count = p - 1;
        //cout << "p:" << p << " nump:" << num_parts << " out:" << out_num_parts << endl;
        for (;p < num_parts; ++p){
            part_begins[p] =  part_begins[assigned_count ];
        }
        for(partId_t i = 0; i < out_num_parts; ++i){
            counts[i] = part_begins[i];

            //cout <<  "me:" <<comm->getRank() <<  " p:" << i << " partend:" <<  part_begins[i] << endl;
        }
        for(lno_t i = numLocalPoints - 1; i >= 0; --i){
            partId_t part = part_shifts[partId_t((assigned_parts[i]+1)/2)];
            //cout <<  "me:" <<comm->getRank() <<  " i:" << i << " part[i]:" << part << " placing to:" << counts[part] -1 << endl;

            permutations[--counts[part]] = i;
        }
        /*
       for(lno_t i = 0; i < numLocalPoints; ++i){
       cout <<  "me:" <<comm->getRank() <<  " i:" << i << " permutations[i]:" << permutations[i] <<endl;
       }
         */
        //delete[]counts;
        freeArray<lno_t>(counts);

        //delete[]part_shifts;
        freeArray<partId_t>(part_shifts);
    }
}

#define MIGRATIONIMBALANCE 0.03
template <typename mvector_t, typename gno_t, typename partId_t>
bool migrateData(
        RCP<Comm<int> > &pcomm,
        const RCP<const Environment> &env,
        RCP<Comm<int> > &comm,

        partId_t *&lrflags,
        RCP<const mvector_t> &vectors,    // on return is the new data

        partId_t *p_pid_np_num_procs_each_part,
        partId_t *p_pid_np_work_num_procs_each_part,
        partId_t num_parts,
        /*
       gno_t *p_gno_np_local_num_coord_each_part,
       gno_t *p_gno_np_work_local_num_coord_each_part,
       gno_t *p_gno_np_global_num_coord_each_part,
         */
        partId_t *ids, partId_t &groupSize,
        partId_t &out_num_part,
        partId_t &partIndexBegin, partId_t futurePartIndex,
        int all2alloption,
        int migration_check_option,
        scalar_t migration_imbalance_cut_off,
        string iteration,
#ifdef omitted2
        bool pqJagged_uniformWeights,
        scalar_t *coordWeights,
        bool allowNonRectelinearPart,
        float **nonRectelinearRatios,
        float *actual_ratios,
        scalar_t *localPartWeights,
        double **partWeights,
        scalar_t *cutCoordinates,
#endif
        scalar_t *&assigned_parts,
        int &coord_dim,
        scalar_t **coords,
        int &weight_dim,
        scalar_t **weight,
        int &multiVectorDim,
        int assignment_type
)          // on return is num procs with left data
{

#ifdef memory_debug
    sleep(1); env->memory("\t\t\t\tBefore migrateData-" + iteration);
#endif
    int migration_option = 1;
    typedef typename mvector_t::scalar_type scalar_t;
    typedef typename mvector_t::local_ordinal_type lno_t;


    //int migration_option = 1;

    partId_t nprocs = comm->getSize();
    lno_t nobj = vectors->getLocalLength();
    size_t nGlobalObj = vectors->getGlobalLength();
    partId_t myRank = comm->getRank();
    if (nprocs <= num_parts) migration_option = 1;
    partId_t allocation_size = num_parts;
    if (migration_option == 1){
        allocation_size = num_parts * (nprocs + 1);
    }
    //gno_t *p_gno_np_local_num_coord_each_part_actual = new gno_t[allocation_size];
    gno_t *p_gno_np_local_num_coord_each_part_actual = allocMemory<gno_t>(allocation_size);

    //gno_t *p_gno_np_work_local_num_coord_each_part_actual = new gno_t[allocation_size];
    gno_t *p_gno_np_work_local_num_coord_each_part_actual = allocMemory<gno_t>(allocation_size);
    //gno_t *p_gno_np_global_num_coord_each_part_actual  = new gno_t[allocation_size];
    gno_t *p_gno_np_global_num_coord_each_part_actual  = allocMemory<gno_t>(allocation_size);

    gno_t *p_gno_np_local_num_coord_each_part = p_gno_np_local_num_coord_each_part_actual;
    gno_t *p_gno_np_work_local_num_coord_each_part = p_gno_np_work_local_num_coord_each_part_actual;
    gno_t *p_gno_np_global_num_coord_each_part  = p_gno_np_global_num_coord_each_part_actual;

    gno_t *p_gno_np_local_num_coord_each_part_mypart = p_gno_np_local_num_coord_each_part_actual;
    gno_t *p_gno_np_work_local_num_coord_each_part_mypart = p_gno_np_work_local_num_coord_each_part_actual;
    gno_t *p_gno_np_global_num_coord_each_part_mypart  = p_gno_np_global_num_coord_each_part_actual;



    partId_t shift_amount = nprocs * num_parts;
    partId_t my_part_shift = myRank * num_parts;

    if (migration_option == 1){

        p_gno_np_local_num_coord_each_part += shift_amount;
        p_gno_np_work_local_num_coord_each_part += shift_amount;
        p_gno_np_global_num_coord_each_part += shift_amount;

        p_gno_np_local_num_coord_each_part_mypart += my_part_shift;
        p_gno_np_work_local_num_coord_each_part_mypart += my_part_shift;
        p_gno_np_global_num_coord_each_part_mypart  += my_part_shift;

    }

    //needs:
    //bool pqJagged_uniformWeights,
    //scalar_t *coordWeights,
    //bool allowNonRectelinearPart,
    //float **nonRectelinearRatios,
    //float **actual_ratios,
    //scalar_t **localPartWeights,
    //scalar_t **partWeights,
    //scalar_t *cutCoordinates,

    memset(p_gno_np_local_num_coord_each_part_actual, 0, sizeof(gno_t)*allocation_size);

    ///////////////
#ifdef omitted2
#ifdef HAVE_ZOLTAN2_OMP
#pragma omp parallel
#endif
    {
        int me = 0;
        int noThreads = 1;
#ifdef HAVE_ZOLTAN2_OMP
        me = omp_get_thread_num();
        noThreads = omp_get_num_threads();
#endif

        //lno_t *myStarts = coordinate_starts[me];
        //lno_t *myEnds = coordinate_ends[me];
        float *myRatios = NULL;
        scalar_t _EPSILON = numeric_limits<scalar_t>::epsilon();
        partId_t noCuts = num_parts - 1; 

        if (allowNonRectelinearPart){

            myRatios = nonRectelinearRatios[me];
#ifdef HAVE_ZOLTAN2_OMP
#pragma omp for
#endif
            for (partId_t i = 0; i < noCuts; ++i){
                float r = actual_ratios[i];

                //cout << "real i:" << i << " :" << r << " " << endl;
                scalar_t leftWeight = r * (localPartWeights[i * 2 + 1] - localPartWeights[i * 2]);
                for(int ii = 0; ii < noThreads; ++ii){
                    if(leftWeight > _EPSILON){

                        scalar_t ithWeight = partWeights[ii][i * 2 + 1] - partWeights[ii][i * 2 ];
                        if(ithWeight < leftWeight){
                            nonRectelinearRatios[ii][i] = ithWeight;
                        }
                        else {
                            nonRectelinearRatios[ii][i] = leftWeight ;
                        }
                        leftWeight -= ithWeight;
                    }
                    else {
                        nonRectelinearRatios[ii][i] = 0;
                    }
                }
            }


            if(noCuts > 0){
                for (partId_t i = noCuts - 1; i > 0 ; --i){          if(ABS(cutCoordinates[i] - cutCoordinates[i -1]) < _EPSILON){
                    myRatios[i] -= myRatios[i - 1] ;
                }
                //cout << "i:" << i << " :" << myRatios[i] << " ";
                myRatios[i] = int ((myRatios[i] + LEAST_SIGNIFICANCE) * SIGNIFICANCE_MUL) / scalar_t(SIGNIFICANCE_MUL);
                }
            }
            /*

             for (partId_t i = 0; i < noCuts; ++i){
             cout << "r i:" << i << " :" <<  myRatios[i] << " " << endl;
             }
             */

        }

#ifdef HAVE_ZOLTAN2_OMP
#pragma omp for
#endif
        for (lno_t i=0; i < nobj; i++){
            partId_t pp = lrflags[i];
            partId_t p = pp / 2;

            if(pp % 2 == 1){
                if(allowNonRectelinearPart && myRatios[p] > _EPSILON * EPS_SCALE){
                    //cout << "pa:" << p << endl;
                    scalar_t w = pqJagged_uniformWeights? 1:coordWeights[i];

                    myRatios[p] -= w;
                    if(myRatios[p] < 0 && p < noCuts - 1 && ABS(cutCoordinates[p+1] - cutCoordinates[p]) < _EPSILON){
                        myRatios[p + 1] += myRatios[p];
                    }
                    lrflags[i] = pp - 1;
                }
                else{
                    //scalar_t currentCut = cutCoordinates[p];
                    //TODO:currently cannot divide 1 line more than 2 parts.
                    //bug cannot be divided, therefore this part should change.
                    //cout << "p:" << p+1 << endl;
                    lrflags[i] = pp + 1;


                }

            }
            ++p_gno_np_local_num_coord_each_part[(lrflags[i])/2];
        }


    }
#endif
    /*
  scalar_t *&assigned_parts,
  int &coord_dim,
  scalar_t **coords,
  int &weight_dim,
  scalar_t **weight,
  int &multiVectorDim,

     */



    if (migration_option == 1){
        memcpy (p_gno_np_local_num_coord_each_part_mypart,
                p_gno_np_local_num_coord_each_part,
                sizeof(gno_t) * (num_parts) );
    }


    env->timerStart(MACRO_TIMERS, "PQJagged ReduceAll-" + iteration);

    try{
        reduceAll<int, gno_t>(
                *comm,
                Teuchos::REDUCE_SUM,
                allocation_size,
                p_gno_np_local_num_coord_each_part_actual,
                p_gno_np_global_num_coord_each_part_actual);
    }
    Z2_THROW_OUTSIDE_ERROR(*env)
    env->timerStop(MACRO_TIMERS, "PQJagged ReduceAll-" + iteration);

    if (migration_check_option == 0){
        scalar_t diff = 0, global_diff = 0;
        if (migration_option == 0 )
        {

            for (partId_t i = 0; i < num_parts; ++i){
                scalar_t ideal_num = p_gno_np_global_num_coord_each_part[i] /  scalar_t(nprocs);
                diff += ABS(ideal_num -
                        p_gno_np_local_num_coord_each_part_mypart[i]) /  (ideal_num);
            }
            diff /= num_parts;
            reduceAll<int, scalar_t>(
                    *comm,
                    Teuchos::REDUCE_SUM,
                    1,
                    &diff,
                    &global_diff);
        }
        else {
            for (partId_t ii = 0; ii < nprocs; ++ii){
                for (partId_t i = 0; i < num_parts; ++i){
                    scalar_t ideal_num = p_gno_np_global_num_coord_each_part[i] /  scalar_t(nprocs);
                    /*
            global_diff += ABS(ideal_num -
              p_gno_np_local_num_coord_each_part_mypart[i]) /  (ideal_num);
                     */
                    global_diff += ABS(ideal_num -
                            p_gno_np_global_num_coord_each_part_actual[ii * num_parts + i]) /  (ideal_num);


                }
            }
            global_diff /= num_parts;

        }
        global_diff /= nprocs;
        if (myRank == 0) {
            cout << "imbalance for next iteration:" << global_diff << endl;
        }



        if(global_diff <= migration_imbalance_cut_off){
            //delete []p_gno_np_local_num_coord_each_part_actual;
            freeArray<gno_t>(p_gno_np_local_num_coord_each_part_actual);
            //delete []p_gno_np_work_local_num_coord_each_part_actual;
            freeArray<gno_t>(p_gno_np_work_local_num_coord_each_part_actual);
            //delete []p_gno_np_global_num_coord_each_part_actual;
            freeArray<gno_t>(p_gno_np_global_num_coord_each_part_actual);

            return false;
        }

    }


    if (nprocs < num_parts){
        //assigned_parts = new scalar_t[nobj];
        assigned_parts = allocMemory<scalar_t>(nobj);
        for (lno_t i = 0; i < nobj; ++i){
            assigned_parts[i] = scalar_t (lrflags[i]);
        }
#ifdef memory_debug
        sleep(1); env->memory("\t\t\t\tBefore create multi");
#endif
        RCP<const mvector_t> tmpvectors = create_multi_vector <gno_t,lno_t,scalar_t,node_t>(
                comm,
                nGlobalObj,
                nobj,
                coord_dim,
                coords,
                weight_dim,
                weight,
#ifdef migrate_gid
                mappedGnos,
#endif
                assigned_parts, multiVectorDim,
                vectors
        );
#ifdef memory_debug
        sleep(1); env->memory("\t\t\t\tAfter create multi");
#endif

        vectors = tmpvectors;
#ifdef memory_debug
        sleep(1); env->memory("\t\t\t\tAfter release multi");
#endif
    }

    ///////////////////////////////////////////////////////
    // Get a list of my new global numbers.

    Array<lno_t> sendCount(nprocs, 0);
    Array<lno_t> recvCount(nprocs, 0);
    Array<gno_t> sendBuf(nobj, 0);
    out_num_part = 0;
    //int assigned_part_count = 0;
    if (1 || nobj > 0){
        if (nprocs > num_parts){
            if(migration_option == 1){
                /*
    	if(comm->getRank() == 0){
          cout << "smart migration" << endl;
        }
                 */
                //partId_t *part_assign_begins = new partId_t [num_parts];
                partId_t *part_assign_begins = allocMemory<partId_t>(num_parts);
                //partId_t *proc_chains = new partId_t [nprocs];
                partId_t *proc_chains = allocMemory<partId_t>(nprocs);

                partId_t left_proc = nprocs;
                partId_t min_required_for_rest = num_parts - 1;
                //find how many processors is needed for each part.

                bool did_i_find_my_group = false;

                partId_t next_proc_to_assign = 0;
                for (partId_t i=0; i < num_parts; i++){
                    partId_t required_proc = static_cast<partId_t>(floor(0.5f + nprocs * (scalar_t (p_gno_np_global_num_coord_each_part[i]) / scalar_t(nGlobalObj))));
                    if (left_proc - required_proc < min_required_for_rest){
                        required_proc = left_proc - (min_required_for_rest);
                    }
                    left_proc -= required_proc;
                    --min_required_for_rest;
                    p_pid_np_num_procs_each_part[i] = required_proc;
                }
                if (left_proc > 0){
                    p_pid_np_num_procs_each_part[0] +=  left_proc;
                }
                //now find what are the best processors with least migration for each part.

                //partId_t *proc_part_assignments = new partId_t[nprocs];
                partId_t *proc_part_assignments = allocMemory<partId_t>(nprocs);

                for (int i = 0; i < nprocs; ++i ){
                    proc_part_assignments[i] = -1;
                    proc_chains[i] = -1;
                }
                for (int i = 0; i < num_parts; ++i ){
                    part_assign_begins[i] = -1;
                }

                //uSortItem<partId_t, lno_t> * proc_points_in_part = new uSortItem<partId_t, partId_t>[nprocs];
                uSortItem<partId_t, lno_t> * proc_points_in_part = allocMemory <uSortItem<partId_t, partId_t> > (nprocs);

                for(partId_t i = 0; i < num_parts; ++i){


                    gno_t total_num_points_in_part = p_gno_np_global_num_coord_each_part[i];
                    if(assignment_type == 0){
                        //cout << "assign=0" << endl;
                        for(partId_t ii = 0; ii < nprocs; ++ii){
                            proc_points_in_part[ii].id = ii;

                            if (proc_part_assignments[ii] == -1){

                                proc_points_in_part[ii].val = p_gno_np_global_num_coord_each_part_actual[ii * num_parts + i];
                            }
                            else {
                                proc_points_in_part[ii].val = -p_gno_np_global_num_coord_each_part_actual[ii * num_parts + i] - 1;
                            }
                        }
                    }
                    else {
                        //cout << "assign=1" << endl;

                        partId_t required_proc = p_pid_np_num_procs_each_part[i];
                        partId_t last_proc_to_assign = next_proc_to_assign + required_proc;
                        for(partId_t ii = 0; ii < nprocs; ++ii){
                            proc_points_in_part[ii].id = ii;

                            if (ii >= next_proc_to_assign && ii < last_proc_to_assign){

                                proc_points_in_part[ii].val = p_gno_np_global_num_coord_each_part_actual[ii * num_parts + i];
                            }
                            else {
                                proc_points_in_part[ii].val = -p_gno_np_global_num_coord_each_part_actual[ii * num_parts + i] - 1;
                            }
                        }
                        next_proc_to_assign = last_proc_to_assign;


                    }

                    env->timerStart(MACRO_TIMERS, "PQJagged Sort-" + iteration);
                    uqsort<partId_t, partId_t>(nprocs, proc_points_in_part);
                    env->timerStop(MACRO_TIMERS, "PQJagged Sort-" + iteration);

                    partId_t required_proc_count =  p_pid_np_num_procs_each_part[i];


                    gno_t ideal_num_points_in_procs =
                            ceil (total_num_points_in_part / scalar_t (required_proc_count));
                    //if(myRank == 0)
                    //cout <<"i:" << i << " assign ideal_num_points_in_procs:" << ideal_num_points_in_procs << endl;

                    partId_t next_part_to_send = nprocs - 1;
                    partId_t next_part_to_send_id = proc_points_in_part[next_part_to_send].id;
                    lno_t space_left = 	ideal_num_points_in_procs - proc_points_in_part[next_part_to_send].val;
                    /*
             if(myRank == 1) cout << "I:" << i << " NEXTnext_part_to_send_id:"<< next_part_to_send_id << " proc_part_assignments:" << proc_part_assignments[next_part_to_send_id]<<endl;
                     */
                    for(partId_t ii = nprocs - 1; ii >= nprocs - required_proc_count; --ii){
                        partId_t partid = proc_points_in_part[ii].id;
                        proc_part_assignments[partid] = i;
                    }
                    if (!did_i_find_my_group){
                        for(partId_t ii = nprocs - 1; ii >= nprocs - required_proc_count; --ii){
                            partId_t partid = proc_points_in_part[ii].id;
                            ids[nprocs - 1 - ii] = partid;
                            //proc_part_assignments[partid] = i;

                            if(partid == myRank){
                                did_i_find_my_group = true;
                                part_assign_begins[i] = myRank;
                                proc_chains[myRank] = -1;
                                sendCount[myRank] = proc_points_in_part[ii].val;
                                partIndexBegin += i * futurePartIndex;
                            }
                        }
                        if (did_i_find_my_group){
                            groupSize = required_proc_count;
                            //cout << "beginningme:" << myRank << " assigned to :" << i << " with group:" << groupSize  << " id:" << ids[0]<< endl;
                        }
                    }
                    for(partId_t ii = nprocs - required_proc_count - 1; ii >= 0; --ii){
                        partId_t partid = proc_points_in_part[ii].id;
                        lno_t to_sent = proc_points_in_part[ii].val;
                        if (to_sent < 0) to_sent = -to_sent - 1;
                        /*
                if(myRank == 1) cout << "i:" << i << " ii:" << ii << " p:" << partid<< " to_sent:" <<to_sent << " next_part_to_send:" << next_part_to_send_id << " space:" << space_left << endl;
                         */
                        while (to_sent > 0){
                            if (to_sent <= space_left){
                                space_left -= to_sent;
                                if (myRank == partid){

                                    sendCount[next_part_to_send_id] = to_sent;

                                    partId_t prev_begin = part_assign_begins[i];
                                    part_assign_begins[i] = next_part_to_send_id;
                                    proc_chains[next_part_to_send_id] = prev_begin;
                                }
                                to_sent = 0;
                            }
                            else {
                                to_sent -= space_left;


                                if (myRank == partid){
                                    sendCount[next_part_to_send_id] = space_left;
                                    partId_t prev_begin = part_assign_begins[i];
                                    part_assign_begins[i] = next_part_to_send_id;
                                    proc_chains[next_part_to_send_id] = prev_begin;

                                }

                                --next_part_to_send;

#ifdef debug_ 
                                //TODO remove comment
                                if(next_part_to_send <  nprocs - required_proc_count ){
                                    cout << "this should not happen next part to send:" << next_part_to_send << endl;

                                }
#endif
                                next_part_to_send_id =  proc_points_in_part[next_part_to_send].id;
                                space_left = ideal_num_points_in_procs - proc_points_in_part[next_part_to_send].val;
                            }
                        }
                    }

                }
                //lno_t *_sendCount_psum = new lno_t[nprocs];
                lno_t *_sendCount_psum = allocMemory<lno_t>(nprocs);

                //lno_t *sendCount_copy = new lno_t[nprocs];
                lno_t *sendCount_copy = allocMemory<lno_t>(nprocs);
                lno_t prefixsum = 0;

                for (int i = 0; i < nprocs; ++i ){
                    _sendCount_psum[i] = prefixsum;
                    //cout << "me:" << myRank << " to:" << i << " many points:" <<  sendCount[i] << endl;
                    prefixsum += sendCount[i];
                    sendCount_copy[i] = sendCount[i];
                }
                ArrayView<const gno_t> gnoList = vectors->getMap()->getNodeElementList();
                for (lno_t i=0; i < nobj; i++){
                    partId_t p = (lrflags[i]+1)/2;
                    gno_t to_send = gnoList[i];
                    partId_t proc_to_sent = part_assign_begins[p];
                    partId_t left_coordinates_to_that_part = sendCount_copy[proc_to_sent];
                    if (left_coordinates_to_that_part == 0){
                        part_assign_begins[p] = proc_chains[proc_to_sent];
                        proc_chains[proc_to_sent] = -1;
                        proc_to_sent = part_assign_begins[p];
                    }
                    sendBuf[_sendCount_psum[proc_to_sent]++] = to_send;
                    --sendCount_copy[proc_to_sent];
                }

                out_num_part = 1;
                //delete []part_assign_begins;
                freeArray<partId_t>(part_assign_begins);
                //delete []proc_chains;
                freeArray<partId_t>(proc_chains);
                //delete []proc_part_assignments;
                freeArray<partId_t>(proc_part_assignments);
                //delete []proc_points_in_part;
                freeArray<uSortItem<partId_t, partId_t> > (proc_points_in_part);
                //delete []_sendCount_psum;
                freeArray<lno_t>(_sendCount_psum);
                //delete []sendCount_copy;
                freeArray<lno_t>(sendCount_copy);
            } else {
                if(comm->getRank() == 0){
                    cout << "easy migration" << endl;
                }

                partId_t left_proc = nprocs;
                partId_t min_required_for_rest = num_parts - 1;
                for (partId_t i=0; i < num_parts; i++){

                    partId_t required_proc = static_cast<partId_t>(floor(0.5f + nprocs * (scalar_t (p_gno_np_global_num_coord_each_part[i]) / scalar_t(nGlobalObj))));
                    /*		if(comm->getRank() == 0){
                cout << "############part:" << i << " left_proc:" << left_proc <<  " required1:" << required_proc << endl;
                }
                     */

                    //if (required_proc > left_proc) required_proc = left_proc;
                    if (left_proc - required_proc < min_required_for_rest){
                        required_proc = left_proc - (min_required_for_rest);
                    }
                    left_proc -= required_proc;
                    --min_required_for_rest;
                    p_pid_np_num_procs_each_part[i] = required_proc;
                    /*if(comm->getRank() == 0){
            cout << "############part:" << i << " required:" << required_proc << endl;
            }*/
                }
                partId_t begin = 0;
                partId_t group_begin = -1, group_end = -1;

                for (partId_t i=0; i < num_parts; i++){
                    if (myRank >= begin && myRank < begin + p_pid_np_num_procs_each_part[i]){
                        group_begin = begin;
                        group_end = begin + p_pid_np_num_procs_each_part[i];
                        partIndexBegin += i * futurePartIndex;
                        break;
                    }
                    begin +=  p_pid_np_num_procs_each_part[i];
                }
                //cout << "me:" << comm->getRank() << " begin:" << group_begin << " end:" << group_end << endl;
                groupSize = group_end - group_begin;
                for (partId_t i = 0; i < groupSize; ++i){
                    ids[i] = begin + i;
                }

                //++assigned_part_count;
                partId_t assigned_part = 0;
                partId_t assign_count = 1;
                for(partId_t i = 0; i < nprocs; ++i){

                    lno_t to_assign = 0;
                    if(assign_count < p_pid_np_num_procs_each_part[assigned_part]){
                        to_assign = p_gno_np_local_num_coord_each_part[assigned_part] / p_pid_np_num_procs_each_part[assigned_part];
                        ++assign_count;

                    }
                    else {
                        to_assign = p_gno_np_local_num_coord_each_part[assigned_part] -
                                (p_pid_np_num_procs_each_part[assigned_part] - 1 ) * (p_gno_np_local_num_coord_each_part[assigned_part] / p_pid_np_num_procs_each_part[assigned_part]);
                        assign_count = 1;
                        ++assigned_part;
                    }
                    sendCount[i] = to_assign;
                }

                for(partId_t i = 1; i < num_parts; ++i){
                    p_gno_np_local_num_coord_each_part[i] += p_gno_np_local_num_coord_each_part[i - 1];
                }
                out_num_part = 1;
                ArrayView<const gno_t> gnoList = vectors->getMap()->getNodeElementList();
                for (lno_t i=0; i < nobj; i++){
                    partId_t p = (lrflags[i]+1)/2;
                    gno_t to_send = gnoList[i];
                    sendBuf[--p_gno_np_local_num_coord_each_part[p]] = to_send;
                }

            }
        }
        else {
            groupSize = 1;
            ids[0] = myRank;

            //calculate the optimal number of coordiates to each part.
            lno_t work_each = nGlobalObj / (scalar_t (nprocs)) + 0.5f;
            //cout << "work each:" << work_each << endl;
            //to hold the index of the processors that is assigned to the part.
            //partId_t *assigned_processor = new partId_t[num_parts];
            //to hold the left space as the number of coordiantes to the optomal number in each proc.
            //lno_t *space_in_each_processor = new lno_t[nprocs];
            lno_t *space_in_each_processor = allocMemory <lno_t>(nprocs);


            //to sort the parts with decreasing order of their coordiantes.
            //id are the part numbers, sort value is the number of points in each.
            //uSortItem<partId_t, gno_t> * part_loads = new uSortItem<partId_t, gno_t>[num_parts];

            uSortItem<partId_t, gno_t> * part_loads  = allocMemory <uSortItem<partId_t, gno_t> >(num_parts);

            //to sort the parts that is assigned to the processors.
            //id is the part number, sort value is the assigned processor id.
            //uSortItem<partId_t, partId_t> * part_assignment = new uSortItem<partId_t, partId_t>[num_parts];
            uSortItem<partId_t, partId_t> * part_assignment  = allocMemory <uSortItem<partId_t, partId_t> >(num_parts);

            //uSortItem<partId_t, gno_t> * proc_load_sort = new uSortItem<partId_t, gno_t>[nprocs];
            uSortItem<partId_t, gno_t> * proc_load_sort = allocMemory <uSortItem<partId_t, gno_t> >(nprocs);

            for (partId_t i = 0; i < num_parts; ++i){
                part_loads[i].id = i;
                part_loads[i].val = p_gno_np_global_num_coord_each_part[i];
            }
            //sort parts with increasing order of loads.
            uqsort<partId_t, gno_t>(num_parts, part_loads);

            //initialize left space in each.
            for (partId_t i = 0; i < nprocs; ++i){
                space_in_each_processor[i] = work_each;
            }

            //assigning parts to the processors
            for (partId_t j = 0; j < num_parts; ++j){
                //sorted with increasing order, traverse inverse.
                partId_t i = part_loads[num_parts - 1 - j].id;

                //assigned processors
                partId_t assignment = -1;
                //if not fit best processor.
                partId_t best_part = 0;
                //load of the part
                gno_t load = p_gno_np_global_num_coord_each_part[i];

                for (partId_t ii = 0; ii < nprocs; ++ii){
                    proc_load_sort[ii].id = ii;
                    proc_load_sort[ii].val =  p_gno_np_global_num_coord_each_part_actual[ii * num_parts + i];

                }
                uqsort<partId_t, gno_t>(nprocs, proc_load_sort);

                //traverse all processors.
                for (partId_t iii = 0; iii < nprocs; ++iii){
                    partId_t ii = proc_load_sort[iii].id;
                    lno_t left_space = space_in_each_processor[ii] - load;
                    //if enought space, assign to this part.
                    if(left_space >= 0 ){
                        assignment = ii;
                        break;
                    }
                    //if space is not enough, store the best candidate part.
                    if (space_in_each_processor[best_part] < space_in_each_processor[ii]){
                        best_part = ii;
                    }

                }
                if (assignment == -1){
                    assignment = best_part;
                }
                space_in_each_processor[assignment] -= load;

                //to sort later, part-i is assigned to the proccessor - assignment.
                part_assignment[j].id = i;
                part_assignment[j].val = assignment;
                //cout << "part:" << i << " with " <<  p_gno_np_global_num_coord_each_part[i] << " coordiantes is assigned to " << assignment << endl;
                //if assigned processor is me, increase the number.
                if (assignment == myRank){
                    out_num_part++;//assigned_part_count;
                }
                //increase the send to that processor by the number of points in that part.
                sendCount[assignment] += p_gno_np_local_num_coord_each_part[i];
            }
            delete []proc_load_sort;
            //sort assignments with respect to the assigned processors.
            uqsort<partId_t, partId_t>(num_parts, part_assignment);
            //calculate the prefix sum for send buffer array.

            lno_t prefix_sum = p_gno_np_local_num_coord_each_part[part_assignment[0].id];

            for(partId_t i = 1; i < num_parts; ++i){

                partId_t id = part_assignment[i].id;
                //cout << "id:" << id << endl;
                p_gno_np_local_num_coord_each_part[id] += prefix_sum;
                prefix_sum = p_gno_np_local_num_coord_each_part[id];
                //cout << "me:" << myRank << " pf: "<< prefix_sum << endl;
            }
            partId_t previous_part = -1;
            lno_t prefix_sum_assigned_part = partIndexBegin;
            for(partId_t i = 0; i < num_parts; ++i){
                if(previous_part !=  part_assignment[i].val){
                    if(myRank == part_assignment[i].val){
                        partIndexBegin = prefix_sum_assigned_part;
                    }
                    previous_part = part_assignment[i].val;
                }
                prefix_sum_assigned_part += futurePartIndex;

            }


            //delete []part_assignment;
            freeArray<uSortItem<partId_t, partId_t> >(part_assignment);
            //delete []part_loads;
            freeArray<uSortItem<partId_t, gno_t> >(part_loads);

            //delete []space_in_each_processor;
            freeArray<lno_t >(space_in_each_processor);

            ArrayView<const gno_t> gnoList = vectors->getMap()->getNodeElementList();
            for (lno_t i=0; i < nobj; i++){
                partId_t p = (lrflags[i]+1)/2;
                gno_t to_send = gnoList[i];
                sendBuf[--p_gno_np_local_num_coord_each_part[p]] = to_send;
            }

        }

    }

    env->timerStart(MACRO_TIMERS, "PQJagged AlltoAll-" + iteration);
    gno_t numMyNewGnos = 0;
    ArrayRCP<gno_t> recvBuf;

#ifdef memory_debug
    sleep(1); env->memory("\t\t\t\tbefore alltoall-" + iteration);
#endif
    if (all2alloption == 2) {
        //partId_t *partIds = new partId_t[nobj];
        partId_t *partIds = allocMemory< partId_t>(nobj);
        partId_t *p = partIds;

        for (int i = 0; i < nprocs; ++i){
            lno_t sendC = sendCount[i];
            for (int ii = 0; ii < sendC; ++ii){
                *(p++) = i;
            }
        }

        env->timerStart(MACRO_TIMERS, "PQJagged Z2PlanCreating-" + iteration);
        Tpetra::Distributor distributor(comm);

        ArrayView<const partId_t> pIds( partIds, nobj);
        numMyNewGnos = distributor.createFromSends(pIds);
        env->timerStop(MACRO_TIMERS, "PQJagged Z2PlanCreating-" + iteration);
        /*
      if (numMyNewGnos!=distributor.getTotalReceiveLength())
          fprintf(stderr, "UVC: SOMETHING is wrong %d != %d\n", numMyNewGnos, distributor.getTotalReceiveLength());
         */
        ArrayRCP<gno_t> recvBuf2(distributor.getTotalReceiveLength());
        //recvBuf.reserve(distributor.getTotalReceiveLength());
        distributor.doPostsAndWaits<gno_t>(sendBuf(), 1, recvBuf2());
        recvBuf = recvBuf2;
        //delete []partIds;
        freeArray<partId_t>(partIds);

    } else if (all2alloption == 1){
        //partId_t *partIds = new partId_t[nobj];
        partId_t *partIds = allocMemory< partId_t>(nobj);
        partId_t *p = partIds;

        for (int i = 0; i < nprocs; ++i){
            lno_t sendC = sendCount[i];
            for (int ii = 0; ii < sendC; ++ii){
                *(p++) = i;
            }
        }
        ZOLTAN_COMM_OBJ *plan = NULL;     /* pointer for communication object */


        MPI_Comm mpi_comm = Teuchos2MPI (comm);
        lno_t incoming = 0;
        int message_tag = 7859;

        env->timerStart(MACRO_TIMERS, "PQJagged Z1PlanCreating-" + iteration);
        int ierr = Zoltan_Comm_Create(&plan, nobj, partIds, mpi_comm, message_tag,
                &incoming);
        env->timerStop(MACRO_TIMERS, "PQJagged Z1PlanCreating-" + iteration);


        ArrayRCP<gno_t> recvBuf2(incoming);
        gno_t *recieves  = recvBuf2.getRawPtr();
        //gno_t *recieves  = new gno_t [incoming];


        message_tag++;
        ierr = Zoltan_Comm_Do(plan, message_tag, (char *) sendBuf.getRawPtr(),
                sizeof(gno_t),
                (char *) recieves);

        ierr = Zoltan_Comm_Destroy(&plan);
        //ArrayView<gno_t> rec(recieves, incoming);
        //recvBuf = arcpFromArrayView(rec);
        numMyNewGnos = incoming;
        recvBuf = recvBuf2;
        //delete []partIds;
        freeArray<partId_t>(partIds);

    } else {
        try{
            AlltoAllv<gno_t>(*comm, *env,
                    sendBuf(), sendCount(),
                    recvBuf, recvCount());
        }
        Z2_FORWARD_EXCEPTIONS
        for (int i=0; i < nprocs; i++){
            numMyNewGnos += recvCount[i];
        }
    }


#ifdef memory_debug
    sleep(1); env->memory("\t\t\t\tafter alltoall-" + iteration);
#endif
    env->timerStop(MACRO_TIMERS, "PQJagged AlltoAll-" + iteration);

    sendCount.clear();
    sendBuf.clear();
    ///////////////////////////////////////////////////////
    // Migrate the multivector of data.
    ///////////////////////////////////////////////////////

    recvCount.clear();



    //delete []p_gno_np_local_num_coord_each_part_actual;
    freeArray<gno_t>(p_gno_np_local_num_coord_each_part_actual);
    //delete []p_gno_np_work_local_num_coord_each_part_actual;
    freeArray<gno_t>(p_gno_np_work_local_num_coord_each_part_actual);
    //delete []p_gno_np_global_num_coord_each_part_actual;
    freeArray<gno_t>(p_gno_np_global_num_coord_each_part_actual);

    env->timerStart(MACRO_TIMERS, "PQJagged Actual_Migration-" + iteration);

    try{
        /*
	if (pcomm->getRank() == 0) {
		//cout << " before mig vector count:" << vectors.count() << endl;
		cout << "nobj:" << nobj <<  " numMyNewGnos:" << numMyNewGnos << endl;

	}
         */
        typedef const Tpetra::Map<lno_t, gno_t, node_t> map_t;

#ifdef memory_debug
        sleep(1); env->memory("\t\t\t\tbefore do migration-" + iteration);
#endif

        if (pcomm->getRank() == 0)
            cout << "\t\t\t\tbefore mig map reference count:" << vectors->getMap().count() << endl;

        RCP<const mvector_t> vvectors = XpetraTraits<mvector_t>::doMigration2(
                vectors, numMyNewGnos, recvBuf.getRawPtr(), env);

        if (pcomm->getRank() == 0){
            cout << "\t\t\t\tafter mig old map reference count:" << vectors->getMap().count() << endl;
            cout << "\t\t\t\tafter mig new map reference count:" << vvectors->getMap().count() << endl;
        }
        {
            Teuchos::ArrayRCP< const scalar_t > r;
            Teuchos::ArrayRCP< const scalar_t > r0 = vectors->getData(0);
            Teuchos::ArrayRCP< const scalar_t > r1 = vectors->getData(1);

            Teuchos::ArrayRCP< const scalar_t > rr0 = vvectors->getData(0);
            Teuchos::ArrayRCP< const scalar_t > rr1 = vvectors->getData(1);

            if (pcomm->getRank() == 0){
                cout << "\t\t\t\t old array r0: " << r0.getRawPtr() << " r1:" << r1.getRawPtr() << endl;
                cout << "\t\t\t\t new rr0: " << rr0.getRawPtr() << " rr1:" << rr1.getRawPtr() << endl;
                cout << "\t\t\t\t old ar reference count:" << r0.count() << " "<< r1.count() << " scalar_t size:" << sizeof(scalar_t) << " gno:" << sizeof(gno_t)<< endl;
                cout << "\t\t\t\t new arr reference count:" << rr0.count() << " "<< rr1.count() << " scalar_t size:" << sizeof(scalar_t) << " gno:" << sizeof(gno_t)<< endl;
            }
            {


#ifdef memory_debug
                sleep(1); env->memory("\t\t\t\tafter do migration-" + iteration);
#endif
                RCP<map_t> subMap = vectors->getMap();

#ifdef memory_debug
                sleep(1); env->memory("\t\t\t\tafter old map is duplicated-" + iteration);
#endif

                if (pcomm->getRank() == 0){
                    cout << "subMap pointer:" << subMap.getRawPtr() << endl;
                    cout << "newMap pointer:" << vvectors->getMap().getRawPtr() << endl;
                }
                if (0 && pcomm->getRank() == 0){
                    cout << "\t\t\t\tduplicated map reference count:" << subMap.count() << endl;
                    cout << "\t\t\t\tafter duplicated old map reference count:" << vectors->getMap().count() << endl;
                    cout << "\t\t\t\tafter duplicated new map reference count:" << vvectors->getMap().count() << endl;
                }


                vectors = vvectors;

#ifdef memory_debug
                sleep(1); env->memory("\t\t\t\tafter release of old multivector-" + iteration);
#endif
                if (0 && pcomm->getRank() == 0){
                    cout << "\t\t\t\t old array r0: " << r0.getRawPtr() << " r1:" << r1.getRawPtr() << endl;
                    cout << "\t\t\t\t new rr0: " << rr0.getRawPtr() << " rr1:" << rr1.getRawPtr() << endl;
                    cout << "\t\t\t\t old ar reference count:" << r0.count() << " "<< r1.count() << " scalar_t size:" << sizeof(scalar_t) << " gno:" << sizeof(gno_t)<< endl;
                    cout << "\t\t\t\t new arr reference count:" << rr0.count() << " "<< rr1.count() << " scalar_t size:" << sizeof(scalar_t) << " gno:" << sizeof(gno_t)<< endl;
                    cout << "\t\t\t\tduplicated map reference count:" << subMap.count() << endl;
                    cout << "\t\t\t\tafter release new map reference count:" << vvectors->getMap().count() << endl;
                }
                subMap = vvectors->getMap();
#ifdef memory_debug
                sleep(1); env->memory("\t\t\t\tafter old subMap set to new-" + iteration);
#endif
            }

#ifdef memory_debug
            sleep(1); env->memory("\t\t\t\tafter old subMap release-" + iteration);
#endif

            if (0 && pcomm->getRank() == 0){
                cout << "\t\t\t\t old array r0: " << r0.getRawPtr() << " r1:" << r1.getRawPtr() << endl;
                cout << "\t\t\t\t new rr0: " << rr0.getRawPtr() << " rr1:" << rr1.getRawPtr() << endl;
                cout << "\t\t\t\t old ar reference count:" << r0.count() << " "<< r1.count() << " scalar_t size:" << sizeof(scalar_t) << " gno:" << sizeof(gno_t)<< endl;
                cout << "\t\t\t\t new arr reference count:" << rr0.count() << " "<< rr1.count() << " scalar_t size:" << sizeof(scalar_t) << " gno:" << sizeof(gno_t)<< endl;
                cout << "\t\t\t\tafter release new map reference count:" << vvectors->getMap().count() << endl;
            }

            r1 = r;
#ifdef memory_debug
            sleep(1); env->memory("\t\t\t\tafter r1 release-" + iteration);
#endif

            if (0 && pcomm->getRank() == 0){
                cout << "\t\t\t\t old array r0: " << r0.getRawPtr() << " r1:" << r1.getRawPtr() << endl;
                cout << "\t\t\t\t new rr0: " << rr0.getRawPtr() << " rr1:" << rr1.getRawPtr() << endl;
                cout << "\t\t\t\t old ar reference count:" << r0.count() << " "<< r1.count() << " scalar_t size:" << sizeof(scalar_t) << " gno:" << sizeof(gno_t)<< endl;
                cout << "\t\t\t\t new arr reference count:" << rr0.count() << " "<< rr1.count() << " scalar_t size:" << sizeof(scalar_t) << " gno:" << sizeof(gno_t)<< endl;
                cout << "\t\t\t\tafter release new map reference count:" << vvectors->getMap().count() << endl;
            }
            r0 = r;
#ifdef memory_debug
            sleep(1); env->memory("\t\t\t\tafter r0 release-" + iteration);
#endif


            if (0 && pcomm->getRank() == 0){
                cout << "\t\t\t\t old array r0: " << r0.getRawPtr() << " r1:" << r1.getRawPtr() << endl;
                cout << "\t\t\t\t new rr0: " << rr0.getRawPtr() << " rr1:" << rr1.getRawPtr() << endl;
                cout << "\t\t\t\t old ar reference count:" << r0.count() << " "<< r1.count() << " scalar_t size:" << sizeof(scalar_t) << " gno:" << sizeof(gno_t)<< endl;
                cout << "\t\t\t\t new arr reference count:" << rr0.count() << " "<< rr1.count() << " scalar_t size:" << sizeof(scalar_t) << " gno:" << sizeof(gno_t)<< endl;
                cout << "\t\t\t\tafter release new map reference count:" << vvectors->getMap().count() << endl;
            }
        }

#ifdef memory_debug
        sleep(1); env->memory("\t\t\t\tafter r0 r1 release-" + iteration);
#endif

    }
    Z2_FORWARD_EXCEPTIONS
    env->timerStop(MACRO_TIMERS, "PQJagged Actual_Migration-" + iteration);

#ifdef memory_debug
    sleep(1); env->memory("\t\t\t\tAfter migrateData-" + iteration);
#endif
    return true;
}
template <typename mvector_t, typename lno_t, typename gno_t, typename part_id>
void create_sub_communicatior(
        const RCP<const Environment> &env,

        RCP<Comm<int> > &pcomm,
        RCP<Comm<int> > &comm,
        partId_t groupSize,
        partId_t *ids,
        RCP <const mvector_t> &mvector,
        int multiVectorDim,
        lno_t &numLocalCoords,
        gno_t &numGlobalCoords
){
    typedef ArrayView<const scalar_t> coordList_t;
    typedef Tpetra::Map<lno_t, gno_t, node_t> map_t;

    //#ifdef memory_debug
#ifdef memory_debug
    sleep(1); env->memory("Sub Comm -1 ");
#endif
    ArrayView<const partId_t> idView(ids, groupSize);

    //#ifdef memory_debug
#ifdef memory_debug
    sleep(1); env->memory("Sub Comm -2 ");
#endif

    /*
     cout << "###me:" << comm->getRank() << " groupSize:" << groupSize << endl;

     for(int i = 0; i < groupSize; ++i){
     cout << "####me:" << comm->getRank() << " id:" << ids[i]<< endl;
     }
     */
#ifdef memory_debug
    sleep(1); env->memory("\t\t\t\t\tsub before creating sub com");
#endif

    comm = comm->createSubcommunicator(idView);

#ifdef memory_debug
    sleep(1); env->memory("\t\t\t\t\tsub after creating sub com");
#endif
    //#ifdef memory_debug
#ifdef memory_debug
    sleep(1); env->memory("Sub Comm -3 ");
#endif
    ArrayView<const gno_t> gnoList = mvector->getMap()->getNodeElementList();
    //#ifdef memory_debug
#ifdef memory_debug
    sleep(1); env->memory("Sub Comm -4 ");
#endif
    size_t localSize = mvector->getLocalLength();
    // Tpetra will calculate the globalSize.
    size_t globalSize = Teuchos::OrdinalTraits<size_t>::invalid();

#ifdef memory_debug
    sleep(1); env->memory("Sub Comm -5 ");
#endif
    //TODO NOT SURE if the global size invalid is what I should do.
    //globalSize = mvector->getGlobalLength();
    //RCP<Comm<int> > comm2 = comm->duplicate();
#ifdef memory_debug
    sleep(1); env->memory("\t\t\t\t\tsub before creating submap");
#endif
    RCP<map_t> subMap;
    try{
        subMap= rcp(new map_t(globalSize, gnoList, 0, comm));
    }
    Z2_THROW_OUTSIDE_ERROR(*env)
#ifdef memory_debug
    sleep(1); env->memory("\t\t\t\t\tsub  after creating submap");
#endif

#ifdef memory_debug
    sleep(1); env->memory("Sub Comm -6 ");
#endif
    //coordList_t *avSubList = new coordList_t [multiVectorDim];
    coordList_t *avSubList = allocMemory<coordList_t>(multiVectorDim);


#ifdef memory_debug
    sleep(1); env->memory("Sub Comm -7 ");
#endif
    for (int dim=0; dim < multiVectorDim; dim++)
        avSubList[dim] = mvector->getData(dim).view(0, localSize);

#ifdef memory_debug
    sleep(1); env->memory("Sub Comm -8 ");
#endif

    ArrayRCP<const ArrayView<const scalar_t> > subVectors =
            arcp(avSubList, 0, multiVectorDim);

#ifdef memory_debug
    sleep(1); env->memory("Sub Comm -9 ");
#endif


#ifdef memory_debug
    sleep(1); env->memory("\t\t\t\t\tsub  before creating new multivector");
#endif
    RCP<const mvector_t> subMvector;

    try{
        subMvector = rcp(new mvector_t(
                subMap, subVectors.view(0, multiVectorDim), multiVectorDim));
    }
    Z2_THROW_OUTSIDE_ERROR(*env)
#ifdef memory_debug
    sleep(1); env->memory("\t\t\t\t\tsub  after creating new multivector");

    sleep(1); env->memory("Sub Comm -10 ");
#endif
    //if (comm->getRank() == 0) cout << " before sub mvector count:" << mvector.count() << endl;
    {
        RCP<const map_t> oldMap = mvector->getMap();
        mvector = subMvector;
#ifdef memory_debug
        sleep(1); env->memory("\t\t\t\t\tsub  after releasing old multivector");
#endif
        if(pcomm->getRank() == 0) {
            cout << "oldMap:" << oldMap.getRawPtr() << endl;
            cout << "newMap:" << subMap.getRawPtr() << endl;
        }
    }
#ifdef memory_debug
    sleep(1); env->memory("\t\t\t\t\tsub  after releasing old map");

    sleep(1); env->memory("Sub Comm -11 ");
#endif
    numLocalCoords = mvector->getLocalLength();
    numGlobalCoords = mvector->getGlobalLength();

}


template <typename gno_t, typename lno_t,typename scalar_t, typename node_t, typename partId_t>
bool migration(
        RCP<Comm<int> > &pcomm, //original communication.
        const RCP<const Environment> &env, //environment
        RCP<Comm<int> > &comm, //current communication object.
        RCP<const Tpetra::MultiVector<scalar_t, lno_t, gno_t, node_t> > &mvector, //multivector
        int pqJagged_multiVectorDim, //multivector dimension

        gno_t &numGlobalPoints, //numGlobal points, output
        lno_t &numLocalPoints, //numLocal points, output
        int coord_dim, // coordinate dimension
        scalar_t **coords, //coordinates.
        int weight_dim, //weight dimension
        scalar_t **weight, //weights
#ifdef migrate_gid
        scalar_t *&mappedGnos,
#endif

        partId_t * &assigned_parts_, //this should not be necessary anymore.
        partId_t num_parts, //current num parts
        partId_t &out_num_part, //output num parts.

        lno_t *&permutation,
        lno_t *&old_permutation,
        lno_t *part_begins,
        partId_t &partIndexBegin,
        partId_t futurePartIndex,
        int migration_option,
        int migration_check_option,
        scalar_t migration_imbalance_cut_off,
        string iteration,


        bool pqJagged_uniformWeights,
        scalar_t *coordWeights,
        bool allowNonRectelinearPart,
        float **nonRectelinearRatios,
        float *actual_ratios,
        scalar_t *localPartWeights,
        double **partWeights,
        scalar_t *cutCoordinates,
        int assignment_type
){


#ifdef memory_debug
    sleep(1); env->memory("Migration Start-" +iteration);
#endif
    partId_t nprocs = comm->getSize();
    scalar_t *assigned_parts = NULL;
    int multiVectorDim = pqJagged_multiVectorDim;
    int prev_num_local = numLocalPoints;

    typedef RCP<const Tpetra::MultiVector<scalar_t, lno_t, gno_t, node_t> >  tmv;
    typedef Tpetra::MultiVector<scalar_t, lno_t, gno_t, node_t> mvector_t;


    //partId_t *p_pid_np_num_procs_each_part = new partId_t[num_parts];
    partId_t *p_pid_np_num_procs_each_part = allocMemory<partId_t>(num_parts);

    //partId_t *p_pid_np_work_num_procs_each_part = new partId_t[num_parts];
    partId_t *p_pid_np_work_num_procs_each_part = allocMemory<partId_t>(num_parts);

    //partId_t *ids = new partId_t[nprocs];
    partId_t *ids = allocMemory<partId_t>(nprocs);
    partId_t groupSize = 0;

    if (
            !migrateData <mvector_t, gno_t, partId_t>(
                    pcomm,
                    env,
                    comm,
                    assigned_parts_,
                    mvector,
                    p_pid_np_num_procs_each_part,
                    p_pid_np_work_num_procs_each_part,
                    num_parts,
                    ids, groupSize,
                    out_num_part//, permutation
                    ,partIndexBegin,
                    futurePartIndex,
                    migration_option,
                    migration_check_option,
                    migration_imbalance_cut_off,
                    iteration,
#ifdef omitted2
                    pqJagged_uniformWeights,
                    coordWeights,
                    allowNonRectelinearPart,
                    nonRectelinearRatios,
                    actual_ratios,
                    localPartWeights,
                    partWeights,
                    cutCoordinates,
#endif
                    assigned_parts,
                    coord_dim,
                    coords,
                    weight_dim,
                    weight,
                    multiVectorDim,
                    assignment_type

            )
    )
    {

#ifdef memory_debug
        sleep(1); env->memory("\t\tafter migrate data comp-" + iteration);
#endif
        //delete []assigned_parts;
        freeArray<scalar_t>(assigned_parts);
        //delete []p_pid_np_num_procs_each_part;
        freeArray<partId_t>(p_pid_np_num_procs_each_part);
        //delete []p_pid_np_work_num_procs_each_part;
        freeArray<partId_t>(p_pid_np_work_num_procs_each_part);
        //delete []ids;
        freeArray<partId_t>(ids);
        return false;
    }
#ifdef memory_debug
    sleep(1); env->memory("\t\tafter migrate data comp-" + iteration);
#endif
    //delete []assigned_parts;
    freeArray<scalar_t>(assigned_parts);
    //delete []p_pid_np_num_procs_each_part;
    freeArray<partId_t>(p_pid_np_num_procs_each_part);
    //delete []p_pid_np_work_num_procs_each_part;
    freeArray<partId_t>(p_pid_np_work_num_procs_each_part);

#ifdef memory_debug
    sleep(1); env->memory("\t\tBefore subcommunicator creation-" + iteration);
#endif
    create_sub_communicatior<mvector_t, lno_t, gno_t, partId_t>(
            env,pcomm, comm,
            groupSize,
            ids,
            mvector, multiVectorDim,numLocalPoints,
            numGlobalPoints
    );
#ifdef memory_debug
    sleep(1); env->memory("\t\tAfter subcommunicator creation-" + iteration);
#endif
    //delete []ids;
    freeArray<partId_t>(ids);

    if (prev_num_local != numLocalPoints){
        //cout << "reallocating:" << prev_num_local << " now:" << numLocalPoints << endl;
        //delete []assigned_parts_;
        freeArray<partId_t>(assigned_parts_);
        //assigned_parts_ = new partId_t[numLocalPoints];
        assigned_parts_ = allocMemory<partId_t>(numLocalPoints);

        //delete []permutation;
        freeArray<lno_t>(permutation);
        //delete []old_permutation;
        freeArray<lno_t>(old_permutation);
#ifdef migrate_gid
        //delete [] mappedGnos;
        mappedGnos = new scalar_t [numLocalPoints];
#endif
        //old_permutation = new lno_t[numLocalPoints];
        old_permutation = allocMemory<lno_t>(numLocalPoints);
        //permutation = new lno_t[numLocalPoints];
        permutation = allocMemory<lno_t>(numLocalPoints);
    }


#ifdef memory_debug
    sleep(1); env->memory("\t\tBefore get multi-" + iteration);
#endif
    get_multi_vector<gno_t,lno_t,scalar_t,node_t>(pcomm,
            comm,
            numLocalPoints, numGlobalPoints,
            coord_dim,
            coords,
            weight_dim,
            weight,
#ifdef migrate_gid
            mappedGnos,
#endif
            assigned_parts,
            mvector,
            out_num_part, permutation,
            part_begins,
            num_parts,pqJagged_multiVectorDim, iteration);

#ifdef memory_debug
    sleep(1); env->memory("\t\tAfter get multi-" + iteration);

    sleep(1); env->memory("Migration End-" +iteration);
#endif
    return true;
}

#endif



template <typename partId_t>
partId_t getPartitionArrays(
        const partId_t *partNo,
        vector <partId_t> &pAlongI, //assumes this vector is empty.
        vector<partId_t> *currentPartitions,
        vector<partId_t> *newFuturePartitions, //assumes this vector is empty.
        partId_t &futurePartNumbers,

        partId_t currentPartitionCount,
        int partArraySize,
        int i,
        partId_t maxPartNo
){
    partId_t outPartCount = 0;
    if(partNo){
        //when the partNo array is provided as input,
        //each current partition will be partition to the same number of parts.
        //we dont need to use the currentPartition vector in this case.

        partId_t p = partNo[i];
        if (p < 1){
            cout << "i:" << i << " p is given as:" << pAlongI[0] << endl;
            exit(1);
        }
        if (p == 1){
            return currentPartitionCount;
        }

        for (partId_t ii = 0; ii < currentPartitionCount; ++ii){
            pAlongI.push_back(p);
        }

        //TODO this should be removed.
        futurePartNumbers /= pAlongI[0];
        outPartCount = currentPartitionCount * pAlongI[0];

        //set the how many more parts each part will be divided.
        //this is obvious when partNo array is provided as input.
        //however, fill this so that weights will be calculated according to this array.
        for (partId_t ii = 0; ii < outPartCount; ++ii){
            newFuturePartitions->push_back(futurePartNumbers);
        }
    }
    else {
        //if partNo array is not provided as input,
        //currentPartitions  hold how many parts each part should be divided.
        //initially it has single number with total number of global parts.

        //calculate the futurePartNumbers from beginning,
        //since each part might be divided into different number of parts.
        futurePartNumbers = 1; //TODO this should be removed.

        //cout << "i:" << i << endl;
        float fEpsilon = numeric_limits<float>::epsilon();
        for (partId_t ii = 0; ii < currentPartitionCount; ++ii){
            //get how many parts a part should be divided.
            partId_t numFuture = (*currentPartitions)[ii];

            //get the ideal number of parts that is close to the
            //(partArraySize - i) root of the numFuture.
            partId_t numParts = getPartCount<partId_t>( numFuture, 1.0f / (partArraySize - i), fEpsilon);
            //partId_t numParts = ceil( pow(numFuture, 1.0f / (partArraySize - i)));// + 0.5f;

            //cout << "\tnumParts:" << numParts << endl;
            if (numParts > maxPartNo){
                cerr << "ERROR: maxPartNo calculation is wrong." << endl;
                exit(1);
            }
            //add this number to pAlonI vector.
            pAlongI.push_back(numParts);


            //increase the output number of parts.
            outPartCount += numParts;

            //ideal number of future partitions for each part.
            partId_t idealNumFuture = numFuture / numParts;
            for (partId_t iii = 0; iii < numParts; ++iii){
                partId_t fNofCuts = idealNumFuture;

                if (iii < numFuture % numParts){
                    //if not uniform, add 1 for the extra parts.
                    ++fNofCuts;
                }
                newFuturePartitions->push_back(fNofCuts);
                //TODO this should be removed.
                if (fNofCuts > futurePartNumbers) futurePartNumbers = fNofCuts;
            }
        }

    }
    return outPartCount;
}

template <typename scalar_t, typename lno_t, typename partId_t>
void getInitialPartAssignments(
        scalar_t &maxCoordinate,
        scalar_t &minCoordinate,
        partId_t &currentPart,
        lno_t *inTotalCounts,
        lno_t *partitionedPointCoordinates,
        scalar_t *pqCoord,
        partId_t *partIds,
        scalar_t _EPSILON,
        partId_t &partition
){
    scalar_t coordinate_range = maxCoordinate - minCoordinate;
    //partId_t currentPart = currentWorkPart + kk;
    lno_t coordinateEnd= inTotalCounts[currentPart];
    lno_t coordinateBegin = currentPart==0 ? 0: inTotalCounts[currentPart -1];

    //if there is single point, or if all points are along a line.
    //set initial part to 0 for all.
    if(ABS(coordinate_range) < _EPSILON ){
        for(lno_t ii = coordinateBegin; ii < coordinateEnd; ++ii){
            partIds[partitionedPointCoordinates[ii]] = 0;
        }
    }
    else{

        //otherwise estimate an initial part for each coordinate.
        //assuming uniform distribution of points.
        scalar_t slice = coordinate_range / partition;

#ifdef HAVE_ZOLTAN2_OMP
#pragma omp parallel for
#endif
        for(lno_t ii = coordinateBegin; ii < coordinateEnd; ++ii){

            lno_t iii = partitionedPointCoordinates[ii];
            partId_t pp = partId_t((pqCoord[iii] - minCoordinate) / slice);
            partIds[iii] = 2 * pp;
        }
    }
}

/*! \brief PQJagged coordinate partitioning algorithm.
 *
 *  \param env   library configuration and problem parameters
 *  \param comm the communicator for the problem
 *  \param coords    a CoordinateModel with user data
 *  \param solution  a PartitioningSolution, on input it
 *      contains part information, on return it also contains
 *      the solution and quality metrics.
 */

template <typename Adapter>
void AlgPQJagged(
        const RCP<const Environment> &env,
        RCP<Comm<int> > &problemComm,
        const RCP<const CoordinateModel<
        typename Adapter::base_adapter_t> > &coords,
        RCP<PartitioningSolution<Adapter> > &solution
)
{
#ifndef INCLUDE_ZOLTAN2_EXPERIMENTAL

    Z2_THROW_EXPERIMENTAL("Zoltan2 PQJagged is strictly experimental software "
            "while it is being developed and tested.")

#else
            /*
             *   typedef typename Adapter::scalar_t scalar_t;
             *     typedef typename Adapter::gno_t gno_t;
             *       typedef typename Adapter::lno_t lno_t;
     if(comm->getRank() == 0){
     cout << "size of gno:" << sizeof(gno_t) << endl;
     cout << "size of lno:" << sizeof(lno_t) << endl;
     cout << "size of scalar_t:" << sizeof(scalar_t) << endl;
     }
             */
            env->timerStart(MACRO_TIMERS, "PQJagged Total");
    env->timerStart(MACRO_TIMERS, "PQJagged Total2");
    // 0 - for decision
    // > 0 - for force migration
    // < 0 - for avoid migration

    env->timerStart(MACRO_TIMERS, "PQJagged Problem_Init");
    RCP<Comm<int> > comm = problemComm->duplicate();

    typedef typename Adapter::scalar_t scalar_t;
    typedef typename Adapter::gno_t gno_t;

    typedef typename Adapter::lno_t lno_t;
    const Teuchos::ParameterList &pl = env->getParameters();

    std::bitset<NUM_RCB_PARAMS> params;
    int numTestCuts = 5;

    int migration_check_option = 0;
    int migration_option = 1;
    scalar_t migration_imbalance_cut_off = 0.03;
    int assignment_type = 0;

    scalar_t imbalanceTolerance;

    multiCriteriaNorm mcnorm;
    bool ignoreWeights=false;

    bool allowNonRectelinearPart = false;
    int concurrentPartCount = 0; // Set to invalid value
    pqJagged_getParameters<scalar_t>(pl,
            imbalanceTolerance,
            mcnorm,
            params,
            numTestCuts,
            ignoreWeights,
            allowNonRectelinearPart,
            concurrentPartCount,
            //force_binary,
            //force_linear,
            migration_check_option,
            migration_option,
            migration_imbalance_cut_off,
            assignment_type);

    if (migration_check_option > 1) migration_check_option = -1;

    //allowNonRectelinearPart = false;

    int coordDim, weightDim; size_t nlc; global_size_t gnc; int criteriaDim;
    pqJagged_getCoordinateValues<Adapter>(
            coords,
            coordDim,
            weightDim,
            nlc,
            gnc,
            criteriaDim,
            ignoreWeights
    );
    lno_t numLocalCoords = nlc;
#ifdef enable_migration
    gno_t numGlobalCoords = gnc;
#endif

    //allocate only two dimensional pointer.
    //raw pointer addresess will be obtained from multivector.
    scalar_t **pqJagged_coordinates = allocMemory<scalar_t *>(coordDim);
    scalar_t **pqJagged_weights = allocMemory<scalar_t *>(criteriaDim);
    bool *pqJagged_uniformParts = allocMemory< bool >(criteriaDim); //if the partitioning results wanted to be uniform.
    scalar_t **pqJagged_partSizes =  allocMemory<scalar_t *>(criteriaDim); //if in a criteria dimension, uniform part is false this shows ratios of the target part weights.
    bool *pqJagged_uniformWeights = allocMemory< bool >(criteriaDim); //if the weights of coordinates are uniform in a criteria dimension.

    ArrayView<const gno_t> pqJagged_gnos;
    size_t numGlobalParts;
    int pqJagged_multiVectorDim;

    pqJagged_getInputValues<Adapter, scalar_t, gno_t>(
            env,
            coords,
            solution,
            params,
            coordDim,
            weightDim,
            numLocalCoords,
            numGlobalParts,
            pqJagged_multiVectorDim,
            pqJagged_coordinates,
            criteriaDim,
            pqJagged_weights,
            pqJagged_gnos,
            ignoreWeights,
            pqJagged_uniformWeights,
            pqJagged_uniformParts,
            pqJagged_partSizes
    );

    int numThreads = 1;

#ifdef HAVE_ZOLTAN2_OMP
#pragma omp parallel shared(numThreads)
    {
        numThreads = omp_get_num_threads();
    }

#endif


    partId_t totalDimensionCut = 0;
    partId_t totalPartCount = 1;
    partId_t maxPartNo = 0;

    partId_t reduceAllCount = 0;

    const partId_t *partNo = NULL;

    partId_t maxTotalCumulativePartCount = 1;//

    int partArraySize = 0;

    if (pl.getPtr<Array <partId_t> >("pqParts")){
        partNo = pl.getPtr<Array <partId_t> >("pqParts")->getRawPtr();
        partArraySize = pl.getPtr<Array <partId_t> >("pqParts")->size() - 1;

        for (int i = 0; i < partArraySize; ++i){
            reduceAllCount += totalPartCount;
            totalPartCount *= partNo[i];
            if(partNo[i] > maxPartNo) maxPartNo = partNo[i];
        }

        maxTotalCumulativePartCount = totalPartCount / partNo[partArraySize - 1];
        numGlobalParts = totalPartCount;
    } else {
        float fEpsilon = numeric_limits<float>::epsilon();
        partArraySize = coordDim;
        partId_t futureNumParts = numGlobalParts;
        for (int i = 0; i < coordDim; ++i){
            partId_t maxNoPartAlongI = getPartCount<partId_t>( futureNumParts, 1.0f / (coordDim - i), fEpsilon);
            //partId_t maxNoPartAlongI = ceil(pow(futureNumParts, 1.0f / (coordDim - i)));// + 0.5f;
            if (maxNoPartAlongI > maxPartNo){
                maxPartNo = maxNoPartAlongI;
            }

            partId_t nfutureNumParts = futureNumParts / maxNoPartAlongI;
            if (futureNumParts % maxNoPartAlongI){
                ++nfutureNumParts;
            }
            futureNumParts = nfutureNumParts;
            //cout << "i:" << i << " fp:" << futureNumParts << endl;
        }
        totalPartCount = numGlobalParts;
        //estimate reduceAll Count here.
        //we find the upperbound instead.
        partId_t p = 1;
        for (int i = 0; i < coordDim; ++i){
            reduceAllCount += p;
            p *= maxPartNo;
        }
        maxTotalCumulativePartCount  = p / maxPartNo;
    }

    //cout << "maxPartNo:" << maxPartNo << endl;


    totalDimensionCut = totalPartCount - 1;
    partId_t maxCutNo = maxPartNo - 1;
    size_t maxTotalPartCount = maxPartNo + size_t(maxCutNo);
    //maxPartNo is P, maxCutNo = P-1, matTotalPartcount = 2P-1
    if (concurrentPartCount == 0)
    {
        // User did not specify concurrentPartCount parameter, Pick a default.
        // Still a conservative default. We could go as big as
        // maxTotalCumulativePartCount, but trying not to use too much memory.
        // Another assumption the total #parts will be large-very large
        if (coordDim == partArraySize)
        {
            // partitioning each dimension only once, pick the default as
            // maxPartNo >> default
            concurrentPartCount = min(Z2_DEFAULT_CON_PART_COUNT, maxPartNo);
        }
        else
        {
            // partitioning each dimension more than once, pick the max
            concurrentPartCount = max(Z2_DEFAULT_CON_PART_COUNT, maxPartNo);
        }
    }

    if(concurrentPartCount > maxTotalCumulativePartCount){
        if(comm->getRank() == 0){
            cerr << "Warning: Concurrent part calculation count ("<< concurrentPartCount << ") has been set bigger than maximum amount that can be used." << " Setting to:" << maxTotalCumulativePartCount << "." << endl;
        }
        concurrentPartCount = maxTotalCumulativePartCount;
    }

    // coordinates of the cut lines. First one is the min, last one is max coordinate.
    // kddnote if (keep_cuts)
    // coordinates of the cut lines.
    //only store this much if cuts are needed to be stored.
    scalar_t *allCutCoordinates = allocMemory< scalar_t>(totalDimensionCut);
    // kddnote else
    //scalar_t *allCutCoordinates = allocMemory< scalar_t>(maxCutNo * concurrentPartCount);

    //as input indices.
    lno_t *partitionedPointCoordinates =  allocMemory< lno_t>(numLocalCoords);
    //as output indices
    lno_t *newpartitionedPointCoordinates = allocMemory< lno_t>(numLocalCoords);
    scalar_t *max_min_array =  allocMemory< scalar_t>(numThreads * 2);

    //initial configuration
    //set each pointer-i to i.
#ifdef HAVE_ZOLTAN2_OMP
#pragma omp parallel for
#endif
    for(size_t i = 0; i < static_cast<size_t>(numLocalCoords); ++i){
        partitionedPointCoordinates[i] = i;
    }

#ifdef HAVE_ZOLTAN2_OMP
#ifdef FIRST_TOUCH
    firstTouch<lno_t>(newpartitionedPointCoordinates, numLocalCoords);
#endif
#endif

    //initially there is a single partition
    partId_t currentPartitionCount = 1;
    //single partition starts at index-0, and ends at numLocalCoords

    //inTotalCounts array holds the end points in partitionedPointCoordinates array
    //for each partition. Initially sized 1, and single element is set to numLocalCoords.
    lno_t *inTotalCounts = allocMemory<lno_t>(1);
    inTotalCounts[0] = static_cast<lno_t>(numLocalCoords);//the end of the initial partition is the end of coordinates.

    //the ends points of the output.
    lno_t *outTotalCounts = NULL;

    //the array holding the points in each part as linked list
    //lno_t *coordinate_linked_list = allocMemory<lno_t>(numLocalCoords);
    //the start and end coordinate of  each part.
    //lno_t **coordinate_starts =  allocMemory<lno_t *>(numThreads);
    //lno_t **coordinate_ends = allocMemory<lno_t *>(numThreads);

    //assign the max size to starts, as it will be reused.
    /*
     for(int i = 0; i < numThreads; ++i){
     coordinate_starts[i] = allocMemory<lno_t>(maxPartNo);
     coordinate_ends[i] = allocMemory<lno_t>(maxPartNo);
     }
     */


#ifdef HAVE_ZOLTAN2_OMP
#ifdef FIRST_TOUCH
#pragma omp parallel
    {
        int me = omp_get_thread_num();
        for(partId_t ii = 0; ii < maxPartNo; ++ii){
            coordinate_starts[me][ii] = 0;
            coordinate_ends[me][ii] = 0;
        }
    }
#endif
#endif




    float *nonRectelinearPart = NULL; //how much weight percentage should a MPI put left side of the each cutline
    float **nonRectRatios = NULL; //how much weight percentage should each thread in MPI put left side of the each cutline

    if(allowNonRectelinearPart){
        //cout << "allowing" << endl;
        nonRectelinearPart = allocMemory<float>(maxCutNo * concurrentPartCount);
#ifdef HAVE_ZOLTAN2_OMP
#ifdef FIRST_TOUCH
        firstTouch<float>(nonRectelinearPart, maxCutNo);
#endif
#endif

        nonRectRatios = allocMemory<float *>(numThreads);

        for(int i = 0; i < numThreads; ++i){
            nonRectRatios[i] = allocMemory<float>(maxCutNo);
        }
#ifdef HAVE_ZOLTAN2_OMP
#ifdef FIRST_TOUCH
#pragma omp parallel
        {
            int me = omp_get_thread_num();
            for(partId_t ii = 0; ii < maxCutNo; ++ii){
                nonRectRatios[me][ii] = 0;
            }
        }
#endif
#endif

    }



    // work array to manipulate coordinate of cutlines in different iterations.
    //necessary because previous cut line information is used for determining the next cutline information.
    //therefore, cannot update the cut work array until all cutlines are determined.
    scalar_t *cutCoordinatesWork = allocMemory<scalar_t>(maxCutNo * concurrentPartCount);
#ifdef HAVE_ZOLTAN2_OMP
#ifdef FIRST_TOUCH
    firstTouch<scalar_t>(cutCoordinatesWork, maxCutNo);
#endif
#endif

    //cumulative part weight ratio array.
    scalar_t *targetPartWeightRatios = allocMemory<scalar_t>(maxPartNo * concurrentPartCount); // the weight ratios at left side of the cuts. First is 0, last is 1.
#ifdef HAVE_ZOLTAN2_OMP
#ifdef FIRST_TOUCH
    firstTouch<scalar_t>(cutPartRatios, maxCutNo);
#endif
#endif


    scalar_t *cutUpperBounds = allocMemory<scalar_t>(maxCutNo * concurrentPartCount);  //upper bound coordinate of a cut line
    scalar_t *cutLowerBounds = allocMemory<scalar_t>(maxCutNo* concurrentPartCount);  //lower bound coordinate of a cut line
    scalar_t *cutLowerWeight = allocMemory<scalar_t>(maxCutNo* concurrentPartCount);  //lower bound weight of a cut line
    scalar_t *cutUpperWeight = allocMemory<scalar_t>(maxCutNo* concurrentPartCount);  //upper bound weight of a cut line

    scalar_t *localMinMaxTotal = allocMemory<scalar_t>(3 * concurrentPartCount); //combined array to exchange the min and max coordinate, and total weight of part.
    scalar_t *globalMinMaxTotal = allocMemory<scalar_t>(3 * concurrentPartCount);//global combined array with the results for min, max and total weight.

    //isDone is used to determine if a cutline is determined already.
    //If a cut line is already determined, the next iterations will skip this cut line.
    bool *isDone = allocMemory<bool>(maxCutNo * concurrentPartCount);
    //myNonDoneCount count holds the number of cutlines that have not been finalized for each part
    //when concurrentPartCount>1, using this information, if myNonDoneCount[x]==0, then no work is done for this part.
    partId_t *myNonDoneCount =  allocMemory<partId_t>(concurrentPartCount);
    //local part weights of each thread.
    double **partWeights = allocMemory<double *>(numThreads);
    //the work manupulation array for partweights.
    double **pws = allocMemory<double *>(numThreads);

    //leftClosesDistance to hold the min distance of a coordinate to a cutline from left (for each thread).
    scalar_t **leftClosestDistance = allocMemory<scalar_t *>(numThreads);
    //leftClosesDistance to hold the min distance of a coordinate to a cutline from right (for each thread)
    scalar_t **rightClosestDistance = allocMemory<scalar_t *>(numThreads);

    //to store how many points in each part a thread has.
    lno_t **partPointCounts = allocMemory<lno_t *>(numThreads);

    for(int i = 0; i < numThreads; ++i){
        //partWeights[i] = allocMemory<scalar_t>(maxTotalPartCount);
        partWeights[i] = allocMemory < double >(maxTotalPartCount * concurrentPartCount);
        rightClosestDistance[i] = allocMemory<scalar_t>(maxCutNo * concurrentPartCount);
        leftClosestDistance[i] = allocMemory<scalar_t>(maxCutNo * concurrentPartCount);
        partPointCounts[i] =  allocMemory<lno_t>(maxPartNo);
    }
#ifdef HAVE_ZOLTAN2_OMP
#ifdef FIRST_TOUCH
#pragma omp parallel
    {
        int me = omp_get_thread_num();
        for(partId_t ii = 0; ii < maxPartNo; ++ii){
            partPointCounts[me][ii] = 0;
        }
        for(size_t ii = 0; ii < maxTotalPartCount; ++ii){
            partWeights[me][ii] = 0;
        }
        for(partId_t ii = 0; ii < maxCutNo; ++ii){
            rightClosestDistance[me][ii] = 0;
            leftClosestDistance[me][ii] = 0;

        }
    }
#endif
#endif
    // kddnote  Needed only when non-rectilinear parts.
    scalar_t *cutWeights = allocMemory<scalar_t>(maxCutNo);
    scalar_t *globalCutWeights = allocMemory<scalar_t>(maxCutNo);

    //for faster communication, concatanation of
    //totalPartWeights sized 2P-1, since there are P parts and P-1 cut lines
    //leftClosest distances sized P-1, since P-1 cut lines
    //rightClosest distances size P-1, since P-1 cut lines.
    scalar_t *totalPartWeights_leftClosests_rightClosests = allocMemory<scalar_t>((maxTotalPartCount + maxCutNo * 2) * concurrentPartCount);
    scalar_t *global_totalPartWeights_leftClosests_rightClosests = allocMemory<scalar_t>((maxTotalPartCount + maxCutNo * 2) * concurrentPartCount);

    partId_t *partIds = NULL;
    ArrayRCP<partId_t> partId;
    if(numLocalCoords > 0){
        partIds = allocMemory<partId_t>(numLocalCoords);
    }

    scalar_t *cutCoordinates =  allCutCoordinates;

    //partId_t leftPartitions = totalPartCount;
    scalar_t maxScalar_t = numeric_limits<scalar_t>::max();
    scalar_t minScalar_t = -numeric_limits<scalar_t>::max();

    env->timerStop(MACRO_TIMERS, "PQJagged Problem_Init");

    env->timerStart(MACRO_TIMERS, "PQJagged Problem_Partitioning");

    //create multivector based on the coordinates, weights, and multivectordim
#ifdef enable_migration
#ifdef migrate_gid
    scalar_t *mappedGnos =  allocMemory<scalar_t> (numLocalCoords);
    for(int i = 0; i < numLocalCoords; ++i){
        mappedGnos[i] = scalar_t (pqJagged_gnos[i]);
    }
    pqJagged_multiVectorDim += 1;
#endif
    typedef Tpetra::MultiVector<scalar_t, lno_t, gno_t, node_t> mvector_t;

#ifdef memory_debug
    sleep(1); env->memory("before create initial");
#endif
    RCP<const mvector_t> mvector =
            create_initial_multi_vector <gno_t,lno_t,scalar_t,node_t>(
                    env,
                    comm,
                    numGlobalCoords,
                    numLocalCoords,
                    coordDim,//                        coord_dim,
                    pqJagged_coordinates,//coords,
                    weightDim, //weight_dim,
                    pqJagged_weights,  //weight,
#ifdef migrate_gid
                    mappedGnos,
#endif
                    pqJagged_multiVectorDim// multiVectorDim
            );

#ifdef memory_debug
    sleep(1); env->memory("after create initial");
#endif
    //int myRank = comm->getRank();
#endif


    scalar_t _EPSILON = numeric_limits<scalar_t>::epsilon();
    partId_t partIndexBegin = 0;
    partId_t futurePartNumbers = totalPartCount;
    bool is_data_migrated = false;

    vector<partId_t> *currentPartitions = new vector<partId_t> ();
    vector<partId_t> *newFuturePartitions = new vector<partId_t> ();
    newFuturePartitions->push_back(numGlobalParts);

    for (int i = 0; i < partArraySize; ++i){

        //partitioning array.
        //size will be as the number of current partitions
        //and this hold how many parts each part will be
        //in the current dimension partitioning.
        vector <partId_t> pAlongI;

        //number of parts that will be obtained at the end of this partitioning.



        //currentPartitions is as the size of current number of parts.
        //holds how many more parts each should be divided in the further iterations.
        //this will be used to calculate pAlongI,
        //as the number of parts that the part will be partitioned
        //in the current dimension partitioning.

        //newFuturePartitions will be as the size of outnumParts,
        //and this will hold how many more parts that each output part
        //should be divided.
        //this array will also be used to determine the weight ratios
        //of the parts.
        //swap the arrays.
        vector<partId_t> *tmpPartVect= currentPartitions;
        currentPartitions = newFuturePartitions;
        newFuturePartitions = tmpPartVect;

        //clear newFuturePartitions array as
        //getPartitionArrays expects it to be empty.
        //it also expects pAlongI to be empty as well.
        newFuturePartitions->clear();

        //returns the total number of output parts for this dimension partitioning.
        partId_t outPartCount = getPartitionArrays<partId_t>(
                partNo,
                pAlongI,
                currentPartitions,
                newFuturePartitions,
                futurePartNumbers,
                currentPartitionCount,
                partArraySize,
                i,
                maxPartNo);

        if(outPartCount == currentPartitionCount) {
            tmpPartVect= currentPartitions;
            currentPartitions = newFuturePartitions;
            newFuturePartitions = tmpPartVect;
            continue;
        }
        /*
    if (comm->getRank() == 0)
    for (int ik = 0; ik < currentPartitions->size(); ++ik){
        cout << "i:" << ik << " currentPartitions:" << (*currentPartitions)[ik] << endl;
    }
         */
#ifdef enable_migration
        int worldSize = comm->getSize();
        long reduceAllPop = reduceAllCount * worldSize;
#endif


        //get the coordinate axis along which the partitioning will be done.
        int coordInd = i % coordDim;
        scalar_t * pqCoord = pqJagged_coordinates[coordInd];
        //convert i to string to be used for debugging purposes.
        string istring = toString<int>(i);


        env->timerStart(MACRO_TIMERS, "PQJagged Problem_Partitioning_" + istring);

        //alloc Memory to point the indices
        //of the parts in the permutation array.
        outTotalCounts = allocMemory<lno_t>(outPartCount);

        //the index where in the outtotalCounts will be written.
        partId_t currentOut = 0;
        //whatever is written to outTotalCounts will be added with previousEnd so that the points will be shifted.
        partId_t previousEnd = 0;

        partId_t currentWorkPart = 0;
        partId_t concurrentPart = min(currentPartitionCount - currentWorkPart, concurrentPartCount);

        //always use binary search algorithm.
        bool useBinarySearch = true;


        bool is_migrated_in_current = false;

        partId_t obtainedPartCount = 0;

        //run for all available parts.
        for (; currentWorkPart < currentPartitionCount; currentWorkPart += concurrentPart){

            concurrentPart = min(currentPartitionCount - currentWorkPart, concurrentPartCount);
#ifdef mpi_communication
            concurrent = concurrentPart;
#endif


            partId_t workPartCount = 0;
            //get the min and max coordinates of each part
            //together with the part weights of each part.
            for(int kk = 0; kk < concurrentPart; ++kk){
                partId_t currentPart = currentWorkPart + kk;

                //if this part wont be partitioned any further
                //dont do any work for this part.
                if (pAlongI[currentPart] == 1){
                    continue;
                }
                ++workPartCount;
                lno_t coordinateEnd= inTotalCounts[currentPart];
                lno_t coordinateBegin = currentPart==0 ? 0: inTotalCounts[currentPart -1];
                //cout << "begin:" << coordinateBegin  << " end:" << coordinateEnd << endl;
                pqJagged_getLocalMinMaxTotalCoord<scalar_t, lno_t>(
                        partitionedPointCoordinates,
                        pqCoord,
                        pqJagged_uniformWeights[0],
                        pqJagged_weights[0],
                        numThreads,
                        coordinateBegin,
                        coordinateEnd,
                        max_min_array,
                        maxScalar_t,
                        minScalar_t,
                        localMinMaxTotal[kk], //min coordinate
                        localMinMaxTotal[kk + concurrentPart], //max coordinate
                        localMinMaxTotal[kk + 2*concurrentPart] //total weight);
                                         ,problemComm
                );
            }


            if (workPartCount > 0){
                //obtain global Min max of the part.
                pqJagged_getGlobalMinMaxTotalCoord<scalar_t>(
                        comm,
                        env,
                        concurrentPart,
                        localMinMaxTotal,
                        globalMinMaxTotal);

                //represents the total number of cutlines
                //whose coordinate should be determined.
                partId_t allDone = 0;

                //Compute weight ratios for parts & cuts:
                //e.g., 0.25  0.25  0.5    0.5  0.75 0.75  1
                //part0  cut0  part1 cut1 part2 cut2 part3
                partId_t cutShifts = 0;
                partId_t partShift = 0;
                for(int kk = 0; kk < concurrentPart; ++kk){
                    scalar_t minCoordinate = globalMinMaxTotal[kk];
                    scalar_t maxCoordinate = globalMinMaxTotal[kk + concurrentPart];

                    partId_t currentPart = currentWorkPart + kk;
                    partId_t partition = pAlongI[currentPart];

                    scalar_t *usedCutCoordinate = cutCoordinates + cutShifts;
                    scalar_t *usedCutPartRatios = targetPartWeightRatios + partShift;
                    //shift the usedCutCoordinate array as noCuts.
                    cutShifts += partition - 1;
                    //shift the partRatio array as noParts.
                    partShift += partition;

                    //cout << "min:" << minCoordinate << " max:" << maxCoordinate << endl;
                    //calculate only if part is not empty,
                    //and part will be further partitioend.
                    if(partition > 1 && minCoordinate <= maxCoordinate){

                        //increase allDone by the number of cuts of the current part's cut line number.
                        allDone += partition - 1;
                        //set the number of cut lines that should be determined for this part.
                        myNonDoneCount[kk] = partition - 1;

                        //get the target weights of the parts.
                        pqJagged_getCutCoord_Weights<scalar_t>(
                                minCoordinate,
                                maxCoordinate,
                                pqJagged_uniformParts[0],
                                pqJagged_partSizes[0],
                                partition - 1,
                                usedCutCoordinate,
                                usedCutPartRatios,
                                numThreads,
#ifdef omitted
                                partNo,
#endif
                                currentPartitions,
                                newFuturePartitions,
                                currentPart,
                                obtainedPartCount
                        );

                        //get the initial estimated part assignments of the coordinates.
                        getInitialPartAssignments<scalar_t, lno_t, partId_t>(
                                maxCoordinate,
                                minCoordinate,
                                currentPart,
                                inTotalCounts,
                                partitionedPointCoordinates,
                                pqCoord,
                                partIds,
                                _EPSILON,
                                partition
                        );
                    }
                    else {
                        // e.g., if have fewer coordinates than parts, don't need to do next dim.
                        myNonDoneCount[kk] = 0;
                    }
                    obtainedPartCount += partition;
                }



                //used imbalance, it is always 0, as it is difficult to estimate a range.
                scalar_t used_imbalance = 0;


                // Determine cut lines for k parts here.
                pqJagged_1D_Partition<scalar_t, lno_t>(
                        env,
                        comm,
                        partitionedPointCoordinates,
                        pqCoord,
                        pqJagged_uniformWeights[0],
                        pqJagged_weights[0],
                        targetPartWeightRatios,
                        globalMinMaxTotal,
                        localMinMaxTotal,
                        //pAlongI[0],
                        numThreads,
                        maxScalar_t,
                        minScalar_t,
                        used_imbalance,
                        currentWorkPart,
                        concurrentPart,
                        inTotalCounts,
                        cutCoordinates,
                        cutCoordinatesWork,
                        leftClosestDistance,
                        rightClosestDistance,
                        cutUpperBounds,
                        cutLowerBounds,
                        cutUpperWeight,
                        cutLowerWeight,
                        isDone,
                        partWeights,
                        totalPartWeights_leftClosests_rightClosests,
                        global_totalPartWeights_leftClosests_rightClosests,
                        allowNonRectelinearPart,
                        nonRectelinearPart,
                        cutWeights,
                        globalCutWeights,
                        allDone,
                        myNonDoneCount,
                        useBinarySearch, // istring,
                        partIds,
#ifdef omitted
                        partNo,
#endif
                        pAlongI
                );



            }
            bool migration_check = false;

#ifdef enable_migration2
            if (futurePartNumbers > 1 && migration_check_option >= 0 && worldSize > 1 && currentPartitionCount == 1){
                env->timerStart(MACRO_TIMERS, "PQJagged Problem_Migration-" + istring);
                int mco = migration_check_option;
                if (reduceAllPop >= forceMigration ){
                    mco = 1;
                }
                partId_t num_parts = pAlongI[0];
                if (
                        migration<gno_t, lno_t, scalar_t, node_t, partId_t>(
                                problemComm,
                                env, comm,
                                mvector, pqJagged_multiVectorDim,
                                numGlobalCoords,//numGlobalPoints,
                                numLocalCoords,
                                coordDim,
                                pqJagged_coordinates, //outout will be modified.
                                weightDim,
                                pqJagged_weights,// output will be modified.
#ifdef migrate_gid
                                mappedGnos,
#endif
                                partIds, num_parts,
                                currentPartitionCount, //output
                                newpartitionedPointCoordinates, //output
                                partitionedPointCoordinates,
                                outTotalCounts //output
                                ,partIndexBegin,
                                futurePartNumbers,
                                migration_option,
                                mco, //migration_check_option,
                                migration_imbalance_cut_off,
                                istring,

                                pqJagged_uniformWeights[0],
                                pqJagged_weights[0],
                                allowNonRectelinearPart,
                                nonRectRatios,
                                nonRectelinearPart,
                                totalPartWeights_leftClosests_rightClosests,
                                partWeights,
                                cutCoordinates,
                                assignment_type
                        )
                )
                {

                    is_migrated_in_current = true;
                    is_data_migrated = true;
                    env->timerStop(MACRO_TIMERS, "PQJagged Problem_Migration-" + istring);
                    reduceAllCount /= num_parts;

                    break;

                }
                else {
                    //          cout <<"me:" <<problemComm->getRank()<< " me2:" << comm->getRank() << " i:" << i << " migration is not done" << endl;
                    //          cout  <<"me:" <<problemComm->getRank()<< " me2:" << comm->getRank() << pqJagged_coordinates[0][0] << endl;
                    env->timerStop(MACRO_TIMERS, "PQJagged Problem_Migration-" + istring);

                    is_migrated_in_current = false;
                    migration_check = true;


                }
                //cout << "numGlobal:" << numGlobalCoords << " numLocal:" << numLocalCoords << endl;
                //break;
            }
#endif


            if(!is_migrated_in_current) {

                partId_t outShift = 0;
                partId_t cutShift = 0;
                size_t tlrShift = 0;
                size_t pwShift = 0;

                for(int kk = 0; kk < concurrentPart; ++kk){
                    partId_t curr = currentWorkPart + kk;
#ifdef omitted
                    partId_t noParts = pAlongI[0];
                    if(!partNo){
                        noParts = pAlongI[curr];
                    }
#endif
#ifndef omitted
                    partId_t noParts = pAlongI[curr];
#endif
                    //if the part is empty, skip the part.
                    if((noParts != 1  )&& globalMinMaxTotal[kk] > globalMinMaxTotal[kk + concurrentPart]) {

                        for(partId_t jj = 0; jj < noParts; ++jj){
                            outTotalCounts[currentOut + outShift + jj] = 0;
                        }
                        cutShift += noParts - 1;
                        tlrShift += (4 *(noParts - 1) + 1);
                        outShift += noParts;
                        pwShift += (2 * (noParts - 1) + 1);
                        continue;
                    }

                    lno_t coordinateEnd= inTotalCounts[curr];
                    lno_t coordinateBegin = curr==0 ? 0: inTotalCounts[curr -1];
                    scalar_t *usedCutCoordinate = cutCoordinates + cutShift;
                    float *usednonRectelinearPart = nonRectelinearPart + cutShift;

                    scalar_t *tlr =  totalPartWeights_leftClosests_rightClosests + tlrShift;

                    for(int ii = 0; ii < numThreads; ++ii){
                        pws[ii] = partWeights[ii] +  pwShift;
                    }


                    if(noParts > 1){

                        // Rewrite the indices based on the computed cuts.
                        getChunksFromCoordinates<lno_t,scalar_t>(
                                noParts,
                                numThreads,
                                partitionedPointCoordinates,
                                pqCoord,
                                pqJagged_uniformWeights[0],
                                pqJagged_weights[0],

                                usedCutCoordinate,
                                coordinateBegin,
                                coordinateEnd,

                                allowNonRectelinearPart,
                                usednonRectelinearPart,
                                tlr,
                                pws,
                                nonRectRatios,
                                //coordinate_linked_list,
                                //coordinate_starts,
                                //coordinate_ends,
                                partPointCounts,

                                newpartitionedPointCoordinates,
                                outTotalCounts + currentOut + outShift,
                                partIds//,
                                ,migration_check
                        );
                    }
                    else {
                        //if this part is partitioned into 1 then just copy the old values.
                        lno_t partSize = coordinateEnd - coordinateBegin;
                        *(outTotalCounts + currentOut + outShift) = partSize;
                        memcpy(newpartitionedPointCoordinates + coordinateBegin, partitionedPointCoordinates + coordinateBegin, partSize * sizeof(lno_t));
                    }
                    cutShift += noParts - 1;
                    tlrShift += (4 *(noParts - 1) + 1);
                    outShift += noParts;
                    pwShift += (2 * (noParts - 1) + 1);
                }

                //shift cut coordinates so that all cut coordinates are stored.
                cutCoordinates += cutShift;

                //getChunks from coordinates partitioned the parts and
                //wrote the indices as if there were a single part.
                //now we need to shift the beginning indices.
                for(partId_t kk = 0; kk < concurrentPart; ++kk){
#ifdef omitted
                    partId_t noParts = pAlongI[0];
                    if(!partNo){
                        noParts = pAlongI[ currentWorkPart + kk];
                    }
#endif
#ifndef omitted
                    partId_t noParts = pAlongI[ currentWorkPart + kk];
#endif
                    for (partId_t ii = 0;ii < noParts ; ++ii){
                        //shift it by previousCount
                        outTotalCounts[currentOut+ii] += previousEnd;
                    }
                    //increase the previous count by current end.
                    previousEnd = outTotalCounts[currentOut + noParts - 1];
                    //increase the current out.
                    currentOut += noParts ;
                }
            }
        }
        // end of this partitioning dimension


        //now check for the migration.
#ifdef enable_migration
        if (
                futurePartNumbers > 1 &&
                migration_check_option >= 0 &&
                worldSize > 1 &&
                currentPartitionCount == 1){
            env->timerStart(MACRO_TIMERS, "PQJagged Problem_Migration-" + istring);

            int mco = migration_check_option;
            if (reduceAllPop >= forceMigration ){
                mco = 1;
            }
            partId_t num_parts = outPartCount;
            if (
                    migration<gno_t, lno_t, scalar_t, node_t, partId_t>(
                            problemComm,
                            env, comm,
                            mvector, pqJagged_multiVectorDim,
                            numGlobalCoords,//numGlobalPoints,
                            numLocalCoords,
                            coordDim,
                            pqJagged_coordinates, //outout will be modified.
                            weightDim,
                            pqJagged_weights,// output will be modified.
#ifdef migrate_gid
                            mappedGnos,
#endif
                            partIds,
                            num_parts,
                            currentPartitionCount, //output
                            newpartitionedPointCoordinates, //output
                            partitionedPointCoordinates,
                            outTotalCounts //output
                            ,partIndexBegin,
                            futurePartNumbers,
                            migration_option,
                            mco, //migration_check_option,
                            migration_imbalance_cut_off,
                            istring,

                            pqJagged_uniformWeights[0],
                            pqJagged_weights[0],
                            allowNonRectelinearPart,
                            nonRectRatios,
                            nonRectelinearPart,
                            totalPartWeights_leftClosests_rightClosests,
                            partWeights,
                            cutCoordinates,
                            assignment_type
                    )
            )
            {

                is_migrated_in_current = true;
                is_data_migrated = true;
                env->timerStop(MACRO_TIMERS, "PQJagged Problem_Migration-" + istring);
                reduceAllCount /= num_parts;

            }
        }
#endif


        // swap the indices' memory
        lno_t * tmp = partitionedPointCoordinates;
        partitionedPointCoordinates = newpartitionedPointCoordinates;
        newpartitionedPointCoordinates = tmp;
        if(!is_migrated_in_current){
            reduceAllCount -= currentPartitionCount;
            currentPartitionCount = outPartCount;
        }
        freeArray<lno_t>(inTotalCounts);
        inTotalCounts = outTotalCounts;

        env->timerStop(MACRO_TIMERS, "PQJagged Problem_Partitioning_" + istring);
    }
    // Partitioning is done

    env->timerStop(MACRO_TIMERS, "PQJagged Problem_Partitioning");
    env->timerStart(MACRO_TIMERS, "PQJagged Part_Assignment");

#ifdef debug_setparts
    string fname = toString<int>(problemComm->getRank());
    fname = fname   + ".print";
    FILE *f = fopen(fname.c_str(), "w");
#endif


#ifdef HAVE_ZOLTAN2_OMP
#pragma omp parallel for
#endif
    for(partId_t i = 0; i < currentPartitionCount;++i){

        lno_t begin = 0;
        lno_t end = inTotalCounts[i];

        if(i > 0) begin = inTotalCounts[i -1];

        /*
#ifdef HAVE_ZOLTAN2_OMP
#pragma omp for
#endif
         */
        for (lno_t ii = begin; ii < end; ++ii){
            lno_t k = partitionedPointCoordinates[ii];
            partIds[k] = i + partIndexBegin;
#ifdef debug_setparts
            fprintf(f, "setting %d with coords: %lf %lf %lf to part %d\n", k, pqJagged_coordinates[0][k],  pqJagged_coordinates[1][k], pqJagged_coordinates[
                                                                                                                                                            ][k], partIds[k]);
#endif

        }
    }

#ifdef debug_setparts
    fclose(f);
#endif
    env->timerStop(MACRO_TIMERS, "PQJagged Part_Assignment");
    ArrayRCP<const gno_t> gnoList;
    if(!is_data_migrated){
        if(numLocalCoords > 0){
            gnoList = arcpFromArrayView(pqJagged_gnos);
        }
    }
#ifdef enable_migration
    else {
#ifdef migrate_gid
        gno_t *actualgnos = new gno_t[numLocalCoords];
        for(int i = 0; i < numLocalCoords; ++i){
            actualgnos[i] = gno_t (mappedGnos[i]);
            //cout <<"me " << problemComm->getRank()<<  " gno_t is " << mappedGnos[i]  << " p:"<<partIds[i]
            //<< " w:" <<  pqJagged_weights[0][i]
            << endl;
        }
        ArrayView<gno_t> rec(actualgnos,numLocalCoords);
        gnoList = arcpFromArrayView(rec);
#endif    
#ifndef migrate_gid
        gnoList =
                arcpFromArrayView(mvector->getMap()->getNodeElementList());
#endif
    }
#endif
    env->timerStop(MACRO_TIMERS, "PQJagged Total2");

    env->timerStart(MACRO_TIMERS, "PQJagged Solution_Part_Assignment");
    partId = arcp(partIds, 0, numLocalCoords, true);

#ifdef migrate_gid
    solution->setParts(gnoList, partId,true);
#endif

#ifndef migrate_gid
    solution->setParts(gnoList, partId,!is_data_migrated);
    /*
    if (is_data_migrated){
        solution->setParts(gnoList, partId,false);
    }
    else {
        solution->setParts(gnoList, partId,true);
    }
    */
#endif
    env->timerStop(MACRO_TIMERS, "PQJagged Solution_Part_Assignment");

    env->timerStart(MACRO_TIMERS, "PQJagged Problem_Free");

    for(int i = 0; i < numThreads; ++i){
        freeArray<lno_t>(partPointCounts[i]);
    }

    freeArray<lno_t *>(partPointCounts);
    freeArray<double *> (pws);

    if(allowNonRectelinearPart){
        freeArray<float>(nonRectelinearPart);
        for(int i = 0; i < numThreads; ++i){
            freeArray<float>(nonRectRatios[i]);
        }
        freeArray<float *>(nonRectRatios);
    }

    freeArray<partId_t>(myNonDoneCount);

    freeArray<scalar_t>(cutWeights);

    freeArray<scalar_t>(globalCutWeights);

    freeArray<scalar_t>(max_min_array);

    freeArray<lno_t>(outTotalCounts);

    freeArray<lno_t>(partitionedPointCoordinates);

    freeArray<lno_t>(newpartitionedPointCoordinates);

    freeArray<scalar_t>(allCutCoordinates);

    freeArray<scalar_t *>(pqJagged_coordinates);

    freeArray<scalar_t *>(pqJagged_weights);

    freeArray<bool>(pqJagged_uniformParts);

    freeArray<scalar_t> (localMinMaxTotal);

    freeArray<scalar_t> (globalMinMaxTotal);

    freeArray<scalar_t *>(pqJagged_partSizes);

    freeArray<bool>(pqJagged_uniformWeights);

    freeArray<scalar_t>(cutCoordinatesWork);

    freeArray<scalar_t>(targetPartWeightRatios);

    freeArray<scalar_t>(cutUpperBounds);

    freeArray<scalar_t>(cutLowerBounds);


    freeArray<scalar_t>(cutLowerWeight);
    freeArray<scalar_t>(cutUpperWeight);
    freeArray<bool>(isDone);
    freeArray<scalar_t>(totalPartWeights_leftClosests_rightClosests);
    freeArray<scalar_t>(global_totalPartWeights_leftClosests_rightClosests);

    for(int i = 0; i < numThreads; ++i){
        freeArray<double>(partWeights[i]);
        freeArray<scalar_t>(rightClosestDistance[i]);
        freeArray<scalar_t>(leftClosestDistance[i]);
    }

    freeArray<double *>(partWeights);
    freeArray<scalar_t *>(leftClosestDistance);
    freeArray<scalar_t *>(rightClosestDistance);


    env->timerStop(MACRO_TIMERS, "PQJagged Problem_Free");
    env->timerStop(MACRO_TIMERS, "PQJagged Total");



#endif // INCLUDE_ZOLTAN2_EXPERIMENTAL
}

} // namespace Zoltan2





#endif

