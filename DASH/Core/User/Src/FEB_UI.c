#include "cmsis_os.h"


void StartDisplayTask(void *argument)
{

    for (;;)
    {
        osDelay(1);
    }
}

void StartBtnTxLoop(void *argument)
{
    for (;;)
    {
        osDelay(1);
    }
}

void DrawSquareUI(void *argument)
{

}