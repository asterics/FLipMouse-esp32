window.tabAction = {};

window.tabAction.init = function () {
    tabAction.initBtnModeActionTable();
    var modes = flip.getConfig(flip.FLIPMOUSE_MODE) == C.FLIPMOUSE_MODE_MOUSE ? C.BTN_MODES_WITHOUT_STICK : C.BTN_MODES;
    L('#selectActionButton').innerHTML = L.createSelectItems(modes);
    L('#currentAction').innerHTML = getReadable(flip.getConfig(C.BTN_MODES[0]));
    L('#' + flip.getConfig(flip.FLIPMOUSE_MODE)).checked = true;

    L('#SELECT_LEARN_CAT_MOUSE').innerHTML = L.createSelectItems(C.AT_CMDS_MOUSE);
    L('#SELECT_LEARN_CAT_FLIPACTIONS').innerHTML = L.createSelectItems(C.AT_CMDS_FLIP);
    L('#SELECT_LEARN_CAT_KEYBOARD_SPECIAL').innerHTML = L.createSelectItems(C.SUPPORTED_KEYCODES, function (code) {
        return C.KEYCODE_MAPPING[code];
    }, 'SELECT_SPECIAL_KEY');
};

window.tabAction.initBtnModeActionTable = function () {
    L.removeAllChildren('#currentConfigTb');
    var backColor = false;
    var ariaDesc = '<span class="hidden" aria-hidden="false">' + L.translate('DESCRIPTION') + '</span>';
    var ariaAction = '<span class="hidden" aria-hidden="false">' + L.translate('CURR_ACTION') + '</span>';
    var ariaAtCmd = '<span class="hidden" aria-hidden="false">' + L.translate('CURR_AT_CMD') + '</span>';
    var modes = flip.getConfig(flip.FLIPMOUSE_MODE) == C.FLIPMOUSE_MODE_MOUSE ? C.BTN_MODES_WITHOUT_STICK : C.BTN_MODES;
    modes.forEach(function (btnMode) {
        var liElm = L.createElement('li', 'row');
        var changeA = L.createElement('a', '', L.translate(btnMode));
        changeA.href = 'javascript:tabAction.selectActionButton("' + btnMode + '")';
        changeA.title = L.translate('CHANGE_TOOLTIP', L.translate(btnMode));
        var descriptionDiv = L.createElement('div', 'two columns', [ariaDesc, changeA]);
        var currentActionDiv = L.createElement('div', 'four columns', [ariaAction, getReadable(flip.getConfig(btnMode))]);
        var currentAtCmdDiv = L.createElement('div', 'four columns', [ariaAtCmd, flip.getConfig(btnMode)]);
        var spacerDiv = L.createElement('div', 'one column show-mobile space-bottom');
        liElm.appendChild(descriptionDiv);
        liElm.appendChild(currentActionDiv);
        liElm.appendChild(currentAtCmdDiv);
        liElm.appendChild(spacerDiv);
        if(backColor) {
            liElm.style = 'background-color: #e0e0e0;';
        }
        L('#currentConfigTb').appendChild(liElm);
        backColor = !backColor;
    });
};

window.tabAction.selectActionButton = function (btnMode) {
    L('#selectActionButton').value = btnMode;
    L('#selectActionButton').focus();
    refreshCurrentAction(btnMode);
    resetSelects();
    initAdditionalData();
};

function refreshCurrentAction(btnMode) {
    L('#currentAction').innerHTML = getReadable(flip.getConfig(btnMode));
}

window.tabAction.selectActionCategory = function (elem) {
    console.log(elem.id);
    L.setVisible('[id^=WRAPPER_LEARN_CAT]', false);
    L.setVisible('#WRAPPER_' + elem.id);

    resetSelects();
    initAdditionalData();
};

window.tabAction.selectMode = function (elem) {
    flip.setFlipmouseMode(elem.id);
    tabAction.init();
};

tabAction.selectAtCmd = function (atCmd) {
    tabAction.selectedAtCommand = atCmd;
    initAdditionalData(atCmd);
    if(!C.ADDITIONAL_DATA_CMDS.includes(atCmd)) {
        tabAction.setAtCmd(atCmd);
    }
};

tabAction.setAtCmd = function (atCmd) {
    var selectedButton = L('#selectActionButton').value;
    if(atCmd && selectedButton) {
        flip.setButtonAction(selectedButton, atCmd);
        refreshCurrentAction(L('#selectActionButton').value);
    }
};

tabAction.setAtCmdWithAdditionalData = function (data) {
    tabAction.setAtCmd(tabAction.selectedAtCommand + ' ' + data);
};

function initAdditionalData(atCmd) {
    L.setVisible('#WRAPPER_' + C.ADDITIONAL_FIELD_TEXT, false);
    L.setVisible('#WRAPPER_' + C.ADDITIONAL_FIELD_SELECT, false);
    switch (atCmd) {
        case C.AT_CMD_LOAD_SLOT:
            L.setVisible('#WRAPPER_' + C.ADDITIONAL_FIELD_SELECT);
            L('#' + C.ADDITIONAL_FIELD_SELECT).innerHTML = L.createSelectItems(flip.getSlots());
            L('[for=' + C.ADDITIONAL_FIELD_SELECT + ']')[0].innerHTML = 'Slot';
            break;
    }
}

function resetSelects() {
    var atCmd = flip.getConfig(L('#selectActionButton').value);
    //L('#SELECT_'+ C.LEARN_CAT_KEYBOARD).value = atCmd; //TODO add if implemented
    L('#SELECT_'+ C.LEARN_CAT_MOUSE).value = atCmd;
    L('#SELECT_'+ C.LEARN_CAT_FLIPACTIONS).value = atCmd;
}

function processForQueue(queueElem) {
    window.tabAction.queue = window.tabAction.queue  || [];
    if (C.SUPPORTED_KEYCODES.includes(parseInt(queueElem.keyCode))) {
        var currentText = getText(tabAction.queue);
        var isDeleteInstruction = queueElem.keyCode == C.JS_KEYCODE_BACKSPACE && currentText;
        var isSpecialAfterText = currentText && queueElem.keyCode != C.JS_KEYCODE_SPACE && isSpecialKey(queueElem);
        var isSameSpecialAsLast = tabAction.queue.length > 0 && tabAction.queue[tabAction.queue.length-1].key == queueElem.key && isSpecialKey(queueElem);
        if (!isDeleteInstruction && !isSpecialAfterText && !isSameSpecialAsLast) {
            tabAction.queue.push(queueElem);
        } else if (isDeleteInstruction || isSameSpecialAsLast) {
            tabAction.queue.pop();
        }
        tabAction.evalRec();
    }
}

tabAction.handleKeyBoardEvent = function (event) {
    if(event.keyCode == C.JS_KEYCODE_TAB) {
        return; //keyboard navigation should be possible
    }
    if(event.keyCode == 229) {
        tabAction.listenToKeyboardInput = true;
        return;
    }
    event.preventDefault();
    if(event.repeat) {
        return; //ignore auto-repeated events (long keystroke)
    }
    processForQueue(getQueueElem(event));
};

tabAction.handleOnKeyboardInput = function (event) {
    if(!tabAction.listenToKeyboardInput || tabAction.lastInputLength >= event.target.value.length) {
        if(tabAction.lastInputLength > event.target.value.length) {
            tabAction.queue.pop();
            tabAction.evalRec();
        }
        return;
    }
    tabAction.listenToKeyboardInput = false;
    Array.from(event.data).forEach(function (char) {
        var keyCode = char.toUpperCase().charCodeAt(0);
        var queueElem = {
            key: char,
            keyCode: keyCode
        };
        processForQueue(queueElem);
    });
};

tabAction.addSpecialKey = function (keycode) {
    window.tabAction.queue = window.tabAction.queue  || [];
    if(getText(tabAction.queue)) {
        tabAction.resetRec();
    }
    processForQueue({
        key: 'SPECIAL_' + keycode,
        keyCode: keycode
    });
    L('#SELECT_LEARN_CAT_KEYBOARD_SPECIAL').value = -1;
    L('#INPUT_LEARN_CAT_KEYBOARD').focus();
};

tabAction.evalRec = function () {
    var atCmd = getAtCmd(tabAction.queue);
    L('#buttonRecOK').disabled = !atCmd;
    L('#recordedAtCmd').innerHTML = atCmd || L.translate('NONE_BRACKET');
    var readable = getReadable(atCmd);
    var oldA11yText = L('#recordedActionA11y').innerHTML;
    var newA11yText = L.translate('ENTERED_ACTION') + (readable || L.translate('NONE_BRACKET'));
    if(oldA11yText != newA11yText) {
        L('#recordedActionA11y').innerHTML = newA11yText;
    }
    L('#INPUT_LEARN_CAT_KEYBOARD').value = readable;
    tabAction.lastInputLength = readable.length;
};

tabAction.resetRec = function () {
    tabAction.lastInputLength = 0;
    tabAction.queue = [];
    tabAction.evalRec();
};

tabAction.saveRec = function () {
    if(document.onkeydown) {
        document.onkeydown = null;
        L.toggle('#start-rec-button-normal', '#start-rec-button-rec');
    }
    tabAction.setAtCmd(getAtCmd(tabAction.queue));
};

function getQueueElem(event) {
    return {
        key: event.key,
        keyCode: event.keyCode || e.which,
        altKey: event.altKey
    }
}

function isPrintableKey(e) {
    return e.key.length === 1;
}

function isSpecialKey(e) {
    return e.key.length !== 1;
}

/**
 * parses the given keyList and returns the parsed text
 * @param keyList
 * @return the text that is specified by the given keyList, or null if it contains keypress events that do not produce a text
 */
function getText(keyList) {
    var text = '';
    for(var i=0; i<keyList.length; i++) {
        var elm = keyList[i];
        var code = elm.keyCode;
        if(isSpecialKey(elm)) {
            var isAltGr = isAltGrLetter(elm, keyList[i+1], keyList[i+2]);
            if(code != C.JS_KEYCODE_SHIFT && !isAltGr) {
                return null;
            }
            if(isAltGr) {
                i++; //Skip next letter (Alt), continue with printable letter
            }
        } else {
            text += elm.key;
        }
    }
    return text;
}

function isAltGrLetter(e1,e2,e3) {
    if(!e1 || !e2 || !e3) return false;
    return e1.keyCode == C.JS_KEYCODE_CTRL && e2.keyCode == C.JS_KEYCODE_ALT && isPrintableKey(e3) && e3.keyCode && e3.altKey;
}

function getAtCmd(queue) {
    if(!queue || queue.length == 0) {
        return '';
    }
    var atCmd;
    var text = getText(queue);
    if(text && text.length > 1) {
        atCmd = C.AT_CMD_WRITEWORD + ' ' + text;
    } else {
        var postfix = '';
        queue.forEach(function (e) {
            var code = C.KEYCODE_MAPPING[e.keyCode];
            if(code) {
                postfix += code + ' ';
            }
        });
        atCmd = C.AT_CMD_KEYPRESS + ' ' + postfix.trim();
    }
    return atCmd.substring(0, C.MAX_LENGTH_ATCMD);
}

function getReadable(atCmd) {
    if(!atCmd) {
        return '';
    }
    var prefix = atCmd.substring(0, C.LENGTH_ATCMD_PREFIX-1);
    var postfix = atCmd.substring(C.LENGTH_ATCMD_PREFIX);
    if(prefix == C.AT_CMD_KEYPRESS) {
        postfix = postfix.replace(/ /g, ' + ');
    }
    postfix = postfix.replace(/KEY_/g, '');
    return L.translate(prefix, postfix + ' '); //add space to prevent word detection in tablet/smartphone input
}