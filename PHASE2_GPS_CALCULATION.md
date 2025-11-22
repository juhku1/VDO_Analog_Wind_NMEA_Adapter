# Phase 2: GPS-parsinta ja Laskenta

## 🎯 Tavoite

Lisätään GPS-datan parsinta ja lasketaan True Wind sekä VMG, jos suoraa dataa ei ole saatavilla.

---

## 📊 Mitä Toteutettiin

### 1. GPS-parserit

**RMC** - Recommended Minimum Navigation Information
```
$GPRMC,time,status,lat,N,lon,E,speed,course,date,magvar,E*checksum
                                    ↑     ↑
                                   SOG   COG
```
- SOG: Speed Over Ground (knots)
- COG: Course Over Ground (degrees)

**VTG** - Track Made Good and Ground Speed
```
$GPVTG,cogt,T,cogm,M,sog,N,sogk,K*checksum
       ↑              ↑
      COG            SOG
```

**HDT** - True Heading
```
$GPHDT,heading,T*checksum
       ↑
   True Heading
```

**HDM** - Magnetic Heading
```
$GPHDM,heading,M*checksum
       ↑
  Magnetic Heading
```

---

### 2. GPS-datan Tallennus

```cpp
// GPS data - protected by dataMutex
float gps_sog_kn = 0.0;        // Speed Over Ground
float gps_cog_deg = 0.0;       // Course Over Ground
float gps_heading_deg = 0.0;   // True/Magnetic Heading
bool gps_hasSOG = false;
bool gps_hasCOG = false;
bool gps_hasHeading = false;
uint32_t gps_lastUpdate_ms = 0;

// Apparent Wind for calculation
float apparent_speed_kn = 0.0;
float apparent_angle_deg = 0.0;
bool apparent_hasData = false;
uint32_t apparent_lastUpdate_ms = 0;
```

---

### 3. True Wind Laskenta

**Formula**:
```
True Wind = Apparent Wind - Boat Vector

Apparent Wind (relative to boat):
  AWS_x = AWS × sin(AWA)  // Cross component
  AWS_y = AWS × cos(AWA)  // Forward component

Boat Vector (subtract from apparent):
  Boat_x = 0
  Boat_y = SOG

True Wind:
  TWS_x = AWS_x - Boat_x = AWS_x
  TWS_y = AWS_y - Boat_y = AWS_y - SOG
  
  TWS = sqrt(TWS_x² + TWS_y²)
  TWA = atan2(TWS_x, TWS_y) × 180/π
```

**Esimerkki**:
```
Apparent Wind: 10 kn @ 45° (relative to bow)
Boat Speed: 5 kn

AWS_x = 10 × sin(45°) = 7.07 kn
AWS_y = 10 × cos(45°) = 7.07 kn

TWS_x = 7.07 kn
TWS_y = 7.07 - 5 = 2.07 kn

TWS = sqrt(7.07² + 2.07²) = 7.36 kn
TWA = atan2(7.07, 2.07) = 73.7°

True Wind: 7.36 kn @ 73.7°
```

---

### 4. VMG Laskenta

**Yksinkertainen versio** (ilman waypoint):
```cpp
VMG = SOG  // Speed Over Ground
```

**Tulevaisuus** (waypoint-pohjainen):
```cpp
VMG = SOG × cos(angle to waypoint)
```

---

## 🔄 Data Flow

### Skenaario 1: Suora True Wind Data

```
NMEA: $WIMWV,50.0,T,12.0,N,A*XX  (MWV True)
      ↓
Display (dataType=TRUE_WIND):
  windSpeed_kn = 12.0 kn
  windAngle_deg = 50°
```

**Tulos**: Käytetään suoraa dataa ✅

---

### Skenaario 2: Laskettu True Wind

```
NMEA: $WIMWV,45.0,R,10.0,N,A*XX  (MWV Apparent)
      $GPRMC,...,5.0,180,...*XX   (RMC: SOG=5kn, COG=180°)
      ↓
Apparent Wind: 10 kn @ 45°
GPS: SOG = 5 kn
      ↓
calculateTrueWind():
  TWS = 7.36 kn
  TWA = 73.7°
      ↓
Display (dataType=TRUE_WIND):
  windSpeed_kn = 7.36 kn
  windAngle_deg = 73.7°
```

**Tulos**: Lasketaan True Wind ✅

---

### Skenaario 3: VMG

```
NMEA: $GPRMC,...,8.5,180,...*XX  (SOG=8.5kn)
      ↓
calculateVMG():
  VMG = 8.5 kn
      ↓
Display (dataType=VMG):
  windSpeed_kn = 8.5 kn
  windAngle_deg = 0°
```

**Tulos**: VMG näytetään ✅

---

## 🧠 Älykäs Valinta

### updateDisplaysForSentence() Logiikka

```cpp
1. Tallenna Apparent Wind data (MWV R, VWR)
   → apparent_speed_kn, apparent_angle_deg

2. Päivitä displayt jotka haluavat suoraa dataa
   → Apparent Wind displayt saavat MWV(R)/VWR
   → True Wind displayt saavat MWV(T)/VWT

3. Yritä laskea True Wind
   if (hasApparent && hasGPS) {
     calculateTrueWind()
     → Päivitä True Wind displayt (jos ei ole tuoretta suoraa dataa)
   }

4. Yritä laskea VMG
   if (hasGPS) {
     calculateVMG()
     → Päivitä VMG displayt
   }
```

**Prioriteetti**:
1. Suora data (MWV T, VWT) - **Paras**
2. Laskettu data (Apparent + GPS) - **Hyvä**
3. Ei dataa - **Timeout**

---

## 📋 Esimerkkejä

### Esimerkki 1: Molemmat Saatavilla

**NMEA Data**:
```
$WIMWV,45.0,R,10.0,N,A*XX  (Apparent)
$WIMWV,50.0,T,12.0,N,A*XX  (True - suora)
$GPRMC,...,5.0,180,...*XX  (GPS)
```

**Display 0** (dataType=APPARENT_WIND):
```
windSpeed_kn = 10.0 kn
windAngle_deg = 45°
```

**Display 1** (dataType=TRUE_WIND):
```
windSpeed_kn = 12.0 kn  ✅ Käyttää suoraa MWV(T) dataa
windAngle_deg = 50°
```

**Tulos**: Suora data käytetään, laskentaa ei tarvita ✅

---

### Esimerkki 2: Vain Apparent + GPS

**NMEA Data**:
```
$WIMWV,45.0,R,10.0,N,A*XX  (Apparent)
$GPRMC,...,5.0,180,...*XX  (GPS)
// Ei MWV(T) tai VWT
```

**Display 0** (dataType=APPARENT_WIND):
```
windSpeed_kn = 10.0 kn
windAngle_deg = 45°
```

**Display 1** (dataType=TRUE_WIND):
```
windSpeed_kn = 7.36 kn  ✅ Laskettu: Apparent + GPS
windAngle_deg = 73.7°
```

**Tulos**: True Wind lasketaan automaattisesti ✅

---

### Esimerkki 3: VMG

**NMEA Data**:
```
$GPRMC,...,8.5,180,...*XX  (SOG=8.5kn)
```

**Display 2** (dataType=VMG):
```
windSpeed_kn = 8.5 kn  ✅ VMG = SOG
windAngle_deg = 0°
```

**Tulos**: VMG näytetään ✅

---

## 🧪 Testaus

### Serial Monitor

**GPS-parserit**:
```
GPS RMC: SOG=5.0 kn, COG=180.0°
GPS VTG: SOG=5.0 kn, COG=180.0°
GPS HDT: Heading=180.0°
GPS HDM: Heading=180.0° (magnetic)
```

**True Wind -laskenta**:
```
Calculated True Wind: AWS=10.0@45° + SOG=5.0 -> TWS=7.4@74°
Display 1 freq=7Hz (speed=7.4 kn)
```

**VMG**:
```
Display 2 freq=8Hz (speed=8.5 kn)
```

---

### API-testaus

```bash
# Aseta Display 0: Apparent Wind
curl "http://192.168.4.1/api/display?num=1&dataType=0&save=1"

# Aseta Display 1: True Wind (lasketaan jos ei suoraa dataa)
curl "http://192.168.4.1/api/display?num=2&dataType=1&save=1"

# Aseta Display 2: VMG
curl "http://192.168.4.1/api/display?num=3&dataType=2&save=1"
```

---

## 🎯 Hyödyt

### 1. Automaattinen Laskenta
- ✅ Jos ei ole MWV(T), lasketaan Apparent + GPS
- ✅ Käyttäjän ei tarvitse huolehtia

### 2. Prioriteetti
- ✅ Suora data aina parempi kuin laskettu
- ✅ Laskettu data parempi kuin ei dataa

### 3. Joustavuus
- ✅ Toimii eri NMEA-lähteillä
- ✅ RMC TAI VTG (molemmat toimivat)
- ✅ HDT TAI HDM (molemmat toimivat)

### 4. VMG
- ✅ Yksinkertainen VMG (SOG)
- ✅ Valmis waypoint-pohjaiseen VMG:hen

---

## 📝 Rajoitukset

### Data-ikä
- Apparent Wind: Max 5s vanha
- GPS: Max 5s vanha
- Jos vanhempi → Ei lasketa

### Tarkkuus
- True Wind -laskenta olettaa:
  - Tasainen vesi (ei virtaa)
  - Tarkka SOG ja COG
  - Tarkka Apparent Wind

### VMG
- Nykyinen: VMG = SOG (yksinkertainen)
- Tulevaisuus: Waypoint-pohjainen VMG

---

## 🔮 Tulevaisuus

### Phase 3: Waypoint-pohjainen VMG

```cpp
// Tarvitaan:
- Waypoint lat/lon
- Current position (RMC)
- Bearing to waypoint

VMG = SOG × cos(COG - bearing_to_waypoint)
```

### Phase 4: Virtakorjaus

```cpp
// True Wind korjaus vedenvirtaukselle
- Water speed (STW) vs Ground speed (SOG)
- Current vector = SOG - STW
- True Wind korjaus
```

---

## ✅ Yhteenveto

**Toteutettu**:
- ✅ RMC, VTG, HDT, HDM parserit
- ✅ GPS-datan tallennus
- ✅ True Wind -laskenta (Apparent + GPS)
- ✅ VMG-laskenta (yksinkertainen)
- ✅ Älykäs valinta (suora vs. laskettu)

**Hyödyt**:
- ✅ Automaattinen True Wind jos ei suoraa dataa
- ✅ VMG-tuki
- ✅ Joustavuus eri NMEA-lähteille

**Status**: ✅ Phase 2 valmis, kääntyy

**Branch**: `feature-per-display-data`

**Seuraava**: Testaa oikealla GPS-datalla! 🚀
