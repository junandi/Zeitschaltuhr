console.log("loading js...");
//extracting the IP from address line for WebSocket Connection
var url = window.location.href;
var params = url.split('/');
//for(i=0;i<params.length;i++){window.alert(i + ':' + params[i]);}
var sIP = params[2].split(':');
//for(i=0;i<sIP.length;i++){window.alert(i + ':' + sIP[i]);}
//setting up WebSocket connection
var connection = new WebSocket('ws://' + sIP[0] + ':81/', ['arduino']);
console.log('Trying to connect to: ' + 'ws://' + sIP[0] + ':81/');
//window.alert('ws://' + sIP[0] + ':81/');
//var connection = new WebSocket('http://192.168.2.116:81/', ['arduino']);
///*
connection.onopen = function () {
    console.log("Connection is open!")
    connection.send('Connect ' + new Date());
    document.getElementById("active").addEventListener("click",sendTimerState);
    document.getElementById("btn1").addEventListener("click",sendSOn);//connection.send('S1'));
    document.getElementById("btn2").addEventListener("click",sendSOff);//connection.send('S0'));
    document.getElementById("dok").addEventListener("click",updateDate);
    document.getElementById("tok").addEventListener("click",updateTime);
    document.getElementById("honok").addEventListener("click",updateOnHour);
    document.getElementById("hoffok").addEventListener("click",updateOffHour);
    document.getElementById("refresh").addEventListener("click",requestUpdate);
};
connection.onerror = function (error) {console.log('WebSocket Error ', error);};
connection.onmessage = function (e) {
	console.log("Server: ", e.data);
	if (IsJsonString(e.data)) {
		console.log("JSON");
		var msg = JSON.parse(e.data);
        //looking for switch state update
        console.log("Switch is On: ", msg.S);
        var s = "ndf";
        if(msg.S){
            s = "Ein";
        }else{
            s = "Aus";
        }
        document.getElementById('status').innerHTML = s;
        //looking for timer state update
        console.log("Timer is active: ", msg.T);
        var act = document.getElementById('active');
        if(msg.T){
            act.checked = true;
        }
        else{
            act.checked = false;
        }
        //looking for hour On update
        console.log("Time On: ", msg.hOn);
        document.getElementById('hOn').value = msg.hOn;
        //looking for hour Off update
        console.log("Time Off: ", msg.hOff);
        document.getElementById('hOff').value = msg.hOff;
        //looking for Time update
        console.log("Time: ", msg.time);
        document.getElementById('t').value = msg.time;
        //looking for Date update
        console.log("Date: ", msg.date);
        document.getElementById('d').value = msg.date;
        document.getElementById("refresh").className = "icon-spinner11";
	}
	else {
		console.log("NOT JSON");
	}
};
connection.onclose = function (e) {
    console.log('Connection closed - Server: ', e.data);
    //IssueSystemMessage("WS-");
};
//*/

function requestUpdate(){
    document.getElementById("refresh").className = "icon-loop2";
    console.log("Update requested!")
    connection.send("Plz");
}

function sendSOn(){
    console.log("On button clicked!")
    connection.send("S1");
}
function sendSOff(){
    console.log("Off button clicked!")
    connection.send("S0");
}

function updateDate(){
    var d = document.getElementById("d");
    console.log('ND' + d.value);
    connection.send('ND' + d.value);
}
function updateTime(){
    var t = document.getElementById("t");
    console.log('NT' + t.value);
    connection.send('NT' + t.value);
}
function updateOnHour(){
    var hOn = document.getElementById("hOn");
    console.log('NHA' + hOn.value);
    connection.send('NHA' + hOn.value);
}
function updateOffHour(){
    var hOff = document.getElementById("hOff");
    console.log('NHB' + hOff.value);
    connection.send('NHB' + hOff.value);
}
function sendTimerState(){
    if(document.getElementById("active").checked){connection.send('T1');console.log("T1");}else{connection.send('T0');console.log("T0");}
}

function IsJsonString(str) {
    try {
        JSON.parse(str);
    }
    catch (e) {
        return false;
    }
    return true;
}
var SystemMessageTimeout = null;

console.log("js loaded...");
