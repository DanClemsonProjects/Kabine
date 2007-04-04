// @HEADER
// ***********************************************************************
// 
//    Thyra: Interfaces and Support for Abstract Numerical Algorithms
//                 Copyright (2004) Sandia Corporation
// 
// Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
// license for use of this work by or on behalf of the U.S. Government.
// 
// This library is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; either version 2.1 of the
// License, or (at your option) any later version.
//  
// This library is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//  
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
// USA
// Questions? Contact Michael A. Heroux (maherou@sandia.gov) 
// 
// ***********************************************************************
// @HEADER

#ifndef THYRA_DEFAULT_SPMD_MULTI_VECTOR_BASE_HPP
#define THYRA_DEFAULT_SPMD_MULTI_VECTOR_BASE_HPP

// Define to make some verbose output
//#define THYRA_DEFAULT_SPMD_MULTI_VECTOR_VERBOSE_TO_ERROR_OUT

#include "Thyra_DefaultSpmdMultiVectorDecl.hpp"
#include "Thyra_SpmdMultiVectorBase.hpp"
#include "Thyra_DefaultSpmdVector.hpp"

namespace Thyra {

//
// Simple utility class to copy back multi-vector entries
//

template<class Scalar>
class CopyBackSpmdMultiVectorEntries {
public:
  CopyBackSpmdMultiVectorEntries(
    const int numCols, const int cols[]
    ,const Scalar *localValuesView, const Index localSubDim
    ,Scalar *localValues,const Index leadingDim
    )
    :cols_(cols,cols+numCols)
    ,localValuesView_(localValuesView),localSubDim_(localSubDim)
    ,localValues_(localValues),leadingDim_(leadingDim)
    {}
  ~CopyBackSpmdMultiVectorEntries()
    {
      //std::cout << "\nEntering CopyBackSpmdMultiVectorEntries::~CopyBackSpmdMultiVectorEntries()...\n";
      // Copy from contiguous storage
      const int    numCols = cols_.size();
      const Scalar *lvv    = &*localValuesView_;
      Scalar       *lv     = &*localValues_;
      for( int k = 0; k < numCols; ++k ) {
        const int col_k = cols_[k];
        const Scalar *lvv_k  = lvv + localSubDim_*k;
        Scalar       *lv_k   = lv + leadingDim_*col_k;
        //std::cout << "\nlvv_k = ["; for( int j = 0; j < localSubDim_; ++j ) std::cout << lvv_k[j] << ","; std::cout << "]\n";
        std::copy( lvv_k, lvv_k + localSubDim_, lv_k );
        //std::cout << "\nlv_k = ["; for( int j = 0; j < localSubDim_; ++j ) std::cout << lv_k[j] << ","; std::cout << "]\n";
      }
      //std::cout << "\nLeaving CopyBackSpmdMultiVectorEntries::~CopyBackSpmdMultiVectorEntries()\n";
    }
private:
  std::vector<int>   cols_;
  const Scalar       *localValuesView_;
  Index              localSubDim_;
  Scalar             *localValues_;
  Index              leadingDim_;
  // Not defined and not to be called
  CopyBackSpmdMultiVectorEntries();
  CopyBackSpmdMultiVectorEntries(const CopyBackSpmdMultiVectorEntries&);
  CopyBackSpmdMultiVectorEntries& operator=(const CopyBackSpmdMultiVectorEntries&);
};

//
// DefaultSpmdMultiVector
//

// Constructors/initializers/accessors

template<class Scalar>
DefaultSpmdMultiVector<Scalar>::DefaultSpmdMultiVector()
  :leadingDim_(0)
{}

template<class Scalar>
DefaultSpmdMultiVector<Scalar>::DefaultSpmdMultiVector(
  const Teuchos::RefCountPtr<const SpmdVectorSpaceBase<Scalar> >         &spmdRangeSpace
  ,const Teuchos::RefCountPtr<const ScalarProdVectorSpaceBase<Scalar> >  &domainSpace
  )
{
  initialize(spmdRangeSpace,domainSpace);
}

template<class Scalar>
DefaultSpmdMultiVector<Scalar>::DefaultSpmdMultiVector(
  const Teuchos::RefCountPtr<const SpmdVectorSpaceBase<Scalar> >         &spmdRangeSpace
  ,const Teuchos::RefCountPtr<const ScalarProdVectorSpaceBase<Scalar> >  &domainSpace
  ,const Teuchos::RefCountPtr<Scalar>                                    &localValues
  ,const Index                                                           leadingDim
  )
{
  initialize(spmdRangeSpace,domainSpace,localValues,leadingDim);
}

template<class Scalar>
void DefaultSpmdMultiVector<Scalar>::initialize(
  const Teuchos::RefCountPtr<const SpmdVectorSpaceBase<Scalar> >         &spmdRangeSpace
  ,const Teuchos::RefCountPtr<const ScalarProdVectorSpaceBase<Scalar> >  &domainSpace
  )
{
  const Index localSubDim = spmdRangeSpace->localSubDim();
  initialize(
    spmdRangeSpace
    ,domainSpace
    ,Teuchos::rcp(
      localSubDim ? new Scalar[localSubDim*domainSpace->dim()] : 0
      ,Teuchos::DeallocArrayDelete<Scalar>()
      ,true
      )
    ,localSubDim
    );
}

template<class Scalar>
void DefaultSpmdMultiVector<Scalar>::initialize(
  const Teuchos::RefCountPtr<const SpmdVectorSpaceBase<Scalar> >         &spmdRangeSpace
  ,const Teuchos::RefCountPtr<const ScalarProdVectorSpaceBase<Scalar> >  &domainSpace
  ,const Teuchos::RefCountPtr<Scalar>                                    &localValues
  ,const Index                                                           leadingDim
  )
{
#ifdef TEUCHOS_DEBUG
  TEST_FOR_EXCEPT(spmdRangeSpace.get()==NULL);
  TEST_FOR_EXCEPT(domainSpace.get()==NULL);
  TEST_FOR_EXCEPT(localValues.get()==NULL);
  TEST_FOR_EXCEPT(leadingDim < spmdRangeSpace->localSubDim());
#endif
  spmdRangeSpace_ = spmdRangeSpace;
  domainSpace_   = domainSpace;
  localValues_   = localValues;
  leadingDim_    = leadingDim;
  this->updateSpmdSpace();
}

template<class Scalar>
void DefaultSpmdMultiVector<Scalar>::uninitialize(
  Teuchos::RefCountPtr<const SpmdVectorSpaceBase<Scalar> >         *spmdRangeSpace
  ,Teuchos::RefCountPtr<const ScalarProdVectorSpaceBase<Scalar> >  *domainSpace
  ,Teuchos::RefCountPtr<Scalar>                                    *localValues
  ,Index                                                           *leadingDim
  )
{
  if(spmdRangeSpace) *spmdRangeSpace = spmdRangeSpace_;
  if(domainSpace)   *domainSpace   = domainSpace_;
  if(localValues)   *localValues   = localValues_;
  if(leadingDim)    *leadingDim    = leadingDim_;

  spmdRangeSpace_  = Teuchos::null;
  domainSpace_    = Teuchos::null;
  localValues_    = Teuchos::null;
  leadingDim_     = 0;

  this->updateSpmdSpace();
}

// Overridden from EuclideanLinearOpBase

template<class Scalar>
Teuchos::RefCountPtr< const ScalarProdVectorSpaceBase<Scalar> >
DefaultSpmdMultiVector<Scalar>::domainScalarProdVecSpc() const
{
#ifdef THYRA_DEFAULT_SPMD_MULTI_VECTOR_VERBOSE_TO_ERROR_OUT
  std::cerr << "\nSpmdMultiVectorStd<Scalar>::domainScalarProdVecSpc() const called!\n";
#endif
  return domainSpace_;
}

// Overridden from MultiVectorBase

template<class Scalar>
Teuchos::RefCountPtr<VectorBase<Scalar> >
DefaultSpmdMultiVector<Scalar>::col(Index j)
{
#ifdef THYRA_DEFAULT_SPMD_MULTI_VECTOR_VERBOSE_TO_ERROR_OUT
  std::cerr << "\nSpmdMultiVectorStd<Scalar>::col() called!\n";
#endif
#ifdef TEUCHOS_DEBUG
  TEST_FOR_EXCEPT( !( 0 <= j && j < this->domain()->dim() ) );
#endif
  return Teuchos::rcp(
    new DefaultSpmdVector<Scalar>(
      spmdRangeSpace_
      ,Teuchos::rcp( (&*localValues_) + j*leadingDim_, false )
      ,1
      )
    );
  //return Teuchos::rcp(new DefaultVectorMultiVector<Scalar>(subView(Range1D(j,j))));
}

template<class Scalar>
Teuchos::RefCountPtr<MultiVectorBase<Scalar> >
DefaultSpmdMultiVector<Scalar>::subView( const Range1D& col_rng_in )
{
#ifdef THYRA_DEFAULT_SPMD_MULTI_VECTOR_VERBOSE_TO_ERROR_OUT
  std::cerr << "\nSpmdMultiVectorStd<Scalar>::subView() called!\n";
#endif
  const Range1D colRng = this->validateColRange(col_rng_in);
  return Teuchos::rcp(
    new DefaultSpmdMultiVector<Scalar>(
      spmdRangeSpace_
      ,Teuchos::rcp_dynamic_cast<const ScalarProdVectorSpaceBase<Scalar> >(
        spmdRangeSpace_->smallVecSpcFcty()->createVecSpc(colRng.size())
        ,true
        )
      ,Teuchos::rcp( (&*localValues_) + colRng.lbound()*leadingDim_, false )
      ,leadingDim_
      )
    );
}

template<class Scalar>
Teuchos::RefCountPtr<const MultiVectorBase<Scalar> >
DefaultSpmdMultiVector<Scalar>::subView( const int numCols, const int cols[] ) const
{
  Teuchos::RefCountPtr<Scalar> localValuesView = createContiguousCopy(numCols,cols);
  return 
    Teuchos::rcp(
      new DefaultSpmdMultiVector<Scalar>(
        spmdRangeSpace_
        ,Teuchos::rcp_dynamic_cast<const ScalarProdVectorSpaceBase<Scalar> >(
          spmdRangeSpace_->smallVecSpcFcty()->createVecSpc(numCols)
          ,true)
        ,localValuesView
        ,spmdRangeSpace_->localSubDim()
        )
      );
}

template<class Scalar>
Teuchos::RefCountPtr<MultiVectorBase<Scalar> >
DefaultSpmdMultiVector<Scalar>::subView( const int numCols, const int cols[] )
{
  Teuchos::RefCountPtr<Scalar> localValuesView = createContiguousCopy(numCols,cols);
  const Index localSubDim = spmdRangeSpace_->localSubDim();
  Teuchos::RefCountPtr<MultiVectorBase<Scalar> >
    view = Teuchos::rcp(
      new DefaultSpmdMultiVector<Scalar>(
        spmdRangeSpace_
        ,Teuchos::rcp_dynamic_cast<const ScalarProdVectorSpaceBase<Scalar> >(
          spmdRangeSpace_->smallVecSpcFcty()->createVecSpc(numCols)
          ,true)
        ,localValuesView
        ,localSubDim
        )
      );
  Teuchos::RefCountPtr<CopyBackSpmdMultiVectorEntries<Scalar> >
    copyBackView
    = Teuchos::rcp(
      new CopyBackSpmdMultiVectorEntries<Scalar>(
        numCols,cols,&*localValuesView,localSubDim,&*localValues_,leadingDim_
        )
      );
  Teuchos::set_extra_data(copyBackView,"copyBackView",&view,Teuchos::PRE_DESTROY);
  return view;
}

// Overridden from SpmdMultiVectorBase

template<class Scalar>
Teuchos::RefCountPtr<const SpmdVectorSpaceBase<Scalar> >
DefaultSpmdMultiVector<Scalar>::spmdSpace() const
{
#ifdef THYRA_DEFAULT_SPMD_MULTI_VECTOR_VERBOSE_TO_ERROR_OUT
  std::cerr << "\nSpmdMultiVectorStd<Scalar>::spmdSpace() const called!\n";
#endif
  return spmdRangeSpace_;
}

template<class Scalar>
void DefaultSpmdMultiVector<Scalar>::getLocalData(
  Scalar **localValues, Index *leadingDim
  )
{
#ifdef THYRA_DEFAULT_SPMD_MULTI_VECTOR_VERBOSE_TO_ERROR_OUT
  std::cerr << "\nSpmdMultiVectorStd<Scalar>::getLocalData() called!\n";
#endif
#ifdef TEUCHOS_DEBUG
  TEST_FOR_EXCEPT( localValues==NULL );
  TEST_FOR_EXCEPT( leadingDim==NULL );
#endif
  *localValues = &*localValues_;
  *leadingDim  = leadingDim_;
}

template<class Scalar>
void DefaultSpmdMultiVector<Scalar>::commitLocalData( Scalar *localValues )
{
#ifdef THYRA_DEFAULT_SPMD_MULTI_VECTOR_VERBOSE_TO_ERROR_OUT
  std::cerr << "\nSpmdMultiVectorStd<Scalar>::commitLocalData() called!\n";
#endif
#ifdef TEUCHOS_DEBUG
  TEST_FOR_EXCEPT( localValues!=&*localValues_ );
#endif
  // Nothing to commit!
}

template<class Scalar>
void DefaultSpmdMultiVector<Scalar>::getLocalData(
  const Scalar **localValues, Index *leadingDim
  ) const
{
#ifdef THYRA_DEFAULT_SPMD_MULTI_VECTOR_VERBOSE_TO_ERROR_OUT
  std::cerr << "\nSpmdMultiVectorStd<Scalar>::getLocalData() called!\n";
#endif
#ifdef TEUCHOS_DEBUG
  TEST_FOR_EXCEPT( localValues==NULL );
  TEST_FOR_EXCEPT( leadingDim==NULL );
#endif
  *localValues = &*localValues_;
  *leadingDim  = leadingDim_;
}

template<class Scalar>
void DefaultSpmdMultiVector<Scalar>::freeLocalData( const Scalar *localValues ) const
{
#ifdef TEUCHOS_DEBUG
  TEST_FOR_EXCEPT( localValues!=&*localValues_ );
#endif
#ifdef THYRA_DEFAULT_SPMD_MULTI_VECTOR_VERBOSE_TO_ERROR_OUT
  std::cerr << "\nSpmdMultiVectorStd<Scalar>::commitLocalData() called!\n";
#endif
  // Nothing to free
}

// private

template<class Scalar>
Teuchos::RefCountPtr<Scalar>
DefaultSpmdMultiVector<Scalar>::createContiguousCopy(
  const int numCols, const int cols[]
  ) const
{
#ifdef TEUCHOS_DEBUG
  const VectorSpaceBase<Scalar>  &domain   = *domainSpace_;
  const Index                    dimDomain = domain.dim();
  const char msg_err[] = "MultiVectorDefaultBase<Scalar>::subView(numCols,cols[]): Error!";
  TEST_FOR_EXCEPTION( numCols < 1 || dimDomain < numCols, std::invalid_argument, msg_err );
#endif
  const Index localSubDim = spmdRangeSpace_->localSubDim();
  Teuchos::RefCountPtr<Scalar>
    localValuesView
    = Teuchos::rcp(
      new Scalar[numCols*localSubDim], Teuchos::DeallocArrayDelete<Scalar>(), true
      );
  // Copy to contiguous storage
  const Scalar *lv   = &*localValues_;
  Scalar       *lvv  = &*localValuesView;
  for( int k = 0; k < numCols; ++k ) {
    const int col_k = cols[k];
#ifdef TEUCHOS_DEBUG
    TEST_FOR_EXCEPTION(
      !( 0<= col_k && col_k < dimDomain ), std::invalid_argument
      ,msg_err << " col["<<k<<"] = " << col_k
      << " is not in the range [0,"<<(dimDomain-1)<<"]!"
      );
#endif
    const Scalar *lv_k   = lv + leadingDim_*col_k;
    Scalar       *lvv_k  = lvv + localSubDim*k;
    std::copy( lv_k, lv_k + localSubDim, lvv_k );
  }
  return localValuesView;
}

} // end namespace Thyra

#endif // THYRA_DEFAULT_SPMD_MULTI_VECTOR_BASE_HPP
