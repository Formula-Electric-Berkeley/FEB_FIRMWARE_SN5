#include "FEB_SN_Config.h"

#if FEB_SN_IS_FRONT()
const char FEB_SN_VARIANT_NAME[] = "FRONT";
#else
const char FEB_SN_VARIANT_NAME[] = "REAR";
#endif
