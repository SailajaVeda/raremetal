#include "StringArray.h"
#include "InputFile.h"
#include "Error.h"
#include "Tabix.h"
#include "MathVector.h"
#define MATHLIB_STANDALONE
#include <Rmath.h>

// some external utility functions for meta-analysis, including:
//	file processing
//	math calculation

/*** file processingpart ***/
bool SetIfilePosition( IFILE & sfile, Tabix & myTabix, String Chr, int pos );

void tellRvOrRmw( String & buffer, bool & adjust, int marker_col, int cov_col );

bool setFromRvOrRmwAdjust( bool adjust, int marker_col, int cov_col );

void openMetaResultFile( String & prefix, String & filename, IFILE & output, String & method );

/*************************************************************************/

void removeChrFromString( String & chr_token );

/******* math part **********/

double GetGenomicControlFromPvalue(Vector & pvalue);

double GetBetaDensity(double a, double b, double x);

double CalculateCorrCoef(Vector & a,Vector & b);

void RevertAllele(String SNP, String & newSNP);

// for Dajiang's binary method

//double dnorm_mean0( double x );

//double c_inte( double x, void * params );

//double getCorrectC( double ymean,double sigma);


