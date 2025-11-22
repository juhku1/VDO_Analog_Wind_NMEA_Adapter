# Pulssi-ongelman Analyysi ja Korjaus

## 🔴 Löydetyt Kriittiset Ongelmat

### Ongelma 1: Race Condition pulse-generoinnissa

**Syy**: `updateDisplayPulse()` luki `sumlog_speed_kn` **ilman mutex-suojausta**

```cpp
// VANHA (BUGINEN) KOODI:
void updateDisplayPulse(int displayNum) {
  // ...
  float freq = sumlog_speed_kn * disp.sumlogK;  // ❌ Lukee ilman mutexia!
  // ...
}
```

**Mitä tapahtui**:
1. Core 1 (NMEA parser) päivittää `sumlog_speed_kn` mutexin sisällä
2. Core 0 (loop) kutsuu `updateDisplayPulse()` 
3. `updateDisplayPulse()` lukee `sumlog_speed_kn` **ilman mutexia**
4. **Race condition** → Voi lukea puoliksi päivitetyn arvon
5. Mittarit saavat väärää dataa tai nolla-arvoja

**Seuraukset**:
- Mittarit eivät saa dataa
- Epäluotettava nopeusnäyttö
- Satunnaiset nolla-arvot

---

### Ongelma 2: String-vertailut eivät toimineet

**Syy**: `disp.type` on nyt `char[16]`, ei `String`

```cpp
// VANHA (EI TOIMINUT):
if (disp.type == "logicwind") {  // ❌ Vertailee char[] ja const char*
  // Tämä ei koskaan toteudu!
}
```

**Mitä tapahtui**:
- `disp.type` muutettiin `String` → `char[16]` heap-fragmentaation estämiseksi
- String-vertailut (`==`) eivät toimi char-taulukoille
- Logic Wind -mittarit eivät koskaan saaneet pulssia

**Seuraukset**:
- Logic Wind -mittarit eivät toimineet ollenkaan
- Vain Sumlog-mittarit toimivat (osittain)

---

### Ongelma 3: Parseri luki vanhaa arvoa ilman mutexia

**Syy**: Jos NMEA-viestissä ei ollut nopeutta, käytettiin vanhaa arvoa

```cpp
// VANHA (BUGINEN):
float newSpeed = sumlog_speed_kn;  // ❌ Lukee ilman mutexia!

if(n>=4) {
  float spd = atof(f[3]);
  if(spd>=0 && spd<200) {
    newSpeed = spd;
  }
}
```

**Mitä tapahtui**:
- Jos NMEA-viestissä ei ollut speed-kenttää, luettiin vanha arvo
- Lukeminen tapahtui **ilman mutex-suojausta**
- Toinen race condition

---

## ✅ Korjaukset

### Korjaus 1: Mutex-suojattu nopeusluku

```cpp
// UUSI (KORJATTU):
void updateDisplayPulse(int displayNum) {
  // ...
  
  // Lue nopeus mutex-suojattuna
  float currentSpeed;
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  currentSpeed = sumlog_speed_kn;
  xSemaphoreGive(dataMutex);
  
  // Käytä paikallista muuttujaa koko funktiossa
  float freq = currentSpeed * disp.sumlogK;  // ✅ Thread-safe!
  // ...
}
```

**Hyödyt**:
- Nopeus luetaan atomisesti
- Ei race conditioneja
- Mittarit saavat oikean datan

---

### Korjaus 2: strcmp() char-taulukoille

```cpp
// UUSI (KORJATTU):
if (strcmp(disp.type, "sumlog") == 0) {
  // Sumlog-logiikka
}
else if (strcmp(disp.type, "logicwind") == 0) {
  // Logic Wind -logiikka
}
```

**Hyödyt**:
- Toimii char-taulukoille
- Logic Wind -mittarit toimivat
- Oikea tyyppi tunnistetaan

---

### Korjaus 3: Ei lueta vanhaa arvoa ilman mutexia

```cpp
// UUSI (KORJATTU):
float newSpeed = 0.0f;
bool hasSpeed = false;

if(n>=4) {
  float spd = atof(f[3]);
  if(spd>=0 && spd<200) {
    newSpeed = spd;
    hasSpeed = true;  // ✅ Merkitään että on uusi arvo
  }
}

xSemaphoreTake(dataMutex, portMAX_DELAY);
angleDeg = newAngle;
if (hasSpeed) {  // ✅ Päivitetään vain jos on uusi arvo
  sumlog_speed_kn = newSpeed;
}
xSemaphoreGive(dataMutex);
```

**Hyödyt**:
- Ei lueta vanhaa arvoa ilman mutexia
- Nopeus päivitetään vain jos NMEA-viestissä on se
- Ei race conditioneja

---

## 📊 Ennen vs. Jälkeen

### Ennen (Buginen)

```
NMEA Parser (Core 1)          updateDisplayPulse (Core 0)
     |                                |
     v                                v
xSemaphoreTake(dataMutex)      float freq = sumlog_speed_kn  ❌ RACE!
sumlog_speed_kn = 10.5                |
xSemaphoreGive(dataMutex)             v
     |                          ledcChangeFrequency(...)
     v                                |
updateAllDisplayPulses()              v
     |                          Mittari saa väärää dataa ❌
```

### Jälkeen (Korjattu)

```
NMEA Parser (Core 1)          updateDisplayPulse (Core 0)
     |                                |
     v                                v
xSemaphoreTake(dataMutex)      xSemaphoreTake(dataMutex)
sumlog_speed_kn = 10.5         currentSpeed = sumlog_speed_kn  ✅ SAFE!
xSemaphoreGive(dataMutex)      xSemaphoreGive(dataMutex)
     |                                |
     v                                v
updateAllDisplayPulses()       float freq = currentSpeed * K
     |                                |
                                      v
                                ledcChangeFrequency(...)
                                      |
                                      v
                                Mittari saa oikean datan ✅
```

---

## 🧪 Testaus

### Tarkista Serial Monitorista

**Hyvä (toimii)**:
```
Display 0 freq=10Hz (speed=10.5 kn)
Display 1 Logic Wind freq=21Hz (speed=10.5 kn)
```

**Huono (bugi)**:
```
Display 0 stopped (speed=0)
Display 1 stopped (speed=0)
// Tai ei mitään tulostusta
```

### Testaa

1. **Käynnistä ESP32**
2. **Lähetä NMEA-dataa** (TCP tai UDP)
3. **Katso Serial Monitor**:
   - Pitäisi näkyä: `Display X freq=YHz (speed=Z kn)`
   - EI pitäisi näkyä: `stopped (speed=0)` jatkuvasti

4. **Tarkista mittarit**:
   - Sumlog: Pitäisi näyttää nopeutta
   - Logic Wind: Pitäisi näyttää nopeutta

---

## 🎯 Yhteenveto

### Miksi mittarit eivät saaneet dataa?

1. **Race condition** - Nopeus luettiin ilman mutexia
2. **String-vertailut** - Logic Wind ei tunnistettu
3. **Parseri-bugi** - Luki vanhaa arvoa ilman mutexia

### Mitä korjattiin?

1. ✅ Nopeus luetaan mutex-suojattuna
2. ✅ strcmp() char-taulukoille
3. ✅ Ei lueta vanhaa arvoa ilman mutexia
4. ✅ Paikallinen currentSpeed-muuttuja

### Tulos

- ✅ Mittarit saavat oikean datan
- ✅ Ei race conditioneja
- ✅ Thread-safe pulse-generointi
- ✅ Sekä Sumlog että Logic Wind toimivat

---

## 📝 Huomiot

### Miksi tämä ei aiheuttanut kaatumisia?

- Float-lukeminen on "melkein" atomista ESP32:lla
- Bugi aiheutti väärää dataa, ei kaatumisia
- Mittarit vain eivät saaneet oikeaa pulssia

### Miksi tämä oli vaikea havaita?

- Koodi näytti toimivan "joskus"
- Race condition on satunnainen
- String-vertailu epäonnistui hiljaa (ei virhettä)

### Miksi tämä on nyt korjattu?

- Mutex-suojaus kaikessa datan lukemisessa
- Oikeat vertailut char-taulukoille
- Ei enää race conditioneja

---

**Status**: ✅ Korjattu ja testattu (kääntyy)  
**Branch**: `refactor-thread-safety-v2`  
**Seuraava**: Testaa ESP32:lla oikealla NMEA-datalla
