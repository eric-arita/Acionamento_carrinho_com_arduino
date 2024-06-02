#define F_CPU 16000000UL
#include<avr/io.h>
#include<avr/interrupt.h>
#include<util/delay.h>                         


volatile unsigned char estado = 1; // Variável que indica se o pulso do terminal ECHO já terminou. 0 se sim, 1 se não( ou ainda não recebeu mas não faz diferença). 
volatile unsigned char estado_LED = 0; // Variável que indica se o led será aceso(1) ou se será apagado(0).
volatile unsigned char pulso = 0; // variavel que vale 1 quando foi gerado um pulso de 50us no terminal TRIG.
unsigned int lim = 20000; // Variável que controla o período (na verdade o meio-período) que o led fica aceso ou apagado de acordo com a distâcia. 

/*Variáveis contadoras do Timer 0*/
volatile unsigned int cont = 0; // Contadora associada á geração de pulso no terminal TRIG. 
volatile unsigned int cont_comando = 0;// Contadora responsável do reenvio das menságens após 1 segundo. 
volatile unsigned int cont_echo = 0; // Contador de interrupção do temporizador 0 após detectar uma borda de subida em PD2, terminal ECHO.
unsigned int cont_LED = 0; // Contadora do período do LED.


volatile unsigned int T = 0; // Salva o valor de cont_echo após uma borda de descida em PD2.

float distancia; // Armazena a distância medida.
int  num;// Armazena a distância medida como int.

/* VARIAVEIS ASSOCIADAS AOS COMANDOS DA UART */
volatile unsigned char novo_comando = 1; // Quando receber um comando novo, esta variável terá valor 1 e 0 caso contrário. Inicializamos com 1 para que seja enviada a mensagem ao ligar o dispositivo.
volatile unsigned char comando; // Esta variável armazena um comando enviado pela serial, válido ou não.
volatile unsigned char comando_valido = 'q'; // Esta armazena apenas comandos válidos. O propósito é para que comandos inválidos não interiam no programa, ou seja para ignorar comandos inválidos.
volatile unsigned char print_comando = 1; // Vaiável associada ao estado que monitora o envio das menságens. Que ocorre ao iniciar o programa, ao receber uma nova mensagem, e um segundo após a última mensagem transmitida. 
volatile unsigned char comando_DC; 
volatile unsigned char comando_motor; 



/* VARIAVEIS ASSOCIADAS AO DUTY CYCLE*/
volatile unsigned int duty_cycle = 0x3B; // Variável que faz a atualização de OCR0B. Inicialente duty cycle de 60%.
volatile unsigned int sentido = -1; // Esta variavél SENTIDO representa o processo de aumento e diminuição do duty cycle, em outras palavras aceleração e desaceleração do motor, valendo 0 e 1 respectivamente. Por exemplo, tem o valor 0 se o duty cycle vai de 60% para 80% ou 100%. Terá o valor -1 quando não precisa alterar o duty cycle.
volatile unsigned char comando_DC_anterior = '6'; // Esta variável salva o útlimo comando associado ao duty cycle para determinar o sentido do próximo comando relacionado ao duty cycle.

volatile unsigned char i = 0; // Variável que irá percorrer as strings.

/* Mensagens a serem enviadas*/
char comando_w[] = "FRENTE\n";
char comando_OBS[] = "OBSTACULO!\n";
char comando_s[] = "TRAS\n";
char comando_a[] = "ANT-HORARIO\n";
char comando_d[] = "HORARIO\n";
char comando_q[] = "PARADO\n";
char comando_e[] = "DDDcm\n";
char comando_6[] = "Velocidade 60%\n";
char comando_8[] = "Velocidade 80%\n";
char comando_0[] = "Velocidade 100%\n";




/* Inicializacoes dos perifericos envolvidos no sistema. */
void configuracoes(void);
/* Função responsável pelo acionamento dos motores, Para frente, para trás, etc. */
void aciona_motor(void);
/* Função que acende ou apaga o led. */
void aciona_LED(void);

/* Rotina de Serviço de Interrupção do tipo Pin Change. */
ISR(PCINT2_vect);
/* Rotina de Serviço de Interrupção do TIMER 0. */
ISR(TIMER0_OVF_vect);

/* Função associada a Rotina Serviço de Interrupção do tipo Buffer de transmissão vazio. */
ISR(USART_UDRE_vect);

/* Função associada à Rotina Serviço Interrupção do tipo Recepção Completa. */
ISR(USART_RX_vect);

/* Rotina de Serviço de Interrupção do TIMER 2. */
ISR(TIMER2_OVF_vect);



/* Este código é um programa do sistema de um carrinho de controle remoto envolvendo os conceitos visto até agora na diciplina de EA871. O seu algorítimo consite na configuração inicial do sistema pela função configurações (linha 87) e no loop infinito do laço while. Dentro do laço o programa verifica primeiramente se há mensagens novas recebidas pela condicional da linha 91, caso a condição seja verdadeira o seta o bit 5, UDRIE, do registrador UCSR0B que habilita a interrupção do tipo buffer vazio assim permitindo o envio de mensagem pela UART. Temos uma condicional na linha 95 que separa o comando associados ao motor e ao duty cycle. Isso foi feito para casos em que quando retirar o obstáculo após o motor ter parado, fará com que o motor volte á ir pra frente caso último comando associado à direção movimento tenha sido w. Chamamos então a funçao de movimento aciona_motor acionando o motor de acordo com o comando recebido ou .Em seguida temos um switch avaliando a variável COMANDO_DC para alterar a velocidade do motor via PWM, caso necessário. E então zeramos a variável NOVO_COMANDO, linha 129, indicando que o comando recebido foi executado, sendo esta a última instrução da condicional da linha 91. A seguir temos outro IF responsável pela transmissão de mensagem a cada 1 segundo, se a condição for verdadeira, setando a flag UDRIE de UCSR0B e zerando a variável print_comando, esta será setada após 1 segundo dentro do timer 0. Chegamos então à condicional da linha 142 no while que monitora a variável ESTADO, associado ao termino de recepção do sinal echo. Se ESTADO vale zero, calcula-se a distância e armazenamos o resultado na variável DISTANCIA. Ainda dentro do mesmo IF, temos outras condicionais aninhadas que são encarregadas  pela e atualização da variável LIM, que por sua vez desempenha o papel no controle da frequência com que o LED pisca. E por fim atribuimos o valor 1 para ESTADO, que será zerado após uma interrupção PCINT de borda de descida (linha 265). Por ultimo invocamos a função aciona_led que acende ou apaga o led. Assim fechamos um ciclo do laço. Alguns detalhes foram omitidos neste texto mas cada detalhe será devidamente explicado nos comentários junto as instruções.*/




int main(void){
 
    /* Inicializacoes dos perifericos envolvidos no sistema */
  configuracoes();

    /* Loop infinito */
    while (1) {
        if(novo_comando == 1){
            UCSR0B |= 0x20;
            
            /*Esta condicional que separa o comando associado ao movimento do motor( saídas de A1 até A4) e relacionado ao duty cycle serve para casos especícos onde ao remover o obstáculo o motor irá para frente, se este for o último comando de movimento.*/
            if(comando_valido =='6'||comando_valido =='8'||comando_valido =='0'){
                comando_DC = comando_valido;
                }
            else{
                if(comando_valido != 'e')
                    comando_motor = comando_valido;
                }
             
             aciona_motor();
            //condicao associada ao duty cycle    
            switch(comando_DC){
            
                case '0': duty_cycle = 0x63;
                          if(comando_DC_anterior != '0')
                              sentido = 0;
                          comando_DC_anterior = comando_DC;
                          break;
                case '8': duty_cycle = 0x4F;
                          if(comando_DC_anterior == '0')
                              sentido = 1;
                          else{
                              if(comando_DC_anterior == '6')
                                  sentido = 0;
                              else
                                  sentido = -1;
                              }
                          comando_DC_anterior = comando_DC;
                          break;
                case '6': duty_cycle = 0x3B;
                          if(comando_DC_anterior != '6')
                              sentido = 1;
                          comando_DC_anterior = comando_DC;
                          break;
                }
            novo_comando = 0;
            }
        else{
            aciona_motor();
            }
        
        //controle de mensagem
        if(print_comando == 1){
            UCSR0B |= 0x20;
            print_comando = 0;
            }
        
        //sonar
        if(estado == 0){
            distancia = T * 0.8575; // a constante 0.8575 é obtido da formula 343/2 * 50us * 100. 50 us se refere á base temporal e 100 à metro convertido em cm
            num = (int) distancia; // Num sendo int facilita a conversao para string, no momento de fazer divisão inteira. Porém isso acaba diminuindo a precisão da estimativa da distância.
            /*A condicional abaixo é para o controle da frequência e período do led. Possuindo valores constantes para distâncias igual ou inferior à 10cm e para distâncias iguais ou superiores à 100cm. No caso em que a distância está entre 10cm e 100cm o meio-periódo é linearmente proporcional a distância neste intervalo. */
            if(distancia <=10){ 
                lim = 2000; // tomando este caso como exemplo temos um meio período de 2000*50us = 1ms. Então led fica 1ms apado e 1 ms aceso.
                }
            else{
                if(distancia < 100){
                    lim = 200 * (int) distancia;
                    }
                else{
                    lim = 20000;
                    }
                }
            estado = 1;
            
            }
        aciona_LED();
        }
    
    return 0;
    }
    
void configuracoes(void){
 
    cli();
 
    /*configuracoes do TIMER0*/
    OCR0A = 0x63; // TOP de 99, para termos uma base temporal de 50us
    TIMSK0 |= 0x01; // Setamos o TOIE para habilitar interrupção por overflow do TIMER/COUNTER0.
    TCCR0B |= 0x02; //prescaler 8 -> CS02 , CS01 e CS00 => 0, 1 e 0 respectivamente. 
    TCCR0A |= 0x02;// Modo CTC -> (WGM02), WGM01 e WGM00 => (0), 1 e 0 respectivamente.
    
    /*configuracoes do TIMER2 para PWM*/
    OCR2A = 0x63; // TOP = 100
    OCR2B = 0x3B;    // Pulso com largura inicial 60%
    TIMSK2 = 0x01; // Setamos o TOIE para habilitar interrupção por overflow do TIMER/COUNTER2.
    TCCR2B = 0x0B; // CS22, CS21 e CS20 configurados como 0 ,1 e 1 respectivamente para um prescaler de 32 
    TCCR2A = 0x23; // COM2B1 COM2B0 configurados como 1 e 0 para zerar OC2B na ocorrencia de Match e seta o mesmo no inicio do próximo ciclo (BOTTOM), 
                   //WGM02, WGM01 e  WGM00 configurados como 1 ,1 e 1 para trabalharmos no modo FAST PWM com TOP = OCR2A.  
    
    /*Configuracao da UART*/
    UBRR0H = 0x00;// para descrever 103 nao precisamos dos 8 bits mais significativo de UBRRn 
    UBRR0L = 0x67;//  temos baud rate de 9600  
	 
    UCSR0A &= 0x00; // U2X0 e MPCM0 iguais à zero para desabilitar double speed e modo multiprocessador 
    UCSR0B |= 0xB8; // Foi setado UDRIE0 pra interrupção de do tipo buffer vazio e TXEN0 para habi o trasmissor
    UCSR0C |= 0x06; //Nota usamos a configuração 0, 1 e 1 em UCSZ02, UCSZ01 e UCSZ00 para definir o numero de 8 bits de danos num frame.  VEr linha 451
    
    /*Configracoes GPIO*/
    DDRC |= 0x3F; // PC0 À PC5 habilitados como saídas.
    DDRD |= 0x08;// PD3 como saida habilitado;  
    DDRD &= 0xFB;// PD2 como entrada habilitado;
    PORTD &= 0x00;
    //PORTC &= 0x00;
    

   /*Configuracoes da rotina de servico de interrupcao PCINT2 */  
    _delay_ms(10);// delay para ignorar as interrupcoes causadas pelo transitorio.
    PCICR |= 0x04;// Habilita interrupcoes do grupo PCINT2
    PCMSK2 |= 0x04;// Habilita interrupcoes PCINT18
   
    sei();
    }

/*Função que liga ou desliga o led contectado ao terminal A5. */
void aciona_LED(void){
    
  if(estado_LED == 0){
        PORTC &= 0xDF;
        }
    else{
        PORTC |= 0x20;
        }
    }

/* Interrupcao de temporização */
/*A Base de tempo adotada para este temporizador foi de 50us. Começamos com a iteração dos contadores e seguimos com duas condicionais uma que verifica PULSO e outra que verifica o contador CONT. O que verifica CONT seta a saida PORTC0 a cada 4000*50us = 200ms e a variável PULSO, e na próxima chamada da interrupção a condicional que verifica a variável PULSO zera a saída PORTC0, totalizando num pulso de  aproximadamente 50us no TRIG. A terceira condicional que avalia a CONT_COMANDO está associado o envio de mensagem após 1 segundo depois da transmissão da última. E a condicional que verifica CONT_LED alterna o valor da variável ESTADO_LED à cada meio perído do led. */
ISR(TIMER0_OVF_vect){
 
  /* incrementa as variaveis de repetição */
    cont++;
    cont_comando++;
    cont_echo++;
    cont_LED++;
   
   
    if(pulso == 1){
        PORTC &= ~(0x01);
        pulso = 0;
        }   
    if (cont >= 4000){
        PORTC |= 0x01;
        cont = 0;
        pulso = 1;
        }
    
    if(cont_comando >= 20000){
        print_comando = 1;
         
        }

    if(cont_LED >= lim){
        cont_LED = 0;
        if(estado_LED == 0){
            estado_LED = 1;
            }
        else{
            estado_LED = 0;
            }
        }
    }

/* Rotina de Interrupção do tipo pin change.*/
/* Detectando uma mudança de borda na entrada de PD2 esta rotina é chamada. Se for borda de subida, o contador CONT_ECHO é zerado. Ao detectar uma borda de descida salvamos o valor na variável T, que será usada o calculo da distância no programa principal, então zeradmos a variável ESTADO para que permita calcular a distância. Relembrando que este pulso gerado no termial ECHO ocorre logo após outro pulso gerado no TRIG.*/
ISR(PCINT2_vect){
    
    if((PIND & 0x04) == 4){
        cont_echo = 0;
        }
    else{
        T = cont_echo;
        estado = 0;
        }
    }
    
    
 /* Esta Rotina do Temporizador 2 é responsável para gerar PWM associada à velocidade do motor. Dependendo da  variável SENTIDO o motor acelera(0) ou desacelera(1) até chegar na velocidade desejada. E não faz nada(-1) caso atinja o duty cycle desejado. A variável DUTY CYCLE armazena o valor para se obter 60, 80 e 100% do duty cycle */   
ISR(TIMER2_OVF_vect){
    
    switch(sentido){
        case 0: PORTD |= 0x04;
                   OCR2B++;
                   if(OCR2B >= duty_cycle){
                       sentido = -1;
                       }
                      break;
        case 1: PORTD &= 0xF7;
                   OCR2B--;
                   if(OCR2B <= duty_cycle){
                       sentido = -1;
                       }
                      break;
        case -1: ; 
        }
    }
    
   
/* Rotina de Serviço de Interrupção de Recepção Completa.*/
/* Ao recever alum comando pela UART, armazenamos na variável COMANDO. Se este for válido será armazenada em COMANDO_VALIDO, assim evitamos que comandos inválidos atrapalhem no funcionamento do sistema. Atribuimos em NOVO_COMANDO o valor 1, que permite a execução do comando recebido no programa principal. */
ISR(USART_RX_vect){

    comando = UDR0;
    
    if(comando == 'w'||comando == 's'||comando == 'a'||comando == 'd'||comando == 'q'||comando == 'e'||comando == '6'||comando == '8'||comando == '0'){
        comando_valido = comando;
        novo_comando = 1;
        }
    }   
 
 
/* Esta interrupção do tipo BUffer de Transmissão Vazio é resposável por transmitir cada mensagem serialmente via UART. Em cada caso do switch é enviado as strings correspondentes aos comandos recebidos caractere por caractere á cada chamda, e zerando o iterador i como também desabilitando esta interrupção ao enviar toda string, que será habilitada quando receber um novo comando ou depois de 1 segundo. A lógica é a mesma para dos os casos mas temos 2 variações para os casos w E e, sendo que o primeiro depende da distancia com o obstáculo e o segundo converte a distancia em string antes de enviar os caracteres. */
ISR(USART_UDRE_vect){
  
    switch(comando_valido){
        
        case 'w': if(distancia > 10){
                      if(comando_w[i]!= '\0'){
                          UDR0 = comando_w[i];
                          i++;
                          }
                      else{
                          i = 0;
                          UCSR0B &= 0xDF;
                          cont_comando = 0;
                          }
                      }
                  else{
                      if(comando_OBS[i]!= '\0'){
                          UDR0 = comando_OBS[i];
                          i++;
                          }
                      else{
                          i = 0;
                          UCSR0B &= 0xDF;
                          cont_comando = 0;
                          }
                      
                      }
                      break;
                      
        case 's': if(comando_s[i]!= '\0'){
                          UDR0 = comando_s[i];
                          i++;
                          }
                      else{
                          i = 0;
                          UCSR0B &= 0xDF;
                          cont_comando = 0;
                          }
                      break;
        case 'a': if(comando_a[i]!= '\0'){
                          UDR0 = comando_a[i];
                          i++;
                          }
                      else{
                          i = 0;
                          UCSR0B &= 0xDF;
                          cont_comando = 0;
                          }
                      break;
        case 'd': if(comando_d[i]!= '\0'){
                          UDR0 = comando_d[i];
                          i++;
                          }
                      else{
                          i = 0;
                          UCSR0B &= 0xDF;
                          cont_comando = 0;
                          }
                      break;
        case 'q': if(comando_q[i]!= '\0'){
                          UDR0 = comando_q[i];
                          i++;
                          }
                      else{
                          i = 0;
                          UCSR0B &= 0xDF;
                          cont_comando = 0;
                          }
                      break;
                      
        case 'e': if(comando_e[i]!= '\0'){
                          
                          comando_e[0] = num / 100 + '0';
                          comando_e[1] = ( num - (num / 100)* 100) / 10 + '0';
                          comando_e[2] =  num - (num / 100) * 100 - (( num - (num / 100)* 100) / 10) * 10 + '0';
                          UDR0 = comando_e[i];
                          i++;
                          }
                      else{
                          i = 0;
                          UCSR0B &= 0xDF;
                          cont_comando = 0;
                          }
                      break;
        case '6': if(comando_6[i]!= '\0'){
                          UDR0 = comando_6[i];
                          i++;
                          }
                      else{
                          i = 0;
                          UCSR0B &= 0xDF;
                          cont_comando = 0;
                          }
                      break;
        case '8': if(comando_8[i]!= '\0'){
                          UDR0 = comando_8[i];
                          i++;
                          }
                      else{
                          i = 0;
                          UCSR0B &= 0xDF;
                          cont_comando = 0;
                          }
                      break;
        case '0': if(comando_0[i]!= '\0'){
                          UDR0 = comando_0[i];
                          i++;
                          }
                      else{
                          i = 0;
                          UCSR0B &= 0xDF;
                          cont_comando = 0;
                          }
                      break;
        }
    }
    
/*Esta função simplesmente configura as saídas PORTC de acordo com o comando recebido.*/
void aciona_motor(void){

    switch(comando_motor){
        
        
        case 's': PORTC &= 0xF5;
                  PORTC |= 0x14;
                  break;
        case 'a': PORTC &= 0xF3;
                  PORTC |= 0x12;
                  break;
        case 'd': PORTC &= 0xED;
                  PORTC |= 0x0C;
                  break;
        
        case 'w': if(distancia > 10){
                      PORTC &= 0xEB;
                      PORTC |= 0x0A;
                      break;
                      }
        case 'q': PORTC &= 0xE1;
                  break;
                  
        default: break;
        
        }
    }
    
//UCSR0C
    //UMSEL01	UNSEL00	UMP01		UPM00		USBS0		UCSZ01		UCSZ00		UCP0L0
    //	 0	  0		  0		  0		  0	  	   1		   1		   0	    
    //	0 e 0 para 		Desabilita a paridade		para 1 bit 					 Assincrona	  
    //	assincrono						 de parada					 


