// Epetra_Object Test routine

#ifdef EPETRA_MPI
#include "Epetra_MpiComm.h"
#include <mpi.h>
#else
#include "Epetra_SerialComm.h"
#endif

#include "Epetra_Object.h"

int main(int argc, char *argv[]) {

#ifdef EPETRA_MPI

  // Initialize MPI

  MPI_Init(&argc,&argv);
  int size, rank; // Number of MPI processes, My process ID

  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  Epetra_MpiComm comm( MPI_COMM_WORLD );

#else

  int size = 1; // Serial case (not using MPI)
  int rank = 0;

  Epetra_SerialComm comm;

#endif

  bool verbose = false;

  // Check if we should print results to standard out
  if (argc>1) if (argv[1][0]=='-' && argv[1][1]=='v') verbose = true;


  // I'm alive !!!
  if (verbose) cout << comm <<endl;

  Epetra_Object obj;

  if (verbose) cout << "This is the default Epetra_Object Name: " << obj <<endl;

  obj.SetLabel("New name for Epetra_Object");

  if (verbose) cout << "This should say \"New name for Epetra_Object\": " << obj <<endl;

  assert(obj.GetTracebackMode()==ConstDefaultTracebackMode);
  if (verbose) cout << "Default Traceback Mode value = " << obj.GetTracebackMode() << endl;

  obj.SetTracebackMode(ConstDefaultTracebackMode-1);
  if (verbose) cout << "Set Traceback Mode value to one less than default = " << obj.GetTracebackMode() << endl;
  assert(obj.GetTracebackMode()==ConstDefaultTracebackMode-1);

#ifdef EPETRA_MPI
  MPI_Finalize();
#endif
  return 0;
}

/*
  end of file main.cc
*/
