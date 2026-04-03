#include <stdlib.h>
#include "../onem2m.h"
#include "../util.h"

int create_fcin(oneM2MPrimitive *o2pt, RTNode *parent_rtnode)
{
	return handle_error(o2pt, RSC_OPERATION_NOT_ALLOWED, "FlexContainerInstance cannot be created via API");
}

int update_fcin(oneM2MPrimitive *o2pt, RTNode *target_rtnode)
{
	return handle_error(o2pt, RSC_OPERATION_NOT_ALLOWED, "FlexContainerInstance cannot be updated via API");
}
