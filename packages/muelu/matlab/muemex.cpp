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
//                    Jonathan Hu       (jhu@sandia.gov)
//                    Andrey Prokopenko (aprokop@sandia.gov)
//                    Ray Tuminaro      (rstumin@sandia.gov)
//
// ***********************************************************************
//
// @HEADER

#include "muemex.h"
#ifdef HAVE_MUELU_MATLAB
#define ABS(x)   ((x)>0?(x):(-(x)))
#define MAX(x,y) ((x)>(y)?(x):(y))
#define FLOOR(x) ((int)(x))
#define ISINT(x) ((x)==0?(((x-(int)(x))<1e-15)?true:false):(((x-(int)(x))<1e-15*ABS(x))?true:false))
#define IS_FALSE 0
#define IS_TRUE 1
#define MUEMEX_ERROR -1

using namespace std;
using namespace Teuchos;

extern void _main();

typedef enum
{
    MODE_SETUP,
    MODE_SOLVE,
    MODE_CLEANUP,
    MODE_STATUS,
    MODE_AGGREGATE,
    MODE_SETUP_MAXWELL,
    MODE_SOLVE_NEWMATRIX,
    MODE_ERROR
} MODE_TYPE;

/* MUEMEX Teuchos Parameters*/
#define MUEMEX_INTERFACE "muemex: interface"

/* Default values */
#define MUEMEX_DEFAULT_LEVELS 10
#define MUEMEX_DEFAULT_NUMPDES 1
#define MUEMEX_DEFAULT_ADAPTIVEVECS 0
#define MUEMEX_DEFAULT_USEDEFAULTNS true

/* Debugging */
//#define VERBOSE_OUTPUT

/* Stuff for MATLAB R2006b vs. previous versions */
#if(defined(MX_API_VER) && MX_API_VER >= 0x07030000)
#else
typedef int mwIndex;
#endif

//Declare and call default constructor for data_pack_list vector (starts empty)
vector<muelu_data_pack*> muelu_data_pack_list::list;
int muelu_data_pack_list::nextID = 0;

/**************************************************************/
/**************************************************************/
/**************************************************************/
/* Epetra utility functions */

/* mwIndex_to_int - does a data copy and wraps mwIndex's to ints, in the case
   where they're not the same size.  This routine allocates memory
   WARNING: This does not address overflow.
   Parameters:
   N         - Number of unknowns in array [I]
   mwi_array - Array of mwIndex objects [I]
   Return value: mwIndex objects cast down to ints
*/
int* mwIndex_to_int(int N, mwIndex* mwi_array)
{
	int i, *rv = new int[N];
	for(i = 0; i < N; i++)
		rv[i] = (int) mwi_array[i];
	return rv;
}

/*******************************/ 
Epetra_CrsMatrix* epetra_setup(int Nrows, int Ncols, int* rowind, int* colptr, double* vals)
{
	Epetra_SerialComm Comm;
	Epetra_Map RangeMap(Nrows, 0, Comm);
	Epetra_Map DomainMap(Ncols, 0, Comm);
	Epetra_CrsMatrix *A = new Epetra_CrsMatrix(Epetra_DataAccess::Copy, RangeMap, DomainMap, 0);
	/* Do the matrix assembly */
	for(int i = 0; i < Ncols; i++)
	{
		for(int j = colptr[i]; j < colptr[i + 1]; j++)
		{
    		A->InsertGlobalValues(rowind[j], 1, &vals[j], &i);
    	}
	}
	A->FillComplete(DomainMap, RangeMap);
	return A;
}

/*******************************/
Epetra_CrsMatrix * epetra_setup_from_prhs(const mxArray* mxa, bool rewrap_ints)
{
	int* colptr, *rowind;
	double* vals = mxGetPr(mxa);

	int nr = mxGetM(mxa);
	int nc = mxGetN(mxa);

	if(rewrap_ints)
	{
		colptr = mwIndex_to_int(nc + 1, mxGetJc(mxa));
		rowind = mwIndex_to_int(colptr[nc], mxGetIr(mxa));
	}
	else
	{
		rowind = (int*) mxGetIr(mxa);
		colptr = (int*) mxGetJc(mxa);
	}

	Epetra_CrsMatrix* A = epetra_setup(nr, nc, rowind, colptr, vals);

	if(rewrap_ints)
	{
		delete [] rowind;
		delete [] colptr;
	}
	return A;
}

/*******************************/
//Use Belos to solve the matrix, instead of AztecOO like in mlmex
int epetra_solve(Teuchos::ParameterList *SetupList, Teuchos::ParameterList *TPL, Epetra_CrsMatrix* A, double* b, double* x, int &iters)
{
	using Teuchos::ParameterList;
	using Teuchos::parameterList;
	using Teuchos::RCP;
	using Teuchos::rcp;
	int i;
	int N = A->NumMyRows();
    RCP<Epetra_Vector> LHS = rcp(new Epetra_Vector(A->RowMap()));
    RCP<Epetra_Vector> RHS = rcp(new Epetra_Vector(A->RowMap()));
	RCP<Epetra_CrsMatrix> matrix = rcp(A, false);
    /* Fill LHS/RHS */
    LHS->PutScalar(0);
    for(i = 0; i < N; i++)
		(*RHS)[i] = b[i];
    /* Create the new Teuchos List */
	RCP<ParameterList> tmp = rcp(SetupList, true);
	tmp->setParameters(*TPL);
    /* Define Problem / Solver */
	RCP<Belos::LinearProblem<double, Epetra_MultiVector, Epetra_Operator>> Problem = rcp(new Belos::LinearProblem<double, Epetra_MultiVector, Epetra_Operator> (matrix, LHS, RHS));
	/* Gather options from the given Teuchos::ParameterList */
	//Belos can get these from the ParamList by itself, right?
	//With AztecOO each must be passed manually so i'm not sure
	//int    NumIters = tmp->get("krylov: max iterations", 1550);
    //double Tol      = tmp->get("krylov: tolerance", 1e-9);
    string type     = tmp->get("krylov: type", "GMRES");
    //int    output   = tmp->get("krylov: output level",10);
    //int    kspace   = tmp->get("krylov: kspace",30);
    //string conv     = tmp->get("krylov: conv", "r0");
    /*Create the Belos solver factory and solver */
	Belos::SolverFactory<double, Epetra_MultiVector, Epetra_Operator> factory;
    tmp->print();
	RCP<Belos::SolverManager<double, Epetra_MultiVector, Epetra_Operator>> solver = factory.create(type, tmp);
	/* Hand the solver the problem */
	solver->setProblem(Problem);
    /* Do the solve */
    solver->solve();
mexPrintf("Hello world 3");
    /* Fill Solution */
    for(i = 0; i < N; i++) 
	    x[i] = (*LHS)[i];

    /* Solver Output */
    iters = solver->getNumIters();
    return IS_TRUE;
}   /*end solve*/

/**************************************************************/
/**************************************************************/
/**************** muelu_data_pack class functions ****************/
/**************************************************************/
/**************************************************************/
muelu_data_pack::muelu_data_pack() : id(MUEMEX_ERROR), List(NULL), next(NULL) {}
muelu_data_pack::~muelu_data_pack()
{
	if(List)
		delete List;
}

/*************************************************************
**************************************************************
*************** mueluapi_data_pack class functions ***********
**************************************************************
**************************************************************
mueluapi_data_pack::mueluapi_data_pack()
{
	
}

mueluapi_data_pack::~mueluapi_data_pack()
{
	
} *end destructor*

* mueluapi_data_pack::status - This function does a status query on theas
   MUELUAPI_DATA_PACK passed in.
   Returns: IS_TRUE
*
int mueluapi_data_pack::status(){
  mexPrintf("**** Problem ID %d [MUELUAPI] ****\n",id);
  if(A) mexPrintf("Matrix: %dx%d w/ %d nnz\n",A->NumGlobalRows(),A->NumGlobalCols(),A->NumGlobalNonzeros());
  mexPrintf(" Operator complexity = %e\n",operator_complexity);
  if(List){mexPrintf("Parameter List:\n");List->print(cout,1);}
  mexPrintf("\n");
  return IS_TRUE;
}end status*/

/**************************************************************/
/**************************************************************/
/**************************************************************/
/* mueluapi_data_pack::setup - This function does the setup phase for MueLu, pulling
   parameters from the Teuchos list, and calling the aggregation routines
   Parameters:
   N       - Number of unknowns
   rowind  - Row indices of matrix (CSC format) [I]
   colptr  - Column indices of matrix (CSC format) [I]
   vals    - Nonzero values of matrix (CSC format) [I]
   Returns: IS_TRUE if setup was succesful, IS_FALSE otherwise
*
int mueluapi_data_pack::setup(int N,int* rowind,int* colptr, double* vals)
{
	//TODO:MueLu multigrid object(s) setup here?
	return 0;
}   end setup*/	

/**************************************************************/
/**************************************************************/
/**************************************************************/
/* mueluapi_data_pack::solve - Given two Teuchos lists, one in the MLAPI_DATA_PACK, and one of
   solve-time options, this routine calls the relevant solver and returns the solution.
   Parameters:
   TPL     - Teuchos list of solve-time options [I]
   N       - Number of unknowns [I]
   b       - RHS vector [I]
   x       - solution vector [O]
   iters   - number of iterations taken [O] (NOT IMPLEMENTED)
   Returns: IS_TRUE if solve was succesful, IS_FALSE otherwise
*
int mueluapi_data_pack::solve(Teuchos::ParameterList *TPL, int N, double* b, double* x, int &iters)
{
	//TODO: Invoke MueLu multigrid preconditioner?
	return IS_TRUE;
}*end solve*/

/**************************************************************/
/**************************************************************/
/************* muelu_epetra_data_pack class functions ************/
/**************************************************************/
/**************************************************************/
muelu_epetra_data_pack::muelu_epetra_data_pack():muelu_data_pack(), A(NULL) {}
muelu_epetra_data_pack::~muelu_epetra_data_pack()
{
	if(A)
		delete A;
}

/* muelu_epetra_data_pack_status - This function does a status query on the
   MUELU_EPETRA_DATA_PACK passed in.
   Returns: IS_TRUE
*/
int muelu_epetra_data_pack::status()
{
	mexPrintf("**** Problem ID %d [MueLu_Epetra] ****\n", id);
	if(A)
		mexPrintf("Matrix: %dx%d w/ %d nnz\n", A->NumGlobalRows(), A->NumGlobalCols(), A->NumMyNonzeros());
	//TODO: Find out what operator complexity is
	//mexPrintf(" Operator complexity = %e\n",operator_complexity);
	if(List)
	{
		mexPrintf("Parameter List:\n");
		List->print(cout, 1);
	}
	mexPrintf("\n");
	return IS_TRUE;
}/*end status*/

/**************************************************************/
/**************************************************************/
/**************************************************************/
/* muelu_epetra_data_pack::setup - This function does the setup phase for ML_Epetra, pulling
   key parameters from the Teuchos list, and calling the aggregation routines
   Parameters:
   N       - Number of unknowns [I]
   rowind  - Row indices of matrix (CSC format) [I]
   colptr  - Column indices of matrix (CSC format) [I]
   vals    - Nonzero values of matrix (CSC format) [I]
   Returns: IS_TRUE if setup was succesful, IS_FALSE otherwise
*/
int muelu_epetra_data_pack::setup(int N, int* rowind, int* colptr, double* vals)
{
	/* Matrix Fill */
	A = epetra_setup(N, N, rowind, colptr, vals);
	return IS_TRUE;
}/*end setup*/

/**************************************************************/
/**************************************************************/
/**************************************************************/
/* muelu_epetra_data_pack::solve - Given two Teuchos lists, one in the muelu_epetra_data_pack, and one of
   solve-time options, this routine calls the relevant solver and returns the solution.
   Parameters:
   TPL     - Teuchos list of solve-time options [I]
   A       - The matrix to solve with (may not be the one the preconditioned was used for)
   b       - RHS vector [I]
   x       - solution vector [O]
   iters   - number of iterations taken [O]
   Returns: IS_TRUE if solve was succesful, IS_FALSE otherwise
*/
int muelu_epetra_data_pack::solve(Teuchos::ParameterList* TPL, Epetra_CrsMatrix* Amat, double* b, double* x, int &iters)
{
	return epetra_solve(List, TPL, Amat, b, x, iters);
}/*end solve*/

/**************************************************************/
/**************************************************************/
/*******  muelu_data_pack list namespace functions ************/
/**************************************************************/
/**************************************************************/

void muelu_data_pack_list::clearAll()
{
	for(auto i : list)
		delete i;
	list.clear();
}

/* add - Adds an muelu_data_pack to the list.
   Parameters:
   D       - The muelu_data_pack. [I]
   Returns: problem id number of D
*/
int muelu_data_pack_list::add(muelu_data_pack *D)
{
    D->id = nextID;
    nextID++;
	list.push_back(D);
	return D->id;
}   /*end add*/

/* find - Finds problem by id
   Parameters:
   id      - ID number [I]
   Returns: pointer to muelu_data_pack matching 'id', if found, NULL if not
   found.
*/
muelu_data_pack* muelu_data_pack_list::find(int id)
{
	if(isInList(id))
    {
        for(auto problem : list)
        {
            if(problem->id == id)
                return problem;
        }
    }
    return NULL;
}/*end find*/

/* remove - Removes problem by id
   Parameters:
   id      - ID number [I]
   Returns: IS_TRUE if remove was succesful, IS_FALSE otherwise
*/
int muelu_data_pack_list::remove(int id)
{
    int index = -1;
    for(int i = 0; i < int(list.size()); i++)
    {
        if(list[i]->id == id)
        {
            index = i;
            break;
        }
    }
    if(index == -1)
    {
        mexErrMsgTxt("Error: Tried to clean up a problem that doesn't exist.");
        return IS_FALSE;
    }
    //delete the muelu_data_pack at the location pointed to by list[index]
	delete list[index];
    //and then remove the pointer from the list
    mexPrintf("Removing problem with ID #%d and index %d\n", id, index);
	list.erase(list.begin() + index);
	return IS_TRUE;
}/*end remove*/

/* size - Number of stored problems */
int muelu_data_pack_list::size()
{
	return list.size();
}

/* Returns the status of all members of the list
   Returns IS_TRUE
*/
int muelu_data_pack_list::status_all()
{
    int i = 0;
    for(auto problem : list)
    {
        mexPrintf("Problem at index %d has ID %d\n", i, problem->id);
        i++;
    }
	mexPrintf("\n");
    //This prints all the existing problems in ascending order by ID
    for(i = 0; i < nextID; i++)
    {
        for(auto problem : list)
        {
            if(problem->id == i)
            {
                problem->status();
                break;
            }
        }
    }
	return IS_TRUE;
}/*end status_all */

bool muelu_data_pack_list::isInList(int id)
{
    bool rv = false;
    for(auto problem : list)
    {
        if(problem->id == id)
        {
            rv = true;
            break;
        }
    }
    return rv;
}

/* sanity_check - sanity checks the first couple of arguements and returns the
   program mode.
   Parameters:
   nrhs    - Number of program inputs [I]
   prhs    - The problem inputs [I]
   Return value: Which mode to run the program in.
*/

MODE_TYPE sanity_check(int nrhs, const mxArray *prhs[])
{
	MODE_TYPE rv = MODE_ERROR;
	double *modes;
	/* Check for mode */
	if(nrhs == 0)
		mexErrMsgTxt("Error: Invalid Inputs\n");

	/* Pull mode data from 1st Input */
	modes = mxGetPr(prhs[0]);
	switch ((MODE_TYPE) modes[0])
	{
		case MODE_SETUP:
			if(nrhs > 1 && mxIsSparse(prhs[1]))
			{
				if(nrhs > 3 && mxIsSparse(prhs[2]) && mxIsSparse(prhs[3]))
				//Uncomment next line when implementing Maxwell mode
				//With the comment, rv keeps value MODE_ERROR.
					//rv = MODE_SETUP_MAXWELL;
					rv = MODE_ERROR;
		  		else
					rv = MODE_SETUP;
			}
			else
			{
				mexErrMsgTxt("Error: Invalid input for setup\n");
			}
			break;
		case MODE_SOLVE:
			if(nrhs > 2 && mxIsNumeric(prhs[1]) && !mxIsSparse(prhs[2]) && mxIsNumeric(prhs[2]))
				rv = MODE_SOLVE;
			else
				mexErrMsgTxt("Error: Invalid input for solve\n");
			break;
		case MODE_SOLVE_NEWMATRIX:
			if(nrhs > 3 && mxIsNumeric(prhs[1]) && mxIsSparse(prhs[2]) && mxIsNumeric(prhs[3]))
				rv = MODE_SOLVE_NEWMATRIX;
			else
				mexErrMsgTxt("Error: Invalid input for solve\n");
			break;
		case MODE_CLEANUP:
			if(nrhs == 1 || nrhs == 2)
				rv = MODE_CLEANUP;
			else
				mexErrMsgTxt("Error: Extraneous args for cleanup\n");
			break;
		case MODE_STATUS:
			if(nrhs == 1 || nrhs == 2)
				rv = MODE_STATUS;
			else
				mexErrMsgTxt("Error: Extraneous args for status\n");
			break;
		case MODE_AGGREGATE:
			if(nrhs > 1 && mxIsSparse(prhs[1]))
				//Uncomment the next line and remove one after when implementing aggregate mode
			    //rv = MODE_AGGREGATE;
				rv = MODE_ERROR;
			else
				mexErrMsgTxt("Error: Invalid input for aggregate\n");
			break;
		default:
			  printf("Mode number = %d\n", (int) modes[0]);
			  mexErrMsgTxt("Error: Invalid input mode\n");
	};
	return rv;
}	
/*end sanity_check*/

void csc_print(int n, int* rowind, int* colptr, double* vals)
{
    int i, j;
    for(i = 0; i < n; i++)
        for(j = colptr[i]; j < colptr[i + 1]; j++)
            mexPrintf("%d %d %20.16e\n", rowind[j], i, vals[j]);
}

void parse_list_item(Teuchos::ParameterList& List, char *option_name, const mxArray *prhs)
{
	mxClassID cid;
	int i, M, N, *opt_int;
	char *opt_char;
	double *opt_float;
	string opt_str;
	Teuchos::ParameterList sublist;
	mxArray *cell1, *cell2;

	/* Pull relevant info the the option value */
	cid = mxGetClassID(prhs);
	M = mxGetM(prhs);
	N = mxGetN(prhs);

	/* Add to the Teuchos list */
	switch(cid)
    {
	    case mxCHAR_CLASS:
		    // String
        	opt_char = mxArrayToString(prhs);
        	opt_str = opt_char;
        	List.set(option_name, opt_str);
        	mxFree(opt_char);
        	break;
	    case mxDOUBLE_CLASS:
	    case mxSINGLE_CLASS:
		    // Single or double
		    //NTS: Does not deal with complex args
		    opt_float = mxGetPr(prhs);
		    if(M == 1 && N == 1 && ISINT(opt_float[0]))
		    {
			    List.set(option_name, (int) opt_float[0]);
		    } /*end if*/
		    else if(M == 1 && N == 1)
		    {
			    List.set(option_name, opt_float[0]);
		    }/*end if*/
		    else if(M == 0 || N == 0)
		    {
			    List.set(option_name, (double*) NULL);
	      	}
	      	else
		    {
		        List.set(option_name, opt_float);
		    }/*end else*/
		    break;
	    case mxLOGICAL_CLASS:
        // Bool
	    if(M == 1 && N == 1)
		    List.set(option_name, mxIsLogicalScalarTrue(prhs));
        else
		    List.set(option_name, mxGetLogicals(prhs));
        	//NTS: The else probably doesn't work.
        break;
	    case mxINT8_CLASS:
	    case mxUINT8_CLASS:
	    case mxINT16_CLASS:
	    case mxUINT16_CLASS:
	    case mxINT32_CLASS:
	    case mxUINT32_CLASS:
	        // Integer
	        opt_int = (int*) mxGetData(prhs);
	        if(M == 1 && N == 1)
			    List.set(option_name, opt_int[0]);
	        else
			    List.set(option_name, opt_int);
	        break;
	        // NTS: 64-bit ints will break on a 32-bit machine.  We
	        // should probably detect machine type, or somthing, but that would
	        // involve a non-trivial quantity of autoconf kung fu.
	    case mxCELL_CLASS:
		    // Interpret a cell list as a nested teuchos list.
		    // NTS: Assuming that it's a 1D row ordered array
		    for(i = 0; i < N; i += 2)
		    {
			    cell1 = mxGetCell(prhs, i);
			    cell2 = mxGetCell(prhs, i + 1);
			    if(!mxIsChar(cell1))
				    mexErrMsgTxt("Error: Input options are not in ['parameter',value] format!\n");
          		opt_char = mxArrayToString(cell1);
          		parse_list_item(sublist, opt_char, cell2);
          		List.set(option_name, sublist);
          		mxFree(opt_char);
        	}
        	break;
	    case mxINT64_CLASS:
	    case mxUINT64_CLASS:
	    case mxFUNCTION_CLASS:
	    case mxUNKNOWN_CLASS:
	    case mxSTRUCT_CLASS:
	    default:
	        mexPrintf("Error parsing input option: %s [type=%d]\n", option_name, cid);
	        mexErrMsgTxt("Error: An input option is invalid!\n");
	};
}

/**************************************************************/
/**************************************************************/
/**************************************************************/
/* build_teuchos_list - takes the inputs (barring the solver mode and
  matrix/rhs) and turns them into a Teuchos list for use by MLAPI.
   Parameters:
   nrhs    - Number of program inputs [I]
   prhs    - The problem inputs [I]
   Return value: Teuchos list containing all parameters passed in by the user.
*/
Teuchos::ParameterList* build_teuchos_list(int nrhs,const mxArray *prhs[])
{
	Teuchos::ParameterList* TPL = new Teuchos::ParameterList;
	char* option_name;
	for(int i = 0; i < nrhs; i += 2)
	{
		if(i == nrhs - 1 || !mxIsChar(prhs[i]))
			mexErrMsgTxt("Error: Input options are not in ['parameter',value] format!\n");
		/* What option are we setting? */
    	option_name = mxArrayToString(prhs[i]);
    	/* Parse */
    	parse_list_item(*TPL, option_name, prhs[i + 1]);
    	/* Free memory */
    	mxFree(option_name);
	}
	return TPL;
}
/*end build_teuchos_list*/

void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
	double* id;
	int rv;
	int* rowind;
	int* colptr;
	//Arrays representing vectors
	double* b;
	double* x;
	double* vals;
	int nr;
	int nc;
	int iters;
	string intf;
	Teuchos::ParameterList* List;
	MODE_TYPE mode;
	bool rewrap_ints = false;
	muelu_data_pack* D = nullptr;
	//ml_maxwell_data_pack * Dhat=0;
	Epetra_CrsMatrix* A;
	/* Sanity Check Input */
	mode = sanity_check(nrhs, prhs);
	/* Set flag if mwIndex and int are not the same size */
	/* NTS: This can be an issue on 64 bit architectures */
	if(sizeof(int) != sizeof(mwIndex))
		rewrap_ints = true;
	switch(mode)
	{
		//All possible modes are accounted for.
	    case MODE_SETUP:
	    	mexPrintf("MueMex in setup mode.\n");
			nr = mxGetM(prhs[1]);
			nc = mxGetN(prhs[1]);
			if(nrhs > 2)
				List = build_teuchos_list(nrhs - 2, &(prhs[2]));
			else
				List = new Teuchos::ParameterList;
			intf = List->get(MUEMEX_INTERFACE, "epetra");
			if(intf == "mueluapi")
				D = new mueluapi_data_pack();
			else
				D = new muelu_epetra_data_pack();
			D->List = List;
			/* Pull matrix in CSC format */
			vals = mxGetPr(prhs[1]);
			if(rewrap_ints)
			{
				colptr = mwIndex_to_int(nc + 1, mxGetJc(prhs[1]));
				rowind = mwIndex_to_int(colptr[nc], mxGetIr(prhs[1]));
			}
			else
			{
				rowind = (int*) mxGetIr(prhs[1]);
				colptr = (int*) mxGetJc(prhs[1]);
			}
			mexPrintf("Finished setup phase.\n");
			D->setup(nr, rowind, colptr, vals);
			rv = muelu_data_pack_list::add(D);
			plhs[0] = mxCreateNumericMatrix(1, 1, mxINT32_CLASS, mxREAL);
			id = (double*) mxGetData(plhs[0]);
			*id = double(rv);
			if(nlhs > 1)
				plhs[1] = mxCreateDoubleScalar(D->operator_complexity);
			if(rewrap_ints)
			{
				delete[] rowind;
				delete[] colptr;
			}
			mexLock();
	        break;
	    case MODE_SOLVE:
			mexPrintf("MueMex in solving mode.\n");
			/* Are there problems set up? */
			if(muelu_data_pack_list::size() == 0)
				mexErrMsgTxt("Error: No problems set up, cannot solve.\n");
			/* Get the Problem Handle */
			id = (double*) mxGetData(prhs[1]);
			D = muelu_data_pack_list::find(int(*id));
			if(!D)
            {
				mexErrMsgTxt("Error: Problem handle not allocated.\n");
                break;
            }
			/* Pull Problem Size */
			nr = mxGetM(prhs[2]);
			A = D->GetMatrix();
			/* Pull RHS */
			b = mxGetPr(prhs[2]);
			/* Teuchos List*/
			if(nrhs > 4)
				List = build_teuchos_list(nrhs - 3, &(prhs[3]));
			else
				List = new Teuchos::ParameterList;
			/* Allocate Solution Space */
			plhs[0] = mxCreateDoubleMatrix(nr, 1, mxREAL);
			x = mxGetPr(plhs[0]);
			/* Sanity Check Matrix / RHS */
			if(nr != A->NumMyRows() || A->NumMyRows() != A->NumMyCols())
				mexErrMsgTxt("Error: Size Mismatch in Input\n");
			/* Run Solver */
			D->solve(List, A, b, x, iters);
			/* Output Iteration Count */
			if(nlhs > 1)
			{
				plhs[1] = mxCreateDoubleScalar((double) iters);
			}
			/* Cleanup */
			A = 0; // we don't own this
			delete List;
	        break;
		case MODE_SOLVE_NEWMATRIX:
			mexPrintf("MueMex in new matrix solving mode.\n");
			/* Are there problems set up? */
			if(muelu_data_pack_list::size() == 0)
				mexErrMsgTxt("Error: No problems set up, cannot solve.\n");
			/* Get the Problem Handle */
			id = (double*) mxGetData(prhs[1]);
			D = muelu_data_pack_list::find(int(*id));
			if(!D)
				mexErrMsgTxt("Error: Problem handle not allocated.\n");
			/* Pull Problem Size */
			nr = mxGetM(prhs[2]);
			nc = mxGetN(prhs[2]);
			if(nr != D->NumMyRows() && nc != D->NumMyCols())
				mexErrMsgTxt("Error: Problem size mismatch.\n");
			/* Pull RHS */
			b = mxGetPr(prhs[3]);
			/* Teuchos List*/
			if(nrhs > 4)
				List = build_teuchos_list(nrhs - 4, &(prhs[4]));
			else
				List = new Teuchos::ParameterList;
			/* Allocate Solution Space */
			plhs[0] = mxCreateDoubleMatrix(nr, 1, mxREAL);
			x = mxGetPr(plhs[0]);
			/* Sanity Check Matrix / RHS */
			if(nr != nc || nr != (int) mxGetM(prhs[2]))
				mexErrMsgTxt("Error: Size Mismatch in Input\n");
			// Grab the input matrix
			A = epetra_setup_from_prhs(prhs[2], rewrap_ints);
			/* Run Solver */
			D->solve(List, A, b, x, iters);
			/* Output Iteration Count */
			if(nlhs > 1)
			{
				plhs[1] = mxCreateDoubleScalar((double) iters);
			}
			/* Cleanup */
			delete List;
			delete A;
			break;
	    case MODE_CLEANUP:
			mexPrintf("MueMex in cleanup mode.\n");
			if(muelu_data_pack_list::size() > 0 && nrhs == 1)
			{
				/* Cleanup all problems */
				for(int i = 0; i < muelu_data_pack_list::size(); i++)
					mexUnlock();
				muelu_data_pack_list::clearAll();
				rv = 1;
			}
			else if(muelu_data_pack_list::size() > 0 && nrhs == 2)
			{
				/* Cleanup one problem */
				int probID = (int) *((double*) mxGetData(prhs[1]));
                mexPrintf("Cleaning up problem #%d\n", probID);
				rv = muelu_data_pack_list::remove(probID);
				if(rv)
					mexUnlock();
			}   /*end elseif*/
			else
            {
				rv = 0;
            }
            /* Set return value */
			plhs[0] = mxCreateNumericMatrix(1, 1, mxINT32_CLASS, mxREAL);
			id = (double*) mxGetData(plhs[0]);
			*id = double(rv);
	        break;
	    case MODE_STATUS:
			mexPrintf("MueMex in status checking mode.\n");
			if(muelu_data_pack_list::size() > 0 && nrhs == 1)
			{
			  /* Status check on all problems */
				rv = muelu_data_pack_list::status_all();
			}/*end if*/
			else if(muelu_data_pack_list::size() > 0 && nrhs == 2)
			{
				/* Status check one problem */
                int probID = *((double*) mxGetData(prhs[1]));
				D = muelu_data_pack_list::find(probID);
				if(!D)
					mexErrMsgTxt("Error: Problem handle not allocated.\n");
				rv = D->status();
			}/*end elseif*/
			else
				rv = 0;
			/* Set return value */
			plhs[0] = mxCreateNumericMatrix(1, 1, mxINT32_CLASS, mxREAL);
			id = (double*) mxGetData(plhs[0]);
			*id = double(rv);
	        break;
	    case MODE_ERROR:
	        mexPrintf("MueMex error.");
			break;
		//TODO: Will implement these modes later.
		case MODE_AGGREGATE:
		case MODE_SETUP_MAXWELL:
	    default:
	        mexPrintf("Mode not supported yet.");
	}
}


#else
#error "Do not have MATLAB"
#endif

