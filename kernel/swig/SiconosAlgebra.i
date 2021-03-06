// The SiconosAlgebra classes are handled seperately from the other Kernel classes
// This is because we define typemaps for them. It look like swig forgets some typemaps
// after being told more about a class (more specifically after applying PY_REGISTER_WITHOUT_DIRECTOR)
// Hence, we declare them fully here, and just after we define the typemaps (note the %include KernelTypes.i at the end)


%include KernelTypes.i

%include SiconosMatrix.hpp
%include SimpleMatrix.hpp
%include SiconosVector.hpp
%include BlockVector.hpp
