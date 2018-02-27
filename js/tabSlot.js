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
    flipActionThenToggle(flip.createSlot, [slotName], toggleElementList, progressBarId).then(function () {
        initSlots();
        L('#newSlotLabel').value = '';
    });
};

window.tabSlot.deleteSlot = function (toggleElementList, progressBarId) {
    var slotName = L('#selectSlotDelete').value;
    var confirmMessage = L.isLang('de') ? 'Möchten Sie den Slot "' + slotName + '" wirklich löschen?' : 'Do you really want to delete the slot "' + slotName + '"?';
    if(!window.confirm(confirmMessage)){
        return;
    }
    flipActionThenToggle(flip.deleteSlot, [slotName], toggleElementList, progressBarId).then(function () {
        initSlots();
        L('#newSlotLabel').value = '';
    });
};