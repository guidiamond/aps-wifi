/*
 * IncFile1.h
 *
 * Created: 20/06/2020 17:15:30
 *  Author: Damn DiamonD
 */ 


#ifndef DEFS_H_
#define DEFS_H_

// LED
#define LED_PIO           PIOC                  // periferico que controla o LED
#define LED_PIO_ID        ID_PIOC               // ID do periférico PIOC (controla LED)
#define LED_IDX           8u                    // ID do LED no PIO
#define LED_IDX_MASK      (1u << LED_IDX)       // Mascara para CONTROLARMOS o LED

// Configuracoes do botao
#define BUT_PIO           PIOA
#define BUT_PIO_ID		  ID_PIOA
#define BUT_PIO_IDX       11
#define BUT_PIO_IDX_MASK (1 << BUT_PIO_IDX)

#define ID_PLACA		          "234"		  
#define TASK_WIFI_STACK_SIZE      (6*4096/sizeof(portSTACK_TYPE))
#define TASK_WIFI_PRIORITY        (1)
#define TASK_PROCESS_STACK_SIZE   (4*4096/sizeof(portSTACK_TYPE))
#define TASK_PROCESS_PRIORITY     (0)


#define AFEC_POT AFEC0
#define AFEC_POT_ID ID_AFEC0
#define AFEC_POT_CHANNEL 0
#define TASK_LCD_STACK_SIZE            (4*1024/sizeof(portSTACK_TYPE))
#define TASK_LCD_STACK_PRIORITY        (tskIDLE_PRIORITY)
#endif 