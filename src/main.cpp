#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>

// ============================================================================
// PIN DEFINITIONS
// ============================================================================
const int MAIN_ROTOR_LEFT = 25;   // PWM pin for left main rotor
const int MAIN_ROTOR_RIGHT = 26;  // PWM pin for right main rotor
const int TAIL_ROTOR_CW = 32;     // PWM pin for clockwise rotation
const int TAIL_ROTOR_CCW = 27;    // PWM pin for counterclockwise rotation

// ============================================================================
// PWM CONFIGURATION
// ============================================================================
const int PWM_FREQUENCY = 10000;  // 10 kHz
const int PWM_RESOLUTION = 8;     // 8-bit (0-255)
const int PWM_CHANNEL_LEFT = 0;
const int PWM_CHANNEL_RIGHT = 1;
const int PWM_CHANNEL_TAIL_CW = 2;
const int PWM_CHANNEL_TAIL_CCW = 3;

// ============================================================================
// AUTOLAND SAFETY CONFIGURATION
// ============================================================================
const unsigned long CONNECTION_TIMEOUT = 3000;  // 3s without client => autoland
const int AUTOLAND_DECREMENT = 12;              // reduce speed step (in 0..255 units)
const unsigned long AUTOLAND_CHECK_INTERVAL = 200;  // ms

// ============================================================================
// CONTROL VARIABLES
// ============================================================================
uint8_t main_rotor_speed = 0;     // 0-255
int8_t tail_rotor_position = 0;   // -127..127 (negative=CCW, positive=CW)
int8_t pitch_control = 0;         // -100..100
int8_t roll_control = 0;          // -100..100
uint8_t main_rotor_trim = 128;    // 0..255 where 128=neutral (NO TRIM). 
                                   // Left motor speed = main_speed + offset(trim)
                                   // Right motor speed = main_speed - offset(trim)

// Connection / autoland
bool wsClientConnected = false;      // Track if WebSocket client is actually connected
bool autoLandActive = false;
unsigned long lastAutolandCheck = 0;

Preferences preferences;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Forward declarations
void setupWiFi();
void setupPWM();
void setupWebServer();
void loadTrim();
void saveTrim();
void setMainRotorSpeed(uint8_t speed);
void setTailRotorPosition(int8_t position);
void handleControlMessage(const String &msg);

// Helper: parse an int value from a small JSON-like string (fast, lightweight)
bool parseJsonIntField(const String &body, const char *key, int &value) {
    String needle = String("\"") + key + "\"";
    int keyIndex = body.indexOf(needle);
    if (keyIndex < 0) return false;
    int colon = body.indexOf(':', keyIndex + needle.length());
    if (colon < 0) return false;
    int i = colon + 1;
    while (i < (int)body.length() && isspace(body[i])) i++;
    int start = i;
    if (i < (int)body.length() && body[i] == '-') i++;
    while (i < (int)body.length() && isDigit(body[i])) i++;
    if (i == start) return false;
    value = body.substring(start, i).toInt();
    return true;
}

// Embedded HTML UI (WebSocket client)
const char* htmlContent = R"rawliteral(
<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no, viewport-fit=cover">
<title>Helicopter Transmitter</title>
<style>
:root{--bg:#0c1016;--panel:#1b222c;--accent:#00e676;--muted:#92a0af;--text:#e8edf2}
html,body{height:100%;margin:0;padding:0;background:linear-gradient(180deg,#0b0f14,#020305);-webkit-user-select:none;touch-action:none}
body{display:flex;align-items:center;justify-content:center;font-family:Arial,Helvetica,sans-serif;color:var(--text)}
.container{width:min(960px,calc(100% - 16px));border-radius:14px;padding:12px;background:linear-gradient(180deg,#13171b,#0b0d10);box-shadow:0 20px 60px rgba(0,0,0,0.6);transform:scale(0.7);transform-origin:top center;margin:0 auto}
.header{text-align:center;color:var(--accent);font-weight:800;margin-bottom:8px}
.metrics{display:grid;grid-template-columns:repeat(5,1fr);gap:8px;margin-bottom:10px}
.metric{background:rgba(255,255,255,0.02);padding:8px;border-radius:8px;text-align:center;font-size:12px;color:var(--muted)}
.controls{display:flex;gap:12px}
.panel{flex:1;background:linear-gradient(180deg,#10161b,#071014);padding:10px;border-radius:12px;border:1px solid rgba(255,255,255,0.03)}
.gimbal{touch-action:none;user-select:none;position:relative;margin:8px auto;border-radius:50%;width:260px;height:260px;background:radial-gradient(circle at 30% 30%,rgba(255,255,255,0.03),transparent 20%)}
.left .gimbal{width:86px;height:280px;border-radius:14px}
.stick{position:absolute;left:50%;top:50%;transform:translate(-50%,-50%);width:54px;height:54px;border-radius:50%;background:radial-gradient(circle at 30% 30%,#7dffb1,#00e676);box-shadow:0 6px 18px rgba(0,0,0,0.6)}
.label{font-size:12px;color:var(--muted);text-align:center}
.footer{margin-top:8px;text-align:center;font-family:monospace;color:var(--accent)}
.yaw-row{display:flex;align-items:center;gap:8px;margin-top:6px}
input[type=range]{-webkit-appearance:none;height:28px;background:transparent;width:100%}
@media (max-width:680px){.controls{flex-direction:column}}
</style>
</head>
<body>
<div class="container">
  <div class="header">HELICOPTER TRANSMITTER</div>
  <div class="metrics">
    <div class="metric">THROTTLE<br><strong id="throttleVal">0%</strong></div>
    <div class="metric">YAW<br><strong id="yawVal">0</strong></div>
    <div class="metric">MAIN TRIM<br><strong id="trimVal">50%</strong></div>
    <div class="metric">PITCH<br><strong id="pitchVal">0</strong></div>
    <div class="metric">ROLL<br><strong id="rollVal">0</strong></div>
  </div>
  <div class="controls">
    <div class="panel left">
      <div class="label" style="text-align:center;font-weight:700">LEFT STICK<br>Throttle (non-centering)</div>
      <div class="gimbal" id="leftG"><div class="stick" id="leftS"></div></div>
    </div>
    <div class="panel">
      <div class="label" style="text-align:center;font-weight:700">RIGHT STICK<br>Pitch / Roll (centering)</div>
      <div class="gimbal" id="rightG"><div class="stick" id="rightS"></div></div>
      <div class="yaw-row"><div style="flex:1;color:var(--muted)">Yaw</div><input id="yaw" type="range" min="-100" max="100" value="0"></div>
    </div>
  </div>
  <div style="display:flex;gap:8px;margin-top:10px;justify-content:center">
    <button id="trimDown">MAIN -</button>
    <button id="trimUp">MAIN +</button>
    <button id="saveTrim">Save Trim</button>
    <button id="reset">Reset</button>
  </div>
  <div class="footer" id="status">DISCONNECTED</div>
</div>

<script>
// Lightweight control client using WebSocket for low latency
let ws=null;
let state={throttle:0,yaw:0,pitch:0,roll:0,trim:50};
const leftG=document.getElementById('leftG'), rightG=document.getElementById('rightG');
const leftS=document.getElementById('leftS'), rightS=document.getElementById('rightS');
const throttleVal=document.getElementById('throttleVal');
const yawVal=document.getElementById('yawVal');
const trimVal=document.getElementById('trimVal');
const pitchVal=document.getElementById('pitchVal');
const rollVal=document.getElementById('rollVal');
const status=document.getElementById('status');
const yawSlider=document.getElementById('yaw');

function clamp(v,a,b){return v<a?a:(v>b?b:v)}
function setStick(el,x,y){el.style.transform=`translate(calc(-50% + ${x}px), calc(-50% + ${y}px))`}

// Send updates via ws (throttled using rAF)
let pending=false;
function sendState(){
  if (!ws || ws.readyState!==1) return;
  const msg = JSON.stringify({type:'control',throttle:state.throttle,yaw:state.yaw,pitch:state.pitch,roll:state.roll,trim:state.trim});
  ws.send(msg);
}
function scheduleSend(){ if (pending) return; pending=true; requestAnimationFrame(()=>{ sendState(); pending=false; }); }

// Pointer / touch handlers
function attachG(g,mode){
  let active=false;
  function pointerMove(clientX,clientY){
    const r=g.getBoundingClientRect();
    const cx=r.left+r.width/2, cy=r.top+r.height/2;
    const x=clientX-cx, y=clientY-cy;
    if (mode==='throttle'){
      // vertical slider: map top->100 bottom->0
      const max=r.height/2; let vy = clamp(-y/max, -1, 1); // -1..1
      let pct = Math.round(((vy+1)/2)*100); // 0..100
      state.throttle = clamp(pct,0,100);
      state.pitch = 0; state.roll = 0; // no change from this stick
    } else {
      const radius = Math.min(r.width,r.height)/2 - 26;
      const dist=Math.sqrt(x*x+y*y); const scale= dist>radius?radius/dist:1;
      const nx=x*scale, ny=y*scale;
      state.roll = Math.round(clamp((nx/radius)*100,-100,100));
      state.pitch = Math.round(clamp((-ny/radius)*100,-100,100));
    }
    updateUI(); scheduleSend();
  }
  g.addEventListener('pointerdown', e=>{ e.preventDefault(); active=true; g.setPointerCapture && g.setPointerCapture(e.pointerId); pointerMove(e.clientX,e.clientY); }, {passive:false});
  g.addEventListener('pointermove', e=>{ if(!active) return; e.preventDefault(); pointerMove(e.clientX,e.clientY); }, {passive:false});
  g.addEventListener('pointerup', e=>{ active=false; if(mode!=='throttle'){ state.pitch=0; state.roll=0; updateUI(); scheduleSend(); } }, {passive:false});
  // touch fallback
  g.addEventListener('touchstart', e=>{ e.preventDefault(); const t=e.touches[0]; pointerMove(t.clientX,t.clientY); }, {passive:false});
  g.addEventListener('touchmove', e=>{ e.preventDefault(); const t=e.touches[0]; pointerMove(t.clientX,t.clientY); }, {passive:false});
  g.addEventListener('touchend', e=>{ e.preventDefault(); if(mode!=='throttle'){ state.pitch=0; state.roll=0; updateUI(); scheduleSend(); } }, {passive:false});
}

attachG(leftG,'throttle'); attachG(rightG,'direction');

// Yaw slider
yawSlider.addEventListener('input', e=>{ state.yaw=parseInt(e.target.value); updateUI(); scheduleSend(); });

// Trim buttons
document.getElementById('trimUp').addEventListener('click', ()=>{ state.trim=Math.min(100,state.trim+5); updateUI(); scheduleSend(); });
document.getElementById('trimDown').addEventListener('click', ()=>{ state.trim=Math.max(0,state.trim-5); updateUI(); scheduleSend(); });
document.getElementById('reset').addEventListener('click', ()=>{ state={throttle:0,yaw:0,pitch:0,roll:0,trim:50}; updateUI(); scheduleSend(); status.textContent='RESET'; setTimeout(()=>status.textContent=ws&&ws.readyState===1?'CONNECTED':'DISCONNECTED',1000); });
document.getElementById('saveTrim').addEventListener('click', ()=>{ if(ws&&ws.readyState===1) ws.send(JSON.stringify({type:'saveTrim',trim:state.trim})); });

function updateUI(){ throttleVal.textContent=state.throttle+"%"; yawVal.textContent=state.yaw; trimVal.textContent=state.trim+"%"; pitchVal.textContent=state.pitch; rollVal.textContent=state.roll; // stick positions
  const lrect=leftG.getBoundingClientRect(); const ly = (1 - state.throttle/100)*lrect.height - lrect.height/2; setStick(leftS,0,ly);
  const rrect=rightG.getBoundingClientRect(); const radius=Math.min(rrect.width,rrect.height)/2 - 26; setStick(rightS,(state.roll/100)*radius,(-state.pitch/100)*radius);
}

// WebSocket connection
function connectWS(){
  const proto = (location.protocol === 'https:') ? 'wss' : 'ws';
  const host = location.host; // includes port when non-standard
  ws = new WebSocket(proto + '://' + host + '/ws');
  ws.addEventListener('open', ()=>{ status.textContent='CONNECTED'; lastMsg('connected'); });
  ws.addEventListener('close', ()=>{ status.textContent='DISCONNECTED'; setTimeout(connectWS,1000); });
  ws.addEventListener('message', ev=>{
    try{ const m=JSON.parse(ev.data); if(m.type==='status'){ /* could update UI with telemetry */ } }
    catch(e){}
  });
}

function lastMsg(m){ console.log(m); }

// prevent default scrolling
window.addEventListener('touchmove', e=>{ e.preventDefault(); }, {passive:false});

// initial UI
updateUI(); connectWS();
</script>
</body>
</html>
)rawliteral";

void setupPreferences() {
  preferences.begin("helicopter", false);
}

void loadTrim() {
  // Load trim from NVS. Default to 128 (neutral: left=right, no differential)
  main_rotor_trim = preferences.getUChar("trim_main", 128);
  int trimPercent = map(main_rotor_trim, 0, 255, 0, 100);
  Serial.printf("Loaded trim - Main: %d (%%: %d)\n", main_rotor_trim, trimPercent);
}

void saveTrim() {
  preferences.putUChar("trim_main", main_rotor_trim);
  int trimPercent = map(main_rotor_trim, 0, 255, 0, 100);
  Serial.printf("Trim saved to NVS: %d (%%: %d)\n", main_rotor_trim, trimPercent);
}

void setupWiFi() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("RC_Helicopter_Control", "12345678", 1, false, 4);
  IPAddress IP = WiFi.softAPIP();
  Serial.println("\n=== WiFi Hotspot Started ===");
  Serial.print("SSID: RC_Helicopter_Control\n");
  Serial.print("Password: 12345678\n");
  Serial.print("IP Address: ");
  Serial.println(IP);
  Serial.println("Open browser to http://192.168.4.1");
}

void setupPWM() {
  ledcSetup(PWM_CHANNEL_LEFT, PWM_FREQUENCY, PWM_RESOLUTION);
  ledcSetup(PWM_CHANNEL_RIGHT, PWM_FREQUENCY, PWM_RESOLUTION);
  ledcSetup(PWM_CHANNEL_TAIL_CW, PWM_FREQUENCY, PWM_RESOLUTION);
  ledcSetup(PWM_CHANNEL_TAIL_CCW, PWM_FREQUENCY, PWM_RESOLUTION);
  ledcAttachPin(MAIN_ROTOR_LEFT, PWM_CHANNEL_LEFT);
  ledcAttachPin(MAIN_ROTOR_RIGHT, PWM_CHANNEL_RIGHT);
  ledcAttachPin(TAIL_ROTOR_CW, PWM_CHANNEL_TAIL_CW);
  ledcAttachPin(TAIL_ROTOR_CCW, PWM_CHANNEL_TAIL_CCW);
  Serial.println("PWM configured");
}

void setMainRotorSpeed(uint8_t speed) {
  // apply trim differential: main_rotor_trim 0..255 => -trimRange..+trimRange
  int trimPercent = map(main_rotor_trim, 0, 255, 0, 100);
  int trimOffset = trimPercent - 50; // -50..50
  int offset = map(trimOffset, -50, 50, -20, 20);
  int left = (int)speed + offset;
  int right = (int)speed - offset;
  
  // Apply roll-based motor reduction (smooth proportional control)
  // Right stick right (roll > 0): reduce right motor
  // Right stick left (roll < 0): reduce left motor
  // Reduction recovers smoothly as stick returns to center
  if (roll_control > 0) {
    // right stick right -> reduce right motor smoothly (up to 25% reduction)
    int reduction = map(roll_control, 0, 100, 0, speed / 4);
    right = constrain(right - reduction, 0, 255);
  } else if (roll_control < 0) {
    // right stick left -> reduce left motor smoothly (up to 25% reduction)
    int reduction = map(-roll_control, 0, 100, 0, speed / 4);
    left = constrain(left - reduction, 0, 255);
  }
  
  left = constrain(left, 0, 255);
  right = constrain(right, 0, 255);
  ledcWrite(PWM_CHANNEL_LEFT, left);
  ledcWrite(PWM_CHANNEL_RIGHT, right);
  main_rotor_speed = speed;
}

void setTailRotorPosition(int8_t position) {
  int pos = constrain((int)position, -127, 127);
  tail_rotor_position = pos;
  // Proportional PWM control: apply PWM according to input value
  // Positive position (up) -> CW rotation (GPIO 32)
  // Negative position (down) -> CCW rotation (GPIO 23)
  if (pos > 5) {
    // Clockwise: apply PWM proportional to position value
    uint8_t pwm = map(pos, 0, 127, 0, 255);
    ledcWrite(PWM_CHANNEL_TAIL_CW, pwm);
    ledcWrite(PWM_CHANNEL_TAIL_CCW, 0);
    Serial.printf("Tail CW: pos=%d, GPIO32_PWM=%d, GPIO23_PWM=0\n", pos, pwm);
  } else if (pos < -5) {
    // Counter-clockwise (reverse): apply PWM proportional to negative position
    uint8_t pwm = map(-pos, 0, 127, 0, 255);
    ledcWrite(PWM_CHANNEL_TAIL_CCW, pwm);
    ledcWrite(PWM_CHANNEL_TAIL_CW, 0);
    Serial.printf("Tail CCW: pos=%d, GPIO23_PWM=%d, GPIO32_PWM=0\n", pos, pwm);
  } else {
    // Idle: both off
    ledcWrite(PWM_CHANNEL_TAIL_CW, 0);
    ledcWrite(PWM_CHANNEL_TAIL_CCW, 0);
    Serial.printf("Tail IDLE: both motors off\n");
  }
}

void handleControlMessage(const String &msg) {
  // parse fields: throttle (0..100), pitch, roll, trim
  // Tail motor controlled by pitch (right stick up/down)
  int v;
  if (parseJsonIntField(msg, "throttle", v)) {
    uint8_t duty = map(constrain(v, 0, 100), 0, 100, 0, 255);
    setMainRotorSpeed(duty);
  }
  if (parseJsonIntField(msg, "pitch", v)) {
    pitch_control = constrain(v, -100, 100);
    // Right stick up/down directly controls tail rotor motor
    // Up (positive) = CW (GPIO 32), Down (negative) = CCW (GPIO 23)
    int8_t tail = (int8_t)map(constrain(v, -100, 100), -100, 100, -127, 127);
    Serial.printf("DEBUG: Pitch=%d -> Tail=%d\n", v, tail);
    setTailRotorPosition(tail);
  }
  if (parseJsonIntField(msg, "roll", v)) {
    roll_control = constrain(v, -100, 100);
  }
  if (parseJsonIntField(msg, "yaw", v)) {
    // Yaw slider reserved for future use
  }
  if (parseJsonIntField(msg, "trim", v)) {
    main_rotor_trim = (uint8_t)map(constrain(v, 0, 100), 0, 100, 0, 255);
  }
}

void onWsEvent(AsyncWebSocket *serverW, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("WS: client %u connected\n", client->id());
    wsClientConnected = true;
    autoLandActive = false;
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("WS: client %u disconnected - starting autoland\n", client->id());
    wsClientConnected = false;
    autoLandActive = true;  // Trigger autoland immediately on disconnect
    lastAutolandCheck = millis();
  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->opcode == WS_TEXT) {
      String msg = String((char*)data, len);
      // handle simple control or saveTrim
      if (msg.indexOf("\"type\":\"saveTrim\"") >= 0) {
        saveTrim();
      } else {
        handleControlMessage(msg);
      }
      // Reset autoland timer on any message (while connected, no autoland)
      if (wsClientConnected && autoLandActive) {
        autoLandActive = false;
        Serial.println("WebSocket activity detected - canceling autoland");
      }
    }
  }
}

void setupWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){ request->send(200, "text/html", htmlContent); });
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.begin();
  Serial.println("Web server + WebSocket started");
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\nESP32 Helicopter Controller - starting\n");
  setupPreferences(); loadTrim();
  setupPWM(); setupWiFi(); setupWebServer();
  // initialize motors
  setMainRotorSpeed(0);
  setTailRotorPosition(0);
}

void loop() {
  unsigned long now = millis();
  // Autoland: only runs after WebSocket disconnect (not input timeout)
  if (autoLandActive && (now - lastAutolandCheck) > AUTOLAND_CHECK_INTERVAL) {
    lastAutolandCheck = now;
    if (main_rotor_speed > AUTOLAND_DECREMENT) {
      uint8_t newSpeed = main_rotor_speed - AUTOLAND_DECREMENT;
      setMainRotorSpeed(newSpeed);
      Serial.printf("Autoland: speed -> %d%%\n", map(newSpeed,0,255,0,100));
    } else {
      setMainRotorSpeed(0);
      setTailRotorPosition(0);  // also center tail during autoland
      autoLandActive = false;
      Serial.println("Autoland complete - motors stopped");
    }
  }
  // keep loop tight for responsiveness
}
