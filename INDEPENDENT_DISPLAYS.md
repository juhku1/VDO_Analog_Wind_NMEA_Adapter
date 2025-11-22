# Itsenäiset Displayt - Eri Avaimet Eri Näytöille

## ✅ Korjattu!

Nyt jokainen display on **täysin itsenäinen** ja voi käyttää **eri NMEA-avaimia**.

---

## 🔴 Alkuperäinen Ongelma

**Mitä teit ensin**:
```cpp
// Päivitit per-display datan oikein
displays[i].windSpeed_kn = speed;
displays[i].windAngle_deg = angle;

// MUTTA päivitit myös globaalit
angleDeg = angle;  // ❌ Ylikirjoitti AINA
sumlog_speed_kn = speed;  // ❌ Ylikirjoitti AINA
```

**Seuraus**:
- Display 0 sai MWV-datan → OK
- Display 1 sai VWR-datan → OK
- **MUTTA** globaalit ylikirjoitettiin viimeisellä lauseella
- Jos VWR tuli viimeisenä, globaalit sisälsivät VWR-dataa
- Web UI näytti väärää dataa

---

## ✅ Korjaus

**Poistettu globaalien päivitys**:
```cpp
void updateDisplaysForSentence(const char* sentenceType, int angle, float speed, bool hasSpeed) {
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  
  for (int i = 0; i < 3; i++) {
    if (!displays[i].enabled) continue;
    
    if (strcmp(displays[i].sentence, sentenceType) == 0) {
      displays[i].windAngle_deg = angle;  // ✅ Vain per-display
      if (hasSpeed) {
        displays[i].windSpeed_kn = speed;  // ✅ Vain per-display
      }
      displays[i].lastUpdate_ms = now;
    }
  }
  
  // ✅ EI enää päivitetä angleDeg ja sumlog_speed_kn
  
  xSemaphoreGive(dataMutex);
  updateAllDisplayPulses();
}
```

---

## 📊 Ennen vs. Jälkeen

### Ennen (Buginen)

```
NMEA Data:
$WIMWV,45.0,R,10.5,N,A*XX  -> MWV
$IIVWR,30.0,L,8.2,N*XX     -> VWR (viimeisenä)

Display 0 (sentence="MWV"):
  displays[0].windSpeed_kn = 10.5  ✅ Oikein
  displays[0].windAngle_deg = 45   ✅ Oikein

Display 1 (sentence="VWR"):
  displays[1].windSpeed_kn = 8.2   ✅ Oikein
  displays[1].windAngle_deg = 330  ✅ Oikein

Globaalit (päivitetty VWR:llä):
  sumlog_speed_kn = 8.2  ❌ VWR-data (pitäisi olla MWV)
  angleDeg = 330         ❌ VWR-data (pitäisi olla MWV)

Web UI näyttää: 8.2 kn, 330°  ❌ VÄÄRIN (pitäisi näyttää Display 0 data)
```

### Jälkeen (Korjattu)

```
NMEA Data:
$WIMWV,45.0,R,10.5,N,A*XX  -> MWV
$IIVWR,30.0,L,8.2,N*XX     -> VWR

Display 0 (sentence="MWV"):
  displays[0].windSpeed_kn = 10.5  ✅ Oikein
  displays[0].windAngle_deg = 45   ✅ Oikein

Display 1 (sentence="VWR"):
  displays[1].windSpeed_kn = 8.2   ✅ Oikein
  displays[1].windAngle_deg = 330  ✅ Oikein

Globaalit:
  sumlog_speed_kn = 0.0  ✅ Ei päivitetä (ei käytetä)
  angleDeg = 0           ✅ Ei päivitetä (ei käytetä)

Web UI lukee Display 0:n datan:
  display0_speed = displays[0].windSpeed_kn = 10.5  ✅ OIKEIN
  display0_angle = displays[0].windAngle_deg = 45   ✅ OIKEIN

Web UI näyttää: 10.5 kn, 45°  ✅ OIKEIN
```

---

## 🎯 Nyt Toimii

### Esimerkki 1: Eri Lauseet

**Konfiguraatio**:
```
Display 0: sentence="MWV", type="sumlog"
Display 1: sentence="VWR", type="sumlog"
Display 2: sentence="VWT", type="logicwind"
```

**NMEA Data**:
```
$WIMWV,45.0,R,10.5,N,A*XX
$IIVWR,30.0,L,8.2,N*XX
$IIVWT,60.0,R,12.3,N*XX
```

**Tulos**:
```
Display 0: 10.5 kn, 45°   (MWV)
Display 1: 8.2 kn, 330°   (VWR, L=vasen)
Display 2: 12.3 kn, 60°   (VWT)
```

**Jokainen display saa OMAN datansa!** ✅

---

### Esimerkki 2: Yksi Lähde Lakkaa

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

**Tulos**:
```
Display 0: 10.5 kn, 45°  ✅ Toimii
Display 1: timeout (4s) -> 0 kn  ✅ Vain Display 1 menee nollaan
```

**Display 0 jatkaa toimintaa normaalisti!** ✅

---

## 🔧 Tekniset Muutokset

### 1. updateDisplaysForSentence()

**Poistettu**:
```cpp
// EI enää:
angleDeg = angle;
sumlog_speed_kn = speed;
```

**Säilytetty**:
```cpp
// Vain debugging:
snprintf(lastSentenceType, sizeof(lastSentenceType), "%s", sentenceType);
```

---

### 2. Web UI (handleStatus)

**Ennen**:
```cpp
j += ",\"speed_kn\":";    j += sumlog_speed_kn;  // ❌ Globaali
```

**Jälkeen**:
```cpp
// Lue Display 0:n data
float display0_speed;
xSemaphoreTake(dataMutex, portMAX_DELAY);
display0_speed = displays[0].windSpeed_kn;  // ✅ Display 0
xSemaphoreGive(dataMutex);

j += ",\"speed_kn\":";    j += display0_speed;  // ✅ Oikea data
```

---

### 3. Manuaalinen Kulman Asetus (handleGoto)

**Ennen**:
```cpp
angleDeg = v;  // ❌ Globaali
setOutputsDeg(0, angleDeg);
```

**Jälkeen**:
```cpp
// Aseta Display 0:n kulma
xSemaphoreTake(dataMutex, portMAX_DELAY);
displays[0].windAngle_deg = v;  // ✅ Display 0
xSemaphoreGive(dataMutex);

setOutputsDeg(0, 0);  // Päivitä DAC
```

---

## 📝 Globaalit Muuttujat

**Status**: Säilytetty mutta EI käytetä

```cpp
// Määritelty mutta ei päivitetä:
float sumlog_speed_kn = 0.0;  // Ei käytetä
int angleDeg = 0;             // Ei käytetä
```

**Miksi säilytetty?**
- Backward compatibility (jos joku vanha koodi viittaa niihin)
- Voidaan poistaa myöhemmin jos ei tarvita

**Tulevaisuudessa**: Voidaan poistaa kokonaan.

---

## 🧪 Testaus

### Tarkista Serial Monitorista

**Hyvä (toimii)**:
```
Display 0 freq=10Hz (speed=10.5 kn)  // MWV-data
Display 1 freq=8Hz (speed=8.2 kn)    // VWR-data
Display 2 Logic Wind freq=24Hz (speed=12.3 kn)  // VWT-data
```

**Huono (bugi)**:
```
Display 0 freq=8Hz (speed=8.2 kn)  // ❌ Väärä data (pitäisi olla 10.5)
Display 1 freq=8Hz (speed=8.2 kn)  // ✅ Oikea data
```

Jos kaikki displayt näyttävät samaa dataa, globaalit päivitetään vielä jossain.

---

### Testaa Web UI

1. Avaa: http://192.168.4.1
2. Katso status-sivu
3. Pitäisi näyttää **Display 0:n data** (päämitta)
4. Jos näyttää väärää dataa, ongelma Web UI:ssa

---

## ✅ Yhteenveto

**Ongelma**: Globaalit ylikirjoitettiin viimeisellä lauseella

**Ratkaisu**: Poistettu globaalien päivitys kokonaan

**Tulos**:
- ✅ Jokainen display täysin itsenäinen
- ✅ Eri displayt voivat käyttää eri NMEA-avaimia
- ✅ Web UI näyttää Display 0:n datan (päämitta)
- ✅ Ei enää globaalien ylikirjoitusta

**Status**: ✅ Korjattu ja testattu (kääntyy)

**Branch**: `feature-per-display-data`

---

## 🚀 Seuraavat Askeleet

1. **Testaa PC:llä**:
   ```bash
   git checkout feature-per-display-data
   pio run -t upload
   pio device monitor
   ```

2. **Konfiguroi eri lauseet**:
   - Display 0: MWV
   - Display 1: VWR
   - Display 2: VWT

3. **Lähetä NMEA-dataa**:
   - Käytä tools/nmea_wind_sender_gui.py
   - Tai oikea NMEA-lähde

4. **Tarkista**:
   - Jokainen display saa oman datansa
   - Serial Monitor näyttää eri arvot
   - Web UI näyttää Display 0:n datan

5. **Jos toimii**:
   ```bash
   git checkout main
   git merge feature-per-display-data
   git push origin main
   ```

**Nyt eri näytöille voi tehdä eri avaimet!** 🎉
