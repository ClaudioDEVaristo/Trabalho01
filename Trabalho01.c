#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include <string.h>
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/pwm.h"
#include "inc/ssd1306.h"
#include "inc/font.h"
#include "Trabalho01.pio.h"
#include "inc/config_matriz.h"

/*
============================= DEFINIÇÕES DE PINOS ========================================
*/

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C

#define JOYSTICK_X_PIN 26  // GPIO para eixo X
#define JOYSTICK_Y_PIN 27  // GPIO para eixo Y
#define JOYSTICK_PB 22 // GPIO para botão do Joystick
#define Botao_A 5 // GPIO para botão A
#define vermelho 13 //GPIO para a cor vermelha do RGB
#define verde 11 //GPIO para a cor verde do RGB
#define azul 12 //GPIO para a cor azul do RGB
#define buzz 10 //GPIO para o buzzer

#define SCREEN_WIDTH 128 //Largura do display
#define SCREEN_HEIGHT 64 //Altura do display
#define SQUARE_SIZE 8 //Tamanho do quadrado 8x8
#define TAM_SENHA 7 // Tamanho da senha

/*
============================= VARIÁVEIS GLOBAIS ========================================
*/
const char senha_correta[] = "7355608"; //Senha para ativação da bomba

PIO pio_config(); //Configs PIO
void define_numero(char numero, PIO pio, uint sm); //Função para os sprites

ssd1306_t ssd;
uint static volatile ultimoTime = 0;
volatile bool iniciarJogo = false; 
bool estadoBotao;
char c;

const uint16_t period = 20000; //  20ms = 20000 ticks
const float DIVIDER_PWM = 125.0; //Divisor de clock
uint16_t pwm_level = 500;
uint slice_num;

int x = (SCREEN_HEIGHT - SQUARE_SIZE) / 2; //Centralizar em X
int y = (SCREEN_WIDTH - SQUARE_SIZE) / 2; //Centralizar em Y

/*
============================= FUNÇÕES ========================================
*/

int map_joystick_to_velocity(int raw) { //Função para mapear o joystick e retornar uma velocidade
    const int center = 2048;
    int delta = raw - center;

    // Margem morta pra ignorar pequenos ruídos
    if (abs(delta) < 100) return 0;

    int speed = delta / 512;

    // Garante que velocidade nunca passe de -3 a +3
    if (speed > 3) speed = 3;
    if (speed < -3) speed = -3;

    return speed;
}

void setup_pwm(){ //Inicialização do PWM
  gpio_set_function(buzz, GPIO_FUNC_PWM);
  slice_num = pwm_gpio_to_slice_num(buzz);

  pwm_set_clkdiv(slice_num, 125.0); // 125 MHz / 125 = 1 MHz
  pwm_set_wrap(slice_num, 250);     // 1 MHz / 250 = 4 kHz (frequência audível)
  pwm_set_chan_level(slice_num, pwm_gpio_to_channel(buzz), 0); //Inicia o BUZZ desligado
  pwm_set_enabled(slice_num, true);// Inicia o PWM
}

void buzzer_explosao() { //Função para simular o barulho de explosão
  uint slice = pwm_gpio_to_slice_num(buzz);
  uint chan = pwm_gpio_to_channel(buzz);

  pwm_set_clkdiv(slice, 125.0); // Garante clock de 1 MHz

  // 3 bips curtos
  for (int i = 0; i < 3; i++) {
      pwm_set_wrap(slice, 500); // 2 kHz
      pwm_set_chan_level(slice, chan, 250); // 50%
      sleep_ms(200);
      pwm_set_chan_level(slice, chan, 0);
      sleep_ms(100);
  }

  // Pausa dramática
  sleep_ms(400);

  // Explosão final: som agudo contínuo
  pwm_set_wrap(slice, 200); // 5 kHz
  pwm_set_chan_level(slice, chan, 100); // 50%
  sleep_ms(1000);
  pwm_set_chan_level(slice, chan, 0);
}


//rotina da interrupção
void callback_button(uint botao, uint32_t events)
{
  uint timeAtual = to_us_since_boot(get_absolute_time()); // Tempo em milissegundos 
  //250 ms para evitar ação dupla do botão
  if(timeAtual - ultimoTime > 250000){
    ultimoTime = timeAtual;

   estadoBotao = gpio_get(botao);

    if(!estadoBotao){
      if(botao == Botao_A){
        printf("O botao A foi precionado!!\n");
        iniciarJogo = true;

      }
    }
  }
}

// Pisca a cor ligada ao pino `pin` durante `seconds` segundos, num ciclo
// de 500 ms ligado / 500 ms desligado.
void blink_color(uint pin, int seconds) {
    int cycles = seconds;  // 1 ciclo = 1 s (500 ms on + 500 ms off)
    for (int i = 0; i < cycles; i++) {
        gpio_put(pin, 1);
        pwm_set_chan_level(slice_num, pwm_gpio_to_channel(buzz), 250); //Aciona o buzz com Duty-C de 50%
        sleep_ms(500);

        gpio_put(pin, 0);
        pwm_set_chan_level(slice_num, pwm_gpio_to_channel(buzz), 0); //Desliga o buzz
        sleep_ms(500);
    }
}

void bombaPlantada(){ //Função para a lógica da bomba plantada

    PIO pio = pio_config();

    char senha_digitada[TAM_SENHA + 1] = {0}; // +1 pro '\0'
    int i = 0;

    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);
    ssd1306_draw_string(&ssd, "SENHA: *******", 8, 30);
    ssd1306_send_data(&ssd); 

    while (i < TAM_SENHA) {
    scanf("%c", &c); 

    if (c >= '0' && c <= '9') { // Só aceita números
        senha_digitada[i] = c;
        i++;

        // Atualiza tela com números digitados
        ssd1306_fill(&ssd, false);
        ssd1306_draw_string(&ssd, "SENHA: ", 8, 30);
        ssd1306_draw_string(&ssd, senha_digitada, 60, 30);
        ssd1306_send_data(&ssd);
    }
}
senha_digitada[TAM_SENHA] = '\0';

// Verifica se senha está correta
if (strcmp(senha_digitada, senha_correta) == 0) {
    ssd1306_draw_string(&ssd, "ACERTOU!", 30, 50);
    ssd1306_send_data(&ssd);
    gpio_put(verde, 1);
    // 1) Pisca verde por 15 s
    blink_color(verde, 15);

    // 2) Pisca azul por 10 s
    blink_color(azul, 10);

    // 3) Pisca vermelho por 5 s
    blink_color(vermelho, 5);

    buzzer_explosao();//Chama a função para o barulho da explosão

    for(int i; i < 6; i++){
      define_numero(i, pio, 0);//Chama a função para o desenho das explosões
      sleep_ms(250);
    }
} else {
    ssd1306_draw_string(&ssd, "ERROU!", 30, 50);
    gpio_put(vermelho, 1);

    buzzer_explosao();//Chama a função para o barulho da explosão

    for(int i = 0; i < 6; i++){
      define_numero(i, pio, 0);//Chama a função para o desenho das explosões
      sleep_ms(250);
    }
}
ssd1306_send_data(&ssd);//Atualiza o display

sleep_ms(3000); 

    // Espera para mostrar resultado
    // Garante que comece tudo desligado
    gpio_put(vermelho,   0);
    gpio_put(verde, 0);
    gpio_put(azul,  0);
}

/*
============================= MAIN E LOOP ========================================
*/

int main()
{
  //Chama as funções de callback
  gpio_set_irq_enabled_with_callback(Botao_A, GPIO_IRQ_EDGE_FALL, true, &callback_button);

  stdio_init_all();
  setup_pwm();

  //inicialização dos pinos
  gpio_init(Botao_A);
  gpio_set_dir(Botao_A, GPIO_IN);
  gpio_pull_up(Botao_A);

  gpio_init(vermelho);
  gpio_set_dir(vermelho, GPIO_OUT);
  gpio_init(verde);
  gpio_set_dir(verde, GPIO_OUT);
  gpio_init(azul);
  gpio_set_dir(azul, GPIO_OUT);

  // I2C Initialisation. Using it at 400Khz.
  i2c_init(I2C_PORT, 400 * 1000);

  gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); // Seta o pino GPIO para a função I2C
  gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // Seta o pino GPIO para a função I2C
  gpio_pull_up(I2C_SDA); // Pull up o data line
  gpio_pull_up(I2C_SCL); // Pull up o clock line
  ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
  ssd1306_config(&ssd); // Configura o display
  ssd1306_send_data(&ssd); // Envia os dados para o display

  // Limpa o display. O display inicia com todos os pixels apagados.
  ssd1306_fill(&ssd, false);
  ssd1306_send_data(&ssd);

  adc_init();//inicia o ADC
  adc_gpio_init(JOYSTICK_X_PIN);//Inicia o ADC Para o Joystick no eixo X
  adc_gpio_init(JOYSTICK_Y_PIN);//Inicia o ADC Para o Joystick no eixo Y
  
  uint16_t adc_value_x;
  uint16_t adc_value_y;  
  char str_x[5];  // Buffer para armazenar a string
  char str_y[5];  // Buffer para armazenar a string  
  
  bool cor = true;

  //lógica para continuar na tela arm até que a interrupção seja ativada 
    while(iniciarJogo == false){
    ssd1306_draw_string(&ssd, "Pressione ARM", 8, 30);
    ssd1306_send_data(&ssd);
        if(iniciarJogo == true){
            bombaPlantada();
        }
    }

  while (true)
  { 
    adc_select_input(0); // Seleciona o ADC para eixo X. O pino 26 como entrada analógica
    adc_value_x = adc_read();
    adc_select_input(1); // Seleciona o ADC para eixo Y. O pino 27 como entrada analógica
    adc_value_y = adc_read();    
    
    // Mapeia para velocidades
    int vx = map_joystick_to_velocity(adc_value_x);
    int vy = map_joystick_to_velocity(adc_value_y);

    // Atualiza posições
    x -= vx;
    y += vy;
    
    // Limites da tela
    if (x < 0) x = 0;
    if (x > SCREEN_HEIGHT - SQUARE_SIZE) x = SCREEN_HEIGHT - SQUARE_SIZE;
    
    if (y < 0) y = 0;
    if (y > SCREEN_WIDTH - SQUARE_SIZE) y = SCREEN_WIDTH - SQUARE_SIZE;

    // Atualiza o conteúdo do display com animações
    ssd1306_fill(&ssd, !cor); // Limpa o displa

    ssd1306_rect(&ssd, x, y, SQUARE_SIZE, SQUARE_SIZE, cor, 1);
    ssd1306_send_data(&ssd); // Atualiza o display

    sleep_ms(100);
    } 
  }
