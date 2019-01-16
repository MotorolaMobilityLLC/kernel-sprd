#ifndef GETYUVINPUT_H
#define GETYUVINPUT_H

#include "dcam_params.h"

#define ROTATE0			0
#define ROTATE90		1
#define ROTATE270		2
#define ROTATE180		3
#define MIRROR			4
#define FLIP			5

void InitDcamInfo(struct DCAMINFO *pDCAMInfo);
#endif
