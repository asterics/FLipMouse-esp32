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


window.tabSlot.selectSlot = function (select) {
    var config = flip.setSlot(select.value);
    L('.slot-select').forEach(function (elem) {
        elem.value = select.value;
    });
    tabAction.init();
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
