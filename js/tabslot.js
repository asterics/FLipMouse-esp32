window.tabSlot = {};

tabSlot.initSlots = function () {
    var slots = flip.getSlots();
    L('#delete-slot-button').disabled = slots.length <= 1;
    L('#create-slot-button').disabled = true;
    L.removeAllChildren('.slot-select');
    slots.forEach(function (slot) {
        var option = document.createElement("option");
        option.value = slot;
        option.innerHTML = slot;
        L('.slot-select').forEach(function (elem) {
            elem.appendChild(option.cloneNode(true));
            elem.value = flip.getCurrentSlot();
        });
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
    var slotName = L('#newSlotLabel').value;
    actionAndToggle(flip.createSlot, [slotName], toggleElementList, progressBarId).then(function () {
        tabSlot.initSlots();
        L('#newSlotLabel').value = '';
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
        L('#newSlotLabel').value = '';
    });
};

window.tabSlot.resetConfig = function (toggleElementList, progressBarId) {
    var confirmMessage = L.translate('CONFIRM_RESET_SLOTS');
    if(!window.confirm(confirmMessage)){
        return;
    }
    actionAndToggle(flip.restoreDefaultConfiguration, [], toggleElementList, progressBarId);
};