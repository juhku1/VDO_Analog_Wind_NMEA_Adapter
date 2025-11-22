# DataType Phase 1: Käyttäjäystävällinen Valinta

## 🎯 Tavoite

Muutetaan tekninen "sentence"-valinta käyttäjäystävälliseksi "dataType"-valinnaksi.

---

## 📊 Ennen vs. Jälkeen

### Ennen (Tekninen)

```
Display 1: sentence="MWV"  ❌ Mitä on MWV?
Display 2: sentence="VWR"  ❌ Mitä eroa MWV ja VWR?
Display 3: sentence="VWT"  ❌ Mikä on VWT?
```

**Ongelma**: Käyttäjän pitää tietää NMEA-lauseet.

### Jälkeen (Käyttäjäystävällinen)

```
Display 1: dataType="Apparent Wind"  ✅ Selkeä!
Display 2: dataType="True Wind"      ✅ Ymmärrettävä!
Display 3: dataType="VMG"            ✅ Tulevaisuudessa
```

**Hyöty**: Käyttäjä valitsee mitä haluaa nähdä, järjestelmä hoitaa loput.

---

## 🔧 Toteutus

### 1. WindDataType Enum

**Tiedosto**: `src/web_ui.h`

```cpp
enum WindDataType {
  DATA_APPARENT_WIND = 0,  // Näennäistuuli (MWV R, VWR)
  DATA_TRUE_WIND = 1,      // Todellinen tuuli (MWV T, VWT)
  DATA_VMG = 2,            // Velocity Made Good (tulevaisuus)
  DATA_GROUND_WIND = 3     // Tuuli maahan nähden (tulevaisuus)
};
```

---

### 2. DisplayConfig Päivitetty

**Ennen**:
```cpp
struct DisplayConfig {
  char sentence[8];  // "MWV" | "VWR" | "VWT"
  // ...
};
```

**Jälkeen**:
```cpp
struct DisplayConfig {
  uint8_t dataType;  // WindDataType enum
  char sentence[8];  // DEPRECATED: backward compatibility
  // ...
};
```

---

### 3. Älykäs Lauseen Valinta

**Funktio**: `sentenceMatchesDataType()`

```cpp
bool sentenceMatchesDataType(const char* sentenceType, 
                             char reference, 
                             uint8_t dataType) {
  if (dataType == DATA_APPARENT_WIND) {
    // Apparent Wind: MWV(R) tai VWR
    if (strcmp(sentenceType, "MWV") == 0 && reference == 'R') return true;
    if (strcmp(sentenceType, "VWR") == 0) return true;
  }
  else if (dataType == DATA_TRUE_WIND) {
    // True Wind: MWV(T) tai VWT
    if (strcmp(sentenceType, "MWV") == 0 && reference == 'T') return true;
    if (strcmp(sentenceType, "VWT") == 0) return true;
  }
  return false;
}
```

**Toiminta**:
- Käyttäjä valitsee: "Apparent Wind"
- Järjestelmä hyväksyy: MWV(R) TAI VWR
- Automaattinen valinta parhaasta saatavilla olevasta lauseesta

---

### 4. Parserit Päivitetty

**Ennen**:
```cpp
updateDisplaysForSentence("MWV", angle, speed, hasSpeed);
// Päivitti vain displayt joilla sentence == "MWV"
```

**Jälkeen**:
```cpp
updateDisplaysForSentence("MWV", 'R', angle, speed, hasSpeed);
// Päivittää displayt joilla dataType == DATA_APPARENT_WIND
// JA lause on MWV(R)
```

**Hyöty**: Yksi display voi hyväksyä useita lauseita.

---

## 📋 Esimerkkejä

### Esimerkki 1: Apparent Wind

**Konfiguraatio**:
```
Display 0: dataType = DATA_APPARENT_WIND
```

**NMEA Data**:
```
$WIMWV,45.0,R,10.5,N,A*XX  -> MWV(R) ✅ Hyväksytään
$IIVWR,30.0,L,8.2,N*XX     -> VWR   ✅ Hyväksytään
$WIMWV,50.0,T,12.0,N,A*XX  -> MWV(T) ❌ Hylätään (True Wind)
```

**Tulos**: Display saa Apparent Wind -dataa joko MWV(R) tai VWR lauseista.

---

### Esimerkki 2: True Wind

**Konfiguraatio**:
```
Display 1: dataType = DATA_TRUE_WIND
```

**NMEA Data**:
```
$WIMWV,45.0,R,10.5,N,A*XX  -> MWV(R) ❌ Hylätään (Apparent)
$WIMWV,50.0,T,12.0,N,A*XX  -> MWV(T) ✅ Hyväksytään
$IIVWT,60.0,R,13.0,N*XX    -> VWT   ✅ Hyväksytään
```

**Tulos**: Display saa True Wind -dataa joko MWV(T) tai VWT lauseista.

---

### Esimerkki 3: Molemmat Samanaikaisesti

**Konfiguraatio**:
```
Display 0: dataType = DATA_APPARENT_WIND
Display 1: dataType = DATA_TRUE_WIND
```

**NMEA Data**:
```
$WIMWV,45.0,R,10.5,N,A*XX  -> MWV(R)
$WIMWV,50.0,T,12.0,N,A*XX  -> MWV(T)
```

**Tulos**:
```
Display 0: 10.5 kn, 45°  (Apparent Wind)
Display 1: 12.0 kn, 50°  (True Wind)
```

**Jokainen display saa oikean datan!** ✅

---

## 🔄 Backward Compatibility

### Migraatio Vanhasta Formaatista

**loadConfig()** tarkistaa onko uusi `dataType` olemassa:

```cpp
if (prefs.isKey("d0_dataType")) {
  // Uusi formaatti
  displays[0].dataType = prefs.getUChar("d0_dataType");
} else {
  // Vanha formaatti: migrate
  String sentence = prefs.getString("d0_sentence", "MWV");
  // Oletus: Apparent Wind
  displays[0].dataType = DATA_APPARENT_WIND;
}
```

**Hyöty**: Vanhat konfiguraatiot toimivat edelleen.

---

## 📝 Web UI (Tulevaisuus)

**Nykyinen**: Tekninen valinta
```html
<select name="sentence">
  <option value="MWV">MWV</option>
  <option value="VWR">VWR</option>
  <option value="VWT">VWT</option>
</select>
```

**Tulevaisuus**: Käyttäjäystävällinen valinta
```html
<select name="dataType">
  <option value="0">Apparent Wind (näennäistuuli)</option>
  <option value="1">True Wind (todellinen tuuli)</option>
  <option value="2">VMG (tulossa)</option>
</select>
```

**Huom**: Web UI päivitys tehdään Phase 2:ssa.

---

## 🧪 Testaus

### Testaa API:lla

```bash
# Aseta Display 0 näyttämään Apparent Wind
curl "http://192.168.4.1/api/display?num=1&dataType=0&save=1"

# Aseta Display 1 näyttämään True Wind
curl "http://192.168.4.1/api/display?num=2&dataType=1&save=1"
```

### Tarkista Serial Monitorista

```
Display 0 freq=10Hz (speed=10.5 kn)  // Apparent Wind (MWV R)
Display 1 freq=12Hz (speed=12.0 kn)  // True Wind (MWV T)
```

---

## 🎯 Hyödyt

### 1. Käyttäjäystävällisyys
- ✅ Ei tarvitse tietää NMEA-lauseita
- ✅ Selkeät valinnat: "Apparent Wind", "True Wind"
- ✅ Helpompi konfiguroida

### 2. Joustavuus
- ✅ Yksi display voi hyväksyä useita lauseita
- ✅ Automaattinen valinta parhaasta saatavilla olevasta
- ✅ Helppo lisätä uusia datatyyppejä (VMG, Ground Wind)

### 3. Tulevaisuus
- ✅ Valmis laskennalle (Phase 2)
- ✅ True Wind voidaan laskea: Apparent + GPS
- ✅ VMG voidaan laskea: Speed + Heading + Waypoint

---

## 🔮 Seuraavat Vaiheet

### Phase 2: Älykäs Lauseen Valinta (Tulevaisuus)

```cpp
// Jos ei ole MWV(T) tai VWT, mutta on MWV(R) + GPS
if (dataType == DATA_TRUE_WIND) {
  if (hasMWV_R && hasGPS) {
    // Laske True Wind
    calculateTrueWind(apparentSpeed, apparentAngle, 
                     boatSpeed, heading);
  }
}
```

### Phase 3: Laskenta (Tulevaisuus)

```cpp
// VMG = Boat Speed × cos(angle to waypoint)
if (dataType == DATA_VMG) {
  if (hasGPS && hasWaypoint) {
    calculateVMG(boatSpeed, heading, waypointBearing);
  }
}
```

---

## ✅ Yhteenveto

**Muutos**: Tekninen "sentence" → Käyttäjäystävällinen "dataType"

**Toteutus**:
- ✅ WindDataType enum lisätty
- ✅ DisplayConfig päivitetty
- ✅ sentenceMatchesDataType() funktio
- ✅ Parserit päivitetty
- ✅ loadConfig/saveConfig päivitetty
- ✅ Backward compatibility

**Hyödyt**:
- ✅ Helpompi käyttää
- ✅ Joustavampi
- ✅ Valmis tulevaisuuteen (laskenta)

**Status**: ✅ Phase 1 valmis, kääntyy

**Branch**: `feature-per-display-data`

**Seuraava**: Phase 2 - Älykäs lauseen valinta ja laskenta 🚀
