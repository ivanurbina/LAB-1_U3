 // FileName:        HVAC_IO.c
 // Dependencies:    HVAC.h
 // Processor:       MSP432
 // Board:           MSP432P401R
 // Program version: CCS V8.3 TI
 // Company:         Texas Instruments
 // Description:     Funciones de control de HW a través de estados.
 // Authors:         Ivan Urbina, Ernesto Arciniega, Luis Macias
 // Updated:         11/2022

#include "HVAC.h"

//Botones para persianas, inicio del sistema y secuencia de luces
#define Persiana_1   BSP_BUTTON1
#define Persiana_2   BSP_BUTTON2
#define ON_OFF       BSP_BUTTON3
#define SL_OF        BSP_BUTTON5

//LEDs de la tarjeta
#define ROJO    BSP_LED1
#define R       BSP_LED2
#define V       BSP_LED3
#define A       BSP_LED4

//Funcion para terminar programa
extern void bye(void);

/* Archivos sobre los cuales se escribe toda la información */

// Entradas y salidas.
FILE _PTR_ input_port = NULL, _PTR_ output_port = NULL;

//fd_adc para inicializar modulo, fd_ch_T, fd_ch_H, fd_ch_W 3 canales para cada luz
FILE _PTR_ fd_adc = NULL, _PTR_ fd_ch_T = NULL, _PTR_ fd_ch_H = NULL,_PTR_ fd_ch_W = NULL;

// Comunicación serial asíncrona.
FILE _PTR_ fd_uart = NULL;

//Banderas boleanas para el control del programa
bool event = FALSE;            // Evento I/O que fuerza impresión inmediata.
bool b_1 = false;
bool b_2 = false;
bool persiana1 = false;
bool persiana2 = false;
bool sl = false;            //Bandera para la secuencia de luces

//Banderas para arranque - paro
bool inicio = true;
bool final = false;
bool shut_down = false;

//Arreglos para convertir a caracter lo leido por el ADC e imprimirlo por UART
char lux1 [10];
char lux2 [10];
char lux3 [10];

//Arreglo que cuarda cada valor convertido a luxes (Escala 1/10), valx que guarda lo leido por el ADC, i y estado para control.
uint8_t luxes[3],i,estado = 0;
uint32_t val = 0,val1 = 0,val2 = 0;

// Estructuras iniciales.

const ADC_INIT_STRUCT adc_init =
{
    ADC_RESOLUTION_DEFAULT,                                                     // Resolución.
    ADC_CLKDiv8                                                                 // División de reloj.
};

//Canal para la Luz 1
const ADC_INIT_CHANNEL_STRUCT adc_ch_param =
{
    AN0,                                                                         // Fuente de lectura, 'ANx'.
    ADC_CHANNEL_MEASURE_LOOP,                                                    // Banderas de inicialización
    50000,                                                                       // Periodo en uS, base 1000.
    ADC_TRIGGER_1                                                                // Trigger lógico que puede activar este canal.
};

//Canal para la Luz 2
const ADC_INIT_CHANNEL_STRUCT adc_ch_param2 =
{
    AN1,                                                                         // Fuente de lectura, 'ANx'.
    ADC_CHANNEL_MEASURE_LOOP,                                                    // Banderas de inicialización
    50000,                                                                       // Periodo en uS, base 1000.
    ADC_TRIGGER_2                                                                // Trigger lógico que puede activar este canal.
};

//Canal para la Luz 3
const ADC_INIT_CHANNEL_STRUCT adc_ch_param3 =
{
    AN5,                                                                         // Fuente de lectura, 'ANx'.
    ADC_CHANNEL_MEASURE_LOOP,                                                    // Banderas de inicialización
    50000,                                                                       // Periodo en uS, base 1000.
    ADC_TRIGGER_3                                                                // Trigger lógico que puede activar este canal.
};

static uint_32 data[] =                                                          // Formato de las entradas.
{                                                                                // Se prefirió un solo formato.
    Persiana_1,
    Persiana_2,
    ON_OFF,
    SL_OF,
    GPIO_LIST_END
};

static const uint_32 R_LED[] =                                                    // Formato de los leds, uno por uno.
{
     ROJO,
     GPIO_LIST_END
};

static const uint_32 RGB_R[] =                                                   // Formato de los leds, uno por uno.
{
     R,
     GPIO_LIST_END
};

const uint_32 RGB_V[] =                                                         // Formato de los leds, uno por uno.
{
     V,
     GPIO_LIST_END
};

static const uint_32 RGB_A[] =                                                   // Formato de los leds, uno por uno.
{
     A,
     GPIO_LIST_END
};

/**********************************************************************************
 * Function: INT_SWI
 * Preconditions: Interrupción habilitada, registrada e inicialización de módulos.
 * Overview: Función que es llamada cuando se genera
 *           la interrupción
 * Input: None.
 * Output: None.
 **********************************************************************************/
void INT_SWI(void)
{
    Int_clear_gpio_flags(input_port);

    ioctl(input_port, GPIO_IOCTL_READ, &data);

    if((data[0] & GPIO_PIN_STATUS) == 0)        //Persiana 1
    {
        b_1 = true;

    }

    if((data[1] & GPIO_PIN_STATUS) == 0)      //Persiana 2
    {
        b_2 = true;
    }

    if((data[2] & GPIO_PIN_STATUS) == NORMAL_STATE_EXTRA_BUTTONS)   //Secuencia de luces
    {
        sl ^= true;
        event = true;
    }

    if((data[3] & GPIO_PIN_STATUS) == NORMAL_STATE_EXTRA_BUTTONS)   //Boton de arranque - paro
    {
        if(estado == 2)
            shut_down = true;

        if(estado == 1)
        {
             final = true;
             estado = 2;
        }

         if(estado == 0)
         {
             inicio = false;
             estado = 1;
         }
    }

    return;
}

/*FUNCTION******************************************************************************
*
* Function Name    : HVAC_InicialiceIO
* Returned Value   : boolean; inicialización correcta.
* Comments         :
*    Abre los archivos e inicializa las configuraciones deseadas de entrada y salida GPIO.
*
*END***********************************************************************************/

boolean HVAC_InicialiceIO(void)
{
    // Estructuras iniciales de entradas y salidas.

    const uint_32 output_set[] =
    {
        ROJO   | GPIO_PIN_STATUS_0,
        R      | GPIO_PIN_STATUS_0,
        V      | GPIO_PIN_STATUS_0,
        A      | GPIO_PIN_STATUS_0,
        GPIO_LIST_END
    };

    const uint_32 input_set[] =
    {
        Persiana_1,
        Persiana_2,
        ON_OFF,
        SL_OF,
        GPIO_LIST_END
    };

    // Iniciando GPIO.
    ////////////////////////////////////////////////////////////////////

    output_port =  fopen_f("gpio:write", (char_ptr) &output_set);
    input_port =   fopen_f("gpio:read", (char_ptr) &input_set);

    if (output_port) { ioctl(output_port, GPIO_IOCTL_WRITE_LOG0, NULL); }   // Inicialmente salidas apagadas.
    ioctl (input_port, GPIO_IOCTL_SET_IRQ_FUNCTION, INT_SWI);               // Declarando interrupción.

    return (input_port != NULL) && (output_port != NULL);
}

/*FUNCTION******************************************************************************
*
* Function Name    : HVAC_InicialiceADC
* Returned Value   : boolean; inicialización correcta.
* Comments         :
*    Abre los archivos e inicializa las configuraciones deseadas para
*    el módulo general ADC y tres de sus canales.
*
*END***********************************************************************************/
boolean HVAC_InicialiceADC (void)
{
    // Iniciando ADC y canales.
    fd_adc   = fopen_f("adc:",  (const char*) &adc_init);               // Módulo.
    fd_ch_T =  fopen_f("adc:1", (const char*) &adc_ch_param);           // Canal uno.
    fd_ch_H =  fopen_f("adc:2", (const char*) &adc_ch_param2);          // Canal dos.
    fd_ch_W =  fopen_f("adc:3", (const char*) &adc_ch_param3);          // Canal dos.

    return (fd_adc != NULL) && (fd_ch_H != NULL) && (fd_ch_T != NULL);  // Valida que se crearon los archivos.
}

/*FUNCTION******************************************************************************
*
* Function Name    : HVAC_InicialiceUART
* Returned Value   : boolean; inicialización correcta.
* Comments         :
*    Abre los archivos e inicializa las configuraciones deseadas para
*    configurar el modulo UART (comunicación asíncrona).
*
*END***********************************************************************************/
boolean HVAC_InicialiceUART (void)
{
    // Estructura inicial de comunicación serie.
    const UART_INIT_STRUCT uart_init =
    {
        /* Selected port */        1,
        /* Selected pins */        {1,2},
        /* Clk source */           SM_CLK,
        /* Baud rate */            BR_115200,

        /* Usa paridad */          NO_PARITY,
        /* Bits protocolo  */      EIGHT_BITS,
        /* Sobremuestreo */        OVERSAMPLE,
        /* Bits de stop */         ONE_STOP_BIT,
        /* Dirección TX */         LSB_FIRST,

        /* Int char's \b */        NO_INTERRUPTION,
        /* Int char's erróneos */  NO_INTERRUPTION
    };

    // Inicialización de archivo.
    fd_uart = fopen_f("uart:", (const char*) &uart_init);

    return (fd_uart != NULL); // Valida que se crearon los archivos.
}

/*FUNCTION******************************************************************************
*
* Function Name    : funcion_inicial
* Returned Value   : None.
* Comments         :
*   Funcion que se ejecuta al inicar el sistema, espera que se presione l boton 2.5
*
*END***********************************************************************************/
void funcion_inicial(void)
{
    printf("Presione el boton P2.5 para iniciar\r\n");
    while(inicio);                                          //Espera que se presione, se desenclava en la interrupción.
    printf("Aplicacion iniciada\r\n");
    ioctl(output_port, GPIO_IOCTL_WRITE_LOG1, &R_LED);      //Prende LED rojo
}

/*FUNCTION******************************************************************************
*
* Function Name    : HVAC_Heartbeat
* Returned Value   : None.
* Comments         :
*
*   Funcion que arranca los canales del ADC, lee los valores y evalua los estados del
*   sistema para pararlo, secuencia de luces y la secuencia con el RGB
*END***********************************************************************************/
void HVAC_Heartbeat(void)
{

   boolean flag = TRUE;
   static boolean bandera_inicial = 0;

   if(bandera_inicial == 0)
   {
       ioctl (fd_ch_H, IOCTL_ADC_RUN_CHANNEL, NULL);
       ioctl (fd_ch_T, IOCTL_ADC_RUN_CHANNEL, NULL);
       ioctl (fd_ch_W, IOCTL_ADC_RUN_CHANNEL, NULL);
       bandera_inicial = 1;
   }

   //Valor se guarda en val, flag nos dice si fue exitoso.

    flag =  (fd_adc && fread_f(fd_ch_H, &val, sizeof(val))) ? 1 : 0;

    if(flag != TRUE)
    {
        printf("Error al leer archivo. Cierre del programa\r\n");
        exit(1);
    }

    flag =  (fd_adc && fread_f(fd_ch_T, &val1, sizeof(val1))) ? 1 : 0;

    if(flag != TRUE)
    {
        printf("Error al leer archivo. Cierre del programa\r\n");
        exit(1);
    }

    flag =  (fd_adc && fread_f(fd_ch_W, &val2, sizeof(val2))) ? 1 : 0;

    if(flag != TRUE)
    {
        printf("Error al leer archivo. Cierre del programa\r\n");
        exit(1);
    }

    //Conversion a escala de 1-10 leida por el ADC
    luxes[0] = (val*11) / 16384;
    luxes[1] = (val1*11) / 16384;
    luxes[2] = (val2*11) / 16384;

    if(b_1) //Si se presion el boton P1.1, abre o cierra la persiana 1
    {
        O_C_P1();
        b_1 = false;
    }

    if(b_2) //Si se presion el boton P1.4, abre o cierra la persiana 2
    {
        O_C_P2();
        b_2 = false;
    }

    if(sl)  //Si se presiona el boton P2.3 prende o apaga la secuencia de luces
        secuencia();
    else
    {
        ioctl(output_port, GPIO_IOCTL_WRITE_LOG0, &RGB_R);
        ioctl(output_port, GPIO_IOCTL_WRITE_LOG0, &RGB_V);
        ioctl(output_port, GPIO_IOCTL_WRITE_LOG0, &RGB_A);
    }

    if(final)   //Si se presiona el boton P2.5 entra en la ventana de 5 seg para terminar
        terminar_programa();

    return;
}

/*FUNCTION******************************************************************************
*
* Function Name    : HVAC_PrintState
* Returned Value   : None.
* Comments         :
*    Imprime via UART la situación actual del sistema
*END***********************************************************************************/
void HVAC_PrintState(void)
{
    static char iterations = 0;

    iterations++;                                           // A base de iteraciones para imprimir cada cierto tiempo.
    if(iterations >= ITERATIONS_TO_PRINT || event == TRUE)
    {
        iterations = 0;
        event = FALSE;

        print("\n\r");

        sprintf(lux1,"Luz 1 = %d\n\r",luxes[0]);
        print(lux1);

        sprintf(lux2,"Luz 2 = %d\n\r",luxes[1]);
        print(lux2);

        sprintf(lux3,"Luz 3 = %d\n\r",luxes[2]);
        print(lux3);

        if(persiana1)
            print("Persiana 1 abierta\n\r");
        else
            print("Persiana 1 cerrada\n\r");

        if(persiana2)
            print("Persiana 2 abierta\n\r");
        else
            print("Persiana 2 cerrada\n\r");

        if(sl)
            print("Secuencia encendida\n\r");
        else
            print("Secuencia apagada\n\r");

    }
}

/*FUNCTION******************************************************************************
*
* Function Name    :  O_C_P1
* Returned Value   : None.
* Comments         :
*    Abre o cierra la persiana 1 segun el estado acutual de la misma
*END***********************************************************************************/
void O_C_P1(void)
{
    if(!persiana1)
    {
        persiana1 = true;
        print("P1 UP...\n\r");
    }
    else if(persiana1)
    {
        persiana1 = false;
        print("P1 DOWN...\n\r");
    }

    sleep(5);
}

/*FUNCTION******************************************************************************
*
* Function Name    :  O_C_P2
* Returned Value   : None.
* Comments         :
*    Abre o cierra la persiana 2 segun el estado acutual de la misma
*END***********************************************************************************/
void O_C_P2(void)
{
    if(!persiana2)
    {
        persiana2 = true;
        print("P2 UP...\n\r");
    }
    else if(persiana2)
    {
        persiana2 = false;
        print("P2 DOWN...\n\r");
    }

    sleep(5);
}

/*FUNCTION******************************************************************************
*
* Function Name    : secuencia
* Returned Value   : None.
* Comments         :
*   Realiza la secuencia de colores con el RGB
*END***********************************************************************************/
void secuencia(void)
{
    switch(i)   //Genera distintos colores con el RGB
    {
        case 0:
            ioctl(output_port, GPIO_IOCTL_WRITE_LOG1, &RGB_R);
            ioctl(output_port, GPIO_IOCTL_WRITE_LOG0, &RGB_V);
            ioctl(output_port, GPIO_IOCTL_WRITE_LOG0, &RGB_A);
            usleep(500000);
            i++;
        break;

       case 1:
           ioctl(output_port, GPIO_IOCTL_WRITE_LOG0, &RGB_R);
           ioctl(output_port, GPIO_IOCTL_WRITE_LOG1, &RGB_V);
           ioctl(output_port, GPIO_IOCTL_WRITE_LOG0, &RGB_A);
           usleep(500000);
           i++;
        break;

       case 2:
           ioctl(output_port, GPIO_IOCTL_WRITE_LOG1, &RGB_R);
           ioctl(output_port, GPIO_IOCTL_WRITE_LOG1, &RGB_V);
           ioctl(output_port, GPIO_IOCTL_WRITE_LOG0, &RGB_A);
           usleep(500000);
           i++;
        break;

       case 3:
           ioctl(output_port, GPIO_IOCTL_WRITE_LOG0, &RGB_R);
           ioctl(output_port, GPIO_IOCTL_WRITE_LOG0, &RGB_V);
           ioctl(output_port, GPIO_IOCTL_WRITE_LOG1, &RGB_A);
           usleep(500000);
           i++;
        break;

       case 4:
           ioctl(output_port, GPIO_IOCTL_WRITE_LOG1, &RGB_R);
           ioctl(output_port, GPIO_IOCTL_WRITE_LOG0, &RGB_V);
           ioctl(output_port, GPIO_IOCTL_WRITE_LOG1, &RGB_A);
           usleep(500000);
           i++;
        break;

       case 5:
           ioctl(output_port, GPIO_IOCTL_WRITE_LOG0, &RGB_R);
           ioctl(output_port, GPIO_IOCTL_WRITE_LOG1, &RGB_V);
           ioctl(output_port, GPIO_IOCTL_WRITE_LOG1, &RGB_A);
           usleep(500000);
           i++;
        break;

       case 6:
           ioctl(output_port, GPIO_IOCTL_WRITE_LOG1, &RGB_R);
           ioctl(output_port, GPIO_IOCTL_WRITE_LOG1, &RGB_V);
           ioctl(output_port, GPIO_IOCTL_WRITE_LOG1, &RGB_A);
           usleep(500000);
           i = 0;
        break;
    }
    event = true;   //Forza la impresion
}

/*FUNCTION******************************************************************************
*
* Function Name    : terminar_programa
* Returned Value   : None.
* Comments         :
*   Es llamada cuando se desea entrar a la ventana de 5 segundos para terminar el
*   programa, si se presiona por segunda vez el boton de P2.5, llama a la funcion
*   bye(); para terminar el programa
*END***********************************************************************************/
void terminar_programa(void)
{
    uint8_t i;

    print("Vuelva a presionar si desea terminar tiene 5seg\n\r");

    for(i = 0; i<50; i++)   //Genera la ventana de 5 segundos
    {
        usleep(100000);     //50 * 100,000us = 5Mus = 5seg
        if(shut_down)       //Bandera se hace true cuando se presiona el boton por segunda vez
        {
            print("Aplicacion terminada");
            ioctl(output_port, GPIO_IOCTL_WRITE_LOG0, &R_LED);
            ioctl(output_port, GPIO_IOCTL_WRITE_LOG0, &RGB_R);
            ioctl(output_port, GPIO_IOCTL_WRITE_LOG0, &RGB_V);
            ioctl(output_port, GPIO_IOCTL_WRITE_LOG0, &RGB_A);
            i = 50;                                            //No se desea imprimir hasta que termine el for
            bye();                                             //Termina el programa
        }
    }

    if(!shut_down)  //Si no se presiona por segunda vez, continua el programa e imprime por UART
    {
        print("\n\rSe termino el tiempo, app continua...");
        final = false;
        estado = 1;
        usleep(500000); //Delay para ver el mensaje anterior
    }
}
