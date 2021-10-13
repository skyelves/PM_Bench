set(RNG_SOURCES rng.cpp rng.h)

add_library(pmbench-rng STATIC ${RNG_SOURCES})
target_compile_features(pmbench-rng PUBLIC c_std_11 cxx_std_17)