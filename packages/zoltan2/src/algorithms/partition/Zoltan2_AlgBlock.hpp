#ifndef _ZOLTAN2_ALGBLOCK_HPP_
#define _ZOLTAN2_ALGBLOCK_HPP_

#include <Zoltan2_IdentifierModel.hpp>
#include <Zoltan2_PartitioningSolution.hpp>
#include <Zoltan2_Metric.hpp>

#include <Teuchos_ParameterList.hpp>

#include <sstream>
#include <string>

/*! \file Zoltan2_AlgBlock.hpp
 *  \brief The algorithm for block partitioning.
 */

typedef zoltan2_partId_t partId_t;

namespace Zoltan2{

/*! Block partitioning method.
 *
 *  \param env   library configuration and problem parameters
 *  \param problemComm  the communicator for the problem
 *  \param ids    an Identifier model
 *  \param solution  a Solution object, containing part information
 *
 *  Preconditions: The parameters in the environment have been
 *    processed (committed).  No special requirements on the
 *    identifiers.
 */


template <typename Adapter>
void AlgPTBlock(
  const RCP<const Environment> &env,
  const RCP<const Comm<int> > &problemComm,
  const RCP<const IdentifierModel<Adapter> > &ids, 
  RCP<PartitioningSolution<typename Adapter::user_t> > &solution
) 
{
  using std::string;
  using std::ostringstream;
  typedef typename Adapter::lno_t lno_t;
  typedef typename Adapter::gno_t gno_t;
  typedef typename Adapter::scalar_t scalar_t;

  ////////////////////////////////////////////////////////
  // Library parameters of interest:
  //
  //    are we printing out debug messages
  //    are we timing
  //    are we computing memory used

  bool debug = env->doStatus();
#if 0
  bool timing = env->doTiming();
  bool memstats = env->doMemoryProfiling();
#endif

  if (debug)
    env->debug(DETAILED_STATUS, string("Entering AlgBlock"));

  int rank = env->myRank_;
  int nprocs = env->numProcs_;

  // Parameters that may drive algorithm choices
  //    speed_versus_quality
  //    memory_versus_speed

#if 0
  const Teuchos::ParameterList &pl = env->getParameters();
  const string defaultVal("balance");

  const string *mvr = pl.getPtr<string>("memory_versus_speed");
  if (!mvr)
    mvr = &defaultVal;
  
  const string *svq = pl.getPtr<string>("speed_versus_quality");
  if (!svq)
    svq = &defaultVal;

  bool fastSolution = (*svq==string("speed"));
  bool goodSolution = (*svq==string("quality"));
  bool balancedSolution = (*svq==string("balance"));
 
  bool lowMemory = (*mvr==string("memory"));
  bool lowRunTime = (*mvr==string("speed"));
  bool balanceMemoryRunTime = (*mvr==string("balance"));
#endif

  ////////////////////////////////////////////////////////
  // Problem parameters of interest:
  //    objective
  //    imbalance_tolerance

#if 0
  const string *obj=NULL;
  const double *tol=NULL;

  if (env->hasPartitioningParameters()){
    const Teuchos::ParameterList &plPart = pl.sublist("partitioning");

    obj = plPart.getPtr<string>("objective");
    tol = plPart.getPtr<double>("imbalance_tolerance");
  }

  double imbalanceTolerance = (tol ? *tol : 1.1);
  string objective = (obj ? *obj : string("balance_object_weight"));

  bool balanceCount = (objective == string("balance_object_count"));
  bool balanceWeight = (objective == string("balance_object_weight"));
  bool minTotalWeight = 
    (objective == string("multicriteria_minimize_total_weight"));
  bool minMaximumWeight = 
    (objective == string("multicriteria_minimize_maximum_weight"));
  bool balanceTotalMaximum = 
    (objective == string("multicriteria_balance_total_maximum"));
#endif

  ////////////////////////////////////////////////////////
  // From the IdentifierModel we need:
  //    the number of gnos
  //    number of weights per gno
  //    the weights
  // TODO: modify algorithm for weight dimension greater than 1.

  size_t numGnos = ids->getLocalNumIdentifiers();
  int wtflag = ids->getIdentifierWeightDim();

  int weightDim = (wtflag ? wtflag : 1);

  ArrayView<const gno_t> idList;
  ArrayView<StridedInput<lno_t, scalar_t> > wgtList;
  
  ids->getIdentifierList(idList, wgtList);

  ////////////////////////////////////////////////////////
  // From the Solution we get part information.
  //
  //   TODO: for now, we have 1 part per proc and all
  //   part sizes are the same.

  size_t numGlobalParts = solution->getGlobalNumberOfParts();
#if 0
  size_t numLocalParts = solution->getLocalNumberOfParts();
#endif
  
  ////////////////////////////////////////////////////////
  // The algorithm
  //
  // Block partitioning algorithm lifted from zoltan/src/simple/block.c
  // The solution is:
  //    a list of part numbers in gno order
  //    an imbalance for each weight 

  scalar_t wtsum(0);

  if (wtflag){
    for (size_t i=0; i<numGnos; i++)
      wtsum += wgtList[0][i];          // [] operator knows stride
  }
  else
    wtsum = static_cast<scalar_t>(numGnos);

  Array<scalar_t> scansum(nprocs+1, 0);

  Teuchos::gatherAll<int, scalar_t>(*problemComm, 1, &wtsum, nprocs,
    scansum.getRawPtr()+1);

  /* scansum = sum of weights on lower processors, excluding self. */

  for (int i=2; i<=nprocs; i++)
    scansum[i] += scansum[i-1];

  scalar_t globalTotalWeight = scansum[nprocs];

  /* Overwrite part_sizes with cumulative sum (inclusive) part_sizes. */
  /* A cleaner way is to make a copy, but this works. */

  Array<scalar_t> part_sizes(numGlobalParts, 1.0/numGlobalParts); 
  for (int i=1; i<numGlobalParts; i++)
    part_sizes[i] += part_sizes[i-1];

  if (debug){
    ostringstream oss("Part sizes: ");
    for (int i=0; i < numGlobalParts; i++)
      oss << part_sizes[i] << " ";
    oss << "\n";
    oss << std::endl << "Weights : ";
    for (int i=0; i <= nprocs; i++)
      oss << scansum[i] << " ";
    oss << "\n";
    env->debug(VERBOSE_DETAILED_STATUS, oss.str());
  }

  /* Loop over objects and assign partition. */
  partId_t part = 0;
  wtsum = scansum[rank];
  Array<scalar_t> partTotal(numGlobalParts, 0);
  ArrayRCP<partId_t> gnoPart= arcp(new partId_t [numGnos], 0, numGnos);

  for (size_t i=0; i<numGnos; i++){
    scalar_t gnoWeight = (wtflag? wgtList[0][i] : 1.0);
    /* wtsum is now sum of all lower-ordered object */
    /* determine new partition number for this object,
       using the "center of gravity" */
    while (part<numGlobalParts-1 && 
           (wtsum+0.5*gnoWeight) > part_sizes[part]*globalTotalWeight)
      part++;
    gnoPart[i] = part;
    partTotal[part] += gnoWeight;
    wtsum += gnoWeight;
  }

  ////////////////////////////////////////////////////////////
  // Compute the imbalance.

  ArrayRCP<float> imbalance = arcp(new float[weightDim], 0, weightDim);

  // TODO - get part sizes from the solution object.  For now, 
  //    an empty part size array means uniform parts.

  ArrayView<float> defaultPartSizes(Teuchos::null);
  Array<ArrayView<float> > partSizes(weightDim, defaultPartSizes);

  // TODO have partNums default to 0 through numGlobalParts-1 in
  //    imbalances() call.
  Array<partId_t> partNums(numGlobalParts);
  for (partId_t i=0; i < numGlobalParts; i++) partNums[i] = i;

  Array<ArrayView<scalar_t> > partWeights(1);
  partWeights[0] = partTotal.view(0, numGlobalParts);

  try{
    imbalances<scalar_t>(env, problemComm, numGlobalParts, 
      partSizes, partNums.view(0, numGlobalParts),
         partWeights, imbalance.view(0, weightDim));
  }
  Z2_FORWARD_EXCEPTIONS;
  

  ////////////////////////////////////////////////////////////
  // Done
  
  if (debug){
#if 0
    if (imbalance[0] > Teuchos::as<scalar_t>(imbalanceTolerance)){
      ostringstream oss("Warning: imbalance is ");
      oss << imbalance[0] << std::endl;
      env->debug(BASIC_STATUS, oss.str());
    }
    else{
#endif
      ostringstream oss("Imbalance: ");
      oss << imbalance[0] << std::endl;
      env->debug(DETAILED_STATUS, oss.str());
#if 0
    }
#endif
  }

  // Done, update the solution

  solution->setParts(idList, gnoPart, imbalance);

  if (debug)
    env->debug(DETAILED_STATUS, string("Exiting AlgBlock"));
}

}   // namespace Zoltan2

#endif
