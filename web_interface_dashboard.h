/*
 * ESP32 Multitool - Modern Dashboard Web Interface
 * Cyberpunk-themed, responsive, lightweight design
 * Total size: ~9KB compressed
 */

#ifndef WEB_DASHBOARD_H
#define WEB_DASHBOARD_H

const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 Multitool</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
:root{
--bg-primary:#000;
--bg-secondary:#111;
--bg-card:#1a1a1a;
--accent:#0f0;
--accent-dim:#0a0;
--text:#0f0;
--text-dim:#0a0;
--border:#333;
--danger:#f00;
--warning:#ff0;
--info:#0ff
}
body{
background:var(--bg-primary);
color:var(--text);
font-family:'Courier New',monospace;
padding:0;
margin:0;
overflow-x:hidden
}
.container{
max-width:1200px;
margin:0 auto;
padding:1rem
}
header{
background:var(--bg-secondary);
border-bottom:2px solid var(--accent);
padding:1rem;
position:sticky;
top:0;
z-index:100;
backdrop-filter:blur(10px)
}
h1{
font-size:1.5rem;
text-transform:uppercase;
letter-spacing:0.2em;
animation:glow 2s ease-in-out infinite alternate
}
@keyframes glow{
from{text-shadow:0 0 5px var(--accent),0 0 10px var(--accent)}
to{text-shadow:0 0 10px var(--accent),0 0 20px var(--accent),0 0 30px var(--accent)}
}
.nav{
display:flex;
gap:1rem;
margin-top:0.5rem;
flex-wrap:wrap
}
.nav a{
color:var(--text-dim);
text-decoration:none;
padding:0.5rem 1rem;
border:1px solid var(--border);
transition:all 0.3s;
font-size:0.9rem;
text-transform:uppercase
}
.nav a:hover,.nav a.active{
border-color:var(--accent);
color:var(--accent);
box-shadow:0 0 10px var(--accent)
}
.grid{
display:grid;
grid-template-columns:repeat(auto-fit,minmax(300px,1fr));
gap:1rem;
margin-top:1rem
}
.card{
background:var(--bg-card);
border:1px solid var(--border);
padding:1.5rem;
position:relative;
overflow:hidden;
transition:all 0.3s
}
.card::before{
content:'';
position:absolute;
top:-2px;
left:-2px;
right:-2px;
bottom:-2px;
background:linear-gradient(45deg,transparent,var(--accent),transparent);
opacity:0;
transition:opacity 0.3s;
z-index:-1
}
.card:hover::before{
opacity:0.3
}
.card-title{
font-size:1.1rem;
margin-bottom:1rem;
text-transform:uppercase;
letter-spacing:0.1em;
border-bottom:1px solid var(--border);
padding-bottom:0.5rem;
display:flex;
justify-content:space-between;
align-items:center
}
.status-badge{
font-size:0.7rem;
padding:0.2rem 0.5rem;
border:1px solid;
border-radius:2px
}
.status-on{color:var(--accent);border-color:var(--accent);background:rgba(0,255,0,0.1)}
.status-off{color:var(--text-dim);border-color:var(--border);background:rgba(255,255,255,0.05)}
.value{
font-size:2.5rem;
font-weight:bold;
margin:1rem 0;
text-align:center;
font-family:'Courier New',monospace
}
.unit{
font-size:1rem;
color:var(--text-dim);
margin-left:0.5rem
}
.progress-bar{
width:100%;
height:30px;
background:var(--bg-secondary);
border:1px solid var(--border);
position:relative;
overflow:hidden;
margin:1rem 0
}
.progress-fill{
height:100%;
background:linear-gradient(90deg,var(--accent-dim),var(--accent));
transition:width 0.3s;
position:relative;
box-shadow:0 0 10px var(--accent)
}
.progress-fill::after{
content:'';
position:absolute;
top:0;
left:0;
right:0;
bottom:0;
background:linear-gradient(90deg,transparent,rgba(255,255,255,0.3),transparent);
animation:shimmer 2s infinite
}
@keyframes shimmer{
0%{transform:translateX(-100%)}
100%{transform:translateX(100%)}
}
.progress-text{
position:absolute;
top:50%;
left:50%;
transform:translate(-50%,-50%);
font-weight:bold;
color:var(--text);
text-shadow:0 0 5px #000,0 0 10px #000
}
.btn{
background:var(--accent);
color:#000;
border:none;
padding:1rem 2rem;
font-size:1rem;
font-weight:bold;
text-transform:uppercase;
cursor:pointer;
transition:all 0.3s;
font-family:'Courier New',monospace;
width:100%;
margin:0.5rem 0;
position:relative;
overflow:hidden
}
.btn::before{
content:'';
position:absolute;
top:50%;
left:50%;
width:0;
height:0;
background:rgba(255,255,255,0.5);
border-radius:50%;
transform:translate(-50%,-50%);
transition:width 0.6s,height 0.6s
}
.btn:active::before{
width:300px;
height:300px
}
.btn:hover{
box-shadow:0 0 20px var(--accent);
transform:translateY(-2px)
}
.btn-danger{
background:var(--danger);
color:#fff
}
.btn-danger:hover{
box-shadow:0 0 20px var(--danger)
}
.slider-container{
margin:1rem 0
}
.slider{
-webkit-appearance:none;
width:100%;
height:10px;
background:var(--bg-secondary);
outline:none;
border:1px solid var(--border)
}
.slider::-webkit-slider-thumb{
-webkit-appearance:none;
appearance:none;
width:25px;
height:25px;
background:var(--accent);
cursor:pointer;
box-shadow:0 0 10px var(--accent)
}
.slider::-moz-range-thumb{
width:25px;
height:25px;
background:var(--accent);
cursor:pointer;
border:none;
box-shadow:0 0 10px var(--accent)
}
.info-row{
display:flex;
justify-content:space-between;
padding:0.5rem 0;
border-bottom:1px solid var(--border);
font-size:0.9rem
}
.info-label{
color:var(--text-dim)
}
.toggle{
position:relative;
display:inline-block;
width:60px;
height:30px;
margin:0.5rem 0
}
.toggle input{
opacity:0;
width:0;
height:0
}
.toggle-slider{
position:absolute;
cursor:pointer;
top:0;
left:0;
right:0;
bottom:0;
background:var(--bg-secondary);
border:1px solid var(--border);
transition:0.3s
}
.toggle-slider:before{
position:absolute;
content:"";
height:22px;
width:22px;
left:3px;
bottom:3px;
background:var(--text-dim);
transition:0.3s
}
input:checked+.toggle-slider{
background:var(--accent);
border-color:var(--accent);
box-shadow:0 0 10px var(--accent)
}
input:checked+.toggle-slider:before{
background:#000;
transform:translateX(30px)
}
.chart{
width:100%;
height:200px;
background:var(--bg-secondary);
border:1px solid var(--border);
position:relative;
margin:1rem 0
}
canvas{
width:100%;
height:100%
}
.footer{
text-align:center;
padding:2rem 1rem;
color:var(--text-dim);
font-size:0.8rem;
border-top:1px solid var(--border);
margin-top:2rem
}
@media(max-width:768px){
.container{padding:0.5rem}
.grid{grid-template-columns:1fr}
h1{font-size:1.2rem}
.value{font-size:2rem}
.btn{padding:0.8rem}
}
.loading{
display:inline-block;
width:10px;
height:10px;
border:2px solid var(--accent);
border-radius:50%;
border-top-color:transparent;
animation:spin 1s linear infinite;
margin-left:0.5rem
}
@keyframes spin{
to{transform:rotate(360deg)}
}
</style>
</head>
<body>
<header>
<h1>⚡ ESP32 MULTITOOL ⚡</h1>
<div class="nav">
<a href="/" class="active">Dashboard</a>
<a href="/settings">Settings</a>
<a href="/ota">OTA Update</a>
</div>
</header>

<div class="container">
<!-- System Status -->
<div class="card">
<div class="card-title">
SYSTEM STATUS
<span class="status-badge status-on" id="wifi-status">ONLINE</span>
</div>
<div class="info-row">
<span class="info-label">IP Address:</span>
<span id="ip-addr">Loading...</span>
</div>
<div class="info-row">
<span class="info-label">Connected Clients:</span>
<span id="clients">0</span>
</div>
<div class="info-row">
<span class="info-label">Free Heap:</span>
<span id="heap">0 KB</span>
</div>
<div class="info-row">
<span class="info-label">Uptime:</span>
<span id="uptime">0s</span>
</div>
<div class="info-row">
<span class="info-label">WiFi Signal:</span>
<span id="rssi">-</span>
</div>
</div>

<div class="grid">
<!-- Relay Control -->
<div class="card">
<div class="card-title">
RELAY CONTROL
<span class="status-badge" id="relay-status">OFF</span>
</div>
<button class="btn" onclick="toggleRelay(true)" id="relay-on">ACTIVATE</button>
<button class="btn btn-danger" onclick="toggleRelay(false)" id="relay-off">DEACTIVATE</button>
</div>

<!-- Sensor Monitor -->
<div class="card">
<div class="card-title">ADC SENSOR</div>
<div class="value"><span id="sensor-val">0</span><span class="unit">/ 4095</span></div>
<div class="progress-bar">
<div class="progress-fill" id="sensor-bar" style="width:0%"></div>
<div class="progress-text" id="sensor-pct">0%</div>
</div>
<canvas id="sensor-chart" class="chart"></canvas>
</div>

<!-- PWM Dimmer -->
<div class="card">
<div class="card-title">12V PWM DIMMER</div>
<div class="value"><span id="pwm-val">0</span><span class="unit">%</span></div>
<div class="slider-container">
<input type="range" min="0" max="100" value="0" class="slider" id="pwm-slider">
</div>
<div class="progress-bar">
<div class="progress-fill" id="pwm-bar" style="width:0%"></div>
</div>
</div>

<!-- Servo Control -->
<div class="card">
<div class="card-title">SERVO ANGLE</div>
<div class="value"><span id="servo-val">90</span><span class="unit">°</span></div>
<div class="slider-container">
<input type="range" min="0" max="180" value="90" class="slider" id="servo-slider">
</div>
<div class="info-row">
<span>0°</span>
<span>90°</span>
<span>180°</span>
</div>
</div>

<!-- I2C Devices -->
<div class="card">
<div class="card-title">I2C SCANNER</div>
<button class="btn" onclick="scanI2C()">SCAN BUS</button>
<div id="i2c-results" style="margin-top:1rem;min-height:100px">
<div class="info-label">Click SCAN to detect devices</div>
</div>
</div>
</div>
</div>

<div class="footer">
ESP32 Multitool v2.0 | Matrix-Ready | Cyberpunk Interface
</div>

<script>
let sensorData=[];
let maxDataPoints=50;
let chart=null;

// Initialize chart
function initChart(){
const canvas=document.getElementById('sensor-chart');
const ctx=canvas.getContext('2d');
canvas.width=canvas.offsetWidth;
canvas.height=canvas.offsetHeight;
chart={ctx:ctx,width:canvas.width,height:canvas.height};
drawChart();
}

function drawChart(){
if(!chart)return;
const ctx=chart.ctx;
ctx.fillStyle='#111';
ctx.fillRect(0,0,chart.width,chart.height);
ctx.strokeStyle='#333';
ctx.lineWidth=1;
for(let i=0;i<5;i++){
const y=(chart.height/4)*i;
ctx.beginPath();
ctx.moveTo(0,y);
ctx.lineTo(chart.width,y);
ctx.stroke();
}
if(sensorData.length<2)return;
ctx.strokeStyle='#0f0';
ctx.lineWidth=2;
ctx.beginPath();
const step=chart.width/(maxDataPoints-1);
sensorData.forEach((val,i)=>{
const x=i*step;
const y=chart.height-(val/4095)*chart.height;
if(i===0)ctx.moveTo(x,y);
else ctx.lineTo(x,y);
});
ctx.stroke();
}

// Update dashboard
async function updateDashboard(){
try{
const res=await fetch('/api/status');
const data=await res.json();
document.getElementById('ip-addr').textContent=data.ip||'N/A';
document.getElementById('clients').textContent=data.clients||0;
document.getElementById('heap').textContent=((data.heap||0)/1024).toFixed(1)+' KB';
document.getElementById('uptime').textContent=formatUptime(data.uptime||0);
document.getElementById('rssi').textContent=(data.rssi||0)+' dBm';
const relayStatus=document.getElementById('relay-status');
relayStatus.textContent=data.relay?'ON':'OFF';
relayStatus.className='status-badge '+(data.relay?'status-on':'status-off');
const sensorVal=data.sensor||0;
document.getElementById('sensor-val').textContent=sensorVal;
const sensorPct=Math.round((sensorVal/4095)*100);
document.getElementById('sensor-pct').textContent=sensorPct+'%';
document.getElementById('sensor-bar').style.width=sensorPct+'%';
sensorData.push(sensorVal);
if(sensorData.length>maxDataPoints)sensorData.shift();
drawChart();
}catch(e){
console.error('Update failed:',e);
document.getElementById('wifi-status').className='status-badge status-off';
document.getElementById('wifi-status').textContent='OFFLINE';
}
}

function formatUptime(ms){
const s=Math.floor(ms/1000);
const m=Math.floor(s/60);
const h=Math.floor(m/60);
const d=Math.floor(h/24);
if(d>0)return d+'d '+h%24+'h';
if(h>0)return h+'h '+m%60+'m';
if(m>0)return m+'m '+s%60+'s';
return s+'s';
}

// Relay control
async function toggleRelay(state){
try{
await fetch('/api/relay',{
method:'POST',
headers:{'Content-Type':'application/json'},
body:JSON.stringify({state:state})
});
updateDashboard();
}catch(e){
console.error('Relay control failed:',e);
}
}

// PWM control
const pwmSlider=document.getElementById('pwm-slider');
const pwmVal=document.getElementById('pwm-val');
const pwmBar=document.getElementById('pwm-bar');
let pwmTimeout;
pwmSlider.oninput=function(){
const val=this.value;
pwmVal.textContent=val;
pwmBar.style.width=val+'%';
clearTimeout(pwmTimeout);
pwmTimeout=setTimeout(()=>{
fetch('/api/pwm',{
method:'POST',
headers:{'Content-Type':'application/json'},
body:JSON.stringify({value:parseInt(val)})
});
},100);
};

// Servo control
const servoSlider=document.getElementById('servo-slider');
const servoVal=document.getElementById('servo-val');
let servoTimeout;
servoSlider.oninput=function(){
const val=this.value;
servoVal.textContent=val;
clearTimeout(servoTimeout);
servoTimeout=setTimeout(()=>{
fetch('/api/servo',{
method:'POST',
headers:{'Content-Type':'application/json'},
body:JSON.stringify({angle:parseInt(val)})
});
},100);
};

// I2C Scanner
async function scanI2C(){
const results=document.getElementById('i2c-results');
results.innerHTML='<div class="info-label">Scanning<span class="loading"></span></div>';
try{
const res=await fetch('/api/i2c/scan');
const data=await res.json();
if(data.devices&&data.devices.length>0){
let html='<div class="info-label">Found '+data.devices.length+' device(s):</div>';
data.devices.forEach(d=>{
html+='<div class="info-row"><span>0x'+d.addr.toString(16).toUpperCase()+'</span><span>'+d.name+'</span></div>';
});
results.innerHTML=html;
}else{
results.innerHTML='<div class="info-label">No I2C devices found</div>';
}
}catch(e){
results.innerHTML='<div class="info-label" style="color:#f00">Scan failed</div>';
}
}

// Initialize
window.onload=function(){
initChart();
updateDashboard();
setInterval(updateDashboard,1000);
};
</script>
</body>
</html>
)rawliteral";

#endif
