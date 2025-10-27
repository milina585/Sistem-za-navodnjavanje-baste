#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

String server = "https://bipfunctionns2024.azurewebsites.net/api/";
const char* api_key = "w2i38XCAg7tsHgqUcYKSpWy13STX6fAwwd7hHPvCxqG8AzFu_4n3cA==";

const char* ssid = "Wokwi-GUEST";
const char* password = "";

#define BUF_SIZE 200
char buffer[BUF_SIZE];
const char *payload =  
"{ \
  \"sender\": \"navodnjavanje\", \
  \"recipient\": \"server\", \
  \"messageText\": \"Temp: %.1f*C, Vlaznost: %.1f%%, Zemlja: %d\" \
}";

#define DHT_PIN 15
#define SOIL_PIN 34
#define PUMP_PIN 2
#define BUTTON_PIN 4

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET     -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 ekran(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

DHT dht(DHT_PIN, DHT22);

unsigned long poslednjeMerenje = 0;
unsigned long interval = 5000;

unsigned long poslednjeSlanje = 0;
const unsigned long intervalSlanja = 30000;

unsigned long poslednjiPritisak = 0;
const unsigned long debounceDelay = 200;

int trenutniRezim = 0;
int granicaVlaznosti = 1500;

float temperatura, vlaznostVazduha;
int vlaznostZemlje;
bool pumpaUkljucena;

bool alarmAktivan = false;            
unsigned long vremePocetkaRada = 0;   
const unsigned long maxRadnoVreme = 20000; 

void setup() {
  Serial.begin(115200);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  dht.begin();

  if (!ekran.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("Greška pri pokretanju ekrana!");
    while (true);
  }
  ekran.clearDisplay();
  ekran.setTextSize(1);
  ekran.setTextColor(SSD1306_WHITE);
  ekran.setCursor(0,0);
  ekran.println("Povezivanje na WiFi...");
  ekran.display();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  Serial.println("Povezan na WiFi!");
  ekran.clearDisplay();
  ekran.setCursor(0,0);
  ekran.println("Povezan na WiFi!");
  ekran.display();
  delay(1000);
}

void send_message(float temp, float vlaz, int zemlja) {
  HTTPClient http;
  String url = server + "Message";
  http.begin(url);
  http.addHeader("x-functions-key", api_key, true, true);

  snprintf(buffer, BUF_SIZE, payload, temp, vlaz, zemlja);
  Serial.println("Slanje podataka:");
  Serial.println(buffer);

  int httpCode = http.POST(buffer);
  if (httpCode > 0) {
    Serial.printf("HTTP kod: %d\n", httpCode);
    if (httpCode == HTTP_CODE_OK) {
      String resp = http.getString();
      Serial.println("Odgovor servera:");
      Serial.println(resp);
    }
  } else {
    Serial.printf("Greška pri POST: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
}

void prikaziNaEkranu() {
  ekran.clearDisplay();
  ekran.setCursor(0,0);
  ekran.setTextSize(1);
  ekran.setTextColor(SSD1306_WHITE);

  if (alarmAktivan) {
    ekran.setTextSize(2);
    ekran.setCursor(30, 25);
    ekran.println("ALARM!");
    ekran.display();
    return; 
  }

  switch (trenutniRezim) {
    case 0:
      ekran.println("Senzori:");
      ekran.printf("Temp: %.1f C\n", temperatura);
      ekran.printf("VlaznVz: %.1f %%\n", vlaznostVazduha);
      ekran.printf("VlazZemlje: %d\n", vlaznostZemlje);
      break;
    case 1:
      ekran.println("Status sistema:");
      ekran.printf("Pumpa: %s\n", pumpaUkljucena ? "UKLJUCENA" : "ISKLJUCENA");
      ekran.printf("Interval: %lus\n", interval/1000);
      ekran.printf("Granica: %d\n", granicaVlaznosti);
      break;
    case 2:
      ekran.println("Komande:");
      ekran.println(" set interval [s]");
      ekran.println(" pump on/off");
      ekran.println(" set granica [0-4095]");
      break;
  }

  ekran.display();
}


void obradiKomandu(String cmd) {
  cmd.trim();
  if (cmd.startsWith("set interval ")) {
    int s = cmd.substring(13).toInt();
    if (s > 0) { interval = s * 1000; 
    Serial.printf("Interval: %ds\n", s); }
  } else if (cmd == "pump on") {
    pumpaUkljucena = true; 
    Serial.println("Pumpa ukljucena manuelno.");
   } else if (cmd == "pump off") {
    pumpaUkljucena = false; 
    Serial.println("Pumpa iskljucena manuelno.");
    vremePocetkaRada = 0;     
    if (alarmAktivan) {       
      alarmAktivan = false;
    }
    prikaziNaEkranu();  
  } else if (cmd.startsWith("set granica ")) {
    int v = cmd.substring(12).toInt();
    if (v >= 0 && v <= 4095) { granicaVlaznosti = v; 
    Serial.printf("Granica: %d\n", v); }
  } else Serial.println("Nepoznata komanda.");
}

void loop() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    unsigned long sada = millis();
    if (sada - poslednjiPritisak > debounceDelay) {
      poslednjiPritisak = sada;
      trenutniRezim = (trenutniRezim + 1) % 3;
      prikaziNaEkranu();
    }
  }

  if (millis() - poslednjeMerenje > interval) {
    poslednjeMerenje = millis();

    temperatura = dht.readTemperature();
    vlaznostVazduha = dht.readHumidity();
    vlaznostZemlje = analogRead(SOIL_PIN);

    pumpaUkljucena = (vlaznostZemlje < granicaVlaznosti);

  if (pumpaUkljucena && vremePocetkaRada == 0) {
  vremePocetkaRada = millis();  
}
else if (!pumpaUkljucena && vremePocetkaRada != 0) {
  vremePocetkaRada = 0;         
  if (alarmAktivan) {           
    alarmAktivan = false;
    prikaziNaEkranu(); 
  }
}

if (vremePocetkaRada != 0 && (millis() - vremePocetkaRada > maxRadnoVreme)) {
  if (!alarmAktivan) {
    alarmAktivan = true;
    prikaziNaEkranu();
  }
}

    Serial.printf("Merenje: T=%.1fC, Vz=%.1f%%, Z=%d, Pumpa=%s\n",
                  temperatura, vlaznostVazduha, vlaznostZemlje,
                  pumpaUkljucena ? "UKLJUCENA" : "ISKLJUCENA");
  }

  if (millis() - poslednjeSlanje > intervalSlanja) {
    poslednjeSlanje = millis();
    send_message(temperatura, vlaznostVazduha, vlaznostZemlje);
  }

  if (Serial.available()) {
    String kom = Serial.readStringUntil('\n');
    obradiKomandu(kom);
  }

  digitalWrite(PUMP_PIN, pumpaUkljucena ? HIGH : LOW);
}
