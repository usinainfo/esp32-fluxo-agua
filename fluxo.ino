/************************************************************
--------------  Autor: Gustavo R Stroschon  -----------------
-----------------   Data: 05/03/2020  -----------------------
----------------- Função do programa: -----------------------
-Automatizar o monitoramento de consumo de agua pelo celular-
*************************************************************/

#include <Wire.h>
#include "RTClib.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>

void salvar_dados();
float MiliLitros = 0;

// Defina o nome da rede e a senha a qual voce deseja gerar
#define ssid "Sensor_de_Fluxo"
#define password  "12345678"

WiFiServer server(80);

RTC_DS1307 rtc;

#define portaVazao GPIO_NUM_25

volatile int pulsos_vazao = 0;
float vazao = 0;

unsigned long ultimo_valor_salvo = 0;

float vazao_somando;

void IRAM_ATTR Interrupcao(void* arg) { // funçao chamada cada vez que o sensor de fluxo mandar um pulso
  pulsos_vazao++;                   //soma a variavel de contagem de pulsos do sensor de fluxo de agua
  portYIELD_FROM_ISR();
}

void Configurar_interrupcao(gpio_num_t Port) {
  pinMode(portaVazao, INPUT_PULLUP);            //configura pino como entrada
  gpio_set_intr_type(Port, GPIO_INTR_NEGEDGE);  //tipo de interrupçao
  gpio_intr_enable(Port);                       //ativa a porta
  gpio_install_isr_service(0);                  //instala a interrupçao
  gpio_isr_handler_add(Port, Interrupcao, (void*) Port); // oque fazer ao detectar a interupçao
}

void setup() {

  Wire1.begin(); //inicia protocolo de comunicaçao
  rtc.begin();//inicia a comunicaçao com o rtc

  Serial.begin(115200);

  if (!SD.begin()) { // caso o cartao nao tenha iniciado
    Serial.println("Erro ao iniciar a comunicacao com o cartao SD...");
      ESP.restart();
  }

  DateTime now = rtc.now();  //cria um objeto com as informaçoes de data e hora

  Configurar_interrupcao((gpio_num_t) portaVazao); //chama a funçao que ira configurar a interrupçao

  Serial.println("Configurando o ponto de acesso wifi...");

  // caso voce queira retirar o parametro de password sua rede ficara livre(sem senha para se conectar nela)
  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("O ip que voce deve digitar para acessar as informaçoes é : ");
  Serial.println(myIP);
  server.begin();

  Serial.println();
  Serial.println("Servidor iniciado com sucesso!!!");
}

void loop() {
  salvar_dados();   //faz  verificaçao e salvamento dos dados

  WiFiClient client = server.available();   // armazena as informacoes do cliente a qual se conectou a rede

  if (client) {                             // se alguem se conectar a rede
    Serial.println("Novo cliente conectado as informacoes sobre o fluxo da agua.");
    String currentLine = "";                // variavel com algumas informaçoes adicionais sobre o cliente
    while (client.connected()) {            // enquando alguem estiver conectado...

      if (client.available()) {             // e se ele carregar o ip
        char c = client.read();
        if (c == '\n') {                    // quando carregar a pagina mostre....
          if (currentLine.length() == 0) {
            salvar_dados();
            
            // mande as informacoes abaixo para o navegador do cliente
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();

            client.print("<p>O valor atual do consumo do dia em litros:");
            client.print(vazao_somando);
            client.print("</p><br>");

            client.print("<p> Os valores diarios estao abaixo: <br>");

            Serial.printf("Lendo o arquivo: %s\n", "/Fluxo_de_agua.txt");

            File file = SD.open("/Fluxo_de_agua_dia.txt"); // abre o arquivo para a leitura
            if (!file) { //caso o arquivo nao tenha sido iniciado ou aberto
              Serial.println("Erro ao abrir arquivo para a leitura");
            }

            while (file.available()) { //se tiver dados pra ler...
              client.write(file.read()); //mostre ao navegador do cliente
            }

            file.close(); // fecha o arquivo

            client.print("</p><br>");
            client.println();

            break;                          // apos isso as informacoes ja foram enviadas , sai do laco while
          } else {                          // se a pagina for recarregada
            currentLine = "";
          }
        } else if (c != '\r') {
            currentLine = "";
        }
      }
    }
    // fecha a conexao
    client.stop();
    Serial.println("Cliente desconectou.");
  }
}

void salvar_dados() {
  if ((millis() - ultimo_valor_salvo) > 1000) { // caso ja tenha passado 1 segundo do ultimo dado aferido e salvo
    ultimo_valor_salvo = millis();

    DateTime now = rtc.now(); //guarda as informaçoes de data e tempo no objeto now

    //converte a quantia de pulsos que o sensor mandou para a vazao da agua em litros por minuto
    vazao = pulsos_vazao / 5.5;
    pulsos_vazao = 0;
    MiliLitros = vazao / 60;
    vazao_somando = vazao_somando + MiliLitros; // calcula a vazao total do dia
    if(now.hour() == 23 && now.minute() == 59 && now.second() == 59){ // caso ja seja 23:59:59 vamos salvar os dados no cartao sd
       Serial.println(" ---------------------------------- ");
       Serial.println(vazao_somando);
       Serial.println(" ---------------------------------- ");

       String dataMessage = String(now.day()) + "/" + String(now.month()) + "/" + String(now.year()) + " ---  " + String(vazao_somando) + " L/dia <br> \n "; // cria a string que sera salva no cartao sd
       Serial.println(dataMessage);
       EditarArquivo(SD, "/Fluxo_de_agua_dia.txt", dataMessage.c_str());

      vazao_somando = 0; // reinicia a contagem de vazao de agua diarios
    }

    // mostra o valor da leitura do sensor
    Serial.print(" Sensor de Vazao esta registrando "); Serial.print(MiliLitros); Serial.println(" litros/Segundo");

  }
}

void EditarArquivo(fs::FS &fs, const char * local, const char * mensagem) {
  Serial.printf("editando o arquivo: %s\n", local);

  File file = fs.open(local, FILE_APPEND);
  if (!file) {
    Serial.println("Falha ao abrir o arquivo para editar");
    return;
  }
  file.print(mensagem);
  file.close();
}
