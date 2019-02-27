/* This file is auto-generated from countpairs_s_mu_mocks_impl.c.src */
#ifndef DOUBLE_PREC
#define DOUBLE_PREC
#endif
// # -*- mode: c -*-
/* File: countpairs_s_mu_mocks_impl.c.src */
/*
  This file is a part of the Corrfunc package
  Copyright (C) 2015-- Manodeep Sinha (manodeep@gmail.com)
  License: MIT LICENSE. See LICENSE file under the top-level
  directory at https://github.com/manodeep/Corrfunc/
*/

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <gsl/gsl_interp.h>


#include "countpairs_s_mu_mocks_impl_double.h"
#include "countpairs_s_mu_mocks_kernels_double.c"
#include "cellarray_mocks_double.h"
#include "gridlink_mocks_impl_double.h"

#include "defs.h"
#include "utils.h"
#include "cosmology_params.h"
#include "set_cosmo_dist.h"
#include "cpu_features.h"
#include "progressbar.h"
#include "proj_functions_double.h"

#if defined(_OPENMP)
#include <omp.h>
#endif

int interrupt_status_DDsmu_mocks_double=EXIT_SUCCESS;

void interrupt_handler_countpairs_s_mu_mocks_double(int signo)
{
    fprintf(stderr,"Received signal = `%s' (signo = %d). Aborting \n",strsignal(signo), signo);
    interrupt_status_DDsmu_mocks_double = EXIT_FAILURE;
}


int check_ra_dec_cz_s_mu_double(const int64_t N, double *phi, double *theta, double *cz)
{

    if(N==0) {
        return EXIT_SUCCESS;
    }
    if(phi == NULL || theta == NULL || cz == NULL) {
        fprintf(stderr,"Input arrays can not be NULL. Have RA = %p DEC = %p cz = %p\n", phi, theta, cz);
        return EXIT_FAILURE;
    }

    int fix_cz  = 0;
    int fix_ra  = 0;
    int fix_dec = 0;

    const double max_cz_threshold = 10.0;//if I find that max cz is smaller than this threshold, then I will assume z has been supplied rather than cz
    double max_cz = 0.0;
    //Check input cz -> ensure that cz contains cz and not z
    for(int64_t i=0;i<N;i++) {
        if(cz[i] > max_cz) max_cz = cz[i];
        if(phi[i] < 0.0) {
            fix_ra = 1;
        }
        if(theta[i] > 90.0) {
            fix_dec = 1;
        }
        if(theta[i] > 180) {
            fprintf(stderr,"theta[%"PRId64"] = %"REAL_FORMAT"should be less than 180 deg\n", i, theta[i]);
            return EXIT_FAILURE;
        }
    }
    if(max_cz < max_cz_threshold) fix_cz = 1;

    //Only run the loop if something needs to be fixed
    if(fix_cz==1 || fix_ra == 1 || fix_dec == 1) {
        if(fix_ra == 1) {
            fprintf(stderr,"%s> Out of range values found for ra. Expected ra to be in the range [0.0,360.0]. Found ra values in [-180,180] -- fixing that\n", __FUNCTION__);
        }
        if(fix_dec == 1) {
            fprintf(stderr,"%s> Out of range values found for dec. Expected dec to be in the range [-90.0,90.0]. Found dec values in [0,180] -- fixing that\n", __FUNCTION__);
        }
        if(fix_cz == 1)  {
            fprintf(stderr,"%s> Out of range values found for cz. Expected input to be `cz' but found `z' instead. max_cz (found in input) = %"REAL_FORMAT" threshold "
                    "= %"REAL_FORMAT"\n",__FUNCTION__,max_cz,max_cz_threshold);
        }

        for(int64_t i=0;i<N;i++) {
            if(fix_ra==1) {
                phi[i] += (double) 180.0;
            }
            if(fix_dec==1) {
                theta[i] -= (double) 90.0;
            }
            if(fix_cz == 1) {
                cz[i] *= (double) SPEED_OF_LIGHT;//input was z -> convert to cz
            }
        }
    }

    return EXIT_SUCCESS;
}


countpairs_mocks_func_ptr_double countpairs_s_mu_mocks_driver_double(const struct config_options *options)
{

    static countpairs_mocks_func_ptr_double function = NULL;
    static isa old_isa=-1;
    if(old_isa == options->instruction_set) {
        return function;
    }

    /* Array of function pointers */
    countpairs_mocks_func_ptr_double allfunctions[] = {
#ifdef __AVX__
        countpairs_s_mu_mocks_avx_intrinsics_double,
#endif
#ifdef __SSE4_2__
      countpairs_s_mu_mocks_sse_intrinsics_double,
#endif
      countpairs_s_mu_mocks_fallback_double
    };

    const int num_functions = sizeof(allfunctions)/sizeof(void *);
    const int fallback_offset = num_functions - 1;
#if defined(__AVX__) || defined __SSE4_2__
    const int highest_isa = instrset_detect();
#endif
    int curr_offset = 0;

    /* Now check if AVX is supported by the CPU */
    int avx_offset = fallback_offset;
#ifdef __AVX__
    avx_offset = highest_isa >= 7 ? curr_offset:fallback_offset;
    curr_offset++;
#endif

    /* Is the SSE function supported at runtime and enabled at compile-time?*/
    int sse_offset = fallback_offset;
#ifdef __SSE4_2__
    sse_offset = highest_isa >= 6 ? curr_offset:fallback_offset;
    curr_offset++;
#endif
    if( curr_offset != fallback_offset) {
      fprintf(stderr,"ERROR: Bug in code (current offset = %d *should equal* fallback function offset = %d)\n",
              curr_offset, fallback_offset);
      return NULL;
    }

    int function_dispatch=0;
    /* Check that cpu supports feature */
    if(options->instruction_set >= 0) {
        switch(options->instruction_set) {
        case(AVX512F):
        case(AVX2):
        case(AVX):function_dispatch=avx_offset;break;
        case(SSE42): function_dispatch=sse_offset;break;
        default:function_dispatch=fallback_offset;break;
        }
    }

    if(function_dispatch >= num_functions) {
      fprintf(stderr,"In %s> ERROR: Could not resolve the correct function.\n Function index = %d must lie between [0, %d)\n",
              __FUNCTION__, function_dispatch, num_functions);
      return NULL;
    }
    function = allfunctions[function_dispatch];
    old_isa = options->instruction_set;

    if(options->verbose){
        // This must be first (AVX/SSE may be aliased to fallback)
        if(function_dispatch == fallback_offset){
            fprintf(stderr,"Using fallback kernel\n");
        } else if(function_dispatch == avx_offset){
            fprintf(stderr,"Using AVX kernel\n");
        } else if(function_dispatch == sse_offset){
            fprintf(stderr,"Using SSE kernel\n");
        } else {
            printf("Unknown kernel!\n");
        }
    }

    return function;
}


int countpairs_mocks_s_mu_double(const int64_t ND1, double *ra1, double *dec1, double *czD1,
                                 const int64_t ND2, double *ra2, double *dec2, double *czD2,
                                 const int numthreads,
                                 const int autocorr,
                                 const char *sbinfile,
                                 const double max_mu,
                                 const int nmu_bins,
                                 const int cosmology,
                                 results_countpairs_mocks_s_mu *results,
                                 struct config_options *options, struct extra_options *extra)
{

    if(options->float_type != sizeof(double)) {
        fprintf(stderr,"ERROR: In %s> Can only handle arrays of size=%zu. Got an array of size = %zu\n",
                __FUNCTION__, sizeof(double), options->float_type);
        return EXIT_FAILURE;
    }

    // If no extra options were passed, create dummy options
    // This allows us to pass arguments like "extra->weights0" below;
    // they'll just be NULLs, which is the correct behavior
    struct extra_options dummy_extra;
    if(extra == NULL){
      weight_method_t dummy_method = NONE;
      dummy_extra = get_extra_options(dummy_method);
      extra = &dummy_extra;
    }

    int need_weightavg = extra->weight_method != NONE;

    options->sort_on_z = 1;
    struct timeval t0;
    if(options->c_api_timer) {
        gettimeofday(&t0, NULL);
    }
    if(options->fast_divide_and_NR_steps >= MAX_FAST_DIVIDE_NR_STEPS) {
        fprintf(stderr, ANSI_COLOR_MAGENTA"Warning: The number of requested Newton-Raphson steps = %u is larger than max. allowed steps = %u."
                " Switching to a standard divide"ANSI_COLOR_RESET"\n",
                options->fast_divide_and_NR_steps, MAX_FAST_DIVIDE_NR_STEPS);
        options->fast_divide_and_NR_steps = 0;
    }

    //Check inputs
    if(ND1 == 0 || (autocorr == 0 && ND2 == 0)) {
        return EXIT_SUCCESS;
    }

    //Check inputs
    int status1 = check_ra_dec_cz_s_mu_double(ND1, ra1, dec1, czD1);
    if(status1 != EXIT_SUCCESS) {
        return status1;
    }
    if(autocorr==0) {
        int status2 = check_ra_dec_cz_s_mu_double(ND2, ra2, dec2, czD2);
        if(status2 != EXIT_SUCCESS) {
            return status2;
        }
    }

#if defined(_OPENMP)
    omp_set_num_threads(numthreads);
#else
    (void) numthreads;
#endif

    if(options->max_cells_per_dim == 0) {
        fprintf(stderr,"Warning: Max. cells per dimension is set to 0 - resetting to `NLATMAX' = %d\n", NLATMAX);
        options->max_cells_per_dim = NLATMAX;
    }
    for(int i=0;i<3;i++) {
        if(options->bin_refine_factors[i] < 1) {
            fprintf(stderr,"Warning: bin refine factor along axis = %d *must* be >=1. Instead found bin refine factor =%d\n",
                    i, options->bin_refine_factors[i]);
            reset_bin_refine_factors(options);
            break;/* all factors have been reset -> no point continuing with the loop */
        }
    }

    /* setup interrupt handler -> mostly useful during the python execution.
       Let's Ctrl-C abort the extension  */
    SETUP_INTERRUPT_HANDLERS(interrupt_handler_countpairs_s_mu_mocks_double);

    //Try to initialize cosmology - code will exit if comoslogy is not implemented.
    //Putting in a different scope so I can call the variable status
    {
        int status = init_cosmology(cosmology);
        if(status != EXIT_SUCCESS) {
            return status;
        }
    }

    /***********************
     *initializing the  bins
     ************************/
    double *supp;
    int nsbin;
    double smin,smax;
    setup_bins(sbinfile,&smin,&smax,&nsbin,&supp);
    if( ! (smin > 0.0 && smax > 0.0 && smin < smax && nsbin > 0)) {
        fprintf(stderr,"Error: Could not setup with S bins correctly. (smin = %lf, smax = %lf, with nbins = %d). Expected non-zero smin/smax with smax > smin and nbins >=1 \n",
                smin, smax, nsbin);
        return EXIT_FAILURE;
    }


    if(max_mu <= 0.0 || max_mu > 1.0) {
        fprintf(stderr,"Error: max_mu (max. value for the cosine of the angle with line of sight) must be greater than 0 and at most 1).\n"
                "The passed value is max_mu = %lf. Please change it to be > 0 and <= 1.0\n", max_mu);
        return EXIT_FAILURE;
    }

    if(nmu_bins < 1 ) {
        fprintf(stderr,"Error: Number of mu bins = %d must be at least 1\n", nmu_bins);
        return EXIT_FAILURE;
    }

    //Change cz into co-moving distance
    double *D1 = NULL, *D2 = NULL;
    if(options->is_comoving_dist == 0) {
        D1 = my_malloc(sizeof(*D1),ND1);
        D2 = autocorr == 0 ? my_malloc(sizeof(*D2),ND2):D1;
    } else {
        D1 = czD1;
        D2 = autocorr == 0 ? czD2:czD1;
    }

    if(D1 == NULL || D2 == NULL) {
        free(D1);free(D2);
        return EXIT_FAILURE;
    }


    if(options->is_comoving_dist == 0) {
        //Setup variables to do the cz->comoving distance
        double czmax = 0.0;
        const double inv_speed_of_light = 1.0/SPEED_OF_LIGHT;
        get_max_double(ND1, czD1, &czmax);
        if(autocorr == 0) {
            get_max_double(ND2, czD2, &czmax);
        }
        const double zmax = czmax * inv_speed_of_light + 0.01;

        const int workspace_size = 10000;
        double *interp_redshift  = my_calloc(sizeof(*interp_redshift), workspace_size);//the interpolation is done in 'z' and not in 'cz'
        double *interp_comoving_dist = my_calloc(sizeof(*interp_comoving_dist),workspace_size);
        int Nzdc = set_cosmo_dist(zmax, workspace_size, interp_redshift, interp_comoving_dist, cosmology);
        if(Nzdc < 0) {
            free(interp_redshift);free(interp_comoving_dist);
            return EXIT_FAILURE;
        }

        gsl_interp *interpolation;
        gsl_interp_accel *accelerator;
        accelerator =  gsl_interp_accel_alloc();
        interpolation = gsl_interp_alloc (gsl_interp_linear,Nzdc);
        gsl_interp_init(interpolation, interp_redshift, interp_comoving_dist, Nzdc);
        for(int64_t i=0;i<ND1;i++) {
            D1[i] = gsl_interp_eval(interpolation, interp_redshift, interp_comoving_dist, czD1[i]*inv_speed_of_light, accelerator);
        }

        if(autocorr==0) {
            for(int64_t i=0;i<ND2;i++) {
                D2[i] = gsl_interp_eval(interpolation, interp_redshift, interp_comoving_dist, czD2[i]*inv_speed_of_light, accelerator);
            }
        }
        free(interp_redshift);free(interp_comoving_dist);
        gsl_interp_free(interpolation);
        gsl_interp_accel_free(accelerator);
    }

    double *X1 = my_malloc(sizeof(*X1), ND1);
    double *Y1 = my_malloc(sizeof(*Y1), ND1);
    double *Z1 = my_malloc(sizeof(*Z1), ND1);
    if(X1 == NULL || Y1 == NULL || Z1 == NULL) {
        free(X1);free(Y1);free(Z1);
        return EXIT_FAILURE;
    }
    for(int64_t i=0;i<ND1;i++) {
        X1[i] = D1[i]*COSD(dec1[i])*COSD(ra1[i]);
        Y1[i] = D1[i]*COSD(dec1[i])*SIND(ra1[i]);
        Z1[i] = D1[i]*SIND(dec1[i]);
    }

    double *X2,*Y2,*Z2;
    if(autocorr==0) {
        X2 = my_malloc(sizeof(*X2), ND2);
        Y2 = my_malloc(sizeof(*Y2), ND2);
        Z2 = my_malloc(sizeof(*Z2), ND2);
        for(int64_t i=0;i<ND2;i++) {
            X2[i] = D2[i]*COSD(dec2[i])*COSD(ra2[i]);
            Y2[i] = D2[i]*COSD(dec2[i])*SIND(ra2[i]);
            Z2[i] = D2[i]*SIND(dec2[i]);
        }
    } else {
        X2 = X1;
        Y2 = Y1;
        Z2 = Z1;
    }

    double supp_sqr[nsbin];
    for(int i=0; i < nsbin;i++) {
        supp_sqr[i] = supp[i]*supp[i];
    }
    const double mu_max = (double) max_mu;

    double xmin=1e10,ymin=1e10,zmin=1e10;
    double xmax=-1e10,ymax=-1e10,zmax=-1e10;
    get_max_min_data_double(ND1, X1, Y1, Z1, &xmin, &ymin, &zmin, &xmax, &ymax, &zmax);

    if(autocorr==0) {
        get_max_min_data_double(ND2, X2, Y2, Z2, &xmin, &ymin, &zmin, &xmax, &ymax, &zmax);
    }

    const double xdiff = xmax-xmin;
    const double ydiff = ymax-ymin;
    const double zdiff = zmax-zmin;
    if(get_bin_refine_scheme(options) == BINNING_DFL) {
        if(smax < 0.05*xdiff) {
            options->bin_refine_factors[0] = 1;
      }
        if(smax < 0.05*ydiff) {
            options->bin_refine_factors[1] = 1;
        }
        if(smax < 0.05*zdiff) {
            options->bin_refine_factors[2] = 1;
        }
    }

    /*---Create 3-D lattice--------------------------------------*/
    int nmesh_x=0,nmesh_y=0,nmesh_z=0;
    cellarray_mocks_index_particles_double *lattice1 = gridlink_mocks_index_particles_double(ND1, X1, Y1, Z1, D1, &(extra->weights0),
                                                                                             xmin, xmax, ymin, ymax, zmin, zmax,
                                                                                             smax, smax, smax,
                                                                                             options->bin_refine_factors[0],
                                                                                             options->bin_refine_factors[1],
                                                                                             options->bin_refine_factors[2],
                                                                                             &nmesh_x, &nmesh_y, &nmesh_z,
                                                                                             options);
    if(lattice1 == NULL) {
        return EXIT_FAILURE;
    }

    /* If there too few cells (BOOST_CELL_THRESH is ~10), and the number of cells can be increased, then boost bin refine factor by ~1*/
    const double avg_np = ((double)ND1)/(nmesh_x*nmesh_y*nmesh_z);
    const int8_t max_nmesh = fmax(nmesh_x, fmax(nmesh_y, nmesh_z));
    if((max_nmesh <= BOOST_CELL_THRESH || avg_np >= BOOST_NUMPART_THRESH)
        && max_nmesh < options->max_cells_per_dim) {
      fprintf(stderr,"%s> gridlink seems inefficient. nmesh = (%d, %d, %d); avg_np = %.3g. ", __FUNCTION__, nmesh_x, nmesh_y, nmesh_z, avg_np);
      if(get_bin_refine_scheme(options) == BINNING_DFL) {
            fprintf(stderr,"Boosting bin refine factor - should lead to better performance\n");
            // Only boost the first two dimensions.  Prevents excessive refinement.
            for(int i=0;i<2;i++) {
              options->bin_refine_factors[i] += BOOST_BIN_REF;
            }

            free_cellarray_mocks_index_particles_double(lattice1, nmesh_x * (int64_t) nmesh_y * nmesh_z);
            lattice1 = gridlink_mocks_index_particles_double(ND1, X1, Y1, Z1, D1, &(extra->weights0),
                                                             xmin, xmax, ymin, ymax, zmin, zmax,
                                                             smax, smax, smax,
                                                             options->bin_refine_factors[0],
                                                             options->bin_refine_factors[1],
                                                             options->bin_refine_factors[2],
                                                             &nmesh_x, &nmesh_y, &nmesh_z,
                                                             options);
            if(lattice1 == NULL) {
                return EXIT_FAILURE;
            }
        } else {
            fprintf(stderr,"Boosting bin refine factor could have helped. However, since custom bin refine factors "
                  "= (%d, %d, %d) are being used - continuing with inefficient mesh\n", options->bin_refine_factors[0],
                  options->bin_refine_factors[1], options->bin_refine_factors[2]);

        }
    }

    cellarray_mocks_index_particles_double *lattice2 = NULL;
    if(autocorr==0) {
        int ngrid2_x=0,ngrid2_y=0,ngrid2_z=0;
        lattice2 = gridlink_mocks_index_particles_double(ND2, X2, Y2, Z2, D2, &(extra->weights1),
                                                         xmin, xmax,
                                                         ymin, ymax,
                                                         zmin, zmax,
                                                         smax, smax, smax,
                                                         options->bin_refine_factors[0],
                                                         options->bin_refine_factors[1],
                                                         options->bin_refine_factors[2],
                                                         &ngrid2_x, &ngrid2_y, &ngrid2_z, options);
        if(lattice2 == NULL) {
            return EXIT_FAILURE;
        }
        if( ! (nmesh_x == ngrid2_x && nmesh_y == ngrid2_y && nmesh_z == ngrid2_z) ) {
            fprintf(stderr,"Error: The two sets of 3-D lattices do not have identical bins. First has dims (%d, %d, %d) while second has (%d, %d, %d)\n",
                    nmesh_x, nmesh_y, nmesh_z, ngrid2_x, ngrid2_y, ngrid2_z);
            return EXIT_FAILURE;
        }
    } else {
        lattice2 = lattice1;
    }
    free(X1);free(Y1);free(Z1);
    if(autocorr == 0) {
        free(X2);free(Y2);free(Z2);
    }

    if(options->is_comoving_dist == 0) {
        free(D1);
        if(autocorr == 0) {
            free(D2);
        }
    }



    const int64_t totncells = (int64_t) nmesh_x * (int64_t) nmesh_y * (int64_t) nmesh_z;
    {
        int status = assign_ngb_cells_mocks_index_particles_double(lattice1, lattice2, totncells,
                                                                   options->bin_refine_factors[0], options->bin_refine_factors[1], options->bin_refine_factors[2],
                                                                   nmesh_x, nmesh_y, nmesh_z,
                                                                   autocorr);
        if(status != EXIT_SUCCESS) {
            free_cellarray_mocks_index_particles_double(lattice1, totncells);
            if(autocorr == 0) {
                free_cellarray_mocks_index_particles_double(lattice2, totncells);
            }
            free(supp);
            return EXIT_FAILURE;
        }
    }
    /*---Gridlink-variables----------------*/
    const int totnbins = (nmu_bins+1)*(nsbin+1);
    const int nprojbins = nsbin-1;
#if defined(_OPENMP)
    uint64_t **all_npairs = (uint64_t **) matrix_calloc(sizeof(uint64_t), numthreads, totnbins);
    double **all_savg = NULL;
    if(options->need_avg_sep){
        all_savg = (double **) matrix_calloc(sizeof(double),numthreads,totnbins);
    }
    double **all_weightavg = NULL;
    if(need_weightavg) {
      all_weightavg = (double **) matrix_calloc(sizeof(double),numthreads,totnbins);
    }
    double **all_projpairs = (double **) matrix_calloc(sizeof(double),numthreads,nprojbins);
    double **all_projpairs_tensor = (double **) matrix_calloc(sizeof(double),numthreads,nprojbins*nprojbins);


#else //USE_OMP
    uint64_t npairs[totnbins];
    double savg[totnbins], weightavg[totnbins], projpairs[nprojbins];
    double projpairs_tensor[nprojbins*nprojbins];

    for(int i=0; i <totnbins;i++) {
        npairs[i] = 0;
        if(options->need_avg_sep) {
            savg[i] = ZERO;
        }
        if(need_weightavg) {
            weightavg[i] = ZERO;
        }
    }
    for(int i=0;i<nprojbins;i++) {
        projpairs[i] = ZERO;
        for(int j=0;j<nprojbins;j++) {
            projpairs_tensor[i*nprojbins+j] = ZERO;
        }
    }
#endif //USE_OMP

    /* runtime dispatch - get the function pointer */
    countpairs_mocks_func_ptr_double countpairs_s_mu_mocks_function_double = countpairs_s_mu_mocks_driver_double(options);
    if(countpairs_s_mu_mocks_function_double == NULL) {
        return EXIT_FAILURE;
    }

    int interrupted=0,numdone=0, abort_status=EXIT_SUCCESS;
    if(options->verbose) {
        init_my_progressbar(totncells,&interrupted);
    }


#if defined(_OPENMP)
#pragma omp parallel shared(numdone, abort_status, interrupt_status_DDsmu_mocks_double)
    {
        const int tid = omp_get_thread_num();
        uint64_t npairs[totnbins];
        double savg[totnbins], weightavg[totnbins], projpairs[nprojbins];
        double projpairs_tensor[nprojbins][nprojbins];

        for(int i=0;i<totnbins;i++) {
            npairs[i] = 0;
            if(options->need_avg_sep) {
                savg[i] = ZERO;
            }
            if(need_weightavg) {
                weightavg[i] = ZERO;
            }
        }
        for(int i=0;i<nprojbins;i++) {
            projpairs[i] = ZERO;
            for(int j=0;j<nprojbins;j++) {
                projpairs_tensor[i*nprojbins+j] = ZERO;
            }
        }

#pragma omp for  schedule(dynamic)
#endif//USE_OMP

        /*---Loop-over-Data1-particles--------------------*/
        for(int64_t index1=0;index1<totncells;index1++) {

#if defined(_OPENMP)
#pragma omp flush (abort_status, interrupt_status_DDsmu_mocks_double)
#endif
            if(abort_status == EXIT_SUCCESS && interrupt_status_DDsmu_mocks_double == EXIT_SUCCESS) {
                //omp cancel was introduced in omp 4.0 - so this is my way of checking if loop needs to be cancelled
                /* If the verbose option is not enabled, avoid outputting anything unnecessary*/
                if(options->verbose) {
#if defined(_OPENMP)
                    if (omp_get_thread_num() == 0)
#endif
                        my_progressbar(numdone,&interrupted);


#if defined(_OPENMP)
#pragma omp atomic
#endif
                    numdone++;
                }

                const cellarray_mocks_index_particles_double *first  = &(lattice1[index1]);
                if(first->nelements == 0) {
                    continue;
                }
                double *x1 = first->x;
                double *y1 = first->y;
                double *z1 = first->z;
                double *d1 = first->cz;
                const weight_struct_double *weights1 = &(first->weights);
                const int64_t N1 = first->nelements;

                if(autocorr == 1) {
                    int same_cell = 1;
                    double *this_savg = options->need_avg_sep ? &(savg[0]):NULL;
                    double *this_weightavg = need_weightavg ? weightavg:NULL;
                    const int status = countpairs_s_mu_mocks_function_double(N1, x1, y1, z1, d1, weights1,
                                                                             N1, x1, y1, z1, d1, weights1,
                                                                             same_cell,
                                                                             options->fast_divide_and_NR_steps,
                                                                             smax, smin, nsbin,
                                                                             nmu_bins, supp_sqr, mu_max,
                                                                             this_savg, npairs, projpairs,
                                                                             projpairs_tensor,
                                                                             this_weightavg, extra->weight_method);
                    /* This actually causes a race condition under OpenMP - but mostly
                       I care that an error occurred - rather than the exact value of
                       the error status */
                    abort_status |= status;
                }

                for(int64_t ngb=0;ngb<first->num_ngb;ngb++){
                    const cellarray_mocks_index_particles_double *second = first->ngb_cells[ngb];
                    if(second->nelements == 0) {
                        continue;
                    }
                    const int same_cell = 0;
                    double *x2 = second->x;
                    double *y2 = second->y;
                    double *z2 = second->z;
                    double *d2 = second->cz;
                    const weight_struct_double *weights2 = &(second->weights);
                    const int64_t N2 = second->nelements;
                    double *this_savg = options->need_avg_sep ? &(savg[0]):NULL;
                    double *this_weightavg = need_weightavg ? weightavg:NULL;
                    const int status = countpairs_s_mu_mocks_function_double(N1, x1, y1, z1, d1, weights1,
                                                                             N2, x2, y2, z2, d2, weights2,
                                                                             same_cell,
                                                                             options->fast_divide_and_NR_steps,
                                                                             smax, smin, nsbin,
                                                                             nmu_bins, supp_sqr, mu_max,
                                                                             this_savg, npairs, projpairs,
                                                                             projpairs_tensor,
                                                                             this_weightavg, extra->weight_method);
                    /* This actually causes a race condition under OpenMP - but mostly
                       I care that an error occurred - rather than the exact value of
                       the error status */
                    abort_status |= status;
                }//loop over ngb cells
            }//abort_status check
        }//i loop over ND1 particles
#if defined(_OPENMP)
        for(int i=0;i<totnbins;i++) {
            all_npairs[tid][i] = npairs[i];
            if(options->need_avg_sep) {
                all_savg[tid][i] = savg[i];
            }
            if(need_weightavg) {
                all_weightavg[tid][i] = weightavg[i];
            }
        }
        for (int i=0;i<nprojbins;i++) {
            all_projpairs[tid][i] = projpairs[i];
            for(int j=0;j<nprojbins;j++) {
                all_projpairs_tensor[tid][i*nprojbins+j] = projpairs_tensor[i*nprojbins+j];
            }
        }
    }//close the omp parallel region
#endif//USE_OMP

    free_cellarray_mocks_index_particles_double(lattice1,totncells);
    if(autocorr == 0) {
        free_cellarray_mocks_index_particles_double(lattice2,totncells);
    }

    if(abort_status != EXIT_SUCCESS || interrupt_status_DDsmu_mocks_double != EXIT_SUCCESS) {
        /* Cleanup memory here if aborting */
        free(supp);
#if defined(_OPENMP)
        matrix_free((void **) all_npairs, numthreads);
        if(options->need_avg_sep) {
            matrix_free((void **) all_savg, numthreads);
        }
        if(need_weightavg) {
            matrix_free((void **) all_weightavg, numthreads);
        }
        matrix_free((void **) all_projpairs, numthreads);
        matrix_free((void **) all_projpairs_tensor, numthreads);
#endif
        return EXIT_FAILURE;
    }

    if(options->verbose) {
        finish_myprogressbar(&interrupted);
    }



#if defined(_OPENMP)
    uint64_t npairs[totnbins];
    double savg[totnbins], weightavg[totnbins], projpairs[nprojbins];
    double projpairs_tensor[nprojbins*nprojbins];
    for(int i=0;i<totnbins;i++) {
        npairs[i] = 0;
        if(options->need_avg_sep) {
            savg[i] = ZERO;
        }
        if(need_weightavg) {
            weightavg[i] = ZERO;
        }
    }
    for(int i=0;i<nprojbins;i++) {
        projpairs[i] = ZERO;
        for(int j=0;j<nprojbins;j++) {
            projpairs_tensor[i*nprojbins+j] = ZERO;
        }
    }

    for(int i=0;i<numthreads;i++) {
        for(int j=0;j<totnbins;j++) {
            npairs[j] += all_npairs[i][j];
            if(options->need_avg_sep) {
                savg[j] += all_savg[i][j];
            }
            if(need_weightavg) {
                weightavg[j] += all_weightavg[i][j];
            }
        }
        for(int j=0;j<nprojbins;j++) {
            projpairs[j] += all_projpairs[i][j];
            for(int k=0;k<nprojbins;k++) {
                projpairs_tensor[j*nprojbins+k] += all_projpairs_tensor[i][j*nprojbins+k];
            }
        }
    }
    matrix_free((void **) all_npairs, numthreads);
    if(options->need_avg_sep) {
        matrix_free((void **) all_savg, numthreads);
    }
    if(need_weightavg) {
        matrix_free((void **) all_weightavg, numthreads);
    }
    matrix_free((void **) all_projpairs, numthreads);
    matrix_free((void **) all_projpairs_tensor, numthreads);

#endif //USE_OMP

    //The code does not double count for autocorrelations
    //which means the npairs and savg values need to be doubled;
    if(autocorr == 1) {
        const uint64_t int_fac = 2;
        const double dbl_fac = (double) 2.0;
        for(int i=0;i<totnbins;i++) {
            npairs[i] *= int_fac;
            if(options->need_avg_sep) {
                savg[i] *= dbl_fac;
            }
            if(need_weightavg) {
                weightavg[i] *= dbl_fac;
            }
        }
        //TODO: do i also want to double this? think so
        for(int i=0;i<nprojbins;i++) {
            projpairs[i] *= dbl_fac;
            for(int j=0;j<nprojbins;j++) {
                projpairs_tensor[i*nprojbins+j] *= dbl_fac;
            }
        }

    }

    for(int i=0;i<totnbins;i++) {
        if(npairs[i] > 0) {
            if(options->need_avg_sep) {
                savg[i] /= (double) npairs[i] ;
            }
            if(need_weightavg) {
                weightavg[i] /= (double) npairs[i];
            }
        }
    }
    // don't need proj_pairs here, not averaging



    results->nsbin   = nsbin;
    results->nmu_bins = nmu_bins;
    results->mu_max = max_mu;//NOTE max_mu which is double and not mu_max (which might be float)
    results->mu_min = ZERO;
    results->npairs = my_malloc(sizeof(*(results->npairs)), totnbins);
    results->projpairs = my_malloc(sizeof(*(results->npairs)), nprojbins);
    results->projpairs_tensor = my_malloc(sizeof(*(results->npairs)), nprojbins*nprojbins);
    results->supp   = my_malloc(sizeof(*(results->supp))  , nsbin);
    results->savg  = my_malloc(sizeof(*(results->savg)) , totnbins);
    results->weightavg  = my_calloc(sizeof(double)  , totnbins);
    if(results->npairs == NULL || results->supp == NULL || results->savg == NULL || results->weightavg == NULL || results->projpairs == NULL) {
        free_results_mocks_s_mu(results);
        free(supp);
        return EXIT_FAILURE;
    }

    for(int i=0;i<nsbin;i++) {
        results->supp[i] = supp[i];
        for(int j=0;j<nmu_bins;j++) {
            const int index = i*(nmu_bins+1) + j;
            if( index >= totnbins ) {
                fprintf(stderr, "ERROR: In %s> index = %d must be in range [0, %d)\n", __FUNCTION__, index, totnbins);
                free_results_mocks_s_mu(results);
                free(supp);
                return EXIT_FAILURE;
            }
            results->npairs[index] = npairs[index];
            results->savg[index] = ZERO;
            results->weightavg[index] = ZERO;
            if(options->need_avg_sep) {
                results->savg[index] = savg[index];
            }
            if(need_weightavg) {
                results->weightavg[index] = weightavg[index];
            }
        }
    }
    for(int i=0;i<nprojbins;i++) {
        results->projpairs[i] = projpairs[i];
        for(int j=0;j<nprojbins;j++) {
                results->projpairs_tensor[i*nprojbins+j] = projpairs_tensor[i*nprojbins+j];
            }
    }
    free(supp);

    /* reset interrupt handlers to default */
    RESET_INTERRUPT_HANDLERS();
    reset_bin_refine_factors(options);

    if(options->c_api_timer) {
        struct timeval t1;
        gettimeofday(&t1, NULL);
        options->c_api_time = ADD_DIFF_TIME(t0, t1);
    }

    return EXIT_SUCCESS;
}
