// matrix/kaldi-vector.cc

// Copyright 2009-2011  Microsoft Corporation;  Lukas Burget;
//                      Saarland University;   Go Vivace Inc.;  Ariya Rastrow;
//                      Petr Schwarz;  Yanmin Qian;  Jan Silovsky;
//                      Haihua Xu; Wei Shi
//                2015  Guoguo Chen
//                2017  Daniel Galvez


// See ../../COPYING for clarification regarding multiple authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED
// WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
// MERCHANTABLITY OR NON-INFRINGEMENT.
// See the Apache 2 License for the specific language governing permissions and
// limitations under the License.

#include <algorithm>
#include <string>
#include "matrix/cblas-wrappers.h"
#include "matrix/kaldi-vector.h"
#include "matrix/kaldi-matrix.h"
#include "matrix/sp-matrix.h"
#include "matrix/sparse-matrix.h"

namespace kaldi {


template<typename Real> inline const Real* Get64Ones() {
  // The C++ standard doesn't seem to provide a compact way to do this.
  static const Real ones[64] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
  return ones;
}


template<typename Real>
Real VecVec(const VectorBase<Real> &a,
            const VectorBase<Real> &b) {
  MatrixIndexT adim = a.Dim();
  KALDI_ASSERT(adim == b.Dim());
  return cblas_Xdot(adim, a.Data(), a.Stride(), b.Data(), b.Stride());
}

template
float VecVec<>(const VectorBase<float> &a,
               const VectorBase<float> &b);
template
double VecVec<>(const VectorBase<double> &a,
                const VectorBase<double> &b);

template<typename Real, typename OtherReal>
Real VecVec(const VectorBase<Real> &a,
            const VectorBase<OtherReal> &b) {
  MatrixIndexT adim = a.Dim();
  KALDI_ASSERT(adim == b.Dim());
  const Real *a_data = a.Data();
  const OtherReal *b_data = b.Data();
  MatrixIndexT a_stride = a.Stride(), b_stride = b.Stride();
  Real sum = 0.0;
  for (MatrixIndexT i = 0; i < adim; i++)
    sum += a_data[i * a_stride] * b_data[i * b_stride];
  return sum;
}

// instantiate the template above.
template
float VecVec<>(const VectorBase<float> &a,
               const VectorBase<double> &b);
template
double VecVec<>(const VectorBase<double> &a,
                const VectorBase<float> &b);


template<>
template<>
void VectorBase<float>::AddVec(const float alpha,
                               const VectorBase<float> &v) {
  KALDI_ASSERT(dim_ == v.dim_);
  KALDI_ASSERT(&v != this);
  cblas_Xaxpy(dim_, alpha, v.Data(), v.stride_, data_, stride_);
}

template<>
template<>
void VectorBase<double>::AddVec(const double alpha,
                                const VectorBase<double> &v) {
  KALDI_ASSERT(dim_ == v.dim_);
  KALDI_ASSERT(&v != this);
  cblas_Xaxpy(dim_, alpha, v.Data(), v.stride_, data_, stride_);
}

template<typename Real>
void VectorBase<Real>::AddMatVec(const Real alpha,
                                  const MatrixBase<Real> &M,
                                  MatrixTransposeType trans,
                                  const VectorBase<Real> &v,
                                  const Real beta) {
  KALDI_ASSERT((trans == kNoTrans && M.NumCols() == v.dim_ && M.NumRows() == dim_)
               || (trans == kTrans && M.NumRows() == v.dim_ && M.NumCols() == dim_));
  KALDI_ASSERT(&v != this);
  cblas_Xgemv(trans, M.NumRows(), M.NumCols(), alpha, M.Data(), M.Stride(),
              v.Data(), v.stride_, beta, data_, stride_);
}

template<typename Real>
void VectorBase<Real>::AddMatSvec(const Real alpha,
                                  const MatrixBase<Real> &M,
                                  MatrixTransposeType trans,
                                  const VectorBase<Real> &v,
                                  const Real beta) {
  KALDI_ASSERT((trans == kNoTrans && M.NumCols() == v.dim_ && M.NumRows() == dim_)
               || (trans == kTrans && M.NumRows() == v.dim_ && M.NumCols() == dim_));
  KALDI_ASSERT(&v != this);
  Xgemv_sparsevec(trans, M.NumRows(), M.NumCols(), alpha, M.Data(), M.Stride(),
                  v.Data(), v.stride_, beta, data_, stride_);
  return;
}

template<typename Real>
void VectorBase<Real>::AddSpVec(const Real alpha,
                                const SpMatrix<Real> &M,
                                const VectorBase<Real> &v,
                                const Real beta) {
  KALDI_ASSERT(M.NumRows() == v.dim_ && dim_ == v.dim_);
  KALDI_ASSERT(&v != this);
  cblas_Xspmv(alpha, M.NumRows(), M.Data(), v.Data(), v.stride_,
              beta, data_, stride_);
}


template<typename Real>
void VectorBase<Real>::MulTp(const TpMatrix<Real> &M,
                              const MatrixTransposeType trans) {
  KALDI_ASSERT(M.NumRows() == dim_);
  cblas_Xtpmv(trans, M.Data(), M.NumRows(), data_, stride_);
}

template<typename Real>
void VectorBase<Real>::Solve(const TpMatrix<Real> &M,
                        const MatrixTransposeType trans) {
  KALDI_ASSERT(M.NumRows() == dim_);
  cblas_Xtpsv(trans, M.Data(), M.NumRows(), data_, stride_);
}


template<typename Real>
inline void Vector<Real>::Init(const MatrixIndexT dim) {
  this->stride_ = 1;
  KALDI_ASSERT(dim >= 0);
  if (dim == 0) {
    this->dim_ = 0;
    this->data_ = NULL;
    return;
  }
  MatrixIndexT size;
  void *data;
  void *free_data;

  size = dim * sizeof(Real);

  if ((data = KALDI_MEMALIGN(16, size, &free_data)) != NULL) {
    this->data_ = static_cast<Real*> (data);
    this->dim_ = dim;
  } else {
    throw std::bad_alloc();
  }
}


template<typename Real>
void Vector<Real>::Resize(const MatrixIndexT dim, MatrixResizeType resize_type) {
  // the next block uses recursion to handle what we have to do if
  // resize_type == kCopyData.
  if (resize_type == kCopyData) {
    if (this->data_ == NULL || dim == 0) resize_type = kSetZero;  // nothing to copy.
    else if (this->dim_ == dim) { return; } // nothing to do.
    else {
      // set tmp to a vector of the desired size.
      Vector<Real> tmp(dim, kUndefined);
      if (dim > this->dim_) {
        memcpy(tmp.data_, this->data_, sizeof(Real)*this->dim_);
        memset(tmp.data_+this->dim_, 0, sizeof(Real)*(dim-this->dim_));
      } else {
        memcpy(tmp.data_, this->data_, sizeof(Real)*dim);
      }
      tmp.Swap(this);
      // and now let tmp go out of scope, deleting what was in *this.
      return;
    }
  }
  // At this point, resize_type == kSetZero or kUndefined.

  if (this->data_ != NULL) {
    if (this->dim_ == dim) {
      if (resize_type == kSetZero) this->SetZero();
      return;
    } else {
      Destroy();
    }
  }
  Init(dim);
  if (resize_type == kSetZero) this->SetZero();
}


/// Copy data from another vector
template<typename Real>
void VectorBase<Real>::CopyFromVec(const VectorBase<Real> &v) {
  KALDI_ASSERT(Dim() == v.Dim());
  if (data_ != v.data_) {
    std::memcpy(this->data_, v.data_, dim_ * sizeof(Real));
  }
}

template<typename Real>
template<typename OtherReal>
void VectorBase<Real>::CopyFromPacked(const PackedMatrix<OtherReal>& M) {
  SubVector<OtherReal> v(M);
  this->CopyFromVec(v);
}
// instantiate the template.
template void VectorBase<float>::CopyFromPacked(const PackedMatrix<double> &other);
template void VectorBase<float>::CopyFromPacked(const PackedMatrix<float> &other);
template void VectorBase<double>::CopyFromPacked(const PackedMatrix<double> &other);
template void VectorBase<double>::CopyFromPacked(const PackedMatrix<float> &other);


template<typename Real>
template<typename OtherReal>
void VectorBase<Real>::CopyFromVec(const VectorBase<OtherReal> &other) {
  KALDI_ASSERT(dim_ == other.Dim());
  Real * __restrict__  ptr = data_;
  MatrixIndexT dim = dim_, stride = stride_, other_stride = other.Stride();
  const OtherReal * __restrict__ other_ptr = other.Data();
  for (MatrixIndexT i = 0; i < dim; i++)
    ptr[i * stride] = other_ptr[i * other_stride];
}

template void VectorBase<float>::CopyFromVec(const VectorBase<double> &other);
template void VectorBase<double>::CopyFromVec(const VectorBase<float> &other);

// Remove element from the vector. The vector is not reallocated
template<typename Real>
void Vector<Real>::RemoveElement(MatrixIndexT i) {
  KALDI_ASSERT(i <  this->dim_ && "Access out of vector");
  Real *data = data_;
  MatrixIndexT dim = dim_, stride = stride_;
  for (MatrixIndexT j = i + 1; j <  dim; j++)
    data[(j-1) * stride] =  data[j * stride];
  dim_--;
}


/// Deallocates memory and sets object to empty vector.
template<typename Real>
void Vector<Real>::Destroy() {
  /// we need to free the data block if it was defined
  if (this->data_ != NULL)
    KALDI_MEMALIGN_FREE(this->data_);
  this->data_ = NULL;
  this->dim_ = 0;
}

template<typename Real>
void VectorBase<Real>::SetZero() {
  std::memset(data_, 0, dim_ * sizeof(Real));
}

template<typename Real>
bool VectorBase<Real>::IsZero(Real cutoff) const {
  Real abs_max = 0.0;
  Real *data = data_;
  MatrixIndexT dim = dim_, stride = stride_;
  for (MatrixIndexT i = 0; i < dim; i++)
    abs_max = std::max(std::abs(data[i * stride]), abs_max);
  return (abs_max <= cutoff);
}

template<typename Real>
void VectorBase<Real>::SetRandn() {
  kaldi::RandomState rstate;
  MatrixIndexT last = (Dim() % 2 == 1) ? Dim() - 1 : Dim();
  Real *data = data_;
  MatrixIndexT stride = stride_;
  for (MatrixIndexT i = 0; i < last; i += 2) {
    kaldi::RandGauss2(data + i * stride, data + (i + 1)*stride, &rstate);
  }
  if (Dim() != last) data[last * stride] = static_cast<Real>(kaldi::RandGauss(&rstate));
}

template<typename Real>
void VectorBase<Real>::SetRandUniform() {
  kaldi::RandomState rstate;
  Real *data = data_;
  MatrixIndexT dim = dim_, stride = stride_;
  for (MatrixIndexT i = 0; i < dim; i++) {
    data[i * stride] = RandUniform(&rstate);
  }
}

template<typename Real>
MatrixIndexT VectorBase<Real>::RandCategorical() const {
  kaldi::RandomState rstate;
  Real sum = this->Sum();
  KALDI_ASSERT(this->Min() >= 0.0 && sum > 0.0);
  Real r = RandUniform(&rstate) * sum;
  Real *data = data_;
  MatrixIndexT dim = dim_, stride = stride_;
  Real running_sum = 0.0;
  for (MatrixIndexT i = 0; i < dim; i++) {
    running_sum += data[i * stride];
    if (r < running_sum) return i;
  }
  return dim_ - 1; // Should only happen if RandUniform()
                   // returns exactly 1, or due to roundoff.
}

template<typename Real>
void VectorBase<Real>::Set(Real f) {
  // Why not use memset here?
  Real *data = data_;
  MatrixIndexT dim = dim_, stride = stride_;
  for (MatrixIndexT i = 0; i < dim; i++) { data[i * stride] = f; }
}

template<typename Real>
void VectorBase<Real>::CopyRowsFromMat(const MatrixBase<Real> &mat) {
  KALDI_ASSERT(dim_ == mat.NumCols() * mat.NumRows());

  Real *inc_data = data_;
  MatrixIndexT stride = stride_;
  const MatrixIndexT cols = mat.NumCols(), rows = mat.NumRows();

  if (mat.Stride() == mat.NumCols()) {
    memcpy(inc_data, mat.Data(), cols*rows*sizeof(Real));
  } else {
    for (MatrixIndexT i = 0; i < rows; i++) {
      // copy the data to the propper position
      memcpy(inc_data, mat.RowData(i), cols * sizeof(Real));
      // set new copy position
      inc_data += cols * stride;
    }
  }
}

template<typename Real>
template<typename OtherReal>
void VectorBase<Real>::CopyRowsFromMat(const MatrixBase<OtherReal> &mat) {
  KALDI_ASSERT(dim_ == mat.NumCols() * mat.NumRows());
  Real *vec_data = data_;
  MatrixIndexT stride = stride_;
  const MatrixIndexT cols = mat.NumCols(),
      rows = mat.NumRows();

  for (MatrixIndexT i = 0; i < rows; i++) {
    const OtherReal *mat_row = mat.RowData(i);
    for (MatrixIndexT j = 0; j < cols; j++) {
      vec_data[j * stride] = static_cast<Real>(mat_row[j]);
    }
    vec_data += cols * stride;
  }
}

template
void VectorBase<float>::CopyRowsFromMat(const MatrixBase<double> &mat);
template
void VectorBase<double>::CopyRowsFromMat(const MatrixBase<float> &mat);


template<typename Real>
void VectorBase<Real>::CopyColsFromMat(const MatrixBase<Real> &mat) {
  KALDI_ASSERT(dim_ == mat.NumCols() * mat.NumRows());

  Real*       inc_data = data_;
  MatrixIndexT stride = stride_;
  const MatrixIndexT  cols     = mat.NumCols(), rows = mat.NumRows(), stride = mat.Stride();
  const Real *mat_inc_data = mat.Data();

  for (MatrixIndexT i = 0; i < cols; i++) {
    for (MatrixIndexT j = 0; j < rows; j++) {
      inc_data[j * stride] = mat_inc_data[j*stride];
    }
    mat_inc_data++;
    inc_data += rows * stride;
  }
}

template<typename Real>
void VectorBase<Real>::CopyRowFromMat(const MatrixBase<Real> &mat, MatrixIndexT row) {
  KALDI_ASSERT(row < mat.NumRows());
  KALDI_ASSERT(dim_ == mat.NumCols());
  const Real *mat_row = mat.RowData(row);
  memcpy(data_, mat_row, sizeof(Real)*dim_);
}

template<typename Real>
template<typename OtherReal>
void VectorBase<Real>::CopyRowFromMat(const MatrixBase<OtherReal> &mat, MatrixIndexT row) {
  KALDI_ASSERT(row < mat.NumRows());
  KALDI_ASSERT(dim_ == mat.NumCols());
  const OtherReal *mat_row = mat.RowData(row);
  Real *data = data_;
  MatrixIndexT dim = dim_, stride = stride_;
  for (MatrixIndexT i = 0; i < dim; i++)
    data[i * stride] = static_cast<Real>(mat_row[i]);
}

template
void VectorBase<float>::CopyRowFromMat(const MatrixBase<double> &mat, MatrixIndexT row);
template
void VectorBase<double>::CopyRowFromMat(const MatrixBase<float> &mat, MatrixIndexT row);

template<typename Real>
template<typename OtherReal>
void VectorBase<Real>::CopyRowFromSp(const SpMatrix<OtherReal> &sp, MatrixIndexT row) {
  KALDI_ASSERT(row < sp.NumRows());
  KALDI_ASSERT(dim_ == sp.NumCols());

  const OtherReal *sp_data = sp.Data();

  sp_data += (row*(row+1)) / 2; // takes us to beginning of this row.
  Real *data = data_;
  MatrixIndexT dim = dim_, stride = stride_;
  MatrixIndexT i;
  for (i = 0; i < row; i++) // copy consecutive elements.
    data[i * stride] = static_cast<Real>(*(sp_data++));
  for(; i < dim; ++i, sp_data += i)
    data[i * stride] = static_cast<Real>(*sp_data);
}

template
void VectorBase<float>::CopyRowFromSp(const SpMatrix<double> &mat, MatrixIndexT row);
template
void VectorBase<double>::CopyRowFromSp(const SpMatrix<float> &mat, MatrixIndexT row);
template
void VectorBase<float>::CopyRowFromSp(const SpMatrix<float> &mat, MatrixIndexT row);
template
void VectorBase<double>::CopyRowFromSp(const SpMatrix<double> &mat, MatrixIndexT row);


#ifdef HAVE_MKL
template<>
void VectorBase<float>::ApplyPow(float power) { vsPowx(dim_, data_, power, data_); }
template<>
void VectorBase<double>::ApplyPow(double power) { vdPowx(dim_, data_, power, data_); }
#else
// takes elements to a power.  Throws exception if could not (but only for power != 1 and power != 2).
template<typename Real>
void VectorBase<Real>::ApplyPow(Real power) {
  Real *data = data_;
  MatrixIndex dim = dim_, stride = stride_;
  if (power == 1.0) return;
  if (power == 2.0) {
    for (MatrixIndexT i = 0; i < dim; i++)
      data[i * stride] = data[i * stride] * data[i * stride];
  } else if (power == 0.5) {
    for (MatrixIndexT i = 0; i < dim; i++) {
      if (!(data[i * stride] >= 0.0))
        KALDI_ERR << "Cannot take square root of negative value "
                  << data[i * stride];
      data[i * stride] = std::sqrt(data[i * stride]);
    }
  } else {
    for (MatrixIndexT i = 0; i < dim_; i++) {
      data[i * stride] = pow(data[i * stride], power);
      if (data[i * stride] == HUGE_VAL) {  // HUGE_VAL is what errno returns on error.
        KALDI_ERR << "Could not raise element "  << i << " to power "
                  << power << ": returned value = " << data[i * stride];
      }
    }
  }
}
#endif

// takes absolute value of the elements to a power.
// Throws exception if could not (but only for power != 1 and power != 2).
template<typename Real>
void VectorBase<Real>::ApplyPowAbs(Real power, bool include_sign) {
  Real *data = data_;
  MatrixIndexT dim = dim_, stride = stride_;
  if (power == 1.0)
    for (MatrixIndexT i = 0; i < dim; i++)
      data[i * stride] = (include_sign && data[i * stride] < 0 ? -1 : 1) * std::abs(data[i * stride]);
  if (power == 2.0) {
    for (MatrixIndexT i = 0; i < dim; i++)
      data[i * stride] = (include_sign && data[i * stride] < 0 ? -1 : 1) * data[i * stride] * data[i * stride];
  } else if (power == 0.5) {
    for (MatrixIndexT i = 0; i < dim; i++) {
      data[i * stride] = (include_sign && data[i * stride] < 0 ? -1 : 1) * std::sqrt(std::abs(data[i * stride]));
    }
  } else if (power < 0.0) {
    for (MatrixIndexT i = 0; i < dim; i++) {
      data[i * stride] = (data[i * stride] == 0.0 ? 0.0 : pow(std::abs(data[i * stride]), power));
      data[i * stride] *= (include_sign && data[i * stride] < 0 ? -1 : 1);
      if (data[i * stride] == HUGE_VAL) {  // HUGE_VAL is what errno returns on error.
        KALDI_ERR << "Could not raise element "  << i << "to power "
                  << power << ": returned value = " << data[i * stride];
      }
    }
  } else {
    for (MatrixIndexT i = 0; i < dim; i++) {
      data[i * stride] = (include_sign && data[i * stride] < 0 ? -1 : 1) * pow(std::abs(data[i * stride]), power);
      if (data[i * stride] == HUGE_VAL) {  // HUGE_VAL is what errno returns on error.
        KALDI_ERR << "Could not raise element "  << i << "to power "
                  << power << ": returned value = " << data[i * stride];
      }
    }
  }
}

// Computes the p-th norm. Throws exception if could not.
template<typename Real>
Real VectorBase<Real>::Norm(Real p) const {
  KALDI_ASSERT(p >= 0.0);
  Real *data = data_;
  MatrixIndexT dim = dim_, stride = stride_;
  Real sum = 0.0;
  if (p == 0.0) {
    for (MatrixIndexT i = 0; i < dim; i++)
      if (data[i * stride] != 0.0) sum += 1.0;
    return sum;
  } else if (p == 1.0) {
    for (MatrixIndexT i = 0; i < dim; i++)
      sum += std::abs(data[i * stride]);
    return sum;
  } else if (p == 2.0) {
    for (MatrixIndexT i = 0; i < dim; i++)
      sum += data[i * stride] * data[i * stride];
    return std::sqrt(sum);
  } else if (p == std::numeric_limits<Real>::infinity()){
    for (MatrixIndexT i = 0; i < dim; i++)
      sum = std::max(sum, std::abs(data[i * stride]));
    return sum;
  } else {
    Real tmp;
    bool ok = true;
    for (MatrixIndexT i = 0; i < dim; i++) {
      tmp = pow(std::abs(data[i * stride]), p);
      if (tmp == HUGE_VAL) // HUGE_VAL is what pow returns on error.
        ok = false;
      sum += tmp;
    }
    tmp = pow(sum, static_cast<Real>(1.0/p));
    KALDI_ASSERT(tmp != HUGE_VAL); // should not happen here.
    if (ok) {
      return tmp;
    } else {
      Real maximum = this->Max(), minimum = this->Min(),
          max_abs = std::max(maximum, -minimum);
      KALDI_ASSERT(max_abs > 0); // Or should not have reached here.
      Vector<Real> tmp(*this);
      tmp.Scale(1.0 / max_abs);
      return tmp.Norm(p) * max_abs;
    }
  }
}

template<typename Real>
bool VectorBase<Real>::ApproxEqual(const VectorBase<Real> &other, float tol) const {
  if (dim_ != other.dim_) KALDI_ERR << "ApproxEqual: size mismatch "
                                    << dim_ << " vs. " << other.dim_;
  KALDI_ASSERT(tol >= 0.0);
  if (tol != 0.0) {
    Vector<Real> tmp(*this);
    tmp.AddVec(-1.0, other);
    return (tmp.Norm(2.0) <= static_cast<Real>(tol) * this->Norm(2.0));
  } else { // Test for exact equality.
    const Real *data = data_;
    const Real *other_data = other.data_;
    MatrixIndex other_stride = other.stride_, stride = stride_;
    for (MatrixIndexT dim = dim_, i = 0; i < dim; i++)
      if (data[i * stride] != other_data[i * other_stride]) return false;
    return true;
  }
}

template<typename Real>
Real VectorBase<Real>::Max() const {
  Real ans = - std::numeric_limits<Real>::infinity();
  const Real *data = data_;
  MatrixIndexT i, dim = dim_, stride = stride_;
  for (i = 0; i + 4 <= dim; i += 4) {
    Real a1 = data[i*stride], a2 = data[(i+1)*stride], a3 = data[(i+2)*stride], a4 = data[(i+3)*stride];
    if (a1 > ans || a2 > ans || a3 > ans || a4 > ans) {
      Real b1 = (a1 > a2 ? a1 : a2), b2 = (a3 > a4 ? a3 : a4);
      if (b1 > ans) ans = b1;
      if (b2 > ans) ans = b2;
    }
  }
  for (; i < dim; i++)
    if (data[i * stride] > ans) ans = data[i * stride];
  return ans;
}

template<typename Real>
Real VectorBase<Real>::Max(MatrixIndexT *index_out) const {
  if (dim_ == 0) KALDI_ERR << "Empty vector";
  Real ans = - std::numeric_limits<Real>::infinity();
  MatrixIndexT index = 0;
  const Real *data = data_;
  MatrixIndexT i, dim = dim_, stride = stride_;
  for (i = 0; i + 4 <= dim; i += 4) {
    Real a1 = data[i*stride], a2 = data[(i+1)*stride], a3 = data[(i+2)*stride], a4 = data[(i+3)*stride];
    if (a1 > ans || a2 > ans || a3 > ans || a4 > ans) {
      if (a1 > ans) { ans = a1; index = i; }
      if (a2 > ans) { ans = a2; index = i + 1; }
      if (a3 > ans) { ans = a3; index = i + 2; }
      if (a4 > ans) { ans = a4; index = i + 3; }
    }
  }
  for (; i < dim; i++)
    if (data[i * stride] > ans) { ans = data[i * stride]; index = i; }
  *index_out = index;
  return ans;
}

template<typename Real>
Real VectorBase<Real>::Min() const {
  Real ans = std::numeric_limits<Real>::infinity();
  const Real *data = data_;
  MatrixIndexT i, dim = dim_, stride = stride_;
  for (i = 0; i + 4 <= dim; i += 4) {
    Real a1 = data[i*stride], a2 = data[(i+1)*stride], a3 = data[(i+2)*stride], a4 = data[(i+3)*stride];
    if (a1 < ans || a2 < ans || a3 < ans || a4 < ans) {
      Real b1 = (a1 < a2 ? a1 : a2), b2 = (a3 < a4 ? a3 : a4);
      if (b1 < ans) ans = b1;
      if (b2 < ans) ans = b2;
    }
  }
  for (; i < dim; i++)
    if (data[i*stride] < ans) ans = data[i*stride];
  return ans;
}

template<typename Real>
Real VectorBase<Real>::Min(MatrixIndexT *index_out) const {
  if (dim_ == 0) KALDI_ERR << "Empty vector";
  Real ans = std::numeric_limits<Real>::infinity();
  MatrixIndexT index = 0;
  const Real *data = data_;
  MatrixIndexT i, dim = dim_, stride = stride_;
  for (i = 0; i + 4 <= dim; i += 4) {
    Real a1 = data[i*stride], a2 = data[(i+1)*stride], a3 = data[(i+2)*stride], a4 = data[(i+3)*stride];
    if (a1 < ans || a2 < ans || a3 < ans || a4 < ans) {
      if (a1 < ans) { ans = a1; index = i; }
      if (a2 < ans) { ans = a2; index = i + 1; }
      if (a3 < ans) { ans = a3; index = i + 2; }
      if (a4 < ans) { ans = a4; index = i + 3; }
    }
  }
  for (; i < dim; i++)
    if (data[i*stride] < ans) { ans = data[i*stride]; index = i; }
  *index_out = index;
  return ans;
}


template<typename Real>
template<typename OtherReal>
void VectorBase<Real>::CopyColFromMat(const MatrixBase<OtherReal> &mat, MatrixIndexT col) {
  KALDI_ASSERT(col < mat.NumCols());
  KALDI_ASSERT(dim_ == mat.NumRows());
  Real *data = data_;
  MatrixIndexT dim = dim_, stride = stride_;
  for (MatrixIndexT i = 0; i < dim; i++)
    data[i * stride] = mat(i, col);
  // can't do this very efficiently so don't really bother. could improve this though.
}
// instantiate the template above.
template
void VectorBase<float>::CopyColFromMat(const MatrixBase<float> &mat, MatrixIndexT col);
template
void VectorBase<float>::CopyColFromMat(const MatrixBase<double> &mat, MatrixIndexT col);
template
void VectorBase<double>::CopyColFromMat(const MatrixBase<float> &mat, MatrixIndexT col);
template
void VectorBase<double>::CopyColFromMat(const MatrixBase<double> &mat, MatrixIndexT col);

template<typename Real>
void VectorBase<Real>::CopyDiagFromMat(const MatrixBase<Real> &M) {
  KALDI_ASSERT(dim_ == std::min(M.NumRows(), M.NumCols()));
  cblas_Xcopy(dim_, M.Data(), M.Stride() + 1, data_, stride_);
}

template<typename Real>
void VectorBase<Real>::CopyDiagFromPacked(const PackedMatrix<Real> &M) {
  KALDI_ASSERT(dim_ == M.NumCols());
  Real *data = data_;
  MatrixIndexT stride = stride_, dim = dim_;
  for (MatrixIndexT i = 0; i < dim; i++)
    data[i * stride] = M(i, i);
  // could make this more efficient.
}

template<typename Real>
Real VectorBase<Real>::Sum() const {
  // Do a dot-product with a size-1 array with a stride of 0 to
  // implement sum. This allows us to access SIMD operations in a
  // cross-platform way via your BLAS library.
  Real one(1);
  return cblas_Xdot(dim_, data_, stride_, &one, 0);
}

template<typename Real>
Real VectorBase<Real>::SumLog() const {
  double sum_log = 0.0;
  double prod = 1.0;
  Real *data = data_;
  MatrixIndexT dim = dim_, stride = stride_;
  for (MatrixIndexT i = 0; i < dim; i++) {
    prod *= data[i * stride];
    // Possible future work (arnab): change these magic values to pre-defined
    // constants
    if (prod < 1.0e-10 || prod > 1.0e+10) {
      sum_log += Log(prod);
      prod = 1.0;
    }
  }
  if (prod != 1.0) sum_log += Log(prod);
  return sum_log;
}

template<typename Real>
void VectorBase<Real>::AddRowSumMat(Real alpha, const MatrixBase<Real> &M,
                                    Real beta) {
  KALDI_ASSERT(dim_ == M.NumCols());
  // the BLAS standard does not support vectors with stride zero, even though
  // some implementations (such as Mac's accelerate framework and I believe
  // CUBLAS) seem to allow it.  We compile a fixed-size (64) vector of ones
  // into the program.
  const Real *ones = Get64Ones<Real>();

  MatrixIndexT num_rows = M.NumRows();
  for (MatrixIndexT row_offset = 0; row_offset < num_rows; row_offset += 64) {
    MatrixIndexT this_num_rows =
        std::min<MatrixIndexT>(64, num_rows - row_offset);
    cblas_Xgemv(kTrans, this_num_rows, M.NumCols(), alpha,
                M.RowData(row_offset), M.Stride(), ones, 1,
                beta, data_, stride_);
    beta = 1.0;
  }
}


template<typename Real>
void VectorBase<Real>::AddColSumMat(Real alpha, const MatrixBase<Real> &M,
                                    Real beta) {
  KALDI_ASSERT(dim_ == M.NumRows());
  // the BLAS standard does not support vectors with stride zero, even though
  // some implementations (such as Mac's accelerate framework and I believe
  // CUBLAS) seem to allow it.  We compile a fixed-size (64) vector of ones
  // into the program.
  const Real *ones = Get64Ones<Real>();

  MatrixIndexT num_cols = M.NumCols();
  for (MatrixIndexT col_offset = 0; col_offset < num_cols; col_offset += 64) {
    MatrixIndexT this_num_cols =
        std::min<MatrixIndexT>(64, num_cols - col_offset);
    cblas_Xgemv(kNoTrans, M.NumRows(), this_num_cols, alpha,
                M.Data() + col_offset, M.Stride(), ones, 1,
                beta, data_, stride_);
    beta = 1.0;
  }
}

template<typename Real>
Real VectorBase<Real>::LogSumExp(Real prune) const {
  Real sum;
  if (sizeof(sum) == 8) sum = kLogZeroDouble;
  else sum = kLogZeroFloat;
  Real max_elem = Max(), cutoff;
  if (sizeof(Real) == 4) cutoff = max_elem + kMinLogDiffFloat;
  else cutoff = max_elem + kMinLogDiffDouble;
  if (prune > 0.0 && max_elem - prune > cutoff) // explicit pruning...
    cutoff = max_elem - prune;

  double sum_relto_max_elem = 0.0;

  const Real *data = data_;
  MatrixIndexT dim = dim_, stride = stride_;

  for (MatrixIndexT i = 0; i < dim; i++) {
    BaseFloat f = data[i * stride];
    if (f >= cutoff)
      sum_relto_max_elem += Exp(f - max_elem);
  }
  return max_elem + Log(sum_relto_max_elem);
}

template<typename Real>
void VectorBase<Real>::InvertElements() {
  MatrixIndexT dim = dim_, stride = stride_;
  for (MatrixIndexT i = 0; i < dim; i++) {
    data_[i * stride] = static_cast<Real>(1) / data_[i * stride];
  }
}

template<typename Real>
void VectorBase<Real>::ApplyLog() {
  MatrixIndexT dim = dim_, stride = stride_;
  Real *data = data_;
  for (MatrixIndexT i = 0; i < dim; i++) {
    if (data[i * stride] < 0.0)
      KALDI_ERR << "Trying to take log of a negative number.";
    data[i * stride] = Log(data[i * stride]);
  }
}

template<typename Real>
void VectorBase<Real>::ApplyLog(const VectorBase<Real> &v) {
  KALDI_ASSERT(dim_ == v.Dim());
  MatrixIndexT dim = dim_, stride = stride_, v_stride = v.stride_;
  Real *data = data_;
  const Real *v_data = v.data_;
  for (MatrixIndexT i = 0; i < dim; i++) {
    data[i * stride] = Log(v_data[i * v_stride]);
  }
}

template<typename Real>
void VectorBase<Real>::ApplyExp() {
  Real *data = data_;
  MatrixIndexT dim = dim_, stride = stride_;
  for (MatrixIndexT i = 0; i < dim; i++) {
    data[i * stride] = Exp(data[i * stride]);
  }
}

template<typename Real>
void VectorBase<Real>::ApplyAbs() {
  Real *data = data_;
  MatrixIndexT dim = dim_, stride = stride_;
  for (MatrixIndexT i = 0; i < dim; i++) {
    data[i * stride] = std::abs(data[i * stride]);
  }
}

template<typename Real>
void VectorBase<Real>::ApplyFloor(Real floor_val, MatrixIndexT *floored_count) {
  Real *data = data_;
  MatrixIndexT dim = dim_, stride = stride_;
  if (floored_count == nullptr) {
    for (MatrixIndexT i = 0; i < dim; i++) {
      data[i] = std::max(data[i * stride], floor_val);
    }
  } else {
    MatrixIndexT num_floored = 0;
    for (MatrixIndexT i = 0; i < dim; i++) {
      if (data[i * stride] < floor_val) {
        data[i * stride] = floor_val;
        num_floored++;
      }
    }
    *floored_count = num_floored;
  }
}

template<typename Real>
void VectorBase<Real>::ApplyCeiling(Real ceil_val, MatrixIndexT *ceiled_count) {
  MatrixIndexT dim = dim_, stride = stride_;
  Real *data = data_;
  if (ceiled_count == nullptr) {
    for (MatrixIndexT i = 0; i < dim; i++) {
      data[i * stride] = std::min(data[i * stride], ceil_val);
    }
  } else {
    MatrixIndexT num_changed = 0;
    for (MatrixIndexT i = 0; i < dim; i++) {
      if (data[i * stride] > ceil_val) {
        data_[i * stride] = ceil_val;
        num_changed++;
      }
    }
    *ceiled_count = num_changed;
  }
}

template<typename Real>
MatrixIndexT VectorBase<Real>::ApplyFloor(const VectorBase<Real> &floor_vec) {
  MatrixIndexT dim = dim_, stride = stride_,
      floor_stride = floor_vec.stride_;
  Real *data = data_;
  const Real *floor_data = floor_vec.data_;
  KALDI_ASSERT(floor_vec.dim_ == dim);
  MatrixIndexT num_floored = 0;
  for (MatrixIndexT i = 0; i < dim; i++) {
    if (data[i * stride] < floor_data[i * floor_stride]) {
      data_[i * stride] = floor_data[i * floor_stride];
      num_floored++;
    }
  }
  return num_floored;
}

template<typename Real>
Real VectorBase<Real>::ApplySoftMax() {
  Real max = this->Max(), sum = 0.0;
  Real *data = data_;
  MatrixIndexT dim = dim_, stride = stride_;
  for (MatrixIndexT i = 0; i < dim; i++) {
    sum += (data[i * stride] = Exp(data[i * stride] - max));
  }
  this->Scale(1.0 / sum);
  return max + Log(sum);
}

template<typename Real>
Real VectorBase<Real>::ApplyLogSoftMax() {
  Real max = this->Max(), sum = 0.0;
  Real *data = data_;
  MatrixIndexT dim = dim_, stride = stride_;
  for (MatrixIndexT i = 0; i < dim; i++) {
    sum += Exp((data[i * stride] -= max));
  }
  sum = Log(sum);
  this->Add(-1.0 * sum);
  return max + sum;
}

#ifdef HAVE_MKL
template<>
void VectorBase<float>::Tanh(const VectorBase<float> &src) {
  KALDI_ASSERT(dim_ == src.dim_);
  vsTanh(dim_, src.data_, data_);
}
template<>
void VectorBase<double>::Tanh(const VectorBase<double> &src) {
  KALDI_ASSERT(dim_ == src.dim_);
  vdTanh(dim_, src.data_, data_);
}
#else
template<typename Real>
void VectorBase<Real>::Tanh(const VectorBase<Real> &src) {
  KALDI_ASSERT(dim_ == src.dim_);
  Real *data = data_;
  MatrixIndexT dim = dim_, stride = stride_, src_stride = src.stride_;
  for (MatrixIndexT i = 0; i < dim; i++) {
    Real x = src.data[i * src_stride];
    if (x > 0.0) {
      Real inv_expx = Exp(-x);
      x = -1.0 + 2.0 / (1.0 + inv_expx * inv_expx);
    } else {
      Real expx = Exp(x);
      x = 1.0 - 2.0 / (1.0 + expx * expx);
    }
    data[i * stride] = x;
  }
}
#endif

#ifdef HAVE_MKL
// Implementing sigmoid based on tanh.
template<>
void VectorBase<float>::Sigmoid(const VectorBase<float> &src) {
  KALDI_ASSERT(dim_ == src.dim_);
  this->CopyFromVec(src);
  this->Scale(0.5);
  vsTanh(dim_, data_, data_);
  this->Add(1.0);
  this->Scale(0.5);
}
template<>
void VectorBase<double>::Sigmoid(const VectorBase<double> &src) {
  KALDI_ASSERT(dim_ == src.dim_);
  this->CopyFromVec(src);
  this->Scale(0.5);
  vdTanh(dim_, data_, data_);
  this->Add(1.0);
  this->Scale(0.5);
}
#else
template<typename Real>
void VectorBase<Real>::Sigmoid(const VectorBase<Real> &src) {
  KALDI_ASSERT(dim_ == src.dim_);
  Real *data = data_;
  MatrixIndexT dim = dim_, stride = stride_, src_stride = src.stride_;
  for (MatrixIndexT i = 0; i < dim; i++) {
    Real x = src.data[i * src_stride];
    // We aim to avoid floating-point overflow here.
    if (x > 0.0) {
      x = 1.0 / (1.0 + Exp(-x));
    } else {
      Real ex = Exp(x);
      x = ex / (ex + 1.0);
    }
    data[i * stride] = x;
  }
}
#endif


template<typename Real>
void VectorBase<Real>::Add(Real c) {
  Real *data = data_;
  MatrixIndexT dim = dim_, stride = stride_;
  for (MatrixIndexT i = 0; i < dim; i++) {
    data[i * stride] += c;
  }
}

template<typename Real>
void VectorBase<Real>::Scale(Real alpha) {
  cblas_Xscal(dim_, alpha, data_, stride_);
}

template<typename Real>
void VectorBase<Real>::MulElements(const VectorBase<Real> &v) {
  KALDI_ASSERT(dim_ == v.dim_);
  Real *data = data_, *v_data = v.data_;
  MatrixIndexT dim = dim_, v_stride = v.stride_, stride = stride_;
  for (MatrixIndexT i = 0; i < dim; i++) {
    data[i * stride] *= v_data[i * v_stride];
  }
}

template<typename Real>  // Set each element to y = (x == orig ? changed : x).
void VectorBase<Real>::ReplaceValue(Real orig, Real changed) {
  Real *data = data_;
  MatrixIndexT dim = dim_, stride = stride_;
  for (MatrixIndexT i = 0; i < dim; i++)
    if (data[i * stride] == orig) data[i * stride] = changed;
}


template<typename Real>
template<typename OtherReal>
void VectorBase<Real>::MulElements(const VectorBase<OtherReal> &v) {
  KALDI_ASSERT(dim_ == v.Dim());
  const OtherReal *other_ptr = v.Data();
  Real *data = data_;
  MatrixIndexT dim = dim_, v_stride = v.Stride(), stride = stride_;
  for (MatrixIndexT i = 0; i < dim; i++) {
    data_[i * stride] *= other_ptr[i * v_stride];
  }
}
// instantiate template.
template
void VectorBase<float>::MulElements(const VectorBase<double> &v);
template
void VectorBase<double>::MulElements(const VectorBase<float> &v);


template<typename Real>
void VectorBase<Real>::AddVecVec(Real alpha, const VectorBase<Real> &v,
                                 const VectorBase<Real> &r, Real beta) {
  KALDI_ASSERT(v.data_ != this->data_ && r.data_ != this->data_);
  // We pretend that v is a band-diagonal matrix.
  KALDI_ASSERT(dim_ == v.dim_ && dim_ == r.dim_);
  cblas_Xgbmv(kNoTrans, dim_, dim_, 0, 0, alpha, v.data_, v.stride_,
              r.data_, r.stride_, beta, this->data_, stride_);
}


template<typename Real>
void VectorBase<Real>::DivElements(const VectorBase<Real> &v) {
  KALDI_ASSERT(dim_ == v.dim_);
  Real *data = data_;
  MatrixIndexT dim = dim_, v_stride = v.stride_, stride = stride_;
  for (MatrixIndexT i = 0; i < dim; i++) {
    data_[i * stride] /= v.data_[i * v_stride];
  }
}

template<typename Real>
template<typename OtherReal>
void VectorBase<Real>::DivElements(const VectorBase<OtherReal> &v) {
  KALDI_ASSERT(dim_ == v.Dim());
  const OtherReal *other_ptr = v.Data();
  MatrixIndexT dim = dim_, v_stride = v.Stride(), stride = stride_;
  for (MatrixIndexT i = 0; i < dim; i++) {
    data[i * stride] /= other_ptr[i * v_stride];
  }
}
// instantiate template.
template
void VectorBase<float>::DivElements(const VectorBase<double> &v);
template
void VectorBase<double>::DivElements(const VectorBase<float> &v);

template<typename Real>
void VectorBase<Real>::AddVecDivVec(Real alpha, const VectorBase<Real> &v,
                                    const VectorBase<Real> &rr, Real beta) {
  KALDI_ASSERT((dim_ == v.dim_ && dim_ == rr.dim_));
  Real *data = data_, *v_data = v.data_, *rr_data = rr.data_;
  MatrixIndexT dim = dim_, v_stride = v.stride_, rr_stride = rr.stride_, stride = stride_;
  for (MatrixIndexT i = 0; i < dim; i++) {
    data[i * stride] = alpha * v_data[i * v_stride]/rr_data[i * rr_stride] +
      beta * data[i * stride];
  }
}

template<typename Real>
template<typename OtherReal>
void VectorBase<Real>::AddVec(const Real alpha, const VectorBase<OtherReal> &v) {
  KALDI_ASSERT(dim_ == v.dim_);
  // remove __restrict__ if it causes compilation problems.
  Real *__restrict__ data = data_;
  OtherReal *__restrict__ other_data = v.data_;
  MatrixIndexT dim = dim_, v_stride = v.stride_, stride = stride_;
  if (alpha != 1.0)
    for (MatrixIndexT i = 0; i < dim; i++)
      data[i * stride] += alpha * other_data[i * v_stride];
  else
    for (MatrixIndexT i = 0; i < dim; i++)
      data[i * stride] += other_data[i * v_stride];
}

template
void VectorBase<float>::AddVec(const float alpha, const VectorBase<double> &v);
template
void VectorBase<double>::AddVec(const double alpha, const VectorBase<float> &v);

template<typename Real>
template<typename OtherReal>
void VectorBase<Real>::AddVec2(const Real alpha, const VectorBase<OtherReal> &v) {
  KALDI_ASSERT(dim_ == v.dim_);
  // remove __restrict__ if it causes compilation problems.
  Real *__restrict__ data = data_;
  OtherReal *__restrict__ other_data = v.data_;
  MatrixIndexT dim = dim_, v_stride = v.stride_, stride = stride_;
  if (alpha != 1.0)
    for (MatrixIndexT i = 0; i < dim; i++)
      data[i * stride] += alpha * other_data[i * v_stride] * other_data[i * v_stride];
  else
    for (MatrixIndexT i = 0; i < dim; i++)
      data[i * stride] += other_data[i * v_stride] * other_data[i * v_stride];
}

template
void VectorBase<float>::AddVec2(const float alpha, const VectorBase<double> &v);
template
void VectorBase<double>::AddVec2(const double alpha, const VectorBase<float> &v);


template<typename Real>
void VectorBase<Real>::Read(std::istream & is,  bool binary, bool add) {
  if (add) {
    Vector<Real> tmp(Dim());
    tmp.Read(is, binary, false);  // read without adding.
    if (this->Dim() != tmp.Dim()) {
      KALDI_ERR << "VectorBase::Read, size mismatch " << this->Dim()<<" vs. "<<tmp.Dim();
    }
    this->AddVec(1.0, tmp);
    return;
  } // now assume add == false.

  //  In order to avoid rewriting this, we just declare a Vector and
  // use it to read the data, then copy.
  Vector<Real> tmp;
  tmp.Read(is, binary, false);
  if (tmp.Dim() != Dim())
    KALDI_ERR << "VectorBase<Real>::Read, size mismatch "
              << Dim() << " vs. " << tmp.Dim();
  CopyFromVec(tmp);
}


template<typename Real>
void Vector<Real>::Read(std::istream & is,  bool binary, bool add) {
  if (add) {
    Vector<Real> tmp(this->Dim());
    tmp.Read(is, binary, false);  // read without adding.
    if (this->Dim() == 0) this->Resize(tmp.Dim());
    if (this->Dim() != tmp.Dim()) {
      KALDI_ERR << "Vector<Real>::Read, adding but dimensions mismatch "
                << this->Dim() << " vs. " << tmp.Dim();
    }
    this->AddVec(1.0, tmp);
    return;
  } // now assume add == false.

  std::ostringstream specific_error;
  MatrixIndexT pos_at_start = is.tellg();

  if (binary) {
    int peekval = Peek(is, binary);
    const char *my_token =  (sizeof(Real) == 4 ? "FV" : "DV");
    char other_token_start = (sizeof(Real) == 4 ? 'D' : 'F');
    if (peekval == other_token_start) {  // need to instantiate the other type to read it.
      typedef typename OtherReal<Real>::Real OtherType;  // if Real == float, OtherType == double, and vice versa.
      Vector<OtherType> other(this->Dim());
      other.Read(is, binary, false);  // add is false at this point.
      if (this->Dim() != other.Dim()) this->Resize(other.Dim());
      this->CopyFromVec(other);
      return;
    }
    std::string token;
    ReadToken(is, binary, &token);
    if (token != my_token) {
      if (token.length() > 20) token = token.substr(0, 17) + "...";
      specific_error << ": Expected token " << my_token << ", got " << token;
      goto bad;
    }
    int32 size;
    ReadBasicType(is, binary, &size);  // throws on error.
    if ((MatrixIndexT)size != this->Dim())  this->Resize(size);
    if (size > 0)
      is.read(reinterpret_cast<char*>(this->data_), sizeof(Real)*size);
    if (is.fail()) {
      specific_error << "Error reading vector data (binary mode); truncated "
          "stream? (size = " << size << ")";
      goto bad;
    }
    return;
  } else {  // Text mode reading; format is " [ 1.1 2.0 3.4 ]\n"
    std::string s;
    is >> s;
    // if ((s.compare("DV") == 0) || (s.compare("FV") == 0)) {  // Back compatibility.
    //  is >> s;  // get dimension
    //  is >> s;  // get "["
    // }
    if (is.fail()) { specific_error << "EOF while trying to read vector."; goto bad; }
    if (s.compare("[]") == 0) { Resize(0); return; } // tolerate this variant.
    if (s.compare("[")) {
      if (s.length() > 20) s = s.substr(0, 17) + "...";
      specific_error << "Expected \"[\" but got " << s;
      goto bad;
    }
    std::vector<Real> data;
    while (1) {
      int i = is.peek();
      if (i == '-' || (i >= '0' && i <= '9')) {  // common cases first.
        Real r;
        is >> r;
        if (is.fail()) { specific_error << "Failed to read number."; goto bad; }
        if (! std::isspace(is.peek()) && is.peek() != ']') {
          specific_error << "Expected whitespace after number."; goto bad;
        }
        data.push_back(r);
        // But don't eat whitespace... we want to check that it's not newlines
        // which would be valid only for a matrix.
      } else if (i == ' ' || i == '\t') {
        is.get();
      } else if (i == ']') {
        is.get();  // eat the ']'
        this->Resize(data.size());
        for (size_t j = 0; j < data.size(); j++)
          this->data_[j * this->stride_] = data[j * data.stride_];
        i = is.peek();
        if (static_cast<char>(i) == '\r') {
          is.get();
          is.get();  // get \r\n (must eat what we wrote)
        } else if (static_cast<char>(i) == '\n') { is.get(); } // get \n (must eat what we wrote)
        if (is.fail()) {
          KALDI_WARN << "After end of vector data, read error.";
          // we got the data we needed, so just warn for this error.
        }
        return;  // success.
      } else if (i == -1) {
        specific_error << "EOF while reading vector data.";
        goto bad;
      } else if (i == '\n' || i == '\r') {
        specific_error << "Newline found while reading vector (maybe it's a matrix?)";
        goto bad;
      } else {
        is >> s;  // read string.
        if (!KALDI_STRCASECMP(s.c_str(), "inf") ||
            !KALDI_STRCASECMP(s.c_str(), "infinity")) {
          data.push_back(std::numeric_limits<Real>::infinity());
          KALDI_WARN << "Reading infinite value into vector.";
        } else if (!KALDI_STRCASECMP(s.c_str(), "nan")) {
          data.push_back(std::numeric_limits<Real>::quiet_NaN());
          KALDI_WARN << "Reading NaN value into vector.";
        } else {
          if (s.length() > 20) s = s.substr(0, 17) + "...";
          specific_error << "Expecting numeric vector data, got " << s;
          goto  bad;
        }
      }
    }
  }
  // we never reach this line (the while loop returns directly).
bad:
  KALDI_ERR << "Failed to read vector from stream.  " << specific_error.str()
            << " File position at start is "
            << pos_at_start<<", currently "<<is.tellg();
}


template<typename Real>
void VectorBase<Real>::Write(std::ostream & os, bool binary) const {
  if (!os.good()) {
    KALDI_ERR << "Failed to write vector to stream: stream not good";
  }
  if (binary) {
    std::string my_token = (sizeof(Real) == 4 ? "FV" : "DV");
    WriteToken(os, binary, my_token);

    int32 size = Dim();  // make the size 32-bit on disk.
    KALDI_ASSERT(Dim() == (MatrixIndexT) size);
    WriteBasicType(os, binary, size);
    os.write(reinterpret_cast<const char*>(Data()), sizeof(Real) * size);
  } else {
    MatrixIndexT stride = this->stride_;
    os << " [ ";
    for (MatrixIndexT i = 0; i < Dim(); i++)
      os << (*this)(i * stride) << " ";
    os << "]\n";
  }
  if (!os.good())
    KALDI_ERR << "Failed to write vector to stream";
}


template<typename Real>
void VectorBase<Real>::AddVec2(const Real alpha, const VectorBase<Real> &v) {
  KALDI_ASSERT(dim_ == v.dim_);
  Real *data = data, v_data = v.data_;
  MatrixIndexT dim = dim_, v_stride = v.stride_, stride = stride_;
  for (MatrixIndexT i = 0; i < dim; i++)
    data[i * stride] += alpha * v_data[i * v_stride] * v_data[i * v_stride];
}

// this <-- beta*this + alpha*M*v.
template<typename Real>
void VectorBase<Real>::AddTpVec(const Real alpha, const TpMatrix<Real> &M,
                                const MatrixTransposeType trans,
                                const VectorBase<Real> &v,
                                const Real beta) {
  KALDI_ASSERT(dim_ == v.dim_ && dim_ == M.NumRows());
  if (beta == 0.0) {
    if (&v != this) CopyFromVec(v);
    MulTp(M, trans);
    if (alpha != 1.0) Scale(alpha);
  } else {
    Vector<Real> tmp(v);
    tmp.MulTp(M, trans);
    if (beta != 1.0) Scale(beta);  // *this <-- beta * *this
    AddVec(alpha, tmp);          // *this += alpha * M * v
  }
}

template<typename Real>
Real VecMatVec(const VectorBase<Real> &v1, const MatrixBase<Real> &M,
               const VectorBase<Real> &v2) {
  KALDI_ASSERT(v1.Dim() == M.NumRows() && v2.Dim() == M.NumCols());
  Vector<Real> vtmp(M.NumRows());
  vtmp.AddMatVec(1.0, M, kNoTrans, v2, 0.0);
  return VecVec(v1, vtmp);
}

template
float VecMatVec(const VectorBase<float> &v1, const MatrixBase<float> &M,
                const VectorBase<float> &v2);
template
double VecMatVec(const VectorBase<double> &v1, const MatrixBase<double> &M,
                 const VectorBase<double> &v2);

template<typename Real>
void Vector<Real>::Swap(Vector<Real> *other) {
  std::swap(this->data_, other->data_);
  std::swap(this->dim_, other->dim_);
}


template<typename Real>
void VectorBase<Real>::AddDiagMat2(
    Real alpha, const MatrixBase<Real> &M,
    MatrixTransposeType trans, Real beta) {
  if (trans == kNoTrans) {
    KALDI_ASSERT(this->dim_ == M.NumRows());
    MatrixIndexT rows = this->dim_, cols = M.NumCols(),
      mat_stride = M.Stride(), stride = this->stride_;
    Real *data = this->data_;
    const Real *mat_data = M.Data();
    for (MatrixIndexT i = 0; i < rows; i++, mat_data += mat_stride, data += stride)
      *data = beta * *data + alpha * cblas_Xdot(cols, mat_data, 1,
                                                mat_data, 1);
  } else {
    KALDI_ASSERT(this->dim_ == M.NumCols());
    MatrixIndexT rows = M.NumRows(), cols = this->dim_,
      mat_stride = M.Stride(), stride = stride_;
    Real *data = this->data_;
    const Real *mat_data = M.Data();
    for (MatrixIndexT i = 0; i < cols; i++, mat_data++, data += stride)
      *data = beta * *data + alpha * cblas_Xdot(rows, mat_data, mat_stride,
                                                 mat_data, mat_stride);
  }
}

template<typename Real>
void VectorBase<Real>::AddDiagMatMat(
    Real alpha,
    const MatrixBase<Real> &M, MatrixTransposeType transM,
    const MatrixBase<Real> &N, MatrixTransposeType transN,
    Real beta) {
  MatrixIndexT dim = this->dim_,
      M_col_dim = (transM == kTrans ? M.NumRows() : M.NumCols()),
      N_row_dim = (transN == kTrans ? N.NumCols() : N.NumRows());
  KALDI_ASSERT(M_col_dim == N_row_dim); // this is the dimension we sum over
  MatrixIndexT M_row_stride = M.Stride(), M_col_stride = 1;
  if (transM == kTrans) std::swap(M_row_stride, M_col_stride);
  MatrixIndexT N_row_stride = N.Stride(), N_col_stride = 1;
  if (transN == kTrans) std::swap(N_row_stride, N_col_stride);
  MatrixIndexT stride = this->stride_;

  Real *data = this->data_;
  const Real *Mdata = M.Data(), *Ndata = N.Data();
  for (MatrixIndexT i = 0; i < dim; i++, Mdata += M_row_stride, Ndata += N_col_stride, data += stride) {
    *data = beta * *data + alpha * cblas_Xdot(M_col_dim, Mdata, M_col_stride, Ndata, N_row_stride);
  }
}


template class Vector<float>;
template class Vector<double>;
template class VectorBase<float>;
template class VectorBase<double>;

}  // namespace kaldi
