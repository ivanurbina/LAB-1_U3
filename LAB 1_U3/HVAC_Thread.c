 // FileName:        HVAC_Thread.c
 // Dependencies:    HVAC.h
 // Processor:       MSP432
 // Board:           MSP432P401R
 // Program version: CCS V8.3 TI
 // Company:         Texas Instruments
 // Description:     Definición de las funciones de los threads de HVAC_Thread().
 // Authors:         José Luis Chacón M. y Jesús Alejandro Navarro Acosta.
 // Updated:         11/2018

#include "HVAC.h"                           // Incluye definición del sistema.

bool boton = true;

/*********************************THREAD******************************************
 * Function: HVAC_Thread
 * Preconditions: None.
 * Overview: Realiza la lectura de la temperatura y controla salidas actualizando
 *           a su vez entradas. Imprime estados. También contiene el heartbeat.
 * Input:  Apuntador vacío que puede apuntar cualquier tipo de dato.
 * Output: None.
 *
 ********************************************************************************/
void *HVAC_Thread(void *arg0)
{
   bool flag = TRUE;


   SystemInit();
   flag &= HVAC_InicialiceIO();
   flag &= HVAC_InicialiceADC();
   flag &= HVAC_InicialiceUART();

   if(flag != TRUE)
   {
       printf("Error al crear archivo. Cierre del programa.\r\n");
       exit(1);
   }

   funcion_inicial();

   while(boton)
   {
       HVAC_PrintState();
       HVAC_Heartbeat();
       usleep(10000);
   }
   return 0;
}

void bye(void)  //Desenclava el while que llama a HVAC_PrintState(); y HVAC_Heartbeat(); cuando termina el programa
{
    boton = false;
}
