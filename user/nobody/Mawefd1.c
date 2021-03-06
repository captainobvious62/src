/* Acoustic time-domain FD modeling */
/*
  Copyright (C) 2007 Colorado School of Mines
  
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include <rsf.h>
#include "fdutil.h"

/* check: dt<= 0.2 * min(dx,dz)/vmin */

#define NOP 2 /* derivative operator half-size */

#define C0 -2.500000 /*    c0=-30./12.; */
#define CA +1.333333 /*    ca=+16./12.; */
#define CB -0.083333 /*    cb=- 1./12.; */

#define C1  0.66666666666666666666 /*  2/3  */	
#define C2 -0.08333333333333333333 /* -1/12 */

/* centered FD derivative stencils */
#define D1(a,i2,i1,s) (C2*(a[i2  ][i1+2] - a[i2  ][i1-2]) +  \
                       C1*(a[i2  ][i1+1] - a[i2  ][i1-1])  )*s
#define D2(a,i2,i1,s) (C2*(a[i2+2][i1  ] - a[i2-2][i1  ]) +  \
                       C1*(a[i2+1][i1  ] - a[i2-1][i1  ])  )*s

int main(int argc, char* argv[])
{
    bool verb,free,snap; 
    int  jsnap;
    int  jdata;

    /* I/O files */
    sf_file Fwav=NULL; /* wavelet   */
    sf_file Fsou=NULL; /* sources   */
    sf_file Frec=NULL; /* receivers */
    sf_file Fvel=NULL; /* velocity  */
    sf_file Fden=NULL; /* density   */
    sf_file Fdat=NULL; /* data      */
    sf_file Fwfl=NULL; /* wavefield */

    /* I/O arrays */
    float  *ww=NULL;           /* wavelet   */
    pt2d   *ss=NULL;           /* sources   */
    pt2d   *rr=NULL;           /* receivers */

    float **vpin=NULL;         /* velocity  */
    float **roin=NULL;         /* density   */
    float **vp=NULL;           /* velocity in expanded domain */
    float **ro=NULL;           /* density  in expanded domain */

    float **vt=NULL;           /* temporary vp*vp * dt*dt */

    float  *dd=NULL;           /* data      */
    float **um,**uo,**up,**ua,**ut; /* wavefield: um = U @ t-1; uo = U @ t; up = U @ t+1 */

    /* cube axes */
    sf_axis at,a1,a2,as,ar;
    int     nt,n1,n2,ns,nr,nb;
    int     it,i1,i2;
    float   dt,d1,d2,id1,id2,dt2;

    /* linear interpolation weights/indices */
    lint2d cs,cr;

    fdm2d    fdm;
    abcone2d abc;     /* abc */
    sponge2d spo;

    /* FD operator size */
    float co,ca2,cb2,ca1,cb1;

    int ompchunk;
#ifdef _OPENMP
    int ompnth,ompath;
#endif

    sf_axis    c1=NULL, c2=NULL;
    int       nq1,nq2;
    float     oq1,oq2;
    float     dq1,dq2;
    float     **uc=NULL;

    /*------------------------------------------------------------*/
    /* init RSF */
    sf_init(argc,argv);

    if(! sf_getint("ompchunk",&ompchunk)) ompchunk=1;  
    /* OpenMP data chunk size */
#ifdef _OPENMP
    if(! sf_getint("ompnth",  &ompnth))     ompnth=0;  
    /* OpenMP available threads */

#pragma omp parallel
    ompath=omp_get_num_threads();
    if(ompnth<1) ompnth=ompath;
    omp_set_num_threads(ompnth);
    sf_warning("using %d threads of a total of %d",ompnth,ompath);
#endif

    if(! sf_getbool("verb",&verb)) verb=false; /* verbosity flag */
    if(! sf_getbool("snap",&snap)) snap=false; /* wavefield snapshots flag */
    if(! sf_getbool("free",&free)) free=false; /* free surface flag*/

    Fwav = sf_input ("in" ); /* wavelet   */
    Fvel = sf_input ("vel"); /* velocity  */
    Fsou = sf_input ("sou"); /* sources   */
    Frec = sf_input ("rec"); /* receivers */
    Fwfl = sf_output("wfl"); /* wavefield */
    Fdat = sf_output("out"); /* data      */
    Fden = sf_input ("den"); /* density   */

    /* axes */
    at = sf_iaxa(Fwav,2); sf_setlabel(at,"t"); if(verb) sf_raxa(at); /* time */
    as = sf_iaxa(Fsou,2); sf_setlabel(as,"s"); if(verb) sf_raxa(as); /* sources */
    ar = sf_iaxa(Frec,2); sf_setlabel(ar,"r"); if(verb) sf_raxa(ar); /* receivers */
    a1 = sf_iaxa(Fvel,1); sf_setlabel(a1,"z"); if(verb) sf_raxa(a1); /* depth */
    a2 = sf_iaxa(Fvel,2); sf_setlabel(a2,"x"); if(verb) sf_raxa(a2); /* space */

    nt = sf_n(at); dt = sf_d(at);
    ns = sf_n(as);
    nr = sf_n(ar);
    n1 = sf_n(a1); d1 = sf_d(a1);
    n2 = sf_n(a2); d2 = sf_d(a2);

    if(! sf_getint("jdata",&jdata)) jdata=1;
    if(snap) {  /* save wavefield every *jsnap* time steps */
	if(! sf_getint("jsnap",&jsnap)) jsnap=nt;
    }

    /*------------------------------------------------------------*/
    /* expand domain for FD operators and ABC */
    if( !sf_getint("nb",&nb) || nb<NOP) nb=NOP;

    fdm=fdutil_init(verb,free,a1,a2,nb,ompchunk);

    sf_setn(a1,fdm->n1pad); sf_seto(a1,fdm->o1pad); if(verb) sf_raxa(a1);
    sf_setn(a2,fdm->n2pad); sf_seto(a2,fdm->o2pad); if(verb) sf_raxa(a2);
    /*------------------------------------------------------------*/

    /* setup output data header */
    sf_oaxa(Fdat,ar,1);

    sf_setn(at,nt/jdata);
    sf_setd(at,dt*jdata);
    sf_oaxa(Fdat,at,2);

    /* setup output wavefield header */
    if(snap) {
	if(!sf_getint  ("nq1",&nq1)) nq1=sf_n(a1);
	if(!sf_getint  ("nq2",&nq2)) nq2=sf_n(a2);
	if(!sf_getfloat("oq1",&oq1)) oq1=sf_o(a1);
	if(!sf_getfloat("oq2",&oq2)) oq2=sf_o(a2);
	dq1=sf_d(a1);
	dq2=sf_d(a2);

	c1 = sf_maxa(nq1,oq1,dq1);
	c2 = sf_maxa(nq2,oq2,dq2);

	/* check if the imaging window fits in the wavefield domain */

	uc=sf_floatalloc2(sf_n(c1),sf_n(c2));

	sf_setn(at,nt/jsnap);
	sf_setd(at,dt*jsnap);

	sf_oaxa(Fwfl,c1,1);
	sf_oaxa(Fwfl,c2,2);
	sf_oaxa(Fwfl,at,3);
    }

    ww = sf_floatalloc( 1);
    dd = sf_floatalloc(nr);

    /*------------------------------------------------------------*/
    /* setup source/receiver coordinates */
    ss = (pt2d*) sf_alloc(ns,sizeof(*ss)); 
    rr = (pt2d*) sf_alloc(nr,sizeof(*rr)); 

    pt2dread1(Fsou,ss,ns,2); /* read (x,z) coordinates */
    pt2dread1(Frec,rr,nr,2); /* read (x,z) coordinates */

    cs = lint2d_make(ns,ss,fdm);
    cr = lint2d_make(nr,rr,fdm);

    /*------------------------------------------------------------*/
    /* setup FD coefficients */
    dt2 =    dt*dt;
    id1 = 1/d1;
    id2 = 1/d2;

    co = C0 * (id2*id2+id1*id1);
    ca2= CA *  id2*id2;
    cb2= CB *  id2*id2;
    ca1= CA *          id1*id1;
    cb1= CB *          id1*id1;

    /*------------------------------------------------------------*/ 
    /* setup model */
    roin=sf_floatalloc2(n1,   n2   ); 
    ro  =sf_floatalloc2(fdm->n1pad,fdm->n2pad); 
    sf_floatread(roin[0],n1*n2,Fden); 
    expand(roin,ro,fdm);

    vpin=sf_floatalloc2(n1,   n2   ); 
    vp  =sf_floatalloc2(fdm->n1pad,fdm->n2pad); 
    vt  =sf_floatalloc2(fdm->n1pad,fdm->n2pad); 
    sf_floatread(vpin[0],n1*n2,Fvel);
    expand(vpin,vp,fdm);

    for    (i2=0; i2<fdm->n2pad; i2++) {
	for(i1=0; i1<fdm->n1pad; i1++) {
	    vt[i2][i1] = vp[i2][i1] * vp[i2][i1] * dt2;
	}
    }

    /* free surface */
    if(free) {
	for    (i2=0; i2<fdm->n2pad; i2++) {
	    for(i1=0; i1<fdm->nb; i1++) {
		vt[i2][i1]=0;
	    }
	}
    }

    /*------------------------------------------------------------*/
    /* allocate wavefield arrays */
    um=sf_floatalloc2(fdm->n1pad,fdm->n2pad);
    uo=sf_floatalloc2(fdm->n1pad,fdm->n2pad);
    up=sf_floatalloc2(fdm->n1pad,fdm->n2pad);
    ua=sf_floatalloc2(fdm->n1pad,fdm->n2pad);

    for    (i2=0; i2<fdm->n2pad; i2++) {
	for(i1=0; i1<fdm->n1pad; i1++) {
	    um[i2][i1]=0;
	    uo[i2][i1]=0;
	    up[i2][i1]=0;
	    ua[i2][i1]=0;
	}
    }

    /*------------------------------------------------------------*/
    /* one-way abc setup */
    abc = abcone2d_make(NOP,dt,vp,free,fdm);
    /* sponge abc setup */
    spo = sponge2d_make(fdm);

    /*------------------------------------------------------------*/
    /* 
     *  MAIN LOOP
     */
    /*------------------------------------------------------------*/
    if(verb) fprintf(stderr,"\n");
    for (it=0; it<nt; it++) {
	if(verb) fprintf(stderr,"\b\b\b\b\b%d",it);
	
	/* read source data */
	sf_floatread(ww,1,Fwav);

#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic,fdm->ompchunk) private(i2,i1) shared(fdm,ua,uo,co,ca2,ca1,cb2,cb1,id2,id1)
#endif
	for    (i2=NOP; i2<fdm->n2pad-NOP; i2++) {
	    for(i1=NOP; i1<fdm->n1pad-NOP; i1++) {
		
		/* 4th order Laplacian operator */
		ua[i2][i1] = 
		    co * uo[i2  ][i1  ] + 
		    ca2*(uo[i2-1][i1  ] + uo[i2+1][i1  ]) +
		    cb2*(uo[i2-2][i1  ] + uo[i2+2][i1  ]) +
		    ca1*(uo[i2  ][i1-1] + uo[i2  ][i1+1]) +
		    cb1*(uo[i2  ][i1-2] + uo[i2  ][i1+2]);
		
		/* density term */
		ua[i2][i1] -= (
		    D1(uo,i2,i1,id1) * D1(ro,i2,i1,id1) +
		    D2(uo,i2,i1,id2) * D2(ro,i2,i1,id2) ) / ro[i2][i1];
	    }
	}   
	
	/* inject acceleration source */
	lint2d_inject1(ua,ww[0],cs);

	/* step forward in time */
#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic,fdm->ompchunk) private(i2,i1) shared(fdm,ua,uo,um,up,vt,dt2)
#endif
	for    (i2=0; i2<fdm->n2pad; i2++) {
	    for(i1=0; i1<fdm->n1pad; i1++) {
		up[i2][i1] = 2*uo[i2][i1] 
		    -          um[i2][i1] 
		    +          ua[i2][i1] * vt[i2][i1];
	    }
	}
	/* circulate wavefield arrays */
	ut=um;
	um=uo;
	uo=up;
	up=ut;
	
	/* one-way abc apply */
	abcone2d_apply(uo,um,NOP,abc,fdm);
	sponge2d_apply(um,spo,fdm);
	sponge2d_apply(uo,spo,fdm);
	sponge2d_apply(up,spo,fdm);

	/* extract data */
	lint2d_extract(uo,dd,cr);

	if(snap && (it+1)%jsnap==0) {
	    cut2d(uo,uc,fdm,c1,c2);
	    sf_floatwrite(uc[0],sf_n(c1)*sf_n(c2),Fwfl);
	}
	if(         it   %jdata==0) sf_floatwrite(dd,nr,Fdat);
    }
    if(verb) fprintf(stderr,"\n");    

    exit (0);
}
