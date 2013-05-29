#include <rsf.h>
#include "fdutil.h"
#include <stdio.h>
#include <math.h>

#ifdef _OPENMP
#include <omp.h>
#include "omputil.h"
#endif 


#ifndef _fdutil_h

typedef struct fdm2 *fdm2d;
/*^*/

typedef struct fdm3 *fdm3d;
/*^*/

typedef struct lcoef2 *lint2d;
/*^*/

typedef struct lcoef3 *lint3d;
/*^*/

typedef struct abc2 *abcone2d;
/*^*/

typedef struct abc3 *abcone3d;
/*^*/

typedef struct spon *sponge;
/*^*/

typedef struct ofg *ofg2d;
/*^*/

typedef struct PML2D *PML2D;
/*^*/

typedef struct PML3D *PML3D;
/*^*/

struct fdm2{
    int nb;
    int   nz,nzpad;
    int   nx,nxpad;
    float oz,ozpad;
    float ox,oxpad;
    float dz;
    float dx;
    bool verb;
    bool free;
    int ompchunk;
};
/*^*/

struct fdm3{
    int nb;
    int   nz,nzpad;
    int   nx,nxpad;
    int   ny,nypad;
    float oz,ozpad;
    float ox,oxpad;
    float oy,oypad;
    float dz;
    float dx;
    float dy;
    bool verb;
    bool free;
    int ompchunk;
};
/*^*/

struct lcoef2{
    int n;
    float *w00;
    float *w01;
    float *w10;
    float *w11;
    int *jz;
    int *jx;
};
/*^*/

struct lcoef3{
    int n;
    float *w000;
    float *w001;
    float *w010;
    float *w011;
    float *w100;
    float *w101;
    float *w110;
    float *w111;
    int *jz;
    int *jx;
    int *jy;
};
/*^*/

struct abc2{
    bool free;
    float *bzl;
    float *bzh;
    float *bxl;
    float *bxh;
};
/*^*/

struct abc3{
    bool free;
    float**bzl;
    float**bzh;
    float**bxl;
    float**bxh;
    float**byl;
    float**byh;
};
/*^*/

struct spon{
    float *w;
};
/*^*/

struct ofg{
    float **tt;
};
/*^*/

struct PML2D{
    float **up;
    float **down;
    float **right;
    float **left;
    float **upright;
    float **downright;
    float **downleft;
    float **upleft;    
};
/*^*/

struct PML3D{
    float ***Upsizx; float ***Upsizy; float ***UPSIzxy;
    float ***Dpsizx; float ***Dpsizy; float ***DPSIzxy;
    float ***Fpsiyx; float ***Fpsiyz; float ***FPSIyzx;
    float ***Bpsiyx; float ***Bpsiyz; float ***BPSIyzx;    
    float ***Lpsixy; float ***Lpsixz; float ***LPSIxyz;
    float ***Rpsixy; float ***Rpsixz; float ***RPSIxyz;        
    float ***ULPSIzxy; float ***URPSIzxy;
    float ***DLPSIzxy; float ***DRPSIzxy;    
    float ***UFPSIyzx; float ***UBPSIyzx;
    float ***DFPSIyzx; float ***DBPSIyzx;
    float ***LFPSIxyz; float ***RFPSIxyz;
    float ***LBPSIxyz; float ***RBPSIxyz;        
    float ***ULU; float ***URU; float ***DRU; float ***DLU;
    float ***UFU; float ***UBU; float ***DBU; float ***DFU;
    float ***LFU; float ***RFU; float ***RBU; float ***LBU;        
    float ***ULFW;   float ***URFW;   float ***DRFW;   float ***DLFW;
    float ***ULBW;   float ***URBW;   float ***DRBW;   float ***DLBW;     
};
/*^*/

#endif

static float** bell;
static float***bell3d;
static int    nbell;

/*------------------------------------------------------------*/
/* stencils and stencil parameters*/

#define NOP 4 /* derivative operator half-size */
#define alfa 1e-3 /*PML sigmoid std dev*/
#define mPML 200 /* max value of the PML*/
/* Muir's derivative operator */
#define C1 +0.598144  //  1225/ 1024      /2 
#define C2 -0.039876  // -1225/(1024*  15)/2
#define C3 +0.004785  //  1225/(1024* 125)/2 
#define C4 -0.000348  // -1225/(1024*1715)/2 

/***************/
/* 2D STENCILS */
/***************/
/*  2D forward FD derivative stencils */
#define Fz(a,ix,iz,s) (2*C4*(a[ix  ][iz+4] - a[ix  ][iz-3]) +     \
                       2*C3*(a[ix  ][iz+3] - a[ix  ][iz-2]) +     \
                       2*C2*(a[ix  ][iz+2] - a[ix  ][iz-1]) +     \
                       2*C1*(a[ix  ][iz+1] - a[ix  ][iz  ])  )*s
#define Fx(a,ix,iz,s) (2*C4*(a[ix+4][iz  ] - a[ix-3][iz  ]) +     \
                       2*C3*(a[ix+3][iz  ] - a[ix-2][iz  ]) +     \
                       2*C2*(a[ix+2][iz  ] - a[ix-1][iz  ]) +     \
                       2*C1*(a[ix+1][iz  ] - a[ix  ][iz  ])  )*s

/* 2D backward FD derivative stencils */
#define Bz(a,ix,iz,s) (2*C4*(a[ix  ][iz+3] - a[ix  ][iz-4]) +     \
                       2*C3*(a[ix  ][iz+2] - a[ix  ][iz-3]) +     \
                       2*C2*(a[ix  ][iz+1] - a[ix  ][iz-2]) +     \
                       2*C1*(a[ix  ][iz  ] - a[ix  ][iz-1])  )*s
#define Bx(a,ix,iz,s) (2*C4*(a[ix+3][iz  ] - a[ix-4][iz  ]) +     \
                       2*C3*(a[ix+2][iz  ] - a[ix-3][iz  ]) +     \
                       2*C2*(a[ix+1][iz  ] - a[ix-2][iz  ]) +     \
                       2*C1*(a[ix  ][iz  ] - a[ix-1][iz  ])  )*s

/***************/
/* 3D STENCILS */
/***************/
/*  3D forward FD derivative stencils */
#define Fz3(a,iy,ix,iz,s) (2*C4*(a[iy][ix  ][iz+4] - a[iy ][ix  ][iz-3]) +     \
                       2*C3*( a[iy][ix  ][iz+3] - a[iy ][ix  ][iz-2]) +     \
                       2*C2*( a[iy][ix  ][iz+2] - a[iy ][ix  ][iz-1]) +     \
                       2*C1*( a[iy][ix  ][iz+1] - a[iy ][ix  ][iz  ])  )*s
#define Fx3(a,iy,ix,iz,s) (2*C4*(a[iy][ix+4][iz  ] - a[iy ][ix-3][iz  ]) +     \
                       2*C3*( a[iy][ix+3][iz  ] - a[iy ][ix-2][iz  ]) +     \
                       2*C2*( a[iy][ix+2][iz  ] - a[iy ][ix-1][iz  ]) +     \
                       2*C1*( a[iy][ix+1][iz  ] - a[iy ][ix  ][iz  ])  )*s
#define Fy3(a,iy,ix,iz,s) (2*C4*(a[iy+4][ix][iz  ] - a[iy-3][ix ][iz  ]) +     \
                       2*C3*( a[iy+3][ix][iz  ] - a[iy-2][ix ][iz  ]) +     \
                       2*C2*( a[iy+2][ix][iz  ] - a[iy-1][ix ][iz  ]) +     \
                       2*C1*( a[iy+1][ix][iz  ] - a[iy  ][ix ][iz  ])  )*s

/* 3D backward FD derivative stencils */
#define Bz3(a,iy,ix,iz,s) (2*C4*(a[iy][ix  ][iz+3] - a[iy ][ix  ][iz-4]) +     \
                       2*C3*( a[iy][ix  ][iz+2] - a[iy ][ix  ][iz-3]) +     \
                       2*C2*( a[iy][ix  ][iz+1] - a[iy ][ix  ][iz-2]) +     \
                       2*C1*( a[iy][ix  ][iz  ] - a[iy ][ix  ][iz-1])  )*s
#define Bx3(a,iy,ix,iz,s) (2*C4*(a[iy][ix+3][iz  ] - a[iy ][ix-4][iz  ]) +     \
                       2*C3*( a[iy][ix+2][iz  ] - a[iy ][ix-3][iz  ]) +     \
                       2*C2*( a[iy][ix+1][iz  ] - a[iy ][ix-2][iz  ]) +     \
                       2*C1*( a[iy][ix  ][iz  ] - a[iy ][ix-1][iz  ])  )*s
#define By3(a,iy,ix,iz,s) (2*C4*(a[iy+3][ix][iz  ] - a[iy-4][ix ][iz  ]) +     \
                       2*C3*( a[iy+2][ix][iz  ] - a[iy-3][ix ][iz  ]) +     \
                       2*C2*( a[iy+1][ix][iz  ] - a[iy-2][ix ][iz  ]) +     \
                       2*C1*( a[iy  ][ix][iz  ] - a[iy-1][ix ][iz  ])  )*s
/*------------------------------------------------------------*/


/*------------------------------------------------------------*/
fdm2d fdutil_init(bool verb_, 
		  bool free_,
		  sf_axis az_, 
		  sf_axis ax_, 
		  int     nb_,
		  int ompchunk_) 
/*< init fdm utilities >*/
{ 
    fdm2d fdm;
    fdm = (fdm2d) sf_alloc(1,sizeof(*fdm));

    fdm->free=free_;
    fdm->verb=verb_;

    fdm->nb=nb_;

    fdm->nz=sf_n(az_);
    fdm->nx=sf_n(ax_);

    fdm->dz=sf_d(az_);
    fdm->dx=sf_d(ax_);

    fdm->oz=sf_o(az_);
    fdm->ox=sf_o(ax_);

    fdm->nzpad=sf_n(az_)+2*fdm->nb;
    fdm->nxpad=sf_n(ax_)+2*fdm->nb;
	
    fdm->ozpad=sf_o(az_)-fdm->nb*fdm->dz;
    fdm->oxpad=sf_o(ax_)-fdm->nb*fdm->dx;

    fdm->ompchunk=ompchunk_;

    return fdm;
}

/*------------------------------------------------------------*/
fdm3d fdutil3d_init(bool verb_, 
		    bool free_,
		    sf_axis az_, 
		    sf_axis ax_, 
		    sf_axis ay_, 
		    int     nb_,
		    int ompchunk_) 
/*< init fdm utilities >*/
{ 
    fdm3d fdm;
    fdm = (fdm3d) sf_alloc(1,sizeof(*fdm));

    fdm->free=free_;
    fdm->verb=verb_;

    fdm->nb=nb_;

    fdm->nz=sf_n(az_);
    fdm->nx=sf_n(ax_);
    fdm->ny=sf_n(ay_);

    fdm->dz=sf_d(az_);
    fdm->dx=sf_d(ax_);
    fdm->dy=sf_d(ay_);

    fdm->oz=sf_o(az_);
    fdm->ox=sf_o(ax_);
    fdm->oy=sf_o(ay_);

    fdm->nzpad=sf_n(az_)+2*fdm->nb;
    fdm->nxpad=sf_n(ax_)+2*fdm->nb;
    fdm->nypad=sf_n(ay_)+2*fdm->nb;
	
    fdm->ozpad=sf_o(az_)-fdm->nb*fdm->dz;
    fdm->oxpad=sf_o(ax_)-fdm->nb*fdm->dx;
    fdm->oypad=sf_o(ay_)-fdm->nb*fdm->dy;

    fdm->ompchunk=ompchunk_;

    return fdm;
}

/*------------------------------------------------------------*/
ofg2d offgrid_init(fdm2d fdm)
/*< init off-grid interpolation >*/
{
    ofg2d ofg;
    ofg = (ofg2d) sf_alloc(1,sizeof(*ofg));
    
    ofg->tt = sf_floatalloc2(fdm->nzpad,fdm->nxpad);
    
    return ofg;
}

/*------------------------------------------------------------*/
void offgridfor(float **ti,
		ofg2d  ofg,
		fdm2d  fdm)
/*< forward off-grid interpolation (in place) >*/
{
    int iz,ix;

    /* zero output */
    for     (ix=0;ix<fdm->nx;ix++) {
	for (iz=0;iz<fdm->nz;iz++) {
	    ofg->tt[ix][iz]=0;
	}
    }

    for     (ix=0;ix<fdm->nx-1;ix++) {
	for (iz=0;iz<fdm->nz-1;iz++) {
	    ofg->tt[ix][iz] =
		ti[ix  ][iz  ] + 
		ti[ix+1][iz  ] + 
		ti[ix  ][iz+1] + 
		ti[ix+1][iz+1];
	}
    }   

    /* copy to input array */
    for     (ix=0;ix<fdm->nx;ix++) {
	for (iz=0;iz<fdm->nz;iz++) {
	    ti[ix][iz] = 0.25 * ofg->tt[ix][iz];
	}
    }
}

/*------------------------------------------------------------*/
void offgridadj(float **ti,
		ofg2d  ofg,
		fdm2d  fdm)
/*< adjoint off-grid interpolation (in place) >*/
{
    int iz,ix;

    /* zero output */
    for     (ix=0;ix<fdm->nx;ix++) {
	for (iz=0;iz<fdm->nz;iz++) {
	    ofg->tt[ix][iz]=0;
	}
    }
    
    for     (ix=0;ix<fdm->nx-1;ix++) {
	for (iz=0;iz<fdm->nz-1;iz++) {
	    ofg->tt[ix  ][iz  ] += ti[ix][iz];
	    ofg->tt[ix+1][iz  ] += ti[ix][iz];
	    ofg->tt[ix  ][iz+1] += ti[ix][iz];
	    ofg->tt[ix+1][iz+1] += ti[ix][iz];
	}
    }   

    /* copy to input array */
    for     (ix=0;ix<fdm->nx;ix++) {
	for (iz=0;iz<fdm->nz;iz++) {
	    ti[ix][iz] = 0.25 * ofg->tt[ix][iz];
	}
    }
}

/*------------------------------------------------------------*/
void expand(float** a, 
	    float** b, 
	    fdm2d fdm)
/*< expand domain >*/
{
    int iz,ix;

    for     (ix=0;ix<fdm->nx;ix++) {
	for (iz=0;iz<fdm->nz;iz++) {
	    b[fdm->nb+ix][fdm->nb+iz] = a[ix][iz];
	}
    }

    for     (ix=0; ix<fdm->nxpad; ix++) {
	for (iz=0; iz<fdm->nb;    iz++) {
	    b[ix][           iz  ] = b[ix][           fdm->nb  ];
	    b[ix][fdm->nzpad-iz-1] = b[ix][fdm->nzpad-fdm->nb-1];
	}
    }

    for     (ix=0; ix<fdm->nb;    ix++) {
	for (iz=0; iz<fdm->nzpad; iz++) {
	    b[           ix  ][iz] = b[           fdm->nb  ][iz];
	    b[fdm->nxpad-ix-1][iz] = b[fdm->nxpad-fdm->nb-1][iz];
	}
    }
}

/*------------------------------------------------------------*/
void expand3d(float ***a, 
	      float ***b, 
	      fdm3d  fdm)
/*< expand domain >*/
{
    int iz,ix,i3;

    for         (i3=0;i3<fdm->ny;i3++) {
	for     (ix=0;ix<fdm->nx;ix++) {
	    for (iz=0;iz<fdm->nz;iz++) {
		b[fdm->nb+i3][fdm->nb+ix][fdm->nb+iz] = a[i3][ix][iz];
	    }
	}
    }

    for         (i3=0; i3<fdm->nypad; i3++) {
	for     (ix=0; ix<fdm->nxpad; ix++) {
	    for (iz=0; iz<fdm->nb;    iz++) {
		b[i3][ix][           iz  ] = b[i3][ix][           fdm->nb  ];
		b[i3][ix][fdm->nzpad-iz-1] = b[i3][ix][fdm->nzpad-fdm->nb-1];
	    }
	}
    }


    for         (i3=0; i3<fdm->nypad; i3++) {
	for     (ix=0; ix<fdm->nb;    ix++) {
	    for (iz=0; iz<fdm->nzpad; iz++) {
		b[i3][           ix  ][iz] = b[i3][           fdm->nb  ][iz];
		b[i3][fdm->nxpad-ix-1][iz] = b[i3][fdm->nxpad-fdm->nb-1][iz];
	    }
	}
    }

    for         (i3=0; i3<fdm->nb;    i3++) {
	for     (ix=0; ix<fdm->nxpad; ix++) {
	    for (iz=0; iz<fdm->nzpad; iz++) {
		b[           i3  ][ix][iz] = b[           fdm->nb  ][ix][iz];
		b[fdm->nypad-i3-1][ix][iz] = b[fdm->nypad-fdm->nb-1][ix][iz];
	    }
	}
    }

}


/*------------------------------------------------------------*/
void cut2d(float**  a,
	   float**  b,
	   fdm2d  fdm,
	   sf_axis c1, 
	   sf_axis c2)
/*< cut a rectangular wavefield subset >*/
{
    int iz,ix;
    int fz,fx;

    fz = (floor)((sf_o(c1)-fdm->ozpad)/fdm->dz);
    fx = (floor)((sf_o(c2)-fdm->oxpad)/fdm->dx);

    for     (ix=0;ix<sf_n(c2);ix++) {
	for (iz=0;iz<sf_n(c1);iz++) {
	    b[ix][iz] = a[fx+ix][fz+iz];
	}
    }
}

/*------------------------------------------------------------*/
void cut3d(float*** a,
	   float*** b,
	   fdm3d  fdm,
	   sf_axis c1, 
	   sf_axis c2,
	   sf_axis c3)
/*< cut a rectangular wavefield subset >*/
{
    int iz,ix,iy;
    int fz,fx,fy;

    fz = (floor)((sf_o(c1)-fdm->ozpad)/fdm->dz);
    fx = (floor)((sf_o(c2)-fdm->oxpad)/fdm->dx);
    fy = (floor)((sf_o(c3)-fdm->oypad)/fdm->dy);

    for         (iy=0;iy<sf_n(c3);iy++) {
	for     (ix=0;ix<sf_n(c2);ix++) {
	    for (iz=0;iz<sf_n(c1);iz++) {
		b[iy][ix][iz] = a[fy+iy][fx+ix][fz+iz];
	    }
	}
    }
}

/*------------------------------------------------------------*/
void bfill(float** b, 
	   fdm2d fdm)
/*< fill boundaries >*/
{
    int iz,ix;
    
    for     (ix=0; ix<fdm->nxpad; ix++) {
	for (iz=0; iz<fdm->nb;    iz++) {
	    b[ix][           iz  ] = b[ix][           fdm->nb  ];
	    b[ix][fdm->nzpad-iz-1] = b[ix][fdm->nzpad-fdm->nb-1];
	}
    }
    
    for     (ix=0; ix<fdm->nb;    ix++) {
	for (iz=0; iz<fdm->nzpad; iz++) {
	    b[           ix  ][iz] = b[           fdm->nb  ][iz];
	    b[fdm->nxpad-ix-1][iz] = b[fdm->nxpad-fdm->nb-1][iz];
	}
    }
}

/*------------------------------------------------------------*/
lint2d lint2d_make(int    na, 
		   pt2d*  aa, 
		   fdm2d fdm)
/*< init 2D linear interpolation >*/
{
    lint2d ca;
    int    ia;
    float f1,f2;
    
    ca = (lint2d) sf_alloc(1,sizeof(*ca));

    ca->n = na;

    ca->w00 = sf_floatalloc(na);
    ca->w01 = sf_floatalloc(na);
    ca->w10 = sf_floatalloc(na);
    ca->w11 = sf_floatalloc(na);

    ca->jz  = sf_intalloc(na);
    ca->jx  = sf_intalloc(na);

    for (ia=0;ia<na;ia++) {
	
	if(aa[ia].z >= fdm->ozpad && 
	   aa[ia].z <  fdm->ozpad + (fdm->nzpad-1)*fdm->dz &&
	   aa[ia].x >= fdm->oxpad && 
	   aa[ia].x <  fdm->oxpad + (fdm->nxpad-1)*fdm->dx   ) {
	    
	    ca->jz[ia] = (int)( (aa[ia].z-fdm->ozpad)/fdm->dz);
	    ca->jx[ia] = (int)( (aa[ia].x-fdm->oxpad)/fdm->dx);
	    
	    f1 = (aa[ia].z-fdm->ozpad)/fdm->dz - ca->jz[ia];
	    f2 = (aa[ia].x-fdm->oxpad)/fdm->dx - ca->jx[ia];
	} else {
        sf_warning("YOUR SOURCES ARE OUTSIDE OF THE GRID!!!\n");
	    ca->jz[ia] = 0; 
	    ca->jx[ia] = 0;
	    
	    f1 = 1; 
	    f2 = 0;
	}

	ca->w00[ia] = (1-f1)*(1-f2);
	ca->w01[ia] = (  f1)*(1-f2);
	ca->w10[ia] = (1-f1)*(  f2);
	ca->w11[ia] = (  f1)*(  f2);

    }

    return ca;
}

/*------------------------------------------------------------*/
lint3d lint3d_make(int    na, 
		   pt3d*  aa, 
		   fdm3d fdm)
/*< init 3D linear interpolation >*/
{
    lint3d ca;
    int    ia;
    float f1,f2,f3;
    
    ca = (lint3d) sf_alloc(1,sizeof(*ca));

    ca->n = na;

    ca->w000 = sf_floatalloc(na);
    ca->w001 = sf_floatalloc(na);
    ca->w010 = sf_floatalloc(na);
    ca->w011 = sf_floatalloc(na);
    ca->w100 = sf_floatalloc(na);
    ca->w101 = sf_floatalloc(na);
    ca->w110 = sf_floatalloc(na);
    ca->w111 = sf_floatalloc(na);

    ca->jz  = sf_intalloc(na);
    ca->jx  = sf_intalloc(na);
    ca->jy  = sf_intalloc(na);

    for (ia=0;ia<na;ia++) {
	
	if(aa[ia].z >= fdm->ozpad && 
	   aa[ia].z <  fdm->ozpad + (fdm->nzpad-1)*fdm->dz &&
	   aa[ia].x >= fdm->oxpad && 
	   aa[ia].x <  fdm->oxpad + (fdm->nxpad-1)*fdm->dx &&   
	   aa[ia].y >= fdm->oypad && 
	   aa[ia].y <  fdm->oypad + (fdm->nypad-1)*fdm->dy  ) {
	    
	    ca->jz[ia] = (int)( (aa[ia].z-fdm->ozpad)/fdm->dz);
	    ca->jx[ia] = (int)( (aa[ia].x-fdm->oxpad)/fdm->dx);
	    ca->jy[ia] = (int)( (aa[ia].y-fdm->oypad)/fdm->dy);
	    
	    f1 = (aa[ia].z-fdm->ozpad)/fdm->dz - ca->jz[ia];
	    f2 = (aa[ia].x-fdm->oxpad)/fdm->dx - ca->jx[ia];
	    f3 = (aa[ia].y-fdm->oypad)/fdm->dy - ca->jy[ia];

	} else {
	    ca->jz[ia] = 0; 
	    ca->jx[ia] = 0;
	    ca->jy[ia] = 0;
	    
	    f1 = 1; 
	    f2 = 0;
	    f3 = 0;
	}

	ca->w000[ia] = (1-f3)*(1-f1)*(1-f2);
	ca->w001[ia] = (1-f3)*(  f1)*(1-f2);
	ca->w010[ia] = (1-f3)*(1-f1)*(  f2);
	ca->w011[ia] = (1-f3)*(  f1)*(  f2);

	ca->w100[ia] = (  f3)*(1-f1)*(1-f2);
	ca->w101[ia] = (  f3)*(  f1)*(1-f2);
	ca->w110[ia] = (  f3)*(1-f1)*(  f2);
	ca->w111[ia] = (  f3)*(  f1)*(  f2);
    }

    return ca;
}


/*------------------------------------------------------------*/
void lint2d_hold(float**uu,
		 float *ww,
		 lint2d ca)
/*< hold fixed value in field >*/
{
    int   ia;
    float wa;

#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic,1) private(ia,wa) shared(ca,ww,uu)
#endif
    for (ia=0;ia<ca->n;ia++) {
	wa = ww[ia];
	
	uu[ ca->jx[ia]   ][ ca->jz[ia]   ] = wa;
	uu[ ca->jx[ia]   ][ ca->jz[ia]+1 ] = wa;
	uu[ ca->jx[ia]+1 ][ ca->jz[ia]   ] = wa;
	uu[ ca->jx[ia]+1 ][ ca->jz[ia]+1 ] = wa;
    }
}

/*------------------------------------------------------------*/
void lint2d_inject(float**uu,
		   float *ww,
		   lint2d ca)
/*< inject into wavefield >*/
{
    int   ia;
    float wa;

#ifdef _OPENMP
#pragma omp parallel for \
    schedule(dynamic,1) \
    private(ia,wa) \
    shared(ca,ww,uu)
#endif
    for (ia=0;ia<ca->n;ia++) {
	wa = ww[ia];
	
	uu[ ca->jx[ia]   ][ ca->jz[ia]   ] -= wa * ca->w00[ia];
	uu[ ca->jx[ia]   ][ ca->jz[ia]+1 ] -= wa * ca->w01[ia];
	uu[ ca->jx[ia]+1 ][ ca->jz[ia]   ] -= wa * ca->w10[ia];
	uu[ ca->jx[ia]+1 ][ ca->jz[ia]+1 ] -= wa * ca->w11[ia];
    }
}

/*------------------------------------------------------------*/
void lint3d_inject(float***uu,
		   float  *ww,
		   lint3d  ca)
/*< inject into wavefield >*/
{
    int   ia;
    float wa;

#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic,1) private(ia,wa) shared(ca,ww,uu)
#endif
    for (ia=0;ia<ca->n;ia++) {
	wa = ww[ia];
	
	uu[ ca->jy[ia]   ][ ca->jx[ia]   ][ ca->jz[ia]   ] -= wa * ca->w000[ia];
	uu[ ca->jy[ia]   ][ ca->jx[ia]   ][ ca->jz[ia]+1 ] -= wa * ca->w001[ia];
	uu[ ca->jy[ia]   ][ ca->jx[ia]+1 ][ ca->jz[ia]   ] -= wa * ca->w010[ia];
	uu[ ca->jy[ia]   ][ ca->jx[ia]+1 ][ ca->jz[ia]+1 ] -= wa * ca->w011[ia];
	uu[ ca->jy[ia]+1 ][ ca->jx[ia]   ][ ca->jz[ia]   ] -= wa * ca->w100[ia];
	uu[ ca->jy[ia]+1 ][ ca->jx[ia]   ][ ca->jz[ia]+1 ] -= wa * ca->w101[ia];
	uu[ ca->jy[ia]+1 ][ ca->jx[ia]+1 ][ ca->jz[ia]   ] -= wa * ca->w110[ia];
	uu[ ca->jy[ia]+1 ][ ca->jx[ia]+1 ][ ca->jz[ia]+1 ] -= wa * ca->w111[ia];
    }
}

/*------------------------------------------------------------*/
void lint2d_inject1(float**uu,
		    float  ww,
		    lint2d ca)
/*< inject into wavefield >*/
{
    int   ia;

#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic,1) private(ia) shared(ca,ww,uu)
#endif
    for (ia=0;ia<ca->n;ia++) {

	uu[ ca->jx[ia]   ][ ca->jz[ia]   ] -= ww * ca->w00[ia];
	uu[ ca->jx[ia]   ][ ca->jz[ia]+1 ] -= ww * ca->w01[ia];
	uu[ ca->jx[ia]+1 ][ ca->jz[ia]   ] -= ww * ca->w10[ia];
	uu[ ca->jx[ia]+1 ][ ca->jz[ia]+1 ] -= ww * ca->w11[ia];
    }
}

/*------------------------------------------------------------*/
void lint3d_inject1(float***uu,
		    float   ww,
		    lint3d  ca)
/*< inject into wavefield >*/
{
    int   ia;

#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic,1) private(ia) shared(ca,ww,uu)
#endif
    for (ia=0;ia<ca->n;ia++) {

	uu[ ca->jy[ia]   ][ ca->jx[ia]   ][ ca->jz[ia]   ] -= ww * ca->w000[ia];
	uu[ ca->jy[ia]   ][ ca->jx[ia]   ][ ca->jz[ia]+1 ] -= ww * ca->w001[ia];
	uu[ ca->jy[ia]   ][ ca->jx[ia]+1 ][ ca->jz[ia]   ] -= ww * ca->w010[ia];
	uu[ ca->jy[ia]   ][ ca->jx[ia]+1 ][ ca->jz[ia]+1 ] -= ww * ca->w011[ia];
	uu[ ca->jy[ia]+1 ][ ca->jx[ia]   ][ ca->jz[ia]   ] -= ww * ca->w100[ia];
	uu[ ca->jy[ia]+1 ][ ca->jx[ia]   ][ ca->jz[ia]+1 ] -= ww * ca->w101[ia];
	uu[ ca->jy[ia]+1 ][ ca->jx[ia]+1 ][ ca->jz[ia]   ] -= ww * ca->w110[ia];
	uu[ ca->jy[ia]+1 ][ ca->jx[ia]+1 ][ ca->jz[ia]+1 ] -= ww * ca->w111[ia];
    }
}

/*------------------------------------------------------------*/
void lint2d_extract(float**uu,
		    float* dd,
		    lint2d ca)
/*< extract from wavefield >*/
{
    int ia;

#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic,1) private(ia) shared(ca,dd,uu)
#endif
    for (ia=0;ia<ca->n;ia++) {
	dd[ia] =
	    uu[ ca->jx[ia]  ][ ca->jz[ia]  ] * ca->w00[ia] +
	    uu[ ca->jx[ia]  ][ ca->jz[ia]+1] * ca->w01[ia] +
	    uu[ ca->jx[ia]+1][ ca->jz[ia]  ] * ca->w10[ia] +
	    uu[ ca->jx[ia]+1][ ca->jz[ia]+1] * ca->w11[ia];
    }
}  

void lint3d_extract(float***uu,
		    float  *dd,
		    lint3d  ca)
/*< extract from wavefield >*/
{
    int ia;

#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic,1) private(ia) shared(ca,dd,uu)
#endif
    for (ia=0;ia<ca->n;ia++) {
	dd[ia] =
	    uu[ ca->jy[ia]  ][ ca->jx[ia]  ][ ca->jz[ia]  ] * ca->w000[ia] +
	    uu[ ca->jy[ia]  ][ ca->jx[ia]  ][ ca->jz[ia]+1] * ca->w001[ia] +
	    uu[ ca->jy[ia]  ][ ca->jx[ia]+1][ ca->jz[ia]  ] * ca->w010[ia] +
	    uu[ ca->jy[ia]  ][ ca->jx[ia]+1][ ca->jz[ia]+1] * ca->w011[ia] +
	    uu[ ca->jy[ia]+1][ ca->jx[ia]  ][ ca->jz[ia]  ] * ca->w100[ia] +
	    uu[ ca->jy[ia]+1][ ca->jx[ia]  ][ ca->jz[ia]+1] * ca->w101[ia] +
	    uu[ ca->jy[ia]+1][ ca->jx[ia]+1][ ca->jz[ia]  ] * ca->w110[ia] +
	    uu[ ca->jy[ia]+1][ ca->jx[ia]+1][ ca->jz[ia]+1] * ca->w111[ia];
    }
}  


/*------------------------------------------------------------*/
void fdbell_init(int n)
/*< init bell taper >*/
{
    int   iz,ix;
    float s;

    nbell = n;
    s = 0.5*nbell;

    bell=sf_floatalloc2(2*nbell+1,2*nbell+1);

    for    (ix=-nbell;ix<=nbell;ix++) {
	for(iz=-nbell;iz<=nbell;iz++) {
	    bell[nbell+ix][nbell+iz] = exp(-(iz*iz+ix*ix)/s);
	}
    }    
}

/*------------------------------------------------------------*/
void fdbell3d_init(int n)
/*< init bell taper >*/
{
    int   iz,ix,i3;
    float s;

    nbell = n;
    s = 0.5*nbell;

    bell3d=sf_floatalloc3(2*nbell+1,2*nbell+1,2*nbell+1);

    for        (i3=-nbell;i3<=nbell;i3++) {
	for    (ix=-nbell;ix<=nbell;ix++) {
	    for(iz=-nbell;iz<=nbell;iz++) {
		bell3d[nbell+i3][nbell+ix][nbell+iz] = exp(-(iz*iz+ix*ix+i3*i3)/s);
	    }
	}    
    }
}

/*------------------------------------------------------------*/
void lint2d_bell(float**uu,
		 float *ww,
		 lint2d ca)
/*< apply bell taper >*/
{
    int   ia,iz,ix;
    float wa;

    for    (ix=-nbell;ix<=nbell;ix++) {
	for(iz=-nbell;iz<=nbell;iz++) {
	    
	    for (ia=0;ia<ca->n;ia++) {
		wa = ww[ia] * bell[nbell+ix][nbell+iz];

		uu[ ix+ca->jx[ia]   ][ iz+ca->jz[ia]   ] -= wa * ca->w00[ia];
		uu[ ix+ca->jx[ia]   ][ iz+ca->jz[ia]+1 ] -= wa * ca->w01[ia];
		uu[ ix+ca->jx[ia]+1 ][ iz+ca->jz[ia]   ] -= wa * ca->w10[ia];
		uu[ ix+ca->jx[ia]+1 ][ iz+ca->jz[ia]+1 ] -= wa * ca->w11[ia];
	    }

	}
    }
}

/*------------------------------------------------------------*/
void lint3d_bell(float***uu,
		 float  *ww,
		 lint3d  ca)
/*< apply bell taper >*/
{
    int   ia,iz,ix,iy;
    float wa;

    for        (iy=-nbell;iy<=nbell;iy++) {
	for    (ix=-nbell;ix<=nbell;ix++) {
	    for(iz=-nbell;iz<=nbell;iz++) {
		
		for (ia=0;ia<ca->n;ia++) {
		    wa = ww[ia] * bell3d[nbell+iy][nbell+ix][nbell+iz];
		    
		    uu[ iy+ca->jy[ia]   ][ ix+ca->jx[ia]   ][ iz+ca->jz[ia]   ] -= wa * ca->w000[ia];
		    uu[ iy+ca->jy[ia]   ][ ix+ca->jx[ia]   ][ iz+ca->jz[ia]+1 ] -= wa * ca->w001[ia];
		    uu[ iy+ca->jy[ia]   ][ ix+ca->jx[ia]+1 ][ iz+ca->jz[ia]   ] -= wa * ca->w010[ia];
		    uu[ iy+ca->jy[ia]   ][ ix+ca->jx[ia]+1 ][ iz+ca->jz[ia]+1 ] -= wa * ca->w011[ia];
		    uu[ iy+ca->jy[ia]+1 ][ ix+ca->jx[ia]   ][ iz+ca->jz[ia]   ] -= wa * ca->w100[ia];
		    uu[ iy+ca->jy[ia]+1 ][ ix+ca->jx[ia]   ][ iz+ca->jz[ia]+1 ] -= wa * ca->w101[ia];
		    uu[ iy+ca->jy[ia]+1 ][ ix+ca->jx[ia]+1 ][ iz+ca->jz[ia]   ] -= wa * ca->w110[ia];
		    uu[ iy+ca->jy[ia]+1 ][ ix+ca->jx[ia]+1 ][ iz+ca->jz[ia]+1 ] -= wa * ca->w111[ia];
		}
		
	    }
	}
    }
}

/*------------------------------------------------------------*/
abcone2d abcone2d_make(int     nop,
		       float    dt,
		       float**  vv,
		       bool   free, 
		       fdm2d   fdm)
/*< init 2D ABC >*/

/*This absorbing boundary condition follows the work done in
Absorbing Boundary Conditions , by Robert Clayton and Bjorn Engquist

Which can be found at:

http://sepwww.stanford.edu/public/docs/sep11/11_12_abs.html
*/
{
    abcone2d abc;
    int iz,ix;
    float d;

    abc = (abcone2d) sf_alloc(1,sizeof(*abc));

    abc->free = free;

    abc->bzl = sf_floatalloc(fdm->nxpad);
    abc->bzh = sf_floatalloc(fdm->nxpad);
    abc->bxl = sf_floatalloc(fdm->nzpad);
    abc->bxh = sf_floatalloc(fdm->nzpad);

    for (ix=0;ix<fdm->nxpad;ix++) {
	d = vv[ix][           nop  ] *dt/fdm->dz; abc->bzl[ix] = (1-d)/(1+d);
	d = vv[ix][fdm->nzpad-nop-1] *dt/fdm->dz; abc->bzh[ix] = (1-d)/(1+d);
    }
    for (iz=0;iz<fdm->nzpad;iz++) {
	d = vv[           nop  ][iz] *dt/fdm->dx; abc->bxl[iz] = (1-d)/(1+d);
	d = vv[fdm->nxpad-nop-1][iz] *dt/fdm->dx; abc->bxh[iz] = (1-d)/(1+d);
    }

    return abc;
}

/*------------------------------------------------------------*/
abcone3d abcone3d_make(int     nop,
		       float    dt,
		       float ***vv,
		       bool   free, 
		       fdm3d   fdm)
/*< init 3D ABC >*/

/*This absorbing boundary condition follows the work done in
Absorbing Boundary Conditions , by Robert Clayton and Bjorn Engquist

Which can be found at:

http://sepwww.stanford.edu/public/docs/sep11/11_12_abs.html
*/

{
    abcone3d abc;
    int iz,ix,iy;
    float d;

    abc = (abcone3d) sf_alloc(1,sizeof(*abc));

    abc->free = free;

    /* z */
    abc->bzl = sf_floatalloc2(fdm->nxpad,fdm->nypad);
    abc->bzh = sf_floatalloc2(fdm->nxpad,fdm->nypad);

    /* x */
    abc->bxl = sf_floatalloc2(fdm->nzpad,fdm->nypad);
    abc->bxh = sf_floatalloc2(fdm->nzpad,fdm->nypad);

    /* y */
    abc->byl = sf_floatalloc2(fdm->nzpad,fdm->nxpad);
    abc->byh = sf_floatalloc2(fdm->nzpad,fdm->nxpad);

    for     (iy=0;iy<fdm->nypad;iy++) {
	for (ix=0;ix<fdm->nxpad;ix++) {
	    d = vv[iy][ix][           nop  ] *dt/fdm->dz; abc->bzl[iy][ix] = (1-d)/(1+d);
	    d = vv[iy][ix][fdm->nzpad-nop-1] *dt/fdm->dz; abc->bzh[iy][ix] = (1-d)/(1+d);
	}
    }

    for     (iy=0;iy<fdm->nypad;iy++) {
	for (iz=0;iz<fdm->nzpad;iz++) {
	    d = vv[iy][           nop  ][iz] *dt/fdm->dx; abc->bxl[iy][iz] = (1-d)/(1+d);
	    d = vv[iy][fdm->nxpad-nop-1][iz] *dt/fdm->dx; abc->bxh[iy][iz] = (1-d)/(1+d);
	}
    }
    
    for     (ix=0;ix<fdm->nxpad;ix++) {
	for (iz=0;iz<fdm->nzpad;iz++) {
	    d = vv[           nop  ][ix][iz] *dt/fdm->dy; abc->byl[ix][iz] = (1-d)/(1+d);
	    d = vv[fdm->nypad-nop-1][ix][iz] *dt/fdm->dy; abc->byh[ix][iz] = (1-d)/(1+d);
	}
    }

    return abc;
}


/*------------------------------------------------------------*/
void abcone2d_apply(float**   uo,
		    float**   um,
		    int      nop,
		    abcone2d abc,
		    fdm2d    fdm)
/*< apply 2D ABC >*/
{
    int iz,ix,iop;

#ifdef _OPENMP
#pragma omp parallel for			\
    schedule(dynamic,1)				\
    private(iz,ix,iop)				\
    shared(fdm,nop,uo,um,abc)
#endif
    for(ix=0;ix<fdm->nxpad;ix++) {
	for(iop=0;iop<nop;iop++) {

	    /* top BC */
	    if(!abc->free) { /* not free surface, apply ABC */
		iz = nop-iop;
		uo      [ix][iz  ] 
		    = um[ix][iz+1] 
		    +(um[ix][iz  ]
		      - uo[ix][iz+1]) * abc->bzl[ix];
	    }

	    /* bottom BC */
	    iz = fdm->nzpad-nop+iop-1;
	    uo      [ix][iz  ] 
		= um[ix][iz-1]
		+(um[ix][iz  ]
		- uo[ix][iz-1]) * abc->bzh[ix];
	}
    }

#ifdef _OPENMP
#pragma omp parallel for			\
    schedule(dynamic,1)				\
    private(iz,ix,iop)				\
    shared(fdm,nop,uo,um,abc)
#endif
    for(iz=0;iz<fdm->nzpad;iz++) {
	for(iop=0;iop<nop;iop++) {

	    /* left BC */
	    ix = nop-iop;
	    uo      [ix  ][iz] 
		= um[ix+1][iz] 
		+(um[ix  ][iz]
		- uo[ix+1][iz]) * abc->bxl[iz];

	    /* right BC */
	    ix = fdm->nxpad-nop+iop-1;
	    uo      [ix  ][iz] 
		= um[ix-1][iz]
		+(um[ix  ][iz]
		- uo[ix-1][iz]) * abc->bxh[iz];
	}
    }
}

/*------------------------------------------------------------*/
void abcone3d_apply(float  ***uo,
		    float  ***um,
		    int      nop,
		    abcone3d abc,
		    fdm3d    fdm)
/*< apply 3D ABC >*/
{
    int iz,ix,iy,iop;

#ifdef _OPENMP
#pragma omp parallel for			\
    schedule(dynamic,1)				\
    private(iz,ix,iy,iop)			\
    shared(fdm,nop,uo,um,abc)
#endif

    for    (iy=0;iy<fdm->nypad;iy++) {
	for(ix=0;ix<fdm->nxpad;ix++) {
	    for(iop=0;iop<nop;iop++) {
		
		/* z min */
		if(!abc->free) { /* not free surface, apply ABC */
		    iz = nop-iop;
		    uo      [iy][ix][iz  ] 
			= um[iy][ix][iz+1] 
			+(um[iy][ix][iz  ]
			- uo[iy][ix][iz+1]) * abc->bzl[iy][ix];
		}
		
		/* z max */
		iz = fdm->nzpad-nop+iop-1;
		uo      [iy][ix][iz  ] 
		    = um[iy][ix][iz-1]
		    +(um[iy][ix][iz  ]
		    - uo[iy][ix][iz-1]) * abc->bzh[iy][ix];
	    }
	}
    }
    
#ifdef _OPENMP
#pragma omp parallel for			\
    schedule(dynamic,1)				\
    private(iz,ix,iy,iop)			\
    shared(fdm,nop,uo,um,abc)
#endif

    for    (iy=0;iy<fdm->nypad;iy++) {
	for(iz=0;iz<fdm->nzpad;iz++) {
	    for(iop=0;iop<nop;iop++) {
		
		/* x min */
		ix = nop-iop;
		uo      [iy][ix  ][iz] 
		    = um[iy][ix+1][iz] 
		    +(um[iy][ix  ][iz]
		    - uo[iy][ix+1][iz]) * abc->bxl[iy][iz];
		
		/* x max */
		ix = fdm->nxpad-nop+iop-1;
		uo      [iy][ix  ][iz] 
		    = um[iy][ix-1][iz]
		    +(um[iy][ix  ][iz]
		    - uo[iy][ix-1][iz]) * abc->bxh[iy][iz];
	    }
	}
    }

#ifdef _OPENMP
#pragma omp parallel for			\
    schedule(dynamic,1)				\
    private(iz,ix,iy,iop)			\
    shared(fdm,nop,uo,um,abc)
#endif

    for    (ix=0;ix<fdm->nxpad;ix++) {
	for(iz=0;iz<fdm->nzpad;iz++) {
	    for(iop=0;iop<nop;iop++) {
		
		/* y min */
		iy = nop-iop;
		uo      [iy  ][ix][iz] 
		    = um[iy+1][ix][iz] 
		    +(um[iy  ][ix][iz]
		    - uo[iy+1][ix][iz]) * abc->byl[ix][iz];
		
		/* y max */
		iy = fdm->nypad-nop+iop-1;
		uo      [iy  ][ix][iz] 
		    = um[iy-1][ix][iz]
		    +(um[iy  ][ix][iz]
		    - uo[iy-1][ix][iz]) * abc->byh[ix][iz];
	    }
	}
    }

}


/*------------------------------------------------------------*/
sponge sponge_make(int nb)
/*< init boundary sponge >*/

/* Sponge boundary conditions multiply incoming wavefields
by smaller coefficients to attenuate the wavefield over time and space.

The sponge coefficients need to deviate from 1 very gradually to ensure
that there are no induced reflections caused by large impedance 
contrasts */
{
    sponge spo;
    int   ib;
    float sb,fb;
    
    spo = (sponge) sf_alloc(1,sizeof(*spo));    
    spo->w = sf_floatalloc(nb);
    sb = 4.0*nb;               
    for(ib=0; ib<nb; ib++) {
	fb = ib/(sqrt(2.0)*sb);
	spo->w[ib] = exp(-fb*fb);
    }
    return spo;
}

/*------------------------------------------------------------*/
void sponge2d_apply(float**   uu,
		    sponge   spo,
		    fdm2d    fdm)
/*< apply boundary sponge >*/
{
    int iz,ix,ib,ibz,ibx;
    float w;

#ifdef _OPENMP
#pragma omp parallel for			\
    schedule(dynamic,1)				\
    private(ib,iz,ix,ibz,ibx,w)			\
    shared(fdm,uu)
#endif
    for(ib=0; ib<fdm->nb; ib++) {
	w = spo->w[fdm->nb-ib-1];

	ibz = fdm->nzpad-ib-1;
	for(ix=0; ix<fdm->nxpad; ix++) {
//        fprintf(stderr,"Value before: %f\n",uu[ix][ib]);
	    uu[ix][ib ] *= w; /*    top sponge */
	    uu[ix][ibz] *= w; /* bottom sponge */
 //       fprintf(stderr,"Value after: %f\n",uu[ix][ib]);
	}

	ibx = fdm->nxpad-ib-1;
	for(iz=0; iz<fdm->nzpad; iz++) {
	    uu[ib ][iz] *= w; /*   left sponge */
	    uu[ibx][iz] *= w; /*  right sponge */
	}

    }
}

void sponge2d_apply_test(float**   uu,
		    sponge   spo,
		    fdm2d    fdm)
/*< apply boundary sponge >*/
{
    int iz,ix,ib,ibz,ibx;
    float w;

#ifdef _OPENMP
#pragma omp parallel for			\
    schedule(dynamic,1)				\
    private(ib,iz,ix,ibz,ibx,w)			\
    shared(fdm,uu)
#endif
    for(ib=0; ib<fdm->nb; ib++) {
	w = spo->w[ib];

	ibz = fdm->nzpad-ib-1;
	for(ix=0; ix<fdm->nxpad; ix++) {
	    uu[ix][ib ] *= w; /*    top sponge */
	    uu[ix][ibz] *= w; /* bottom sponge */
	}

	ibx = fdm->nxpad-ib-1;
	for(iz=0; iz<fdm->nzpad; iz++) {
	    uu[ib ][iz] *= w; /*   left sponge */
	    uu[ibx][iz] *= w; /*  right sponge */
	}

    }
}

/*------------------------------------------------------------*/
void sponge3d_apply(float  ***uu,
		    sponge   spo,
		    fdm3d    fdm)
/*< apply boundary sponge >*/
{
    int iz,ix,iy,ib,ibz,ibx,iby;
    float w;

#ifdef _OPENMP
#pragma omp parallel for			\
    schedule(dynamic,1)				\
    private(ib,iz,ix,iy,ibz,ibx,iby,w)		\
    shared(fdm,uu)
#endif
    for(ib=0; ib<fdm->nb; ib++) {
	w = spo->w[fdm->nb-ib-1];

	ibz = fdm->nzpad-ib-1;
	for    (iy=0; iy<fdm->nypad; iy++) {
	    for(ix=0; ix<fdm->nxpad; ix++) {
		uu[iy][ix][ib ] *= w; /* z min */
		uu[iy][ix][ibz] *= w; /* z max */
	    }
	}

	ibx = fdm->nxpad-ib-1;
	for    (iy=0; iy<fdm->nypad; iy++) {
	    for(iz=0; iz<fdm->nzpad; iz++) {
		uu[iy][ib ][iz] *= w; /* x min */
		uu[iy][ibx][iz] *= w; /* x max */
	    }
	}
	
	iby = fdm->nypad-ib-1;
	for    (ix=0; ix<fdm->nxpad; ix++) {
	    for(iz=0; iz<fdm->nzpad; iz++) {
		uu[ib ][ix][iz] *= w; /* x min */
		uu[iby][ix][iz] *= w; /* x max */
	    }
	}

    }
} 

float min(float x, float y){
    if (x < y) return x;
    else return y;
}

bool cfl_generic(
    float vpmin, float vpmax, 
    float dx, float dy, float dz,
    float dt, float fmax, float safety, 
    int intervals, char *wave) 
/*< cfl check for both 2d and 3d acoustic fdcode >*/
{ 
    int dim = 3;
    float dmin;

    if (dy < 0)dim = 2;  
    
    if (dim == 2) dmin = min(dx,dz); 
    else dmin = min(dx,min(dy,dz)); 
    
    float tdp = dt * vpmax *sqrt(2); // maximum distance
    if (dmin > tdp) sf_warning("CFL: Stability check ... %s-wave... PASSED", wave);
    else { 
        sf_error("CFL: Stability check ... FAILED ... minimum grid sampling: %f !> %f", dmin, tdp); 
    } 
    float wplength = safety*vpmin / fmax;
  
    bool passed;
    if (dim == 2) passed = wplength > intervals*sqrt(dx*dx+dz*dz);
    else passed = wplength > intervals*sqrt(dx*dx+dy*dy+dz*dz); 
    
    if (passed) {
        sf_warning("CFL: Accuracy check ... %s-wave ... PASSED", wave); 
    }
    else {  
        if (dim == 2) {
            sf_warning("CFL: fmax must be less than < %f",
                safety*vpmin/intervals/sqrt(dx*dx+dz*dz)); 
        }
        else {
            sf_warning("CFL: fmax must be less than < %f",
                safety*vpmin/intervals/sqrt(dx*dx+dy*dy+dz*dz));
            sf_error("CFL: Accuracy check ... %s-wave ... FAILED",wave);   
        }
    }
    return true;
}

bool cfl_elastic(
    float vpmin, float vpmax, 
    float vsmin, float vsmax,
    float dx, float dy, float dz,
    float dt, float fmax, float safety, 
    int intervals) 
/*< cfl check for both 2d and 3d elastic fdcode >*/
{
    bool ppass = cfl_generic(vpmin,vpmax,
                              dx,dy,dz,dt,
                              fmax,safety,intervals,"P");
    bool spass = cfl_generic(vsmin,vsmax,
                              dx,dy,dz,dt,
                              fmax,safety,intervals,"S");

    return ppass && spass;
}

bool cfl_acoustic(
    float vpmin, float vpmax,
    float dx, float dy, float dz,
    float dt, float fmax, float safety,
    int intervals)
/*< cfl check for acoustic wave equation >*/
{
    bool ppass = cfl_generic(vpmin,vpmax,
                              dx,dy,dz,dt,
                              fmax,safety,intervals,"P");
    return ppass;
}


/****************/
/* PML ROUTINES */
/****************/
/*------------------------------------------------------------*/
PML2D pml2d_init(fdm2d fdm)
                
/*< initialize the 2D PML >*/
{
    int ix, iz;
    PML2D pml;
    /* sides of the computational domain */
    //float **up;
    //float **down;
    //float **right;
    //float **left;
    /* corners of the computational domain */
    //float **upright;
    //float **downright;
    //float **downleft;
    //float **upleft; 

    pml = (PML2D) sf_alloc(1,sizeof(*pml));
	
	/* sides of the computational domain */
	pml->up    = sf_floatalloc2(fdm->nb, fdm->nxpad);
	pml->down  = sf_floatalloc2(fdm->nb, fdm->nxpad);
    pml->right = sf_floatalloc2(fdm->nzpad, fdm->nb);
    pml->left  = sf_floatalloc2(fdm->nzpad, fdm->nb);
    
    /* corners of the computational domain */
	pml->upright    = sf_floatalloc2(fdm->nb, fdm->nb);
	pml->downright  = sf_floatalloc2(fdm->nb, fdm->nb);
    pml->downleft   = sf_floatalloc2(fdm->nb, fdm->nb);
    pml->upleft     = sf_floatalloc2(fdm->nb, fdm->nb);    

    #ifdef _OPENMP
    #pragma omp parallel private(ix,iz)
    #endif
    {
    #ifdef _OPENMP    
    #pragma omp for \
       schedule(dynamic,fdm->ompchunk)
    #endif
    for (ix=0; ix<fdm->nxpad; ix++){
       for (iz=0; iz<fdm->nb; iz++){
          pml->up[ix][iz]=0;
          pml->down[ix][iz]=0;
       }
    }
    
    #ifdef _OPENMP    
    #pragma omp for \
       schedule(dynamic,fdm->ompchunk)
    #endif
    for (ix=0; ix<fdm->nb; ix++){
       for (iz=0; iz<fdm->nzpad; iz++){
          pml->right[ix][iz]=0;
          pml->left[ix][iz]=0;
       }
    }
    
    #ifdef _OPENMP    
    #pragma omp for \
       schedule(dynamic,fdm->ompchunk)
    #endif
    for (ix=0; ix<fdm->nb; ix++){
       for (iz=0; iz<fdm->nb; iz++){
          pml->upright[ix][iz]=0;
          pml->upleft[ix][iz]=0;
          pml->downright[ix][iz]=0;
          pml->downleft[ix][iz]=0;
       }
    }


	} // end parallel section
	return pml;
}

/*------------------------------------------------------------*/
PML3D pml3d_init(fdm3d fdm)
                
/*< initialize the 3D PML >*/
{

// INITIALIZATION to zeros is still missing!

   PML3D pml;
    
   pml = (PML3D) sf_alloc(1,sizeof(*pml));
   
    pml->Upsizx  = sf_floatalloc3(fdm->nb, fdm->nxpad, fdm->nypad);
    pml->Upsizy  = sf_floatalloc3(fdm->nb, fdm->nxpad, fdm->nypad);
    
    pml->Dpsizx  = sf_floatalloc3(fdm->nb, fdm->nxpad, fdm->nypad);
    pml->Dpsizy  = sf_floatalloc3(fdm->nb, fdm->nxpad, fdm->nypad);
    

    pml->Fpsiyx = sf_floatalloc3(fdm->nzpad, fdm->nxpad, fdm->nb);
    pml->Fpsiyz = sf_floatalloc3(fdm->nzpad, fdm->nxpad, fdm->nb);
    
    pml->Bpsiyx = sf_floatalloc3(fdm->nzpad, fdm->nxpad, fdm->nb);
    pml->Bpsiyz = sf_floatalloc3(fdm->nzpad, fdm->nxpad, fdm->nb);
   
    
    pml->Lpsixy = sf_floatalloc3(fdm->nzpad, fdm->nb, fdm->nypad);
    pml->Lpsixz = sf_floatalloc3(fdm->nzpad, fdm->nb, fdm->nypad);
    
    pml->Rpsixy = sf_floatalloc3(fdm->nzpad, fdm->nb, fdm->nypad);
    pml->Rpsixz = sf_floatalloc3(fdm->nzpad, fdm->nb, fdm->nypad);
    
    
    /*edges*/
    pml->ULPSIzxy = sf_floatalloc3(fdm->nb, fdm->nb, fdm->nypad);
    pml->DLPSIzxy = sf_floatalloc3(fdm->nb, fdm->nb, fdm->nypad);
    
    pml->URPSIzxy = sf_floatalloc3(fdm->nb, fdm->nb, fdm->nypad);
    pml->DRPSIzxy = sf_floatalloc3(fdm->nb, fdm->nb, fdm->nypad);

    pml->UFPSIyzx= sf_floatalloc3(fdm->nb, fdm->nxpad, fdm->nb);
    pml->UBPSIyzx= sf_floatalloc3(fdm->nb, fdm->nxpad, fdm->nb);

    pml->DFPSIyzx= sf_floatalloc3(fdm->nb, fdm->nxpad, fdm->nb);
    pml->DBPSIyzx= sf_floatalloc3(fdm->nb, fdm->nxpad, fdm->nb);

    pml->LFPSIxyz= sf_floatalloc3(fdm->nzpad, fdm->nb, fdm->nb);
    pml->LBPSIxyz= sf_floatalloc3(fdm->nzpad, fdm->nb, fdm->nb);

    pml->RFPSIxyz= sf_floatalloc3(fdm->nzpad, fdm->nb, fdm->nb);
    pml->RBPSIxyz= sf_floatalloc3(fdm->nzpad, fdm->nb, fdm->nb);
     
    pml->ULU = sf_floatalloc3(fdm->nb, fdm->nb, fdm->nypad);
    pml->URU = sf_floatalloc3(fdm->nb, fdm->nb, fdm->nypad);
    pml->DRU = sf_floatalloc3(fdm->nb, fdm->nb, fdm->nypad);
    pml->DLU = sf_floatalloc3(fdm->nb, fdm->nb, fdm->nypad);
    
    pml->UFU = sf_floatalloc3(fdm->nb, fdm->nxpad, fdm->nb);
    pml->UBU = sf_floatalloc3(fdm->nb, fdm->nxpad, fdm->nb);
    pml->DBU = sf_floatalloc3(fdm->nb, fdm->nxpad, fdm->nb);
    pml->DFU = sf_floatalloc3(fdm->nb, fdm->nxpad, fdm->nb);

    pml->LFU = sf_floatalloc3(fdm->nzpad, fdm->nb, fdm->nb);
    pml->RFU = sf_floatalloc3(fdm->nzpad, fdm->nb, fdm->nb);
    pml->RBU = sf_floatalloc3(fdm->nzpad, fdm->nb, fdm->nb);
    pml->LBU = sf_floatalloc3(fdm->nzpad, fdm->nb, fdm->nb);    
    
    /* corners */
    pml->ULFW = sf_floatalloc3(fdm->nb, fdm->nb, fdm->nb);
    pml->URFW = sf_floatalloc3(fdm->nb, fdm->nb, fdm->nb);
    pml->DRFW = sf_floatalloc3(fdm->nb, fdm->nb, fdm->nb);
    pml->DLFW = sf_floatalloc3(fdm->nb, fdm->nb, fdm->nb);
    pml->ULBW = sf_floatalloc3(fdm->nb, fdm->nb, fdm->nb);
    pml->URBW = sf_floatalloc3(fdm->nb, fdm->nb, fdm->nb);
    pml->DRBW = sf_floatalloc3(fdm->nb, fdm->nb, fdm->nb);
    pml->DLBW = sf_floatalloc3(fdm->nb, fdm->nb, fdm->nb);
   
    return pml;
    
}


/*------------------------------------------------------------*/
void pml2d_velApply(float **vz,
                    float **vx,
                    float dt,
                    float *sigma,
                    fdm2d fdm)
/*< Application of the 2D PML to the velocity field >*/
{
/* it's a simple damping of the velocity field:
it DOES NOT require auxiliary variables */
    int ix, iz;
    int nx,nz,nb;

    
    nx = fdm->nxpad;
    nz = fdm->nzpad;
    nb = fdm->nb;    



		/* LEFT SIDE AND RIGHT */
		for    (ix=0; ix<nb; ix++) {
			for(iz=0; iz<nz; iz++) {
		       
				vx[ix][iz] +=  -sigma[ix]*vx[ix][iz]*dt;
				vx[nx-ix-1][iz] += -sigma[ix]*vx[nx-ix-1][iz]*dt;

			}
		}

		/* UP SIDE AND DOWN */
		for    (ix=0; ix<nx; ix++) {
			for(iz=0; iz<nb; iz++) {			
				vz[ix][iz] +=  -sigma[iz]*vz[ix][iz]*dt;
				vz[ix][nz-iz-1] +=  -sigma[iz]*vz[ix][nz-iz-1]*dt;		       
			}
		}
	   	   

}

/*------------------------------------------------------------*/
void pml3d_velApply(float ***vx,
                    float ***vy,
                    float ***vz,
                    float dt,
                    float *sigma,
                    fdm3d fdm)
/*< Application of the 3D PML to the velocity field >*/
{
/* it's a simple damping of the velocity fiel:
it DOES NOT require auxiliary variables */

    int ix, iy, iz;
    int nx, ny, nz, nb;
    
    nx = fdm->nxpad;
    ny = fdm->nypad;
    nz = fdm->nzpad;
    nb = fdm->nb;


/* now we have 6 sides of the cube!*/
      /****************************/
       #ifdef _OPENMP
       #pragma omp parallel for \
              schedule(dynamic,fdm->ompchunk) \
              private(ix,iy,iz) \
              shared(vx, dt, nb, nx, ny, nz,sigma)
       #endif
       /****************************/
       /* LEFT SIDE AND RIGHT SIDE */
	   for    (iy=0; iy<ny; iy++) {
	     for  (ix=0; ix<nb; ix++) {
	       for(iz=0; iz<nz; iz++) {
		      vx[iy][ix][iz] +=  -sigma[ix]*vx[iy][ix][iz]*dt;
		      vx[iy][nx-ix-1][iz] +=  -sigma[ix]*vx[iy][nx-ix-1][iz]*dt;
	       }
	     }
	   }

      /****************************/
       #ifdef _OPENMP
       #pragma omp parallel for \
              schedule(dynamic,fdm->ompchunk) \
              private(ix,iy,iz) \
              shared(vz, dt, nb, nx, ny, nz,sigma)
       #endif
       /****************************/
       /* UP SIDE AND BOTTOM SIDE */
	   for    (iy=0; iy<ny; iy++) {
	     for  (ix=0; ix<nx; ix++) {
	       for(iz=0; iz<nb; iz++) {
		      vz[iy][ix][iz] +=  -sigma[iz]*vz[iy][ix][iz]*dt;
		      vz[iy][ix][nz-iz-1] +=  -sigma[iz]*vz[iy][ix][nz-iz-1]*dt;
	       }
	     }
	   }

      /****************************/
       #ifdef _OPENMP
       #pragma omp parallel for \
              schedule(dynamic,fdm->ompchunk) \
              private(ix,iy,iz) \
              shared(vy, dt, nb, nx, ny, nz,sigma)
       #endif
       /****************************/
       /* FRONT SIDE AND BACK SIDE */
       for    (iy=0; iy<nb; iy++) {
	     for  (ix=0; ix<nx; ix++) {
	       for(iz=0; iz<nz; iz++) {
		      vy[iy][ix][iz] +=  -sigma[iy]*vy[iy][ix][iz]*dt;
		      vy[ny-iy-1][ix][iz] +=  -sigma[iy]*vy[ny-iy-1][ix][iz]*dt;
	       }
	     }
	   }


}

/*------------------------------------------------------------*/
void pml2d_presApply(float   **u,
                     float  **vx,
                     float  **vz, 
                     float    dt,
                     PML2D   pml,
                     float **com,
                     float *sigma,
                     fdm2d   fdm)
/*< Application of the 2D PML to the pressure field >*/
{

    int ix,iz,nx,nz,nb,shiftx,shiftz;
    float idx,idz;
	float sigmax,sigmaz;
	
	idx=1/fdm->dx;
	idz=1/fdm->dz;
	
    nx = fdm->nxpad;
    nz = fdm->nzpad;	
	nb=fdm->nb;
    shiftx=nx-nb+1;
	shiftz=nz-nb+1;

        /* LEFT SIDE*/
        for    (ix=NOP; ix<nb; ix++) {
        		sigmax=sigma[ix];
            for(iz=NOP; iz<nz-NOP; iz++) {

			   pml->left[ix][iz] += Fz(vz,ix,iz,idz)*dt;
		       //u[ix][iz] += - sigmax*u[ix][iz]*dt;		
            }
        }

        /* Up-Left CORNER*/
	    for    (ix=NOP; ix<nb; ix++) {
	    		sigmax=sigma[ix];
	        for(iz=NOP; iz<nb; iz++) {
	        	sigmaz=sigma[iz];

				pml->upleft[ix][iz] += u[ix][iz]*dt;
				u[ix][iz] += -sigmax*sigmaz*pml->upleft[ix][iz]*dt;
	        }
	    }	

        /* UP SIDE*/

	    for    (ix=NOP; ix<nx-NOP; ix++) {
		for(iz=NOP; iz<nb; iz++) {
		       sigmaz=sigma[iz];

		       pml->up[ix][iz] += Fx(vx,ix,iz,idx)*dt;
       		   //u[ix][iz] += - sigmaz*u[ix][iz]*dt;
	        }
	    }


        /* Up-Right CORNER*/

    	for    (ix=nx-nb+1; ix<nx-NOP; ix++) {
    			sigmax=sigma[nx-ix];
	        for(iz=NOP; iz<nb; iz++) {
	        	sigmaz=sigma[iz];
	        	
               pml->upright[ix-shiftx][iz] += u[ix][iz]*dt;
		       u[ix][iz] += -sigmax*sigmaz*pml->upright[ix-shiftx][iz]*dt;
	        }
	    }

        /* RIGHT SIDE*/
	    for    (ix=nx-nb+1; ix<nx-NOP; ix++) {
	    		sigmax=sigma[nx-ix];
	        for(iz=NOP; iz<nz-NOP; iz++) {

		       pml->right[ix-shiftx][iz] += Fz(vz,ix,iz,idz)*dt;
		       //u[ix][iz] += - sigmax*u[ix][iz]*dt;
	        }
	    }


		/* Bottom-Right CORNER*/
		for    (ix=nx-nb+1; ix<nx-NOP; ix++) {
    			sigmax=sigma[nx-ix];
	        for(iz=nz-nb+1; iz<nz-NOP; iz++) {
	        	sigmaz=sigma[nz-iz];

               pml->downright[ix-shiftx][iz-shiftz] += u[ix][iz]*dt;
		       u[ix][iz] += -sigmax*sigmaz*pml->downright[ix-shiftx][iz-shiftz]*dt;
    	    }
	    }



		/* BOTTOM SIDE*/

		for    (ix=NOP; ix<nx-NOP; ix++) {
        for(iz=nz-nb+1; iz<nz-NOP; iz++) {
	        	sigmaz=sigma[nz-iz];
	
               pml->down[ix][iz-shiftz] += Fx(vx,ix,iz,idx)*dt;
		       //u[ix][iz] += - sigmaz*u[ix][iz]*dt;
	        }
	    }



		/* Bottom-Left CORNER*/
		for    (ix=NOP; ix<nb; ix++) {
    			sigmax=sigma[ix];
	        for(iz=nz-nb+1; iz<nz-NOP; iz++) {
	        	sigmaz=sigma[nz-iz];

	           pml->downleft[ix][iz-shiftz] += u[ix][iz]*dt;
		       u[ix][iz] += -sigmax*sigmaz*pml->downleft[ix][iz-shiftz]*dt;
	        }
	    }



		/************************************************************************/
		/* auxiliary ODE */

		/* LEFT SIDE*/
		for     (ix=NOP; ix<nb; ix++){
    			sigmax=sigma[ix];
	        for (iz=NOP; iz<nz-NOP; iz++){

			    //pml->left[ix][iz] += com[ix][iz]*sigmax*Fz(vz,ix,iz,idz)*dt;
		        u[ix][iz] += - sigmax*u[ix][iz]*dt + com[ix][iz]*sigmax*pml->left[ix][iz]*dt;	
		    }
	    }

		/* UP SIDE*/

		for    (ix=NOP; ix<nx-NOP; ix++) {
            for(iz=NOP; iz<nb; iz++) {
            	sigmaz=sigma[iz];

		       //pml->up[ix][iz] += com[ix][iz]*sigmaz*Fx(vx,ix,iz,idx)*dt;
               u[ix][iz] += - sigmaz*u[ix][iz]*dt + com[ix][iz]*sigmaz*pml->up[ix][iz]*dt;
            }
        }
	
		/* RIGHT SIDE*/
		for    (ix=nx-nb+1; ix<nx-NOP; ix++) {
 	    		sigmax=sigma[nx-ix];
            for(iz=NOP; iz<nz-NOP; iz++) {

		       //pml->right[ix-shiftx][iz] += com[ix][iz]*sigmax*Fz(vz,ix,iz,idz)*dt;
               u[ix][iz] += - sigmax*u[ix][iz]*dt + com[ix][iz]*sigmax*pml->right[ix-shiftx][iz]*dt;
		    }
	    }

		/* BOTTOM SIDE*/

		for    (ix=NOP; ix<nx-NOP; ix++) {
	        for(iz=nz-nb+1; iz<nz-NOP; iz++) {
	        	sigmaz=sigma[nz-iz];

               //pml->down[ix][iz-shiftz] += com[ix][iz]*sigmaz*Fx(vx,ix,iz,idx)*dt;
               u[ix][iz] += - sigmaz*u[ix][iz]*dt + com[ix][iz]*sigmaz*pml->down[ix][iz-shiftz]*dt;		   		   
            }   
        } 		


}

/*------------------------------------------------------------*/
void pml3d_presApply(float   ***u,
                     float  ***vx,
                     float  ***vy,
                     float  ***vz,
                     float     dt,
                     PML3D    pml,
                     float ***com,
                     float *sigma,
                     fdm3d    fdm)
/*< Application of the 3D PML to the pressure field >*/
{

    int ix,iy,iz,nb,shiftx,shifty,shiftz;
    int nx,ny,nz;
    float idx,idy,idz;
	float sigmax, sigmay, sigmaz;
	
	idx=1/fdm->dx;
	idy=1/fdm->dy;
	idz=1/fdm->dz;
	
	nb=fdm->nb;

    nx=fdm->nxpad;
    ny=fdm->nypad;
    nz=fdm->nzpad;

    shiftx=nx-nb;
    shifty=ny-nb;
	shiftz=nz-nb;

/*THE ORDER MATTERS!*/
/*******************************************************************************/
        /****************************/
       #ifdef _OPENMP
       #pragma omp parallel for \
              schedule(dynamic,fdm->ompchunk) \
              private(ix,iy,iz,sigmax,sigmay,sigmaz) \
              shared(u, dt, nb, fdm, pml)
       #endif
       /****************************/  
		// UP-LEFT-FRONT + UP-RIGHT-FRONT + DOWN-RIGHT-FRONT
		for     (iy=NOP; iy<nb; iy++){
				sigmay=sigma[iy];
		  for   (ix=NOP; ix<nb; ix++){
		  		sigmax=sigma[ix];
			for (iz=NOP; iz<nb; iz++){
				sigmaz=sigma[iz];
				
				//ULF
				pml->ULFW[iy][ix][iz] += pml->ULU[iy][ix][iz]*dt; 
				pml->ULU[iy][ix][iz]  += u[iy][ix][iz]*dt;
				
				pml->UFPSIyzx[iy][ix][iz] += sigmay*pml->Upsizx[iy][ix][iz]*dt;
			
				u[iy][ix][iz] +=  -( sigmax*sigmay*sigmaz*pml->ULFW[iy][ix][iz] + 
				                    (sigmax*sigmay + sigmax*sigmaz + sigmay*sigmaz)*pml->ULU[iy][ix][iz])*dt
				                    + com[iy][ix][iz]*pml->UFPSIyzx[iy][ix][iz]*dt;
				                    
				//UPRF
				pml->URFW[iy][ix][iz] += pml->URU[iy][ix][iz]*dt;
				pml->URU[iy][ix][iz] += u[iy][nx-ix-1][iz]*dt;
				
				pml->UFPSIyzx[iy][nx-ix-1][iz] += sigmay*pml->Upsizx[iy][nx-ix-1][iz]*dt;
				
				u[iy][nx-ix-1][iz] += -(sigmax*sigmay*sigmaz*pml->URFW[iy][ix][iz] + 
				                  (sigmax*sigmay + sigmax*sigmaz + sigmay*sigmaz)*pml->URU[iy][ix][iz])*dt 
				                  + com[iy][nx-ix-1][iz]*pml->UFPSIyzx[iy][nx-ix-1][iz]*dt;
				                  
				//DRF
				pml->DRFW[iy][ix][iz] += pml->DRU[iy][ix][iz]*dt;
				pml->DRU[iy][ix][iz] += u[iy][nx-ix-1][nz-iz-1]*dt;
				
				pml->DFPSIyzx[iy][nx-ix-1][iz] += sigmay*pml->Dpsizx[iy][nx-ix-1][iz]*dt;
			
				u[iy][nx-ix-1][nz-iz-1] += -(sigmax*sigmay*sigmaz*pml->DRFW[iy][ix][iz] +
				                  (sigmax*sigmay + sigmax*sigmaz + sigmay*sigmaz)*pml->DRU[iy][ix][iz])*dt 
				                  + com[iy][nx-ix-1][nz-iz-1]*pml->DFPSIyzx[iy][nx-ix-1][iz]*dt;
				                  
				//DLF
				pml->DLFW[iy][ix][iz] += pml->DLU[iy][ix][iz]*dt; 
				pml->DLU[iy][ix][iz] += u[iy][ix][nz-iz-1]*dt;
				
				pml->DFPSIyzx[iy][ix][iz] += sigmay*pml->Dpsizx[iy][ix][iz]*dt;
				
				u[iy][ix][nz-iz-1] += -(sigmax*sigmay*sigmaz*pml->DLFW[iy][ix][iz] + 
				                  (sigmax*sigmay + sigmax*sigmaz + sigmay*sigmaz)*pml->DLU[iy][ix][iz])*dt
				                  + com[iy][ix][nz-iz-1]*pml->DFPSIyzx[iy][ix][iz]*dt;
				                  
				/************************************************************************/                  
				//ULB
				pml->ULBW[iy][ix][iz] += pml->ULU[ny-iy-1][ix][iz]*dt; 
				pml->ULU[ny-iy-1][ix][iz] += u[ny-iy-1][ix][iz]*dt;
				
				pml->UBPSIyzx[iy][ix][iz] += sigmay*pml->Upsizx[ny-iy-1][ix][iz]*dt;
		
				u[ny-iy-1][ix][iz] += -(sigmax*sigmay*sigmaz*pml->ULBW[iy][ix][iz] +
				                  (sigmax*sigmay + sigmax*sigmaz + sigmay*sigmaz)*pml->ULU[ny-iy-1][ix][iz])*dt 
				                  + com[ny-iy-1][ix][iz]*pml->UBPSIyzx[iy][ix][iz]*dt;
				                  
				//URB
				pml->URBW[iy][ix][iz] += pml->URU[ny-iy-1][ix][iz]*dt;
				pml->URU[ny-iy-1][ix][iz] += u[ny-iy-1][nx-ix-1][iz]*dt;
				
				pml->UBPSIyzx[iy][nx-ix-1][iz] += sigmay*pml->Upsizx[ny-iy-1][nx-ix-1][iz]*dt;
			
				u[ny-iy-1][nx-ix-1][iz] += -(sigmax*sigmay*sigmaz*pml->URBW[iy][ix][iz] + 
				                  (sigmax*sigmay + sigmax*sigmaz + sigmay*sigmaz)*pml->URU[ny-iy-1][ix][iz])*dt
				                  + com[ny-iy-1][nx-ix-1][iz]*pml->UBPSIyzx[iy][nx-ix-1][iz]*dt;
				                  
				//DRB
				pml->DRBW[iy][ix][iz] += pml->DRU[ny-iy-1][ix][iz]*dt; 
				pml->DRU[ny-iy-1][ix][iz] += u[ny-iy-1][nx-ix-1][nz-iz-1]*dt;
				
				pml->DBPSIyzx[iy][nx-ix-1][iz] += sigmay*pml->Dpsizx[ny-iy-1][nx-ix-1][iz]*dt;
			
				u[ny-iy-1][nx-ix-1][nz-iz-1] += -(sigmax*sigmay*sigmaz*pml->DRBW[iy][ix][iz] + 
				                  (sigmax*sigmay + sigmax*sigmaz + sigmay*sigmaz)*pml->DRU[ny-iy-1][ix][iz])*dt
				                  + com[ny-iy-1][nx-ix-1][nz-iz-1]*pml->DBPSIyzx[iy][nx-ix-1][iz]*dt;
				                  
				// DBL
				pml->DLBW[iy][ix][iz] += pml->DLU[ny-iy-1][ix][iz]*dt;
				pml->DLU[ny-iy-1][ix][iz] += u[ny-iy-1][ix][nz-iz-1]*dt;
				
				pml->DBPSIyzx[iy][ix][iz] += sigmay*pml->Dpsizx[ny-iy-1][ix][iz]*dt;
			
				u[ny-iy-1][ix][nz-iz-1] += -(sigmax*sigmay*sigmaz*pml->DLBW[iy][ix][iz] +
				                  (sigmax*sigmay + sigmax*sigmaz + sigmay*sigmaz)*pml->DLU[ny-iy-1][ix][iz])*dt
				                  +com[ny-iy-1][ix][nz-iz-1]*pml->DBPSIyzx[iy][ix][iz]*dt;				
			}
		  }
		}
		
		/***********************************************/
		/* EDGES */
		

        /****************************/
       #ifdef _OPENMP
       #pragma omp parallel for \
              schedule(dynamic,fdm->ompchunk) \
              private(ix,iy,iz,sigmax,sigmay,sigmaz) \
              shared(u, dt, nb, fdm, pml)
       #endif
       /****************************/
		for     (iy=NOP; iy<nb; iy++){
				sigmay=sigma[iy];
				
		  for   (ix=NOP; ix<nb; ix++){
				sigmax=sigma[ix];
				
			for (iz=nb; iz<nz-nb; iz++){

				// LEFT FRONT			
			    pml->LFPSIxyz[iy][ix][iz] += sigmax*pml->Fpsiyz[iy][ix][iz]*dt;
				pml->LFU[iy][ix][iz]  += u[iy][ix][iz]*dt;
				
				u[iy][ix][iz] += (com[iy][ix][iz]*pml->LFPSIxyz[iy][ix][iz] - sigmax*sigmay*pml->LFU[iy][ix][iz])*dt;
				
				// RIGHT FRONT
				pml->RFPSIxyz[iy][ix][iz] += sigmax*pml->Fpsiyz[iy][nx-ix-1][iz]*dt;
				pml->RFU[iy][ix][iz] += u[iy][nx-ix-1][iz]*dt;
				
				u[iy][nx-ix-1][iz] += (com[iy][nx-ix-1][iz]*pml->RFPSIxyz[iy][ix][iz] - sigmax*sigmay*pml->RFU[iy][ix][iz])*dt;				
		
			}
		  }
		}
		

        /****************************/
       #ifdef _OPENMP
       #pragma omp parallel for \
              schedule(dynamic,fdm->ompchunk) \
              private(ix,iy,iz,sigmax,sigmay,sigmaz) \
              shared(u, dt, nb, fdm, pml)
       #endif
       /****************************/  
		for     (iy=NOP; iy<nb; iy++){
				sigmay=sigma[iy];
		  for   (ix=nb; ix<nx-nb; ix++){
		  		
			for (iz=NOP; iz<nb; iz++){
				sigmaz=sigma[iz];
				
				
				// UP FRONT
				pml->UFPSIyzx[iy][ix][iz] += sigmay*pml->Upsizx[iy][ix][iz]*dt;
				pml->UFU[iy][ix][iz] += u[iy][ix][iz]*dt;
		   
				u[iy][ix][iz] += (1*com[iy][ix][iz]*pml->UFPSIyzx[iy][ix][iz] - sigmay*sigmaz*pml->UFU[iy][ix][iz])*dt;        

				// DOWN FRONT
				pml->DFPSIyzx[iy][ix][iz] += sigmay*pml->Dpsizx[iy][ix][iz]*dt;
				pml->DFU[iy][ix][iz] += u[iy][ix][nz-iz-1]*dt;
				
				u[iy][ix][nz-iz-1] += (com[iy][ix][nz-iz-1]*pml->DFPSIyzx[iy][ix][iz] -  sigmay*sigmaz*pml->DFU[iy][ix][iz])*dt;		
			}
		  }
		}
		

        /****************************/
       #ifdef _OPENMP
       #pragma omp parallel for \
              schedule(dynamic,fdm->ompchunk) \
              private(ix,iy,iz,sigmax,sigmay,sigmaz) \
              shared(u, dt, nb, fdm, pml)
       #endif
       /****************************/  
		for     (iy=nb; iy<ny-nb; iy++){
		  for   (ix=NOP; ix<nb; ix++){
		  		sigmax=sigma[ix];
			for (iz=NOP; iz<nb; iz++){
				sigmaz=sigma[iz];
				
				
				// UP LEFT
				pml->ULPSIzxy[iy][ix][iz] += sigmaz*pml->Lpsixz[iy][ix][iz]*dt;
				pml->ULU[iy][ix][iz] += u[iy][ix][iz]*dt;
				
				u[iy][ix][iz] += (1*com[iy][ix][iz]*pml->ULPSIzxy[iy][ix][iz] - sigmax*sigmaz*pml->ULU[iy][ix][iz])*dt;

				// UP RIGHT
				pml->URPSIzxy[iy][ix][iz] += sigmaz*pml->Rpsixz[iy][ix][iz]*dt;
				pml->URU[iy][ix][iz] += u[iy][nx-ix-1][iz]*dt;
				
				u[iy][nx-ix-1][iz] += (com[iy][nx-ix-1][iz]*pml->URPSIzxy[iy][ix][iz] - sigmax*sigmaz*pml->URU[iy][ix][iz])*dt;				
		
			}
		  }
		}

        /****************************/
       #ifdef _OPENMP
       #pragma omp parallel for \
              schedule(dynamic,fdm->ompchunk) \
              private(ix,iy,iz,sigmax,sigmay,sigmaz) \
              shared(u, dt, nb, fdm, pml)
       #endif
       /****************************/  
		// down left
		for     (iy=nb; iy<ny-nb; iy++){
		  for   (ix=NOP; ix<nb; ix++){
			    sigmax=sigma[ix];
			for (iz=nz-nb; iz<nz-NOP; iz++){
				sigmaz=sigma[nz - iz];
				
				// DOWN LEFT
				pml->DLPSIzxy[iy][ix][iz-shiftz] += sigmaz*pml->Lpsixz[iy][ix][iz]*dt;
				pml->DLU[iy][ix][iz-shiftz] += u[iy][ix][iz]*dt;
				
				u[iy][ix][iz] += (1*com[iy][ix][iz]*pml->DLPSIzxy[iy][ix][iz-shiftz] - sigmax*sigmaz*pml->DLU[iy][ix][iz-shiftz])*dt;

				// DOWN RIGHT
				pml->DRPSIzxy[iy][ix][iz-shiftz] += sigmaz*pml->Rpsixz[iy][ix][iz]*dt;
				pml->DRU[iy][ix][iz-shiftz] += u[iy][nx-ix-1][iz]*dt;
				
				u[iy][nx-ix-1][iz] += (com[iy][nx-ix-1][iz]*pml->DRPSIzxy[iy][ix][iz-shiftz] - sigmax*sigmaz*pml->DRU[iy][ix][iz-shiftz])*dt;
		
			}
		  }
		}
		

		/****************************/
       #ifdef _OPENMP
       #pragma omp parallel for \
              schedule(dynamic,fdm->ompchunk) \
              private(ix,iy,iz,sigmax,sigmay,sigmaz) \
              shared(u, dt, nb, fdm, pml)
       #endif
       /****************************/  
		for     (iy=ny-nb; iy<ny-NOP; iy++){
				sigmay=sigma[ny - iy];
		  for   (ix=NOP; ix<nb; ix++){
		  		sigmax=sigma[ix];
			for (iz=nb; iz<nz-nb; iz++){
				
				// LEFT BACK
				pml->LBPSIxyz[iy-shifty][ix][iz] += sigmax*pml->Bpsiyz[iy-shifty][ix][iz]*dt;
				pml->LBU[iy-shifty][ix][iz] += u[iy][ix][iz]*dt;
				
				u[iy][ix][iz] += (1*com[iy][ix][iz]*pml->LBPSIxyz[iy-shifty][ix][iz] - sigmax*sigmay*pml->LBU[iy-shifty][ix][iz])*dt;

				// RIGHT BACK
				pml->RBPSIxyz[iy-shifty][ix][iz] += sigmax*pml->Bpsiyz[iy-shifty][nx-ix-1][iz]*dt;
				pml->RBU[iy-shifty][ix][iz] += u[iy][nx-ix-1][iz]*dt;
		
				u[iy][nx-ix-1][iz] += (com[iy][nx-ix-1][iz]*pml->RBPSIxyz[iy-shifty][ix][iz] - sigmax*sigmay*pml->RBU[iy-shifty][ix][iz])*dt;

			}
		  }
		}
		

        /****************************/
       #ifdef _OPENMP
       #pragma omp parallel for \
              schedule(dynamic,fdm->ompchunk) \
              private(ix,iy,iz,sigmax,sigmay,sigmaz) \
              shared(u, dt, nb, fdm, pml)
       #endif
       /****************************/  
		
		for     (iy=ny-nb; iy<ny-NOP; iy++){
				sigmay=sigma[ny - iy];
		  for   (ix=nb; ix<nx-nb; ix++){
			for (iz=NOP; iz<nb; iz++){
				sigmaz=sigma[iz];
				
				// UP BACK
				pml->UBPSIyzx[iy-shifty][ix][iz] += sigmay*pml->Upsizx[iy][ix][iz]*dt;
				pml->UBU[iy-shifty][ix][iz] += u[iy][ix][iz]*dt;
				
				u[iy][ix][iz] += (com[iy][ix][iz]*pml->UBPSIyzx[iy-shifty][ix][iz] - sigmay*sigmaz*pml->UBU[iy-shifty][ix][iz])*dt;

				// DOWN BACK
				pml->DBPSIyzx[iy-shifty][ix][iz] += sigmay*pml->Dpsizx[iy][ix][iz]*dt;
				pml->DBU[iy-shifty][ix][iz-shiftz] += u[iy][ix][nz-iz-1]*dt;
				
				u[iy][ix][nz-iz-1] += (com[iy][ix][nz-iz-1]*pml->DBPSIyzx[iy-shifty][ix][iz] - sigmay*sigmaz*pml->DBU[iy-shifty][ix][iz])*dt;
		
			}
		  }
		}



/*****************************/
/* SIDES */

/****************************/
#ifdef _OPENMP
#pragma omp parallel for \
   schedule(dynamic,fdm->ompchunk) \
   private(ix,iy,iz) \
   shared(pml, u, vy, vz, com, dt, nb, nx, ny, nz, idy, idz,sigma)
#endif
/****************************/
for     (iy=NOP; iy<ny-NOP; iy++){
  for   (ix=NOP; ix<nb; ix++){
	for (iz=NOP; iz<nz-NOP; iz++){

		// LEFT
		pml->Lpsixy[iy][ix][iz] += sigma[ix]*Fy3(vy, iy, ix, iz, idy)*dt;
		pml->Lpsixz[iy][ix][iz] += sigma[ix]*Fz3(vz, iy, ix, iz, idz)*dt;
		
		u[iy][ix][iz] += com[iy][ix][iz]*(pml->Lpsixy[iy][ix][iz] + pml->Lpsixz[iy][ix][iz])*dt - sigma[ix]*u[iy][ix][iz]*dt;
		
		
		// RIGHT
		pml->Rpsixy[iy][ix][iz] += sigma[ix]*Fy3(vy, iy, nx-ix-1, iz, idy)*dt;
		pml->Rpsixz[iy][ix][iz] += sigma[ix]*Fz3(vz, iy, nx-ix-1, iz, idz)*dt;
		
		u[iy][nx-ix-1][iz] += com[iy][nx-ix-1][iz]*(pml->Rpsixy[iy][ix][iz] + pml->Rpsixz[iy][ix][iz])*dt - sigma[ix]*u[iy][nx-ix-1][iz]*dt;
	}
  }
}


/****************************/
#ifdef _OPENMP
#pragma omp parallel for \
   schedule(dynamic,fdm->ompchunk) \
   private(ix,iy,iz) \
   shared(pml, u, vx, vy, com, dt, nb, nx, ny, nz, idx, idy,sigma)
#endif
/****************************/
for     (iy=NOP; iy<ny-NOP; iy++){
  for   (ix=NOP; ix<nx-NOP; ix++){
	for (iz=NOP; iz<nb; iz++){

		// UP
		pml->Upsizx[iy][ix][iz] += sigma[iz]*Fx3(vx, iy, ix, iz, idx)*dt;
		pml->Upsizy[iy][ix][iz] += sigma[iz]*Fy3(vy, iy, ix, iz, idy)*dt;
		
		u[iy][ix][iz] += com[iy][ix][iz]*(pml->Upsizx[iy][ix][iz] +  pml->Upsizy[iy][ix][iz])*dt - sigma[iz]*u[iy][ix][iz]*dt;

		// DOWN
		pml->Dpsizx[iy][ix][iz] += sigma[iz]*Fx3(vx, iy, ix, nz-iz-1, idx)*dt;
		pml->Dpsizy[iy][ix][iz] += sigma[iz]*Fy3(vy, iy, ix, nz-iz-1, idy)*dt;

		u[iy][ix][nz-iz-1] += com[iy][ix][nz-iz-1]*(pml->Dpsizx[iy][ix][iz] + pml->Dpsizy[iy][ix][iz])*dt - sigma[iz]*u[iy][ix][nz-iz-1]*dt;
	}
  }
}

/****************************/
#ifdef _OPENMP
#pragma omp parallel for \
   schedule(dynamic,fdm->ompchunk) \
   private(ix,iy,iz) \
   shared(pml, u, vx, vz, com, dt, nb, nx, ny, nz, idx, idz,sigma)
#endif
/****************************/
for     (iy=NOP; iy<nb; iy++){
  for   (ix=NOP; ix<nx-NOP; ix++){
	for (iz=NOP; iz<nz-NOP; iz++){

		// FRONT
		pml->Fpsiyx[iy][ix][iz] += sigma[iy]*Fx3(vx, iy, ix, iz, idx)*dt;
		pml->Fpsiyz[iy][ix][iz] += sigma[iy]*Fz3(vz, iy, ix, iz, idz)*dt;

		u[iy][ix][iz] += com[iy][ix][iz]*(pml->Fpsiyx[iy][ix][iz] + pml->Fpsiyz[iy][ix][iz])*dt - sigma[iy]*u[iy][ix][iz]*dt;

		// BACK
		pml->Bpsiyx[iy][ix][iz] += sigma[iy]*Fx3(vx, ny-iy-1, ix, iz, idx)*dt;
		pml->Bpsiyz[iy][ix][iz] += sigma[iy]*Fz3(vz, ny-iy-1, ix, iz, idz)*dt;
		
		u[ny-iy-1][ix][iz] += com[ny-iy-1][ix][iz]*(pml->Bpsiyx[iy][ix][iz] + pml->Bpsiyz[iy][ix][iz])*dt - sigma[iy]*u[ny-iy-1][ix][iz]*dt;
	}
  }
}


}

/*------------------------------------------------------------*/
void pml2d_free(PML2D pml)
                
/*< free the memory allocated for the 2D PML >*/
{
	/* sides of the computational domain */
    free(pml->up[0]);    free(pml->up);
    free(pml->down[0]);  free(pml->down);
    free(pml->right[0]); free(pml->right);
    free(pml->left[0]);  free(pml->left);
    
    /* corners of the computational domain */
    free(pml->upright[0]);   free(pml->upright);
    free(pml->downright[0]); free(pml->downright);
    free(pml->upleft[0]);    free(pml->upleft);
    free(pml->downleft[0]);  free(pml->downleft);
	
}


/*------------------------------------------------------------*/
void pml3d_free(PML3D pml)
                
/*< free the memory allocated for the 3D PML >*/
{

    // sides
    free(pml->Upsizx[0][0]); free(pml->Upsizx[0]); free(pml->Upsizx);
    free(pml->Upsizy[0][0]); free(pml->Upsizy[0]); free(pml->Upsizy);
    free(pml->Dpsizx[0][0]); free(pml->Dpsizx[0]); free(pml->Dpsizx);
    free(pml->Dpsizy[0][0]); free(pml->Dpsizy[0]); free(pml->Dpsizy);
    free(pml->Fpsiyx[0][0]); free(pml->Fpsiyx[0]); free(pml->Fpsiyx);
    free(pml->Fpsiyz[0][0]); free(pml->Fpsiyz[0]); free(pml->Fpsiyz);
    free(pml->Bpsiyx[0][0]); free(pml->Bpsiyx[0]); free(pml->Bpsiyx);
    free(pml->Bpsiyz[0][0]); free(pml->Bpsiyz[0]); free(pml->Bpsiyz);
    
    free(pml->Lpsixy[0][0]); free(pml->Lpsixy[0]); free(pml->Lpsixy);
    free(pml->Lpsixz[0][0]); free(pml->Lpsixz[0]); free(pml->Lpsixz);
    free(pml->Rpsixy[0][0]); free(pml->Rpsixy[0]); free(pml->Rpsixy);
    free(pml->Rpsixz[0][0]); free(pml->Rpsixz[0]); free(pml->Rpsixz);
    
    /*edges*/
    free(pml->UFPSIyzx[0][0]);free(pml->UFPSIyzx[0]);free(pml->UFPSIyzx);
    free(pml->UBPSIyzx[0][0]);free(pml->UBPSIyzx[0]);free(pml->UBPSIyzx);    

    free(pml->DFPSIyzx[0][0]);free(pml->DFPSIyzx[0]);free(pml->DFPSIyzx);
    free(pml->DBPSIyzx[0][0]);free(pml->DBPSIyzx[0]);free(pml->DBPSIyzx);
    
    free(pml->ULPSIzxy[0][0]);free(pml->ULPSIzxy[0]);free(pml->ULPSIzxy);
    free(pml->URPSIzxy[0][0]);free(pml->URPSIzxy[0]);free(pml->URPSIzxy);

    free(pml->DRPSIzxy[0][0]);free(pml->DRPSIzxy[0]);free(pml->DRPSIzxy);    
    free(pml->DLPSIzxy[0][0]);free(pml->DLPSIzxy[0]);free(pml->DLPSIzxy);

    free(pml->LFPSIxyz[0][0]);free(pml->LFPSIxyz[0]);free(pml->LFPSIxyz);
    free(pml->RFPSIxyz[0][0]);free(pml->RFPSIxyz[0]);free(pml->RFPSIxyz);    

    free(pml->LBPSIxyz[0][0]);free(pml->LBPSIxyz[0]);free(pml->LBPSIxyz);
    free(pml->RBPSIxyz[0][0]);free(pml->RBPSIxyz[0]);free(pml->RBPSIxyz);    
    
    free(pml->ULU[0][0]); free(pml->ULU[0]); free(pml->ULU);
    free(pml->URU[0][0]); free(pml->URU[0]); free(pml->URU);
    free(pml->DRU[0][0]); free(pml->DRU[0]); free(pml->DRU);
    free(pml->DLU[0][0]); free(pml->DLU[0]); free(pml->DLU);
    
    free(pml->UFU[0][0]); free(pml->UFU[0]); free(pml->UFU);
    free(pml->UBU[0][0]); free(pml->UBU[0]); free(pml->UBU);
    free(pml->DBU[0][0]); free(pml->DBU[0]); free(pml->DBU);
    free(pml->DFU[0][0]); free(pml->DFU[0]); free(pml->DFU);

    free(pml->LFU[0][0]); free(pml->LFU[0]); free(pml->LFU);
    free(pml->RFU[0][0]); free(pml->RFU[0]); free(pml->RFU);
    free(pml->RBU[0][0]); free(pml->RBU[0]); free(pml->RBU);
    free(pml->LBU[0][0]); free(pml->LBU[0]); free(pml->LBU);
    
    /* corners */
    free(pml->ULFW[0][0]); free(pml->ULFW[0]); free(pml->ULFW);
    free(pml->URFW[0][0]); free(pml->URFW[0]); free(pml->URFW);
    free(pml->DRFW[0][0]); free(pml->DRFW[0]); free(pml->DRFW);
    free(pml->DLFW[0][0]); free(pml->DLFW[0]); free(pml->DLFW);
    free(pml->ULBW[0][0]); free(pml->ULBW[0]); free(pml->ULBW);
    free(pml->URBW[0][0]); free(pml->URBW[0]); free(pml->URBW);
    free(pml->DRBW[0][0]); free(pml->DRBW[0]); free(pml->DRBW);
    free(pml->DLBW[0][0]); free(pml->DLBW[0]); free(pml->DLBW);
    
}