/*
 * ESP32 Multitool - Settings Page
 * Password management, MQTT, network configuration
 */

#ifndef WEB_SETTINGS_H
#define WEB_SETTINGS_H

const char SETTINGS_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Settings - ESP32 Multitool</title>
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
--danger:#f00
}
body{
background:var(--bg-primary);
color:var(--text);
font-family:'Courier New',monospace;
padding:0;
margin:0
}
.container{
max-width:800px;
margin:0 auto;
padding:1rem
}
header{
background:var(--bg-secondary);
border-bottom:2px solid var(--accent);
padding:1rem;
margin-bottom:2rem
}
h1{
font-size:1.5rem;
text-transform:uppercase;
letter-spacing:0.2em
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
.card{
background:var(--bg-card);
border:1px solid var(--border);
padding:2rem;
margin-bottom:2rem
}
.card-title{
font-size:1.2rem;
margin-bottom:1.5rem;
text-transform:uppercase;
letter-spacing:0.1em;
border-bottom:1px solid var(--border);
padding-bottom:0.5rem
}
.form-group{
margin-bottom:1.5rem
}
label{
display:block;
margin-bottom:0.5rem;
color:var(--text-dim);
text-transform:uppercase;
font-size:0.9rem
}
input[type="text"],input[type="password"],input[type="number"]{
width:100%;
padding:0.8rem;
background:var(--bg-secondary);
border:1px solid var(--border);
color:var(--text);
font-family:'Courier New',monospace;
font-size:1rem;
transition:all 0.3s
}
input:focus{
outline:none;
border-color:var(--accent);
box-shadow:0 0 10px rgba(0,255,0,0.3)
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
margin-top:1rem
}
.btn:hover{
box-shadow:0 0 20px var(--accent);
transform:translateY(-2px)
}
.btn-danger{
background:var(--danger);
color:#fff
}
.btn-secondary{
background:var(--bg-secondary);
color:var(--text);
border:1px solid var(--border)
}
.alert{
padding:1rem;
margin-bottom:1rem;
border:1px solid;
display:none
}
.alert-success{
border-color:var(--accent);
background:rgba(0,255,0,0.1);
color:var(--accent)
}
.alert-error{
border-color:var(--danger);
background:rgba(255,0,0,0.1);
color:var(--danger)
}
.help-text{
font-size:0.8rem;
color:var(--text-dim);
margin-top:0.3rem
}
.footer{
text-align:center;
padding:2rem 1rem;
color:var(--text-dim);
font-size:0.8rem;
border-top:1px solid var(--border)
}
@media(max-width:768px){
.container{padding:0.5rem}
.card{padding:1rem}
h1{font-size:1.2rem}
}
</style>
</head>
<body>
<header>
<h1>⚙️ SETTINGS</h1>
<div class="nav">
<a href="/">Dashboard</a>
<a href="/settings" class="active">Settings</a>
<a href="/ota">OTA Update</a>
</div>
</header>

<div class="container">
<div id="alert" class="alert"></div>

<!-- Password Settings -->
<div class="card">
<div class="card-title">🔐 Security</div>
<form id="password-form" onsubmit="return updatePassword(event)">
<div class="form-group">
<label>Current Password</label>
<input type="password" id="current-pass" required>
</div>
<div class="form-group">
<label>New Web Password</label>
<input type="password" id="new-pass" minlength="8" required>
<div class="help-text">Minimum 8 characters</div>
</div>
<div class="form-group">
<label>New OTA Password</label>
<input type="password" id="ota-pass" minlength="8" required>
<div class="help-text">For over-the-air firmware updates</div>
</div>
<button type="submit" class="btn">UPDATE PASSWORDS</button>
</form>
</div>

<!-- MQTT Settings -->
<div class="card">
<div class="card-title">📡 MQTT Configuration</div>
<form id="mqtt-form" onsubmit="return updateMQTT(event)">
<div class="form-group">
<label>MQTT Broker</label>
<input type="text" id="mqtt-server" value="broker.hivemq.com" required>
<div class="help-text">Server hostname or IP address</div>
</div>
<div class="form-group">
<label>Port</label>
<input type="number" id="mqtt-port" value="1883" min="1" max="65535" required>
</div>
<div class="form-group">
<label>Client ID</label>
<input type="text" id="mqtt-client" value="ESP32_Multitool" required>
</div>
<div class="form-group">
<label>Username (optional)</label>
<input type="text" id="mqtt-user">
</div>
<div class="form-group">
<label>Password (optional)</label>
<input type="password" id="mqtt-pass">
</div>
<button type="submit" class="btn">SAVE MQTT SETTINGS</button>
</form>
</div>

<!-- Network Settings -->
<div class="card">
<div class="card-title">🌐 Network</div>
<div class="form-group">
<label>WiFi SSID</label>
<input type="text" id="wifi-ssid" readonly>
</div>
<div class="form-group">
<label>IP Address</label>
<input type="text" id="wifi-ip" readonly>
</div>
<div class="form-group">
<label>MAC Address</label>
<input type="text" id="wifi-mac" readonly>
</div>
<button class="btn btn-secondary" onclick="loadNetworkInfo()">REFRESH INFO</button>
</div>

<!-- System Actions -->
<div class="card">
<div class="card-title">⚠️ System Actions</div>
<button class="btn btn-secondary" onclick="resetWiFi()">RESET WIFI SETTINGS</button>
<button class="btn btn-danger" onclick="rebootDevice()">REBOOT DEVICE</button>
<div class="help-text" style="margin-top:1rem">
Warning: Resetting WiFi will restart the device in configuration mode
</div>
</div>
</div>

<div class="footer">
ESP32 Multitool v2.0 | Secure Configuration Interface
</div>

<script>
function showAlert(message,type){
const alert=document.getElementById('alert');
alert.textContent=message;
alert.className='alert alert-'+type;
alert.style.display='block';
setTimeout(()=>{alert.style.display='none'},5000);
}

async function updatePassword(e){
e.preventDefault();
const current=document.getElementById('current-pass').value;
const newPass=document.getElementById('new-pass').value;
const otaPass=document.getElementById('ota-pass').value;
try{
const res=await fetch('/api/password',{
method:'POST',
headers:{'Content-Type':'application/json'},
body:JSON.stringify({current,newpass:newPass,otapass:otaPass})
});
if(res.ok){
showAlert('Passwords updated successfully','success');
document.getElementById('password-form').reset();
}else{
showAlert('Failed to update passwords: '+(await res.text()),'error');
}
}catch(e){
showAlert('Error: '+e.message,'error');
}
return false;
}

async function updateMQTT(e){
e.preventDefault();
const config={
server:document.getElementById('mqtt-server').value,
port:parseInt(document.getElementById('mqtt-port').value),
client:document.getElementById('mqtt-client').value,
user:document.getElementById('mqtt-user').value,
pass:document.getElementById('mqtt-pass').value
};
try{
const res=await fetch('/api/mqtt',{
method:'POST',
headers:{'Content-Type':'application/json'},
body:JSON.stringify(config)
});
if(res.ok){
showAlert('MQTT settings saved','success');
}else{
showAlert('Failed to save MQTT settings','error');
}
}catch(e){
showAlert('Error: '+e.message,'error');
}
return false;
}

async function loadNetworkInfo(){
try{
const res=await fetch('/api/network');
const data=await res.json();
document.getElementById('wifi-ssid').value=data.ssid||'N/A';
document.getElementById('wifi-ip').value=data.ip||'N/A';
document.getElementById('wifi-mac').value=data.mac||'N/A';
}catch(e){
showAlert('Failed to load network info','error');
}
}

async function resetWiFi(){
if(!confirm('Reset WiFi settings? Device will reboot in configuration mode.')){
return;
}
try{
await fetch('/api/wifi/reset',{method:'POST'});
showAlert('WiFi reset. Device rebooting...','success');
setTimeout(()=>{window.location.href='/'},3000);
}catch(e){
showAlert('Error: '+e.message,'error');
}
}

async function rebootDevice(){
if(!confirm('Reboot device now?')){
return;
}
try{
await fetch('/api/reboot',{method:'POST'});
showAlert('Device rebooting...','success');
setTimeout(()=>{window.location.href='/'},10000);
}catch(e){
showAlert('Error: '+e.message,'error');
}
}

window.onload=loadNetworkInfo;
</script>
</body>
</html>
)rawliteral";

#endif
