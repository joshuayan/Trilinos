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
    \brief Shows how to solve the Navier-Stokes control problem.
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

#include "ROL_TpetraMultiVector.hpp"
#include "ROL_Algorithm.hpp"
#include "ROL_Reduced_Objective_SimOpt.hpp"

#include "../TOOLS/meshmanager.hpp"
#include "../TOOLS/pdeconstraint.hpp"
#include "../TOOLS/pdeobjective.hpp"
#include "../TOOLS/pdevector.hpp"
#include "pde_thermal-fluids.hpp"
#include "obj_thermal-fluids.hpp"

typedef double RealT;

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

    /*** Initialize main data structure. ***/
    ROL::SharedPointer<MeshManager<RealT> > meshMgr
      = ROL::makeShared<MeshManager_BackwardFacingStepChannel<RealT>>(*parlist);
    // Initialize PDE describing Navier-Stokes equations.
    ROL::SharedPointer<PDE_ThermalFluids<RealT> > pde
      = ROL::makeShared<PDE_ThermalFluids<RealT>>(*parlist);
    ROL::SharedPointer<ROL::Constraint_SimOpt<RealT> > con
      = ROL::makeShared<PDE_Constraint<RealT>>(pde,meshMgr,comm,*parlist,*outStream);
    // Cast the constraint and get the assembler.
    ROL::SharedPointer<PDE_Constraint<RealT> > pdecon
      = ROL::dynamicPointerCast<PDE_Constraint<RealT> >(con);
    ROL::SharedPointer<Assembler<RealT> > assembler = pdecon->getAssembler();
    con->setSolveParameters(*parlist);
    pdecon->outputTpetraData();

    // Create state vector and set to zeroes
    ROL::SharedPointer<Tpetra::MultiVector<> > u_ptr, p_ptr, du_ptr, r_ptr, z_ptr, dz_ptr;
    u_ptr  = assembler->createStateVector();     u_ptr->randomize();
    p_ptr  = assembler->createStateVector();     p_ptr->randomize();
    du_ptr = assembler->createStateVector();     du_ptr->randomize();
    r_ptr  = assembler->createResidualVector();  r_ptr->randomize();
    z_ptr  = assembler->createControlVector();   z_ptr->randomize();
    dz_ptr = assembler->createControlVector();   dz_ptr->randomize();
    ROL::SharedPointer<ROL::Vector<RealT> > up, pp, dup, rp, zp, dzp;
    up  = ROL::makeShared<PDE_PrimalSimVector<RealT>>(u_ptr,pde,assembler);
    pp  = ROL::makeShared<PDE_PrimalSimVector<RealT>>(p_ptr,pde,assembler);
    dup = ROL::makeShared<PDE_PrimalSimVector<RealT>>(du_ptr,pde,assembler);
    rp  = ROL::makeShared<PDE_DualSimVector<RealT>>(r_ptr,pde,assembler);
    zp  = ROL::makeShared<PDE_PrimalOptVector<RealT>>(z_ptr,pde,assembler);
    dzp = ROL::makeShared<PDE_PrimalOptVector<RealT>>(dz_ptr,pde,assembler);
    // Create ROL SimOpt vectors
    ROL::Vector_SimOpt<RealT> x(up,zp);
    ROL::Vector_SimOpt<RealT> d(dup,dzp);

    // Initialize objective function.
    std::vector<ROL::SharedPointer<QoI<RealT> > > qoi_vec(2,ROL::nullPointer);
    qoi_vec[0] = ROL::makeShared<QoI_State_ThermalFluids<RealT>>(*parlist,
                                                                 pde->getVelocityFE(),
                                                                 pde->getPressureFE(),
                                                                 pde->getThermalFE(),
                                                                 pde->getFieldHelper());
    qoi_vec[1] = ROL::makeShared<QoI_L2Penalty_ThermalFluids<RealT>>(pde->getVelocityFE(),
                                                                     pde->getPressureFE(),
                                                                     pde->getThermalFE(),
                                                                     pde->getThermalBdryFE(),
                                                                     pde->getBdryCellLocIds(),
                                                                     pde->getFieldHelper());
    ROL::SharedPointer<StdObjective_ThermalFluids<RealT> > std_obj
      = ROL::makeShared<StdObjective_ThermalFluids<RealT>>(*parlist);
    ROL::SharedPointer<ROL::Objective_SimOpt<RealT> > obj
      = ROL::makeShared<PDE_Objective<RealT>>(qoi_vec,std_obj,assembler);
    ROL::SharedPointer<ROL::SimController<RealT> > stateStore
      = ROL::makeShared<ROL::SimController<RealT>>();
    ROL::SharedPointer<ROL::Reduced_Objective_SimOpt<RealT> > robj
      = ROL::makeShared<ROL::Reduced_Objective_SimOpt<RealT>>(obj, con, stateStore, up, zp, pp, true, false);

    //up->zero();
    //zp->zero();
    //z_ptr->putScalar(1.e0);
    //dz_ptr->putScalar(0);

    // Run derivative checks
    bool checkDeriv = parlist->sublist("Problem").get("Check derivatives",false);
    if ( checkDeriv ) {
      *outStream << "Check Gradient of Full Objective Function" << std::endl;
      obj->checkGradient(x,d,true,*outStream);
      *outStream << std::endl << "Check Hessian of Full Objective Function" << std::endl;
      obj->checkHessVec(x,d,true,*outStream);
      *outStream << std::endl << "Check Jacobian of Constraint" << std::endl;
      con->checkApplyJacobian(x,d,*up,true,*outStream);
      *outStream << std::endl << "Check Jacobian_1 of Constraint" << std::endl;
      con->checkApplyJacobian_1(*up,*zp,*dup,*rp,true,*outStream);
      *outStream << std::endl << "Check Jacobian_2 of Constraint" << std::endl;
      con->checkApplyJacobian_2(*up,*zp,*dzp,*rp,true,*outStream);
      *outStream << std::endl << "Check Hessian of Constraint" << std::endl;
      con->checkApplyAdjointHessian(x,*dup,d,x,true,*outStream);
      *outStream << std::endl << "Check Hessian_11 of Constraint" << std::endl;
      con->checkApplyAdjointHessian_11(*up,*zp,*pp,*dup,*rp,true,*outStream);
      *outStream << std::endl << "Check Hessian_12 of Constraint" << std::endl;
      con->checkApplyAdjointHessian_12(*up,*zp,*pp,*dup,*dzp,true,*outStream);
      *outStream << std::endl << "Check Hessian_21 of Constraint" << std::endl;
      con->checkApplyAdjointHessian_21(*up,*zp,*pp,*dzp,*rp,true,*outStream);
      *outStream << std::endl << "Check Hessian_22 of Constraint" << std::endl;
      con->checkApplyAdjointHessian_22(*up,*zp,*pp,*dzp,*dzp,true,*outStream);

      *outStream << std::endl << "Check Adjoint Jacobian of Constraint" << std::endl;
      con->checkAdjointConsistencyJacobian(*dup,d,x,true,*outStream);
      *outStream << std::endl << "Check Adjoint Jacobian_1 of Constraint" << std::endl;
      con->checkAdjointConsistencyJacobian_1(*pp,*dup,*up,*zp,true,*outStream);
      *outStream << std::endl << "Check Adjoint Jacobian_2 of Constraint" << std::endl;
      con->checkAdjointConsistencyJacobian_2(*pp,*dzp,*up,*zp,true,*outStream);

      *outStream << std::endl << "Check Constraint Solve" << std::endl;
      con->checkSolve(*up,*zp,*rp,true,*outStream);
      *outStream << std::endl << "Check Inverse Jacobian_1 of Constraint" << std::endl;
      con->checkInverseJacobian_1(*rp,*dup,*up,*zp,true,*outStream);
      *outStream << std::endl << "Check Inverse Adjoint Jacobian_1 of Constraint" << std::endl;
      con->checkInverseAdjointJacobian_1(*rp,*pp,*up,*zp,true,*outStream);

      *outStream << std::endl << "Check Gradient of Reduced Objective Function" << std::endl;
      robj->checkGradient(*zp,*dzp,true,*outStream);
      *outStream << std::endl << "Check Hessian of Reduced Objective Function" << std::endl;
      robj->checkHessVec(*zp,*dzp,true,*outStream);
    }
    up->zero();
    zp->zero();

    RealT tol(1.e-8);
    con->solve(*rp,*up,*zp,tol);
    pdecon->outputTpetraVector(u_ptr,"state_uncontrolled.txt");

    bool useCompositeStep = parlist->sublist("Problem").get("Full space",false);
    ROL::SharedPointer<ROL::Algorithm<RealT> > algo;
    if ( useCompositeStep ) {
      algo = ROL::makeShared<ROL::Algorithm<RealT>>("Composite Step",*parlist,false);
      algo->run(x,*rp,*obj,*con,true,*outStream);
    }
    else {
      algo = ROL::makeShared<ROL::Algorithm<RealT>>("Trust Region",*parlist,false);
      algo->run(*zp,*robj,true,*outStream);
      std::vector<RealT> param;
      stateStore->get(*up,param);
    }

    // Output.
    assembler->printMeshData(*outStream);
    Teuchos::Array<RealT> res(1,0);
    //con->solve(*rp,*up,*zp,tol);
    pdecon->outputTpetraVector(u_ptr,"state.txt");
    pdecon->outputTpetraVector(z_ptr,"control.txt");
    con->value(*rp,*up,*zp,tol);
    r_ptr->norm2(res.view(0,1));
    *outStream << "Residual Norm: " << res[0] << std::endl;
    errorFlag += (res[0] > 1.e-6 ? 1 : 0);
    //pdecon->outputTpetraData();

    ROL::SharedPointer<ROL::Objective_SimOpt<RealT> > obj0
      = ROL::makeShared<IntegralObjective<RealT>>(qoi_vec[0],assembler);
    RealT val = obj0->value(*up,*zp,tol);
    *outStream << "Vorticity Value: " << val << std::endl;
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
