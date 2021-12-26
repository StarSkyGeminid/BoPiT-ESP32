#include <Arduino.h>
#include <stdio.h>
#include <ArduinoJson.h>
#include <FirebaseESP32.h>
#include "addons/RTDBHelper.h"
#include "time.h"
#include <ReadDHT.h>
#include <SoilMosture.h>
#include <ReadDallas.h>

#define NTP_SERVER "pool.ntp.org"

class FirebaseController
{
    FirebaseData fbdo;
    ReadDHT dht;
    SoilMosture lembab;
    ReadDallas dallas;
    time_t now;
    struct tm *timeinfo;

    const char *ntpServer = NTP_SERVER;
    const long gmtOffset_sec = 25200;
    const int daylightOffset_sec = 0;

    void showError()
    {
        Serial.println(fbdo.errorReason());
    }

    unsigned long getHour()
    {
        time(&now);
        timeinfo = localtime(&now);
        return timeinfo->tm_hour;
    }

public:
    FirebaseController()
    {
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    }

    unsigned int getKelembabanTanah()
    {
        if (Firebase.getInt(fbdo, F("/sensorData/kelembabanTanah")))
        {
            if (fbdo.dataTypeEnum() == fb_esp_rtdb_data_type_integer)
            {
                return fbdo.to<int>();
            }
        }
        else
        {
            Serial.println(fbdo.errorReason());
        }
        return 0;
    }

    unsigned long getMinute()
    {
        time(&now);
        timeinfo = localtime(&now);
        return timeinfo->tm_min;
    }

    void setSensorData()
    {
        FirebaseJson json;
        json.add("kelembabanUdara", dht.KelembabanUdara());
        json.add("suhuUdara", dht.SuhuUdara());
        json.add("kelembabanTanah", lembab.getKelembaban());
        json.add("suhuTanah", dallas.getSuhuTanah());
        Firebase.updateNode(fbdo, F("/sensorData"), json);
    }

    void sendNotification()
    {
        Firebase.getString(fbdo, "/Token");
        fbdo.fcm.begin("AAAAJn7rcU0:APA91bECX7dFufaRYc9LzLbSXmIQCFpqmHiJnhg9_h-Mk2W03gaSWPeUmZTxw5235Caqcchm36tyeblsDSFLHyZEfmrA0w9gjTrWtLG6mdprG7tUw8tOUn64ak4W2Emk_ZXhKYx3EYRH");
        fbdo.fcm.setPriority("high");
        fbdo.fcm.setTimeToLive(5000);
        fbdo.fcm.setNotifyMessage("Informasi", "Penyiraman telah berhasil dilakukan!");
        fbdo.fcm.setDataMessage("{\"debit\":89}");
        fbdo.fcm.addDeviceToken(fbdo.to<String>());
        Firebase.sendMessage(fbdo, 0);
        fbdo.fcm.clearDeviceToken();
    }

    bool isServiceOn()
    {
        if (Firebase.getBool(fbdo, F("/sensorData/status")))
        {
            return fbdo.to<bool>();
        }
        else
        {
            return false;
        }
    }

    bool isScheduleRun(bool running)
    {
        char buff[15];
        bool enable;
        int minute;
        String path = "/jadwal/" + String(getHour());
        path.toCharArray(buff, path.length() + 1);
        DynamicJsonDocument doc(2048);
        Firebase.get(fbdo, buff);
        if (fbdo.jsonString() == NULL)
        {
            return false;
        }
        else
        {
            deserializeJson(doc, fbdo.jsonString());
            enable = doc["enable"];
            minute = doc["minute"];
            if (enable && minute == getMinute() && !running)
            {
                return true;
            }
            else
            {
                return false;
            }
        }
    }

    bool isAutomasiRun(bool running)
    {
        DynamicJsonDocument doc(2048);
        Firebase.get(fbdo, F("/Automasi"));
        deserializeJson(doc, fbdo.jsonString());
        if (doc.containsKey("Kelembaban") && !running)
        {
            JsonObject kelembaban = doc["Kelembaban"];
            bool isEnable = kelembaban["enable"];
            const char *getOprs = kelembaban["operator"];
            int value = kelembaban["value"];
            if (*getOprs == '<' && isEnable)
            {
                return lembab.getKelembaban() < value;
            }
            else if (*getOprs == '=' && isEnable)
            {
                return lembab.getKelembaban() == value;
            }
            else if (*getOprs == '>' && isEnable)
            {
                return lembab.getKelembaban() > value;
            }
            else
            {
                return false;
            }
        }
        else if (doc.containsKey("Suhu") && !running)
        {
            JsonObject Suhu = doc["Suhu"];
            bool isEnable = Suhu["enable"];
            const char *getOprs = Suhu["operator"];
            int value = Suhu["value"];
            if (*getOprs == '<' && isEnable)
            {
                return dallas.getSuhuTanah() < value;
            }
            else if (*getOprs == '=' && isEnable)
            {
                return dallas.getSuhuTanah() == value;
            }
            else if (*getOprs == '>' && isEnable)
            {
                return dallas.getSuhuTanah() > value;
            }
            else
            {
                return false;
            }
        }
        else
        {
            Serial.println("GADA BOSS");
            return false;
        }
    }
};