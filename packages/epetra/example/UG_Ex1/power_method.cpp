#include <stdio.h>
#include <stdlib.h>
#include "Epetra_Comm.h"
#include "Epetra_Map.h"
#include "Epetra_Vector.h"
#include "Epetra_CrsMatrix.h"
// Simple Power method algorithm
double power_method(const Epetra_CrsMatrix& A) {  
  // variable needed for iteration
  double lambda = 0.0;
  int niters = A.RowMap().NumGlobalElements()*10;
  double tolerance = 1.0e-10;
  // Create vectors
  Epetra_Vector q(A.RowMap());
  Epetra_Vector z(A.RowMap());
  Epetra_Vector resid(A.RowMap());
  // Fill z with random Numbers
  z.Random();
  // variable needed for iteration
  double normz;
  double residual = 0;
  int iter = 0;
  while (iter==0 || (iter < niters && residual > tolerance)) {
    z.Norm2(&normz); // Compute 2-norm of z
    q.Scale(1.0/normz, z);
    A.Multiply(false, q, z); // Compute z = A*q
    q.Dot(z, &lambda); // Approximate maximum eigenvalue
    if (iter%10==0 || iter+1==niters) {
      // Compute A*q - lambda*q every 10 iterations
      resid.Update(1.0, z, -lambda, q, 0.0);
      resid.Norm2(&residual);
      if (q.Map().Comm().MyPID()==0)
	cout << "Iter = " << iter << "  Lambda = " << lambda 
	     << "  Two-norm of A*q - lambda*q = " 
	     << residual << endl;
    } 
    iter++;
  }
  return(lambda);
}
