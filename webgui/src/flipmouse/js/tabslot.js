window.tabSlot = {};

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
	if(C.IS_ELECTRON) {
		L.setVisible('#processSelectionDiv', true);
		//get current process list & active process for given slot
		electronUtils.getProcessListForSlot(flip.getCurrentSlot()).then(list => {
			L('.process-select').forEach(function (elem) {
				elem.innerHTML = L.createSelectItems(list.processList);
				elem.value = list.activationProcess;
			});
		});
	}
};

window.tabSlot.selectProcess = function (select) {
	if(!C.IS_ELECTRON) return;
	electronUtils.updateSlotAndProcess(flip.getCurrentSlot(), select.value);
	L('.process-select').forEach(function (elem) {
		elem.value = select.value;
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

window.tabSlot.uploadSlot = function () {
	
	var file = L('#selectSlotUpload').files;
    var reader=new FileReader();
    reader.readAsText(file[0]);
    reader.onloadend = function(e) {
		flip.parseConfig(e.target.result);
		tabSlot.initSlots();
	};
};

window.tabSlot.downloadSlot = function () {
    var slotName = L('#selectSlotDelete').value;
    var d = new Date();
    var datestr = d.getDate() + "." + (d.getMonth()+1) + "." + d.getFullYear()
    downloadasTextFile(slotName + "-" + datestr + ".set",flip.getSlotConfigText(slotName));
};

window.tabSlot.downloadAllSlots = function () {
	var configstr = "";
	flip.getSlots().forEach( function(item) {
		configstr = configstr + flip.getSlotConfigText(item) + "\n";
	});
    var d = new Date();
    var datestr = d.getDate() + "." + (d.getMonth()+1) + "." + d.getFullYear()
    downloadasTextFile("flipmouseconfig-" + datestr + ".set",configstr);
};

//THX: https://phpcoder.tech/wp-content/cache/all/create-dynamically-generated-text-file-and-download-using-javascript/index.html
function downloadasTextFile(filename, text) {
    var element = document.createElement('a');
    element.setAttribute('href', 'data:text/plain;charset=utf-8,' + encodeURIComponent(text));
    element.setAttribute('download', filename);

    element.style.display = 'none';
    document.body.appendChild(element);

    element.click();

    document.body.removeChild(element);
}

window.tabSlot.resetConfig = function (toggleElementList, progressBarId) {
    var confirmMessage = L.translate('CONFIRM_RESET_SLOTS');
    if(!window.confirm(confirmMessage)){
        return;
    }
    actionAndToggle(flip.restoreDefaultConfiguration, [], toggleElementList, progressBarId);
};
