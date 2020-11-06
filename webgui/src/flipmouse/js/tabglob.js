var fs = require('fs');
var http = require('https');
var tmp = require('temporary');
var os = require('os');

window.tabGlobal = {};

window.tabGlobal.updateVersionTeensy = "";

tabGlobal.showCheckmark = function (btn) {
    L.setVisible(btn.children[1], true, 'inline');
    setTimeout(function () {
        L.setVisible(btn.children[1], false);
    }, 3000);
};

var download = function(url, dest, cb) {
  var file = fs.createWriteStream(dest);
  var request = http.get(url, function(response) {
	response.pipe(file);
	file.on('finish', function() {
	  file.close(cb);  // close() is async, call cb after close completes.
	});
  }).on('error', function(err) { // Handle errors
	fs.unlink(dest); // Delete the file async. (But we don't check the result)
	if (cb) cb(err.message);
  });
};


window.tabGlobal.init = function () {
	var backColor = false;
	var ariaTeensyL = '<span class="hidden" aria-hidden="false">' + L.translate('TEENSY_VERSION_CURRENT') + '</span>';
    var ariaTeensyW = '<span class="hidden" aria-hidden="false">' + L.translate('TEENSY_VERSION_INSTALLABLE') + '</span>';
    var ariaBTL = '<span class="hidden" aria-hidden="false">' + L.translate('BT_VERSION_CURRENT') + '</span>';
    var ariaBTW = '<span class="hidden" aria-hidden="false">' + L.translate('BT_VERSION_INSTALLABLE') + '</span>';
	
	//add versions for FLipMouse Teensy Board	
	var xhr = new XMLHttpRequest();
    xhr.open('GET', 'https://api.github.com/repos/asterics/FLipMouse/releases/latest', true);
    xhr.responseType = 'json';
    xhr.onload = function() {
		var status = xhr.status;
		var version;
		
		//parse tagname from GitHub response
		if (status === 200) {
			version = xhr.response['tag_name'];
		} else {
			version = '???';
		}
		window.tabGlobal.updateVersionTeensy = version;
		
		//build up list entry.
		var liElm = L.createElement('li', 'row', null, backColor ? 'background-color: #e0e0e0;' : null);
		var descriptionDiv = L.createElement('div', 'two columns', 'Mainboard');
		var currentVersion = L.createElement('div', 'four columns', [ariaTeensyL, flip.VERSION]);
		var availableVersion = L.createElement('div', 'four columns', [ariaTeensyW, version]);
		var spacerDiv = L.createElement('div', 'one column show-mobile space-bottom');
		liElm.appendChild(descriptionDiv);
		liElm.appendChild(currentVersion);
		liElm.appendChild(availableVersion);
		liElm.appendChild(spacerDiv);
		L('#updatesTb').appendChild(liElm);
		backColor = !backColor;
	};
    xhr.send();
    
	//add versions for Bluetooth Board	
	var xhr2 = new XMLHttpRequest();
    xhr2.open('GET', 'https://api.github.com/repos/asterics/esp32_mouse_keyboard/releases/latest', true);
    xhr2.responseType = 'json';
    xhr2.onload = function() {
		var status = xhr2.status;
		var version;
		
		//parse tagname from GitHub response
		if (status === 200) {
			version = xhr2.response['tag_name'];
		} else {
			version = '???';
		}
		
		//build up list entry.
		var liElm = L.createElement('li', 'row', null, backColor ? 'background-color: #e0e0e0;' : null);
		var descriptionDiv = L.createElement('div', 'two columns', 'Bluetooth');
		
		//TODO: load BT board version....
		var currentVersion = L.createElement('div', 'four columns', [ariaBTL, 'TBD: read via AT cmd']);
		var availableVersion = L.createElement('div', 'four columns', [ariaBTW, version]);
		var spacerDiv = L.createElement('div', 'one column show-mobile space-bottom');
		liElm.appendChild(descriptionDiv);
		liElm.appendChild(currentVersion);
		liElm.appendChild(availableVersion);
		liElm.appendChild(spacerDiv);
		L('#updatesTb').appendChild(liElm);
		backColor = !backColor;
	};
    xhr2.send();
}


window.tabGlobal.UpdateTeensy = async function (elem) {
	//check where we should get the binary from
	//TBD: currently fixed to internet.
	var tmpfile = new tmp.Dir();
	/*var url = "https://github.com/asterics/FLipMouse/releases/download/"+window.tabGlobal.updateVersionTeensy+"/FlipMouse.zip";
	console.log(url);
	download(url, tmpfile.path,
		function(msg) {
			if(msg) console.log(msg);
			else console.log(tmpfile.path);
			//unzip
			
			
	});*/
	
	//source: https://javascript.info/fetch-progress
	// Step 1: start the fetch and obtain a reader
	let response = await fetch("https://github.com/asterics/FLipMouse/releases/download/"+ window.tabGlobal.updateVersionTeensy +"/FLipWare.ino.hex");

	const reader = response.body.getReader();

	// Step 2: get total length
	const contentLength = +response.headers.get('Content-Length');

	// Step 3: read the data
	let receivedLength = 0; // received that many bytes at the moment
	let chunks = []; // array of received binary chunks (comprises the body)
	var myFile = fs.createWriteStream(tmpfile.path+'/FLipWare.ino.hex');
	while(true) {
	  const {done, value} = await reader.read();

	  if (done) {
		await myFile.end();
		//check which binary we need...
		var binary = "";
		switch(os.platform()) {
			case "linux": binary = "./tycmd"; break;
			case "win32": binary = "tycmd.exe"; break;
			case "darwin": binary = "./tycmd.mac"; break;
		}
		let spawn = require("child_process").spawn;
		let bat = spawn(binary, [
			"upload",          // we want to upload
			tmpfile.path+'/FLipWare.ino.hex' // this hex file
		]);

		bat.stdout.on("data", (data) => {
			//console.log(data);
		});

		bat.stderr.on("data", (err) => {
			console.log("ERR: " + err);
		});

		bat.on("exit", (code) => {
			console.log("EXIT: " + code);
		});

		break;
	  }

	  myFile.write(value);
	  //chunks.push(value);
	  receivedLength += value.length;

	  console.log(`Received ${receivedLength} of ${contentLength}`)
	}
}

window.tabGlobal.UpdateBluetooth = function (elem) {
	//TODO: change path to either internet or selected file.<
	var filepath = '/home/beni/Projects/FLipMouse-esp32-asterics/webgui/src/flipmouse/hidd_demos.bin';
	var fileContent = fs.createReadStream(filepath,{ highWaterMark: 1 * 32 });
	
	//issue addon update request -> Teensy switches to USB->UART passthrough
	//for the bootloader from the addon.
	flip.sendATCmd(C.AT_CMD_UPGRADE_ADDON).then(function(response) {
		//disable all other AT commands
		//will be re-enabled if the drain event sendRawData is emitted
		flip.inRawMode = true;
		//read all at once & send to the raw data sending function.
		fs.readFile(filepath, (err, data) => {
			flip.sendRawData(data,20000);
		});
	});
}


