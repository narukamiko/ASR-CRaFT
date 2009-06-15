#ifndef CRF_H_
#define CRF_H_

#define CRF_PACKAGE_VERSION "v0.01a"
#define CRF_VERSION CRF_PACKAGE_VERSION

#include <stdexcept>
#include <iostream>
#include <string>
#include <cstring>
#include <fstream>
#include <sstream>
#include <assert.h>

#include "time.h"
#include "math.h"

extern "C" {
#include "cblas.h"
}

#include "QuickNet.h"


#include "CRF_LogMath.h"

using namespace std;
using namespace CRF_LogMath;

enum seqtype {SEQUENTIAL, RANDOM_NO_REPLACE, RANDOM_REPLACE};

enum ftrmaptype {STDSTATE, STDTRANS, STDSPARSE, STDSPARSETRANS, INFILE};

#endif /*CRF_H_*/
