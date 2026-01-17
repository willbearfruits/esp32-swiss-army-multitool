/*
 * ESP32 Multitool - OTA Update Page
 * File upload interface for firmware updates
 */

#ifndef WEB_OTA_H
#define WEB_OTA_H

const char OTA_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>OTA Update - ESP32 Multitool</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
:root{
--bg-primary:#000;
--bg-secondary:#111;
--bg-card:#1a1a1a;
--accent:#0f0;
--text:#0f0;
--text-dim:#0a0;
--border:#333;
--warning:#ff0;
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
.warning-box{
background:rgba(255,255,0,0.1);
border:1px solid var(--warning);
color:var(--warning);
padding:1rem;
margin-bottom:2rem;
font-size:0.9rem
}
.file-input-wrapper{
position:relative;
overflow:hidden;
display:inline-block;
width:100%;
margin:1rem 0
}
.file-input-wrapper input[type=file]{
position:absolute;
left:-9999px
}
.file-input-label{
display:block;
padding:1rem;
background:var(--bg-secondary);
border:2px dashed var(--border);
text-align:center;
cursor:pointer;
transition:all 0.3s;
text-transform:uppercase
}
.file-input-label:hover{
border-color:var(--accent);
box-shadow:0 0 10px rgba(0,255,0,0.3)
}
.file-info{
margin:1rem 0;
padding:0.5rem;
background:var(--bg-secondary);
border:1px solid var(--border);
display:none
}
.progress-container{
width:100%;
height:40px;
background:var(--bg-secondary);
border:1px solid var(--border);
position:relative;
margin:2rem 0;
display:none
}
.progress-bar{
height:100%;
background:linear-gradient(90deg,var(--accent),#0ff);
width:0%;
transition:width 0.3s;
position:relative;
box-shadow:0 0 20px var(--accent)
}
.progress-text{
position:absolute;
top:50%;
left:50%;
transform:translate(-50%,-50%);
font-weight:bold;
font-size:1.2rem;
text-shadow:0 0 5px #000
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
margin-top:1rem;
display:none
}
.btn:hover:not(:disabled){
box-shadow:0 0 20px var(--accent);
transform:translateY(-2px)
}
.btn:disabled{
opacity:0.5;
cursor:not-allowed
}
.status-message{
padding:1rem;
margin:1rem 0;
border:1px solid;
text-align:center;
display:none;
animation:pulse 2s infinite
}
@keyframes pulse{
0%,100%{opacity:1}
50%{opacity:0.7}
}
.status-success{
border-color:var(--accent);
background:rgba(0,255,0,0.1);
color:var(--accent)
}
.status-error{
border-color:var(--danger);
background:rgba(255,0,0,0.1);
color:var(--danger)
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
.footer{
text-align:center;
padding:2rem 1rem;
color:var(--text-dim);
font-size:0.8rem;
border-top:1px solid var(--border)
}
</style>
</head>
<body>
<header>
<h1>🔄 OTA UPDATE</h1>
<div class="nav">
<a href="/">Dashboard</a>
<a href="/settings">Settings</a>
<a href="/ota" class="active">OTA Update</a>
</div>
</header>

<div class="container">
<div class="warning-box">
⚠️ WARNING: Do not disconnect power during update. Device will reboot automatically after upload completes.
</div>

<!-- Current Firmware Info -->
<div class="card">
<div class="card-title">📦 Current Firmware</div>
<div class="info-row">
<span class="info-label">Version:</span>
<span>v2.0.0</span>
</div>
<div class="info-row">
<span class="info-label">Build Date:</span>
<span id="build-date">Loading...</span>
</div>
<div class="info-row">
<span class="info-label">Sketch Size:</span>
<span id="sketch-size">Loading...</span>
</div>
<div class="info-row">
<span class="info-label">Free Space:</span>
<span id="free-space">Loading...</span>
</div>
</div>

<!-- Upload Form -->
<div class="card">
<div class="card-title">📤 Upload New Firmware</div>
<form id="upload-form">
<div class="file-input-wrapper">
<input type="file" id="file-input" accept=".bin" onchange="handleFileSelect(event)">
<label for="file-input" class="file-input-label">
<span>📁 CLICK TO SELECT .BIN FILE</span>
</label>
</div>
<div id="file-info" class="file-info"></div>
<button type="submit" id="upload-btn" class="btn">🚀 START UPDATE</button>
</form>
<div id="progress-container" class="progress-container">
<div id="progress-bar" class="progress-bar"></div>
<div id="progress-text" class="progress-text">0%</div>
</div>
<div id="status-message" class="status-message"></div>
</div>
</div>

<div class="footer">
ESP32 Multitool v2.0 | Secure OTA Updates via HTTPS
</div>

<script>
let selectedFile=null;

async function loadFirmwareInfo(){
try{
const res=await fetch('/api/firmware');
const data=await res.json();
document.getElementById('build-date').textContent=data.buildDate||'Unknown';
document.getElementById('sketch-size').textContent=formatBytes(data.sketchSize||0);
document.getElementById('free-space').textContent=formatBytes(data.freeSpace||0);
}catch(e){
console.error('Failed to load firmware info',e);
}
}

function formatBytes(bytes){
if(bytes<1024)return bytes+' B';
if(bytes<1048576)return(bytes/1024).toFixed(1)+' KB';
return(bytes/1048576).toFixed(1)+' MB';
}

function handleFileSelect(e){
const file=e.target.files[0];
if(!file)return;
if(!file.name.endsWith('.bin')){
alert('Please select a .bin firmware file');
return;
}
selectedFile=file;
document.getElementById('file-info').innerHTML=
'<strong>Selected:</strong> '+file.name+' ('+formatBytes(file.size)+')';
document.getElementById('file-info').style.display='block';
document.getElementById('upload-btn').style.display='block';
}

document.getElementById('upload-form').onsubmit=async function(e){
e.preventDefault();
if(!selectedFile){
alert('Please select a file first');
return;
}
const uploadBtn=document.getElementById('upload-btn');
const progressContainer=document.getElementById('progress-container');
const progressBar=document.getElementById('progress-bar');
const progressText=document.getElementById('progress-text');
const statusMessage=document.getElementById('status-message');
uploadBtn.disabled=true;
progressContainer.style.display='block';
statusMessage.style.display='none';
const formData=new FormData();
formData.append('firmware',selectedFile);
try{
const xhr=new XMLHttpRequest();
xhr.upload.addEventListener('progress',function(e){
if(e.lengthComputable){
const percentComplete=(e.loaded/e.total)*100;
progressBar.style.width=percentComplete+'%';
progressText.textContent=Math.round(percentComplete)+'%';
}
});
xhr.addEventListener('load',function(){
if(xhr.status===200){
progressBar.style.width='100%';
progressText.textContent='100%';
statusMessage.textContent='✓ UPDATE SUCCESSFUL - REBOOTING...';
statusMessage.className='status-message status-success';
statusMessage.style.display='block';
setTimeout(function(){
window.location.href='/';
},10000);
}else{
statusMessage.textContent='✗ UPDATE FAILED: '+xhr.responseText;
statusMessage.className='status-message status-error';
statusMessage.style.display='block';
uploadBtn.disabled=false;
}
});
xhr.addEventListener('error',function(){
statusMessage.textContent='✗ UPLOAD ERROR - CHECK CONNECTION';
statusMessage.className='status-message status-error';
statusMessage.style.display='block';
uploadBtn.disabled=false;
});
xhr.open('POST','/update');
xhr.send(formData);
}catch(e){
statusMessage.textContent='✗ ERROR: '+e.message;
statusMessage.className='status-message status-error';
statusMessage.style.display='block';
uploadBtn.disabled=false;
}
};

window.onload=loadFirmwareInfo;
</script>
</body>
</html>
)rawliteral";

#endif
