window.tabSlot = {};

//require is only useful in electron.
var inElectron = false;
var activateOnProcess;

//check every second, if a process is active, which should load a slot.
setInterval(function() {
	if (typeof activateOnProcess !== 'undefined') {
		//load list of currently running processes
		ps.lookup({}, function(err, resultList ) {
			if (err) {
				throw new Error( err );
			}
			//we just need process names
			resultList = resultList.map(a => a.command);
			//remove duplicates (we trigger on one process name anyway and the list is shorter for iter)
			resultList = resultList.filter((v, i, a) => a.indexOf(v) === i);
			//compare list of slots/process names against currently running ones
			for (var i = 0; i < activateOnProcess.length; i++) {
				var index = resultList.findIndex(function(item, j){
					return item === activateOnProcess[i]["process"];
				});
				if(index !== -1)
				{
					//load this slot, but only if not already active...
					if(flip.getCurrentSlot() !== activateOnProcess[i]["slot"])
					{
						var config = flip.setSlot(activateOnProcess[i]["slot"]);
						L('.slot-select').forEach(function (elem) {
							elem.value = activateOnProcess[i]["slot"];
						});
						tabAction.init();
						tabSlot.initProcesses();
						initWithConfig(config);
						console.log("loading slot " + activateOnProcess[i]["slot"] +", because of process " + resultList[index]);
					}
					break;
				}
			}
		});
	}
}, 1000);

if (navigator.userAgent.toLowerCase().indexOf(' electron/') > -1) {
	var inElectron = true;
	var ps = require('ps-node');
	var storage = require('electron-json-storage');
}

window.tabSlot.selectPort = function (select) {
	_serialport = select.value;
	console.log("Selected port: " + _serialport);
	//TODO: set port in communicator
	L('.port-select').forEach(function (elem) {
		elem.value = select.value;
	});
};
    
tabSlot.initPorts = function () {
    //TODO: get ports from communicator, but how?!?
    if(window.flip._communicator) {
        var slots = window.flip._communicator.getPorts();
    } else {
        console.log("_communicator not available");
        return;
    }
    //TODO: en/disable connect/disconnect buttons
    /*L('#delete-slot-button').disabled = slots.length <= 1;
    L('#create-slot-button').disabled = true;*/
    L('.port-select').forEach(function (elem) {
        elem.innerHTML = L.createSelectItems(slots);
    });
};

tabSlot.initSlots = function () {
    var slots = flip.getSlots();
    L('#delete-slot-button').disabled = slots.length <= 1;
    L('#create-slot-button').disabled = true;
    L('.slot-select').forEach(function (elem) {
        elem.innerHTML = L.createSelectItems(slots);
        elem.value = flip.getCurrentSlot();
    });
};

tabSlot.initProcesses = function () {
	if(inElectron === false) {
		var resultList = ["not available / nicht verfügbar"];
		//add the process list to the GUI
		L('.process-select').forEach(function (elem) {
			elem.innerHTML = L.createSelectItems(resultList);
		});
		return;
	}
	
	storage.setDataPath(require('os').homedir());
	console.log("Looking for config json here: " + storage.getDataPath());
	
	//get an active process if one is set for this slot
	storage.get('FLipMouseGUIConfiguration',function(error, processActivationList) {
		//if we got a list back (JSON file exists & contains data)
		if (typeof processActivationList !== 'undefined') {
			//find active slot in list
			var index = processActivationList.findIndex(function(item, i){
				 return item.slot === flip.getCurrentSlot()
			});
			activateOnProcess = processActivationList;
			//if index found -> set name of process
			if(index !== -1) active = processActivationList[index]["process"];
			else active = "";
		} else active = "";
		
		ps.lookup({}, function(err, resultList ) {
			if (err) {
				throw new Error( err );
			}
			//sort list descending by PID (lists recently opened programs first)
			resultList.sort(function(a, b) {
				return parseInt(b.pid) - parseInt(a.pid);
			});
			//note: on Linux/Mac the first entry is the ps command -> do not list.
			//resultList.shift();
			//we just need process names
			resultList = resultList.map(a => a.command);
			//if we have an activation process, add to the beginning of the list
			resultList.unshift(active);
			//remove duplicates (we trigger on one process name anyway and the list is shorter)
			resultList = resultList.filter((v, i, a) => a.indexOf(v) === i);
			//add an "unselected" item at the beginning
			if(active !== "") resultList.unshift("");
			
			//add the process list to the GUI
			L('.process-select').forEach(function (elem) {
				elem.innerHTML = L.createSelectItems(resultList);
				elem.value = active;
			});
		});
	});
	//TODO: load config into runner...
};

window.tabSlot.selectProcess = function (select) {
	if(inElectron === false) return;
	
	storage.get('FLipMouseGUIConfiguration',function(error, processActivationList) {
		var newlist = [{slot: flip.getCurrentSlot(), process: select.value}];
		if (typeof processActivationList !== 'undefined') {
				//find active slot in list
				var index = processActivationList.findIndex(function(item, i){
					 return item.slot === flip.getCurrentSlot()
				});
				//if index found -> update
				if(index !== -1) processActivationList[index]["process"] = select.value;
				//else: concat new entry into array
				else processActivationList = processActivationList.concat(newlist);
				storage.set('FLipMouseGUIConfiguration',processActivationList);
				activateOnProcess = processActivationList;
		} else {
			storage.set('FLipMouseGUIConfiguration',newlist);
		}
	    L('.process-select').forEach(function (elem) {
			elem.value = select.value;
		});
	});
};


window.tabSlot.selectSlot = function (select) {
    var config = flip.setSlot(select.value);
    L('.slot-select').forEach(function (elem) {
        elem.value = select.value;
    });
    tabAction.init();
    tabSlot.initProcesses();
    initWithConfig(config);
};

window.tabSlot.saveSlotLabelChanged = function (element) {
    L('#create-slot-button').disabled = !element.value;
};

window.tabSlot.createSlot = function (toggleElementList, progressBarId) {
    var slotName = L('#newSlotLabelEn') ? L('#newSlotLabelEn').value : L('#newSlotLabelDe').value;
    actionAndToggle(flip.createSlot, [slotName], toggleElementList, progressBarId).then(function () {
        tabSlot.initSlots();
        L.setValue('#newSlotLabelEn', '');
        L.setValue('#newSlotLabelDe', '');
    });
};

window.tabSlot.deleteSlot = function (toggleElementList, progressBarId) {
    var slotName = L('#selectSlotDelete').value;
    var confirmMessage = L.translate('CONFIRM_DELETE_SLOT', slotName);
    if(!window.confirm(confirmMessage)){
        return;
    }
    actionAndToggle(flip.deleteSlot, [slotName], toggleElementList, progressBarId).then(function () {
        tabSlot.initSlots();
        L.setValue('#newSlotLabelEn', '');
        L.setValue('#newSlotLabelDe', '');
    });
};

window.tabSlot.resetConfig = function (toggleElementList, progressBarId) {
    var confirmMessage = L.translate('CONFIRM_RESET_SLOTS');
    if(!window.confirm(confirmMessage)){
        return;
    }
    actionAndToggle(flip.restoreDefaultConfiguration, [], toggleElementList, progressBarId);
};
