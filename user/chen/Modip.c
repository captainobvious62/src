/* omnidirectional dip estimation  */

#include <rsf.h>
#include "odip.h"


int main(int argc, char*argv[])
{
	sf_file in, out;
	int nf, n1, n2, n3, m1, m2, rect[2], niter, liter, interp;
	int i3;
	bool verb;
	float ****wav, **dip, radius, eta;

	sf_init(argc, argv);

	in  = sf_input("in");
	out = sf_output("out");

	if (SF_FLOAT != sf_gettype(in)) sf_error("Need float type");

	if (!sf_histint(in, "n1", &n1)) sf_error("No n1= in input");
	if (!sf_histint(in, "n2", &n2)) sf_error("No n2= in input");
	if (!sf_histint(in, "n3", &m1)) sf_error("No m1= in input");
	if (!sf_histint(in, "n4", &m2)) sf_error("No m2= in input");
	n3 = sf_leftsize(in, 4);
	sf_unshiftdim2(in,out,3);

	if (!sf_getint("rect1",&rect[0])) rect[0]=1;
	/* dip smoothness on 1st axis */
	if (!sf_getint("rect2",&rect[1])) rect[1]=1;
	/* dip smoothness on 2nd axis */
	
	if (!sf_getint("order",&nf)) nf=1;
	/* pwd order */
	if(m1 != 2*nf+1 || m2!= m1) 
		sf_error("order dismatch");
	if (!sf_getint("niter",&niter)) niter=5;
	/* number of iterations */
	if (!sf_getint("liter",&liter)) liter=20;
	/* number of linear iterations */
	if (!sf_getint("interp",&interp)) interp=0;
	/* interpolation method: 
	0: maxflat
	1: Lagrange 
	2: B-Spline */
	if (!sf_getfloat("radius", &radius)) radius = 1.0;
	/* interpolating radius for opwd */
	if (!sf_getfloat("eta", &eta)) eta = 0.75;
	/* steps for iteration */
	if (!sf_getbool("verb", &verb)) verb = false;
	/* verbosity flag */

	wav = sf_floatalloc4(n1, n2, m1, m2);
	dip = sf_floatalloc2(n1,n2);

	/* initialize dip estimation */
	odip_init(interp, nf, radius, n1, n2, rect, liter, verb);


	for(i3=0; i3<n3; i3++)
	{
		sf_floatread(wav[0][0][0], n1*n2*m1*m2, in);
		odip(wav, dip, niter, eta);
		sf_floatwrite(dip[0], n1*n2, out);
	}

	odip_close();
	free(dip[0]);
	free(dip);
	free(wav[0][0][0]);
	free(wav[0][0]);
	free(wav[0]);
	free(wav);
	return 0;
}



