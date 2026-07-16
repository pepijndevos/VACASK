#ifndef __ASA241_DEFINED
#define __ASA241_DEFINED

#include "common.h"

// Wichura’s Algorithm AS 241, inverse CDF of standard normal distribution N(0,1)
// GNU LGPL license

namespace NAMESPACE {

void normal_01_cdf_values ( int *n_data, double *x, double *fx );
float r4_normal_01_cdf_inverse ( float p );
double r8_normal_01_cdf_inverse ( double p );

}

#endif
