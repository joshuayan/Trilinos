// $Id$
// $Source$

//@HEADER
// ************************************************************************
//
//            LOCA: Library of Continuation Algorithms Package
//                 Copyright (2005) Sandia Corporation
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
// Questions? Contact Roger Pawlowski (rppawlo@sandia.gov) or
// Eric Phipps (etphipp@sandia.gov), Sandia National Laboratories.
// ************************************************************************
//  CVS Information
//  $Source$
//  $Author$
//  $Date$
//  $Revision$
// ************************************************************************
//@HEADER

//-----------------------------------------------------------------------------
#ifndef Problem_Interface_H
#define Problem_Interface_H

// Interface to the NLS_PetraGroup to provide for
// residual and matrix fill routines.

// ---------- Standard Includes ----------
#include <iostream>
#include "Epetra_Vector.h"
#include "LOCA_Epetra.H"

// ---------- Forward Declarations ----------
class FiniteElementProblem;
namespace LOCA {
  class ParameterVector;
}


class  Problem_Interface :
  public LOCA::Epetra::Interface::TimeDependent,
  public NOX::Epetra::Interface::Jacobian
{
public:
  Problem_Interface(FiniteElementProblem& Problem);
  ~Problem_Interface();

  //! Compute and return F
  bool computeF(const Epetra_Vector& x, Epetra_Vector& F, FillType flag);

  //! Compute an explicit Jacobian
  bool computeJacobian(const Epetra_Vector& x, Epetra_Operator& Jac);

  /*!
  * \brief Call user routine for computing the shifted matrix
  * \f$\alpha J + \beta M\f$ where \f$J\f$ is the Jacobian matrix
  * and \f$M\f$ is the mass matrix.
  */
  bool computeShiftedMatrix(double alpha, double beta,
                const Epetra_Vector& x,
                 Epetra_Operator& A);

  //! Set a parameter in the user's code.
  void setParameters(const LOCA::ParameterVector& params);

  //! Application Operator: Object that points to the user's evaluation routines.
  /*! This is used to point to the actual routines and to store
   *  auxiliary data required by the user's application for function/Jacobian
   *  evaluations that NOX does not need to know about.  This is type of
   *  passdown class design by the application code.
   */
  FiniteElementProblem& problem;

  //! Number of residual fills
  int numFillsF;
};

#endif

