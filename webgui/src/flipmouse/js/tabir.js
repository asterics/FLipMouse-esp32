window.tabIR = {};

tabIR.initIR = function () {
    /*var slots = flip.getSlots();
    L('#delete-slot-button').disabled = slots.length <= 1;
    L('#create-slot-button').disabled = true;
    L('.slot-select').forEach(function (elem) {
        elem.innerHTML = L.createSelectItems(slots);
        elem.value = flip.getCurrentSlot();
    });*/
};

window.tabIR.saveSlotLabelChanged = function (element) {
    L('#create-slot-button').disabled = !element.value;
};

window.tabIR.createSlot = function (toggleElementList, progressBarId) {
	///TODO
    /*var slotName = L('#newSlotLabelEn') ? L('#newSlotLabelEn').value : L('#newSlotLabelDe').value;
    actionAndToggle(flip.createSlot, [slotName], toggleElementList, progressBarId).then(function () {
        tabSlot.initSlots();
        L.setValue('#newSlotLabelEn', '');
        L.setValue('#newSlotLabelDe', '');
    });*/
};

window.tabIR.deleteSlot = function (toggleElementList, progressBarId) {
	///TODO...
    /*var slotName = L('#selectSlotDelete').value;
    var confirmMessage = L.translate('CONFIRM_DELETE_IR', slotName);
    if(!window.confirm(confirmMessage)){
        return;
    }
    actionAndToggle(flip.deleteSlot, [slotName], toggleElementList, progressBarId).then(function () {
        tabSlot.initSlots();
        L.setValue('#newSlotLabelEn', '');
        L.setValue('#newSlotLabelDe', '');
    });*/
};

window.tabIR.resetConfig = function (toggleElementList, progressBarId) {
    var confirmMessage = L.translate('CONFIRM_DELETE_ALL_IR');
    if(!window.confirm(confirmMessage)){
        return;
    }
    actionAndToggle(flip.deleteAllIR, [], toggleElementList, progressBarId);
};
