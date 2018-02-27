window.tabSlot = {};
window.tabSlot.selectSlot = function (select) {
    var config = flip.setSlot(select.value);
    L('.slot-select').forEach(function (elem) {
        elem.value = select.value;
    });
    initWithConfig(config);
};

window.tabSlot.saveSlotLabelChanged = function (element) {
    L('#create-slot-button').disabled = !element.value;
};

window.tabSlot.createSlot = function (toggleElementList, progressBarId) {
    var slotName = L('#newSlotLabel').value;
    actionAndToggle(flip.createSlot, [slotName], toggleElementList, progressBarId).then(function () {
        initSlots();
        L('#newSlotLabel').value = '';
    });
};

window.tabSlot.deleteSlot = function (toggleElementList, progressBarId) {
    var slotName = L('#selectSlotDelete').value;
    var confirmMessage = L.translate('Do you really want to delete the slot "' + slotName + '"?', 'Möchten Sie den Slot "' + slotName + '" wirklich löschen?');
    if(!window.confirm(confirmMessage)){
        return;
    }
    actionAndToggle(flip.deleteSlot, [slotName], toggleElementList, progressBarId).then(function () {
        initSlots();
        L('#newSlotLabel').value = '';
    });
};

window.tabSlot.resetConfig = function (toggleElementList, progressBarId) {
    var confirmMessage = L.translate('Do you really want to reset the FLipMouse to the default configuration?', 'Möchten Sie die FLipMouse wirklich auf die Standardeinstellungen zurücksetzen?');
    if(!window.confirm(confirmMessage)){
        return;
    }
    actionAndToggle(flip.restoreDefaultConfiguration, [], toggleElementList, progressBarId);
};