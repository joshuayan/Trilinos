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
    \brief Shows how to solve the stochastic Stefan-Boltzmann problem.
*/

#include "Teuchos_Comm.hpp"
#include "Teuchos_oblackholestream.hpp"
#include "Teuchos_GlobalMPISession.hpp"
#include "Teuchos_XMLParameterListHelpers.hpp"

#include "Tpetra_DefaultPlatform.hpp"
#include "Tpetra_Version.hpp"

#include <iostream>
#include <algorithm>
//#include <fenv.h>

#include "ROL_Algorithm.hpp"
#include "ROL_Bounds.hpp"
#include "ROL_Reduced_Objective_SimOpt.hpp"
#include "ROL_MonteCarloGenerator.hpp"
#include "ROL_OptimizationProblem.hpp"
#include "ROL_TpetraTeuchosBatchManager.hpp"

#include "../TOOLS/meshmanager.hpp"
#include "../TOOLS/pdeconstraint.hpp"
#include "../TOOLS/pdeobjective.hpp"
#include "../TOOLS/pdevector.hpp"
#include "../TOOLS/batchmanager.hpp"
#include "pde_stoch_stefan_boltzmann.hpp"
#include "obj_stoch_stefan_boltzmann.hpp"
#include "mesh_stoch_stefan_boltzmann.hpp"

typedef double RealT;

template<class Real>
Real random(const Teuchos::Comm<int> &comm,
            const Real a = -1, const Real b = 1) {
  Real val(0), u(0);
  if ( Teuchos::rank<int>(comm)==0 ) {
    u   = static_cast<Real>(rand())/static_cast<Real>(RAND_MAX);
    val = (b-a)*u + a;
  }
  Teuchos::broadcast<int,Real>(comm,0,1,&val);
  return val;
}

int main(int argc, char *argv[]) {
//  feenableexcept(FE_DIVBYZERO | FE_INVALID | FE_OVERFLOW);

  // This little trick lets us print to std::cout only if a (dummy) command-line argument is provided.
  int iprint     = argc - 1;
  ROL::SharedPointer<std::ostream> outStream;
  Teuchos::oblackholestream bhs; // outputs nothing

  /*** Initialize communicator. ***/
  Teuchos::GlobalMPISession mpiSession (&argc, &argv, &bhs);
  ROL::SharedPointer<const Teuchos::Comm<int> > comm
    = Tpetra::DefaultPlatform::getDefaultPlatform().getComm();
  ROL::SharedPointer<const Teuchos::Comm<int> > serial_comm
    = ROL::makeShared<Teuchos::SerialComm<int>>();
  const int myRank = comm->getRank();
  if ((iprint > 0) && (myRank == 0)) {
    outStream = ROL::makeSharedFromRef(std::cout);
  }
  else {
    outStream = ROL::makeSharedFromRef(bhs);
  }
  int errorFlag  = 0;

  // *** Example body.
  try {

    /*** Read in XML input ***/
    std::string filename = "input.xml";
    ROL::SharedPointer<Teuchos::ParameterList> parlist = ROL::makeShared<Teuchos::ParameterList>();
    Teuchos::updateParametersFromXmlFile( filename, parlist.ptr() );

    // Problem dimensions
    const int stochDim = 32, controlDim = 1;
    const RealT one(1); 

    /*************************************************************************/
    /***************** BUILD GOVERNING PDE ***********************************/
    /*************************************************************************/
    /*** Initialize main data structure. ***/
    ROL::SharedPointer<MeshManager<RealT> > meshMgr
      = ROL::makeShared<MeshManager_BackwardFacingStepChannel<RealT>>(*parlist);
    //  = ROL::makeShared<StochasticStefanBoltzmannMeshManager<RealT>>(*parlist);
    // Initialize Stefan-Boltzmann PDE.
    ROL::SharedPointer<StochasticStefanBoltzmannPDE<RealT> > pde
      = ROL::makeShared<StochasticStefanBoltzmannPDE<RealT>>(*parlist);
    ROL::SharedPointer<ROL::Constraint_SimOpt<RealT> > con
      = ROL::makeShared<PDE_Constraint<RealT>>(pde,meshMgr,serial_comm,*parlist,*outStream);
    // Cast the constraint and get the assembler.
    ROL::SharedPointer<PDE_Constraint<RealT> > pdecon
      = ROL::dynamicPointerCast<PDE_Constraint<RealT> >(con);
    ROL::SharedPointer<Assembler<RealT> > assembler = pdecon->getAssembler();
    con->setSolveParameters(*parlist);

    /*************************************************************************/
    /***************** BUILD VECTORS *****************************************/
    /*************************************************************************/
    ROL::SharedPointer<Tpetra::MultiVector<> >  u_ptr = assembler->createStateVector();
    ROL::SharedPointer<Tpetra::MultiVector<> >  p_ptr = assembler->createStateVector();
    ROL::SharedPointer<Tpetra::MultiVector<> > du_ptr = assembler->createStateVector();
    u_ptr->randomize();  //u_ptr->putScalar(static_cast<RealT>(1));
    p_ptr->randomize();  //p_ptr->putScalar(static_cast<RealT>(1));
    du_ptr->randomize(); //du_ptr->putScalar(static_cast<RealT>(0));
    ROL::SharedPointer<ROL::Vector<RealT> > up
      = ROL::makeShared<PDE_PrimalSimVector<RealT>>(u_ptr,pde,assembler);
    ROL::SharedPointer<ROL::Vector<RealT> > pp
      = ROL::makeShared<PDE_PrimalSimVector<RealT>>(p_ptr,pde,assembler);
    ROL::SharedPointer<ROL::Vector<RealT> > dup
      = ROL::makeShared<PDE_PrimalSimVector<RealT>>(du_ptr,pde,assembler);
    // Create residual vectors
    ROL::SharedPointer<Tpetra::MultiVector<> > r_ptr = assembler->createResidualVector();
    r_ptr->randomize(); //r_ptr->putScalar(static_cast<RealT>(1));
    ROL::SharedPointer<ROL::Vector<RealT> > rp
      = ROL::makeShared<PDE_DualSimVector<RealT>>(r_ptr,pde,assembler);
    // Create control vector and set to ones
    ROL::SharedPointer<std::vector<RealT> >  z_ptr = ROL::makeShared<std::vector<RealT>>(controlDim);
    ROL::SharedPointer<std::vector<RealT> > dz_ptr = ROL::makeShared<std::vector<RealT>>(controlDim);
    ROL::SharedPointer<std::vector<RealT> > yz_ptr = ROL::makeShared<std::vector<RealT>>(controlDim);
    // Create control direction vector and set to random
    for (int i = 0; i < controlDim; ++i) {
      (*z_ptr)[i]  = random<RealT>(*comm);
      (*dz_ptr)[i] = 1e-3*random<RealT>(*comm);
      (*yz_ptr)[i] = random<RealT>(*comm);
    }
    ROL::SharedPointer<ROL::Vector<RealT> > zp
      = ROL::makeShared<PDE_OptVector<RealT>>(ROL::makeShared<ROL::StdVector<RealT>>(z_ptr));
    ROL::SharedPointer<ROL::Vector<RealT> > dzp
      = ROL::makeShared<PDE_OptVector<RealT>>(ROL::makeShared<ROL::StdVector<RealT>>(dz_ptr));
    ROL::SharedPointer<ROL::Vector<RealT> > yzp
      = ROL::makeShared<PDE_OptVector<RealT>>(ROL::makeShared<ROL::StdVector<RealT>>(yz_ptr));
    //ROL::SharedPointer<Tpetra::MultiVector<> >  z_ptr = assembler->createControlVector();
    //ROL::SharedPointer<Tpetra::MultiVector<> > dz_ptr = assembler->createControlVector();
    //ROL::SharedPointer<Tpetra::MultiVector<> > yz_ptr = assembler->createControlVector();
    //z_ptr->randomize();  z_ptr->putScalar(static_cast<RealT>(280));
    //dz_ptr->randomize(); //dz_ptr->putScalar(static_cast<RealT>(0));
    //yz_ptr->randomize(); //yz_ptr->putScalar(static_cast<RealT>(0));
    //ROL::SharedPointer<ROL::TpetraMultiVector<RealT> > zpde
    //  = ROL::makeShared<PDE_PrimalOptVector<RealT>>(z_ptr,pde,assembler);
    //ROL::SharedPointer<ROL::TpetraMultiVector<RealT> > dzpde
    //  = ROL::makeShared<PDE_PrimalOptVector<RealT>>(dz_ptr,pde,assembler);
    //ROL::SharedPointer<ROL::TpetraMultiVector<RealT> > yzpde
    //  = ROL::makeShared<PDE_PrimalOptVector<RealT>>(yz_ptr,pde,assembler);
    //ROL::SharedPointer<ROL::Vector<RealT> > zp  = ROL::makeShared<PDE_OptVector<RealT>>(zpde);
    //ROL::SharedPointer<ROL::Vector<RealT> > dzp = ROL::makeShared<PDE_OptVector<RealT>>(dzpde);
    //ROL::SharedPointer<ROL::Vector<RealT> > yzp = ROL::makeShared<PDE_OptVector<RealT>>(yzpde);
    // Create ROL SimOpt vectors
    ROL::Vector_SimOpt<RealT> x(up,zp);
    ROL::Vector_SimOpt<RealT> d(dup,dzp);

    /*************************************************************************/
    /***************** BUILD COST FUNCTIONAL *********************************/
    /*************************************************************************/
    std::vector<ROL::SharedPointer<QoI<RealT> > > qoi_vec(2,ROL::nullPointer);
    qoi_vec[0] = ROL::makeShared<QoI_StateCost<RealT>>(pde->getVolFE(),*parlist);
    //qoi_vec[1] = ROL::makeShared<QoI_ControlCost<RealT>>(
    //  pde->getVolFE(),pde->getBdryFE(0),pde->getBdryCellLocIds(0),*parlist);
    qoi_vec[1] = ROL::makeShared<QoI_AdvectionCost<RealT>>();
    ROL::SharedPointer<StochasticStefanBoltzmannStdObjective<RealT> > std_obj
      = ROL::makeShared<StochasticStefanBoltzmannStdObjective<RealT>>(*parlist);
    ROL::SharedPointer<ROL::Objective_SimOpt<RealT> > obj
      = ROL::makeShared<PDE_Objective<RealT>>(qoi_vec,std_obj,assembler);
    ROL::SharedPointer<ROL::Reduced_Objective_SimOpt<RealT> > objReduced
      = ROL::makeShared<ROL::Reduced_Objective_SimOpt<RealT>>(obj, con, up, zp, pp, true, false);

    /*************************************************************************/
    /***************** BUILD BOUND CONSTRAINT ********************************/
    /*************************************************************************/
    RealT upper = parlist->sublist("Problem").get("Upper Advection Bound", 100.0);
    RealT lower = parlist->sublist("Problem").get("Lower Advection Bound",-100.0);
    ROL::SharedPointer<std::vector<RealT> > zlo_ptr = ROL::makeShared<std::vector<RealT>>(controlDim,lower);
    ROL::SharedPointer<std::vector<RealT> > zhi_ptr = ROL::makeShared<std::vector<RealT>>(controlDim,upper);
    ROL::SharedPointer<ROL::Vector<RealT> > zlop
      = ROL::makeShared<PDE_OptVector<RealT>>(ROL::makeShared<ROL::StdVector<RealT>>(zlo_ptr));
    ROL::SharedPointer<ROL::Vector<RealT> > zhip
      = ROL::makeShared<PDE_OptVector<RealT>>(ROL::makeShared<ROL::StdVector<RealT>>(zhi_ptr));
    //ROL::SharedPointer<Tpetra::MultiVector<> >  zlo_ptr = assembler->createControlVector();
    //ROL::SharedPointer<Tpetra::MultiVector<> >  zhi_ptr = assembler->createControlVector();
    //zlo_ptr->putScalar(static_cast<RealT>(280));
    //zhi_ptr->putScalar(static_cast<RealT>(370));
    //ROL::SharedPointer<ROL::TpetraMultiVector<RealT> > zlopde
    //  = ROL::makeShared<PDE_PrimalOptVector<RealT>>(zlo_ptr,pde,assembler);
    //ROL::SharedPointer<ROL::TpetraMultiVector<RealT> > zhipde
    //  = ROL::makeShared<PDE_PrimalOptVector<RealT>>(zhi_ptr,pde,assembler);
    //ROL::SharedPointer<ROL::Vector<RealT> > zlop = ROL::makeShared<PDE_OptVector<RealT>>(zlopde);
    //ROL::SharedPointer<ROL::Vector<RealT> > zhip = ROL::makeShared<PDE_OptVector<RealT>>(zhipde);
    ROL::SharedPointer<ROL::BoundConstraint<RealT> > bnd
      = ROL::makeShared<ROL::Bounds<RealT>>(zlop,zhip);

    /*************************************************************************/
    /***************** BUILD SAMPLER *****************************************/
    /*************************************************************************/
    int nsamp = parlist->sublist("Problem").get("Number of Samples",100);
    std::vector<RealT> tmp = {-one,one};
    std::vector<std::vector<RealT> > bounds(stochDim,tmp);
    ROL::SharedPointer<ROL::BatchManager<RealT> > bman
      = ROL::makeShared<PDE_OptVector_BatchManager<RealT>>(comm);
    ROL::SharedPointer<ROL::SampleGenerator<RealT> > sampler
      = ROL::makeShared<ROL::MonteCarloGenerator<RealT>>(nsamp,bounds,bman);

    /*************************************************************************/
    /***************** BUILD STOCHASTIC PROBLEM ******************************/
    /*************************************************************************/
    ROL::OptimizationProblem<RealT> opt(objReduced,zp,bnd);
    parlist->sublist("SOL").set("Initial Statistic", one);
    opt.setStochasticObjective(*parlist,sampler);

    /*************************************************************************/
    /***************** RUN VECTOR AND DERIVATIVE CHECKS **********************/
    /*************************************************************************/
    bool checkDeriv = parlist->sublist("Problem").get("Check Derivatives",false);
    if ( checkDeriv ) {
      up->checkVector(*pp,*dup,true,*outStream);
      zp->checkVector(*yzp,*dzp,true,*outStream);
      obj->checkGradient(x,d,true,*outStream);
      obj->checkHessVec(x,d,true,*outStream);
      con->checkApplyJacobian(x,d,*up,true,*outStream);
      con->checkApplyAdjointHessian(x,*dup,d,x,true,*outStream);
      con->checkAdjointConsistencyJacobian(*dup,d,x,true,*outStream);
      con->checkInverseJacobian_1(*up,*up,*up,*zp,true,*outStream);
      con->checkInverseAdjointJacobian_1(*up,*up,*up,*zp,true,*outStream);
      objReduced->checkGradient(*zp,*dzp,true,*outStream);
      objReduced->checkHessVec(*zp,*dzp,true,*outStream);
      opt.check(*outStream);
    }

    /*************************************************************************/
    /***************** SOLVE OPTIMIZATION PROBLEM ****************************/
    /*************************************************************************/
    ROL::Algorithm<RealT> algo("Trust Region",*parlist,false);
    (*z_ptr)[0] = parlist->sublist("Problem").get("Advection Magnitude",0.0);
    u_ptr->putScalar(450.0);
    std::clock_t timer = std::clock();
    algo.run(opt,true,*outStream);
    *outStream << "Optimization time: "
               << static_cast<RealT>(std::clock()-timer)/static_cast<RealT>(CLOCKS_PER_SEC)
               << " seconds." << std::endl << std::endl;

    /*************************************************************************/
    /***************** OUTPUT RESULTS ****************************************/
    /*************************************************************************/
    std::clock_t timer_print = std::clock();
    assembler->printMeshData(*outStream);
    // Output control to file
    //pdecon->outputTpetraVector(z_ptr,"control.txt");
    *outStream << std::endl << "Advection value: " << (*z_ptr)[0] << std::endl;
    // Output expected state and samples to file
    *outStream << std::endl << "Print Expected Value of State" << std::endl;
    up->zero(); pp->zero(); dup->zero();
    RealT tol(1.e-8);
    ROL::SharedPointer<ROL::BatchManager<RealT> > bman_Eu
      = ROL::makeShared<ROL::TpetraTeuchosBatchManager<RealT>>(comm);
    std::vector<RealT> sample(stochDim);
    std::stringstream name_samp;
    name_samp << "samples_" << bman->batchID() << ".txt";
    std::ofstream file_samp;
    file_samp.open(name_samp.str());
    file_samp << std::scientific << std::setprecision(15);
    for (int i = 0; i < sampler->numMySamples(); ++i) {
      *outStream << "Sample i = " << i << std::endl;
      sample = sampler->getMyPoint(i);
      con->setParameter(sample);
      con->solve(*rp,*dup,*zp,tol);
      up->axpy(sampler->getMyWeight(i),*dup);
      for (int j = 0; j < stochDim; ++j) {
        file_samp << std::setw(25) << std::left << sample[j];
      }
      file_samp << std::endl;
    }
    file_samp.close();
    bman_Eu->sumAll(*up,*pp);
    pdecon->outputTpetraVector(p_ptr,"mean_state.txt");
    // Build objective function distribution
    *outStream << std::endl << "Print Objective CDF" << std::endl;
    RealT val1(0), val2(0);
    int nsamp_dist = parlist->sublist("Problem").get("Number of Output Samples",100);
    ROL::SharedPointer<ROL::Objective_SimOpt<RealT> > stateCost
      = ROL::makeShared<IntegralObjective<RealT>>(qoi_vec[0],assembler);
    ROL::SharedPointer<ROL::Reduced_Objective_SimOpt<RealT> > redStateCost
      = ROL::makeShared<ROL::Reduced_Objective_SimOpt<RealT>>(stateCost, con, up, zp, pp, true, false);
    ROL::SharedPointer<ROL::Objective_SimOpt<RealT> > ctrlCost
      = ROL::makeShared<IntegralObjective<RealT>>(qoi_vec[1],assembler);
    ROL::SharedPointer<ROL::Reduced_Objective_SimOpt<RealT> > redCtrlCost
      = ROL::makeShared<ROL::Reduced_Objective_SimOpt<RealT>>(ctrlCost, con, up, zp, pp, true, false);
    ROL::SharedPointer<ROL::SampleGenerator<RealT> > sampler_dist
      = ROL::makeShared<ROL::MonteCarloGenerator<RealT>>(nsamp_dist,bounds,bman);
    std::stringstream name;
    name << "obj_samples_" << bman->batchID() << ".txt";
    std::ofstream file;
    file.open(name.str());
    file << std::scientific << std::setprecision(15);
    for (int i = 0; i < sampler_dist->numMySamples(); ++i) {
      sample = sampler_dist->getMyPoint(i);
      redStateCost->setParameter(sample);
      val1 = redStateCost->value(*zp,tol);
      redCtrlCost->setParameter(sample);
      val2 = redCtrlCost->value(*zp,tol);
      for (int j = 0; j < stochDim; ++j) {
        file << std::setw(25) << std::left << sample[j];
      }
      file << std::setw(25) << std::left << val1;
      file << std::setw(25) << std::left << val2 << std::endl;
    }
    file.close();
    *outStream << "Output time: "
               << static_cast<RealT>(std::clock()-timer_print)/static_cast<RealT>(CLOCKS_PER_SEC)
               << " seconds." << std::endl << std::endl;

    Teuchos::Array<RealT> res(1,0);
    con->solve(*rp,*up,*zp,tol);
    r_ptr->norm2(res.view(0,1));

    /*************************************************************************/
    /***************** CHECK RESIDUAL NORM ***********************************/
    /*************************************************************************/
    *outStream << "Residual Norm: " << res[0] << std::endl << std::endl;
    errorFlag += (res[0] > 1.e-6 ? 1 : 0);
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
