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
#include <iostream>
#include <fstream>
#include <limits>
#include <vector>
#include <Teuchos_RCP.hpp>
#include <Teuchos_ParameterList.hpp>
#include <Teuchos_CommandLineProcessor.hpp>
#include <Tpetra_CrsMatrix.hpp>
#include <Tpetra_Core.hpp>
#include <Tpetra_Vector.hpp>
#include <MatrixMarket_Tpetra.hpp>
#include <Zoltan2_XpetraCrsMatrixAdapter.hpp>
#include <Zoltan2_TestHelpers.hpp>
#include <Zoltan2_ColoringProblem.hpp>

using Teuchos::RCP;
using namespace std;

/////////////////////////////////////////////////////////////////////////////
// Program to demonstrate use of Zoltan2 to color a TPetra matrix
// (read from a MatrixMarket file or generated by Galeri::Xpetra).
// We assume the matrix is structurally symmetric.
// Usage:
//     a.out [--inputFile=filename] [--outputFile=outfile] [--verbose]
//           [--x=#] [--y=#] [--z=#] [--matrix={Laplace1D,Laplace2D,Laplace3D}
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// Eventually want to use Teuchos unit tests to vary z2TestLO and
// GO.  For now, we set them at compile time based on whether Tpetra
// is built with explicit instantiation on.  (in Zoltan2_TestHelpers.hpp)

typedef zlno_t z2TestLO;
typedef zgno_t z2TestGO;
typedef zscalar_t z2TestScalar;

typedef Tpetra::CrsMatrix<z2TestScalar, z2TestLO, z2TestGO> SparseMatrix;
typedef Tpetra::Vector<z2TestScalar, z2TestLO, z2TestGO> Vector;
typedef Vector::node_type Node;

typedef Zoltan2::XpetraCrsMatrixAdapter<SparseMatrix> SparseMatrixAdapter;

int validateColoring(RCP<SparseMatrix> A, int *color)
// returns 0 if coloring is valid, nonzero if invalid
{
  int nconflicts = 0;
  Teuchos::ArrayView<const zlno_t> indices;
  Teuchos::ArrayView<const zscalar_t> values; // Not used

  // Count conflicts in the graph.
  // Loop over local rows, treat local column indices as edges.
  zlno_t n = A->getNodeNumRows();
  for (zlno_t i=0; i<n; i++) {
    A->getLocalRowView(i, indices, values);
    for (zlno_t j=0; j<indices.size(); j++) {
      if ((indices[j]<n) && (indices[j]!=i) && (color[i]==color[indices[j]])){
        nconflicts++;
        //std::cout << "Debug: found conflict (" << i << ", " << indices[j] << ")" << std::endl;
      }
    }
  }

  return nconflicts;
}

int checkBalance(zlno_t n, int *color)
// Check size of color classes
{
  // Find max color
  int maxColor = 0;
  for (zlno_t i=0; i<n; i++) {
    if (color[i] > maxColor) maxColor = color[i];
  }

  // Compute color class sizes
  Teuchos::Array<int> colorCount(maxColor+1);
  for (zlno_t i=0; i<n; i++) {
    colorCount[color[i]]++;
  }

  // Find min and max, excluding color 0.
  int smallest = 1;
  int largest  = 1;
  zlno_t small = colorCount[1];
  zlno_t large = colorCount[1];
  for (int i=1; i<=maxColor; i++){
    if (colorCount[i] < small){
      small = colorCount[i];
      smallest = i;
    }
    if (colorCount[i] > large){
      large = colorCount[i];
      largest = i;
    }
  }

  std::cout << "Color size[0:2] = " << colorCount[0] << ", " << colorCount[1] << ", " << colorCount[2] << std::endl;
  std::cout << "Largest color class = " << largest << " with " << colorCount[largest] << " vertices." << std::endl;
  std::cout << "Smallest color class = " << smallest << " with " << colorCount[smallest] << " vertices." << std::endl;

  return 0;
}

/////////////////////////////////////////////////////////////////////////////
int main(int narg, char** arg)
{
  std::string inputFile = "";            // Matrix Market file to read
  std::string outputFile = "";           // Output file to write
  bool verbose = false;                  // Verbosity of output
  int testReturn = 0;

  ////// Establish session.
  Teuchos::GlobalMPISession mpiSession(&narg, &arg, NULL);
  RCP<const Teuchos::Comm<int> > comm = Tpetra::getDefaultComm();
  int me = comm->getRank();

  // Read run-time options.
  Teuchos::CommandLineProcessor cmdp (false, false);
  cmdp.setOption("inputFile", &inputFile,
                 "Name of a Matrix Market file in the data directory; "
                 "if not specified, a matrix will be generated by Galeri.");
  cmdp.setOption("outputFile", &outputFile,
                 "Name of file to write the coloring");
  cmdp.setOption("verbose", "quiet", &verbose,
                 "Print messages and results.");
  cout << "Starting everything" << endl;

  //////////////////////////////////
  // Even with cmdp option "true", I get errors for having these
  //   arguments on the command line.  (On redsky build)
  // KDDKDD Should just be warnings, right?  Code should still work with these
  // KDDKDD params in the create-a-matrix file.  Better to have them where
  // KDDKDD they are used.
  int xdim=10;
  int ydim=10;
  int zdim=10;
  std::string matrixType("Laplace3D");

  cmdp.setOption("x", &xdim,
                "number of gridpoints in X dimension for "
                "mesh used to generate matrix.");
  cmdp.setOption("y", &ydim,
                "number of gridpoints in Y dimension for "
                "mesh used to generate matrix.");
  cmdp.setOption("z", &zdim,
                "number of gridpoints in Z dimension for "
                "mesh used to generate matrix.");
  cmdp.setOption("matrix", &matrixType,
                "Matrix type: Laplace1D, Laplace2D, or Laplace3D");

  //////////////////////////////////
  // Coloring options to test.
  //////////////////////////////////
  std::string colorMethod("FirstFit");
  //int balanceColors = 0;
  cmdp.setOption("color_choice", &colorMethod,
       "Color choice method: FirstFit, LeastUsed, Random, RandomFast");
  // cmdp.setOption("balance_colors", &balanceColors,
  //                "Balance the size of color classes: 0/1 for false/true");

  //////////////////////////////////
  cmdp.parse(narg, arg);

  RCP<UserInputForTests> uinput;

  if (inputFile != ""){ // Input file specified; read a matrix
    uinput = rcp(new UserInputForTests(testDataFilePath, inputFile, comm, true));
  }
  else                  // Let Galeri generate a matrix

    uinput = rcp(new UserInputForTests(xdim, ydim, zdim, matrixType, comm, true, true));

  RCP<SparseMatrix> Matrix = uinput->getUITpetraCrsMatrix();

  if (me == 0)
    cout << "NumRows     = " << Matrix->getGlobalNumRows() << endl
         << "NumNonzeros = " << Matrix->getGlobalNumEntries() << endl
         << "NumProcs = " << comm->getSize() << endl;

  ////// Specify problem parameters
  Teuchos::ParameterList params;
  params.set("color_choice", colorMethod);
  //params.set("balance_colors", balanceColors); // TODO

  ////// Create an input adapter for the Tpetra matrix.
  SparseMatrixAdapter adapter(Matrix);

  ////// Create and solve ordering problem
  try
  {
  Zoltan2::ColoringProblem<SparseMatrixAdapter> problem(&adapter, &params);
  cout << "Going to color" << endl;
  problem.solve();

  ////// Basic metric checking of the coloring solution
  size_t checkLength;
  int *checkColoring;
  Zoltan2::ColoringSolution<SparseMatrixAdapter> *soln = problem.getSolution();

  cout << "Going to get results" << endl;
  // Check that the solution is really a coloring
  checkLength = soln->getColorsSize();
  checkColoring = soln->getColors();

  if (outputFile != "") {
    ofstream colorFile;

    // Write coloring to file,
    // each process writes local coloring to a separate file
    //std::string fname = outputFile + "." + me;
    std::stringstream fname;
    fname << outputFile << "." << comm->getSize() << "." << me;
    colorFile.open(fname.str().c_str());
    for (size_t i=0; i<checkLength; i++){
      colorFile << " " << checkColoring[i] << endl;
    }
    colorFile.close();
  }

  // Print # of colors on each proc.
  cout << "No. of colors on proc " << me << " : " << soln->getNumColors() << endl;

  cout << "Going to validate the soln" << endl;
  // Verify that checkColoring is a coloring
  testReturn = validateColoring(Matrix, checkColoring);

  // Check balance (not part of pass/fail for now)
  checkBalance((zlno_t)checkLength, checkColoring);

  } catch (std::exception &e){
      std::cout << "Exception caught in coloring" << std::endl;
      std::cout << e.what() << std::endl;
      std::cout << "FAIL" << std::endl;
      return 0;
  }

  if (me == 0) {
    if (testReturn)
      std::cout << "Solution is not a valid coloring; FAIL" << std::endl;
    else
      std::cout << "PASS" << std::endl;
  }

}

