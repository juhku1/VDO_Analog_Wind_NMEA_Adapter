#include "web_ui.h"

// Yksinkertainen HTML-sivu ESP32:n muistissa
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
  <head>
    <meta charset="utf-8">
    <title>Wind Calibrator</title>
    <style>
      body { font-family: sans-serif; padding: 2em; }
      input { margin: 0.5em 0; padding: 0.4em; width: 60px; }
    </style>
  </head>
  <body>
    <h1>Wind Calibrator</h1>
    <form action="/save" method="POST">
      <label>Offset: <input name="offset" type="number" step="0.1"></label><br>
      <label>Gain: <input name="gain" type="number" step="0.01"></label><br>
      <input type="submit" value="Save">
    </form>
    <form action="/freeze" method="POST">
      <button type="submit">Freeze Angle</button>
    </form>
  </body>
</html>
)rawliteral";

// Asetukset (Preferences) talletetaan esim. nimellä "windcfg"
void setupWebUI(WebServer& server, Preferences& prefs) {
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", HTML_PAGE);
  });

  server.on("/save", HTTP_POST, [&server, &prefs]() {
    if (server.hasArg("offset") && server.hasArg("gain")) {
      float offset = server.arg("offset").toFloat();
      float gain = server.arg("gain").toFloat();
      prefs.putFloat("offset", offset);
      prefs.putFloat("gain", gain);
      server.send(200, "text/plain", "Saved OK");
    } else {
      server.send(400, "text/plain", "Missing fields");
    }
  });

  server.on("/freeze", HTTP_POST, [&server]() {
    // Aseta jokin globaali tila tai lippu freeze-toiminnolle
    server.send(200, "text/plain", "Freeze triggered");
  });
}
