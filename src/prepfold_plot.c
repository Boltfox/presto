#include "prepfold.h"

/********************************************/
/* The following is taken from ppgplot.c by */
/* Nick Patavalis (npat@ariadne.di.uoa.gr)  */
/********************************************/

void
minmax (float *v, int nsz, float *min, float *max)
{
    register float *e;
    register float mn, mx;

    for (mn=mx=*v, e=v+nsz; v < e; v++)
	if (*v > mx) mx = *v;
	else if (*v < mn) mn = *v;
    *min = mn;
    *max = mx;
}


void 
lininterp (float min, float max, int npts, float *v)
{
    register int i;
    register float step;
    register float lev;

    step = (max-min) / (npts-1);
    lev = min;
    for (i=0; i<npts; i++) {
	v[i] = lev;
	lev += step;
    }
}


static void   
autocal2d(float *a, int rn, int cn,
	  float *fg, float *bg, int nlevels, float *levels,
	  float *x1, float *x2, float *y1, float *y2, 
	  float *tr)
{
/*     int i; */
    float dx1, dx2, dy1, dy2;

    /* autocalibrate intensity-range. */
    if (*fg == *bg) {
	minmax(a,rn*cn,bg,fg);
/* 	fprintf(stderr,"Intensity range:\n  fg=%f\n  bg=%f\n",*fg,*bg); */
    }
    
    if ((nlevels >= 2) && (levels))
	lininterp(*bg, *fg, nlevels, levels);
    
    /* autocalibrate x-y range. */
    if ((*x1 == *x2) || (*y1 == *y2)) cpgqwin(&dx1,&dx2,&dy1,&dy2);
    if (*x1 == *x2) {*x1=dx1; *x2=dx2;}
    if (*y1 == *y2) {*y1=dy1; *y2=dy2;}
/*     fprintf(stderr,"Xrange: [%f, %f]\nYrange[%f, %f]\n",*x1,*x2,*y1,*y2); */
    
    /* calculate transformation vector. */
    tr[2] = tr[4] = 0.0;    
    tr[1] = (*x2 - *x1) / cn;
    tr[0] = *x1 - (tr[1] / 2);
    tr[5] = (*y2 - *y1) / rn;
    tr[3] = *y1 - (tr[5] / 2);
	
/*     fprintf(stderr,"Tansformation vector:\n"); */
/*     for (i=0; i<6; fprintf(stderr,"  tr[%d]=%f\n",i,tr[i]),i++); */
}

/********************************************/


void prepfold_plot(prepfoldinfo *search)
/* Make the beautiful 1 page prepfold output */
{
  int ii, jj, kk, ll, mm, len, profindex=0;
  int totpdelay=0, totpddelay=0, pdelay, pddelay;
  double profavg=0.0, profvar=0.0;
  double N=0.0, T, dphase, pofact, *currentprof, *lastprof;
  double delay, parttime, *pdprofs, *dtmparr, bestp, bestpd, bestpdd;
  float *ftmparr1, *ftmparr2;
  foldstats currentstats, beststats;
  /* Best Fold Plot */
  float *bestprof=NULL, *phasetwo=NULL;
  /* Profiles vs Time */
  float *timeprofs=NULL, *parttimes=NULL;
  /* RedChi vs Time */
  float *timechi=NULL;
  /* Profiles vs DM */
  float *dmprofs=NULL, *phaseone=NULL;
  /* DM vs RedChi */  
  float *dmchi=NULL;
  /* Period vs RedChi */  
  float *periodchi=NULL;
  /* P-dot vs RedChi */  
  float *pdotchi=NULL;
  /* Period P-dot 2D */  
  float *ppdot2d=NULL;

strcpy(search->pgdev, "/CPS");

  if (search->fold.pow==1.0){ /* Barycentric periods */
    bestp = search->bary.p1;
    bestpd = search->bary.p2;
    bestpdd = search->bary.p3;
  } else {                   /* Topocentric periods */
    bestp = search->topo.p1;
    bestpd = search->topo.p2;
    bestpdd = search->topo.p3;
  }
  /* Time interval of 1 profile bin */

  dphase = 1.0 / (search->fold.p1 * search->proflen);

  /* Find out how many total points were folded */

  for (ii = 0; ii < search->npart; ii++)
    N += search->stats[ii * search->nsub].numdata;

  /* Calculate the time per part and the total observation time */

  parttime = search->stats[0].numdata * search->dt;
  T = N * search->dt;
  pofact = search->fold.p1 * search->fold.p1;

  /* Allocate the non-DM specific arrays */

  bestprof = gen_fvect(2 * search->proflen);
  phasetwo = gen_freqs(2 * search->proflen, 0.0, 
		       1.0 / search->proflen);
  timeprofs = gen_fvect(2 * search->proflen * search->npart);
  parttimes = gen_freqs(search->npart + 1, 0.0, parttime);
  timechi = gen_fvect(search->npart + 1);
  timechi[0] = 0.0;
  periodchi = gen_fvect(search->numperiods);
  pdotchi = gen_fvect(search->numpdots);
  ppdot2d = gen_fvect(search->numperiods * search->numpdots);
  pdprofs = gen_dvect(search->npart * search->proflen);
  currentprof = gen_dvect(search->proflen);
  lastprof = gen_dvect(search->proflen);
  for (ii = 0; ii < search->proflen; ii++)
    lastprof[ii] = 0.0;

  /* Find the delays for the best periods and p-dots */
  
  for (ii = 0; ii < search->numperiods; ii++)
    if (search->periods[ii]==bestp){
      totpdelay = ii - (search->numperiods - 1) / 2;
      break;
    }
  
  for (ii = 0; ii < search->numpdots; ii++)
    if (search->pdots[ii]==bestpd){
      totpddelay = ii - (search->numpdots - 1) / 2;
      break;
    }
      
  /* Correct profiles for best DM */

  if (search->nsub > 1){
    int *dmdelays;
    double *ddprofs, *subbanddelays, hif, hifdelay;
    foldstats *ddstats;

    /* Allocate DM specific arrays */

    dmprofs = gen_fvect(search->proflen * search->npart);
    phaseone = gen_freqs(search->proflen, 0.0, 
			 1.0 / search->proflen);
    dmchi = gen_fvect(search->numdms);
    
    /* Allocate local DM specific arrays*/

    ddprofs = gen_dvect(search->npart * search->proflen);
    ddstats = (foldstats *)malloc(search->npart * sizeof(foldstats));
    dmdelays = gen_ivect(search->nsub);

    /* Doppler corrected hi freq and its delay at best DM */

    hif = doppler(search->lofreq + (search->numchan - 1.0) * 
		  search->chan_wid, search->avgvoverc);
    hifdelay = delay_from_dm(search->bestdm, hif);

    /* De-disperse and combine the subbands */
    
    for (ii = 0; ii < search->numdms; ii++){  /* Loop over DMs */
      hifdelay = delay_from_dm(search->dms[ii], hif);
      subbanddelays = subband_delays(search->numchan, search->nsub, 
				     search->dms[ii], search->lofreq, 
				     search->chan_wid, search->avgvoverc);
      for (jj = 0; jj < search->nsub; jj++)
	dmdelays[jj] = ((int) ((subbanddelays[jj] - hifdelay) / 
			       dphase + 0.5)) % search->proflen;
      free(subbanddelays);

      /* Make the DM vs subband plot */

      for (jj = 0; jj < search->nsub; jj++){

	/* Copy the subband parts into a single array */

	for (kk = 0; kk < search->npart; kk++)
	  memcpy(ddprofs + kk * search->proflen, search->rawfolds + 
		 (kk * search->nsub + jj) * search->proflen, 
		 sizeof(double) * search->proflen);

	/* Correct each part for the current pdot */
	  
	for (kk = 0; kk < search->npart; kk++){
	  profindex = kk * search->proflen;
	  pddelay = (int) ((-0.5 * parttimes[kk] * parttimes[kk] * 
			    (search->pdots[jj] * search->fold.p1 * 
			     search->fold.p1 + search->fold.p2) / dphase) 
			   + 0.5);
	  shift_prof(ddprofs + profindex, search->proflen, pddelay, 
		     pdprofs + profindex);
	}
	
	/* Correct each part for the current pdot */

	combine_profs(pdprofs, ddstats, search->npart, search->proflen, 
		      totpdelay, currentprof, &currentstats);

	/* Place the profile into the DM array */

	double2float(currentprof, dmprofs + jj * search->proflen, 
		     search->proflen);
      }

      combine_subbands(search->rawfolds, search->stats, search->npart, 
		       search->nsub, search->proflen, dmdelays, 
		       ddprofs, ddstats);

      /* Perform the P-dot and Period searches */
      
      if (search->dms[ii]==search->bestdm){
	for (jj = 0; jj < search->numpdots; jj++){
	  
	  /* Correct each part for the current pdot */
	  
	  for (kk = 0; kk < search->npart; kk++){
	    profindex = kk * search->proflen;
	    pddelay = (int) ((-0.5 * parttimes[kk] * parttimes[kk] * 
			     (search->pdots[jj] * search->fold.p1 * 
			      search->fold.p1 + search->fold.p2) / dphase) 
			     + 0.5);
	    shift_prof(ddprofs + profindex, search->proflen, pddelay, 
		       pdprofs + profindex);
	  }
	
	  /* Search over the periods */
	  
	  for (kk = 0; kk < search->numperiods; kk++){
	    pdelay = kk - (search->numperiods - 1) / 2;
	    combine_profs(pdprofs, ddstats, search->npart, search->proflen, 
			  pdelay, currentprof, &currentstats);

	    /* Add to the periodchi array */

	    if (search->pdots[jj]==bestpd) 
	      periodchi[kk] = currentstats.redchi;

	    /* Add to the pdotchi array */

	    if (search->periods[kk]==bestp) 
	      pdotchi[jj] = currentstats.redchi;

	    /* Add to the ppdot2d array */

	    ppdot2d[jj * search->numperiods + kk] = currentstats.redchi;

	    /* Generate the time based arrays */

	    if (search->periods[kk]==bestp && search->pdots[jj]==bestpd){
	      int wrap;

	      /* The Best Prof */

	      double2float(currentprof, bestprof, search->proflen);
	      double2float(currentprof, bestprof + search->proflen, 
			   search->proflen);

	      /* Add this point to dmchi */

	      dmchi[ii] = currentstats.redchi;

	      /* Copy these statistics */

	      beststats = currentstats;

	      /* The profs at each of the npart times */

	      for (ll = 0; ll < search->npart; ll++){
		profindex = ll * search->proflen;
		wrap = (int)((double) (ll * pdelay) / 
			     ((double) search->npart) 
			     + 0.5) % search->proflen;
		shift_prof(pdprofs + profindex, search->proflen, wrap, 
			   currentprof);
		double2float(currentprof, timeprofs + 2 * profindex, 
			     search->proflen);
		double2float(currentprof, timeprofs + 2 * profindex + 
			     search->proflen, search->proflen);
		for (mm = 0; mm < search->proflen; mm++)
		  lastprof[mm] += currentprof[mm];
		profavg += ddstats[ll].prof_avg;
		profvar += ddstats[ll].prof_var;
		timechi[ll+1] = (chisqr(lastprof, search->proflen, 
					profavg, profvar) / 
				 (double) (search->proflen - 1.0));
	      }
	    }
	  }
	}

      /* Only check the best P and P-dot */

      } else {

	/* Correct each part for the current pdot */
	  
	for (kk = 0; kk < search->npart; kk++){
	  profindex = kk * search->proflen;
	  pddelay = (int) ((-0.5 * parttimes[kk] * parttimes[kk] * 
			    (search->pdots[jj] * search->fold.p1 * 
			     search->fold.p1 + search->fold.p2) / dphase) 
			   + 0.5);
	  shift_prof(ddprofs + profindex, search->proflen, pddelay, 
		     pdprofs + profindex);
	}
	
	/* Correct each part for the current pdot */

	combine_profs(pdprofs, ddstats, search->npart, search->proflen, 
		      totpdelay, currentprof, &currentstats);
	dmchi[ii] = currentstats.redchi;
      }
    }
    free(ddprofs);
    free(ddstats);
    free(dmdelays);

  /* No DM corrections */

  } else {

    for (jj = 0; jj < search->numpdots; jj++){
      
      /* Correct each part for the current pdot */
      
      for (kk = 0; kk < search->npart; kk++){
	profindex = kk * search->proflen;
	pddelay = (int) ((-0.5 * parttimes[kk] * parttimes[kk] * 
			  (search->pdots[jj] * search->fold.p1 * 
			   search->fold.p1 + search->fold.p2) / dphase) 
			 + 0.5);
	shift_prof(search->rawfolds + profindex, search->proflen, 
		   pddelay, pdprofs + profindex);
      }
      
      /* Search over the periods */
      
      for (kk = 0; kk < search->numperiods; kk++){
	pdelay = kk - (search->numperiods - 1) / 2;
	combine_profs(pdprofs, search->stats, search->npart, 
		      search->proflen, pdelay, currentprof, 
		      &currentstats);
	
	/* Add to the periodchi array */
	
	if (search->pdots[jj]==bestpd) 
	  periodchi[kk] = currentstats.redchi;
	
	/* Add to the pdotchi array */
	
	if (search->periods[kk]==bestp) 
	  pdotchi[jj] = currentstats.redchi;
	
	/* Add to the ppdot2d array */
	
	ppdot2d[jj * search->numperiods + kk] = currentstats.redchi;
	
	/* Generate the time based arrays */
	
	if (search->periods[kk]==bestp && search->pdots[jj]==bestpd){
	  int wrap;
	  
	  /* The Best Prof */

	  double2float(currentprof, bestprof, search->proflen);
	  double2float(currentprof, bestprof + search->proflen, 
		       search->proflen);
	  
	  /* Copy these statistics */

	  beststats = currentstats;

	  /* The profs at each of the npart times */
	  
	  for (ll = 0; ll < search->npart; ll++){
	    profindex = ll * search->proflen;
	    wrap = (int)((double) (ll * pdelay) / 
			 ((double) search->npart) 
			 + 0.5) % search->proflen;
	    shift_prof(pdprofs + profindex, search->proflen, wrap, 
		       currentprof);
	    double2float(currentprof, timeprofs + 2 * profindex, 
			 search->proflen);
	    double2float(currentprof, timeprofs + 2 * profindex + 
			 search->proflen, search->proflen);
	    for (mm = 0; mm < search->proflen; mm++)
	      lastprof[mm] += currentprof[mm];
	    profavg += search->stats[ll].prof_avg;
	    profvar += search->stats[ll].prof_var;
	    timechi[ll+1] = (chisqr(lastprof, search->proflen, 
				    profavg, profvar) / 
			     (double) (search->proflen - 1.0));
	  }
	}
      }
    }
  }

  /*
   *  Now plot the results
   */

  {
    float min, max, over;

    /* Open and prep our device */

    cpgopen(search->pgdev);
    cpgpap(10.25, 8.5/11.0);
    cpgpage();
    cpgiden();
    cpgsch(0.8);
    
    /* Time versus phase */

    cpgsvp (0.06, 0.27, 0.09, 0.68);
    cpgswin(0.0, 1.999, 0.0, T);
    {
      int mincol, maxcol, numcol, nr, nc;
      float l[2] = {0.0, 1.0};
      float r[2] = {1.0, 0.0};
      float g[2] = {1.0, 0.0};
      float b[2] = {1.0, 0.0};
      float fg = 0.0, bg = 0.0, tr[6], *levels;
      float x1 = 0.0, y1 = 0.0, x2 = 1.999, y2 = T;

      nr = search->npart;
      nc = 2 * search->proflen;
      cpgqcol(&mincol, &maxcol);
      mincol += 2;
      cpgscir(mincol, maxcol);
      numcol = maxcol - mincol + 1;
      levels = gen_fvect(numcol);
      cpgctab(l, r, g, b, numcol, 1.0, 0.5);
      autocal2d(timeprofs, nr, nc, &fg, &bg, numcol,
		levels, &x1, &x2, &y1, &y2, tr);
      cpgimag(timeprofs, nc, nr, 0+1, nc, 0+1, nr, bg, fg, tr);
      free(levels);
    }
    cpgbox ("BCNST", 0.0, 0, "BCNST", 0.0, 0);
    cpgmtxt("B", 2.6, 0.5, 0.5, "Phase");
    cpgmtxt("L", 2.1, 0.5, 0.5, "Time (s)");

    /*  Time versus Reduced chisqr */

    cpgsvp (0.27, 0.36, 0.09, 0.68);
    find_min_max_arr(search->npart+1, timechi, &min, &max);
    cpgswin(0.0, 1.1 * max, 0.0, T);
    cpgbox ("BCNST", 0.0, 0, "BCST", 0.0, 0);
    cpgmtxt("B", 2.6, 0.5, 0.5, "Reduced \\gx\\u2\\d");
    cpgline(search->npart+1, timechi, parttimes);

    /* Combined best profile */

    {
      float x[2] = {0.0, 2.2}, avg[2];
      float errx = 2.1, erry = profavg, errlen;

      cpgsvp (0.06, 0.291, 0.68, 0.94);
      find_min_max_arr(2 * search->proflen, bestprof, &min, &max);
      over = 0.1 * (max - min);
      cpgswin(0.0, 2.2, min - over, max + over);
      cpgbox ("BCST", 1.0, 4, "BC", 0.0, 0);
      cpgmtxt("T", 1.0, 0.5, 0.5, "2 Pulses of Best Profile");
      cpgline(2 * search->proflen, phasetwo, bestprof);
      cpgsls(4);
      avg[0] = avg[1] = profavg;
      cpgline(2, x, avg);
      cpgsls(1);
      errlen = sqrt(profvar);
      cpgerrb(6, 1, &errx, &erry, &errlen, 2);
      cpgpt(1, &errx, &erry, 5);
    }

    if (search->nsub > 1){

      /* DM vs reduced chisqr */

      cpgsvp (0.43, 0.66, 0.09, 0.22);
      find_min_max_arr(search->proflen, dmchi, &min, &max);
      cpgswin(search->dms[0], search->dms[search->numdms-1], 
	      0.0, 1.1 * max);
      cpgbox ("BCNST", 0.0, 0, "BCNST", 0.0, 0);
      cpgmtxt("L", 2.0, 0.5, 0.5, "Reduced \\gx\\u2\\d");
      cpgmtxt("B", 2.6, 0.5, 0.5, "DM");
      ftmparr1 = gen_fvect(search->numdms);
      double2float(search->dms, ftmparr1, search->numdms);
      cpgline(search->numdms, ftmparr1, dmchi);
      free(ftmparr1);

      /* Plots for each subband */

      /*   cpgsvp (0.43, 0.66, 0.3, 0.68); */
      /*   cpgswin(0.0-0.01, 1.0+0.01, 0.0, nsub+1.0); */
      /*   cpgbox("BCNST", 0.25, 2, "BNST", 0.0, 0); */
      /*   cpgmtxt("L", 2.0, 0.5, 0.5, "Sub-band"); */
      /*   cpgswin(0.0-0.01, 1.0+0.01, min(freqs)-2.0, max(freqs)+2.0); */
      /*   cpgbox("", 0.2, 2, "CMST", 0.0, 0); */
      /*   cpgmtxt("R", 2.3, 0.5, 0.5, "Frequency (MHz)"); */
      /*   cpgmtxt("B", 2.5, 0.5, 0.5, "Phase"); */

    } else {

      /* No DM search */

      cpgsvp (0.43, 0.66, 0.09, 0.22);
      cpgswin(0.0, 1.0, 0.0, 1.0);
      cpgbox ("BC", 0.0, 0, "BC", 0.0, 0);
      cpgmtxt("L", 2.0, 0.5, 0.5, "Reduced \\gx\\u2\\d");
      cpgmtxt("B", 2.6, 0.5, 0.5, "DM");
      cpgmtxt("T", -3.5, 0.5, 0.5, "No DM Search");
      cpgsvp (0.43, 0.66, 0.3, 0.68);
      cpgswin(0.0, 1.0, 0.0, 1.0);
      cpgbox ("BC", 0.0, 0, "BC", 0.0, 0);
      cpgmtxt("L", 2.0, 0.5, 0.5, "Sub-band");
      cpgswin(0.0, 1.0, 0.0, 1.0);
      cpgbox("", 0.0, 0, "C", 0.0, 0);
      cpgmtxt("R", 2.3, 0.5, 0.5, "Frequency (MHz)");
      cpgmtxt("B", 2.5, 0.5, 0.5, "Phase");
      cpgmtxt("T", -9, 0.5, 0.5, "No DM Search");
    }

    {
      int mincol, maxcol, numcol, nr, nc;
      float l[7] = {0.0, 0.167, 0.333, 0.5, 0.667, 0.833, 1.0};
      float r[7] = {0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0};
      float g[7] = {0.0, 0.0, 1.0, 1.0, 1.0, 0.0, 1.0};
      float b[7] = {0.0, 1.0, 1.0, 0.0, 0.0, 0.0, 1.0};
      float fg = 0.0, bg = 0.0, tr[6], *levels;
      float x1, x2, y1, y2;
      double pfold, pdfold;
      char pout[100], pdout[100];

      /* Period vs reduced chisqr */

      cpgsvp (0.74, 0.94, 0.41, 0.51);
      pfold = 1000.0 / search->fold.p1;
      ftmparr1 = gen_fvect(search->numperiods);
      for (ii = 0; ii < search->numperiods; ii++)
	ftmparr1[ii] = search->periods[ii] * 1000.0 - pfold;
      find_min_max_arr(search->numperiods, periodchi, &min, &max);
      x1 = ftmparr1[search->numperiods-1];
      x2 = ftmparr1[0];
      cpgswin(x1, x2, 0.0, 1.1 * max);
      cpgbox ("BCNST", 0.0, 0, "BCMST", 0.0, 0);
      sprintf(pout, "Period - %-.8f (ms)", pfold);
      cpgmtxt("B", 2.4, 0.5, 0.5, pout);
      cpgmtxt("R", 2.6, 0.5, 0.5, "Reduced \\gx\\u2\\d");
      cpgline(search->numperiods, ftmparr1, periodchi);
      free(ftmparr1);

      /* P-dot vs reduced chisqr */

      cpgsvp (0.74, 0.94, 0.58, 0.68);
      pdfold = -search->fold.p2 / (search->fold.p1 * search->fold.p1);
      ftmparr1 = gen_fvect(search->numpdots);
      for (ii = 0; ii < search->numpdots; ii++)
	ftmparr1[ii] = search->pdots[ii] - pdfold;
      find_min_max_arr(search->numpdots, pdotchi, &min, &max);
      y1 = ftmparr1[search->numpdots-1];
      y2 = ftmparr1[0];
      cpgswin(y1, y2, 0.0, 1.1 * max);
      cpgbox ("BCNST", 0.0, 0, "BCMST", 0.0, 0);
      if (pdfold < 0.0)
	sprintf(pdout, "Pdot + %-.5g (s/s)", fabs(pdfold));
      else
	sprintf(pdout, "Pdot-%-.5g (s/s)", pdfold);
      cpgmtxt("B", 2.4, 0.5, 0.5, pdout);
      cpgmtxt("R", 2.6, 0.5, 0.5, "Reduced \\gx\\u2\\d");
      cpgline(search->numpdots, ftmparr1, pdotchi);
      free(ftmparr1);

      /* P P-dot image */

      cpgsvp (0.74, 0.94, 0.09, 0.29);
      cpgswin(x1, x2, y1, y2);
      nr = search->numpdots;
      nc = search->numperiods;
      cpgqcol(&mincol, &maxcol);
      mincol += 2;
      cpgscir(mincol, maxcol);
      numcol = maxcol - mincol + 1;
      levels = gen_fvect(numcol);
      cpgctab(l, r, g, b, numcol, 1.0, 0.5);
      autocal2d(ppdot2d, nr, nc, &fg, &bg, numcol,
		levels, &x1, &x2, &y1, &y2, tr);
      cpgimag(ppdot2d, nc, nr, 0+1, nc, 0+1, nr, bg, fg, tr);
      cpgbox("BNST", 0.0, 0, "BNST", 0.0, 0);
      cpgmtxt("B", 2.6, 0.5, 0.5, pout);
      cpgmtxt("L", 2.1, 0.5, 0.5, pdout);
      x1 = 1.0 / search->periods[search->numperiods-1] - search->fold.p1;
      x2 = 1.0 / search->periods[0] - search->fold.p1;
      y1 = -search->pdots[search->numperiods-1] * 
	search->fold.p1 * search->fold.p1 - search->fold.p2;
      y2 = -search->pdots[0] * 
	search->fold.p1 * search->fold.p1 - search->fold.p2;
      cpgswin(x1, x2, y1, y2);
      cpgbox("CMST", 0.0, 0, "CMST", 0.0, 0);
      sprintf(pout, "Freq - %-.6f (Hz)", search->fold.p1);
      cpgmtxt("T", 1.9, 0.5, 0.5, pout);
      if (search->fold.p2 < 0.0)
	sprintf(pdout, "F-dot + %-.5g (Hz)", fabs(search->fold.p2));
      else
	sprintf(pdout, "F-dot - %-.5g (Hz)", search->fold.p2);
      cpgmtxt("R", 2.5, 0.5, 0.5, pdout);
      free(levels);
    }

    {
      char out[200], out2[100];

      /* Add the Data Info area */

      cpgsvp (0.291, 0.54, 0.68, 0.94);
      cpgswin(-0.1, 1.00, -0.1, 1.1);
      cpgsch(1.0);
      sprintf(out, "File:  %-s", search->filenm);
      cpgmtxt("T", 1.0, 0.5, 0.5, out);
      cpgsch(0.7);
      sprintf(out, "Candidate:  %-s", search->candnm);
      cpgtext(0.0, 1.0, out);
      sprintf(out, "Telescope:  %-s", search->telescope);
      cpgtext(0.0, 0.9, out);
      sprintf(out, "Epoch\\dtopo\\u = %-.12f", search->tepoch);
      cpgtext(0.0, 0.8, out);
      sprintf(out, "Epoch\\dbary\\u = %-.12f", search->bepoch);
      cpgtext(0.0, 0.7, out);
      sprintf(out, "T\\dsamp\\u = %f", search->dt);
      cpgtext(0.0, 0.6, out);
      sprintf(out, "N\\dfolded\\u = %-.0f", N);
      cpgtext(0.0, 0.5, out);
      sprintf(out, "Data Avg = %.3f", beststats.data_avg);
      cpgtext(0.0, 0.4, out);
      sprintf(out, "\\gs\\ddata\\u = %.2f", sqrt(beststats.data_var));
      cpgtext(0.0, 0.3, out);
      sprintf(out, "Bins/profile = %d", search->proflen);
      cpgtext(0.0, 0.2, out);
      sprintf(out, "Prof Avg = %.3f", beststats.prof_avg);
      cpgtext(0.0, 0.1, out);
      sprintf(out, "\\gs\\dprof\\u = %.2f", sqrt(beststats.prof_var));
      cpgtext(0.0, 0.0, out);

      /* Calculate the values of P and Q since we know X and DF */

      {
	int chiwhich=1, chistatus=0, goodsig=1;
	double chip=0.0, chiq=0.0, chixmeas=0.0, chidf=0.0, chitmp=0.0;
	double normz=0.0, normmean=0.0, normstdev=1.0;
	
	chidf = search->proflen - 1.0;
	chixmeas = beststats.redchi * chidf;
	cdfchi(&chiwhich, &chip, &chiq, &chixmeas, &chidf, &chistatus, &chitmp);
	if (chistatus != 0){
	  if (chistatus < 0)
	    printf("\nInput parameter %d to cdfchi() was out of range.\n", 
		   chistatus);
	  else if (chistatus == 3)
	    printf("\nP + Q do not equal 1.0 in cdfchi().\n");
	  else if (chistatus == 10)
	    printf("\nError in cdfgam().\n");
	  else printf("\nUnknown error in cdfchi().\n");
	}
	
	/* Calculate the equivalent sigma */
	
	chiwhich = 2;
	cdfnor(&chiwhich, &chip, &chiq, &normz, &normmean, &normstdev, 
	       &chistatus, &chitmp);
	if (chistatus != 0) goodsig=0;

	/* Add the Fold Info area */
      
	cpgsvp (0.54, 0.94, 0.68, 0.94);
	cpgswin(-0.05, 1.05, -0.1, 1.1);
	cpgsch(0.8);
	cpgmtxt("T", 1.0, 0.5, 0.5, "Search Information");
	cpgsch(0.7);
	cpgtext(0.0, 1.0, "   Best Fit Parameters");
	if (goodsig)
	  sprintf(out2, "(\\(0248)%.1f\\gs)", normz);
	else 
	  sprintf(out2, " ");
	sprintf(out, "Reduced \\gx\\u2\\d = %.3f   P(Noise) < %.3g   %s", 
		beststats.redchi, chiq, out2);
	cpgtext(0.0, 0.9, out);
	if (search->nsub > 1){
	  sprintf(out, "Best DM = %.3f", search->bestdm);
	  cpgtext(0.0, 0.8, out);
	}
	cpgtext(0.0, 0.7, "P\\dtopo\\u = 11.232312424562(88)");
	cpgtext(0.58, 0.7, "P\\dbary\\u = 11.232312424562(88)");
	cpgtext(0.0, 0.6, "P'\\dtopo\\u = 1.2345e-12");
	cpgtext(0.58, 0.6, "P'\\dbary\\u = 1.2345e-12");
	cpgtext(0.0, 0.5, "P''\\dtopo\\u = 1.2345e-12");
	cpgtext(0.58, 0.5, "P''\\dbary\\u = 1.2345e-12");
	cpgtext(0.0, 0.3, "   Binary Parameters");
	cpgtext(0.0, 0.2, "P\\dorb\\u (s) = 65636.123213");
	cpgtext(0.58, 0.2, "a\\d1\\usin(i)/c (s) = 1.132213");
	cpgtext(0.0, 0.1, "e = 0.0000");
	cpgtext(0.58, 0.1, "\\gw (rad) = 1.232456");
	cpgtext(0.0, 0.0, "T\\dperi\\u = 50000.123234221323");
      }
    }
    cpgclos();
  }
  free(bestprof);
  free(phasetwo);
  free(timeprofs);
  free(parttimes);
  free(timechi);
  free(periodchi);
  free(pdotchi);
  free(ppdot2d);
  free(pdprofs);
  free(currentprof);
  free(lastprof);
  if (search->nsub > 1){
    free(dmprofs);
    free(phaseone);
    free(dmchi);
  }
}
