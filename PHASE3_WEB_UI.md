# Phase 3: Web UI Päivitys

## 🎯 Tavoite

Päivitetään Web UI käyttämään käyttäjäystävällistä dataType-valintaa teknisen NMEA sentence -valinnan sijaan.

---

## 📊 Ennen vs. Jälkeen

### Ennen (Tekninen)

```html
<select id="displaySentence">
  <option value="MWV">MWV (Wind Speed & Angle)</option>
  <option value="VWR">VWR (Relative Wind)</option>
  <option value="VWT">VWT (True Wind)</option>
</select>
```

**Ongelma**: Käyttäjän pitää tietää NMEA-lauseet.

---

### Jälkeen (Käyttäjäystävällinen)

```html
<select id="displayDataType">
  <option value="0">Apparent Wind</option>
  <option value="1">True Wind</option>
  <option value="2">VMG (Velocity Made Good)</option>
</select>
```

**Hyöty**: Selkeät valinnat, ei teknistä jargonia!

---

## 🔧 Toteutus

### 1. DataType-nimet

**Tiedosto**: `src/web_pages.cpp`

```cpp
const char* getDataTypeName(uint8_t dataType) {
  switch(dataType) {
    case DATA_APPARENT_WIND: return "Apparent Wind";
    case DATA_TRUE_WIND: return "True Wind";
    case DATA_VMG: return "VMG";
    case DATA_GROUND_WIND: return "Ground Wind";
    default: return "Unknown";
  }
}

const char* getDataTypeDescription(uint8_t dataType) {
  switch(dataType) {
    case DATA_APPARENT_WIND: 
      return "Wind relative to boat (from MWV R or VWR)";
    case DATA_TRUE_WIND: 
      return "True wind (from MWV T, VWT, or calculated from Apparent + GPS)";
    case DATA_VMG: 
      return "Velocity Made Good (from GPS)";
    case DATA_GROUND_WIND: 
      return "Wind relative to ground (future)";
    default: return "";
  }
}
```

---

### 2. HTML Dropdown

**Ennen**:
```html
<label>NMEA Sentence</label>
<select id=displaySentence>
  <option value="MWV">MWV (Wind Speed & Angle)</option>
  <option value="VWR">VWR (Relative Wind)</option>
  <option value="VWT">VWT (True Wind)</option>
</select>
```

**Jälkeen**:
```html
<label>Wind Data Type</label>
<select id=displayDataType>
  <option value="0">Apparent Wind</option>
  <option value="1">True Wind</option>
  <option value="2">VMG (Velocity Made Good)</option>
</select>
<span class="info-icon" data-tooltip="Apparent Wind: Wind relative to boat. True Wind: Calculated or from NMEA. VMG: Speed from GPS">i</span>
```

---

### 3. JavaScript Päivitys

**Tallennus**:
```javascript
async function saveDisplaySettings(){
  const dataType = document.getElementById('displayDataType').value;
  
  const params = new URLSearchParams({
    dataType: dataType,  // Uusi kenttä
    // ... muut kentät
  });
  
  await fetch('/api/display?num=' + DISPLAY_NUM + '&action=save', {
    method: 'POST',
    body: params
  });
}
```

**Lataus**:
```javascript
async function loadDisplayValues() {
  const r = await fetch('/api/display?num=' + DISPLAY_NUM); 
  const j = await r.json();
  
  document.getElementById('displayDataType').value = 
    j.dataType !== undefined ? j.dataType : 0;
}
```

---

### 4. API Päivitys

**Tiedosto**: `src/web_ui.cpp`

**JSON Response**:
```cpp
String json = "{";
json += "\"enabled\":" + String(displays[arrayIndex].enabled ? "true" : "false");
json += ",\"type\":\"" + String(displays[arrayIndex].type) + "\"";
json += ",\"dataType\":" + String(displays[arrayIndex].dataType);  // UUSI
json += ",\"offsetDeg\":" + String(displays[arrayIndex].offsetDeg);
// ...
json += "}";
```

---

## 🖥️ Käyttöliittymä

### Display Configuration Page

```
┌─────────────────────────────────────────┐
│ Display 1 Settings                      │
├─────────────────────────────────────────┤
│ ☑ Enable Display                        │
│                                         │
│ Display Type:                           │
│ ┌─────────────────┐                    │
│ │ Sumlog         ▼│                    │
│ └─────────────────┘                    │
│                                         │
│ Wind Data Type:                         │
│ ┌─────────────────┐                    │
│ │ Apparent Wind  ▼│ ⓘ                 │
│ └─────────────────┘                    │
│   Options:                              │
│   • Apparent Wind                       │
│   • True Wind                           │
│   • VMG (Velocity Made Good)            │
│                                         │
│ [Save Settings]                         │
└─────────────────────────────────────────┘
```

**Tooltip** (ⓘ):
```
Apparent Wind: Wind relative to boat
True Wind: Calculated or from NMEA
VMG: Speed from GPS
```

---

## 📋 Käyttöesimerkit

### Esimerkki 1: Apparent Wind

1. Avaa: http://192.168.4.1/display1
2. Valitse: "Wind Data Type" → "Apparent Wind"
3. Tallenna
4. **Tulos**: Display näyttää MWV(R) tai VWR dataa

---

### Esimerkki 2: True Wind

1. Avaa: http://192.168.4.1/display2
2. Valitse: "Wind Data Type" → "True Wind"
3. Tallenna
4. **Tulos**: 
   - Jos on MWV(T) tai VWT → Käyttää suoraa dataa
   - Jos ei ole → Laskee Apparent + GPS

---

### Esimerkki 3: VMG

1. Avaa: http://192.168.4.1/display3
2. Valitse: "Wind Data Type" → "VMG (Velocity Made Good)"
3. Tallenna
4. **Tulos**: Display näyttää SOG:n GPS:stä

---

## 🔄 Backward Compatibility

### Vanhat Konfiguraatiot

**loadConfig()** migroi automaattisesti:
```cpp
if (prefs.isKey("d0_dataType")) {
  // Uusi formaatti
  displays[0].dataType = prefs.getUChar("d0_dataType");
} else {
  // Vanha formaatti: oletus Apparent Wind
  displays[0].dataType = DATA_APPARENT_WIND;
}
```

**Web UI** tukee molempia:
- Uusi: `dataType` (käytetään ensisijaisesti)
- Vanha: `sentence` (backward compatibility)

---

## 🎯 Hyödyt

### 1. Käyttäjäystävällisyys
- ✅ Ei tarvitse tietää NMEA-lauseita
- ✅ Selkeät valinnat: "Apparent Wind", "True Wind", "VMG"
- ✅ Tooltip-ohje jokaiselle vaihtoehdolle

### 2. Automaattinen Toiminta
- ✅ "True Wind" → Järjestelmä valitsee parhaan lähteen
- ✅ Jos ei suoraa dataa → Laskee automaattisesti
- ✅ Käyttäjän ei tarvitse huolehtia

### 3. Tulevaisuus
- ✅ Helppo lisätä uusia datatyyppejä
- ✅ Ground Wind, Current, jne.
- ✅ Valmis laskennalle

---

## 🧪 Testaus

### Web UI Testaus

1. **Avaa Web UI**: http://192.168.4.1
2. **Mene Display 1 -sivulle**
3. **Tarkista dropdown**:
   - Pitäisi näkyä: "Wind Data Type"
   - Vaihtoehdot: Apparent Wind, True Wind, VMG
4. **Valitse "True Wind"**
5. **Tallenna**
6. **Lataa sivu uudelleen**
7. **Tarkista**: Valinta säilynyt

### API Testaus

```bash
# Hae konfiguraatio
curl "http://192.168.4.1/api/display?num=1"

# Pitäisi palauttaa:
{
  "enabled": true,
  "type": "sumlog",
  "dataType": 1,  # True Wind
  "offsetDeg": 0,
  ...
}

# Aseta dataType
curl "http://192.168.4.1/api/display?num=1&dataType=1&save=1"
```

---

## 📝 Muutokset Tiedostoissa

### src/web_pages.cpp
- ✅ Lisätty `getDataTypeName()` ja `getDataTypeDescription()`
- ✅ Korvattu "NMEA Sentence" dropdown "Wind Data Type" dropdownilla
- ✅ Päivitetty JavaScript: `displayDataType` kenttä
- ✅ Päivitetty `saveDisplaySettings()` ja `loadDisplayValues()`

### src/web_ui.cpp
- ✅ Lisätty `dataType` JSON-responssiin
- ✅ Lisätty `dataType` parametrin käsittely POST-requestissa

### Ei muutettu
- ❌ Backend-logiikka (jo Phase 1:ssä)
- ❌ NMEA-parserit (jo Phase 2:ssa)
- ❌ Laskenta (jo Phase 2:ssa)

---

## ✅ Yhteenveto

**Muutos**: Tekninen "NMEA Sentence" → Käyttäjäystävällinen "Wind Data Type"

**Toteutus**:
- ✅ Dropdown päivitetty
- ✅ JavaScript päivitetty
- ✅ API päivitetty
- ✅ Backward compatibility

**Hyödyt**:
- ✅ Helpompi käyttää
- ✅ Ei teknistä jargonia
- ✅ Automaattinen toiminta

**Status**: ✅ Phase 3 valmis, kääntyy

**Branch**: `feature-per-display-data`

**Seuraava**: Testaa Web UI:lla! 🚀

---

## 🖼️ Kuvakaappaukset (Konsepti)

### Ennen
```
NMEA Sentence: [MWV ▼]
  - MWV (Wind Speed & Angle)
  - VWR (Relative Wind)
  - VWT (True Wind)
```
❌ Tekninen, vaikea ymmärtää

### Jälkeen
```
Wind Data Type: [Apparent Wind ▼] ⓘ
  - Apparent Wind
  - True Wind
  - VMG (Velocity Made Good)
```
✅ Selkeä, ymmärrettävä!

---

**Valmis testattavaksi!** 🎉
