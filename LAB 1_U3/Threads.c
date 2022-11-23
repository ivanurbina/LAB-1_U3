 // FileName:        Threads.c
 // Dependencies:    HVAC.h
 // Processor:       MSP432
 // Board:           MSP432P401R
 // Program version: CCS V8.3 TI
 // Company:         Texas Instruments
 // Description:     Activa las funciones del hilo HVAC_thread.
 //                  Main del proyecto.
 // Authors:         José Luis Chacón M. y Jesús Alejandro Navarro Acosta.
 // Updated:         11/2018

#include "HVAC.h"

extern void *HVAC_Thread(void *arg0);   // Thread que arrancará inicialmente.

int main(void)
{
    pthread_t           hvac_thread;
    pthread_attr_t      pAttrs;
    struct sched_param  priParam;
    int                 retc;
    int                 detachState;

   pthread_attr_init(&pAttrs);                                  /* Reinicio de parámetros. */

   detachState = PTHREAD_CREATE_DETACHED;                       // Los recursos se liberarán después del término del thread.
   retc = pthread_attr_setdetachstate(&pAttrs, detachState);    // Además, al hilo no se le puede unir otro (join).
   if (retc != 0) { while (1); }

   /**********************
   ** Thread principal. **
   **********************/

   priParam.sched_priority = 1;                                     // Mayor prioridad a la tarea principal.
   retc |= pthread_attr_setstacksize(&pAttrs, THREADSTACKSIZE1);    // Así se determinaría el tamaño del stack.
   if (retc != 0) { while (1); }
   pthread_attr_setschedparam(&pAttrs, &priParam);
   retc = pthread_create(&hvac_thread, &pAttrs, HVAC_Thread, NULL); // Creación del thread.
   if (retc != 0) { while (1); }

   /* Arranque del sistema. */
   BIOS_start();

   return (0);
}
