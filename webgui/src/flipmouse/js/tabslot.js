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
	if(!C.IS_ELECTRON) {
		var resultList = ["not available / nicht verfügbar"];
		//add the process list to the GUI
		L('.process-select').forEach(function (elem) {
			elem.innerHTML = L.createSelectItems(resultList);
		});
		return;
	}
	//get current process list & active process for given slot
	electronUtils.getProcessListForSlot(flip.getCurrentSlot()).then(list => {
		L('.process-select').forEach(function (elem) {
			elem.innerHTML = L.createSelectItems(list.processList);
			elem.value = list.activationProcess;
		});
	}); 
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

window.tabSlot.resetConfig = function (toggleElementList, progressBarId) {
    var confirmMessage = L.translate('CONFIRM_RESET_SLOTS');
    if(!window.confirm(confirmMessage)){
        return;
    }
    actionAndToggle(flip.restoreDefaultConfiguration, [], toggleElementList, progressBarId);
};
