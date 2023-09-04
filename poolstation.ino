#include <WiFi.h>
#include <WiFiMulti.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <HTTPClient.h>
#include <stdio.h>

// DS18B20 pin
const int oneWireBus = 4;

OneWire oneWire(oneWireBus);

DallasTemperature sensors(&oneWire);

WiFiMulti wifiMulti;

HTTPClient http;

struct SensorInfo
{
  const DeviceAddress Address;
  const char* Name;
  int Index;
};

template<typename T, size_t Len>
size_t ArrLen(T (&arr)[Len])
{
  return Len;
}

SensorInfo sensorsInfo[] =
{
  { { 0x28, 0x1F, 0x05, 0x81, 0xE3, 0xE1, 0x3C, 0x6A }, "PoolIn", -1 },
  { { 0x28, 0xC5, 0x52, 0x81, 0xE3, 0xE1, 0x3C, 0xA5 }, "AdHoc", -1 }
};

void printAddress(DeviceAddress deviceAddress)
{ 
  for (uint8_t i = 0; i < 8; i++)
  {
    Serial.print("0x");
    if (deviceAddress[i] < 0x10) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
    if (i < 7) Serial.print(", ");
  }
  Serial.println("");
}

void setup()
{
  Serial.begin(115200);

  //wifiMulti.addAP("Pretty fly for a 2.4GHz", "XYZ");
  wifiMulti.addAP("IOLA", "XYZ");

  sensors.begin();

  Serial.println("Locating devices...");
  Serial.print("Found ");
  int deviceCount = sensors.getDeviceCount();
  Serial.print(deviceCount, DEC);
  Serial.println(" devices.");

  Serial.println("Printing addresses...");
  for (int i = 0; i < deviceCount; i++)
  {
    Serial.print("Sensor ");
    Serial.print(i);
    Serial.print(" : ");
    DeviceAddress address;
    sensors.getAddress(address, i);
    printAddress(address);

    for (SensorInfo& si : sensorsInfo)
    {
      if (memcmp(si.Address, address, ArrLen(si.Address)) == 0)
      {
        si.Index = i;
        Serial.print("Found sensor ");
        Serial.print(si.Name);
        Serial.print(" at index ");
        Serial.println(si.Index, DEC);
        break;
      }
    }
  }

  http.setReuse(true);
}

unsigned long nextPost;

const unsigned long PostInterval = 60000;

void tick()
{
  unsigned long curMillis = millis();
  if (wifiMulti.run() != WL_CONNECTED)
  {
    Serial.println("Not connected");
    return;
  }

  if (curMillis < nextPost)
  {
    return;
  }

  sensors.requestTemperatures();
  float temperatures[8];
  int deviceCount = sensors.getDeviceCount();
  for (int i = 0; i < deviceCount; i++)
  {
    temperatures[i] = sensors.getTempCByIndex(i);
  }

  Serial.println("Sending data");

  for (const SensorInfo& si : sensorsInfo)
  {
    if (si.Index == -1)
    {
      continue;
    }

    float temp = temperatures[si.Index];
    Serial.print("Sensor ");
    Serial.print(si.Name);
    Serial.print(" : ");
    Serial.print(temp);
    Serial.println("ºC");

    char json[128];
    snprintf(json, sizeof(json), "{\"location\":\"%s\",\"temperature\":%f}", si.Name, temp);

    Serial.print("Sending ");
    Serial.println(json);

    for (int i = 0; i < 10; i++)
    {
      http.begin("http://beta.nextbeer.dk/set");
      int httpResponseCode = http.POST(json);

      bool success = httpResponseCode > 0;

      if (success)
      {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();
        Serial.println(payload);
      }
      else
      {
        Serial.printf("Error: %s\n", http.errorToString(httpResponseCode).c_str());
        Serial.printf("Try %d\n...", i + 2);
      }

      http.end();

      if (success)
      {
        break;
      }
    }
  }

  nextPost = (nextPost == 0 ? curMillis : nextPost) + PostInterval;
}

void loop()
{
  tick();
  delay(1000);
}
