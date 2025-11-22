# Per-Display Data Refactoring

## 🎯 Tavoite

Jokainen display voi nyt saada **omat deg ja knot muuttujat**, jotta niihin voidaan ajaa **eri NMEA-lauseita**.

---

## 🔄 Mitä Muuttui

### Ennen (Vanha Rakenne)

```cpp
// Globaalit muuttujat - KAIKKI displayt käyttivät samoja
float sumlog_speed_kn = 0.0;
int angleDeg = 0;

// Parseri päivitti globaaleja
angleDeg = newAngle;
sumlog_speed_kn = newSpeed;

// Kaikki displayt saivat saman datan
updateDisplayPulse(0); // Käyttää sumlog_speed_kn
updateDisplayPulse(1); // Käyttää samaa sumlog_speed_kn
updateDisplayPulse(2); // Käyttää samaa sumlog_speed_kn
```

**Ongelma**: Jos Display 0 halusi MWV-dataa ja Display 1 halusi VWR-dataa, molemmat saivat saman datan (viimeisimmän parsitun).

---

### Jälkeen (Uusi Rakenne)

```cpp
// DisplayConfig-rakenne sisältää nyt per-display datan
struct DisplayConfig {
  // ... vanhat kentät ...
  
  // UUDET: Per-display wind data
  float windSpeed_kn;    // Tämän displayn nopeus
  int windAngle_deg;     // Tämän displayn kulma
  uint32_t lastUpdate_ms; // Milloin päivitetty viimeksi
};

// Parseri päivittää vain ne displayt jotka haluavat tämän sentence-tyypin
updateDisplaysForSentence("MWV", angle, speed, hasSpeed);
// -> Päivittää vain displayt joilla sentence == "MWV"

// Jokainen display käyttää omaa dataansa
updateDisplayPulse(0); // Käyttää displays[0].windSpeed_kn
updateDisplayPulse(1); // Käyttää displays[1].windSpeed_kn
updateDisplayPulse(2); // Käyttää displays[2].windSpeed_kn
```

**Hyöty**: Jokainen display voi seurata eri NMEA-lausetta!

---

## 📊 Uusi Data Flow

### Esimerkki: 3 Displaytä, Eri Lauseet

```
Display 0: sentence="MWV", type="sumlog"
Display 1: sentence="VWR", type="logicwind"
Display 2: sentence="VWT", type="sumlog"

NMEA Data:
$WIMWV,45.0,R,10.5,N,A*XX  -> MWV
$IIVWR,30.0,L,8.2,N*XX     -> VWR
$IIVWT,60.0,R,12.3,N*XX    -> VWT

Tulos:
Display 0: angle=45°, speed=10.5kn (MWV)
Display 1: angle=330°, speed=8.2kn (VWR, L=vasen)
Display 2: angle=60°, speed=12.3kn (VWT)
```

Jokainen display saa **oman datansa** oikeasta NMEA-lauseesta!

---

## 🔧 Tekniset Muutokset

### 1. DisplayConfig-rakenne Laajennettu

**Tiedosto**: `src/web_ui.h`

```cpp
struct DisplayConfig {
  // Vanhat kentät (säilytetty)
  bool enabled;
  char type[16];
  char sentence[8];
  int offsetDeg;
  float sumlogK;
  int sumlogFmax;
  int pulseDuty;
  int pulsePin;
  int gotoAngle;
  
  // UUDET kentät
  float windSpeed_kn;    // Per-display nopeus
  int windAngle_deg;     // Per-display kulma
  uint32_t lastUpdate_ms; // Viimeinen päivitys
};
```

---

### 2. Uusi Funktio: updateDisplaysForSentence()

**Tiedosto**: `src/wind_project.ino`

```cpp
void updateDisplaysForSentence(const char* sentenceType, 
                               int angle, 
                               float speed, 
                               bool hasSpeed) {
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  
  uint32_t now = millis();
  for (int i = 0; i < 3; i++) {
    if (!displays[i].enabled) continue;
    
    // Päivitä vain jos display haluaa tämän sentence-tyypin
    if (strcmp(displays[i].sentence, sentenceType) == 0) {
      displays[i].windAngle_deg = angle;
      if (hasSpeed) {
        displays[i].windSpeed_kn = speed;
      }
      displays[i].lastUpdate_ms = now;
    }
  }
  
  // Päivitä myös globaalit (backward compatibility)
  angleDeg = angle;
  if (hasSpeed) {
    sumlog_speed_kn = speed;
  }
  
  xSemaphoreGive(dataMutex);
  
  // Päivitä pulssi-outputit
  updateAllDisplayPulses();
}
```

**Toiminta**:
1. Käy läpi kaikki displayt
2. Tarkistaa onko `displays[i].sentence == sentenceType`
3. Jos on, päivittää sen displayn datan
4. Päivittää myös globaalit muuttujat (yhteensopivuus)
5. Kutsuu `updateAllDisplayPulses()` päivittämään pulssi-outputit

---

### 3. Parserit Yksinkertaistettu

**Ennen**:
```cpp
bool parseMWV(char* line) {
  // ... parsinta ...
  
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  angleDeg = newAngle;
  sumlog_speed_kn = newSpeed;
  snprintf(lastSentenceType, ...);
  xSemaphoreGive(dataMutex);
  
  updateAllDisplayPulses();
  return true;
}
```

**Jälkeen**:
```cpp
bool parseMWV(char* line) {
  // ... parsinta ...
  
  // Yksi funktio hoitaa kaiken!
  updateDisplaysForSentence("MWV", newAngle, newSpeed, hasSpeed);
  return true;
}
```

**Hyöty**: Vähemmän koodia, selkeämpi logiikka.

---

### 4. updateDisplayPulse() Käyttää Per-Display Dataa

**Ennen**:
```cpp
void updateDisplayPulse(int displayNum) {
  // Luki globaalin muuttujan
  float currentSpeed;
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  currentSpeed = sumlog_speed_kn; // SAMA KAIKILLE
  xSemaphoreGive(dataMutex);
  
  // ...
}
```

**Jälkeen**:
```cpp
void updateDisplayPulse(int displayNum) {
  // Lukee displayn OMAN datan
  float currentSpeed;
  uint32_t lastUpdate;
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  currentSpeed = displays[displayNum].windSpeed_kn; // OMA DATA
  lastUpdate = displays[displayNum].lastUpdate_ms;
  xSemaphoreGive(dataMutex);
  
  // Tarkistaa timeout per-display
  uint32_t dataAge = millis() - lastUpdate;
  if (dataAge > 4000) {
    currentSpeed = 0.0f;
  }
  
  // ...
}
```

**Hyöty**: Jokainen display voi olla eri NMEA-lähteestä, eri timeout.

---

### 5. setOutputsDeg() Käyttää Per-Display Kulmaa

**Ennen**:
```cpp
void setOutputsDeg(int displayNum, int deg) {
  // Käytti parametrina annettua kulmaa
  int adj = wrap360(deg + displays[displayNum].offsetDeg);
  // ...
}
```

**Jälkeen**:
```cpp
void setOutputsDeg(int displayNum, int deg) {
  // Lukee displayn OMAN kulman
  int displayAngle;
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  displayAngle = displays[displayNum].windAngle_deg; // OMA KULMA
  xSemaphoreGive(dataMutex);
  
  int adj = wrap360(displayAngle + displays[displayNum].offsetDeg);
  
  // Vain Display 0 päivittää DAC:ia (yksi DAC kaikille)
  if (displayNum == 0) {
    dac.setDACOutVoltage(sin_mV, CH_SIN);
    dac.setDACOutVoltage(cos_mV, CH_COS);
  }
}
```

**Huom**: Vain Display 0 ohjaa DAC:ia, koska on vain yksi GP8403 DAC.

---

## 🧪 Testaus

### Testiskenaario 1: Eri Lauseet Eri Displayille

**Konfiguraatio**:
```
Display 0: sentence="MWV", type="sumlog"
Display 1: sentence="VWR", type="sumlog"
```

**NMEA Data**:
```
$WIMWV,45.0,R,10.5,N,A*XX
$IIVWR,30.0,L,8.2,N*XX
```

**Odotettu Tulos**:
```
Display 0 freq=10Hz (speed=10.5 kn)  // MWV-data
Display 1 freq=8Hz (speed=8.2 kn)    // VWR-data
```

---

### Testiskenaario 2: Sama Lause Kaikille

**Konfiguraatio**:
```
Display 0: sentence="MWV", type="sumlog"
Display 1: sentence="MWV", type="logicwind"
Display 2: sentence="MWV", type="sumlog"
```

**NMEA Data**:
```
$WIMWV,45.0,R,10.5,N,A*XX
```

**Odotettu Tulos**:
```
Display 0 freq=10Hz (speed=10.5 kn)
Display 1 Logic Wind freq=21Hz (speed=10.5 kn)
Display 2 freq=10Hz (speed=10.5 kn)
```

Kaikki saavat saman datan (MWV), mutta eri tyyppiset mittarit.

---

### Testiskenaario 3: Timeout Per-Display

**Konfiguraatio**:
```
Display 0: sentence="MWV"
Display 1: sentence="VWR"
```

**NMEA Data**:
```
$WIMWV,45.0,R,10.5,N,A*XX  // Jatkuvasti
// VWR lakkaa tulemasta
```

**Odotettu Tulos**:
```
Display 0 freq=10Hz (speed=10.5 kn)  // Toimii
Display 1 timeout! Last data 4000 ms ago - zeroing speed  // Timeout
Display 1 stopped (speed=0)
```

Display 0 jatkaa toimintaa, Display 1 menee nollaan.

---

## 📝 Yhteensopivuus

### Backward Compatibility

Globaalit muuttujat **säilytetty**:
```cpp
extern float sumlog_speed_kn;  // Päivitetään edelleen
extern int angleDeg;           // Päivitetään edelleen
```

**Miksi?**
- Web UI käyttää vielä näitä
- Vanha koodi toimii edelleen
- Helpottaa siirtymää

**Tulevaisuudessa**: Web UI voidaan päivittää näyttämään per-display dataa.

---

## 🎯 Hyödyt

### 1. Joustavuus
- ✅ Jokainen display voi seurata eri NMEA-lausetta
- ✅ Display 0 voi näyttää MWV, Display 1 VWR, Display 2 VWT

### 2. Luotettavuus
- ✅ Per-display timeout
- ✅ Jos yksi NMEA-lähde lakkaa, muut jatkavat

### 3. Selkeys
- ✅ Parserit yksinkertaisempia
- ✅ Yksi funktio hoitaa päivityksen
- ✅ Helpompi ymmärtää data flow

### 4. Skaalautuvuus
- ✅ Helppo lisätä uusia displayjä
- ✅ Helppo lisätä uusia NMEA-lauseita

---

## 🚀 Käyttöönotto

### PC:llä (VS Code)

```bash
git checkout feature-per-display-data
pio run -t upload
pio device monitor
```

### Konfigurointi Web UI:ssa

1. Mene osoitteeseen: http://192.168.4.1
2. Valitse Display 1 → Sentence: "MWV"
3. Valitse Display 2 → Sentence: "VWR"
4. Tallenna

Nyt Display 1 seuraa MWV-lauseita, Display 2 VWR-lauseita!

---

## 📊 Muistin Käyttö

**Ennen**:
```
RAM: 15.4% (50,328 bytes)
```

**Jälkeen**:
```
RAM: 15.4% (50,368 bytes)  // +40 bytes
```

**Lisäys**: 40 tavua (3 displaytä × ~13 tavua per display)

**Hyväksyttävä**: Alle 0.1% lisäys, paljon joustavuutta vastineeksi.

---

## 🔮 Tulevaisuus

### Mahdolliset Parannukset

1. **Web UI päivitys**
   - Näytä per-display data
   - Näytä viimeinen päivitysaika
   - Näytä mikä sentence-tyyppi aktiivinen

2. **Useampi DAC**
   - Tuki useammalle GP8403 DAC:lle
   - Jokainen display voi ohjata omaa DAC:ia

3. **NMEA-lähteen valinta**
   - Display 0: TCP-lähde
   - Display 1: UDP-lähde
   - Display 2: HTTP-lähde

4. **Data-aggregointi**
   - Keskiarvo useammasta lähteestä
   - Paras saatavilla oleva data

---

## ✅ Yhteenveto

**Muutos**: Jokainen display saa nyt **omat deg ja knot muuttujat**

**Hyöty**: Eri displayt voivat seurata **eri NMEA-lauseita**

**Toteutus**: 
- ✅ DisplayConfig laajennettu
- ✅ updateDisplaysForSentence() lisätty
- ✅ Parserit yksinkertaistettu
- ✅ Per-display timeout
- ✅ Backward compatible

**Status**: ✅ Kääntyy, valmis testaukseen

**Branch**: `feature-per-display-data`

---

**Seuraava**: Testaa ESP32:lla eri NMEA-lauseilla! 🚀
