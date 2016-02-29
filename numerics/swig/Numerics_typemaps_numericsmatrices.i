#define GET_INT(OBJ,VAR)                                                \
  if(!PyInt_Check(OBJ))                                                 \
  {                                                                     \
    throw(std::invalid_argument(#OBJ ": expecting an int")); \
  }                                                                     \
  VAR = PyInt_AsLong(OBJ)

// need a PySequence_Check
#define GET_INTS(OBJ,INDEX,VAR)                                          \
  PyObject *_TEMP##VAR = PySequence_GetItem(OBJ,INDEX);                 \
  if (!PyInt_Check(_TEMP##VAR))                                         \
  {                                                                     \
    Py_XDECREF(_TEMP##VAR);                                             \
    throw(std::invalid_argument(#OBJ ": expecting an int")); \
  }                                                                     \
  VAR = PyInt_AsLong(_TEMP##VAR);                                   \
  Py_DECREF(_TEMP##VAR)

// XXX this is a hack --xhub
#undef SICONOS_INT64

%inline %{

#ifndef NDEBUG
static inline void _sn_check_nnz(PyObject* mat, cs_sparse *M)
{
  PyObject *auto_nnz = PyObject_GetAttrString(mat,"nnz");
  assert(PyInt_AsLong(auto_nnz) == M->nzmax);
  Py_XDECREF(auto_nnz);
}
#else
static inline void _sn_check_nnz(PyObject* mat, cs_sparse *M) {};
#endif

#include "SiconosConfig.h"

#ifdef SICONOS_INT64
#define NPY_INT_TYPE NPY_INT64
#else
#define NPY_INT_TYPE NPY_INT32
#endif

#define ALLOC_CTRL_I 0x1
#define ALLOC_CTRL_P 0x2

#define CS_TO_SCIPY(TYPE, P_LEN) \
  if (!M) \
  { \
    Py_INCREF(Py_None); \
    return Py_None; \
  } \
  else \
  { \
    /* get sys.modules dict */ \
    PyObject* sys_mod_dict = PyImport_GetModuleDict(); \
\
    /* get the csr module object */ \
    PyObject* scipy_mod = PyMapping_GetItemString(sys_mod_dict, (char *)"scipy.sparse." #TYPE);\
\
    if (!scipy_mod) \
    { \
      PyErr_SetString(PyExc_RuntimeError, "Did you import scipy.sparse." #TYPE "?"); \
      return NULL; \
    } \
\
    npy_intp this_M_x_dims[1] = { M->nzmax }; \
\
    npy_intp this_M_i_dims[1] = { M->nzmax }; \
\
    npy_intp this_M_p_dims[1] = { P_LEN + 1 }; \
\
    PyObject* out_data = PyArray_SimpleNewFromData(1,this_M_x_dims,NPY_DOUBLE,M->x); \
    if(!out_data) { PyErr_SetString(PyExc_RuntimeError, "Could not extract M->x"); return NULL; }; \
\
    PyObject* out_indices = PyArray_SimpleNewFromData(1,this_M_i_dims, NPY_INT_TYPE, M->i); \
    if(!out_indices) {  PyErr_SetString(PyExc_RuntimeError, "Could not extract M->i"); return NULL; }; \
\
    PyObject* out_indptr = PyArray_SimpleNewFromData(1,this_M_p_dims, NPY_INT_TYPE,M->p); \
    if(!out_indptr) {  PyErr_SetString(PyExc_RuntimeError, "Could not extract M->p"); return NULL; }; \
\
    /* Warning ! m is the number of rows, n the number of columns ! --xhub */ \
    PyObject* out_shape = PyTuple_Pack(2, PyInt_FromLong(M->m), PyInt_FromLong(M->n)); \
    if(!out_shape) {  PyErr_SetString(PyExc_RuntimeError, "Could not extract M->m or M->n"); return NULL; }; \
\
    PyObject* out_nnz = PyInt_FromLong(M->nzmax); \
    if(!out_nnz) {  PyErr_SetString(PyExc_RuntimeError, "Could not extract M->nzmax"); return NULL; }; \
\
    /* call the class inside the csr module */ \
    PyObject* out_mat = PyObject_CallMethodObjArgs(scipy_mod, PyString_FromString((char *) #TYPE "_matrix"), out_shape, NULL); \
\
    if(out_mat) \
    { \
      PyObject_SetAttrString(out_mat,"data", out_data); \
      PyObject_SetAttrString(out_mat,"indices", out_indices); \
      PyObject_SetAttrString(out_mat,"indptr", out_indptr); \
\
      _sn_check_nnz(out_mat, M); \
\
      return out_mat; \
    } \
    else \
    { \
      PyErr_SetString(PyExc_RuntimeError, "Could not create " #TYPE " matrix"); \
      return NULL; \
    } \
  }

static inline bool is_Pyobject_scipy_sparse_matrix(PyObject* o, PyObject* scipy_mod)
 {
    bool ret;
    PyObject* res = PyObject_CallMethodObjArgs(scipy_mod, PyString_FromString("issparse"), o, NULL);

    if (!res) return false;

    ret = (res == Py_True);
    Py_DECREF(res);

    return ret;
}

%}


%define %SAFE_CAST_INT(pyvar, len, dest_array, array_pyvar, indvar, alloc)
{
    int array_pyvartype_ = PyArray_TYPE((PyArrayObject *)pyvar);
    switch (array_pyvartype_)
    {
      case NPY_INT32:
      {
        array_pyvar = obj_to_array_allow_conversion(pyvar, NPY_INT32, indvar);
        if (!array_pyvar) { PyErr_SetString(PyExc_RuntimeError, "Could not get array for variable" #pyvar); return 0; }

%#ifdef SICONOS_INT64
        PyErr_Warn(PyExc_UserWarning, "Performance warning: the vector of indices or pointers is in int32, but siconos has 64-bits integers: we have to perform a conversion. Consider given sparse matrix in the right format");
        dest_array = (int64_t*) malloc(len * sizeof(int64_t));
        if(!dest_array) { PyErr_SetString(PyExc_RuntimeError, "Allocation of i or p failed (triggered by conversion to int32)"); return 0; }
        for(unsigned i = 0; i < len; ++i)
        {
          dest_array[i] = ((int32_t *) array_data(array_pyvar)) [i];
        }
        if (*indvar) Py_DECREF(array_pyvar);
        *indvar = 0;
        alloc = true;
%#else
        dest_array = (int32_t *) array_data(array_pyvar);
%#endif
        break;
      }
      case NPY_INT64:
      {
        array_pyvar = obj_to_array_allow_conversion(pyvar, NPY_INT64, indvar);
        if (!array_pyvar) { PyErr_SetString(PyExc_RuntimeError, "Could not get array for variable " #pyvar); return 0; }

%#ifdef SICONOS_INT64
        dest_array = (int64_t *) array_data(array_pyvar);
%#else
        PyErr_Warn(PyExc_UserWarning, "Performance warning: the vector of indices or pointers is in int64, but siconos has 32-bits integers: we have to perform a conversion. Consider given sparse matrix in the right format");
        dest_array = (int32_t*) malloc(len * sizeof(int32_t));
        if(!dest_array) { PyErr_SetString(PyExc_RuntimeError, "Allocation of i or p failed (triggered by conversion to int64)"); return 0; }
        for(unsigned i = 0; i < len; ++i)
        {
          dest_array[i] = ((int64_t *) array_data(array_pyvar)) [i];
        }
        if (*indvar) Py_DECREF(array_pyvar);
        *indvar = 0;
        alloc = true;
%#endif
        break;
      }
      default:
      {
        PyObject *errmsg;
        errmsg = PyUString_FromString("Unknown type ");
        PyUString_ConcatAndDel(&errmsg, PyObject_Repr((PyObject *)PyArray_DESCR((PyArrayObject *)pyvar)));
        PyUString_ConcatAndDel(&errmsg, PyUString_FromFormat(" for variable " #pyvar));
        PyErr_SetObject(PyExc_TypeError, errmsg);
        Py_DECREF(errmsg);
        return 0;
      }
    }
}
%enddef


%fragment("NumericsMatrix", "header", fragment="NumPy_Fragments")
{
  int cs_convert_from_scipy_sparse(PyObject* obj, CSparseMatrix** m, PyArrayObject* array_data_, int* array_data_ctrl_, PyArrayObject* array_i_, int* array_i_ctrl_, PyArrayObject* array_p_, int* array_p_ctrl_, int* alloc_ctrl)
  {

  assert(m);
  /* get sys.modules dict */
  PyObject* sys_mod_dict = PyImport_GetModuleDict();
  /* get the scipy module object */ 
  PyObject* scipy_mod = PyMapping_GetItemString(sys_mod_dict, (char *)"scipy.sparse");

  if (!scipy_mod) 
  { 
    PyErr_SetString(PyExc_RuntimeError, "Did you import scipy.sparse ?");
    return 0;
  }

  bool isspmat = is_Pyobject_scipy_sparse_matrix(obj, scipy_mod);
  if (isspmat == false)
  {
    return -1;
  }
  else
  {
    PyObject* shape_ = PyObject_GetAttrString(obj, "shape");

    unsigned nrows, ncols;
    GET_INTS(shape_, 0, nrows);
    GET_INTS(shape_, 1, ncols);

    Py_DECREF(shape_);


    PyObject* res;

    res = PyObject_CallMethodObjArgs(scipy_mod, PyString_FromString("isspmatrix_csc"), obj, NULL);
    bool is_csc = (res == Py_True);
    Py_DECREF(res);

    if (is_csc)
    {
      // csc

      PyObject* nnz_ = PyObject_GetAttrString(obj, "nnz");
      size_t nzmax = PyInt_AsLong(nnz_);
      Py_DECREF(nnz_);

      CSparseMatrix* M = cs_spalloc(nrows, ncols, nzmax, 1, 0);
      *m = M;
      if(!M) { PyErr_SetString(PyExc_RuntimeError, "Allocation of the csc matrix failed"); return 0; };

      PyObject* data_ = PyObject_GetAttrString(obj, "data");
      PyObject* indices_ = PyObject_GetAttrString(obj, "indices");
      PyObject* indptr_ = PyObject_GetAttrString(obj, "indptr");

      array_data_ = obj_to_array_allow_conversion(data_, NPY_DOUBLE, array_data_ctrl_);
      if (!array_data_) { PyErr_SetString(PyExc_RuntimeError, "Could not get a pointer to the data array"); return 0; }

      M->x = (double*)array_data(array_data_);

      bool alloc_p = false;
      %SAFE_CAST_INT(indptr_, M->n + 1, M->p, array_p_, array_p_ctrl_, alloc_p);
      if (alloc_p) { *alloc_ctrl |= ALLOC_CTRL_P; };
      bool alloc_i = false;
      %SAFE_CAST_INT(indices_, nzmax, M->i, array_i_, array_i_ctrl_, alloc_i);
      if (alloc_i) { *alloc_ctrl |= ALLOC_CTRL_I; };

      return 1;
    }

#ifdef WITH_CSR
    res = PyObject_CallMethodObjArgs(scipy_mod, PyString_FromString("isspmatrix_csr"), obj, NULL);
    bool is_csr = (res == Py_True);
    Py_DECREF(res);

    if (is_csr)
    {
      // csr

      PyObject* nnz_ = PyObject_GetAttrString(obj, "nnz");
      size_t nzmax = PyInt_AsLong(nnz_);
      Py_DECREF(nnz_);

      CSparseMatrix* M = cs_spalloc(ncols, nrows, nzmax, 1, 0);
      M->nz = -2;
      M->m = nrows;
      M->n = ncols;
      *m = M;

      if(!M) { PyErr_SetString(PyExc_RuntimeError, "Allocation of the M matrix failed"); return 0; };

      PyObject* data_ = PyObject_GetAttrString(obj, "data");
      PyObject* indices_ = PyObject_GetAttrString(obj, "indices");
      PyObject* indptr_ = PyObject_GetAttrString(obj, "indptr");

      array_data_ = obj_to_array_allow_conversion(data_, NPY_DOUBLE, array_data_ctrl_);
      if (!array_data_) { PyErr_SetString(PyExc_RuntimeError, "Could not get a pointer to the data array"); return 0; }

      M->x = (double*)array_data(array_data_);

      bool alloc_p = false;
      %SAFE_CAST_INT(indptr_, nrows + 1, M->p, array_p_, array_p_ctrl_, alloc_p);
      if (alloc_p) { *alloc_ctrl |= ALLOC_CTRL_P; };
      bool alloc_i = false;
      %SAFE_CAST_INT(indices_, nzmax, M->i, array_i_, array_i_ctrl_, alloc_i);
      if (alloc_i) { *alloc_ctrl |= ALLOC_CTRL_I; };

      return 1;
    }
#endif /* WITH_CSR */

    res = PyObject_CallMethodObjArgs(scipy_mod, PyString_FromString("isspmatrix_coo"), obj, NULL);
    bool is_coo = (res == Py_True);
    Py_DECREF(res);

    PyObject* coo;
    int coo_new_alloc;
    if (!is_coo)
    {
      PyErr_Warn(PyExc_UserWarning, "Performance warning: the given sparse matrix is neither csc or coo, we have to perform a conversion to coo");
      coo = PyObject_CallMethodObjArgs(scipy_mod, PyString_FromString("coo_matrix"), obj, NULL);
      if (!coo) { if (!PyErr_Occurred()) { PyErr_SetString(PyExc_RuntimeError, "Conversion to coo failed!"); }; return 0; }
      coo_new_alloc = 1;
    }
    else
    {
      coo = obj;
      coo_new_alloc = 0;
    }

    // triplet
    PyObject* nnz_ = PyObject_GetAttrString(coo, "nnz");
    size_t nnz = PyInt_AsLong(nnz_);
    Py_DECREF(nnz_);

    CSparseMatrix* M = cs_spalloc(nrows, ncols, nnz, 1, 1);
    *m = M;
    M->nz = nnz;

    if(!M) { PyErr_SetString(PyExc_RuntimeError, "Allocation of the triplet matrix failed"); return 0; }

    PyObject* data_ = PyObject_GetAttrString(coo, "data");
    PyObject* row_ = PyObject_GetAttrString(coo, "row");
    PyObject* col_ = PyObject_GetAttrString(coo, "col");

    array_data_ = obj_to_array_allow_conversion(data_, NPY_DOUBLE, array_data_ctrl_);
    if (!array_data_) { PyErr_SetString(PyExc_RuntimeError, "Could not get a pointer to the data array"); return 0; }

    M->x = (double*)array_data(array_data_);

    bool alloc_p = false;
    %SAFE_CAST_INT(col_, nnz, M->p, array_p_, array_p_ctrl_, alloc_p);
    if (alloc_p) { *alloc_ctrl |= ALLOC_CTRL_P; };
    bool alloc_i = false;
    %SAFE_CAST_INT(row_, nnz, M->i, array_i_, array_i_ctrl_, alloc_i);
    if (alloc_i) { *alloc_ctrl |= ALLOC_CTRL_I; };

    if (coo_new_alloc)
    {
      Py_DECREF(coo);
    }

    return 1;
  }
  }

  int NM_convert_from_scipy_sparse(PyObject* obj, NumericsMatrix* m, PyArrayObject* array_data_, int* array_data_ctrl_, PyArrayObject* array_i_, int* array_i_ctrl_, PyArrayObject* array_p_, int* array_p_ctrl_, int* alloc_ctrl)
  {
    CSparseMatrix* csm = NULL;
    int res = cs_convert_from_scipy_sparse(obj, &csm, array_data_, array_data_ctrl_, array_i_, array_i_ctrl_, array_p_, array_p_ctrl_, alloc_ctrl);
    if (res > 0)
    {
      m->storageType = NM_SPARSE;

      if (csm->nz > 0)
      {
        m->matrix2->triplet = csm;
      }
      else if (csm->nz == -1)
      {
        m->matrix2->csc = csm;
      }
      else if (csm->nz == -2)
      {
        m->matrix2->csr = csm;
      }
      else
      {
        PyErr_SetString(PyExc_RuntimeError, "Unknown CSparseMatrix from cs_convert_from_scipy_sparse");
        return 0;
      }

      NM_update_size(m);
    }

    return res;
  }


  NumericsMatrix* NM_convert_from_python(PyObject* obj, NumericsMatrix* tmpmat, PyArrayObject* array_data_, int* array_ctrl, PyArrayObject* array_i_, int* array_i_ctrl_, PyArrayObject* array_p_, int* array_p_ctrl_, int* alloc_ctrl)
  {
  void* argp = NULL;
  NumericsMatrix* out = NULL;
  int res = SWIG_ConvertPtr(obj, &argp, $descriptor(NumericsMatrix *), %convertptr_flags);
  if (SWIG_IsOK(res))
  {
    out = %reinterpret_cast(argp, NumericsMatrix *);
  }
  else
  {
    tmpmat = newNumericsMatrix();
    int sp_conv = NM_convert_from_scipy_sparse(obj, tmpmat, array_data_, array_ctrl, array_i_, array_i_ctrl_, array_p_, array_p_ctrl_, alloc_ctrl);
    if (!sp_conv) { return NULL; }
    else if (sp_conv < 0)
    {
      array_data_ = obj_to_array_fortran_allow_conversion(obj, NPY_DOUBLE, array_ctrl);

      if (!array_data_ || !require_dimensions(array_data_, 2) ||
          !require_native(array_data_) || !require_fortran(array_data_)) return NULL;

      tmpmat->storageType = NM_DENSE;
      tmpmat->size0 =  array_size(array_data_, 0);
      tmpmat->size1 =  array_size(array_data_, 1);
      tmpmat->matrix0 = (double *)array_data(array_data_);
    }

    out = tmpmat;
  }

  return out;

  }

PyObject* cs_sparse_to_csr_matrix(cs_sparse *M)
{
  CS_TO_SCIPY(csr, M->m);
}

PyObject* cs_sparse_to_csc_matrix(cs_sparse *M)
{
  CS_TO_SCIPY(csc, M->n);
}

PyObject* cs_sparse_to_coo_matrix(cs_sparse *M)
{
  if (!M)
  {
    Py_INCREF(Py_None);
    return Py_None;
  }
  else
  {
    /* get sys.modules dict */
    PyObject* sys_mod_dict = PyImport_GetModuleDict();

    /* get the csr module object */
    PyObject* scipy_mod = PyMapping_GetItemString(sys_mod_dict, (char *)"scipy.sparse.coo");\

    if (!scipy_mod)
    {
      PyErr_SetString(PyExc_RuntimeError, "Did you import scipy.sparse.coo?");
      return NULL;
    }

    npy_intp this_M_x_dims[1] = { M->nz };
    npy_intp this_M_i_dims[1] = { M->nz };
    npy_intp this_M_p_dims[1] = { M->nz };

    PyObject* out_data = PyArray_SimpleNewFromData(1,this_M_x_dims,NPY_DOUBLE,M->x);
    if(!out_data) { PyErr_SetString(PyExc_RuntimeError, "Could not extract M->x"); return NULL; };

    PyObject* col_indices = PyArray_SimpleNewFromData(1,this_M_i_dims, NPY_INT_TYPE, M->i);
    if(!col_indices) {  PyErr_SetString(PyExc_RuntimeError, "Could not extract M->i"); return NULL; };

    PyObject* row_indices = PyArray_SimpleNewFromData(1,this_M_p_dims, NPY_INT_TYPE,M->p);
    if(!row_indices) {  PyErr_SetString(PyExc_RuntimeError, "Could not extract M->p"); return NULL; };

    PyObject* out_shape = PyTuple_Pack(2, PyInt_FromLong(M->m), PyInt_FromLong(M->n));
    if(!out_shape) {  PyErr_SetString(PyExc_RuntimeError, "Could not extract M->m or M->n"); return NULL; };

    PyObject* out_nnz = PyInt_FromLong(M->nz);
    if(!out_nnz) {  PyErr_SetString(PyExc_RuntimeError, "Could not extract M->nz"); return NULL; };

    /* call the class inside the csr module */
    PyObject* out_mat = PyObject_CallMethodObjArgs(scipy_mod, PyString_FromString((char *) "coo_matrix"), out_shape, NULL);

    if(out_mat)
    {
      PyObject_SetAttrString(out_mat, "data", out_data);
      PyObject_SetAttrString(out_mat, "row", row_indices);
      PyObject_SetAttrString(out_mat, "col", col_indices);

#ifndef NDEBUG
      PyObject *auto_nnz = PyObject_GetAttrString(out_mat, "nnz");
      assert(PyInt_AsLong(auto_nnz) == M->nz);
      Py_XDECREF(auto_nnz);
#endif

      return out_mat;
    }
    else
    {
      PyErr_SetString(PyExc_RuntimeError, "Could not create coo matrix");
      return NULL;
    }

  }

}

  PyObject* NM_to_python(NumericsMatrix* m)
  {
  if (m)
  {
    npy_intp dims[2];
    dims[0] = m->size0;
    dims[1] = m->size1;
    if (m->matrix0)
    {
      PyObject *obj = PyArray_SimpleNewFromData(2, dims, NPY_DOUBLE, m->matrix0);
      PyArrayObject *array = (PyArrayObject*) obj;
      if (!array || !require_fortran(array)) { return NULL; }
      return obj;
    }
    else if(m->matrix1)
    {
      // matrix is sparse : return opaque pointer
      return SWIG_NewPointerObj(SWIG_as_voidptr(m->matrix1), $descriptor(SparseBlockStructuredMatrix *), 0);
    }
    else if(m->matrix2)
    {
      switch(m->matrix2->origin)
      {
      case NS_CSC:
      {
       return cs_sparse_to_csc_matrix(NM_csc(m));
      }
      case NS_TRIPLET:
      {
        return cs_sparse_to_coo_matrix(NM_triplet(m));
      }
      case NS_CSR:
      {
        return cs_sparse_to_csr_matrix(NM_csr(m));
      }
      default:
      {
        PyErr_SetString(PyExc_RuntimeError, "The given sparse matrix has no valid origin. Please file a bug");
        return NULL;
      }
      }
    }
    else
    {
      PyErr_SetString(PyExc_RuntimeError, "The given matrix is of unknown type. Please file a bug");
      return NULL;
    }
  }
  else
  {
     Py_INCREF(Py_None);
     return  Py_None;
  }
  }

  void NM_clean_cs(CSparseMatrix* m, int alloc_ctrl)
  {
    assert(m);
    if (alloc_ctrl & ALLOC_CTRL_P) { assert(m->p); free(m->p); m->p = NULL; }
    if (alloc_ctrl & ALLOC_CTRL_I) { assert(m->i); free(m->i); m->i = NULL; }
  }

  int NM_clean(NumericsMatrix* M, int alloc_ctrl)
  {
    if (M->storageType == NM_SPARSE)
    {
      assert(M->matrix2);
      switch (M->matrix2->origin)
      {
      case NS_CSC:
      {
        NM_clean_cs(M->matrix2->csc, alloc_ctrl);
        break;
      }
      case NS_CSR:
      {
        NM_clean_cs(M->matrix2->csr, alloc_ctrl);
        break;
      }
      case NS_TRIPLET:
      {
        NM_clean_cs(M->matrix2->triplet, alloc_ctrl);
        break;
      }
      default:
      {
        PyErr_SetString(PyExc_RuntimeError, "The origin of the sparse matrix is unknown!");
        return 0;
      }
      }
    }
    return 1;

  }


} // end fragment NumericsMatrix


// Numpy array -> NumericsMatrix
%typemap(in, fragment="NumericsMatrix") (NumericsMatrix* A) 
 // free in typemap(freearg)
 (PyArrayObject* array_ = NULL,
 int array_ctrl_ = 0,
 PyArrayObject* array_i_ = NULL,
 int array_i_ctrl_ = 0,
 PyArrayObject* array_p_ = NULL,
 int array_p_ctrl_ = 0,
 int alloc_ctrl_ = 0,
 NumericsMatrix *nummat = NULL)
{
   $1 = NM_convert_from_python($input, nummat, array_, &array_ctrl_, array_i_, &array_i_ctrl_, array_p_, &array_p_ctrl_, &alloc_ctrl_);
}


%typemap(memberin) (NumericsMatrix* A) {
 //  %typemap(memberin) (NumericsMatrix* A)
 // perform a deep copy
 if (!$1) { $1 = createNumericsMatrix($input->storageType, $input->size0, $input->size1); }
 NM_copy($input, $1);
}

%typemap(freearg) (NumericsMatrix* A) {
  // %typemap(freearg) (NumericsMatrix* A)
  if (nummat$argnum) { free(nummat$argnum); }
  if (array_ctrl_$argnum && array_$argnum) { Py_DECREF(array_$argnum); }
  if (array_i_ctrl_$argnum && array_i_$argnum) { Py_DECREF(array_i_$argnum); }
  if (array_p_ctrl_$argnum && array_p_$argnum) { Py_DECREF(array_p_$argnum); }

  if (!NM_clean(nummat$argnum, alloc_ctrl_$argnum)) { return NULL;} 

}

%typemap(out, fragment="NumericsMatrix") (NumericsMatrix* A) {
  $result = NM_to_python($1);
  if (!$result) SWIG_fail;
}

%typemap(freearg) (double *z)
{
 
}


%typemap(out) (double* q) {
  npy_intp dims[1] = { arg1->size };

  if ($1)
  {
    PyObject *obj = PyArray_SimpleNewFromData(1, dims, NPY_DOUBLE, $1);
    PyArrayObject *array = (PyArrayObject*) obj;
    if (!array || !require_fortran(array)) SWIG_fail;
    $result = obj;
  }
  else
   {
     Py_INCREF(Py_None);
     $result = Py_None;
   }
 }

%apply (NumericsMatrix *A) { (NumericsMatrix *m) };
%apply (NumericsMatrix *A) { (NumericsMatrix *M) };
%apply (NumericsMatrix *A) { (NumericsMatrix *H) };

// SBM handling

%typemap(in) (const SparseBlockStructuredMatrix* const A, double *denseMat)
(npy_intp dims[2])
{
  int res1=0;
  void* temp = NULL;
  res1 = SWIG_ConvertPtr($input, &temp, $1_descriptor, 0 |  0);
  if (!SWIG_IsOK(res1))
  {
    SWIG_exception_fail(SWIG_ArgError(res1), 
                        "in method '" 
                        " $symname "
                        "', argument " "1"" of type '" "SparseBlockStructuredMatrix *""'");
  };
  SparseBlockStructuredMatrix* A = (SparseBlockStructuredMatrix *) 0;
  A = reinterpret_cast< SparseBlockStructuredMatrix * >(temp);
  assert(A);
  dims[0] = A->blocksize0[A->blocknumber0-1];
  dims[1] = A->blocksize0[A->blocknumber0-1];
  $1 = A;

  $2 = (double *) malloc(dims[0] * dims[1] * sizeof(double));
}

// FIX: do not work
%newobject SBMtoDense(const SparseBlockStructuredMatrix* const A, double *denseMat);
%typemap(newfree) (double *denseMat)
{
  // %typemap(newfree) (double *denseMat)
  if($1)
  {
    free($1);
  }
}


%typemap(argout) (double *denseMat) 
{
  
  PyObject* pyarray = FPyArray_SimpleNewFromData(2,                
                                                 dims1,                  
                                                 NPY_DOUBLE,            
                                                 $1);
  $result = pyarray;
}

// conversion python string -> FILE
%typemap(in) (FILE *file=NULL)
{
  // %typemap(in) (FILE *file)
  %#if PY_MAJOR_VERSION < 3
  $1 = fopen(PyString_AsString($input), "r");
  %#else
  PyObject* tmp_ascii = PyUnicode_AsASCIIString($input);
  $1 = fopen(PyBytes_AsString(tmp_ascii), "r");
  Py_DECREF(tmp_ascii);
  %#endif
  if (!$1)
  {
    SWIG_exception_fail(SWIG_IOError, "in method '" "$symname" "' cannot fopen file");
  }
}

%typemap(freearg) (FILE *file)
{
  // %typemap(freearg) (FILE *file)
  if($1)
  {
    fclose($1);
  }
}

%typemap(in, numinputs=0) (SparseBlockStructuredMatrix* outSBM) 
{
  $1 = (SparseBlockStructuredMatrix*) malloc(sizeof(SparseBlockStructuredMatrix));
  if(!$1) SWIG_fail;

  $1->block = NULL;
  $1->index1_data = NULL;
  $1->index2_data = NULL;

}

%typemap(argout) (SparseBlockStructuredMatrix* outSBM)
{
  if(!$1) SWIG_fail;

  $result = SWIG_Python_AppendOutput($result,
                                     SWIG_NewPointerObj(SWIG_as_voidptr($1), $1_descriptor, 0));
}



%typemap(in) (const SparseBlockStructuredMatrix* const A, cs_sparse *outSparseMat)
{
  void *swig_arp;
  int swig_res = SWIG_ConvertPtr($input,&swig_arp,$1_descriptor, 0 | 0);

  if (SWIG_IsOK(swig_res))
  {
    $1 = (SparseBlockStructuredMatrix*) swig_arp;
    $2 = (cs_sparse*) malloc(sizeof(cs_sparse));
    if(!$2) SWIG_fail;

    SBMtoSparseInitMemory($1,$2);
  }
  else
    SWIG_fail;
}


%typemap(argout) (cs_sparse *outSparseMat)
{
  PyObject* csrm = cs_sparse_to_csr_matrix($1);
  if (!csrm) { SWIG_fail; }
  $result = SWIG_Python_AppendOutput($result, csrm);
}

%typemap(out) (cs_sparse *)
{
  $result = cs_sparse_to_csc_matrix($1);
  if (!$result) { SWIG_fail; }
}

%typemap(in) (cs_sparse* M)
   (int array_data_ctrl_ = 0,
   int array_i_ctrl_ = 0,
   int array_p_ctrl_ = 0,
   PyArrayObject *array_data_ = NULL,
   PyArrayObject *array_i_ = NULL,
   PyArrayObject *array_p_ = NULL,
   int alloc_ctrl_ = 0,
   cs_sparse *M = NULL)
{
  try
  {
    M = (cs_sparse *) malloc(sizeof(cs_sparse));
    if(!M) { PyErr_SetString(PyExc_RuntimeError, "Failed to allocate a cs_sparse"); SWIG_fail; }

    int res = cs_convert_from_scipy_sparse($input, &M, array_data_, &array_data_ctrl_, array_i_, &array_i_ctrl_, array_p_, &array_p_ctrl_, &alloc_ctrl_);

    if (!res) { SWIG_fail; }
    else if (res < 0) { PyErr_SetString(PyExc_RuntimeError, "Error the matrix is not sparse!"); SWIG_fail; }

    $1 = M;
  }
  catch (const std::invalid_argument& e)
  {
    SWIG_exception(SWIG_ValueError, e.what());
  }
}

%typemap(freearg) (cs_sparse* M)
{

  if (array_data_$argnum && array_data_ctrl_$argnum) { Py_DECREF(array_data_$argnum); }

  if (array_i_$argnum && array_i_ctrl_$argnum) { Py_DECREF(array_i_$argnum); }

  if (array_p_$argnum && array_p_ctrl_$argnum) { Py_DECREF(array_p_$argnum); }

  NM_clean_cs(M$argnum, alloc_ctrl_$argnum);

  if(M$argnum) { cs_spfree(M$argnum); }
}

%apply (cs_sparse *M) {(const cs_sparse const * m)};

%apply (cs_sparse *M) {(cs_sparse const * m)};

%apply (cs_sparse *M) {(const cs_sparse * m)};

%apply (cs_sparse *M) {(cs_sparse * m)};

%apply (cs_sparse *M) {(const cs_sparse const *sparseMat)};

%apply (cs_sparse *M) {(cs_sparse *sparseMat)};


%inline
%{
  void getSBM(SparseBlockStructuredMatrix* M, SparseBlockStructuredMatrix* outSBM)
  {
    outSBM=M;
  }
%}

// issue with typemap out and is useless for now
// convert matrix to scipy.sparse.csc and do the job there
%ignore SBMRowToDense;
