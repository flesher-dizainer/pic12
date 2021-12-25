/*
 * File:   main.c
 * Author: Dom
 *
 * Created on 10 декабря 2021 г., 11:47
 */


#include <xc.h>
#include <pic.h>
#include <pic12f629.h>

typedef union {
    struct {
        unsigned start_reley                   :1;
        unsigned reserv2                    :1;
        unsigned SEARCH_TIME_BIT           :1;
        unsigned TMR1IF                    :1;
        unsigned T0IF                   :1;
        unsigned uart_ok                    :1;
    };
} flag_gp_t;
volatile  flag_gp_t flag_gp;
volatile unsigned int time_bit = 0;//время бита
volatile unsigned int time_bit_old = 0;//время бита
volatile unsigned char time_bit_count = 0;//количество измерений поиска времени бит

volatile unsigned long time_tmr1 = 0;//
volatile unsigned long time_tmr2 = 0;//
const unsigned long  count_gpio =  16000;
const unsigned long sleep_count =  300000;

char uart_data = 0;
char flag_on ; //флаг для разрешения включения компрессора
char EEPROM_DATA = 0;
char EEPROM_ADR = 0;

// CONFIG
#pragma config FOSC = INTRCIO   // Oscillator Selection bits (INTOSC oscillator: I/O function on GP4/OSC2/CLKOUT pin, I/O function on GP5/OSC1/CLKIN)
#pragma config WDTE = OFF       // Watchdog Timer Enable bit (WDT disabled)
#pragma config PWRTE = OFF      // Power-Up Timer Enable bit (PWRT disabled)
#pragma config MCLRE = OFF       // GP3/MCLR pin function select (GP3/MCLR pin function is MCLR)
#pragma config BOREN = OFF      // Brown-out Detect Enable bit (BOD disabled)
#pragma config CP = OFF         // Code Protection bit (Program Memory code protection is disabled)
#pragma config CPD = OFF        // Data Code Protection bit (Data memory code protection is disabled)

void setup (void){
   //для назначения входов и выходов, нужно указать банк 1 и настроить trisio
   // STATUS = 0x20;//выбор банка 1
    CMCON = 0x07;
    STATUSbits.RP0 = 1;//выбор банка 1
    OPTION_REG = 3; //делитель таймера 1
    TRISIO = 0xff;//все порты как входы
    TRISIObits.TRISIO2 = 0; //порт2 как выход реле
    TRISIObits.TRISIO1 = 0; //порт1 как выход светодиода
    IOCbits.IOCB4 = 1; //разрешаем прерывание по изменению на входе порта 4
    PIE1bits.TMR1IE = 1; //разрешили прерывание таймера 1
    STATUSbits.RP0 = 0;//выбор банка 0
    GPIObits.GP1 = 1;//зажигаем диод
    GPIObits.GP2 = 0;//отключаем реле
    time_bit_count = 0;//обнуление времени бита
    time_bit = 0;//обнуление количества измерений времени бита
    time_bit_old = 0;
    time_tmr1 = 0;
    time_tmr2 = 0;
    //INTCON = 0x00;//обнуление intcon
   // T1CONbits.T1CKPS0 = 1;
    //T1CONbits.T1CKPS1 = 1;//делитель таймера 1 /8
   // T1CONbits.TMR1ON; //включили таймер 1
    T1CON = 0x01;//делитель таймера 1(8), выкл таймера 1
    INTCONbits.GPIE = 0; //разрешили прерывание от изменения сигнала на gpio
    INTCONbits.PEIE = 1; //разрешили прерывание от перефирии, для прерывания от таймера 1
    INTCONbits.T0IE = 0; //разрешили прерывание от таймера 0
    INTCONbits.GIE = 1;//разрешили глобальное прерывание
    //INTCON = 0xE8; //0bx11101000
    flag_gp.SEARCH_TIME_BIT = 0;
    flag_gp.T0IF = 0;
    flag_gp.TMR1IF = 0;
    flag_gp.start_reley = 0;
    flag_gp.uart_ok = 0;
    uart_data = 0;

}
  void save_eeprom(){
      STATUSbits.RP0 = 0;
      PIR1bits.EEIF = 0;
      STATUSbits.RP0 = 1;
      EEADR = EEPROM_ADR;
      EEDATA = EEPROM_DATA;
      EECON1bits.WREN = 1;
      INTCONbits.GIE = 0;
      EECON2 = 0x55;
      EECON2 = 0xAA;
      EECON1bits.WR = 1;
      STATUSbits.RP0 = 0;
      while( PIR1bits.EEIF == 0 ){}
      STATUSbits.RP0 = 1;
      EECON1bits.WREN = 0;//сбросить в 0 после окончания записи
      INTCONbits.GIE = 1;
  }
  

  void start_nasos(void){
   if ( flag_gp.start_reley == 1 ){
      GPIObits.GP2 = 1;
      time_tmr1 = 0;
      }
  }
  void stop_nasos (void){
      if ( GPIObits.GP2 == 1 ){
         flag_gp.start_reley = 0;//
         GPIObits.GP2 = 0;
         time_tmr2 = 0;
         GPIObits.GP1 =1;
         INTCONbits.GPIE = 0;
      }
  }
  
void main(void) {
   unsigned char count_uart;
    static char array_uart[8];
    setup();
    while(1){
        //основной поток программы
        //55 DD 77 5F DD D7 F7 FD
        //55 DD 77 5F DD D7 F7 FD - inable relay
        if ( flag_gp.start_reley == 0 ){
            if (time_tmr2 > sleep_count){
                flag_gp.start_reley = 1;
                time_tmr2 = 0;
                GPIObits.GP1 = 0;
                INTCONbits.GPIE = 1;
            }
        }else{
            time_tmr2 = 0;
        }
        
        if (GPIObits.GP2 == 1){
            if ( time_tmr1 > count_gpio){
             stop_nasos();   
            }
        }

        if ( flag_gp.SEARCH_TIME_BIT == 1){
            if ( flag_gp.uart_ok == 1 ){                
                flag_gp.uart_ok = 0;
                   
                if ( uart_data == 0x55 ){
                  count_uart = 0;  
                }
                
                array_uart[count_uart] = uart_data;
                
                if ( count_uart == 3 ){
                    if (array_uart[0] == 0x55 ){
                        if (array_uart[1] == 0x77 ){
                            stop_nasos();
                        }                        
                    }
                }
                
                if ( count_uart == 7 ){
                   for ( char i = 0; i < 8; i++){
                      //55 DD 77 5F DD D7 F7 FD - inable relay 
                      if ( array_uart[0] == 0x55 ){
                          if ( array_uart[1] == 0xDD ){
                               if ( array_uart[2] == 0x77 ){
                                   if ( array_uart[3] == 0x5F ){
                                       start_nasos();
                                  }                         
                              }                         
                          }                         
                      }
                          
                   } 
                }
 
               count_uart++;
               if (count_uart == 8){count_uart = 0;}
            }
            

        }
    }
    return;
}

void interrupt global_interrup (void){
    STATUSbits.RP0 = 0;
    INTCONbits.GIE = 0;
    
    if ( INTCONbits.GPIE == 1 ){
        if(INTCONbits.GPIF == 1) {
            //поиск времени бита
            if ( flag_gp.start_reley == 1){
            if ( flag_gp.SEARCH_TIME_BIT == 0 ){
                //GPIObits.GP1 = 0;
            //работаем через таймер 1
            //откл глобал прерывания
            
            while ( time_bit_count < 20 ){
                //ост таймер 1
                T1CON = 0x00;
                //убираем флаг прерывания tmr1
                PIR1bits.TMR1IF = 0;
                //обнуляем т1
                TMR1 = 0x0000;

                //ждём высокого уровня 
                while ( GPIObits.GP4 == 0 ){}
                //ждём низкого уровня
                while ( GPIObits.GP4 == 1){}
                //вкл таймер 1
                T1CON = 0x01;
                //ждём высоко уровня
                while ( GPIObits.GP4 == 0 ){}
                //ост таймер 1
                T1CON = 0x00;
                //считываем значение таймера 1
                //time_bit = 0xFFFF * time_bit_tmr1;
                time_bit = TMR1;
                T1CON = 0x01;
                //увеличиваем количество считанных периодов на 1

                if ( time_bit_count > 3){
                    if ( time_bit > time_bit_old ){
                       time_bit =  time_bit_old;
                    }
                }
                
                time_bit_count++;
                time_bit_old = time_bit;
            }
            flag_gp.SEARCH_TIME_BIT = 1;
            
            //нужно сохранить время бита в еепром 4 байта
            for (char i = 0; i < 2; i++){
                EEPROM_DATA = ( time_bit >> (( 1 - i ) * 8 ) )& 0xFF;
               //EEPROM_DATA
               EEPROM_ADR = i;
               save_eeprom();
            }
           // GPIObits.GP1 = 1;            
            }
            }
            //---------
//urta data read
            if ( flag_gp.start_reley == 1 ){
            if ( flag_gp.SEARCH_TIME_BIT == 1 ){
                if ( GPIObits.GP4 == 0 ){ 
                    uart_data = 0;
                    //откл таймер 1
                    T1CON = 0x00;
                    //ждём половину времени
                    TMR1 = 0xFFFF - ( time_bit / 2 );
                    T1CON = 0x01;
                    PIR1bits.TMR1IF = 0;
                    while ( PIR1bits.TMR1IF == 0 ){}//ждём пол бита
                    //проверяем старт бит
                    if (GPIObits.GP4 == 1){
                        INTCONbits.GIE = 1;
                        flag_gp.uart_ok = 0;                        
                        return;
                    }
                    
                    PIR1bits.TMR1IF = 0;
                    TMR1 = 0xFFFF - time_bit;
                    while ( PIR1bits.TMR1IF == 0 ){}//ждём бит
                    PIR1bits.TMR1IF = 0;
                    TMR1 = 0xFFFF - time_bit;
                    if ( GPIObits.GP4 == 1){
                        uart_data = 0x01;
                    }
                    while ( PIR1bits.TMR1IF == 0 ){}//ждём бит
                    PIR1bits.TMR1IF = 0;
                    TMR1 = 0xFFFF - time_bit;
                    if ( GPIObits.GP4 == 1){
                        uart_data += ( 1 << 1 );
                    } 
                    while ( PIR1bits.TMR1IF == 0 ){}//ждём бит
                    PIR1bits.TMR1IF = 0;
                    TMR1 = 0xFFFF - time_bit;
                    if ( GPIObits.GP4 == 1){
                        uart_data += ( 1 << 2 );
                    }
                    while ( PIR1bits.TMR1IF == 0 ){}//ждём бит
                    PIR1bits.TMR1IF = 0;
                    TMR1 = 0xFFFF - time_bit;
                    if ( GPIObits.GP4 == 1){
                        uart_data += ( 1 << 3 );
                    }
                    while ( PIR1bits.TMR1IF == 0 ){}//ждём бит
                    PIR1bits.TMR1IF = 0;
                    TMR1 = 0xFFFF - time_bit;
                    if ( GPIObits.GP4 == 1){
                        uart_data += ( 1 << 4 );
                    } 
                    while ( PIR1bits.TMR1IF == 0 ){}//ждём бит
                    PIR1bits.TMR1IF = 0;
                    TMR1 = 0xFFFF - time_bit;
                    if ( GPIObits.GP4 == 1){
                        uart_data += ( 1 << 5 );
                    }
                    while ( PIR1bits.TMR1IF == 0 ){}//ждём бит
                    PIR1bits.TMR1IF = 0;
                    TMR1 = 0xFFFF - time_bit;
                    if ( GPIObits.GP4 == 1){
                        uart_data += ( 1 << 6 );
                    }
                    while ( PIR1bits.TMR1IF == 0 ){}//ждём бит
                    PIR1bits.TMR1IF = 0;
                    TMR1 = 0xFFFF - time_bit;
                    if ( GPIObits.GP4 == 1){
                        uart_data += ( 1 << 7 );
                    }
                    while ( PIR1bits.TMR1IF == 0 ){}//ждём stop bit
                    if ( GPIObits.GP4 == 0){
                        //всё плохо
                        INTCONbits.GIE = 1;
                        flag_gp.uart_ok = 0;                        
                        return;                        
                    }
                    flag_gp.uart_ok = 1;
                }
            }
            //нужно прочитать порт перед сбросом флага прерывания
            char R = GPIObits.GP4; 
            INTCONbits.GPIF = 0;
            //flag_gp.GPIF = 1;
        }
        }
    }

    if ( INTCONbits.T0IE == 1){
       if ( INTCONbits.T0IF == 1 ){          
           //flag_gp.T0IF = 1;
           INTCONbits.T0IF = 0;
       }
    }
    
    if (INTCONbits.PEIE == 1){
        if( PIR1bits.TMR1IF == 1 ){ 
           // flag_gp.TMR1IF = 1;
            PIR1bits.TMR1IF = 0;
            TMR1 = 0xFFFF - 1030;
            time_tmr1++;
            time_tmr2++;
        }
    }
    INTCONbits.GIE = 1;
}
