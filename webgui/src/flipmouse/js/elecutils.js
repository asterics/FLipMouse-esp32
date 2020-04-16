window.electronUtils = {};

//guard this whole file for running in electron
if (C.IS_ELECTRON) {
	
var ps = require('ps-node');
var storage = require('electron-json-storage');
storage.setDataPath(require('os').homedir());
console.log("Looking for config json here: " + storage.getDataPath());
const activeWindow = require('active-win');

var processActivationMapping;

//check every 500ms, if a window is active, which should load a slot.
setInterval(function() {
	if (processActivationMapping) {
		(async () => {
			var windowName = await activeWindow();
			var processPath = windowName.owner.path;
			for (var i = 0; i < processActivationMapping.length; i++) {
				if(processPath === processActivationMapping[i].process)
				{
					//load this slot, but only if not already active...
					if(flip.getCurrentSlot() !== processActivationMapping[i]["slot"])
					{
						var config = flip.setSlot(processActivationMapping[i]["slot"]);
						L('.slot-select').forEach(function (elem) {
							elem.value = processActivationMapping[i]["slot"];
						});
						tabAction.init();
						tabSlot.initProcesses();
						initWithConfig(config);
						console.log("loading slot " + processActivationMapping[i]["slot"] +", because of process " + processPath);
					}
					break;
				}
			}
		})();
	}
}, 500);

window.electronUtils.updateSlotAndProcess = function (slotname, processname) {
	//check if mapping is available, if yes: update current list
	if (processActivationMapping) {
		//find active slot in list
		var index = processActivationMapping.findIndex(item => item.slot === slotname);
		//if index found -> update
		if(index !== -1) processActivationMapping[index]["process"] = processname;
		//if not found -> add new entry into mapping array
		else processActivationMapping = processActivationMapping.concat([{slot: slotname, process: processname}]);
		//store updated list into JSON file
		storage.set(C.ELECTRON_CONFIG_FILENAME,processActivationMapping);
	} else {
	//if no -> set a new list
		processActivationMapping = [{slot: slotname, process: processname}]
		storage.set(C.ELECTRON_CONFIG_FILENAME,processActivationMapping);
	}
};

window.electronUtils.getProcessListForSlot = function (slotname) {
	//active element of process list (for GUI)
	var active = "";
	//active process list
	var processes;
	//if we got a list back (JSON file exists & contains data)
	if (processActivationMapping) {
		//find active slot in list
		var index = processActivationMapping.findIndex(item => item.slot === slotname);
		//if index found -> set name of process
		if(index !== -1) active = processActivationMapping[index]["process"];
	}

	return new Promise(resolve => {
        ps.lookup({}, function(err, resultList ) {
			if (err) {
				throw new Error( err );
			}
			//sort list descending by PID (lists recently opened programs first)
			resultList.sort(function(a, b) {
				return parseInt(b.pid) - parseInt(a.pid);
			});
			//we just need process names
			resultList = resultList.map(a => a.command);
			//if we have an activation process, add to the beginning of the list
			resultList.unshift(active);
			//remove duplicates (we trigger on one process name anyway and the list is shorter)
			resultList = resultList.filter((v, i, a) => a.indexOf(v) === i);
			//add an "unselected" item at the beginning
			if(active !== "") resultList.unshift("");
			//now return: process list & active process
			resolve({ processList: resultList , activationProcess: active });
		});

	}); 
};

//load process mapping from JSON file when loading this JS file
storage.get(C.ELECTRON_CONFIG_FILENAME,function(error, storedProcessMapping) {
	processActivationMapping = storedProcessMapping;
});

}
