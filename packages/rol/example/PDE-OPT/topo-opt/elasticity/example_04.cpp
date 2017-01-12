// @HEADER
// ************************************************************************
//
//               Rapid Optimization Library (ROL) Package
//                 Copyright (2014) Sandia Corporation
//
// Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
// license for use of this work by or on behalf of the U.S. Government.
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
// Questions? Contact lead developers:
//              Drew Kouri   (dpkouri@sandia.gov) and
//              Denis Ridzal (dridzal@sandia.gov)
//
// ************************************************************************
// @HEADER

/*! \file  example_01.cpp
    \brief Shows how to solve the stuctural topology optimization problem.
*/

#include "Teuchos_Comm.hpp"
#include "Teuchos_Time.hpp"
#include "Teuchos_oblackholestream.hpp"
#include "Teuchos_GlobalMPISession.hpp"
#include "Teuchos_XMLParameterListHelpers.hpp"

#include "Tpetra_DefaultPlatform.hpp"
#include "Tpetra_Version.hpp"

#include <iostream>
#include <algorithm>

#include "ROL_Algorithm.hpp"
#include "ROL_AugmentedLagrangian.hpp"
#include "ROL_ScaledStdVector.hpp"
#include "ROL_Reduced_Objective_SimOpt.hpp"
#include "ROL_Reduced_EqualityConstraint_SimOpt.hpp"
#include "ROL_BoundConstraint.hpp"
#include "ROL_CompositeEqualityConstraint_SimOpt.hpp"
#include "ROL_MonteCarloGenerator.hpp"
#include "ROL_StochasticProblem.hpp"
#include "ROL_TpetraTeuchosBatchManager.hpp"

#include "../../TOOLS/pdeconstraint.hpp"
#include "../../TOOLS/linearpdeconstraint.hpp"
#include "../../TOOLS/pdeobjective.hpp"
#include "../../TOOLS/pdevector.hpp"
#include "../../TOOLS/integralconstraint.hpp"
#include "mesh_topo-opt.hpp"
#include "pde_topo-opt.hpp"
#include "obj_topo-opt.hpp"

typedef double RealT;

template<class Real>
void setUpAndSolve(ROL::StochasticProblem<Real> &opt,
                   Teuchos::ParameterList &parlist,
                   std::ostream &outStream) {
  ROL::Algorithm<RealT> algo("Trust Region",parlist,false);
  Teuchos::Time timer("Optimization Time", true);
  algo.run(opt,true,outStream);
  timer.stop();
  outStream << "Total optimization time = " << timer.totalElapsedTime() << " seconds." << std::endl;
}

template<class Real>
void print(ROL::Objective<Real> &obj,
           const ROL::Vector<Real> &z,
           ROL::SampleGenerator<Real> &sampler,
           const int ngsamp,
           const Teuchos::RCP<const Teuchos::Comm<int> > &comm,
           const std::string &filename) {
  Real tol(1e-8);
  // Build objective function distribution
  int nsamp = sampler.numMySamples();
  std::vector<Real> values(nsamp);
  for (int i = 0; i < nsamp; ++i) {
    std::vector<RealT> sample = sampler.getMyPoint(i);
    obj.setParameter(sample);
    values[i] = obj.value(z,tol);
  }
  std::vector<Real> gvalues(ngsamp);
  Teuchos::gather<int,Real>(&values[0],nsamp,&gvalues[0],ngsamp,0,*comm);

  // Print
  int rank = Teuchos::rank<int>(*comm);
  if ( !rank ) {
    std::stringstream name;
    name << filename;
    std::ofstream file;
    file.open(name.str());
    file << std::scientific << std::setprecision(15);
    for (int i = 0; i < ngsamp; ++i) {
      file << std::setw(25) << std::left << gvalues[i] << std::endl;
    }
    file.close();
  }
}

int main(int argc, char *argv[]) {
  // This little trick lets us print to std::cout only if a (dummy) command-line argument is provided.
  int iprint     = argc - 1;
  Teuchos::RCP<std::ostream> outStream;
  Teuchos::oblackholestream bhs; // outputs nothing

  /*** Initialize communicator. ***/
  Teuchos::GlobalMPISession mpiSession (&argc, &argv, &bhs);
  Teuchos::RCP<const Teuchos::Comm<int> > comm
    = Tpetra::DefaultPlatform::getDefaultPlatform().getComm();
  Teuchos::RCP<const Teuchos::Comm<int> > serial_comm
    = Teuchos::rcp(new Teuchos::SerialComm<int>());
  const int myRank = comm->getRank();
  if ((iprint > 0) && (myRank == 0)) {
    outStream = Teuchos::rcp(&std::cout, false);
  }
  else {
    outStream = Teuchos::rcp(&bhs, false);
  }
  int errorFlag  = 0;

  // *** Example body.
  try {
    RealT tol(1e-8), one(1);

    /*** Read in XML input ***/
    std::string filename = "input_ex04.xml";
    Teuchos::RCP<Teuchos::ParameterList> parlist = Teuchos::rcp( new Teuchos::ParameterList() );
    Teuchos::updateParametersFromXmlFile( filename, parlist.ptr() );

    // Retrieve parameters.
    const RealT volFraction  = parlist->sublist("Problem").get("Volume Fraction", 0.4);
    const RealT objFactor    = parlist->sublist("Problem").get("Objective Scaling", 1e-4);

    /*** Initialize main data structure. ***/
    Teuchos::RCP<MeshManager<RealT> > meshMgr
      = Teuchos::rcp(new MeshManager_TopoOpt<RealT>(*parlist));
    // Initialize PDE describing elasticity equations.
    Teuchos::RCP<PDE_TopoOpt<RealT> > pde
      = Teuchos::rcp(new PDE_TopoOpt<RealT>(*parlist));
    Teuchos::RCP<ROL::EqualityConstraint_SimOpt<RealT> > con
      = Teuchos::rcp(new PDE_Constraint<RealT>(pde,meshMgr,serial_comm,*parlist,*outStream));
    // Initialize the filter PDE.
    Teuchos::RCP<PDE_Filter<RealT> > pdeFilter
      = Teuchos::rcp(new PDE_Filter<RealT>(*parlist));
    Teuchos::RCP<ROL::EqualityConstraint_SimOpt<RealT> > conFilter
      = Teuchos::rcp(new Linear_PDE_Constraint<RealT>(pdeFilter,meshMgr,serial_comm,*parlist,*outStream));
    // Cast the constraint and get the assembler.
    Teuchos::RCP<PDE_Constraint<RealT> > pdecon
      = Teuchos::rcp_dynamic_cast<PDE_Constraint<RealT> >(con);
    Teuchos::RCP<Assembler<RealT> > assembler = pdecon->getAssembler();
    pdecon->printMeshData(*outStream);
    con->setSolveParameters(*parlist);

    // Create state vector.
    Teuchos::RCP<Tpetra::MultiVector<> > u_rcp = assembler->createStateVector();
    u_rcp->randomize();
    Teuchos::RCP<ROL::Vector<RealT> > up
      = Teuchos::rcp(new PDE_PrimalSimVector<RealT>(u_rcp,pde,assembler,*parlist));
    Teuchos::RCP<Tpetra::MultiVector<> > p_rcp = assembler->createStateVector();
    p_rcp->randomize();
    Teuchos::RCP<ROL::Vector<RealT> > pp
      = Teuchos::rcp(new PDE_PrimalSimVector<RealT>(p_rcp,pde,assembler,*parlist));
    // Create control vector.
    Teuchos::RCP<Tpetra::MultiVector<> > z_rcp = assembler->createControlVector();
    //z_rcp->randomize();
    z_rcp->putScalar(volFraction);
    //z_rcp->putScalar(0);
    Teuchos::RCP<ROL::Vector<RealT> > zp
      = Teuchos::rcp(new PDE_PrimalOptVector<RealT>(z_rcp,pde,assembler,*parlist));
    // Create residual vector.
    Teuchos::RCP<Tpetra::MultiVector<> > r_rcp = assembler->createResidualVector();
    r_rcp->putScalar(0.0);
    Teuchos::RCP<ROL::Vector<RealT> > rp
      = Teuchos::rcp(new PDE_DualSimVector<RealT>(r_rcp,pde,assembler,*parlist));
    // Create state direction vector.
    Teuchos::RCP<Tpetra::MultiVector<> > du_rcp = assembler->createStateVector();
    du_rcp->randomize();
    //du_rcp->putScalar(0);
    Teuchos::RCP<ROL::Vector<RealT> > dup
      = Teuchos::rcp(new PDE_PrimalSimVector<RealT>(du_rcp,pde,assembler,*parlist));
    // Create control direction vector.
    Teuchos::RCP<Tpetra::MultiVector<> > dz_rcp = assembler->createControlVector();
    dz_rcp->randomize();
    dz_rcp->scale(0.01);
    Teuchos::RCP<ROL::Vector<RealT> > dzp
      = Teuchos::rcp(new PDE_PrimalOptVector<RealT>(dz_rcp,pde,assembler,*parlist));
    // Create control test vector.
    Teuchos::RCP<Tpetra::MultiVector<> > rz_rcp = assembler->createControlVector();
    rz_rcp->randomize();
    Teuchos::RCP<ROL::Vector<RealT> > rzp
      = Teuchos::rcp(new PDE_PrimalOptVector<RealT>(rz_rcp,pde,assembler,*parlist));

    Teuchos::RCP<Tpetra::MultiVector<> > dualu_rcp = assembler->createStateVector();
    Teuchos::RCP<ROL::Vector<RealT> > dualup
      = Teuchos::rcp(new PDE_DualSimVector<RealT>(dualu_rcp,pde,assembler,*parlist));
    Teuchos::RCP<Tpetra::MultiVector<> > dualz_rcp = assembler->createControlVector();
    Teuchos::RCP<ROL::Vector<RealT> > dualzp
      = Teuchos::rcp(new PDE_DualOptVector<RealT>(dualz_rcp,pde,assembler,*parlist));

    // Create ROL SimOpt vectors.
    ROL::Vector_SimOpt<RealT> x(up,zp);
    ROL::Vector_SimOpt<RealT> d(dup,dzp);

    // Initialize "filtered" or "unfiltered" constraint.
    Teuchos::RCP<ROL::EqualityConstraint_SimOpt<RealT> > pdeWithFilter;
    bool useFilter  = parlist->sublist("Problem").get("Use Filter", true);
    if (useFilter) {
      pdeWithFilter
        = Teuchos::rcp(new ROL::CompositeEqualityConstraint_SimOpt<RealT>(con, conFilter, *rp, *rp, *up, *zp, *zp));
    }
    else {
      pdeWithFilter = con;
    }
    pdeWithFilter->setSolveParameters(*parlist);

    // Initialize compliance objective function.
    std::vector<Teuchos::RCP<QoI<RealT> > > qoi_vec(2,Teuchos::null);
    qoi_vec[0] = Teuchos::rcp(new QoI_TopoOpt<RealT>(pde->getFE(),
                                                     pde->getLoad(),
                                                     pde->getFieldHelper(),
                                                     objFactor));
    qoi_vec[1] = Teuchos::rcp(new QoI_Volume_TopoOpt<RealT>(pde->getFE(),
                                                            pde->getFieldHelper(),
                                                            *parlist));
    RealT lambda = parlist->sublist("Problem").get("Volume Cost Parameter",1.0);
    Teuchos::RCP<StdObjective_TopoOpt<RealT> > std_obj
      = Teuchos::rcp(new StdObjective_TopoOpt<RealT>(lambda));
    Teuchos::RCP<ROL::Objective_SimOpt<RealT> > obj
      = Teuchos::rcp(new PDE_Objective<RealT>(qoi_vec,std_obj,assembler));
    // Initialize volume objective
    Teuchos::RCP<IntegralObjective<RealT> > volObj
      = Teuchos::rcp(new IntegralObjective<RealT>(qoi_vec[1],assembler));

    // Initialize reduced compliance function.
    bool storage = parlist->sublist("Problem").get("Use state storage",true);
    Teuchos::RCP<ROL::SimController<RealT> > stateStore
      = Teuchos::rcp(new ROL::SimController<RealT>());
    Teuchos::RCP<ROL::Reduced_Objective_SimOpt<RealT> > objRed
      = Teuchos::rcp(new
        ROL::Reduced_Objective_SimOpt<RealT>(obj,pdeWithFilter,
                                             stateStore,up,zp,pp,
                                             storage));

    // Initialize bound constraints.
    Teuchos::RCP<Tpetra::MultiVector<> > lo_rcp = assembler->createControlVector();
    Teuchos::RCP<Tpetra::MultiVector<> > hi_rcp = assembler->createControlVector();
    lo_rcp->putScalar(0.0); hi_rcp->putScalar(1.0);
    Teuchos::RCP<ROL::Vector<RealT> > lop
      = Teuchos::rcp(new PDE_PrimalOptVector<RealT>(lo_rcp,pde,assembler));
    Teuchos::RCP<ROL::Vector<RealT> > hip
      = Teuchos::rcp(new PDE_PrimalOptVector<RealT>(hi_rcp,pde,assembler));
    Teuchos::RCP<ROL::BoundConstraint<RealT> > bnd
      = Teuchos::rcp(new ROL::BoundConstraint<RealT>(lop,hip));

    /*************************************************************************/
    /***************** BUILD SAMPLER *****************************************/
    /*************************************************************************/
    int nsamp      = parlist->sublist("Problem").get("Number of samples", 4);
    int nsamp_dist = parlist->sublist("Problem").get("Number of Output Samples",100);
    Teuchos::Array<RealT> loadMag
      = Teuchos::getArrayFromStringParameter<double>(parlist->sublist("Problem").sublist("Load"), "Magnitude");
    int nLoads = loadMag.size();
    int dim    = 2;
    std::vector<Teuchos::RCP<ROL::Distribution<RealT> > > distVec(dim*nLoads);
    for (int i = 0; i < nLoads; ++i) {
      std::stringstream sli;
      sli << "Stochastic Load " << i;
      Teuchos::ParameterList magList;
      magList.sublist("Distribution") = parlist->sublist("Problem").sublist(sli.str()).sublist("Magnitude");
      //magList.print(*outStream);
      distVec[i*dim + 0] = ROL::DistributionFactory<RealT>(magList);
      Teuchos::ParameterList angList;
      angList.sublist("Distribution") = parlist->sublist("Problem").sublist(sli.str()).sublist("Polar Angle");
      //angList.print(*outStream);
      distVec[i*dim + 1] = ROL::DistributionFactory<RealT>(angList);
    }
    Teuchos::RCP<ROL::BatchManager<RealT> > bman
      = Teuchos::rcp(new ROL::TpetraTeuchosBatchManager<RealT>(comm));
    Teuchos::RCP<ROL::SampleGenerator<RealT> > sampler
      = Teuchos::rcp(new ROL::MonteCarloGenerator<RealT>(nsamp,distVec,bman));
    Teuchos::RCP<ROL::SampleGenerator<RealT> > sampler_dist
      = Teuchos::rcp(new ROL::MonteCarloGenerator<RealT>(nsamp_dist,distVec,bman));

    /*************************************************************************/
    /***************** SOLVE OPTIMIZATION PROBLEMS ***************************/
    /*************************************************************************/
    Teuchos::RCP<ROL::StochasticProblem<RealT> > opt;
    std::vector<RealT> vol;
    std::vector<RealT> var;

    /*************************************************************************/
    /***************** SOLVE RISK NEUTRAL ************************************/
    /*************************************************************************/
    // Solve.
    parlist->sublist("SOL").set("Stochastic Optimization Type","Risk Neutral");
    opt = Teuchos::rcp(new ROL::StochasticProblem<RealT>(*parlist,objRed,sampler,zp,bnd));
    opt->setSolutionStatistic(one);
    setUpAndSolve<RealT>(*opt,*parlist,*outStream);
    // Output.
    vol.push_back(volObj->value(*up,*zp,tol));
    var.push_back(opt->getSolutionStatistic());
    std::string DensRN = "density_RN.txt";
    pdecon->outputTpetraVector(z_rcp,DensRN);
    std::string ObjRN = "obj_samples_RN.txt";
    print<RealT>(*objRed,*zp,*sampler_dist,nsamp_dist,comm,ObjRN);

    /*************************************************************************/
    /***************** SOLVE MEAN PLUS CVAR **********************************/
    /*************************************************************************/
    const int N = parlist->sublist("Problem").get("Denominator for Convex Combination",8);
    RealT mu(0);
    parlist->sublist("SOL").set("Stochastic Optimization Type","Risk Averse");
    parlist->sublist("SOL").sublist("Risk Measure").set("Name","Quantile-Based Quadrangle");
    parlist->sublist("SOL").sublist("Risk Measure").sublist("Quantile-Based Quadrangle").set("Confidence Level",0.9);
    parlist->sublist("SOL").sublist("Risk Measure").sublist("Quantile-Based Quadrangle").set("Smoothing Parameter",1e-4);
    parlist->sublist("SOL").sublist("Risk Measure").sublist("Quantile-Based Quadrangle").sublist("Distribution").set("Name","Parabolic");
    parlist->sublist("SOL").sublist("Risk Measure").sublist("Quantile-Based Quadrangle").sublist("Distribution").sublist("Parabolic").set("Lower Bound",0.0);
    parlist->sublist("SOL").sublist("Risk Measure").sublist("Quantile-Based Quadrangle").sublist("Distribution").sublist("Parabolic").set("Upper Bound",1.0);
    for (int i = 0; i < N; ++i) {
      mu = static_cast<RealT>(N-i-1)/static_cast<RealT>(N);
      // Solve.
      parlist->sublist("SOL").sublist("Risk Measure").sublist("Quantile-Based Quadrangle").set("Convex Combination Parameter",mu);
      opt = Teuchos::rcp(new ROL::StochasticProblem<RealT>(*parlist,objRed,sampler,zp,bnd));
      opt->setSolutionStatistic(var[i]);
      setUpAndSolve<RealT>(*opt,*parlist,*outStream);
      // Output.
      vol.push_back(volObj->value(*up,*zp,tol));
      var.push_back(opt->getSolutionStatistic());
      std::stringstream nameDens;
      nameDens << "density_CVaR_" << i << ".txt";
      pdecon->outputTpetraVector(z_rcp,nameDens.str().c_str());
      std::stringstream nameObj;
      nameObj << "obj_samples_CVaR_" << i << ".txt";
      print<RealT>(*objRed,*zp,*sampler_dist,nsamp_dist,comm,nameObj.str());
    }

    /*************************************************************************/
    /***************** PRINT VOLUME AND VAR **********************************/
    /*************************************************************************/
    const int rank = Teuchos::rank<int>(*comm);
    if ( !rank ) {
      std::stringstream nameVOL, nameVAR;
      nameVOL << "vol.txt";
      nameVAR << "var.txt";
      std::ofstream fileVOL, fileVAR;
      fileVOL.open(nameVOL.str());
      fileVAR.open(nameVAR.str());
      fileVOL << std::scientific << std::setprecision(15);
      fileVAR << std::scientific << std::setprecision(15);
      int size = var.size();
      for (int i = 0; i < size; ++i) {
        fileVOL << std::setw(25) << std::left << vol[i] << std::endl;
        fileVAR << std::setw(25) << std::left << var[i] << std::endl;
      }
      fileVOL.close();
      fileVAR.close();
    }

    // Get a summary from the time monitor.
    Teuchos::TimeMonitor::summarize();
  }
  catch (std::logic_error err) {
    *outStream << err.what() << "\n";
    errorFlag = -1000;
  }; // end try

  if (errorFlag != 0)
    std::cout << "End Result: TEST FAILED\n";
  else
    std::cout << "End Result: TEST PASSED\n";

  return 0;
}
