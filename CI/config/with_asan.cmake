# from default, test solvers with sanitizer
include(CI/config/default.cmake)
set_option(USE_SANITIZER asan)
