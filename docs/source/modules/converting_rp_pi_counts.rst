.. _converting_rp_pi_counts:

Converting :math:`(r_p, \pi)` pairs into a projected correlation function
==========================================================================

Pair counts in :math:`(r_p, \pi)` can be converted into a projected correlation function
by using the helper function `Corrfunc.utils.convert_rp_pi_counts_to_wp`.

.. code:: python

          from Corrfunc.theory import DDrppi
          from Corrfunc.io import read_catalog
          from Corrfunc.utils import convert_rp_pi_counts_to_cf
          
          # Read the supplied galaxies on a periodic box          
          X, Y, Z = read_catalog()
          boxsize = 420.0

          # Generate randoms on the box          
          rand_N = 3*N
          rand_X = np.random.uniform(0, boxsize, rand_N)
          rand_Y = np.random.uniform(0, boxsize, rand_N)
          rand_Z = np.random.uniform(0, boxsize, rand_N)
          nthreads = 2
          pimax = 40.0

          # Setup the bins   
          bins = np.linspace(0.1, 10.0, 10)

          # Auto pair counts in DD          
          autocorr=1
          DD_counts = DDrppi(autocorr, nthreads, bins, X, Y, Z,
                             periodic=False, verbose=True)

          # Cross pair counts in DR          
          autocorr=0                   
          DR_counts = DDrppi(autocorr, nthreads, bins, X, Y, Z,
                             rand_X, rand_Y, rand_Z,
                             periodic=False, verbose=True)

          # Auto pairs counts in RR          
          autocorr=1
          RR_counts = DDrppi(autocorr, nthreads, bins, rand_X, rand_Y, rand_Z,
                             periodic=False, verbose=True)

          # All the pair counts are done, get the correlation function          
          wp = convert_rp_pi_counts_to_cf(N, N, rand_N, rand_N,
                                          DD_counts, DR_counts,
                                          DR_counts, RR_counts)

See the complete reference here :py:mod:`Corrfunc`.  

   
                   