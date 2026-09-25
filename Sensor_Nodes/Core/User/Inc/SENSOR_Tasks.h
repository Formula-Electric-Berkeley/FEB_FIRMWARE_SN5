#ifndef SENSOR_TASKS_H
#define SENSOR_TASKS_H

#ifdef __cplusplus
extern "C"
{
#endif

  void StartUartRxTask(void *argument);
  void StartCanRxTask(void *argument);
  void StartCanTxTask(void *argument);
  void StartCanPubTask(void *argument);
  void StartSensorTask(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_TASKS_H */
