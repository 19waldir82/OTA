
// Exemplo de WebOTA 
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <Update.h>
#include <ESPmDNS.h>

//Parâmetros da rede WiFi
const char* ssid = "Thuliv";
const char* password = "90iojknm";

//Parâmetros de rede
IPAddress ip(192, 168, 1, 250);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
const uint32_t PORTA = 80; //A porta que será utilizada (padrão 80)

//Algumas informações que podem ser interessantes
const uint32_t chipID = (uint32_t)(ESP.getEfuseMac() >> 32); //um ID exclusivo do Chip...
const String CHIP_ID = "<p> Chip ID: " + String(chipID) + "</p>"; // montado para ser usado no HTML
const String VERSION = "<p> Versão: 1.0 </p>"; //Exemplo de um controle de versão

//Informações interessantes agrupadas
const String INFOS = VERSION + CHIP_ID;

//Sinalizador de autorização do OTA
boolean OTA_AUTORIZADO = false;

//inicia o servidor na porta selecionada
//aqui testamos na porta 3000, ao invés da 80 padrão
WebServer server(PORTA);

//Páginas HTML utilizadas no procedimento OTA
String index1 = "<!DOCTYPE html><html><head><title>Minha Casa</title><meta charset='UTF-8'></head><body><h1>Minha Casa</h1>"+ INFOS +"<form method='POST' action='/arquivo' enctype='multipart/form-data'><h2><p><label>Chave: </label><input type='text' name='autorizacao'> <input type='submit'value='Ok'></p></h2></form></body></html>";
String index2 = "<!DOCTYPE html><html><head><title>Minha Casa</title><meta charset='UTF-8'></head><body><h1>Minha Casa</h1>"+ INFOS +"<form method='POST'action='/update' enctype='multipart/form-data'><p><input type='file' name='update'></p><p><input type='submit' value='Atualizar'></p></form</body></html>";
String atualizado = "<!DOCTYPE html><html><head><title>Minha Casa</title><meta charset='UTF-8'></head><body><h1>Minha Casa</h1><h2>Atualização bem sucedida!</h2></body></html>";
String chaveIncorreta = "<!DOCTYPE html><html><head><title>Minha Casa</title><meta charset='UTF-8'></head><body><h1>Minha Casa</h1>"+ INFOS +"<h2>Chave incorreta</h2</body></html>";

//Setup
void setup(void)
{
  Serial.begin(115200); //Serial para debug

  WiFi.mode(WIFI_AP_STA); //Comfigura o ESP32 como ponto de acesso e estação

  WiFi.begin(ssid, password);// inicia a conexão com o WiFi

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  WiFi.config(ip, gateway, subnet);
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  MDNS.begin("talkinghome");
 

  if (WiFi.status() == WL_CONNECTED) //aguarda a conexão
  {
    //atende uma solicitação para a raiz
    // e devolve a página 'verifica'
    server.on("/", HTTP_GET, []() //atende uma solicitação para a raiz
    {
      server.sendHeader("Connection", "close");
      server.send(200, "text/html", index1);
    });

    //atende uma solicitação para a página avalia
    server.on("/arquivo", HTTP_POST, [] ()
    {
      Serial.println("Em server.on /avalia: args= " + String(server.arg("autorizacao"))); //somente para debug

      if (server.arg("autorizacao") != "90iojknm") // confere se o dado de autorização atende a avaliação
      {
        //se não atende, serve a página indicando uma falha
        server.sendHeader("Connection", "close");
        server.send(200, "text/html", chaveIncorreta);
        //ESP.restart();
      }
      else
      {
        //se atende, solicita a página de índice do servidor
        // e sinaliza que o OTA está autorizado
        OTA_AUTORIZADO = true;
        server.sendHeader("Connection", "close");
        server.send(200, "text/html", index2);
      }
    });

    //serve a página de indice do servidor
    //para seleção do arquivo
    server.on("/index2", HTTP_GET, []()
    {
      server.sendHeader("Connection", "close");
      server.send(200, "text/html", index2);
    });

    //tenta iniciar a atualização . . .
    server.on("/update", HTTP_POST, []()
    {
      //verifica se a autorização é false.
      //Se for falsa, serve a página de erro e cancela o processo.
      if (OTA_AUTORIZADO == false)
      {
        server.sendHeader("Connection", "close");
        server.send(200, "text/html", chaveIncorreta);
        return;
      }
      //Serve uma página final que depende do resultado da atualização
      server.sendHeader("Connection", "close");
      server.send(200, "text/html", (Update.hasError()) ? chaveIncorreta : atualizado);
      delay(1000);
      ESP.restart();
    }, []()
    {
      //Mas estiver autorizado, inicia a atualização
      HTTPUpload& upload = server.upload();
      if (upload.status == UPLOAD_FILE_START)
      {
        Serial.setDebugOutput(true);
        Serial.printf("Atualizando: %s\n", upload.filename.c_str());
        if (!Update.begin())
        {
          //se a atualização não iniciar, envia para serial mensagem de erro.
          Update.printError(Serial);
        }
      }
      else if (upload.status == UPLOAD_FILE_WRITE)
      {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
        {
          //se não conseguiu escrever o arquivo, envia erro para serial
          Update.printError(Serial);
        }
      }
      else if (upload.status == UPLOAD_FILE_END)
      {
        if (Update.end(true))
        {
          //se finalizou a atualização, envia mensagem para a serial informando
          Serial.printf("Atualização bem sucedida! %u\nReiniciando...\n", upload.totalSize);
        }
        else
        {
          //se não finalizou a atualização, envia o erro para a serial.
          Update.printError(Serial);
        }
        Serial.setDebugOutput(false);
      }
      else
      {
        //se não conseguiu identificar a falha no processo, envia uma mensagem para a serial
        Serial.printf("Atualização falhou inesperadamente! (possivelmente a conexão foi perdida.): status=%d\n", upload.status);
      }
    });

    server.begin(); //inicia o servidor
  }
}

void loop(void)
{
  //manipula clientes conectados
  server.handleClient();
}
