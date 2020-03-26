window.tabIR = {};

tabIR.initIR = function () {
    flip.sendATCmd(C.AT_CMD_IR_LIST).then(function(response) {
		var list = response.trim().split('\n');
		//map returned list for IR commands & remove parts without "IRCommand"
		var irNames = list.map(function (element) {
			if(element.startsWith("IRCommand")) {
				return element.substring(element.indexOf(':') + 1);
			}
		});
		//remove any undefined elements
		irNames = irNames.filter(function (el) {
		  return el != undefined;
		});
		//use array only if valid
		if(irNames.length > 0) {
			L('.ir-select').forEach(function (elem) {
				elem.innerHTML = L.createSelectItems(irNames);
			});
			//enable play/delete button if no IR cmd available
			L('#delete-ir-button').disabled = false;
			L('#play-ir-button').disabled = false;
		} else {
			L('.ir-select').forEach(function (elem) { elem.innerHTML = ""; });
			//disable play/delete button if no IR cmd available
			L('#delete-ir-button').disabled = true;
			L('#play-ir-button').disabled = true;
		}
	    
		//disable IR create button, until a name is entered
		L('#create-ir-button').disabled = true;
    });
};

window.tabIR.saveIRLabelChanged = function (element) {
    L('#create-ir-button').disabled = !element.value;
};

window.tabIR.createIR = function (toggleElementList, progressBarId) {
    var irCmd = L('#newIRLabelEn') ? L('#newIRLabelEn').value : L('#newIRLabelDe').value;
    actionAndToggle(flip.sendATCmdWithParam, [C.AT_CMD_IR_RECORD, irCmd, 10000], toggleElementList).then(function () {
		tabIR.initIR();
		L.setValue('#newIRLabelEn', '');
        L.setValue('#newIRLabelDe', '');
	});
};

window.tabIR.deleteIR = function (toggleElementList, progressBarId) {
    var irCmd = L('#selectIRDelete').value;
    var confirmMessage = L.translate('CONFIRM_DELETE_IR', irCmd);
    if(!window.confirm(confirmMessage)){
        return;
    }
    flip.sendATCmdWithParam(C.AT_CMD_IR_DELETE, irCmd);
    tabIR.initIR();
};

window.tabIR.playIR = function (toggleElementList) {
    var irCmd = L('#selectIRDelete').value;
    flip.sendATCmdWithParam(C.AT_CMD_IR_PLAY, irCmd);
};

window.tabIR.resetConfig = function (toggleElementList, progressBarId) {
    var confirmMessage = L.translate('CONFIRM_DELETE_ALL_IR');
    if(!window.confirm(confirmMessage)){
        return;
    }
    flip.sendATCmd(C.AT_CMD_IR_DELETEALL);
    tabIR.initIR();
};
