/**
 ******************************************************************************
 * @file           : FEB_SN_Config.h
 * @brief          : Sensor Node FRONT/REAR variant selector and sensor population.
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 ******************************************************************************
 */

#ifndef FEB_SN_CONFIG_H
#define FEB_SN_CONFIG_H

#ifdef __cplusplus
extern "C"
{
#endif

#define FEB_SN_VARIANT_FRONT 1
#define FEB_SN_VARIANT_REAR 2

#ifndef FEB_SENSOR_NODE_VARIANT
#error "FEB_SENSOR_NODE_VARIANT must be defined by the build system (FRONT or REAR)"
#endif
#if (FEB_SENSOR_NODE_VARIANT != FEB_SN_VARIANT_FRONT) && (FEB_SENSOR_NODE_VARIANT != FEB_SN_VARIANT_REAR)
#error "FEB_SENSOR_NODE_VARIANT must be FEB_SN_VARIANT_FRONT or FEB_SN_VARIANT_REAR"
#endif

#define FEB_SN_IS_FRONT() (FEB_SENSOR_NODE_VARIANT == FEB_SN_VARIANT_FRONT)
#define FEB_SN_IS_REAR() (FEB_SENSOR_NODE_VARIANT == FEB_SN_VARIANT_REAR)

  extern const char FEB_SN_VARIANT_NAME[];

#if FEB_SN_IS_FRONT()
#define FEB_SN_HAS_IMU 0
#define FEB_SN_HAS_MAG 0
#define FEB_SN_HAS_GPS 0
#define FEB_SN_HAS_WSS 1
#define FEB_SN_HAS_FUSION 0
#define FEB_SN_HAS_LINEAR_POTENTIOMETER 1
#else /* REAR */
#define FEB_SN_HAS_IMU 1
#define FEB_SN_HAS_MAG 1
#define FEB_SN_HAS_GPS 1
#define FEB_SN_HAS_WSS 1
#define FEB_SN_HAS_FUSION 1
#define FEB_SN_HAS_LINEAR_POTENTIOMETER 1
#endif

/* Consistency checks: composite features require their primitives. */
#if FEB_SN_HAS_FUSION && (!FEB_SN_HAS_IMU || !FEB_SN_HAS_MAG)
#error "FEB_SN_HAS_FUSION requires both FEB_SN_HAS_IMU and FEB_SN_HAS_MAG"
#endif

#ifdef __cplusplus
}
#endif

#endif /* FEB_SN_CONFIG_H */
