/* File: projection.c */
/*
  This file is an extenstion of the Corrfunc package
  Copyright (C) 2020-- Kate Storey-Fisher (kstoreyfisher@gmail.com)
  License: MIT LICENSE. See LICENSE file under the top-level
  directory at https://github.com/kstoreyf/Corrfunc/
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "projection.h" //function proto-type for API
#include "proj_functions_double.h"//actual implementations for double
#include "proj_functions_float.h"//actual implementations for float


int compute_amplitudes(int nprojbins, int nd1, int nd2, int nr1, int nr2,
            void *dd, void *dr, void *rd, void *rr, void *qq, void *amps, size_t element_size)
{
    if( ! (element_size == sizeof(float) || element_size == sizeof(double))){
        fprintf(stderr,"ERROR: In %s> Can only handle doubles or floats. Got an array of size = %zu\n",
                __FUNCTION__, element_size);
        return EXIT_FAILURE;
    }
    if(element_size == sizeof(float)) {
        return compute_amplitudes_float(nprojbins, nd1, nd2, nr1, nr2,
            (float *) dd, (float *) dr, (float *) rd, (float *) rr, (float *) qq, (float *) amps);
    } else {
        return compute_amplitudes_double(nprojbins, nd1, nd2, nr1, nr2,
            (double *) dd, (double *) dr, (double *) rd, (double *) rr, (double *) qq, (double *) amps);
    }
}


int evaluate_xi(int nprojbins, void *amps, int nsvals, void *svals,
                      int nsbins, void *sbins, void *xi, proj_method_t proj_method, size_t element_size, char *projfn)
{
    if( ! (element_size == sizeof(float) || element_size == sizeof(double))){
        fprintf(stderr,"ERROR: In %s> Can only handle doubles or floats. Got an array of size = %zu\n",
                __FUNCTION__, element_size);
        return EXIT_FAILURE;
    }

    if(element_size == sizeof(float)) {
        return evaluate_xi_float(nprojbins, (float *) amps, nsvals, (float *) svals,
                      nsbins, (float *) sbins, (float *) xi, proj_method, projfn);
    } else {
        return evaluate_xi_double(nprojbins, (double *) amps, nsvals, (double *) svals,
                      nsbins, (double *) sbins, (double *) xi, proj_method, projfn);
    }
}



int qq_analytic(double rmin, double rmax, int nd, double volume, int nprojbins, 
        int nsbins, void *sbins, void *rr, void *qq, proj_method_t proj_method, 
        size_t element_size, char *projfn)
{
    if( ! (element_size == sizeof(float) || element_size == sizeof(double))){
        fprintf(stderr,"ERROR: In %s> Can only handle doubles or floats. Got an array of size = %zu\n",
                __FUNCTION__, element_size);
        return EXIT_FAILURE;
    }

    if(element_size == sizeof(float)) {
        return qq_analytic_float((float) rmin, (float) rmax, nd, (float) volume,
                    nprojbins, nsbins, (float *) sbins, (float *) rr, (float *) qq,
                    proj_method, projfn);
    } else {
        return qq_analytic_double((double) rmin, (double) rmax, nd, (double) volume,
                    nprojbins, nsbins, (double *) sbins, (double *) rr, (double *) qq,
                    proj_method, projfn);
    }
}
